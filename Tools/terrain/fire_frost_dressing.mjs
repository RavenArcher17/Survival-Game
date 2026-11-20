// The round island's Volcanic and Frozen tundra dressed as their design says (cooks/fire-frost 3.3.1 / 3.4.1; Raven 11-20
// "make sure they look like they are supposed to"): each sub-zone's trees, scrub and rocks, its lava that burns and its
// ice that slips. Reads the finished ground (run after round_ground.mjs) and writes Data/wildreach_dressing.json, which
// UWRDressingSubsystem loads on L_Wildreach (the same builder, kinds, harvests and ledger as the Old Island's dressing).
// Also Data/wildreach_places.json (the places), wildreach_world.json (villages, shrines) and the dressing's pois.
// Also Data/wildreach_life.json: the creature dens per sub-zone and the roaming minis' loops.
//   node Tools/terrain/fire_frost_dressing.mjs
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import { fbm } from "./round_island.mjs";
import * as FF from "./fire_frost.mjs";

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
	const places = {
		_comment: "Generated by Tools/terrain/fire_frost_dressing.mjs from Tools/terrain/fire_frost_layout.json: the Old Island's Fire and Frost places, kit, ground features and micro-hazards at their round island spots (map metres = game cm / 100), for UWRIslandPlaces on L_Wildreach. Sub-zones have no shapes here: the land's own zone letters say where each lies.",
		zones,
		places: FFL.places,
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
		return { ...rest, map: "L_Wildreach", land: "island", at, yaw: yawTo(at, nearest(at, L.camps)) };
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
