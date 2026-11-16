// The Ancient forest's living dressing (cooks/biome-forest.md 2.1-2.2, 2.7, 3.5; cooks/build-biome-forest.md 1.4,
// 2.5 "Trees (instances)", 2.6 the decoration rule in numbers; ledger R11, R15). Reads
//   Data/forest.json               the land's contract (Tools/terrain/forest.mjs): frame, zone grid, canopy targets,
//                                  giants, fallen giants, thornbrakes, routes, rivers, falls, sites, kit, spots, arenas,
//                                  walkways, the Hollow Root, the village and camps
//   Art/Terrain/forest_height.r16  the ground (2 m samples from the frame's x0/y0, value = 32768 + metres * 16)
//   Art/Terrain/island_height.r16  only to leave the strip under the Old Island's own ground alone
// and writes the forest_* instance kinds into Data/island_dressing.json (the map's one dressing file), the ground layer
// (moss, leaf litter, ground cover: section 8) into Data/forest_ground.json, and each kind's rows
// [x cm, y cm, z cm, yaw deg, scale] where scale = the wanted height over the species' reference height in
// Source/Wildreach/WRTreeKit.cpp (drawn and harvested by AWRDressing, Source/Wildreach/WRWorldDressing.cpp).
// Re-running it is safe (and needed after Tools/terrain/forest.mjs): every forest_* row is removed first and the rest
// of the file is kept as it is (one JSON.stringify of the whole file, as storm_peaks.mjs and swamp.mjs write it).
//   node Tools/terrain/forest_dressing.mjs
//
// The rule (THE BAR: full and alive, never cluttered; every tree has a reason to stand where it does):
//  - Fallen giants and the standing giants come first (forest.json places them: 12 kit logs + the Nurse Log, giants
//    only in F4/F5 every 45-70 m, each with its thornbrake ring); this script draws the standing giants and the dead
//    snags from those rows, untouched, and keeps everything else off their trunks, rings and logs.
//  - Light gaps of 25-40 m are cut round every POI so each sits in a shaft of light (camps 60, the village its radius),
//    and an edge ring of hazel and saplings grows round each forest gap (the clearing's edge, Far Cry 5's read).
//  - Tree density follows the canopy gradient 0% coast -> 40% F2/F3 -> 75% F4 -> 90% F5 (giants supply most of F5),
//    blended across zone edges over ~100 m (zones are sampled through a 112 m wide kernel).
//  - F1: hedgerows on field edges (hazel, holly, bramble with an oak standard every 30-50 m), running through every
//    hedgerow-gap kit along its hedge line; a few lone field oaks. Orchards are the farm sites' own (WRForestScenerySites).
//  - F2: alder and ash on the banks, two of the alder variants leaning out over the water, ferns thick, mossy
//    boulders along the channel; pines from the tundra neck fading out by y +2,500.
//  - F3: beech and lime groves, holly understory; crystal-border boulders on the south edge.
//  - F4: oaks and beech 25-40 m under the giants, holly and hazel, ferns and bramble clumps.
//  - F5: the giants' green dusk: a thin understory of young beech, holly and saplings, ferns over the root-mat.
//  - Nothing on roads (5 m clear of the edge for trees), trails, the Hornwarden's and the Wild Hunt's ways, rivers
//    (channel + bank), falls, leaf pits, the Hollow Root's chasms, the Sinkroot pit, the Rootgrave plug, the arenas,
//    or under the canopy walkways (where a crown would push through the deck).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const t0 = Date.now();
const F = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'forest.json'), 'utf8'));
const { x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS } = F.frame;
const ISLAND_EDGE = F.frame.islandEdge ?? 2016;
const HB = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'forest_height.r16'));
if (HB.length !== N * N * 2) throw new Error(`forest_height.r16 is ${HB.length} bytes, the frame wants ${N * N * 2}`);
const HF = new Float32Array(N * N);
for (let k = 0; k < N * N; k++) HF[k] = (HB.readUInt16LE(k * 2) - 32768) / UNITS;

// ---------------------------------------------------------------- helpers
const DEG = Math.PI / 180;
const lerp = (a, b, t) => a + (b - a) * t;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
let seed = 424242;
const R = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
const RR = (a, b) => a + (b - a) * R();
const pick = (table) => { let s = 0; for (const [, w] of table) s += w; let r = R() * s; for (const [v, w] of table) { r -= w; if (r <= 0) return v; } return table[table.length - 1][0]; };
const hash2 = (a, b) => { let h = (a * 374761393 + b * 668265263) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };

function hAt(x, y) {
	const fx = clamp((x - X0) / STEP, 0, N - 1.001), fy = clamp((y - Y0) / STEP, 0, N - 1.001);
	const i = Math.floor(fx), j = Math.floor(fy), u = fx - i, v = fy - j, k = j * N + i;
	return lerp(lerp(HF[k], HF[k + 1], u), lerp(HF[k + N], HF[k + N + 1], u), v);
}
const slopeAt = (x, y) => Math.hypot(hAt(x + 1.5, y) - hAt(x - 1.5, y), hAt(x, y + 1.5) - hAt(x, y - 1.5)) / 3;

