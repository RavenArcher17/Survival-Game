// Island dressing placements: decorative trees, rocks and shrubs, plus cave sites, computed from the
// heightmap and the approved layout. Writes Data/island_dressing.json, which every machine reads at
// runtime (UWRDressingSubsystem) - no map rebuild needed to change the dressing.
//   node Tools/terrain/dressing.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SIZE = 4033, UNITS_PER_METRE = 32, HALF = (SIZE - 1) * 100 / 2, RIDGE_X = 2016;
const raw = fs.readFileSync(path.join(root, "Art/Terrain/island_height.r16"));
const layout = JSON.parse(fs.readFileSync(path.join(root, "Docs/terrain/layout_proposal.json"), "utf8"));

const h = (x, y) => {
	const i = Math.max(0, Math.min(SIZE - 1, Math.round(x))), j = Math.max(0, Math.min(SIZE - 1, Math.round(y)));
	return (raw.readUInt16LE((j * SIZE + i) * 2) - 32768) / UNITS_PER_METRE;
};
const slope = (x, y) => Math.max(Math.abs(h(x + 2, y) - h(x - 2, y)), Math.abs(h(x, y + 2) - h(x, y - 2))) / 4;
// River beds: noticeably lower than the ground around them.
const inHollow = (x, y) => {
	let sum = 0;
	for (const [dx, dy] of [[14, 0], [-14, 0], [0, 14], [0, -14]]) sum += h(x + dx, y + dy);
	return sum / 4 - h(x, y) > 1.5;
};
function rng(seed) { return () => { seed |= 0; seed = (seed + 0x6d2b79f5) | 0; let t = Math.imul(seed ^ (seed >>> 15), 1 | seed); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const rand = rng(layout.seed * 7919 + 13);

// Keep clear: spawn camps, boss arenas, mini-boss sites.
const clear = [
	...layout.spawns.map(s => ({ x: s.x, y: s.y, r: 70 })),
	...layout.bosses.map(b => ({ x: b.x, y: b.y, r: 160 })),
	...layout.miniBosses.map(b => ({ x: b.x, y: b.y, r: 70 })),
];
const isClear = (x, y, extra = 0) => clear.every(c => Math.hypot(c.x - x, c.y - y) > c.r + extra);
const world = (x, y) => [Math.round(x * 100 - HALF), Math.round(y * 100 - HALF), Math.round(h(x, y) * 100)];

// ---- caves first (dressing keeps away from them): 3 per biome in the midlands
const caves = [];
for (const biome of ["volcanic", "tundra"]) {
	let tries = 0;
	while (caves.filter(c => c.biome === biome).length < 3 && tries++ < 20000) {
		const off = 500 + rand() * 650;
		const x = biome === "volcanic" ? RIDGE_X - off : RIDGE_X + off, y = 350 + rand() * 3330;
		if (h(x, y) < 8 || !isClear(x, y, 180) || inHollow(x, y)) continue;
		let lo = Infinity, hi = -Infinity;
		for (let dx = -14; dx <= 14; dx += 7) for (let dy = -14; dy <= 14; dy += 7) { const v = h(x + dx, y + dy); lo = Math.min(lo, v); hi = Math.max(hi, v); }
		if (hi - lo > 5) continue;  // needs a fairly flat footprint
		if (caves.some(c => Math.hypot(c.mx - x, c.my - y) < 450)) continue;
		// Entrance faces downhill-ish / away from the ridge so you walk in from the lowlands.
		const yaw = biome === "volcanic" ? 180 : 0;
		const [wx, wy, wz] = world(x, y);
		caves.push({ biome, mx: x, my: y, x: wx, y: wy, z: Math.round(lo * 100), yaw });
	}
}
clear.push(...caves.map(c => ({ x: c.mx, y: c.my, r: 48 })));

// ---- decoration
const types = { char_tree: [], basalt_rock: [], ember_shrub: [], pine: [], ice_rock: [], snow_shrub: [] };
const STEP = 9;
for (let gy = 150; gy < 3880; gy += STEP) {
	for (let gx = 150; gx < 3880; gx += STEP) {
		const x = gx + (rand() - 0.5) * STEP, y = gy + (rand() - 0.5) * STEP;
		const z = h(x, y), s = slope(x, y);
		if (z < 2.5 || Math.abs(x - RIDGE_X) < 160 || inHollow(x, y) || !isClear(x, y)) continue;
		const west = x < RIDGE_X, r = rand(), high = z > 520 || s > 0.45;
		let type = null, scale = 0.8 + rand() * 0.6;
		if (s > 0.9) continue;
		if (high) { if (r < 0.10) type = west ? "basalt_rock" : "ice_rock"; }
		else if (r < 0.16) type = west ? "char_tree" : "pine";
		else if (r < 0.22) type = west ? "basalt_rock" : "ice_rock";
		else if (r < 0.34) { type = west ? "ember_shrub" : "snow_shrub"; scale = 0.6 + rand() * 0.6; }
		if (!type) continue;
		const [wx, wy, wz] = world(x, y);
		types[type].push(wx, wy, wz, Math.round(rand() * 360), Math.round(scale * 100) / 100);
	}
}
const counts = Object.fromEntries(Object.entries(types).map(([k, v]) => [k, v.length / 5]));

// ---- biome gear-check hazards (M3 plan): the lava river (volcanic) burns without lava resistance; the
// icy tundra rivers and the glacier plateau are slick without ice resistance. A river's core is the deep
// middle of its trench; its kind comes from the nearest river line (same lines as layout_proposal.mjs).
const RIVERS = [
	{ kind: "water", pts: [[1760, 1250], [1350, 1150], [900, 1320], [450, 1150], [80, 1230]] },
	{ kind: "lava", pts: [[1700, 2900], [1250, 3050], [700, 2850], [80, 2950]] },
	{ kind: "ice", pts: [[2300, 1300], [2800, 1150], [3300, 1350], [3950, 1250]] },
	{ kind: "ice", pts: [[2330, 2850], [2900, 3000], [3450, 2800], [3950, 2900]] },
];
const distSeg = (px, py, [ax, ay], [bx, by]) => {
	const dx = bx - ax, dy = by - ay, t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
	return Math.hypot(px - ax - t * dx, py - ay - t * dy);
};
const nearestRiver = (x, y) => {
	let best = null, bd = Infinity;
	for (const r of RIVERS) for (let i = 1; i < r.pts.length; i++) { const d = distSeg(x, y, r.pts[i - 1], r.pts[i]); if (d < bd) { bd = d; best = r; } }
	return bd < 260 ? best.kind : null;
};
const CELL = 6, GLACIER = { x: 2820, y: 2016 };
const hazards = { cell: CELL * 100, lava: [], ice: [] };
for (let y = 100; y < 3930; y += CELL) {
	for (let x = 100; x < 3930; x += CELL) {
		const z = h(x, y);
		if (z < 1 || !isClear(x, y)) continue;
		let kind = null;
		const around = (h(x + 40, y) + h(x - 40, y) + h(x, y + 40) + h(x, y - 40)) / 4;
		// Lava only in the deepest middle (~24 m wide, a few seconds to cross); ice rivers a bit wider.
		if (around - z > 3.2) kind = nearestRiver(x, y);
		if (kind === "lava" && around - z < 4.5) kind = null;
		const dg = Math.hypot(x - GLACIER.x, y - GLACIER.y);
		if (!kind && x > RIDGE_X && dg > 175 && dg < 330 && slope(x, y) < 0.25) kind = "ice";
		if (kind === "lava" || kind === "ice") hazards[kind].push(...world(x, y));
	}
}
// ---- places of interest for the in-game map list (M)
const poiAt = (name, kind, x, y) => { const [wx, wy, wz] = world(x, y); return { name, kind, x: wx, y: wy, z: wz }; };
const pois = [
	...layout.bosses.map(b => poiAt(b.name.replace(/ \(.*\)/, ""), "boss", b.x, b.y)),
	...layout.miniBosses.map(b => poiAt(b.name, "mini-boss", b.x, b.y)),
	...layout.passes.map(p => poiAt(p.name, "pass", p.x, p.y)),
	...layout.spawns.map((s, i) => poiAt(`${s.biome === "volcanic" ? "Volcanic" : "Tundra"} camp ${i % 3 + 1}`, "camp", s.x, s.y)),
	...caves.map((c, i) => poiAt(`${c.biome === "volcanic" ? "Volcanic" : "Tundra"} cave ${i % 3 + 1}`, "cave", c.mx, c.my)),
];
const out = {
	_comment: "Generated by Tools/terrain/dressing.mjs from the heightmap + layout. Instances are flat [x, y, z, yaw, scale, ...] in cm/degrees; hazards are flat [x, y, z, ...] cell centres. Read at runtime by UWRDressingSubsystem.",
	map: "L_Island", counts, caves: caves.map(({ biome, x, y, z, yaw }) => ({ biome, x, y, z, yaw })), instances: types, hazards, pois,
};
console.log("hazard cells", "lava", hazards.lava.length / 3, "ice", hazards.ice.length / 3);
fs.writeFileSync(path.join(root, "Data/island_dressing.json"), JSON.stringify(out));
console.log("caves", caves.length, JSON.stringify(caves.map(c => [c.biome, Math.round(c.mx), Math.round(c.my)])));
console.log("instances", JSON.stringify(counts), "total", Object.values(counts).reduce((a, b) => a + b, 0));
console.log("file KB", Math.round(fs.statSync(path.join(root, "Data/island_dressing.json")).size / 1024));
