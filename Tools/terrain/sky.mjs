// The Sky islands (cooks/biome-sky.md, cooks/build-biome-sky.md): writes
//   Data/lands/sky.json + sky_ground.bin   the Hollow Cape's ground as a runtime heightfield land (WRLand): the Windward
//                                          Cliffs (W1 ground) on the east coast, the downs west to the Flue's crown, the
//                                          Flue's rim (its throat is hidden: the shaft is scenery), camps
//   Data/sky.json                          everything above the ground (parsed by FWRSkyData, WRSkyTypes.cpp): islands,
//                                          sites, kit, lifts, bridges, updrafts, gust lanes, hazards, the Undercloud,
//                                          resources, the sky graph, arenas, routes, the Turning Wind's splines
//   Data/island_dressing.json "pois"       the map's place list (camps, landmarks)
// Run: node Tools/terrain/sky.mjs   (it prints the island rules' check: sizes, gaps, glide reach, POI counts)
// Map metres: x east, y south, z up; game cm = m x 100. Edit this script, not the json.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const X0 = -1150, Y0 = -10100, STEP = 5, N = 611, UNITS = 16;
const SEED = 7711;

// ---------------------------------------------------------------- noise (deterministic)
const hash = (x, y) => { let h = (x * 374761393 + y * 668265263 + SEED * 2147483647) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
	const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
	const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
	return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
const ridged = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, w = 1, n = 0; for (let i = 0; i < oct; i++) { let v = 1 - Math.abs(vnoise(x * f, y * f) * 2 - 1); v *= v; v *= w; w = Math.min(1, v * 2); s += v * a; n += a; a *= 0.5; f *= 2.1; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
const smax = (a, b, k) => { const h = Math.max(0, Math.min(1, 0.5 + 0.5 * (a - b) / k)); return lerp(b, a, h) + k * h * (1 - h); };
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 90717;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
const r1 = v => +v.toFixed(1);
const DEG = Math.PI / 180;
const C = hex => [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16)];

// ---------------------------------------------------------------- the cape's ground (W1 Windward Cliffs + the Flue's crown)
const FLUE = { x: 0, y: -8800, throat: 125, rim: 700, floor: -40 };
// The coasts: the Windward Cliffs (east), the north cliffs, the Chain Cliffs' south shore (the Ocean's Blowhole Shelf is
// beyond, a later lane: open sea until then), and a low west shore where the Fungal surface's ground goes later.
const coastE = y => 1755 + 38 * fbm(y / 300 + 3.1, 1.7) + 26 * Math.exp(-(((y + 8650) / 60) ** 2));   // a cove at the Shard Fall
const coastN = x => -9790 + 46 * fbm(x / 280 + 8.4, 5.2);
const coastS = x => -7955 + 28 * fbm(x / 250 + 1.9, 2.6);
const coastW = y => -1050 + 40 * fbm(y / 260 + 6.6, 0.4);
// Cliff-top heights (m): the Windward Cliffs 150-320 (highest mid-coast), the Chain Cliffs 80-120, the north 80-150.
const topE = y => interp([[-9790, 150], [-9450, 250], [-9050, 320], [-8700, 300], [-8400, 240], [-8150, 190], [-7955, 110]], y) + 14 * fbm(y / 90, 4.4);
const topN = x => interp([[-1050, 70], [0, 120], [900, 150], [1760, 160]], x) + 10 * fbm(x / 80, 9.1);
const topS = x => interp([[-1050, 50], [600, 70], [1300, 95], [1760, 110]], x) + 8 * fbm(x / 70, 2.2);
const topW = () => 55;

function edgeOf(x, y) {
	const dE = coastE(y) - x, dN = y - coastN(x), dS = coastS(x) - y, dW = x - coastW(y);
	let d = dE, top = topE(y), side = 'e';
	if (dN < d) { d = dN; top = topN(x); side = 'n'; }
	if (dS < d) { d = dS; top = topS(x); side = 's'; }
	if (dW < d) { d = dW; top = topW(); side = 'w'; }
	return { d, top, side };
}
// The apron under the cliffs (shingle and fallen levistone) and the face's width (78-84 degrees).
const apronW = y => 18 + 14 * (fbm(y / 120, 6.3) * 0.5 + 0.5);
function ground(x, y) {
	const e = edgeOf(x, y);
	if (e.d < -40) return { h: -18, sea: true, e };
	const r = Math.hypot(x - FLUE.x, y - FLUE.y);
	// The downs: the cliff tops rise gently inland, rolled by long swells and dimpled.
	const inland = Math.max(0, e.d);
	let downs = e.top + Math.min(70, inland * 0.07) + 9 * fbm(x / 210, y / 210) + 3 * fbm(x / 47 + 11, y / 47);
	// The Flue's crown: a broad volcanic dome to its rim at 700 m, crags on its shoulders.
	const dome = FLUE.rim - 460 * Math.pow(sstep(150, 980, r), 0.8) + 22 * ridged(x / 160, y / 160) * sstep(160, 300, r) * (1 - sstep(700, 950, r));
	let h = smax(downs, dome, 60);
	// The rim: a lip that rises 10 m and breaks into the throat.
	if (r < 190) h = lerp(FLUE.rim - 2, FLUE.rim + 10, sstep(125, 160, r)) - 10 * sstep(160, 190, r) + 10 + 3 * ridged(x / 20, y / 20);
	// The cliff: apron, then the face up to the cliff top (the downs carry on above it).
	const aw = apronW(y);
	const faceW = Math.max(14, e.top * 0.18);
	let cliff;
	if (e.d < 0) cliff = -6 + 7 * sstep(-40, 0, e.d);
	else if (e.d < aw) cliff = 1 + 2.5 * (e.d / aw);
	else cliff = 3.5 + (e.top - 3.5) * Math.pow(sstep(aw, aw + faceW, e.d), 0.75);
	if (e.d < aw + faceW) h = Math.min(h, cliff + 4 * ridged(x / 14, y / 14) * sstep(aw, aw + 6, e.d));
	return { h, sea: e.d < 0, e, r, aw, faceW };
}

const SURF = { turf: 0, shingle: 1, shards: 2, thrift: 3, volcanic: 4, spore: 5, path: 6, beach: 7, hidden: 255 };
// The shard fields at the cliff foot: where the islands above shed their keels (the richest under the Shard Fall).
const SHARD_FIELDS = [[-8650, 120], [-8950, 70], [-9300, 80], [-8300, 60], [-9600, 50]];
const HF = new Float32Array(N * N), SURFM = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
	const g = ground(x, y);
	HF[k] = g.h;
	let s = SURF.turf;
	if (g.sea && g.e.d < -30) s = SURF.hidden;
	else if (g.r !== undefined && g.r < FLUE.throat + 8) s = SURF.hidden;           // the throat: the shaft is scenery
	else if (g.r !== undefined && g.r < 230) s = SURF.spore;
	else if (g.e.d < (g.aw ?? 20)) s = g.e.side === 'e' && SHARD_FIELDS.some(([sy, w]) => Math.abs(y - sy) < w) ? SURF.shards : (g.e.side === 'w' ? SURF.beach : SURF.shingle);
	else if (g.h > 420) s = SURF.volcanic;
	else if (g.e.d > (g.aw ?? 20) + (g.faceW ?? 20) && g.e.d < (g.aw ?? 20) + (g.faceW ?? 20) + 22 && g.e.side !== 'w') s = SURF.thrift;
	SURFM[k] = s;
}
const hAt = (x, y) => {
	const fi = (x - X0) / STEP, fj = (y - Y0) / STEP, i = Math.max(0, Math.min(N - 2, Math.floor(fi))), j = Math.max(0, Math.min(N - 2, Math.floor(fj)));
	const tx = Math.min(1, Math.max(0, fi - i)), ty = Math.min(1, Math.max(0, fj - j));
	const a = HF[j * N + i], b = HF[j * N + i + 1], c = HF[(j + 1) * N + i], d = HF[(j + 1) * N + i + 1];
	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
};
const paintPath = (pts, w) => {
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay] = pts[s], [bx, by] = pts[s + 1], L = Math.hypot(bx - ax, by - ay);
		for (let t = 0; t <= L; t += 2) {
			const px = ax + (bx - ax) * t / L, py = ay + (by - ay) * t / L;
			for (let dj = -w; dj <= w; dj += STEP) for (let di = -w; di <= w; di += STEP) {
				if (di * di + dj * dj > w * w) continue;
				const i = Math.round((px + di - X0) / STEP), j = Math.round((py + dj - Y0) / STEP);
				if (i < 0 || j < 0 || i >= N || j >= N) continue;
				const k = j * N + i;
				if (SURFM[k] === SURF.turf || SURFM[k] === SURF.thrift) SURFM[k] = SURF.path;
			}
		}
	}
};

