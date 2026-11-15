// The Desert / Sun & sand (cooks/biome-desert.md, cooks/build-biome-desert.md): writes
//   Data/lands/desert.json + desert_ground.bin  the desert's ground as a runtime heightfield land (WRLand): the Wreck
//                                             Strand, the Erg's crescent dunes and the Mother Dune, the wadi and Oasis
//                                             Reach, the White Pan (the dry lake the river died in), the Red Canyons'
//                                             mesas and slot canyons, the Haboob Wall's scoured pavement, the camps
//   Data/desert.json                          everything standing on it (parsed by FWRDesertData, WRDesertTypes.cpp):
//                                             landmarks, sites, kit POIs, decoration, caves, the Hourhold, nodes, spots,
//                                             caches, hazards (quicksand, brine, the belt, flood wadis, dust devils),
//                                             arenas, routes, the tilted stelae, dig spots, portal slots
//   Data/island_dressing.json "pois"          the map's place list (camps, landmarks)
// Run: node Tools/terrain/desert.mjs   (prints the POI spacing check)
// Map metres: x east, y south (numbers grow south), z up; game cm = m x 100.
// THE COORDINATE RULE (build plan 2.1): the cook's coordinates are world-map's. This world puts the Desert between the
// Fungal plateau (its south strip, which this land cuts below y -7,800), the Storm landscape's north coast (y ~ -5,690)
// and the Ocean land's Spine (its west face from x ~ -300; this land cuts the Ocean's west sea). Every cook y goes
// through Y() (the Ocean's own squeeze, so Saltgate lines up) and every cook x through X() (the cook's Spine foot at
// -150 lands on this world's -300).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const X0 = -3104, Y0 = -7960, STEP = 4, N = 711, UNITS = 16;
const SEED = 6151;
const X = xd => -3000 + (xd + 3000) * 0.947;
const Y = yd => -7955 + (yd + 8000) * 0.8673;
const SEAM_N = -7800, SEAM_E = -280, SEAM_S = -5652;
const PAN = 26.0;                                  // the White Pan's level (m): the basin the old river died in

// ---------------------------------------------------------------- noise (deterministic)
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
const smax = (a, b, k) => { const h = clamp(0.5 + 0.5 * (a - b) / k, 0, 1); return lerp(b, a, h) + k * h * (1 - h); };
const smin = (a, b, k) => -smax(-a, -b, k);
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 61511;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
const r1 = v => +v.toFixed(1);
const C = hex => [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16)];
const distSegT = (px, py, ax, ay, bx, by) => { const dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy; const t = L ? clamp(((px - ax) * dx + (py - ay) * dy) / L, 0, 1) : 0; return { d: Math.hypot(px - ax - dx * t, py - ay - dy * t), t }; };
const distSeg = (px, py, ax, ay, bx, by) => distSegT(px, py, ax, ay, bx, by).d;
// Distance to a polyline and the place along it (its length from the start), plus the point's interpolated level.
function along(px, py, pts) {
	let best = { d: 1e9, s: 0, t: 0, at: 0 }, acc = 0;
	for (let s = 0; s < pts.length - 1; s++) {
		const [ax, ay] = pts[s], [bx, by] = pts[s + 1], L = Math.hypot(bx - ax, by - ay);
		const r = distSegT(px, py, ax, ay, bx, by);
		if (r.d < best.d) best = { d: r.d, s, t: r.t, at: acc + L * r.t };
		acc += L;
	}
	best.total = acc;
	return best;
}
const inPoly = (x, y, P) => { let c = false; for (let i = 0, j = P.length - 1; i < P.length; j = i++) { const [xi, yi] = P[i], [xj, yj] = P[j]; if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c; } return c; };

// ---------------------------------------------------------------- neighbours (read so the seams meet)
const readFungal = () => {
	try {
		const b = fs.readFileSync(path.join(root, 'Data', 'lands', 'fungal_ground.bin')); const n = 581;
		return (x, y) => { const i = Math.round((x + 2480) / 4), j = Math.round((y + 9800) / 4); if (i < 0 || j < 0 || i >= n || j >= n) return null; if (b[n * n * 2 + j * n + i] === 255) return null; return (b.readUInt16LE((j * n + i) * 2) - 32768) / 16; };
	} catch { return () => null; }
};
const fungalH = readFungal();
const readOcean = () => {
	try {
		const b = fs.readFileSync(path.join(root, 'Data', 'lands', 'ocean_ground.bin')); const n = 1001;
		const S = (i, j) => (b.readUInt16LE((clamp(j, 0, n - 1) * n + clamp(i, 0, n - 1)) * 2) - 32768) / 16;
		// Bilinear, as the land draws it (its samples are 5 m apart, ours 4).
		return (x, y) => { const fi = (x + 600) / 5, fj = (y + 7960) / 5, i = Math.floor(fi), j = Math.floor(fj), tx = fi - i, ty = fj - j; if (i < 0 || j < 0 || i >= n - 1 || j >= n - 1) return null; return lerp(lerp(S(i, j), S(i + 1, j), tx), lerp(S(i, j + 1), S(i + 1, j + 1), tx), ty); };
	} catch { return () => null; }
};
const oceanH = readOcean();
const readStorm = () => {
	try {
		const b = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16')); const n = 2017;
		return (x, y) => { const i = Math.round((x + 2900) / 2), j = Math.round((y + 5936) / 2); if (i < 0 || j < 0 || i >= n || j >= n) return null; return (b.readUInt16LE((j * n + i) * 2) - 32768) / 16; };
	} catch { return () => null; }
};
const stormH = readStorm();

// ---------------------------------------------------------------- the shape of the land (build plan 2.1)
// The west coast: from the Fungal plateau's sea cliffs (x ~ -2,290 at the north seam) out to the cook's -3,000 and back
// in to the Storm coast's west end (x ~ -2,505 at the south seam). Noise roughens it; two shallow bays hold the camps'
// ships.
const COASTX = y => {
	let c = interp([[-7800, -2296], [-7700, -2520], [-7580, -2770], [-7420, -2915], [-7000, -2972], [-6500, -2958], [-6100, -2948], [-5900, -2862], [-5760, -2660], [-5652, -2505]], y);
	c += 16 * fbm(y / 140, 3.1, 4) + 6 * fbm(y / 40, 1.7, 3);
	c += 26 * Math.exp(-(((y - Y(-6700)) / 60) ** 2));       // the Strand camp's bay
	c += 18 * Math.exp(-(((y - Y(-5900)) / 50) ** 2));       // the Caravan camp's cove
	return c;
};
// The general rise inland (m): the strand, the Erg's base, the Pan basin's rim, then the Spine's foot.
const RISE = [[-3000, 0], [-2700, 4], [-2400, 9], [-2100, 16], [-1800, 24], [-1500, 31], [-1300, 34], [-1180, 32], [-1000, 30], [-500, 30], [-280, 30]];
const E = x => interp(RISE, x);
// The Pan: the dry lake the river died in (D3's east, the cook's x -1,100..-150, y -7,450..-6,000), a flat at PAN.
const PAN_POLY = [[X(-1100), Y(-7420)], [X(-700), Y(-7445)], [X(-300), Y(-7430)], [X(-180), Y(-7200)], [X(-170), Y(-6700)], [X(-200), Y(-6150)], [X(-500), Y(-6040)], [X(-850), Y(-6060)], [X(-1080), Y(-6150)], [X(-1130), Y(-6500)], [X(-1150), Y(-7000)]];
// The main wadi: from the Pan's west shore down to the coast (the cook's "dry river from the coast at y -6,000 east to
// the Pan"), its bed falling from the Pan's level to the sea; past the Sevenfold Wells.
const WADI = [[X(-1120), Y(-6180)], [X(-1250), Y(-6230)], [X(-1450), Y(-6250)], [X(-1700), Y(-6230)], [X(-1950), Y(-6180)], [X(-2200), Y(-6150)], [X(-2450), Y(-6080)], [X(-2700), Y(-6010)], [X(-2950), Y(-5990)], [X(-3080), Y(-5985)]];
// The two flood wadis from the Storm rib's river gaps (x -2,050 and -1,350 at the seam): slot canyons through the mesas;
// the west one runs to the sea, the east one north into the main wadi.
const FLOOD = [
	{ id: 'west_gap', pts: [[-2050, SEAM_S], [-2060, -5760], [-2120, -5850], [-2260, -5930], [-2420, -5975], [-2600, -5992]], w: 10 },
	{ id: 'east_gap', pts: [[-1350, SEAM_S], [-1345, -5760], [-1320, -5880], [-1330, -6000], [-1360, -6110], [-1380, -6210]], w: 9 },
];
// Side slots in the Red Canyons (6-8 m wide, 14-30 m deep).
const SLOTS = [
	[[X(-1900), Y(-5550)], [X(-1820), Y(-5650)], [X(-1760), Y(-5760)], [X(-1700), Y(-5900)]],
	[[X(-1100), Y(-5420)], [X(-1060), Y(-5480)], [X(-1000), Y(-5600)], [X(-960), Y(-5760)]],
	[[X(-700), Y(-5480)], [X(-720), Y(-5600)], [X(-760), Y(-5760)], [X(-820), Y(-5900)]],
	[[X(-1600), Y(-5650)], [X(-1550), Y(-5800)], [X(-1460), Y(-5900)]],
];
// Saltgate's west mouth: the Ocean's pass floor at the seam (x -280), then a ramp climbing west-north-west onto the Pan
// north of the plaza (the plaza is 60 m south of the pass's line).
const SG_LINE = [[-280, -6797], [-360, -6818], [-450, -6852], [-540, -6884], [-640, -6905]];
const SG_FLOOR = [[0, 29], [90, 27], [190, 26.5], [380, PAN]];
// The Mother Dune (L4): a star dune 140 m tall, three razor arms.
const MOTHER = { x: X(-1450), y: Y(-7250), h: 140, arms: [20, 140, 260] };
// The plaza (2.14): a 140 m disc of limestone flattened into the Pan.
const PLAZA = { x: X(-600), y: Y(-6600), r: 70 };
// The Glass Canyon's mouth (L10) and its floor: a cleft into the Spine's foot.
const GLASS = { x: X(-350), y: Y(-5600) };

// ---------------------------------------------------------------- dunes (research: barchans, 11-degree backs, 32-degree slipfaces)
// A crescent field: one barchan per jittered 200 x 170 m cell, its crest bent so the horns point downwind (west); the
// back faces the east wind. The max of the 3x3 cells' shapes. Returns { h, lee } (lee: on a slipface).
function barchans(x, y, amp, cellX = 200, cellY = 170) {
	const ci = Math.floor(x / cellX), cj = Math.floor(y / cellY);
	let best = 0, lee = false;
	for (let a = -1; a <= 1; a++) for (let b = -1; b <= 1; b++) {
		const i = ci + a, j = cj + b;
		const cx = (i + 0.25 + 0.5 * hash(i * 7 + 1, j * 13 + 5)) * cellX, cy = (j + 0.2 + 0.6 * hash(i * 11 + 3, j * 5 + 9)) * cellY;
		const H = amp * (0.55 + 0.45 * hash(i * 3 + 17, j * 19 + 2));
		const W = 70 + 40 * hash(i * 23 + 4, j * 7 + 11);
		const ly = (y - cy) / W;
		if (Math.abs(ly) >= 1) continue;
		const lx = (x - cx) + 0.55 * W * ly * ly;
		const A = H * Math.pow(1 - ly * ly, 0.7);
		const z = lx < 0 ? A + lx * 0.62 : A - lx * 0.19;
		if (z > best) { best = z; lee = lx < 0 && z > 1.5; }
	}
	return { h: best, lee };
}
function motherDune(x, y) {
	const dx = x - MOTHER.x, dy = y - MOTHER.y, r = Math.hypot(dx, dy);
	let h = MOTHER.h - r * 0.42;                                        // the peak: a 23-degree cone
	let lee = false;
	for (const a of MOTHER.arms) {
		const ux = Math.cos(a * Math.PI / 180), uy = Math.sin(a * Math.PI / 180);
		const s = dx * ux + dy * uy, n = Math.abs(-dx * uy + dy * ux);
		if (s < 0 || s > 520) continue;
		const crest = MOTHER.h * (1 - s / 560) ** 1.3;
		const z = crest - n * 0.58;                                     // a razor arm, 30-degree sides
		if (z > h) { h = z; lee = n > 4; }
	}
	return { h: Math.max(0, h), lee: lee || r > 30 };
}

