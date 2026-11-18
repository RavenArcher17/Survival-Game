// The Graveyard: belt B5 west of the Spine, south-west of the Old Island (cooks/biome-graveyard.md 2.1-2.8, 2.10-2.14,
// 2.20; cooks/build-biome-graveyard.md 2.1-2.10; cooks/world-map.md 2.2-2.4). Continent metres: +x east, +y south,
// origin at the island's centre; game cm = map metres x 100 (like the forest). Writes
//   Data/lands/graveyard.json      the runtime land (WRLand): frame, heightfield, zone grid, surfaces, bands, the
//                                  Rootgrave cut into the forest's ground, catacomb ways, camps
//   Data/lands/graveyard_ground.bin uint16 heights (32768 + m * 16) row by row, then one surface byte per sample
//   Data/graveyard.json            THE contract parsed into FWRGraveData (Source/Wildreach/WRGraveTypes.cpp): sites,
//                                  kit POIs, roads and their tōrō, grave fields and rows, trees, walls, hazards, water,
//                                  crypt mouths, the catacombs' rooms and links, arenas, the Gravedigger's route,
//                                  resources, secrets, lore, run-back stones; generated, edit this script
//   Data/island_dressing.json      only its "pois" (the map's place list): camps, crypt mouths, landmarks
//   .lane-tmp/graveyard.png        a shaded layout preview (1 px = 4 m; not committed)
// node Tools/terrain/graveyard.mjs
//
// The ground joins three others. The Spine's crest is the forest's (forest.mjs CREST / CREST_H): east of it the forest's
// ground is the ground and these samples are hidden; west of it this land never drops below the forest's west face (it
// reads Data/lands/forest_ground.bin, so no forest triangle shows through). The island's own landscape wins north of its
// south shore (Art/Terrain/island_height.r16). Rootgrave: a box of the forest's ground (its rockfall plug) is cut while
// this land is live (WRLand "cuts"), and these samples make the pass floor, meeting the forest's samples exactly on the
// box's edge ring.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
// On the forest's 4 m grid (x -1450 + 4i, y +1696 + 4j), so the Rootgrave seam shares its samples.
export const X0 = -2802, Y0 = 1548, STEP = 4, N = 789, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;
const SEED = 4177;

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
const smin = (a, b, k) => { const h = Math.max(0, Math.min(1, 0.5 + 0.5 * (b - a) / k)); return lerp(b, a, h) - k * h * (1 - h); };
const smax = (a, b, k) => -smin(-a, -b, k);
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 51413;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
const r1 = v => +v.toFixed(1);
const r2 = v => +v.toFixed(2);
const DEG = Math.PI / 180;
const yawOf = (dx, dy) => Math.round(Math.atan2(dy, dx) / DEG);
const distSeg = (px, py, ax, ay, bx, by) => {
	const dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
	const t = L2 > 0 ? Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / L2)) : 0;
	return { d: Math.hypot(px - ax - dx * t, py - ay - dy * t), t };
};
function distLine(px, py, pts) {
	let best = { d: 1e9, s: 0, t: 0, along: 0 }, acc = 0;
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay] = pts[s], [bx, by] = pts[s + 1], L = Math.hypot(bx - ax, by - ay);
		const r = distSeg(px, py, ax, ay, bx, by);
		if (r.d < best.d) best = { d: r.d, s, t: r.t, along: acc + L * r.t };
		acc += L;
	}
	best.total = acc;
	return best;
}
const inPoly = (x, y, P) => { let c = false; for (let i = 0, j = P.length - 1; i < P.length; j = i++) { const [xi, yi] = P[i], [xj, yj] = P[j]; if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c; } return c; };

// ---------------------------------------------------------------- the neighbours' ground
// The forest's heightfield (its own samples: the Spine's west face, the Rootgrave plug).
const forestLand = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'lands', 'forest.json'), 'utf8'));
const FH = forestLand.heightfield;
const forestBin = fs.readFileSync(path.join(root, 'Data', 'lands', FH.file));
const FN = FH.samples;
const forestRaw = (x, y) => { const i = Math.round((x - FH.x0) / FH.step), j = Math.round((y - FH.y0) / FH.step); if (i < 0 || j < 0 || i >= FN || j >= FN) return null; return forestBin.readUInt16LE((j * FN + i) * 2); };
const forestH = (x, y) => { const v = forestRaw(x, y); return v === null ? null : (v - FH.offset) / FH.unitsPerMetre; };
const forestHidden = (x, y) => { const i = Math.round((x - FH.x0) / FH.step), j = Math.round((y - FH.y0) / FH.step); if (i < 0 || j < 0 || i >= FN || j >= FN) return true; return forestBin[FN * FN * 2 + j * FN + i] === 255; };
// The island's landscape (1 m samples centred on the origin, 32768 + m * 32).
const ISL = { size: 4033, half: 2016 };
let island = null;
try { island = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16')); } catch { island = null; }
const islandH = (x, y) => {
	if (!island) return null;
	const i = Math.round(x + ISL.half), j = Math.round(y + ISL.half);
	if (i < 0 || j < 0 || i >= ISL.size || j >= ISL.size) return null;
	return (island.readUInt16LE((j * ISL.size + i) * 2) - 32768) / 32;
};

// ---------------------------------------------------------------- the layout (metres, from the design cook)
// The Spine's crest (the forest's, world-map 2.3): x and height by y.
const CREST = [[1696, 150], [1950, 200], [2100, 40], [2300, -190], [2450, -380], [2600, -600], [2700, -940], [2850, -1070], [3050, -1140], [3300, -1150], [3500, -1130], [3800, -1050], [4200, -900], [4500, -850], [4750, -800], [5000, -650], [5400, -450], [5800, -350]];
const CREST_H = [[1696, 620], [1900, 170], [2000, 160], [2150, 300], [2350, 520], [2600, 750], [2850, 690], [3050, 470], [3200, 270], [3300, 190], [3400, 270], [3550, 460], [3800, 560], [4200, 600], [4500, 560], [4750, 430], [4950, 160], [5150, -20]];
const crestX = y => interp(CREST, y);
const crestH = y => interp(CREST_H, y);
const ROOTGRAVE = { x: -1150, y: 3300, saddle: 206 };
// The cut: the forest's ground inside this box is dropped while the Graveyard is live (its plug and the west face).
const CUT = { x0: -1290, y0: 3196, x1: -870, y1: 3404 };
const inCut = (x, y) => x > CUT.x0 && x < CUT.x1 && y > CUT.y0 && y < CUT.y1;
const onCutRing = (x, y) => !inCut(x, y) && x > CUT.x0 - STEP - 0.5 && x < CUT.x1 + STEP + 0.5 && y > CUT.y0 - STEP - 0.5 && y < CUT.y1 + STEP + 0.5;

// Camps (world-map 2.5): each in a chalk coombe's mouth over a shingle cove, by its castaway wreck.
const CAMPS = [{ x: -2450, y: 2750 }, { x: -2450, y: 3500 }, { x: -2350, y: 4300 }];
// Chalk headlands (points) between the coves: [y, out m, half width].
const HEADS = [[2560, 70, 60], [3120, 90, 70], [3880, 80, 70], [4520, 60, 55]];
function coastX(y) {
	let x = -2665 + 55 * fbm(y / 520, 1.3, 4) + 22 * fbm(y / 130, 7.1, 3) + 6 * fbm(y / 30, 3.3, 2);
	for (const [hy, out, hw] of HEADS) x -= out * Math.exp(-(((y - hy) / hw) ** 2));
	for (const c of CAMPS) {
		// The cove: the shore pulled in to 80 m in front of the camp (a round bay 2 x 110 m across between chalk points).
		const d = Math.abs(y - c.y);
		const want = c.x - 80 + 5 * fbm(y / 25, 4.4, 2);
		if (d < 110) x = lerp(x, want, 0.5 + 0.5 * Math.cos(Math.PI * d / 110));
		x -= 30 * Math.exp(-(((d - 135) / 28) ** 2));
	}
	return x;
}
// The north coast on the Bone Sound (west of the neck), and the neck itself running north to the island's shore.
const NECK = { x0: -700, x1: -250 };
function northY(x) {
	const sound = 2350 + 70 * fbm(x / 420, 2.2, 4) + 26 * fbm(x / 110, 8.8, 3) + 8 * fbm(x / 30, 4.1, 2) - 45 * Math.exp(-(((x + 1900) / 120) ** 2));
	// The neck: from x -780 eastward the land runs north to the island's south shore (it meets the island's own ground).
	const t = sstep(NECK.x0 - 80, NECK.x0 + 20, x);
	return lerp(sound, Y0 - 50, t);
}
// The neck's west shore (x by y between the island and the Bone Sound's coast).
const neckWestX = y => NECK.x0 + 60 * fbm(y / 300, 5.5, 4) + 20 * fbm(y / 90, 1.1, 3) + 6 * fbm(y / 25, 7.3, 2) - 40 * sstep(2250, 2350, y);
// The south coast: grey dead ground running to low cliffs over the sea (the Void lies past it).
const southY = x => 4655 + 28 * fbm(x / 280, 9.9, 3) + 9 * fbm(x / 70, 2.2, 2);

// The downs' trend by t = distance from the crest / the crest-to-coast width (bands squeezed: the biome is narrow).
// Highland foot H_hi tracks the crest (lower under the Rootgrave saddle); the rest is the fixed chalk-downs curve.
const hHi = y => Math.max(150, Math.min(215, crestH(y) - 32));
function downs(t, y) {
	const hi = hHi(y);
	if (t <= 0.12) return hi;
	if (t <= 0.45) return lerp(hi, 145, (t - 0.12) / 0.33);
	return interp([[0.45, 145], [0.58, 121], [0.7, 100], [0.82, 82], [0.92, 70], [1.0, 62]], t);
}

// Dry valleys (coombes): the chalk downs' own valleys, each from a head on the downs to a mouth on the coast; the three
// camps sit in their mouths. [points from head to mouth], width (m), floor at the mouth (m).
const COOMBES = [
	{ id: 'north_coombe', pts: [[-1950, 2640], [-2120, 2700], [-2300, 2740], [-2420, 2752], [-2560, 2752]], w: 75, mouth: 9 },
	{ id: 'middle_coombe', pts: [[-1850, 3580], [-2050, 3560], [-2250, 3520], [-2420, 3502], [-2560, 3500]], w: 80, mouth: 9 },
	{ id: 'south_coombe', pts: [[-1800, 4250], [-2000, 4280], [-2200, 4300], [-2330, 4302], [-2460, 4300]], w: 75, mouth: 9 },
	{ id: 'yew_vale', pts: [[-1750, 3000], [-1950, 3050], [-2150, 3080], [-2350, 3010], [-2650, 2980]], w: 60, mouth: 26 },
	{ id: 'ford_vale', pts: [[-1600, 3980], [-1850, 4000], [-2100, 4020], [-2250, 4050], [-2420, 4080], [-2700, 4100]], w: 70, mouth: 18 },
];
for (const c of COOMBES) c.len = distLine(c.pts[0][0], c.pts[0][1], c.pts).total;

// ---------------------------------------------------------------- the base ground
const SURF = { zone: 0, chalk: 1, soil: 2, flag: 3, bone: 4, lime: 5, pave: 6, shingle: 7, cliff: 8, bed: 9, asphodel: 10, grey: 11, litter: 12, hidden: 255 };
function edges(x, y) {
	const eW = x - coastX(y);
	const eN = y - northY(x);
	const eS = southY(x) - y;
	// West of the neck's shore, north of the Bone Sound's coast: the sea.
	const eNeck = y < 2420 && x > NECK.x0 - 160 ? x - neckWestX(y) : 1e9;
	return { eW, eN, eS, eNeck, e: Math.min(eW, eN, eS, eNeck) };
}
function base(x, y) {
	const cx = crestX(y), ch = crestH(y);
	const E = edges(x, y);
	const width = Math.max(400, cx - coastX(y));
	const d = cx - x, t = d / width;
	let h = downs(Math.max(0, t), y);
	// The neck: a saddle along the Spine's west skirt from the island's shore (a few metres) up to the downs at +2,400.
	if (y < 2450 && x > NECK.x0 - 200) {
		const k = sstep(2450, 2300, y) * sstep(NECK.x0 - 200, NECK.x0 - 60, x);
		const neckH = interp([[1550, 4], [1750, 9], [1950, 28], [2150, 62], [2300, 105], [2450, 150]], y) + 10 * fbm(x / 160, y / 160, 3);
		h = lerp(h, neckH, k);
	}
	// Rolling chalk downs: long smooth swells, a gentler second octave, very little fine roughness (turf on chalk).
	h += 13 * fbm(x / 520, y / 520, 3) + 4.5 * fbm(x / 170, y / 170, 3) + 0.7 * fbm(x / 35, y / 35, 2);
	// The Great Yew's hill: the only real hill in the grave fields (14 m, round-topped).
	h += 14 * Math.exp(-((Math.hypot(x + 2300, y - 3150) / 85) ** 2));
	// Coombes: a smooth U floor sinking toward the mouth, sides rising parabolically.
	for (const c of COOMBES) {
		const r = distLine(x, y, c.pts);
		if (r.d > c.w * 2.5) continue;
		const s = r.along / c.len;
		const floorZ = lerp(h - 6, c.mouth, Math.pow(s, 0.8));
		const v = floorZ + 0.0042 * r.d * r.d * (70 / c.w);
		h = smin(h, v, 6);
	}
	// The Spine: a steep limestone face from the crest down to the foot (40-55 degrees, ridged into buttresses).
	const steep = 1.15 + 0.35 * fbm(y / 200, 4.4, 3);
	const face = ch - Math.max(0, d) * steep + 22 * Math.max(0, ridged(x / 60, y / 60, 4) - 0.35) * sstep(0, 60, d);
	if (d > -30) h = smax(h, face, 18);
	// The coast: white chalk cliffs (near sheer) except in the coombes' mouths and the coves (shingle beaches).
	let surf = SURF.zone;
	const low = h < 22;
	const fall = low ? sstep(-45, 25, E.e) : sstep(-10, 9, E.e);
	const sea = -14 + 4 * fbm(x / 90, y / 90, 2);
	if (E.e < 60) {
		h = lerp(sea, h, fall);
		if (E.e > -30 && E.e < 26 && low) surf = SURF.shingle;
		else if (E.e > -12 && E.e < 12 && !low) surf = SURF.cliff;
	}
	// The south edge goes grey (the Void's border, KEPT): bands do the colour; the ground is the same chalk.
	return { h, surf, t, d };
}

// ---------------------------------------------------------------- the grid
const t0 = Date.now();
const HF = new Float32Array(N * N);
const SURFM = new Uint8Array(N * N);
const TT = new Float32Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const s = base(X0 + i * STEP, Y0 + j * STEP);
	HF[j * N + i] = s.h; SURFM[j * N + i] = s.surf; TT[j * N + i] = s.t;
}
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
// A rotated rectangle's samples: fn(k, u along, v across).
function inRect(cx, cy, yaw, hu, hv, fn) {
	const R = Math.hypot(hu, hv) + STEP, c = Math.cos(yaw * DEG), s = Math.sin(yaw * DEG);
	around(cx, cy, R, (k, d, sx, sy) => { const u = (sx - cx) * c + (sy - cy) * s, v = -(sx - cx) * s + (sy - cy) * c; if (Math.abs(u) <= hu && Math.abs(v) <= hv) fn(k, u, v, sx, sy); });
}
const slopeAt = (x, y) => Math.hypot(hAt(x + 2, y) - hAt(x - 2, y), hAt(x, y + 2) - hAt(x, y - 2)) / 4;
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

