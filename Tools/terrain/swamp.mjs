// The Swamp (the Mire): the east half of belt B3 under the Crown's east face, north of the Old Island
// (cooks/biome-swamp.md 2.1-2.8, 2.13-2.14, 2.20; cooks/build-biome-swamp.md 1.3, 2.1-2.3; cooks/world-map.md 2.2).
// Continent metres: +x east, +y south, origin at the island's centre. Writes
//   Art/Terrain/swamp_height.r16   2017 x 2017 samples, 2 m apart, from (X0, Y0); value = 32768 + metres * 16
//                                  (import with landscape scale 200, 200, 800: Tools/terrain/import_swamp.py)
//   Data/swamp.json                what the game builds on it at runtime (sites, water kinds, routes, arenas, the
//                                  Sunken Coven, secrets): generated, edit this script
//   Docs/terrain/swamp.png         shaded layout preview (1 px = 8 m)
//   Data/island_dressing.json      the swamp's trees, reeds and shrubs (harvestable) and its map POIs
// node Tools/terrain/swamp.mjs [--preview-only]
//
// Placement (build doc 1.3): the cook measured everything from "the cliff foot at about x +300". Storm peaks built the
// Crown's east face further east, so every cook point (x, y) maps to (cliffFoot(y) + x - 300, y), with cliffFoot read
// from Art/Terrain/storm_height.r16. Fields (zones, coast distance, noise) are computed in cook space; round things
// (pools, mounds, the Throat) are built in world space round their mapped centres so they stay round.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const PREVIEW_ONLY = process.argv.includes('--preview-only');
export const X0 = 250, Y0 = -5592, STEP = 2, N = 2017, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;
const SEED = 73;
const SEA_Z = 0.6;              // the drawn water surface (sea.level -20 cm + water.surfaceOffset 80 cm)

// ---------------------------------------------------------------- noise
const hash = (x, y) => { let h = (x * 374761393 + y * 668265263 + SEED * 2147483647) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
	const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
	const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
	return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// ---------------------------------------------------------------- the neighbours' heightmaps
const SF = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'storm_peaks.json'), 'utf8')).frame;
const stormRaw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16'));
const stormH = (x, y) => {
	const i = Math.round((x - SF.x0) / SF.step), j = Math.round((y - SF.y0) / SF.step);
	if (i < 0 || j < 0 || i >= SF.samples || j >= SF.samples) return null;
	return (stormRaw.readUInt16LE((j * SF.samples + i) * 2) - 32768) / SF.unitsPerMetre;
};
const ISL = { size: 4033, half: 2016 };
const islandRaw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16'));
const islandH = (x, y) => {
	const i = Math.round(x + ISL.half), j = Math.round(y + ISL.half);
	if (i < 0 || j < 0 || i >= ISL.size || j >= ISL.size) return null;
	return (islandRaw.readUInt16LE((j * ISL.size + i) * 2) - 32768) / 32;
};

// The Crown's / Spine's east face: the easternmost storm ground above 40 m at each y (every 4 m), and its top (the
// biome's west boundary: where the storm ground first rises above 300 m going west, or the 40 m line where it is lower).
const CF_STEP = 4, CF_Y0 = Y0 - 40, CF_N = Math.ceil((Y1 + 40 - CF_Y0) / CF_STEP) + 1;
const cfRaw = new Float32Array(CF_N), topRaw = new Float32Array(CF_N);
for (let k = 0; k < CF_N; k++) {
	const y = CF_Y0 + k * CF_STEP;
	let cf = null, top = null;
	for (let x = SF.x0 + (SF.samples - 1) * SF.step; x >= 0; x -= 2) {
		const h = stormH(x, y);
		if (h === null) continue;
		if (cf === null && h > 40) cf = x;
		if (cf !== null && h > 300) { top = x; break; }
		if (cf !== null && x < cf - 400) break;
	}
	cfRaw[k] = cf === null ? 250 : cf;
	topRaw[k] = top === null ? (cf === null ? 250 : cf) : top;
}
const smoothArr = (arr, win) => { const out = new Float32Array(arr.length); const w = Math.round(win / CF_STEP); for (let k = 0; k < arr.length; k++) { let s = 0, n = 0; for (let q = -w; q <= w; q++) { const v = arr[clamp(k + q, 0, arr.length - 1)]; s += v; n++; } out[k] = s / n; } return out; };
const cfAct = smoothArr(cfRaw, 16), cfSmooth = smoothArr(cfRaw, 260), topSm = smoothArr(topRaw, 40);
const sample = (arr, y) => { const f = (y - CF_Y0) / CF_STEP, k = clamp(Math.floor(f), 0, CF_N - 2), t = clamp(f - k, 0, 1); return lerp(arr[k], arr[k + 1], t); };
const cliffAct = y => sample(cfAct, y);
const OFF = y => sample(cfSmooth, y) + 25 - 300;          // cook x -> world x
const M = (cx, y) => [cx + OFF(y), y];                    // cook point -> world point
const cookX = (wx, y) => wx - OFF(y);

// ---------------------------------------------------------------- the layout (cook coordinates, DC 2.2-2.6, 2.13-2.14)
// Sub-zones (DC 2.2) in cook space, boundaries wiggled 60 m (the blends).
function zoneCook(cx, y) {
	const w = 320 * fbm(cx / 520 + 7.1, y / 520, 4) + 50 * fbm(cx / 90, y / 90, 2);
	if (y > -2550 + 160 * fbm(cx / 300, 3.3, 3)) return 'f';                     // the frost border (frozen bog)
	if (y < -4500 + w && cx >= 1100 + w) return '2';                            // M2 mangroves
	if (y < -5000 + w) return '2';                                              // the estuary north of the gut mouth
	if (cx >= 1800 + w) return '1';                                             // M1 reed flats (coast band)
	if (y < -3400 + w && cx < 1100 + w) return '5';                             // M5 bone mounds (the Crown foot)
	if (y < -3400 + w) return '3';                                              // M3 boardwalks
	return '4';                                                                 // M4 murk pools (and the cliff foot south)
}

// Coast: cook x of the shore by y (ragged, with tidal creeks), the north estuary, the south edge (the Rime Sound).
const coastCook = y => 2450 + 90 * fbm(y / 520, 1.9, 4) + 40 * fbm(y / 130, 6.1, 3);
const NECK_W = [[1250, -2460], [1180, -2300], [1100, -2120], [1050, -1960], [1020, -1800], [1000, -1640], [990, -1560]];  // world x for the last ones (island side)

// Round features: [cookX, y, ...]. Heights are metres above the sea's zero (the water surface draws at +0.6).
const P = (cx, y) => { const [x] = M(cx, y); return { x, y }; };
const THROAT = P(900, -3600);
const DEEP = P(1300, -3100);
const MAW = P(1500, -2700);
const OSSUARY = P(700, -4300);
const WALLOW = P(1050, -2600);
const MERE = P(1350, -4200);
const DOME = P(1200, -4000);
const LEECH = P(1650, -2550);
const TITHE = P(1640, -3720);
const WALKING = P(1500, -3850);
const HANGING = P(1180, -2950);
const SALT = P(2300, -3550);
const CATHEDRAL = P(1500, -4950);
const GUT_MOUTH = { y: -4885 };          // Stormgut's east mouth (storm_peaks.mjs gorge 'stormgut')

// Pools: [x, y, radius, floor, kind] in world space (kind = the water look).
const POOLS = [
	[MAW.x, MAW.y, 30, -2.4, 'd'],                         // Mother Maw's nest pool (duckweed, 1.5-3 m deep)
	[WALLOW.x, WALLOW.y, 25, -1.0, 'b'],                   // the Bloat's Wallow
	[MERE.x, MERE.y, 150, -2.0, 'm'],                      // Corpse-Candle Mere (300 m)
	[LEECH.x, LEECH.y, 22, -1.2, 'b'],                     // the Leech Pond
	[DEEP.x, DEEP.y, 20, -14, 'b'],                        // the Deep Pool (top 2 m bleedwater)
];

