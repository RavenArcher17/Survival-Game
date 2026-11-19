// The round island's ground (plan phase 3 "One big world", step 2: the first slice, Hearthmoot with its meadow, lake and
// main roads, on a first pass of the whole island). Reads the signed-off layout from round_island.mjs (the same coast,
// wandering borders, meadow, lake, stream, roads and forts) and writes one runtime heightfield land for the new map:
//   Data/lands/island.json            the land (WRLand: map L_Wildreach, zone letters per land, surfaces, far detail levels)
//   Data/lands/island_ground.bin      4 m samples over ±6.6 km, zlib: uint16 heights (offset + m x 16) then a surface byte each
//   Data/sea_mask_L_Wildreach.json    where the open sea is (the sea and the water you swim in are drawn from it)
//   Saved/RoundIsland/ground.png      a shaded picture of it from above, to check by eye
// Every land's ground here is a first pass (its broad shape: Storm's peaks, the Desert's dunes and mesas, the Tundra's
// plateau, the Swamp's pools, the volcano); each land's slice replaces its part with its own design later.
//   node Tools/terrain/round_ground.mjs
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import * as RI from "./round_island.mjs";
import * as PLAN from "./hearthmoot_plan.mjs";

const { LANDS, lands, coastR, meadowR, warpDeg, fbm, pt, bearingOf, norm, dAng, gauss, RAD, CITY_WALL, LAKE, lakeOutline, stream, trunks, islets, edgeOf, FIRST_DEG, inPoly } = RI;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const t0 = Date.now();
const log = (...a) => console.log(`[${((Date.now() - t0) / 1000).toFixed(1)} s]`, ...a);

// ---- inputs ------------------------------------------------------------------------------------------------------
const STEP = 4, HALF = 6600, N = HALF * 2 / STEP + 1;       // samples 4 m apart over ±6.6 km
const UNITS = 16, OFFSET = 32768;
const MAP = "L_Wildreach";
const LAKE_LEVEL = 16;          // m: the meadow lake's water
const MEADOW_H = 18;            // m: the meadow's floor
const CITY_TOP = 41;            // m: the hill Hearthmoot's walls stand on
const CITY_FLAT = 345;          // m: the flat top (the walls are at 300)
const BLEND_W = 250;            // m either side of a border where the two lands' shapes mix
const MEADOW_RISE = 450;        // m: from the meadow's edge up into the lands
const ROAD_W = 4, TRUNK_W = 6;  // m: half widths
const ROAD_GRADE = 0.11;        // steepest a road climbs
const SEA_FLOOR = -30;          // m: open sea floor below this isn't built (the sea covers it; the deep floor is dark)
const LAND_ABOVE = 1.5;         // m: a sea-mask cell whose lowest corner is above this is dry land
// Each land's first-pass character (its own slice redoes it): a base, a highland peak inland, and its own shapes.
const LAND_INDEX = Object.fromEntries(LANDS.map((l, i) => [l.id, i]));
const CLIFFS = [[315.3, 6], [266.6, 7], [94.2, 7]];          // bearings (and widths) where the coast is cliff, not beach
const VOLCANO = { bearing: 266.6, inFromCoast: 1500, height: 430, radius: 1400, crater: 200, craterDepth: 90 };

// ---- helpers --------------------------------------------------------------------------------------------------------
const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
const TAB = 7200;
const tab = (f) => { const a = new Float32Array(TAB + 1); for (let i = 0; i <= TAB; i++) a[i] = f(i * 360 / TAB); return a; };
const look = (a, deg) => { const f = norm(deg) / 360 * TAB, i = Math.floor(f); return a[i] + (a[i + 1] - a[i]) * (f - i); };
const coastTab = tab(coastR), meadowTab = tab(meadowR);
const coastAt = (deg) => look(coastTab, deg), meadowAt = (deg) => look(meadowTab, deg);
const cum = [...LANDS.map((_, i) => norm(edgeOf(i) - FIRST_DEG)), 360];
const cliffAt = (deg) => Math.max(...CLIFFS.map(([b, w]) => gauss(deg, b, w)));
const volcano = pt(VOLCANO.bearing, coastR(VOLCANO.bearing) - VOLCANO.inFromCoast);

