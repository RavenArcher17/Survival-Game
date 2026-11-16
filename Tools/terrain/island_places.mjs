// The Fire and Frost places on the Old Island (cooks/cookpass-world.md 3.2-3.6): sub-zones, landmarks, story sites, the
// two waymark roads, the hazards' spots and the kit POIs. Hand places are written here in metres (L_Island frame: +x
// east, +y south) and settled onto dry, gentle ground off the heightmap; kit POIs are dealt by the coverage rule (every
// walkable spot within 125 m of a place, kit places at least 150 m apart). Writes Data/island_places.json, read at
// runtime by UWRIslandPlaces (WRIslandPlaces.cpp), which builds every place on every machine.
//   node Tools/terrain/island_places.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SIZE = 4033, UPM = 32, HALFM = (SIZE - 1) / 2;
const raw = fs.readFileSync(path.join(root, "Art/Terrain/island_height.r16"));
const h = (x, y) => {
	const i = Math.max(0, Math.min(SIZE - 1, Math.round(x + HALFM))), j = Math.max(0, Math.min(SIZE - 1, Math.round(y + HALFM)));
	return (raw.readUInt16LE((j * SIZE + i) * 2) - 32768) / UPM;
};
const slope = (x, y) => Math.max(Math.abs(h(x + 3, y) - h(x - 3, y)), Math.abs(h(x, y + 3) - h(x, y - 3))) / 6;
const dressing = JSON.parse(fs.readFileSync(path.join(root, "Data/island_dressing.json"), "utf8"));
const world = JSON.parse(fs.readFileSync(path.join(root, "Data/world.json"), "utf8"));
const walk = JSON.parse(fs.readFileSync(path.join(root, "Data/island_walk.json"), "utf8"));
const cellKey = (x, y) => `${Math.round(x / 6)},${Math.round(y / 6)}`;   // 6 m cells, as the dressing's hazard cells
const lava = new Set(), ice = new Set();
for (let i = 0; i < dressing.hazards.lava.length; i += 3) lava.add(cellKey(dressing.hazards.lava[i] / 100, dressing.hazards.lava[i + 1] / 100));
for (let i = 0; i < dressing.hazards.ice.length; i += 3) ice.add(cellKey(dressing.hazards.ice[i] / 100, dressing.hazards.ice[i + 1] / 100));
const nearSet = (S, x, y, r) => { for (let dx = -r; dx <= r; dx += 6) for (let dy = -r; dy <= r; dy += 6) if (S.has(cellKey(x + dx, y + dy))) return true; return false; };
const isLava = (x, y, r = 0) => nearSet(lava, x, y, r);
const isIce = (x, y, r = 0) => nearSet(ice, x, y, r);
const round = v => Math.round(v * 10) / 10;

// Everything the places keep clear of: arenas, shrines, villages, camps, caves (metres, radius).
const keepOut = [];
for (const p of dressing.pois) {
	const x = p.x / 100, y = p.y / 100;
	if (Math.abs(x) > 2000 || Math.abs(y) > 2016) continue;
	const r = p.kind === "boss" ? 45 : p.kind === "mini-boss" ? 32 : p.kind === "camp" ? 40 : p.kind === "cave" ? 30 : 25;
	keepOut.push({ x, y, r, what: p.name });
}
const biomeCentre = id => world.biomes[id].centre;
const camp = name => { const p = dressing.pois.find(q => q.name === name); return [p.x / 100, p.y / 100]; };
const villages = {};
for (const v of world.villages) {
	const [cx, cy] = camp(v.camp), [bx, by] = biomeCentre(v.biome);
	const d = Math.hypot(bx - cx, by - cy);
	villages[v.id] = [cx + (bx - cx) / d * v.inland, cy + (by - cy) / d * v.inland];
	keepOut.push({ x: villages[v.id][0], y: villages[v.id][1], r: v.radius / 100 + 20, what: v.name });
}
for (const s of world.shrines.list) if (s.at && Math.abs(s.at[0]) < 2000 && Math.abs(s.at[1]) < 2016) keepOut.push({ x: s.at[0], y: s.at[1], r: 16, what: s.id });
const clearOf = (x, y, extra = 0, skip = "") => keepOut.every(k => k.what === skip || Math.hypot(k.x - x, k.y - y) > k.r + extra);

/** The flattest dry spot within R of (x, y) (lava, sea and keep-outs excluded); the nearest wins a tie. */
function settle(x, y, R = 30, opts = {}) {
	const { maxSlope = 0.35, allowIce = true, extra = 0, skip = "", quiet = false } = opts;
	let best = null, bestScore = 1e9;
	for (let r = 0; r <= R; r += 3) for (let a = 0; a < 360; a += r === 0 ? 360 : Math.max(6, 360 / (r / 2))) {
		const px = x + r * Math.cos(a * Math.PI / 180), py = y + r * Math.sin(a * Math.PI / 180);
		if (h(px, py) < 3 || isLava(px, py, 6) || (!allowIce && isIce(px, py)) || !clearOf(px, py, extra, skip)) continue;
		const s = slope(px, py);
		if (s > maxSlope) continue;
		const score = s + r / (R + 1) * 0.15;
		if (score < bestScore) { bestScore = score; best = [round(px), round(py)]; }
	}
	if (!best && quiet) return null;
	if (!best) console.warn(`settle: nothing within ${R} m of ${x}, ${y}; kept as written`);
	return best || [x, y];
}

// ---------------------------------------------------------------------------------------------------- sub-zones
// Tested in order: rings first (they lie over the rectangles). Metres.
const zones = [
	{ id: "V5", name: "The Crater Highlands", biome: "volcanic", ring: [-786, 0, 0, 520], bed: "ff.V5", colour: "FF8A3A" },
	{ id: "T5", name: "The Glacier Plateau", biome: "tundra", ring: [804, 0, 0, 340], bed: "ff.T5", colour: "BFE6FF" },
	{ id: "V1", name: "The Ash Plains", biome: "volcanic", rect: [-2100, -2100, -1250, 2100], bed: "ff.V1", colour: "D8D2C4" },
	{ id: "V2", name: "The Lava Fields", biome: "volcanic", rect: [-1250, 450, -150, 2100], bed: "ff.V2", colour: "FF6A1A" },
	{ id: "V3", name: "The Obsidian Ridges", biome: "volcanic", rect: [-1250, -2100, -800, -350], bed: "ff.V3", colour: "9A8FB0" },
	{ id: "V4", name: "The Solfatara", biome: "volcanic", rect: [-800, -2100, -150, -350], bed: "ff.V4", colour: "D9C230" },
	{ id: "V5", name: "The Crater Highlands", biome: "volcanic", rect: [-1250, -350, -150, 450], bed: "ff.V5", colour: "FF8A3A" },
	{ id: "T1", name: "The Snow Flats", biome: "tundra", rect: [1250, -2100, 2100, 2100], bed: "ff.T1", colour: "EEF2F5" },
	{ id: "T2", name: "The Pine Hollows", biome: "tundra", rect: [150, 450, 1250, 2100], bed: "ff.T2", colour: "9DB2C8" },
	{ id: "T3", name: "The Frozen Lakes", biome: "tundra", rect: [150, -2100, 1250, -550], bed: "ff.T3", colour: "9FD4E0" },
	{ id: "T4", name: "The Ice Shelves", biome: "tundra", rect: [150, -550, 1250, 450], bed: "ff.T4", colour: "5FA8D3" },
];
const zoneAt = (x, y) => {
	for (const z of zones) {
		if (z.ring) { const d = Math.hypot(x - z.ring[0], y - z.ring[1]); if (d >= z.ring[2] && d <= z.ring[3]) return z.id; }
		else if (x >= z.rect[0] && y >= z.rect[1] && x <= z.rect[2] && y <= z.rect[3]) return z.id;
	}
	return "";
};