// ---------------------------------------------------------------- the ground
const zoneOf = (x, y) => {
	if (y < Y(-7450)) return x < X(-2200) ? 'D1' : 'D5';
	const coast = COASTX(y);
	if (x < Math.max(coast + 300, X(-2300) + 40 * fbm(y / 300, 4.4))) return 'D1';
	if (y > Y(-6000) + 30 * fbm(x / 260, 1.3)) return 'D4';
	if (inPoly(x, y, PAN_POLY) || x > X(-1100)) return 'D3';
	if (y > Y(-6200) + 25 * fbm(x / 200, 7.7)) return 'D3';           // Oasis Reach: the wadi's corridor
	return 'D2';
};
// Quicksand pockets (2.8): where water saturates sand (oasis margins, the wadi bed, the Mother Dune's foot), the Jaw.
const QUICK = [];
// Brine patches (2.8): D1's sabkha flats, 30 patches 6-15 m.
const BRINE = [];

function ground(x, y) {
	const coast = COASTX(y);
	const d = x - coast;                                   // inland distance from the west coast
	const z = zoneOf(x, y);
	if (d < 0) {
		// The sea off the strand: a beach shelf falling to 14 m.
		return { h: lerp(0.3, -14, Math.pow(sstep(0, 160, -d), 0.8)) + 0.4 * fbm(x / 40, y / 40), s: 'sea', z };
	}
	let h = E(x) * sstep(40, 300, d) + (0.3 + 2.7 * sstep(0, 95, d)) * (1 - sstep(200, 320, d));
	let s = 'zone';
	// D1: sea-grass dunes 4-12 m behind the beach; sabkha flats (salt) in the hollows between them.
	const dunes1 = (4 + 8 * Math.max(0, 0.5 + 0.5 * fbm(x / 70, y / 55, 4))) * sstep(90, 170, d) * (1 - sstep(520, 760, d));
	const sabkha = d > 160 && d < 650 && fbm(x / 210 + 3.3, y / 170, 4) < -0.12;
	if (sabkha) { h = Math.min(h, 1.4 + 0.2 * fbm(x / 12, y / 12, 2)); s = 'salt'; }
	else h += dunes1;
	if (d < 95) s = 'sand';
	// D2 and D5: crescents on the Erg base; the Mother Dune; D5's drifts smaller over scoured pavement.
	const ergK = sstep(X(-2300) - 60, X(-2300) + 140, x) * (1 - sstep(X(-1120), X(-1060), x));
	const d2 = (z === 'D2' || z === 'D3' && !inPoly(x, y, PAN_POLY)) ? 1 : 0;
	const southFade = 1 - sstep(Y(-6420), Y(-6250), y);                   // the dunes stop at the wadi's corridor
	let lee = false;
	if (y > Y(-7450) - 40) {
		const b = barchans(x, y, 26, 210, 175);
		const k = ergK * southFade * (z === 'D1' ? 0.25 : 1) * sstep(-0.1, 0.4, 0.5 + 0.5 * fbm(x / 600, y / 600, 2) + 0.35);
		h += b.h * k;
		if (b.lee && k > 0.5) lee = true;
	}
	const md = motherDune(x, y);
	if (md.h > 0.5) { const before = h; h = smax(h, E(x) + md.h, 6); if (h > before + 2 && md.lee) lee = true; }
	if (y < Y(-7450) + 40 && x > X(-2200) - 60) {
		// D5: scoured pavement rising north to the Fungal strip, drifts 3-8 m in lines across the wind.
		const rise = lerp(E(x) + 8, 52, sstep(Y(-7450), SEAM_N + 40, y));
		const drift = 5.5 * Math.max(0, fbm(x / 90, y / 40, 3)) * sstep(Y(-7450), Y(-7450) - 60, y);
		h = lerp(h, rise + drift, sstep(Y(-7450) + 40, Y(-7450) - 60, y));
		s = drift > 1.2 ? 'blown' : 'pavement';
		lee = false;
	}
	// The Pan: flat at PAN, its shore a gentle 5-10 m slope down from the rim; polygon cracks (3 cm) over it.
	const inPan = inPoly(x, y, PAN_POLY);
	{
		// Distance to the Pan's edge (approx, by polygon sampling).
		let dPan = 1e9; for (let i = 0; i < PAN_POLY.length; i++) { const a = PAN_POLY[i], b = PAN_POLY[(i + 1) % PAN_POLY.length]; dPan = Math.min(dPan, distSeg(x, y, a[0], a[1], b[0], b[1])); }
		const t = inPan ? 1 - sstep(0, 70, dPan) * 0 : 1 - sstep(0, 120, dPan);
		if (inPan) {
			const cell = 1.4, u = x / cell, v = y / cell;
			const fx = u - Math.floor(u), fy = v - Math.floor(v);
			const crack = Math.min(fx, 1 - fx, fy, 1 - fy) < 0.06 + 0.04 * hash(Math.floor(u), Math.floor(v)) ? 0.03 : 0;
			const shore = sstep(0, 90, dPan);
			h = lerp(h, PAN + 0.25 * fbm(x / 80, y / 80, 3) - crack, shore);
			s = shore > 0.6 ? 'hardpan' : 'gravel';
			lee = false;
		} else if (t > 0) h = lerp(h, Math.max(PAN + 0.5, Math.min(h, PAN + 0.5 + dPan * 0.12)), t * sstep(PAN + 1, PAN + 20, h) * 0.85);
	}
	// D4: the Red Canyons' mesas (flat tops 60-95 m with sheer edges), rising from the wadi corridor; slot canyons cut in.
	if (z === 'D4' || (y > Y(-6000) - 80 && x > X(-2350))) {
		const k = sstep(Y(-6060), Y(-5960), y) * sstep(X(-2400), X(-2250), x);
		const m = fbm(x / 240 + 9, y / 240, 4);
		const top = E(x) + 32 + 22 * sstep(-0.6, 0.6, fbm(x / 700, y / 700, 2)) + 12 * sstep(0.0, 0.25, m);      // two mesa levels
		const mesa = lerp(E(x) + 6, top, sstep(-0.18, -0.05, m));
		h = lerp(h, mesa + 0.6 * fbm(x / 9, y / 9, 2), k);
		if (k > 0.5) { s = 'redrock'; lee = false; }
	}
	// The Spine's foot (east seam): a concave foot up to the Ocean land's own heights at x -280.
	if (x > -520) {
		const o = oceanH(SEAM_E, y);
		if (o !== null) {
			const t = sstep(-520, SEAM_E, x);
			const target = lerp(h, o, Math.pow(t, 1.6));
			h = target + 4 * fbm(x / 30, y / 30, 3) * t * (1 - t) * 4;
			if (t > 0.45 && (s === 'zone' || s === 'hardpan')) s = 'spine';
		}
	}
	// The Glass Canyon (L10): a cleft 30 m wide into the Spine's foot, its floor and walls fused glass.
	{
		const dx = x - GLASS.x, dy = y - GLASS.y;
		const along0 = -dx, lat = Math.abs(dy + 0.15 * dx);
		if (along0 > -180 && along0 < 220 && lat < 40) {
			const floor = E(x) + 6;
			const depthK = 1 - sstep(14, 40, lat);
			h = lerp(h, Math.min(h, floor + 1.5 * fbm(x / 12, y / 12, 2)), depthK * sstep(-180, -120, along0) * (1 - sstep(170, 220, along0)));
			if (depthK > 0.4) { s = 'glass'; lee = false; }
		}
	}
	// The main wadi: a gravel bed 3.5 m under its banks, 30-60 m wide, falling from the Pan to the sea.
	{
		const a = along(x, y, WADI);
		const w = lerp(18, 30, a.at / a.total);
		if (a.d < w + 40) {
			const bed = lerp(PAN - 0.6, 0.3, Math.pow(a.at / a.total, 0.9)) + 0.4 * fbm(x / 25, y / 25, 2);
			const bank = sstep(w, w + 40, a.d);
			h = lerp(Math.min(h, bed), h, bank);
			if (a.d < w) s = 'gravel';
			lee = false;
		}
	}
	// The flood wadis: slot canyons 9-10 m wide, their floors from the Storm seam's level down to the sea / the main wadi.
	for (const f of FLOOD) {
		const a = along(x, y, f.pts);
		if (a.d < f.w + 16) {
			const start = stormH(f.pts[0][0], SEAM_S) ?? 20;
			const endAt = f.id === 'west_gap' ? 0.5 : (along(f.pts[f.pts.length - 1][0], f.pts[f.pts.length - 1][1], WADI).at / along(0, 0, WADI).total);
			const endH = f.id === 'west_gap' ? 0.8 : lerp(PAN - 0.6, 0.3, Math.pow(endAt, 0.9)) - 0.4;
			const floor = lerp(Math.max(start, endH + 1), endH, a.at / a.total);
			const wall = sstep(f.w * 0.5, f.w * 0.5 + 6, a.d);
			h = lerp(Math.min(h, floor), h, wall);
			if (a.d < f.w * 0.5 + 1) { s = 'slot'; lee = false; }
		}
	}
	for (const sl of SLOTS) {
		const a = along(x, y, sl);
		if (a.d < 12) {
			const floor = h - lerp(14, 26, 0.5 + 0.5 * fbm(x / 80, y / 80, 2));
			h = lerp(floor, h, sstep(3, 9, a.d));
			if (a.d < 4) s = 'slot';
		}
	}
	// Saltgate's west mouth: the pass's floor at the seam, a ramp up onto the Pan north of the plaza.
	{
		const a = along(x, y, SG_LINE);
		if (a.d < 90) {
			const floor = interp(SG_FLOOR, a.at);
			const half = lerp(55, 32, a.at / a.total);
			h = lerp(floor + 0.4 * fbm(x / 20, y / 20, 2), h, sstep(half, half + 30, a.d) * sstep(0, 25, a.at) + (1 - sstep(0, 25, a.at)) * sstep(half, half + 30, a.d));
			if (a.d < half - 6) s = 'road';
		}
	}
	// The plaza: a 140 m disc of limestone at the Pan's level.
	{
		const r = Math.hypot(x - PLAZA.x, y - PLAZA.y);
		if (r < PLAZA.r + 30) { h = lerp(PAN + 0.35, h, sstep(PLAZA.r, PLAZA.r + 30, r)); if (r < PLAZA.r) { s = 'plaza'; lee = false; } }
	}
	if (s === 'zone') s = z === 'D1' ? (d < 95 ? 'sand' : 'grass') : z === 'D2' ? 'softsand' : z === 'D3' ? 'oasis' : z === 'D4' ? 'redrock' : 'pavement';
	if (s === 'oasis' && !inPan) s = h > PAN + 4 && sstep(0, 1, 0.5 + 0.5 * fbm(x / 120, y / 120, 3)) > 0.55 ? 'softsand' : 'oasis';
	if (lee && (s === 'softsand' || s === 'grass' || s === 'oasis')) s = 'slipface';
	// The shore band: wet sand below +1.2 m.
	if (h < 1.2 && d >= 0 && s !== 'salt') s = 'wetsand';
	return { h, s, z, d };
}