// Land k's height at (x, y): c = 0 at the coast, 1 at the meadow's edge.
const prof = (c) => smooth(0, 0.6, c) * (1 - 0.85 * smooth(0.65, 1, c));
function landH(k, x, y, c) {
	const p = prof(c), s = 100 + k * 17;
	switch (LANDS[k].id) {
		case "storm_peaks": {
			// The peaks stand in the middle of the land; towards the meadow it falls to foothills the roads can cross.
			const ridge = Math.max(0, 1 - Math.abs(fbm(x / 1100, y / 1100, s, 5)) * 1.6);
			const mid = smooth(0.1, 0.45, c) * (1 - smooth(0.55, 0.85, c));
			return 25 + 200 * p + 380 * ridge * ridge * mid + 12 * fbm(x / 140, y / 140, s + 1, 3);
		}
		case "desert": {
			const dune = 5 * (0.5 + 0.5 * Math.sin((x * 0.5 + y * 0.866) / 38 + 4 * fbm(x / 500, y / 500, s + 1, 3))) * smooth(0.04, 0.25, c);
			const m = fbm(x / 1300, y / 1300, s + 2, 4);
			const mesa = 45 * smooth(0.2, 0.26, m) * smooth(0.15, 0.4, c);
			return 14 + 55 * p + dune + mesa + 3 * fbm(x / 200, y / 200, s + 3, 2);
		}
		case "forest": return 12 + 95 * p + 28 * fbm(x / 700, y / 700, s, 4) * smooth(0, 0.2, c) + 5 * fbm(x / 160, y / 160, s + 1, 3);
		case "tundra": {
			const h = fbm(x / 900, y / 900, s + 3, 3);
			return 18 + 120 * smooth(0, 0.4, c) * (1 - 0.7 * smooth(0.75, 1, c)) + 10 * fbm(x / 600, y / 600, s, 4) - 10 * smooth(0.3, 0.38, h) * smooth(0.1, 0.3, c);
		}
		case "swamp": {
			const q = fbm(x / 260, y / 260, s + 4, 3);
			return 1.6 + 9 * p + 2 * fbm(x / 300, y / 300, s, 3) - 3.2 * smooth(0.12, 0.24, q) * (1 - smooth(0.7, 0.95, c));
		}
		case "ocean": return 4 + 32 * p + 8 * fbm(x / 500, y / 500, s, 4) * smooth(0, 0.15, c);
		case "graveyard": return 10 + 72 * p + 20 * fbm(x / 500, y / 500, s, 4) * smooth(0, 0.2, c) + 5 * fbm(x / 120, y / 120, s + 1, 3);
		case "wasteland": {
			const v = (1 - Math.abs(fbm(x / 900, y / 900, s, 4))) * 5, f = v - Math.floor(v);
			const terrace = (Math.floor(v) + smooth(0.4, 0.6, f)) / 5;
			const n = Math.abs(fbm(x / 700, y / 700, s + 5, 3));
			return 12 + 78 * p + 28 * terrace * smooth(0, 0.25, c) - 18 * Math.max(0, 1 - n / 0.03) * smooth(0.1, 0.3, c);
		}
		case "volcanic": return 15 + 110 * p + 22 * fbm(x / 600, y / 600, s, 4) * smooth(0, 0.2, c);
	}
	return 10 + 50 * p;
}

// ---- 1. every sample's shape: the lands blended across their borders, the meadow, the coast and the sea floor ------
const H = new Float32Array(N * N), S = new Uint8Array(N * N), OWN = new Int8Array(N * N);
const SURF = { bed: 1, bank: 2, sand: 3, shore_rock: 6, road: 7, main_road: 10, plaza: 11, lake_bed: 12, cobble: 13,
	wheat: 14, wheat2: 15, barley: 16, barley2: 17, greens: 18, greens2: 19, ploughed: 20, ploughed2: 21, fallow: 22, fallow2: 23 };
for (let j = 0; j < N; j++) {
	const y = -HALF + j * STEP;
	for (let i = 0; i < N; i++) {
		const x = -HALF + i * STEP, r = Math.hypot(x, y), deg = bearingOf(x, y), k = j * N + i;
		const cR = coastAt(deg), mR = meadowAt(deg);
		const dc = cR - r + 40 * fbm(x / 300, y / 300, 9, 3);           // metres inland of the coast (a ragged line)
		const c = Math.min(1, Math.max(0, 1 - (r - mR) / (cR - mR)));
		// The two lands either side of the nearest border, mixed over BLEND_W.
		const wd = norm(deg + warpDeg(x, y, Math.max(r, 1)) - FIRST_DEG);
		let a = 0; while (a < 8 && wd >= cum[a + 1]) a++;
		const toLow = (wd - cum[a]) * RAD * r, toHigh = (cum[a + 1] - wd) * RAD * r;
		let h = landH(a, x, y, c);
		const near = Math.min(toLow, toHigh);
		if (near < BLEND_W && r > 300) {
			const b = toLow < toHigh ? (a + 8) % 9 : (a + 1) % 9;
			h = lerp(h, landH(b, x, y, c), 0.5 * (1 - near / BLEND_W));
		}
		OWN[k] = a;
		// The volcano rises out of the Volcanic near its cape (and its flanks run onto the neighbours').
		const dv = Math.hypot(x - volcano[0], y - volcano[1]);
		if (dv < VOLCANO.radius) {
			h += VOLCANO.height * Math.pow(1 - dv / VOLCANO.radius, 1.3) * (1 + 0.08 * fbm(x / 120, y / 120, 77, 3));
			if (dv < VOLCANO.crater) h -= VOLCANO.craterDepth * (1 - (dv / VOLCANO.crater) ** 2);
		}
		// The meadow: a green basin round the city's hill; the lands rise out of it over MEADOW_RISE.
		if (r < mR + MEADOW_RISE) {
			let hm = MEADOW_H + 2.2 * fbm(x / 420, y / 420, 21, 3) + 22 * (1 - smooth(250, 700, r));
			hm = lerp(hm, CITY_TOP, 1 - smooth(CITY_FLAT, CITY_FLAT + 40, r));
			h = lerp(hm, h, smooth(0, MEADOW_RISE, r - mR));
		}
		// The coast: beaches, cliffs where the land wants them, then the shelving sea floor and the reefs.
		const cliff = cliffAt(deg), beachW = 70 * (1 - cliff) + 8;
		if (dc >= 0) {
			h = lerp(0.9 + 0.4 * fbm(x / 60, y / 60, 5, 2), h, smooth(0, beachW, dc));
			if (dc < beachW * 0.8 && cliff < 0.5) S[k] = SURF.sand;
			else if (dc < 14 && cliff >= 0.5) S[k] = SURF.shore_rock;
		} else {
			const o = -dc, fall = 1 + 2 * cliff;
			h = -(0.5 + 2.5 * fall * smooth(0, 300, o) + 7 * smooth(300, 1200, o) + 30 * smooth(1200, 2600, o));
			if (o > 250 && o < 1250) h += 2.2 * Math.max(0, fbm(x / 80, y / 80, 31, 3));   // the reefs
			if (o < 300) S[k] = SURF.sand;
		}
		H[k] = h;
	}
}
// The islets off the coast.
for (const s of islets) {
	const [ix, iy] = s.at, R = s.radius;
	for (let j = Math.max(0, Math.floor((iy - 1.6 * R + HALF) / STEP)); j < N; j++) {
		const y = -HALF + j * STEP; if (y > iy + 1.6 * R) break; if (y < iy - 1.6 * R) continue;
		for (let i = Math.max(0, Math.floor((ix - 1.6 * R + HALF) / STEP)); i < N; i++) {
			const x = -HALF + i * STEP; if (x > ix + 1.6 * R) break;
			const d = Math.hypot(x - ix, y - iy) / (R * (1 + 0.3 * fbm(x / 60, y / 60, 55, 2)));
			if (d < 1) { const k = j * N + i; H[k] = Math.max(H[k], 6 * (1 - d * d) + 0.5); if (d > 0.6) S[k] = SURF.shore_rock; }
		}
	}
}
log("shapes done");