// ---------------------------------------------------------------------------------------------------- hand places
// kind (the builder in WRIslandPlacesLook.cpp), id, name, at, face (a point it faces, or yaw), settle radius, landmark.
const places = [];
const add = (p, settleR = 30, opts = {}) => {
	const at = settleR > 0 ? settle(p.at[0], p.at[1], settleR, opts) : p.at;
	const out = { ...p, at };
	if (p.faceAt) out.yaw = round(Math.atan2(p.faceAt[1] - at[1], p.faceAt[0] - at[0]) * 180 / Math.PI);
	delete out.faceAt;
	out.zone = zoneAt(at[0], at[1]);
	places.push(out);
	keepOut.push({ x: at[0], y: at[1], r: p.clear ?? 30, what: p.id });
	return out;
};

// ---------------------------------------------------------------------------------------------------- site rules
// Aaron 11-16 (q-world-moved): "they need a redesign so they are placed with clear intention instead of just placed
// wherever the nearest land is." Each place whose fire-frost.md spot didn't fit the built island is found by the landform
// its story needs (cooks/wild-answers-1116.md 4.3): a headland for a beacon, a closed hollow for a lake, a crest with a
// sight line for a watchtower, the road for a caravan... Each rule scans a radius round the design spot, scores every
// candidate, and returns the best with its facing; if nothing qualifies the design spot is kept and the tool says so.
// The reason goes in the row ("why"), so the data says why every place stands where it does.
const SEA = 1.5;                                              // metres: below this the island is sea
const isSea = (x, y) => h(x, y) < SEA;
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);
const rays = (n) => Array.from({ length: n }, (_, k) => [Math.cos(k * 2 * Math.PI / n), Math.sin(k * 2 * Math.PI / n)]);
const R8 = rays(8), R16 = rays(16), R32 = rays(32);
/** The steepest slope over a footprint of radius r (centre, two rings of 8). */
const footSlope = (x, y, r) => { let m = slope(x, y); for (const [dx, dy] of R8) m = Math.max(m, slope(x + dx * r, y + dy * r), slope(x + dx * r / 2, y + dy * r / 2)); return m; };
/** Dry land a place can stand on: no sea, no lava, clear of camps / villages / shrines / arenas / earlier places. */
const standable = (x, y, r = 6, maxSlope = 0.35) => h(x, y) >= 3 && !isLava(x, y, 6) && clearOf(x, y, curClear) && footSlope(x, y, r) <= maxSlope;
/** The nearest sea along 16 rays out to maxR: { d, dir } (d = Infinity if none). */
function nearestSea(x, y, maxR = 400, step = 3) {
	let best = { d: Infinity, dir: [1, 0] };
	for (const [dx, dy] of R16) for (let d = 0; d <= Math.min(maxR, best.d); d += step) if (isSea(x + dx * d, y + dy * d)) { if (d < best.d) best = { d, dir: [dx, dy] }; break; }
	return best;
}
/** Is the ground below the straight line from (ax, ay, az) to (bx, by, bz)? (every 6 m) */
function sees(ax, ay, az, bx, by, bz) {
	const L = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.floor(L / 6));
	for (let k = 1; k < n; k++) { const t = k / n; if (h(ax + (bx - ax) * t, ay + (by - ay) * t) > az + (bz - az) * t - 0.5) return false; }
	return true;
}
/** Scan a disc of radius R round (cx, cy) every step metres; fn(x, y) returns { score, ... } or null. */
function scan(cx, cy, R, step, fn) {
	let best = null;
	for (let dx = -R; dx <= R; dx += step) for (let dy = -R; dy <= R; dy += step) {
		if (dx * dx + dy * dy > R * R) continue;
		const r = fn(cx + dx, cy + dy);
		if (r && (!best || r.score > best.score)) best = { at: [round(cx + dx), round(cy + dy)], ...r };
	}
	return best;
}
const ruled = [];
/** The place being sited keeps its own clear radius off everything already placed (not just its centre). */
let curClear = 0;
const CLEAR = { slag_light: 20, fox_pingo: 25, miners_grave: 22, miners_camp: 18, charred_caravan: 22, shard_field: 45, ridge_watch: 14, acid_pool: 22,
	tusk_midden: 18, sledge_line: 25, fishing_camp: 18, drowned_sledge: 15, giant_pine: 25, glacimaw_boneyard: 18, watch_cairn: 12, moulin: 12 };
