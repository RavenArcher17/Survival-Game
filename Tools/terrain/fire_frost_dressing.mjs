// The round island's Volcanic and Frozen tundra dressed as their design says (cooks/fire-frost 3.3.1 / 3.4.1; Raven 11-20
// "make sure they look like they are supposed to"): each sub-zone's trees, scrub and rocks, its lava that burns and its
// ice that slips. Reads the finished ground (run after round_ground.mjs) and writes Data/wildreach_dressing.json, which
// UWRDressingSubsystem loads on L_Wildreach (the same builder, kinds, harvests and ledger as the Old Island's dressing).
// Also Data/wildreach_places.json (the places), wildreach_world.json (villages, shrines) and the dressing's pois.
// Also Data/wildreach_life.json: the creature dens per sub-zone and the roaming minis' loops.
// And the whole island's coast (every land's beaches, round 7) and the meadow lake's banks: pebbles, wrack, drift logs,
// marram grass, stranded ice.
//   node Tools/terrain/fire_frost_dressing.mjs
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import { fbm, coastR, trunks } from "./round_island.mjs";
import * as FF from "./fire_frost.mjs";
import * as HP from "./hearthmoot_plan.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const read = (p) => JSON.parse(fs.readFileSync(path.join(root, p), "utf8"));
const LAND = read("Data/lands/island.json"), FFL = read("Tools/terrain/fire_frost_layout.json");
const FEAT = read("Saved/RoundIsland/fire_frost_features.json");
const HF = LAND.heightfield, N = HF.samples, STEP = HF.step, X0 = HF.x0, Y0 = HF.y0;
const BIN = zlib.inflateSync(fs.readFileSync(path.join(root, "Data/lands", HF.file)));
const hAt = (x, y) => {
	const fx = Math.max(0, Math.min(N - 1.001, (x - X0) / STEP)), fy = Math.max(0, Math.min(N - 1.001, (y - Y0) / STEP)), i = Math.floor(fx), j = Math.floor(fy), tx = fx - i, ty = fy - j;
	const s = (c, r) => { const k = r * N + c; return ((BIN[k * 2] | (BIN[k * 2 + 1] << 8)) - HF.offset) / HF.unitsPerMetre; };
	return s(i, j) * (1 - tx) * (1 - ty) + s(i + 1, j) * tx * (1 - ty) + s(i, j + 1) * (1 - tx) * ty + s(i + 1, j + 1) * tx * ty;
};
const surfAt = (x, y) => BIN[N * N * 2 + Math.round((y - Y0) / STEP) * N + Math.round((x - X0) / STEP)];
const slopeAt = (x, y) => Math.atan(Math.hypot(hAt(x + 3, y) - hAt(x - 3, y), hAt(x, y + 3) - hAt(x, y - 3)) / 6) * 180 / Math.PI;
const G = LAND.zoneGrid;
const LETTER_ZONE = Object.fromEntries(LAND.zones.map((z) => [z.letter, z.id]));
const zoneAt = (x, y) => { const r = G.rows[Math.floor((y - G.y0) / G.cell)]; return r ? LETTER_ZONE[r[Math.floor((x - G.x0) / G.cell)]] : undefined; };
const SURF = Object.fromEntries(LAND.surfaces.map((s) => [s.id, s.class]));

// ---- where nothing grows or stands ----------------------------------------------------------------------------------
const BARE = new Set(["road", "main_road", "plaza", "lake_bed", "cobble", "lava", "lava_crust", "lake_ice", "ice", "crevasse", "acid", "bed", "bank"].map((k) => SURF[k]).filter((v) => v !== undefined));
const clear = [];   // [x, y, r]
const keep = (p, r) => p && clear.push([p[0], p[1], r]);
for (const id of ["volcanic", "tundra"]) {
	const L = FFL.lands[id];
	keep(L.village, 150); keep(L.mouthShrine, 30); keep(L.champion, 30);
	for (const c of L.camps) keep(c, 55);
	for (let i = 0; i < L.road.length; i++) keep(L.road[i], 9);
}
for (const k of ["places", "sites", "caves", "shrines"]) for (const o of FFL[k] || []) if (Array.isArray(o.at)) keep(o.at, k === "sites" ? 45 : 28);
for (const o of FFL.kit || []) if (Array.isArray(o.at)) keep(o.at, 10);
keep(FEAT.volcano, 150);   // Vulkarn's crater floor
for (const t of trunks) keep(t.fort, 42);   // (round 9: the Tundra's spruces grew in the south-east fort's yard)
for (const h of FFL.hidden || []) for (let i = 1; i < h.route.length; i++) { const [ax, ay] = h.route[i - 1], [bx, by] = h.route[i], L = Math.hypot(bx - ax, by - ay); for (let t = 0; t <= L; t += 10) keep([ax + (bx - ax) * t / L, ay + (by - ay) * t / L], 14); }
const lava = LAND.lava.river;
const CELL = 40, buckets = new Map();
for (const [x, y, r] of clear) {
	for (let i = Math.floor((x - r) / CELL); i <= Math.floor((x + r) / CELL); i++) for (let j = Math.floor((y - r) / CELL); j <= Math.floor((y + r) / CELL); j++) {
		const k = i + "," + j; if (!buckets.has(k)) buckets.set(k, []); buckets.get(k).push([x, y, r]);
	}
}
const isClear = (x, y, pad) => !(buckets.get(Math.floor(x / CELL) + "," + Math.floor(y / CELL)) || []).some(([cx, cy, r]) => Math.hypot(x - cx, y - cy) < r + pad);
const lavaDist = (x, y) => { let d = 1e9; for (let i = 1; i < lava.length; i++) { const [ax, ay] = lava[i - 1], [bx, by] = lava[i], dx = bx - ax, dy = by - ay, t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy || 1))); d = Math.min(d, Math.hypot(x - ax - t * dx, y - ay - t * dy)); } return d; };