const SURF = { zone: 0, sand: 1, wetsand: 2, salt: 3, brine: 4, grass: 5, softsand: 6, slipface: 7, shellgrit: 8, quicksand: 9, oasis: 10, gravel: 11, hardpan: 12, redrock: 13, slot: 14, glass: 15, pavement: 16, blown: 17, road: 18, nitre: 19, plaza: 20, sea: 21, spine: 22, hidden: 255 };
const HF = new Float32Array(N * N), SURFM = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
	// Not ours: north of the Fungal seam, east of the Ocean seam, south of the Storm seam.
	if (y < SEAM_N || x > SEAM_E || y > SEAM_S) { HF[k] = -14; SURFM[k] = SURF.hidden; continue; }
	const g = ground(x, y);
	let h = g.h;
	// The north seam: the Fungal land's own heights on the row at -7,800, blended in over 140 m (its sea / artefact
	// columns ignored: ours stands there).
	const fN = fungalH(x, SEAM_N);
	if (fN !== null && fN > 12) h = lerp(h, fN, Math.pow(sstep(SEAM_N + 140, SEAM_N, y), 1.4));
	// The south seam: the Storm land's own heights on the row at -5,652 (its north coast flat below the rib), over 170 m.
	const sS = stormH(x, SEAM_S);
	if (sS !== null) h = lerp(h, sS, Math.pow(sstep(SEAM_S - 170, SEAM_S, y), 1.3));
	if (y === SEAM_S && sS !== null) h = sS;
	if (y === SEAM_N && fN !== null && fN > 12) h = fN;
	if (x === SEAM_E) { const o = oceanH(SEAM_E, y); if (o !== null) h = o; }
	HF[k] = h;
	SURFM[k] = SURF[g.s] ?? SURF.zone;
	if (h < -0.4 && g.d < 0) SURFM[k] = SURF.sea;
}
const hAt = (x, y) => {
	const fi = (x - X0) / STEP, fj = (y - Y0) / STEP, i = clamp(Math.floor(fi), 0, N - 2), j = clamp(Math.floor(fj), 0, N - 2);
	const tx = clamp(fi - i, 0, 1), ty = clamp(fj - j, 0, 1);
	const a = HF[j * N + i], b = HF[j * N + i + 1], c = HF[(j + 1) * N + i], d = HF[(j + 1) * N + i + 1];
	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
};
const surfAt = (x, y) => SURFM[clamp(Math.round((y - Y0) / STEP), 0, N - 1) * N + clamp(Math.round((x - X0) / STEP), 0, N - 1)];
const paint = (cx, cy, r, cls, dz = 0, keep = null) => {
	for (let j = Math.floor((cy - r - Y0) / STEP); j <= Math.ceil((cy + r - Y0) / STEP); j++) for (let i = Math.floor((cx - r - X0) / STEP); i <= Math.ceil((cx + r - X0) / STEP); i++) {
		if (i < 0 || j < 0 || i >= N || j >= N) continue;
		const k = j * N + i, x = X0 + i * STEP, y = Y0 + j * STEP, dd = Math.hypot(x - cx, y - cy);
		if (dd > r || SURFM[k] === SURF.hidden) continue;
		if (keep && !keep.includes(SURFM[k])) continue;
		SURFM[k] = cls;
		if (dz) HF[k] += dz * (1 - sstep(r * 0.5, r, dd));
	}
};

// ---------------------------------------------------------------- the places (cook 2.3, 2.4, 2.5, 2.6, 2.11)
const at = (xd, yd, extra = {}) => ({ x: X(xd), y: Y(yd), ...extra });
const place = (o) => { o.z = o.z ?? r1(hAt(o.x, o.y)); o.x = r1(o.x); o.y = r1(o.y); return o; };
const WHALE_ROAD = [[X(-1850), Y(-7000)], [X(-1250), Y(-6800)]];
const landmarks = [
	{ id: 'haboob_wall', kind: 'haboob_wall', name: 'the Haboob Wall', x: X(-1200), y: Y(-7450), yaw: 0, size: 300 },
	{ id: 'gnomon', kind: 'gnomon', name: "Ahrumet's Gnomon", ...at(-600, -6600), yaw: 0, size: 30 },
	{ id: 'sun_gate', kind: 'sun_gate', name: 'the Broken Sun Gate', ...at(-1700, -6050), yaw: 90 },
	{ id: 'mother_dune', kind: 'mother_dune', name: 'the Mother Dune', x: MOTHER.x, y: MOTHER.y, yaw: 0, size: 140 },
	{ id: 'whale_road', kind: 'whale_road', name: 'the Whale Road', x: WHALE_ROAD[0][0], y: WHALE_ROAD[0][1], yaw: 0 },
	{ id: 'keelyard', kind: 'keelyard', name: 'the Keelyard', ...at(-2050, -6450), yaw: 20 },
	{ id: 'tall_wreck', kind: 'tall_wreck', name: 'the Tall Wreck', ...at(-2700, -6300), yaw: 200 },
	{ id: 'fallen_hour', kind: 'fallen_hour', name: 'the Fallen Hour', ...at(-900, -7200), yaw: 250 },
	{ id: 'needles', kind: 'needles', name: 'the Needles', ...at(-1100, -5650), yaw: 0 },
	{ id: 'glass_canyon', kind: 'glass_canyon', name: 'the Glass Canyon', x: GLASS.x, y: GLASS.y, yaw: 180 },
	{ id: 'sevenfold_wells', kind: 'sevenfold_wells', name: 'the Sevenfold Wells', ...at(-1250, -6250), yaw: 0 },
	{ id: 'leviathan_ribs', kind: 'leviathan_ribs', name: 'the Leviathan Ribs', x: -300, y: -6797, yaw: 200 },
].map(place);

const sites = [
	// D1 the Wreck Strand
	{ id: 'S1', kind: 'tall_wreck_site', name: 'the Tall Wreck', zone: 'D1', ...at(-2700, -6300), yaw: 200 },
	{ id: 'S2', kind: 'salt_cutters', name: "the Salt-cutters' camp", zone: 'D1', ...at(-2600, -7100), yaw: 60 },
	{ id: 'S3', kind: 'signal_fire', name: 'the Signal Fire', zone: 'D1', ...at(-2800, -5700), yaw: 0 },
	{ id: 'S4', kind: 'brine_pit', name: 'the Brine-pit drowned', zone: 'D1', ...at(-2400, -6950), yaw: 140 },
	{ id: 'S5', kind: 'caravan_oasis', name: 'the Caravan Oasis', zone: 'D1', ...at(-2500, -5550), yaw: 90 },
	{ id: 'S6', kind: 'shell_shrine', name: 'the sea-grass shrine', zone: 'D1', ...at(-2850, -7300), yaw: 270 },
	// D2 the Erg
	{ id: 'S7', kind: 'ship_in_dune', name: 'the Ship in the Dune', zone: 'D2', ...at(-1650, -7150), yaw: 250 },
	{ id: 'S8', kind: 'jackal_tomb', name: 'the Jackal tomb', zone: 'D2', ...at(-1900, -6700), yaw: 90 },
	{ id: 'S9', kind: 'swallowed_caravan', name: 'the Swallowed Caravan', zone: 'D2', ...at(-1300, -7000), yaw: 100 },
	{ id: 'S10', kind: 'navigators_stones', name: "the Navigator's Stones", zone: 'D2', ...at(-2150, -7300), yaw: 0 },
	{ id: 'S11', kind: 'stelae_avenue', name: 'the dune-buried stelae', zone: 'D2', ...at(-1100, -6550), yaw: 90 },
	{ id: 'S12', kind: 'kite_feast', name: 'the Kite feast', zone: 'D2', ...at(-2250, -6550), yaw: 0 },
	{ id: 'S13', kind: 'lost_expedition', name: 'the Lost Expedition', zone: 'D2', ...at(-1500, -6350), yaw: 0 },
	{ id: 'S14', kind: 'booming_slope', name: 'the Booming Slope', zone: 'D2', ...at(-1750, -7400), yaw: 270 },
	// D3 Oasis Reach and the White Pan
	{ id: 'S15', kind: 'waystation', name: 'the Waystation of the First Hour', zone: 'D3', ...at(-2200, -6050), yaw: 90 },
	{ id: 'S16', kind: 'qanat_line', name: 'the Qanat Line', zone: 'D3', ...at(-1950, -5950), yaw: 0 },
	{ id: 'S17', kind: 'gardeners_row', name: "the Gardener's Last Row", zone: 'D3', ...at(-1400, -6100), yaw: 0 },
	{ id: 'S18', kind: 'measuring_wall', name: 'the Measuring Wall', zone: 'D3', ...at(-900, -6050), yaw: 0 },
	{ id: 'S19', kind: 'pilgrims_ring', name: "the Pilgrims' Ring", zone: 'D3', ...at(-1050, -6800), yaw: 0 },
	{ id: 'S20', kind: 'salt_cathedral', name: 'the Salt cathedral', zone: 'D3', ...at(-400, -7000), yaw: 0 },
	{ id: 'S21', kind: 'avenue_of_hours', name: 'the Avenue of Hours', zone: 'D3', ...at(-750, -6350), yaw: 0 },
	{ id: 'S22', kind: 'sunken_greenhouse', name: 'the Sunken Greenhouse', zone: 'D3', ...at(-1600, -6250), yaw: 0 },
	{ id: 'S23', kind: 'cartographers_table', name: "the Cartographer's Table", zone: 'D3', ...at(-300, -6300), yaw: 270 },
	// D4 the Red Canyons
	{ id: 'S24', kind: 'nitre_canyon', name: 'the Nitre Canyon', zone: 'D4', ...at(-1800, -5600), yaw: 0 },
	{ id: 'S25', kind: 'hands_wall', name: 'the Hands Wall', zone: 'D4', ...at(-1300, -5500), yaw: 0 },
	{ id: 'S26', kind: 'copper_drift', name: 'the Copper Drift', zone: 'D4', ...at(-700, -5750), yaw: 0 },
	{ id: 'S27', kind: 'flood_line', name: 'the Flood Line', zone: 'D4', ...at(-1050, -5450), yaw: 0 },
	{ id: 'S28', kind: 'kite_cliffs', name: 'the Kite Cliffs', zone: 'D4', ...at(-500, -5450), yaw: 0 },
	{ id: 'S29', kind: 'lookout', name: 'the Lookout', zone: 'D4', ...at(-1600, -5750), yaw: 0 },
	// D5 the Haboob Wall
	{ id: 'S30', kind: 'last_caravan', name: 'the Last Caravan', zone: 'D5', ...at(-1900, -7700), yaw: 0 },
	{ id: 'S31', kind: 'sieves', name: 'the Sieves', zone: 'D5', x: -1200, y: -7790, yaw: 0 },
	{ id: 'S32', kind: 'storm_tower', name: 'the Storm Tower', zone: 'D5', ...at(-600, -7700), yaw: 90 },
	{ id: 'S33', kind: 'glass_field', name: 'the Glass Field', zone: 'D5', ...at(-1600, -7600), yaw: 0 },
	{ id: 'S34', kind: 'faceless_sentinels', name: 'the Faceless Sentinels', zone: 'D5', x: X(-400), y: -7760, yaw: 0 },
	{ id: 'S35', kind: 'storm_wreck', name: 'the Storm Wreck', zone: 'D5', ...at(-2100, -7800), yaw: 30 },
	// Shrines (cook 2.11; the shrine rows in world.json stand on these)
	{ id: 'shrine_hours', kind: 'shrine_great', name: 'the Great Shrine of the Hours', zone: 'D3', ...at(-1700, -6050), yaw: 90 },
	{ id: 'shrine_strand', kind: 'shrine', name: 'the Strand Shrine', zone: 'D1', ...at(-2650, -6900) },
	{ id: 'shrine_erg', kind: 'shrine_skull', name: 'the Erg Shrine', zone: 'D2', ...at(-1850, -6850) },
	{ id: 'shrine_canyon', kind: 'shrine_spire', name: 'the Canyon Shrine', zone: 'D4', ...at(-1250, -5600) },
	{ id: 'shrine_wall', kind: 'shrine', name: 'the Wall Shrine', zone: 'D5', ...at(-1500, -7650) },
	{ id: 'shrine_saltgate', kind: 'shrine', name: 'the Saltgate Mouth Shrine', zone: 'D3', x: -360, y: -6760 },
	// Amastan's shade, the plaza's run-back stone and meridian strip, the Hourhold's three ways in
	{ id: 'amastan_shade', kind: 'amastan_shade', name: "Amastan's shade", zone: 'D3', x: X(-1700) + 14, y: Y(-6050) + 16, yaw: 90 },
	{ id: 'plaza_runback', kind: 'runback', name: "the Sundial Plaza's run-back stone", zone: 'D3', x: PLAZA.x - 85, y: PLAZA.y + 85 },
	// Camps (KEPT coordinates): a palm-frond windbreak, a fire ring, the newcomers' ship on the beach below
	{ id: 'camp_caravan', kind: 'camp', name: 'the Caravan camp', zone: 'D1', camp: true, ...at(-2550, -5900), yaw: 270 },
	{ id: 'camp_strand', kind: 'camp', name: 'the Strand camp', zone: 'D1', camp: true, ...at(-2550, -6700), yaw: 270 },
	{ id: 'camp_wall', kind: 'camp', name: 'the Wall camp', zone: 'D1', camp: true, ...at(-2350, -7600), yaw: 270 },
].map(place);
const S = id => sites.find(q => q.id === id);
// Camps sit on a flat dune top 6-11 m up: the nearest flat ground in rings outward.
for (const s of sites.filter(s => s.camp)) {
	let best = null;
	for (let r = 0; r < 260 && !best; r += 4) for (let a = 0; a < 360 && !best; a += 10) {
		const x = s.x + Math.cos(a * Math.PI / 180) * r, y = s.y + Math.sin(a * Math.PI / 180) * r, h = hAt(x, y);
		const flat = Math.abs(hAt(x + 7, y) - hAt(x - 7, y)) + Math.abs(hAt(x, y + 7) - hAt(x, y - 7)) < 1.4;
		if (h >= 5 && h <= 14 && flat && along(x, y, WADI).d > 60) best = [x, y];
	}
	if (best) [s.x, s.y] = [r1(best[0]), r1(best[1])];
	s.z = r1(hAt(s.x, s.y));
	// Its newcomer ship on the beach due west (world-map: every camp has its ship).
	let bx = s.x; while (bx > COASTX(s.y) + 30 && hAt(bx, s.y) > 2.2) bx -= 4;
	s.ship = [r1(bx), r1(s.y + 12), r1(hAt(bx, s.y + 12))];
}
// The Sieves are the Fungal land's ground (the sinkholes into its caverns): their rim heights come from it.
{ const sv = S('S31'); const f = fungalH(sv.x, sv.y); if (f !== null) sv.z = r1(f); }
{ const fs2 = S('S34'); const f = fungalH(fs2.x, fs2.y); if (f !== null && fs2.y < SEAM_N) fs2.z = r1(f); }

