// The Ancient forest: belt B5 east of the Spine, south of the Old Island (cooks/biome-forest.md 2.1-2.7, 2.11, 2.13,
// 2.14, 2.22; cooks/build-biome-forest.md 2.1-2.6, 2.15; cooks/world-map.md 2.2-2.3). Continent metres: +x east, +y south,
// origin at the island's centre. Writes
//   Art/Terrain/forest_height.r16  2017 x 2017 samples, 2 m apart, from (X0, Y0); value = 32768 + metres * 16
//                                  (import with landscape scale 200, 200, 800: Tools/terrain/import_forest.py)
//   Data/forest.json               THE contract parsed into FWRForestData (Source/Wildreach/WRForestTypes.h): frame,
//                                  sub-zone grid, sites, kit POIs, shrines, giants, fallen giants, routes, rivers, falls,
//                                  thornbrakes, spots, walkways, arenas, the Hollow Root's rooms, the village; generated,
//                                  edit this script
//   Docs/terrain/forest.png        shaded layout preview (1 px = 8 m)
//   Data/island_dressing.json      only its "pois" (the map's place list): the forest's camps, caves and landmarks
//                                  (the forest_* tree instances are Tools/terrain/forest_dressing.mjs's)
// node Tools/terrain/forest.mjs [--preview-only]
//
// The frame is the plan's 4,032 m square moved 208 m north (y +1,696 instead of +1,904): the Old Island's south shore
// east of its ridge lies at y +1,700..+1,900, so the south-east neck (x +250..+700) only meets it if the frame reaches
// there. In the overlap the island's own ground wins (this land is written 3 m under it), as Storm peaks does.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const PREVIEW_ONLY = process.argv.includes('--preview-only');
export const X0 = -1450, Y0 = 1696, STEP = 2, N = 2017, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;
const SEED = 53;
const ISLAND_EDGE = 2016;    // the island landscape's south edge (m); world.islandSouthEdge

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
const smax = (a, b, k) => { const h = Math.max(0, Math.min(1, 0.5 + 0.5 * (b - a) / k)); return lerp(a, b, h) + k * h * (1 - h); };
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 98765;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
const r1 = v => +v.toFixed(1);
const r2 = v => +v.toFixed(2);
const DEG = Math.PI / 180;
const yawOf = (dx, dy) => Math.round(Math.atan2(dy, dx) / DEG);

// ---------------------------------------------------------------- the layout (metres, from the design cook)
const CAMPS = [{ x: 2300, y: 2800 }, { x: 2350, y: 3600 }, { x: 2250, y: 4400 }];
const VILLAGE = { x: 2100, y: 3450, r: 150 };
const HEARTS = { x: -500, y: 3600, z: 210, r: 70 };
const NECK = { x0: 250, x1: 700 };
// The Spine's crest (world-map 2.3): x and height by y. It comes off the island's ridge end (+200, +1,950), bends west
// through the Rootlands bend (-600, +2,600, 750 m) to the Rootgrave saddle (-1,150, +3,300, 190 m), the south spine
// (-900, +4,200, 600 m), and runs on toward the Severance, sinking into the sea past the south coast.
const CREST = [[1696, 150], [1950, 200], [2100, 40], [2300, -190], [2450, -380], [2600, -600], [2700, -940], [2850, -1070], [3050, -1140], [3300, -1150], [3500, -1130], [3800, -1050], [4200, -900], [4500, -850], [4750, -800], [5000, -650], [5400, -450], [5800, -350]];
const CREST_H = [[1696, 620], [1900, 170], [2000, 160], [2150, 300], [2350, 520], [2600, 750], [2850, 690], [3050, 470], [3200, 270], [3300, 190], [3400, 270], [3550, 460], [3800, 560], [4200, 600], [4500, 560], [4750, 430], [4950, 160], [5150, -20]];
const crestX = y => interp(CREST, y);
const crestH = y => interp(CREST_H, y);
const ROOTGRAVE = { x: -1150, y: 3300, plug: 230 };

// Coasts. East: x by y, with a cove under each camp; north (the Pine Sound): y by x east of the neck; south: y by x.
// The east coast is a ragged line, not a ruler: a long swing of +-80 m typical (up to ~150 m) (bays and points a few hundred metres apart),
// shorter frays, five rocky headlands that stand out into the sea (the coast road's landmarks between the camps), and a
// cove under each camp: a round bay 60-70 m deep between two points, its beach the camp's landing, the camp 60 m back
// from the water on its terrace. The coast road always keeps 45 m of land to seaward.
const HEADLANDS = [[2620, 60, 55], [3020, 75, 60], [3330, 55, 45], [3980, 80, 65], [4230, 50, 40]];   // [y, out m, half width]
const COAST_ROAD = [[2800, 2300], [2950, 2330], [3150, 2290], [3330, 2200], [3450, 2160], [3540, 2250], [3600, 2350], [3800, 2300], [4000, 2240], [4200, 2200], [4400, 2250]];
function coastX(y) {
	let x = 2440 + 190 * fbm(y / 640, 3.1, 4) + 55 * fbm(y / 150, 8.3, 3) + 12 * fbm(y / 35, 1.7, 2);
	for (const [hy, out, hw] of HEADLANDS) x += out * Math.exp(-(((y - hy) / hw) ** 2));
	x += 45 * Math.exp(-(((y - 2920) / 90) ** 2));                            // the dune spit the Wreck Meadow lies on
	for (const c of CAMPS) {
		// The cove: the shore pulled in to 60 m in front of the camp, round in plan (a cosine bowl 2 x 85 m across), and a
		// point either side of it (the cove's arms, 35 m proud of the line).
		const d = Math.abs(y - c.y);
		const want = c.x + 60 + 6 * fbm(y / 30, 4.4, 2);
		if (d < 85) x = lerp(x, want, 0.5 + 0.5 * Math.cos(Math.PI * d / 85));
		x += 35 * Math.exp(-(((d - 115) / 30) ** 2));
	}
	// Never closer than 45 m to the coast road's seaward edge.
	if (y >= COAST_ROAD[0][0] && y <= COAST_ROAD[COAST_ROAD.length - 1][0]) x = smax(x, interp(COAST_ROAD, y) + 48, 30);
	return x;
}
// Where the east shore is a headland's rock (no dunes, sea rocks instead): 0..1.
const headlandAt = y => Math.min(1, HEADLANDS.reduce((s, [hy, , hw]) => s + Math.exp(-(((y - hy) / (hw * 0.8)) ** 2)), 0) + CAMPS.reduce((s, c) => s + Math.exp(-(((Math.abs(y - c.y) - 115) / 22) ** 2)), 0));
function northY(x) {
	// The neck (and the Spine's skirt west of it) runs on north over the island's sea to its south shore.
	if (x < NECK.x1 - 20) return Y0 - 50;
	const t = sstep(NECK.x1 - 20, NECK.x1 + 90, x);
	return lerp(Y0 - 50, 2350 + 30 * fbm(x / 260, 5.7, 3) - 25 * Math.exp(-(((x - 2050) / 230) ** 2)), t);
}
// The south coast: rocky, ragged (+-80 m swings typical, points and inlets), never inside the Crystal border's strip (its
// boulders and the Crystal Roots stand on land, 4,450-4,600, with 60 m to spare).
const southY = x => {
	const v = 4775 + 190 * fbm(x / 420, 9.9, 4) + 55 * fbm(x / 110, 2.2, 3) + 12 * fbm(x / 30, 6.6, 2);
	return v < 4700 ? 4660 + 40 * Math.exp((v - 4700) / 40) : v;   // a soft floor: the shore eases out, never a straight run
};

// The inland trend (DC 2.1): coast 2-8 m, meadows rolling 10-60 m, midland 60-140, highland 140-260 at the cliff foot,
// with the Great Falls' 45 m basalt step at x +300 (a cliff across the Alder's valley, a long slope elsewhere).
const TW = [[-1300, 248], [-900, 240], [-700, 230], [-400, 214], [-100, 198], [150, 166], [300, 154], [600, 146]];   // west of the step
const TE = [[0, 118], [300, 107], [450, 100], [700, 87], [1000, 70], [1400, 50], [1800, 33], [2200, 15], [2400, 7]]; // east of it
function trend(x, y) {
	const valley = sstep(2480, 2580, y) * (1 - sstep(2900, 3000, y));          // the falls' valley: a sheer step
	const w = lerp(320, 10, valley);
	let t = lerp(interp(TW, x), interp(TE, x), sstep(300 - w / 2, 300 + w / 2, x));
	// Highland heights by y: high in the north under the bend, low toward Rootgrave and the Four Hearts' floor.
	const hl = sstep(-150, -480, x);
	t += hl * interp([[2600, 32], [2800, 24], [3000, 4], [3250, -22], [3600, -34], [4000, -18], [4400, -8]], y);
	// The north low ground: toward the Pine Sound the land falls to the neck's 5-25 m pine hollows.
	const neck = 13 + 7 * fbm(x / 140, y / 140, 3);
	t = lerp(neck, t, sstep(2330, x < 300 ? 2720 : 2600, y));
	// F1 meadows: rolling; F3: a broad terrace shelf; low variation everywhere.
	t += 9 * fbm(x / 600, y / 600, 3) + (x > 1300 ? 7 * fbm(x / 230, y / 230, 3) : 4 * fbm(x / 260, y / 260, 3));
	return t;
}
// The sub-zone a point belongs to by the cook's map (2.2). The cook's lines are a ruler's (x -400, x +1,400, y +3,000,
// y +3,700); the land's edges are not: each point is moved by a two-scale domain warp before the test (a long swing of
// typically 80-150 m, up to ~250 m, that bends a border into lobes and bays, and a shorter ~30 m one that frays it), so every border wanders
// like a real woodland edge: the meadows push tongues into the oaks along the streams, the old growth sends outliers
// into the beech, and the ground colour (WRLand) blends 60-120 m across whatever line this draws.
// The warp fades out round the places whose sub-zone is part of their design (the Four Hearts and the Bower are Elder
// Canopy, the Clear-cut is old growth, the Paper Hollow and the Strangler Arch Grovekin ground): the cook's rule holds
// there, the borders wander everywhere else.
const ZONE_PINS = [[-500, 3600, 160], [-550, 4250, 120], [600, 3300, 170], [900, 4200, 80], [300, 4000, 80]];
function zoneWarp(x, y) {
	let k = 1;
	for (const [px, py, r] of ZONE_PINS) k = Math.min(k, sstep(r, r + 220, Math.hypot(x - px, y - py)));
	const wx = 330 * fbm(x / 560 + 3.7, y / 560 + 1.9, 4) + 90 * fbm(x / 150 + 11.1, y / 150 + 5.3, 3);
	const wy = 280 * fbm(x / 560 + 8.2, y / 560 + 6.4, 4) + 80 * fbm(x / 150 + 2.6, y / 150 + 9.8, 3);
	return [x + wx * k, y + wy * k];
}
function designZone(x, y) {
	const [u, v] = zoneWarp(x, y);
	if (v < 2350) return '2';                     // the tundra neck: F2's north
	if (u < -400) return '5';
	if (u > 1400) return '1';
	if (v < 3000) return '2';
	if (v < 3700) return '4';
	return '3';
}
const DETAIL = { '1': [3, 160], '2': [6, 110], '3': [4, 140], '4': [8, 90], '5': [10, 80] };

function base(x, y) {
	const cx = crestX(y), ch = crestH(y);
	// Edges: east coast, the Pine Sound (north), the south coast, west of the Spine's crest (falls to the sea).
	const eE = coastX(y) - x, eN = y - northY(x), eS = southY(x) - y;
	const eF = Math.min(x - X0, X1 - x, y - Y0, Y1 - y) - 30;
	const coastD = Math.min(eE, eN * 1.1, eS);
	const land = sstep(-30, 40, Math.min(coastD, eF));
	let h = trend(x, y);
	// Dunes along the east coast (6-12 m), the salt marsh at the Pine Sound (flat, cut by tidal creeks), a rocky south coast.
	// The dunes: a ridge 45 m back from the waterline (two rows where the beach is wide), hummocked and broken by
	// blowouts, none on the rocky headlands and the coves' arms (rock there, falling steep into the sea).
	const hd = headlandAt(y);
	const duneRow = Math.exp(-(((eE - 45) / 28) ** 2)) + 0.55 * Math.exp(-(((eE - 95) / 22) ** 2)) * sstep(-0.1, 0.3, fbm(y / 180, 7.7, 2));
	const blowout = 1 - 0.8 * sstep(0.25, 0.45, fbm(x / 25, y / 35, 3));
	const dune = duneRow * (6 + 6 * Math.max(0, fbm(x / 40, y / 90, 3) + 0.35)) * blowout * sstep(2400, 2500, y) * (1 - hd);
	const marsh = sstep(1700, 1820, x) * (1 - sstep(2280, 2380, x)) * (1 - sstep(2440, 2500, y)) * sstep(-60, 10, eN);
	const coastRise = eS < eE && eS < eN ? sstep(0, 70, eS) : sstep(0, lerp(220, 70, hd * sstep(-50, 0, eN)), Math.min(eE, eN));
	h = lerp(1.2 + dune, h + dune * 0.5, coastRise);
	if (eS < 140) h += (1 - sstep(10, 140, eS)) * 10 * Math.max(0, ridged(x / 50, y / 50, 3) - 0.2);   // sea rocks
	if (eE < 120 && eE < eS) h += hd * sstep(-60, -10, eE) * (1 - sstep(5, 120, eE)) * 12 * Math.max(0, ridged(x / 40, y / 40, 3) - 0.15);   // the headlands' rock
	h = lerp(h, 0.9 + 0.8 * fbm(x / 60, y / 60, 2) - 1.6 * Math.max(0, ridged(x / 90, y / 70, 3) - 0.55) * 3, marsh * 0.92);
	// Detail by sub-zone (plan 2.2.10).
	const z = designZone(x, y), [amp, wl] = DETAIL[z];
	h += amp * 0.5 * fbm(x / wl, y / wl, 4) * coastRise;
	if (z === '4' || z === '5') h += amp * 0.25 * Math.max(0, ridged(x / 45, y / 45, 3) - 0.4) * coastRise;   // hummocks, root mounds
	// The Spine: a steep east face from the crest (55-65 deg, ridged), talus where it meets the floor; west of the crest
	// it falls to the sea (the Graveyard's lane builds that side).
	const d = x - cx;
	let spine;
	if (d >= 0) {
		const steep = 1.85 + 0.5 * fbm(y / 180, 4.4, 3);
		spine = ch - d * steep + 30 * Math.max(0, ridged(x / 70, y / 70, 4) - 0.3) * sstep(0, 60, d) * (1 - sstep(ch * 0.4, ch * 0.6, d * steep));
		h = smax(h * land - 14 * (1 - land), spine, 28);
	} else {
		spine = ch - (-d) * 2.0 + 20 * fbm(x / 80, y / 80, 3);
		h = smax(-14, lerp(-14, spine, sstep(0, 50, eF + 30)), 10);
	}
	// The ground's surface where the coast decides it (the runtime land's colour, WRLand): salt marsh, dune sand, the
	// beach, the headlands' shore rock. Inland ground is its sub-zone's (0).
	let surf = 0;
	const shore = Math.min(eE, eS, eN);
	if (marsh > 0.5) surf = SURF.marsh;
	else if (dune > 1.2 && eE < 140) surf = SURF.dune;
	else if (shore < 75 && shore > -40) surf = hd > 0.45 && eE <= shore + 1 ? SURF.shoreRock : SURF.sand;
	return { h, land: d >= 0 ? land : (spine > 1 ? 1 : 0), surf };
}