// ---- 2. rasterised lines: distance and position along every road and the stream ----------------------------------
// For each polyline, the samples within `reach` of it learn their distance and how far along (metres) the line they are.
function raster(points, reach, D, A) {
	let along0 = 0;
	for (let s = 1; s < points.length; s++) {
		const [ax, ay] = points[s - 1], [bx, by] = points[s], L = Math.hypot(bx - ax, by - ay) || 1e-6;
		const i0 = Math.max(0, Math.floor((Math.min(ax, bx) - reach + HALF) / STEP)), i1 = Math.min(N - 1, Math.ceil((Math.max(ax, bx) + reach + HALF) / STEP));
		const j0 = Math.max(0, Math.floor((Math.min(ay, by) - reach + HALF) / STEP)), j1 = Math.min(N - 1, Math.ceil((Math.max(ay, by) + reach + HALF) / STEP));
		for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
			const x = -HALF + i * STEP, y = -HALF + j * STEP;
			const t = Math.min(1, Math.max(0, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / (L * L)));
			const d = Math.hypot(x - (ax + t * (bx - ax)), y - (ay + t * (by - ay)));
			const k = j * N + i;
			if (d < reach && d < D[k]) { D[k] = d; A[k] = along0 + t * L; }
		}
		along0 += L;
	}
	return along0;
}
const lengthOf = (pts) => pts.reduce((t, p, i) => i ? t + Math.hypot(p[0] - pts[i - 1][0], p[1] - pts[i - 1][1]) : 0, 0);
const sampleH = (x, y) => {
	const fi = (x + HALF) / STEP, fj = (y + HALF) / STEP, i = Math.max(0, Math.min(N - 2, Math.floor(fi))), j = Math.max(0, Math.min(N - 2, Math.floor(fj)));
	const u = fi - i, v = fj - j, k = j * N + i;
	return (H[k] * (1 - u) + H[k + 1] * u) * (1 - v) + (H[k + N] * (1 - u) + H[k + N + 1] * u) * v;
};

// Every road: each land's own road from its middle camp to its crossroads, and the four main roads to the gates.
const roads = [
	...lands.map((l) => ({ id: l.id, pts: l.road, half: ROAD_W, main: false, trunk: trunks.findIndex((t) => t.lands.includes(l.id)) })),
	...trunks.map((t, i) => ({ id: "main_" + t.lands.join("_"), pts: t.points, half: TRUNK_W, main: true, trunk: i })),
];
// Passes: within 380 m of a road the high ground is lowered (the road finds a way between the peaks).
{
	const D = new Float32Array(N * N).fill(1e9), A = new Float32Array(N * N);
	for (const rd of roads) raster(rd.pts, 380, D, A);
	for (let k = 0; k < N * N; k++) if (D[k] < 380) { const v = 1 - smooth(30, 380, D[k]); H[k] = lerp(H[k], Math.min(H[k], 30 + 0.35 * H[k]), v); }
	log("passes done");
}

// ---- 3. the lake in the meadow and its stream to the sea ----------------------------------------------------------
const lakeBox = lakeOutline.reduce((b, [x, y]) => [Math.min(b[0], x), Math.min(b[1], y), Math.max(b[2], x), Math.max(b[3], y)], [1e9, 1e9, -1e9, -1e9]);
const lakeEdgeDist = (x, y) => {
	let best = 1e9;
	for (let s = 0; s < lakeOutline.length; s++) {
		const [ax, ay] = lakeOutline[s], [bx, by] = lakeOutline[(s + 1) % lakeOutline.length], L2 = (bx - ax) ** 2 + (by - ay) ** 2 || 1e-6;
		const t = Math.min(1, Math.max(0, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2));
		best = Math.min(best, Math.hypot(x - ax - t * (bx - ax), y - ay - t * (by - ay)));
	}
	return best;
};
{
	const P = 160;
	for (let j = Math.floor((lakeBox[1] - P + HALF) / STEP); j <= Math.ceil((lakeBox[3] + P + HALF) / STEP); j++) for (let i = Math.floor((lakeBox[0] - P + HALF) / STEP); i <= Math.ceil((lakeBox[2] + P + HALF) / STEP); i++) {
		const x = -HALF + i * STEP, y = -HALF + j * STEP, k = j * N + i, d = lakeEdgeDist(x, y);
		if (inPoly([x, y], lakeOutline)) { H[k] = LAKE_LEVEL - Math.min(6, 0.6 + d * 0.045); S[k] = SURF.lake_bed; }
		else if (d < P) {
			H[k] = Math.max(H[k], LAKE_LEVEL + 0.5 + d * 0.012);   // the meadow never dips under the lake beside it
			if (d < 6) H[k] = lerp(LAKE_LEVEL + 0.3, H[k], d / 6);
			if (d < 5) S[k] = SURF.bank;
		}
	}
}
const streamLen = lengthOf(stream);
{
	const D = new Float32Array(N * N).fill(1e9), A = new Float32Array(N * N);
	raster(stream, 240, D, A);
	for (let k = 0; k < N * N; k++) {
		if (D[k] >= 240) continue;
		const s = A[k] / streamLen, bed = LAKE_LEVEL - 0.6 - (LAKE_LEVEL - 0.6 + 1.2) * Math.pow(s, 0.85), d = D[k];
		const cut = d < 3.5 ? bed : d < 12 ? bed + 0.5 + (d - 3.5) * 0.3 : bed + 3 + (d - 12) * 0.17;
		if (cut < H[k]) { H[k] = cut; if (d < 3.5) S[k] = SURF.bed; else if (d < 9) S[k] = SURF.bank; }
	}
	log("lake and stream done");
}

