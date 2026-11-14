// Storm peaks: the west half of belt B3 (the Crown) north of the Old Island (cooks/biome-lightning.md,
// cooks/build-biome-lightning.md 2.1-2.2, cooks/world-map.md 2.2-2.3). Continent metres: +x east, +y south, origin at
// the island's centre. Writes
//   Art/Terrain/storm_height.r16   2017 x 2017 samples, 2 m apart, from (X0, Y0); value = 32768 + metres * 16
//                                  (import with landscape scale 200, 200, 800: Tools/terrain/import_storm.py)
//   Data/storm_peaks.json          what the game builds on it at runtime (sites with heights, routes, bridges,
//                                  hazards, kit POIs, rods, the sub-zone grid): generated, edit this script
//   Docs/terrain/storm_peaks.png   shaded layout preview (1 px = 8 m)
// node Tools/terrain/storm_peaks.mjs [--preview-only]
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const PREVIEW_ONLY = process.argv.includes('--preview-only');
export const X0 = -2900, Y0 = -5936, STEP = 2, N = 2017, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;
const SEED = 41;

// ---------------------------------------------------------------- noise
const hash = (x, y) => { let h = (x * 374761393 + y * 668265263 + SEED * 2147483647) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
	const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
	const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
	return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
// Ridged multifractal (Musgrave): sharp crests, soft hollows; each octave weighted by the last.
const ridged = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, w = 1, n = 0; for (let i = 0; i < oct; i++) { let v = 1 - Math.abs(vnoise(x * f, y * f) * 2 - 1); v *= v; v *= w; w = Math.min(1, v * 2); s += v * a; n += a; a *= 0.5; f *= 2.1; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
const smax = (a, b, k) => { const h = Math.max(0, Math.min(1, 0.5 + 0.5 * (b - a) / k)); return lerp(a, b, h) + k * h * (1 - h); };
const interp = (pts, v) => { // piecewise linear over [[v, value]...], v sorted ascending
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};

// ---------------------------------------------------------------- the layout (metres, from the design cook)
const CROWN = { x: -200, y: -3300 };
const CAMPS = [{ x: -2250, y: -2700 }, { x: -2300, y: -3700 }, { x: -2150, y: -4700 }];
// The Spine's crest north of the Crown (world-map 2.3): x by y, and its height.
const CREST = [[-5936, 330], [-5700, 300], [-4900, 200], [-4100, 0], [-3300, -200], [-2600, -80], [-2100, 0], [-1904, 20]];
const CREST_H = [[-5936, 900], [-5700, 900], [-5200, 760], [-4900, 420], [-4600, 760], [-4100, 1150], [-2500, 700], [-2300, 600], [-2050, 320], [-1960, 40], [-1904, -10]];
const crestX = y => interp(CREST, y);
const crestH = y => interp(CREST_H, y);

// West coast x by y (with headlands and the camps' coves), south shore y by x (Ash Sound), east foot of the Spine.
function coastX(y) {
	let x = -2600 + 110 * fbm(y / 600, 1.7, 4) + 40 * fbm(y / 140, 5.3, 3);
	x -= 110 * Math.exp(-(((y + 3950) / 110) ** 2));             // the Bell Rod headland
	x -= 70 * Math.exp(-(((y + 4250) / 80) ** 2));              // the Halite Cave headland
	for (const c of CAMPS) x += 120 * Math.exp(-(((y - c.y) / 95) ** 2));  // coves below the camps
	return x;
}
const NECK = { x0: -720, x1: -230 };
function southShoreY(x) {
	if (x > NECK.x0 && x < NECK.x1 + 380) return -1700;   // the neck and the Spine's south end run on to the island
	const t = x < NECK.x0 ? sstep(NECK.x0 - 160, NECK.x0, x) : 1 - sstep(NECK.x1 + 380, NECK.x1 + 520, x);
	return lerp(-2400 + 45 * fbm(x / 380, 8.1, 3), -1700, t);
}
function eastFootX(y) {
	const dy = (y - CROWN.y) / 1350;
	const crownFoot = Math.abs(dy) < 1 ? CROWN.x + 1250 * Math.sqrt(1 - dy * dy) : -1e9;
	return Math.max(crestX(y) + 520 + 60 * fbm(y / 300, 3.3, 3), crownFoot);
}

// Anchors: every site the cook gives a height to. [x, y, h, coreRadius, falloff].
const ANCHORS = [
	// Camps (S1 lowland shelves above their coves), the coast road's sites.
	[-2250, -2700, null, 55, 140], [-2300, -3700, null, 55, 140], [-2150, -4700, null, 55, 140],
	[-2500, -3950, 90, 38, 110],     // Bell Rod headland
	[-2150, -3150, 140, 32, 120],    // Storm Mill shoulder
	[-2050, -3700, null, 70, 120],    // rye terraces, Midgap
	[-2300, -4300, null, 30, 100],     // Windgap Shrine
	[-600, -2300, null, 60, 160],      // the Neck road
	// S2 Cliff Paths
	[-1880, -3350, 280, 20, 35],     // Strikerock tor (split added later)
	[-1700, -3500, 335, 26, 70],     // Goatherd's cave ledge
	[-1650, -3000, 400, 60, 80],     // Thunder Rams' Ring meadow
	[-1550, -2900, 420, 22, 60],     // Thunderstone Arch path
	[-1800, -2650, null, 40, 90],     // the Bellwether's Fold
	[-1400, -3250, 600, 14, 40],     // Struck Climbers' ledge
	[-1450, -3300, 520, 16, 40],     // Storm stair foot
	// S3
	[-1300, -3880, 590, 18, 40],     // Copperfall lip (the fall drops 90 m into the bowl)
	[-1350, -3960, 500, 30, 50],     // Copperfall bowl (55 m across)
	[-1500, -4350, 420, 18, 50], [-1120, -4350, 420, 18, 50],  // Bridge of Nine Spans towers
	[-1050, -4850, 360, 14, 40],     // Kitecut nests (on the rim)
	[-600, -4900, 400, 45, 80],      // Stormgut Fort at the west mouth
	[-420, -4950, 395, 30, 60],      // the Burnt Drake in the gut
	[-650, -4880, 405, 15, 40],      // Stormgut Mouth Shrine
	// S4 Struck Crags
	[-1180, -3700, 780, 18, 50],     // Copper Adit mouth
	[-1150, -3150, 820, 95, 90],     // the Rodfield
	[-1000, -3050, 1050, 26, 50], [-1000, -2850, 975, 26, 50], [-1000, -2650, 900, 26, 50], // the Glass Scar slab
	[-1020, -2700, 960, 14, 30],     // Glass Scar Shrine
	[-560, -3520, 1050, 26, 40],     // the Rust Pools ledge
	// S5 Hanging Monastery ledges
	[-860, -3250, 680, 22, 30],      // kitchens
	[-760, -3200, 700, 18, 26],      // the Feeding Stones
	[-800, -3330, 780, 14, 20],      // the prayer gallery
	[-720, -3380, 860, 12, 18],      // the Melted Cell
	[-800, -3420, 880, 14, 20],      // the Hanging Shrine
	[-830, -3300, 730, 12, 18],      // a cell ledge below the gallery
	[-700, -3330, 830, 10, 16],      // a cell ledge below the Melted Cell
	[-600, -3450, 900, 30, 40],      // the Warden Door on the Ring
	// The Summit Nest: a bowl 60 m across at 1,600 m (its rim is added after)
	[-200, -3300, 1600, 30, 26],
	// The Twelve Stairs' Storm stair break
	[-1300, -3330, 700, 10, 30],
];

// Gorges: centreline points [x, y, floor, width].
const GORGES = [
	{ id: 'thundergorge', pts: [[-1350, -3985, 470, 40], [-1335, -4080, 330, 90], [-1310, -4220, 230, 260], [-1310, -4350, 200, 330], [-1250, -4440, 195, 200], [-1100, -4480, 190, 120], [-950, -4540, 185, 110], [-780, -4630, 210, 100], [-640, -4760, 300, 90], [-520, -4880, 370, 80]] },
	{ id: 'kitecut', pts: [[-1880, -4790, 330, 30], [-1700, -4810, 290, 55], [-1400, -4830, 250, 65], [-1050, -4850, 220, 65], [-900, -4790, 210, 60], [-800, -4690, 205, 60]] },
	{ id: 'saltslot', pts: [[-1920, -5170, 300, 25], [-1700, -5190, 260, 45], [-1300, -5205, 235, 55], [-950, -5180, 240, 50], [-720, -5100, 300, 45], [-560, -4990, 375, 40]] },
	// Stormgut: the slot between the Crown and the north Spine (world-map: 1,600 x 120 m, saddle 420 m).
	{ id: 'stormgut', pts: [[-700, -4930, 385, 110], [-420, -4950, 395, 120], [-100, -4930, 410, 120], [200, -4900, 420, 120], [500, -4890, 380, 120], [800, -4880, 300, 130], [1132, -4870, 150, 140]] },
];
// The Desert rib spur (world-map: 200-350 m, two river gaps).
const RIB_Y = -5420, RIB_GAPS = [-2050, -1350];

// Needles: [x, y, top, rx, ry] (Galvara's Eyrie, the Bridge of Nine Spans' 8 pillars).
const NEEDLES = [[-1250, -4600, 760, 36, 24]];
const BRIDGE9 = { x0: -1500, x1: -1120, y: -4350, deck: 420 };
for (let k = 1; k <= 8; k++) NEEDLES.push([lerp(BRIDGE9.x0, BRIDGE9.x1, k / 9), BRIDGE9.y, BRIDGE9.deck - 4, 7, 7]);

// ---------------------------------------------------------------- the island under the neck
const ISL = { size: 4033, half: 2016 };
let island = null;
try { island = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16')); } catch { island = null; }
const islandH = (x, y) => {
	if (!island) return null;
	const i = Math.round(x + ISL.half), j = Math.round(y + ISL.half);
	if (i < 0 || j < 0 || i >= ISL.size || j >= ISL.size) return null;
	return (island.readUInt16LE((j * ISL.size + i) * 2) - 32768) / 32;
};

// ---------------------------------------------------------------- base surface
function base(x, y) {
	const xc = coastX(y), ys = southShoreY(x), xe = eastFootX(y);
	// The frame's own north and east edges are coast too (the Desert lane extends the land north; the Swamp, east): the
	// landscape never ends in a wall.
	const eN = y - (Y0 + 260 + 60 * fbm(x / 400, 2.2, 3)), eF = (X1 - 220) - x;
	const eW = x - xc, eS = ys - y, eE = Math.min(xe - x, eF);
	const edge = Math.min(eW, eS, eE, eN);
	const land = sstep(-25, 45, edge);
	if (land <= 0.0001) return { h: -14, land: 0, u: eW, edge };
	// Inland distance from the nearest coast (west or south), cut by the three S1 valleys.
	let u = Math.min(eW, eS * 1.15 + 120, eE * 2.5, eN * 1.6 + 60);
	const valley = (yc, w) => Math.exp(-(((y - yc) / w) ** 2)) * (1 - sstep(-1750, -1500, x));
	const cut = 330 * valley(-4700, 150) + 300 * valley(-3700, 140) + 280 * valley(-2800, 130);
	const ue = Math.max(0, u - cut);
	const cliffK = 1 - Math.max(...CAMPS.map(c => Math.exp(-(((y - c.y) / 110) ** 2))));    // coves have beaches
	let h = 32 * sstep(0, 26, ue) * cliffK + 6 * sstep(0, 120, ue)
		+ 262 * Math.pow(sstep(26, 760, ue), 1.15)
		+ 350 * sstep(700, 1450, ue)
		+ 80 * sstep(1380, 2100, ue);
	// The gorge country north of the Crown is lower and the Desert rib lands north of it are low.
	const north = sstep(-4050, -4500, y);
	h = lerp(h, Math.min(h, 120 + 520 * sstep(80, 1300, ue)), north);
	const desert = sstep(-5300, -5480, y);
	h = lerp(h, 18 + 70 * sstep(0, 900, ue) + 25 * fbm(x / 300, y / 300, 3), desert);
	// The rib spur with its two river gaps.
	let rib = 280 * Math.exp(-(((y - RIB_Y) / 110) ** 2)) * sstep(0, 160, ue);
	for (const g of RIB_GAPS) rib *= 1 - 0.85 * Math.exp(-(((x - g) / 70) ** 2));
	h = smax(h, rib + 10 * fbm(x / 90, y / 90, 3), 25);

	// The Crown massif.
	const dx = x - CROWN.x, dy = y - CROWN.y, r = Math.hypot(dx, dy);
	const theta = Math.atan2(dy, dx);                // 0 east, +pi/2 south, +-pi west, -pi/2 north
	const west = Math.max(0, -Math.cos(theta)), south = Math.max(0, Math.sin(theta));
	const ringSide = Math.min(1, west * 1.4 + south * 1.2);
	const horn = 1000 * (1 - sstep(320, 700, r)) + 650 * Math.exp(-((r / 230) ** 2));
	let shoulder = 1110 - 210 * sstep(240, 600, r);
	// Outward the Crown falls to the land: a cliff on the west face (the Hanging Monastery), gentler south-west
	// toward the Struck Crags' spur.
	const fallStart = 560, fallW = lerp(700, 240, west);
	shoulder -= (900 - lerp(650, 560, south)) * sstep(fallStart, fallStart + fallW, r);
	shoulder -= 640 * sstep(fallStart + fallW, 2100, r);
	let crown = Math.max(horn, shoulder);
	// The Ring: a terrace at 900 m round the south and west sides.
	const ring = sstep(340, 400, r) * (1 - sstep(540, 590, r)) * ringSide;
	crown = lerp(crown, 900 + 4 * fbm(x / 60, y / 60, 2), ring);
	h = smax(h, crown, 60);

	// The Spine north and south of the Crown: a ridge on the crest line.
	const cx = crestX(y), ch = crestH(y), d = Math.abs(x - cx);
	const spine = ch * Math.exp(-((d / 260) ** 2)) + 60 * Math.max(0, ridged(x / 200, y / 200, 4) - 0.3) * Math.exp(-((d / 300) ** 2));
	h = smax(h, spine * (y < -2000 ? 1 : sstep(-1904, -2000, y)), 50);
	// East of the crest the Spine falls in sheer faces to the sea (the Swamp's side, R2; the Crown keeps its own east face).
	if (x > cx && r > 1100) h = Math.min(h, lerp(ch, -14, sstep(40, 600, x - cx)) + 25 * fbm(x / 80, y / 80, 3));

	// Detail: craggy where high, smooth on the wind-flattened coast.
	const high = sstep(250, 750, h);
	// The gorge country: pinnacles and needles between the slots (DC 2.2 S3).
	const s3 = sstep(-4000, -4150, y) * (1 - sstep(-5300, -5420, y)) * sstep(-1950, -1800, x);
	h += s3 * (110 * Math.max(0, ridged(x / 180, y / 180, 4) - 0.45) + 70 * Math.max(0, vnoise(x / 55, y / 55) - 0.72) * 3);
	h += (45 * (ridged(x / 260, y / 260, 5) - 0.35) + 12 * fbm(x / 70, y / 70, 3)) * high
		+ (5 * fbm(x / 160, y / 160, 3) + 1.5 * fbm(x / 30, y / 30, 2)) * (1 - high);
	// S2: benches and risers (terraced slate cliffs) between 260 and 650 m on the land side of the Crown.
	const s2 = sstep(650, 760, ue) * (1 - sstep(1380, 1480, ue)) * (1 - north) * (1 - sstep(900, 1200, r < 900 ? 1200 : 0));
	if (s2 > 0) {
		const stepH = 70, k = h / stepH, fl = Math.floor(k), fr = k - fl;
		const terr = (fl + sstep(0.7, 0.95, fr)) * stepH;
		h = lerp(h, terr, 0.75 * s2);
	}
	return { h: h * land - 14 * (1 - land), land, u: ue, edge, r };
}

// ---------------------------------------------------------------- generate the grid
const t0 = Date.now();
const HF = new Float32Array(N * N);
const LAND = new Float32Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const s = base(X0 + i * STEP, Y0 + j * STEP);
	HF[j * N + i] = s.h; LAND[j * N + i] = s.land;
}
console.error(`base ${((Date.now() - t0) / 1000).toFixed(1)} s`);
const idx = (x, y) => { const i = Math.round((x - X0) / STEP), j = Math.round((y - Y0) / STEP); return (i < 0 || j < 0 || i >= N || j >= N) ? -1 : j * N + i; };
const hAt = (x, y) => { // bilinear
	const fx = (x - X0) / STEP, fy = (y - Y0) / STEP;
	const i = Math.max(0, Math.min(N - 2, Math.floor(fx))), j = Math.max(0, Math.min(N - 2, Math.floor(fy)));
	const tx = Math.min(1, Math.max(0, fx - i)), ty = Math.min(1, Math.max(0, fy - j));
	const a = HF[j * N + i], b = HF[j * N + i + 1], c = HF[(j + 1) * N + i], d = HF[(j + 1) * N + i + 1];
	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
};
// Visit every sample within R of (x, y).
function around(x, y, R, fn) {
	const i0 = Math.max(0, Math.floor((x - R - X0) / STEP)), i1 = Math.min(N - 1, Math.ceil((x + R - X0) / STEP));
	const j0 = Math.max(0, Math.floor((y - R - Y0) / STEP)), j1 = Math.min(N - 1, Math.ceil((y + R - Y0) / STEP));
	for (let j = j0; j <= j1; j++) for (let i = i0; i <= i1; i++) {
		const sx = X0 + i * STEP, sy = Y0 + j * STEP, d = Math.hypot(sx - x, sy - y);
		if (d <= R) fn(j * N + i, d, sx, sy);
	}
}

// Anchors (in order: later ones win where they overlap).
for (const [x, y, H0, rc, rf] of ANCHORS) {
	const H = H0 === null ? hAt(x, y) : H0;      // null: level the site at the ground's own height
	around(x, y, rc + rf, (k, d) => {
		const w = 1 - sstep(rc, rc + rf, d);
		HF[k] = lerp(HF[k], H + (d < rc ? 0.6 * fbm(x / 9 + d, y / 9, 2) : 0), w);
	});
}

// Gorges: walls rise steeply from the floor (k = 4 is ~76 deg), scree aprons at the foot.
for (const g of GORGES) {
	const pts = g.pts;
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay, af, aw] = pts[s], [bx, by, bf, bw] = pts[s + 1];
		const len = Math.hypot(bx - ax, by - ay), n = Math.ceil(len / 3);
		for (let q = 0; q <= n; q++) {
			const t = q / n, x = lerp(ax, bx, t), y = lerp(ay, by, t), fl = lerp(af, bf, t), w = lerp(aw, bw, t);
			const R = w / 2 + 160;
			around(x, y, R, (k, d, sx, sy) => {
				const out = Math.max(0, d - w / 2);
				const wall = fl + (out < 18 ? out * 0.55 : 18 * 0.55 + (out - 18) * 4.2) + 6 * fbm(sx / 25, sy / 25, 3);
				if (wall < HF[k]) HF[k] = wall;
			});
		}
	}
}
// Needles: sheer stone columns with flat tops.
for (const [x, y, top, rx, ry] of NEEDLES) {
	const R = Math.max(rx, ry) * 4;
	around(x, y, R, (k, d, sx, sy) => {
		const e = Math.hypot((sx - x) / rx, (sy - y) / ry);
		const v = e <= 1 ? top + 0.4 * fbm(sx / 8, sy / 8, 2) : top - (e - 1) * Math.max(rx, ry) * 6 + 8 * fbm(sx / 12, sy / 12, 2);
		if (v > HF[k]) HF[k] = v;
	});
}
// Strikerock: the tor split down the middle by one old strike (a 6 m notch, N-S).
around(-1880, -3350, 26, (k, d, sx) => { if (Math.abs(sx - (-1880)) < 3) HF[k] = Math.min(HF[k], 262); });
// The Summit Nest's rim: a 30 m bowl inside a low granite lip, the Spire's horn rising round the north side.
around(-200, -3300, 46, (k, d) => { if (d > 30) HF[k] = Math.max(HF[k], 1600 + 28 * sstep(30, 40, d)); });