// ---------------------------------------------------------------- the grid
const t0 = Date.now();
const HF = new Float32Array(N * N);
const LAND = new Float32Array(N * N);
// The ground's surface class per sample (written beside the heights for the runtime land, WRLand's "surfaces"):
// 0 the sub-zone's own ground, then river bed, gravel bank, beach sand, dune, salt marsh, shore rock, road, trail, the
// Hollow Root's floor; HIDDEN = no ground here (the island's own landscape covers it in the overlap strip).
const SURF = { zone: 0, bed: 1, bank: 2, sand: 3, dune: 4, marsh: 5, shoreRock: 6, road: 7, trail: 8, hollow: 9, hidden: 255 };
const SURFM = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const s = base(X0 + i * STEP, Y0 + j * STEP);
	HF[j * N + i] = s.h; LAND[j * N + i] = s.land; SURFM[j * N + i] = s.surf;
}
console.error(`base ${((Date.now() - t0) / 1000).toFixed(1)} s`);
const idx = (x, y) => { const i = Math.round((x - X0) / STEP), j = Math.round((y - Y0) / STEP); return (i < 0 || j < 0 || i >= N || j >= N) ? -1 : j * N + i; };
const hAt = (x, y) => {
	const fx = (x - X0) / STEP, fy = (y - Y0) / STEP;
	const i = Math.max(0, Math.min(N - 2, Math.floor(fx))), j = Math.max(0, Math.min(N - 2, Math.floor(fy)));
	const tx = Math.min(1, Math.max(0, fx - i)), ty = Math.min(1, Math.max(0, fy - j));
	const a = HF[j * N + i], b = HF[j * N + i + 1], c = HF[(j + 1) * N + i], d = HF[(j + 1) * N + i + 1];
	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
};
function around(x, y, R, fn) {
	const i0 = Math.max(0, Math.floor((x - R - X0) / STEP)), i1 = Math.min(N - 1, Math.ceil((x + R - X0) / STEP));
	const j0 = Math.max(0, Math.floor((y - R - Y0) / STEP)), j1 = Math.min(N - 1, Math.ceil((y + R - Y0) / STEP));
	for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
		const sx = X0 + i * STEP, sy = Y0 + j * STEP, d = Math.hypot(sx - x, sy - y);
		if (d <= R) fn(j * N + i, d, sx, sy);
	}
}
const slopeAt = (x, y) => Math.hypot(hAt(x + 2, y) - hAt(x - 2, y), hAt(x, y + 2) - hAt(x, y - 2)) / 4;
// Walk a polyline in steps of `every` metres: fn(x, y, t along segment, segment index, distance along).
function walkLine(pts, every, fn) {
	let acc = 0;
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay] = pts[s], [bx, by] = pts[s + 1], len = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.ceil(len / every));
		for (let q = 0; q < n; q++) { const t = q / n; fn(lerp(ax, bx, t), lerp(ay, by, t), t, s, acc + len * t); }
		acc += len;
	}
	const L = pts[pts.length - 1];
	fn(L[0], L[1], 1, pts.length - 2, acc);
}
const lineLen = pts => { let L = 0; for (let s = 0; s < pts.length - 1; s++) L += Math.hypot(pts[s + 1][0] - pts[s][0], pts[s + 1][1] - pts[s][1]); return L; };

// ---------------------------------------------------------------- the Alder and its streams (carved first: everything reacts)
// Each: [x, y, water surface, width, depth, flags]; flags g = gorge walls (the Root Bridge's slot), f = a ford's gravel bar.
const RIVERS = [
	{ id: 'alder_upper', kind: 'river', name: 'The Alder', pts: [[-700, 2750, 250, 6, 1.2], [-620, 2762, 243, 9, 1.6], [-520, 2770, 233, 12, 2], [-400, 2776, 222, 13, 2.2], [-260, 2790, 206, 14, 2.4],
		[-170, 2797, 194, 13, 2.6, 'g'], [-100, 2800, 186, 15, 2.6, 'g'], [-30, 2795, 178, 15, 2.6, 'g'], [60, 2782, 168, 16, 2.4], [160, 2752, 158, 17, 2.4], [240, 2716, 153, 18, 2.2], [289, 2692, 151, 20, 1.6]] },
	{ id: 'alder_lower', kind: 'river', name: 'The Alder', pts: [[322, 2700, 105, 24, 3.5], [400, 2712, 101, 20, 2.6], [520, 2716, 93, 21, 2.8], [700, 2708, 78, 22, 3], [880, 2702, 62, 24, 3], [1010, 2722, 51, 26, 2.8],
		[1100, 2750, 45, 34, 0.8, 'f'], [1200, 2752, 40, 26, 2.6], [1320, 2728, 34, 24, 3], [1450, 2668, 28, 25, 3.2], [1600, 2575, 21, 26, 3.2], [1780, 2552, 15, 27, 3.4], [1950, 2570, 10, 28, 3.4],
		[2120, 2610, 6, 29, 3.4], [2300, 2645, 3, 30, 3.6], [2440, 2652, 0.5, 34, 4]] },
	// The mill race (DC 2.4 #3): a dug leat taken off the Alder above the falls' pool, contoured north along the hillside
	// to the overshot wheel, its tailrace back down to the river below the mill.
	{ id: 'mill_race', kind: 'race', name: 'The mill race', pts: [[650, 2700, 84, 3, 1.4], [668, 2640, 83.5, 3, 1.4], [700, 2580, 83, 3, 1.4], [780, 2548, 82.6, 3, 1.5], [890, 2540, 82.2, 3, 1.5],
		[905, 2548, 78, 3, 1.2], [960, 2575, 74, 3.5, 1.2], [1020, 2630, 64, 4, 1.2], [1060, 2690, 54, 4, 1]] },
	// Otter Steps (DC 2.4 #14): a brook off the neck's pine hollows down five 1.5 m drops to the Pine Sound.
	{ id: 'otter_steps', kind: 'stream', name: 'Otter Steps', pts: [[780, 2545, 44, 4, 0.8], [740, 2505, 38, 5, 1], [700, 2470, 33, 6, 1.2], [650, 2448, 33, 6, 1, 's'], [600, 2425, 31.5, 6, 1, 's'],
		[550, 2402, 30, 6, 1, 's'], [520, 2390, 28.5, 6, 1, 's'], [500, 2380, 27, 7, 1.2, 's'], [530, 2340, 22, 7, 1.2], [610, 2310, 14, 7, 1.2], [700, 2295, 7, 8, 1.2], [790, 2290, 0.5, 9, 1.4]] },
	// The Hermit's brook: from the spring under the Hermit's Oak north-east past the Truffle glade to the Alder.
	{ id: 'hermit_brook', kind: 'stream', name: "The Hermit's brook", pts: [[1112, 3528, 60, 3, 0.8], [1150, 3430, 56, 4, 1], [1215, 3320, 51, 4, 1], [1265, 3180, 46, 5, 1.1], [1300, 3030, 41, 5, 1.2],
		[1330, 2900, 36, 6, 1.2], [1345, 2790, 33.5, 6, 1.2], [1340, 2735, 32.8, 7, 1]] },
];
// Otter Steps: each 's' point is the foot of a 1.5 m drop (the falls rows below).
const CAP = new Float32Array(N * N).fill(1e9);    // the most the ground may be (channel, banks, valley)
const LEV = new Float32Array(N * N).fill(-1e9);   // the least it must be (banks that hold the water in)
const CH = new Uint8Array(N * N);                 // in a channel or its banks (routes and levees leave it)
for (const R of RIVERS) {
	const dense = [];
	for (let s = 0; s < R.pts.length - 1; s++) {
		const a = R.pts[s], b = R.pts[s + 1], n = Math.max(1, Math.ceil(Math.hypot(b[0] - a[0], b[1] - a[1]) / 3));
		for (let q = 0; q < n; q++) {
			const t = q / n;
			// A step ('s' on b): the surface holds level, then drops at b.
			const S = b[5] === 's' ? (t < 0.85 ? a[2] : lerp(a[2], b[2], (t - 0.85) / 0.15)) : lerp(a[2], b[2], t);
			dense.push({ x: lerp(a[0], b[0], t), y: lerp(a[1], b[1], t), S, w: lerp(a[3], b[3], t), dep: lerp(a[4], b[4], t), g: a[5] === 'g' && b[5] === 'g' ? 1 : (a[5] === 'g' || b[5] === 'g') ? 1 - Math.abs(t - (a[5] === 'g' ? 0 : 1)) : 0, f: (a[5] === 'f' ? 1 - t : 0) + (b[5] === 'f' ? t : 0) });
		}
	}
	const L = R.pts[R.pts.length - 1];
	dense.push({ x: L[0], y: L[1], S: L[2], w: L[3], dep: L[4], g: 0, f: L[5] === 'f' ? 1 : 0 });
	R.dense = dense;
	for (const p of dense) {
		const half = p.w / 2, bank = p.dep * 3, valley = p.g > 0.5 ? 6 : 70;
		const R0 = half + bank + valley;
		around(p.x, p.y, R0, (k, d) => {
			let cap;
			const bed = p.S - p.dep;
			if (d <= half) cap = bed + (p.f > 0.3 ? 0.4 * (d / half) : 0) - (1 - d / half) * 0.4;
			else if (d <= half + bank) cap = bed + (d - half) / 3;
			else cap = p.S + 1.5 * (1 - p.g) + (d - half - bank) * (p.g > 0.5 ? 6 : 0.32);
			if (p.g > 0.5 && d > half) cap = p.S + (d - half) * 6;   // the gorge: sheer walls
			if (cap < CAP[k]) CAP[k] = cap;
			if (d <= half + bank) CH[k] = 1;
			// The river's own ground: wet stones in the channel, gravel on the banks and bars (+4 m of washed shingle).
			if (d <= half) SURFM[k] = SURF.bed;
			else if (d <= half + bank + 4 && SURFM[k] !== SURF.bed) SURFM[k] = SURF.bank;
			else if (d <= half + bank + 7 && p.S + 1.2 > LEV[k]) LEV[k] = p.S + 1.2;
		});
	}
}
// The falls' plunge pool (30 m round, 7 m deep) and the Herne's Ford gravel bar's wide shallow.
const POOL = { x: 322, y: 2700, r: 16, S: 105 };
around(POOL.x, POOL.y, POOL.r + 14, (k, d) => {
	const cap = d < POOL.r ? POOL.S - 7 + 5 * (d / POOL.r) ** 2 : POOL.S - 2 + (d - POOL.r) * 0.6;
	if (cap < CAP[k]) CAP[k] = cap;
	if (d < POOL.r + 3) CH[k] = 1;
});
const applyRivers = () => { for (let k = 0; k < N * N; k++) { if (HF[k] > CAP[k]) HF[k] = CAP[k]; if (!CH[k] && HF[k] < LEV[k]) HF[k] = LEV[k]; } };
applyRivers();

// ---------------------------------------------------------------- anchors (every landmark, site, camp, the village, each shrine
// and arena: a plateau at its cook spot, core flat, smooth falloff). [x, y, height or null (the ground there), core, falloff, dome m]
const ANCHORS = [
	// Camps over their coves (60 m at 6 m) and the village's 300 m terrace at 20 m.
	...CAMPS.map(c => [c.x, c.y, 6, 30, 55, 0]),
	[VILLAGE.x, VILLAGE.y, 20, 150, 90, 1.5],
	// Meadow hills: the Antler Barrow's (55 m), the Ringfort's (70 m, flat top inside the hedge), the Badger Sett's (40 m).
	[1700, 2700, 55, 35, 150, 3], [1600, 4200, 70, 38, 170, 0], [1860, 4005, 40, 20, 120, 4],
	// F1 and F2 sites.
	[2390, 2905, null, 40, 50, 0], [1900, 3050, null, 70, 80, 0], [1550, 3250, null, 25, 60, 1], [1950, 3800, null, 12, 30, 0],
	[900, 2550, null, 30, 40, 0], [1105, 2790, null, 18, 30, 0], [450, 2380, null, 22, 40, 0], [340, 2735, 107.5, 14, 18, 0],
	// F4: the Clear-cut's 250 m gentle dome, the Woodcutters' Camp, the Truffle glade, the Nurse Log's ground, the Long
	// stones, the Lost Path's glade, the Hermit's Oak.
	[600, 3300, null, 125, 70, 3], [700, 3200, null, 25, 35, 0], [1200, 3300, null, 35, 50, 0], [0, 3250, null, 48, 40, 0],
	[1000, 3100, null, 14, 25, 0], [200, 3450, null, 55, 40, 0], [1100, 3550, null, 20, 35, 0], [-300, 3000, null, 30, 40, 0],
	// F3: the Long-hall, Bread-stone Lane, Herb Terraces (stepped below), the Cradle Wood, the Paper Hollow's ring, the
	// Strangler Arch, the Root-Mine head, the Bell-Tree, the Crystal Roots.
	[600, 3900, null, 32, 45, 0], [800, 4050, null, 30, 40, 0], [350, 4250, null, 40, 40, 0], [900, 4200, null, 35, 40, 0],
	[300, 4000, null, 20, 35, 0], [500, 4350, null, 18, 30, 0], [200, 4450, null, 16, 30, 0], [200, 4545, null, 10, 20, 0],
	// F5: the Bower's 100 m floor, the Sky Larder, the Canopy Lift's foot, the Rootgrave Fort, the Lantern Line, the
	// Rotted Treant, the Canopy Shrine's foot.
	[-550, 4250, null, 50, 45, 0], [-650, 3900, null, 22, 30, 0], [-600, 2905, null, 18, 30, 0], [-750, 3250, null, 35, 40, 0], [-630, 3400, null, 46, 30, 0],
	[-700, 3450, null, 20, 35, 0], [-790, 3345, null, 30, 40, 0], [-620, 3050, null, 14, 25, 0],
	// The Clearing of Four Hearts: 140 m flat at 210 m, the altar terrace behind the west heart-tree.
	[HEARTS.x, HEARTS.y, HEARTS.z, HEARTS.r, 26, 0], [-588, 3600, HEARTS.z + 1.5, 12, 10, 0],
];
for (const [x, y, H0, rc, rf, dome] of ANCHORS) {
	const H = H0 === null ? hAt(x, y) : H0;
	around(x, y, rc + rf, (k, d, sx, sy) => {
		const w = 1 - sstep(rc, rc + rf, d);
		const top = H + dome * (1 - Math.min(1, d / Math.max(1, rc)) ** 2) + (d < rc ? 0.35 * fbm(sx / 9, sy / 9, 2) : 0);
		HF[k] = lerp(HF[k], top, w);
	});
}
// The Antler Barrow: a long turf mound 44 x 14 m, 4 m high, on the hilltop, its door facing south-east.
const BARROW = { x: 1700, y: 2700, yaw: 140, len: 44, wid: 14, h: 4 };
around(BARROW.x, BARROW.y, 40, (k, d, sx, sy) => {
	const c = Math.cos(BARROW.yaw * DEG), s = Math.sin(BARROW.yaw * DEG), lx = (sx - BARROW.x) * c + (sy - BARROW.y) * s, ly = -(sx - BARROW.x) * s + (sy - BARROW.y) * c;
	const e = Math.hypot(lx / (BARROW.len / 2), ly / (BARROW.wid / 2));
	if (e < 1.25) HF[k] += BARROW.h * Math.max(0, 1 - e * e) ** 0.6;
});
// The Herb Terraces (DC 2.4 #25): five stone-faced steps 9 m deep up a slope, falling north.
{
	const T = { x: 1100, y: 3850, yaw: -90 };
	const H = hAt(T.x, T.y);
	around(T.x, T.y, 60, (k, d, sx, sy) => {
		const along = (sx - T.x) * Math.cos(T.yaw * DEG) + (sy - T.y) * Math.sin(T.yaw * DEG);
		const across = -(sx - T.x) * Math.sin(T.yaw * DEG) + (sy - T.y) * Math.cos(T.yaw * DEG);
		if (Math.abs(across) > 30 || Math.abs(along) > 24) return;
		const step = Math.floor((along + 24) / 9.6);
		const w = 1 - sstep(24, 30, Math.abs(across));
		HF[k] = lerp(HF[k], H - 4 + step * 1.6, w);
	});
}
// The Spine's Rootgrave plug: a rockfall filling the saddle to 230 m, sheer on the forest side (sealed until the
// Graveyard opens). The pass mouth below it keeps the forest's side (headstones, lanterns).
around(ROOTGRAVE.x, ROOTGRAVE.y, 140, (k, d, sx, sy) => {
	const e = Math.hypot((sx - ROOTGRAVE.x) / 1.0, (sy - ROOTGRAVE.y) / 1.6);
	const v = ROOTGRAVE.plug + 6 * fbm(sx / 12, sy / 12, 3) - Math.max(0, e - 55) * 2.2;
	if (v > HF[k]) HF[k] = v;
});
// A rock rib across the Alder at the Root Bridge: the gorge cuts it (rims 20 m over the water).
around(-100, 2800, 70, (k, d, sx, sy) => {
	const across = Math.abs(sx + 100);
	const v = 208 + 3 * fbm(sx / 10, sy / 10, 2) - Math.max(0, across - 25) * 0.6;
	if (v > HF[k] && Math.abs(sy - 2800) < 60) HF[k] = lerp(HF[k], v, 1 - sstep(40, 60, Math.abs(sy - 2800)));
});