// ---- 4. flat ground where things will stand: the city's top, the forts, the crossroads, the camps -----------------
// Flat ground of radius at level, its cut and fill sloping back at no more than CUT_SLOPE (never a wall), eased.
const CUT_SLOPE = 0.4;
function pad([px, py], radius, blend, level) {
	const L = level ?? sampleH(px, py), reach = radius + Math.max(blend, 160);
	for (let j = Math.floor((py - reach + HALF) / STEP); j <= Math.ceil((py + reach + HALF) / STEP); j++) for (let i = Math.floor((px - reach + HALF) / STEP); i <= Math.ceil((px + reach + HALF) / STEP); i++) {
		if (i < 0 || j < 0 || i >= N || j >= N) continue;
		const x = -HALF + i * STEP, y = -HALF + j * STEP, k = j * N + i, d = Math.hypot(x - px, y - py);
		if (d >= reach) continue;
		const s = Math.max(0, d - radius) * CUT_SLOPE;
		const held = Math.min(Math.max(H[k], L - s), L + s);
		H[k] = lerp(L, held, smooth(radius, radius + blend, d));
	}
	return L;
}
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { const x = -HALF + i * STEP, y = -HALF + j * STEP, r = Math.hypot(x, y), k = j * N + i; if (r < CITY_FLAT + 40) H[k] = lerp(CITY_TOP, H[k], smooth(CITY_FLAT, CITY_FLAT + 40, r)); if (r < PLAN.PLAZA_R) S[k] = SURF.plaza; }
// The city's streets cobbled (Tools/terrain/hearthmoot_plan.mjs); the rest of the hilltop is grass yards (zone Y).
for (const st of PLAN.streets) {
	const pts = st.pts;
	for (let s = 1; s < pts.length; s++) {
		const [ax, ay] = pts[s - 1], [bx, by] = pts[s], reach = st.half;
		for (let j = Math.floor((Math.min(ay, by) - reach + HALF) / STEP); j <= Math.ceil((Math.max(ay, by) + reach + HALF) / STEP); j++) for (let i = Math.floor((Math.min(ax, bx) - reach + HALF) / STEP); i <= Math.ceil((Math.max(ax, bx) + reach + HALF) / STEP); i++) {
			const x = -HALF + i * STEP, y = -HALF + j * STEP, k = j * N + i;
			if (S[k] !== SURF.plaza && PLAN.segDist([x, y], [ax, ay], [bx, by]).d < reach) S[k] = SURF.cobble;
		}
	}
}
// The fields: each its crop in rows (two shades, 2.5 m apart, across its width).
for (const f of PLAN.fields) {
	const xs = f.poly.map((p) => p[0]), ys = f.poly.map((p) => p[1]);
	for (let j = Math.floor((Math.min(...ys) + HALF) / STEP); j <= Math.ceil((Math.max(...ys) + HALF) / STEP); j++) for (let i = Math.floor((Math.min(...xs) + HALF) / STEP); i <= Math.ceil((Math.max(...xs) + HALF) / STEP); i++) {
		const x = -HALF + i * STEP, y = -HALF + j * STEP, k = j * N + i;
		if (!inPoly([x, y], f.poly)) continue;
		const row = Math.floor(((x - f.poly[0][0]) * f.v[0] + (y - f.poly[0][1]) * f.v[1]) / 2.5) % 2;
		S[k] = SURF[f.crop + (row ? "2" : "")];
	}
}
// The lake's beach: a sand strip on the city side, sloping gently into the water.
for (const p of PLAN.beach) {
	for (let j = Math.floor((p[1] - 20 + HALF) / STEP); j <= Math.ceil((p[1] + 20 + HALF) / STEP); j++) for (let i = Math.floor((p[0] - 20 + HALF) / STEP); i <= Math.ceil((p[0] + 20 + HALF) / STEP); i++) {
		const x = -HALF + i * STEP, y = -HALF + j * STEP, k = j * N + i, d = Math.hypot(x - p[0], y - p[1]);
		if (d < 18 && !inPoly([x, y], lakeOutline)) { S[k] = SURF.sand; H[k] = Math.min(H[k], LAKE_LEVEL + 0.25 + d * 0.06); }
	}
}
const fortLevels = trunks.map((t) => pad(t.fort, 45, 30));
// Each crossroads sits in a pass: no higher than its fort plus a gentle climb (CROSS_CLIMB of the way), the ground
// round it lowered into a bowl whose sides rise out at CROSS_SIDE, so the main road and the lands' roads can reach it.
const CROSS_CLIMB = 0.06, CROSS_SIDE = 0.28;
const crossLevels = trunks.map((t, n) => {
	const [cx, cy] = t.crossroads, level = Math.min(sampleH(cx, cy), fortLevels[n] + CROSS_CLIMB * Math.hypot(cx - t.fort[0], cy - t.fort[1]));
	for (let j = Math.floor((cy - 400 + HALF) / STEP); j <= Math.ceil((cy + 400 + HALF) / STEP); j++) for (let i = Math.floor((cx - 400 + HALF) / STEP); i <= Math.ceil((cx + 400 + HALF) / STEP); i++) {
		const x = -HALF + i * STEP, y = -HALF + j * STEP, d = Math.hypot(x - cx, y - cy), k = j * N + i;
		if (d < 400) H[k] = Math.min(H[k], level + Math.max(0, d - 35) * CROSS_SIDE);
	}
	return pad(t.crossroads, 25, 25, level);
});
for (const l of lands) for (const c of l.camps) pad(c, 30, 25, Math.max(2.2, sampleH(...c)));
log("pads done");