// ---------------------------------------------------------------- routes (the Rodwardens' cut paths)
// Each route: points [x, y, height or null]; null takes the ground there. Width 8 m (stairs 4 m), graded <= 20 deg.
const ROUTES = [
	{ id: 'coast_road', kind: 'road', w: 7, rods: true, pts: [[-2250, -2700, null], [-2330, -3000, null], [-2280, -3300, null], [-2300, -3700, null], [-2330, -4000, null], [-2300, -4300, null], [-2180, -4500, null], [-2150, -4700, null]] },
	{ id: 'rod_road', kind: 'road', w: 7, rods: true, pts: [[-2300, -3700, null], [-2130, -3560, null], [-1980, -3440, 230], [-1880, -3390, 262], [-1800, -3460, 300], [-1700, -3500, 335], [-1690, -3300, 370], [-1650, -3050, 400], [-1600, -2960, 410], [-1550, -2900, 420]] },
	{ id: 'southgap', kind: 'road', w: 7, rods: true, pts: [[-2250, -2700, null], [-2050, -2720, null], [-1900, -2660, null], [-1800, -2650, null], [-1700, -2760, 370], [-1600, -2860, 410], [-1550, -2900, 420]] },
	{ id: 'neck_road', kind: 'road', w: 7, rods: true, pts: [[-470, -1960, null], [-520, -2150, null], [-600, -2300, null], [-760, -2440, null], [-950, -2560, null], [-1250, -2620, null], [-1550, -2640, null], [-1800, -2650, null]] },
	{ id: 'crag_path', kind: 'path', w: 6, gust: true, pts: [[-1550, -2900, 420], [-1480, -3000, 480], [-1420, -3150, 560], [-1400, -3250, 600], [-1330, -3220, 680], [-1240, -3180, 760], [-1150, -3150, 820], [-1080, -3000, 870], [-1040, -2800, 930], [-1020, -2700, 960]] },
	{ id: 'scar_up', kind: 'path', w: 5, pts: [[-1020, -2700, 960], [-990, -2860, 990], [-1000, -3040, 1048]] },
	{ id: 'monastery_down', kind: 'path', w: 5, gust: true, pts: [[-1150, -3150, 820], [-1050, -3200, 770], [-960, -3240, 720], [-860, -3250, 680]] },
	{ id: 'monastery_stair', kind: 'stair', w: 4, pts: [[-860, -3250, 680], [-760, -3200, 700], [-760, -3290, 745], [-800, -3330, 780], [-760, -3360, 820], [-720, -3380, 860], [-780, -3410, 875], [-800, -3420, 880], [-700, -3450, 895], [-600, -3450, 900]] },
	{ id: 'north_road', kind: 'road', w: 7, rods: true, pts: [[-2150, -4700, null], [-2250, -4500, null], [-2300, -4300, null], [-2100, -4250, null], [-1850, -4300, null], [-1650, -4340, 400], [-1500, -4350, 420]] },
	{ id: 'eyrie_road', kind: 'road', w: 6, pts: [[-1120, -4350, 420], [-1080, -4450, 425], [-1130, -4560, 430], [-1200, -4620, 430]] },
	{ id: 'gut_road', kind: 'road', w: 6, rods: true, pts: [[-1120, -4350, 420], [-980, -4400, 430], [-860, -4440, 440], [-760, -4520, 430], [-700, -4620, 420], [-680, -4760, 410], [-650, -4880, 405], [-600, -4900, 400]] },
	{ id: 'copper_road', kind: 'path', w: 6, pts: [[-1500, -4350, 420], [-1470, -4200, 450], [-1420, -4060, 480], [-1350, -3990, 500], [-1290, -3880, 590], [-1240, -3800, 690], [-1180, -3700, 780], [-1160, -3450, 800], [-1150, -3150, 820]] },
	{ id: 'storm_stair', kind: 'stair', w: 4, pts: [[-1450, -3300, 520], [-1380, -3320, 610], [-1300, -3330, 700]] },
];
// The Last Stair: from the Ring past the Rust Pools ledge, spiralling round the Spire's west and north to the nest.
{
	const pts = [[-600, -3450, 900], [-560, -3520, 1050]];
	const a0 = Math.atan2(-3520 - CROWN.y, -560 - CROWN.x);
	const turns = 0.95, steps = 22, r0 = 420, r1 = 60, h0 = 1050, h1 = 1600;
	for (let s = 1; s <= steps; s++) {
		const t = s / steps, a = a0 + t * turns * 2 * Math.PI, r = lerp(r0, r1, Math.pow(t, 0.8));
		pts.push([CROWN.x + Math.cos(a) * r, CROWN.y + Math.sin(a) * r, lerp(h0, h1, t)]);
	}
	pts.push([-200, -3300, 1600]);
	ROUTES.push({ id: 'last_stair', kind: 'stair', w: 5, gust: true, pts });
}
// Galvara's Eyrie: the cut stair spiralling up the needle's outside, 3.3 turns from the foot to the top flat.
{
	const [cx, cy, top] = NEEDLES[0];
	const pts = [[-1200, -4620, 430]];
	const a0 = Math.atan2(-4620 - cy, -1200 - cx), turns = 3.3, steps = 66;
	for (let s = 1; s <= steps; s++) {
		const t = s / steps, a = a0 + t * turns * 2 * Math.PI, r = lerp(52, 44, t);
		pts.push([cx + Math.cos(a) * r, cy + Math.sin(a) * r * 0.85, lerp(432, top, t)]);
	}
	pts.push([cx + Math.cos(a0 + turns * 2 * Math.PI) * 18, cy + Math.sin(a0 + turns * 2 * Math.PI) * 12, top]);
	ROUTES.push({ id: 'eyrie_stair', kind: 'stair', w: 4, gust: true, pts });
}
// Resolve null heights from the ground, then grade every route (<= 0.36 rise per metre) by relaxing it.
for (const r of ROUTES) {
	for (const p of r.pts) if (p[2] === null) p[2] = Math.max(4, hAt(p[0], p[1]));
	// Densify to 6 m steps with the height interpolated, then smooth with the ground's shape where unanchored.
	const dense = [];
	for (let s = 0; s < r.pts.length - 1; s++) {
		const [ax, ay, ah] = r.pts[s], [bx, by, bh] = r.pts[s + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 6));
		for (let q = 0; q < n; q++) {
			const t = q / n, x = lerp(ax, bx, t), y = lerp(ay, by, t);
			const lin = lerp(ah, bh, t), g = Math.max(2, hAt(x, y));
			// Roads follow the ground between their fixed points (a low pass), stairs keep the straight grade.
			dense.push([x, y, r.kind === 'stair' ? lin : lerp(lin, g, 0.5 * Math.sin(Math.PI * t))]);
		}
	}
	dense.push(r.pts[r.pts.length - 1].slice());
	for (let it = 0; it < 40; it++) for (let q = 1; q < dense.length - 1; q++) dense[q][2] = lerp(dense[q][2], (dense[q - 1][2] + dense[q + 1][2]) / 2, 0.5);
	r.dense = dense;
}
for (const r of ROUTES) {
	const half = r.w / 2, blend = r.kind === 'road' ? 14 : 10;
	for (let q = 0; q < r.dense.length - 1; q++) {
		const [ax, ay, ah] = r.dense[q], [bx, by, bh] = r.dense[q + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 1.5));
		for (let s = 0; s <= n; s++) {
			const t = s / n, x = lerp(ax, bx, t), y = lerp(ay, by, t), H = lerp(ah, bh, t);
			around(x, y, half + blend, (k, d) => {
				const w = 1 - sstep(half, half + blend, d);
				if (w <= 0) return;
				const target = r.kind === 'stair' ? Math.round(H / 0.4) * 0.4 : H;   // stairs: 40 cm treads
				HF[k] = lerp(HF[k], target, w);
			});
		}
	}
}