// ---------------------------------------------------------------- gorges and hollows (plan 2.2.6)
// Wasp Ravine: a 20 m cut with overhanging lips, east-west through the F4/F3 edge.
const RAVINE = [[800, 3560, 14], [860, 3552, 16], [920, 3548, 16], [990, 3556, 14]];
{
	const top = Math.max(...RAVINE.map(p => hAt(p[0], p[1])));
	for (let s = 0; s < RAVINE.length - 1; s++) {
		const [ax, ay, aw] = RAVINE[s], [bx, by, bw] = RAVINE[s + 1];
		walkLine([[ax, ay], [bx, by]], 2, (x, y, t) => {
			const w = lerp(aw, bw, t), floor = top - 20 + 2 * t;
			around(x, y, w / 2 + 30, (k, d, sx, sy) => {
				const out = Math.max(0, d - w / 2);
				const v = floor + (out < 2 ? out * 3 : 6 + (out - 2) * 9) + 2 * fbm(sx / 7, sy / 7, 2);
				if (v < HF[k]) HF[k] = v;
			});
		});
	}
	// The ravine's ends ramp up so a body can walk in (east and west).
	for (const [ex, ey, dir] of [[RAVINE[0][0], RAVINE[0][1], -1], [RAVINE[3][0], RAVINE[3][1], 1]]) {
		walkLine([[ex, ey], [ex + dir * 60, ey]], 2, (x, y, t) => around(x, y, 6, (k) => { const v = top - 20 + t * 22; if (v < HF[k]) HF[k] = v; }));
	}
}
const HOLLOWS = [[1300, 4400, 62, 8], [-150, 3600, 32, 4], [450, 3600, 16, 3]];   // the Sunken Orchard, the Coil Hollow, the Bear's Larder
for (const [x, y, r, dep] of HOLLOWS) around(x, y, r * 1.6, (k, d) => { HF[k] -= dep * (1 - sstep(r * 0.55, r * 1.6, d)); });

// ---------------------------------------------------------------- caves (plan 2.2.8): a flat pad for the AWRCave shell (its chamber
// +-9 m round the spot, its tunnel out along its yaw 23 m), the ground rising behind it.
const CAVES = [
	{ id: 'badger_sett', name: 'The Badger Sett', x: 1792, y: 4000, yaw: 180, zone: '1' },
	{ id: 'alder_grotto', name: 'The Alder Grotto', x: 294, y: 2664, yaw: 15, zone: '2', floor: 105.4 },
	{ id: 'root_mine', name: 'The Root-Mine', x: 500, y: 4350, yaw: 90, zone: '3' },
	{ id: 'foxfire_cellar', name: 'The Foxfire Cellar', x: 715, y: 3150, yaw: 120, zone: '4' },
	{ id: 'sinkroot', name: 'The Sinkroot', x: -630, y: 3400, yaw: 0, zone: '5' },
];
// The Sinkroot (DC 2.4 #35, C5): a 60 m shaft where the roots pulled the ground apart, a ledge spiralling down its wall
// (0.85 turns) to the floor where the cave opens; the squeeze from its floor to the Hollow Root's Iron Seam. Moved from
// the cook's (-650, +2,800) to beside the Iron Seam: the Hollow Root runs from the Eldest to the west heart-tree (plan
// 2.2.7) and the squeeze must meet it (plan 2.2.8, F-S3); (-650, +2,800) also sits on the Alder's spring.
const SINK = { x: -630, y: 3400, rTop: 42, rFloor: 28, depth: 60 };
for (const c of CAVES) {
	if (c.id === 'sinkroot') continue;
	const H = c.floor ?? hAt(c.x, c.y);
	const fx = Math.cos(c.yaw * DEG), fy = Math.sin(c.yaw * DEG);
	around(c.x, c.y, 40, (k, d, sx, sy) => {
		const along = (sx - c.x) * fx + (sy - c.y) * fy, across = Math.abs(-(sx - c.x) * fy + (sy - c.y) * fx);
		const inPad = along > -12 && along < 26 && across < 12;
		const w = inPad ? 1 : 1 - sstep(0, 8, Math.max(along - 26, -12 - along, across - 12));
		if (w > 0) HF[k] = lerp(HF[k], H, w);
		if (along < -12 && across < 20) HF[k] = Math.max(HF[k], H + Math.min(14, (-12 - along) * 1.4));   // the hillside behind
	});
	c.z = H;
}
{
	const rim = hAt(SINK.x, SINK.y);
	SINK.rim = rim; SINK.floor = rim - SINK.depth;
	const turns = 0.85, a0 = 0;
	around(SINK.x, SINK.y, SINK.rTop + 8, (k, d, sx, sy) => {
		if (d > SINK.rTop + 4) { HF[k] = Math.min(HF[k], rim + (d - SINK.rTop - 4) * 0.5); return; }
		let ang = Math.atan2(sy - SINK.y, sx - SINK.x) - a0; while (ang < 0) ang += 2 * Math.PI;
		const frac = ang / (2 * Math.PI * turns);
		const ledgeIn = lerp(SINK.rTop - 6, SINK.rFloor + 1, Math.min(1, frac));
		let v;
		if (d < SINK.rFloor) v = SINK.floor;
		else if (frac <= 1 && d >= ledgeIn - 0.5 && d <= ledgeIn + 5.5) v = rim - SINK.depth * frac;   // the ledge, 6 m wide
		else if (d > SINK.rTop) v = rim;
		else v = SINK.floor + (d - SINK.rFloor) * 0.15;                                                  // the shaft's floor bowl
		HF[k] = Math.min(HF[k], v);
	});
	const c = CAVES.find(c => c.id === 'sinkroot');
	c.x = SINK.x - 6; c.y = SINK.y; c.z = SINK.floor; c.yaw = 0;   // the cave on the shaft floor, its mouth toward the squeeze (east)
}

// ---------------------------------------------------------------- routes (Grovekin roads 6 m <= 12 deg, elk trails 3 m, the coast
// road, the village lanes; carved last so routes always walk). Points [x, y, height or null, flag]; flag 'b' = a bridge
// span starts here (no carving until the next point), 'f' = a ford (the road goes down into the river bed).
const ROUTES = [
	{ id: 'coast_road', kind: 'road', w: 6, pts: [[2300, 2800], [2330, 2950], [2290, 3150], [2200, 3330], [2160, 3450], [2250, 3540], [2350, 3600], [2300, 3800], [2240, 4000], [2200, 4200], [2250, 4400]] },
	{ id: 'neck_road', kind: 'road', w: 6, pts: [[470, 1760], [480, 1950], [470, 2150], [455, 2300], [450, 2380], [560, 2420], [650, 2490], [760, 2520], [900, 2525], [980, 2590], [1050, 2680], [1100, 2750, null, 'f'], [1150, 2810], [1300, 2860], [1450, 2850], [1620, 2790], [1700, 2735]] },
	{ id: 'barrow_road', kind: 'road', w: 6, pts: [[1700, 2735], [1880, 2780], [2050, 2790], [2300, 2800]] },
	{ id: 'falls_road', kind: 'road', w: 6, pts: [[900, 2525], [800, 2620], [600, 2660], [450, 2668], [360, 2672]] },
	// From the falls' foot the old road climbs the scarp's north shoulder where the step fades, runs the north rim west,
	// crosses the gorge on the Root Bridge and goes on into the deep wood.
	{ id: 'bridge_road', kind: 'road', w: 6, pts: [[450, 2668], [470, 2560], [380, 2470], [250, 2520], [170, 2600], [80, 2680], [0, 2740], [-60, 2770], [-100, 2784, null, 'b'], [-100, 2816], [-160, 2860], [-250, 2930], [-300, 2968]] },
	{ id: 'eldest_west', kind: 'road', w: 6, pts: [[-250, 2930], [-380, 2900], [-470, 2895], [-560, 2905]] },
	{ id: 'canopy_road', kind: 'road', w: 5, pts: [[-560, 2905], [-600, 2960], [-620, 3050], [-660, 3150], [-720, 3230], [-750, 3250]] },
	{ id: 'grove_road', kind: 'road', w: 6, pts: [[1100, 3550], [1060, 3380], [1000, 3150], [880, 3150], [750, 3195], [700, 3200]] },
	{ id: 'clearcut_road', kind: 'road', w: 6, pts: [[700, 3200], [480, 3215], [300, 3240], [120, 3240], [0, 3200], [-120, 3120], [-210, 3040], [-262, 3016]] },
	{ id: 'village_road', kind: 'road', w: 6, pts: [[2160, 3450], [1950, 3470], [1700, 3500], [1450, 3540], [1250, 3560], [1100, 3550]] },
	{ id: 'terrace_road', kind: 'road', w: 6, pts: [[1100, 3550], [1120, 3700], [1060, 3820], [900, 3870], [700, 3900], [600, 3900]] },
	{ id: 'lane_road', kind: 'road', w: 5, pts: [[600, 3900], [700, 3990], [800, 4050], [700, 4120], [500, 4080], [380, 4020], [300, 4000], [200, 4030]] },
	{ id: 'cradle_road', kind: 'road', w: 5, pts: [[300, 4000], [320, 4120], [350, 4250], [430, 4320], [500, 4350]] },
	{ id: 'crystal_road', kind: 'road', w: 5, pts: [[500, 4350], [380, 4420], [250, 4450], [200, 4450], [190, 4520], [200, 4545]] },
	{ id: 'hive_road', kind: 'road', w: 5, pts: [[800, 4050], [880, 4130], [900, 4165]] },
	{ id: 'south_road', kind: 'road', w: 6, pts: [[2250, 4400], [2050, 4330], [1850, 4260], [1640, 4250], [1500, 4320], [1350, 4380], [1150, 4330], [1000, 4250], [935, 4215]] },
	{ id: 'ringfort_road', kind: 'road', w: 5, pts: [[1640, 4250], [1600, 4235]] },
	{ id: 'hedge_road', kind: 'road', w: 5, pts: [[2300, 3800], [2100, 3820], [1950, 3800], [1880, 3900], [1830, 4000], [1850, 4120], [1850, 4260]] },
	{ id: 'farm_road', kind: 'road', w: 5, pts: [[2290, 3150], [2100, 3100], [1900, 3050], [1700, 3150], [1550, 3250], [1450, 3400], [1450, 3540]] },
	{ id: 'west_road', kind: 'road', w: 5, pts: [[200, 4030], [60, 4040], [-80, 4060], [-250, 4120], [-400, 4180], [-490, 4225]] },
	{ id: 'larder_road', kind: 'road', w: 5, pts: [[-400, 4180], [-480, 4060], [-600, 3960], [-650, 3900], [-660, 3780], [-610, 3720], [-540, 3705]] },
	{ id: 'rootgrave_road', kind: 'road', w: 5, pts: [[-660, 3780], [-700, 3600], [-700, 3450], [-760, 3380], [-790, 3300], [-750, 3250], [-850, 3290], [-930, 3300]] },
	{ id: 'coil_road', kind: 'road', w: 5, pts: [[0, 3200], [-60, 3350], [-150, 3480], [-160, 3600], [-120, 3700], [-60, 3820], [60, 3900], [200, 3960], [300, 4000]] },
	{ id: 'larder_track', kind: 'road', w: 5, pts: [[300, 3240], [380, 3400], [440, 3540], [450, 3600], [520, 3700], [600, 3800], [600, 3900]] },
	{ id: 'ravine_track', kind: 'road', w: 5, pts: [[1060, 3380], [960, 3450], [880, 3500], [740, 3560], [600, 3570], [450, 3600]] },
	// The Fern Stair (DC 2.4 #32): stone steps from the floor below up to the Clearing's east edge.
	{ id: 'fern_stair', kind: 'stair', w: 5, pts: [[-300, 3660, null], [-356, 3640, null], [-420, 3618, HEARTS.z]] },
	{ id: 'hearts_track', kind: 'road', w: 5, pts: [[-160, 3600], [-240, 3640], [-300, 3660]] },
	// Elk trails (3 m): where the herds walk between grazing, water and cover.
	{ id: 'elk_ford_north', kind: 'trail', w: 3, pts: [[1100, 2750, null, 'f'], [1060, 2640], [1150, 2520], [1300, 2440], [1500, 2420], [1700, 2440]] },
	{ id: 'elk_meadow', kind: 'trail', w: 3, pts: [[1700, 2735], [1700, 2900], [1650, 3050], [1550, 3250], [1400, 3200], [1200, 3300], [1050, 3250], [1000, 3150]] },
	{ id: 'elk_falls', kind: 'trail', w: 3, pts: [[1150, 2810], [950, 2790], [750, 2770], [560, 2770], [430, 2760], [360, 2760]] },
	{ id: 'elk_deep', kind: 'trail', w: 3, pts: [[120, 3240], [140, 3360], [200, 3450], [260, 3560], [300, 3680], [450, 3750], [600, 3800]] },
	{ id: 'elk_eldest', kind: 'trail', w: 3, pts: [[-262, 3016], [-215, 3130], [-210, 3240], [-260, 3360], [-320, 3480], [-356, 3640]] },
	{ id: 'elk_canopy', kind: 'trail', w: 3, pts: [[-620, 3050], [-655, 3180], [-680, 3300], [-700, 3450]] },
	{ id: 'elk_bower', kind: 'trail', w: 3, pts: [[-490, 4225], [-420, 4380], [-300, 4480], [-100, 4500], [100, 4470], [200, 4450]] },
];
// Village lanes (inside the 300 m terrace): a cross of lanes and a ring.
{
	const L = [];
	for (let a = 0; a < 360; a += 30) L.push([VILLAGE.x + Math.cos(a * DEG) * 95, VILLAGE.y + Math.sin(a * DEG) * 95]);
	L.push(L[0].slice());
	ROUTES.push({ id: 'village_ring', kind: 'road', w: 5, lane: true, pts: L });
	ROUTES.push({ id: 'village_lane_ew', kind: 'road', w: 5, lane: true, pts: [[VILLAGE.x - 140, VILLAGE.y], [VILLAGE.x + 140, VILLAGE.y + 10]] });
	ROUTES.push({ id: 'village_lane_ns', kind: 'road', w: 5, lane: true, pts: [[VILLAGE.x - 5, VILLAGE.y - 140], [VILLAGE.x + 5, VILLAGE.y + 140]] });
}
const inRiver = (x, y) => { const k = idx(x, y); return k >= 0 && CH[k] === 1; };
// Resolve heights from the ground; fords sit in the river bed; grade every route by relaxing it.
for (const r of ROUTES) {
	for (const p of r.pts) {
		if (p[2] === null || p[2] === undefined) p[2] = Math.max(2, hAt(p[0], p[1]));
		if (p[3] === 'f') p[2] = Math.min(p[2], hAt(p[0], p[1]) + 0.2);
	}
	const dense = [];
	for (let s = 0; s < r.pts.length - 1; s++) {
		const [ax, ay, ah, af] = r.pts[s], [bx, by, bh] = r.pts[s + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 6));
		for (let q = 0; q < n; q++) {
			const t = q / n, x = lerp(ax, bx, t), y = lerp(ay, by, t);
			const lin = lerp(ah, bh, t), g = Math.max(1.5, hAt(x, y));
			dense.push([x, y, af === 'b' ? lin : r.kind === 'stair' ? lin : lerp(lin, g, 0.6 * Math.sin(Math.PI * t)), af === 'b' ? 1 : 0, (q === 0) ? 1 : 0]);
		}
	}
	const last = r.pts[r.pts.length - 1];
	dense.push([last[0], last[1], last[2], 0, 1]);
	for (let it = 0; it < 60; it++) for (let q = 1; q < dense.length - 1; q++) if (!dense[q][4] && !dense[q][3]) dense[q][2] = lerp(dense[q][2], (dense[q - 1][2] + dense[q + 1][2]) / 2, 0.5);
	r.dense = dense;
}
for (const r of ROUTES) {
	const half = r.w / 2, blend = r.kind === 'trail' ? 5 : 8;
	for (let q = 0; q < r.dense.length - 1; q++) {
		if (r.dense[q][3]) continue;   // the bridge span: the deck carries it
		const [ax, ay, ah] = r.dense[q], [bx, by, bh] = r.dense[q + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 1.5));
		for (let s = 0; s <= n; s++) {
			const t = s / n, x = lerp(ax, bx, t), y = lerp(ay, by, t), H = lerp(ah, bh, t);
			around(x, y, half + blend, (k, d) => {
				const w = 1 - sstep(half, half + blend, d);
				if (w <= 0) return;
				let target = r.kind === 'stair' ? Math.round(H / 0.4) * 0.4 : H;
				if (CH[k] && target > HF[k]) return;        // routes never fill a channel: they ford it
				if (r.kind === 'trail') target = lerp(HF[k], target, 0.7);
				HF[k] = lerp(HF[k], target, w);
				// The way's worn surface (its middle; the blend edge stays the ground's), never over the river's own.
				if (w > 0.6 && SURFM[k] !== SURF.bed && SURFM[k] !== SURF.bank) SURFM[k] = r.kind === 'trail' ? SURF.trail : SURF.road;
			});
		}
	}
}
applyRivers();