// ---------------------------------------------------------------- base surface (world point)
const NECK = NECK_W.map(([a, y], k) => k < 2 ? [M(a, y)[0], y] : [a + (M(1250, -2460)[0] - 1250) * (1 - k / (NECK_W.length - 1)), y]);
function distToPolyline(x, y, pts) {
	let best = Infinity, tBest = 0, segBest = 0;
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay] = pts[s], [bx, by] = pts[s + 1];
		const dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
		const t = L2 > 0 ? clamp(((x - ax) * dx + (y - ay) * dy) / L2, 0, 1) : 0;
		const d = Math.hypot(x - (ax + t * dx), y - (ay + t * dy));
		if (d < best) { best = d; tBest = t; segBest = s; }
	}
	return { d: best, s: segBest, t: tBest };
}
function base(x, y) {
	const cx = cookX(x, y);
	const zone = zoneCook(cx, y);
	// Land mask: west of the coast, north of the Rime Sound (or on the neck), south of the estuary's end.
	const eCoast = coastCook(y) - cx;
	const southEdge = -2400 + 30 * fbm(cx / 240, 2.7, 3);
	const eSouth = southEdge - y;
	const neck = distToPolyline(x, y, NECK);
	const neckW = 70 + 25 * fbm(y / 120, 4.4, 2);
	const eNeck = neckW - neck.d;
	// North: the mangrove estuary breaks into islands north of -5,000 and ends in the sea by -5,450.
	const isl = fbm(cx / 110 + 3.3, y / 110, 4);
	const eNorth = y > -5000 ? 400 : (isl + 0.12 - 0.5 * sstep(-5000, -5460, y)) * 300;
	const edge = Math.min(eCoast, Math.max(eSouth, eNeck), eNorth);
	const land = sstep(-30, 40, edge);
	let h;
	// Dry ground: +1.1..+2.1 m with low hummocks; the coast's reed flats lower (mudflats near the sea).
	h = 1.6 + 0.45 * fbm(x / 80, y / 80, 4) + 0.22 * fbm(x / 18, y / 18, 3);
	if (zone === '1') h -= 0.4 * (1 - sstep(0, 260, eCoast));
	// Water (cook 2.2): a water field per sub-zone; floors below the surface.
	let wf = 0, floor = -1.2;
	if (zone === '3') { wf = sstep(0.04, 0.2, fbm(cx / 190 + 11, y / 190, 4)); floor = -0.8 - 0.8 * vnoise(x / 60, y / 60); }      // open murk between bog domes
	if (zone === '4') { wf = sstep(0.1, 0.24, fbm(cx / 95 + 21, y / 95, 4)); floor = -1.0 - 1.5 * vnoise(x / 40, y / 40); }       // murk pools in cypress swamp
	if (zone === '5') { wf = 0.85 * sstep(0.12, 0.28, fbm(cx / 120 + 31, y / 120, 4)); floor = -0.6 - 0.8 * vnoise(x / 50, y / 50); } // bleed channels between mounds
	if (zone === '2') { const r = Math.abs(fbm(cx / 170 + 41, y / 170, 4)); wf = 1 - sstep(0.03, 0.09, r); floor = -2 - 2 * vnoise(x / 70, y / 70); }  // braided channels
	if (zone === '1') { const r = Math.abs(fbm(cx / 260 + 51, y / 140, 3)); wf = (1 - sstep(0.035, 0.085, r)) * sstep(60, 700, 2600 - cx); floor = -1.5; }  // tidal creeks into the reeds
	if (zone === 'f') wf = 0;
	h = lerp(h, floor, wf);
	// M3 raised-bog domes (+3..+4.5 m, 100-200 m across) and the Bloodmoss Dome; M5 bone mounds.
	if (zone === '3') { const dome = sstep(0.16, 0.4, fbm(cx / 160 + 61, y / 160, 3)); if (dome > 0) h = lerp(h, 1.4 + 3.0 * dome, sstep(0, 0.3, dome)); }
	if (zone === '5') { const mound = sstep(0.26, 0.6, fbm(cx / 70 + 71, y / 70, 3)); if (mound > 0) h = lerp(h, 1.6 + 9 * Math.pow(mound, 1.6), sstep(0, 0.3, mound)); }
	// M4 hummocks; M2 pneumatophore roughness on the dry ground.
	if (zone === '4' && h > SEA_Z) h += 0.6 * Math.max(0, vnoise(x / 9, y / 9) - 0.55);
	if (zone === '2' && h > SEA_Z) h += 0.15 * vnoise(x / 3, y / 3);
	// Frost border / the neck: a frozen bog just over the water.
	if (zone === 'f') h = 0.9 + 0.35 * fbm(x / 60, y / 60, 3) + (eNeck > 0 ? 0.6 * sstep(0, 40, eNeck) : 0);
	// Talus at the cliff foot: scree rising to the face (hidden under the storm ground where that is higher).
	const fromFace = x - cliffAct(y);
	if (fromFace < 70) h = Math.max(h, 2 + 30 * Math.pow(1 - clamp(fromFace / 70, 0, 1), 2));
	return { h: lerp(-14, h, land), land, zone };
}