// ---------------------------------------------------------------- camps, lifts (world-map 2.5; build plan 2.5)
const CAMPS = [{ x: 1600, y: -8300 }, { x: 1500, y: -8900 }, { x: 1000, y: -9300 }];
for (const c of CAMPS) c.z = hAt(c.x, c.y);

// ---------------------------------------------------------------- the islands (SKY 2.2 table; centres placed for the rules)
// id, name, zone, x, y, top altitude (m), size (longest axis, m), aspect (long/short), yaw, kind, extras.
// Kinds: turf (green, grazed), garden (terraced beds gone wild), rock (bare granite, bridge islands), temple (white stone
// paving), lagoon (a rain-filled basin), barrow (the Undercloud: wet, grey), crown (Titan's Crown).
const ISLANDS = [
	// W1 the Lift Steps (over the Windward Cliffs; low, green, gentle; Kitewright farm terraces)
	['first_isle', 'the First Isle', 'W1', 1662, -8205, 520, 140, 1.25, 20, 'turf', { terraces: 1 }],
	['still_pool', 'Still Pool Isle', 'W1', 1555, -8880, 560, 120, 1.15, 70, 'turf', { pool: 1 }],
	['terrace_isle', 'Terrace Isle', 'W1', 1080, -9255, 600, 260, 1.5, 35, 'garden', { terraces: 4 }],
	['larder_isle', 'Larder Isle', 'W1', 1690, -8400, 620, 90, 1.2, -30, 'turf', {}],
	['ewe_isle', 'Ewe Isle', 'W1', 1540, -8560, 640, 70, 1.1, 10, 'turf', {}],
	['gap_rock', 'Gap Rock', 'W1', 1600, -8700, 660, 50, 1.3, 55, 'rock', {}],
	['chainfoot', 'Chainfoot', 'W1', 1300, -9030, 690, 110, 1.3, -10, 'turf', {}],
	['tumble', 'Tumble', 'W1', 1440, -8740, 720, 60, 1.4, 80, 'rock', {}],
	['highstep', 'Highstep', 'W1', 1260, -9150, 745, 150, 1.35, 25, 'turf', {}],
	// W2 the Cloud Gardens (over the sea east of the cape)
	['weeping_isle', 'the Weeping Isle', 'W2', 2500, -8600, 880, 420, 1.35, 15, 'garden', { terraces: 3, falls: 3, caves: 3, beard: 60 }],
	['flax_isle', 'Flax Isle', 'W2', 1980, -8340, 640, 200, 1.4, -20, 'garden', { terraces: 3 }],
	['silk_isle', 'Silk Isle', 'W2', 2190, -8180, 700, 180, 1.3, 40, 'garden', {}],
	['rain_lagoon', "Gulper's Bight", 'W2', 2850, -8420, 760, 240, 1.2, 0, 'lagoon', { lagoon: 1 }],
	['kitefield', 'Kitefield', 'W2', 2760, -9010, 820, 300, 1.45, -25, 'turf', { hill: 1 }],
	['berry_isle', 'Berry Isle', 'W2', 2010, -8560, 660, 90, 1.2, 5, 'garden', {}],
	['nets', 'Nets', 'W2', 2160, -8800, 690, 70, 1.3, 60, 'garden', {}],
	['twin_east', 'Twin Pools (east)', 'W2', 3160, -8620, 730, 60, 1.1, 0, 'turf', { pool: 1 }],
	['twin_west', 'Twin Pools (west)', 'W2', 3120, -8710, 730, 55, 1.15, 30, 'turf', { pool: 1 }],
	['moss_stair', 'Moss Stair', 'W2', 2330, -8930, 790, 110, 1.6, -45, 'garden', { terraces: 4 }],
	['drift_rock', 'Drift Rock', 'W2', 3040, -8880, 850, 45, 1.2, 15, 'rock', {}],
	// W3 the Ruined Sky-bridges (east to west along the north, rocky and narrow)
	['eyrie_rock', 'Eyrie Rock', 'W3', 2960, -9480, 900, 120, 1.3, 10, 'rock', {}],
	['tollgate', 'Tollgate', 'W3', 2790, -9690, 795, 160, 1.4, -35, 'rock', {}],
	['winch_isle', 'Winch Isle', 'W3', 2595, -9790, 760, 70, 1.2, 0, 'rock', {}],
	['span_north', 'Span North', 'W3', 2440, -9905, 805, 90, 1.25, -30, 'rock', {}],
	['span_south', 'Span South', 'W3', 2245, -10010, 805, 85, 1.25, -30, 'rock', {}],
	['mane_ledges', 'Mane Ledges', 'W3', 1985, -10075, 960, 200, 1.5, -10, 'rock', { ledges: 1 }],
	['long_deck', 'Long Deck', 'W3', 1745, -10140, 930, 60, 1.8, 0, 'rock', {}],
	['needle', 'Needle', 'W3', 1560, -10120, 990, 40, 1.1, 0, 'rock', { spire: 1 }],
	['last_toll', 'Last Toll', 'W3', 1340, -10090, 1020, 130, 1.3, 15, 'rock', {}],
	['brink', 'Brink', 'W3', 1130, -10030, 1040, 75, 1.35, 40, 'rock', {}],
	// W4 the Floating Shrines (round the Flue; high, cold, bright)
	['lid_gate', 'Lid Gate', 'W4', 0, -8655, 1230, 110, 1.15, 90, 'temple', {}],
	['titans_crown', "Titan's Crown", 'W4', 580, -9260, 1300, 230, 1.3, 30, 'crown', {}],
	['wheel_isle', 'Wheel Isle', 'W4', 300, -8545, 1150, 140, 1.25, -15, 'temple', {}],
	['vane_east', 'Vane Rocks (east)', 'W4', 470, -8420, 1120, 70, 1.4, 35, 'rock', {}],
	['vane_west', 'Vane Rocks (west)', 'W4', 560, -8530, 1120, 60, 1.3, -20, 'rock', {}],
	['rose_temple', 'Rose Temple', 'W4', 60, -9070, 1180, 120, 1.1, 0, 'temple', {}],
	['spore_scar', 'Spore Scar', 'W4', -255, -8620, 1080, 150, 1.3, 50, 'garden', { spore: 1 }],
	['bell_rock', 'Bell Rock', 'W4', 330, -9110, 1260, 50, 1.2, 0, 'temple', {}],
	// W5 the Undercloud (inside the bank at (+2,600, -9,600), 500-600 m)
	['kite_barrow', 'Kite Barrow', 'W5', 2470, -9570, 540, 160, 1.3, 20, 'barrow', {}],
	['tube_isle', 'Tube Isle', 'W5', 2700, -9440, 560, 90, 1.3, -40, 'barrow', {}],
	['bonebell', 'Bonebell', 'W5', 2830, -9640, 520, 70, 1.2, 10, 'barrow', {}],
	['lantern_shelf', 'Lantern Shelf', 'W5', 2660, -9790, 580, 80, 1.4, 70, 'barrow', { overhang: 1 }],
	['drowned_garden', 'the Drowned Garden', 'W5', 2330, -9760, 510, 120, 1.3, -15, 'barrow', { tilt: 30 }],
	['last_kite', 'the Last Kite', 'W5', 2690, -9250, 540, 60, 1.2, 0, 'barrow', {}],
];
const islands = ISLANDS.map(([id, name, zone, x, y, z, size, aspect, yaw, kind, ex], idx) => ({
	id, name, zone, x, y, z, size, aspect, yaw, kind, seed: 101 + idx * 37,
	r: size / 2, rMin: size / 2 / aspect,
	claim: size >= 120, ...ex,
}));
const isl = Object.fromEntries(islands.map(i => [i.id, i]));
// A point on an island's top at local (u along its long axis, v across), as fractions of its half-axes.
const onIsland = (id, u, v) => {
	const I = isl[id], c = Math.cos(I.yaw * DEG), s = Math.sin(I.yaw * DEG), a = u * I.r, b = v * I.rMin;
	return [r1(I.x + a * c - b * s), r1(I.y + a * s + b * c)];
};