// ---- what each sub-zone grows (per hectare; trees clump, rocks scatter) ----------------------------------------------
// [kind, per ha, scale min, scale max, steepest ground (deg), clumped, steep-only (cliffs), surfaces it keeps to,
//  group: [fewest, most, nearest m, furthest m] lesser ones round each (seracs stand in broken clusters, 11-22)]
const T = (kind, ha, s0, s1, slope = 28, clump = 0, only = null, group = null) => ({ kind, ha, s0, s1, slope, clump, only, group });
const ON = (...ids) => new Set(ids.map((k) => SURF[k]));
const PLAN = {
	V1: [T("char_tree", 1.2, 0.8, 1.2, 26, 0.6), T("ember_shrub", 5, 0.7, 1.2, 30, 0.5), T("basalt_rock", 1.5, 0.6, 1.5, 38), T("pumice_rock", 1.5, 0.5, 1.3, 34), T("lava_rubble", 6, 0.6, 1.1, 16, 0.7)],
	V2: [T("basalt_rock", 3, 0.6, 1.6, 38), T("ember_shrub", 1.5, 0.7, 1.1, 30, 0.5), T("char_tree", 0.4, 0.8, 1.2, 26, 0.6),
		T("pine", 30, 0.7, 1.15, 26, 0.4, ON("kipuka")), T("kipuka_shrub", 14, 0.6, 1.1, 30, 0.4, ON("kipuka")), T("lava_rubble", 80, 0.6, 1.3, 16, 0.6)],
	V3: [T("obsidian_blade", 6, 0.7, 1.5, 45, 0.5), T("basalt_rock", 2, 0.6, 1.4, 40), T("lava_rubble", 40, 0.6, 1.2, 16, 0.6)],
	V4: [T("basalt_rock", 0.8, 0.5, 1.2, 36), T("ember_shrub", 0.5, 0.6, 1.0, 28, 0.5)],
	V5: [T("basalt_rock", 2, 0.6, 1.5, 40), T("basalt_column", 0.6, 0.8, 1.5, 30, 0.6), T("char_tree", 0.3, 0.8, 1.1, 24, 0.6), T("lava_rubble", 60, 0.6, 1.3, 16, 0.6)],
	V6: [T("ember_shrub", 3, 0.7, 1.2, 30, 0.5), T("basalt_rock", 0.8, 0.6, 1.3, 38), T("char_tree", 0.4, 0.8, 1.2, 26, 0.6),
		T("pine", 1.5, 0.7, 1.1, 26, 0.7, ON("cinder_grass")), T("kipuka_shrub", 2, 0.6, 1.0, 30, 0.5, ON("cinder_grass")), T("lava_rubble", 12, 0.6, 1.1, 16, 0.7)],
	V7: [T("char_tree", 28, 0.9, 1.6, 30, 0.45), T("ember_shrub", 6, 0.7, 1.2, 30, 0.5), T("basalt_rock", 0.4, 0.6, 1.2, 38)],
	T1: [T("granite_rock", 1.2, 0.8, 2.2, 38), T("frost_shrub", 1.5, 0.6, 1.0, 30, 0.5), T("ice_boulder", 0.4, 0.6, 1.3, 38)],
	T2: [T("snow_pine", 32, 0.75, 1.3, 30, 0.45), T("frost_shrub", 5, 0.6, 1.1, 30, 0.5), T("granite_rock", 0.4, 0.7, 1.6, 38)],
	T3: [T("snow_pine", 3, 0.7, 1.2, 26, 0.7), T("frost_shrub", 5, 0.5, 0.9, 30, 0.5), T("ice_boulder", 0.8, 0.6, 1.4, 38)],
	T4: [T("serac", 0.7, 0.8, 1.45, 45, 0.6, null, [2, 5, 3.5, 9]), T("ice_boulder", 2.5, 0.6, 1.6, 40)],
	T5: [T("ice_boulder", 1.5, 0.6, 1.5, 40), T("serac", 0.18, 0.8, 1.35, 40, 0.6, null, [1, 4, 3.5, 8]), T("basalt_rock", 6, 0.6, 1.4, 40, 0, ON("basalt"))],
	// (round 14: green pines on the Thaw and the Steppe: the snow-laden spruce stood on their green grass)
	T6: [T("frost_shrub", 7, 0.5, 1.0, 30, 0.5), T("pine", 1.2, 0.7, 1.1, 26, 0.7), T("granite_rock", 0.6, 0.7, 1.6, 38)],
	T7: [T("granite_rock", 1.2, 0.8, 2.4, 38), T("frost_shrub", 0.8, 0.6, 1.0, 30, 0.5), T("pine", 0.2, 0.8, 1.1, 24, 0.8)],
};
// A hash random per cell (the same on every run).
const rand = (i, j, k) => { let h = (Math.imul(i, 374761393) + Math.imul(j, 668265263) + Math.imul(k, 2147483647)) | 0; h = Math.imul(h ^ (h >>> 13), 1274126177); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const out = {}, counts = {};
const add = (kind, x, y, z, yaw, s) => { (out[kind] ||= []).push(Math.round(x * 100), Math.round(y * 100), Math.round(z * 100), Math.round(yaw), +s.toFixed(2)); counts[kind] = (counts[kind] || 0) + 1; };
const C = 10;   // m: one try per kind per 10 m cell (0.01 ha)
const xs = [], ys = [];
for (const id of ["volcanic", "tundra"]) for (const p of FFL.lands[id].road) { xs.push(p[0]); ys.push(p[1]); }
for (let gx = -6600; gx < 6600; gx += C) {
	for (let gy = -6600; gy < 6600; gy += C) {
		const zc = zoneAt(gx + C / 2, gy + C / 2), plan = zc && PLAN[zc];
		if (!plan) continue;
		const ci = Math.round(gx / C), cj = Math.round(gy / C);
		for (let p = 0; p < plan.length; p++) {
			const P = plan[p];
			// Clumped kinds grow in groves: the noise thins them out between and packs them in (0 .. ~2x).
			const grove = P.clump ? Math.max(0, Math.min(2.2, 1 + 2.2 * fbm(gx / 140, gy / 140, 600 + p, 3) / P.clump * 0.5)) : 1;
			const lambda = P.ha / 100 * grove;
			const tries = Math.floor(lambda) + (rand(ci, cj, p * 7 + 1) < lambda - Math.floor(lambda) ? 1 : 0);
			for (let t = 0; t < tries; t++) {
				const x = gx + rand(ci, cj, p * 7 + 2 + t * 13) * C, y = gy + rand(ci, cj, p * 7 + 3 + t * 13) * C;
				if (zoneAt(x, y) !== zc) continue;
				const h = hAt(x, y);
				if (h < 1.2) continue;
				const sf = surfAt(x, y), sl = slopeAt(x, y);
				if (P.only === "steep") { if (sl < 32) continue; }
				else if (P.only) { if (!P.only.has(sf)) continue; }
				else if (BARE.has(sf) || sf === SURF.kipuka && P.kind !== "pine" && P.kind !== "kipuka_shrub") continue;
				if (P.only !== "steep" && sl > P.slope) continue;
				if (!isClear(x, y, P.kind === "pine" || P.kind === "snow_pine" || P.kind === "char_tree" ? 3 : 1.5)) continue;
				const ld = lavaDist(x, y);
				if (ld < LAND.lava.width / 2 + (P.kind === "char_tree" || P.kind === "pine" || P.kind === "snow_pine" ? 18 : 8)) continue;
				const s = P.s0 + (P.s1 - P.s0) * rand(ci, cj, p * 7 + 4 + t * 13);
				add(P.kind, x, y, h, rand(ci, cj, p * 7 + 5 + t * 13) * 360, s);
				if (P.group) {
					// lesser ones crowded round it, leaning their own ways
					const [g0, g1, r0, r1] = P.group, R = (m, k) => rand(ci, cj, p * 7 + 60 + t * 13 + m * 7 + k);
					const n = g0 + Math.floor(R(0, 0) * (g1 - g0 + 1));
					for (let m = 1; m <= n; m++) {
						const a = R(m, 1) * 2 * Math.PI, d = r0 + (r1 - r0) * R(m, 2), x2 = x + Math.cos(a) * d, y2 = y + Math.sin(a) * d;
						if (zoneAt(x2, y2) !== zc || hAt(x2, y2) < 1.2 || BARE.has(surfAt(x2, y2)) || slopeAt(x2, y2) > P.slope || !isClear(x2, y2, 1.5)) continue;
						if (lavaDist(x2, y2) < LAND.lava.width / 2 + 8) continue;
						add(P.kind, x2, y2, hAt(x2, y2), R(m, 3) * 360, s * (0.5 + 0.42 * R(m, 4)));
					}
				}
			}
		}
	}
}
// ---- cliffs: the Tundra's steep rock faces covered in panels of jointed granite (11-22 round 6) ------------------------
// The 4 m ground drew a cliff as its texture stretched down a smooth wall. The Tundra's ground steeper than CLIFF_DEG,
// where it isn't ice, gets panels (SM_Crag, 10 m across) by the area of its face, one to every CLIFF_AREA m2 of it (a
// near-vertical face has many times the face of its footprint: spaced over the map, a 55 m wall got a row every 40 m up
// it), each facing straight down the slope, its point on the face (the game sets it there by its middle and leans it back
// toward the slope: WRWorldDressing's Laid). (Round 14: the steep ice grows out of the land as fluted ice walls instead,
// WRLandCaves LcWallPush; the Volcanic's panels, laid close up every ridge, read as brick walls: its steep ground is
// carved into gullies by round_ground.mjs and drawn as layered rock by M_IslandGround.)
const CLIFF_DEG = 40, CLIFF_STEP = 3, CLIFF_AREA = 55, CLIFF_AREA_ICE = 38;
{
	// Their own clearings: a place stands on level ground, so a cliff panel need only keep 16 m from it (the dressing's
	// 28 m bared the wall behind the Frozen Whaler), but the Glasswall stands against its ridge (its 52 m of glass runs
	// into the slope): 32 m; the rest as the dressing's.
	const cliffClear = [];
	const keepC = (p, r) => p && cliffClear.push([p[0], p[1], r]);
	for (const id of ["volcanic", "tundra"]) {
		const L = FFL.lands[id];
		keepC(L.village, 150); keepC(L.mouthShrine, 30); keepC(L.champion, 30);
		for (const c of L.camps) keepC(c, 55);
		for (let i = 0; i < L.road.length; i++) keepC(L.road[i], 9);
	}
	for (const o of FFL.places || []) if (Array.isArray(o.at)) keepC(o.at, o.kind === "glasswall" || o.id === "glasswall" ? 32 : 16);
	for (const k of ["sites", "caves"]) for (const o of FFL[k] || []) if (Array.isArray(o.at)) keepC(o.at, k === "sites" ? 45 : 28);
	for (const o of FFL.shrines || []) if (Array.isArray(o.at)) keepC(o.at, 16);
	for (const o of FFL.kit || []) if (Array.isArray(o.at)) keepC(o.at, 8);
	keepC(FEAT.volcano, 150);
	for (const h of FFL.hidden || []) for (let i = 1; i < h.route.length; i++) { const [ax, ay] = h.route[i - 1], [bx, by] = h.route[i], L = Math.hypot(bx - ax, by - ay); for (let t = 0; t <= L; t += 10) keepC([ax + (bx - ax) * t / L, ay + (by - ay) * t / L], 12); }
	const cb = new Map();
	for (const [cx, cy, r] of cliffClear) for (let i = Math.floor((cx - r) / CELL); i <= Math.floor((cx + r) / CELL); i++) for (let j = Math.floor((cy - r) / CELL); j <= Math.floor((cy + r) / CELL); j++) { const k = i + "," + j; if (!cb.has(k)) cb.set(k, []); cb.get(k).push([cx, cy, r]); }
	const cliffOk = (x, y) => !(cb.get(Math.floor(x / CELL) + "," + Math.floor(y / CELL)) || []).some(([cx, cy, r]) => Math.hypot(x - cx, y - cy) < r + 4);
	// (round 7: ice faces on the ice itself; a steep face in the snow is rock showing through it: granite panels)
	const ICE = new Set(["glacier", "ice", "crevasse", "lake_ice"].map((k) => SURF[k]));
	let cliffs = 0;
	for (let gx = -6600; gx < 6600; gx += CLIFF_STEP) for (let gy = -6600; gy < 6600; gy += CLIFF_STEP) {
		const ci = Math.round(gx / CLIFF_STEP), cj = Math.round(gy / CLIFF_STEP);
		const sl0 = slopeAt(gx, gy);
		if (sl0 < CLIFF_DEG - 3) continue;
		// (the Tundra's ice faces closer: SM_IceFace's sides sink back into the slope, so laid overlapping they join into one
		// fluted wall, where the rock's panels, further apart, left the ground's own wall showing between them; round 7)
		const zc0 = zoneAt(gx, gy), icy0 = zc0 && /^T/.test(zc0) && ICE.has(surfAt(gx, gy));
		const want = CLIFF_STEP * CLIFF_STEP / Math.max(0.1, Math.cos(Math.min(84, sl0) * Math.PI / 180)) / (icy0 ? CLIFF_AREA_ICE : CLIFF_AREA);
		const tries = Math.floor(want) + (rand(ci, cj, 905) < want - Math.floor(want) ? 1 : 0);
		for (let t = 0; t < tries; t++) {
		const x = gx + rand(ci, cj, 901 + t * 11) * CLIFF_STEP, y = gy + rand(ci, cj, 902 + t * 11) * CLIFF_STEP;
		const zc = zoneAt(x, y);
		if (!zc || !PLAN[zc] || !/^T/.test(zc)) continue;
		const h = hAt(x, y);
		if (h < 2 || slopeAt(x, y) < CLIFF_DEG) continue;
		if (!cliffOk(x, y) || lavaDist(x, y) < LAND.lava.width / 2 + 6) continue;
		const sf = surfAt(x, y);
		// (the ice grows its own walls out of the land: WRLandCaves)
		if (sf === SURF.road || sf === SURF.main_road || ICE.has(sf)) continue;
		// downhill: against the ground's gradient
		const gxh = hAt(x + 3, y) - hAt(x - 3, y), gyh = hAt(x, y + 3) - hAt(x, y - 3), gl = Math.hypot(gxh, gyh) || 1;
		const dx = -gxh / gl, dy = -gyh / gl;
		add("granite_cliff", x, y, h, Math.atan2(dy, dx) * 180 / Math.PI + (rand(ci, cj, 903 + t * 11) - 0.5) * 16, 0.85 + 0.45 * rand(ci, cj, 904 + t * 11));
		cliffs++;
		}
	}
	console.log("cliffs:", cliffs, "panels on ground over", CLIFF_DEG, "degrees");
}
// ---- the coast: what the sea leaves on its beaches (11-22 round 7) ---------------------------------------------------
// The beaches ran bare sand to the water's edge. Along the whole island's coast (every land; the cliffs' and the islets'
// rock aside): patches of sea-worn pebbles in the swash where a stretch is stony (on the Tundra's shingle everywhere), the
// tide's strand line of weed a little above the water, drift logs up the beach lying along it, marram grass over the
// back of the sandy beaches, and on the Tundra's shore stranded ice; round the meadow's lake, pebbles at the water's edge
// away from the beach, the jetty and the reeds. Each draws on its own salts of the cell hash, so nothing else moves.
{
	// (the sea's material lifts and waves its surface over its level: the water's edge you see lies near +0.75 m (the
	// pebbles laid to +0.75 stood in the water's edge), so the strand line, the swash and the logs are measured from there)
	const SHOWN = 0.75, LAKE = LAND.lake.level;
	const sm = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
	const RL = read("Tools/terrain/round_island_layout.json");
	const away = [];
	for (const l of RL.lands) for (const c of l.camps || []) away.push([c[0], c[1], 60]);
	away.push([HP.jetty.at[0], HP.jetty.at[1], 24]);
	for (const p of HP.beach) away.push([p[0], p[1], 14]);
	for (const [rx, ry] of HP.reeds) away.push([rx, ry, 7]);
	const coastOk = (x, y) => !away.some(([cx, cy, r]) => Math.hypot(x - cx, y - cy) < r) && isClear(x, y, 2) && lavaDist(x, y) > LAND.lava.width / 2 + 12;
	const NOT = new Set(["road", "main_road", "plaza", "cobble", "lava", "lava_crust", "shore_rock"].map((k) => SURF[k]).filter((v) => v !== undefined));
	const SANDY = new Set([SURF.sand, SURF.black_sand, SURF.shingle]);
	// Which land a shore point is in (out in the swash the zone grid says seabed: look a little inland).
	const landOf = (x, y) => {
		const r = Math.hypot(x, y) || 1;
		for (let d = 0; d <= 90; d += 15) {
			const z = zoneAt(x * (1 - d / r), y * (1 - d / r));
			if (z && z !== "seabed" && z !== "deep") return /^V\d/.test(z) ? "volcanic" : /^T\d/.test(z) ? "tundra" : z;
		}
		return null;
	};
	// marram on the back of the beach, drift logs (more where forests stand behind the shore, most on the Tundra's)
	const DUNE = { forest: 1, swamp: 0.8, ocean: 1, meadow: 1, storm_peaks: 0.45, graveyard: 0.5, wasteland: 0.3, desert: 0.2 };
	const DRIFT = { forest: 2, swamp: 1.6, ocean: 1.8, meadow: 1.2, storm_peaks: 1.4, graveyard: 1, wasteland: 0.8, desert: 0.4, volcanic: 0.4, tundra: 2.2 };
	const WRACK = { volcanic: 0.5, tundra: 0.6 };
	const tangent = (x, y) => { const gx = hAt(x + 2, y) - hAt(x - 2, y), gy = hAt(x, y + 2) - hAt(x, y - 2), gl = Math.hypot(gx, gy) || 1; return Math.atan2(gx / gl, -gy / gl) * 180 / Math.PI; };
	const before = { ...counts };
	const tries = (lam, ci, cj, salt) => Math.floor(lam) + (rand(ci, cj, salt) < lam - Math.floor(lam) ? 1 : 0);
	const C2 = STEP * STEP;
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const k = j * N + i, h0 = ((BIN[k * 2] | (BIN[k * 2 + 1] << 8)) - HF.offset) / HF.unitsPerMetre;
		if (h0 < -1.3 || h0 > 3.4) continue;
		const cx = X0 + i * STEP, cy = Y0 + j * STEP;
		const lakeSide = Math.abs(h0 - LAKE) < 1.2 && Math.hypot(cx - LAND.lake.at[0], cy - LAND.lake.at[1]) < 420;
		const r = Math.hypot(cx, cy), deg = (Math.atan2(cx, -cy) * 180 / Math.PI + 360) % 360;
		if (!lakeSide && Math.abs(r - coastR(deg)) > 260) continue;
		const pick = (salt) => [cx + (rand(i, j, salt) - 0.5) * STEP, cy + (rand(i, j, salt + 1) - 0.5) * STEP];
		if (lakeSide) {
			// the lake's banks: pebbles at the water's edge
			const lam = C2 / 46 * sm(-0.1, 0.35, fbm(cx / 120, cy / 120, 1731, 2));
			for (let t = 0, n = tries(lam, i, j, 1730); t < n; t++) {
				const [x, y] = pick(1732 + t * 5), h = hAt(x, y);
				if (h < LAKE - 0.45 || h > LAKE + 0.35 || NOT.has(surfAt(x, y)) || !coastOk(x, y)) continue;
				add("beach_pebbles", x, y, h, rand(i, j, 1734 + t * 5) * 360, 0.7 + 0.5 * rand(i, j, 1735 + t * 5));
			}
			continue;
		}
		const land = landOf(cx, cy);
		if (!land || land === "city" || land === "yards") continue;
		// pebbles in the swash (stony stretches; the Tundra's shingle all along)
		{
			const stony = land === "tundra" ? 1 : sm(-0.05, 0.35, fbm(cx / 160, cy / 160, 1701, 2)) + 0.06;
			for (let t = 0, n = tries(C2 / 34 * stony, i, j, 1700); t < n; t++) {
				const [x, y] = pick(1702 + t * 5), h = hAt(x, y);
				if (h < SHOWN - 0.5 || h > (land === "tundra" ? 1.6 : SHOWN + 0.35) || NOT.has(surfAt(x, y)) || !coastOk(x, y)) continue;
				add(land === "volcanic" ? "black_pebbles" : land === "tundra" ? "frost_pebbles" : "beach_pebbles", x, y, h, tangent(x, y) + (rand(i, j, 1704 + t * 5) - 0.5) * 40, 0.7 + 0.5 * rand(i, j, 1705 + t * 5));
			}
		}
		// the strand line: weed a little above the water, lying along the shore
		{
			const lam = 0.26 * sm(-0.3, 0.25, fbm(cx / 200, cy / 200, 1711, 2)) * (WRACK[land] ?? 1);
			for (let t = 0, n = tries(lam, i, j, 1710); t < n; t++) {
				const [x, y] = pick(1712 + t * 5), h = hAt(x, y);
				if (h < SHOWN + 0.05 || h > SHOWN + 0.55 || !SANDY.has(surfAt(x, y)) || !coastOk(x, y)) continue;
				add("wrack", x, y, h, tangent(x, y) + (rand(i, j, 1714 + t * 5) - 0.5) * 28, 0.8 + 0.45 * rand(i, j, 1715 + t * 5));
			}
		}
		// drift logs up the beach, along it
		{
			const lam = 0.0085 * (DRIFT[land] ?? 1);
			for (let t = 0, n = tries(lam, i, j, 1720); t < n; t++) {
				const [x, y] = pick(1722 + t * 5), h = hAt(x, y);
				if (h < SHOWN + 0.35 || h > 2.4 || !SANDY.has(surfAt(x, y)) || !coastOk(x, y)) continue;
				add("driftwood", x, y, h, tangent(x, y) + (rand(i, j, 1724 + t * 5) - 0.5) * 60 + (rand(i, j, 1726 + t * 5) < 0.5 ? 180 : 0), 0.8 + 0.4 * rand(i, j, 1725 + t * 5));
			}
		}
		// marram over the back of the sandy beaches, in tussocks
		if (DUNE[land]) {
			const lam = C2 / 12 * DUNE[land] * sm(-0.1, 0.35, fbm(cx / 45, cy / 45, 1741, 3)) * sm(1.6, 2.5, h0);
			for (let t = 0, n = Math.min(3, tries(lam, i, j, 1740)); t < n; t++) {
				const [x, y] = pick(1742 + t * 5), h = hAt(x, y);
				if (h < 1.6 || surfAt(x, y) !== SURF.sand || !coastOk(x, y)) continue;
				// a tussock: the tuft and two to four more crowded round it
				const m = 3 + Math.floor(rand(i, j, 1746 + t * 5) * 3);
				for (let q = 0; q < m; q++) {
					const a = rand(i, j, 1760 + t * 17 + q) * 2 * Math.PI, d = q === 0 ? 0 : 0.25 + 0.5 * rand(i, j, 1761 + t * 17 + q);
					const qx = x + Math.cos(a) * d, qy = y + Math.sin(a) * d;
					add("dune_grass", qx, qy, hAt(qx, qy), rand(i, j, 1762 + t * 17 + q) * 360, 0.7 + 0.5 * rand(i, j, 1763 + t * 17 + q));
				}
			}
		}
		// the Tundra's stranded ice: floes and broken blocks in the shallows and on the shingle
		if (land === "tundra") {
			const lam = 0.22 * sm(-0.2, 0.3, fbm(cx / 300, cy / 300, 1751, 2));
			for (let t = 0, n = tries(lam, i, j, 1750); t < n; t++) {
				const [x, y] = pick(1752 + t * 5), h = hAt(x, y);
				if (h < SHOWN - 0.35 || h > SHOWN + 0.6 || NOT.has(surfAt(x, y)) || !coastOk(x, y)) continue;
				add("shore_ice", x, y, h, rand(i, j, 1754 + t * 5) * 360, 0.5 + 0.7 * rand(i, j, 1755 + t * 5) ** 1.5);
			}
		}
	}
	// The stream's banks (round 9: it ran between bare grass banks, a canal): pebbles at its water's edge, in patches, laid
	// along it. Its own salts.
	{
		const SR = LAND.lake.stream;
		for (let q = 1; q < SR.length; q++) {
			const [ax, ay, al] = SR[q - 1], [bx, by, bl] = SR[q], L = Math.hypot(bx - ax, by - ay) || 1, ux = (bx - ax) / L, uy = (by - ay) / L;
			const yaw = Math.atan2(uy, ux) * 180 / Math.PI;
			for (let t = 0; t < L; t += 1.6) {
				const u = t / L, cx = ax + (bx - ax) * u, cy = ay + (by - ay) * u, lvl = al + (bl - al) * u;
				const ci = Math.round((cx - X0) / STEP), cj = Math.round((cy - Y0) / STEP), salt = (q * 97 + Math.round(t * 10)) % 100000;
				for (const side of [-1, 1]) {
					if (rand(ci, cj, 1801 + salt * 7 + side) > 0.42 * sm(-0.2, 0.3, fbm(cx / 60, cy / 60, 1807, 2))) continue;
					const off = 3.0 + 1.6 * rand(ci, cj, 1803 + salt * 7 + side), x = cx - uy * side * off, y = cy + ux * side * off, h = hAt(x, y);
					if (h < lvl - 0.5 || h > lvl + 0.35 || NOT.has(surfAt(x, y)) || !isClear(x, y, 1)) continue;
					add("beach_pebbles", x, y, h, yaw + (rand(ci, cj, 1805 + salt * 7 + side) - 0.5) * 30, 0.55 + 0.4 * rand(ci, cj, 1806 + salt * 7 + side));
				}
			}
		}
	}
	console.log("coast:", ["beach_pebbles", "black_pebbles", "frost_pebbles", "wrack", "driftwood", "dune_grass", "shore_ice"].map((kk) => kk + " " + ((counts[kk] || 0) - (before[kk] || 0))).join(", "));
}
// ---- hazards: the lava burns where its molten channel runs; the lake ice and the glacier's polished ice are slick ----
const HCELL = 600;
const lavaCells = [], iceCells = [];
{
	const seen = new Set();
	for (let i = 1; i < lava.length; i++) {
		const [ax, ay, al] = lava[i - 1], [bx, by, bl] = lava[i], L = Math.hypot(bx - ax, by - ay);
		for (let d = 0; d <= L; d += 2) {
			const t = d / L, cx = ax + (bx - ax) * t, cy = ay + (by - ay) * t, lv = al + (bl - al) * t;
			for (let o = -LAND.lava.width / 2; o <= LAND.lava.width / 2; o += 2) {
				const nx = -(by - ay) / L, ny = (bx - ax) / L, px = cx + nx * o, py = cy + ny * o;
				const k = Math.floor(px * 100 / HCELL) + "," + Math.floor(py * 100 / HCELL);
				if (seen.has(k)) continue;
				seen.add(k);
				const [kx, ky] = k.split(",").map(Number);
				lavaCells.push((kx + 0.5) * HCELL, (ky + 0.5) * HCELL, Math.round(lv * 100));
			}
		}
	}
	const iceS = new Set([SURF.lake_ice, SURF.ice]);
	for (let x = -6600; x < 6600; x += HCELL / 100) for (let y = -6600; y < 6600; y += HCELL / 100) {
		const cx = x + HCELL / 200, cy = y + HCELL / 200, z = zoneAt(cx, cy);
		if (!z || !/^T/.test(z) || !iceS.has(surfAt(cx, cy))) continue;
		iceCells.push(Math.round(cx * 100), Math.round(cy * 100), Math.round(hAt(cx, cy) * 100));
	}
}
const file = {
	_comment: "Generated by Tools/terrain/fire_frost_dressing.mjs from the round island's ground (Data/lands/island.json + island_ground.bin) and Tools/terrain/fire_frost_layout.json: edit the scripts, not this file. The Volcanic's and the Frozen tundra's trees, scrub and rocks per sub-zone, as the Old Island's dressing file (instances are flat [x, y, z, yaw, scale] in cm/degrees; hazards are flat [x, y, z] 6 m cell centres: lava burns, ice slips). Read at runtime by UWRDressingSubsystem on L_Wildreach.",
	map: "L_Wildreach",
	counts,
	instances: out,
	// The cells are for play only (lava burns, ice slips): WRRoundIsland draws the lava river, the ground draws the ice.
	hazards: { cell: HCELL, drawCells: false, lava: lavaCells, ice: iceCells },
};
fs.writeFileSync(path.join(root, "Data/wildreach_dressing.json"), JSON.stringify(file));
console.log("dressing:", Object.entries(counts).map(([k, n]) => k + " " + n).join(", "), "| lava cells", lavaCells.length / 3, "ice cells", iceCells.length / 3);