// ---- 5. the roads: each one's grade smoothed along it (never steeper than ROAD_GRADE), the ground flattened to it --
const roadInfo = [];
for (const rd of roads) {
	// Dense points along the road, the ground's height under each, smoothed over 160 m, then held to the grade both ways.
	const dense = [];
	for (let s = 1; s < rd.pts.length; s++) {
		const [ax, ay] = rd.pts[s - 1], [bx, by] = rd.pts[s], L = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.ceil(L / 8));
		for (let q = s === 1 ? 0 : 1; q <= n; q++) dense.push([ax + (bx - ax) * q / n, ay + (by - ay) * q / n]);
	}
	const raw = dense.map(([x, y]) => sampleH(x, y));
	const at = [0]; for (let q = 1; q < dense.length; q++) at.push(at[q - 1] + Math.hypot(dense[q][0] - dense[q - 1][0], dense[q][1] - dense[q - 1][1]));
	const sm = raw.map((_, q) => { let s2 = 0, w = 0; for (let o = q; o >= 0 && at[q] - at[o] < 80; o--) { s2 += raw[o]; w++; } for (let o = q + 1; o < raw.length && at[o] - at[q] < 80; o++) { s2 += raw[o]; w++; } return s2 / w; });
	// The road ends where it must: a land's road on its crossroads' pad; a main road starts there and ends on the
	// city's top at its gate. Each end eased in over the last few hundred metres, then held there.
	const last = sm.length - 1, ease = (from, to, q) => smooth(from, to, at[q]);
	const pinEnd = (level, over) => { const d = level - sm[last]; for (let q = 0; q <= last; q++) sm[q] += d * ease(at[last] - over, at[last], q); };
	const pinStart = (level, over) => { const d = level - sm[0]; for (let q = 0; q <= last; q++) sm[q] += d * (1 - ease(0, over, q)); };
	if (rd.main) { pinStart(crossLevels[rd.trunk], 250); pinEnd(CITY_TOP, 250); } else pinEnd(crossLevels[rd.trunk], 300);
	const ends = [sm[0], sm[last]];
	for (let q = 1; q < sm.length; q++) sm[q] = Math.min(sm[q], sm[q - 1] + ROAD_GRADE * (at[q] - at[q - 1]));
	for (let q = sm.length - 2; q >= 0; q--) sm[q] = Math.min(sm[q], sm[q + 1] + ROAD_GRADE * (at[q + 1] - at[q]));
	for (let q = 0; q < sm.length; q++) sm[q] = Math.max(sm[q], 1.2);
	if (rd.main) sm[0] = ends[0];
	sm[last] = ends[1];
	roadInfo.push({ ...rd, dense, grade: sm, at, length: at[at.length - 1], climb: sm.reduce((m, v, q) => q ? Math.max(m, Math.abs(v - sm[q - 1]) / Math.max(1e-6, at[q] - at[q - 1])) : 0, 0) });
}
{
	const D = new Float32Array(N * N).fill(1e9), A = new Float32Array(N * N), WHO = new Int16Array(N * N).fill(-1);
	roadInfo.forEach((rd, n) => {
		// Each road's own pass so a sample takes the nearest road's grade.
		for (let s = 1; s < rd.dense.length; s++) {
			const [ax, ay] = rd.dense[s - 1], [bx, by] = rd.dense[s], L = Math.hypot(bx - ax, by - ay) || 1e-6, reach = rd.half + 90;
			const i0 = Math.max(0, Math.floor((Math.min(ax, bx) - reach + HALF) / STEP)), i1 = Math.min(N - 1, Math.ceil((Math.max(ax, bx) + reach + HALF) / STEP));
			const j0 = Math.max(0, Math.floor((Math.min(ay, by) - reach + HALF) / STEP)), j1 = Math.min(N - 1, Math.ceil((Math.max(ay, by) + reach + HALF) / STEP));
			for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
				const x = -HALF + i * STEP, y = -HALF + j * STEP, t = Math.min(1, Math.max(0, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / (L * L)));
				const d = Math.hypot(x - (ax + t * (bx - ax)), y - (ay + t * (by - ay))), k = j * N + i;
				if (d < reach && d - rd.half < D[k] - (WHO[k] >= 0 ? roadInfo[WHO[k]].half : 0)) { D[k] = d; A[k] = lerp(rd.grade[s - 1], rd.grade[s], t); WHO[k] = n; }
			}
		}
	});
	for (let k = 0; k < N * N; k++) {
		if (WHO[k] < 0) continue;
		// The road's bed at its grade; beside it the ground held within CUT_SLOPE of it (cuttings and banks, not walls).
		const rd = roadInfo[WHO[k]], d = D[k], s = Math.max(0, d - rd.half) * CUT_SLOPE;
		H[k] = lerp(A[k], Math.min(Math.max(H[k], A[k] - s), A[k] + s), smooth(rd.half, rd.half + 6, d));
		if (d < rd.half && Math.hypot(-HALF + (k % N) * STEP, -HALF + Math.floor(k / N) * STEP) >= CITY_FLAT) S[k] = rd.main ? SURF.main_road : SURF.road;
	}
	log("roads done");
}