// ---------------------------------------------------------------- the chalk stream (the Ford of Washing's winterbourne)
// [x, y, water surface, width, depth]: from a spring at the Chapel Heights' foot west down the ford vale to the sea.
const STREAM = [[-1700, 3985, null, 2, 0.4], [-1850, 4000, null, 2.5, 0.5], [-2000, 4012, null, 3, 0.6], [-2120, 4022, null, 3, 0.6], [-2250, 4050, null, 4, 0.35], [-2330, 4062, null, 3.5, 0.6],
	[-2420, 4080, null, 4, 0.7], [-2560, 4092, null, 4.5, 0.8], [-2640, 4098, null, 5, 0.9]];
{
	// The water falls with the vale (never uphill): each point's surface is the ground there less 0.3 m, kept falling.
	let last = 1e9;
	for (const p of STREAM) { const g = hAt(p[0], p[1]) - 0.3; p[2] = Math.min(last - 0.05, g); last = p[2]; }
	STREAM[STREAM.length - 1][2] = Math.min(STREAM[STREAM.length - 1][2], 0.3);
	for (let s = 0; s < STREAM.length - 1; s++) {
		const a = STREAM[s], b = STREAM[s + 1], n = Math.max(1, Math.ceil(Math.hypot(b[0] - a[0], b[1] - a[1]) / 2));
		for (let q = 0; q <= n; q++) {
			const t = q / n, x = lerp(a[0], b[0], t), y = lerp(a[1], b[1], t), S = lerp(a[2], b[2], t), w = lerp(a[3], b[3], t), dep = lerp(a[4], b[4], t);
			around(x, y, w / 2 + 10, (k, d) => {
				const half = w / 2;
				const cap = d <= half ? S - dep + 0.15 * (d / half) : S + 0.25 + (d - half) * 0.35;
				if (HF[k] > cap) HF[k] = cap;
				if (d <= half + 1.2) SURFM[k] = SURF.bed;
			});
		}
	}
}

// ---------------------------------------------------------------- sites (the cook's coordinates; each anchors its ground)
// kind, id, name, x, y, yaw, flat radius (core m), falloff m, extra.
const sites = [];
const S = (kind, id, x, y, o = {}) => { const s = { kind, id, x, y, yaw: 0, ...o }; sites.push(s); return s; };
// Landmarks L1-L5.
S('chapel', 'L1', -1500, 3800, { name: 'The Chapel of Nhal', landmark: true, yaw: 0, flat: [60, 40], arena: 'nhal' });
S('great_yew', 'L2', -2300, 3150, { name: 'The Great Yew', landmark: true, yaw: 200 });
S('belfry', 'L3', -1800, 2900, { name: 'The Belfry', landmark: true, yaw: 0, flat: [28, 30], arena: 'bell_ringer' });
S('bone_giant', 'L4', -1300, 3250, { name: 'The Bone Giant of Rootgrave', landmark: true, yaw: 15 });
S('lantern_stair', 'L5', -1200, 4150, { name: 'The Lantern Stair', landmark: true, yaw: 0 });
// Hand sites H1-H13.
S('lychgate_chapel', 'H1', -2350, 3450, { name: 'The Lychgate Chapel', hand: true, yaw: 180, flat: [26, 25] });
S('row_of_named', 'H2', -2500, 3000, { name: 'Row of the Named', hand: true, yaw: 0, flat: [34, 20] });
S('dog_graves', 'H3', -2200, 2700, { name: 'The Dog Graves', hand: true, yaw: 0, flat: [14, 15] });
S('ford_of_washing', 'H4', -2250, 4050, { name: 'The Ford of Washing', hand: true, yaw: 90 });
S('smiths_row', 'H5', -1900, 2450, { name: "Smith's Row", hand: true, yaw: 0, flat: [24, 20] });
S('seven_names', 'H6', -1750, 2650, { name: 'The Seven Names', hand: true, yaw: 270, flat: [18, 20] });
S('broken_procession', 'H7', -1650, 3150, { name: 'The Broken Procession', hand: true, yaw: 0 });
S('watch_house', 'H8', -2050, 4250, { name: 'The Watch House', hand: true, yaw: 0, flat: [20, 20], lair: 'gravedigger' });
S('lime_kilns', 'H9', -1850, 4350, { name: 'The Lime Kilns', hand: true, yaw: 270, flat: [26, 20] });
S('rows_end', 'H10', -1900, 4580, { name: 'Where the Rows End', hand: true, yaw: 0, flat: [30, 20] });
S('lantern_pond', 'H11', -1560, 3700, { name: 'The Lantern Pond', hand: true, yaw: 0 });
S('root_split_tombs', 'H12', -1250, 3380, { name: 'The Root-Split Tombs', hand: true, yaw: 0, flat: [16, 14] });
S('stair_foot', 'H13', -1350, 4050, { name: 'Foot of the Stair', hand: true, yaw: 0, flat: [16, 16] });
// Camps and their wrecks.
CAMPS.forEach((c, i) => S('camp', `camp${i + 1}`, c.x, c.y, { name: ['North Cove camp', 'Lychgate Cove camp', 'Washing Cove camp'][i], camp: true, yaw: 180, flat: [28, 35], zAt: 12 }));
// Run-back stones (GY 2.11): Nhal's by the pond west of the door, the Bell-Ringer's outside the yard gate.
const runback = [{ boss: 'nhal', x: -1555, y: 3800 }, { boss: 'bell_ringer', x: -1850, y: 2900 }];
// Shrines (GY 2.11): the Great Shrine at the Lychgate Chapel; three lesser on the surface; the Watchers' Vault below.
const shrines = [
	{ id: 'gy_lantern', name: 'Great Shrine of the Lantern', x: -2335, y: 3478, great: true },
	{ id: 'gy_seven_names', name: 'The Seven Names', x: -1772, y: 2650 },
	{ id: 'gy_lime_kilns', name: 'The Lime Kilns', x: -1872, y: 4372 },
	{ id: 'gy_stair_foot', name: 'Foot of the Stair', x: -1362, y: 4062 },
	{ id: 'gy_watchers_vault', name: "The Watchers' Vault", x: -2000, y: 3300, under: 'C1' },
];

// Anchors: flatten each site's ground to its own level (core flat, smooth falloff). The chapel's terrace sits a little
// proud of the slope (a built platform: its west end on a retaining wall).
function anchor(x, y, yaw, hu, hv, fall, level) {
	const L = level ?? hAt(x, y);
	const c = Math.cos(yaw * DEG), s = Math.sin(yaw * DEG);
	around(x, y, Math.hypot(hu, hv) + fall, (k, d, sx, sy) => {
		const u = Math.abs((sx - x) * c + (sy - y) * s), v = Math.abs(-(sx - x) * s + (sy - y) * c);
		const out = Math.hypot(Math.max(0, u - hu), Math.max(0, v - hv));
		const w = 1 - sstep(0, fall, out);
		if (w > 0) HF[k] = lerp(HF[k], L, w);
	});
	return L;
}
for (const s of sites) {
	if (!s.flat) continue;
	s.level = anchor(s.x, s.y, s.yaw, s.flat[0], s.flat[1], s.camp ? 40 : 22, s.zAt);
}