// ---- life: the dens of each sub-zone's creatures (the Old Island's spawn_regions.json kinds, given out by sub-zone),
// and the roaming minis' loops moved onto their lands. Data/wildreach_life.json, read by WRRoundIsland (server).
{
	// Per sub-zone: dens wanted, the day table and the night table (weights). Lowland kinds on the coast bands, the
	// midland herds and flyers further in, the highland predators on the crater and the glacier.
	const LIFE = {
		V1: [10, { pumicehog: 5, ashstrider: 4, cinderfang: 1 }, { cinderfang: 3, glowmoth: 2 }],
		V2: [14, { ashstrider: 3, kilnmite: 3, basaltback: 2, emberhorn: 2 }, { cinderfang: 2, glowmoth: 3, kilnmite: 1 }],
		V3: [7, { kilnmite: 4, scorchwing: 2, basaltback: 2 }, { scorchwing: 2, basaltback: 1 }],
		V4: [8, { kilnmite: 3, scorchwing: 3, ashstrider: 1 }, { glowmoth: 3, scorchwing: 2 }],
		V5: [6, { magmaw: 2, basaltback: 3, scorchwing: 2 }, { magmaw: 3 }],
		V6: [8, { pumicehog: 3, emberhorn: 3, ashstrider: 2 }, { cinderfang: 2, glowmoth: 2 }],
		V7: [9, { emberhorn: 3, pumicehog: 2, cinderfang: 1 }, { cinderfang: 3, glowmoth: 3 }],
		T1: [10, { snowpuff: 5, floeback: 3, aurora_fox: 2 }, { gloomhowl: 3, aurora_fox: 3 }],
		T2: [14, { frostantler: 4, aurora_fox: 2, snowpuff: 2 }, { gloomhowl: 4, frostantler: 1 }],
		T3: [10, { floeback: 4, rimeshell: 3, snowpuff: 2 }, { floeback: 2, glacimaw: 2 }],
		T4: [6, { rimeshell: 3, glacimaw: 2, woolly_drudge: 1 }, { glacimaw: 3 }],
		T5: [5, { rimeclaw: 2, glacimaw: 2 }, { rimeclaw: 3 }],
		T6: [9, { woolly_drudge: 4, frostantler: 3, snowpuff: 2 }, { gloomhowl: 3, frostantler: 2 }],
		T7: [9, { woolly_drudge: 5, frostantler: 3, aurora_fox: 1 }, { gloomhowl: 3, woolly_drudge: 1 }],
	};
	const GAP = 170;
	const roll = (table, r) => { const e = Object.entries(table), tot = e.reduce((t, [, w]) => t + w, 0); let p = r * tot; for (const [k, w] of e) { p -= w; if (p <= 0) return k; } return e.at(-1)[0]; };
	const cand = {};
	for (let gx = -6600; gx < 6600; gx += 20) for (let gy = -6600; gy < 6600; gy += 20) {
		const z = zoneAt(gx, gy); if (!LIFE[z]) continue;
		(cand[z] ||= []).push([gx + 10, gy + 10, rand(Math.round(gx / 20), Math.round(gy / 20), 977)]);
	}
	const dens = [], denCount = {};
	const taken = [];
	for (const [z, [want, day, night]] of Object.entries(LIFE)) {
		const list = (cand[z] || []).sort((a, b) => a[2] - b[2]);
		let n = 0;
		for (const [x, y, r] of list) {
			if (n >= want) break;
			const h = hAt(x, y), sf = surfAt(x, y);
			if (h < 2 || BARE.has(sf) && !(z === "T3" && sf === SURF.lake_ice) || slopeAt(x, y) > 28) continue;
			if (!isClear(x, y, 25) || lavaDist(x, y) < 45) continue;
			// Not by a drop: the ground within 45 m all round keeps within 10 m of the den's (no ice cliff, crater wall or ridge edge).
			if ([0, 1, 2, 3, 4, 5, 6, 7].some((k) => [20, 45].some((r) => Math.abs(hAt(x + r * Math.cos(k * Math.PI / 4), y + r * Math.sin(k * Math.PI / 4)) - h) > 10))) continue;
			if (taken.some(([tx, ty]) => Math.hypot(tx - x, ty - y) < GAP)) continue;
			taken.push([x, y]);
			const d = roll(day, rand(Math.round(x), Math.round(y), 31)), nt = roll(night, rand(Math.round(x), Math.round(y), 37));
			dens.push([Math.round(x), Math.round(y), d, nt, z]);
			n++;
		}
		denCount[z] = n;
	}
	// The roaming minis' loops (creatures.json roam, Old Island cm) moved onto their wedges.
	const CRE = read("Data/creatures.json"), rows = Array.isArray(CRE.creatures || CRE) ? (CRE.creatures || CRE) : Object.values(CRE.creatures || CRE);
	const roam = {};
	for (const c of rows) if (c.roam && c.archetype === "boss" && ["cinderhorn", "old_tusker"].includes(c.id)) {
		const side = c.roam[0][0] < 0 ? "volcanic" : "tundra";
		roam[c.id] = c.roam.map(([x, y]) => FF.toRound(side, [x / 100, y / 100]));
	}
	// Each loop kept in its own country, well off the roads, villages and camps: Old Tusker, an old mammoth bull, on the
	// Mammoth Steppe; Cinderhorn out on the Ash Plains (the Plough Trench is on its loop). Same shape, moved whole.
	const HOME = { cinderhorn: ["V1"], old_tusker: ["T7"] };
	for (const [id, pts] of Object.entries(roam)) {
		const cx = pts.reduce((t, p) => t + p[0], 0) / pts.length, cy = pts.reduce((t, p) => t + p[1], 0) / pts.length;
		let fit = null;
		for (const pad of [450, 350, 250, 150]) {
			for (const z of HOME[id]) for (const [x, y] of (cand[z] || [])) {
				const moved = pts.map(([px, py]) => [px - cx + x, py - cy + y]);
				if (moved.every(([mx, my]) => zoneAt(mx, my) === z && isClear(mx, my, pad) && slopeAt(mx, my) < 28 && !BARE.has(surfAt(mx, my)) && hAt(mx, my) > 3 && [0, 2, 4, 6].every((k) => Math.abs(hAt(mx + 45 * Math.cos(k * Math.PI / 4), my + 45 * Math.sin(k * Math.PI / 4)) - hAt(mx, my)) < 12))) { fit = moved; break; }
			}
			if (fit) { console.log("roam", id, "clear by", pad, "m"); break; }
		}
		roam[id] = (fit || pts).map((p) => p.map(Math.round));
	}
	const life = {
		_comment: "Generated by Tools/terrain/fire_frost_dressing.mjs: the Volcanic's and the Frozen tundra's creature dens on the round island (map metres; each den a day pack and a night pack, kinds from spawn_regions.json given out by sub-zone) and the roaming minis' loops moved onto their lands. Read by WRRoundIsland (server).",
		activation: 12000, respawn: 600, dens, roam,
	};
	fs.writeFileSync(path.join(root, "Data/wildreach_life.json"), JSON.stringify(life));
	console.log("life:", dens.length, "dens", JSON.stringify(denCount), "| roam", Object.entries(roam).map(([k, v]) => k + " " + JSON.stringify(v[0]) + " " + (zoneAt(...v[0]) || "?")).join(", "));
}