// ---- 6. zone letters (16 m cells): each land's letter with a ragged blended border, the meadow, the city, the sea ---
const LETTERS = "123456789";
const CELL = 16, GW = Math.ceil(HALF * 2 / CELL);
const rows = [];
for (let gj = 0; gj < GW; gj++) {
	let row = "";
	for (let gi = 0; gi < GW; gi++) {
		const x = -HALF + (gi + 0.5) * CELL, y = -HALF + (gj + 0.5) * CELL, r = Math.hypot(x, y), deg = bearingOf(x, y);
		if (r < CITY_FLAT - 45) { row += "Y"; continue; }   // inside the walls: grass yards (the streets are cobbled surfaces)
		if (r < CITY_FLAT + 10) { row += "C"; continue; }
		if (r < meadowAt(deg) + 60 * fbm(x / 140, y / 140, 23, 3)) { row += "M"; continue; }
		if (r > coastAt(deg) + 1100) { row += "D"; continue; }   // the deep floor past the reefs: dark, so where it stops building never shows
		if (r > coastAt(deg) + 40) { row += "~"; continue; }
		const wd = norm(deg + warpDeg(x, y, r) + 170 * fbm(x / 420, y / 420, 31, 3) / Math.max(r, 1000) / RAD - FIRST_DEG);
		let a = 0; while (a < 8 && wd >= cum[a + 1]) a++;
		row += LETTERS[a];
	}
	rows.push(row);
}
log("zone letters done");