// ---------------------------------------------------------------- caves (cook 2.6) and the Hourhold (2.5)
const caves = [
	{ id: 'nitre_caves', name: 'the Nitre Caves', zone: 'D4', ...at(-1800, -5600), roof: 6, rooms: [[-24, 6, 10], [-46, 18, 12], [-70, 10, 9], [-86, -10, 11]], mouth: { dx: 0, dy: 0, yaw: 180, w: 6, h: 5 }, depth: -18, ore: ['copper_ore', 'tin_ore', 'nitre', 'copper_ore', 'nitre', 'bat_guano'], creatures: { glasstail: 2 } },
	{ id: 'qanat', name: 'the Qanat', zone: 'D3', ...at(-1950, -5950), roof: 3.5, rooms: [[30, 0, 5], [60, -2, 5], [90, 2, 6], [130, 0, 8]], mouth: { dx: 0, dy: 0, yaw: 0, w: 2.5, h: 3 }, depth: -14, flooded: [[45, 0, 100, 0]], ore: ['copper_ore', 'clay', 'copper_ore'], creatures: { wellfrog: 3, glasstail: 1 }, shaft: true },
	{ id: 'ssarroks_hollow', name: "Ssarrok's Hollow", zone: 'D2', x: MOTHER.x + 130, y: MOTHER.y + 150, roof: 14, rooms: [[0, 0, 24], [30, 22, 14], [-24, 30, 12]], mouth: { dx: 26, dy: 0, yaw: 0, w: 14, h: 9 }, depth: -12, sandFloor: true, ore: ['sunglass', 'gold_flakes', 'sunglass', 'gold_flakes'], creatures: { dunejaw: 2 }, arena: 'ssarrok' },
	{ id: 'sieve_throats', name: 'the Sieve Throats', zone: 'D5', ...at(-1150, -7700), roof: 7, rooms: [[0, 0, 10], [-20, -16, 9], [-36, -34, 12], [-44, -60, 10]], mouth: { dx: 0, dy: 0, yaw: 90, w: 7, h: 6 }, depth: -40, ore: ['iron_ore', 'sunglass', 'iron_ore', 'deepcap'], creatures: { glasstail: 3 } },
].map(c => { c.x = r1(c.x); c.y = r1(c.y); c.z = r1(hAt(c.x, c.y)); c.floor = r1(c.z + c.depth); return c; });

// The Hourhold: the Sunwatch's temple city under the Erg (centre KEPT: the cook's (-1,500, -7,000)), its rooms 20-70 m
// below the dunes, laid along its buried processional street from the Lee Door (north, under the Mother Dune's lee) to
// the Spring (deepest). Room rows: id, name, dx, dy (m from the centre), floor z (m), w, d (m), h (m), yaw.
const HH = { x: r1(X(-1500)), y: r1(Y(-7000)) };
const HSURF = hAt(HH.x, HH.y);
const hh = (id, name, dx, dy, z, w, d, h, extra = {}) => ({ id, name, dx, dy, z, w, d, h, ...extra });
const hourhold = {
	at: [HH.x, HH.y], surface: r1(HSURF),
	rooms: [
		hh('street', 'the Sand-choked street', 0, -150, 4, 14, 70, 12, { sandRamp: true }),
		hh('hall_of_hours', 'the Hall of Hours', 0, -60, -2, 24, 60, 14, { pillars: 24, shafts: 6 }),
		hh('scribes', "the Scribes' rooms", -26, -70, -2, 14, 22, 5),
		hh('granary', 'the Granary', 26, -80, -2, 14, 18, 6),
		hh('cistern', 'the Cistern Below', 0, 10, -14, 36, 36, 12, { water: -10.5, pillars: 16, well: [8, 8] }),
		hh('copper_deep', 'the Copper Deep', -40, 20, -10, 14, 20, 6),
		hh('iron_deep', 'the Iron Deep', -60, 60, -36, 12, 18, 6),
		hh('barque_dock', 'the Sun Barque Dock', 44, 10, -12, 18, 40, 12),
		hh('ushabti_hall', 'the Ushabti Hall', 0, 60, -16, 26, 34, 8, { ushabti: 12 }),
		hh('weighing_room', 'the Weighing Room', -32, 60, -16, 14, 14, 7),
		hh('glasstail_nest', 'the Glasstail nest', 34, 66, -18, 16, 16, 6),
		hh('kennels', 'the Jackal kennels', 34, -30, -6, 14, 12, 5),
		hh('mirror_gallery', 'the Mirror Gallery', 0, 104, -20, 30, 14, 8, { mirrors: 4 }),
		hh('sun_vault', 'the Sun-Vault', 0, 130, -20, 12, 12, 6, { sealed: 'gallery' }),
		hh('drowned_stair', 'the Drowned Stair', -36, 104, -26, 10, 24, 8, { quicksand: true }),
		hh('seventh_vault', 'the Seventh Tilted Vault', -36, 134, -32, 10, 10, 6, { sealed: 'long_noon' }),
		hh('collapse', 'the Collapse', 54, 110, -34, 26, 26, 30, { rope: true }),
		hh('faces', 'the Workshop of Faces', -40, -30, -6, 16, 18, 8),
		hh('lens_forge', 'the Lens Forge', -46, 0, -8, 14, 14, 7),
		hh('spring', 'the Spring', 54, 150, -42, 18, 18, 9, { spring: true }),
	],
	// The buried ways in (cook 2.5), each a sloping gallery from its mouth to a room.
	ways: [],
};
for (const r of hourhold.rooms) { r.z = r1(HSURF + r.z - 18); if (r.water !== undefined) r.water = r1(r.z + 3.5); }
const hRoom = id => hourhold.rooms.find(r => r.id === id);
// Corridors between rooms: [a, b] (the scenery runs a 4 m gallery between their nearest walls).
hourhold.links = [['street', 'hall_of_hours'], ['hall_of_hours', 'scribes'], ['hall_of_hours', 'granary'], ['hall_of_hours', 'faces'], ['faces', 'lens_forge'], ['hall_of_hours', 'kennels'],
	['hall_of_hours', 'cistern'], ['cistern', 'copper_deep'], ['cistern', 'barque_dock'], ['cistern', 'ushabti_hall'], ['ushabti_hall', 'weighing_room'], ['ushabti_hall', 'glasstail_nest'],
	['ushabti_hall', 'mirror_gallery'], ['mirror_gallery', 'sun_vault'], ['mirror_gallery', 'drowned_stair'], ['drowned_stair', 'seventh_vault'], ['mirror_gallery', 'collapse'], ['collapse', 'spring'],
	['copper_deep', 'iron_deep']];
// The ways in: H-E1 the Lee Door (the Mother Dune's lee foot), H-E2 the Jaw (under the Fallen Hour's chin), H-E3 the
// Dig (the Lost Expedition's pit). Each a gallery sloping down from its mouth to its room.
{
	const lee = { x: MOTHER.x - 150, y: MOTHER.y + 40 };
	const fh = landmarks.find(l => l.id === 'fallen_hour');
	const s13 = S('S13');
	const way = (id, kind, mx, my, to, opens) => { const r = hRoom(to); hourhold.ways.push({ id, kind, from: [r1(mx), r1(my), r1(hAt(mx, my))], to, toAt: [r1(HH.x + r.dx), r1(HH.y + r.dy), r.z], opens }); };
	way('H-E1', 'lee_door', lee.x, lee.y, 'street', 'after_sandstorm');
	way('H-E2', 'jaw', fh.x - 14, fh.y + 6, 'kennels', 'always');
	way('H-E3', 'dig', s13.x, s13.y - 8, 'barque_dock', 'dig');
}