// ---- part 2: the places (landmarks, kit, ground features, micro-hazards), the map's points of interest, and the
// villages and shrines, all at their round island spots. Data/wildreach_places.json (UWRIslandPlaces),
// wildreach_dressing.json "pois" (the map, the discovery banner, the world systems' start), Data/wildreach_world.json
// (UWRWorldSystems: villages, shrines; each row "map": "L_Wildreach").
{
	const OLDP = read("Data/island_places.json"), WORLD = read("Data/world.json");
	const NEWZ = {
		V6: { name: "The Cinder Terraces", biome: "volcanic", bed: "ff.V1", colour: "6B5646" },
		V7: { name: "The Ashfall Woods", biome: "volcanic", bed: "ff.V1", colour: "A9A8AB" },
		T6: { name: "The Thaw", biome: "tundra", bed: "ff.T1", colour: "9FB08F" },
		T7: { name: "The Mammoth Steppe", biome: "tundra", bed: "ff.T1", colour: "C2A66A" },
	};
	const seen = new Set(), zones = [];
	for (const z of OLDP.zones) if (!seen.has(z.id)) { seen.add(z.id); zones.push({ id: z.id, name: z.name, biome: z.biome, bed: z.bed, colour: z.colour }); }
	for (const [id, z] of Object.entries(NEWZ)) zones.push({ id, ...z });
	// Thin ice only on ice: each patch onto the nearest lake ice within 200 m, or left out.
	const iceS = new Set([SURF.lake_ice, SURF.ice]);
	const onIce = (o) => {
		const [x, y] = o.at;
		if (iceS.has(surfAt(x, y))) return o;
		let best = null;
		for (let dx = -200; dx <= 200; dx += 10) for (let dy = -200; dy <= 200; dy += 10) {
			const d = Math.hypot(dx, dy); if (d > 200 || (best && d >= best[0])) continue;
			if (iceS.has(surfAt(x + dx, y + dy)) && iceS.has(surfAt(x + dx + 8, y + dy)) && iceS.has(surfAt(x + dx, y + dy + 8))) best = [d, [Math.round(x + dx), Math.round(y + dy)]];
		}
		return best ? { ...o, at: best[1] } : null;
	};
	const thin = FFL.hazards.thinIce.map(onIce).filter(Boolean);
	// The Fallen Smith (11-22): the port to the round island moved him to the road's lava crossing but left his hammer's
	// bridge points where the river was on the Old Island, 4.5 km off (his arm and haft ran across the map). His haft
	// spans the round island's river at the stretch nearest him; his frame turns so local +Y points from the river to
	// his bank (he lies 26 m back from the haft's end, along the bank), moved along the bank if his 80 m would lie
	// across the road.
	const smithRow = (p) => {
		const near = (x, y) => {
			let best = null;
			for (let i = 1; i < lava.length; i++) {
				const [ax, ay] = lava[i - 1], [bx, by] = lava[i], dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
				const t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / L2)), px = ax + dx * t, py = ay + dy * t, d = Math.hypot(x - px, y - py);
				if (!best || d < best.d) best = { d, p: [px, py], tan: [dx / Math.sqrt(L2), dy / Math.sqrt(L2)] };
			}
			return best;
		};
		let at = [...p.at], n = null, r = null;
		for (let pass = 0; pass < 3; pass++) {
			r = near(...at);
			n = r.d > 1 ? [(at[0] - r.p[0]) / r.d, (at[1] - r.p[1]) / r.d] : [-r.tan[1], r.tan[0]];
			const X = [n[1], -n[0]], cr = FFL.lands.volcanic.smithBridge;
			if (!cr) break;
			const roadX = (cr[0] - at[0]) * X[0] + (cr[1] - at[1]) * X[1];
			const shift = roadX >= 0 ? Math.min(0, roadX - 34) : Math.max(0, roadX + 58);
			if (Math.abs(shift) < 0.5) break;
			at = [at[0] + X[0] * shift, at[1] + X[1] * shift];
		}
		const half = LAND.lava.width / 2 + 4;
		const R = (v) => Math.round(v * 10) / 10;
		return { ...p, at: at.map(Math.round), yaw: R(Math.atan2(n[1], n[0]) * 180 / Math.PI - 90),
			bridge: { from: [R(r.p[0] + n[0] * half), R(r.p[1] + n[1] * half)], to: [R(r.p[0] - n[0] * half), R(r.p[1] - n[1] * half)] } };
	};
	const placeRows = FFL.places.map((p) => p.kind === "fallen_smith" ? smithRow(p) : p);
	console.log("fallen smith:", JSON.stringify(placeRows.find((p) => p.kind === "fallen_smith")));
	const places = {
		_comment: "Generated by Tools/terrain/fire_frost_dressing.mjs from Tools/terrain/fire_frost_layout.json: the Old Island's Fire and Frost places, kit, ground features and micro-hazards at their round island spots (map metres = game cm / 100), for UWRIslandPlaces on L_Wildreach. Sub-zones have no shapes here: the land's own zone letters say where each lies.",
		zones,
		places: placeRows,
		roads: [],
		ground: FFL.ground,
		kit: FFL.kit,
		hazards: { thinIce: thin, fumaroles: FFL.hazards.fumaroles, geysers: FFL.hazards.geysers, katabatic: FFL.hazards.katabatic },
	};
	fs.writeFileSync(path.join(root, "Data/wildreach_places.json"), JSON.stringify(places));
	// Points of interest (cm): the camps, the bosses and minis, the caves (the map's fill-in list and banners).
	const cm = (p, z) => ({ x: Math.round(p[0] * 100), y: Math.round(p[1] * 100), z: Math.round((z ?? hAt(p[0], p[1])) * 100) });
	const pois = [];
	for (const id of ["volcanic", "tundra"]) {
		const L = FFL.lands[id], nm = id === "volcanic" ? "Volcanic" : "Tundra";
		L.camps.forEach((c, i) => pois.push({ name: `${nm} camp ${i + 1}`, kind: "camp", ...cm(c), biome: id }));
	}
	for (const s of FFL.sites) pois.push({ name: s.name, kind: s.kind, ...cm(s.at), biome: s.land });
	file.pois = pois;
	fs.writeFileSync(path.join(root, "Data/wildreach_dressing.json"), JSON.stringify(file));
	// Villages and shrines: the Old Island's rows, moved (same ids: the paths, quests and saves know them by id).
	const yawTo = (a, b) => Math.round(Math.atan2(b[1] - a[1], b[0] - a[0]) * 180 / Math.PI);
	const nearest = (p, list) => list.reduce((b, q) => (!b || Math.hypot(q[0] - p[0], q[1] - p[1]) < Math.hypot(b[0] - p[0], b[1] - p[1]) ? q : b), null);
	const villages = WORLD.villages.filter((v) => /volcanic|tundra/.test(v.biome)).map((v) => {
		const L = FFL.lands[v.biome], at = L.village;
		const { camp, inland, ...rest } = v;
		// (round 12) eight households in two loose rings (five in one ring read as a toy camp)
		return { ...rest, huts: Math.max(rest.huts || 0, 8), map: "L_Wildreach", land: "island", at, yaw: yawTo(at, nearest(at, L.camps)) };
	});
	const shrineRows = (WORLD.shrines.list || WORLD.shrines).filter((s) => /volcanic|tundra/.test(s.biome)).map((s) => {
		if (s.great || !s.at) return { ...s, map: "L_Wildreach" };
		const moved = FFL.shrines.find((q) => q.id === s.id);
		if (!moved) return null;
		const at = moved.at, road = nearest(at, FFL.lands[s.biome].road);
		return { ...s, map: "L_Wildreach", land: "island", at, yaw: yawTo(at, road) };
	}).filter(Boolean);
	const world = {
		_comment: "Generated by Tools/terrain/fire_frost_dressing.mjs: the round island's own world rows (villages, shrines), read by UWRWorldSystems after world.json's on L_Wildreach. Same ids as the Old Island's rows, which aren't live there.",
		villages,
		shrines: { list: shrineRows },
	};
	fs.writeFileSync(path.join(root, "Data/wildreach_world.json"), JSON.stringify(world, null, 1));
	console.log("places:", FFL.places.length, "places,", FFL.kit.length, "kit,", FFL.ground.length, "ground,", thin.length + "/" + FFL.hazards.thinIce.length, "thin ice on ice,", FFL.hazards.fumaroles.length, "fumaroles,", FFL.hazards.geysers.length, "geysers | pois", pois.length, "| villages", villages.length, "shrines", shrineRows.length);
}