// ---------------------------------------------------------------- the strip over the island's north shore
let joined = 0;
if (island) {
	for (let j = 0; j < N; j++) {
		const y = Y0 + j * STEP;
		if (y < -ISL.half) continue;
		for (let i = 0; i < N; i++) {
			const x = X0 + i * STEP, ih = islandH(x, y);
			if (ih === null) continue;
			const k = j * N + i;
			const inNeck = x > NECK.x0 - 40 && x < NECK.x1 + 360;
			let v = inNeck ? Math.max(HF[k], 6 + 4 * fbm(x / 50, y / 50, 2)) : Math.min(HF[k], ih - 3);
			if (inNeck && ih > v - 1.5) v = ih - 1.5;      // the island's own ground wins where it is higher: no z-fight
			if (!inNeck && v > ih - 1.5) v = ih - 3;
			HF[k] = v; joined++;
		}
	}
}

// ---------------------------------------------------------------- sub-zones (32 m grid) and slope
const ZC = 32, ZN = Math.ceil((N - 1) * STEP / ZC);
const slopeAt = (x, y) => Math.hypot(hAt(x + 2, y) - hAt(x - 2, y), hAt(x, y + 2) - hAt(x, y - 2)) / 4;
function subzone(x, y) {
	const h = hAt(x, y);
	if (h < 1.5) return '~';
	const k = idx(x, y);
	if (k >= 0 && LAND[k] < 0.5 && h < 2) return '~';
	if (x > crestX(y) + 120 && y < -2200) return 'e';                         // the Crown's east face: not Storm peaks land
	if (x >= -1000 && x <= -600 && y >= -3600 && y <= -3000 && h >= 640 && h <= 920) return '5';
	if (Math.hypot(x - CROWN.x, y - CROWN.y) < 360 || h > 1180) return '6';  // the Spire (summit, the Last Stair)
	if (y < -4000 && x > -1900) return '3';
	if (h >= 600 && x > -1600) return '4';
	if (h >= 235) return '2';
	return '1';
}
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) row += subzone(X0 + (i + 0.5) * ZC, Y0 + (j + 0.5) * ZC);
	zoneRows.push(row);
}
const zoneAt = (x, y) => { const i = Math.floor((x - X0) / ZC), j = Math.floor((y - Y0) / ZC); return (i < 0 || j < 0 || i >= ZN || j >= ZN) ? '~' : zoneRows[j][i]; };