// ---------------------------------------------------------------- the Rootgrave pass (the cut box) and the Root-Split mouth
// Inside the box this land is the ground: a pass floor from the forest's own road end down the saddle to the
// Root-Split Tombs, its walls rising to meet the forest's samples on the box's edge ring exactly.
{
	const floorAt = x => {
		const fe = forestH(CUT.x1, 3300) ?? 219;
		if (x >= ROOTGRAVE.x) return lerp(ROOTGRAVE.saddle, fe, sstep(ROOTGRAVE.x, CUT.x1, x));
		return lerp(hAt(CUT.x0 - 20, 3300), ROOTGRAVE.saddle, sstep(CUT.x0 - 20, ROOTGRAVE.x, x));
	};
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
		if (!(inCut(x, y) || onCutRing(x, y))) continue;
		const fh = forestH(x, y);
		if (onCutRing(x, y)) { if (fh !== null) HF[k] = fh; continue; }
		// Distance to the box's edge: near it, the forest's own ground; inside, the pass.
		const edge = Math.min(x - CUT.x0, CUT.x1 - x, y - CUT.y0, CUT.y1 - y);
		const halfFloor = 28 + 6 * fbm(x / 30, 1.7, 2);
		const across = Math.abs(y - 3300 - 8 * fbm(x / 120, 3.3, 2));
		const wall = floorAt(x) + Math.max(0, across - halfFloor) * 0.9;
		let v = Math.min(wall, fh !== null ? Math.max(fh, floorAt(x)) : wall);
		if (x < ROOTGRAVE.x) v = Math.min(v, Math.max(HF[k], floorAt(x) + Math.max(0, across - halfFloor) * 0.9));
		HF[k] = fh !== null ? lerp(fh, v, sstep(0, 22, edge)) : v;
	}
}

// ---------------------------------------------------------------- the Lantern Pond, the Bone Pits, lime pits, the stream's ford
const POND = { x: -1560, y: 3700, rx: 30, ry: 20, depth: 1.2 };
{
	const S0 = hAt(POND.x, POND.y) - 0.5;
	POND.z = r2(S0);
	around(POND.x, POND.y, 46, (k, d, sx, sy) => {
		const e = Math.hypot((sx - POND.x) / POND.rx, (sy - POND.y) / POND.ry);
		const v = e < 1 ? S0 - POND.depth * (1 - e * e) - 0.2 : S0 + 0.25 + (e - 1) * 8;
		if (HF[k] > v) HF[k] = v;
		if (e < 1.05) SURFM[k] = SURF.bed;
	});
}
// The Bone Pits (G3): open burial pits, 20-30 m across, 4-6 m deep, their floors bone scree.
const BONE_PITS = [[-1980, 3880, 13, 5], [-1760, 3920, 11, 4.5], [-2080, 4100, 14, 6], [-1700, 4160, 10, 4], [-1880, 4480, 12, 5]];
for (const [x, y, r, dep] of BONE_PITS) {
	const top = hAt(x, y);
	around(x, y, r + 10, (k, d) => {
		const v = d < r ? top - dep + 0.8 * (d / r) ** 2 : top - dep + 0.8 + (d - r) * (dep / 4);
		if (HF[k] > v) HF[k] = v;
		if (d < r + 3) SURFM[k] = SURF.bone;
	});
}
// Lime pits (G3): quicklime slurry basins round the kilns, 0.8 m deep, a white crust on their rims.
const LIME = [];
{
	const want = [[-1812, 4378, 9], [-1895, 4318, 7], [-1810, 4300, 6], [-1932, 4402, 11], [-1760, 4420, 8], [-2010, 4380, 7], [-1700, 4310, 6]];
	for (const [x, y, r] of want) {
		const z = hAt(x, y);
		around(x, y, r + 5, (k, d) => {
			const v = d < r ? z - 0.8 + 0.3 * (d / r) ** 2 : z;
			if (d < r && HF[k] > v) HF[k] = v;
			if (d < r + 4) SURFM[k] = SURF.lime;
		});
		LIME.push([x, y, r, r2(z - 0.12)]);
	}
}

// ---------------------------------------------------------------- roads (graded; chalk worn white)
// Points [x, y]; heights from the ground, relaxed into a grade; a tōrō every 30 m (the Mourners' readable night).
const ROADS = [
	{ id: 'neck_road', name: 'The Neck Road', w: 5, pts: [[-470, 1640], [-480, 1800], [-500, 1980], [-560, 2160], [-640, 2330], [-760, 2450], [-950, 2530], [-1200, 2580], [-1450, 2610], [-1700, 2640]] },
	{ id: 'smiths_road', name: "The Smiths' Road", w: 5, pts: [[-1700, 2640], [-1760, 2760], [-1790, 2850], [-1800, 2950], [-1760, 3060], [-1680, 3140], [-1640, 3260], [-1700, 3380], [-1820, 3470]] },
	{ id: 'north_road', name: 'The North Road', w: 4, pts: [[-1700, 2640], [-1800, 2560], [-1900, 2470], [-2050, 2560], [-2200, 2650], [-2330, 2720], [-2440, 2745]] },
	{ id: 'coast_road', name: 'The Coast Road', w: 5, pts: [[-2440, 2745], [-2400, 2850], [-2470, 2960], [-2460, 3080], [-2420, 3200], [-2400, 3330], [-2380, 3440], [-2420, 3520], [-2400, 3650], [-2380, 3800], [-2350, 3950], [-2320, 4070], [-2330, 4200], [-2350, 4295]] },
	{ id: 'funeral_road', name: 'The Funeral Road', w: 5, pts: [[-2380, 3440], [-2250, 3470], [-2120, 3480], [-1980, 3500], [-1820, 3470], [-1760, 3560], [-1700, 3660], [-1630, 3760], [-1580, 3800], [-1540, 3800]] },
	{ id: 'yew_lane', name: 'The Yew Lane', w: 3.5, pts: [[-2460, 3080], [-2390, 3110], [-2330, 3130]] },
	{ id: 'watchers_road', name: "The Watchers' Road", w: 4, pts: [[-1980, 3500], [-2000, 3650], [-2030, 3800], [-2060, 3950], [-2100, 4060], [-2080, 4170], [-2055, 4235]] },
	{ id: 'kiln_road', name: 'The Kiln Road', w: 4, pts: [[-2055, 4235], [-1980, 4290], [-1890, 4340], [-1880, 4440], [-1900, 4560]] },
	{ id: 'ford_road', name: 'The Ford Road', w: 4, pts: [[-2320, 4070], [-2270, 4055], [-2200, 4040], [-2120, 4060]] },
	{ id: 'rootgrave_road', name: 'The Rootgrave Road', w: 4, pts: [[-1540, 3800], [-1480, 3700], [-1420, 3580], [-1350, 3470], [-1270, 3380], [-1200, 3320], [-1150, 3300], [-1050, 3300], [-930, 3300], [-874, 3300]] },
	{ id: 'stair_road', name: 'The Stair Road', w: 4, pts: [[-1460, 3880], [-1420, 3960], [-1360, 4040], [-1290, 4100], [-1240, 4140]] },
];
for (const r of ROADS) {
	const dense = [];
	for (let s = 0; s < r.pts.length - 1; s++) {
		const [ax, ay] = r.pts[s], [bx, by] = r.pts[s + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 6));
		for (let q = 0; q < n; q++) { const t = q / n, x = lerp(ax, bx, t), y = lerp(ay, by, t); dense.push([x, y, Math.max(1.5, hAt(x, y))]); }
	}
	const last = r.pts[r.pts.length - 1];
	dense.push([last[0], last[1], Math.max(1.5, hAt(last[0], last[1]))]);
	// The road at the Rootgrave seam keeps the forest's own ground (its end point is on the cut's edge ring).
	for (let it = 0; it < 50; it++) for (let q = 1; q < dense.length - 1; q++) dense[q][2] = lerp(dense[q][2], (dense[q - 1][2] + dense[q + 1][2]) / 2, 0.5);
	r.dense = dense;
	const half = r.w / 2, blend = 7;
	for (let q = 0; q < dense.length - 1; q++) {
		const [ax, ay, ah] = dense[q], [bx, by, bh] = dense[q + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 1.5));
		for (let s = 0; s <= n; s++) {
			const t = s / n, x = lerp(ax, bx, t), y = lerp(ay, by, t), H = lerp(ah, bh, t);
			around(x, y, half + blend, (k, d, sx, sy) => {
				if (onCutRing(sx, sy) || SURFM[k] === SURF.bed) return;
				const w = 1 - sstep(half, half + blend, d);
				if (w <= 0) return;
				HF[k] = lerp(HF[k], H, w);
				if (w > 0.6) SURFM[k] = SURF.chalk;
			});
		}
	}
}
// Re-carve the stream where a road crosses it (the Ford Road fords it; the coast road's bridge is a plank span).
for (let s = 0; s < STREAM.length - 1; s++) {
	const a = STREAM[s], b = STREAM[s + 1];
	walkLine([[a[0], a[1]], [b[0], b[1]]], 2, (x, y, t) => {
		const S = lerp(a[2], b[2], t), w = lerp(a[3], b[3], t), dep = lerp(a[4], b[4], t);
		around(x, y, w / 2 + 1, (k, d) => { const cap = S - dep + 0.15 * Math.min(1, d / (w / 2)); if (HF[k] > cap) HF[k] = cap; SURFM[k] = SURF.bed; });
	});
}

// ---------------------------------------------------------------- sub-zones (16 m grid, domain-warped borders, pinned at sites)
const ZONE_PINS = [[-1500, 3800, 140], [-1800, 2900, 90], [-2050, 4250, 70], [-1850, 4350, 80], [-2300, 3150, 110], [-2350, 3450, 60], [-1300, 3250, 80]];
function zoneWarp(x, y) {
	let k = 1;
	for (const [px, py, r] of ZONE_PINS) k = Math.min(k, sstep(r, r + 160, Math.hypot(x - px, y - py)));
	const wx = 110 * fbm(x / 420 + 3.7, y / 420 + 1.9, 4) + 32 * fbm(x / 120 + 11.1, y / 120 + 5.3, 3);
	const wy = 100 * fbm(x / 420 + 8.2, y / 420 + 6.4, 4) + 28 * fbm(x / 120 + 2.6, y / 120 + 9.8, 3);
	return [x + wx * k, y + wy * k];
}
// The cook's boxes (GY 2.2): G1 the west coast strip, G2 the north midland, G3 the south midland, G4 against the Spine.
function designZone(x, y) {
	const [u, v] = zoneWarp(x, y);
	if (y < 2420 && x > NECK.x0 - 120) return 'n';
	if (Math.hypot((x - ROOTGRAVE.x) / 1.3, y - ROOTGRAVE.y) < 140 || inCut(x, y)) return 'r';
	if (u < -2150) return '1';
	if (u > -1650 + 120 * sstep(3300, 3500, v) * 0 && v > 3300 && v < 4400 && u > -1750) return '4';
	if (v < 3300) return '2';
	if (v > 3700) return '3';
	return u > -1750 ? '4' : '1';
}
const ZC = 16, ZN = Math.ceil((N - 1) * STEP / ZC);
function subzone(x, y) {
	const k = idx(x, y);
	const h = hAt(x, y);
	if (h < 0.6) return '~';
	const cx = crestX(y);
	if (x > cx - 2 && !inCut(x, y)) return 's';
	// The Spine's face: steeper than 35 degrees within its foot's reach.
	const d = cx - x;
	if (d < 360 && slopeAt(x, y) > 0.72 && !inCut(x, y)) return 's';
	return designZone(x, y);
}
// The forest's zone letter at a point (its grid in forest.json), for the cut's edges.
const FZ = forestLand.zoneGrid;
const forestZone = (x, y) => { const i = Math.floor((x - FZ.x0) / FZ.cell), j = Math.floor((y - FZ.y0) / FZ.cell); return (i < 0 || j < 0 || j >= FZ.rows.length || i >= FZ.rows[j].length) ? '~' : FZ.rows[j][i]; };
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) {
		const x = X0 + (i + 0.5) * ZC, y = Y0 + (j + 0.5) * ZC;
		// Within 28 m of the cut's edge east of the crest: the forest's own colour, so the pass walls carry no seam.
		const edge = Math.min(x - CUT.x0, CUT.x1 - x, y - CUT.y0, CUT.y1 - y);
		if (inCut(x, y) && edge < 28 && x > crestX(y) - 40) { const f = forestZone(x, y); row += f === 's' ? 'b' : f === '~' ? 'r' : 'f'; continue; }
		row += subzone(x, y);
	}
	zoneRows.push(row);
}
const zoneAt = (x, y) => { const i = Math.floor((x - X0) / ZC), j = Math.floor((y - Y0) / ZC); return (i < 0 || j < 0 || i >= ZN || j >= ZN) ? '~' : zoneRows[j][i]; };