/** Runs a rule and records what it chose (or that nothing fit and the design spot stands). */
function site(id, design, rule, why, fallback) {
	curClear = CLEAR[id] ?? 15;
	const r = rule();
	curClear = 0;
	if (!r) {
		// Never back onto the sea or a cliff: the spot the last pass settled it on.
		const at = fallback();
		console.warn(`site ${id}: no ground fits its rule near ${design}; kept the settled spot ${at}`);
		ruled.push({ id, moved: Math.round(dist(design, at)), fit: false });
		return { at, why: `${why} (no ground fit the rule: the nearest even ground)` };
	}
	ruled.push({ id, moved: Math.round(dist(design, r.at)), fit: true });
	return { ...r, why };
}
/** A headland: dry ground at the water's edge with the most sea round it at 150 m, high up; faces the open sea. */
const headland = (c, R) => scan(c[0], c[1], R, 6, (x, y) => {
	if (!standable(x, y, 5, 0.45) || h(x, y) < 5) return null;
	if (nearestSea(x, y, 40).d > 30) return null;
	let wet = 0, sx = 0, sy = 0;
	for (const [dx, dy] of R32) if (isSea(x + dx * 150, y + dy * 150)) { wet++; sx += dx; sy += dy; }
	const n = Math.hypot(sx, sy) || 1;
	return { score: wet / 32 * 10 + Math.min(h(x, y), 35) / 35 * 3 - dist([x, y], c) / R, face: [x + sx / n * 100, y + sy / n * 100] };
});
/** A closed hollow (inside `zone` if given): flat floor, ground at least `rise` m higher all round at 30 m; the deeper and nearer, the better. */
const hollow = (c, R, rise = 2, zone = "") => scan(c[0], c[1], R, 6, (x, y) => {
	if (zone && zoneAt(x, y) !== zone) return null;
	if (h(x, y) < 3 || isLava(x, y, 10) || !clearOf(x, y, curClear) || footSlope(x, y, 6) > 0.2) return null;
	let low = Infinity;
	for (const [dx, dy] of R16) low = Math.min(low, h(x + dx * 30, y + dy * 30) - h(x, y));
	if (low < rise) return null;
	return { score: Math.min(low, 10) - dist([x, y], c) / R * 3 };
});
/** A crest with an unbroken sight line from a tower top (eye m up) to `target`, a bonus for also seeing `also`. */
const overlook = (c, R, target, also, side, eye = 9) => scan(c[0], c[1], R, 8, (x, y) => {
	if (!side(x, y) || !standable(x, y, 5, 0.35)) return null;
	const z = h(x, y), d = dist([x, y], target);
	if (d < 250 || d > 800) return null;
	if (!sees(x, y, z + eye, target[0], target[1], h(target[0], target[1]) + 4)) return null;
	let higher = 0;
	for (const [dx, dy] of R16) if (h(x + dx * 50, y + dy * 50) > z + 1) higher++;
	const alsoSeen = also && sees(x, y, z + eye, also[0], also[1], h(also[0], also[1]) + 20) ? 1 : 0;
	return { score: z / 40 - higher * 0.6 + alsoSeen * 2 - dist([x, y], c) / R, face: target, seesAlso: !!alsoSeen };
});
/** The nearest point of a polyline to c, then the flattest standable spot within 14 m of it; faces `face`. */
const onRoad = (c, line, face) => {
	let bp = null, bd = Infinity;
	for (let i = 1; i < line.length; i++) {
		const [ax, ay] = line[i - 1], [bx, by] = line[i], L2 = (bx - ax) ** 2 + (by - ay) ** 2;
		const t = Math.max(0, Math.min(1, ((c[0] - ax) * (bx - ax) + (c[1] - ay) * (by - ay)) / L2));
		const p = [ax + (bx - ax) * t, ay + (by - ay) * t];
		if (dist(p, c) < bd) { bd = dist(p, c); bp = p; }
	}
	return scan(bp[0], bp[1], 14, 3, (x, y) => standable(x, y, 8, 0.25) ? { score: -slope(x, y) * 10 - dist([x, y], bp) / 14, face } : null);
};
/** The longest straight, flat, dry run (300-420 m) starting near c: { from, to }. */
const flatRun = (c, R) => {
	let best = null;
	for (let dx = -R; dx <= R; dx += 20) for (let dy = -R; dy <= R; dy += 20) {
		const ax = c[0] + dx, ay = c[1] + dy;
		if (dx * dx + dy * dy > R * R || !standable(ax, ay, 4, 0.14)) continue;
		for (let a = 0; a < 360; a += 10) {
			const ux = Math.cos(a * Math.PI / 180), uy = Math.sin(a * Math.PI / 180);
			let L = 0;
			for (let d = 10; d <= 420; d += 10) { const x = ax + ux * d, y = ay + uy * d; if (h(x, y) < 3 || isLava(x, y, 8) || slope(x, y) > 0.14 || !clearOf(x, y, curClear)) break; L = d; }
			if (L < 300) continue;
			const score = L - Math.hypot(dx, dy) / R * 60;
			if (!best || score > best.score) best = { score, from: [round(ax), round(ay)], to: [round(ax + ux * L), round(ay + uy * L)] };
		}
	}
	if (best) best.at = [round((best.from[0] + best.to[0]) / 2), round((best.from[1] + best.to[1]) / 2)];
	return best;
};
/** A gentle apron under a steep face: uphill, the slope reaches 0.6+ within 40-120 m. Faces downhill. */
const apron = (c, R) => scan(c[0], c[1], R, 8, (x, y) => {
	if (!standable(x, y, 8, 0.3)) return null;
	const gx = h(x + 3, y) - h(x - 3, y), gy = h(x, y + 3) - h(x, y - 3), g = Math.hypot(gx, gy);
	if (g < 1e-3) return null;
	const ux = gx / g, uy = gy / g;
	let steep = 0;
	for (let d = 40; d <= 120; d += 8) steep = Math.max(steep, slope(x + ux * d, y + uy * d));
	if (steep < 0.6) return null;
	return { score: Math.min(steep, 1.5) * 3 - dist([x, y], c) / R * 2, face: [x - ux * 100, y - uy * 100] };
});
/** Flat open lowland: a 15 m footprint under 0.12, 4-60 m up, no sea within 40 m, no lake ice. */
const lowland = (c, R) => scan(c[0], c[1], R, 8, (x, y) => {
	if (h(x, y) < 4 || h(x, y) > 60 || isIce(x, y, 12) || !clearOf(x, y, curClear) || nearestSea(x, y, 40).d <= 40) return null;
	const fs = footSlope(x, y, 15);
	return fs > 0.12 ? null : { score: -fs * 30 - dist([x, y], c) / R * 2 };
});
/** A bank: flat footing (0.2) with a 0.3-0.8 slope rising behind it (3 m+ in 18 m); faces away from the bank. */
const bank = (c, R) => scan(c[0], c[1], R, 6, (x, y) => {
	if (!standable(x, y, 4, 0.2)) return null;
	let best = null;
	for (const [dx, dy] of R16) {
		const s = Math.max(slope(x + dx * 10, y + dy * 10), slope(x + dx * 16, y + dy * 16));
		const rise = h(x + dx * 18, y + dy * 18) - h(x, y);
		if (s >= 0.3 && s <= 0.8 && rise > 3 && (!best || rise > best.rise)) best = { rise, dir: [dx, dy] };
	}
	return best ? { score: Math.min(best.rise, 10) / 5 - dist([x, y], c) / R * 2, face: [x - best.dir[0] * 100, y - best.dir[1] * 100] } : null;
});
/** Flat snow 60-160 m in from the coast: faces the coast. */
const nearCoast = (c, R) => scan(c[0], c[1], R, 8, (x, y) => {
	if (!standable(x, y, 10, 0.15) || isIce(x, y)) return null;
	const sea = nearestSea(x, y, 170);
	if (sea.d < 60 || sea.d > 160) return null;
	return { score: -slope(x, y) * 10 - dist([x, y], c) / R * 3, face: [x + sea.dir[0] * 100, y + sea.dir[1] * 100] };
});
/** The nearest dry, ice-free shore along 16 rays. */
const shoreOf = (x, y, maxR) => {
	let best = { d: Infinity, dir: [1, 0] };
	for (const [dx, dy] of R16) for (let d = 3; d <= Math.min(maxR, best.d); d += 3) { const px = x + dx * d, py = y + dy * d; if (!isIce(px, py) && h(px, py) >= 3) { if (d < best.d) best = { d, dir: [dx, dy] }; break; } }
	return best;
};
/** Lake ice lo-hi m out from the shore (ice all round within 10 m): faces the shore it was walked out from. */
const onLakeIce = (c, R, lo, hi) => scan(c[0], c[1], R, 6, (x, y) => {
	if (!isIce(x, y) || !isIce(x, y, 10) || slope(x, y) > 0.1 || !clearOf(x, y, curClear)) return null;
	const sh = shoreOf(x, y, hi + 10);
	if (sh.d < lo || sh.d > hi) return null;
	return { score: -dist([x, y], c) / R * 3, face: [x + sh.dir[0] * 100, y + sh.dir[1] * 100] };
});
/** A meltwater low: gentle ground at least 1.5 m below the mean of a 25 m ring. */
const iceLow = (c, R) => scan(c[0], c[1], R, 6, (x, y) => {
	if (!standable(x, y, 5, 0.25)) return null;
	let m = 0;
	for (const [dx, dy] of R16) m += h(x + dx * 25, y + dy * 25) / 16;
	const depth = m - h(x, y);
	return depth < 1.5 ? null : { score: Math.min(depth, 6) - dist([x, y], c) / R * 2 };
});
/** The flattest level disc of radius r. */
const flatDisc = (c, R, r) => scan(c[0], c[1], R, 6, (x, y) => {
	if (h(x, y) < 3 || isLava(x, y, 6) || !clearOf(x, y, curClear)) return null;
	const fs = footSlope(x, y, r);
	return fs > 0.2 ? null : { score: -fs * 20 - dist([x, y], c) / R * 2 };
});
/** A ravine a span m wide: both banks 3.5 m+ above the middle and within 4 m of each other, nothing rising over the line
 *  between them; faces from the higher bank (the root plate, local -X) to the lower. */