// ---------------------------------------------------------------- sites (DC 2.3 landmarks, 2.4 hand sites)
const S = (id, kind, x, y, extra = {}) => ({ id, kind, x, y, z: +hAt(x, y).toFixed(1), ...extra });
const sites = [
	S('bell_rod', 'bell_rod', -2500, -3950, { name: 'The Bell Rod', landmark: true, yaw: 0 }),
	S('strikerock', 'strikerock', -1880, -3350, { name: 'Strikerock', landmark: true }),
	S('thunderstone_arch', 'arch', -1550, -2900, { name: 'The Thunderstone Arch', landmark: true, yaw: 35 }),
	S('galvaras_eyrie', 'eyrie', -1250, -4600, { name: "Galvara's Eyrie", landmark: true }),
	S('bridge_nine', 'bridge_nine', -1310, -4350, { name: 'The Bridge of Nine Spans', landmark: true }),
	S('copperfall', 'copperfall', -1350, -3960, { name: 'The Copperfall', landmark: true, lip: [-1300, -3880, +hAt(-1300, -3880).toFixed(1)] }),
	S('glass_scar', 'glass_scar', -1000, -2850, { name: 'The Glass Scar', landmark: true, from: [-1000, -3050], to: [-1000, -2650] }),
	S('hanging_monastery', 'monastery', -800, -3300, { name: 'The Hanging Monastery', landmark: true }),
	S('storm_mill', 'storm_mill', -2150, -3150, { name: 'The Storm Mill', landmark: true }),
	S('summit_nest', 'summit_nest', -200, -3300, { name: 'The Summit Nest', landmark: true }),
	S('bell_rod_base', 'keeper_hut', -2470, -3925, { name: 'Bell Rod keeper\'s hut', loot: { copper_ore: 8, lore_keepers_tally: 1 } }),
	S('rye_terraces', 'rye_terraces', -2050, -3700, { name: 'The rye terraces', loot: { stormrye_seeds: 6 } }),
	S('goatherds_cave', 'goatherd', -1700, -3500, { name: "Goatherd's cave", loot: { gentian_seeds: 3 } }),
	S('feeding_stones', 'feeding_stones', -760, -3200, { name: 'The Feeding Stones', loot: { lore_torc_of_the_given: 1 } }),
	S('melted_cell', 'melted_cell', -720, -3380, { name: 'The Melted Cell', loot: { lore_struck_ones_torc: 1 } }),
	S('kitchens', 'kitchens', -860, -3250, { name: 'Monastery kitchens', loot: { rock_salt: 4, pine_nuts: 6 } }),
	S('kitecut_nests', 'kite_nests', -1050, -4850, { name: 'Kitecut Nests', loot: { stormkite_feather: 6 } }),
	S('stormgut_fort', 'fort', -600, -4900, { name: 'The Stormgut Fort' }),
	S('burnt_drake', 'burnt_drake', -420, -4950, { name: 'The Burnt Drake', loot: { lore_rod_through_a_rib: 1 } }),
	S('halite_cave', 'cave_mouth', -2560, -4250, { name: 'The Halite Cave', cave: 'halite' }),
	S('copper_adit', 'cave_mouth', -1180, -3700, { name: 'The Copper Adit', cave: 'adit' }),
	S('thundergorge_grotto', 'cave_mouth', -1600, -4450, { name: 'Thundergorge Grotto', cave: 'grotto' }),
	S('rodfield', 'rodfield', -1150, -3150, { name: 'The Rodfield' }),
	S('rust_pools', 'rust_pools', -560, -3520, { name: 'The Rust Pools', loot: { iron_ore: 6, kharvox_feather: 2 } }),
	S('struck_climbers', 'struck_climbers', -1400, -3250, { name: 'Struck Climbers', loot: { iron_axe: 1, rope: 2, grounding_charm: 1 } }),
	S('rams_ring', 'rams_ring', -1650, -3000, { name: "The Thunder Rams' Ring", loot: { thunderhorn_horn: 3 } }),
	S('bellwether_fold', 'bellwether_fold', -1800, -2650, { name: "The Bellwether's Fold" }),
	S('storm_stair', 'storm_stair', -1300, -3330, { name: 'The Twelve Stairs: the Storm stair' }),
	S('neck_road', 'neck_cairns', -600, -2300, { name: 'The Neck Road' }),
	S('warden_door', 'warden_door', -600, -3450, { name: 'The Warden Door' }),
	S('ring_terrace', 'ring_west', -640, -3330, { name: 'The Ring' }),
];
CAMPS.forEach((c, n) => sites.push(S(`camp_${n + 1}`, 'camp', c.x, c.y, { name: `Storm camp ${n + 1}`, wreck: [coastX(c.y) + 25, c.y + 10] })));