// ---------------------------------------------------------------- hazards (cook 2.8)
// Quicksand: at oasis margins, the wadi bed, the Mother Dune's foot, before the Jaw.
{
	const add = (x, y, r) => { QUICK.push([r1(x), r1(y), r1(hAt(x, y) - 0.25), r]); paint(x, y, r, SURF.quicksand, -0.25); };
	for (let k = 0; k < 5; k++) { const a = (200 + k * 28) * Math.PI / 180, rr = 150 + 30 * rnd(); add(MOTHER.x + Math.cos(a) * rr, MOTHER.y + Math.sin(a) * rr, 5 + 3 * rnd()); }
	for (let k = 0; k < 6; k++) { const t = 0.15 + k * 0.13; const s2 = Math.floor(t * (WADI.length - 1)); const f = t * (WADI.length - 1) - s2; add(lerp(WADI[s2][0], WADI[s2 + 1][0], f), lerp(WADI[s2][1], WADI[s2 + 1][1], f) + (rnd() - 0.5) * 12, 4 + 3 * rnd()); }
	const wl = landmarks.find(l => l.id === 'sevenfold_wells');
	for (let k = 0; k < 4; k++) { const a = (k * 90 + 30) * Math.PI / 180; add(wl.x + Math.cos(a) * 52, wl.y + Math.sin(a) * 52, 4 + 2 * rnd()); }
	const fh = landmarks.find(l => l.id === 'fallen_hour'); add(fh.x - 8, fh.y + 4, 6);
}
// Brine crust patches on the sabkha (30).
{
	let tries = 0;
	while (BRINE.length < 30 && tries++ < 20000) {
		const x = lerp(-2950, -2300, rnd()), y = lerp(-7500, -5750, rnd());
		if (surfAt(x, y) !== SURF.salt) continue;
		if (BRINE.some(b => Math.hypot(b[0] - x, b[1] - y) < 60)) continue;
		const r = 3 + rnd() * 4.5;
		let ok = true; for (let a = 0; a < 6.28; a += 0.8) if (surfAt(x + Math.cos(a) * r, y + Math.sin(a) * r) !== SURF.salt) ok = false;
		if (!ok) continue;
		BRINE.push([r1(x), r1(y), r1(hAt(x, y)), r1(r)]);
		paint(x, y, r, SURF.brine);
	}
}
// The belt (D5): its south line is the Wall's face; it runs north over the Fungal strip to the plateau's rise.
const belt = { south: r1(Y(-7450)), north: -7980, west: r1(X(-2200)), east: -150 };
// Flood wadis (their splines in world metres, with the ledges every 40 m) and dust devils' Pan.
const floods = FLOOD.map(f => {
	const pts = f.pts.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]);
	const ledges = [];
	let acc = 0, next = 30;
	for (let s = 0; s < f.pts.length - 1; s++) {
		const [ax, ay] = f.pts[s], [bx, by] = f.pts[s + 1], L = Math.hypot(bx - ax, by - ay);
		while (next < acc + L) {
			const t = (next - acc) / L, x = lerp(ax, bx, t), y = lerp(ay, by, t);
			const nx = -(by - ay) / L, ny = (bx - ax) / L, side = ledges.length % 2 ? 1 : -1;
			const lx = x + nx * side * (f.w * 0.5 - 0.6), ly = y + ny * side * (f.w * 0.5 - 0.6);
			ledges.push([r1(lx), r1(ly), r1(hAt(x, y) + 3.2), side]);
			next += 40;
		}
		acc += L;
	}
	return { id: f.id, pts, width: f.w, ledges };
});
const devils = { poly: PAN_POLY.map(([x, y]) => [r1(x), r1(y)]), level: PAN, hours: [11, 16] };

// ---------------------------------------------------------------- kit POIs (cook 2.4 table): Poisson >= 150 m per sub-zone
const pois = [...landmarks.map(l => [l.x, l.y]), ...sites.map(s => [s.x, s.y]), ...caves.map(c => [c.x, c.y])];
const near = (x, y, r) => pois.some(([px, py]) => (px - x) ** 2 + (py - y) ** 2 < r * r);
const inFrame = (x, y) => x > X0 + 20 && x < SEAM_E - 20 && y > SEAM_N + 20 && y < SEAM_S - 20;
const KIT = [
	// kind, variants, counts D1..D5
	['wreck_fragment', 3, [6, 4, 0, 0, 2]],
	['caravan', 3, [2, 4, 3, 1, 2]],
	['stela', 2, [1, 3, 5, 2, 1]],
	['ruin', 3, [1, 3, 5, 2, 3]],
	['outcrop', 4, [2, 2, 4, 4, 2]],
	['creature_set', 4, [3, 4, 3, 3, 1]],
	['seep', 2, [2, 1, 4, 1, 0]],
	['cave_mouth', 2, [0, 0, 1, 4, 1]],
	['bones', 3, [2, 2, 0, 1, 1]],
];
const ZONES = ['D1', 'D2', 'D3', 'D4', 'D5'];
const kit = [];
const okGround = (x, y, kind) => {
	const s = surfAt(x, y), h = hAt(x, y);
	if (s === SURF.hidden || s === SURF.sea || h < 1.4) return false;
	if (s === SURF.slipface || s === SURF.quicksand || s === SURF.brine || s === SURF.slot && kind !== 'cave_mouth') return false;
	if (Math.hypot(x - PLAZA.x, y - PLAZA.y) < PLAZA.r + 40) return false;
	if (kind === 'seep' && !(s === SURF.oasis || s === SURF.gravel || s === SURF.grass || s === SURF.softsand)) return false;
	if (kind === 'cave_mouth' && !(s === SURF.redrock || s === SURF.slot || s === SURF.pavement)) return false;
	if (kind === 'wreck_fragment' && zoneOf(x, y) === 'D1' && !(h < 6)) return false;
	return true;
};
for (const [kind, variants, counts] of KIT) {
	counts.forEach((count, zi) => {
		const z = ZONES[zi];
		for (let c = 0; c < count; c++) {
			let p = null;
			for (let tries = 0; tries < 6000 && !p; tries++) {
				const x = lerp(X0 + 30, SEAM_E - 30, rnd()), y = lerp(SEAM_N + 30, SEAM_S - 30, rnd());
				if (zoneOf(x, y) !== z || !okGround(x, y, kind) || near(x, y, 150)) continue;
				p = { x, y };
			}
			if (!p) { console.error(`kit ${kind} in ${z}: no room for ${c + 1}`); continue; }
			const o = { kind, variant: Math.floor(rnd() * variants), x: r1(p.x), y: r1(p.y), z: r1(hAt(p.x, p.y)), yaw: Math.round(rnd() * 360), zone: z };
			// Stelae stand on the old roads facing along them; outcrops say what they hold.
			if (kind === 'outcrop') o.ore = z === 'D1' ? 'rock_salt' : z === 'D4' ? (c % 2 ? 'copper_ore' : 'sunstone') : z === 'D5' ? 'sunglass' : z === 'D3' ? (c % 2 ? 'sunstone' : 'rock_salt') : 'sunstone';
			kit.push(o); pois.push([o.x, o.y]);
		}
	});
}
// The roads (the route rule): the wadi road, the Avenue of Hours, the caravan routes, the stelae road across the Erg.
const ROUTES = {
	wadiRoad: WADI.slice().reverse(),
	avenue: [[S('S21').x, S('S21').y], [PLAZA.x, PLAZA.y + PLAZA.r]],
	caravanNorth: [[S('camp_strand').x, S('camp_strand').y], [X(-2150), Y(-7300)], [X(-1900), Y(-7700)], [X(-1500), Y(-7650)]],
	stelaeRoad: [[X(-2200), Y(-6050)], [X(-1850), Y(-6850)], [X(-1300), Y(-7000)], [X(-1000), Y(-7250)]],
	caravanSouth: [[S('camp_caravan').x, S('camp_caravan').y], [X(-2200), Y(-6050)], [X(-1700), Y(-6050)]],
};
for (const [name, pts] of Object.entries(ROUTES)) {
	for (let s = 0; s < pts.length - 1; s++) {
		const L = Math.hypot(pts[s + 1][0] - pts[s][0], pts[s + 1][1] - pts[s][1]);
		for (let t = 0; t <= L; t += 20) {
			const x = lerp(pts[s][0], pts[s + 1][0], t / L), y = lerp(pts[s][1], pts[s + 1][1], t / L);
			if (!inFrame(x, y) || near(x, y, 125)) continue;
			const z = zoneOf(x, y);
			const kind = name === 'avenue' || name === 'stelaeRoad' ? 'stela' : name === 'wadiRoad' ? (z === 'D1' ? 'wreck_fragment' : 'seep') : 'caravan';
			if (!okGround(x, y, kind === 'seep' ? 'ruin' : kind)) continue;
			const o = { kind, variant: Math.floor(rnd() * 2), x: r1(x), y: r1(y), z: r1(hAt(x, y)), yaw: Math.round(Math.atan2(pts[s + 1][1] - pts[s][1], pts[s + 1][0] - pts[s][0]) * 180 / Math.PI), zone: z, route: name };
			kit.push(o); pois.push([x, y]);
		}
	}
}
// The density rule (a POI every 150-250 m): anywhere in the five sub-zones still more than 200 m from every place gets
// the kit piece that fits its ground.
{
	const fillKind = (z, s) => z === 'D1' ? (s === SURF.salt ? 'outcrop' : 'wreck_fragment') : z === 'D2' ? (rnd() < 0.5 ? 'bones' : 'caravan') : z === 'D3' ? (rnd() < 0.5 ? 'ruin' : 'stela') : z === 'D4' ? (rnd() < 0.5 ? 'outcrop' : 'ruin') : (rnd() < 0.5 ? 'ruin' : 'caravan');
	for (let pass = 0; pass < 3; pass++) for (let y = SEAM_N + 40; y < SEAM_S - 30; y += 30) for (let x = X0 + 40; x < SEAM_E - 30; x += 30) {
		if (near(x, y, 180)) continue;
		const s = surfAt(x, y), z = zoneOf(x, y);
		const kind = fillKind(z, s);
		if (!okGround(x, y, kind)) continue;
		const o = { kind, variant: Math.floor(rnd() * 2), x: r1(x), y: r1(y), z: r1(hAt(x, y)), yaw: Math.round(rnd() * 360), zone: z, fill: true };
		if (kind === 'outcrop') o.ore = z === 'D1' ? 'rock_salt' : z === 'D4' ? 'copper_ore' : 'sunstone';
		kit.push(o); pois.push([x, y]);
	}
}

// ---------------------------------------------------------------- decoration (build plan 2.2's rule): instances by kind
const decor = { grass: [], driftwood: [], saltbush: [], palm: [], reed: [], acacia: [], pear: [], rock: [], pebbles: [], hoodoo: [], shell: [] };
const nearSite = (x, y, r) => sites.some(s => (s.x - x) ** 2 + (s.y - y) ** 2 < r * r) || landmarks.some(s => (s.x - x) ** 2 + (s.y - y) ** 2 < r * r && s.id !== 'haboob_wall' && s.id !== 'mother_dune');
for (let j = 0; j < N; j += 1) for (let i = 0; i < N; i += 1) {
	const k = j * N + i; const s = SURFM[k];
	if (s === SURF.hidden || s === SURF.sea) continue;
	const x = X0 + i * STEP + (rnd() - 0.5) * STEP, y = Y0 + j * STEP + (rnd() - 0.5) * STEP, h = hAt(x, y);
	if (nearSite(x, y, 14) || Math.hypot(x - PLAZA.x, y - PLAZA.y) < PLAZA.r + 10) continue;
	const r = rnd();
	const row = sc => [r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(sc)];
	if (s === SURF.grass && r < 0.2) decor.grass.push(row(0.7 + rnd() * 0.6));
	else if (s === SURF.sand && h > 1.0 && h < 2.0 && r < 0.04) decor.driftwood.push(row(0.6 + rnd() * 0.9));
	else if (s === SURF.sand && r < 0.01) decor.shell.push(row(0.8 + rnd() * 0.5));
	else if (s === SURF.salt && r < 0.012) decor.saltbush.push(row(0.6 + rnd() * 0.6));
	else if (s === SURF.oasis && r < 0.016) {
		// Palms cluster near the wadi (water under the gravel): within 60 m of its line, else acacias.
		if (along(x, y, WADI).d < 60 && rnd() < 0.6) decor.palm.push(row(0.75 + rnd() * 0.4));
		else if (r < 0.006) decor.acacia.push(row(0.7 + rnd() * 0.6));
		else decor.pear.push(row(0.7 + rnd() * 0.6));
	}
	else if ((s === SURF.gravel || s === SURF.oasis) && along(x, y, WADI).d < 70 && r < 0.03) decor.palm.push(row(0.7 + rnd() * 0.45));
	else if (s === SURF.gravel && r < 0.036) decor.acacia.push(row(0.6 + rnd() * 0.6));
	else if (s === SURF.redrock && r < 0.008) decor.rock.push(row(1 + rnd() * 2.5));
	else if (s === SURF.redrock && r < 0.012) decor.pear.push(row(0.6 + rnd() * 0.5));
	else if (s === SURF.redrock && r < 0.0135 && zoneOf(x, y) === 'D4') decor.hoodoo.push(row(0.6 + rnd() * 0.8));
	else if (s === SURF.pavement && r < 0.02) decor.pebbles.push(row(0.8 + rnd() * 0.6));
	else if (s === SURF.softsand && r < 0.0015) decor.rock.push(row(0.6 + rnd() * 1.2));
}
// The oases' water edges: reeds round the quicksand pockets in the wadi, palms round the seeps (the kit) and the Wells.
for (const q of QUICK) for (let k = 0; k < 6; k++) { const a = rnd() * 6.283, rr = q[3] + 1 + rnd() * 3; decor.reed.push([r1(q[0] + Math.cos(a) * rr), r1(q[1] + Math.sin(a) * rr), r1(hAt(q[0] + Math.cos(a) * rr, q[1] + Math.sin(a) * rr)), Math.round(rnd() * 360), r1(0.8 + rnd() * 0.5)]); }