const ravine = (c, R, span = 34) => {
	let best = null;
	for (let dx = -R; dx <= R; dx += 4) for (let dy = -R; dy <= R; dy += 4) {
		if (dx * dx + dy * dy > R * R) continue;
		const mx = c[0] + dx, my = c[1] + dy, mz = h(mx, my);
		if (!clearOf(mx, my, curClear)) continue;
		for (let a = 0; a < 180; a += 10) {
			const ux = Math.cos(a * Math.PI / 180), uy = Math.sin(a * Math.PI / 180);
			const A = [mx - ux * span / 2, my - uy * span / 2], B = [mx + ux * span / 2, my + uy * span / 2];
			const ha = h(A[0], A[1]), hb = h(B[0], B[1]);
			const depth = Math.min(ha, hb) - mz;
			if (depth < 3.5 || Math.abs(ha - hb) > 4 || slope(A[0], A[1]) > 0.5 || slope(B[0], B[1]) > 0.5) continue;
			let ok = true;
			for (let t = 0.1; t < 0.95 && ok; t += 0.1) if (h(A[0] + (B[0] - A[0]) * t, A[1] + (B[1] - A[1]) * t) > ha + (hb - ha) * t - 1) ok = false;
			if (!ok) continue;
			const score = Math.min(depth, 15) - Math.abs(ha - hb) - Math.hypot(dx, dy) / R * 4;
			if (!best || score > best.score) best = { score, at: [round(mx), round(my)], face: ha >= hb ? B : A, depth: round(depth) };
		}
	}
	return best;
};
/** From `from` toward the nearest shore, lo-hi m out: the flattest spot; faces the shore. */
const fleeing = (from, lo, hi) => {
	const sea = nearestSea(from[0], from[1], 2500, 10);
	let best = null;
	for (let d = lo; d <= hi; d += 6) for (const off of [-12, -6, 0, 6, 12]) {
		const x = from[0] + sea.dir[0] * d - sea.dir[1] * off, y = from[1] + sea.dir[1] * d + sea.dir[0] * off;
		if (!standable(x, y, 6, 0.3)) continue;
		const score = -slope(x, y) * 10 - Math.abs(off) / 12;
		if (!best || score > best.score) best = { score, at: [round(x), round(y)], face: [x + sea.dir[0] * 100, y + sea.dir[1] * 100] };
	}
	return best;
};
/** Lower ground lo-hi m from `from`, at least `drop` m below it, flat enough for tents; faces `from`. */
const below = (from, lo, hi, drop) => scan(from[0], from[1], hi, 6, (x, y) => {
	const d = dist([x, y], from);
	if (d < lo || !standable(x, y, 8, 0.2) || h(from[0], from[1]) - h(x, y) < drop) return null;
	return { score: -slope(x, y) * 10 - d / hi, face: from };
});
/** add() for a ruled place: its spot as chosen (no settle), facing as the rule says unless the row gives one. */
const addRuled = (p, r, extra = {}) => add({ ...p, at: r.at, ...(r.face && !p.faceAt ? { faceAt: r.face } : {}), why: r.why, ...extra }, 0);