// ---------------------------------------------------------------- rods (waymarks every 150 m on rod roads, the Rodfield, the rim)
const rods = [];
for (const r of ROUTES.filter(r => r.rods)) {
	let acc = 75;
	for (let q = 1; q < r.dense.length; q++) {
		const [ax, ay] = r.dense[q - 1], [bx, by] = r.dense[q];
		acc += Math.hypot(bx - ax, by - ay);
		if (acc >= 150) {
			acc = 0;
			const nx = -(by - ay), ny = bx - ax, l = Math.hypot(nx, ny) || 1;
			const x = bx + nx / l * 6, y = by + ny / l * 6;
			rods.push({ x: +x.toFixed(1), y: +y.toFixed(1), z: +hAt(x, y).toFixed(1), h: 7, kind: 'waymark', route: r.id });
		}
	}
}
{ // The Rodfield: about 200 rods, 3 to 12 m tall, in rough rows ("the field of the sky").
	let n = 0;
	for (let row = -6; row <= 6; row++) for (let c = -8; c <= 8; c++) {
		const jx = hash(row + 50, c + 70) - 0.5, jy = hash(row + 90, c + 30) - 0.5;
		const x = -1150 + c * 11 + jx * 5 + row * 2, y = -3150 + row * 13 + jy * 5;
		if (Math.hypot((x + 1150) / 95, (y + 3150) / 85) > 1) continue;
		rods.push({ x: +x.toFixed(1), y: +y.toFixed(1), z: +hAt(x, y).toFixed(1), h: +(3 + 9 * hash(row, c)).toFixed(1), kind: 'field' });
		n++;
	}
	console.error(`rodfield ${n}`);
}