// Zone surfaces where the zone's own floor isn't turf: G2's flagstone streets near the mausoleums, G4's limestone
// pavement, asphodel meadows in G1 (white specks), yew litter under the great yews.
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const k = j * N + i;
	if (SURFM[k] !== SURF.zone) continue;
	const x = X0 + i * STEP, y = Y0 + j * STEP, z = zoneAt(x, y);
	if (z === '4' && slopeAt(x, y) < 0.35 && fbm(x / 40, y / 40, 3) > 0.12) SURFM[k] = SURF.pave;
	else if (z === '1' && fbm(x / 70 + 5, y / 70 + 2, 3) > 0.38) SURFM[k] = SURF.asphodel;
	else if (z === '3' && fbm(x / 36 + 9, y / 36 + 1, 3) > 0.42) SURFM[k] = SURF.bone;
}

// ---------------------------------------------------------------- occupancy for placement
const placed = [];   // [x, y, r]
const occupy = (x, y, r) => placed.push([x, y, r]);
const clear = (x, y, r) => placed.every(([px, py, pr]) => Math.hypot(px - x, py - y) > pr + r);
for (const s of sites) occupy(s.x, s.y, s.flat ? Math.max(s.flat[0], s.flat[1]) + 8 : s.landmark ? 45 : 22);
for (const s of shrines) if (!s.under) occupy(s.x, s.y, 10);
const nearRoad = (x, y, R) => { for (const r of ROADS) { const q = distLine(x, y, r.dense); if (q.d < R) return true; } return false; };
const onLand = (x, y, minH = 4) => hAt(x, y) > minH && zoneAt(x, y) !== '~' && zoneAt(x, y) !== 's';

// ---------------------------------------------------------------- the catacombs (GY 2.5): rooms and links
// Floors (absolute m): L1 under its own ground (22 m down), L2 at 46 m, L3 at 12 m: all above the sea, every ceiling
// under at least 6 m of chalk (checked below). kind: what the room is (its builder and its idea).
const rooms = [];
const R = (id, kind, level, x, y, w, l, h, o = {}) => { const r = { id, kind, level, x, y, w, l, h, yaw: 0, ...o }; rooms.push(r); return r; };
// L1, the Crypt Gates.
R('C1', 'watchers_vault', 1, -2000, 3300, 16, 12, 5, { name: "The Watchers' Vault", hand: true, shrine: 'gy_watchers_vault' });
R('C2', 'niches_unnamed', 1, -2190, 3300, 200, 3.2, 3.2, { name: 'Niches of the Unnamed', hand: true, yaw: 0, corridor: true });
R('C12', 'chapel_crypt', 1, -1500, 3800, 22, 12, 5, { name: 'The Chapel Crypt', hand: true, sealed: 'nhal' });
// The crypt mouths' rooms (each mouth's stair ends in one) and the two subsidence holes' rooms.
const MOUTHS = [
	{ id: 'M1', x: -2380, y: 3070, yaw: 90, room: 'K1' }, { id: 'M2', x: -2280, y: 3640, yaw: 0, room: 'K2' }, { id: 'M3', x: -1880, y: 2780, yaw: 180, room: 'K3' },
	{ id: 'M4', x: -1620, y: 2990, yaw: 90, room: 'K4' }, { id: 'M5', x: -1450, y: 4010, yaw: 270, room: 'K5' }, { id: 'M6', x: -1990, y: 4020, yaw: 270, room: 'K6' },
];
const HOLES = [{ id: 'P1', x: -2420, y: 3330, room: 'K7' }, { id: 'P2', x: -2200, y: 3880, room: 'K8' }];
const kitL1 = ['family_vault', 'niche_corridor', 'family_vault', 'wickbeetle_nest', 'family_vault', 'niche_corridor', 'cryptstalker_lair', 'family_vault'];
MOUTHS.forEach((m, i) => {
	// The room sits at the stair's foot: 34 m on along the mouth's facing (the stair runs down that way).
	const c = Math.cos(m.yaw * DEG), s = Math.sin(m.yaw * DEG);
	R(m.room, kitL1[i], 1, m.x + c * 40, m.y + s * 40, 12, 10, 4, { mouth: m.id, yaw: m.yaw });
});
HOLES.forEach((p, i) => R(p.room, kitL1[6 + i], 1, p.x, p.y, 12, 12, 5, { hole: p.id }));
// L2, the Ossuary.
R('C4', 'skull_wheel', 2, -2090, 3390, 14, 8, 5, { name: 'The Skull Wall of the Wheel', hand: true });
R('C5', 'chandelier_hall', 2, -2000, 3520, 40, 40, 14, { name: 'The Chandelier Hall', hand: true, arena: 'ossuary_lord', round: true });
R('C6', 'bone_river', 2, -2050, 3700, 70, 12, 5, { name: 'The Bone River', hand: true, yaw: 20 });
R('C7', 'sorting_room', 2, -1870, 3420, 18, 12, 4.5, { name: 'The Sorting Room', hand: true });
R('D2', 'draught_corridor', 2, -1880, 3600, 50, 3, 3, { yaw: 90, corridor: true, draught: true });
R('D3', 'draught_corridor', 2, -2180, 3520, 40, 3, 3, { yaw: 0, corridor: true, draught: true });
// L3, the Old Quarry and Cisterns.
R('C8', 'starfall_pit', 3, -1700, 4000, 18, 18, 9, { name: 'The Starfall Pit', hand: true, round: true });
R('C9', 'brood_gallery', 3, -1900, 4120, 60, 30, 8, { name: 'The Brood Gallery', hand: true, arena: 'crypt_mother' });
R('C10', 'three_cisterns', 3, -2060, 3990, 54, 18, 7, { name: 'The Three Cisterns', hand: true, yaw: 10 });
R('C11', 'quarry_face', 3, -1810, 3930, 24, 14, 6, { name: 'The Quarry Face', hand: true });
// The spiral (the Draught Stair, C3) and the quarry ramp join the levels.
R('C3', 'draught_stair', 1, -1960, 3405, 12, 12, 0, { name: 'The Draught Stair', hand: true, spiral: true, draught: true, toLevel: 2 });
R('Q1', 'quarry_stair', 2, -1900, 3830, 12, 12, 0, { name: 'The Quarry Stair', spiral: true, toLevel: 3 });

// The chapel crypt's stair (C12 up to the chapel's grounds, sealed until Nhal's first kill): a spiral under a stone
// crypt house on the chapel's north side. Its centre sits exactly on a sample so hiding that one sample makes an
// 8 x 8 m hole (the four cells round it), which the house's 9 m walls cover.
const chapelSite = sites.find(s => s.id === 'L1');
const chapelStair = (() => {
	const i = Math.round((chapelSite.x + 29 - X0) / STEP), j = Math.round((chapelSite.y - 22 - Y0) / STEP);
	const x = X0 + i * STEP, y = Y0 + j * STEP;
	SURFM[j * N + i] = SURF.hidden;
	return { x, y, z: r2(hAt(chapelSite.x, chapelSite.y)) };
})();
R('C12S', 'crypt_stair', 1, chapelStair.x, chapelStair.y, 9, 9, 0, { name: 'The Chapel Crypt stair', spiral: true, sealed: 'nhal' });
// Links: corridors between rooms (polylines on the level; floors interpolate). [a, b, via points].
const LINKS = [
	['K1', 'C2', [[-2300, 3110], [-2290, 3200]]], ['C2', 'C1', []], ['K7', 'C2', [[-2350, 3300]]], ['C1', 'C3', [[-1985, 3360]]],
	['K2', 'C2', [[-2250, 3520], [-2240, 3400]]], ['K3', 'K4', [[-1760, 2840], [-1700, 2940]]], ['K3', 'C1', [[-1930, 2960], [-1980, 3120], [-1995, 3240]]],
	['C1', 'C12', [[-1880, 3420], [-1760, 3560], [-1650, 3700], [-1560, 3780]]], ['C12', 'K5', [[-1500, 3900], [-1460, 3960]]], ['C12', 'C12S', [[-1500, 3780]]],
	['K2', 'K8', [[-2260, 3760], [-2220, 3840]]], ['K8', 'K6', [[-2120, 3940], [-2040, 4000]]], ['K6', 'K5', [[-1800, 4050], [-1600, 4050], [-1520, 4020]]],
	// L2.
	['C3', 'C7', [[-1920, 3420]], 2], ['C7', 'C4', [[-1980, 3400]], 2], ['C4', 'D3', [[-2140, 3450]], 2], ['D3', 'C5', [[-2100, 3520]], 2],
	['C7', 'D2', [[-1880, 3500]], 2], ['D2', 'C5', [[-1950, 3560]], 2], ['C5', 'C6', [[-2030, 3620]], 2], ['D2', 'Q1', [[-1890, 3700], [-1895, 3780]], 2],
	// L3.
	['Q1', 'C11', [[-1880, 3880]], 3], ['C11', 'C8', [[-1750, 3960]], 3], ['C11', 'C10', [[-1900, 3960], [-1980, 3980]], 3], ['C11', 'C9', [[-1850, 4040]], 3], ['C8', 'C9', [[-1760, 4080]], 3],
];
// Kit rooms off the corridors (GY 2.5's mix): each a side room every ~70 m of corridor, alternating sides.
const KIT_MIX = { 1: ['niche_corridor', 'family_vault', 'wickbeetle_nest', 'flooded_stair', 'cryptstalker_lair', 'collapsed_squeeze', 'grave_seam'],
	2: ['grave_seam', 'silver_vein', 'skull_wall_puzzle', 'niche_corridor', 'cryptstalker_lair', 'grave_seam', 'family_vault', 'silver_vein'],
	3: ['silver_vein', 'grave_seam', 'cistern_bank', 'cryptstalker_lair', 'collapsed_squeeze', 'flooded_stair'] };