// ---------------------------------------------------------------- resources (cook 2.7): nodes the server spawns
const resources = [];
const node = (item, x, y, z, extra = {}) => resources.push({ item, x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), ...extra });
// Sunstone: 60 on sun-facing D3 / D4 rock (the south-facing faces: the ground falls south) and the Pan's salt cracks.
{
	let n = 0, tries = 0;
	while (n < 60 && tries++ < 40000) {
		const x = lerp(X(-2300), -300, rnd()), y = lerp(Y(-7400), SEAM_S - 40, rnd());
		const z = zoneOf(x, y), s = surfAt(x, y);
		if (!(z === 'D3' || z === 'D4')) continue;
		const sunFace = hAt(x, y - 4) - hAt(x, y + 4) > 0.6;           // falls south: faces the sun
		const panCrack = s === SURF.hardpan && rnd() < 0.18;
		if (!((s === SURF.redrock && sunFace) || panCrack)) continue;
		if (resources.some(r => r.item === 'sunstone' && Math.hypot(r.x - x, r.y - y) < 45)) continue;
		node('sunstone', x, y); n++;
	}
}
// Long Noon bloom nodes: 20 on the Pan (hidden until the event).
const bloom = [];
for (let k = 0; k < 20; k++) { let x, y, t = 0; do { x = lerp(X(-1050), X(-250), rnd()); y = lerp(Y(-7350), Y(-6150), rnd()); } while ((!inPoly(x, y, PAN_POLY) || Math.hypot(x - PLAZA.x, y - PLAZA.y) < PLAZA.r + 30) && t++ < 200); bloom.push([r1(x), r1(y), r1(hAt(x, y))]); }
// Sunglass at the Glass Field (S33), the Glass Canyon, and the outcrops that hold it.
{ const g = S('S33'); for (let k = 0; k < 14; k++) { const a = rnd() * 6.283, r = 4 + rnd() * 34; node('sunglass', g.x + Math.cos(a) * r, g.y + Math.sin(a) * r); } }
for (let k = 0; k < 8; k++) node('sunglass', GLASS.x - 30 - rnd() * 150, GLASS.y + (rnd() - 0.5) * 24);
// Rock salt: the Pan's cakes and D1's sabkha. Nitre: the D4 crusts. Copper: the D4 malachite seams.
for (let k = 0; k < 16; k++) { let x, y, t = 0; do { x = lerp(X(-1050), -320, rnd()); y = lerp(Y(-7350), Y(-6150), rnd()); } while (surfAt(x, y) !== SURF.hardpan && t++ < 200); node('rock_salt', x, y); }
for (let k = 0; k < 14; k++) { let x, y, t = 0; do { x = lerp(-2950, -2300, rnd()); y = lerp(-7500, -5750, rnd()); } while (surfAt(x, y) !== SURF.salt && t++ < 300); node('rock_salt', x, y); }
{ const n2 = S('S24'); for (let k = 0; k < 6; k++) node('nitre', n2.x + (rnd() - 0.5) * 40, n2.y + (rnd() - 0.5) * 30); }
for (const sl of SLOTS) for (let k = 0; k < 2; k++) { const p = sl[1 + k]; node(k ? 'copper_ore' : 'nitre', p[0] + 3, p[1]); }
{ const cd = S('S26'); for (let k = 0; k < 5; k++) node('copper_ore', cd.x + (rnd() - 0.5) * 30, cd.y + (rnd() - 0.5) * 30); }
// Clay and red ochre in the wadi's cut banks; gold flakes in its gravel.
for (let k = 0; k < 12; k++) {
	const t = 0.1 + rnd() * 0.8, s2 = Math.floor(t * (WADI.length - 1)), f = t * (WADI.length - 1) - s2;
	const x = lerp(WADI[s2][0], WADI[s2 + 1][0], f), y = lerp(WADI[s2][1], WADI[s2 + 1][1], f);
	const side = k % 2 ? 1 : -1;
	node(k % 3 === 0 ? 'red_ochre' : 'clay', x, y + side * 28);
	if (k % 2 === 0) node('gold_flakes', x + 4, y);
}
// Acacia wood and palm fibre come off the trees; prickly pear and dates are the wild crops (seeds now and then).
for (const a of decor.acacia.filter((q, k) => k % 3 === 0).slice(0, 60)) node('acacia_log', a[0], a[1], a[2], { tree: true, scale: a[4] });
for (const p of decor.palm.filter((q, k) => k % 2 === 0).slice(0, 80)) node('dates', p[0], p[1], p[2], { tree: true, wild: true, scale: p[4] });
for (const p of decor.pear.filter((q, k) => k % 3 === 0).slice(0, 70)) node('prickly_pear', p[0], p[1], p[2], { wild: true, scale: p[4] });
// Ores in the caves.
for (const c of caves) c.ore.forEach((ore, k) => { const room = c.rooms[k % c.rooms.length]; node(ore, c.x + room[0] + (rnd() - 0.5) * room[2] * 0.6, c.y + room[1] + (rnd() - 0.5) * room[2] * 0.6, c.floor + 0.2, { cave: c.id }); });
// The Hourhold's deep ore: copper and tin in the Copper Deep, iron 60 m down in the Iron Deep.
{
	const cd = hRoom('copper_deep'); for (let k = 0; k < 4; k++) node(k % 2 ? 'tin_ore' : 'copper_ore', HH.x + cd.dx + (k - 1.5) * 3, HH.y + cd.dy + cd.d * 0.4, cd.z + 0.2, { hall: true });
	const id = hRoom('iron_deep'); for (let k = 0; k < 4; k++) node('iron_ore', HH.x + id.dx + (k - 1.5) * 2.6, HH.y + id.dy + id.d * 0.4, id.z + 0.2, { hall: true });
}

// ---------------------------------------------------------------- what the places pay (cook 2.4, 2.5, 2.20): spots and caches
const spots = [], caches = [];
const spot = (kind, id, x, y, z, title, verb, seconds, gives = {}, lore = '', respawnHours = 0, extra = {}) => spots.push({ kind, id, x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), title, verb, seconds, gives, lore, respawnHours, ...extra });
const cache = (x, y, z, yaw, title, items, lore, refillHours) => caches.push({ x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), yaw, title, items, lore, refillHours });
{
	const s1 = S('S1'); cache(s1.x + 4, s1.y + 3, null, 200, 'The Tall Wreck\'s hold', { rope: 3, linen: 2, iron_nails: 3 }, 'Empty water casks, every bung knocked out. The labels match the camps\' casks.', 96);
	const s2 = S('S2'); cache(s2.x - 6, s2.y + 2, null, 60, 'A salt-cutter\'s sledge', { rock_salt: 6, rope: 1 }, 'A harness snapped clean through, dragged toward the dunes.', 72);
	const s4 = S('S4'); cache(s4.x + 2, s4.y, s4.z - 0.4, 140, 'A salt-crusted pack', { copper_ingot: 3, rock_salt: 2 }, 'Copper ingots under a skin of salt crystals, still in the dead Dromad\'s pack.', 0);
	const s5 = S('S5'); cache(s5.x + 10, s5.y - 4, null, 90, 'The broken salt-caravan', { rock_salt: 4, dates: 4, linen: 2 }, 'The guards\' spears still point outward.', 72);
	const s7 = S('S7'); spot('take', 'sea_god_figurehead', s7.x + 12, s7.y, s7.z + 2, 'The Ship in the Dune', 'Prise off the sun disc', 3, { bronze_ingot: 2, lore_sun_disc_nail: 1 }, 'Under the Sunwatch\'s sun disc, the sea people\'s god: a face with kelp for hair.');
	const s8 = S('S8'); cache(s8.x, s8.y, null, 90, 'The burial niche', { gold_flakes: 3, bone: 2 }, 'Gilt dust on the floor, and pup prints in it.', 48);
	const s9 = S('S9'); cache(s9.x + 30, s9.y, null, 100, 'A Dromad\'s load', { linen: 3, dates: 3, copper_ingot: 1 }, 'Eight in a line, sunk to the neck. The ninth is a trough.', 72);
	const s10 = S('S10'); spot('lens', 'navigator_holder', s10.x, s10.y, s10.z + 3, "The Navigator's Stones", 'Study the empty lens holder', 2, { bp_sunwatch_lens: 1 }, 'The holder is a bronze ring for a disc of stone that sees the sun through cloud. Sun lines run from it to the horizon.');
	const s13 = S('S13'); spot('take', 'expedition_journal', s13.x + 6, s13.y + 2, null, 'The Lost Expedition', 'Take the journal', 2, { lore_expedition_journal: 1 }, '');
	const s15 = S('S15'); cache(s15.x, s15.y + 3, null, 90, 'A waystation larder', { dates: 4, clay: 2, rope: 1 }, 'Stone benches face east. The water channel still trickles.', 48);
	const s17 = S('S17'); spot('take', 'gardener_stone', s17.x, s17.y, null, "The Gardener's Last Row", 'Heave the fallen stone', 4, { date_palm_seeds: 2, prickly_pear_seeds: 2 }, 'You can\'t move it far. Water finds the gap anyway, and the dead row drinks.');
	const s18 = S('S18'); spot('take', 'measuring_chisel', s18.x + 18, s18.y, null, 'The Measuring Wall', 'Pull the chisel from the groove', 2, { bronze_ingot: 1, lore_measuring_chisel: 1 }, '');
	const s20 = S('S20'); cache(s20.x, s20.y, null, 0, 'A salt-cutter\'s niche', { rock_salt: 8, sunstone: 1 }, 'Tool marks in the salt, and a lens of crystal in the roof that throws a moving spot of sun.', 72);
	const s22 = S('S22'); spot('take', 'living_plant', s22.x, s22.y, null, 'The Sunken Greenhouse', 'Take a cutting of the living plant', 2, { prickly_pear_seeds: 3, date_palm_seeds: 1 }, 'Everything else in here is a brown stalk. This one found a crack of water.', 72);
	const s23 = S('S23'); spot('take', 'old_coastline', s23.x, s23.y, null, "The Cartographer's Table", 'Take a rubbing of the map', 2, { lore_old_coastline: 1 }, 'The carved coast runs where the Pan is now. Waves are cut where you\'re standing.');
	const s25 = S('S25'); spot('rubbing', 'blue_hands', s25.x, s25.y + 3, null, 'The Hands Wall', 'Take an ochre rubbing of the blue hands', 3, { ochre_rubbing: 1 }, 'Hundreds of ochre hands, old. One set small and blue, at child height, new.');
	const s26 = S('S26'); cache(s26.x, s26.y, null, 0, "A miner's chest", { copper_ore: 6, rope: 2 }, 'A cage of bones where the Glasstails took the miners.', 96);
	const s29 = S('S29'); spot('take', 'lookout_cache', s29.x + 2, s29.y, null, 'The Lookout', 'Search the watchpost', 2, { sunglass: 2, rope: 2 }, '');
	const s30 = S('S30'); cache(s30.x, s30.y, null, 0, 'The Last Caravan\'s strongbox', { copper_ingot: 4, linen: 3, gold_flakes: 4 }, 'They roped their beasts in a ring and waited for a storm that never ended.', 0);
	const s32 = S('S32'); spot('lens', 'storm_tower_lens', s32.x - 3, s32.y + 2, s32.z + 5, 'The Storm Tower', 'Take the lens-grinder\'s slate', 2, { bp_sunwatch_lens: 1 }, 'On the lee side, out of the sand: a slate of lens-grinding marks, the recipe for a stone that finds the sun.');
	const s35 = S('S35'); cache(s35.x + 3, s35.y, null, 30, "A skiff's locker", { rope: 2, linen: 2, iron_nails: 2 }, 'Its sail is wrapped round a stela a kilometre from the sea.', 96);
	// The Hourhold's things.
	const hr = (id) => { const r = hRoom(id); return [HH.x + r.dx, HH.y + r.dy, r.z]; };
	const [sx, sy, sz] = hr('scribes');
	['lore_tablet_hours', 'lore_tablet_water', 'lore_tablet_colossi', 'lore_tablet_storm'].forEach((t, k) => spot('take', `tablet_${k + 1}`, sx - 4 + k * 2.6, sy + 6, sz, "A scribe's tablet", 'Take the tablet', 2, { [t]: 1 }));
	const [gx, gy, gz] = hr('granary'); spot('take', 'good_dates', gx + 4, gy, gz, 'A sealed jar', 'Break the seal', 2, { dates: 6 }, 'Every other jar is dust. This one is full of dates, sweet as the day they were sealed.');
	const [vx, vy, vz] = hr('sun_vault'); cache(vx, vy + 2, vz, 0, 'The Sun-Vault', { lore_glass_scarab: 1, sunstone: 20, gem_topaz: 1, ring_sunglass: 1 }, 'A scarab of golden glass, the size of your palm.', 0);
	const [lx, ly, lz] = hr('lens_forge'); spot('take', 'eye_mould', lx, ly, lz, 'The Lens Forge', 'Lift the great mould', 3, { lore_eye_mould: 1, sunstone: 3 }, '');
	const [ex, ey, ez] = hr('seventh_vault'); cache(ex, ey, ez, 0, 'The Seventh Vault', { lore_last_king_disc: 1, sunstone: 12, upgrade_shard: 2, radiant_essence: 2 }, 'A sun disc of beaten gold, the last king\'s, laid face down.', 0);
	const [fx, fy, fz] = hr('faces'); spot('take', 'ahrumet_face', fx, fy + 6, fz, 'The Workshop of Faces', 'Look into the unfinished face', 1, {}, 'A face with no eye yet. The socket is the size of a man. You know this face: you have seen it on the Pan.');
	const [wx, wy, wz] = hr('weighing_room'); spot('take', 'feather', wx, wy, wz, 'The Weighing Room', 'Lift the bronze feather', 2, { bronze_ingot: 1 }, 'The scale doesn\'t move when you take it. It was balanced against nothing.');
	const [px, py, pz] = hr('spring'); spot('runback', 'spring_brazier', px - 5, py, pz, 'The Spring', 'Light the brazier', 2, {}, 'Water rises clear out of the rock: the dead river\'s source, sealed under the sand.');
	// The Mirror Gallery's four mirrors (turned by hand; the beam reaches the Sun-Vault's lock when all four are right).
	const [mx, my, mz] = hr('mirror_gallery');
	[[-12, -4], [-4, 4], [4, -4], [12, 4]].forEach(([dx, dy], k) => spot('mirror', `gallery_mirror_${k}`, mx + dx, my + dy, mz, 'A bronze mirror on a pivot', 'Turn the mirror', 0.6, {}, '', 0, { mirror: k, right: [2, 5, 7, 1][k] }));
	// H-E3's dig: hold E 4 s with a shovel or any pick, once for the server.
	const w3 = hourhold.ways.find(w => w.id === 'H-E3'); spot('dig', 'hourhold_dig', w3.from[0], w3.from[1], w3.from[2], 'A dug pit and a carved lintel', 'Clear the pit', 4, {}, 'The lintel\'s sun disc is the same as the gate\'s. Steps go down under the sand.');
	// The plaza's meridian strip (2.14: stepping onto it wakes Ahrumet) is the boss's; its run-back stone is a spot.
	const rb = S('plaza_runback'); spot('runback', 'plaza_runback', rb.x, rb.y, rb.z, "The Sundial Plaza's run-back stone", 'Touch the stone', 1, {}, '');
}
// Dig spots (a sand mound every ~200 m across D1-D3: the Scarab digger's finds).
const digs = [];
for (let y = Y(-7400); y < Y(-5700); y += 200) for (let x = X(-2900); x < X(-400); x += 200) {
	const px = x + (rnd() - 0.5) * 120, py = y + (rnd() - 0.5) * 120, z = zoneOf(px, py), s = surfAt(px, py);
	if (!['D1', 'D2', 'D3'].includes(z) || !(s === SURF.softsand || s === SURF.grass || s === SURF.sand || s === SURF.oasis)) continue;
	digs.push([r1(px), r1(py), r1(hAt(px, py))]);
}
// The seven tilted stelae (the Long Noon, 2.10): seven of the avenue's and the stelae road's, each with a cache 18-30 m
// along its shadow's line; the seventh points to the Seventh Vault.
const tilted = [];
{
	const stelae = kit.filter(k => k.kind === 'stela');
	for (let k = 0; k < 7 && k < stelae.length; k++) {
		const st = stelae[Math.floor(k * stelae.length / 7)];
		st.tilted = true; st.lean = 4;
		const a = (st.yaw + 90) * Math.PI / 180, L = 18 + rnd() * 12;
		tilted.push({ stela: [st.x, st.y, st.z], cache: [r1(st.x + Math.cos(a) * L), r1(st.y + Math.sin(a) * L), r1(hAt(st.x + Math.cos(a) * L, st.y + Math.sin(a) * L))], seventh: k === 6 });
	}
}