// ---------------------------------------------------------------- the Hollow Root (plan 2.2.7, DC 2.5): rooms carved as flat-floored
// pits 10-60 m under the forest floor, joined by 6-10 m passages, floors dropping room to room; the runtime roofs them
// with root mats so they are dark (AWR... in WRForestHollow.cpp). From the Eldest's Door west and north along the
// Eldest's roots to the Sinkroot (the Iron Seam's squeeze), then the Heart Root runs south under the Elder Canopy to
// the Four Hearts' west tree, one way.
const ROOMS = [
	{ id: 'root_hall', name: 'The Root Hall', x: -322, y: 3052, hx: 20, hy: 12, yaw: 200, dep: 12 },
	{ id: 'cradle_vault', name: 'The Cradle Vault', x: -262, y: 3098, hx: 15, hy: 11, yaw: 120, dep: 15 },
	{ id: 'grove_of_bones', name: 'The Grove of Bones', x: -318, y: 3142, hx: 16, hy: 10, yaw: 180, dep: 18 },
	{ id: 'weeping_roots', name: 'The Weeping Roots', x: -385, y: 3122, hx: 11, hy: 9, yaw: 210, dep: 21 },
	{ id: 'mossmaul_den', name: 'Mossmaul Den', x: -445, y: 3085, hx: 12, hy: 10, yaw: 240, dep: 24 },
	{ id: 'bootlace_gallery', name: 'The Bootlace Gallery', x: -505, y: 3122, hx: 14, hy: 7, yaw: 300, dep: 26 },
	{ id: 'thrumming_root', name: 'The Thrumming Root', x: -472, y: 3182, hx: 11, hy: 11, yaw: 0, dep: 29 },
	{ id: 'drowned_root', name: 'The Drowned Root', x: -410, y: 3215, hx: 13, hy: 10, yaw: 200, dep: 32, water: 3 },
	{ id: 'seed_store', name: 'The Seed Store', x: -440, y: 3272, hx: 8, hy: 7, yaw: 180, dep: 33 },
	{ id: 'wasp_grave', name: 'The Wasp Grave', x: -500, y: 3262, hx: 10, hy: 9, yaw: 120, dep: 34 },
	{ id: 'glowworm_chasm', name: 'The Glowworm Chasm', x: -540, y: 3312, hx: 14, hy: 14, yaw: 0, dep: 35, chasm: 25 },
	{ id: 'lattice_chamber', name: 'The Lattice Chamber', x: -500, y: 3372, hx: 11, hy: 11, yaw: 90, dep: 58 },
	{ id: 'iron_seam', name: 'The Iron Seam', x: -550, y: 3412, hx: 10, hy: 7, yaw: 160, dep: 58 },
	{ id: 'heart_root', name: 'The Heart Root', x: -567, y: 3586, hx: 5, hy: 5, yaw: 45, dep: 0 },
];
// The Heart Root's tunnel: a root way 8 m wide from the Iron Seam south, climbing to the Clearing's floor beside the west
// heart-tree (one way: WRForestHollow.cpp's root curtain opens only from inside; the floor meets the Clearing's).
const HEART_ROOT = [[-550, 3412], [-575, 3460], [-595, 3520], [-588, 3560], [-567, 3586]];
const surface0 = new Float32Array(N * N); surface0.set(HF);   // the forest floor before the Hollow Root (the roof's height)
const HOLLOW_CELLS = [];
{
	// Floors: each room's floor is the ground over it minus its depth (the chasm's floor 25 m below its entry).
	for (const R of ROOMS) {
		const g = hAt(R.x, R.y);
		R.top = g; R.floor = g - R.dep;
		if (R.id === 'lattice_chamber' || R.id === 'iron_seam') R.floor = Math.min(R.floor, ROOMS[10].floor - (ROOMS[10].chasm || 0));
		if (R.id === 'heart_root') R.floor = HEARTS.z - 0.3;
	}
	// Keep the floors falling room to room (no climbs) until the chasm's bottom.
	for (let i = 1; i < 13; i++) if (ROOMS[i].floor > ROOMS[i - 1].floor - 2) ROOMS[i].floor = ROOMS[i - 1].floor - 2;
	ROOMS[11].floor = Math.min(ROOMS[11].floor, ROOMS[10].floor - ROOMS[10].chasm);
	ROOMS[12].floor = ROOMS[11].floor - 1;
	const carve = (k, v) => { if (v < HF[k] - 0.5) SURFM[k] = SURF.hollow; if (v < HF[k]) { HF[k] = v; } };
	const markCell = (x, y) => HOLLOW_CELLS.push([x, y]);
	for (const R of ROOMS) {
		const c = Math.cos(R.yaw * DEG), s = Math.sin(R.yaw * DEG), rad = Math.hypot(R.hx, R.hy) + 3;
		around(R.x, R.y, rad, (k, d, sx, sy) => {
			const lx = (sx - R.x) * c + (sy - R.y) * s, ly = -(sx - R.x) * s + (sy - R.y) * c;
			const e = Math.max(Math.abs(lx) / R.hx, Math.abs(ly) / R.hy);
			if (e > 1) return;
			let v = R.floor + 0.4 * fbm(sx / 5, sy / 5, 2);
			if (R.chasm) {
				// The Glowworm Chasm: the entry ledge round the rim, a root stair spiralling down the wall to a floor 25 m below.
				const rr = Math.hypot(sx - R.x, sy - R.y), r0 = Math.min(R.hx, R.hy);
				const prev = ROOMS[ROOMS.indexOf(R) - 1], a0 = Math.atan2(prev.y - R.y, prev.x - R.x);
				let ang = Math.atan2(sy - R.y, sx - R.x) - a0; while (ang < 0) ang += 2 * Math.PI;
				const frac = ang / (2 * Math.PI * 1.1);
				if (rr > r0 - 3.5) v = R.floor - R.chasm * Math.min(1, frac);
				else v = R.floor - R.chasm;
			}
			carve(k, v);
		});
		for (let a = -R.hx; a <= R.hx; a += 8) for (let b = -R.hy; b <= R.hy; b += 8) markCell(R.x + a * c - b * s, R.y + a * s + b * c);
	}
	// Passages between rooms in order (8 m, graded floor to floor), the Eldest's Door ramp down into the Root Hall, the
	// Truffler's dig (a short drop into the Cradle Vault), the Heart Root's tunnel.
	const passage = (pts, w, z0, z1) => {
		const L = lineLen(pts);
		walkLine(pts, 1.5, (x, y, t, sIdx, along) => {
			const v = lerp(z0, z1, along / Math.max(1, L));
			around(x, y, w / 2, (k) => carve(k, v + 0.3 * fbm(x / 4, y / 4, 2)));
			if (Math.round(along) % 6 === 0) markCell(x, y);
		});
	};
	for (let i = 0; i < 12; i++) {
		const A = ROOMS[i], B = ROOMS[i + 1];
		const zA = A.chasm ? A.floor - A.chasm : A.floor;
		passage([[A.x, A.y], [B.x, B.y]], i === 11 ? 6 : 8, zA, B.floor);
	}
	const door = { x: -296, y: 3008 };
	passage([[door.x, door.y], [ROOMS[0].x, ROOMS[0].y]], 4, hAt(door.x, door.y) - 0.5, ROOMS[0].floor);
	passage(HEART_ROOT, 8, ROOMS[12].floor, HEARTS.z - 0.3);
	ROOMS.door = door;
	// F-S3 the Sinkroot squeeze (DC 2.22): a crawl 2 m wide from the shaft's floor at its east foot to the Iron Seam, graded
	// floor to floor; the Hollow Root's code roofs it at 2.1 m (WRForestHollow.cpp) like every passage, so it's a crouch.
	passage([[SINK.x + SINK.rFloor, SINK.y], [ROOMS[12].x, ROOMS[12].y]], 2, SINK.floor, ROOMS[12].floor);
}

// ---------------------------------------------------------------- the strip over the island's south shore
const ISL = { size: 4033, half: 2016 };
let island = null;
try { island = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16')); } catch { island = null; }
const islandH = (x, y) => {
	if (!island) return null;
	const i = Math.round(x + ISL.half), j = Math.round(y + ISL.half);
	if (i < 0 || j < 0 || i >= ISL.size || j >= ISL.size) return null;
	return (island.readUInt16LE((j * ISL.size + i) * 2) - 32768) / 32;
};
let joined = 0;
if (island) {
	for (let j = 0; j < N; j++) {
		const y = Y0 + j * STEP;
		if (y > ISL.half) continue;
		for (let i = 0; i < N; i++) {
			const x = X0 + i * STEP, ih = islandH(x, y);
			if (ih === null) continue;
			const k = j * N + i;
			if (ih > 0.5) { HF[k] = Math.min(HF[k], ih - 3); SURFM[k] = SURF.hidden; joined++; }        // the island's own ground wins: no z-fight
			else if (HF[k] > ih - 1 && HF[k] < 1 && !(x > NECK.x0 - 60 && x < NECK.x1 + 60)) HF[k] = Math.min(HF[k], ih - 2);
			// Wherever this land lies under the island's own ground (its sea floor too), the runtime land builds nothing:
			// one ground, the island's (WRLand drops a triangle with a hidden corner).
			if (HF[k] < ih - 0.25) SURFM[k] = SURF.hidden;
		}
	}
}

// ---------------------------------------------------------------- sub-zones (16 m grid)
const ZC = 16, ZN = Math.ceil((N - 1) * STEP / ZC);
const hollowSet = new Set(HOLLOW_CELLS.map(([x, y]) => `${Math.floor((x - X0) / ZC)},${Math.floor((y - Y0) / ZC)}`));
function subzone(x, y) {
	const h = hAt(x, y);
	if (hollowSet.has(`${Math.floor((x - X0) / ZC)},${Math.floor((y - Y0) / ZC)}`)) return 'h';
	if (h < 1.0) {
		const k = idx(x, y);
		if (k < 0 || LAND[k] < 0.5 || h < 0.3) return '~';
	}
	const cx = crestX(y);
	if (x < cx) return h > 1 ? 's' : '~';
	if (Math.hypot(x - ROOTGRAVE.x, (y - ROOTGRAVE.y) / 1.6) < 75) return 's';
	// The Spine's face: steeper than 35 deg within its foot's reach.
	const sl = Math.max(slopeAt(x, y), slopeAt(x + 6, y), slopeAt(x - 6, y));
	const footX = cx + Math.max(0, crestH(y) - trend(x, y)) / 1.85 + 40;
	if (x < footX && sl > 0.7) return 's';
	return designZone(x, y);
}
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) row += subzone(X0 + (i + 0.5) * ZC, Y0 + (j + 0.5) * ZC);
	zoneRows.push(row);
}
const zoneAt = (x, y) => { const i = Math.floor((x - X0) / ZC), j = Math.floor((y - Y0) / ZC); return (i < 0 || j < 0 || i >= ZN || j >= ZN) ? '~' : zoneRows[j][i]; };
console.error(`terrain ${((Date.now() - t0) / 1000).toFixed(1)} s`);

// ---------------------------------------------------------------- occupancy helpers for placement
const routeGrid = new Map();   // 8 m cells -> nearest route point info
for (const r of ROUTES) for (let q = 0; q < r.dense.length - 1; q++) {
	const [ax, ay, az] = r.dense[q], [bx, by, bz] = r.dense[q + 1];
	const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 3));
	for (let s = 0; s <= n; s++) {
		const x = lerp(ax, bx, s / n), y = lerp(ay, by, s / n), key = `${Math.floor(x / 8)},${Math.floor(y / 8)}`;
		if (!routeGrid.has(key)) routeGrid.set(key, []);
		routeGrid.get(key).push({ x, y, z: lerp(az, bz, s / n), r, w: r.w, dx: bx - ax, dy: by - ay });
	}
}
function nearRoute(x, y, R) {
	let best = null, bd = R;
	const c0 = Math.floor((x - R) / 8), c1 = Math.floor((x + R) / 8), d0 = Math.floor((y - R) / 8), d1 = Math.floor((y + R) / 8);
	for (let a = c0; a <= c1; a++) for (let b = d0; b <= d1; b++) {
		const L = routeGrid.get(`${a},${b}`); if (!L) continue;
		for (const p of L) { const d = Math.hypot(p.x - x, p.y - y) - p.w / 2; if (d < bd) { bd = d; best = p; } }
	}
	return best ? { ...best, d: bd } : null;
}
const riverDist = (x, y, R = 60) => {
	let bd = R;
	for (const Rv of RIVERS) for (const p of Rv.dense) { if (Math.abs(p.x - x) > R + 20 || Math.abs(p.y - y) > R + 20) continue; const d = Math.hypot(p.x - x, p.y - y) - p.w / 2; if (d < bd) bd = d; }
	return bd;
};
const riverSurfaceNear = (x, y) => {
	let best = null, bd = 1e9;
	for (const Rv of RIVERS) for (const p of Rv.dense) { const d = Math.hypot(p.x - x, p.y - y); if (d < bd) { bd = d; best = p; } }
	return best;
};
const inHollow = (x, y, pad = 0) => {
	for (const R of ROOMS) if (Math.hypot(R.x - x, R.y - y) < Math.hypot(R.hx, R.hy) + pad) return true;
	return zoneAt(x, y) === 'h' || (pad > 0 && [[pad, 0], [-pad, 0], [0, pad], [0, -pad]].some(([a, b]) => zoneAt(x + a, y + b) === 'h'));
};
const taken = [];   // [x, y, clear radius] of everything placed: sites, kit, trees with their gaps
// A POI keeps its spacing (r) from every other place; a solid thing (a giant's trunk, a trail tree: waymarks, not
// places) only keeps its own clearance, or the trail trees every 160 m down every road would leave no room for a POI.
const clearOf = (x, y, r) => taken.every(([tx, ty, tr, solid]) => Math.hypot(tx - x, ty - y) >= (solid ? tr : Math.max(r, tr)));