// ---- part 3: the caves (underground: round_ground.mjs), the bosses and their altars, the resource nodes, the
// hidden tubes' routes and each land's event centre.
{
	const WORLD = read("Data/world.json"), LIFE = JSON.parse(fs.readFileSync(path.join(root, "Data/wildreach_life.json"), "utf8"));
	// Caves: underground (round_ground.mjs sites them, cuts their mouths and writes island.json "caves"; WRLand builds
	// their rock). Each AWRCave stands on its hall's floor, its +X toward the passage and the mouth (AWRCave's chamber
	// layout: the ore, the hazard, the burrowers and the glow are placed from it), underground: no sculpted shell.
	const caves = [], cavePois = [];
	(LAND.caves || []).filter((c) => !c.room).forEach((c, index) => {
		const [hx, hy, hf] = c.hall, [dx, dy] = c.dir;
		caves.push({ biome: c.biome, cave: c.id, x: Math.round(hx * 100), y: Math.round(hy * 100), z: Math.round(hf * 100), yaw: +(Math.atan2(-dy, -dx) * 180 / Math.PI).toFixed(2), variant: -1, scale: 1, underground: true });
		cavePois.push({ name: c.name ? c.name.replace(/^the /, "The ") : `${c.biome === "volcanic" ? "Volcanic" : "Tundra"} cave ${index % 3 + 1}`, kind: "cave", x: Math.round(c.mouth[0] * 100), y: Math.round(c.mouth[1] * 100), z: Math.round(c.mouth[2] * 100), biome: c.biome });
	});
	file.caves = caves;
	// Nothing of the dressing stands in a cave's mouth or the way up to it (the passage's first metres lie just under
	// the ground there, and a sunk boulder would stand in it).
	let cut = 0;
	const inMouth = (x, y) => (LAND.caves || []).some((c) => { const [mx, my] = c.mouth, [dx, dy] = c.dir, s = (x - mx) * dx + (y - my) * dy, lat = -(x - mx) * dy + (y - my) * dx; return Math.hypot(x - mx, y - my) < 16 || (s > -14 && s < 14 && Math.abs(lat) < 9) || (c.shapes || []).some((q) => q.k === "shaft" && Math.hypot(x - q.c[0], y - q.c[1]) < q.r + 5); });
	for (const [kind, arr] of Object.entries(file.instances)) {
		const keepI = [];
		for (let i = 0; i + 4 < arr.length; i += 5) {
			if (inMouth(arr[i] / 100, arr[i + 1] / 100)) { cut++; continue; }
			keepI.push(arr[i], arr[i + 1], arr[i + 2], arr[i + 3], arr[i + 4]);
		}
		file.instances[kind] = keepI;
		file.counts[kind] = keepI.length / 5;
	}
	console.log("caves cleared of", cut, "dressing pieces");
	file.pois = file.pois.filter((p) => p.kind !== "cave").concat(cavePois);
	fs.writeFileSync(path.join(root, "Data/wildreach_dressing.json"), JSON.stringify(file));
	// The bosses at their arenas (the majors' altars 40 m east of them on the arena floor), the minis at their lairs.
	const ID = { "Vulkarn": "vulkarn", "Skaldrith": "skaldrith", "Obsidian Warden": "obsidian_warden", "Ignessa": "ignessa", "Grimjaw": "grimjaw", "Pale Hunter": "pale_hunter" };
	const bosses = FFL.sites.filter((s) => ID[s.name]).map((s) => {
		const row = { id: ID[s.name], at: s.at };
		if (s.name === "Vulkarn") Object.assign(row, { altar: "fire", altarAt: [s.at[0] + 40, s.at[1]] });
		if (s.name === "Skaldrith") Object.assign(row, { altar: "frost", altarAt: [s.at[0] + 40, s.at[1]] });
		return row;
	});
	// Resource nodes (the Old Island's ore, sulfur, lichen and berries; trees, rocks and scrub are the dressing's): each land's
	// share by sub-zone, better metal deeper in.
	const NODES = {
		copper_ore: [10, ["V1", "V2", "V6", "V7"], ["T1", "T2", "T6", "T7"]], tin_ore: [6, ["V2", "V3"], ["T3", "T4"]], iron_ore: [6, ["V5", "V3"], ["T5", "T4"]],
		sulfur_crystal: [14, ["V4", "V4", "V4", "V5"], null], lichen: [14, null, ["T1", "T6", "T7", "T5"]], berries: [8, ["V2", "V6"], ["T2", "T6"]],
	};
	const nodes = [], taken = [];
	let seed = 0;
	for (const [item, [count, vz, tz]] of Object.entries(NODES)) for (const zones of [vz, tz]) {
		if (!zones) continue;
		let n = 0;
		for (let tries = 0; n < count && tries < 40000; tries++) {
			const x = -6400 + rand(tries, ++seed, 7) * 12800, y = -6400 + rand(seed, tries, 11) * 12800, z = zoneAt(x, y);
			if (!zones.includes(z) || hAt(x, y) < 2 || slopeAt(x, y) > 26 || BARE.has(surfAt(x, y)) || !isClear(x, y, 6) || lavaDist(x, y) < 30) continue;
			if (item === "berries" && z === "V2" && surfAt(x, y) !== SURF.kipuka) continue;
			if (taken.some(([tx, ty]) => Math.hypot(tx - x, ty - y) < 60)) continue;
			taken.push([x, y]);
			nodes.push([Math.round(x), Math.round(y), item, Math.round(rand(seed, tries, 13) * 360)]);
			n++;
		}
	}
	LIFE.bosses = bosses;
	LIFE.nodes = nodes;
	fs.writeFileSync(path.join(root, "Data/wildreach_life.json"), JSON.stringify(LIFE));
	// The hidden tubes on their routes (fire_frost.mjs lays them on gentle ground), and each land's centre for its event.
	const WW = JSON.parse(fs.readFileSync(path.join(root, "Data/wildreach_world.json"), "utf8"));
	WW.hidden = WORLD.hidden.filter((h) => /volcanic|tundra/.test(h.biome)).map((h) => { const m = FFL.hidden.find((q) => q.id === h.id); return m ? { ...h, map: "L_Wildreach", route: m.route } : null; }).filter(Boolean);
	WW.biomes = {
		volcanic: { ...WORLD.biomes.volcanic, centre: FFL.lands.volcanic.boss },
		tundra: { ...WORLD.biomes.tundra, centre: FFL.lands.tundra.boss },
	};
	fs.writeFileSync(path.join(root, "Data/wildreach_world.json"), JSON.stringify(WW, null, 1));
	const counts2 = {}; for (const n of nodes) counts2[n[2]] = (counts2[n[2]] || 0) + 1;
	console.log("part 3: caves", caves.length + "/" + FFL.caves.length, "| bosses", bosses.map((b) => b.id).join(","), "| nodes", JSON.stringify(counts2), "| tubes", WW.hidden.map((h) => h.id + " " + h.route.length + " pts").join(", "));
}