// The strip over the Old Island's south shore: where the island's own ground covers this land, the island's dressing
// is what stands there (forest.mjs writes this land 3 m under it; WRLand builds nothing there).
let island = null;
try { island = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16')); } catch { island = null; }
const ISL = { size: 4033, half: 2016 };
function underIsland(x, y) {
	if (y > ISLAND_EDGE) return false;
	if (!island) return true;
	const i = Math.round(x + ISL.half), j = Math.round(y + ISL.half);
	if (i < 0 || j < 0 || i >= ISL.size || j >= ISL.size) return false;
	const ih = (island.readUInt16LE((j * ISL.size + i) * 2) - 32768) / 32;
	return ih > 0.5 || hAt(x, y) < ih - 0.25;
}

// ---------------------------------------------------------------- zones, the canopy gradient, the coast
const Z = F.zones;
const zoneAt = (x, y) => {
	const i = Math.floor((x - Z.x0) / Z.cell), j = Math.floor((y - Z.y0) / Z.cell);
	if (i < 0 || j < 0 || j >= Z.height || i >= Z.width) return '~';
	return Z.rows[j][i];
};
const LIVING = new Set(['1', '2', '3', '4', '5']);
// Smoothed zone weights per 16 m cell (a 7 x 7 cell Gaussian, ~112 m): the 60-120 m blends of DC 2.2. A tree's zone
// is drawn from these weights, so beech creeps into the oak groves near F3 and ash thins out into F4 naturally.
const ZW = new Array(Z.width * Z.height);
{
	const K = 3, sig = 1.6;
	for (let j = 0; j < Z.height; j++) for (let i = 0; i < Z.width; i++) {
		const w = {}; let tot = 0;
		for (let dj = -K; dj <= K; dj++) for (let di = -K; di <= K; di++) {
			const jj = j + dj, ii = i + di;
			if (jj < 0 || ii < 0 || jj >= Z.height || ii >= Z.width) continue;
			const c = Z.rows[jj][ii];
			if (!LIVING.has(c)) continue;
			const g = Math.exp(-(di * di + dj * dj) / (2 * sig * sig));
			w[c] = (w[c] || 0) + g; tot += g;
		}
		if (tot > 0) for (const c in w) w[c] /= tot;
		ZW[j * Z.width + i] = w;
	}
}
const weightsAt = (x, y) => {
	const i = Math.floor((x - Z.x0) / Z.cell), j = Math.floor((y - Z.y0) / Z.cell);
	if (i < 0 || j < 0 || j >= Z.height || i >= Z.width) return {};
	return ZW[j * Z.width + i];
};
// Distance to the sea (m, 16 m steps) for the coast band: no hedges on the dunes and the salt marsh.
const SEA = new Float32Array(Z.width * Z.height).fill(1e9);
{
	const q = [];
	for (let j = 0; j < Z.height; j++) for (let i = 0; i < Z.width; i++) if (Z.rows[j][i] === '~') { SEA[j * Z.width + i] = 0; q.push(j * Z.width + i); }
	for (let h = 0; h < q.length; h++) {
		const c = q[h], i = c % Z.width, j = (c - i) / Z.width;
		for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
			const ii = i + di, jj = j + dj;
			if (ii < 0 || jj < 0 || ii >= Z.width || jj >= Z.height) continue;
			const n = jj * Z.width + ii;
			if (SEA[n] > SEA[c] + Z.cell) { SEA[n] = SEA[c] + Z.cell; q.push(n); }
		}
	}
}
const seaDist = (x, y) => {
	const i = Math.floor((x - Z.x0) / Z.cell), j = Math.floor((y - Z.y0) / Z.cell);
	if (i < 0 || j < 0 || j >= Z.height || i >= Z.width) return 0;
	return SEA[j * Z.width + i];
};

// ---------------------------------------------------------------- distance fields (2 m grid over the frame)
// routeD: metres from the nearest walked way's edge (roads, trails, the stair, the Hornwarden's loop, the Wild Hunt's
// gallop, the lost paths, the Hollow Root's chasm passages). riverD: metres from the nearest water's edge (negative in
// the water), riverDir: which way the water is (for the leaning alders).
const G = 2, GW = Math.ceil((N - 1) * STEP / G) + 1;
const routeD = new Float32Array(GW * GW).fill(99);
const riverD = new Float32Array(GW * GW).fill(99);
function stamp(field, ax, ay, bx, by, wa, wb, reach) {
	const minx = Math.min(ax, bx) - Math.max(wa, wb) - reach, maxx = Math.max(ax, bx) + Math.max(wa, wb) + reach;
	const miny = Math.min(ay, by) - Math.max(wa, wb) - reach, maxy = Math.max(ay, by) + Math.max(wa, wb) + reach;
	const i0 = Math.max(0, Math.floor((minx - X0) / G)), i1 = Math.min(GW - 1, Math.ceil((maxx - X0) / G));
	const j0 = Math.max(0, Math.floor((miny - Y0) / G)), j1 = Math.min(GW - 1, Math.ceil((maxy - Y0) / G));
	const dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy || 1;
	for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
		const px = X0 + i * G, py = Y0 + j * G;
		const t = clamp(((px - ax) * dx + (py - ay) * dy) / L2, 0, 1);
		const d = Math.hypot(px - (ax + dx * t), py - (ay + dy * t)) - lerp(wa, wb, t);
		const k = j * GW + i;
		if (d < field[k]) field[k] = d;
	}
}
const fieldAt = (field, x, y) => {
	const i = Math.round((x - X0) / G), j = Math.round((y - Y0) / G);
	if (i < 0 || j < 0 || i >= GW || j >= GW) return 99;
	return field[j * GW + i];
};
for (const r of F.routes) {
	if (r.kind === 'walkway') continue;            // up in the canopy: handled as walkways below
	const hw = (r.w || 3) / 2;
	for (let q = 0; q + 1 < r.pts.length; q++) stamp(routeD, r.pts[q][0], r.pts[q][1], r.pts[q + 1][0], r.pts[q + 1][1], hw, hw, 9);
	if (r.pts.length === 1) stamp(routeD, r.pts[0][0], r.pts[0][1], r.pts[0][0], r.pts[0][1], hw, hw, 9);
}
for (const r of F.rivers) {
	const W = r.widths || [];
	for (let q = 0; q + 1 < r.pts.length; q++) stamp(riverD, r.pts[q][0], r.pts[q][1], r.pts[q + 1][0], r.pts[q + 1][1], (W[q] ?? 12) / 2, (W[q + 1] ?? 12) / 2, 20);
}
for (const f of F.falls) stamp(riverD, f.top[0], f.top[1], f.bottom[0], f.bottom[1], f.width / 2 + 4, f.width / 2 + 4, 20);
const routeAt = (x, y) => fieldAt(routeD, x, y);
const riverAt = (x, y) => fieldAt(riverD, x, y);
const towardWater = (x, y) => {
	const gx = riverAt(x + 2, y) - riverAt(x - 2, y), gy = riverAt(x, y + 2) - riverAt(x, y - 2);
	return Math.atan2(-gy, -gx) / DEG;
};