// ---------------------------------------------------------------- bridges, gusts, overhangs
const bridges = [
	{ id: 'nine_spans', name: 'The Bridge of Nine Spans', from: [BRIDGE9.x0, BRIDGE9.y, BRIDGE9.deck], to: [BRIDGE9.x1, BRIDGE9.y, BRIDGE9.deck], spans: 9, broken: [5], winches: true },
];
// Short rope bridges across the slots (20-60 m), where the north road and gut road meet them.
const SHORT = [[-1700, -4810, 0], [-1400, -4830, 0], [-900, -4790, 25], [-1300, -5205, 0], [-950, -5180, 10]];
for (const [x, y, rot] of SHORT) {
	// Span the gorge across its local direction: find the two rims by walking out until the ground is up near the rim.
	const g = GORGES.find(G => G.pts.some(p => Math.hypot(p[0] - x, p[1] - y) < 260));
	const w = g ? Math.max(30, g.pts.reduce((a, p) => Math.hypot(p[0] - x, p[1] - y) < Math.hypot(a[0] - x, a[1] - y) ? p : a)[3] + 20) : 40;
	const a = [x, y - w / 2 - 6], b = [x, y + w / 2 + 6];
	const za = hAt(a[0], a[1]), zb = hAt(b[0], b[1]), deck = Math.min(za, zb);
	bridges.push({ id: `rope_${bridges.length}`, name: 'Rope bridge', from: [a[0], a[1], +deck.toFixed(1)], to: [b[0], b[1], +deck.toFixed(1)], spans: 1, broken: [], winches: false, yaw: rot });
}
const gusts = ROUTES.filter(r => r.gust).map(r => ({ route: r.id, pts: r.dense.filter((_, q) => q % 4 === 0).map(p => [+p[0].toFixed(1), +p[1].toFixed(1), +p[2].toFixed(1)]) }));
// Overhangs (rockfall): where a path runs at the foot of a riser 25+ m tall within 12 m uphill.
const overhangs = [];
for (const r of ROUTES.filter(r => r.kind !== 'stair')) {
	for (let q = 0; q < r.dense.length; q += 5) {
		const [x, y, z] = r.dense[q];
		let best = null;
		for (let a = 0; a < 16; a++) {
			const ang = a / 16 * 2 * Math.PI, sx = x + Math.cos(ang) * 14, sy = y + Math.sin(ang) * 14, sh = hAt(sx, sy);
			if (sh - z > 25 && (!best || sh - z > best.d)) best = { d: sh - z, ang };
		}
		if (best && !overhangs.some(o => Math.hypot(o.x - x, o.y - y) < 120)) overhangs.push({ x: +x.toFixed(1), y: +y.toFixed(1), z: +z.toFixed(1), face: +(best.ang * 180 / Math.PI).toFixed(0), wall: +best.d.toFixed(0) });
	}
}

// ---------------------------------------------------------------- kit POIs (DC 2.4 kit table), spacing >= 150 m
const KIT = [
	{ kind: 'bidental', zones: '124', n: 8, v: 3 }, { kind: 'fulgurite_field', zones: '14', n: 7, v: 3, sand: true },
	{ kind: 'outcrop', zones: '234', n: 9, v: 4 }, { kind: 'basking_rock', zones: '124', n: 5, v: 2 },
	{ kind: 'puffram_fold', zones: '1', n: 6, v: 3 }, { kind: 'tarn', zones: '12', n: 6, v: 3, flat: true },
	{ kind: 'waystation', zones: '123', n: 6, v: 3 }, { kind: 'kite_roost', zones: '23', n: 4, v: 2 },
	{ kind: 'mound_field', zones: '12', n: 4, v: 2, flat: true }, { kind: 'kill_scene', zones: '12', n: 4, v: 4 },
	{ kind: 'struck_tree', zones: '12', n: 4, v: 2 },
];
const kit = [];
const taken = [...sites.map(s => [s.x, s.y]), ...bridges.map(b => [(b.from[0] + b.to[0]) / 2, (b.from[1] + b.to[1]) / 2])];
const far = (x, y, gap) => taken.every(([tx, ty]) => Math.hypot(tx - x, ty - y) >= gap);
let rngState = 12345;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
function place(K, gap, near) {
	for (let tries = 0; tries < 4000; tries++) {
		const x = near ? near[0] + (rnd() - 0.5) * 240 : lerp(X0 + 50, -300, rnd());
		const y = near ? near[1] + (rnd() - 0.5) * 240 : lerp(-5450, -2250, rnd());
		const z = zoneAt(x, y);
		if (!K.zones.includes(z)) continue;
		if (K.sand && z === '1' && y > -5250) continue;            // fulgurite fields in S1 only on the Desert rib sand
		const sl = slopeAt(x, y);
		if (sl > (K.flat ? 0.15 : 0.45)) continue;
		if (!far(x, y, gap)) continue;
		// The triangle rule: on a rise or round a bend (higher than the ground 40 m around, or within 25 m of a route).
		let rise = 0; for (let a = 0; a < 8; a++) rise += hAt(x, y) - hAt(x + Math.cos(a * 0.785) * 40, y + Math.sin(a * 0.785) * 40);
		const nearRoute = ROUTES.some(r => r.dense.some(p => Math.hypot(p[0] - x, p[1] - y) < 25));
		if (rise < 0 && !nearRoute && !K.flat) continue;
		const v = Math.floor(rnd() * K.v);
		const item = { kind: K.kind, variant: v, x: +x.toFixed(1), y: +y.toFixed(1), z: +hAt(x, y).toFixed(1), yaw: Math.round(rnd() * 360), zone: z };
		kit.push(item); taken.push([x, y]);
		return item;
	}
	return null;
}
for (const K of KIT) for (let n = 0; n < K.n; n++) place(K, 150);
// Kill set-pieces and roosts fill the rest; then the 125 m rule along every route (fire-frost 4.5 SPEC): any route
// point further than 125 m from a POI gets the kit piece that fits its zone.
const FILL = { '1': ['bidental', 'puffram_fold', 'struck_tree', 'waystation'], '2': ['waystation', 'outcrop', 'bidental', 'kite_roost'], '3': ['outcrop', 'kite_roost', 'waystation'], '4': ['bidental', 'outcrop', 'basking_rock'], '5': [], '6': [] };
let filled = 0;
for (const r of ROUTES) for (const [x, y] of r.dense) {
	if (!far(x, y, 125)) continue;
	const z = zoneAt(x, y), list = FILL[z] || [];
	if (!list.length) { taken.push([x, y]); continue; }
	const K = KIT.find(k => k.kind === list[filled % list.length]);
	if (place({ ...K, zones: z }, 110, [x, y])) filled++; else taken.push([x, y]);
}
console.error(`kit ${kit.length} (route fill ${filled}), overhangs ${overhangs.length}, rods ${rods.length}`);