// ---------------------------------------------------------------- sites (DC 2.3 landmarks, 2.4 hand sites, 2.6 caves, camps, village)
const S = (id, kind, x, y, extra = {}) => ({ id, kind, name: extra.name, x, y, z: r1(hAt(x, y)), yaw: extra.yaw ?? 0, ...extra });
const sites = [
	// The twelve landmarks (the four arena ones are drawn by the boss track: four_hearts, clear_cut, paper_hollow, bower).
	S('four_hearts', 'four_hearts', HEARTS.x, HEARTS.y, { name: 'The Four Hearts', landmark: true, z: HEARTS.z }),
	S('eldest', 'eldest', -300, 3000, { name: 'The Eldest', landmark: true, yaw: 25 }),
	S('great_falls', 'great_falls', 300, 2650, { name: 'The Great Falls', landmark: true, yaw: 0 }),
	S('rotted_treant', 'rotted_treant', -790, 3345, { name: 'The Rotted Treant', landmark: true, yaw: 175 }),
	S('clear_cut', 'clear_cut', 600, 3300, { name: 'The Clear-cut', landmark: true }),
	S('paper_hollow', 'paper_hollow', 900, 4200, { name: 'The Paper Hollow', landmark: true }),
	S('bower', 'bower', -550, 4250, { name: 'The Bower', landmark: true }),
	S('hermits_oak', 'hermits_oak', 1100, 3550, { name: "The Hermit's Oak", landmark: true, yaw: 20 }),
	S('strangler_arch', 'strangler_arch', 300, 4000, { name: 'The Strangler Arch', landmark: true, yaw: 90 }),
	S('antler_barrow', 'antler_barrow', 1700, 2700, { name: 'The Antler Barrow', landmark: true, yaw: BARROW.yaw }),
	S('bell_tree', 'bell_tree', 200, 4450, { name: 'The Bell-Tree', landmark: true }),
	S('ringfort', 'ringfort', 1600, 4200, { name: 'The Ringfort', landmark: true, yaw: 200 }),
	// Hand-made sites (DC 2.4), with the side each faces (its yaw: toward the road that reaches it).
	S('wreck_meadow', 'wreck_meadow', 2390, 2905, { name: 'The Wreck Meadow', yaw: 160 }),
	S('farmstead', 'farmstead', 1900, 3050, { name: 'The abandoned farmstead', yaw: 270 }),
	S('mill_race', 'mill_race', 900, 2550, { name: 'The Mill Race', yaw: 180 }),
	S('truffle_glade', 'truffle_glade', 1200, 3300, { name: 'The Truffle glade', yaw: 200 }),
	S('woodcutters_camp', 'woodcutters_camp', 700, 3200, { name: "The Woodcutters' Camp", yaw: 200 }),
	S('hanging_oak', 'hanging_oak', 1550, 3250, { name: 'The Hanging Oak', yaw: 300 }),
	S('hernes_ford', 'hernes_ford', 1105, 2790, { name: "Herne's Ford", yaw: 270 }),
	S('falls_pool', 'falls_pool', 300, 2700, { name: 'The Great Falls pool', yaw: 180 }),
	S('root_bridge', 'root_bridge', -100, 2800, { name: 'The Root Bridge', yaw: 90 }),
	S('otter_steps', 'otter_steps', 600, 2445, { name: 'Otter Steps', yaw: 200 }),
	S('frosted_ferns', 'frosted_ferns', 450, 2380, { name: 'The Frosted Ferns', yaw: 0 }),
	S('lost_path', 'lost_path', 200, 3450, { name: 'The Lost Path' }),
	S('nurse_log', 'nurse_log', 0, 3250, { name: 'The Nurse Log', yaw: 160 }),
	S('bears_larder', 'bears_larder', 450, 3600, { name: "The Bear's Larder", yaw: 0 }),
	S('wasp_ravine', 'wasp_ravine', 900, 3550, { name: 'Wasp Ravine', yaw: 0 }),
	S('coil_hollow', 'coil_hollow', -150, 3600, { name: 'The Coil Hollow' }),
	S('offering_stones', 'offering_stones', 1000, 3100, { name: 'The Offering Stones', yaw: 270 }),
	S('long_hall', 'long_hall', 600, 3900, { name: 'The Grovekin Long-hall', yaw: 0 }),
	S('cradle_wood', 'cradle_wood', 350, 4250, { name: 'The Cradle Wood' }),
	S('herb_terraces', 'herb_terraces', 1100, 3850, { name: 'The Herb Terraces', yaw: 270 }),
	S('bread_stone_lane', 'bread_stone_lane', 800, 4050, { name: 'Bread-stone Lane', yaw: 30 }),
	S('root_mine', 'root_mine', 500, 4350, { name: 'The Root-Mine head', yaw: 90, cave: 'root_mine' }),
	S('sunken_orchard', 'sunken_orchard', 1300, 4400, { name: 'The Sunken Orchard' }),
	S('crystal_roots', 'crystal_roots', 200, 4545, { name: 'The Crystal Roots', yaw: 270 }),
	S('rootgrave_fort', 'rootgrave_fort', -750, 3250, { name: 'The Rootgrave Fort', yaw: 180 }),
	S('lantern_line', 'lantern_line', -700, 3450, { name: 'Lantern Line', yaw: 190 }),
	S('fern_stair', 'fern_stair', -432, 3618, { name: 'The Fern Stair', yaw: 160, z: HEARTS.z }),
	S('canopy_lift', 'canopy_lift', -600, 2905, { name: 'The Canopy Lift', yaw: 90 }),
	S('sky_larder', 'sky_larder', -650, 3900, { name: 'The Sky Larder', yaw: 0 }),
	S('sinkroot', 'sinkroot', SINK.x, SINK.y, { name: 'The Sinkroot', z: r1(SINK.rim), depth: SINK.depth }),
	S('eel_trap', 'eel_trap', 652, 2452, { name: 'The eel trap', yaw: 205 }),
];
for (const c of CAVES) {
	if (c.id === 'root_mine') continue;   // the Root-Mine head's own row carries its cave
	sites.push(S(c.id === 'sinkroot' ? 'sinkroot_cave' : c.id, 'cave_mouth', c.x, c.y, { name: c.name, yaw: c.yaw, cave: c.id, z: r1(c.z) }));
}
CAMPS.forEach((c, n) => sites.push(S(`camp_${n + 1}`, 'camp', c.x, c.y, { name: `Forest camp ${n + 1}`, yaw: yawOf(coastX(c.y) - c.x, 0) })));
sites.push(S('village', 'village', VILLAGE.x, VILLAGE.y, { name: 'Alderhaven', yaw: 0 }));
for (const s of sites) {
	s.zone = zoneAt(s.x, s.y);
	if (s.zone === 'h' || s.zone === 's') s.zone = designZone(s.x, s.y);
	taken.push([s.x, s.y, s.kind === 'camp' ? 70 : s.kind === 'village' ? 170 : s.landmark ? 55 : 35]);
}
for (const k of [[HEARTS.x, HEARTS.y, 95], [-550, 4250, 70], [600, 3300, 130], [900, 4200, 45]]) taken.push(k);

// ---------------------------------------------------------------- shrines (DC 2.11: five, guarded, fast travel)
const shrines = [
	{ id: 'grove_circle', name: 'The Grove Circle', x: 600, y: 3900, yaw: 0, guard: [] },
	{ id: 'hedge', name: 'Hedge Shrine', x: 1950, y: 3800, yaw: 90, guard: ['truffler', 'truffler'] },
	{ id: 'falls', name: 'Falls Shrine', x: 336, y: 2738, yaw: 300, guard: ['rootcoil'], deck: 2.4 },
	{ id: 'nurse', name: 'Nurse Shrine', x: 22, y: 3236, yaw: 160, guard: ['thornlynx', 'thornlynx'] },
	{ id: 'canopy', name: 'Canopy Shrine', x: -620, y: 3050, yaw: 90, guard: ['thornlynx', 'bramblewasp'], deck: 30 },
].map(s => ({ ...s, z: r1(hAt(s.x, s.y)), zone: designZone(s.x, s.y) }));
for (const s of shrines) taken.push([s.x, s.y, 30]);

// ---------------------------------------------------------------- arenas (DC 2.13, 2.14) and their fixed points
const P3 = (x, y, z) => [r1(x), r1(y), r1(z ?? hAt(x, y))];
const arenas = {};
{
	const hz = HEARTS.z, hearts = [[0, -55], [55, 0], [0, 55], [-55, 0]].map(([dx, dy]) => P3(HEARTS.x + dx, HEARTS.y + dy, hz));
	const stones = [45, 135, 225, 315].map(a => P3(HEARTS.x + Math.cos(a * DEG) * 40, HEARTS.y + Math.sin(a * DEG) * 40, hz));
	arenas.sylvaroth = { x: HEARTS.x, y: HEARTS.y, z: hz, radius: HEARTS.r, runback: P3(-372, 3626), altar: P3(-588, 3600, hz + 1.5), points: [...hearts, ...stones] };
	arenas.old_gnarl = { x: 600, y: 3300, z: r1(hAt(600, 3300)), radius: 125, runback: P3(778, 3292), altar: null, points: [P3(600, 3300)] };
	// The Paper Hollow: the dead oak's foot, the hive's three gallery floors (4, 9, 14 m) and the crown (18.5 m): one comb on each.
	const pz = hAt(900, 4200);
	arenas.bramblequeen = { x: 900, y: 4200, z: r1(pz), radius: 40, runback: P3(968, 4232), altar: null,
		points: [[4, 200], [9, 320], [14, 80], [18.5, 0]].map(([h, a], n) => P3(900 + Math.cos(a * DEG) * (n === 3 ? 3.2 : 6), 4200 + Math.sin(a * DEG) * (n === 3 ? 3.2 : 6), pz + h)), floors: [4, 9, 14, 18.5] };
	// The Bower: six hollow oaks 30 m apart on a ring (a hexagon of side 30 m).
	arenas.grey_matron = { x: -550, y: 4250, z: r1(hAt(-550, 4250)), radius: 45, runback: P3(-475, 4205), altar: null,
		points: [0, 60, 120, 180, 240, 300].map(a => P3(-550 + Math.cos((a + 15) * DEG) * 30, 4250 + Math.sin((a + 15) * DEG) * 30)) };
}

// ---------------------------------------------------------------- giants: the named ones first
const giants = [];
const G = (kind, x, y, h, r, extra = {}) => { const g = { kind, x: r1(x), y: r1(y), z: r1(hAt(x, y)), h: r1(h), r: r2(r), yaw: extra.yaw ?? Math.round(hash(Math.round(x), Math.round(y)) * 360), variant: extra.variant ?? 0, ...extra }; giants.push(g); return g; };
arenas.sylvaroth.points.slice(0, 4).forEach((p, n) => G('heart', p[0], p[1], 140, 4.5, { variant: n, z: p[2] }));
G('eldest', -300, 3000, 130, 5.2, { yaw: 25 });
G('treant', -790, 3345, 70, 3.2, { yaw: 175 });   // the Rotted Treant: fallen along its yaw toward the pass, root plate here
arenas.grey_matron.points.forEach((p, n) => G('bower_oak', p[0], p[1], 70, 2.6, { variant: n }));
G('bell_tree', 200, 4450, 72, 3.0, { yaw: 30 });
G('hanging_oak', 1550, 3250, 24, 1.1, { yaw: 300 });
G('hermit_oak', 1100, 3550, 26, 1.4, { yaw: 20 });
G('paper_oak', 900, 4200, 50, 2.2);
G('strangler', 300, 4000, 60, 4.2, { yaw: 90 });
// The Ringfort's wall: pleached beeches grown into one hedge, a ring 60 m across, the gate (a living lintel) facing its road.
for (let a = 0; a < 360; a += 15) {
	const gate = Math.abs(((a - 200 + 540) % 360) - 180) < 8;
	if (gate) continue;
	G('ringfort_beech', 1600 + Math.cos(a * DEG) * 30, 4200 + Math.sin(a * DEG) * 30, 18, 0.45, { yaw: a + 90, variant: a / 15 });
}
// Trees with a canopy job: the Canopy Lift's giant (its wheel hangs in the fork), the Canopy Shrine's, the Sky Larder's pair.
const liftGiant = G('giant', -616, 2914, 112, 3.9, { use: 'lift' });
const shrineGiant = G('giant', -634, 3062, 108, 3.8, { use: 'canopy_shrine' });
const larderA = G('giant', -650, 3886, 116, 4.0, { use: 'sky_larder' }), larderB = G('giant', -650, 3914, 110, 3.8, { use: 'sky_larder' });
for (const g of giants) taken.push([g.x, g.y, g.kind === 'ringfort_beech' ? 4 : g.r + 8, true]);

// ---------------------------------------------------------------- fallen giants (placed first, DC 3.5: 12 kit + the Nurse Log)
const fallen = [];
const kit = [];
let kitN = 0;
const addKit = (kind, x, y, extra = {}) => {
	const k = { id: `${kind}_${++kitN}`, kind, variant: extra.variant ?? 0, x: r1(x), y: r1(y), z: r1(extra.z ?? hAt(x, y)), yaw: Math.round(extra.yaw ?? 0), zone: designZone(x, y), ...extra };
	delete k.extraClear;
	kit.push(k); taken.push([x, y, extra.extraClear ?? 40, kind === 'trail_tree']);
	return k;
};
{
	// The Nurse Log: 90 m along the old road's south side, its broken amber end by the Nurse Shrine.
	const a = [-70, 3265], b = [16, 3236];
	const dir = [b[0] - a[0], b[1] - a[1]], L = Math.hypot(...dir), u = [dir[0] / L, dir[1] / L];
	const A = [b[0] - u[0] * 90, b[1] - u[1] * 90];
	fallen.push({ a: P3(A[0], A[1]), b: P3(b[0], b[1]), r: 2.4, nodes: 12, nurse: true });
}
const FALLEN_ZONES = '345';
let fallenN = 0;
for (let tries = 0; tries < 6000 && fallenN < 12; tries++) {
	const x = lerp(-820, 1380, rnd()), y = lerp(2950, 4600, rnd());
	const z = zoneAt(x, y);
	if (!FALLEN_ZONES.includes(z)) continue;
	// A fallen giant is seen from a road (15-60 m off it) and lies along the slope's contour.
	const nr = nearRoute(x, y, 60);
	if (!nr || nr.d < 16) continue;
	if (!clearOf(x, y, 55) || inHollow(x, y, 30) || riverDist(x, y, 40) < 30) continue;
	const L = 42 + rnd() * 26, gx = hAt(x + 3, y) - hAt(x - 3, y), gy = hAt(x, y + 3) - hAt(x, y - 3);
	let ang = Math.atan2(gy, gx) + Math.PI / 2 + (rnd() - 0.5) * 0.6;
	const ax = x - Math.cos(ang) * L / 2, ay = y - Math.sin(ang) * L / 2, bx = x + Math.cos(ang) * L / 2, by = y + Math.sin(ang) * L / 2;
	if (Math.abs(hAt(ax, ay) - hAt(bx, by)) > L * 0.18) continue;
	if (nearRoute(ax, ay, 10) || nearRoute(bx, by, 10) || inHollow(ax, ay, 15) || inHollow(bx, by, 15)) continue;
	if (!clearOf(ax, ay, 25) || !clearOf(bx, by, 25)) continue;
	const r = 1.6 + rnd() * 0.9, nodes = 4 + Math.floor(rnd() * 5);
	fallen.push({ a: P3(ax, ay), b: P3(bx, by), r: r2(r), nodes });
	addKit('fallen_giant', x, y, { variant: fallenN % 3, yaw: Math.round(ang / DEG), extraClear: L / 2 + 20 });
	fallenN++;
}