// ---- out --------------------------------------------------------------------------------------------------------------
const bytes = Buffer.alloc(N * N * 3);
let lo = 1e9, hi = -1e9;
for (let k = 0; k < N * N; k++) {
	const v = Math.max(0, Math.min(65535, Math.round(OFFSET + H[k] * UNITS)));
	bytes[k * 2] = v & 255; bytes[k * 2 + 1] = v >> 8;
	bytes[N * N * 2 + k] = S[k];
	lo = Math.min(lo, H[k]); hi = Math.max(hi, H[k]);
}
const packed = zlib.deflateSync(bytes, { level: 9 });
fs.writeFileSync(path.join(root, "Data/lands/island_ground.bin"), packed);
const sRGB = (h) => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
const ZONES = [
	{ id: "storm_peaks", letter: "1", name: "Storm peaks", ground: [104, 110, 106], groundAlt: [122, 130, 112], layers: { stones: 0.55, grass: 0.25, soil: 0.2 } },
	{ id: "desert", letter: "2", name: "the Desert", ground: [214, 186, 132], groundAlt: [200, 166, 110], layers: { sand: 0.95, stones: 0.05 } },
	{ id: "forest", letter: "3", name: "the Ancient forest", ground: [72, 112, 54], groundAlt: [92, 122, 60], layers: { grass: 0.4, litter: 0.3, moss: 0.3 } },
	{ id: "tundra", letter: "4", name: "the Frozen tundra", ground: [226, 232, 238], groundAlt: [198, 212, 224], layers: { sand: 0.8, stones: 0.2 } },
	{ id: "swamp", letter: "5", name: "the Swamp", ground: [80, 86, 54], groundAlt: [94, 98, 62], layers: { moss: 0.5, soil: 0.5 } },
	{ id: "ocean", letter: "6", name: "the Ocean shore", ground: [176, 178, 136], groundAlt: [122, 150, 92], layers: { sand: 0.5, grass: 0.5 } },
	{ id: "graveyard", letter: "7", name: "the Graveyard", ground: [112, 116, 96], groundAlt: [98, 100, 86], layers: { soil: 0.5, grass: 0.3, stones: 0.2 } },
	{ id: "wasteland", letter: "8", name: "the Corrupted wasteland", ground: [104, 88, 112], groundAlt: [86, 72, 96], layers: { soil: 0.5, stones: 0.5 } },
	{ id: "volcanic", letter: "9", name: "the Volcanic", ground: [46, 44, 44], groundAlt: [70, 56, 50], layers: { stones: 0.7, soil: 0.3 } },
	{ id: "meadow", letter: "M", name: "Hearthmoot's meadow", ground: [96, 150, 62], groundAlt: [124, 166, 70], layers: { grass: 0.85, soil: 0.15 }, speck: { colors: [[236, 232, 244], [230, 196, 70], [160, 120, 196]], amount: 0.08, scale: 4 } },
	{ id: "city", letter: "C", name: "Hearthmoot's walls", ground: [118, 140, 82], groundAlt: [130, 146, 86], layers: { grass: 0.7, soil: 0.3 } },
	{ id: "yards", letter: "Y", name: "Hearthmoot's yards", ground: [92, 140, 64], groundAlt: [112, 150, 70], layers: { grass: 0.85, soil: 0.15 }, speck: { colors: [[236, 232, 244], [230, 196, 70]], amount: 0.04, scale: 3 } },
	{ id: "seabed", letter: "~", name: "the sea floor", ground: [176, 164, 124], groundAlt: [150, 144, 112], layers: { sand: 0.9, stones: 0.1 } },
	{ id: "deep", letter: "D", name: "the deep sea floor", ground: [34, 46, 56], groundAlt: [30, 40, 50], layers: { sand: 0.6, stones: 0.4 } },
];
const land = {
	_comment: "Generated by Tools/terrain/round_ground.mjs from the signed-off layout (Tools/terrain/round_island.mjs; edit the scripts, not this file). The round island as one runtime heightfield land on its own map (map): 4 m samples in island_ground.bin (zlib: uint16 heights = offset + m x unitsPerMetre, then a surface byte each), built as procedural-mesh chunks, full detail near you and coarser far off (lod), with collision only near players. Zones by letter: one per land (each land's slice replaces its first-pass ground later), the meadow (M), Hearthmoot's hill top (C), the sea floor (~); zoneBiomes says which biome each letter is.",
	id: "island", biome: "meadow", name: "the round island", map: MAP,
	origin: [0, 0, 0], mapCentre: [0, 0], seed: 11, grid: STEP, chunk: 200, drawDistance: 1400000, chunksPerFrame: 6, seaFloor: SEA_FLOOR,
	heightfield: { file: "island_ground.bin", x0: -HALF, y0: -HALF, step: STEP, samples: N, unitsPerMetre: UNITS, offset: OFFSET, surface: true, compressed: "zlib", rawBytes: bytes.length },
	lod: { _comment: "Far detail levels (WRLand): a chunk this far (m) from your view uses every 2nd, 4th, 8th sample; collision only within collisionRange m of a player.", ranges: [700, 1600, 3600], collisionRange: 600 },
	zoneGrid: { cell: CELL, x0: -HALF, y0: -HALF, width: GW, height: GW, rows },
	zoneBlend: 40,
	zoneBiomes: { "1": "storm_peaks", "2": "desert", "3": "forest", "4": "tundra", "5": "swamp", "6": "ocean", "7": "graveyard", "8": "wasteland", "9": "volcanic", M: "meadow", C: "meadow", Y: "meadow", "~": "ocean", D: "ocean" },
	groundMaterial: "/Game/Terrain/M_IslandGround.M_IslandGround",
	zones: ZONES.map((z) => ({ id: z.id, letter: z.letter, name: z.name, ground: z.ground, groundAlt: z.groundAlt, rough: 0.9, ...(z.speck ? { speck: z.speck } : {}), layers: z.layers })),
	surfaces: [
		{ id: "bed", class: SURF.bed, color: [70, 76, 78], rough: 0.45, layers: { stones: 0.85, sand: 0.15 } },
		{ id: "bank", class: SURF.bank, color: [70, 92, 54], rough: 0.7, layers: { grass: 0.4, soil: 0.35, moss: 0.25 } },
		{ id: "sand", class: SURF.sand, color: [206, 190, 146], rough: 0.8, layers: { sand: 1 } },
		{ id: "shore_rock", class: SURF.shore_rock, color: [110, 106, 100], rough: 0.8, layers: { stones: 0.9, sand: 0.1 } },
		{ id: "road", class: SURF.road, color: [118, 104, 79], rough: 0.9, layers: { soil: 0.5, stones: 0.5 } },
		{ id: "main_road", class: SURF.main_road, color: [132, 124, 108], rough: 0.85, layers: { stones: 0.8, soil: 0.2 } },
		{ id: "plaza", class: SURF.plaza, color: [150, 142, 124], rough: 0.85, layers: { stones: 0.9, sand: 0.1 } },
		{ id: "lake_bed", class: SURF.lake_bed, color: [92, 96, 74], rough: 0.5, layers: { soil: 0.5, moss: 0.3, sand: 0.2 } },
		{ id: "cobble", class: SURF.cobble, color: [128, 122, 112], rough: 0.8, layers: { stones: 0.95, sand: 0.05 } },
		{ id: "wheat", class: SURF.wheat, color: [206, 176, 86], rough: 0.9, layers: { grass: 0.6, soil: 0.4 } },
		{ id: "wheat2", class: SURF.wheat2, color: [178, 148, 70], rough: 0.9, layers: { grass: 0.5, soil: 0.5 } },
		{ id: "barley", class: SURF.barley, color: [178, 178, 104], rough: 0.9, layers: { grass: 0.7, soil: 0.3 } },
		{ id: "barley2", class: SURF.barley2, color: [150, 154, 88], rough: 0.9, layers: { grass: 0.6, soil: 0.4 } },
		{ id: "greens", class: SURF.greens, color: [70, 124, 46], rough: 0.9, layers: { grass: 0.8, soil: 0.2 } },
		{ id: "greens2", class: SURF.greens2, color: [86, 70, 50], rough: 0.95, layers: { soil: 0.9, grass: 0.1 } },
		{ id: "ploughed", class: SURF.ploughed, color: [96, 72, 50], rough: 0.95, layers: { soil: 1 } },
		{ id: "ploughed2", class: SURF.ploughed2, color: [78, 58, 42], rough: 0.95, layers: { soil: 1 } },
		{ id: "fallow", class: SURF.fallow, color: [126, 146, 72], rough: 0.9, layers: { grass: 0.8, soil: 0.2 } },
		{ id: "fallow2", class: SURF.fallow2, color: [118, 136, 66], rough: 0.9, layers: { grass: 0.8, soil: 0.2 } },
	],
	slopeRock: { color: [88, 84, 80], fromDeg: 33, toDeg: 47, ledge: [96, 112, 70] },
	lake: { level: LAKE_LEVEL, at: RI.lakeAt.map(Math.round), outline: lakeOutline.map((p) => p.map(Math.round)),
		// The stream's points with its water's level (m): 0.4 m over its bed, down from the lake to the sea.
		stream: (() => { let L = 0; return stream.map((p, i) => { if (i) L += Math.hypot(p[0] - stream[i - 1][0], p[1] - stream[i - 1][1]); const s = L / streamLen; return [Math.round(p[0]), Math.round(p[1]), +(LAKE_LEVEL - 0.6 - (LAKE_LEVEL - 0.6 + 1.2) * Math.pow(s, 0.85) + 0.4).toFixed(2)]; }); })() },
	buildFirst: [[0, 0], ...trunks.map((t) => t.gate.map(Math.round))],
	// Until each land's own camps are built (its slice), everyone starts in Hearthmoot (WRGameMode: world.cityStartTag).
	camps: { tag: "hearthmoot", safeRadius: 40, stationsAt: -1, list: [[0, 34, 0], [30, -17, 120], [-30, -17, 240]] },
};
fs.writeFileSync(path.join(root, "Data/lands/island.json"), JSON.stringify(land));