// ---------------------------------------------------------------- arenas, routes (cook 2.13, 2.14)
const wells = landmarks.find(l => l.id === 'sevenfold_wells');
const SSARROK_ROUTE = [[MOTHER.x + 160, MOTHER.y + 150], [X(-1600), Y(-7100)], [X(-1850), Y(-7000)], [X(-1700), Y(-6850)], [X(-1450), Y(-6850)], [X(-1300), Y(-6950)], [X(-1250), Y(-7150)], [MOTHER.x + 160, MOTHER.y + 150]].map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]);
const SSARROK_DUSK = [[X(-1850), Y(-7000)], [X(-2300), Y(-6900)], [X(-2700), Y(-6800)], [COASTX(Y(-6700)) + 30, Y(-6700)]].map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]);
const arenas = {
	ahrumet: {
		at: [PLAZA.x, PLAZA.y, r1(hAt(PLAZA.x, PLAZA.y))], radius: PLAZA.r, stelaRing: 25, stelae: 12,
		gnomon: [PLAZA.x, PLAZA.y], throne: [PLAZA.x, r1(PLAZA.y - PLAZA.r + 6)], meridian: [[PLAZA.x, PLAZA.y], [PLAZA.x, r1(PLAZA.y - PLAZA.r + 14)]],
		runback: [S('plaza_runback').x, S('plaza_runback').y, S('plaza_runback').z], pit: 10,
	},
	mirage_matriarch: { at: [wells.x, wells.y, wells.z], radius: 35, pools: Array.from({ length: 7 }, (_, k) => { const a = k / 7 * 6.283 + 0.3, r = k === 0 ? 0 : 20; return [r1(wells.x + Math.cos(a) * r), r1(wells.y + Math.sin(a) * r), k === 0 ? 4 : 2.6]; }), deepest: 0 },
	glass_empress: { at: [r1(GLASS.x - 90), r1(GLASS.y + 14), r1(hAt(GLASS.x - 90, GLASS.y + 14))], radius: 32 },
	ssarrok: { route: SSARROK_ROUTE, dusk: SSARROK_DUSK, lair: caves.find(c => c.id === 'ssarroks_hollow') ? [caves.find(c => c.id === 'ssarroks_hollow').x, caves.find(c => c.id === 'ssarroks_hollow').y, caves.find(c => c.id === 'ssarroks_hollow').floor] : null },
};

// ---------------------------------------------------------------- portal slots (cook 2.22.2: 5; 2 Common, Uncommon, Rare, high)
const slotAt = (xd, yd) => { const x = X(xd), y = Y(yd); return { x: r1(x), y: r1(y), z: r1(hAt(x, y)) }; };
const portalSlots = [{ rank: '1', ...slotAt(-2450, -6400) }, { rank: '1', ...slotAt(-2000, -7200) }, { rank: '2', ...slotAt(-1500, -6650) }, { rank: '3', ...slotAt(-800, -6950) }, { rank: 'high', ...slotAt(-1500, -5800) }];