// ---------------------------------------------------------------- keep-outs (circles, rings, logs, walls) in a 32 m hash
// Each keep-out says what it keeps out: T trees, S shrubs (hazel, holly, bramble, saplings), N ferns, K rocks.
const HC = 32, keep = new Map();
function addKeep(x, y, r, what, test) {
	const k = { x, y, r, what, test };
	for (let j = Math.floor((y - r) / HC); j <= Math.floor((y + r) / HC); j++)
		for (let i = Math.floor((x - r) / HC); i <= Math.floor((x + r) / HC); i++) {
			const key = i * 100003 + j;
			if (!keep.has(key)) keep.set(key, []);
			keep.get(key).push(k);
		}
}
const ALL = 'TSNK';
function keptOut(x, y, cls) {
	const list = keep.get(Math.floor(x / HC) * 100003 + Math.floor(y / HC));
	if (!list) return false;
	for (const k of list) {
		if (!k.what.includes(cls)) continue;
		if (k.test ? k.test(x, y) : Math.hypot(x - k.x, y - k.y) < k.r) return true;
	}
	return false;
}
const segD = (px, py, ax, ay, bx, by) => { const dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy || 1, t = clamp(((px - ax) * dx + (py - ay) * dy) / L2, 0, 1); return Math.hypot(px - ax - dx * t, py - ay - dy * t); };

// Light gaps (DC 2.1 "gaps are places", plan 2.6: 25-40 m round every POI). Ferns may grow into a gap's outer part
// (the light feeds them), never into the place itself. Gaps inside the canopy (F2-F5) get an edge ring (below).
const gaps = [];
function gap(x, y, r, ring = true) {
	addKeep(x, y, r, 'TSK');
	addKeep(x, y, r * 0.7, 'N');
	if (ring) gaps.push({ x, y, r });
}
const SITE_GAP = { camp: 60, village: 0, cave_mouth: 20, alder_grotto: 20, badger_sett: 22, foxfire_cellar: 20, sinkroot_cave: 50, sinkroot: 50,
	root_mine: 25, hanging_oak: 30, hermits_oak: 32, clear_cut: 0, four_hearts: 0, bower: 0, paper_hollow: 0, ringfort: 42, lost_path: 22 };
for (const s of F.sites) {
	const r = SITE_GAP[s.kind] ?? SITE_GAP[s.id] ?? (s.landmark ? 40 : 30);
	if (s.kind === 'village') continue;            // the village below
	if (r > 0) gap(s.x, s.y, r, LIVING.has(s.zone) && s.zone !== '1');
}
for (const s of F.shrines) gap(s.x, s.y, 30, s.zone !== '1');
if (F.village) gap(F.village.x, F.village.y, (F.village.radius || 150) + 15, false);
for (const c of F.camps || []) gap(c.x, c.y, 60, false);
// The arenas: the fights' floors stay as their tracks built them (stumps of the Clear-cut, the Bower's moss, the
// Clearing of Four Hearts); their rims are light gaps too.
for (const [id, a] of Object.entries(F.arenas || {})) gap(a.x, a.y, a.radius + (id === 'old_gnarl' ? 6 : 14), true);
// Kit POIs: a gap the size of what's there; trail trees and hedge gaps are small marks on a route.
const KIT_GAP = { fallen_giant: 25, trail_tree: 5, hedgerow_gap: 4, beehive_tree: 12, house_tree: 25, field_ruin: 16, foxfire_bank: 9,
	wasp_nest: 10, kill_scene: 15, ford: 12, canopy_platform: 10, thornbrake_cache: 10 };