// ---------------------------------------------------------------- Aeolith's ring (L1, SKY 2.11)
const RING = { x: FLUE.x, y: FLUE.y, z: 1250, radius: 28, platforms: [] };
for (let p = 0; p < 8; p++) {
	const a = -90 + p * 45;   // platform 1 due south (toward the Lid Gate), going round clockwise seen from above
	RING.platforms.push({ a: (a + 360) % 360 + 90, size: [16, 14, 18, 15, 17, 14, 16, 18][p] });
}
// Platform 1 is the one toward the Lid Gate (south = +y): angle 90 deg.
RING.platforms = RING.platforms.map((p, k) => ({ a: (90 + k * 45) % 360, size: p.size }));

// ---------------------------------------------------------------- the Undercloud (SKY 2.7)
const UNDER = { x: 2600, y: -9600, r: 500, bottom: 500, top: 600, gap: [2700, -9130], gapR: 10 };

// ---------------------------------------------------------------- sites: landmarks, hand sites, shrines, camps, Undercloud
const sites = [];
const site = (id, kind, name, at, extra = {}) => { sites.push({ id, kind, name, x: r1(at[0]), y: r1(at[1]), yaw: extra.yaw ?? 0, island: extra.island ?? null, ...extra }); };
// Camps on the cliff tops.
CAMPS.forEach((c, k) => site(`camp${k + 1}`, 'camp', ['the Chain Cliffs camp', 'the Still Pool camp', 'the Terrace camp'][k], [c.x, c.y], { camp: true, yaw: 0 }));
// Landmarks.
site('L1', 'serpents_ring', "the Serpent's Ring", [RING.x, RING.y], { landmark: true, z: RING.z });
site('L2', 'weeping_falls', 'the Weeping Isle', onIsland('weeping_isle', 0.15, 0.55), { landmark: true, island: 'weeping_isle', yaw: 105 });
site('L3', 'fallen_span', 'the Fallen Span', [(isl.span_north.x + isl.span_south.x) / 2, (isl.span_north.y + isl.span_south.y) / 2], { landmark: true, z: 805 });
site('L4', 'paper_titan', 'the Paper Titan', onIsland('titans_crown', -0.1, 0), { landmark: true, island: 'titans_crown', yaw: 200, hand: 'H15' });
site('L5', 'great_kite', 'the Great Kite', onIsland('titans_crown', 0.55, 0.1), { landmark: true, island: 'titans_crown' });
site('L6', 'chain_cliffs', 'the Chain Cliffs', [1600, -7990], { landmark: true });
// Hand sites (H1-H16; H15 is L4).
const coastFoot = (y) => { const x = coastE(y) - apronW(y) * 0.5; return [x, y]; };
site('H1', 'shard_fall', 'the Shard Fall', coastFoot(-8650), { hand: true, yaw: 0 });
site('H2', 'lift_wreck', 'the Lift Wreck', [1250, -9050], { hand: true, yaw: 30 });
site('H3', 'larder_tree', 'the Larder Tree', onIsland('larder_isle', 0.1, 0.1), { hand: true, island: 'larder_isle' });
site('H4', 'feather_rookery', 'the Feather Rookery', onIsland('ewe_isle', 0, 0), { hand: true, island: 'ewe_isle' });
site('H5', 'lift_station', 'the Lift Station', onIsland('chainfoot', 0.5, 0.2), { hand: true, island: 'chainfoot', yaw: 300, to: onIsland('highstep', -0.55, 0.2), toIsland: 'highstep' });
site('H6', 'cloud_garden', 'the Cloud Garden', onIsland('flax_isle', -0.1, 0), { hand: true, island: 'flax_isle' });
site('H7', 'silk_house', 'the Silk-House', onIsland('silk_isle', 0.05, -0.1), { hand: true, island: 'silk_isle', yaw: 220 });
site('H8', 'kite_field', 'the Kite Field', onIsland('kitefield', 0, 0), { hand: true, island: 'kitefield', yaw: 155 });
site('H9', 'gulpers_bight', "Gulper's Bight", onIsland('rain_lagoon', 0, 0), { hand: true, island: 'rain_lagoon' });
site('H10', 'toll_house', 'the Toll House', onIsland('tollgate', -0.5, 0.1), { hand: true, island: 'tollgate', yaw: 205 });
site('H11', 'harriers_eyrie', "the Harrier's Eyrie", onIsland('eyrie_rock', 0.1, 0), { hand: true, island: 'eyrie_rock' });
site('H12', 'glider_graves', 'the Glider Graves', onIsland('brink', 0.35, 0.1), { hand: true, island: 'brink', yaw: 100 });
site('H13', 'wind_wheel', 'the Wind Wheel', onIsland('wheel_isle', 0.05, 0), { hand: true, island: 'wheel_isle', yaw: 200 });
site('H13b', 'wheel_vanes', 'the Fallen Vanes', [(isl.vane_east.x + isl.vane_west.x) / 2, (isl.vane_east.y + isl.vane_west.y) / 2], { hand: true, island: 'vane_east', yaw: 300 });
site('H14', 'spore_scar', 'the Spore Scar', onIsland('spore_scar', 0, 0), { hand: true, island: 'spore_scar' });
site('H16', 'serpent_door', 'the Serpent Door', onIsland('lid_gate', 0, 0), { hand: true, island: 'lid_gate', yaw: 270 });
// Shrine settings (the shrine stone itself is world.json's; these are the places round it).
site('S_still_pool', 'great_shrine', 'the Great Shrine of the Still Pool', onIsland('still_pool', -0.1, 0), { island: 'still_pool', yaw: 90, shrine: 'sky_still_pool' });
site('S_gardens', 'shrine_gardens', 'the Gardens Shrine', onIsland('moss_stair', 0.45, 0), { island: 'moss_stair', yaw: 0, shrine: 'sky_gardens' });
site('S_toll', 'shrine_toll', 'the Toll Shrine', onIsland('last_toll', 0, 0.1), { island: 'last_toll', yaw: 90, shrine: 'sky_toll' });
site('S_rose', 'shrine_rose', 'the Rose Shrine', onIsland('rose_temple', 0, 0), { island: 'rose_temple', yaw: 0, shrine: 'sky_rose' });
site('S_barrow', 'shrine_barrow', 'the Barrow Shrine', onIsland('kite_barrow', 0.4, 0.2), { island: 'kite_barrow', yaw: 0, shrine: 'sky_barrow' });
// The Undercloud (U1-U8).
site('U1', 'kite_graveyard', 'the Kite Graveyard', onIsland('kite_barrow', -0.15, -0.1), { hand: true, island: 'kite_barrow' });
site('U2', 'bone_tubes', 'the Bone Tubes', onIsland('tube_isle', 0, 0), { hand: true, island: 'tube_isle', yaw: 130 });
site('U3', 'bonebell', 'the Bonebell', onIsland('bonebell', 0, 0), { hand: true, island: 'bonebell' });
site('U4', 'hushwing_roost', 'the Hushwing Roost', onIsland('lantern_shelf', 0.3, 0), { hand: true, island: 'lantern_shelf', yaw: 250 });
site('U5', 'gardeners_grave', "the Gardener's Grave", onIsland('drowned_garden', 0, 0), { hand: true, island: 'drowned_garden', yaw: 60 });
site('U6', 'first_kite', 'the First Kite', onIsland('last_kite', 0, 0), { hand: true, island: 'last_kite' });
site('U7', 'newcomers_kite', "the Newcomer's Kite", onIsland('kite_barrow', 0.7, -0.35), { hand: true, island: 'kite_barrow', yaw: 330 });
site('U8', 'keel_vault', 'the Keel Vault', onIsland('tube_isle', 0.2, 0), { hand: true, island: 'tube_isle', under: true });
// The Weeping Isle's three caves (SKY 2.6, 2.8): mouths in the south lip; the third behind the middle fall.
site('C1', 'weeping_cave', 'the Weeping Isle, the first cave', onIsland('weeping_isle', -0.55, 0.75), { island: 'weeping_isle', yaw: 110, cave: 1 });
site('C2', 'weeping_cave', 'the Weeping Isle, the second cave', onIsland('weeping_isle', 0.6, 0.72), { island: 'weeping_isle', yaw: 100, cave: 2 });
site('C3', 'weeping_cave', 'behind the falls', onIsland('weeping_isle', 0.15, 0.82), { island: 'weeping_isle', yaw: 105, cave: 3, secret: true });