// Volcanic landmarks.
add({ kind: "crater_plume", id: "crater_plume", why: "Vulkarn's crater itself (the island's centre-west peak)", line: "Vulkarn's forge breathes smoke over the whole island.", name: "Vulkarn's Crater", at: [-786, 0], landmark: true, clear: 0 }, 0);
add({ kind: "bellows_stacks", id: "bellows", why: "fire-frost.md's spot (moved 6 m onto even ground): the three hornitos on the Solfatara's open floor facing the crater rim", line: "Three spatter chimneys the smiths worked as bellows. They still breathe fire in turn.", name: "The Bellows Stacks", at: [-620, -760], faceAt: [-1133, -341], landmark: true, clear: 40 }, 40);
// The Fallen Smith lies south of the lava river; his hammer reaches north across it (the span is measured below).
const smith = add({ kind: "fallen_smith", id: "fallen_smith", why: "Pinned by the measured lava crossing: his hammer haft spans the river where the bridge row below finds both banks", line: "A forge giant split by frost, Rime-folk spearheads still in his skull. His hammer is the only bridge over the river.", name: "The Fallen Smith", at: [-880, 990], landmark: true, clear: 45 }, 14);
add({ kind: "glasswall", id: "glasswall", why: "At the foot of the obsidian cliff face (fire-frost.md's spot, 15 m onto the ledge), facing out from the glass", line: "A cliff of black glass. Something is cut into the ledge nine metres up.", name: "Glasswall", at: [-1180, -985], faceAt: [-1180, -900], landmark: true, clear: 40 }, 25);
add({ kind: "blue_fire", id: "blue_fire", why: "The sulfur vents on the Solfatara's fumarole ground (fire-frost.md's spot, 24 m onto it)", line: "Sulfur vents the miners worked. At night their fire burns blue.", name: "The Blue Fire", at: [-430, -1160], landmark: true, clear: 30 }, 40);
{
	const r = site("slag_light", [-1880, 120], () => headland([-1880, 120], 450),
		"The smiths' beacon for ships on the west coast: the west headland's tip (the most open sea round it, on the cliff top at the water's edge), lit by the Kipuka's shrine. fire-frost.md's sea stack at -1880, +120 is open water on the built island", () => settle(-1560, 120, 50, { maxSlope: 0.45 }));
	addRuled({ kind: "slag_light", id: "slag_light", line: "The smiths' beacon on the west headland. It burns again once the Kipuka's shrine is kindled.", name: "The Slag Light", landmark: true, clear: 20 }, r);
}
add({ kind: "kipuka", id: "kipuka", why: "Pinned by the Kipuka shrine's clearing in the lava field", line: "Old forest the lava flowed around: the only living green in the Volcanic.", name: "Ash Pine Kipuka", at: [-1285, 610], clearing: world.shrines.list.find(q => q.id === "kipuka").at, landmark: true, clear: 70 }, 20, { maxSlope: 0.5, skip: "kipuka" });
add({ kind: "wisp_spire", id: "wisp_spire", why: "Pinned by Ignessa's arena (the spire behind her ring)", line: "A twisted spire behind Ignessa's ring, crowned with her wisps.", name: "Wisp Spire", at: [-1036, 742], faceAt: [-1036, 784], landmark: true, clear: 12 }, 8, { maxSlope: 0.6, skip: "Ignessa" });
// Tundra landmarks.
const rim = villages.rimehold;
const gateDir = (() => { const dx = 804 - rim[0], dy = 0 - rim[1], d = Math.hypot(dx, dy); return [dx / d, dy / d]; })();
add({ kind: "tusk_gate", id: "tusk_gate", why: "Set from Rimehold village toward the glacier: the village's gate", line: "The Rime-folk's gate of mammoth tusks, facing the glacier.", name: "The Tusk Gate", at: [rim[0] + gateDir[0] * 72, rim[1] + gateDir[1] * 72], faceAt: [804, 0], landmark: true, clear: 14 }, 12, { maxSlope: 0.4 });
add({ kind: "great_inuksuk", id: "great_inuksuk", why: "fire-frost.md's spot on the rise over the Inuksuk Road, its arm pointing at the glacier", line: "Nine metres of stacked stone. Its arm points at the glacier.", name: "The Great Inuksuk", at: [1150, 120], faceAt: [804, 0], landmark: true, clear: 20 }, 30);
add({ kind: "frozen_whaler", id: "frozen_whaler", why: "In the ice shelf's old shoreline (fire-frost.md's spot, 39 m onto the shelf), prow toward the old sea", line: "A Rime-folk longboat locked in the ice from when the shelf was sea.", name: "The Frozen Whaler", at: [520, -420], faceAt: [700, -560], landmark: true, clear: 30 }, 40, { maxSlope: 0.45 });
add({ kind: "weeping_seracs", id: "weeping_seracs", why: "On the shelf's edge below the glacier bowl (fire-frost.md's spot), facing the bowl", line: "A wall of blue ice towers. Listen for the crack before one falls.", name: "The Weeping Seracs", at: [640, 250], faceAt: [804, 0], landmark: true, clear: 45 }, 30, { maxSlope: 0.45 });
{
	const r = site("fox_pingo", [1700, 900], () => lowland([1700, 900], 450),
		"Pingos rise from flat permafrost lowland (drained lake beds): the flattest open snow of the Snow Flats near the design spot, away from the sea and the lake ice, under open sky for the aurora", () => settle(1450, 900, 60));
	addRuled({ kind: "fox_pingo", id: "fox_pingo", line: "An ice-cored mound where the aurora foxes gather.", name: "The Fox Circle Pingo", landmark: true, clear: 25 }, r);
}
add({ kind: "pale_pines", id: "pale_pines", why: "Pinned by the Pale Hunter's arena", line: "Dead white pines round the Pale Hunter's ground. No light reaches here.", name: "The Pale Pines", at: [1034, 834], landmark: true, clear: 0 }, 0);
add({ kind: "ice_arch", id: "ice_arch", why: "Pinned by Grimjaw's arena over the river ice", line: "A natural ice bridge over the river ice. Something lives beneath it.", name: "The Ice Arch", at: [1064, -766], faceAt: [1400, -766], landmark: true, clear: 0 }, 0);