for (const k of F.kit) gap(k.x, k.y, KIT_GAP[k.kind] ?? 20, (KIT_GAP[k.kind] ?? 20) >= 15 && k.zone !== '1');
// Giants and snags: their trunk, buttresses and root crown (the thornbrake ring keeps the rest).
for (const g of F.giants) {
	const big = ['heart', 'eldest', 'strangler'].includes(g.kind);
	addKeep(g.x, g.y, Math.max(g.r * 3.2, g.r + 5) + (big ? 6 : 0), ALL);
}
// Thornbrakes: the ring and what it encloses (the giant's root crown), or a wall's box. T4's AWRThornbrake draws them.
for (const b of F.brakes) {
	if (b.ring) addKeep(b.x, b.y, b.r + b.depth + 1.5, ALL);
	else {
		const c = Math.cos(b.yaw * DEG), s = Math.sin(b.yaw * DEG), hl = b.length / 2 + 1.5, hd = b.depth / 2 + 1.5;
		addKeep(b.x, b.y, Math.hypot(hl, hd), ALL, (x, y) => { const dx = x - b.x, dy = y - b.y; return Math.abs(dx * c + dy * s) < hl && Math.abs(-dx * s + dy * c) < hd; });
	}
}
// Fallen giants: the log, its root plate (r x 2.6 across at A) and a clear end at the amber heartwood (B). Ferns grow
// along the log's flanks (its shade and rot feed them).
for (const l of F.fallen) {
	const [ax, ay] = l.a, [bx, by] = l.b, len = Math.hypot(bx - ax, by - ay);
	const mx = (ax + bx) / 2, my = (ay + by) / 2;
	addKeep(mx, my, len / 2 + l.r * 3 + 4, 'TSK', (x, y) => segD(x, y, ax, ay, bx, by) < l.r + 3 || Math.hypot(x - ax, y - ay) < l.r * 2.8 || Math.hypot(x - bx, y - by) < l.r + 6);
	addKeep(mx, my, len / 2 + l.r * 3 + 2, 'N', (x, y) => segD(x, y, ax, ay, bx, by) < l.r + 0.6 || Math.hypot(x - ax, y - ay) < l.r * 2.6);
}
// Spots: leaf pits (their sag ring must read), foxfire banks and glowworm banks (lights on rotten wood / under banks),
// the wasp nests and the Hollow Root's ways in.
for (const s of F.spots) {
	if (s.kind === 'leaf_pit') addKeep(s.x, s.y, (s.r || 3) + 1.5, ALL);
	else if (s.kind === 'widowmaker' || s.kind === 'perch') continue;   // high in the canopy
	else addKeep(s.x, s.y, Math.max(3, (s.r || 0) + 1), ALL);
}
// The Hollow Root's rooms (chasms roofed with root mats at runtime), the Sinkroot pit, the Rootgrave plug.
for (const h of F.hollow || []) addKeep(h.x, h.y, Math.hypot(h.hx, h.hy) + 6, ALL);
if (F.sinkroot) addKeep(F.sinkroot.x, F.sinkroot.y, (F.sinkroot.rTop || 42) + 8, ALL);
if (F.rootgrave) addKeep(F.rootgrave.plug[0], F.rootgrave.plug[1], (F.rootgrave.plugRadius || 75) + 5, ALL);
// Under the canopy walkways (20-40 m up): no tree whose crown would push through a deck. Ferns and rocks are fine.
const decks = [...(F.walkways || []).map(w => [w.a, w.b]), ...F.routes.filter(r => r.kind === 'walkway').flatMap(r => r.pts.slice(1).map((p, q) => [r.pts[q], p]))];
for (const [a, b] of decks) addKeep((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, Math.hypot(b[0] - a[0], b[1] - a[1]) / 2 + 8, 'TS', (x, y) => segD(x, y, a[0], a[1], b[0], b[1]) < 7);

// ---------------------------------------------------------------- placing
// Species: the WRTreeKit grammar's reference heights (cm) and the trunk / clump spacing each needs (m).
const REF = { oak: 2200, beech: 2800, lime: 3000, alder: 1800, ash: 2600, hazel: 600, holly: 900, sapling: 500, giant: 8000, dead_giant: 6000, fern: 120, bramble: 180, boulder: 150, stump: 100, pine: 1000 };
const KIND_SPECIES = { forest_oak: 'oak', forest_beech: 'beech', forest_lime: 'lime', forest_alder: 'alder', forest_alder_lean: 'alder', forest_ash: 'ash',
	forest_hazel: 'hazel', forest_holly: 'holly', forest_sapling: 'sapling', forest_giant: 'giant', forest_dead: 'dead_giant', forest_fern: 'fern',
	forest_bramble: 'bramble', forest_rock: 'boulder', forest_stump: 'stump', forest_pine: 'pine' };
const out = Object.fromEntries(Object.keys(KIND_SPECIES).map(k => [k, []]));
// Trunk spacing hash (8 m): what already stands, so trees keep their distance and shrubs don't grow into trunks.
const TC = 8, trunks = new Map();
function spaced(x, y, r) {
	for (let j = Math.floor((y - 12) / TC); j <= Math.floor((y + 12) / TC); j++)
		for (let i = Math.floor((x - 12) / TC); i <= Math.floor((x + 12) / TC); i++) {
			const l = trunks.get(i * 100003 + j);
			if (l) for (const t of l) if (Math.hypot(t[0] - x, t[1] - y) < t[2] + r) return false;
		}
	return true;
}
function claim(x, y, r) { const key = Math.floor(x / TC) * 100003 + Math.floor(y / TC); if (!trunks.has(key)) trunks.set(key, []); trunks.get(key).push([x, y, r]); }
// The ground a tree stands on: the lowest of its centre and six points round its flare, so the root flare never floats
// on a slope (the uphill side is buried, as real roots are).
function baseZ(x, y, flareM) {
	let z = hAt(x, y);
	if (flareM > 0.2) for (let a = 0; a < 6; a++) z = Math.min(z, hAt(x + Math.cos(a * 60 * DEG) * flareM, y + Math.sin(a * 60 * DEG) * flareM));
	return z;
}
function put(kind, x, y, heightM, yaw, flareM = 0, sinkM = 0) {
	const sp = KIND_SPECIES[kind];
	const scale = sp === 'pine' ? heightM / 10 : (heightM * 100) / REF[sp];
	const z = baseZ(x, y, flareM) - sinkM;
	out[kind].push(Math.round(x * 100), Math.round(y * 100), Math.round(z * 100), Math.round(((yaw % 360) + 360) % 360), +scale.toFixed(2));
}
// Common ground test for everything this script plants.
function ground(x, y, cls, maxSlope) {
	if (underIsland(x, y)) return false;
	const z = zoneAt(x, y);
	if (!LIVING.has(z)) return false;
	if (hAt(x, y) < 0.8) return false;                                       // the beach and the sea's edge
	if (slopeAt(x, y) > maxSlope) return false;
	const rd = routeAt(x, y), wd = riverAt(x, y);
	if (rd < { T: 5, S: 1.5, N: 0.6, K: 2 }[cls]) return false;
	if (wd < { T: 1.2, S: 0.8, N: 0.3, K: -1.2 }[cls]) return false;
	return !keptOut(x, y, cls);
}

// 1. The standing giants and dead snags from forest.json (the skyline: trunks are the landmarks). Landmark trees
//    (heart, eldest, treant, bower oaks, bell-tree, hermit/hanging/paper oaks, strangler, ringfort beeches) are the
//    scenery's (WRForestScenery*); the 555 giants and 29 dead are drawn here, harvest-proof ("The roots hold it.").
for (const g of F.giants) {
	if (g.kind === 'giant') { put('forest_giant', g.x, g.y, g.h, g.yaw, g.r * 1.6, 0.6); claim(g.x, g.y, g.r * 2.5); }
	else if (g.kind === 'dead') { put('forest_dead', g.x, g.y, g.h, g.yaw, g.r * 1.4, 0.5); claim(g.x, g.y, g.r * 2.2); }
	else claim(g.x, g.y, g.r * 2.5);
}
for (const l of F.fallen) { claim(l.a[0], l.a[1], l.r * 2.6); claim(l.b[0], l.b[1], l.r + 2); }

// 1b. Alder along every bank of F2 first (the river's own line of trees, DC 2.2 "rocky river valley through alder"):
//    placed before the canopy so the bank line is continuous and the canopy fills in behind it.
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 4) for (let x = X0; x < X0 + (N - 1) * STEP; x += 4) {
	const px = x + RR(0, 4), py = y + RR(0, 4), wd = riverAt(px, py);
	if (wd < 1.2 || wd > 5 || zoneAt(px, py) !== '2' || R() > 0.6) continue;
	if (!ground(px, py, 'T', 0.95) || !spaced(px, py, 3.5)) continue;   // alder roots grip steep banks
	claim(px, py, 3.5);
	const hgt = RR(11, 19);
	if (R() < 0.6) put('forest_alder_lean', px, py, hgt, towardWater(px, py) + RR(-20, 20), 0.4, 0.15);
	else put('forest_alder', px, py, hgt, RR(0, 360), 0.4, 0.15);
}