// ---------------------------------------------------------------- generate the grid
const t0 = Date.now();
const HF = new Float32Array(N * N);
const RAMP = new Uint8Array(N * N);
const LANDM = new Float32Array(N * N);   // 1 inside the swamp's land mask (its pools included), 0 the open sea   // the Stormgut ramp: lifted over the storm ground on purpose
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const s = base(X0 + i * STEP, Y0 + j * STEP);
	HF[j * N + i] = s.h; LANDM[j * N + i] = s.land;
}
console.error(`base ${((Date.now() - t0) / 1000).toFixed(1)} s`);
const hAt = (x, y) => {
	const fx = (x - X0) / STEP, fy = (y - Y0) / STEP;
	const i = clamp(Math.floor(fx), 0, N - 2), j = clamp(Math.floor(fy), 0, N - 2);
	const tx = clamp(fx - i, 0, 1), ty = clamp(fy - j, 0, 1);
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
// Level a site: core radius flat at H (null = the ground's own height, at least 1.6 m so it stays dry), smooth falloff.
function level(x, y, H0, rc, rf, minH = 1.6) {
	const H = H0 === null ? Math.max(minH, hAt(x, y)) : H0;
	around(x, y, rc + rf, (k, d, sx, sy) => {
		const w = 1 - sstep(rc, rc + rf, d);
		HF[k] = lerp(HF[k], H + (d < rc ? 0.12 * fbm(sx / 7, sy / 7, 2) : 0), w);
	});
	return H;
}
// A pool: flat floor inside 0.6 R, banks rising to the ground at R.
function pool(x, y, R, floor) {
	around(x, y, R * 1.25, (k, d, sx, sy) => {
		const t = sstep(R * 0.55, R * 1.05, d);
		const v = lerp(floor + 0.3 * fbm(sx / 11, sy / 11, 2), Math.max(1.2, HF[k]), t);
		HF[k] = Math.min(HF[k], v);
	});
}
const cone = (x, y, Rbase, Rtop, top) => around(x, y, Rbase, (k, d, sx, sy) => {
	const v = d <= Rtop ? top + 0.15 * fbm(sx / 6, sy / 6, 2) : lerp(top, 1.4, Math.pow((d - Rtop) / (Rbase - Rtop), 0.8)) + 0.4 * fbm(sx / 5, sy / 5, 2);
	HF[k] = Math.max(HF[k], v);
});

// Sites levelled (DC 2.4): camps, the Walking House pad, the Tithe Mound, the Salt Lick pan, the Peat Cut dome...
const CAMPS = [[2250, -2750], [2250, -3800], [2150, -4800]].map(([cx, y]) => P(cx, y));
for (const c of CAMPS) level(c.x, c.y, null, 45, 60, 1.8);
level(WALKING.x, WALKING.y, 1.9, 14, 30);
level(TITHE.x, TITHE.y, null, 4, 26); cone(TITHE.x, TITHE.y, 26, 9, 3.0);
level(SALT.x, SALT.y, 0.85, 40, 40);
level(HANGING.x, HANGING.y, 2.2, 18, 30);
const PEAT = P(1250, -3750); level(PEAT.x, PEAT.y, 3.2, 70, 50);
level(DOME.x, DOME.y, null, 1, 1); cone(DOME.x, DOME.y, 120, 60, 3.6);
const STILT = P(2000, -3300); level(STILT.x, STILT.y, 1.4, 60, 50);
const CLAY = P(2100, -4200); level(CLAY.x, CLAY.y, 1.5, 30, 30);
const WRECK = P(2050, -5200); level(WRECK.x, WRECK.y, -1.4, 18, 20);
const GARWEIR = P(1800, -4700); level(GARWEIR.x, GARWEIR.y, 1.3, 12, 20);
const ROOKERY = P(620, -4620); level(ROOKERY.x, ROOKERY.y, 2.4, 50, 40);
const WEEPPOOLS = P(380, -3900);
const FROZEN = P(1250, -2460); level(FROZEN.x, FROZEN.y, 1.2, 20, 30);
const THORN = P(520, -4950);
const ROD_LINE = { a: P(450, -4800), b: P(1050, -4800) };
for (const [x, y, R, f] of POOLS) pool(x, y, R, f);
// Mother Maw's nest: a compost mound (+2.2 m, 12 m) in the middle of her pool.
cone(MAW.x, MAW.y, 13, 4, 2.2);
// The Hex-Warden's Ossuary: a bone mound 25 m high, a flat top 36 m across; a ramp climbs its east flank.
const OSS_TOP = 26.5;
cone(OSSUARY.x, OSSUARY.y, 62, 18, OSS_TOP);
// The Mere's reed island (M-S1's end) and the near-shore punt jetty.
const CANDLE_ISLAND = { x: MERE.x + 70, y: MERE.y - 75 };
cone(CANDLE_ISLAND.x, CANDLE_ISLAND.y, 14, 6, 1.3);

// The Mother's Throat (DC 2.14): a 110 m flooded sinkhole; rim bank (+4 m, the west side a 7 m wall she anchors to),
// upper ring +1.5 (dry), middle ring +0.4 (20 cm of water), lower ring 0.0 (60 cm), the central pool to -8 m.
const TH = { r: 55, upper: 1.5, middle: 0.4, lower: 0.0, pool: -8 };
around(THROAT.x, THROAT.y, 90, (k, d, sx, sy) => {
	const a = Math.atan2(sy - THROAT.y, sx - THROAT.x);
	const west = Math.max(0, -Math.cos(a));
	let v;
	if (d < 20) v = lerp(TH.pool, TH.lower, sstep(12, 20, d));
	else if (d < 32) v = TH.lower + 0.05 * fbm(sx / 5, sy / 5, 2);
	else if (d < 45) v = lerp(TH.lower, TH.middle, sstep(31, 33, d)) + 0.05 * fbm(sx / 5, sy / 5, 2);
	else if (d < 55) v = lerp(TH.middle, TH.upper, sstep(44, 46, d));
	else if (d < 62) v = lerp(TH.upper, 4 + 3 * west, sstep(55, 58, d));
	else v = lerp(4 + 3 * west, HF[k], sstep(62, 90, d));
	HF[k] = v;
});
const ang = deg => deg * Math.PI / 180;
const onRing = (r, deg) => ({ x: THROAT.x + Math.cos(ang(deg)) * r, y: THROAT.y + Math.sin(ang(deg)) * r });
// Four ossuary mounds on the middle ring (the east half: she faces east from her west-wall anchor).
const MOUNDS = [-110, -40, 40, 110].map(dg => onRing(38.5, dg));
for (const m of MOUNDS) cone(m.x, m.y, 7, 3, 3.6);
// Six rod plinths between the rings (1.5 m above the middle ring).
const RODS = [[-150, 32.5], [-90, 45], [-30, 32.5], [30, 45], [90, 32.5], [150, 45]].map(([dg, r]) => onRing(r, dg));
for (const p of RODS) around(p.x, p.y, 3.2, (k, d) => { HF[k] = Math.max(HF[k], d < 2 ? 2.0 : lerp(2.0, HF[k], sstep(2, 3.2, d))); });
// The Throat Stair: a bone-paved ramp through the east rim down to the upper ring; the run-back stone 50 m out.
const STAIR_TOP = onRing(80, -18), STAIR_FOOT = onRing(52, -18);
const ANCHOR = onRing(57, 180);
const RUNBACK_V = onRing(130, -18);

// Stormgut's east mouth: the gut's floor leaves the Crown 120 m up; a scree ramp runs down east to the swamp
// (the Rodwardens' march: the Bled Roc lies on it, the Thorn Gate faces up it).
{
	const yg = GUT_MOUTH.y;
	let x0 = null;
	for (let x = 1132; x >= 300; x -= 2) { const v = stormH(x, yg); if (v !== null && v > 60) { x0 = x; break; } }
	if (x0 !== null) {
		const top = stormH(x0, yg);
		const len = top / 0.28;
		const pts = [];
		for (let s = 0; s <= 40; s++) pts.push([x0 - 8 + (len + 8) * s / 40, yg + 10 * Math.sin(s / 6)]);
		for (let q = 0; q < pts.length; q++) {
			const [px, py] = pts[q];
			const zt = lerp(top + 1, 2, q / (pts.length - 1));
			around(px, py, 55, (k, d, sx, sy) => {
				const sh = stormH(sx, sy);
				const w = 1 - sstep(28, 55, d);
				let v = lerp(HF[k], zt + 0.5 * fbm(sx / 8, sy / 8, 2), w);
				if (sh !== null && sh > v - 1) {
					if (w < 0.5) return;                     // the gut's own walls: ours stays hidden under them
					v = sh + 1.0;                            // the ramp's head lies just over the gut's floor
				}
				if (v > HF[k]) HF[k] = v;
				if (w >= 0.5) RAMP[k] = 1;
			});
		}
		GUT_MOUTH.x = x0; GUT_MOUTH.top = top; GUT_MOUTH.len = len;
	}
}

// ---------------------------------------------------------------- routes (Coven boardwalks, reed causeways, bone and root paths)
// kind causeway / bone / root: a ground ridge (+1.4 m, bone +1.8); boardwalk: planks on posts (built at runtime) over
// the water, deck +1.2 m (0.6 m over the surface) or 0.3 m over the ground where that is higher.
const C = (cx, y) => M(cx, y);
const ROUTES = [
	{ id: 'coast_causeway', kind: 'causeway', w: 5, pts: [C(2250, -2750), C(2300, -3000), C(2230, -3300), C(2300, -3550), C(2250, -3800), C(2200, -4050), C(2100, -4200), C(2200, -4500), C(2150, -4800)] },
	{ id: 'camp2_spur', kind: 'causeway', w: 4, pts: [C(2250, -3800), C(2120, -3740), C(2000, -3700)] },
	{ id: 'drowned_causeway', kind: 'boardwalk', w: 2, pts: [C(2000, -3700), C(1800, -3690), C(1640, -3700), C(1450, -3710), C(1250, -3700), C(1050, -3690), [STAIR_TOP.x + 30, STAIR_TOP.y - 30]] },
	{ id: 'house_spur', kind: 'boardwalk', w: 2, pts: [C(1520, -3708), C(1505, -3780), [WALKING.x + 4, WALKING.y + 20]] },
	{ id: 'peat_spur', kind: 'boardwalk', w: 2, pts: [C(1260, -3702), [PEAT.x, PEAT.y + 32]] },
	{ id: 'mere_jetty', kind: 'boardwalk', w: 2, pts: [C(1360, -3706), C(1360, -3880), C(1350, -4010), [MERE.x - 10, MERE.y + 130]] },
	{ id: 'dome_walk', kind: 'boardwalk', w: 2, pts: [C(1150, -3695), C(1180, -3850), [DOME.x - 20, DOME.y + 55]] },
	{ id: 'south_walk', kind: 'boardwalk', w: 2, pts: [[TITHE.x, TITHE.y + 26], C(1600, -3500), C(1500, -3300), C(1380, -3160), [DEEP.x + 30, DEEP.y - 10], C(1250, -3020), [HANGING.x + 20, HANGING.y - 18]] },
	{ id: 'stilt_spur', kind: 'causeway', w: 4, pts: [C(2230, -3300), STILT ? [STILT.x + 50, STILT.y] : C(2000, -3300)] },
	{ id: 'south_causeway', kind: 'causeway', w: 4, pts: [C(2250, -2750), C(2000, -2700), C(1800, -2620), [LEECH.x + 30, LEECH.y - 5], C(1560, -2620), C(1380, -2780), [HANGING.x + 15, HANGING.y + 20]] },
	{ id: 'wallow_path', kind: 'causeway', w: 3, pts: [[HANGING.x - 15, HANGING.y + 15], C(1100, -2800), C(1080, -2660), [WALLOW.x + 26, WALLOW.y - 8]] },
	{ id: 'frost_path', kind: 'causeway', w: 3, pts: [C(1380, -2780), C(1300, -2600), [FROZEN.x, FROZEN.y - 15], [FROZEN.x - 10, FROZEN.y + 40], ...NECK.slice(2).map(([x, y]) => [x, y])] },
	{ id: 'bone_causeway', kind: 'bone', w: 4, pts: [[STAIR_TOP.x, STAIR_TOP.y], C(1050, -3700), C(820, -3800), C(560, -3870), [WEEPPOOLS.x + 30, WEEPPOOLS.y], C(520, -4050), C(640, -4200), [OSSUARY.x + 66, OSSUARY.y + 10], C(800, -4420), C(700, -4560), [ROOKERY.x + 20, ROOKERY.y + 40], C(560, -4760), C(500, -4840), [THORN.x, THORN.y - 0]] },
	{ id: 'rod_line', kind: 'bone', w: 3, pts: [[ROD_LINE.a.x, ROD_LINE.a.y], [ROD_LINE.b.x, ROD_LINE.b.y], C(1300, -4600), C(1480, -4480), C(1700, -4420), C(2100, -4400)] },
	{ id: 'mangrove_roots', kind: 'root', w: 3, pts: [C(2150, -4800), C(1960, -4740), [GARWEIR.x + 15, GARWEIR.y], C(1650, -4850), [CATHEDRAL.x + 40, CATHEDRAL.y], C(1700, -5050), C(1900, -5120), [WRECK.x - 25, WRECK.y]] },
	{ id: 'cathedral_link', kind: 'root', w: 3, pts: [[CATHEDRAL.x - 40, CATHEDRAL.y], C(1250, -4920), C(1050, -4880), [ROD_LINE.b.x, ROD_LINE.b.y]] },
	{ id: 'clay_spur', kind: 'causeway', w: 3, pts: [C(2200, -4050), [CLAY.x + 35, CLAY.y]] },
	{ id: 'salt_spur', kind: 'causeway', w: 3, pts: [C(2300, -3550), [SALT.x - 30, SALT.y]] },
	// Mother Maw's boardwalk square (the Bosses track builds its breakable sections; the generator keeps its line).
];
// Ossuary ramp: up the east flank at 0.33 rise per metre.
ROUTES.push({ id: 'ossuary_ramp', kind: 'bone', w: 4, ramp: true, pts: [[OSSUARY.x + 66, OSSUARY.y + 10], [OSSUARY.x + 40, OSSUARY.y + 24], [OSSUARY.x + 18, OSSUARY.y + 10]] });
ROUTES.find(r => r.id === 'ossuary_ramp').heights = [1.8, 12, OSS_TOP];
for (const r of ROUTES) {
	const dense = [];
	const spacing = r.kind === 'boardwalk' ? 2.4 : 6;
	for (let s = 0; s < r.pts.length - 1; s++) {
		const [ax, ay] = r.pts[s], [bx, by] = r.pts[s + 1];
		const n = Math.max(1, Math.round(Math.hypot(bx - ax, by - ay) / spacing));
		for (let q = 0; q < n; q++) {
			const t = q / n;
			const wob = r.kind === 'boardwalk' ? 0 : 3 * fbm((ax + (bx - ax) * t) / 40, (ay + (by - ay) * t) / 40, 2);
			const x = lerp(ax, bx, t) + wob * (by - ay) / Math.max(1, Math.hypot(bx - ax, by - ay));
			const y = lerp(ay, by, t) - wob * (bx - ax) / Math.max(1, Math.hypot(bx - ax, by - ay));
			let z;
			if (r.heights) z = lerp(r.heights[s], r.heights[s + 1], t);
			else if (r.kind === 'boardwalk') z = Math.max(SEA_Z + 0.6, hAt(x, y) + 0.3);
			else z = r.kind === 'bone' ? Math.max(1.8, hAt(x, y)) : Math.max(1.4, hAt(x, y));
			dense.push([x, y, z]);
		}
	}
	const last = r.pts[r.pts.length - 1];
	dense.push([last[0], last[1], r.heights ? r.heights[r.heights.length - 1] : r.kind === 'boardwalk' ? Math.max(SEA_Z + 0.6, hAt(last[0], last[1]) + 0.3) : Math.max(1.6, hAt(last[0], last[1]))]);
	// Boardwalk decks run level-ish: smooth the deck height.
	if (r.kind === 'boardwalk') for (let it = 0; it < 30; it++) for (let q = 1; q < dense.length - 1; q++) dense[q][2] = Math.max(SEA_Z + 0.55, lerp(dense[q][2], (dense[q - 1][2] + dense[q + 1][2]) / 2, 0.5));
	r.dense = dense;
}
// Ridges for the ground routes.
for (const r of ROUTES) {
	if (r.kind === 'boardwalk') continue;
	const half = r.w / 2, blend = r.ramp ? 6 : 8;
	for (let q = 0; q < r.dense.length - 1; q++) {
		const [ax, ay, ah] = r.dense[q], [bx, by, bh] = r.dense[q + 1];
		const n = Math.max(1, Math.ceil(Math.hypot(bx - ax, by - ay) / 1.5));
		for (let s = 0; s <= n; s++) {
			const t = s / n, x = lerp(ax, bx, t), y = lerp(ay, by, t), H = lerp(ah, bh, t);
			around(x, y, half + blend, (k, d) => {
				const w = 1 - sstep(half, half + blend, d);
				if (w <= 0) return;
				HF[k] = r.ramp ? lerp(HF[k], H, w) : Math.max(HF[k], lerp(HF[k], H, w));
			});
		}
	}
}
// The Drowned Causeway is broken in four places (jump, wade or swim; DC 2.4 #17), never two rotten planks in a row on it;
// 15% rotten elsewhere.
let rseed = 4242; const RR = () => { rseed = (rseed * 16807) % 2147483647; return rseed / 2147483647; };
for (const r of ROUTES) {
	if (r.kind !== 'boardwalk') continue;
	const n = r.dense.length - 1;
	r.rotten = []; r.broken = [];
	if (r.id === 'drowned_causeway') for (const f of [0.18, 0.41, 0.63, 0.84]) { const at = Math.round(f * n); const len = 1 + Math.floor(RR() * 2); for (let q = 0; q < len; q++) r.broken.push(at + q); }
	for (let q = 0; q < n; q++) {
		if (r.broken.includes(q) || r.broken.includes(q - 1) || r.broken.includes(q + 1)) continue;
		if (r.rotten.includes(q - 1)) continue;
		if (RR() < 0.15) r.rotten.push(q);
	}
}

// ---------------------------------------------------------------- the strip over the island (the Rime Sound neck)
let joined = 0;
for (let j = 0; j < N; j++) {
	const y = Y0 + j * STEP;
	if (y < -ISL.half) continue;
	for (let i = 0; i < N; i++) {
		const x = X0 + i * STEP, ih = islandH(x, y);
		if (ih === null) continue;
		const k = j * N + i;
		const nd = distToPolyline(x, y, NECK).d;
		const inNeck = nd < 110;
		let v = inNeck ? HF[k] : Math.min(HF[k], ih - 3);
		if (inNeck && ih > v - 1.5) v = ih - 1.5;      // the island's own ground wins where it is higher: no z-fight
		HF[k] = v; joined++;
	}
}
// Under Storm peaks: wherever the storm ground stands above ours, ours sinks 3 m under it (hidden), except on the gut
// ramp (already lifted over it) and the talus where we're higher.
let hidden = 0;
for (let j = 0; j < N; j++) {
	const y = Y0 + j * STEP;
	for (let i = 0; i < N; i++) {
		const x = X0 + i * STEP;
		if (x > SF.x0 + (SF.samples - 1) * SF.step) break;
		const sh = stormH(x, y);
		if (sh === null) continue;
		const k = j * N + i;
		if (RAMP[k]) continue;
		if (sh > HF[k] - 1.5 && sh > -5) { HF[k] = Math.min(HF[k], sh - 3); hidden++; }
	}
}

// ---------------------------------------------------------------- sub-zones (32 m grid) and water kinds (8 m grid)
const isSwampLand = (x, y) => x > sample(topSm, y) && (y < -ISL.half || distToPolyline(x, y, NECK).d < 120);
const ZC = 32, ZN = Math.ceil((N - 1) * STEP / ZC);
function subzone(x, y) {
	if (!isSwampLand(x, y)) return '~';
	const h = hAt(x, y);
	const sh = stormH(x, y);
	if (sh !== null && sh > h + 1) return '~';                 // under Storm peaks' ground: not ours
	const li = clamp(Math.round((x - X0) / STEP), 0, N - 1), lj = clamp(Math.round((y - Y0) / STEP), 0, N - 1);
	if (LANDM[lj * N + li] < 0.5 && !RAMP[lj * N + li]) return '~';   // the open sea
	return zoneCook(cookX(x, y), y);
}
const zoneRows = [];
for (let j = 0; j < ZN; j++) { let row = ''; for (let i = 0; i < ZN; i++) row += subzone(X0 + (i + 0.5) * ZC, Y0 + (j + 0.5) * ZC); zoneRows.push(row); }
const zoneAt = (x, y) => { const i = Math.floor((x - X0) / ZC), j = Math.floor((y - Y0) / ZC); return (i < 0 || j < 0 || i >= ZN || j >= ZN) ? '~' : zoneRows[j][i]; };

const WC = 8, WN = Math.ceil((N - 1) * STEP / WC);
const GAS = [];          // filled below (kit), painted 'g'
const poolKind = (x, y) => { for (const [px, py, R, , kind] of POOLS) if (Math.hypot(x - px, y - py) < R * 1.05) return kind; return null; };
function waterKind(x, y) {
	const z = zoneAt(x, y);
	if (z === '~') return '.';
	let lo = Infinity;
	for (let a = -3; a <= 3; a += 3) for (let b = -3; b <= 3; b += 3) lo = Math.min(lo, hAt(x + a, y + b));
	const c = hAt(x, y);
	for (const g of GAS) if (Math.hypot(x - g.x, y - g.y) < g.r) return 'g';
	if (Math.hypot(x - THROAT.x, y - THROAT.y) < 62) return c < SEA_Z ? 'm' : '.';        // her arena: its own rules
	if (c >= SEA_Z) {
		// Quag (DC 2.8): M1 mudflats and M4 pool margins: wet ground just above the surface, glossy grey mud.
		if (c < SEA_Z + 0.5 && (z === '1' || z === '4') && vnoise(x / 45 + 9, y / 45) > 0.52) return 'q';
		return '.';
	}
	const pk = poolKind(x, y);
	if (pk) return pk;
	if (z === '1' || z === '2') return 'k';                                                 // brackish: salt kills leeches
	if (z === 'f') return 'm';
	const n = vnoise(x / 130 + 17, y / 130 + 5);
	if (z === '5') return n < 0.62 ? 'b' : 'm';
	if (z === '4') return n < 0.36 ? 'b' : n < 0.78 ? 'd' : 'm';
	if (z === '3') return n < 0.12 ? 'b' : n < 0.42 ? 'd' : 'm';
	return 'm';
}

// ---------------------------------------------------------------- sites (DC 2.3 landmarks, 2.4 hand sites, camps, caves)
const r1 = v => +(+v).toFixed(1);
const S = (id, kind, x, y, extra = {}) => ({ id, kind, x: r1(x), y: r1(y), z: r1(hAt(x, y)), zone: zoneAt(x, y), ...extra });
// The Weeping Wall's five falls: off the storm cliff top (the first ground over 300 m going west) into plunge pools.
const FALLS = [];
for (const [k, y] of [-3560, -3720, -3880, -4040, -4200].entries()) {
	const foot = cliffAct(y) + 10;
	let tx = foot, top = 0;
	for (let x = foot; x > foot - 500; x -= 2) { const v = stormH(x, y); if (v === null) break; if (v > top) { top = v; tx = x; } if (v > 300 && stormH(x - 30, y) !== null && stormH(x - 30, y) < v + 25) break; }
	const px = foot + 25;
	pool(px, y, 16, -3);
	FALLS.push({ id: `fall_${k + 1}`, x: r1(tx), y: r1(y), top: r1(top), bottom: r1(SEA_Z), width: [14, 18, 24, 18, 12][k], pool: [r1(px), r1(y), -3] });
}
const THIRD = FALLS[2];
const sites = [
	S('weeping_wall', 'weeping_wall', cliffAct(-3880) + 30, -3880, { name: 'The Weeping Wall', landmark: true }),
	S('mothers_throat', 'mothers_throat', THROAT.x, THROAT.y, { name: "The Mother's Throat", landmark: true }),
	S('bled_roc', 'bled_roc', ...M(420, -4860), { name: 'The Bled Roc', landmark: true, yaw: 10 }),
	S('walking_house', 'walking_house', WALKING.x, WALKING.y, { name: 'The Walking House', landmark: true, yaw: 200 }),
	S('hanging_cypress', 'hanging_cypress', HANGING.x, HANGING.y, { name: 'The Hanging Cypress', landmark: true, keyCage: [r1(HANGING.x + 6), r1(HANGING.y - 3), 18] }),
	S('mangrove_cathedral', 'mangrove_cathedral', CATHEDRAL.x, CATHEDRAL.y, { name: 'The Mangrove Cathedral', landmark: true }),
	S('corpse_mere', 'corpse_mere', MERE.x, MERE.y, { name: 'Corpse-Candle Mere', landmark: true, weir: [r1(MERE.x), r1(MERE.y - 140)], punt: [r1(MERE.x - 10), r1(MERE.y + 128)] }),
	S('rod_line', 'rod_line', (ROD_LINE.a.x + ROD_LINE.b.x) / 2, ROD_LINE.a.y, { name: 'The Rod Line', from: [r1(ROD_LINE.a.x), r1(ROD_LINE.a.y)], to: [r1(ROD_LINE.b.x), r1(ROD_LINE.b.y)], loot: { copper_ore: 3, lore_tally_cord: 1 } }),
	S('thorn_gate', 'thorn_gate', THORN.x, THORN.y, { name: 'The Thorn Gate', yaw: 180, loot: { bone: 10, hunting_bow: 1, murkjaw_arrow: 12 } }),
	S('ossuary', 'ossuary', OSSUARY.x, OSSUARY.y, { name: "Hex-Warden's Ossuary", top: OSS_TOP }),
	S('throat_stair', 'throat_stair', STAIR_TOP.x, STAIR_TOP.y, { name: 'The Throat Stair', foot: [r1(STAIR_FOOT.x), r1(STAIR_FOOT.y), TH.upper] }),
	S('weeping_pools', 'weeping_pools', WEEPPOOLS.x, WEEPPOOLS.y, { name: 'The Weeping Pools', loot: { bog_iron: 12, lore_bloomery_mould: 1 } }),
	S('rookery', 'rookery', ROOKERY.x, ROOKERY.y, { name: 'Hexcrow Rookery', loot: { hexcrow_feather: 20 } }),
	S('maw_nest', 'maw_nest', MAW.x, MAW.y, { name: "Mother Maw's Nest" }),
	S('bloat_wallow', 'bloat_wallow', WALLOW.x, WALLOW.y, { name: "The Bloat's Wallow" }),
	S('deep_pool', 'deep_pool', DEEP.x, DEEP.y, { name: 'The Deep Pool', radius: 20 }),
	S('leech_pond', 'leech_pond', LEECH.x, LEECH.y, { name: 'The Leech Pond', post: [r1(LEECH.x + 24), r1(LEECH.y)] }),
	S('frozen_bog', 'frozen_bog', FROZEN.x, FROZEN.y, { name: 'The Frozen Bog', loot: { ice_block: 4, lichen: 6 } }),
	S('tithe_mound', 'tithe_mound', TITHE.x, TITHE.y, { name: 'The Tithe Mound' }),
	S('peat_cut', 'peat_cut', PEAT.x, PEAT.y, { name: 'The Peat Cut', yaw: 15, loot: { peat: 20, lore_braided_cord: 1 }, fourth: [r1(PEAT.x + 55), r1(PEAT.y - 30)] }),
	S('bloodmoss_dome', 'bloodmoss_dome', DOME.x, DOME.y, { name: 'The Bloodmoss Dome', nodes: 24 }),
	S('drowned_causeway', 'drowned_causeway', ...M(1500, -3705), { name: 'The Drowned Causeway' }),
	S('stilt_village', 'stilt_village', STILT.x, STILT.y, { name: 'The Stilt Village', loot: { slimeshell_slime: 6, bog_rice_seeds: 9 } }),
	S('clay_pit', 'clay_pit', CLAY.x, CLAY.y, { name: 'The Clay Pit', loot: { clay: 12 } }),
	S('salt_lick', 'salt_lick', SALT.x, SALT.y, { name: 'The Salt Lick', loot: { salt: 10 } }),
	S('estuary_wreck', 'estuary_wreck', WRECK.x, WRECK.y, { name: 'The Estuary Wreck', yaw: 35, loot: { iron_ingot: 2, rope: 4, linen: 3, lore_ships_log_mire: 1 } }),
	S('gar_weir', 'gar_weir', GARWEIR.x, GARWEIR.y, { name: 'Gar Weir' }),
	// The four landmark-foot sites of cook 2.4 (#10, #13, #18, #23), each 150-250 m from its landmark on its own dry ground.
	S('cage_yard', 'cage_yard', 1527.6, -2986.5, { name: "The Cage-Smith's Yard", yaw: 10, loot: { bog_iron: 6, iron_ingot: 1, rope: 2 } }),
	S('house_yard', 'house_yard', 1837.1, -3936, { name: 'The Drying Yard', yaw: 35, loot: { bloodmoss: 6, leech: 3, salt: 2 } }),
	S('mere_shore', 'mere_shore', 1811.8, -4341.4, { name: 'Candle Landing', yaw: 135, loot: { rope: 2, planks: 4, fish_bog_eel: 1 } }),
	S('cathedral_floor', 'cathedral_floor', 1921.7, -4750, { name: 'The Oyster Roots', yaw: 270, loot: { salt: 4, rope: 2, bone: 3 } }),
];
CAMPS.forEach((c, n) => sites.push(S(`camp_${n + 1}`, 'camp', c.x, c.y, { name: `Mire camp ${n + 1}`, yaw: 270 })));
const CAVES = [['reedmouth', 'Reedmouth', ...M(2050, -2900), 90], ['eel_hole', 'Eel Hole', ...M(1700, -5050), 0], ['peat_cellar', 'Peat Cellar', ...M(1300, -4350), 120], ['gullet', 'The Gullet', cliffAct(THIRD.y) + 6, THIRD.y, 270]];
for (const [id, name, x, y, yaw] of CAVES) { level(x, y, null, 10, 20, 1.6); sites.push(S(id, 'cave', x, y, { name, cave: id, yaw })); }
const SHRINES = [
	{ id: 'tithe_mound', name: 'The Tithe Mound', ...TITHE, great: true, guard: [] },
	{ id: 'salt_cross', name: 'Salt Cross', ...P(2150, -3250), guard: ['bogsnout', 'bogsnout', 'gorgeleech', 'gorgeleech'] },
	{ id: 'root_shrine', name: 'Root Shrine', ...CATHEDRAL, guard: ['murkjaw', 'hexcrow', 'hexcrow'] },
	{ id: 'cage_shrine', name: 'Cage Shrine', x: HANGING.x - 12, y: HANGING.y + 10, guard: ['bogstalker', 'bogstalker'] },
	{ id: 'falls_shrine', name: 'Falls Shrine', x: cliffAct(-3840) + 85, y: -3840, guard: ['gorgeleech', 'gorgeleech', 'gorgeleech', 'hexcrow', 'hexcrow'] },
].map(s => { level(s.x, s.y, null, 6, 16, 1.7); return { ...s, x: r1(s.x), y: r1(s.y), z: r1(hAt(s.x, s.y)) }; });
for (const s of sites) s.z = r1(hAt(s.x, s.y));
const sx = s => [s.x, s.y];

// ---------------------------------------------------------------- kit POIs (DC 2.4), spacing >= 150 m, route fill at 125 m
const KIT = [
	{ kind: 'stilt_hut', zones: '13', n: 5, v: 3, dry: true }, { kind: 'bloodmoss_hummock', zones: '34', n: 6, v: 3, dry: true },
	{ kind: 'bog_iron_shallows', zones: '145', n: 4, v: 2, wet: true }, { kind: 'cage_gallows', zones: '45', n: 4, v: 3, dry: true },
	{ kind: 'egg_reeds', zones: '14', n: 4, v: 2, edge: true }, { kind: 'kill_scene', zones: '34', n: 4, v: 4, dry: true },
	{ kind: 'gas_field', zones: '34', n: 3, v: 2 }, { kind: 'sinkhole', zones: '45', n: 3, v: 3, dry: true },
	{ kind: 'peat_stack', zones: '3', n: 3, v: 2, dry: true }, { kind: 'reedfowl_nest', zones: '12', n: 3, v: 2, edge: true },
	{ kind: 'broken_punt', zones: '23', n: 3, v: 2, edge: true }, { kind: 'toad_ring', zones: '34', n: 2, v: 2, dry: true },
];
const kit = [];
const arenaKeep = [[THROAT.x, THROAT.y, 140], [MAW.x, MAW.y, 70], [OSSUARY.x, OSSUARY.y, 90], [WALLOW.x, WALLOW.y, 50]];
const taken = [...sites.map(sx), ...SHRINES.map(sx), ...arenaKeep.map(([x, y]) => [x, y])];
let kseed = 919; const KR = () => { kseed = (kseed * 16807) % 2147483647; return kseed / 2147483647; };
const nearWater = (x, y) => { for (let a = 0; a < 6.28; a += 0.8) if (hAt(x + Math.cos(a) * 10, y + Math.sin(a) * 10) < SEA_Z) return true; return false; };
function place(K, minGap, at = null) {
	for (let tries = 0; tries < 900; tries++) {
		const x = at ? at[0] + (KR() - 0.5) * 120 : X0 + KR() * (N - 1) * STEP, y = at ? at[1] + (KR() - 0.5) * 120 : Y0 + KR() * (N - 1) * STEP;
		const z = zoneAt(x, y);
		if (!K.zones.includes(z)) continue;
		const h = hAt(x, y);
		if (K.dry && h < SEA_Z + 0.4) continue;
		if (K.wet && (h > SEA_Z + 0.3 || h < -1.2)) continue;
		if (K.edge && !(h > SEA_Z + 0.1 && nearWater(x, y))) continue;
		if (taken.some(([tx, ty]) => Math.hypot(tx - x, ty - y) < minGap)) continue;
		if (arenaKeep.some(([ax, ay, r]) => Math.hypot(ax - x, ay - y) < r)) continue;
		const item = { kind: K.kind, variant: Math.floor(KR() * K.v), x: r1(x), y: r1(y), z: r1(h), yaw: Math.round(KR() * 360), zone: z };
		if (K.kind === 'gas_field') {
			const vents = []; const nv = 6 + Math.floor(KR() * 7);
			for (let q = 0; q < nv; q++) { const a = KR() * 6.283, r = 4 + KR() * 16; vents.push([r1(x + Math.cos(a) * r), r1(y + Math.sin(a) * r), r1(hAt(x + Math.cos(a) * r, y + Math.sin(a) * r))]); }
			GAS.push({ x, y, z: h, r: 24, vents });
		}
		kit.push(item); taken.push([x, y]);
		return true;
	}
	return false;
}
for (const K of KIT) for (let q = 0; q < K.n; q++) place(K, 150);
// Route fill: every route point more than 125 m from any POI gets the kit piece that fits its zone.
const FILL = { '1': ['stilt_hut', 'reedfowl_nest', 'egg_reeds'], '2': ['reedfowl_nest', 'broken_punt'], '3': ['peat_stack', 'kill_scene', 'bloodmoss_hummock', 'toad_ring'], '4': ['cage_gallows', 'bloodmoss_hummock', 'kill_scene', 'sinkhole'], '5': ['cage_gallows', 'sinkhole'], f: [] };
let filled = 0;
for (const r of ROUTES) for (let q = 0; q < r.dense.length; q += 8) {
	const [x, y] = r.dense[q];
	if (taken.some(([tx, ty]) => Math.hypot(tx - x, ty - y) < 125)) continue;
	const z = zoneAt(x, y);
	const opts = FILL[z] || [];
	if (!opts.length) { taken.push([x, y]); continue; }
	const K = KIT.find(k => k.kind === opts[filled % opts.length]);
	if (place({ ...K, zones: z }, 110, [x, y])) filled++; else taken.push([x, y]);
}
// Fetish posts: the Coven's waymarks every ~160 m along its routes (boardwalks and bone paths), a crow skull facing the
// Throat (DC 2.20 beat 2).
for (const r of ROUTES) {
	if (r.kind !== 'boardwalk' && r.kind !== 'bone') continue;
	let acc = 80;
	for (let q = 1; q < r.dense.length; q++) {
		acc += Math.hypot(r.dense[q][0] - r.dense[q - 1][0], r.dense[q][1] - r.dense[q - 1][1]);
		if (acc < 160) continue;
		acc = 0;
		const [x, y] = r.dense[q];
		const dx = r.dense[q][0] - r.dense[q - 1][0], dy = r.dense[q][1] - r.dense[q - 1][1], L = Math.hypot(dx, dy) || 1;
		const side = (q % 2 ? 1 : -1) * (r.w / 2 + 1.2);
		const px = x - dy / L * side, py = y + dx / L * side;
		kit.push({ kind: 'fetish_post', variant: q % 3, x: r1(px), y: r1(py), z: r1(Math.max(hAt(px, py), SEA_Z - 0.6)), yaw: Math.round(Math.atan2(THROAT.y - py, THROAT.x - px) * 180 / Math.PI), zone: zoneAt(px, py), waymark: true });
	}
}

// ---------------------------------------------------------------- water grid (after the kit: gas fields paint 'g')
const waterRows = [];
let wCount = { m: 0, d: 0, b: 0, q: 0, k: 0, g: 0 };
for (let j = 0; j < WN; j++) {
	let row = '';
	for (let i = 0; i < WN; i++) { const c = waterKind(X0 + (i + 0.5) * WC, Y0 + (j + 0.5) * WC); row += c; if (wCount[c] !== undefined) wCount[c]++; }
	waterRows.push(row);
}
// The candle path's water is duckweed (the hidden boardwalk lies under it, DC 2.20 M-S1).
const CANDLE = { from: [MERE.x - 10, MERE.y + 120, SEA_Z - 0.3], pts: [], island: [CANDLE_ISLAND.x, CANDLE_ISLAND.y, 1.3] };
{
	const ctrl = [[MERE.x - 10, MERE.y + 120], [MERE.x - 45, MERE.y + 60], [MERE.x + 20, MERE.y + 15], [MERE.x - 15, MERE.y - 30], [MERE.x + 35, MERE.y - 55], [CANDLE_ISLAND.x - 8, CANDLE_ISLAND.y + 6]];
	for (let s = 0; s < ctrl.length - 1; s++) for (let q = 0; q < 10; q++) { const t = q / 10; CANDLE.pts.push([r1(lerp(ctrl[s][0], ctrl[s + 1][0], t)), r1(lerp(ctrl[s][1], ctrl[s + 1][1], t)), SEA_Z - 0.3]); }
	CANDLE.pts.push([r1(ctrl[ctrl.length - 1][0]), r1(ctrl[ctrl.length - 1][1]), SEA_Z - 0.3]);
	for (const [x, y] of CANDLE.pts) for (let a = -2; a <= 2; a++) for (let b = -2; b <= 2; b++) {
		const i = Math.floor((x + a * 6 - X0) / WC), j = Math.floor((y + b * 6 - Y0) / WC);
		if (i >= 0 && j >= 0 && i < WN && j < WN && waterRows[j][i] === 'm') waterRows[j] = waterRows[j].slice(0, i) + 'd' + waterRows[j].slice(i + 1);
	}
	CANDLE.from = CANDLE.from.map(r1); CANDLE.island = CANDLE.island.map(r1);
}

// ---------------------------------------------------------------- arenas, the Bloat's route, the Hex-Warden's walk, the Coven
const P3 = (x, y, z = null) => [r1(x), r1(y), r1(z === null ? hAt(x, y) : z)];
const arenas = {
	vharra: {
		...Object.fromEntries(Object.entries({ x: THROAT.x, y: THROAT.y, z: TH.upper }).map(([k, v]) => [k, r1(v)])), radius: 55,
		rings: { upper: [45, 55, TH.upper], middle: [32, 45, TH.middle], lower: [20, 32, TH.lower], pool: [0, 20, TH.pool], rim: [55, 62, 4] },
		mounds: MOUNDS.map(m => P3(m.x, m.y, 3.6)), rods: RODS.map(p => P3(p.x, p.y, 2.0)), anchor: P3(ANCHOR.x, ANCHOR.y, 2.0),
		stair: [P3(STAIR_TOP.x, STAIR_TOP.y), P3(STAIR_FOOT.x, STAIR_FOOT.y, TH.upper)], runback: P3(RUNBACK_V.x, RUNBACK_V.y),
		gate: P3(THROAT.x + Math.cos(ang(-60)) * 54, THROAT.y + Math.sin(ang(-60)) * 54, TH.upper),
		cypress: Array.from({ length: 12 }, (_, k) => P3(THROAT.x + Math.cos(ang(k * 30 + 15)) * 66, THROAT.y + Math.sin(ang(k * 30 + 15)) * 66)),
	},
	mother_maw: { x: r1(MAW.x), y: r1(MAW.y), z: r1(hAt(MAW.x, MAW.y)), nest: P3(MAW.x, MAW.y), pool: 30, square: { half: 22, z: 1.25, gaps: ['n', 's'] }, runback: P3(MAW.x + 75, MAW.y - 20) },
	hex_warden: { x: r1(OSSUARY.x), y: r1(OSSUARY.y), z: OSS_TOP, top: OSS_TOP, totems: [45, 135, 225, 315].map(dg => P3(OSSUARY.x + Math.cos(ang(dg)) * 13, OSSUARY.y + Math.sin(ang(dg)) * 13, OSS_TOP)), runback: P3(OSSUARY.x + 95, OSSUARY.y + 15) },
	bloat: { lair: P3(WALLOW.x, WALLOW.y), runback: P3(WALLOW.x + 60, WALLOW.y - 40), route: [] },
};
{
	const ctrl = [[WALLOW.x, WALLOW.y], [LEECH.x - 26, LEECH.y], [MAW.x + 34, MAW.y - 4], [HANGING.x + 24, HANGING.y + 4], M(1210, -3300), [DOME.x + 10, DOME.y + 70], [DOME.x - 40, DOME.y + 20], M(1050, -3400), M(1020, -3000), [WALLOW.x, WALLOW.y]];
	for (let s = 0; s < ctrl.length - 1; s++) {
		const [ax, ay] = ctrl[s], [bx, by] = ctrl[s + 1];
		const n = Math.max(1, Math.round(Math.hypot(bx - ax, by - ay) / 20));
		for (let q = 0; q < n; q++) { const t = q / n; const x = lerp(ax, bx, t), y = lerp(ay, by, t); arenas.bloat.route.push(P3(x, y)); }
	}
	let L = 0; for (let q = 1; q < arenas.bloat.route.length; q++) L += Math.hypot(arenas.bloat.route[q][0] - arenas.bloat.route[q - 1][0], arenas.bloat.route[q][1] - arenas.bloat.route[q - 1][1]);
	arenas.bloat.length = Math.round(L);
}
// The Hex-Warden's Red Tide night walk: Thorn Gate -> the bone causeway -> the Throat Stair -> the Drowned Causeway -> the
// Tithe Mound (and back the same way).
const sub = (pts, every) => pts.filter((_, q) => q % every === 0 || q === pts.length - 1).map(p => P3(p[0], p[1], p[2]));
const hexwalk = [...sub(ROUTES.find(r => r.id === 'bone_causeway').dense.slice().reverse(), 4), ...sub(ROUTES.find(r => r.id === 'drowned_causeway').dense.slice().reverse(), 9).slice(0, -1)];
{
	const t = hexwalk.findIndex(p => Math.hypot(p[0] - TITHE.x, p[1] - TITHE.y) < 40);
	if (t > 0) hexwalk.length = t + 1;
}
// The Sunken Coven (DC 2.5): rooms 6-20 m under the ground beside the Deep Pool, lanes toward the Throat; the cellar shaft
// from the Walking House joins the Ringing Room by the Coven's tithe drain.
const R = (id, cx, y, w, d, h, z) => { const [x] = M(cx, y); return { id, x: r1(x), y, z, w, d, h }; };
const coven = {
	deepPool: P3(DEEP.x, DEEP.y, -14), radius: 20, mouth: P3(DEEP.x - 19, DEEP.y - 4, -13),
	chimney: P3(...M(1380, -3040)), cellar: P3(WALKING.x, WALKING.y, -13),
	rooms: [
		R('drowned_lane', 1250, -3150, 60, 14, 5, -9), R('tithe_hall', 1180, -3230, 30, 20, 8, -10), R('library', 1205, -3290, 12, 10, 4, -6.5),
		R('gar_pool', 1290, -3255, 20, 20, 10, -14), R('bog_gallery', 1120, -3305, 40, 10, 5, -11), R('birthing_pools', 1060, -3385, 25, 25, 6, -12),
		R('ringing_room', 1050, -3470, 15, 15, 5, -12), R('reliquary', 1120, -3450, 10, 10, 4, -12), R('bell_pit', 980, -3520, 14, 14, 8, -13),
		R('weeping_gate', 0, 0, 8, 6, 5, -1),
	],
	lanes: [['mouth', 'drowned_lane'], ['chimney', 'drowned_lane'], ['drowned_lane', 'tithe_hall'], ['tithe_hall', 'library'], ['tithe_hall', 'gar_pool'], ['tithe_hall', 'bog_gallery'], ['bog_gallery', 'birthing_pools'], ['birthing_pools', 'ringing_room'], ['ringing_room', 'reliquary'], ['ringing_room', 'bell_pit'], ['bell_pit', 'weeping_gate'], ['cellar', 'ringing_room']],
};
{ const g = coven.rooms.find(r => r.id === 'weeping_gate'); g.x = arenas.vharra.gate[0] + 6; g.y = r1(arenas.vharra.gate[1] - 6); g.z = -1; }

// ---------------------------------------------------------------- boundary (the biome's west edge) and write
const boundary = [];
for (let y = Y0; y <= -1500; y += 20) boundary.push([y, r1(sample(topSm, y))]);
const out = {
	_comment: 'Generated by Tools/terrain/swamp.mjs (edit the script, not this file). The Swamp (the Mire), the east half of belt B3 (cooks/biome-swamp.md, cooks/build-biome-swamp.md 2.3): continent metres (+x east, +y south, origin the island centre; world cm = metres x 100), z = ground metres. frame: the landscape (2 m samples). boundary: [y, x] the biome\'s west edge (the Crown\'s east-face top). zones (32 m): 1 Reed Flats, 2 Mangrove Thickets, 3 Rotting Boardwalks, 4 Murk Pools, 5 Bone Mounds, f the frost border and the Rime Sound neck, ~ not the swamp. water (8 m): . dry, m murk, d duckweed, b bleedwater, q quag, k brackish, g a marsh-gas field. seaZ: the drawn water surface. routes: boardwalk pts are plank stations 2.4 m apart (deck height z); rotten / broken are plank indices. Read by FWRSwampData (WRSwampTypes.cpp).',
	frame: { x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, zScale: 100 * 128 / UNITS },
	seaZ: SEA_Z, boundary,
	zones: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
	water: { cell: WC, x0: X0, y0: Y0, width: WN, height: WN, rows: waterRows },
	camps: CAMPS.map(c => ({ x: r1(c.x), y: r1(c.y), z: r1(hAt(c.x, c.y)), yaw: 270 })),
	sites, shrines: SHRINES, kit,
	routes: ROUTES.map(r => ({ id: r.id, kind: r.kind, w: r.w, pts: (r.kind === 'boardwalk' ? r.dense : r.dense.filter((_, q) => q % 2 === 0 || q === r.dense.length - 1)).map(p => [r1(p[0]), r1(p[1]), +p[2].toFixed(2)]), ...(r.kind === 'boardwalk' ? { rotten: r.rotten, broken: r.broken } : {}) })),
	gas: GAS.map(g => ({ x: r1(g.x), y: r1(g.y), z: r1(g.z), r: g.r, vents: g.vents })),
	falls: FALLS, arenas, hexwalk, candle: CANDLE, coven,
	gut: GUT_MOUTH.x ? { mouth: P3(GUT_MOUTH.x, GUT_MOUTH.y, GUT_MOUTH.top), foot: P3(GUT_MOUTH.x + GUT_MOUTH.len, GUT_MOUTH.y) } : null,
	neck: NECK.map(([x, y]) => P3(x, y)),
};
fs.writeFileSync(path.join(root, 'Data', 'swamp.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- preview (8 m per pixel)
const PR = Math.floor((N - 1) * STEP / 8);
const img = Buffer.alloc(PR * PR * 3);
const ZCOL = { '1': [180, 154, 90], '2': [70, 96, 60], '3': [96, 100, 84], '4': [106, 110, 88], '5': [214, 205, 184], f: [205, 220, 230] };
const WCOL = { m: [22, 24, 26], d: [141, 176, 64], b: [110, 26, 22], k: [50, 70, 76], q: [110, 106, 96], g: [120, 140, 180] };
for (let j = 0; j < PR; j++) for (let i = 0; i < PR; i++) {
	const x = X0 + i * 8 + 4, y = Y0 + j * 8 + 4, h = hAt(x, y);
	const z = zoneAt(x, y);
	const wi = Math.floor((x - X0) / WC), wj = Math.floor((y - Y0) / WC);
	const wk = waterRows[wj] ? waterRows[wj][wi] : '.';
	let c;
	const sh = stormH(x, y);
	if (sh !== null && sh > h + 1) c = [90, 90, 100];
	else if (z === '~') c = h < SEA_Z ? [30, 70, 115] : [120, 120, 110];
	else if (wk !== '.' && WCOL[wk]) c = WCOL[wk];
	else c = ZCOL[z] || [100, 100, 100];
	const dzdx = (hAt(x + 8, y) - hAt(x - 8, y)) / 16, dzdy = (hAt(x, y + 8) - hAt(x, y - 8)) / 16;
	const shade = Math.max(0.4, Math.min(1.3, 0.95 + (-dzdx * 4 - dzdy * 4) * 0.8));
	const k = (j * PR + i) * 3;
	img[k] = Math.min(255, c[0] * shade); img[k + 1] = Math.min(255, c[1] * shade); img[k + 2] = Math.min(255, c[2] * shade);
}
const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / 8), cj = Math.round((y - Y0) / 8); for (let j = cj - r; j <= cj + r; j++) for (let i = ci - r; i <= ci + r; i++) if (i >= 0 && j >= 0 && i < PR && j < PR && (i - ci) ** 2 + (j - cj) ** 2 <= r * r) img.set(col, (j * PR + i) * 3); };
for (const r of ROUTES) for (const p of r.dense) dot(p[0], p[1], 0, r.kind === 'boardwalk' ? [230, 210, 160] : r.kind === 'bone' ? [255, 250, 235] : [200, 160, 90]);
for (const k of kit) dot(k.x, k.y, k.waymark ? 0 : 2, k.waymark ? [255, 255, 255] : [20, 20, 20]);
for (const s of sites) dot(s.x, s.y, s.landmark ? 5 : 3, s.landmark ? [122, 10, 20] : [255, 255, 255]);
for (const s of SHRINES) dot(s.x, s.y, 4, [255, 150, 40]);
for (const p of arenas.bloat.route) dot(p[0], p[1], 0, [255, 90, 160]);
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
fs.writeFileSync(path.join(root, 'Docs', 'terrain', 'swamp.png'), png(PR, PR, img));

if (!PREVIEW_ONLY) {
	const buf = Buffer.alloc(N * N * 2);
	for (let k = 0; k < N * N; k++) buf.writeUInt16LE(clamp(Math.round(32768 + HF[k] * UNITS), 0, 65535), k * 2);
	fs.writeFileSync(path.join(root, 'Art', 'Terrain', 'swamp_height.r16'), buf);
}
let land = 0; for (let j = 0; j < ZN; j++) for (let i = 0; i < ZN; i++) if (zoneRows[j][i] !== '~') land++;
const wTot = Object.values(wCount).reduce((a, b) => a + b, 0) - wCount.q - wCount.g;
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, swampKm2: +(land * ZC * ZC / 1e6).toFixed(2), joinedStrip: joined, hiddenUnderStorm: hidden, sites: sites.length, kit: kit.length, kitPois: kit.filter(k => !k.waymark).length, routeFill: filled, water: wCount, bleedShare: +(wCount.b / wTot).toFixed(3), duckweedShare: +(wCount.d / wTot).toFixed(3), bloatLoopM: arenas.bloat.length, hexwalk: hexwalk.length }));