// ---------------------------------------------------------------- kit POIs (DC 2.4 kit table), each with its reason
const roadPts = (pred) => { const out = []; for (const r of ROUTES) { if (!pred(r)) continue; for (let q = 0; q < r.dense.length; q += 2) out.push({ p: r.dense[q], r, q }); } return out; };
function placeKit(kind, want, zones, pick, gap = 140) {
	let n = 0;
	for (let tries = 0; tries < 8000 && n < want; tries++) {
		const c = pick();
		if (!c) continue;
		const { x, y } = c;
		if (!zones.includes(zoneAt(x, y))) continue;
		if (!clearOf(x, y, gap) || inHollow(x, y, 12) || slopeAt(x, y) > 0.35) continue;
		if (c.offRoad !== false && nearRoute(x, y, 5)) continue;
		if (riverDist(x, y, 20) < (kind === 'ford' ? -100 : 8)) continue;
		addKit(kind, x, y, { variant: n % c.variants, yaw: c.yaw ?? 0, ...(c.extra || {}) });
		n++;
	}
	return n;
}
// Beside a road: a point on a route of the kinds given, set off its side by `off` m, facing (or along) it.
const beside = (kinds, off, alongYaw) => () => {
	const all = beside.cache[kinds] ??= roadPts(r => kinds.includes(r.kind) && !r.lane);
	const e = all[Math.floor(rnd() * all.length)];
	if (!e) return null;
	const nx = e.r.dense[Math.min(e.q + 1, e.r.dense.length - 1)], px = e.r.dense[Math.max(e.q - 1, 0)];
	const dx = nx[0] - px[0], dy = nx[1] - px[1], l = Math.hypot(dx, dy) || 1, side = rnd() < 0.5 ? -1 : 1;
	const x = e.p[0] - dy / l * side * (off + e.r.w / 2), y = e.p[1] + dx / l * side * (off + e.r.w / 2);
	return { x, y, yaw: alongYaw ? yawOf(dx, dy) : yawOf(e.p[0] - x, e.p[1] - y), variants: 3, route: e.r.id };
};
beside.cache = {};
const counts = {};
// Trail trees: the Grovekin waymark, 120-200 m apart on every Grovekin road in the woods, bent at the knee and pointing
// along the road away from the Four Hearts (the warning, DC 2.20).
{
	let n = 0;
	for (const r of ROUTES.filter(r => r.kind === 'road' && !r.lane)) {
		let acc = 60 + rnd() * 60;
		for (let q = 1; q < r.dense.length; q++) {
			const [ax, ay] = r.dense[q - 1], [bx, by] = r.dense[q];
			acc += Math.hypot(bx - ax, by - ay);
			if (acc < 160) continue;
			const z = zoneAt(bx, by);
			if (z === '1' || z === '~' || z === 's') { acc = 100; continue; }
			let dx = bx - ax, dy = by - ay; const l = Math.hypot(dx, dy) || 1;
			if (Math.hypot(bx + dx - HEARTS.x, by + dy - HEARTS.y) < Math.hypot(bx - HEARTS.x, by - HEARTS.y)) { dx = -dx; dy = -dy; }
			const x = bx + dy / l * (r.w / 2 + 2.5), y = by - dx / l * (r.w / 2 + 2.5);
			if (!clearOf(x, y, 60) || inHollow(x, y, 6)) continue;
			addKit('trail_tree', x, y, { variant: n % 3, yaw: yawOf(dx, dy), route: r.id, extraClear: 30 });
			acc = 0; n++;
		}
	}
	counts.trail_tree = n;
}
counts.hedgerow_gap = placeKit('hedgerow_gap', 10, '1', beside(['road', 'trail'], 3, true));
counts.beehive_tree = placeKit('beehive_tree', 7, '13', beside(['road', 'trail'], 14, false), 150);
counts.house_tree = placeKit('house_tree', 8, '35', beside(['road'], 8, false), 120);
counts.field_ruin = placeKit('field_ruin', 6, '1', beside(['road', 'trail'], 22, false), 160);
counts.foxfire_bank = placeKit('foxfire_bank', 6, '45', beside(['road'], 9, true), 140);
counts.wasp_nest = placeKit('wasp_nest', 7, '34', beside(['road', 'trail'], 16, false), 140);
counts.kill_scene = placeKit('kill_scene', 6, '24', beside(['trail'], 7, false), 150);
counts.thornbrake_cache = placeKit('thornbrake_cache', 7, '45', beside(['road', 'trail'], 34, false), 150);
// Fords: where an elk trail or road meets a stream or the Alder in F2 (stepping stones with a fish run).
{
	let n = 0;
	const crossings = [];
	for (const r of ROUTES) for (let q = 0; q < r.dense.length; q += 2) { const [x, y] = r.dense[q]; if (inRiver(x, y)) crossings.push([x, y]); }
	for (const Rv of RIVERS) for (let q = 20; q < Rv.dense.length - 20 && n < 6; q += 37) {
		const p = Rv.dense[q];
		if (zoneAt(p.x, p.y) !== '2' && Rv.kind === 'river') continue;
		if (crossings.some(([cx, cy]) => Math.hypot(cx - p.x, cy - p.y) < 90)) continue;
		if (!clearOf(p.x, p.y, 150) || p.g > 0.2 || Rv.id === 'mill_race') continue;
		const nx = Rv.dense[q + 1].x - p.x, ny = Rv.dense[q + 1].y - p.y;
		addKit('ford', p.x, p.y, { variant: n % 2, yaw: yawOf(nx, ny), river: Rv.id, z: r1(p.S) });
		n++;
	}
	counts.ford = n;
}
// Canopy platforms (F5): walkway nodes round giants 20-40 m up; each gets its own giant, the walkways join them below.
const canopyNodes = [];
{
	const want = [[-560, 2985], [-700, 3010], [-690, 3125], [-560, 3830], [-740, 3960], [-610, 4010]];
	want.forEach(([x, y], n) => {
		const g = G('giant', x, y, 100 + (n % 3) * 9, 3.6 + (n % 2) * 0.3, { use: 'canopy_platform' });
		const toward = n < 3 ? [-616, 2914] : [-650, 3900];
		let dx = toward[0] - x, dy = toward[1] - y; const l = Math.hypot(dx, dy) || 1;
		const px = x + dx / l * (g.r + 3), py = y + dy / l * (g.r + 3);
		const deck = 20 + (n * 7) % 21;
		const k = addKit('canopy_platform', px, py, { variant: n % 2, yaw: yawOf(dx, dy), deck, extraClear: 20 });
		canopyNodes.push({ k, g, deck });
		taken.push([x, y, g.r + 8, true]);
	});
	counts.canopy_platform = canopyNodes.length;
}
counts.fallen_giant = fallenN;
// The 125 m rule along every route (fire-frost 4.5): any route point further than 125 m from a POI gets the kit piece
// that fits its zone, set beside the route.
const FILL = { '1': ['hedgerow_gap', 'field_ruin', 'beehive_tree'], '2': ['kill_scene', 'trail_tree', 'foxfire_bank'], '3': ['house_tree', 'beehive_tree', 'trail_tree'], '4': ['foxfire_bank', 'kill_scene', 'trail_tree', 'wasp_nest'], '5': ['house_tree', 'foxfire_bank', 'trail_tree'] };
let filled = 0;
const poiNear = (x, y, R) => [...sites, ...kit, ...shrines].some(p => Math.hypot(p.x - x, p.y - y) < R);
for (const r of ROUTES) {
	if (r.lane || r.kind === 'stair') continue;
	for (let q = 0; q < r.dense.length; q += 3) {
		const [x, y] = r.dense[q];
		if (poiNear(x, y, 125)) continue;
		const z = zoneAt(x, y), list = FILL[z];
		if (!list) continue;
		const kind = list[filled % list.length];
		const nx = r.dense[Math.min(q + 1, r.dense.length - 1)], px = r.dense[Math.max(q - 1, 0)];
		const dx = nx[0] - px[0], dy = nx[1] - px[1], l = Math.hypot(dx, dy) || 1, side = (filled % 2) ? 1 : -1;
		const off = kind === 'trail_tree' ? r.w / 2 + 2.5 : kind === 'hedgerow_gap' ? r.w / 2 + 3 : r.w / 2 + 12;
		const kx = x - dy / l * side * off, ky = y + dx / l * side * off;
		if (inHollow(kx, ky, 6) || slopeAt(kx, ky) > 0.45 || riverDist(kx, ky, 15) < 5) continue;
		let yaw = kind === 'trail_tree' || kind === 'hedgerow_gap' ? yawOf(dx, dy) : yawOf(x - kx, y - ky);
		addKit(kind, kx, ky, { variant: filled % 3, yaw, fill: true, extraClear: 30 });
		counts[kind] = (counts[kind] || 0) + 1;
		filled++;
	}
}
console.error(`kit ${kit.length} (route fill ${filled}): ${JSON.stringify(counts)}`);

// ---------------------------------------------------------------- generated giants (F4 every 55-70 m, F5 every 45-60 m), off routes,
// rivers, POI light gaps (25-40 m) and the Hollow Root, with a root crown and a thornbrake ring each
const giantCells = new Map();
const gkey = (x, y) => `${Math.floor(x / 40)},${Math.floor(y / 40)}`;
const giantNear = (x, y, R) => { const a0 = Math.floor((x - R) / 40), a1 = Math.floor((x + R) / 40), b0 = Math.floor((y - R) / 40), b1 = Math.floor((y + R) / 40); for (let a = a0; a <= a1; a++) for (let b = b0; b <= b1; b++) { const L = giantCells.get(`${a},${b}`); if (L) for (const g of L) if (Math.hypot(g.x - x, g.y - y) < R) return true; } return false; };
const addGiantCell = g => { const k = gkey(g.x, g.y); if (!giantCells.has(k)) giantCells.set(k, []); giantCells.get(k).push(g); };
giants.forEach(addGiantCell);
const poiGap = [...sites.map(s => [s.x, s.y, s.landmark ? 40 : 30]), ...kit.map(k => [k.x, k.y, k.kind === 'trail_tree' ? 12 : 28]), ...shrines.map(s => [s.x, s.y, 30]),
	[HEARTS.x, HEARTS.y, 85], [-550, 4250, 50], [600, 3300, 135], [900, 4200, 35], ...fallen.flatMap(f => [[f.a[0], f.a[1], 12], [f.b[0], f.b[1], 12], [(f.a[0] + f.b[0]) / 2, (f.a[1] + f.b[1]) / 2, 14]])];
let made = 0;
for (let gy = 2600; gy < 4720; gy += 7) for (let gx = -900; gx < 1450; gx += 7) {
	const x = gx + (hash(gx, gy) - 0.5) * 6, y = gy + (hash(gy, gx) - 0.5) * 6;
	const z = zoneAt(x, y);
	if (z !== '4' && z !== '5') continue;
	const spacing = z === '5' ? 45 + 15 * hash(gx + 3, gy) : 55 + 15 * hash(gx, gy + 3);
	if (giantNear(x, y, spacing)) continue;
	if (poiGap.some(([px, py, pr]) => Math.hypot(px - x, py - y) < pr)) continue;
	const h = z === '5' ? 90 + 40 * hash(gx + 7, gy + 1) : 60 + 40 * hash(gx + 1, gy + 7);
	const rr = 0.032 * h + 0.3;
	const ringOut = rr * 1.6 + 1.5 + 6;
	const nr = nearRoute(x, y, ringOut + 6);
	if (nr) continue;
	if (riverDist(x, y, 30) < ringOut + 4 || inHollow(x, y, ringOut + 4) || slopeAt(x, y) > 0.5) continue;
	const g = G(hash(gx + 11, gy + 5) < 0.06 ? 'dead' : 'giant', x, y, h, rr);
	addGiantCell(g);
	made++;
}
console.error(`giants ${giants.length} (${made} generated)`);

// ---------------------------------------------------------------- thornbrakes (DC 2.8; plan 2.6): a ring round every giant's root
// crown (most with a 2-3 m gap toward the nearest way, Yaw = the gap's direction), a closed ring round each thornbrake cache
// (Guards = its kit id), the Eldest's ring guarding its Door, walls across four old F4 roads with a detour each, and the
// Lost Path's wall with its gap.
const brakes = [];
let brakeArea = 0;
for (const g of giants) {
	if (!['giant', 'dead', 'eldest'].includes(g.kind)) continue;
	if (g.use === 'lift') continue;   // the lift's foot stays clear
	const inner = g.r * 1.6 + 1.5, depth = g.kind === 'eldest' ? 7 : 3 + 6 * hash(Math.round(g.x * 3), Math.round(g.y * 3));
	const nr = nearRoute(g.x, g.y, 120);
	const gapYaw = nr ? yawOf(nr.x - g.x, nr.y - g.y) : g.yaw;
	const closed = g.kind === 'eldest' || hash(Math.round(g.x), Math.round(g.y) + 17) < 0.2;
	brakes.push({ x: g.x, y: g.y, z: g.z, ring: true, r: r2(inner), depth: r2(depth), length: 0, yaw: gapYaw, h: r2(2 + 2 * hash(Math.round(g.y), Math.round(g.x))), gap: closed ? 0 : r2(2 + hash(Math.round(g.x) + 5, Math.round(g.y)) ), guards: g.kind === 'eldest' ? 'eldests_door' : '' });
	brakeArea += Math.PI * ((inner + depth) ** 2 - inner ** 2);
}
for (const k of kit.filter(k => k.kind === 'thornbrake_cache')) {
	const depth = [8, 6, 9][k.variant % 3];
	brakes.push({ x: k.x, y: k.y, z: k.z, ring: true, r: 3, depth, length: 0, yaw: k.yaw, h: 3.2, gap: 0, guards: k.id });
	brakeArea += Math.PI * ((3 + depth) ** 2 - 9);
}
// Walls across four old roads in the deep wood (each 80-150 m round by its detour trail).
const detours = [];
{
	const want = [['clearcut_road', 0.42], ['coil_road', 0.3], ['larder_track', 0.45], ['ravine_track', 0.55]];
	for (const [rid, f] of want) {
		const r = ROUTES.find(r => r.id === rid), q = Math.floor(r.dense.length * f), p = r.dense[q];
		const nx = r.dense[q + 2], px = r.dense[q - 2];
		const dx = nx[0] - px[0], dy = nx[1] - px[1], l = Math.hypot(dx, dy), ux = dx / l, uy = dy / l;
		const depth = 6 + (detours.length % 3) * 2;
		brakes.push({ x: r1(p[0]), y: r1(p[1]), z: r1(p[2]), ring: false, r: 0, depth, length: r.w + 18, yaw: yawOf(-uy, ux), h: 3.4, gap: 0, guards: '' });
		brakeArea += depth * (r.w + 18);
		// The way round: off the road 55 m before, out 40-50 m to the freer side, back on 55 m after.
		let side = 1;
		const free = s => [0.5, 1].every(t => !giantNear(p[0] - uy * s * 45 * t, p[1] + ux * s * 45 * t, 14));
		if (!free(1) && free(-1)) side = -1;
		const ox = -uy * side, oy = ux * side;
		const pts = [[p[0] - ux * 55, p[1] - uy * 55], [p[0] - ux * 30 + ox * 38, p[1] - uy * 30 + oy * 38], [p[0] + ox * 48, p[1] + oy * 48], [p[0] + ux * 30 + ox * 38, p[1] + uy * 30 + oy * 38], [p[0] + ux * 55, p[1] + uy * 55]];
		const extra = lineLen(pts) - 110;
		detours.push({ id: `${rid}_detour`, kind: 'trail', w: 3, pts: pts.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]), extra: Math.round(extra) });
	}
}
console.error(`thornbrakes ${brakes.length}, ${(brakeArea / 1e6).toFixed(3)} km2 (detours ${detours.map(d => d.extra).join(', ')} m longer)`);

// ---------------------------------------------------------------- the Lost Path (DC 2.4 #16, F-S1): a foxfire loop that comes back on
// itself, and the true way the trail trees' knees point: four bends to a gap in a thornbrake wall and the hunter's cache in a
// buttress hollow.
const LOST = { x: 200, y: 3450 };
const lostLoop = [];
for (let a = 0; a <= 360; a += 15) lostLoop.push([LOST.x + Math.cos(a * DEG) * 48 + 6 * Math.sin(a * 3 * DEG), LOST.y + Math.sin(a * DEG) * 38]);
const lostTrue = [[LOST.x + 48, LOST.y], [LOST.x + 85, LOST.y + 40], [LOST.x + 70, LOST.y + 100], [LOST.x + 120, LOST.y + 135], [LOST.x + 95, LOST.y + 185], [LOST.x + 115, LOST.y + 215]];
const lostEnd = lostTrue[lostTrue.length - 1];
{
	for (let n = 1; n <= 4; n++) {
		const [ax, ay] = lostTrue[n], [bx, by] = lostTrue[n + 1];
		addKit('trail_tree', ax + 3, ay, { variant: n % 3, yaw: yawOf(bx - ax, by - ay), route: 'lost_path_true', lost: n, extraClear: 10 });
	}
	const [ax, ay] = lostTrue[4];
	const yw = yawOf(lostEnd[0] - ax, lostEnd[1] - ay);
	brakes.push({ x: r1(lostEnd[0] - Math.cos(yw * DEG) * 6), y: r1(lostEnd[1] - Math.sin(yw * DEG) * 6), z: r1(hAt(lostEnd[0], lostEnd[1])), ring: false, r: 0, depth: 6, length: 30, yaw: yw + 90, h: 3.5, gap: 2.4, guards: 'lost_path_end' });
}

// ---------------------------------------------------------------- walkways: the Bower's six (the boss track's AWRForestWalkway) and the
// Elder Canopy's (static): deck ends at the trunks' platforms, 15-25 m (Bower) and 20-40 m (canopy) up
const walkways = [];
{
	const oaks = arenas.grey_matron.points;
	for (let n = 0; n < 6; n++) {
		const A = oaks[n], B = oaks[(n + 1) % 6];
		const dx = B[0] - A[0], dy = B[1] - A[1], l = Math.hypot(dx, dy), ux = dx / l, uy = dy / l, R = 2.6 + 1.2;
		const deck = 15 + (n * 4) % 11;
		walkways.push({ id: `bower_${n + 1}`, a: [r1(A[0] + ux * R), r1(A[1] + uy * R), r1(A[2] + deck)], b: [r1(B[0] - ux * R), r1(B[1] - uy * R), r1(B[2] + deck + ((n % 2) ? 2 : -2))], arena: 'grey_matron', trees: [n, (n + 1) % 6] });
	}
	// The Elder Canopy's walkways: lift top -> platforms -> the Canopy Shrine (north), the Sky Larder's pair and its platforms (south).
	const node = (g, deck, toward) => { const dx = toward[0] - g.x, dy = toward[1] - g.y, l = Math.hypot(dx, dy) || 1; return [r1(g.x + dx / l * (g.r + 2)), r1(g.y + dy / l * (g.r + 2)), r1(g.z + deck)]; };
	const liftDeck = 30, shrineDeck = 30, larderDeck = 35;
	const join = (id, gA, dA, gB, dB) => walkways.push({ id, a: node(gA, dA, [gB.x, gB.y]), b: node(gB, dB, [gA.x, gA.y]), arena: '' });
	const [p1, p2, p3, p4, p5, p6] = canopyNodes;
	join('canopy_lift_1', liftGiant, liftDeck, p1.g, p1.deck);
	join('canopy_1_2', p1.g, p1.deck, p2.g, p2.deck);
	join('canopy_2_shrine', p2.g, p2.deck, shrineGiant, shrineDeck);
	join('canopy_shrine_3', shrineGiant, shrineDeck, p3.g, p3.deck);
	join('canopy_lift_2', liftGiant, liftDeck, p2.g, p2.deck);
	join('larder_span', larderA, larderDeck, larderB, larderDeck);
	join('larder_4', larderA, larderDeck, p4.g, p4.deck);
	join('larder_5', larderB, larderDeck, p5.g, p5.deck);
	join('canopy_5_6', p5.g, p5.deck, p6.g, p6.deck);
	// Canopy platform kit rows take the deck height of the walkway end nearest them (WRForestShape::CanopyDeckZ reads it).
	for (const c of canopyNodes) {
		let best = null, bd = 1e9;
		for (const w of walkways) for (const e of [w.a, w.b]) { const d = Math.hypot(e[0] - c.g.x, e[1] - c.g.y); if (d < bd) { bd = d; best = e; } }
		if (best) { c.k.x = best[0]; c.k.y = best[1]; c.k.deck = r1(best[2] - c.k.z); }
	}
}