// 2. Edge rings round the forest's light gaps: hazel and saplings at the clearing's rim, the wood closing in again.
for (const gp of gaps) {
	const n = Math.max(6, Math.round(2 * Math.PI * gp.r / 6.5));
	for (let q = 0; q < n; q++) {
		const a = (q / n + RR(-0.3, 0.3) / n) * 2 * Math.PI, rr = gp.r + RR(0.5, 5.5);
		const x = gp.x + Math.cos(a) * rr, y = gp.y + Math.sin(a) * rr;
		if (zoneAt(x, y) === '1' || R() < 0.18 || !ground(x, y, 'S', 0.8) || !spaced(x, y, 1.6)) continue;
		if (R() < 0.6) { put('forest_hazel', x, y, RR(3.2, 5.8), RR(0, 360), 0.4, 0.15); claim(x, y, 1.8); }
		else { put('forest_sapling', x, y, RR(2.8, 6), RR(0, 360), 0, 0.1); claim(x, y, 1.0); }
	}
}

// 3. F1 hedgerows: the newcomer fields are a Voronoi of field seeds ~150 m apart; a field edge is where two seeds are
//    equally near. Each hedgerow-gap kit gets a mirrored pair of seeds across its way (+-70 m along its local X), so a
//    hedge line runs through the kit along its local Y, as the kit's own hedge does (WRForestSceneryKit HedgerowGap).
//    About two edges in three are hedged; the rest are open (a field wall's ruin, a ditch).
const seeds = [];
const kitPairs = [];
for (const k of F.kit.filter(k => k.kind === 'hedgerow_gap')) {
	const c = Math.cos(k.yaw * DEG), s = Math.sin(k.yaw * DEG);
	kitPairs.push([seeds.length, seeds.length + 1]);
	seeds.push([k.x + c * 70, k.y + s * 70, true], [k.x - c * 70, k.y - s * 70, true]);
}
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 150) for (let x = X0; x < X0 + (N - 1) * STEP; x += 150) {
	const sx = x + RR(10, 140), sy = y + RR(10, 140);
	if (zoneAt(sx, sy) !== '1' || seeds.some(p => p[2] && Math.hypot(p[0] - sx, p[1] - sy) < 110)) continue;
	seeds.push([sx, sy, false]);
}
const forced = new Set(kitPairs.map(([a, b]) => a * 10000 + b));
const seedHash = new Map();
for (let s = 0; s < seeds.length; s++) { const key = Math.floor(seeds[s][0] / 200) * 100003 + Math.floor(seeds[s][1] / 200); if (!seedHash.has(key)) seedHash.set(key, []); seedHash.get(key).push(s); }
function nearestTwo(x, y) {
	let a = -1, b = -1, da = 1e9, db = 1e9;
	for (let j = Math.floor(y / 200) - 2; j <= Math.floor(y / 200) + 2; j++) for (let i = Math.floor(x / 200) - 2; i <= Math.floor(x / 200) + 2; i++) {
		const l = seedHash.get(i * 100003 + j); if (!l) continue;
		for (const s of l) { const d = Math.hypot(seeds[s][0] - x, seeds[s][1] - y); if (d < da) { db = da; b = a; da = d; a = s; } else if (d < db) { db = d; b = s; } }
	}
	return [a, b, da, db];
}
const hedgePts = [];
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 2) for (let x = X0; x < X0 + (N - 1) * STEP; x += 2) {
	if (zoneAt(x, y) !== '1' || seaDist(x, y) < 110) continue;
	const [a, b, da, db] = nearestTwo(x, y);
	if (a < 0 || b < 0 || db - da > 1.6) continue;
	const lo = Math.min(a, b), hi = Math.max(a, b);
	if (!forced.has(lo * 10000 + hi) && hash2(lo, hi) > 0.66) continue;
	hedgePts.push([x + RR(-0.6, 0.6), y + RR(-0.6, 0.6), lo * 10000 + hi]);
}
// Oak standards first (one every 30-50 m along a hedge), then the hedge's shrubs between them.
for (let i = hedgePts.length - 1; i > 0; i--) { const j = Math.floor(R() * (i + 1)); [hedgePts[i], hedgePts[j]] = [hedgePts[j], hedgePts[i]]; }
const standards = [];
for (const [x, y] of hedgePts) {
	if (standards.some(s => Math.hypot(s[0] - x, s[1] - y) < RR(30, 50))) continue;
	if (!ground(x, y, 'T', 0.55) || !spaced(x, y, 6)) continue;
	standards.push([x, y]);
	put('forest_oak', x, y, RR(15, 24), RR(0, 360), 0.9, 0.2);
	claim(x, y, 3);
}
for (const [x, y] of hedgePts) {
	if (!ground(x, y, 'S', 0.7) || !spaced(x, y, 1.4)) continue;
	const roll = R();
	if (roll < 0.52) { put('forest_hazel', x, y, RR(3, 4.8), RR(0, 360), 0.4, 0.15); claim(x, y, 1.6); }
	else if (roll < 0.74) { put('forest_holly', x, y, RR(3.5, 6.5), RR(0, 360), 0.3, 0.15); claim(x, y, 1.5); }
	else if (roll < 0.9) { put('forest_bramble', x, y, RR(1.1, 1.7), RR(0, 360), 0, 0.1); claim(x, y, 1.2); }
	else { put('forest_sapling', x, y, RR(2.5, 4.5), RR(0, 360), 0, 0.1); claim(x, y, 1); }
}
// Lone field oaks and the coast's windbreak oaks (canopy 0-8%): one every ~110 m where nothing else stands.
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 110) for (let x = X0; x < X0 + (N - 1) * STEP; x += 110) {
	const px = x + RR(0, 110), py = y + RR(0, 110);
	if (zoneAt(px, py) !== '1' || R() < 0.55 || seaDist(px, py) < 60) continue;
	if (!ground(px, py, 'T', 0.45) || !spaced(px, py, 14)) continue;
	put('forest_oak', px, py, RR(18, 26), RR(0, 360), 1.0, 0.2);
	claim(px, py, 4);
}