// ---------------------------------------------------------------- shrines, arenas, routes for creatures
const shrines = [
	{ id: 'strikerock', x: -1872, y: -3350 }, { id: 'windgap', x: -2300, y: -4300 }, { id: 'stormgut_mouth', x: -650, y: -4880 },
	{ id: 'glass_scar', x: -1020, y: -2700 }, { id: 'hanging', x: -800, y: -3420 },
].map(s => ({ ...s, z: +hAt(s.x, s.y).toFixed(1) }));
const arenas = {
	kharvox: { x: -200, y: -3300, z: +hAt(-200, -3300).toFixed(1), radius: 3000, rods: 12, rodRadius: 2600, runback: lastStairPoint(50), altar: [-200 + 22, -3300 + 6], ossuary: [-200 + 18, -3300 - 4], nest: [-200, -3300 - 18] },
	galvara: { x: -1250, y: -4600, z: +hAt(-1250, -4600).toFixed(1), runback: [-1150, -4560, +hAt(-1150, -4560).toFixed(1)] },
	coilback: { x: -1350, y: -3960, z: +hAt(-1350, -3960).toFixed(1), runback: [-1420, -4060, +hAt(-1420, -4060).toFixed(1)] },
};
function lastStairPoint(back) {
	const r = ROUTES.find(r => r.id === 'last_stair').dense;
	let acc = 0;
	for (let q = r.length - 1; q > 0; q--) { acc += Math.hypot(r[q][0] - r[q - 1][0], r[q][1] - r[q - 1][1]); if (acc >= back) return [+r[q][0].toFixed(1), +r[q][1].toFixed(1), +r[q][2].toFixed(1)]; }
	return r[0];
}
// The Bellwether's loop (DC 2.10): Fold -> Rams' Ring -> under the Arch -> Struck Climbers -> Storm Mill shoulder -> Fold.
const bellwether = [[-1800, -2650], [-1700, -2800], [-1650, -3000], [-1560, -2930], [-1480, -3000], [-1420, -3150], [-1700, -3300], [-1950, -3200], [-2150, -3150], [-2050, -2900], [-1900, -2700]].map(([x, y]) => [x, y, +hAt(x, y).toFixed(1)]);

// ---------------------------------------------------------------- write
const r1 = v => +v.toFixed(1);
const out = {
	_comment: 'Generated by Tools/terrain/storm_peaks.mjs (edit the script, not this file). Storm peaks, the west half of belt B3 (cooks/biome-lightning.md): continent metres (+x east, +y south, origin the island centre; world cm = metres x 100). frame: the landscape (2 m samples). zones: sub-zone letters on a cell grid (1 Wind Passes, 2 Cliff Paths, 3 Rope Bridges, 4 Struck Crags, 5 Cliff Shrines, 6 the Spire, e the Crown east face, ~ sea). Read by UWRStormWorld (WRStormWorld.cpp).',
	frame: { x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, zScale: 100 * 128 / UNITS },
	zones: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
	sites, shrines, arenas, bellwether,
	routes: ROUTES.map(r => ({ id: r.id, kind: r.kind, w: r.w, pts: r.dense.filter((_, q) => q % 3 === 0 || q === r.dense.length - 1).map(p => [r1(p[0]), r1(p[1]), r1(p[2])]) })),
	rods, bridges, gusts, overhangs, kit,
};
fs.writeFileSync(path.join(root, 'Data', 'storm_peaks.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- preview (8 m per pixel)
const PR = Math.floor((N - 1) * STEP / 8);
const img = Buffer.alloc(PR * PR * 3);
const ZCOL = { '1': [124, 138, 106], '2': [92, 98, 108], '3': [138, 140, 146], '4': [170, 172, 178], '5': [217, 213, 204], '6': [70, 72, 80], e: [80, 80, 90] };
for (let j = 0; j < PR; j++) for (let i = 0; i < PR; i++) {
	const x = X0 + i * 8 + 4, y = Y0 + j * 8 + 4, h = hAt(x, y);
	let c = h < 0.5 ? [30, 70, 115] : (ZCOL[zoneAt(x, y)] || [100, 100, 100]);
	if (h >= 0.5 && slopeAt(x, y) > 0.75) c = c.map(v => v * 0.6);
	const dzdx = (hAt(x + 8, y) - hAt(x - 8, y)) / 16, dzdy = (hAt(x, y + 8) - hAt(x, y - 8)) / 16;
	const shade = Math.max(0.3, Math.min(1.3, 0.95 + (-dzdx * 0.6 - dzdy * 0.6) * 0.8));
	const k = (j * PR + i) * 3;
	img[k] = Math.min(255, c[0] * shade); img[k + 1] = Math.min(255, c[1] * shade); img[k + 2] = Math.min(255, c[2] * shade);
}
const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / 8), cj = Math.round((y - Y0) / 8); for (let j = cj - r; j <= cj + r; j++) for (let i = ci - r; i <= ci + r; i++) if (i >= 0 && j >= 0 && i < PR && j < PR && (i - ci) ** 2 + (j - cj) ** 2 <= r * r) img.set(col, (j * PR + i) * 3); };
for (const r of ROUTES) for (const p of r.dense) dot(p[0], p[1], 0, r.kind === 'stair' ? [255, 230, 120] : [200, 160, 90]);
for (const k of kit) dot(k.x, k.y, 2, [40, 40, 40]);
for (const s of sites) dot(s.x, s.y, s.landmark ? 5 : 3, s.landmark ? [185, 167, 255] : [255, 255, 255]);
for (const s of shrines) dot(s.x, s.y, 4, [255, 150, 40]);
for (const b of bridges) for (let t = 0; t <= 1; t += 0.02) dot(lerp(b.from[0], b.to[0], t), lerp(b.from[1], b.to[1], t), 1, [255, 60, 60]);
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
fs.writeFileSync(path.join(root, 'Docs', 'terrain', 'storm_peaks.png'), png(PR, PR, img));