// ---------------------------------------------------------------- dressing (Data/island_dressing.json)
// The decoration rule (DC 2.1-2.2, THE BAR: full and alive, never cluttered): bald cypress with Spanish moss thickest in
// the Murk Pools (in shallow water too, knees round them), red mangroves on every channel edge in M2, reed beds 2 m tall
// on the coast flats and at murk edges, pitcher plants on the bog domes, dead cypress and bone piles on the Bone Mounds,
// iced dead cypress on the frozen bog. Nothing on the routes (6 m clear), at the sites (25 m), in the camps (60 m) or
// the arenas. Kinds are the World track's (WRWorldDressing.cpp swamp rows).
if (!PREVIEW_ONLY) {
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
	const kinds = ['swamp_cypress', 'swamp_mangrove', 'swamp_deadcypress', 'swamp_reeds', 'swamp_pitcher', 'swamp_knees', 'swamp_bones', 'swamp_shrub'];
	const dout = Object.fromEntries(kinds.map(k => [k, []]));
	const clearOf = [...sites.map(s => [s.x, s.y, s.kind === 'camp' ? 60 : s.landmark ? 45 : 25]), ...SHRINES.map(s => [s.x, s.y, 30]), ...arenaKeep, ...kit.filter(k => !k.waymark).map(k => [k.x, k.y, 12])];
	const routeCells = new Set();
	for (const r of ROUTES) for (let q = 0; q < r.dense.length - 1; q++) {
		const [ax, ay] = r.dense[q], [bx, by] = r.dense[q + 1];
		const n = Math.ceil(Math.hypot(bx - ax, by - ay) / 2);
		for (let s = 0; s <= n; s++) { const x = lerp(ax, bx, s / n), y = lerp(ay, by, s / n); for (let dx = -2; dx <= 2; dx++) for (let dy = -2; dy <= 2; dy++) routeCells.add(`${Math.floor(x / 3) + dx},${Math.floor(y / 3) + dy}`); }
	}
	for (const [x, y] of CANDLE.pts) for (let dx = -3; dx <= 3; dx++) for (let dy = -3; dy <= 3; dy++) routeCells.add(`${Math.floor(x / 3) + dx},${Math.floor(y / 3) + dy}`);
	const free = (x, y) => !routeCells.has(`${Math.floor(x / 3)},${Math.floor(y / 3)}`) && clearOf.every(([cx, cy, r]) => Math.hypot(cx - x, cy - y) > r);
	const push = (kind, x, y, yaw, scale, sink = 0) => dout[kind].push(Math.round(x * 100), Math.round(y * 100), Math.round(hAt(x, y) * 100 - sink), Math.round(yaw), +scale.toFixed(2));
	let seed = 555; const Rn = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	for (let gy = Y0 + 10; gy < Y1 - 10; gy += 8) for (let gx = X0 + 10; gx < X1 - 10; gx += 8) {
		const x = gx + (Rn() - 0.5) * 7, y = gy + (Rn() - 0.5) * 7;
		const z = zoneAt(x, y);
		if (z === '~') continue;
		const h = hAt(x, y);
		if (h < -1.6 || !free(x, y)) continue;
		const wet = h < SEA_Z, roll = Rn();
		const canopy = 0.5 + 0.5 * vnoise(x / 70 + 3, y / 70);       // groves and clearings
		if (z === '4') {
			if (h > -1.0 && roll < 0.13 * canopy * 1.6) { push('swamp_cypress', x, y, Rn() * 360, 0.8 + Rn() * 0.6); if (Rn() < 0.7) push('swamp_knees', x + 2.5, y + 1.5, Rn() * 360, 0.7 + Rn() * 0.5); continue; }
			if (!wet && roll < 0.1) { push('swamp_shrub', x, y, Rn() * 360, 0.6 + Rn() * 0.5); continue; }
			if (wet && h > -0.4 && roll < 0.16) { push('swamp_reeds', x, y, Rn() * 360, 0.8 + Rn() * 0.4); continue; }
			if (roll < 0.13) push('swamp_knees', x, y, Rn() * 360, 0.6 + Rn() * 0.6);
		} else if (z === '2') {
			if (h > -1.5 && h < 2.2 && roll < 0.16 * canopy * 1.5) { push('swamp_mangrove', x, y, Rn() * 360, 0.7 + Rn() * 0.6); continue; }
			if (!wet && roll < 0.14) push('swamp_shrub', x, y, Rn() * 360, 0.6 + Rn() * 0.5);
		} else if (z === '1') {
			if (h > -0.6 && h < 1.6 && roll < 0.32) { push('swamp_reeds', x, y, Rn() * 360, 0.9 + Rn() * 0.5); continue; }
			if (!wet && roll < 0.36) { push('swamp_shrub', x, y, Rn() * 360, 0.5 + Rn() * 0.4); continue; }
			if (!wet && roll < 0.365) push('swamp_cypress', x, y, Rn() * 360, 0.6 + Rn() * 0.4);
		} else if (z === '3') {
			if (!wet && h > 2.6 && roll < 0.08) { push('swamp_pitcher', x, y, Rn() * 360, 0.7 + Rn() * 0.5); continue; }
			if (wet && h > -0.5 && roll < 0.16 && nearWater(x, y)) { push('swamp_reeds', x, y, Rn() * 360, 0.8 + Rn() * 0.4); continue; }
			if (roll < 0.03 * canopy) { push('swamp_cypress', x, y, Rn() * 360, 0.7 + Rn() * 0.5); continue; }
			if (roll < 0.04) { push('swamp_deadcypress', x, y, Rn() * 360, 0.7 + Rn() * 0.6); continue; }
			if (!wet && roll < 0.1) push('swamp_shrub', x, y, Rn() * 360, 0.5 + Rn() * 0.4);
		} else if (z === '5') {
			if (roll < 0.035) { push('swamp_deadcypress', x, y, Rn() * 360, 0.8 + Rn() * 0.7); continue; }
			if (!wet && roll < 0.08) { push('swamp_bones', x, y, Rn() * 360, 0.6 + Rn() * 0.8); continue; }
			if (wet && h > -0.4 && roll < 0.12) push('swamp_reeds', x, y, Rn() * 360, 0.7 + Rn() * 0.4);
		} else if (z === 'f') {
			if (roll < 0.02) { push('swamp_deadcypress', x, y, Rn() * 360, 0.6 + Rn() * 0.4); continue; }
			if (roll < 0.1) push('swamp_reeds', x, y, Rn() * 360, 0.6 + Rn() * 0.3);
		}
	}
	for (const k of kinds) dress.instances[k] = dout[k];
	dress.counts = dress.counts || {};
	for (const k of kinds) dress.counts[k] = dout[k].length / 5;
	dress.pois = (dress.pois || []).filter(p => !p.swamp);
	for (const s of sites) {
		const kind = s.kind === 'camp' ? 'camp' : s.kind === 'cave' ? 'cave' : s.landmark ? 'landmark' : null;
		if (!kind) continue;
		dress.pois.push({ name: s.name, kind, x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'swamp', swamp: true });
	}
	fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	console.error(`dressing: ${Object.entries(dout).map(([k, v]) => `${k} ${v.length / 5}`).join(', ')}`);
}