// Volcanic story sites.
add({ kind: "plough_trench", id: "plough_trench", why: "On Cinderhorn's loop (creatures.json roam): the bull ploughs his own trench, the basalt pillars along it", name: "The Plough Trench", at: [-1563, 505], from: [-1572, 400], to: [-1556, 612], clear: 20 }, 0);
{
	const fire = places.find(q => q.id === "blue_fire").at;
	const r = site("miners_grave", [-470, -1110], () => fleeing(fire, 60, 140),
		"The miners died fleeing an eruption toward the shore: on the line from the Blue Fire's pits to the nearest shore, 60-140 m out, the bodies facing the sea they never reached", () => settle(-470, -1110, 40));
	addRuled({ kind: "miners_grave", id: "miners_grave", name: "The Miners' Grave", clear: 22 }, r);
}
{
	const fire = places.find(q => q.id === "blue_fire").at;
	const r = site("miners_camp", [-380, -1240], () => below(fire, 70, 170, 6),
		"\"The miners' camp below\" the Blue Fire: the nearest flat lower ground 70-170 m from the pits (6 m+ below them, out of the fumes), the tents facing the work", () => settle(-380, -1240, 40));
	addRuled({ kind: "miners_camp", id: "miners_camp", name: "The Miners' Camp", clear: 18 }, r);
}
{
	const line = [camp("Volcanic camp 2"), [-900, 1180], [-1200, 690], [-1100, 430], [-1080, -100], villages.emberwick, [-1450, -640], camp("Volcanic camp 1")];
	const r = site("charred_caravan", [-1650, -700], () => onRoad([-1650, -700], line, [-13, -1296]),
		"Smith wagons burn where they stood: on the Slag-Cairn road where it passes nearest the design spot, the guards' blades pointing toward the north pass the attack came from", () => settle(-1450, -700, 40));
	addRuled({ kind: "charred_caravan", id: "charred_caravan", name: "The Charred Caravan", clear: 22 }, r);
}
add({ kind: "tree_molds", id: "tree_molds", why: "In the old lava field beside the Kipuka (fire-frost.md's spot): the lava came through a living forest", name: "The Tree-Mold Field", at: [-1100, 500], clear: 30 }, 40);
add({ kind: "crater_stair", id: "crater_stair", why: "At the crater wall's foot (fire-frost.md's spot), climbing toward the rim", name: "The Crater Stair", at: [-1050, -120], faceAt: [-786, 0], clear: 20 }, 25, { maxSlope: 0.6 });
add({ kind: "wardens_quarry", id: "wardens_quarry", why: "Pinned by the Obsidian Warden's arena: the quarry face beside it", name: "The Warden's Quarry", at: [-1066 - 38, -716 - 10], faceAt: [-1066, -716], clear: 14 }, 10, { maxSlope: 0.45, skip: "Obsidian Warden" });
{
	const r = site("shard_field", [-1450, -500], () => apron([-1450, -500], 500),
		"Obsidian blades are shed by the ridges: the gentle apron at the foot of the steepest obsidian face near the design spot, spread downhill from it", () => settle(-1350, -1100, 40));
	addRuled({ kind: "shard_field", id: "shard_field", name: "The Shard Field", clear: 45 }, r);
}
{
	const r = site("ridge_watch", [-180, -600], () => overlook([-180, -600], 700, [-13, -1296], [-786, 0], (x, y) => x < -150, 12),
		"The smiths' watchtower: the volcanic-side crest with an unbroken sight line from its roof to the north pass (250-800 m off), seeing the crater too (it reveals the Crater Highlands); its slits face the pass", () => settle(-420, -1330, 40, { maxSlope: 0.3 }));
	addRuled({ kind: "ridge_watch", id: "ridge_watch", name: "The Ridge Watch", clear: 14 }, r);
}
add({ kind: "kilnmite_hill", id: "kilnmite_hill", why: "A mound of chewed ore on the Obsidian Ridges' lower ground (fire-frost.md's spot, 50 m onto even ground)", name: "The Kilnmite Hill", at: [-1300, -1260], clear: 20 }, 40);
{
	const r = site("acid_pool", [-230, -980], () => hollow([-560, -950], 450, 0.8, "V4"),
		"Acid run-off from the fumarole fields pools where it can't drain: the deepest closed hollow among the Solfatara's fumarole fields (ground 0.8 m+ higher all round at 30 m: a shallow pan, as the real ones are; fire-frost.md's -230, -980 is on the ridge face), the cart rolled down its side", () => settle(-470, -980, 60, { maxSlope: 0.2 }));
	addRuled({ kind: "acid_pool", id: "acid_pool", name: "The Acid Pool", clear: 22 }, r);
}
add({ kind: "steam_ground", id: "steam_ground_v", why: "On the Solfatara's fumarole ground (fire-frost.md's spot)", name: "The Steam Ground", at: [-520, -620], clear: 22 }, 40);
// Tundra story sites.
add({ kind: "tusker_wallow", id: "tusker_wallow", why: "On Old Tusker's loop (creatures.json roam): he wallows on his own round", name: "Old Tusker's Wallow", at: [1050, 300], clear: 20 }, 20);
{
	const r = site("tusk_midden", [1600, -900], () => bank([1600, -900], 300),
		"Tusks stacked by the dozen half-frozen in a bank: flat footing against a slope that rises behind it, the stacked face looking out from the bank", () => settle(1450, -900, 50, { maxSlope: 0.5 }));
	addRuled({ kind: "tusk_midden", id: "tusk_midden", name: "The Tusk Midden", clear: 18 }, r);
}
{
	const r = site("sledge_line", [1700, 300], () => nearCoast([1700, 300], 420),
		"Three sledges abandoned in a line toward the coast: flat snow 60-160 m inland near the design spot, the line pointing at the nearest shore", () => settle(1450, 650, 50));
	addRuled({ kind: "sledge_line", id: "sledge_line", name: "The Sledge Wreck Line", clear: 25 }, r);
}
{
	const r = site("fishing_camp", [760, -1500], () => onLakeIce([760, -1500], 600, 15, 45),
		"Ice-fishers walk a short way out: lake ice 15-45 m from the shore nearest the design spot, the windbreak's back to the open ice and the camp facing the shore", () => settle(760, -1000, 60, { maxSlope: 0.15 }));
	addRuled({ kind: "fishing_camp", id: "fishing_camp", name: "The Ice-Fishing Camp", clear: 18 }, r);
}
{
	const r = site("drowned_sledge", [1250, -1100], () => onLakeIce([1250, -1100], 650, 20, 120),
		"A sledge and dogs frozen under clear ice where they went through: lake ice 20-120 m out near the design spot; it points at the nearest thin ice (set below, once the thin ice is placed)", () => settle(1150, -850, 60, { maxSlope: 0.15 }));
	addRuled({ kind: "drowned_sledge", id: "drowned_sledge", name: "The Drowned Sledge", clear: 15 }, r);
}
add({ kind: "hunter_camp", id: "hunter_camp", why: "Pinned by the Pale Hunter's arena: the tent at the edge of his dead pines", name: "The Pale Hunter's Camp", at: [990, 905], faceAt: [1034, 834], clear: 14 }, 15, { maxSlope: 0.5 });
add({ kind: "hunters_blind", id: "hunters_blind", why: "Over a game trail in the Pine Hollows (fire-frost.md's spot)", name: "The Hunter's Blind", at: [700, 1300], faceAt: [600, 1300], clear: 12 }, 40);
{
	const r = site("giant_pine", [600, 700], () => ravine([600, 700], 280, 34),
		"The only bridge over a ravine: a 34 m pine lying bank to bank where both banks stand 3.5 m+ over the gully between them, its root plate on the higher bank", () => settle(600, 700, 40));
	addRuled({ kind: "giant_pine", id: "giant_pine", name: "The Fallen Giant Pine", clear: 25 }, r);
}
{
	const r = site("glacimaw_boneyard", [380, 480], () => flatDisc([380, 480], 280, 10),
		"Where the Glacimaw surfaces through the snow: the flattest level 20 m disc of the ice shelf near the design spot, the bone ring round churned snow", () => settle(420, 500, 90));
	addRuled({ kind: "glacimaw_boneyard", id: "glacimaw_boneyard", name: "The Glacimaw Boneyard", clear: 18 }, r);
}
add({ kind: "claw_scars", id: "claw_scars", why: "Down the cliff where Skaldrith lands (fire-frost.md's spot), facing the bowl", name: "The Wyrm Claw Scars", at: [650, -300], faceAt: [804, 0], clear: 12 }, 30, { maxSlope: 0.6 });
{
	const r = site("watch_cairn", [300, -700], () => overlook([300, -700], 520, [-13, -1296], [804, 0], (x, y) => x > 150, 3),
		"The Rime-folk's mirror of the Ridge Watch: the tundra-side crest with an unbroken sight line from the cairn's wall to the north pass, seeing the glacier bowl too (it reveals the Glacier Plateau); spear rack facing the pass", () => settle(380, -720, 110, { maxSlope: 0.35 }));
	addRuled({ kind: "watch_cairn", id: "watch_cairn", name: "The Watch Cairn", clear: 12 }, r);
}
{
	const r = site("moulin", [700, -250], () => iceLow([700, -250], 260),
		"Meltwater finds the lowest point of the ice shelf and drills down: a gentle low 1.5 m+ below the ground round it near the design spot, the channels running in", () => settle(560, -330, 30, { maxSlope: 0.3 }));
	addRuled({ kind: "moulin", id: "moulin", name: "The Moulin of Voices", clear: 12 }, r);
}
add({ kind: "crevasse_maze", id: "crevasse_maze", why: "Below the Crevasse Shrine on the shelf (fire-frost.md's spot)", name: "The Crevasse Maze", at: [440, -70], faceAt: [450, 60], clear: 60 }, 30, { maxSlope: 0.3, skip: "crevasse" });
add({ kind: "steam_ground", id: "steam_ground_t", why: "At the south pass where the Fire's heat leaks through (fire-frost.md's spot)", name: "The Steam Ground", at: [180, 1250], clear: 22 }, 60, { maxSlope: 0.45 });