if (!PREVIEW_ONLY) {
	const buf = Buffer.alloc(N * N * 2);
	for (let k = 0; k < N * N; k++) buf.writeUInt16LE(Math.max(0, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2);
	fs.writeFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16'), buf);
}
let maxH = -1e9, land = 0; for (let k = 0; k < N * N; k++) { maxH = Math.max(maxH, HF[k]); if (HF[k] > 0.5) land++; }
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, highest: Math.round(maxH), landKm2: +(land * STEP * STEP / 1e6).toFixed(2), joinedStrip: joined, sites: sites.length, kit: kit.length, rods: rods.length, bridges: bridges.length, overhangs: overhangs.length }));

// ---------------------------------------------------------------- dressing (Data/island_dressing.json, the map's dressing file)
// The decoration rule (DC 2.1-2.2, THE BAR: full and alive, never cluttered): wind-bent pines in the Wind Passes'
// valleys and on the Cliff Paths' benches, krummholz thinning out up to 1,000 m, granite boulder fields on the Struck
// Crags, slate scree at every cliff foot (no collision: cliffs stay walls), grey-green shrubs and thrift low down.
// Nothing on the routes (8 m clear), at the sites (25 m), in the camps (60 m) or the arenas.
if (!PREVIEW_ONLY) {
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
	const out = { storm_pine: [], granite_rock: [], slate_scree: [], storm_shrub: [] };
	const clearOf = [...sites.map(s => [s.x, s.y, s.kind === 'camp' ? 60 : s.landmark ? 45 : 25]), ...shrines.map(s => [s.x, s.y, 35]),
		[-200, -3300, 70], [-1250, -4600, 60], [-1350, -3960, 45], ...kit.map(k => [k.x, k.y, 14])];
	// Routes: a coarse occupancy grid of route cells (4 m) for the 8 m clearance.
	const routeCells = new Set();
	for (const r of ROUTES) for (let q = 0; q < r.dense.length - 1; q++) {
		const [ax, ay] = r.dense[q], [bx, by] = r.dense[q + 1];
		const n = Math.ceil(Math.hypot(bx - ax, by - ay) / 2);
		for (let s = 0; s <= n; s++) {
			const x = lerp(ax, bx, s / n), y = lerp(ay, by, s / n);
			for (let dx = -2; dx <= 2; dx++) for (let dy = -2; dy <= 2; dy++) routeCells.add(`${Math.floor(x / 4) + dx},${Math.floor(y / 4) + dy}`);
		}
	}
	for (const b of bridges) for (let t = 0; t <= 1; t += 0.01) { const x = lerp(b.from[0], b.to[0], t), y = lerp(b.from[1], b.to[1], t); for (let dx = -3; dx <= 3; dx++) for (let dy = -3; dy <= 3; dy++) routeCells.add(`${Math.floor(x / 4) + dx},${Math.floor(y / 4) + dy}`); }
	const free = (x, y) => !routeCells.has(`${Math.floor(x / 4)},${Math.floor(y / 4)}`) && clearOf.every(([cx, cy, r]) => Math.hypot(cx - x, cy - y) > r);
	const push = (kind, x, y, yaw, scale, sink = 0) => out[kind].push(Math.round(x * 100), Math.round(y * 100), Math.round(hAt(x, y) * 100 - sink), Math.round(yaw), +scale.toFixed(2));
	let seed = 777; const R = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	const valleyNear = (y) => Math.max(...[-4700, -3700, -2800].map(v => Math.exp(-(((y - v) / 160) ** 2))));
	for (let gy = Y0 + 20; gy < -1950; gy += 9) for (let gx = X0 + 20; gx < X1 - 20; gx += 9) {
		const x = gx + (R() - 0.5) * 8, y = gy + (R() - 0.5) * 8;
		const h = hAt(x, y);
		if (h < 2.5) continue;
		const z = zoneAt(x, y);
		if (z === '~' || z === 'e') continue;
		const sl = slopeAt(x, y);
		if (!free(x, y)) continue;
		const roll = R();
		// Pines lean with the east wind: their yaw is kept near east (the meshes have no lean of their own).
		if (sl < 0.55 && h > 8 && h < 1000) {
			const lowland = h < 300, valley = valleyNear(y) * (x < -1700 ? 1 : 0);
			const p = (lowland ? 0.03 + 0.12 * valley : h < 650 ? 0.07 : 0.03 * (1 - (h - 650) / 350));
			if (roll < p) { push('storm_pine', x, y, 80 + R() * 25, h > 600 ? 0.38 + R() * 0.2 : h < 300 ? 0.55 + R() * 0.25 : 0.65 + R() * 0.3); continue; }
		}
		if (sl < 0.85) {
			const p = z === '4' ? 0.12 : z === '6' ? 0.04 : z === '3' ? 0.06 : 0.025;
			if (roll < p) { push('granite_rock', x, y, R() * 360, 0.5 + R() * (z === '4' ? 1.4 : 0.8)); continue; }
		}
		if (sl > 0.9 && sl < 3.5 && roll < 0.035) { push('slate_scree', x, y, R() * 360, 0.6 + R() * 0.9, 120); continue; }
		if (sl < 0.45 && h < 700 && roll < (h < 300 ? 0.3 : 0.14)) push('storm_shrub', x, y, R() * 360, 0.6 + R() * 0.5);
	}
	for (const k of Object.keys(out)) dress.instances[k] = out[k];
	dress.counts = dress.counts || {};
	for (const k of Object.keys(out)) dress.counts[k] = out[k].length / 5;
	// Points of interest the map, compass and deeds read (FWRPoi): the camps, the caves, the landmarks.
	dress.pois = (dress.pois || []).filter(p => !p.storm);
	for (const s of sites) {
		const kind = s.kind === 'camp' ? 'camp' : s.cave ? 'cave' : s.landmark ? 'landmark' : null;
		if (!kind) continue;
		dress.pois.push({ name: s.name, kind, x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'storm_peaks', storm: true });
	}
	fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	console.error(`dressing: ${Object.entries(out).map(([k, v]) => `${k} ${v.length / 5}`).join(', ')}`);
}