// ---------------------------------------------------------------- kit POIs (SKY 2.4: 37 by kind, then the fill rule)
const kit = [];
const kitAt = (kind, island, u, v, extra = {}) => { const at = island ? onIsland(island, u, v) : [u, v]; kit.push({ id: `K${kit.length + 1}`, kind, x: at[0], y: at[1], island, yaw: Math.round(rnd() * 360), variant: Math.floor(rnd() * 4), ...extra }); };
// Kitewright round houses (6)
kitAt('round_house', 'terrace_isle', 0.35, -0.3, { variant: 0 });
kitAt('round_house', 'tumble', 0, 0, { variant: 2 });
kitAt('round_house', 'berry_isle', -0.3, 0.2, { variant: 1 });
kitAt('round_house', 'long_deck', 0.2, 0, { variant: 3 });
kitAt('round_house', 'highstep', 0.3, 0.25, { variant: 1 });
kitAt('round_house', 'mane_ledges', -0.4, 0.1, { variant: 2 });
// Wind-pole cairns (7): island lips and the ground's cliff road
kitAt('wind_cairn', 'gap_rock', 0.2, 0, { variant: 0 });
kitAt('wind_cairn', 'needle', 0, 0, { variant: 2 });
kitAt('wind_cairn', 'drift_rock', 0, 0, { variant: 1 });
kitAt('wind_cairn', 'nets', -0.55, 0.1, { variant: 0 });
kitAt('wind_cairn', 'bell_rock', 0.1, 0.2, { variant: 2 });
kitAt('wind_cairn', null, 1430, -8460, { variant: 1, ground: true });
kitAt('wind_cairn', null, 1180, -9470, { variant: 0, ground: true });
// Keel outcrops (6): under islands (the node hangs under the rim; reached from a ledge or a hover)
for (const [id, v] of [['terrace_isle', 0], ['weeping_isle', 1], ['mane_ledges', 1], ['tollgate', 0], ['titans_crown', 1], ['spore_scar', 2]]) kitAt('keel_outcrop', id, 0.55, -0.4, { variant: v, under: true });
// Gossamer drifts (3)
kitAt('gossamer', 'nets', 0.2, 0, { variant: 1 });
kitAt('gossamer', 'silk_isle', -0.55, 0.3, { variant: 0 });
kitAt('gossamer', 'moss_stair', -0.35, 0.1, { variant: 0 });
// Galeharrier nests (3)
kitAt('harrier_nest', 'highstep', -0.4, -0.3, { variant: 0 });
kitAt('harrier_nest', 'tollgate', 0.45, -0.1, { variant: 1 });
kitAt('harrier_nest', 'needle', 0.1, 0.4, { variant: 0 });
// Windmane ledges (2)
kitAt('mane_ledge', 'mane_ledges', 0.6, 0.6, { variant: 0 });
kitAt('mane_ledge', 'mane_ledges', 0.1, -0.7, { variant: 1 });
// Waterfall pools (4)
kitAt('fall_pool', 'twin_east', 0, 0, { variant: 0 });
kitAt('fall_pool', 'twin_west', 0, 0, { variant: 1 });
kitAt('fall_pool', 'terrace_isle', -0.45, 0.35, { variant: 0 });
kitAt('fall_pool', 'flax_isle', 0.55, 0.3, { variant: 1 });
// Kill set-pieces (2)
kitAt('kill_scene', 'gap_rock', -0.3, 0.2, { variant: 0 });
kitAt('kill_scene', 'berry_isle', 0.35, -0.2, { variant: 1 });
// Vista stones (2)
kitAt('vista', 'moss_stair', -0.75, 0, { variant: 0 });
kitAt('vista', 'last_toll', 0.55, -0.2, { variant: 1 });
// Hazard teachers (2): W1 a gust lane on the cliff lip with three poles and a 5 m drop onto turf; W2 a sinking stone
kitAt('teacher_gust', null, 1690, -9180, { variant: 0, ground: true });
kitAt('teacher_stone', 'flax_isle', 0.85, -0.2, { variant: 0 });

// ---------------------------------------------------------------- lifts (2.5): each camp's cage up to the island above it
// The headframe stands on the island's lip right over the stage on the cliff top; the chains hang plumb.
function lipPointToward(id, tx, ty, inset = 6) {
	const I = isl[id];
	// March from the centre toward the target until the island's ellipse ends.
	const c = Math.cos(I.yaw * DEG), s = Math.sin(I.yaw * DEG);
	const dx = tx - I.x, dy = ty - I.y, L = Math.hypot(dx, dy) || 1;
	let t = 0;
	for (; t < 1000; t += 0.5) {
		const px = dx / L * t, py = dy / L * t, a = px * c + py * s, b = -px * s + py * c;
		if ((a / I.r) ** 2 + (b / I.rMin) ** 2 > 0.82) break;   // the outline's safe inner edge (its noise stays outside 0.9)
	}
	t = Math.max(0, t - inset);
	return [I.x + dx / L * t, I.y + dy / L * t];
}
const lifts = [];
const LIFT_TO = ['first_isle', 'still_pool', 'terrace_isle'];
CAMPS.forEach((c, k) => {
	const id = LIFT_TO[k], I = isl[id];
	// The stage stands on the cliff top under the island's lip on the camp's side, 30-60 m from the camp.
	const lip = lipPointToward(id, c.x, c.y, 4);
	const stage = [lip[0], lip[1]];
	const z = hAt(stage[0], stage[1]);
	lifts.push({ id: `lift${k + 1}`, camp: `camp${k + 1}`, island: id, foot: [r1(stage[0]), r1(stage[1]), r1(z)], top: I.z });
});
// Paths from each camp to its lift's stage.
for (let k = 0; k < 3; k++) paintPath([[CAMPS[k].x, CAMPS[k].y], [lifts[k].foot[0], lifts[k].foot[1]]], 2.5);
// The cliff road linking the camps (and the Chain Cliffs' head).
const ROAD = [[1600, -8030], [1600, -8300], [1560, -8560], [1500, -8900], [1380, -9080], [1250, -9200], [1000, -9300], [900, -9560]];
paintPath(ROAD, 3);
for (const p of ROAD) p.push(r1(hAt(p[0], p[1])));