// 4. The canopy trees (F2-F5): a 6 m jittered grid, each point kept with the chance that makes its zone's cover
//    (cover = 1 - exp(-n * pi * r^2) for n trees a m^2 of crown radius r; giants' crowns already shade F4 ~20% and F5
//    most of its 90%). Species and size by the zone drawn from the blended weights.
const ZONE = {
	'2': { p: 0.2, trees: [['alder', 45], ['ash', 35], ['oak', 8], ['hazel', 7], ['holly', 5]], h: { alder: [12, 20], ash: [18, 28], oak: [18, 26], hazel: [3.5, 6], holly: [5, 9] } },
	'3': { p: 0.1, trees: [['beech', 42], ['lime', 34], ['oak', 9], ['ash', 7], ['holly', 8]], h: { beech: [22, 32], lime: [20, 30], oak: [18, 26], ash: [20, 28], holly: [6, 10] } },
	'4': { p: 0.17, trees: [['oak', 48], ['beech', 34], ['holly', 10], ['hazel', 5], ['lime', 3]], h: { oak: [24, 34], beech: [28, 38], holly: [6, 11], hazel: [4, 6.5], lime: [26, 34] } },
	'5': { p: 0.1, trees: [['beech', 32], ['holly', 30], ['sapling', 22], ['oak', 8], ['hazel', 8]], h: { beech: [12, 20], holly: [6, 11], sapling: [3, 6.5], oak: [16, 24], hazel: [4, 6] } },
};
const CROWN = { oak: 9, beech: 8, lime: 6.6, alder: 3.6, ash: 7, hazel: 2.6, holly: 2.2, sapling: 1.2 };
const zoneDraw = (w) => { let r = R(), last = null; for (const c in w) { last = c; r -= w[c]; if (r <= 0) return c; } return last; };
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 6) for (let x = X0; x < X0 + (N - 1) * STEP; x += 6) {
	const px = x + RR(0, 6), py = y + RR(0, 6);
	const w = weightsAt(px, py);
	let p = 0;
	for (const c in w) p += (ZONE[c]?.p ?? 0) * w[c];
	if (p <= 0 || R() > p) continue;
	const zc = zoneDraw(w);
	const Zd = ZONE[zc];
	if (!Zd) continue;
	let sp = pick(Zd.trees);
	const small = sp === 'hazel' || sp === 'holly' || sp === 'sapling';
	if (!ground(px, py, small ? 'S' : 'T', small ? 0.8 : 0.7)) continue;
	// The tundra neck (DC 2.2): pines and the alder mix in the north of F2, all pine south to y +2,350, gone by +2,500.
	const pineShare = zc === '2' ? clamp((2500 - py) / 150, 0, 1) * 0.75 : 0;
	if (pineShare > 0 && R() < pineShare) sp = 'pine';
	const [h0, h1] = sp === 'pine' ? [12, 20] : Zd.h[sp];
	const hgt = RR(h0, h1);
	const trunkR = sp === 'pine' ? 0.35 : (({ oak: 0.55, beech: 0.45, lime: 0.42, alder: 0.22, ash: 0.35, hazel: 0.4, holly: 0.15, sapling: 0.06 })[sp]) * hgt * 100 / REF[sp];
	const room = small ? 1.4 : sp === 'pine' ? 3 : Math.max(3, (CROWN[sp] * hgt * 100 / REF[sp]) * 0.45);
	if (!spaced(px, py, room)) continue;
	claim(px, py, room);
	if (sp === 'alder') {
		// On the bank (within 4 m of the water) the two leaning variants lean out over it (WRTreeKit leans them along +X).
		const wd = riverAt(px, py);
		if (wd < 4.5 && R() < 0.75) { put('forest_alder_lean', px, py, hgt, towardWater(px, py) + RR(-20, 20), trunkR * 2, 0.15); continue; }
		put('forest_alder', px, py, hgt, RR(0, 360), trunkR * 2, 0.15);
	} else if (sp === 'pine') put('forest_pine', px, py, hgt, RR(0, 360), 0.6, 0.2);
	else put('forest_' + sp, px, py, hgt, RR(0, 360), small ? 0.3 : trunkR * 2.2, small ? 0.12 : 0.25);
}
// 5. The understory: holly and hazel under the canopy where light falls (F3's holly understory, F4's hazel), bramble
//    clumps in F4 (low, harvestable: the hazard thornbrakes are T4's rings), old stumps where the newcomers felled.
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 9) for (let x = X0; x < X0 + (N - 1) * STEP; x += 9) {
	const px = x + RR(0, 9), py = y + RR(0, 9), z = zoneAt(px, py);
	const roll = R();
	if (z === '3' && roll < 0.1 && ground(px, py, 'S', 0.8) && spaced(px, py, 2)) { put('forest_holly', px, py, RR(4, 8.5), RR(0, 360), 0.3, 0.12); claim(px, py, 2); }
	else if (z === '4' && roll < 0.07 && ground(px, py, 'S', 0.8) && spaced(px, py, 2)) { put(R() < 0.5 ? 'forest_hazel' : 'forest_holly', px, py, RR(3.5, 7), RR(0, 360), 0.3, 0.12); claim(px, py, 2); }
	else if (z === '4' && roll < 0.1 && ground(px, py, 'S', 0.7) && spaced(px, py, 1.6)) { put('forest_bramble', px, py, RR(1.0, 1.8), RR(0, 360), 0, 0.1); claim(px, py, 1.4); }
	// Stumps: the newcomers' felling at the forest's edge (F1 field corners, F3's cut groves), a few in a hundred cells.
	else if (((z === '1' && roll > 0.993) || (z === '3' && roll > 0.994)) && ground(px, py, 'S', 0.5) && spaced(px, py, 1.2)) { put('forest_stump', px, py, RR(0.5, 1.0), RR(0, 360), 0.5, 0.1); claim(px, py, 1.2); }
}

// 6. Mossy boulders: the Alder's rocky banks and bed edges, scattered through the old wood, the Crystal border's
//    crystal-veined boulders under moss on F3's south edge (y +4,450..+4,600), and the Spine's talus foot in F5.
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 7) for (let x = X0; x < X0 + (N - 1) * STEP; x += 7) {
	const px = x + RR(0, 7), py = y + RR(0, 7), z = zoneAt(px, py);
	if (!LIVING.has(z)) continue;
	const wd = riverAt(px, py), sl = slopeAt(px, py);
	let p = { '1': 0.002, '2': 0.012, '3': 0.008, '4': 0.012, '5': 0.016 }[z];
	if (wd > -1.2 && wd < 3) p = 0.2;                            // the river's boulders at the water's edge
	if (z === '3' && py > 4450) p = 0.06;                        // the Crystal border
	if (z === '5' && sl > 0.45) p = 0.06;                        // talus under the Spine
	if (R() > p || !ground(px, py, 'K', 1.2) || !spaced(px, py, 1.5)) continue;
	claim(px, py, 1.5);
	const big = R() < 0.25;
	put('forest_rock', px, py, big ? RR(1.8, 3.2) : RR(0.6, 1.6), RR(0, 360), 0, 0.25);
}