// The Fallen Smith's hammer: his arm reaches from the skeleton to the lava's south bank, the haft spans the river and
// the head sinks into the north bank (bridge.from = the haft's south end, bridge.to = its north end, metres).
{
	const [sx, sy] = smith.at;
	let near = null, far = null;
	for (let y = sy - 10; y > sy - 220; y -= 2) {
		const l = isLava(sx, y);
		if (l && near === null) near = y + 2;
		if (near !== null && !l && !isLava(sx, y - 6) && !isLava(sx, y - 12)) { far = y; break; }
	}
	if (near === null || far === null) console.warn("fallen smith: no river crossing found north of the skeleton");
	else smith.bridge = { from: [sx, round(near + 5)], to: [sx, round(far - 6)] };
}

// ---------------------------------------------------------------------------------------------------- roads
const road = (pts, step) => {
	const out = [];
	for (let i = 1; i < pts.length; i++) {
		const [ax, ay] = pts[i - 1], [bx, by] = pts[i], L = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.round(L / step));
		for (let k = i === 1 ? 0 : 1; k <= n; k++) out.push([ax + (bx - ax) * k / n, ay + (by - ay) * k / n]);
	}
	return out;
};
const waymarks = (kind, pts, step) => {
	// A mark that would stand in a camp, a village or on a shrine is left out: the road meets them at their edge.
	const raw = road(pts, step).map(([x, y]) => settle(x, y, 22, { maxSlope: 0.45, quiet: true })).filter(Boolean);
	return raw.map((p, i) => {
		const n = raw[Math.min(raw.length - 1, i + 1)], b = raw[Math.max(0, i - 1)];
		const to = i < raw.length - 1 ? n : [p[0] + (p[0] - b[0]), p[1] + (p[1] - b[1])];
		return { kind, at: p, yaw: round(Math.atan2(to[1] - p[1], to[0] - p[0]) * 180 / Math.PI), zone: zoneAt(p[0], p[1]) };
	});
};
const ember = villages.emberwick;
const roads = [
	{ id: "slag_road", name: "The Slag-Cairn Road", marks: waymarks("slag_cairn", [camp("Volcanic camp 2"), [-900, 1180], [-1200, 690], [-1100, 430], [-1080, -100], ember, [-1450, -640], camp("Volcanic camp 1")], 175) },
	{ id: "inuksuk_road", name: "The Inuksuk Road", marks: waymarks("inuksuk", [camp("Tundra camp 3"), [1180, 900], rim, [1150, 160], [1180, -300], [1150, -700], camp("Tundra camp 2")], 200) },
];
for (const r of roads) for (const m of r.marks) keepOut.push({ x: m.at[0], y: m.at[1], r: 10, what: r.id });

// Sastrugi: fields of wind-carved snow ridges on the open snow, every ridge lying along the prevailing katabatic wind off
// the glacier, so their long axis points at the bowl (a compass you read off the ground, Ghost of Tsushima's wind).
const sastrugi = [];
{
	let seed = 99;
	const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	for (let tries = 0; tries < 4000 && sastrugi.length < 16; tries++) {
		const x = 1150 + rnd() * 500, y = -1500 + rnd() * 3000;
		const z = zoneAt(x, y);
		if (z !== "T1" && z !== "T3") continue;
		if (h(x, y) < 6 || slope(x, y) > 0.2 || isIce(x, y) || !clearOf(x, y, 10)) continue;
		if (sastrugi.some(q => Math.hypot(q.at[0] - x, q.at[1] - y) < 160)) continue;
		const yaw = round(Math.atan2(0 - y, 804 - x) * 180 / Math.PI);
		sastrugi.push({ kind: "sastrugi", at: [round(x), round(y)], yaw, zone: z });
	}
}

// ---------------------------------------------------------------------------------------------------- hazards
// Thin ice: dark patches on the lake ice, 60-120 m apart, kept off the camps' and the shrine's ground.
const thinIce = [];
{
	const want = 8;
	let tries = 0, seed = 7;
	const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	while (thinIce.length < want && tries++ < 5000) {
		const x = 450 + rnd() * 1000, y = -1100 + rnd() * 600;
		if (!isIce(x, y) || !isIce(x, y, 18) || slope(x, y) > 0.12 || !clearOf(x, y, 10)) continue;
		if (thinIce.some(t => Math.hypot(t.at[0] - x, t.at[1] - y) < 70)) continue;
		thinIce.push({ at: [round(x), round(y)], r: round(6 + rnd() * 4) });
	}
}
// The drowned sledge's window points at the nearest thin patch (the story: read the colour).
{
	const ds = places.find(q => q.id === "drowned_sledge");
	let near = null;
	for (const t of thinIce) if (!near || dist(t.at, ds.at) < dist(near.at, ds.at)) near = t;
	if (near) ds.yaw = round(Math.atan2(near.at[1] - ds.at[1], near.at[0] - ds.at[0]) * 180 / Math.PI) - 20;   // the team leads 20 degrees off the sledge's axis
}
const fumaroleFields = [ { at: [-560, -1020], n: 4 }, { at: [-560, -880], n: 3 }, { at: [-680, -480], n: 3 } ];
const fumaroles = [];
for (const f of fumaroleFields) {
	const c = settle(f.at[0], f.at[1], 60);
	for (let i = 0; i < f.n; i++) {
		const a = i * 137.5 * Math.PI / 180, r = 9 + i * 7;
		fumaroles.push({ at: settle(c[0] + r * Math.cos(a), c[1] + r * Math.sin(a), 8, { maxSlope: 0.45 }) });
	}
}
const geysers = [];
for (const g of [[-600, -1180], [-540, -1300], [-470, -880], [-720, -1300]]) geysers.push({ at: settle(g[0], g[1], 50, { maxSlope: 0.25 }) });
const katabatic = { centre: [804, 0], inner: 180, outer: 340 };