// ---------------------------------------------------------------- bridges, chain walks, lines (SKY 2.2-2.4, 2.8, 2.10)
// kind: deck (Kitewright stone deck on chains, standing), planks (plank span on chains), stub (a broken end: a 20-45 m
// stump over nothing), hanging (the Fallen Span's half on its chains), chainwalk (one heavy chain to walk), ladder (a chain
// ladder up an island's flank), line (the Last Kite's silk-and-chain line, climbable).
const bridges = [];
const lip = (a, b, inset = 3) => lipPointToward(a, isl[b].x, isl[b].y, inset);
const bridge = (kind, a, b, ex = {}) => {
	const pa = ex.pa ?? lip(a, b), pb = ex.pb ?? lip(b, a);
	bridges.push({ id: ex.id ?? `${a}-${b}`, kind, a: [r1(pa[0]), r1(pa[1]), ex.za ?? isl[a].z], b: [r1(pb[0]), r1(pb[1]), ex.zb ?? isl[b].z], width: ex.width ?? 4, from: a, to: b, ...ex.more });
};
bridge('deck', 'tollgate', 'span_north', { id: 'sentinel_deck', width: 8, more: { weak: [0.18, 0.34, 0.5, 0.66, 0.82] } });
bridge('planks', 'span_south', 'mane_ledges', { width: 3 });
bridge('planks', 'long_deck', 'needle', { width: 2.5 });
bridge('planks', 'needle', 'last_toll', { width: 2.5 });
bridge('deck', 'last_toll', 'brink', { width: 5 });
bridge('stub', 'mane_ledges', 'long_deck', { width: 3, more: { reach: 0.35 } });        // broken: each end a stub
bridge('stub', 'long_deck', 'mane_ledges', { width: 3, more: { reach: 0.3 } });
bridge('stub', 'eyrie_rock', 'tollgate', { width: 4, more: { reach: 0.4 } });
// The Fallen Span: Span North's half hangs straight down 120 m on its chains; Span South's half is a stub.
bridge('hanging', 'span_north', 'span_south', { id: 'fallen_span', width: 6, more: { hang: 120 } });
bridge('stub', 'span_south', 'span_north', { id: 'fallen_span_stub', width: 6, more: { reach: 0.42 } });
// W1 and W2's garden bridges (planks on chains between the farm islands)
bridge('planks', 'still_pool', 'gap_rock', { width: 2.5 });
bridge('planks', 'ewe_isle', 'larder_isle', { width: 2.5 });
bridge('planks', 'flax_isle', 'silk_isle', { width: 3 });
bridge('planks', 'twin_east', 'twin_west', { width: 2 });
// The Wheel's chain (secret: a 180 m chain walk from Wheel Isle onto the Lid Gate)
bridge('chainwalk', 'wheel_isle', 'lid_gate', { id: 'wheel_chain', width: 0.6 });
// The Lid Gate's chain walk to platform 1 (the arena's way in): to the platform's outer rim.
{
	const p1 = RING.platforms[0], a = p1.a * DEG, pr = RING.radius + p1.size / 2;
	const pb = [RING.x + Math.cos(a) * pr, RING.y + Math.sin(a) * pr];
	bridges.push({ id: 'ring_walk', kind: 'chainwalk', a: [...lipPointToward('lid_gate', pb[0], pb[1], 2).map(r1), isl.lid_gate.z], b: [r1(pb[0]), r1(pb[1]), RING.z], width: 1.2, from: 'lid_gate', to: 'ring' });
}
// Winch Isle's chain ladder back up to the deck's west end (the Sentinel's fall; 20 s climb)
bridges.push({ id: 'winch_ladder', kind: 'ladder', a: [...onIsland('winch_isle', -0.3, 0), isl.winch_isle.z], b: [...lip('span_north', 'winch_isle', 1), isl.span_north.z], width: 0.8, from: 'winch_isle', to: 'span_north' });
// The Last Kite's line: from the Last Kite island up through the bank's top to the Kitefield's lip (a 260-280 m climb)
bridges.push({ id: 'last_kite_line', kind: 'line', a: [...onIsland('last_kite', 0.2, 0), isl.last_kite.z + 1], b: [...lip('kitefield', 'last_kite', 2), isl.kitefield.z], width: 0.3, from: 'last_kite', to: 'kitefield' });
// The Keel Vault's ledge under Tube Isle (chain bolts over the lip lead to it)
bridges.push({ id: 'keel_vault_ledge', kind: 'ladder', a: [...lip('tube_isle', 'bonebell', 1), isl.tube_isle.z], b: [...onIsland('tube_isle', 0.35, 0.3), isl.tube_isle.z - 22], width: 0.8, from: 'tube_isle', to: 'tube_isle' });

// ---------------------------------------------------------------- updrafts (world-map 2.5): ridge lift, island columns, the Flue
const updrafts = [];
updrafts.push({ kind: 'flue', x: FLUE.x, y: FLUE.y, r: 125, lift: 12, base: FLUE.floor, top: 1300 });
// Ridge lift along the Windward Cliffs (only with the wind onto the cliff): segments 200 m long, 60 m deep, over the lip.
for (let y = -9700; y <= -8050; y += 150) {
	const x = coastE(y) - apronW(y) - Math.max(14, topE(y) * 0.18) - 10;
	updrafts.push({ kind: 'ridge', x: r1(x), y: r1(y), r: 75, lift: 8, base: r1(topE(y) - 40), top: r1(topE(y) + 400), normal: [1, 0] });
}
// Island columns: big islands get 3-5 on their windward (east) side, the rest one; every column's top is 150 m over its island.
for (const I of islands) {
	if (I.zone === 'W5') continue;
	const n = I.size > 250 ? 5 : I.size > 150 ? 3 : I.size >= 90 ? 1 : 0;
	for (let k = 0; k < n; k++) {
		const ang = (-60 + (n === 1 ? 60 : k * 120 / (n - 1))) * DEG;   // spread over the east half
		const d = I.r * 1.1 + 25;
		updrafts.push({ kind: 'column', x: r1(I.x + Math.cos(ang) * d), y: r1(I.y + Math.sin(ang) * d), r: 22, lift: 10, base: I.z - 250, top: I.z + 150, island: I.id });
	}
}
// The Undercloud: the gap (+6, keeps the hole open under the Kitefield's lip) and two columns out (+10, 150 m over the bank).
updrafts.push({ kind: 'gap', x: UNDER.gap[0], y: UNDER.gap[1], r: 14, lift: 6, base: 480, top: 640 });
updrafts.push({ kind: 'column', x: 2560, y: -9450, r: 22, lift: 10, base: 480, top: UNDER.top + 150, island: 'kite_barrow' });
updrafts.push({ kind: 'column', x: 2760, y: -9720, r: 22, lift: 10, base: 480, top: UNDER.top + 150, island: 'lantern_shelf' });

// ---------------------------------------------------------------- the island rules' check (printed, not enforced)
const report = [];
const gap = (a, b) => Math.hypot(a.x - b.x, a.y - b.y) - a.rMin * 0.95 - b.rMin * 0.95;
for (const I of islands) if (I.size < 40) report.push(`${I.id} under 40 m`);
for (const I of islands) {
	const near = islands.filter(J => J !== I && J.zone !== 'W5' === (I.zone !== 'W5')).map(J => [J, gap(I, J)]).sort((a, b) => a[1] - b[1])[0];
	if (near && near[1] > 220) report.push(`${I.id}: nearest island ${near[0].id} is ${Math.round(near[1])} m away`);
	// Glide reach: a higher island or a column top within 9 m of glide per 1 m of height (the levistone glider's 5.5:1 is
	// the best; the crude glider's 3:1 is the rule here).
	const fromAbove = islands.some(J => J !== I && J.z > I.z && Math.max(0, gap(I, J)) < (J.z - I.z) * 3)
		|| updrafts.some(u => (u.kind === 'column' || u.kind === 'flue' || u.kind === 'ridge' || u.kind === 'gap') && u.top > I.z && Math.max(0, Math.hypot(u.x - I.x, u.y - I.y) - I.rMin) < (u.top - I.z) * 3);
	if (!fromAbove) report.push(`${I.id}: no glide line from a higher island or a column`);
}

// ---------------------------------------------------------------- the decoration and kit fill rule (on the ground: 125 m)
const poiPts = () => [...sites.map(s => [s.x, s.y]), ...kit.map(k => [k.x, k.y]), ...lifts.map(l => [l.foot[0], l.foot[1]])];
{
	// Along the cliff road: a cairn wherever a point is over 125 m from every POI (world-map rule).
	let added = 0;
	for (let s = 0; s < ROAD.length - 1; s++) {
		const [ax, ay] = ROAD[s], [bx, by] = ROAD[s + 1], L = Math.hypot(bx - ax, by - ay);
		for (let t = 0; t <= L; t += 20) {
			const px = ax + (bx - ax) * t / L, py = ay + (by - ay) * t / L;
			if (poiPts().every(([qx, qy]) => Math.hypot(qx - px, qy - py) > 125)) { kitAt('wind_cairn', null, r1(px + 6), r1(py), { variant: added % 3, ground: true, fill: true }); added++; }
		}
	}
	// Islands: >= 1 POI each, >= 3 on islands over 150 m (tops up with round houses, cairns and pools).
	for (const I of islands) {
		const own = [...sites, ...kit].filter(s => s.island === I.id).length;
		const want = I.size > 150 ? 3 : 1;
		for (let k = own; k < want; k++) kitAt(k % 2 ? 'wind_cairn' : 'round_house', I.id, [-0.5, 0.45, -0.1][k % 3], [0.3, -0.35, 0.5][k % 3], { variant: k % 4, fill: true });
	}
}