const roomById = id => rooms.find(r => r.id === id);
const surfaceUnder = (r, pad = 0) => {
	// The lowest ground over the room's footprint (its corners, middle and edges).
	const c = Math.cos(r.yaw * DEG), s = Math.sin(r.yaw * DEG);
	let lo = 1e9;
	for (const u of [-1, -0.5, 0, 0.5, 1]) for (const v of [-1, 0, 1]) {
		const lx = u * (r.w / 2 + pad), ly = v * (r.l / 2 + pad);
		lo = Math.min(lo, hAt(r.x + lx * c - ly * s, r.y + lx * s + ly * c));
	}
	return lo;
};
const LEVEL_Z = { 2: 46, 3: 12 };
// Every floor under at least 7 m of chalk over its ceiling (a corridor dips under a coombe), never under 3 m above the sea.
const capUnder = (x, y, h) => Math.min(hAt(x, y), hAt(x + 4, y), hAt(x - 4, y), hAt(x, y + 4), hAt(x, y - 4)) - h - 7;
const floorFor = r => {
	const lo = surfaceUnder(r, 3);
	if (r.level === 1) return Math.max(3, Math.min(lo - 22, lo - r.h - 8));
	return Math.max(3, Math.min(LEVEL_Z[r.level], lo - r.h - 7));
};
for (const r of rooms) r.z = r2(floorFor(r));
// The chapel crypt stair runs from the chapel's terrace down to the crypt's floor.
{ const cs = roomById('C12S'); if (cs) { cs.z = r2(hAt(cs.x, cs.y)); cs.bottom = roomById('C12').z; } }
// A crypt mouth's room lies at least 18 m under its mouth (the stair goes down to it).
for (const m of MOUTHS) { const r = roomById(m.room); r.z = r2(Math.max(3, Math.min(r.z, hAt(m.x, m.y) - 18))); }
// The Draught Stair runs from L1's floor at its foot (the Vault's) to L2's.
{ const c3 = roomById('C3'); c3.z = roomById('C1').z; c3.bottom = LEVEL_Z[2]; }
{ const q = roomById('Q1'); q.z = LEVEL_Z[2]; q.bottom = LEVEL_Z[3]; }
const links = [];
let kitN = 0;
for (const [a, b, via, lv] of LINKS) {
	const A = roomById(a), B = roomById(b);
	const level = lv || 1;
	// A spiral room joins at its top or its bottom: whichever is nearer the other room's floor.
	const endZ = (R, other) => R.spiral && R.bottom !== undefined && R.bottom !== null ? (Math.abs(other - R.bottom) < Math.abs(other - R.z) ? R.bottom : R.z) : R.z;
	const za = endZ(A, B.spiral ? (level > A.level ? LEVEL_Z[level] : B.z) : B.z);
	const zb = endZ(B, A.spiral ? (level > B.level ? LEVEL_Z[level] : A.z) : A.z);
	const pts = [[A.x, A.y], ...via, [B.x, B.y]];
	const L = distLine(0, 0, pts).total;
	const dense = [];
	walkLine(pts, 4, (x, y, t, s, along) => dense.push([r1(x), r1(y), lerp(za, zb, along / Math.max(1, L))]));
	// Dip under low ground: never closer than 7 m of chalk over the corridor's 3 m; eased so it ramps, not steps.
	for (let it = 0; it < 3; it++) {
		for (let q = 1; q < dense.length - 1; q++) dense[q][2] = Math.min(dense[q][2], capUnder(dense[q][0], dense[q][1], 3));
		for (let q = 1; q < dense.length - 1; q++) dense[q][2] = Math.min(dense[q][2], (dense[q - 1][2] + dense[q + 1][2]) / 2 + 0.6);
	}
	for (const p of dense) { if (p[2] < 3) console.error(`catacombs: link ${a}-${b} near (${p[0]}, ${p[1]}) can't stay above the sea`); p[2] = r2(Math.max(3, p[2])); }
	links.push({ a, b, level, w: 2.8, h: 3, pts: dense });
	// Kit rooms along it (not too near either end).
	const kinds = KIT_MIX[level];
	let side = 1;
	for (let along = 45; along < L - 40; along += 62 + 16 * rnd()) {
		let p = null, dir = null;
		walkLine(pts, 2, (x, y, t, s, al) => { if (!p && al >= along) { p = [x, y]; dir = [pts[s + 1][0] - pts[s][0], pts[s + 1][1] - pts[s][1]]; } });
		if (!p) break;
		const dl = Math.hypot(dir[0], dir[1]) || 1, nx = -dir[1] / dl * side, ny = dir[0] / dl * side;
		const off = 12;
		const kind = kinds[kitN % kinds.length];
		const kx = p[0] + nx * off, ky = p[1] + ny * off;
		const kh = kind === 'niche_corridor' ? 3 : 4;
		const atZ = (() => { let best = dense[0]; for (const d of dense) if (Math.hypot(d[0] - p[0], d[1] - p[1]) < Math.hypot(best[0] - p[0], best[1] - p[1])) best = d; return best[2]; })();
		const kz = Math.min(atZ, capUnder(kx, ky, kh + 2));
		if (kz < atZ - 0.5 || kz < 3) { along -= 30; side = -side; continue; }
		const kr = R(`K${100 + kitN}`, kind, level, p[0] + nx * off, p[1] + ny * off, kind === 'niche_corridor' ? 18 : 10, kind === 'niche_corridor' ? 3.2 : 9, kind === 'niche_corridor' ? 3 : 4,
			{ yaw: yawOf(nx, ny), kit: true, off: [r1(p[0]), r1(p[1])], draught: false });
		kr.z = r2(atZ);
		links.push({ a: b === kr.id ? a : kr.id, b: '', stub: true, level, w: 2.4, h: 2.8, pts: [[r1(p[0]), r1(p[1]), kr.z], [r1(kr.x), r1(kr.y), kr.z]] });
		kitN++;
		side = -side;
	}
}
// Every room's ceiling under 6 m of chalk at least (log any that isn't: the layout above must be moved).
for (const r of rooms) {
	const lo = surfaceUnder(r, 2);
	const top = r.z + (r.spiral ? 0 : r.h);
	if (lo - top < 6) console.error(`catacombs: ${r.id} (${r.kind}) has ${(lo - top).toFixed(1)} m of chalk over it`);
	if (r.z < 3) console.error(`catacombs: ${r.id} floor ${r.z} m is too near the sea`);
}
// The crypt mouths' stairs: from the ground at the mouth down to their room, a straight flight on the mouth's facing; the
// samples over its first part are hidden (the hole), the porch covers the hole's edge.
const crypts = MOUTHS.map(m => {
	const room = roomById(m.room);
	const top = r2(hAt(m.x, m.y));
	const drop = top - room.z;
	const run = drop / Math.tan(33 * DEG);
	return { id: m.id, x: m.x, y: m.y, z: top, yaw: m.yaw, room: m.room, drop: r2(drop), run: r2(run) };
});
for (const c of crypts) {
	// The porch's yard first (flat), then the hole: samples over the first 7 m of the flight (ceiling not yet under 3 m of
	// chalk), grown 1.5 m. Triangles touching a hidden sample drop: the porch's apron is sized for that (hole + 4 m).
	anchor(c.x, c.y, c.yaw, 9, 7, 10, c.z);
	const cs = Math.cos(c.yaw * DEG), sn = Math.sin(c.yaw * DEG);
	inRect(c.x + cs * 2.5, c.y + sn * 2.5, c.yaw, 5.5, 2.6, k => { SURFM[k] = SURF.hidden; });
	c.hole = [r1(c.x + cs * 2.5), r1(c.y + sn * 2.5), 5.5 + STEP, 2.6 + STEP];
	occupy(c.x, c.y, 24);
}
// The subsidence holes (kit): an open shaft over their room, a rope down, the ground round it slumped.
const holes = HOLES.map(p => {
	const room = roomById(p.room);
	const top = hAt(p.x, p.y);
	inRect(p.x, p.y, 0, 2.2, 2.2, k => { SURFM[k] = SURF.hidden; });
	around(p.x, p.y, 14, (k, d) => { if (d > 3) HF[k] = Math.min(HF[k], lerp(top - 1.6, HF[k], sstep(3, 14, d))); SURFM[k] = SURFM[k] === SURF.hidden ? SURF.hidden : SURF.soil; });
	occupy(p.x, p.y, 20);
	return { id: p.id, x: p.x, y: p.y, z: r2(top - 1.6), room: p.room, drop: r2(top - 1.6 - room.z) };
});
// The Starfall shaft: sealed long ago under a cairn mound on the surface over C8.
const starMound = { x: -1700, y: 4000, z: r2(hAt(-1700, 4000)) };
occupy(starMound.x, starMound.y, 18);

// ---------------------------------------------------------------- grave subsidence (G1): pits under turf lids
// 12% of grave-soil cells (GY 2.8): here 44 pits in G1's fields, each 3 m deep, flattened round so the lid sits flush.
const SUBS = [];
{
	let tries = 0;
	while (SUBS.length < 44 && tries < 20000) {
		tries++;
		const x = -2640 + rnd() * 520, y = 2560 + rnd() * 1900;
		if (zoneAt(x, y) !== '1' || !onLand(x, y, 20) || slopeAt(x, y) > 0.12) continue;
		if (nearRoad(x, y, 6) || !clear(x, y, 6)) continue;
		if (SUBS.some(([px, py]) => Math.hypot(px - x, py - y) < 40)) continue;
		const top = hAt(x, y);
		around(x, y, 9, (k, d) => {
			if (d < 3.2) HF[k] = top - 3;
			else HF[k] = lerp(top, HF[k], sstep(5, 9, d));
			if (d < 6.5) SURFM[k] = SURF.soil;
		});
		SUBS.push([r1(x), r1(y), r2(top), r2(top - 3), rnd() < 0.5 ? 1 : 0]);
	}
}

// ---------------------------------------------------------------- the island's ground wins; the forest's ground east of the crest
let joined = 0, overForest = 0;
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
	if (SURFM[k] === SURF.hidden) continue;
	if (inCut(x, y) || onCutRing(x, y)) continue;
	// The forest's ground: east of the crest it is the ground; west of it this land stays over the forest's west face.
	const fh = forestH(x, y);
	if (fh !== null && !forestHidden(x, y)) {
		if (x > crestX(y) - 1) { SURFM[k] = SURF.hidden; continue; }
		if (HF[k] < 0) { SURFM[k] = SURF.hidden; continue; }       // the sea floor there is the forest's
		if (HF[k] < fh + 0.4) { HF[k] = fh + 0.4; overForest++; }
	}
	// The island's landscape: wherever it has ground (or its sea floor lies over this), it is the ground.
	const ih = y <= ISL.half + 1 ? islandH(x, y) : null;
	if (ih !== null) {
		if (ih > 0.5) { HF[k] = Math.min(HF[k], ih - 3); SURFM[k] = SURF.hidden; joined++; continue; }
		if (HF[k] < ih - 0.25) SURFM[k] = SURF.hidden;
	}
}
// The cut's ring keeps the forest's exact samples; the forest's triangles stop there, these take over inward.
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
	if (onCutRing(x, y)) {
		const raw = forestRaw(x, y);
		if (raw !== null) HF[k] = (raw - 32768) / UNITS;
	}
	// Outside the ring but beyond the crest: still the forest's (the ring's outside neighbours stay hidden).
}
console.error(`terrain ${((Date.now() - t0) / 1000).toFixed(1)} s; over the forest's face ${overForest}, under the island ${joined}`);

// ---------------------------------------------------------------- the site levels after shaping (z for every row)
for (const s of sites) s.z = r2(s.level ?? hAt(s.x, s.y));
for (const s of shrines) s.z = s.under ? roomById(s.under).z : r2(hAt(s.x, s.y));
for (const r of runback) r.z = r2(hAt(r.x, r.y));
for (const r of ROADS) for (const p of r.dense) p[2] = r2(Math.max(p[2], hAt(p[0], p[1]) - 0.05));

// ---------------------------------------------------------------- tōrō along every road (every 30 m, alternating sides)
const toro = [];
for (const r of ROADS) {
	let side = 1, next = 10;
	walkLine(r.dense.map(p => [p[0], p[1]]), 1, (x, y, t, s, along) => {
		if (along < next) return;
		next += 30;
		const a = r.dense[s], b = r.dense[s + 1] || a;
		const dx = b[0] - a[0], dy = b[1] - a[1], L = Math.hypot(dx, dy) || 1;
		const px = x - dy / L * side * (r.w / 2 + 1.4), py = y + dx / L * side * (r.w / 2 + 1.4);
		side = -side;
		if (!onLand(px, py, 2) || inCut(px, py) && Math.abs(py - 3300) > 30) return;
		toro.push([r1(px), r1(py), r2(hAt(px, py)), yawOf(-dy / L * side, dx / L * side), r.id === 'stair_road' ? 1 : 0]);
	});
}