// ---------------------------------------------------------------------------------------------------- kit POIs
const kitRules = {
	V1: { smith_grave: 3, glowmoth_roost: 2, basalt_ring: 2, miner_leanto: 1, giant_tool: 1 },
	V2: { hornito: 3, basalt_ring: 2, glowmoth_roost: 2, giant_tool: 2, smith_grave: 1, sulfur_vent: 1 },
	V3: { smith_grave: 2, basalt_ring: 1, giant_tool: 1, glowmoth_roost: 1 },
	V4: { sulfur_vent: 3, hornito: 2, miner_leanto: 2, smith_grave: 1 },
	V5: { giant_tool: 2, basalt_ring: 2, smith_grave: 1, hornito: 1 },
	T1: { tent_ring: 3, erratic: 2, snowpuff_nest: 2, cache_pole: 2, small_pingo: 1, frozen_carcass: 1 },
	T2: { tent_ring: 2, cache_pole: 2, frozen_carcass: 2, erratic: 1 },
	T3: { small_pingo: 2, frozen_carcass: 2, cache_pole: 1, erratic: 1, tent_ring: 1 },
	T4: { erratic: 3, frozen_carcass: 2, small_pingo: 1 },
	T5: { erratic: 2, frozen_carcass: 1, cache_pole: 1 },
};
const taken = keepOut.map(k => [k.x, k.y]);
for (const q of sastrugi) taken.push(q.at);
const kit = [];
{
	const W = walk, cell = W.cell / 100, ox = W.origin / 100;
	const cands = [];
	for (const [jStr, row] of Object.entries(W.rows)) {
		const j = +jStr;
		for (let i = 0; i < row.length; i++) {
			const c = row[i];
			if (c !== "g" && c !== "f" && c !== "w") continue;
			const x = ox + (i + 0.5) * cell, y = ox + (j + 0.5) * cell;
			if (Math.abs(y) > 1900 || Math.abs(x) > 1900 || Math.abs(x) < 200) continue;
			cands.push([x, y]);
		}
	}
	// A fixed shuffle so the deal is the same each run.
	let seed = 1234567;
	const rnd = () => { seed = (seed * 48271) % 2147483647; return seed / 2147483647; };
	for (let i = cands.length - 1; i > 0; i--) { const k = Math.floor(rnd() * (i + 1)); [cands[i], cands[k]] = [cands[k], cands[i]]; }
	const deck = {}, dealt = {};
	for (const [z, w] of Object.entries(kitRules)) { deck[z] = []; for (const [k, n] of Object.entries(w)) for (let q = 0; q < n; q++) deck[z].push(k); dealt[z] = 0; }
	const nearest = (x, y) => { let d = 1e9; for (const [tx, ty] of taken) d = Math.min(d, Math.hypot(tx - x, ty - y)); return d; };
	for (const [x, y] of cands) {
		if (nearest(x, y) <= 125) continue;
		const spot = settle(x, y, 30, { maxSlope: 0.4, allowIce: false, quiet: true });
		if (!spot) continue;
		if (h(spot[0], spot[1]) < 3 || isIce(spot[0], spot[1]) || isLava(spot[0], spot[1], 10) || slope(spot[0], spot[1]) > 0.4) continue;
		if (nearest(spot[0], spot[1]) < 150 && kit.some(k => Math.hypot(k.at[0] - spot[0], k.at[1] - spot[1]) < 150)) continue;
		const z = zoneAt(spot[0], spot[1]);
		if (!deck[z]) continue;
		// The next kind in the zone's deck that isn't the same as a kit place within 400 m.
		let kind = null;
		for (let t = 0; t < deck[z].length; t++) {
			const k = deck[z][(dealt[z] + t) % deck[z].length];
			if (!kit.some(q => q.kind === k && Math.hypot(q.at[0] - spot[0], q.at[1] - spot[1]) < 400)) { kind = k; dealt[z] += t + 1; break; }
		}
		if (!kind) { kind = deck[z][dealt[z] % deck[z].length]; dealt[z]++; }
		kit.push({ kind, at: spot, yaw: Math.round(rnd() * 360), v: Math.floor(rnd() * 4), zone: z });
		taken.push(spot);
	}
}

// ---------------------------------------------------------------------------------------------------- write
const out = {
	_comment: "Generated by Tools/terrain/island_places.mjs (cooks/cookpass-world.md): the Fire and Frost places on the Old Island, in metres (+x east, +y south). zones are tested in order (rings first); places/roads/kit are built at runtime by UWRIslandPlaces on every machine (WRIslandPlaces*.cpp) on the ground traced under each; hazards are the server's (WRIslandHazards.cpp, numbers in tuning hazards.ff). Re-run the tool after the island or world.json's villages move.",
	zones, places, roads, kit, ground: sastrugi,
	hazards: { thinIce, fumaroles, geysers, katabatic },
};
fs.writeFileSync(path.join(root, "Data/island_places.json"), JSON.stringify(out, null, 1).replace(/\n\s+(-?[\d.]+),?\n\s+(-?[\d.]+)\n\s+\]/g, " $1, $2 ]"));
const byZone = {};
for (const p of [...places, ...kit, ...roads.flatMap(r => r.marks)]) byZone[p.zone] = (byZone[p.zone] || 0) + 1;
console.log(`places ${places.length}, waymarks ${roads.reduce((n, r) => n + r.marks.length, 0)}, kit ${kit.length}, thin ice ${thinIce.length}, fumaroles ${fumaroles.length}, geysers ${geysers.length}`);
console.log("by zone", byZone);
console.log("fallen smith bridge", JSON.stringify(smith.bridge));
for (const r of ruled) console.log("site", r.id.padEnd(18), r.fit ? `moved ${r.moved} m from fire-frost.md` : "NO FIT: the settled spot kept");
for (const p of places) console.log(p.id.padEnd(18), p.at.join(", ").padEnd(16), p.zone, "z", h(p.at[0], p.at[1]).toFixed(0), "slope", slope(p.at[0], p.at[1]).toFixed(2));