// ---------------------------------------------------------------- wind-poles: every 150 m along bridges and on island lips
const poles = [];
for (const b of bridges) {
	if (b.kind === 'line' || b.kind === 'ladder') continue;
	const L = Math.hypot(b.b[0] - b.a[0], b.b[1] - b.a[1]);
	poles.push([b.a[0], b.a[1], b.a[2], Math.round(Math.atan2(b.b[1] - b.a[1], b.b[0] - b.a[0]) / DEG)]);
	if (b.kind !== 'stub' && b.kind !== 'hanging') for (let t = 150; t < L - 30; t += 150) poles.push([r1(lerp(b.a[0], b.b[0], t / L)), r1(lerp(b.a[1], b.b[1], t / L)), r1(lerp(b.a[2], b.b[2], t / L)), Math.round(Math.atan2(b.b[1] - b.a[1], b.b[0] - b.a[0]) / DEG)]);
}

// ---------------------------------------------------------------- gust lanes (SKY 2.5): island lips, bridges, the ground's ridge
// Each lane: points [x, y, z] along it and its drop side (a unit vector, map metres).
const gustLanes = [];
for (const b of bridges) {
	if (!['deck', 'planks'].includes(b.kind)) continue;
	const dx = b.b[0] - b.a[0], dy = b.b[1] - b.a[1], L = Math.hypot(dx, dy);
	// The drop side is downwind (the wind blows from the east: toward -x) across the bridge.
	let nx = -dy / L, ny = dx / L;
	if (nx > 0) { nx = -nx; ny = -ny; }
	gustLanes.push({ id: `lane_${b.id}`, points: [b.a, [r1((b.a[0] + b.b[0]) / 2), r1((b.a[1] + b.b[1]) / 2), r1((b.a[2] + b.b[2]) / 2)], b.b], drop: [r1(nx), r1(ny)], width: b.width / 2 + 3 });
}
for (const I of islands) {
	if (I.zone === 'W5' || I.size < 60) continue;
	// The west (lee) lip: a lane along it, the drop outward.
	const pts = [];
	for (const a of [150, 180, 210]) {
		const ang = a * DEG;
		const t = lipPointToward(I.id, I.x + Math.cos(ang) * 1000, I.y + Math.sin(ang) * 1000, 3);
		pts.push([r1(t[0]), r1(t[1]), I.z]);
	}
	gustLanes.push({ id: `lane_${I.id}`, points: pts, drop: [-1, 0], width: 8, island: I.id });
}
// The W1 teacher: three poles along the cliff lip and a 5 m drop onto turf (a step cut in the lip: see the dressing).
{
	const T = kit.find(k => k.kind === 'teacher_gust');
	gustLanes.push({ id: 'lane_teacher', points: [[T.x, T.y - 18, r1(hAt(T.x, T.y - 18))], [T.x, T.y, r1(hAt(T.x, T.y))], [T.x, T.y + 18, r1(hAt(T.x, T.y + 18))]], drop: [-1, 0], width: 7, teacher: true });
}

// ---------------------------------------------------------------- micro-hazards (SKY 2.5)
// Lee rotors (W1): on the lee (west) side of every Lift Steps island, and over the cliff lips' lee.
const rotors = [];
for (const I of islands.filter(I => I.zone === 'W1')) rotors.push({ x: r1(I.x - I.r - 35), y: I.y, z: I.z - 20, r: 40, h: 70, island: I.id });
// Sinking stones (W2): stepping-stone lines between garden islands (5-9 m stones, 8-14 m apart, a gentle climb).
const stones = [];
const stoneLine = (a, b) => {
	const pa = lip(a, b, 2), pb = lip(b, a, 2), za = isl[a].z, zb = isl[b].z;
	const L = Math.hypot(pb[0] - pa[0], pb[1] - pa[1]), n = Math.max(2, Math.round(L / 11));
	const pts = [];
	for (let k = 1; k < n; k++) { const t = k / n; pts.push([r1(lerp(pa[0], pb[0], t) + 2 * Math.sin(k * 1.7)), r1(lerp(pa[1], pb[1], t) + 2 * Math.cos(k * 2.3)), r1(lerp(za, zb, t) - 0.6), 5 + Math.round(rnd() * 4)]); }
	stones.push({ id: `${a}-${b}`, from: a, to: b, stones: pts });
};
stoneLine('berry_isle', 'nets');
stoneLine('silk_isle', 'weeping_isle');
stoneLine('moss_stair', 'nets');
stoneLine('twin_west', 'drift_rock');
// The teacher stone: one stone 4 m over the turf by Flax Isle's east lip.
{ const T = kit.find(k => k.kind === 'teacher_stone'); stones.push({ id: 'teacher', from: 'flax_isle', to: 'flax_isle', teacher: true, stones: [[T.x, T.y, isl.flax_isle.z + 4, 6]] }); }
// Downbursts (W3): over the glide lines between the bridge islands.
const downbursts = [];
const midOf = (a, b) => [r1((isl[a].x + isl[b].x) / 2), r1((isl[a].y + isl[b].y) / 2), r1(Math.max(isl[a].z, isl[b].z) + 60)];
for (const [a, b] of [['eyrie_rock', 'tollgate'], ['mane_ledges', 'long_deck'], ['span_south', 'mane_ledges'], ['last_toll', 'brink'], ['tollgate', 'span_north']]) downbursts.push({ at: midOf(a, b), r: 15, from: a, to: b });

// ---------------------------------------------------------------- waterfalls (L2 + the pools' falls)
const falls = [];
for (const [u, v, w] of [[-0.25, 1, 14], [0.15, 1, 18], [0.55, 1, 12]]) { const p = onIsland('weeping_isle', u, v * 0.98); falls.push({ island: 'weeping_isle', x: p[0], y: p[1], z: isl.weeping_isle.z - 2, width: w, drop: 250, yaw: isl.weeping_isle.yaw + 90 }); }
for (const [id, u, v, w, d] of [['terrace_isle', -0.6, 0.6, 6, 170], ['flax_isle', 0.7, 0.6, 5, 160], ['twin_east', 0.6, 0.6, 4, 150], ['twin_west', -0.6, 0.6, 3, 150], ['moss_stair', -0.6, 0.7, 4, 180]]) { const p = onIsland(id, u, v * 0.98); falls.push({ island: id, x: p[0], y: p[1], z: isl[id].z - 1, width: w, drop: d, yaw: isl[id].yaw + Math.atan2(v, u) / DEG }); }

// ---------------------------------------------------------------- resources (SKY 2.6, build plan 2.8)
const resources = { levistone_shard: [], keel: [], core: [], gossamer: [], cloudpine: [], iron: [], tin: [], copper: [], windflax: [], cloudberry: [], feather: [], water: [] };
// Shard fields at the cliff foot (W1 ground): nodes along the apron where the fields are.
for (const [sy, w] of SHARD_FIELDS) for (let k = 0; k < Math.round(w / 18); k++) { const y = sy - w + (2 * w) * (k + 0.5) / Math.round(w / 18); const x = coastE(y) - apronW(y) * (0.3 + 0.4 * rnd()); resources.levistone_shard.push([r1(x), r1(y), r1(hAt(x, y))]); }
// Keels under the islands (on the keel outcrops, plus one under every big island)
for (const k of kit.filter(k => k.kind === 'keel_outcrop')) resources.keel.push([k.x, k.y, k.island]);
for (const I of islands) if (I.size >= 150 && I.zone !== 'W5' && !kit.some(k => k.kind === 'keel_outcrop' && k.island === I.id)) { const p = onIsland(I.id, -0.4, 0.5); resources.keel.push([p[0], p[1], I.id]); }
// Cores in the Weeping Isle's caves (2 per cave)
for (const c of sites.filter(s => s.kind === 'weeping_cave')) { resources.core.push([c.x, c.y, 'weeping_isle', c.cave, 0]); resources.core.push([c.x, c.y, 'weeping_isle', c.cave, 1]); }
// Iron and tin in the caves (the deeper the better), copper on the W1 ground
for (const c of sites.filter(s => s.kind === 'weeping_cave')) { resources.iron.push([c.x, c.y, 'weeping_isle', c.cave, 2]); if (c.cave >= 2) resources.tin.push([c.x, c.y, 'weeping_isle', c.cave, 3]); }
for (const [x, y] of [[1230, -8420], [1320, -8690], [1120, -9020], [1420, -9370], [1050, -9620], [1480, -8180]]) resources.copper.push([x, y, r1(hAt(x, y))]);
// Gossamer: on the drifts + on W2 cedars
for (const k of kit.filter(k => k.kind === 'gossamer')) resources.gossamer.push([k.x, k.y, k.island]);
for (const id of ['weeping_isle', 'silk_isle', 'flax_isle', 'kitefield']) { const p = onIsland(id, 0.4, -0.45); resources.gossamer.push([p[0], p[1], id]); }
// Wild Windflax and Cloudberry on W2's terraces (and the Cloud Garden's beds)
for (const id of ['flax_isle', 'weeping_isle', 'moss_stair', 'berry_isle', 'silk_isle']) for (let k = 0; k < 4; k++) { const p = onIsland(id, -0.5 + k * 0.33, 0.25 * ((k % 2) * 2 - 1)); resources[k % 3 === 2 ? 'cloudberry' : 'windflax'].push([p[0], p[1], id]); }
for (const id of ['berry_isle', 'drowned_garden', 'twin_east']) { const p = onIsland(id, 0.3, 0.3); resources.cloudberry.push([p[0], p[1], id]); }
// Cloudpine: a felling tree per island (the island's decoration trees are scenery; these are the axe nodes)
for (const I of islands) if (I.kind !== 'temple' && I.size >= 60) { const p = onIsland(I.id, 0.55, 0.45); resources.cloudpine.push([p[0], p[1], I.id]); if (I.size > 150) { const q = onIsland(I.id, -0.6, -0.3); resources.cloudpine.push([q[0], q[1], I.id]); } }
// Puffwing feathers at the rookery, water at the pools
{ const R = sites.find(s => s.id === 'H4'); for (let k = 0; k < 3; k++) resources.feather.push([r1(R.x + 8 * Math.cos(k * 2.1)), r1(R.y + 8 * Math.sin(k * 2.1)), 'ewe_isle']); }
for (const k of kit.filter(k => k.kind === 'fall_pool')) resources.water.push([k.x, k.y, k.island]);