// ---------------------------------------------------------------- grave fields and their rows
// Fields: rectangles (centre, half sizes) the rows fill, rows north-south 2.6 m apart, stones every 2 m facing east
// (heads west, the dead face the sunrise). Some walled (dry stone). The Row of the Named is its own site.
const FIELD_DEFS = [
	{ id: 'f_north_cove', x: -2360, y: 2860, hu: 34, hv: 40, wall: true }, { id: 'f_yew_east', x: -2200, y: 3160, hu: 30, hv: 34, wall: false },
	{ id: 'f_yew_south', x: -2330, y: 3290, hu: 36, hv: 22, wall: true }, { id: 'f_lychgate', x: -2240, y: 3420, hu: 30, hv: 26, wall: true },
	{ id: 'f_coast_mid', x: -2520, y: 3700, hu: 26, hv: 50, wall: false }, { id: 'f_washing', x: -2420, y: 3920, hu: 34, hv: 34, wall: true },
	{ id: 'f_south_cove', x: -2470, y: 4190, hu: 24, hv: 30, wall: false }, { id: 'f_dog_north', x: -2140, y: 2770, hu: 24, hv: 26, wall: false },
	{ id: 'f_g2_west', x: -2020, y: 2880, hu: 30, hv: 36, wall: true }, { id: 'f_g2_funeral', x: -1900, y: 3260, hu: 28, hv: 30, wall: true },
	{ id: 'f_smiths', x: -2040, y: 2500, hu: 34, hv: 22, wall: false, smiths: true }, { id: 'f_g3_north', x: -2150, y: 3780, hu: 30, hv: 30, wall: false },
	{ id: 'f_g3_kilns', x: -1760, y: 4240, hu: 26, hv: 26, wall: false }, { id: 'f_rows_end', x: -2050, y: 4560, hu: 60, hv: 40, wall: false, ends: true },
	{ id: 'f_g3_mid', x: -2230, y: 4400, hu: 30, hv: 30, wall: false }, { id: 'f_chapel_road', x: -1660, y: 3560, hu: 22, hv: 26, wall: false, staked: true },
];
const fields = [];
let stones = 0;
for (const F of FIELD_DEFS) {
	if (!onLand(F.x, F.y, 6)) { console.error(`field ${F.id} off the land`); continue; }
	const rows = [];
	for (let u = -F.hu + 1.5; u <= F.hu - 1; u += 2.6) {
		// A row: north to south; skip where a road, a site or a pit is (the row breaks and starts again).
		let start = null;
		const flush = (end) => { if (start !== null && end - start >= 3) { const n = Math.floor((end - start) / 2) + 1; rows.push([r1(F.x + u), r1(F.y + start), r1(F.x + u), r1(F.y + start + (n - 1) * 2), n]); stones += n; } start = null; };
		// The Rows End field: its rows stop part-way (the graves planned for more dead than came, GY H10).
		const stopAt = F.ends ? F.hv * (0.1 + 0.7 * rnd()) - F.hv : F.hv;
		for (let v = -F.hv; v <= stopAt; v += 2) {
			const x = F.x + u, y = F.y + v;
			const ok = onLand(x, y, 6) && !nearRoad(x, y, 3.5) && slopeAt(x, y) < 0.3 && clear(x, y, 1.5) && !SUBS.some(([px, py]) => Math.hypot(px - x, py - y) < 7);
			if (ok && start === null) start = v;
			if (!ok) flush(v - 2);
		}
		flush(stopAt);
	}
	// The Rows End's dug graves past the last stones: open, waiting, no stones and no lanterns.
	fields.push({ id: F.id, x: F.x, y: F.y, hu: F.hu, hv: F.hv, wall: F.wall, smiths: !!F.smiths, ends: !!F.ends, staked: !!F.staked, zone: zoneAt(F.x, F.y), rows });
	occupy(F.x, F.y, Math.max(F.hu, F.hv) * 0.85);
}
console.error(`fields ${fields.length}, stones ${stones}`);

// ---------------------------------------------------------------- kit POIs (the 150 m / 125 m rule)
// Fixed kit first (crypt mouths and holes are the catacombs' doors), then the seeded pass by zone weights.
const kit = [];
crypts.forEach(c => kit.push({ kind: 'crypt_mouth', id: c.id, x: c.x, y: c.y, z: c.z, yaw: c.yaw, zone: zoneAt(c.x, c.y), room: c.room }));
holes.forEach(h => kit.push({ kind: 'subsidence_hole', id: h.id, x: h.x, y: h.y, z: h.z, yaw: 0, zone: zoneAt(h.x, h.y), room: h.room }));
kit.push({ kind: 'starfall_mound', id: 'SM', x: starMound.x, y: starMound.y, z: starMound.z, yaw: 0, zone: zoneAt(starMound.x, starMound.y) });
const KIT_WANT = { mausoleum: 6, mortsafe_cluster: 5, soulstone_outcrop: 5, kill_scene: 3, wayside_toro: 4, burned_cottage: 1, asphodel_meadow: 1, ossuary_kitchen: 1, bone_cairn: 2 };
const KIT_ZONES = { mausoleum: '2212', mortsafe_cluster: '13', soulstone_outcrop: '234', kill_scene: '13', wayside_toro: '1234', burned_cottage: '1', asphodel_meadow: '1', ossuary_kitchen: '23', bone_cairn: '134' };
const allPois = () => [...sites.map(s => [s.x, s.y]), ...kit.map(k => [k.x, k.y]), ...fields.map(f => [f.x, f.y])];
const farFromPois = (x, y, R) => allPois().every(([px, py]) => Math.hypot(px - x, py - y) >= R);
for (const [kind, want] of Object.entries(KIT_WANT)) {
	let made = 0, tries = 0;
	while (made < want && tries < 6000) {
		tries++;
		const x = X0 + 150 + rnd() * (X1 - X0 - 300), y = 2420 + rnd() * 2200;
		const z = zoneAt(x, y);
		if (!KIT_ZONES[kind].includes(z) || !onLand(x, y, 10)) continue;
		if (kind === 'wayside_toro' ? !nearRoad(x, y, 9) || nearRoad(x, y, 4) : nearRoad(x, y, 10)) continue;
		if (kind === 'soulstone_outcrop' ? slopeAt(x, y) < 0.18 || slopeAt(x, y) > 0.6 : slopeAt(x, y) > 0.25) continue;
		if (!farFromPois(x, y, kind === 'wayside_toro' ? 110 : 140) || !clear(x, y, 14)) continue;
		const yaw = kind === 'wayside_toro' ? (() => { let best = null; for (const r of ROADS) { const q = distLine(x, y, r.dense); if (!best || q.d < best.d) best = { d: q.d, p: r.dense[q.s] }; } return yawOf(best.p[0] - x, best.p[1] - y); })() : Math.round(rnd() * 360);
		kit.push({ kind, id: `k${kit.length}`, x: r1(x), y: r1(y), z: r2(hAt(x, y)), yaw, zone: z, variant: made });
		occupy(x, y, 14);
		made++;
	}
	if (made < want) console.error(`kit ${kind}: ${made} of ${want}`);
}
// The fill rule: any road point more than 125 m from every POI gets one (a wayside tōrō shrine or a bone cairn).
{
	let filled = 0;
	for (const r of ROADS) for (const p of r.dense) {
		if (!farFromPois(p[0], p[1], 125)) continue;
		const q = r.dense[Math.min(r.dense.length - 1, r.dense.indexOf(p) + 1)], dx = q[0] - p[0], dy = q[1] - p[1], L = Math.hypot(dx, dy) || 1;
		const x = p[0] - dy / L * 9, y = p[1] + dx / L * 9;
		if (!onLand(x, y, 3) || inCut(x, y)) continue;
		const kind = filled % 2 ? 'bone_cairn' : 'wayside_toro';
		kit.push({ kind, id: `k${kit.length}`, x: r1(x), y: r1(y), z: r2(hAt(x, y)), yaw: yawOf(p[0] - x, p[1] - y), zone: zoneAt(x, y), variant: 4 + filled, fill: true });
		filled++;
	}
	console.error(`kit ${kit.length} (road fill ${filled})`);
}

// ---------------------------------------------------------------- trees: yews (G1, G4: by chapels and walls), hawthorns (G1, G3),
// dead elms (G3). The Great Yew is the landmark's own. [kind, x, y, z, height m, yaw, lean]
const trees = [];
{
	const want = { yew: 70, hawthorn: 140, elm: 26 };
	for (const [kind, n] of Object.entries(want)) {
		let made = 0, tries = 0;
		while (made < n && tries < 40000) {
			tries++;
			const x = X0 + 100 + rnd() * (X1 - X0 - 200), y = 2400 + rnd() * 2250;
			const z = zoneAt(x, y);
			if (kind === 'yew' && !'14'.includes(z)) continue;
			if (kind === 'hawthorn' && !'123n'.includes(z)) continue;
			if (kind === 'elm' && z !== '3') continue;
			if (!onLand(x, y, 8) || slopeAt(x, y) > 0.4 || nearRoad(x, y, 4) || !clear(x, y, kind === 'yew' ? 5 : 3)) continue;
			if (trees.some(t => Math.hypot(t[1] - x, t[2] - y) < (kind === 'yew' ? 14 : 8))) continue;
			// Yews stand near walls, chapels and fields (churchyard trees): within 60 m of a field or a hand site.
			if (kind === 'yew' && !fields.concat(sites).some(f => Math.hypot(f.x - x, f.y - y) < 70)) continue;
			const h = kind === 'yew' ? 11 + rnd() * 7 : kind === 'hawthorn' ? 4 + rnd() * 3 : 13 + rnd() * 6;
			// Hawthorns lean away from the sea wind (the west): their crowns pushed east.
			trees.push([kind, r1(x), r1(y), r2(hAt(x, y)), r1(h), Math.round(rnd() * 360), kind === 'hawthorn' ? r1(8 + rnd() * 14) : 0]);
			made++;
		}
	}
}
// Dry-stone walls round the walled fields (a gap on the east side: the gate), iron railings round G2's family plots.
const walls = [];
for (const f of fields) {
	if (!f.wall) continue;
	const a = f.hu + 2, b = f.hv + 2;
	walls.push({ kind: 'drystone', field: f.id, pts: [[r1(f.x + a), r1(f.y - 4)], [r1(f.x + a), r1(f.y - b)], [r1(f.x - a), r1(f.y - b)], [r1(f.x - a), r1(f.y + b)], [r1(f.x + a), r1(f.y + b)], [r1(f.x + a), r1(f.y + 4)]] });
}

// ---------------------------------------------------------------- the Gravedigger's night route (GY 2.13.4)
// Watch House -> Row of the Named -> Dog Graves -> Great Yew -> the Ford -> Lime Kilns -> back; a dig at each of 6 graves.
const GD_STOPS = [[-2490, 3040], [-2210, 2730], [-2270, 3200], [-2240, 4030], [-1840, 4300], [-2440, 2960]];
const gdRoute = [];
{
	const lair = sites.find(s => s.id === 'H8');
	const pts = [[lair.x + 8, lair.y - 14], [-2100, 4060], [-2060, 3950], [-2030, 3800], [-2000, 3650], [-2120, 3480], [-2380, 3440], [-2420, 3200], [-2460, 3080], [-2490, 3040], [-2440, 2960], [-2400, 2850], [-2330, 2720], [-2210, 2730],
		[-2280, 2900], [-2290, 3080], [-2270, 3200], [-2380, 3330], [-2400, 3650], [-2350, 3950], [-2240, 4030], [-2120, 4060], [-1980, 4290], [-1840, 4300], [-1890, 4340], [-1980, 4290], [lair.x + 8, lair.y - 14]];
	walkLine(pts, 8, (x, y) => gdRoute.push([r1(x), r1(y), r2(hAt(x, y))]));
}