// ---------------------------------------------------------------- write Data/desert.json
const out = {
	_comment: 'Generated by Tools/terrain/desert.mjs (edit the script, not this file). Everything the Desert builds over its ground (FWRDesertData, WRDesertTypes.cpp; cooks/build-biome-desert.md 2.1-2.19). Map metres (game cm = m x 100); the cook\'s coordinates went through xMap / yMap. decor rows [x, y, z, yaw, scale]. quicksand / brine rows [x, y, z, r]. hourhold rooms: dx, dy from at, floor z, w x d x h (m). surfaces: the land\'s surface classes (the bytes in desert_ground.bin).',
	xMap: { a: -3000, k: 0.947 }, yMap: { a: -7955, k: 0.8673 }, pan: PAN, frame: [X0, SEAM_N, SEAM_E, SEAM_S],
	zones: { D1: 'the Wreck Strand', D2: 'the Erg', D3: 'Oasis Reach and the White Pan', D4: 'the Red Canyons', D5: 'the Haboob Wall' },
	surfaces: SURF,
	coast: Array.from({ length: 50 }, (_, k) => { const y = lerp(SEAM_N, SEAM_S, k / 49); return [r1(COASTX(y)), r1(y)]; }),
	panPoly: PAN_POLY.map(([x, y]) => [r1(x), r1(y)]),
	wadi: WADI.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]),
	saltgate: SG_LINE.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]),
	whaleRoad: WHALE_ROAD.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]),
	mother: [r1(MOTHER.x), r1(MOTHER.y), MOTHER.h, MOTHER.arms],
	landmarks, sites, kit, decor, caves, hourhold, resources, bloom,
	quicksand: QUICK, brine: BRINE, belt, floods, devils, digs, tilted,
	arenas, routes: Object.fromEntries(Object.entries(ROUTES).map(([k, v]) => [k, v.map(([x, y]) => [r1(x), r1(y)])])),
	portalSlots, spots, caches,
};
fs.writeFileSync(path.join(root, 'Data', 'desert.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- write the land
// Openings the underground needs in the ground: the caves' mouths and the Hourhold's three ways in.
{
	const holes = [...caves.map(c => [c.x + c.mouth.dx, c.y + c.mouth.dy, Math.max(c.mouth.w, c.mouth.h) * 0.5 + 1]), ...hourhold.ways.map(w => [w.from[0], w.from[1], 3.2])];
	for (const [hx, hy, hr] of holes) for (let j = 0; j < N; j++) { const y = Y0 + j * STEP; if (Math.abs(y - hy) > hr + STEP) continue; for (let i = 0; i < N; i++) { const x = X0 + i * STEP; if (Math.hypot(x - hx, y - hy) < hr + STEP * 0.5) SURFM[j * N + i] = SURF.hidden; } }
}
// The roads worn into the ground (the wadi road and the Avenue of Hours).
for (const pts of [ROUTES.avenue]) for (let s = 0; s < pts.length - 1; s++) {
	const [ax, ay] = pts[s], [bx, by] = pts[s + 1], L = Math.hypot(bx - ax, by - ay);
	for (let t = 0; t <= L; t += 2) { const px = ax + (bx - ax) * t / L, py = ay + (by - ay) * t / L; paint(px, py, 3.2, SURF.road, 0, [SURF.hardpan, SURF.gravel, SURF.oasis, SURF.softsand, SURF.zone]); }
}
const ZC = 20, ZN = Math.ceil((N - 1) * STEP / ZC);
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) {
		const x = X0 + (i + 0.5) * ZC, y = Y0 + (j + 0.5) * ZC;
		if (y < SEAM_N || x > SEAM_E || y > SEAM_S) { row += '~'; continue; }
		if (x < COASTX(y) - 40) { row += '~'; continue; }
		row += { D1: 's', D2: 'e', D3: inPoly(x, y, PAN_POLY) ? 'p' : 'o', D4: 'c', D5: 'w' }[zoneOf(x, y)];
	}
	zoneRows.push(row);
}
const camps = sites.filter(s => s.camp);
{
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'desert_ground.bin'), Buffer.concat([hb, sb]));
	const mid = [X0 + (N - 1) * STEP / 2, Y0 + (N - 1) * STEP / 2];
	const sp = (id, cls, hex, rough, speck) => ({ id, class: cls, color: C(hex), rough, ...(speck ? { speck } : {}) });
	const land = {
		_comment: 'Generated by Tools/terrain/desert.mjs (edit the script, not this file). The Desert\'s ground as a runtime heightfield land (WRLand; cooks/build-biome-desert.md 2.1): 4 m samples (uint16 heights = offset + m x unitsPerMetre, then a surface byte each; 255 hidden: the Fungal\'s ground north of y -7,800, the Ocean\'s Spine east of x -280, the Storm\'s south of y -5,652, the caves\' mouths). priority 2 and cuts: the Fungal strip\'s beach and sea and the Ocean\'s west sea are this land\'s while it is live; the seam rows take the neighbours\' own heights. Zones by the grid letter (s D1 the Wreck Strand, e D2 the Erg, o D3 Oasis Reach, p D3 the White Pan, c D4 the Red Canyons, w D5 the Haboob Wall), surfaces by class over them (the movement and the sand-swimmers read the class: softsand, slipface, quicksand, brine, wetsand...). camps: PlayerStarts tagged desert (the Sun path), a safe zone each, the starter stations at the middle one. underground.ways: the caves and the Hourhold (no rescue there).',
		id: 'desert', biome: 'desert', name: 'the Desert', priority: 2,
		origin: [mid[0] * 100, mid[1] * 100, 0], mapCentre: mid,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 1200000, chunksPerFrame: 4,
		seaFloor: -14,
		heightfield: { file: 'desert_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 90,
		zones: [
			{ id: 'D1', letter: 's', name: 'the Wreck Strand', ground: C('#E4D3AE'), groundAlt: C('#D2C29C'), rough: 0.92, region: 'desert_d1', bed: 'zone.D1', speck: { colors: [C('#8FA05A'), C('#F2ECE0')], amount: 0.08, scale: 2 } },
			{ id: 'D2', letter: 'e', name: 'the Erg', ground: C('#D9A85B'), groundAlt: C('#CF9A4E'), rough: 0.93, region: 'desert_d2', bed: 'zone.D2', speck: { colors: [C('#F0C878'), C('#B88444')], amount: 0.14, scale: 1.2 } },
			{ id: 'D3', letter: 'o', name: 'Oasis Reach', ground: C('#C9A46E'), groundAlt: C('#B8945E'), rough: 0.9, region: 'desert_d3', bed: 'zone.D3', speck: { colors: [C('#4E7A3A'), C('#E8D8B0')], amount: 0.1, scale: 2.5 } },
			{ id: 'pan', letter: 'p', name: 'the White Pan', ground: C('#EDE6D6'), groundAlt: C('#E2D9C6'), rough: 0.62, region: 'desert_d3', bed: 'zone.D3pan', speck: { colors: [C('#FFFFFF'), C('#C8BEAA')], amount: 0.12, scale: 1 } },
			{ id: 'D4', letter: 'c', name: 'the Red Canyons', ground: C('#A8462E'), groundAlt: C('#B85A3A'), rough: 0.88, region: 'desert_d4', bed: 'zone.D4', speck: { colors: [C('#3A2A2E'), C('#D88A5A')], amount: 0.14, scale: 3 } },
			{ id: 'D5', letter: 'w', name: 'the Haboob Wall', ground: C('#8C6E52'), groundAlt: C('#7A5E46'), rough: 0.86, region: 'desert_d5', bed: 'zone.D5', speck: { colors: [C('#3A3430'), C('#B08A60')], amount: 0.2, scale: 1 } },
		],
		surfaces: [
			sp('sand', SURF.sand, '#E8DCC0', 0.92, { colors: [C('#D4C4A0'), C('#FFFFFF')], amount: 0.1, scale: 1.5 }),
			sp('wetsand', SURF.wetsand, '#A8977A', 0.35),
			sp('salt', SURF.salt, '#F2EFE6', 0.6, { colors: [C('#D8D2C4'), C('#BDB09A')], amount: 0.2, scale: 1 }),
			sp('brine', SURF.brine, '#D6D2C8', 0.4, { colors: [C('#9A9A92'), C('#F4F2EC')], amount: 0.3, scale: 0.8 }),
			sp('grass', SURF.grass, '#D8C8A0', 0.92, { colors: [C('#8FA05A'), C('#6E7E40')], amount: 0.22, scale: 1.5 }),
			sp('softsand', SURF.softsand, '#D9A85B', 0.94, { colors: [C('#F0C878'), C('#C08C48')], amount: 0.16, scale: 1.1 }),
			sp('slipface', SURF.slipface, '#C8984E', 0.96, { colors: [C('#D8A860')], amount: 0.06, scale: 3 }),
			sp('shellgrit', SURF.shellgrit, '#D8C8A8', 0.9, { colors: [C('#F4ECE0'), C('#B8A888')], amount: 0.3, scale: 0.8 }),
			sp('quicksand', SURF.quicksand, '#7A6446', 0.22, { colors: [C('#5A4A34'), C('#9A8058')], amount: 0.25, scale: 0.6 }),
			sp('oasis', SURF.oasis, '#B89A68', 0.88, { colors: [C('#4E7A3A'), C('#6E8E48'), C('#A8462E')], amount: 0.14, scale: 2 }),
			sp('gravel', SURF.gravel, '#A8967A', 0.9, { colors: [C('#7A6A58'), C('#D8C8A8'), C('#E8C060')], amount: 0.35, scale: 0.7 }),
			sp('hardpan', SURF.hardpan, '#EDE6D6', 0.6, { colors: [C('#BFB6A2'), C('#FFFFFF')], amount: 0.14, scale: 1 }),
			sp('redrock', SURF.redrock, '#A8462E', 0.86, { colors: [C('#3A2A2E'), C('#D88A5A'), C('#C86A40')], amount: 0.2, scale: 2.5 }),
			sp('slot', SURF.slot, '#C88A5A', 0.92, { colors: [C('#A8462E'), C('#E0B080')], amount: 0.2, scale: 1 }),
			sp('glass', SURF.glass, '#2E3A2E', 0.18, { colors: [C('#4A6A3A'), C('#101410'), C('#8AA060')], amount: 0.35, scale: 1.2 }),
			sp('pavement', SURF.pavement, '#6E5A48', 0.7, { colors: [C('#2A2420'), C('#4A3E34'), C('#9A7E5E')], amount: 0.4, scale: 0.6 }),
			sp('blown', SURF.blown, '#B08A60', 0.94, { colors: [C('#C8A070')], amount: 0.1, scale: 2 }),
			sp('road', SURF.road, '#D8CCB0', 0.88, { colors: [C('#C8A070'), C('#EDE6D6')], amount: 0.2, scale: 1 }),
			sp('nitre', SURF.nitre, '#E8E4DC', 0.7, { colors: [C('#A8462E')], amount: 0.2, scale: 1 }),
			sp('plaza', SURF.plaza, '#E4DCC8', 0.7, { colors: [C('#C8B898'), C('#D9A85B')], amount: 0.12, scale: 2 }),
			sp('sea', SURF.sea, '#8E8268', 0.9, { colors: [C('#6E6450')], amount: 0.2, scale: 2 }),
			sp('spine', SURF.spine, '#9A8C78', 0.86, { colors: [C('#B98A52'), C('#6E6A62')], amount: 0.2, scale: 4 }),
		],
		slopeRock: { color: C('#9A5A3E'), fromDeg: 40, toDeg: 56, ledge: C('#3A2A2E') },
		bands: [],
		cuts: [{ land: 'fungal', box: [-2480, SEAM_N, -156, -7470] }, { land: 'ocean', box: [-600, SEAM_N, SEAM_E, -5700] }],
		underground: { letters: '', ways: [
			...caves.flatMap(c => c.rooms.map(([dx, dy, r]) => [r1(c.x + dx), r1(c.y + dy), r1(c.x + dx), r1(c.y + dy), r + 6])),
			...caves.map(c => [r1(c.x + c.mouth.dx), r1(c.y + c.mouth.dy), r1(c.x + c.rooms[0][0]), r1(c.y + c.rooms[0][1]), 8]),
			...hourhold.rooms.map(r => [r1(HH.x + r.dx), r1(HH.y + r.dy), r1(HH.x + r.dx + 0.1), r1(HH.y + r.dy), Math.max(r.w, r.d) * 0.5 + 6]),
			...hourhold.links.map(([a, b]) => { const A = hRoom(a), B = hRoom(b); return [r1(HH.x + A.dx), r1(HH.y + A.dy), r1(HH.x + B.dx), r1(HH.y + B.dy), 6]; }),
			...hourhold.ways.map(w => [w.from[0], w.from[1], w.toAt[0], w.toAt[1], 6]),
		] },
		buildFirst: camps.map(c => [c.x, c.y]),
		camps: { tag: 'desert', safeRadius: 35, list: camps.map(c => [c.x, c.y, 270]), stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'desert.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}

// ---------------------------------------------------------------- the map's place list
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.desert);
		for (const s of [...camps, ...landmarks]) dress.pois.push({ name: s.name, kind: s.camp ? 'camp' : 'landmark', x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'desert', desert: true });
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

// ---------------------------------------------------------------- the spacing check (cook 2.4: a POI every 150-250 m)
{
	const all = [...landmarks, ...sites, ...kit, ...caves];
	const counts = {}; for (const p of all) { const z = p.zone ?? zoneOf(p.x, p.y); counts[z] = (counts[z] || 0) + 1; }
	let worst = 0, worstAt = null;
	for (let y = SEAM_N + 40; y < SEAM_S - 40; y += 50) for (let x = X0 + 40; x < SEAM_E - 40; x += 50) {
		if (hAt(x, y) < 1 || surfAt(x, y) === SURF.hidden) continue;
		let d = 1e9; for (const p of all) d = Math.min(d, Math.hypot(p.x - x, p.y - y));
		if (d > worst) { worst = d; worstAt = [x, y]; }
	}
	const heights = {}; for (const z of ZONES) { const L = []; for (let y = SEAM_N; y < SEAM_S; y += 40) for (let x = X0; x < SEAM_E; x += 40) if (zoneOf(x, y) === z && hAt(x, y) > 0.5) L.push(hAt(x, y)); L.sort((a, b) => a - b); heights[z] = L.length ? `${L[0].toFixed(0)}..${L[L.length - 1].toFixed(0)} (median ${L[L.length >> 1].toFixed(0)})` : '-'; }
	console.error(`landmarks ${landmarks.length}, sites ${sites.length}, kit ${kit.length}, caves ${caves.length}; by zone ${JSON.stringify(counts)}; farthest from any POI ${worst.toFixed(0)} m at ${worstAt}`);
	console.error(`heights ${JSON.stringify(heights)}`);
	console.error(`decor: ${Object.entries(decor).map(([k, v]) => `${k} ${v.length}`).join(', ')}; resources ${resources.length}; quicksand ${QUICK.length}; brine ${BRINE.length}; digs ${digs.length}; spots ${spots.length}; caches ${caches.length}`);
	console.error(`camps: ${camps.map(c => `${c.name} (${c.x}, ${c.y}, ${c.z} m) ship ${c.ship}`).join('; ')}`);
	console.error(`plaza ${PLAZA.x.toFixed(0)}, ${PLAZA.y.toFixed(0)} at ${hAt(PLAZA.x, PLAZA.y).toFixed(1)} m; Hourhold surface ${HSURF.toFixed(1)} m, rooms ${hourhold.rooms.map(r => r.z).join('/')}`);
}