// ---------------------------------------------------------------- the sky graph (flyers: SKY 2.9 flyer rule)
const nodes = [], edges = [];
for (const I of islands) {
	nodes.push([I.x, I.y, I.z + 30, I.id, 'top']);
	for (let a = 0; a < 360; a += 90) { const p = lipPointToward(I.id, I.x + Math.cos(a * DEG) * 900, I.y + Math.sin(a * DEG) * 900, -18); nodes.push([r1(p[0]), r1(p[1]), I.z - 10, I.id, 'lip']); }
}
for (const u of updrafts) if (u.kind !== 'ridge') nodes.push([u.x, u.y, r1(u.top - 20), u.island ?? u.kind, 'column']);
for (let a = 0; a < nodes.length; a++) for (let b = a + 1; b < nodes.length; b++) {
	const d = Math.hypot(nodes[a][0] - nodes[b][0], nodes[a][1] - nodes[b][1], nodes[a][2] - nodes[b][2]);
	if (d < 420 && (nodes[a][3] !== nodes[b][3] || d > 30)) edges.push([a, b]);
}

// ---------------------------------------------------------------- arenas and routes (SKY 2.10, 2.11)
const lidFront = lipPointToward('lid_gate', RING.x, RING.y, 8);
const arenas = {
	aeolith: { x: RING.x, y: RING.y, z: RING.z, radius: 90, runback: [r1(isl.lid_gate.x + 12), r1(isl.lid_gate.y + 20), isl.lid_gate.z], altar: [r1(isl.lid_gate.x - 10), r1(isl.lid_gate.y), isl.lid_gate.z], door: [r1(lidFront[0]), r1(lidFront[1]), isl.lid_gate.z] },
	bridge_sentinel: { x: isl.tollgate.x, y: isl.tollgate.y, z: isl.tollgate.z, radius: 65, runback: [...onIsland('tollgate', 0.7, 0.2), isl.tollgate.z], fallTo: [isl.winch_isle.x, isl.winch_isle.y, isl.winch_isle.z] },
	old_gulper: { x: isl.rain_lagoon.x, y: isl.rain_lagoon.y, z: isl.rain_lagoon.z + 4, radius: 65, runback: [...onIsland('rain_lagoon', -0.95, 0), isl.rain_lagoon.z] },
	kyrra: { x: isl.eyrie_rock.x, y: isl.eyrie_rock.y, z: isl.eyrie_rock.z, radius: 65 },
};
// Kyrra's loop over W3 (2.4 km): Tollgate to Brink and back over Eyrie Rock, 60-120 m over the islands.
const KYRRA = ['eyrie_rock', 'tollgate', 'span_north', 'span_south', 'mane_ledges', 'long_deck', 'last_toll', 'brink', 'last_toll', 'mane_ledges', 'winch_isle', 'eyrie_rock'];
const kyrraRoute = KYRRA.map((id, k) => [isl[id].x, isl[id].y, isl[id].z + 70 + 30 * Math.sin(k)]);
const kyrraLand = ['mane_ledges', 'tollgate', 'last_toll'];

// ---------------------------------------------------------------- the Turning Wind (SKY 2.15)
// Drift splines for islands under 120 m: 40-120 m downwind-reversed (east, the event's wind is from the west), then hold.
const drift = [];
for (const I of islands) {
	// Chained islands hold: a bridge, a lift, a chain walk or a stepping-stone line ends on them (SKY 2.15: the free ones drift).
	if (I.claim || I.zone === 'W5' || bridges.some(b => b.from === I.id || b.to === I.id) || stones.some(s => s.from === I.id || s.to === I.id) || lifts.some(l => l.island === I.id) || ['vane_east', 'vane_west', 'chainfoot'].includes(I.id)) continue;   // the Fallen Vanes lie across both Vane Rocks; Chainfoot's lift station faces its twin
	const len = 40 + Math.round(rnd() * 80), ang = (-25 + rnd() * 50) * DEG;   // eastward, +-25 deg
	drift.push({ island: I.id, to: [r1(I.x + Math.cos(ang) * len), r1(I.y + Math.sin(ang) * len), I.z + Math.round(rnd() * 12 - 6)] });
}
const cloudBridges = [['first_isle', 'highstep'], ['flax_isle', 'weeping_isle'], ['kitefield', 'gap'], ['tollgate', 'last_toll'], ['last_toll', 'rose_temple'], ['wheel_isle', 'lid_gate']].map(([a, b]) => {
	const A = isl[a], B = b === 'gap' ? { x: UNDER.gap[0], y: UNDER.gap[1], z: UNDER.top + 2 } : isl[b];
	const pa = lipPointToward(a, B.x, B.y, 2), pb = b === 'gap' ? [B.x, B.y] : lipPointToward(b, A.x, A.y, 2);
	return { from: a, to: b, a: [r1(pa[0]), r1(pa[1]), A.z], b: [r1(pb[0]), r1(pb[1]), B.z], width: 4 };
});
const shardFalls = [];
for (let k = 0; k < 20; k++) { const I = islands.filter(i => i.zone !== 'W5')[Math.floor(rnd() * 38)]; const p = onIsland(I.id, rnd() * 1.2 - 0.6, rnd() * 1.2 - 0.6); shardFalls.push([p[0], p[1], I.id]); }

// ---------------------------------------------------------------- secrets and lore (SKY 2.7, 2.8, 2.14)
const secrets = [
	{ id: 'keel_vault', kind: 'cache', at: sites.find(s => s.id === 'U8') && [isl.tube_isle.x, isl.tube_isle.y], island: 'tube_isle', under: 22, gives: { levistone_core: 6, kitewright_glaive: 1 } },
	{ id: 'third_cave', kind: 'cave', island: 'weeping_isle', site: 'C3' },
	{ id: 'wind_rose', kind: 'rose', island: 'still_pool', gives: { bronze_ingot: 10, bp_gale_arrow: 1 } },
	{ id: 'bonebell_echo', kind: 'echo', island: 'bonebell', gives: { talisman_last_kite: 1 } },
	{ id: 'wheel_chain', kind: 'walk', bridge: 'wheel_chain' },
	{ id: 'pearl_lagoon', kind: 'cache', island: 'rain_lagoon', depth: 12, gives: { bronze_ingot: 8, levistone: 6, skysilk: 2 } },
];
const lore = [
	['kitewright_bowl', 'H1'], ['kite_of_the_given', 'H2'], ['bone_tube', 'H8'], ['wheelwrights_plan', 'H13'], ['gardeners_mask', 'H14'],
	['unfinished_kite', 'U5'], ['serpent_anchor_rubbing', 'U6'], ['bone_tube', 'U2'],
];