// ---------------------------------------------------------------- spots: widowmakers (F4, dead limbs over paths), leaf pits (F5,
// carved), wasp nests (F3/F4 and Wasp Ravine's eight), Thornlynx perches (high on trunks over elk trails), foxfire banks
// (fallen wood, old roads, the Hollow Root, the Lost Path's loop), glowworms (stream banks, the falls' overhang, cave
// mouths, the Hollow Root), and the Hollow Root's ways in (the Eldest's Door, the Truffler's dig, the Heart Root's mouth,
// the Sinkroot squeeze).
const spots = [];
const spot = (kind, x, y, z, extra = {}) => spots.push({ kind, x: r1(x), y: r1(y), z: r1(z), r: r2(extra.r ?? 0), depth: r2(extra.depth ?? 0), yaw: Math.round(extra.yaw ?? 0) });
{
	// Widowmakers: a route point in F4 (and the F4/F5 edge) passing within 22 m of a giant: the limb hangs over the path.
	let n = 0;
	const gs = giants.filter(g => g.kind === 'giant' || g.kind === 'dead');
	for (const r of ROUTES) {
		if (r.lane) continue;
		for (let q = 0; q < r.dense.length && n < 26; q += 9) {
			const [x, y, z] = r.dense[q];
			if (zoneAt(x, y) !== '4') continue;
			if (spots.some(s => s.kind === 'widowmaker' && Math.hypot(s.x - x, s.y - y) < 110)) continue;
			const g = gs.find(g => Math.hypot(g.x - x, g.y - y) < 32);
			if (!g) continue;
			spot('widowmaker', x, y, z + 0.45 * g.h, { r: 2, yaw: yawOf(g.x - x, g.y - y) });
			n++;
		}
	}
	// Leaf pits: F5 floor 10-60 m off the routes, 4-9 m deep (the deep ones near the Sinkroot), carved below.
	let lp = 0;
	for (let tries = 0; tries < 9000 && lp < 22; tries++) {
		const x = lerp(-880, -380, rnd()), y = lerp(2850, 4520, rnd());
		if (zoneAt(x, y) !== '5' || inHollow(x, y, 15) || slopeAt(x, y) > 0.25) continue;
		const nr = nearRoute(x, y, 60);
		if (!nr || nr.d < 10) continue;
		if (giantNear(x, y, 12) || !clearOf(x, y, 30)) continue;
		if (spots.some(s => s.kind === 'leaf_pit' && Math.hypot(s.x - x, s.y - y) < 60)) continue;
		const nearSink = Math.hypot(x - SINK.x, y - SINK.y) < 450;
		const depth = nearSink ? 7 + 2 * rnd() : 4 + 3 * rnd(), rr = 2 + rnd();
		const z = hAt(x, y);
		around(x, y, rr + 1.5, (k, d) => { if (d < rr) HF[k] = Math.min(HF[k], z - depth); else HF[k] = Math.min(HF[k], z - 0.6 * (1 - (d - rr) / 1.5)); });
		spot('leaf_pit', x, y, z, { r: rr, depth });
		lp++;
	}
	// Wasp nests: each kit nest hangs on a bough by its row; Wasp Ravine's eight under its overhangs; more on F3 roads.
	for (const k of kit.filter(k => k.kind === 'wasp_nest')) {
		const g = giants.find(g => Math.hypot(g.x - k.x, g.y - k.y) < 25);
		spot('wasp_nest', k.x, k.y, k.z + 5 + (k.variant ? 3 : 0), { yaw: k.yaw, r: g ? 1 : 0 });
	}
	for (let n = 0; n < 8; n++) {
		const t = n / 7, p = RAVINE[Math.min(2, Math.floor(t * 3))], q = RAVINE[Math.min(3, Math.floor(t * 3) + 1)];
		const f = t * 3 - Math.floor(t * 3), x = lerp(p[0], q[0], f), y = lerp(p[1], q[1], f), side = n % 2 ? 1 : -1, w = lerp(p[2], q[2], f);
		spot('wasp_nest', x, y + side * (w / 2 - 1), hAt(x, y) + 12 + (n % 3) * 2, { yaw: side > 0 ? 90 : 270, r: 0, depth: 1 });
	}
	// Perches: a giant beside an elk trail in F4/F5 (or an F1 hedge edge): 6-12 m up the side facing the trail.
	let pc = 0;
	for (const r of ROUTES.filter(r => r.kind === 'trail' || r.id.startsWith('elk'))) {
		for (let q = 0; q < r.dense.length && pc < 44; q += 5) {
			const [x, y, z] = r.dense[q];
			const zz = zoneAt(x, y);
			if (zz !== '4' && zz !== '5' && zz !== '2') continue;
			if (spots.some(s => s.kind === 'perch' && Math.hypot(s.x - x, s.y - y) < 70)) continue;
			const g = giants.find(g => (g.kind === 'giant' || g.kind === 'dead') && Math.hypot(g.x - x, g.y - y) < 30);
			if (!g) continue;
			const dx = x - g.x, dy = y - g.y, l = Math.hypot(dx, dy) || 1;
			spot('perch', g.x + dx / l * (g.r + 0.6), g.y + dy / l * (g.r + 0.6), g.z + 6 + 6 * hash(pc, 3), { yaw: yawOf(dx, dy), r: 9 });
			pc++;
		}
	}
	// Foxfire: on the broken ends of fallen giants and along their flanks, on the old F4/F5 roads, round the Eldest's foot,
	// the Lost Path's loop (it glows: the trap), every Hollow Root room (12% light).
	for (const f of fallen) {
		const mx = (f.a[0] + f.b[0]) / 2, my = (f.a[1] + f.b[1]) / 2;
		const dx = f.b[0] - f.a[0], dy = f.b[1] - f.a[1], l = Math.hypot(dx, dy);
		spot('foxfire', mx - dy / l * (f.r + 0.3), my + dx / l * (f.r + 0.3), hAt(mx, my) + 0.3, { r: 4, yaw: yawOf(dx, dy) });
		spot('foxfire', f.b[0] - dx / l * 3, f.b[1] - dy / l * 3, f.b[2] + f.r * 0.5, { r: 2.5 });
	}
	for (const r of ROUTES.filter(r => r.kind === 'road' && !r.lane)) {
		for (let q = 0; q < r.dense.length; q += 23) {
			const [x, y, z] = r.dense[q];
			if (!'45'.includes(zoneAt(x, y)) || spots.some(s => s.kind === 'foxfire' && Math.hypot(s.x - x, s.y - y) < 80)) continue;
			spot('foxfire', x + 4, y + 2, z, { r: 3 });
		}
	}
	for (let a = 0; a < 360; a += 60) spot('foxfire', -300 + Math.cos(a * DEG) * 7, 3000 + Math.sin(a * DEG) * 7, hAt(-300, 3000) + 0.4, { r: 2.2 });
	for (let i = 0; i < lostLoop.length - 1; i += 2) spot('foxfire', lostLoop[i][0], lostLoop[i][1], hAt(lostLoop[i][0], lostLoop[i][1]) + 0.2, { r: 2 });
	for (const R of ROOMS) for (let n = 0; n < 3; n++) spot('foxfire', R.x + Math.cos((R.yaw + n * 120) * DEG) * R.hx * 0.7, R.y + Math.sin((R.yaw + n * 120) * DEG) * R.hy * 0.7, (R.chasm ? R.floor - R.chasm : R.floor) + 0.3, { r: 3 });
	// Glowworms: under the Alder's and the streams' banks in F2 (every ~90 m where a bank stands 1.5 m+ over the water),
	// under the Great Falls' overhang, at the cave mouths, in the Root Hall and down the Glowworm Chasm.
	for (const Rv of RIVERS) {
		for (let q = 10; q < Rv.dense.length; q += 30) {
			const p = Rv.dense[q];
			if (zoneAt(p.x, p.y) !== '2' && zoneAt(p.x, p.y) !== '4') continue;
			const nx = Rv.dense[Math.min(q + 1, Rv.dense.length - 1)].x - p.x, ny = Rv.dense[Math.min(q + 1, Rv.dense.length - 1)].y - p.y, l = Math.hypot(nx, ny) || 1;
			const side = q % 60 ? 1 : -1, bx = p.x - ny / l * side * (p.w / 2 + 1), by = p.y + nx / l * side * (p.w / 2 + 1);
			if (hAt(bx, by) - p.S < 1.5) continue;
			spot('glowworm', bx, by, p.S + 0.6, { r: 3, yaw: yawOf(-(-ny / l * side), -(nx / l * side)) });
		}
	}
	for (let n = 0; n < 5; n++) spot('glowworm', 298 - 2, 2680 + n * 5, 105 + 8 + n, { r: 4, yaw: 0 });
	for (const c of CAVES) spot('glowworm', c.x + Math.cos(c.yaw * DEG) * 22, c.y + Math.sin(c.yaw * DEG) * 22, c.z + 3.5, { r: 3, yaw: c.yaw });
	const rh = ROOMS[0], gc = ROOMS[10];
	for (let n = 0; n < 4; n++) spot('glowworm', rh.x + (n - 1.5) * 9, rh.y, rh.floor + 6, { r: 4 });
	for (let n = 0; n < 10; n++) spot('glowworm', gc.x + Math.cos(n * 36 * DEG) * (gc.hx - 1), gc.y + Math.sin(n * 36 * DEG) * (gc.hy - 1), gc.floor - gc.chasm + 3 + n * 2.2, { r: 3.5, yaw: n * 36 + 180 });
	// The Hollow Root's ways in.
	const door = ROOMS.door;
	spot('hollow_door', door.x, door.y, hAt(door.x, door.y), { r: 1.5, yaw: yawOf(ROOMS[0].x - door.x, ROOMS[0].y - door.y) });
	spot('truffler_dig', -220, 3080, hAt(-220, 3080), { r: 2, depth: r1(hAt(-220, 3080) - ROOMS[1].floor), yaw: yawOf(ROOMS[1].x + 220, ROOMS[1].y - 3080) });
	const hr = HEART_ROOT[HEART_ROOT.length - 1];
	spot('heart_root', hr[0], hr[1], HEARTS.z, { r: 4, depth: 0, yaw: yawOf(HEARTS.x - hr[0], HEARTS.y - hr[1]) });
	spot('sinkroot_squeeze', SINK.x + SINK.rFloor, SINK.y, SINK.floor, { r: 1.2, depth: r1(Math.hypot(ROOMS[12].x - SINK.x, ROOMS[12].y - SINK.y) - SINK.rFloor - ROOMS[12].hx), yaw: yawOf(ROOMS[12].x - SINK.x, ROOMS[12].y - SINK.y) });
	// The same four under the names the Hollow Root's code reads (WRForestHollow.h), and the secrets' anchors it needs:
	// F-S2 the Alder Grotto's squeeze (at the back of cave C2's chamber, under the glowworm overhang) and its dry back
	// room 14 m further into the scarp; F-S4 the barred door (Bread-stone Lane's house-tree 6: WRForestSceneryVillages
	// BreadStoneLane builds it at lane-local (7.7, 10.5) m with its door facing the lane, so the door's foot is here);
	// the eel trap at Otter Steps (its site row too).
	{
		const sq = spots.find(s => s.kind === 'sinkroot_squeeze'), dg = spots.find(s => s.kind === 'truffler_dig'), hr2 = spots.find(s => s.kind === 'heart_root');
		spots.push({ ...dg, kind: 'hollow_dig' }, { ...hr2, kind: 'hollow_heart' }, { ...sq, kind: 'hollow_squeeze' });
		const g = CAVES.find(c => c.id === 'alder_grotto'), back = (g.yaw + 180) * DEG;
		spot('grotto_squeeze', g.x + Math.cos(back) * 9, g.y + Math.sin(back) * 9, g.z, { r: 0.9, depth: 6, yaw: g.yaw + 180 });
		spot('grotto_room', g.x + Math.cos(back) * 20, g.y + Math.sin(back) * 20, g.z, { r: 5, depth: 3.2, yaw: g.yaw });
		const lane = sites.find(s => s.id === 'bread_stone_lane'), ly = lane.yaw * DEG, lx0 = 7.7, ly0 = 10.5 - 0.85 - 3.3;
		spot('barred_door', lane.x + lx0 * Math.cos(ly) - ly0 * Math.sin(ly), lane.y + lx0 * Math.sin(ly) + ly0 * Math.cos(ly), hAt(lane.x, lane.y) + 0.3, { r: 1, yaw: lane.yaw - 90 });
		const eel = sites.find(s => s.id === 'eel_trap');
		spot('eel_trap', eel.x, eel.y, eel.z, { r: 2, yaw: eel.yaw });
	}
}
// The Truffler's dig: a shaft 2 m round straight down from the soft spot to the Cradle Vault's roof level (the roof the
// Hollow Root's code lays over the vault: its floor + 2 x its half height), where the root plug sits until a tamed
// Truffler digs it out; below the plug a short root slide drops on into the vault's floor.
{
	const R = ROOMS[1], top = hAt(-220, 3080);
	const hz = Math.max(4, Math.min(12, R.top - R.floor) / 2), roofZ = Math.min(top - 2.5, R.floor + 2 * hz);
	around(-220, 3080, 2.2, (k) => { if (roofZ < HF[k]) { HF[k] = roofZ; SURFM[k] = SURF.hollow; } });
	walkLine([[-220, 3080], [R.x, R.y]], 1.5, (x, y, t) => around(x, y, 2.2, (k) => { const v = lerp(roofZ, R.floor, Math.min(1, t * 1.4)); if (v < HF[k]) { HF[k] = v; SURFM[k] = SURF.hollow; } }));
}
console.error(`spots ${JSON.stringify(spots.reduce((a, s) => (a[s.kind] = (a[s.kind] || 0) + 1, a), {}))}`);

// ---------------------------------------------------------------- the Hornwarden's roam loop and the Wild Hunt (DC 2.13.4, 2.10)
// The roam: Antler Barrow (rests at noon) -> Hanging Oak (dusk) -> Herne's Ford (drinks at nightfall) -> the meadow coast
// -> back, ~3.2 km, keeping 80 m+ off the camps and the village. The Hunt: the same three at a gallop on a wider loop
// through F1 and F2.
const loop = (pts) => pts.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]);
const roam = loop([[1700, 2740], [1820, 2880], [1760, 3060], [1640, 3180], [1550, 3250], [1420, 3160], [1280, 3020], [1160, 2880], [1105, 2790], [1150, 2700], [1290, 2600], [1470, 2470], [1700, 2440], [1930, 2470], [2120, 2560], [2140, 2680], [1960, 2740], [1800, 2720], [1700, 2740]]);
const hunt = loop([[1700, 2740], [1900, 2900], [1950, 3150], [1780, 3280], [1550, 3250], [1380, 3080], [1220, 2930], [1105, 2790], [980, 2660], [920, 2560], [1060, 2460], [1300, 2420], [1600, 2400], [1900, 2430], [2080, 2560], [2020, 2700], [1700, 2740]]);
for (const p of [...roam, ...hunt]) for (const c of [...CAMPS, VILLAGE]) if (Math.hypot(p[0] - c.x, p[1] - c.y) < 80 + (c.r || 0)) console.error(`WARNING: hunt/roam point ${p} near a safe zone`);
console.error(`roam ${Math.round(lineLen(roam))} m, hunt ${Math.round(lineLen(hunt))} m`);

// ---------------------------------------------------------------- falls (the Great Falls; the five Otter Steps)
const falls = [{ id: 'great_falls', top: [289, 2692, 151], bottom: [POOL.x - 8, POOL.y, POOL.S], width: 20 }];
{
	const R = RIVERS.find(r => r.id === 'otter_steps');
	R.pts.forEach((p, n) => { if (p[5] !== 's') return; const a = R.pts[n - 1]; const dx = p[0] - a[0], dy = p[1] - a[1], l = Math.hypot(dx, dy); falls.push({ id: `otter_${falls.length}`, top: [r1(p[0] - dx / l * 1.2), r1(p[1] - dy / l * 1.2), r1(a[2])], bottom: [r1(p[0]), r1(p[1]), r1(p[2])], width: p[3] }); });
}