// ---------------------------------------------------------------- resources (the gathering, GY 2.7)
const resources = { soulstone: [], bone: [], limestone: [], mortsafe: [], rye: [], asphodel: [], wax: [], seam: [], silver: [], star: [], lime: [] };
for (const k of kit) {
	if (k.kind === 'soulstone_outcrop') for (let i = 0; i < 3; i++) { const a = rnd() * 6.28, d = 2 + rnd() * 5; resources.soulstone.push([r1(k.x + Math.cos(a) * d), r1(k.y + Math.sin(a) * d), r2(hAt(k.x + Math.cos(a) * d, k.y + Math.sin(a) * d))]); }
	if (k.kind === 'mortsafe_cluster') { const n = [3, 5, 9, 3, 5][k.variant % 5]; for (let i = 0; i < n; i++) { const x = k.x + (i % 3 - 1) * 2.4, y = k.y + (Math.floor(i / 3) - 1) * 3.2; resources.mortsafe.push([r1(x), r1(y), r2(hAt(x, y)), k.id]); } }
	if (k.kind === 'burned_cottage') for (let i = 0; i < 10; i++) { const x = k.x + 14 + (i % 5) * 3, y = k.y - 6 + Math.floor(i / 5) * 4; resources.rye.push([r1(x), r1(y), r2(hAt(x, y))]); }
	if (k.kind === 'asphodel_meadow') for (let i = 0; i < 8; i++) { const a = i * 0.78, d = 6 + (i % 3) * 4; resources.asphodel.push([r1(k.x + Math.cos(a) * d), r1(k.y + Math.sin(a) * d), r2(hAt(k.x + Math.cos(a) * d, k.y + Math.sin(a) * d))]); }
}
// Bone piles in the Bone Pits and on the G3 scree; mortsafes in the fields too (over some graves); wild rye and asphodel
// across G1's meadows; limestone at the Spine's foot in G4; wax nests in L1 kit rooms; seams, silver, star iron below.
for (const [x, y, r] of BONE_PITS) for (let i = 0; i < 3; i++) { const a = rnd() * 6.28, d = rnd() * r * 0.6; resources.bone.push([r1(x + Math.cos(a) * d), r1(y + Math.sin(a) * d), r2(hAt(x + Math.cos(a) * d, y + Math.sin(a) * d))]); }
for (const f of fields) if (f.zone === '1' && f.rows.length > 4 && rnd() < 0.6) { const row = f.rows[Math.floor(rnd() * f.rows.length)]; resources.mortsafe.push([row[0], r1((row[1] + row[3]) / 2), r2(hAt(row[0], (row[1] + row[3]) / 2)), f.id]); }
{
	let n = 0, tries = 0;
	while (n < 24 && tries < 8000) { tries++; const x = -2640 + rnd() * 600, y = 2500 + rnd() * 2000; const k = idx(x, y); if (k < 0 || SURFM[k] !== SURF.asphodel || !clear(x, y, 3) || nearRoad(x, y, 4)) continue; resources[n % 3 ? 'asphodel' : 'rye'].push([r1(x), r1(y), r2(hAt(x, y))]); n++; }
	n = 0; tries = 0;
	while (n < 14 && tries < 8000) { tries++; const y = 3350 + rnd() * 1100, x = crestX(y) - 240 - rnd() * 140; if (zoneAt(x, y) !== '4' && zoneAt(x, y) !== 's' || slopeAt(x, y) < 0.3 || slopeAt(x, y) > 1.2 || !clear(x, y, 4)) continue; resources.limestone.push([r1(x), r1(y), r2(hAt(x, y))]); n++; }
}
for (const r of rooms) {
	if (r.kind === 'wickbeetle_nest') resources.wax.push([r.x, r.y, r.z, r.id]);
	if (r.kind === 'grave_seam') resources.seam.push([r.x, r.y, r.z, r.id]);
	if (r.kind === 'silver_vein') resources.silver.push([r.x, r.y, r.z, r.id]);
	if (r.kind === 'starfall_pit') resources.star.push([r.x, r.y, r.z, r.id]);
	if (r.kind === 'quarry_face') resources.limestone.push([r.x, r.y, r.z, r.id]);
	if (r.kind === 'bone_river' || r.kind === 'sorting_room') resources.seam.push([r.x, r.y, r.z, r.id]);
}
resources.lime = LIME.map(([x, y, r, z]) => [x, y, z]).slice(0, 3);

// ---------------------------------------------------------------- secrets and lore (GY 2.6, 2.20)
const yew = sites.find(s => s.id === 'L2');
const named = sites.find(s => s.id === 'H2');
const secrets = [
	{ id: 'S1', kind: 'yew_grave', x: yew.x + 3.5, y: yew.y + 1.5, z: r2(hAt(yew.x, yew.y)), dig: 3, tool: 'pick', gives: { bp_yew_longbow: 1, lore_first_lantern: 1 } },
	{ id: 'S2', kind: 'full_grave', x: named.x - 7.5, y: named.y + 6, z: r2(hAt(named.x, named.y)), dig: 3, tool: 'pick', gives: { mourning_ring: 1, silver_ingot: 6 } },
	{ id: 'S3', kind: 'unlit_lantern', x: 0, y: 0, z: 0, hold: 3, flame: true, xp: 1000 },
	{ id: 'S4', kind: 'seventh_name', x: -1750 - 10, y: 2650, z: 0, hold: 5, needs: 'mourners_chisel', gives: { sextons_pick: 1 } },
	{ id: 'S5', kind: 'drowned_bell', room: 'C10', gives: { lore_bell_founders_mark: 1, soulstone: 8, upgrade_shard: 4 } },
];
secrets[3].z = r2(hAt(secrets[3].x, secrets[3].y));
const lore = [
	{ item: 'lore_lantern_tag', site: 'H2' }, { item: 'lore_washerwomans_bat', site: 'H4' }, { item: 'lore_watchers_tally', site: 'H8' },
	{ item: 'lore_ossuary_ledger', room: 'C7' }, { item: 'mourners_chisel', site: 'H6' },
];
// The Lantern Stair: a zig-zag cut into the Spine face from the Stair Road's end up to where it breaks off in mid-air
// (world-map: every stair breaks off; here 55 m under the crest). Flights of [x, y, z] corners.
const stair = [];
{
	const footX = -1240, footY = 4140, footZ = hAt(footX, footY);
	const crest = crestH(4150), breakZ = Math.round(crest - 55);
	let z = footZ, x = footX, y = footY, dir = 1;
	stair.push([r1(x), r1(y), r2(z)]);
	// Each flight climbs 18 m at 32 degrees along the face (north or south), angling into the Spine so its treads stay
	// cut a metre into the rock: the next corner's x is where the face stands at the corner's height less a metre.
	const faceX = (yy, zz, from) => { let xx = from; for (let n = 0; n < 400 && hAt(xx, yy) < zz - 1.0; n++) xx += 0.5; return xx; };
	while (z < breakZ - 1) {
		const rise = Math.min(18, breakZ - z), run = rise / Math.tan(32 * DEG);
		const last = z + rise >= breakZ - 1;
		y += dir * Math.sqrt(Math.max(1, run * run - 100));
		z += rise;
		// The last flight: it climbs on past where the face falls back and stops in the air (the break), 5 m out.
		x = last ? faceX(y, z, x) - 5 : faceX(y, z, x);
		stair.push([r1(x), r1(y), r2(z)]);
		if (last) break;
		// The landing: 3 m deep, turning back the other way.
		x = faceX(y, z, x + 1);
		dir = -dir;
		stair.push([r1(x), r1(y), r2(z)]);
	}
	secrets[2].x = stair[stair.length - 1][0]; secrets[2].y = stair[stair.length - 1][1]; secrets[2].z = stair[stair.length - 1][2];
	sites.find(s => s.id === 'L5').top = stair[stair.length - 1];
}

// ---------------------------------------------------------------- the content file
const C = c => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];
const contentOut = {
	_comment: 'Generated by Tools/terrain/graveyard.mjs (edit the script, not this file). The Graveyard content (cooks/biome-graveyard.md, cooks/build-biome-graveyard.md), parsed by FWRGraveData (WRGraveTypes.cpp). Positions are map metres = game cm / 100 (x east, y south), z metres. sites: landmarks (L1-L5), hand sites (H1-H13), camps; kit: crypt mouths (stair to their room), subsidence holes, kit POIs by the 150/125 m rule; roads: graded polylines [x,y,z]; toro: [x,y,z,yaw,stair]; fields: grave rows [x0,y0,x1,y1,n] north to south, stones facing east; trees [kind,x,y,z,h,yaw,lean]; hazards: subsidence [x,y,top,floor,den], lime [x,y,r,surface]; catacombs: rooms (x,y centre, z floor, w along yaw, l across, h ceiling) and links (corridors [x,y,z]); gravedigger: the night route; resources: node points per kind; secrets; lore.',
	frame: { x0: X0, y0: Y0, x1: X1, y1: Y1, step: STEP },
	cut: { land: 'forest', box: [CUT.x0, CUT.y0, CUT.x1, CUT.y1] },
	borders: { volcanicTo: 2560, voidFrom: 4450 },
	sites: sites.map(s => ({ id: s.id, kind: s.kind, name: s.name, x: s.x, y: s.y, z: s.z, yaw: s.yaw, landmark: !!s.landmark, hand: !!s.hand, camp: !!s.camp, arena: s.arena || '', lair: s.lair || '', zone: zoneAt(s.x, s.y), top: s.top || null })),
	shrines: shrines.map(s => ({ id: s.id, name: s.name, x: s.x, y: s.y, z: s.z, great: !!s.great, under: s.under || '' })),
	runback,
	kit,
	roads: ROADS.map(r => ({ id: r.id, name: r.name, w: r.w, pts: r.dense.map(p => [r1(p[0]), r1(p[1]), r2(p[2])]) })),
	toro,
	fields,
	trees,
	walls,
	stream: STREAM.map(p => [p[0], p[1], r2(p[2]), p[3]]),
	pond: { x: POND.x, y: POND.y, rx: POND.rx, ry: POND.ry, z: POND.z },
	bonePits: BONE_PITS.map(([x, y, r, d]) => [x, y, r, d, r2(hAt(x, y))]),
	hazards: { subsidence: SUBS, lime: LIME },
	crypts, holes, starMound, chapelStair,
	catacombs: { rooms: rooms.map(r => ({ id: r.id, kind: r.kind, level: r.level, name: r.name || '', x: r1(r.x), y: r1(r.y), z: r.z, w: r.w, l: r.l, h: r.h, yaw: r.yaw, hand: !!r.hand, kit: !!r.kit, round: !!r.round, corridor: !!r.corridor, draught: !!r.draught, spiral: !!r.spiral, ramp: !!r.ramp, bottom: r.bottom ?? null, arena: r.arena || '', shrine: r.shrine || '', sealed: r.sealed || '', mouth: r.mouth || '', hole: r.hole || '', off: r.off || null })), links },
	stair,
	gravedigger: { lair: (() => { const l = sites.find(s => s.id === 'H8'); return [l.x, l.y, l.z]; })(), stops: GD_STOPS.map(([x, y]) => [x, y, r2(hAt(x, y))]), route: gdRoute },
	processions: ['coast_road', 'funeral_road', 'watchers_road', 'kiln_road', 'ford_road'],
	resources,
	secrets,
	lore,
};
fs.writeFileSync(path.join(root, 'Data', 'graveyard.json'), JSON.stringify(contentOut) + '\n');