// ---------------------------------------------------------------- write Data/sky.json
const out = {
	_comment: 'Generated by Tools/terrain/sky.mjs (edit the script, not this file). The Sky islands above the ground (cooks/biome-sky.md, cooks/build-biome-sky.md), parsed by FWRSkyData (WRSkyTypes.cpp). Map metres (x east, y south, z up; game cm = m x 100). islands: centre, top altitude, size (longest axis), aspect (long/short), yaw, kind, seed (the island is built in code from these: WRSkyIsland). sites/kit: x, y on their island (z comes from the island) or on the ground. lifts: the cage\'s stage on the cliff top and the island it rises to. bridges: kind deck/planks/stub/hanging/chainwalk/ladder/line, ends [x,y,z]. updrafts: kind flue/ridge/column/gap, centre, radius, lift (m/s), base and top (m). gustLanes: points and the drop side. rotors, stones, downbursts: the micro-hazards. undercloud: the bank. resources: node points. graph: the flyers\' nodes [x,y,z,island,kind] and edges. arenas, kyrra, event (drift, cloudBridges, shardFalls), secrets, lore.',
	frame: { x0: -700, y0: -10450, x1: 3700, y1: -7950 },
	flue: FLUE,
	islands: islands.map(I => ({ id: I.id, name: I.name, zone: I.zone, x: I.x, y: I.y, z: I.z, size: I.size, aspect: I.aspect, yaw: I.yaw, kind: I.kind, seed: I.seed, claim: I.claim, terraces: I.terraces ?? 0, falls: I.falls ?? 0, caves: I.caves ?? 0, beard: I.beard ?? 0, pool: I.pool ?? 0, lagoon: I.lagoon ?? 0, hill: I.hill ?? 0, tilt: I.tilt ?? 0, spore: I.spore ?? 0, spire: I.spire ?? 0, overhang: I.overhang ?? 0, ledges: I.ledges ?? 0 })),
	ring: RING,
	undercloud: UNDER,
	camps: CAMPS.map(c => [c.x, c.y, r1(c.z)]),
	road: ROAD,
	sites, kit, lifts, bridges, poles, updrafts, gustLanes, rotors, stones, downbursts, falls, resources,
	graph: { nodes, edges },
	arenas,
	kyrra: { route: kyrraRoute.map(p => p.map(r1)), lands: kyrraLand },
	event: { drift, cloudBridges, shardFalls },
	secrets, lore,
};
fs.writeFileSync(path.join(root, 'Data', 'sky.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- write the land (Data/lands/sky.json + sky_ground.bin)
const ZC = 20, ZN = Math.ceil((N - 1) * STEP / ZC);
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) {
		const x = X0 + (i + 0.5) * ZC, y = Y0 + (j + 0.5) * ZC;
		const r = Math.hypot(x - FLUE.x, y - FLUE.y);
		row += x < 950 + 60 * fbm(y / 200, 1.3) ? (r < 980 ? 'f' : 'd') : 'w';
	}
	zoneRows.push(row);
}
{
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'sky_ground.bin'), Buffer.concat([hb, sb]));
	const mid = [X0 + (N - 1) * STEP / 2, Y0 + (N - 1) * STEP / 2];
	const campRows = CAMPS.map(c => [c.x, c.y, 270]);
	const land = {
		_comment: 'Generated by Tools/terrain/sky.mjs (edit the script, not this file). The Hollow Cape\'s ground as a runtime land (WRLand, cooks/build-biome-sky.md 2.1-2.2): the Windward Cliffs (W1 ground: turf to the lip, a 75-85 degree granite face, a shingle-and-shard apron at the sea), the downs west to the Flue\'s crown (a volcanic dome to its rim at 700 m; the throat is hidden here and built as the shaft\'s scenery), the Chain Cliffs\' south shore and a low west shore where the Fungal surface\'s ground goes later (zone f/d: the Flue\'s crown and the downs west of x 950 are the Fungal lane\'s to replace). Heightfield: 5 m samples, uint16 heights = offset + m * unitsPerMetre, then a surface byte each (255 hidden: sea and the throat). camps: PlayerStarts tagged sky_islands (the Wind path), a safe zone each, the stations at camp 2.',
		id: 'sky', biome: 'sky_islands', name: 'the Hollow Cape', priority: 0,
		origin: [mid[0] * 100, mid[1] * 100, 0], mapCentre: mid,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 1200000, chunksPerFrame: 4,
		seaFloor: -18,
		heightfield: { file: 'sky_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 60,
		zones: [
			{ id: 'W1', letter: 'w', name: 'the Windward Cliffs', ground: C('#8FA77A'), groundAlt: C('#9DB59A'), rough: 0.9, speck: { colors: [C('#C9A9C2'), C('#E8E2D8')], amount: 0.05, scale: 3 } },
			{ id: 'flue', letter: 'f', name: "the Flue's crown", ground: C('#6E6A62'), groundAlt: C('#8A8A80'), rough: 0.88, speck: { colors: [C('#4E4A46')], amount: 0.12, scale: 4 } },
			{ id: 'downs', letter: 'd', name: 'the Cape downs', ground: C('#86A06E'), groundAlt: C('#7C9466'), rough: 0.9, speck: { colors: [C('#D8B24A')], amount: 0.03, scale: 4 } },
		],
		surfaces: [
			{ id: 'shingle', class: SURF.shingle, color: C('#9A9690'), rough: 0.82, speck: { colors: [C('#C9C4B6'), C('#5E5A55')], amount: 0.25, scale: 2 } },
			{ id: 'shards', class: SURF.shards, color: C('#8F959A'), rough: 0.5, speck: { colors: [C('#A9C4D9'), C('#CFE9FF')], amount: 0.35, scale: 2 } },
			{ id: 'thrift', class: SURF.thrift, color: C('#8FA77A'), rough: 0.9, speck: { colors: [C('#D98FB8'), C('#E8E2D8')], amount: 0.3, scale: 1.5 } },
			{ id: 'volcanic', class: SURF.volcanic, color: C('#5E5A55'), rough: 0.88, speck: { colors: [C('#8A8A80'), C('#3E3B38')], amount: 0.2, scale: 3 } },
			{ id: 'spore', class: SURF.spore, color: C('#8A8A80'), rough: 0.9, speck: { colors: [C('#39E6C2'), C('#B9B9AE')], amount: 0.08, scale: 2 } },
			{ id: 'path', class: SURF.path, color: C('#A89C86'), rough: 0.92 },
			{ id: 'beach', class: SURF.beach, color: C('#B9AE96'), rough: 0.9 },
		],
		slopeRock: { color: C('#9A9690'), fromDeg: 36, toDeg: 50, ledge: C('#8FA77A') },
		bands: [],
		underground: { letters: '', ways: [] },
		buildFirst: campRows.map(([x, y]) => [x, y]),
		camps: { tag: 'sky_islands', safeRadius: 40, list: campRows, stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'sky.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}

// ---------------------------------------------------------------- the map's place list
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.sky);
		for (const s of sites) {
			const kind = s.camp ? 'camp' : s.landmark ? 'landmark' : null;
			if (!kind) continue;
			const z = s.island ? isl[s.island].z : (s.z ?? hAt(s.x, s.y));
			dress.pois.push({ name: s.name, kind, x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(z * 100), biome: 'sky_islands', sky: true });
		}
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

console.error(`islands ${islands.length}, sites ${sites.length}, kit ${kit.length}, bridges ${bridges.length}, updrafts ${updrafts.length}, lanes ${gustLanes.length}, poles ${poles.length}, graph ${nodes.length}/${edges.length}`);
console.error(`lifts: ${lifts.map(l => `${l.island} from ${l.foot[2]} m to ${l.top} m (${Math.round(l.top - l.foot[2])} m)`).join('; ')}`);
for (const k of Object.keys(resources)) console.error(`  ${k}: ${resources[k].length}`);
if (report.length) console.error('ISLAND RULES:\n  ' + report.join('\n  ')); else console.error('island rules: all hold');