// The sea mask: 4 m cells whose lowest corner is under LAND_ABOVE, flood-filled in from the edge, plus the Swamp's
// pools under the sea's level (they stand at the sea's level too).
{
	const W = N - 1, sea = new Uint8Array(W * W), low = new Uint8Array(W * W);
	for (let j = 0; j < W; j++) for (let i = 0; i < W; i++) { const k = j * N + i; low[j * W + i] = Math.min(H[k], H[k + 1], H[k + N], H[k + N + 1]) < LAND_ABOVE ? 1 : 0; }
	const stack = [];
	for (let i = 0; i < W; i++) { stack.push(i, (W - 1) * W + i, i * W, i * W + W - 1); }
	while (stack.length) { const c = stack.pop(); if (!low[c] || sea[c]) continue; sea[c] = 1; const i = c % W, j = (c - i) / W; if (i > 0) stack.push(c - 1); if (i < W - 1) stack.push(c + 1); if (j > 0) stack.push(c - W); if (j < W - 1) stack.push(c + W); }
	let pools = 0;
	for (let j = 0; j < W; j++) for (let i = 0; i < W; i++) { const c = j * W + i, k = j * N + i; if (!sea[c] && LANDS[OWN[k]].id === "swamp" && Math.max(H[k], H[k + 1], H[k + N], H[k + N + 1]) < -0.2) { sea[c] = 1; pools++; } }
	const mrows = [];
	for (let j = 0; j < W; j++) { const runs = []; let cur = 1, n = 0; for (let i = 0; i < W; i++) { const v = sea[j * W + i]; if (v === cur) n++; else { runs.push(n); cur = v; n = 1; } } runs.push(n); mrows.push(runs.join(" ")); }
	fs.writeFileSync(path.join(root, `Data/sea_mask_${MAP}.json`), JSON.stringify({ _comment: "The open sea on a 4 m grid for the round island's map (Tools/terrain/round_ground.mjs, generated: edit the script, not this file). Each row (south to north, -Y first) is run lengths west to east (-X first), alternating sea, land, sea... starting with sea. Outside the grid is all sea.", map: MAP, cell: STEP * 100, originX: -HALF * 100, originY: -HALF * 100, width: W, height: W, rows: mrows }));
	log(`sea mask: ${sea.reduce((s, v) => s + v, 0)} sea cells (${pools} swamp pool cells)`);
}

// A picture from above (Saved/RoundIsland/ground.png, 1 px = 12 m): zone colours, lit from the north-west, the sea blue.
{
	const P = 1100, step = HALF * 2 / P, img = Buffer.alloc(P * P * 3);
	const zc = Object.fromEntries(ZONES.map((z) => [z.letter, z.ground]));
	for (let py = 0; py < P; py++) for (let px = 0; px < P; px++) {
		const x = -HALF + (px + 0.5) * step, y = -HALF + (py + 0.5) * step, h = sampleH(x, y);
		const dx = sampleH(x + 8, y) - sampleH(x - 8, y), dy = sampleH(x, y + 8) - sampleH(x, y - 8);
		const shade = Math.max(0.35, Math.min(1.3, 0.85 + (-dx - dy) * 0.04));
		let col;
		const gi = Math.min(GW - 1, Math.floor((x + HALF) / CELL)), gj = Math.min(GW - 1, Math.floor((y + HALF) / CELL));
		const s = S[Math.round((y + HALF) / STEP) * N + Math.round((x + HALF) / STEP)];
		if (h < 0 && rows[gj][gi] === "~") col = h > -3 ? [120, 196, 200] : h > -10 ? [70, 150, 172] : [40, 92, 128];
		else if (inPoly([x, y], lakeOutline)) col = [80, 160, 205];
		else if (s === SURF.bed) col = [70, 140, 200];
		else if (s === SURF.road || s === SURF.main_road) col = [92, 70, 40];
		else if (h < 0 && rows[gj][gi] === "5") col = [70, 110, 120];
		else col = (zc[rows[gj][gi]] || [128, 128, 128]).map((v) => v * shade);
		const o = (py * P + px) * 3; img[o] = Math.min(255, col[0]); img[o + 1] = Math.min(255, col[1]); img[o + 2] = Math.min(255, col[2]);
	}
	const crcT = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
	const crc = (b) => { let c = -1; for (const v of b) c = crcT[(c ^ v) & 255] ^ (c >>> 8); return (c ^ -1) >>> 0; };
	const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); };
	const raw = Buffer.alloc(P * (P * 3 + 1)); for (let y = 0; y < P; y++) { raw[y * (P * 3 + 1)] = 0; img.copy(raw, y * (P * 3 + 1) + 1, y * P * 3, (y + 1) * P * 3); }
	const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(P, 0); ihdr.writeUInt32BE(P, 4); ihdr[8] = 8; ihdr[9] = 2;
	fs.mkdirSync(path.join(root, "Saved/RoundIsland"), { recursive: true });
	fs.writeFileSync(path.join(root, "Saved/RoundIsland/ground.png"), Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
log(`ground ${N} x ${N} samples, ${lo.toFixed(1)} to ${hi.toFixed(1)} m; file ${(packed.length / 1e6).toFixed(1)} MB (raw ${(bytes.length / 1e6).toFixed(1)} MB)`);
for (const rd of roadInfo) log(`road ${rd.id}: ${(rd.length / 1000).toFixed(2)} km, steepest ${(rd.climb * 100).toFixed(1)}%, ends at ${rd.grade[rd.grade.length - 1].toFixed(1)} m`);
log(`forts at ${fortLevels.map((v) => v.toFixed(1)).join(", ")} m; the lake at ${LAKE_LEVEL} m; the city's top at ${CITY_TOP} m`);