// ---------------------------------------------------------------- the runtime land (WRLand, Data/lands/graveyard.json)
function writeLand() {
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	// The cut's ring: the forest's raw values, exactly.
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { const x = X0 + i * STEP, y = Y0 + j * STEP; if (onCutRing(x, y)) { const raw = forestRaw(x, y); if (raw !== null) hb.writeUInt16LE(raw, (j * N + i) * 2); } }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'graveyard_ground.bin'), Buffer.concat([hb, sb]));
	const mid = [X0 + (N - 1) * STEP / 2, Y0 + (N - 1) * STEP / 2];
	const campRows = CAMPS.map(c => [c.x, c.y, 180]);
	// Where a body is rightly under the ground: every catacomb room and corridor, every crypt stair and hole shaft.
	const ways = [];
	for (const r of rooms) {
		const c = Math.cos(r.yaw * DEG), s = Math.sin(r.yaw * DEG), hl = Math.max(r.w, r.spiral ? 12 : 0) / 2;
		ways.push([r1(r.x - c * hl), r1(r.y - s * hl), r1(r.x + c * hl), r1(r.y + s * hl), r1(Math.max(r.l, r.spiral ? 12 : 0) / 2 + 2)]);
	}
	for (const L of links) for (let q = 0; q < L.pts.length - 1; q += 3) { const a = L.pts[q], b = L.pts[Math.min(L.pts.length - 1, q + 3)]; ways.push([a[0], a[1], b[0], b[1], 3]); }
	for (const c of crypts) { const cs = Math.cos(c.yaw * DEG), sn = Math.sin(c.yaw * DEG); ways.push([c.x, c.y, r1(c.x + cs * (c.run + 4)), r1(c.y + sn * (c.run + 4)), 3]); }
	for (const h of holes) ways.push([h.x, h.y, h.x + 0.1, h.y, 4]);
	const land = {
		_comment: 'Generated by Tools/terrain/graveyard.mjs (edit the script, not this file). The Graveyard as a runtime land (WRLand, cooks/build-biome-graveyard.md 2.1): its ground is the heightfield in graveyard_ground.bin (4 m samples from x0, y0 in map metres on the forest\'s grid: uint16 heights = offset + m * unitsPerMetre, then one surface byte each), built as procedural-mesh chunks on every machine. Game cm = map metres x 100. priority: where two lands both have ground, the higher wins (the Spine\'s west face is this land\'s). cuts: while this land is live, the forest drops its ground inside the box (Rootgrave\'s rockfall plug): these samples are the pass floor and meet the forest\'s on the box\'s edge ring exactly. Colours (sRGB): zones by the zone grid letter, surfaces by class (chalk paths worn white, fresh grave soil, flagstone, bone scree, lime crust, limestone pavement, shingle, chalk cliff, the stream bed, asphodel, grey dead ground, yew litter), rock on slopes, the Volcanic border\'s ash and the Void border\'s grey as bands. underground.ways: the catacombs\' rooms and corridors and the crypt stairs (no rescue there). camps: PlayerStarts tagged graveyard, a safe zone each, the starter stations at the middle one.',
		id: 'graveyard', biome: 'graveyard', name: 'the Graveyard', priority: 1,
		origin: [mid[0] * 100, mid[1] * 100, 0], mapCentre: mid,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 900000, chunksPerFrame: 4,
		seaFloor: -14,
		heightfield: { file: 'graveyard_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 50,
		zones: [
			{ id: 'G1', letter: '1', name: 'the Grave Fields', ground: C('#6E7A5E'), groundAlt: C('#7C8466'), rough: 0.9, speck: { colors: [C('#F2EFE6'), C('#E6E1D3')], amount: 0.07, scale: 4 } },
			{ id: 'G2', letter: '2', name: 'the Mausoleum Rows', ground: C('#66705A'), groundAlt: C('#4F5B3A'), rough: 0.88, speck: { colors: [C('#8A8580')], amount: 0.08, scale: 5 } },
			{ id: 'G3', letter: '3', name: 'the Bone Pits', ground: C('#8E9078'), groundAlt: C('#9A958C'), rough: 0.92, speck: { colors: [C('#D9CFB8'), C('#EDEBE0')], amount: 0.12, scale: 3 } },
			{ id: 'G4', letter: '4', name: 'Chapel Heights', ground: C('#5E6650'), groundAlt: C('#4A5040'), rough: 0.88, speck: { colors: [C('#B4AFA4')], amount: 0.1, scale: 6 } },
			{ id: 'neck', letter: 'n', name: 'the Neck', ground: C('#6A7158'), groundAlt: C('#5A5652'), rough: 0.9 },
			{ id: 'rootgrave', letter: 'r', name: 'Rootgrave', ground: C('#4C5A34'), groundAlt: C('#4E6B2A'), rough: 0.9, speck: { colors: [C('#F2EFE6')], amount: 0.05, scale: 4 } },
			// The forest's own colours where the pass meets its ground (the cut's edges): its Elder Canopy floor and its basalt Spine.
			{ id: 'forest_floor', letter: 'f', name: 'Rootgrave', ground: C('#3A5222'), groundAlt: C('#5A4A3A'), rough: 0.9, speck: { colors: [C('#2F4419')], amount: 0.1, scale: 6 } },
			{ id: 'forest_spine', letter: 'b', name: 'the Spine', ground: C('#3E3D3B'), groundAlt: C('#4E6B2A'), rough: 0.84 },
			{ id: 'spine', letter: 's', name: 'the Spine', ground: C('#8A8580'), groundAlt: C('#6E6A62'), rough: 0.84 },
			{ id: 'sea', letter: '~', name: 'the sea floor', ground: C('#5C5A4E'), groundAlt: C('#4A4C44'), rough: 0.8 },
		],
		surfaces: [
			{ id: 'chalk', class: SURF.chalk, color: C('#DCD6C6'), rough: 0.9 },
			{ id: 'soil', class: SURF.soil, color: C('#3B3530'), rough: 0.95 },
			{ id: 'flag', class: SURF.flag, color: C('#8A8580'), rough: 0.85 },
			{ id: 'bone', class: SURF.bone, color: C('#CFC6AE'), rough: 0.9, speck: { colors: [C('#EDE6D2'), C('#9A958C')], amount: 0.3, scale: 2 } },
			{ id: 'lime', class: SURF.lime, color: C('#EDEBE0'), rough: 0.95 },
			{ id: 'pave', class: SURF.pave, color: C('#B4AFA4'), rough: 0.86, speck: { colors: [C('#4F5B3A')], amount: 0.12, scale: 3 } },
			{ id: 'shingle', class: SURF.shingle, color: C('#9C968A'), rough: 0.8, speck: { colors: [C('#E6E1D3'), C('#5A5652')], amount: 0.25, scale: 2 } },
			{ id: 'cliff', class: SURF.cliff, color: C('#E2DED2'), rough: 0.88, speck: { colors: [C('#3A3835')], amount: 0.06, scale: 3 } },
			{ id: 'bed', class: SURF.bed, color: C('#7A776C'), rough: 0.45 },
			{ id: 'asphodel', class: SURF.asphodel, color: C('#7C8466'), rough: 0.9, speck: { colors: [C('#F2EFE6'), C('#E8E4D8')], amount: 0.22, scale: 2 } },
			{ id: 'grey', class: SURF.grey, color: C('#9A958C'), rough: 0.95 },
			{ id: 'litter', class: SURF.litter, color: C('#2A2420'), rough: 0.95 },
		],
		slopeRock: { color: C('#C9C4B6'), fromDeg: 34, toDeg: 48, ledge: C('#5E6650') },
		bands: [
			{ id: 'void_border', y0: 4430, y1: 4720, color: C('#9A958C'), amount: 0.65, noise: 35 },
			{ id: 'volcanic_border', y0: 2380, y1: 2520, color: C('#5A5652'), amount: 0.28, noise: 28 },
		],
		cuts: [{ land: 'forest', box: [CUT.x0, CUT.y0, CUT.x1, CUT.y1] }],
		underground: { letters: '', ways },
		buildFirst: campRows.map(([x, y]) => [x, y]),
		camps: { tag: 'graveyard', safeRadius: 40, list: campRows, stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'graveyard.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}
writeLand();
// The map's place list: camps, crypt mouths (caves), landmarks.
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.graveyard);
		for (const s of sites) {
			const kind = s.camp ? 'camp' : s.landmark ? 'landmark' : null;
			if (kind) dress.pois.push({ name: s.name, kind, x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'graveyard', graveyard: true });
		}
		for (const c of crypts) dress.pois.push({ name: 'A crypt mouth', kind: 'cave', x: Math.round(c.x * 100), y: Math.round(c.y * 100), z: Math.round(c.z * 100), biome: 'graveyard', graveyard: true });
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

// ---------------------------------------------------------------- preview (.lane-tmp, not committed)
{
	const PR = N, img = Buffer.alloc(PR * PR * 3);
	const zc = { '1': [110, 122, 94], '2': [102, 112, 90], '3': [142, 144, 120], '4': [94, 102, 80], 'n': [106, 113, 88], 'r': [86, 96, 63], 's': [138, 133, 128], '~': [40, 60, 80] };
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const k = j * N + i, x = X0 + i * STEP, y = Y0 + j * STEP;
		let c = (zc[zoneAt(x, y)] || [0, 0, 0]).slice();
		const s = SURFM[k];
		if (s === SURF.chalk) c = [220, 214, 198]; else if (s === SURF.soil) c = [59, 53, 48]; else if (s === SURF.bone) c = [207, 198, 174]; else if (s === SURF.lime) c = [237, 235, 224];
		else if (s === SURF.bed) c = [60, 90, 120]; else if (s === SURF.shingle) c = [156, 150, 138]; else if (s === SURF.cliff) c = [226, 222, 210]; else if (s === SURF.hidden) c = [255, 0, 255];
		const sh = Math.max(0.45, Math.min(1.3, 1 + (HF[k] - (HF[k - 1] ?? HF[k])) * 0.25 + HF[k] / 900));
		if (HF[k] < 0) c = [30, 50, 70];
		img[k * 3] = Math.min(255, c[0] * sh); img[k * 3 + 1] = Math.min(255, c[1] * sh); img[k * 3 + 2] = Math.min(255, c[2] * sh);
	}
	const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / STEP), cj = Math.round((y - Y0) / STEP); for (let a = -r; a <= r; a++) for (let b = -r; b <= r; b++) { const ii = ci + a, jj = cj + b; if (ii >= 0 && jj >= 0 && ii < N && jj < N) { const k = jj * N + ii; img[k * 3] = col[0]; img[k * 3 + 1] = col[1]; img[k * 3 + 2] = col[2]; } } };
	for (const f of fields) for (const r of f.rows) dot(r[0], (r[1] + r[3]) / 2, 0, [40, 40, 40]);
	for (const t of toro) dot(t[0], t[1], 0, [120, 255, 190]);
	for (const r of rooms) dot(r.x, r.y, 2, r.level === 1 ? [255, 220, 80] : r.level === 2 ? [255, 140, 40] : [255, 60, 60]);
	for (const k of kit) dot(k.x, k.y, 1, [255, 255, 255]);
	for (const s of sites) dot(s.x, s.y, s.landmark ? 4 : 2, s.landmark ? [185, 167, 255] : [255, 255, 0]);
	for (const s of SUBS) dot(s[0], s[1], 0, [255, 0, 0]);
	function png(w, h, rgb) {
		const T = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
		const crc = buf => { let c = 0xffffffff; for (const b of buf) c = T[(c ^ b) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
		const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); };
		const ih = Buffer.alloc(13); ih.writeUInt32BE(w, 0); ih.writeUInt32BE(h, 4); ih[8] = 8; ih[9] = 2;
		const raw = Buffer.alloc((w * 3 + 1) * h);
		for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
		return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ih), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
	}
	fs.mkdirSync(path.join(root, '.lane-tmp'), { recursive: true });
	fs.writeFileSync(path.join(root, '.lane-tmp', 'graveyard.png'), png(PR, PR, img));
}
let land = 0, maxH = -1e9; for (let k = 0; k < N * N; k++) { if (HF[k] > 0.5 && SURFM[k] !== SURF.hidden) land++; maxH = Math.max(maxH, HF[k]); }
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, landKm2: +(land * STEP * STEP / 1e6).toFixed(2), highest: Math.round(maxH), sites: sites.length, kit: kit.length, rooms: rooms.length, links: links.length, toro: toro.length, stones, trees: trees.length, subsidence: SUBS.length }));
