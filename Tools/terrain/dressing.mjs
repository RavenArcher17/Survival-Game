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

const sample = (i, j) => (raw.readUInt16LE((Math.max(0, Math.min(SIZE - 1, j)) * SIZE + Math.max(0, Math.min(SIZE - 1, i))) * 2) - 32768) / UNITS_PER_METRE;
const h = (x, y) => sample(Math.round(x), Math.round(y));
// The landscape's own surface between its 1 m vertices (cave fitting wants the real ground, not the nearest sample).
const hLerp = (x, y) => {
	const i = Math.floor(x), j = Math.floor(y), fx = x - i, fy = y - j;
	const a = sample(i, j), b = sample(i + 1, j), c = sample(i, j + 1), d = sample(i + 1, j + 1);
	return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
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

// The value noise layout_proposal.mjs built the island with (same seed), for the rivers' meander and the lava's edges.
const nseed = layout.seed;
const hash = (x, y) => { let k = (x * 374761393 + y * 668265263 + nseed * 2147483647) | 0; k = (k ^ (k >>> 13)) * 1274126177; return ((k ^ (k >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
	const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
	const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
	return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };

// Keep clear: spawn camps, boss arenas, mini-boss sites.
const clear = [
	...layout.spawns.map(s => ({ x: s.x, y: s.y, r: 70 })),
	...layout.bosses.map(b => ({ x: b.x, y: b.y, r: 160 })),
	...layout.miniBosses.map(b => ({ x: b.x, y: b.y, r: 70 })),
];
const isClear = (x, y, extra = 0) => clear.every(c => Math.hypot(c.x - x, c.y - y) > c.r + extra);
const world = (x, y) => [Math.round(x * 100 - HALF), Math.round(y * 100 - HALF), Math.round(h(x, y) * 100)];

// ---- the lava flow (computed first: the decoration keeps out of it)
// The lava river is the middle of the trench layout_proposal.mjs carved along its meandering line: the same warped
// distance, so the flow follows the real channel. Cover (m) is how far inside the flow's edge a point is: the flow's
// half-width wanders along the river and its edge is ragged; it ends at the shore and short of the camps and arenas.
const LAVA_LINE = [[1700, 2900], [1250, 3050], [700, 2850], [80, 2950]];
const distToLine = (px, py, pts) => {
	let best = Infinity;
	for (let i = 1; i < pts.length; i++) {
		const [ax, ay] = pts[i - 1], [bx, by] = pts[i];
		const dx = bx - ax, dy = by - ay, t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
		best = Math.min(best, Math.hypot(px - ax - t * dx, py - ay - t * dy));
	}
	return best;
};
const lavaCover = (x, y) => {
	const d = distToLine(x + 110 * fbm(x / 260, y / 260 + 9, 3), y + 110 * fbm(x / 260 + 4, y / 260, 3), LAVA_LINE);
	if (d > 30) return -99;
	let c = 9 + 3.5 * fbm(x / 70 + 11, y / 70, 2) + 1.6 * fbm(x / 9 + 2, y / 9 + 5, 2) - d;
	c = Math.min(c, (h(x, y) - 1.5) * 2);  // cools into the sea
	for (const k of clear) c = Math.min(c, Math.hypot(k.x - x, k.y - y) - k.r);
	return c;
};
// The lava's surface over the ground (cm) from its cover: sunk under the ground outside the edge, so the sheet's rim
// always meets the terrain, rising to a thin skin over the middle. The game uses the same numbers (from the file).
const LIFT = { coverLow: -1.2, sink: -45, coverHigh: 2.6, top: 22 };
const lavaLift = c => LIFT.sink + (LIFT.top - LIFT.sink) * sstep(LIFT.coverLow, LIFT.coverHigh, c);
const LAVA_GRID = 2;  // m: every other landscape vertex, so the sheet lies along the landscape's own triangles
const lavaRows = [];
const lavaVisible = new Set();  // "i,j" grid points under visible lava (decoration keeps out)
for (let j = Math.floor(2650 / LAVA_GRID); j <= Math.ceil(3250 / LAVA_GRID); j++) {
	let run = null;
	for (let i = 0; i <= Math.ceil(1900 / LAVA_GRID); i++) {
		const x = i * LAVA_GRID, y = j * LAVA_GRID;
		const c = lavaCover(x, y);
		if (c > -1.6) {
			if (!run) { run = [j, i, 0]; lavaRows.push(run); }
			run.push(Math.round(sample(x, y) * 100), Math.max(-16, Math.min(60, Math.round(c * 10))));
			run[2]++;
			if (lavaLift(c) > 0) lavaVisible.add(`${i},${j}`);
		} else run = null;
	}
}
const inLava = (x, y, margin = 0) => {
	for (let gi = Math.floor((x - margin) / LAVA_GRID); gi <= Math.ceil((x + margin) / LAVA_GRID); gi++)
		for (let gj = Math.floor((y - margin) / LAVA_GRID); gj <= Math.ceil((y + margin) / LAVA_GRID); gj++) if (lavaVisible.has(`${gi},${gj}`)) return true;
	return false;
};

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
// Fit each cave to its ground. The landscape is a cave's floor, so where it stands decides whether its tunnel is open:
// sat at the lowest ground under it, the ground rose 2-3 m up the tunnel and left a crawl-space under the roof (some
// caves couldn't be entered at all). Using the measured meshes (Tools/art/cave_measure.py -> cave_hollows.json), try
// each variant, a turn of the entrance and a size, and pick a height where a survivor walks from outside to the back
// of the chamber with headroom while the walls still reach below the ground all round (no gaps under the rock).
// Smallest size first, then the entrance closest to facing the lowlands, then the most headroom.
const hollows = JSON.parse(fs.readFileSync(path.join(root, "Tools/terrain/cave_hollows.json"), "utf8")).variants;
const CUT = -3;  // the meshes' walls end this far under their origin (cave_gen.py)
const column = (v, x, y) => { const row = v.grid[Math.round(x) - v.x0]; return (row && row[Math.round(y) - v.y0]) || []; };
const isMouth = (x, y) => x > 11.5 && Math.abs(y) < 6;  // the porch's floor lip dips lower: not a wall
// Walking line (cave metres): the tunnel axis from the back of the chamber out past the mouth, a capsule's width either
// side; and where the ore, the gas and the colony go (AWRCave::SpawnContents), which must stand on open floor too.
const walkPts = [];
for (let x = -6; x <= 20; x += 1) for (const y of [-0.7, 0, 0.7]) walkPts.push([x, y, y === 0 ? 2.6 : 2.3]);
for (const [x, y] of [[-5.5, -5.5], [-5.5, 5.5], [0, -6.5], [3, 6.2], [-7, 0], [-3, 0], [2.5, 0]]) walkPts.push([x, y, 2.3]);
const wallPts = [];
for (const v of hollows) {
	const pts = [];
	for (let gx = 0; gx < v.grid.length; gx += 2) for (let gy = 0; gy < v.grid[gx].length; gy += 2) {
		const x = gx + v.x0, y = gy + v.y0, hits = v.grid[gx][gy];
		if (hits.length && hits[0] > CUT + 0.2 && !isMouth(x, y)) pts.push([x, y]);
	}
	wallPts.push(pts);
}
function fitCave(c) {
	const want = c.yaw, lo = c.z / 100;
	let best = null;
	for (const s of [1.0, 1.1, 1.2, 1.3, 1.4, 1.5]) {
		for (let v = 0; v < hollows.length; v++) {
			for (let turn = -60; turn <= 60; turn += 15) {
				const yaw = (want + turn + 360) % 360, a = yaw * Math.PI / 180, ca = Math.cos(a), sa = Math.sin(a);
				// Unreal's yaw turns local +X toward +Y: world = (x cos - y sin, x sin + y cos) * size.
				const at = (x, y) => [c.mx + (x * ca - y * sa) * s, c.my + (x * sa + y * ca) * s];
				let zMax = Infinity;  // walls: ground over the cut everywhere under the rock
				for (const [x, y] of wallPts[v]) { const [wx, wy] = at(x, y); zMax = Math.min(zMax, hLerp(wx, wy) - CUT * s - 0.3); }
				const ground = walkPts.map(([x, y]) => { const [wx, wy] = at(x, y); return hLerp(wx, wy); });
				// Headroom at height z: open air from step height (0.45 m) up, roof at least `need` over the ground.
				const clearAt = z => {
					let worst = Infinity;
					for (let k = 0; k < walkPts.length; k++) {
						const [x, y, need] = walkPts[k], zl = (ground[k] - z) / s;
						const above = column(hollows[v], x, y).filter(q => q > zl + 0.45 / s);
						if (above.length % 2) return -Infinity;  // rock (or a step too high)
						worst = Math.min(worst, (above.length ? Math.min(...above) - zl : 99) * s - need);
					}
					return worst;
				};
				// Highest height the walls allow gives the most headroom; then sink it while a metre of spare headroom
				// stays (the walls bury deeper and less of the mound's foot shows).
				const top = clearAt(zMax);
				if (top < 0) continue;
				let z = zMax;
				while (z > zMax - 4 && clearAt(z - 0.1) >= Math.min(1, top)) z -= 0.1;
				const margin = clearAt(z);
				// Smallest size, then the entrance facing the lowlands, then headroom; the three sculpts take turns.
				const score = s * 100 + Math.abs(turn) * 0.05 - Math.min(margin, 1) + (v === c.index % hollows.length ? 0 : 0.02);
				if (!best || score < best.score) best = { score, s, v, yaw, z, margin, wallSlack: zMax - z };
			}
		}
		if (best) break;
	}
	if (!best) throw new Error(`no fit for the cave at ${Math.round(c.mx)}, ${Math.round(c.my)}`);
	c.variant = best.v; c.scale = best.s; c.yaw = best.yaw; c.z = Math.round(best.z * 100);
	console.log(`cave ${c.biome} ${Math.round(c.mx)},${Math.round(c.my)}: ${hollows[best.v].name} x${best.s} yaw ${best.yaw} z ${(best.z - lo).toFixed(1)} m over the lowest ground, headroom +${best.margin.toFixed(1)} m, walls ${(best.wallSlack + 0.3).toFixed(1)} m under`);
}
caves.forEach((c, index) => fitCave(Object.assign(c, { index })));
clear.push(...caves.map(c => ({ x: c.mx, y: c.my, r: 48 })));

// ---- decoration
const types = { char_tree: [], basalt_rock: [], ember_shrub: [], pine: [], ice_rock: [], snow_shrub: [] };
// Each piece stands on the lowest ground under its footprint (metres, times its scale), so on a slope the uphill side
// buries instead of the downhill roots floating. Only heights change: positions and the random stream stay the same.
const footprint = { char_tree: 3.2, pine: 2.6, basalt_rock: 1.6, ice_rock: 1.6, ember_shrub: 0.8, snow_shrub: 0.8, basalt_cliff: 0, ice_cliff: 0 };  // cliff boulders keep their own height: they cling to near-vertical faces
const groundUnder = (x, y, r) => {
	let lo = h(x, y);
	for (let a = 0; a < 8; a++) lo = Math.min(lo, h(x + Math.cos(a * Math.PI / 4) * r, y + Math.sin(a * Math.PI / 4) * r));
	return Math.round(lo * 100);
};
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
		const yaw = Math.round(rand() * 360);
		// Nothing grows in the lava (checked last, so the random stream and every other piece stay where they were).
		if (west && inLava(x, y, footprint[type] * scale + 1)) continue;
		const [wx, wy] = world(x, y);
		types[type].push(wx, wy, groundUnder(x, y, footprint[type] * scale), yaw, Math.round(scale * 100) / 100);
	}
}
// ---- undergrowth: more shrubs, in clumps (Raven: "bushes are very sparse"). Its own random stream, so the trees and
// rocks above don't move. Lowlands and midlands are thick in places and thin in others (a slow noise decides), the
// highlands and steep ground get few. They're the same gatherable shrubs (fiber, sometimes berries).
const shrubRand = rng(layout.seed * 15485863 + 29);
const SHRUB_STEP = 4.5;
let undergrowth = 0;
for (let gy = 150; gy < 3880; gy += SHRUB_STEP) {
	for (let gx = 150; gx < 3880; gx += SHRUB_STEP) {
		const x = gx + (shrubRand() - 0.5) * SHRUB_STEP, y = gy + (shrubRand() - 0.5) * SHRUB_STEP;
		const keep = shrubRand(), scale = 0.55 + shrubRand() * 0.75, yaw = Math.round(shrubRand() * 360);
		const z = h(x, y), s = slope(x, y);
		if (z < 2.5 || s > 0.6 || Math.abs(x - RIDGE_X) < 160 || inHollow(x, y) || !isClear(x, y)) continue;
		const clump = 0.5 + 0.5 * fbm(x / 35 + 3, y / 35 - 7, 3);
		const chance = (z > 520 || s > 0.45 ? 0.018 : 0.075) * (0.25 + 1.75 * clump * clump);
		if (keep > chance) continue;
		const west = x < RIDGE_X;
		if (west && inLava(x, y, 2)) continue;
		const [wx, wy] = world(x, y);
		types[west ? "ember_shrub" : "snow_shrub"].push(wx, wy, groundUnder(x, y, footprint.ember_shrub * scale), yaw, Math.round(scale * 100) / 100);
		undergrowth++;
	}
}
const counts = Object.fromEntries(Object.entries(types).map(([k, v]) => [k, v.length / 5]));

// ---- biome gear-check hazards (M3 plan): the lava river (volcanic) burns without lava resistance; the
// icy tundra rivers and the glacier plateau are slick without ice resistance. A river's core is the deep
// middle of its trench; its kind comes from the nearest river line (same lines as layout_proposal.mjs).
const RIVERS = [
	{ kind: "water", pts: [[1760, 1250], [1350, 1150], [900, 1320], [450, 1150], [80, 1230]] },
	{ kind: "lava", pts: LAVA_LINE },
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
		// Ice rivers in the deep middle of their trench (lava comes from the flow above).
		if (around - z > 3.2) kind = nearestRiver(x, y);
		const dg = Math.hypot(x - GLACIER.x, y - GLACIER.y);
		if (!kind && x > RIDGE_X && dg > 175 && dg < 330 && slope(x, y) < 0.25) kind = "ice";
		if (kind === "ice") hazards.ice.push(...world(x, y));
	}
}
// Lava cells for everything that only needs to know roughly where the lava is (sound, embers, wildlife keep-outs, the
// server's first look-up): every 6 m cell with visible lava, at the lava's mean surface height there. Whether a spot
// burns is decided on the fine grid (hazards.lavaGrid), which is also what's drawn.
{
	const cells = new Map();
	for (const run of lavaRows) {
		const [j, i0, n] = run;
		for (let k = 0; k < n; k++) {
			const t = run[3 + k * 2], c = run[4 + k * 2] / 10, lift = lavaLift(c);
			if (lift <= 0) continue;
			const X = (i0 + k) * LAVA_GRID * 100 - HALF, Y = j * LAVA_GRID * 100 - HALF;
			const key = `${Math.floor(X / 600)},${Math.floor(Y / 600)}`;
			const e = cells.get(key) || { sum: 0, n: 0 };
			e.sum += t + lift; e.n++;
			cells.set(key, e);
		}
	}
	for (const [key, e] of cells) {
		const [cx, cy] = key.split(",").map(Number);
		hazards.lava.push(cx * 600 + 300, cy * 600 + 300, Math.round(e.sum / e.n));
	}
}
hazards.lavaGrid = {
	_comment: "The lava flow: rows of grid points [row j, first column i, count n, then n pairs (ground cm, cover dm)]; a point is world (i * spacing - half, j * spacing - half). Lava surface = ground + lift(cover), lift = sink..top over cover coverLow..coverHigh (smoothstep, cover in m); lava is where lift > 0.",
	spacing: LAVA_GRID * 100, half: HALF, ...LIFT, rows: lavaRows,
};
// ---- cliff boulders on the ridge's steep faces (hides the stretched landscape triangles there). Its own random
// stream, so adding it doesn't move any decoration above. Decoration only: no collision, so the wall still holds.
types.basalt_cliff = [];
types.ice_cliff = [];
const cliffRand = rng(layout.seed * 104729 + 7);
for (let gy = 150; gy < 3880; gy += 13) {
	for (let gx = RIDGE_X - 230; gx < RIDGE_X + 230; gx += 13) {
		const x = gx + (cliffRand() - 0.5) * 13, y = gy + (cliffRand() - 0.5) * 13;
		const s = slope(x, y), keep = cliffRand();
		if (s < 0.7 || keep > 0.55 || h(x, y) < 3 || !isClear(x, y)) continue;
		const [wx, wy] = world(x, y), yaw = Math.round(cliffRand() * 360), sc = Math.round((1.2 + cliffRand() * 1.6) * 100) / 100;
		types[x < RIDGE_X ? "basalt_cliff" : "ice_cliff"].push(wx, wy, groundUnder(x, y, footprint.basalt_cliff * sc), yaw, sc);
	}
}
counts.basalt_cliff = types.basalt_cliff.length / 5;
counts.ice_cliff = types.ice_cliff.length / 5;

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
	_comment: "Generated by Tools/terrain/dressing.mjs from the heightmap + layout. Instances are flat [x, y, z, yaw, scale, ...] in cm/degrees; hazards are flat [x, y, z, ...] cell centres (lava's fine flow in hazards.lavaGrid); caves carry their mesh variant and size. Read at runtime by UWRDressingSubsystem.",
	map: "L_Island", counts, caves: caves.map(({ biome, x, y, z, yaw, variant, scale }) => ({ biome, x, y, z, yaw, variant, scale })), instances: types, hazards, pois,
};
console.log("hazard cells", "lava", hazards.lava.length / 3, "ice", hazards.ice.length / 3, "| lava grid points", lavaRows.reduce((n, r) => n + r[2], 0), "visible", lavaVisible.size);
fs.writeFileSync(path.join(root, "Data/island_dressing.json"), JSON.stringify(out));
console.log("caves", caves.length, JSON.stringify(caves.map(c => [c.biome, Math.round(c.mx), Math.round(c.my)])));
console.log("instances", JSON.stringify(counts), "total", Object.values(counts).reduce((a, b) => a + b, 0), "| undergrowth added", undergrowth);
console.log("file KB", Math.round(fs.statSync(path.join(root, "Data/island_dressing.json")).size / 1024));