// ---------------------------------------------------------------- write
const routesOut = ROUTES.map(r => ({ id: r.id, kind: r.kind, w: r.w, pts: r.dense.filter((_, q) => q % 2 === 0 || q === r.dense.length - 1).map(p => [r1(p[0]), r1(p[1]), r1(p[2])]) }));
for (const d of detours) routesOut.push({ id: d.id, kind: d.kind, w: d.w, pts: d.pts });
routesOut.push({ id: 'hornwarden', kind: 'roam', w: 8, pts: roam });
routesOut.push({ id: 'wild_hunt', kind: 'hunt', w: 12, pts: hunt });
routesOut.push({ id: 'lost_path_loop', kind: 'lost_path', w: 3, pts: loop(lostLoop) });
routesOut.push({ id: 'lost_path_true', kind: 'lost_path', w: 2, pts: loop(lostTrue) });
for (const w of walkways.filter(w => !w.arena)) routesOut.push({ id: `walk_${w.id}`, kind: 'walkway', w: 1.4, pts: [w.a, w.b] });
// The Hollow Root's passages (floor points) and the Heart Root, for the roofs and the creatures' paths.
{
	const pts = [[ROOMS.door.x, ROOMS.door.y, hAt(ROOMS.door.x, ROOMS.door.y)], ...ROOMS.slice(0, 13).map(R => [R.x, R.y, R.chasm ? R.floor - R.chasm : R.floor])];
	routesOut.push({ id: 'hollow_way', kind: 'hollow', w: 8, pts: pts.map(p => p.map(r1)) });
	routesOut.push({ id: 'heart_root', kind: 'hollow', w: 8, pts: HEART_ROOT.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]) });
}
const riversOut = RIVERS.map(R => {
	const keep = R.dense.filter((p, q) => q % 4 === 0 || q === R.dense.length - 1);
	return { id: R.id, kind: R.kind, name: R.name, pts: keep.map(p => [r1(p.x), r1(p.y), r2(p.S)]), widths: keep.map(p => r1(p.w)) };
});
const dr = ROOMS.find(r => r.id === 'drowned_root');
riversOut.push({ id: 'drowned_root', kind: 'pool', name: 'The Drowned Root', pts: [[r1(dr.x - dr.hx * 0.9), r1(dr.y), r1(dr.floor + dr.water)], [r1(dr.x + dr.hx * 0.9), r1(dr.y), r1(dr.floor + dr.water)]], widths: [r1(dr.hy * 1.8), r1(dr.hy * 1.8)] });
const out = {
	_comment: 'Generated by Tools/terrain/forest.mjs (edit the script, not this file). The Ancient forest, belt B5 east of the Spine (cooks/biome-forest.md, cooks/build-biome-forest.md): continent metres (+x east, +y south, origin the island centre; world cm = metres x 100). Parsed into FWRForestData (Source/Wildreach/WRForestTypes.cpp). frame: the landscape (2 m samples). zones: sub-zone letters on a 16 m grid (1 Meadows and Farmland, 2 Streams and Falls, 3 Grovekin Villages, 4 Old-Growth Groves, 5 Elder Canopy, s the Spine face, h the Hollow Root, ~ sea). Every z is ground (or water surface for rivers); deck = a raised floor above it.',
	frame: { x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, zScale: 100 * 128 / UNITS, islandEdge: ISLAND_EDGE },
	zones: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
	canopy: { '1': 0.04, '2': 0.4, '3': 0.4, '4': 0.75, '5': 0.9, h: 0.95, s: 0, '~': 0 },
	sites: sites.map(s => { const o = { ...s }; if (!o.name) delete o.name; return o; }),
	kit: kit.map(k => { const o = { ...k }; delete o.fill; return o; }),
	shrines, giants: giants.map(g => { const o = { ...g }; return o; }), fallen, routes: routesOut, rivers: riversOut, falls, brakes, spots,
	walkways: walkways.map(w => ({ id: w.id, a: w.a, b: w.b, arena: w.arena || undefined, trees: w.trees })),
	arenas,
	hollow: ROOMS.map(R => ({ id: R.id, name: R.name, x: r1(R.x), y: r1(R.y), z: r1(R.chasm ? R.floor - R.chasm : R.floor), entry: r1(R.floor), roof: r1(R.top), hx: R.hx, hy: R.hy, hz: r1(Math.max(4, Math.min(12, R.top - R.floor + (R.chasm || 0)) / 2)), yaw: R.yaw, water: R.water || 0, chasm: R.chasm || 0 })),
	village: { x: VILLAGE.x, y: VILLAGE.y, z: r1(hAt(VILLAGE.x, VILLAGE.y)), radius: VILLAGE.r, name: 'Alderhaven' },
	camps: CAMPS.map((c, n) => ({ id: `camp_${n + 1}`, x: c.x, y: c.y, z: r1(hAt(c.x, c.y)), yaw: yawOf(coastX(c.y) - c.x, 0) })),
	rootgrave: { saddle: [ROOTGRAVE.x, ROOTGRAVE.y, 190], plug: [ROOTGRAVE.x, ROOTGRAVE.y, ROOTGRAVE.plug], plugRadius: 75, mouth: P3(-860, 3300), scree: [-1000, 3300, 120] },
	sinkroot: { x: SINK.x, y: SINK.y, rim: r1(SINK.rim), floor: r1(SINK.floor), rTop: SINK.rTop, rFloor: SINK.rFloor, ledgeTurns: 0.85 },
	counts: { kit: counts, giants: giants.length, brakes: brakes.length, brakeKm2: +(brakeArea / 1e6).toFixed(3), spots: spots.length },
};
fs.writeFileSync(path.join(root, 'Data', 'forest.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- preview (8 m per pixel)
const PR = Math.floor((N - 1) * STEP / 8);
const img = Buffer.alloc(PR * PR * 3);
const ZCOL = { '1': [127, 163, 74], '2': [78, 122, 58], '3': [94, 140, 49], '4': [74, 58, 42], '5': [58, 82, 34], s: [90, 88, 86], h: [40, 30, 20] };
for (let j = 0; j < PR; j++) for (let i = 0; i < PR; i++) {
	const x = X0 + i * 8 + 4, y = Y0 + j * 8 + 4, h = hAt(x, y);
	const zz = zoneAt(x, y);
	let c = h < 0.5 ? [30, 70, 115] : (ZCOL[zz] || [100, 100, 100]);
	if (zz === '~' && h >= 0.5) c = [150, 150, 140];
	const dzdx = (hAt(x + 8, y) - hAt(x - 8, y)) / 16, dzdy = (hAt(x, y + 8) - hAt(x, y - 8)) / 16;
	const shade = Math.max(0.3, Math.min(1.4, 0.95 + (-dzdx * 0.6 - dzdy * 0.6) * 0.8));
	const k = (j * PR + i) * 3;
	const contour = Math.floor(h / 25) !== Math.floor(hAt(x - 8, y) / 25) ? 0.75 : 1;
	img[k] = Math.min(255, c[0] * shade * contour); img[k + 1] = Math.min(255, c[1] * shade * contour); img[k + 2] = Math.min(255, c[2] * shade * contour);
}
const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / 8), cj = Math.round((y - Y0) / 8); for (let j = cj - r; j <= cj + r; j++) for (let i = ci - r; i <= ci + r; i++) if (i >= 0 && j >= 0 && i < PR && j < PR && (i - ci) ** 2 + (j - cj) ** 2 <= r * r) img.set(col, (j * PR + i) * 3); };
for (const Rv of RIVERS) for (const p of Rv.dense) dot(p.x, p.y, Rv.kind === 'river' ? 1 : 0, [70, 140, 220]);
for (const r of ROUTES) for (const p of r.dense) dot(p[0], p[1], 0, r.kind === 'trail' ? [180, 140, 90] : [235, 200, 120]);
for (const p of roam) dot(p[0], p[1], 1, [255, 120, 40]);
for (const g of giants) dot(g.x, g.y, 0, [20, 40, 10]);
for (const f of fallen) walkLine([f.a, f.b], 4, (x, y) => dot(x, y, 0, [200, 120, 40]));
for (const k of kit) dot(k.x, k.y, 1, [30, 30, 30]);
for (const s of spots) if (s.kind === 'leaf_pit' || s.kind === 'widowmaker') dot(s.x, s.y, 1, [255, 60, 60]);
for (const s of sites) dot(s.x, s.y, s.landmark ? 4 : 2, s.landmark ? [185, 167, 255] : [255, 255, 255]);
for (const s of shrines) dot(s.x, s.y, 3, [255, 150, 40]);
for (const R of ROOMS) dot(R.x, R.y, 1, [255, 230, 80]);
function png(w, h, rgb) {
	const T = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
	const crc = buf => { let c = 0xffffffff; for (const b of buf) c = T[(c ^ b) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
	const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); };
	const ih = Buffer.alloc(13); ih.writeUInt32BE(w, 0); ih.writeUInt32BE(h, 4); ih[8] = 8; ih[9] = 2;
	const raw = Buffer.alloc((w * 3 + 1) * h);
	for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
	return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}
fs.mkdirSync(path.join(root, 'Docs', 'terrain'), { recursive: true });
fs.writeFileSync(path.join(root, 'Docs', 'terrain', 'forest.png'), png(PR, PR, img));

// ---------------------------------------------------------------- the runtime land (WRLand, Data/lands/forest.json)
// The forest is built at runtime on every machine by WRLand (no landscape import): its ground is this heightfield at 4 m
// (1,009 samples a side, bilinear between them), its colour from the sub-zone grid and the surface classes written beside
// the heights. Game cm = map metres x 100 (origin = mapCentre x 100), so every forest.json point is already a game
// point. File: uint16 heights (32768 + m * 16) row by row from (X0, Y0), then one surface byte per sample.
function writeLand() {
	const LS = 4, LN = Math.floor((N - 1) * STEP / LS) + 1, R = LS / STEP;
	const hb = Buffer.alloc(LN * LN * 2), sb = Buffer.alloc(LN * LN);
	for (let j = 0; j < LN; j++) for (let i = 0; i < LN; i++) {
		const ci = i * R, cj = j * R, k = cj * N + ci;
		// The lowest of the sample and its four 2 m neighbours: narrow cuts (the Eldest's Door, a leaf pit, the Sinkroot's
		// ledge, a ford) stay open at 4 m instead of being averaged shut.
		let v = HF[k];
		for (const [a, b] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
			const ii = ci + a, jj = cj + b;
			if (ii >= 0 && jj >= 0 && ii < N && jj < N && SURFM[jj * N + ii] !== SURF.hidden) v = Math.min(v, HF[jj * N + ii]);
		}
		hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + v * UNITS))), (j * LN + i) * 2);
		sb[j * LN + i] = SURFM[k];
	}
	fs.mkdirSync(path.join(root, 'Data', 'lands'), { recursive: true });
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'forest_ground.bin'), Buffer.concat([hb, sb]));
	const C = c => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];
	const mid = [X0 + (N - 1) * STEP / 2, Y0 + (N - 1) * STEP / 2];
	const campRows = CAMPS.map(c => [c.x, c.y, yawOf(coastX(c.y) - c.x, 0)]);
	const land = {
		_comment: 'Generated by Tools/terrain/forest.mjs (edit the script, not this file). The Ancient forest as a runtime land (WRLand, cooks/build-biome-forest.md 2.1-2.4): its ground is the heightfield in forest_ground.bin (4 m samples from x0, y0 in map metres: uint16 heights = offset + m * unitsPerMetre, then one surface byte each), built as procedural-mesh chunks on every machine. Game cm = map metres x 100 (origin = mapCentre x 100), so Data/forest.json positions are game positions. ownFromY: the island owns everything north of its landscape edge (y +2,016 m). Colours (sRGB): zones by the zone grid letter (blended zoneBlend m across their edges, patches of groundAlt, specks), surfaces by class (river bed, gravel bank, sand, dune, salt marsh, shore rock, road, trail, the Hollow Root floor) over them, rock on slopes over slopeRock.fromDeg with ledge moss, the frost band at the tundra neck. camps: PlayerStarts tagged with tag, a safe zone each, the starter stations at the middle one, made by the server at load.',
		id: 'forest', biome: 'forest', name: 'the Ancient forest',
		origin: [mid[0] * 100, mid[1] * 100, 0], mapCentre: mid,
		seed: SEED, grid: LS, chunk: 200, drawDistance: 900000, chunksPerFrame: 4,
		seaFloor: -14, ownFromY: ISLAND_EDGE,
		heightfield: { file: 'forest_ground.bin', x0: X0, y0: Y0, step: LS, samples: LN, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 45,
		zones: [
			{ id: 'F1', letter: '1', name: 'Meadows and Farmland', ground: C('#7FA34A'), groundAlt: C('#9AA552'), rough: 0.9, speck: { colors: [C('#E8E4F0'), C('#8A6BB0'), C('#C9A85A')], amount: 0.1, scale: 5 } },
			{ id: 'F2', letter: '2', name: 'Streams and Falls', ground: C('#4E7A3A'), groundAlt: C('#5B6E42'), rough: 0.88, speck: { colors: [C('#7A7468')], amount: 0.05, scale: 4 } },
			{ id: 'F3', letter: '3', name: 'Grovekin Villages', ground: C('#5E8C31'), groundAlt: C('#8A8478'), rough: 0.86, speck: { colors: [C('#A8C060')], amount: 0.06, scale: 5 } },
			{ id: 'F4', letter: '4', name: 'Old-Growth Groves', ground: C('#4A3A2A'), groundAlt: C('#6A5030'), rough: 0.93, speck: { colors: [C('#4E6B2A')], amount: 0.14, scale: 7 } },
			{ id: 'F5', letter: '5', name: 'Elder Canopy', ground: C('#3A5222'), groundAlt: C('#5A4A3A'), rough: 0.9, speck: { colors: [C('#2F4419')], amount: 0.1, scale: 6 } },
			{ id: 'spine', letter: 's', name: 'The Spine', ground: C('#3E3D3B'), groundAlt: C('#4E6B2A'), rough: 0.84 },
			{ id: 'hollow', letter: 'h', name: 'The Hollow Root', ground: C('#2E261E'), groundAlt: C('#3A3424'), rough: 0.92 },
			{ id: 'sea', letter: '~', name: 'The sea floor', ground: C('#5C5A4E'), groundAlt: C('#4A4C44'), rough: 0.8 },
		],
		surfaces: [
			{ id: 'bed', class: SURF.bed, color: C('#464C4E'), rough: 0.45 },
			{ id: 'bank', class: SURF.bank, color: C('#5A6870'), rough: 0.7 },
			{ id: 'sand', class: SURF.sand, color: C('#C8B88C'), rough: 0.8 },
			{ id: 'dune', class: SURF.dune, color: C('#BEB080'), rough: 0.88, speck: { colors: [C('#8E9656')], amount: 0.3, scale: 3 } },
			{ id: 'marsh', class: SURF.marsh, color: C('#5E6844'), rough: 0.6 },
			{ id: 'shore_rock', class: SURF.shoreRock, color: C('#6E6A64'), rough: 0.8 },
			{ id: 'road', class: SURF.road, color: C('#76684F'), rough: 0.9 },
			{ id: 'trail', class: SURF.trail, color: C('#5E5238'), rough: 0.92 },
			{ id: 'hollow', class: SURF.hollow, color: C('#2B241C'), rough: 0.9 },
		],
		slopeRock: { color: C('#403E3B'), fromDeg: 33, toDeg: 47, ledge: C('#4E6B2A') },
		bands: [{ id: 'frost_neck', y0: 2330, y1: 2520, color: C('#B4BEB6'), amount: 0.35, noise: 30 }],
		// Where a body is rightly under this land's ground (the Hollow Root's grid cells, the Sinkroot squeeze's crawl, the
		// Alder Grotto's squeeze and back room inside the scarp): the server's under-the-ground rescue leaves it alone
		// (WRLand::InHole). Ways: [x1, y1, x2, y2, radius] m.
		underground: { letters: 'h', ways: [
			(() => { const q = spots.find(s => s.kind === 'sinkroot_squeeze'), R = ROOMS.find(r => r.id === 'iron_seam'); return [r1(q.x - 4), q.y, R.x, R.y, 6]; })(),
			(() => { const c = CAVES.find(c => c.id === 'alder_grotto'), r = spots.find(s => s.kind === 'grotto_room'); return [r1(c.x), r1(c.y), r.x, r.y, 9]; })(),
		] },
		buildFirst: campRows.map(([x, y]) => [x, y]),
		camps: { tag: 'forest', safeRadius: 40, list: campRows, stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'forest.json'), JSON.stringify(land) + '\n');
	console.error(`land ${LN} x ${LN} at ${LS} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}

if (!PREVIEW_ONLY) {
	const buf = Buffer.alloc(N * N * 2);
	for (let k = 0; k < N * N; k++) buf.writeUInt16LE(Math.max(0, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2);
	fs.writeFileSync(path.join(root, 'Art', 'Terrain', 'forest_height.r16'), buf);
	writeLand();
	// The map's place list (FWRPoi): the forest's camps, the village, the caves and the landmarks.
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.forest);
		for (const s of sites) {
			const kind = s.kind === 'camp' ? 'camp' : s.kind === 'village' ? 'camp' : s.cave ? 'cave' : s.landmark ? 'landmark' : null;
			if (!kind) continue;
			dress.pois.push({ name: s.name, kind, x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'forest', forest: true });
		}
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}
let maxH = -1e9, land = 0; for (let k = 0; k < N * N; k++) { maxH = Math.max(maxH, HF[k]); if (HF[k] > 0.5) land++; }
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, highest: Math.round(maxH), landKm2: +(land * STEP * STEP / 1e6).toFixed(2), joinedStrip: joined, sites: sites.length, kit: kit.length, giants: giants.length, fallen: fallen.length, brakes: brakes.length, spots: spots.length, walkways: walkways.length, routes: routesOut.length }));
