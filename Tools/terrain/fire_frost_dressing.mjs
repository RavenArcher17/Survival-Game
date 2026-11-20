// The round island's Volcanic and Frozen tundra dressed as their design says (cooks/fire-frost 3.3.1 / 3.4.1; Raven 11-20
// "make sure they look like they are supposed to"): each sub-zone's trees, scrub and rocks, its lava that burns and its
// ice that slips. Reads the finished ground (run after round_ground.mjs) and writes Data/wildreach_dressing.json, which
// UWRDressingSubsystem loads on L_Wildreach (the same builder, kinds, harvests and ledger as the Old Island's dressing).
//   node Tools/terrain/fire_frost_dressing.mjs
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import { fbm } from "./round_island.mjs";

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
// [kind, per ha, scale min, scale max, steepest ground (deg), clumped, steep-only (cliffs), surfaces it keeps to]
const T = (kind, ha, s0, s1, slope = 28, clump = 0, only = null) => ({ kind, ha, s0, s1, slope, clump, only });
const ON = (...ids) => new Set(ids.map((k) => SURF[k]));
const PLAN = {
	V1: [T("char_tree", 1.2, 0.8, 1.2, 26, 0.6), T("ember_shrub", 5, 0.7, 1.2, 30, 0.5), T("basalt_rock", 1.5, 0.6, 1.5, 38), T("pumice_rock", 1.5, 0.5, 1.3, 34)],
	V2: [T("basalt_rock", 3, 0.6, 1.6, 38), T("ember_shrub", 1.5, 0.7, 1.1, 30, 0.5), T("char_tree", 0.4, 0.8, 1.2, 26, 0.6),
		T("pine", 30, 0.7, 1.15, 26, 0.4, ON("kipuka")), T("kipuka_shrub", 14, 0.6, 1.1, 30, 0.4, ON("kipuka"))],
	V3: [T("obsidian_blade", 6, 0.7, 1.5, 45, 0.5), T("basalt_rock", 2, 0.6, 1.4, 40), T("basalt_cliff", 4, 0.7, 1.4, 70, 0, "steep")],
	V4: [T("basalt_rock", 0.8, 0.5, 1.2, 36), T("ember_shrub", 0.5, 0.6, 1.0, 28, 0.5)],
	V5: [T("basalt_rock", 2, 0.6, 1.5, 40), T("basalt_column", 0.6, 0.8, 1.5, 30, 0.6), T("basalt_cliff", 3, 0.7, 1.4, 70, 0, "steep"), T("char_tree", 0.3, 0.8, 1.1, 24, 0.6)],
	V6: [T("ember_shrub", 3, 0.7, 1.2, 30, 0.5), T("basalt_rock", 0.8, 0.6, 1.3, 38), T("char_tree", 0.4, 0.8, 1.2, 26, 0.6),
		T("pine", 1.5, 0.7, 1.1, 26, 0.7, ON("cinder_grass")), T("kipuka_shrub", 2, 0.6, 1.0, 30, 0.5, ON("cinder_grass"))],
	V7: [T("char_tree", 28, 0.9, 1.6, 30, 0.45), T("ember_shrub", 6, 0.7, 1.2, 30, 0.5), T("basalt_rock", 0.4, 0.6, 1.2, 38)],
	T1: [T("granite_rock", 1.2, 0.8, 2.2, 38), T("frost_shrub", 1.5, 0.6, 1.0, 30, 0.5), T("ice_boulder", 0.4, 0.6, 1.3, 38)],
	T2: [T("pine", 32, 0.75, 1.3, 30, 0.45), T("frost_shrub", 5, 0.6, 1.1, 30, 0.5), T("granite_rock", 0.4, 0.7, 1.6, 38)],
	T3: [T("pine", 3, 0.7, 1.2, 26, 0.7), T("frost_shrub", 5, 0.5, 0.9, 30, 0.5), T("ice_boulder", 0.8, 0.6, 1.4, 38)],
	T4: [T("serac", 2.5, 0.7, 1.4, 45, 0.4), T("ice_boulder", 2.5, 0.6, 1.6, 40), T("ice_cliff", 3, 0.7, 1.4, 70, 0, "steep")],
	T5: [T("ice_boulder", 1.5, 0.6, 1.5, 40), T("serac", 0.6, 0.7, 1.3, 40, 0.4), T("ice_cliff", 3, 0.7, 1.4, 70, 0, "steep"), T("basalt_rock", 6, 0.6, 1.4, 40, 0, ON("basalt"))],
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
				if (!isClear(x, y, P.kind === "pine" || P.kind === "char_tree" ? 3 : 1.5)) continue;
				const ld = lavaDist(x, y);
				if (ld < LAND.lava.width / 2 + (P.kind === "char_tree" || P.kind === "pine" ? 18 : 8)) continue;
				const s = P.s0 + (P.s1 - P.s0) * rand(ci, cj, p * 7 + 4 + t * 13);
				add(P.kind, x, y, h, rand(ci, cj, p * 7 + 5 + t * 13) * 360, s);
			}
		}
	}
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