// 7. Ferns: thick on F2's banks (60% fern ground there), in F4/F5's shade, under F3's beech; patches, not a carpet of
//    copies (a patch seed every ~16 m spawns a clump of 3-7). The Hollow Root's mouths and the leaf pits stay bare.
const FERN = { '1': 0.01, '2': 0.3, '3': 0.14, '4': 0.24, '5': 0.34 };
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 16) for (let x = X0; x < X0 + (N - 1) * STEP; x += 16) {
	const px = x + RR(0, 16), py = y + RR(0, 16), z = zoneAt(px, py);
	let p = FERN[z] ?? 0;
	if (z === '2' && riverAt(px, py) < 12) p = 0.75;
	if (R() > p) continue;
	const n = 3 + Math.floor(R() * 5);
	for (let q = 0; q < n; q++) {
		const fx = px + RR(-4, 4), fy = py + RR(-4, 4);
		if (!ground(fx, fy, 'N', 0.95)) continue;
		put('forest_fern', fx, fy, RR(0.8, 1.6), RR(0, 360), 0, 0.05);
	}
}

// 8. The ground layer (cooks/wild-answers-1116.md 4.1; Aaron 11-16 "do the better forest"): what covers the soil in a
//    real old wood and in Electric Dreams' (no landscape grass: litter, moss and low plants, clustered round what
//    feeds them, never an even carpet), drawn with the Electric Dreams scans (Data/forest_art.json) by AWRDressing::
//    BuildForestGround and written to Data/forest_ground.json. It draws from its own random stream, so the trees above
//    (and every saved harvest index) stay exactly as they were.
//    - Moss: a band of tufts round the foot of every boulder, stump and standing giant, along both flanks of every
//      fallen log, and on the stream banks (moss is where shade, wood and water meet the ground: Electric Dreams hides
//      every contact line this way).
//    - Leaf litter: patches under the broadleaf canopy, thickest where leaves collect: hollows (lower than the ground
//      6 m round), the lee of trunks; thin in the meadows; never on a trail (trodden bare, the Trails look bible).
//    - Ground cover: wood anemone, herbs and young elder at the light gaps' rims and along the streams, in clusters.
let gseed = 77713;
const G2 = () => { gseed = (gseed * 48271) % 2147483647; return gseed / 2147483647; };
const GR = (a, b) => a + (b - a) * G2();
const ground2 = { forest_moss: [], forest_leaves: [], forest_cover: [] };
function lay(kind, x, y, scale, sinkM = 0.02) {
	const nx = -(hAt(x + 1, y) - hAt(x - 1, y)) / 2, ny = -(hAt(x, y + 1) - hAt(x, y - 1)) / 2, n = Math.hypot(nx, ny, 1);
	ground2[kind].push(Math.round(x * 100), Math.round(y * 100), Math.round((hAt(x, y) - sinkM) * 100), Math.round(G2() * 360), +scale.toFixed(2),
		Math.round(nx / n * 1000), Math.round(ny / n * 1000));
}
// What a piece of the ground layer may lie on: the forest's living ground, off the routes, the water and the places
// (the ferns' keep-outs: a place's floor stays its own), not on a cliff.
const layable = (x, y, maxSlope = 0.9) => ground(x, y, 'N', maxSlope);
// Contact moss goes right up to what it grows on (a giant's or a log's own keep-out is where it belongs), but never onto
// a route, into the water or off the land.
const contactOk = (x, y) => !underIsland(x, y) && LIVING.has(zoneAt(x, y)) && hAt(x, y) >= 0.8 && routeAt(x, y) >= 0.4 && riverAt(x, y) >= 0.2 && slopeAt(x, y) <= 1.2;
// A ring of moss tufts round a foot of radius r (m): n tufts, pressed against it.
function mossRing(x, y, r, n, s0, s1) {
	for (let q = 0; q < n; q++) {
		const a = (q / n + GR(-0.35, 0.35) / n) * 2 * Math.PI, rr = r * GR(0.85, 1.12);
		const px = x + Math.cos(a) * rr, py = y + Math.sin(a) * rr;
		if (contactOk(px, py)) lay('forest_moss', px, py, GR(s0, s1), 0.03);
	}
}
// The rocks, stumps and giants as placed above (flat [x, y, z, yaw, scale] in cm).
const each = (kind, fn) => { const a = out[kind]; for (let i = 0; i < a.length; i += 5) fn(a[i] / 100, a[i + 1] / 100, a[i + 4]); };
// A boulder's foot is the scan it draws (instance k draws scan k % N, Data/forest_art.json, fitted to its height at most
// maxFit x the scan): the ring hugs that rock, not the height the generator asked for.
let artRock = null;
try { artRock = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'forest_art.json'), 'utf8')).kinds.forest_rock; } catch { artRock = null; }
{
	const a = out.forest_rock;
	for (let i = 0, k = 0; i < a.length; i += 5, k++) {
		const x = a[i] / 100, y = a[i + 1] / 100, s = a[i + 4], hM = s * REF.boulder / 100;
		let r = Math.max(0.5, hM * 0.85);
		if (artRock && artRock.meshes.length) {
			const m = artRock.meshes[k % artRock.meshes.length];
			const fit = Math.min(s * REF.boulder / m.height, artRock.maxFit || 99);
			r = Math.max(0.35, m.width * fit / 200 * 0.9);
		}
		mossRing(x, y, r, Math.round(6 + r * 3), 1.2, 2.4);
	}
}
each('forest_stump', (x, y, s) => { const r = Math.max(0.35, s * 0.55); mossRing(x, y, r, 6, 1.0, 1.8); });
for (const g of F.giants) if (g.kind === 'giant' || g.kind === 'dead') mossRing(g.x, g.y, g.r * GR(1.25, 1.6), Math.round(14 + g.r * 3), 1.4, 2.6);
for (const l of F.fallen) {
	const [ax, ay] = l.a, [bx, by] = l.b, len = Math.hypot(bx - ax, by - ay), ux = (bx - ax) / len, uy = (by - ay) / len;
	for (let d = 1; d < len; d += 1.4) for (const sd of [-1, 1]) {
		const off = (l.r + GR(0.05, 0.4)) * sd, px = ax + ux * d - uy * off, py = ay + uy * d + ux * off;
		if (G2() < 0.8 && contactOk(px, py)) lay('forest_moss', px, py, GR(1.2, 2.2), 0.03);
	}
}
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 3) for (let x = X0; x < X0 + (N - 1) * STEP; x += 3) {
	const px = x + GR(0, 3), py = y + GR(0, 3), wd = riverAt(px, py);
	if (wd < 0.3 || wd > 2.5 || zoneAt(px, py) !== '2' || G2() > 0.25) continue;
	for (let q = 0; q < 4; q++) { const qx = px + GR(-0.6, 0.6), qy = py + GR(-0.6, 0.6); if (layable(qx, qy, 1.1)) lay('forest_moss', qx, qy, GR(1.0, 2.0), 0.03); }
}
// Leaf litter: a 9 m grid; the zone's share, more in hollows, a patch or two (1-1.7 m scans) at each kept point.
const LITTER = { '1': 0.04, '2': 0.12, '3': 0.3, '4': 0.34, '5': 0.4 };
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 9) for (let x = X0; x < X0 + (N - 1) * STEP; x += 9) {
	const px = x + GR(0, 9), py = y + GR(0, 9), z = zoneAt(px, py);
	let p = LITTER[z] ?? 0;
	if (p <= 0) continue;
	let ring = 0;
	for (let a = 0; a < 8; a++) ring += hAt(px + Math.cos(a * 45 * DEG) * 6, py + Math.sin(a * 45 * DEG) * 6) / 8;
	const hollowM = ring - hAt(px, py);
	if (hollowM > 0.12) p = Math.min(0.85, p + 0.45);
	if (G2() > p || !layable(px, py, 0.6)) continue;
	const n = hollowM > 0.12 ? 3 : 1 + (G2() < 0.4 ? 1 : 0);
	for (let q = 0; q < n; q++) { const qx = px + GR(-1.4, 1.4), qy = py + GR(-1.4, 1.4); if (layable(qx, qy, 0.6)) lay('forest_leaves', qx, qy, GR(0.85, 1.35), 0.01); }
}
// Ground cover in the light: clusters every ~14 m round each gap's rim (inside it, where the sun reaches the floor),
// and along F2's banks.
for (const gp of gaps) {
	const n = Math.max(3, Math.round(2 * Math.PI * gp.r / 14));
	for (let q = 0; q < n; q++) {
		const a = (q / n + GR(-0.3, 0.3) / n) * 2 * Math.PI, rr = gp.r * GR(0.6, 1.02);
		const cx = gp.x + Math.cos(a) * rr, cy = gp.y + Math.sin(a) * rr;
		const k = 4 + Math.floor(G2() * 4);
		for (let j = 0; j < k; j++) { const qx = cx + GR(-1.8, 1.8), qy = cy + GR(-1.8, 1.8); if (layable(qx, qy, 0.7)) lay('forest_cover', qx, qy, GR(0.9, 1.5), 0.02); }
	}
}
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 7) for (let x = X0; x < X0 + (N - 1) * STEP; x += 7) {
	const px = x + GR(0, 7), py = y + GR(0, 7), wd = riverAt(px, py);
	if (wd < 1 || wd > 6 || zoneAt(px, py) !== '2' || G2() > 0.15) continue;
	for (let j = 0; j < 4; j++) { const qx = px + GR(-1.5, 1.5), qy = py + GR(-1.5, 1.5); if (layable(qx, qy, 0.8)) lay('forest_cover', qx, qy, GR(0.9, 1.4), 0.02); }
}
// Wood anemone and herb drifts on the floor of the beech-lime groves and the old oaks (F3, F4): ancient woodland's
// spring carpet, in drifts of 5-8 where a 12 m cell takes one.
for (let y = Y0; y < Y0 + (N - 1) * STEP; y += 12) for (let x = X0; x < X0 + (N - 1) * STEP; x += 12) {
	const px = x + GR(0, 12), py = y + GR(0, 12), z = zoneAt(px, py);
	if ((z !== '3' && z !== '4') || G2() > 0.09) continue;
	const k = 5 + Math.floor(G2() * 4);
	for (let j = 0; j < k; j++) { const qx = px + GR(-2.2, 2.2), qy = py + GR(-2.2, 2.2); if (layable(qx, qy, 0.6)) lay('forest_cover', qx, qy, GR(0.8, 1.3), 0.02); }
}
{
	const gpath = path.join(root, 'Data', 'forest_ground.json');
	const gcounts = Object.fromEntries(Object.entries(ground2).map(([k, v]) => [k, v.length / 7]));
	fs.writeFileSync(gpath, JSON.stringify({
		_comment: 'Generated by Tools/terrain/forest_dressing.mjs (section 8; edit the script, not this file; cooks/wild-answers-1116.md 4.1). The Ancient forest\'s ground layer: rows of [x cm, y cm, z cm, yaw deg, scale, normal x * 1000, normal y * 1000], drawn by AWRDressing::BuildForestGround with the Electric Dreams scans in Data/forest_art.json (instance i draws scan i % N, the scan\'s own size x scale, laid on the slope). Looks only, clients only.',
		counts: gcounts, kinds: ground2,
	}) + '\n');
	console.log(JSON.stringify({ ground: gcounts, groundKB: Math.round(fs.statSync(gpath).size / 1024) }));
}

// ---------------------------------------------------------------- write (the forest's kinds only; the rest stays)
const dressPath = path.join(root, 'Data', 'island_dressing.json');
const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
dress.instances = dress.instances || {};
dress.counts = dress.counts || {};
for (const k of Object.keys(dress.instances)) if (k.startsWith('forest_')) delete dress.instances[k];
for (const k of Object.keys(dress.counts)) if (k.startsWith('forest_')) delete dress.counts[k];
for (const k of Object.keys(out)) { dress.instances[k] = out[k]; dress.counts[k] = out[k].length / 5; }
fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
const counts = Object.fromEntries(Object.entries(out).map(([k, v]) => [k, v.length / 5]));
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, total: Object.values(counts).reduce((a, b) => a + b, 0), counts, gaps: gaps.length, hedgeSeeds: seeds.length, fileKB: Math.round(fs.statSync(dressPath).size / 1024) }));
