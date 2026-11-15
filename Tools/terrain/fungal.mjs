// The Fungal caverns: the west half of belt B1, the Hollow Cape's karst plateau over three levels of caverns
// (cooks/biome-fungal.md 2.1-2.14, 2.20; cooks/build-biome-fungal.md 2.0-2.14; cooks/world-map.md 2.2). Map metres:
// +x east, +y south, real metres (game cm = map m x 100, the Sky land's frame). Writes
//   Data/lands/fungal.json        the runtime land (WRLand): frame, heightfield, zone grid, surfaces, cave ways, camps,
//                                 the Spore Crossing, and the cuts that take over the Sky land's west downs
//   Data/lands/fungal_ground.bin  uint16 heights (32768 + m * 16) row by row, then one surface byte per sample
//   Data/fungal.json              THE contract parsed into FWRFungalData (Source/Wildreach/WRFungalTypes.cpp): the caverns
//                                 (halls, dolines, chasms, grottos, tubes, crawls, shafts), sites, kit POIs, shrines,
//                                 dens, resources, clouds, lure lines, springcaps, cap-falls, foul air, the Breath vents,
//                                 arenas, run-back stones, lore, secrets; generated, edit this script
//   Data/island_dressing.json     only its "pois" (the map's place list): camps, descents, landmarks
//   .lane-tmp/fungal.png          a shaded layout preview (1 px = 4 m; not committed)
// node Tools/terrain/fungal.mjs
//
// The Sky lane's Hollow Cape (priority 0) keeps the Flue's crown: a stepped ring about 300 m round the shaft's axis and the
// strip east of x -160 (it buries the shaft's wall, which stands to 700 m). Everything west is this land's (priority 1,
// "cuts" drop the Sky's triangles there). The cut boxes' edges lie on both grids (multiples of 20 m), and this land's
// samples on and near those edges take the Sky land's own heights, so the two grounds meet edge to edge.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const t0 = Date.now();
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
export const X0 = -2480, Y0 = -9800, STEP = 4, N = 581, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;   // -160, -7480
const SEED = 8821;
const MAPC = [-1200, -8500];
const ORIGIN = [MAPC[0] * 100, MAPC[1] * 100, 0];
const FLUE = { x: 0, y: -8800, floor: -40 };

// ---------------------------------------------------------------- noise and helpers (the Crystal script's)
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
let rngState = 8821;
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
const toWorld = (x, y, z) => [Math.round((x - MAPC[0]) * 100 + ORIGIN[0]), Math.round((y - MAPC[1]) * 100 + ORIGIN[1]), Math.round(z * 100)];

// ---------------------------------------------------------------- the Sky land's ground (read, never written)
const SKY = (() => {
	const L = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'lands', 'sky.json'), 'utf8'));
	const h = L.heightfield, b = fs.readFileSync(path.join(root, 'Data', 'lands', h.file)), n = h.samples;
	const at = (i, j) => { i = Math.max(0, Math.min(n - 1, i)); j = Math.max(0, Math.min(n - 1, j)); return (b.readUInt16LE((j * n + i) * 2) - h.offset) / h.unitsPerMetre; };
	return {
		x0: h.x0, y0: h.y0, step: h.step, n,
		height(x, y) {
			const fi = (x - h.x0) / h.step, fj = (y - h.y0) / h.step, i = Math.floor(fi), j = Math.floor(fj), tx = fi - i, ty = fj - j;
			return lerp(lerp(at(i, j), at(i + 1, j), tx), lerp(at(i, j + 1), at(i + 1, j + 1), tx), ty);
		},
	};
})();
// The Sky ground this land replaces (the cut boxes, map metres; their edges on both grids).
const CUTS = [
	[-1160, -10120, -320, -7040],
	[-320, -10120, -240, -8980], [-320, -8620, -240, -7040],
	[-240, -10120, -160, -9060], [-240, -8540, -160, -7040],
];
const inCut = (x, y, grow = 0) => CUTS.some(([a, b, c, d]) => x >= a - grow && x <= c + grow && y >= b - grow && y <= d + grow);
// Owned: west of the Sky land's x0 everywhere, inside the cut boxes beyond it (edges included).
const owned = (x, y) => x < -1150 || inCut(x, y, 0.01);
// The edge between this land and the Sky's ground that stays (the stepped ring and the x -160 line), as points.
const SEAM = [];
{
	const segs = [
		[[-320, -8980], [-320, -8620]], [[-320, -8980], [-240, -8980]], [[-320, -8620], [-240, -8620]],
		[[-240, -9060], [-240, -8980]], [[-240, -8620], [-240, -8540]], [[-240, -9060], [-160, -9060]], [[-240, -8540], [-160, -8540]],
		[[-160, -9800], [-160, -9060]], [[-160, -8540], [-160, -7480]],
	];
	for (const [a, b] of segs) { const L = Math.hypot(b[0] - a[0], b[1] - a[1]); for (let k = 0; k <= L; k += 4) { const x = lerp(a[0], b[0], k / L), y = lerp(a[1], b[1], k / L); SEAM.push([x, y, SKY.height(x, y)]); } }
}
const seamGrid = new Map();
for (const p of SEAM) { const k = `${Math.floor(p[0] / 40)},${Math.floor(p[1] / 40)}`; if (!seamGrid.has(k)) seamGrid.set(k, []); seamGrid.get(k).push(p); }
function nearestSeam(x, y, within = 320) {
	let best = null, bd = within;
	const r = Math.ceil(within / 40), ci = Math.floor(x / 40), cj = Math.floor(y / 40);
	for (let a = -r; a <= r; a++) for (let b = -r; b <= r; b++) {
		const L = seamGrid.get(`${ci + a},${cj + b}`);
		if (!L) continue;
		for (const p of L) { const d = Math.hypot(p[0] - x, p[1] - y); if (d < bd) { bd = d; best = p; } }
	}
	return best ? { d: bd, h: best[2], p: best } : null;
}

// ---------------------------------------------------------------- the coast (fungal 2.1: sea cliffs west and north)
const COAST_RAW = [
	// north coast, west to east, then the east seam (not a coast), the Desert strip's south shore, the west cliffs
	[-2300, -9420], [-2100, -9445], [-1900, -9436], [-1700, -9452], [-1500, -9430], [-1300, -9456], [-1150, -9470],
	[-1000, -9510], [-800, -9566], [-600, -9634], [-400, -9690], [-160, -9715],
	[-120, -9715], [-120, -7540],
	[-160, -7556], [-500, -7540], [-900, -7528], [-1300, -7540], [-1700, -7532], [-2100, -7548], [-2262, -7570],
	[-2290, -7800], [-2302, -8100], [-2296, -8400], [-2310, -8700], [-2292, -9000], [-2305, -9250],
];
const COAST = [];
const COAST_REAL = [];   // which densified points are real coast (the east side is the seam)
for (let i = 0; i < COAST_RAW.length; i++) {
	const a = COAST_RAW[i], b = COAST_RAW[(i + 1) % COAST_RAW.length];
	const L = Math.hypot(b[0] - a[0], b[1] - a[1]), n = Math.max(1, Math.round(L / 12));
	const nx = -(b[1] - a[1]) / L, ny = (b[0] - a[0]) / L;
	const seam = a[0] >= -160 && b[0] >= -160;
	for (let k = 0; k < n; k++) {
		const t = k / n, x = lerp(a[0], b[0], t), y = lerp(a[1], b[1], t);
		const off = seam ? 0 : 12 * fbm(x / 150, y / 150, 4) + 4 * fbm(x / 33 + 9, y / 33, 3);
		COAST.push([x + nx * off, y + ny * off]);
		COAST_REAL.push(!seam);
	}
}
const inPoly = (x, y, P) => { let c = false; for (let i = 0, j = P.length - 1; i < P.length; j = i++) { const [xi, yi] = P[i], [xj, yj] = P[j]; if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c; } return c; };
const coastGrid = new Map();
for (let i = 0; i < COAST.length; i++) { if (!COAST_REAL[i]) continue; const j = (i + 1) % COAST.length; const k = `${Math.floor(COAST[i][0] / 50)},${Math.floor(COAST[i][1] / 50)}`; if (!coastGrid.has(k)) coastGrid.set(k, []); coastGrid.get(k).push([COAST[i], COAST[j]]); }
function coastDist(x, y) {
	let bd = 400;
	const ci = Math.floor(x / 50), cj = Math.floor(y / 50);
	for (let a = -8; a <= 8; a++) for (let b = -8; b <= 8; b++) {
		const L = coastGrid.get(`${ci + a},${cj + b}`);
		if (!L) continue;
		for (const [p, q] of L) bd = Math.min(bd, distSeg(x, y, p[0], p[1], q[0], q[1]).d);
	}
	return inPoly(x, y, COAST) ? bd : -bd;
}

// Camps (world-map KEPT positions), each on its sinkhole rim; its newcomer wreck in the nearest cove below.
const CAMPS = [
	{ id: 'C1', name: 'Rimfall Camp', x: -1800, y: -8300, cove: [-2268, -8300], bowl: [-1772, -8324, 14] },
	{ id: 'C2', name: 'Thrift Camp', x: -1650, y: -8900, cove: [-2272, -8900], bowl: [-1676, -8874, 13] },
	{ id: 'C3', name: 'North Tip Camp', x: -1100, y: -9300, cove: [-1100, -9444], bowl: [-1082, -9276, 12] },
];
for (const c of CAMPS) c.yaw = yawOf(c.cove[0] - c.x, c.cove[1] - c.y);
// The coves: a notch in the cliff with a shingle beach at its head, 3 m over the sea.
const COVES = CAMPS.map(c => { const dx = c.x - c.cove[0], dy = c.y - c.cove[1], L = Math.hypot(dx, dy); return { x: c.cove[0], y: c.cove[1], r: 26, ux: dx / L, uy: dy / L, len: Math.min(230, L - 30) }; });

// ---------------------------------------------------------------- the plateau (fungal 2.1)
// Karst pavement: clints cut by grykes on a joint grid rotated 18 degrees, 6-9 m apart, 0.8 m deep.
function gryke(x, y) {
	const c = Math.cos(18 * DEG), s = Math.sin(18 * DEG);
	const u = x * c + y * s, v = -x * s + y * c;
	const su = 7.2 + 1.5 * fbm(x / 60, y / 60, 2), sv = 8.6 + 1.8 * fbm(x / 70 + 3, y / 70, 2);
	const du = Math.abs(((u / su) % 1 + 1) % 1 - 0.5) * su, dv = Math.abs(((v / sv) % 1 + 1) % 1 - 0.5) * sv;
	return 0.8 * Math.max(1 - sstep(0.0, 0.35, du), 1 - sstep(0.0, 0.35, dv)) * (0.5 + 0.5 * fbm(x / 20, y / 20, 2));
}
const GREAT = { x: -620, y: -8580, r: 130, floor: 115 };
const TITAN_BOWL = { x: -1922, y: -8930, r: 16 };
const SIEVES = [{ x: -1235, y: -7872, r: 7 }, { x: -1196, y: -7840, r: 8 }, { x: -1160, y: -7868, r: 6.5 }];
function plateau(x, y) {
	let h = 92 + 48 * sstep(-2250, -620, x) + 11 * fbm(x / 320, y / 320, 4) + 2.2 * fbm(x / 70, y / 70, 3);
	h += 0.35 * fbm(x / 6, y / 6, 2) - gryke(x, y) * sstep(-8020, -8120, y);
	// The Desert's north strip: sand dunes over the same rock (the Desert lane replaces it), falling to a beach.
	if (y > -8080) {
		const dune = 64 + 5 * Math.sin((x * 0.6 + y) / 38 + 2 * fbm(x / 120, y / 120, 2)) + 3 * fbm(x / 45, y / 45, 3);
		h = lerp(h, dune, sstep(-8080, -7960, y));
	}
	// The Kneeling Titan's doline (fungal 2.3 L2): a dry sinkhole 14 m deep its arm is sunk in to the shoulder.
	{ const d = Math.hypot(x - TITAN_BOWL.x, y - TITAN_BOWL.y); if (d < TITAN_BOWL.r * 1.7) h -= 14 * (1 - sstep(TITAN_BOWL.r * 0.45, TITAN_BOWL.r * 1.7, d)); }
	// The camps' bowls: dry cover-collapse sinkholes 8 m deep, fern-filled.
	for (const c of CAMPS) { const d = Math.hypot(x - c.bowl[0], y - c.bowl[1]); if (d < c.bowl[2] * 1.6) h -= 8 * (1 - sstep(c.bowl[2] * 0.5, c.bowl[2] * 1.6, d)); }
	// The Great Doline round the Ancestor Cap: a bowl 260 m across, its floor at +115, a walkable west side.
	const dg = Math.hypot(x - GREAT.x, y - GREAT.y);
	if (dg < GREAT.r + 30) h = lerp(GREAT.floor + 2 * fbm(x / 20, y / 20, 2), h, sstep(GREAT.r * 0.55, GREAT.r + 30, dg));
	return h;
}
// The Spine's foot: within reach of the Sky ground that stays, rise to it (a concave mountain foot, 55-70 degrees).
function spineFoot(x, y, h) {
	const s = nearestSeam(x, y, 330);
	if (!s) return h;
	const rise = Math.max(0, s.h - h);
	const W = Math.min(300, Math.max(30, rise * 0.62));
	if (s.d >= W) return h;
	const t = 1 - s.d / W;
	const ridges = 6 * fbm(x / 40, y / 40, 3) * t * (1 - t) * 4;
	return lerp(h, s.h, Math.pow(t, 1.7)) + ridges;
}
function coastShape(x, y, h) {
	const s = coastDist(x, y);
	let out = h;
	const south = y > -7700;
	if (south) {
		// The Desert strip's beach: sand falling gently into the sea.
		if (s < 120) out = s >= 0 ? lerp(1.2, h, sstep(0, 120, s)) : lerp(1.2, -14, sstep(0, 60, -s));
	} else if (s < 9) {
		// Sea cliffs: the plateau stops in a 9 m-wide limestone wall; a boulder apron, then the sea floor.
		out = s >= 0 ? lerp(-2.5, h, Math.pow(sstep(0, 9, s), 0.55)) : lerp(-2.5, -16, sstep(0, 45, -s));
	}
	for (const c of COVES) {
		const d = Math.hypot(x - c.x, y - c.y);
		if (d < c.r + 14) out = Math.min(out, lerp(2.8 + 0.4 * fbm(x / 6, y / 6, 2), out, Math.pow(sstep(c.r * 0.6, c.r + 14, d), 0.7)));
		// The gully up from the cove's head to the plateau (a geo's ravine, 23 degrees: the way up from the beach).
		const t = ((x - c.x) * c.ux + (y - c.y) * c.uy) / c.len, lat = Math.abs((x - c.x) * -c.uy + (y - c.y) * c.ux);
		if (t > 0 && t < 1.05 && lat < 16) {
			const top = h;
			const floor = lerp(2.8, top, sstep(0, 1, t)) + 0.3 * fbm(x / 5, y / 5, 2);
			out = Math.min(out, lerp(floor, out, sstep(6, 16, lat)));
		}
	}
	return out;
}

// ---------------------------------------------------------------- the caverns (build cook 2.2)
// Halls: kind hall | doline | chasm | grotto | dome; zone F2..F5, HG; floor (m), rx, ry, h (m), yaw (deg). A doline's
// r is rx = ry and it is open to the sky; a chasm's floor is its talus bottom (its walkways are given by its tubes'
// mouths); a grotto's lake surface is floor + lake.
const HALLS = [
	// F2 the Spore Gardens: doline floors open to the sky.
	{ id: 'G1', kind: 'doline', zone: 'F2', name: 'the Garden Doline', x: -1500, y: -8700, floor: 40, rx: 34, ry: 34, h: 0, shrine: 'garden_doline', light: 'sun' },
	{ id: 'G2', kind: 'doline', zone: 'F2', name: 'the Garden Farm', x: -1400, y: -8630, floor: 38, rx: 28, ry: 28, h: 0, site: 'C2', light: 'sun' },
	{ id: 'G3', kind: 'doline', zone: 'F2', name: 'the Brood Doline', x: -1850, y: -9150, floor: 40, rx: 30, ry: 30, h: 0, arena: 'sporemother', light: 'sun' },
	{ id: 'G4', kind: 'doline', zone: 'F2', name: 'the Springcap Grove', x: -1150, y: -8960, floor: 30, rx: 26, ry: 26, h: 0, site: 'C3', light: 'sun' },
	{ id: 'G5', kind: 'hall', zone: 'F2', name: 'the Cloud Room', x: -1800, y: -8500, floor: 30, rx: 60, ry: 45, h: 38, yaw: 20, site: 'C4', light: 'shaft' },
	{ id: 'G6', kind: 'hall', zone: 'F2', name: 'the Upper Vein', x: -1950, y: -8700, floor: 32, rx: 22, ry: 18, h: 14, yaw: -10, gallery: 'upper_vein', light: 'cap' },
	{ id: 'G7', kind: 'doline', zone: 'F2', name: 'the East Garden', x: -1255, y: -8800, floor: 34, rx: 24, ry: 24, h: 0, light: 'sun' },
	{ id: 'G8', kind: 'doline', zone: 'F2', name: 'the North Garden', x: -1600, y: -9050, floor: 36, rx: 22, ry: 22, h: 0, light: 'sun' },
	// F3 the Glow-moss Halls.
	{ id: 'H1', kind: 'hall', zone: 'F3', name: 'Starfall Hall', x: -1600, y: -8010, floor: -60, rx: 200, ry: 118, h: 70, yaw: 8, landmark: 'L4', lair: 'weaver', shrine: 'starfall', light: 'stars' },
	{ id: 'H2', kind: 'hall', zone: 'F3', name: 'the Lure Gallery', x: -1745, y: -8290, floor: -20, rx: 70, ry: 14, h: 12, yaw: -18, site: 'C5', light: 'lines' },
	{ id: 'H3', kind: 'hall', zone: 'F3', name: "Knockers' Junction", x: -1350, y: -8150, floor: -40, rx: 16, ry: 16, h: 10, site: 'C7', light: 'moss' },
	{ id: 'H4', kind: 'hall', zone: 'F3', name: 'the Spore Archive', x: -1905, y: -8075, floor: -50, rx: 20, ry: 12, h: 9, yaw: 70, site: 'C8', light: 'lamp' },
	{ id: 'H5', kind: 'hall', zone: 'F3', name: 'the Nitre Gallery', x: -1500, y: -7860, floor: -70, rx: 40, ry: 18, h: 14, yaw: -5, site: 'C9', gallery: 'nitre', light: 'moss' },
	{ id: 'H6', kind: 'hall', zone: 'F3', name: 'the Sand Falls', x: -1200, y: -7850, floor: -20, rx: 70, ry: 50, h: 60, yaw: 0, landmark: 'L8', site: 'C6', light: 'sand' },
	{ id: 'H7', kind: 'hall', zone: 'F3', name: 'the fifth tunnel', x: -1290, y: -8235, floor: -38, rx: 12, ry: 8, h: 7, yaw: 30, secret: 'F-S2', light: 'none' },
	// F4 the Mycelium Webs.
	{ id: 'W1', kind: 'chasm', zone: 'F4', name: 'the Shoestring Chasm', x: -900, y: -8250, floor: -205, rx: 72, ry: 30, h: 128, yaw: 0, landmark: 'L5', shrine: 'cable', walk: -140, light: 'amber' },
	{ id: 'W2', kind: 'hall', zone: 'F4', name: 'the Silk Larders', x: -700, y: -8050, floor: -130, rx: 30, ry: 22, h: 22, yaw: 20, site: 'C10', light: 'amber' },
	{ id: 'W3', kind: 'chasm', zone: 'F4', name: 'the Root Ceiling', x: -400, y: -8200, floor: -172, rx: 42, ry: 24, h: 110, yaw: 90, site: 'C11', walk: -110, light: 'amber' },
	{ id: 'W4', kind: 'hall', zone: 'F4', name: 'the Titan Nursery', x: -955, y: -8010, floor: -160, rx: 36, ry: 26, h: 30, yaw: -15, site: 'C12', light: 'amber' },
	{ id: 'W5', kind: 'hall', zone: 'F4', name: 'the Sealed Gate', x: -820, y: -8380, floor: -180, rx: 14, ry: 12, h: 14, site: 'C13', light: 'amber' },
	{ id: 'W6', kind: 'hall', zone: 'F4', name: 'the Deep Vein', x: -505, y: -7905, floor: -180, rx: 30, ry: 18, h: 18, yaw: 10, gallery: 'deep_vein', light: 'none' },
	// F5 the Flooded Grottos.
	{ id: 'R1', kind: 'grotto', zone: 'F5', name: 'the Drowned Granary', x: -500, y: -9050, floor: -36, rx: 200, ry: 60, h: 42, yaw: 0, lake: 6, landmark: 'L7', light: 'water' },
	{ id: 'R2', kind: 'grotto', zone: 'F5', name: 'the Grotto Weir', x: -800, y: -9150, floor: -24, rx: 30, ry: 18, h: 16, yaw: 30, lake: 3, site: 'C14', light: 'water' },
	{ id: 'R3', kind: 'hall', zone: 'F5', name: 'the Air Bell', x: -300, y: -8950, floor: -50, rx: 14, ry: 12, h: 9, site: 'C15', foul: true, light: 'none' },
	{ id: 'R4', kind: 'hall', zone: 'F5', name: 'the Vat Room', x: -350, y: -9150, floor: -50, rx: 16, ry: 14, h: 10, site: 'C16', light: 'water' },
	{ id: 'R5', kind: 'hall', zone: 'F5', name: 'the Steeping Vats', x: -230, y: -9200, floor: -50, rx: 36, ry: 15, h: 16, yaw: 12, arena: 'rot_king', light: 'water' },
	{ id: 'R6', kind: 'chasm', zone: 'F5', name: 'the Toad Leaps', x: -600, y: -8850, floor: -42, rx: 62, ry: 18, h: 50, yaw: 0, site: 'C17', walk: -10, light: 'water' },
	// The river's mouth into the Flue (the Sky lane's shaft): a chamber at the shaft's wall wider than the one wall panel the
	// shaft leaves out for it (17.7 m x 18.5 m), so the hole is closed all round by this rock.
	{ id: 'R9', kind: 'hall', zone: 'F5', name: 'the Flue mouth', x: -137, y: -8789, floor: -40, rx: 12, ry: 13.5, h: 22, yaw: 0, light: 'none' },
	{ id: 'R7', kind: 'grotto', zone: 'F5', name: 'the Rimstone Pocket', x: -205, y: -9300, floor: -56, rx: 16, ry: 14, h: 10, lake: 0.8, gallery: 'rimstone', foul: true, light: 'water' },
	// The Heart Grotto (fungal 2.5): ten rooms in a ring round the Heart chamber, and the chamber.
	{ id: 'HC', kind: 'dome', zone: 'HG', name: 'the Heart chamber', x: -600, y: -8600, floor: -220, rx: 60, ry: 60, h: 70, arena: 'mycelorn', light: 'heart' },
];
// The ring's rooms: angle (degrees, map frame: 0 east, 90 south), radius, size, floor, what each is.
const HG_ROOMS = [
	{ id: 'HG1', name: 'the Lantern Street', a: 138, r: 232, rx: 60, ry: 15, h: 20, floor: -192, street: true },
	{ id: 'HG2', name: 'the Market of Hands', a: 100, r: 238, rx: 26, ry: 20, h: 16, floor: -195 },
	{ id: 'HG3', name: 'the Feeding Hall', a: 62, r: 242, rx: 40, ry: 14, h: 14, floor: -198 },
	{ id: 'HG4', name: "the Elders' Tower", a: 22, r: 250, rx: 22, ry: 22, h: 50, floor: -200 },
	{ id: 'HG9', name: 'the Heart Gate', a: -18, r: 244, rx: 12, ry: 12, h: 12, floor: -202 },
	{ id: 'HG6', name: 'the Koi Pool', a: -66, r: 232, rx: 28, ry: 22, h: 16, floor: -204, lake: 1.6 },
	{ id: 'HG7', name: 'the Seed Vault', a: -108, r: 250, rx: 10, ry: 9, h: 8, floor: -205, vault: true },
	{ id: 'HG8', name: 'the Last House', a: -148, r: 240, rx: 14, ry: 12, h: 12, floor: -200 },
	{ id: 'HG5', name: 'the Mite Warren', a: 176, r: 252, rx: 26, ry: 20, h: 14, floor: -196 },
	{ id: 'HG10', name: 'the Heart Wall', a: 180, r: 104, rx: 22, ry: 18, h: 18, floor: -216 },
];
const HC = HALLS.find(h => h.id === 'HC');
for (const q of HG_ROOMS) {
	const x = HC.x + Math.cos(q.a * DEG) * q.r, y = HC.y + Math.sin(q.a * DEG) * q.r;
	HALLS.push({ id: q.id, kind: q.lake ? 'grotto' : 'hall', zone: 'HG', name: q.name, x: r1(x), y: r1(y), floor: q.floor, rx: q.rx, ry: q.ry, h: q.h, yaw: q.street ? q.a + 90 : q.a + 90, lake: q.lake, room: true, vault: !!q.vault, light: 'city' });
}
const hallById = id => HALLS.find(h => h.id === id);
const into = (id, toward, inset = 0.7, z) => {
	const h = hallById(id);
	const dx = toward[0] - h.x, dy = toward[1] - h.y, L = Math.hypot(dx, dy) || 1;
	const c = Math.cos(-(h.yaw || 0) * DEG), s = Math.sin(-(h.yaw || 0) * DEG);
	const lx = dx / L * c - dy / L * s, ly = dx / L * s + dy / L * c;
	const r = 1 / Math.hypot(lx / h.rx, ly / h.ry) * inset;
	return [r1(h.x + dx / L * r), r1(h.y + dy / L * r), z ?? (h.walk ?? (h.lake ? h.floor + h.lake + 0.4 : h.floor))];
};
// Tubes: kind tube (round, flat floor at 0.55 r below its axis) | crawl (a low wide passage w x h) | stair (a hewn
// passage whose floor steps) | ramp (a tube open to the sky at its top end); pts: [x, y, floor] control points.
const TUBES = [];
const tube = (id, kind, zone, ends, mids, size, extra = {}) => {
	const toward0 = mids[0] || (typeof ends[1] === 'string' ? [hallById(ends[1]).x, hallById(ends[1]).y] : ends[1]);
	const a = typeof ends[0] === 'string' ? into(ends[0], toward0, 0.7, extra.z0) : ends[0];
	const toward1 = mids.length ? mids[mids.length - 1] : a;
	const b = typeof ends[1] === 'string' ? into(ends[1], toward1, 0.7, extra.z1) : ends[1];
	TUBES.push({ id, kind, zone, ctrl: [a, ...mids, b], ...size, ...extra });
};
// F2: the Gardens' floor route and its branches.
tube('G1-G2', 'tube', 'F2', ['G1', 'G2'], [], { r: 6 });
tube('G2-G7', 'tube', 'F2', ['G2', 'G7'], [[-1330, -8700, 36]], { r: 6 });
tube('G7-G4', 'tube', 'F2', ['G7', 'G4'], [[-1210, -8880, 32]], { r: 6 });
tube('G1-G5', 'tube', 'F2', ['G1', 'G5'], [[-1640, -8620, 36]], { r: 6.5 });
tube('G5-G6', 'tube', 'F2', ['G5', 'G6'], [[-1890, -8610, 31]], { r: 5 });
tube('G1-G8', 'tube', 'F2', ['G1', 'G8'], [[-1560, -8880, 38]], { r: 6 });
tube('G8-G3', 'tube', 'F2', ['G8', 'G3'], [[-1720, -9110, 38]], { r: 6 });
// D5 the Sea Cave: the river's mouth on the west cliff, up into the Cloud Room (the river's warm air meets the doline's).
tube('D5', 'tube', 'F5', [[-2290, -8600, 1.6], 'G5'], [[-2150, -8580, 6], [-1960, -8540, 18]], { r: 7 }, { descent: 'D5', river: true });
// D2 the Long Stair: from the fissure on the plateau down past the Gardens' level to Knockers' Junction.
tube('D2', 'stair', 'F3', [[-1262, -8478, 0], 'H3'], [[-1272, -8420, 72], [-1290, -8350, 32], [-1318, -8262, -8]], { w: 5, h: 7 }, { descent: 'D2', landings: true });
tube('D2-G2', 'tube', 'F2', [[-1290, -8350, 32], 'G2'], [[-1340, -8480, 35]], { r: 5 });
// F3.
tube('H3-H2', 'tube', 'F3', ['H3', 'H2'], [[-1520, -8235, -30]], { r: 7 });
tube('H2-G5', 'tube', 'F3', ['H2', 'G5'], [[-1790, -8390, 4]], { r: 6 }, { blend: 'F2' });
tube('H2-H4', 'tube', 'F3', ['H2', 'H4'], [[-1880, -8180, -38]], { r: 6 });
tube('H3-H1', 'tube', 'F3', ['H3', 'H1'], [[-1430, -8080, -52]], { r: 8 });
tube('H4-H1', 'tube', 'F3', ['H4', 'H1'], [], { r: 6 });
tube('H1-H5', 'tube', 'F3', ['H1', 'H5'], [], { r: 7 });
tube('H5-H6', 'tube', 'F3', ['H5', 'H6'], [[-1330, -7850, -44]], { r: 7 });
tube('H3-H7', 'crawl', 'F3', ['H3', 'H7'], [], { w: 5, h: 3.2 }, { secret: 'F-S2', sealed: true });
// F3 -> F4: the long spiral down from Knockers' Junction to the Shoestring Chasm's west mouth.
tube('H3-W1', 'tube', 'F4', ['H3', [-972, -8250, -140]], [[-1250, -8150, -62], [-1160, -8090, -86], [-1080, -8160, -108], [-1040, -8260, -126]], { r: 7 }, { blend: 'F3' });
// F4.
tube('W1-W5', 'tube', 'F4', [[-828, -8250, -140], 'W5'], [[-800, -8320, -162]], { r: 6 });
tube('W1-W2', 'tube', 'F4', [[-828, -8250, -140], 'W2'], [[-760, -8150, -136]], { r: 6.5 });
tube('W1-W4', 'tube', 'F4', [[-940, -8232, -198], 'W4'], [[-950, -8120, -180]], { r: 6 });
tube('W2-W3', 'tube', 'F4', ['W2', [-400, -8158, -110]], [[-560, -8110, -118]], { r: 6 });
tube('W3-W6', 'tube', 'F4', [[-402, -8158, -110], 'W6'], [[-450, -8010, -150]], { r: 6 });
// The mole wall (fungal 2.5 way 3): a tube down from the Root Ceiling's north mouth to a 6 m crawl a tame mole digs.
tube('W3-mole', 'tube', 'F4', [[-400, -8242, -110], [-340, -8420, -168]], [[-372, -8330, -142]], { r: 5 });
tube('mole', 'crawl', 'HG', [[-340, -8420, -168], 'HG4'], [], { w: 5, h: 2.8 }, { sealed: true, mole: true });
tube('W5-HG1', 'tube', 'HG', ['W5', 'HG1'], [], { r: 6 }, { door: 'sealed_gate' });
// D6 the Great Doline's shaft: its spiral ledge down to -80, then the tube to the Silk Larders.
tube('D6-W2', 'tube', 'F4', [[-662, -8528, -80], 'W2'], [[-690, -8360, -98], [-700, -8160, -122]], { r: 6 });
// F2 -> F5: the Springcap Shaft's foot to the Grotto Weir; its side door into the Grove.
tube('D4-G4', 'tube', 'F2', [[-1050, -8900, 30], 'G4'], [], { r: 5 });
tube('D4-R2', 'tube', 'F5', [[-1050, -8900, -20], 'R2'], [[-940, -9010, -22]], { r: 6 }, { blend: 'F2' });
// F5.
tube('R2-R1', 'tube', 'F5', ['R2', 'R1'], [], { r: 7 }, { river: true });
tube('R1-R6', 'tube', 'F5', [[-600, -8994, -29.6], [-650, -8866, -10]], [[-630, -8930, -18]], { r: 6 });
tube('R1-R3', 'tube', 'F5', [[-318, -9010, -29.6], 'R3'], [], { r: 6 });
tube('R3-R4', 'tube', 'F5', ['R3', 'R4'], [], { r: 5 });
tube('R4-R5', 'tube', 'F5', ['R4', 'R5'], [], { r: 6 });
tube('R5-R7', 'crawl', 'F5', ['R5', 'R7'], [[-205, -9258, -55]], { w: 6, h: 2.6 }, { flooded: 1.2 });
// The river through the Toad Leaps' foot and on to the Flue (fungal 2.3 L6): its floor at the Fall Field's.
tube('R6-R8', 'tube', 'F5', [[-540, -8850, -40], [-128, -8790, -39]], [[-420, -8862, -40], [-260, -8820, -40]], { r: 7.5 }, { river: true, flue: true });
tube('R6-R6w', 'tube', 'F5', [[-662, -8850, -40], [-540, -8850, -40]], [], { r: 7.5 }, { river: true });
tube('R8-R3', 'tube', 'F5', [[-320, -8846, -40], 'R3'], [], { r: 5 });
// The Heart Gate (one-way out): from the ring up to the river passage.
tube('HG9-R8', 'tube', 'HG', ['HG9', [-300, -8840, -40]], [[-340, -8760, -150], [-300, -8800, -96]], { r: 6 }, { door: 'heart_gate' });
// The moat: a steep wet stair from the Drowned Granary's south shore, a flooded crawl into the Koi Pool.
tube('moat', 'tube', 'HG', [[-430, -8996, -29.6], [-478, -8880, -176]], [[-380, -8950, -64], [-430, -8900, -104], [-492, -8930, -142]], { r: 5 }, { blend: 'F5' });
tube('moat_crawl', 'crawl', 'HG', [[-478, -8880, -176], 'HG6'], [], { w: 6, h: 2.6 }, { flooded: 1.2 });
// The Heart Grotto's streets round the ring, and the Heart Wall's door into the chamber.
const RING = ['HG1', 'HG2', 'HG3', 'HG4', 'HG9', 'HG6', 'HG7', 'HG8', 'HG5'];
for (let i = 0; i < RING.length; i++) {
	const a = RING[i], b = RING[(i + 1) % RING.length];
	const ha = hallById(a), hb = hallById(b);
	const ma = Math.atan2(ha.y - HC.y, ha.x - HC.x), mb = Math.atan2(hb.y - HC.y, hb.x - HC.x);
	let mid = (ma + mb) / 2; if (Math.abs(ma - mb) > Math.PI) mid += Math.PI;
	const rr = 236;
	tube(`${a}-${b}`, 'tube', 'HG', [a, b], [[r1(HC.x + Math.cos(mid) * rr), r1(HC.y + Math.sin(mid) * rr), (ha.floor + hb.floor) / 2]], { r: 6 }, { street: true, z0: ha.lake ? ha.floor + ha.lake + 0.3 : ha.floor, z1: hb.lake ? hb.floor + hb.lake + 0.3 : hb.floor });
}
tube('HG5-HG10', 'tube', 'HG', ['HG5', 'HG10'], [], { r: 6 });
tube('HG10-HC', 'tube', 'HG', ['HG10', 'HC'], [], { r: 7 }, { door: 'heart_wall' });

// Shafts: descent (a way down: its stair or path built in it), drop, sieve (the Sand Falls' skylights), vent.
const SHAFTS = [];
function catmull(p0, p1, p2, p3, t) {
	const t2 = t * t, t3 = t2 * t;
	return p1.map((_, k) => 0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2 + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3));
}
function densify(ctrl, every = 3) {
	const out = [];
	for (let i = 0; i < ctrl.length - 1; i++) {
		const p0 = ctrl[Math.max(0, i - 1)], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[Math.min(ctrl.length - 1, i + 2)];
		const L = Math.hypot(p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]);
		const n = Math.max(1, Math.ceil(L / every));
		for (let k = 0; k < n; k++) out.push(catmull(p0, p1, p2, p3, k / n));
	}
	out.push(ctrl[ctrl.length - 1]);
	return out.map(p => [r1(p[0]), r1(p[1]), r2(p[2])]);
}

// ---------------------------------------------------------------- the heightfield
const HF = new Float32Array(N * N);
const SURFM = new Uint8Array(N * N);
const SURF = { ground: 0, pave: 1, grass: 2, thrift: 3, cliff: 4, shingle: 5, sand: 6, path: 7, moss: 8, crust: 9, ring: 10, spine: 11, sea: 12, hidden: 255 };
const idx = (i, j) => j * N + i;
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP;
	if (!owned(x, y)) { HF[idx(i, j)] = SKY.height(x, y); continue; }
	HF[idx(i, j)] = coastShape(x, y, spineFoot(x, y, plateau(x, y)));
}
// The seam's own samples are the Sky's, exactly (both grounds share that ring).
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP;
	if (x >= -1150 && owned(x, y) && !CUTS.some(([a, b, c, d]) => x > a + 0.01 && x < c - 0.01 && y > b + 0.01 && y < d - 0.01) && !(x < -1150)) HF[idx(i, j)] = SKY.height(x, y);
}
const hAt = (x, y) => {
	const fi = Math.min(N - 1.001, Math.max(0, (x - X0) / STEP)), fj = Math.min(N - 1.001, Math.max(0, (y - Y0) / STEP));
	const i = Math.floor(fi), j = Math.floor(fj), tx = fi - i, ty = fj - j;
	return lerp(lerp(HF[idx(i, j)], HF[idx(i + 1, j)], tx), lerp(HF[idx(i, j + 1)], HF[idx(i + 1, j + 1)], tx), ty);
};
const slopeAt = (x, y) => Math.hypot(hAt(x + 2, y) - hAt(x - 2, y), hAt(x, y + 2) - hAt(x, y - 2)) / 4;

// ---------------------------------------------------------------- descents (fungal 2.4 D1-D6), the doline mouths, the Sieves
const DESCENTS = [
	{ id: 'D1', name: 'the Garden Doline', x: -1500, y: -8700, hall: 'G1', how: 'spiral_path', r: 34 },
	{ id: 'D2', name: 'the Long Stair', x: -1262, y: -8478, hall: 'H3', how: 'long_stair', r: 4 },
	{ id: 'D3', name: 'the Brood Doline', x: -1850, y: -9150, hall: 'G3', how: 'rope_stair', r: 30 },
	{ id: 'D4', name: 'the Springcap Shaft', x: -1050, y: -8900, hall: 'G4', how: 'shelf_shaft', r: 9 },
	{ id: 'D5', name: 'the Sea Cave', x: -2290, y: -8600, hall: 'G5', how: 'sea_cave', r: 7 },
	{ id: 'D6', name: 'the Great Doline', x: -662, y: -8528, hall: 'W2', how: 'great_spiral', r: 16 },
];
for (const d of DESCENTS) d.z = r2(hAt(d.x, d.y));
// Every doline hall's mouth is a hole in the plateau; the cave skin rolls over its lip.
const MOUTHS = [];
for (const h of HALLS) if (h.kind === 'doline') { h.top = r2(hAt(h.x, h.y) + 6); MOUTHS.push({ x: h.x, y: h.y, r: h.rx - 3.5 }); }
// The Cloud Room's own doline mouth over its north end, and the Springcap Shaft (D4), the Great Doline's shaft (D6).
const CLOUD_MOUTH = { x: -1770, y: -8530, r: 20 };
MOUTHS.push({ x: CLOUD_MOUTH.x, y: CLOUD_MOUTH.y, r: CLOUD_MOUTH.r - 3.5 });
SHAFTS.push({ id: 'cloud_mouth', kind: 'mouth', x: CLOUD_MOUTH.x, y: CLOUD_MOUTH.y, r: CLOUD_MOUTH.r, z0: 50, z1: r2(hAt(CLOUD_MOUTH.x, CLOUD_MOUTH.y) + 6), hall: 'G5' });
{
	const d = DESCENTS.find(q => q.id === 'D4');
	SHAFTS.push({ id: 'D4', kind: 'descent', x: d.x, y: d.y, r: 9, z0: -24, z1: r2(d.z + 6), hall: 'G4', how: d.how });
	MOUTHS.push({ x: d.x, y: d.y, r: 6 });
}
{
	const d = DESCENTS.find(q => q.id === 'D6');
	d.z = r2(hAt(d.x, d.y));
	SHAFTS.push({ id: 'D6', kind: 'descent', x: d.x, y: d.y, r: 16, z0: -84, z1: r2(d.z + 6), hall: 'W2', how: d.how });
	MOUTHS.push({ x: d.x, y: d.y, r: 12 });
}
for (const s of SIEVES) {
	s.z = r2(hAt(s.x, s.y));
	SHAFTS.push({ id: `sieve${SIEVES.indexOf(s) + 1}`, kind: 'sieve', x: s.x, y: s.y, r: s.r, z0: 20, z1: r2(s.z + 6), hall: 'H6' });
	MOUTHS.push({ x: s.x, y: s.y, r: s.r - 2.5 });
}
// The hidden samples: off this land's ground (the Sky's), far sea, the mouths.
const hidden = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = idx(i, j);
	if (!owned(x, y)) { hidden[k] = 1; continue; }
	if (coastDist(x, y) < -110) hidden[k] = 1;
	for (const m of MOUTHS) if (Math.hypot(x - m.x, y - m.y) < m.r) hidden[k] = 1;
}
// Densify the tubes; open the ground wherever a tube's roof stands above it (the Sea Cave's mouth, the Long Stair's
// fissure), and set the stair's top to the ground.
const tubesDense = TUBES.map(t => ({ ...t, pts: densify(t.ctrl) }));
{
	const d2 = tubesDense.find(t => t.id === 'D2');
	d2.pts[0][2] = r2(hAt(d2.pts[0][0], d2.pts[0][1]) - 1.5);
	d2.ctrl[0][2] = d2.pts[0][2];
	tubesDense[tubesDense.indexOf(d2)] = { ...d2, pts: densify(d2.ctrl) };
}
for (const t of tubesDense) {
	const roof = t.kind === 'tube' ? t.r * 1.55 : t.h;
	const half = t.kind === 'tube' ? t.r : t.w / 2;
	for (let q = 0; q < t.pts.length; q++) {
		const p = t.pts[q];
		const top = p[2] + roof;
		if (top < hAt(p[0], p[1]) - 1.0) continue;
		const ci = Math.round((p[0] - X0) / STEP), cj = Math.round((p[1] - Y0) / STEP), rr = Math.ceil((half + 2) / STEP);
		for (let b = -rr; b <= rr; b++) for (let a = -rr; a <= rr; a++) {
			const ii = ci + a, jj = cj + b;
			if (ii < 0 || jj < 0 || ii >= N || jj >= N) continue;
			const x = X0 + ii * STEP, y = Y0 + jj * STEP;
			if (Math.hypot(x - p[0], y - p[1]) > half - 1.5) continue;
			if (top > HF[idx(ii, jj)] - 1.0) hidden[idx(ii, jj)] = 1;
		}
	}
}

// ---------------------------------------------------------------- zones and surfaces
const zoneAt = (x, y) => {
	if (!owned(x, y)) return '~';
	if (coastDist(x, y) < -3) return '~';
	if (y > -8020 + 25 * fbm(x / 140, 3.3, 2)) return 'd';
	const s = nearestSeam(x, y, 90);
	if (s && s.h - hAt(x, y) < 40 && s.d < 80) return 's';
	return '1';
};
const ROUTES = [
	// Deepfarmer paths on the plateau: bare feet worn into the turf between camps, descents and the ring (fungal 2.4).
	{ id: 'west_road', name: 'the Rim Path', pts: [[-1800, -8300], [-1720, -8420], [-1620, -8560], [-1540, -8660]] },
	{ id: 'north_road', name: 'the Rim Path', pts: [[-1650, -8900], [-1590, -8800], [-1530, -8738]] },
	{ id: 'tip_road', name: 'the Tip Path', pts: [[-1100, -9300], [-1140, -9150], [-1220, -9020], [-1320, -8880], [-1440, -8760]] },
	{ id: 'ring_road', name: 'the Ring Path', pts: [[-1535, -8665], [-1460, -8520], [-1380, -8380], [-1350, -8250]] },
	{ id: 'stair_road', name: 'the Stair Path', pts: [[-1380, -8380], [-1300, -8440], [-1266, -8474]] },
	{ id: 'great_road', name: 'the Great Doline Path', pts: [[-1350, -8250], [-1200, -8320], [-1040, -8420], [-900, -8500], [-780, -8560], [-752, -8580]] },
	{ id: 'shaft_road', name: 'the Springcap Path', pts: [[-1220, -9020], [-1130, -8960], [-1062, -8906]] },
	{ id: 'brood_road', name: 'the Brood Path', pts: [[-1650, -8900], [-1740, -9010], [-1812, -9110]] },
];
const RING_OF_CAPS = { x: -1350, y: -8250, r: 100 };
const BREATH_VENTS = { x: -700, y: -8700, r: 60 };
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = idx(i, j);
	if (hidden[k]) { SURFM[k] = SURF.hidden; continue; }
	const z = zoneAt(x, y), h = HF[k], s = slopeAt(x, y);
	let c = SURF.ground;
	if (z === '~' || h < -1) c = SURF.sea;
	else if (h < 3.5 && coastDist(x, y) < 14) c = SURF.shingle;
	else if (s > 1.0) c = SURF.cliff;
	else if (z === 's') c = s > 0.5 ? SURF.cliff : SURF.spine;
	else if (z === 'd') c = SURF.sand;
	else {
		const n = fbm(x / 24, y / 24, 3), m = fbm(x / 11 + 5, y / 11, 2);
		// Limestone pavement; salt-grass and thrift in the grykes and hollows; moss round every doline's lip.
		c = n > 0.28 ? SURF.grass : SURF.pave;
		if (m > 0.6 && coastDist(x, y) < 160) c = SURF.thrift;
		for (const mo of MOUTHS) if (Math.hypot(x - mo.x, y - mo.y) < mo.r + 9 + 3 * fbm(x / 7, y / 7, 2)) c = SURF.moss;
		if (Math.hypot(x - RING_OF_CAPS.x, y - RING_OF_CAPS.y) < RING_OF_CAPS.r - 6) c = SURF.ring;
		if (Math.hypot(x - BREATH_VENTS.x, y - BREATH_VENTS.y) < BREATH_VENTS.r * (0.8 + 0.3 * fbm(x / 20, y / 20, 2))) c = SURF.crust;
		const dg = Math.hypot(x - GREAT.x, y - GREAT.y);
		if (dg < GREAT.r * 0.6) c = SURF.moss;
		for (const R of ROUTES) if (distLine(x, y, R.pts).d < 1.2 + 0.5 * fbm(x / 5, y / 5, 2)) c = SURF.path;
	}
	SURFM[k] = c;
}

// ---------------------------------------------------------------- the check: no cave roof near the ground
{
	const tops = [];
	for (const h of HALLS) {
		if (h.kind === 'doline') continue;
		for (let a = 0; a < 16; a++) for (const f of [0, 0.5, 0.85]) {
			const x = h.x + Math.cos(a * 22.5 * DEG) * h.rx * f, y = h.y + Math.sin(a * 22.5 * DEG) * h.ry * f;
			const top = h.floor + h.h * Math.sqrt(Math.max(0, 1 - f * f));
			if (h.id === 'G5' && Math.hypot(x - CLOUD_MOUTH.x, y - CLOUD_MOUTH.y) < CLOUD_MOUTH.r + 8) continue;
			if (h.id === 'H6' && SIEVES.some(s => Math.hypot(x - s.x, y - s.y) < s.r + 10)) continue;
			tops.push({ id: h.id, x, y, top });
		}
	}
	for (const t of tubesDense) {
		if (t.descent || t.id === 'D5') continue;
		for (const p of t.pts) tops.push({ id: t.id, x: p[0], y: p[1], top: p[2] + (t.kind === 'tube' ? t.r * 1.55 : t.h) });
	}
	let bad = 0, worst = 1e9, at = null; const ids = {};
	for (const c of tops) {
		const cover = hAt(c.x, c.y) - c.top;
		if (MOUTHS.some(m => Math.hypot(c.x - m.x, c.y - m.y) < m.r + 12)) continue;
		if (cover < worst) { worst = cover; at = c; }
		if (cover < 6) { bad++; ids[c.id] = Math.min(ids[c.id] ?? 1e9, +cover.toFixed(1)); }
	}
	if (bad) console.error(`WARNING: ${bad} cave points under 6 m of rock; worst ${worst.toFixed(1)} m at ${at.id} (${at.x.toFixed(0)}, ${at.y.toFixed(0)}) ${JSON.stringify(ids)}`);
}

// ---------------------------------------------------------------- places (fungal 2.3, 2.4, 2.11, 2.20)
const site = (id, kind, name, x, y, extra = {}) => ({ id, kind, name, x: r1(x), y: r1(y), z: r2(extra.z ?? hAt(x, y)), yaw: extra.yaw ?? 0, ...extra });
const sites = [];
for (const c of CAMPS) sites.push(site(c.id, 'camp', c.name, c.x, c.y, { yaw: c.yaw, camp: true, cove: [c.cove[0], c.cove[1], r2(hAt(c.cove[0], c.cove[1]))], bowl: c.bowl }));
// Landmarks (fungal 2.3).
sites.push(site('L1', 'ancestor_cap', 'the Ancestor Cap', GREAT.x, GREAT.y, { landmark: true, z: GREAT.floor, r: GREAT.r, capHeight: 90, capR: 30 }));
sites.push(site('L2', 'kneeling_titan', 'the Kneeling Titan', -1952, -8952, { landmark: true, yaw: yawOf(TITAN_BOWL.x + 1952, TITAN_BOWL.y + 8952), bowl: [TITAN_BOWL.x, TITAN_BOWL.y, TITAN_BOWL.r] }));
sites.push(site('L3', 'ring_of_caps', 'the Ring of Caps', RING_OF_CAPS.x, RING_OF_CAPS.y, { landmark: true, r: RING_OF_CAPS.r, shrine: 'ring' }));
const flueFloorZ = -40;
sites.push(site('L6', 'fall_field', 'the Fall Field', FLUE.x, FLUE.y, { landmark: true, z: flueFloorZ, r: 120 }));
for (const h of HALLS) if (h.landmark && !sites.find(s => s.id === h.landmark)) sites.push(site(h.landmark, h.landmark === 'L4' ? 'false_sky' : h.landmark === 'L5' ? 'shoestring' : h.landmark === 'L7' ? 'drowned_granary' : 'sand_falls', h.name, h.x, h.y, { landmark: true, z: h.walk ?? h.floor, hall: h.id, cave: true }));
for (const d of DESCENTS) sites.push(site(d.id, 'descent', d.name, d.x, d.y, { z: d.z, how: d.how, hall: d.hall, r: d.r }));
// Surface hand sites S1-S8 (fungal 2.4).
sites.push(site('S1', 'down_stair_terraces', 'the Down-Stair Terraces', -1480, -8760, { hand: true, yaw: 160 }));
sites.push(site('S2', 'knocker_cairn', 'the Knocker Cairn', -1272, -8452, { hand: true, yaw: 260 }));
sites.push(site('S3', 'drying_yard', 'the Drying Yard', -1700, -9050, { hand: true, yaw: yawOf(GREAT.x + 1700, GREAT.y + 9050) }));
sites.push(site('S4', 'grain_towers', 'the Grain Towers', -1600, -8350, { hand: true, yaw: 30 }));
sites.push(site('S5', 'puffball_cliffs', 'the Puffball Cliffs', -2240, -9250, { hand: true, yaw: 180 }));
sites.push(site('S6', 'fallen_kite', 'the Fallen Kite', -1050, -8650, { hand: true, yaw: 205 }));
sites.push(site('S7', 'ringstones', 'the Ringstones', RING_OF_CAPS.x, RING_OF_CAPS.y + 6, { hand: true }));
sites.push(site('S8', 'breath_vents', 'the Breath Vents', BREATH_VENTS.x, BREATH_VENTS.y, { hand: true, r: BREATH_VENTS.r }));
// Cavern hand sites C1-C18 in their halls (fungal 2.4).
const caveSite = (id, kind, name, hall, dx = 0, dy = 0, extra = {}) => { const h = hallById(hall); return site(id, kind, name, h.x + dx, h.y + dy, { z: h.walk ?? (h.lake ? h.floor + h.lake : h.floor), hall, hand: true, cave: true, ...extra }); };
sites.push(caveSite('C1', 'offering_niches', 'the Offering Niches', 'G1', -26, 14, { yaw: 150 }));
sites.push(caveSite('C2', 'garden_farm', 'the Garden Farm', 'G2'));
sites.push(caveSite('C3', 'springcap_grove', 'the Springcap Grove', 'G4'));
sites.push(caveSite('C4', 'cloud_room', 'the Cloud Room', 'G5'));
sites.push(caveSite('C5', 'lure_gallery', 'the Lure Gallery', 'H2'));
sites.push(caveSite('C6', 'dunes_within', 'the Dunes Within', 'H6', 18, 14));
sites.push(caveSite('C7', 'knockers_junction', "Knockers' Junction", 'H3'));
sites.push(caveSite('C8', 'spore_archive', 'the Spore Archive', 'H4'));
sites.push(caveSite('C9', 'saltpetre_walls', 'the Saltpetre Walls', 'H5'));
sites.push(caveSite('C10', 'silk_larders', 'the Silk Larders', 'W2'));
sites.push(caveSite('C11', 'root_ceiling', 'the Root Ceiling', 'W3'));
sites.push(caveSite('C12', 'titan_nursery', 'the Titan Nursery', 'W4'));
sites.push(caveSite('C13', 'sealed_gate', 'the Sealed Gate', 'W5'));
sites.push(caveSite('C14', 'grotto_weir', 'the Grotto Weir', 'R2'));
sites.push(caveSite('C15', 'air_bell', 'the Air Bell', 'R3'));
sites.push(caveSite('C16', 'vat_room', 'the Vat Room', 'R4'));
sites.push(caveSite('C17', 'toad_leaps', 'the Toad Leaps', 'R6'));
sites.push(site('C18', 'fallen_champion', 'the Fallen Champion', FLUE.x - 20, FLUE.y + 10, { z: flueFloorZ, hand: true, cave: true, yaw: 160 }));
// The Heart Grotto's rooms (fungal 2.5).
for (const q of HG_ROOMS) { const h = hallById(q.id); sites.push(site(q.id, 'hg_' + q.id.toLowerCase(), q.name, h.x, h.y, { z: h.lake ? h.floor + h.lake : h.floor, hall: q.id, hand: true, cave: true, grotto: true, yaw: h.yaw })); }
// The ore galleries (fungal 2.6) are their halls; the Rimstone Pocket too.

// Shrines (fungal 2.11): the Great Shrine on the Garden Doline's floor (no guard), four lesser.
const shrines = [
	{ id: 'garden_doline', name: 'the Great Shrine of the Garden Doline', x: -1500, y: -8700, z: 40, great: true, hall: 'G1' },
	{ id: 'ring_shrine', name: 'the Ring Shrine', x: RING_OF_CAPS.x + 4, y: RING_OF_CAPS.y - 2, z: r2(hAt(RING_OF_CAPS.x + 4, RING_OF_CAPS.y - 2)), guards: { tunnel_toad: 3 } },
	{ id: 'starfall_shrine', name: 'the Starfall Shrine', x: -1450, y: -7960, z: -60, hall: 'H1', guards: { webspinner: 2 } },
	{ id: 'cable_shrine', name: 'the Cable Shrine', x: -976, y: -8256, z: -140, hall: 'W1', guards: { sporestalker: 1, mycomite: 6 } },
	{ id: 'flue_floor_shrine', name: 'the Flue Floor Shrine', x: 40, y: -8780, z: flueFloorZ, guards: { rotmaw: 1, mycomite: 4 } },
];
// Run-back stones (contested bosses: one outside each arena; the Weaver roams).
const runback = [
	{ boss: 'mycelorn', x: r1(hallById('HG10').x - 40), y: r1(hallById('HG10').y + 30), z: -216 },
	{ boss: 'sporemother', x: -1790, y: -9080, z: r2(hAt(-1790, -9080)) },
	{ boss: 'rot_king', x: -340, y: -9158, z: -50 },
];

// ---------------------------------------------------------------- kit POIs (fungal 2.4: 53, placed with variants)
const kit = [];
const kitAdd = (kind, x, y, z, extra = {}) => kit.push({ id: `k${kit.length + 1}_${kind}`, kind, x: r1(x), y: r1(y), z: r2(z), v: kit.length % 3, ...extra });
// Glow-cap rings on the Deepfarmer routes (world-map SPEC: every 150 m): 4 surface, 10 in the caves (2.4).
const walkEvery = (pts, every, fn, start = every / 2) => { let acc = 0, next = start; for (let i = 1; i < pts.length; i++) { const a = pts[i - 1], b = pts[i]; const L = Math.hypot(b[0] - a[0], b[1] - a[1]); while (acc + L >= next) { const t = (next - acc) / L; fn(lerp(a[0], b[0], t), lerp(a[1], b[1], t), a.length > 2 ? lerp(a[2], b[2], t) : null, yawOf(b[0] - a[0], b[1] - a[1])); next += every; } acc += L; } };
let surfRings = 0;
for (const R of [ROUTES[0], ROUTES[2], ROUTES[5]]) walkEvery(R.pts, 150, (x, y, z, yaw) => { if (surfRings >= 4) return; kitAdd('glowcap_ring', x + Math.cos((yaw + 90) * DEG) * 4, y + Math.sin((yaw + 90) * DEG) * 4, hAt(x, y), { surface: true }); surfRings++; }, 90);
let caveRings = 0;
for (const t of tubesDense) {
	if (caveRings >= 10 || t.kind === 'crawl' || t.zone === 'HG' || t.id.startsWith('D')) continue;
	walkEvery(t.pts, 150, (x, y, z, yaw) => { if (caveRings >= 10) return; const side = t.kind === 'tube' ? t.r * 0.62 : t.w / 2 - 0.8; kitAdd('glowcap_ring', x + Math.cos((yaw + 90) * DEG) * side, y + Math.sin((yaw + 90) * DEG) * side, z, { cave: true, tube: t.id }); caveRings++; }, 70);
}
// Surface kit (fungal 2.4 counts): host cluster 1, puffball fields 2, summited niche 1, mite nest 1, capling meadows 2,
// shelf deadwood 2, doline lookout 1.
kitAdd('host_cluster', -700, -8742, hAt(-700, -8742), { surface: true });
kitAdd('puffball_field', -2232, -9170, hAt(-2232, -9170), { surface: true });
kitAdd('puffball_field', -2236, -8180, hAt(-2236, -8180), { surface: true });
kitAdd('summited_niche', -1590, -9056, hAt(-1590, -9056), { surface: true, over: 'G8' });
kitAdd('mite_nest', -1420, -9150, hAt(-1420, -9150), { surface: true });
kitAdd('capling_meadow', -1770, -8330, hAt(-1770, -8330), { surface: true, bowl: 'C1' });
kitAdd('capling_meadow', -1676, -8874, hAt(-1676, -8874), { surface: true, bowl: 'C2' });
kitAdd('shelf_deadwood', -2050, -8600, hAt(-2050, -8600), { surface: true });
kitAdd('shelf_deadwood', -1350, -9200, hAt(-1350, -9200), { surface: true });
kitAdd('doline_lookout', -1505, -8662, hAt(-1505, -8662), { surface: true, over: 'G1' });
// Cave kit in their halls (host clusters 6, puffball fields 4, ore pockets 5, summited niches 4, mite nests 3, web
// pockets 3, glowgrub colonies 2, capling meadows 2): the hall each belongs in first.
const CAVE_KIT = [
	['host_cluster', 'H1'], ['host_cluster', 'W2'], ['host_cluster', 'R1'], ['host_cluster', 'W4'], ['host_cluster', null], ['host_cluster', null],
	['puffball_field', 'G5'], ['puffball_field', 'G8'], ['puffball_field', 'H6'], ['puffball_field', null],
	['ore_pocket', 'G6'], ['ore_pocket', 'H5'], ['ore_pocket', 'W6'], ['ore_pocket', 'R7'], ['ore_pocket', null],
	['summited_niche', 'G2'], ['summited_niche', 'W3'], ['summited_niche', 'H4'], ['summited_niche', null],
	['mite_nest', 'G6'], ['mite_nest', 'G7'], ['mite_nest', null],
	['web_pocket', 'H2'], ['web_pocket', 'W2'], ['web_pocket', null],
	['glowgrub_colony', 'H3'], ['glowgrub_colony', 'R2'],
	['capling_meadow', 'G7'], ['capling_meadow', 'G2'],
];
const placeIn = (h, k) => { const a = (k * 97 + 33) % 360 * DEG; return [h.x + Math.cos(a) * h.rx * 0.52, h.y + Math.sin(a) * h.ry * 0.52]; };
const poisPts = [...sites.filter(s => s.cave || s.hall).map(s => [s.x, s.y]), ...HALLS.map(h => [h.x, h.y]), ...shrines.map(s => [s.x, s.y])];
const queue = [];
for (const [kind, hall] of CAVE_KIT) {
	if (hall) { const h = hallById(hall); const [x, y] = placeIn(h, kit.length); kitAdd(kind, x, y, h.walk ?? (h.lake ? h.floor + h.lake : h.floor), { hall, cave: true }); }
	else queue.push(kind);
}
// The alcoves: a little hall cut beside a tube wherever the route runs 125 m from any place (the 125 m rule).
const alcoves = [];
const nearPoi = (x, y) => Math.min(...poisPts.map(p => Math.hypot(p[0] - x, p[1] - y)), ...alcoves.map(a => Math.hypot(a.x - x, a.y - y)), ...kit.filter(k => k.cave).map(k => Math.hypot(k.x - x, k.y - y)));
const routeTubes = tubesDense.filter(t => t.kind === 'tube' && t.zone !== 'HG' && !t.descent && !t.street);
let qi = 0, side = 1;
const FILL = ['host_cluster', 'ore_pocket', 'puffball_field', 'summited_niche', 'mite_nest', 'web_pocket', 'glowgrub_colony'];
for (const t of routeTubes) {
	let acc = 0;
	for (let i = 1; i < t.pts.length; i++) {
		const [ax, ay] = t.pts[i - 1], [bx, by, bz] = t.pts[i];
		acc += Math.hypot(bx - ax, by - ay);
		if (acc < 30) continue;
		if (nearPoi(bx, by) < 118) continue;
		const dx = bx - ax, dy = by - ay, L = Math.hypot(dx, dy) || 1;
		const nx = -dy / L * side, ny = dx / L * side;
		side = -side;
		const kind = qi < queue.length ? queue[qi] : FILL[(qi - queue.length) % FILL.length];
		const ar = 9;
		const off = t.r + ar * 0.75;
		const ax2 = bx + nx * off, ay2 = by + ny * off;
		const id = `A${alcoves.length + 1}`;
		const hall = { id, kind: 'hall', zone: t.zone, name: 'an alcove', x: r1(ax2), y: r1(ay2), floor: r2(bz + 0.2), rx: ar, ry: ar * 0.85, h: 7, yaw: yawOf(nx, ny), alcove: true, tube: t.id, light: 'cap' };
		HALLS.push(hall);
		alcoves.push(hall);
		kitAdd(kind, hall.x, hall.y, hall.floor, { hall: id, cave: true });
		qi++;
		acc = 0;
	}
}
for (const a of alcoves) {
	const t = tubesDense.find(q => q.id === a.tube);
	let best = t.pts[0], bd = 1e9;
	for (const p of t.pts) { const d = Math.hypot(p[0] - a.x, p[1] - a.y); if (d < bd) { bd = d; best = p; } }
	tubesDense.push({ id: `${a.id}-throat`, kind: 'tube', zone: a.zone, r: Math.min(4, t.r * 0.66), pts: densify([[best[0], best[1], best[2]], [a.x, a.y, a.floor]]) });
}

// ---------------------------------------------------------------- resources (fungal 2.7)
const resources = { deepcap: [], puffball_sac: [], glow_moss: [], amadou: [], bootlace_rhizomorph: [], webspinner_silk: [], saltpetre: [], copper_ore: [], tin_ore: [], iron_ore: [], gem: [], oyster_cap: [], lions_mane: [], clay: [] };
const node = (kind, x, y, z, of = '') => resources[kind].push([r1(x), r1(y), r2(z), of]);
// Deepcap only on the dead (KEPT): every host cluster, the Offering Niches, the Feeding Hall, the Breath Vents' bones,
// the Fall Field.
for (const k of kit.filter(q => q.kind === 'host_cluster')) for (let n = 0; n < 1 + (k.v % 3); n++) node('deepcap', k.x + Math.cos(n * 2.1) * 1.6, k.y + Math.sin(n * 2.1) * 1.6, k.z, k.id);
for (let n = 0; n < 3; n++) node('deepcap', -1526 + n * 2.5, -8686 + n * 1.2, 40.6, 'C1');
{ const h = hallById('HG3'); for (let n = 0; n < 6; n++) node('deepcap', h.x + (n - 2.5) * 9 * Math.cos((h.yaw) * DEG), h.y + (n - 2.5) * 9 * Math.sin(h.yaw * DEG), h.floor + 0.5, 'HG3'); }
for (let n = 0; n < 3; n++) node('deepcap', BREATH_VENTS.x + Math.cos(n * 2.3) * 30, BREATH_VENTS.y + Math.sin(n * 2.3) * 30, hAt(BREATH_VENTS.x + Math.cos(n * 2.3) * 30, BREATH_VENTS.y + Math.sin(n * 2.3) * 30), 'S8');
for (let n = 0; n < 4; n++) node('deepcap', FLUE.x + Math.cos(n * 1.7 + 0.4) * 70, FLUE.y + Math.sin(n * 1.7 + 0.4) * 70, flueFloorZ + 0.3, 'L6');
// Puffball sacs at the puffball fields; glow-moss on the Halls' floors; amadou on shelf deadwood; rhizomorph in the Webs.
for (const k of kit.filter(q => q.kind === 'puffball_field')) for (let n = 0; n < 3; n++) node('puffball_sac', k.x + Math.cos(n * 2.1) * 5, k.y + Math.sin(n * 2.1) * 5, k.z, k.id);
for (const h of HALLS.filter(q => q.zone === 'F3' && !q.alcove && q.kind === 'hall')) for (let n = 0; n < Math.min(6, Math.ceil(h.rx / 16)); n++) { const a = n * 1.9 + 0.5; node('glow_moss', h.x + Math.cos(a) * h.rx * 0.45, h.y + Math.sin(a) * h.ry * 0.45, h.floor, h.id); }
for (const k of kit.filter(q => q.kind === 'shelf_deadwood')) for (let n = 0; n < 2; n++) node('amadou', k.x + n * 2.2, k.y + 1, k.z + 0.6, k.id);
for (let n = 0; n < 4; n++) node('amadou', -1500 + Math.cos(n * 1.6) * 26, -8700 + Math.sin(n * 1.6) * 26, 41, 'G1');
for (const id of ['W1', 'W3', 'W2']) { const h = hallById(id); for (let n = 0; n < 4; n++) { const a = n * 1.57 + 0.3; node('bootlace_rhizomorph', h.x + Math.cos(a) * h.rx * 0.55, h.y + Math.sin(a) * h.ry * 0.55, (h.walk ?? h.floor), h.id); } }
{ const h = hallById('W2'); for (let n = 0; n < 4; n++) node('webspinner_silk', h.x + Math.cos(n * 1.5) * 12, h.y + Math.sin(n * 1.5) * 9, h.floor, 'C10'); }
{ const h = hallById('H5'); for (let n = 0; n < 6; n++) node('saltpetre', h.x + (n - 2.5) * 12, h.y + (n % 2 ? 1 : -1) * h.ry * 0.7, h.floor + 1.2, 'C9'); }
// Ores by gallery (fungal 2.6): Upper Vein copper + tin; Nitre iron x2; Deep Vein iron x4 + gems x2; Rimstone iron x2 + gem.
{ const h = hallById('G6'); for (let n = 0; n < 3; n++) node('copper_ore', h.x + (n - 1) * 9, h.y - h.ry * 0.6, h.floor, 'upper_vein'); for (let n = 0; n < 2; n++) node('tin_ore', h.x + (n - 0.5) * 12, h.y + h.ry * 0.6, h.floor, 'upper_vein'); }
{ const h = hallById('H5'); for (let n = 0; n < 2; n++) node('iron_ore', h.x + (n ? 22 : -22), h.y, h.floor, 'nitre'); }
{ const h = hallById('W6'); for (let n = 0; n < 4; n++) node('iron_ore', h.x + (n - 1.5) * 12, h.y + (n % 2 ? 7 : -7), h.floor, 'deep_vein'); for (let n = 0; n < 2; n++) node('gem', h.x + (n ? 20 : -20), h.y + 10, h.floor, 'deep_vein'); }
{ const h = hallById('R7'); for (let n = 0; n < 2; n++) node('iron_ore', h.x + (n ? 8 : -8), h.y + 6, h.floor + 1, 'rimstone'); node('gem', h.x, h.y - 8, h.floor + 1, 'rimstone'); }
for (const k of kit.filter(q => q.kind === 'ore_pocket' && !['G6', 'H5', 'W6', 'R7'].includes(q.hall))) node(k.z < -100 ? 'iron_ore' : k.z < 0 ? 'iron_ore' : 'copper_ore', k.x, k.y, k.z, k.id);
// Wild oyster caps and lion's mane in the Gardens and the Cloud Room (C4); clay at the Titan Nursery.
for (const id of ['G1', 'G2', 'G7', 'G4']) { const h = hallById(id); node('oyster_cap', h.x + h.rx * 0.6, h.y - h.ry * 0.3, h.floor + 0.5, id); }
{ const h = hallById('G5'); for (let n = 0; n < 3; n++) node('lions_mane', h.x + Math.cos(n * 2) * 30, h.y + Math.sin(n * 2) * 22, h.floor + 0.5, 'C4'); }
{ const h = hallById('W4'); for (let n = 0; n < 3; n++) node('clay', h.x + (n - 1) * 10, h.y + h.ry * 0.5, h.floor, 'C12'); }

// ---------------------------------------------------------------- hazards' places (fungal 2.8)
// Spore clouds: puffball fields, host clusters, the Breath Vents' out-breath, every third ledge of D6's spiral. Never
// within 4 m of a ledge, a jump or a springcap (checked at runtime too).
const clouds = [];
for (const k of kit.filter(q => q.kind === 'puffball_field')) clouds.push({ x: k.x, y: k.y, z: r2(k.z + 2), r: 8 + (k.v * 2), kind: 'puffball', of: k.id });
for (const k of kit.filter(q => q.kind === 'host_cluster' && q.cave)) clouds.push({ x: r1(k.x + 4), y: k.y, z: r2(k.z + 2), r: 6, kind: 'host', of: k.id });
clouds.push({ x: BREATH_VENTS.x, y: BREATH_VENTS.y, z: r2(hAt(BREATH_VENTS.x, BREATH_VENTS.y) + 2.5), r: 14, kind: 'breath', of: 'S8', breathe: true });
for (let l = 2; l < 12; l += 3) clouds.push({ x: r1(-662 + Math.cos(l * 50 * DEG) * 12.5), y: r1(-8528 + Math.sin(l * 50 * DEG) * 12.5), z: r2(lerp(hAt(-662, -8528), -80, l / 12) + 2), r: 5, kind: 'ledge', of: 'D6' });
// Lure lines (F3 ceilings): the Lure Gallery thick, the False Sky's low lines over its routes (with crouch gaps), web pockets.
const lines = [];
{
	const h = hallById('H2');
	lines.push({ hall: 'H2', x: h.x, y: h.y, rx: h.rx * 0.9, ry: h.ry * 0.7, yaw: h.yaw ?? 0, floor: h.floor, roof: h.floor + h.h, density: 0.34, low: 1.6 });
	const s = hallById('H1');
	lines.push({ hall: 'H1', x: s.x, y: s.y, rx: s.rx * 0.8, ry: s.ry * 0.75, yaw: s.yaw ?? 0, floor: s.floor, roof: s.floor + s.h, density: 0.012, low: 1.7, stars: 6000 });
	for (const k of kit.filter(q => q.kind === 'web_pocket')) { const hh = hallById(k.hall); lines.push({ hall: k.hall, x: k.x, y: k.y, rx: 10, ry: 8, yaw: 0, floor: k.z, roof: k.z + (hh ? Math.min(hh.h, 10) : 7), density: 0.25, low: 1.6, web: k.id }); }
}
// Springcaps (fungal 2.8): the Grove's five-bounce route up to its ledge cache, D4's shelves, the Toad Leaps' far walls.
const springcaps = [];
{
	const h = hallById('G4');
	for (let s = 0; s < 5; s++) { const a = (200 + s * 26) * DEG, rr = h.rx * (0.55 + s * 0.08); springcaps.push({ x: r1(h.x + Math.cos(a) * rr), y: r1(h.y + Math.sin(a) * rr), z: r2(h.floor + s * 9), of: 'C3', ledge: s > 0 }); }
	springcaps.push({ x: r1(h.x + 5), y: r1(h.y + 4), z: h.floor, of: 'C3' });
	springcaps.push({ x: r1(h.x - 7), y: r1(h.y + 9), z: h.floor, of: 'C3' });
	const d = DESCENTS.find(q => q.id === 'D4');
	for (let s = 0; s < 4; s++) springcaps.push({ x: r1(d.x + Math.cos(s * 90 * DEG) * 6.2), y: r1(d.y + Math.sin(s * 90 * DEG) * 6.2), z: r2(-20 + s * 11), of: 'D4', ledge: true });
	const t = hallById('R6');
	for (let s = 0; s < 3; s++) springcaps.push({ x: r1(t.x - t.rx * 0.6 + s * 40), y: r1(t.y + t.ry * 0.82), z: -40, of: 'C17' });
}
// Cap-fall ceilings (F4: the Deep Vein, the Root Ceiling's edges, the Titan Nursery's back).
const capfalls = [];
for (const [id, n] of [['W6', 4], ['W3', 3], ['W4', 2], ['W2', 1]]) { const h = hallById(id); for (let s = 0; s < n; s++) { const a = (s * 113 + 40) * DEG; const x = h.x + Math.cos(a) * h.rx * 0.5, y = h.y + Math.sin(a) * h.ry * 0.5; capfalls.push({ x: r1(x), y: r1(y), z: r2(h.walk ?? h.floor), roof: r2((h.walk ?? h.floor) + Math.min(h.h, 18) * 0.9), hall: id }); } }
// Foul air: the Air Bell, the Rimstone Pocket, the moat's flooded crawl.
const foul = [];
for (const h of HALLS.filter(q => q.foul)) foul.push({ x: h.x, y: h.y, z: h.floor, rx: h.rx, ry: h.ry, h: h.h, hall: h.id });
{ const t = tubesDense.find(q => q.id === 'moat_crawl'); const a = t.pts[0], b = t.pts[t.pts.length - 1]; foul.push({ x: r1((a[0] + b[0]) / 2), y: r1((a[1] + b[1]) / 2), z: r2(Math.min(a[2], b[2])), rx: r1(Math.hypot(b[0] - a[0], b[1] - a[1]) / 2 + 4), ry: 5, h: 4, yaw: yawOf(b[0] - a[0], b[1] - a[1]), tube: 'moat_crawl' }); }
// The Breath vents (fungal 2.8, biome-sky KEPT): three bellows-stones 40 m apart round the Fall Field's centre.
const vents = [0, 1, 2].map(i => ({ i, x: r1(FLUE.x + Math.cos((90 + i * 120) * DEG) * 23), y: r1(FLUE.y + Math.sin((90 + i * 120) * DEG) * 23), z: flueFloorZ }));
// Flooded crawls and lakes (WRWater volumes): every grotto's lake, every flooded crawl.
const water = [];
for (const h of HALLS.filter(q => q.lake)) water.push({ hall: h.id, x: h.x, y: h.y, rx: h.rx, ry: h.ry, yaw: h.yaw || 0, top: r2(h.floor + h.lake), bottom: r2(h.floor - 0.5) });
for (const t of tubesDense.filter(q => q.flooded)) { const a = t.pts[0], b = t.pts[t.pts.length - 1]; water.push({ tube: t.id, x: r1((a[0] + b[0]) / 2), y: r1((a[1] + b[1]) / 2), rx: r1(Math.hypot(b[0] - a[0], b[1] - a[1]) / 2 + 3), ry: r1(t.w / 2), yaw: yawOf(b[0] - a[0], b[1] - a[1]), top: r2(Math.min(a[2], b[2]) + t.flooded), bottom: r2(Math.min(a[2], b[2]) - 0.5) }); }
for (const t of tubesDense.filter(q => q.river)) for (let q = 0; q + 6 < t.pts.length; q += 6) { const a = t.pts[q], b = t.pts[q + 6]; water.push({ tube: t.id, x: r1((a[0] + b[0]) / 2), y: r1((a[1] + b[1]) / 2), rx: r1(Math.hypot(b[0] - a[0], b[1] - a[1]) / 2 + 1), ry: r1(t.r * 0.55), yaw: yawOf(b[0] - a[0], b[1] - a[1]), top: r2(Math.min(a[2], b[2]) + 0.7), bottom: r2(Math.min(a[2], b[2]) - 0.5), river: true }); }

// ---------------------------------------------------------------- dens (fungal 2.12.3 day and night tables)
const DEN_TABLES = {
	F1: { day: { capling: 16, puffball: 10, truffle_mole: 8, shroomback: 6, mycomite: 4 }, night: { tunnel_toad: 10, capling: 8, mycomite: 8, sporestalker: 2 } },
	F2: { day: { capling: 14, shroomback: 8, puffball: 8, mycomite: 8, glowgrub: 4, rotmaw: 2 }, night: { mycomite: 12, glowgrub: 8, rotmaw: 4, webspinner: 2 } },
	F3: { day: { webspinner: 8, glowgrub: 10, truffle_mole: 6, mycomite: 8, puffball: 4 }, night: { webspinner: 12, mycomite: 10, sporestalker: 4, glowgrub: 8 } },
	F4: { day: { webspinner: 10, sporestalker: 8, mycomite: 8 }, night: { sporestalker: 12, webspinner: 10, mycomite: 10 } },
	F5: { day: { tunnel_toad: 12, glowgrub: 8, rotmaw: 6, shroomback: 4 }, night: { rotmaw: 10, tunnel_toad: 8, mycomite: 8, sporestalker: 3 } },
};
const dens = [];
// Surface dens: a Poisson spread over F1 every ~230 m; cave dens in every hall but the arenas, the shrines' and HG's.
for (let y = -9380; y < -8100; y += 230) for (let x = -2200; x < -500; x += 230) {
	const jx = x + 70 * fbm(x / 50, y / 50, 2), jy = y + 70 * fbm(x / 50 + 3, y / 50, 2);
	if (!owned(jx, jy) || coastDist(jx, jy) < 30 || zoneAt(jx, jy) !== '1') continue;
	if (CAMPS.some(c => Math.hypot(c.x - jx, c.y - jy) < 120)) continue;
	if (MOUTHS.some(m => Math.hypot(m.x - jx, m.y - jy) < m.r + 15)) continue;
	dens.push({ x: r1(jx), y: r1(jy), z: r2(hAt(jx, jy)), zone: 'F1', spread: 30 });
}
for (const h of HALLS) {
	if (h.arena || h.zone === 'HG' || h.alcove || h.shrine === 'garden_doline' || h.id === 'H7') continue;
	dens.push({ x: h.x, y: h.y, z: h.walk ?? (h.lake ? h.floor + h.lake : h.floor), zone: h.zone, spread: Math.min(h.rx, h.ry) * 0.6, hall: h.id, cave: true });
}
// The Ring of Caps' toads (night), and the Heart Grotto's rooftop Sporestalkers (fixed: 2 on the Lantern Street).
dens.push({ x: RING_OF_CAPS.x, y: RING_OF_CAPS.y, z: r2(hAt(RING_OF_CAPS.x, RING_OF_CAPS.y)), zone: 'ring', spread: 40, table: { night: { tunnel_toad: 1 } } });
{ const h = hallById('HG1'); dens.push({ x: h.x, y: h.y, z: h.floor, zone: 'HG', spread: 20, hall: 'HG1', cave: true, table: { day: { sporestalker: 1 }, night: { sporestalker: 1 } }, max: 2 }); }
{ const h = hallById('HG5'); dens.push({ x: h.x, y: h.y, z: h.floor, zone: 'HG', spread: 12, hall: 'HG5', cave: true, table: { day: { mycomite: 1 }, night: { mycomite: 1 } }, max: 10 }); }

// ---------------------------------------------------------------- arenas (fungal 2.13, 2.14)
// The Sporemother's six egg sacs on the Brood Doline's wall at 2-6 m.
const g3 = hallById('G3');
const sacs = [0, 1, 2, 3, 4, 5].map(i => ({ i, a: i * 60 + 15, x: r1(g3.x + Math.cos((i * 60 + 15) * DEG) * (g3.rx - 1.6)), y: r1(g3.y + Math.sin((i * 60 + 15) * DEG) * (g3.rx - 1.6)), z: r2(g3.floor + 2 + (i % 3) * 1.8) }));
// The Rot King's eight vats (four full of slurry), the clean channels round the edge.
const r5 = hallById('R5');
const vats = [];
for (let i = 0; i < 8; i++) { const col = i % 4, row = Math.floor(i / 4); const lx = (col - 1.5) * 15, ly = (row - 0.5) * 12; const c = Math.cos(r5.yaw * DEG), s = Math.sin(r5.yaw * DEG); vats.push({ i, x: r1(r5.x + lx * c - ly * s), y: r1(r5.y + lx * s + ly * c), z: r5.floor, r: 3, full: [0, 2, 5, 7].includes(i) }); }
// Mycelorn's arena: terraces, Draught gates, the vein graph, the pool, the altar.
const terraces = [0, 1, 2, 3, 4, 5].map(i => ({ i, a: i * 60 + 30, x: r1(HC.x + Math.cos((i * 60 + 30) * DEG) * 50), y: r1(HC.y + Math.sin((i * 60 + 30) * DEG) * 50), z: HC.floor, w: 10, h: 1.5 }));
const gates = [['N', 270], ['E', 0], ['S', 90], ['W', 180]].map(([id, a]) => ({ id, a, x: r1(HC.x + Math.cos(a * DEG) * 59), y: r1(HC.y + Math.sin(a * DEG) * 59), z: HC.floor }));
// The vein graph: a centre node, an inner ring of 6, an outer ring of 12, joined as the mycelium grows (Physarum).
const veinNodes = [[HC.x, HC.y]];
for (let i = 0; i < 6; i++) veinNodes.push([r1(HC.x + Math.cos(i * 60 * DEG) * 20), r1(HC.y + Math.sin(i * 60 * DEG) * 20)]);
for (let i = 0; i < 12; i++) veinNodes.push([r1(HC.x + Math.cos((i * 30 + 15) * DEG) * 38), r1(HC.y + Math.sin((i * 30 + 15) * DEG) * 38)]);
const veinEdges = [];
for (let i = 0; i < 6; i++) { veinEdges.push([0, 1 + i]); veinEdges.push([1 + i, 1 + (i + 1) % 6]); veinEdges.push([1 + i, 7 + 2 * i]); veinEdges.push([1 + i, 7 + (2 * i + 11) % 12]); }
for (let i = 0; i < 12; i++) veinEdges.push([7 + i, 7 + (i + 1) % 12]);
const pods = [];
for (let i = 0; i < 12; i++) { const a = (i * 30 + 5) * DEG, rr = 28 + (i % 3) * 5; pods.push({ i, x: r1(HC.x + Math.cos(a) * rr), y: r1(HC.y + Math.sin(a) * rr) }); }
// The Weaver's route (fungal 2.13.3): her lair, then the halls of her loop, walked on their ceilings.
const weaverRoute = ['H1', 'H6', 'H1', 'H2', 'H3', 'W1', 'W2', 'W1', 'H3', 'H1'].map(id => { const h = hallById(id); return [h.x, h.y, r2((h.walk ?? h.floor) + Math.min(h.h * 0.85, 40))]; });

// ---------------------------------------------------------------- lore and secrets (fungal 2.20)
const lore = [
	{ item: 'lore_terrace_tally', site: 'S1' }, { item: 'knocker_idol', site: 'S2' }, { item: 'lore_spore_print_jar', site: 'C8' },
	{ item: 'lore_titan_mould_sherd', site: 'C12' }, { item: 'lore_divers_bell', site: 'C15' }, { item: 'lore_kite_harness_token', site: 'S6' },
	{ item: 'lore_champions_kite_token', site: 'C18' }, { item: 'lore_elders_bowl', site: 'HG4' }, { item: 'lore_heart_seed_husk', site: 'HG7' },
];
const secrets = [
	{ id: 'F-S1', name: "the ring's door", site: 'L3', x: RING_OF_CAPS.x, y: RING_OF_CAPS.y, z: r2(hAt(RING_OF_CAPS.x, RING_OF_CAPS.y)), need: 'midnight_toad' },
	{ id: 'F-S2', name: 'the fifth tunnel', site: 'C7', hall: 'H7', need: 'mole_knock' },
	{ id: 'F-S3', name: 'the dark star', site: 'L4', x: r1(hallById('H1').x + 120), y: r1(hallById('H1').y - 40), z: r2(hallById('H1').floor + 52), need: 'climb' },
	{ id: 'F-S4', name: 'behind the Sand Falls', site: 'L8', x: r1(hallById('H6').x - 2), y: r1(hallById('H6').y + 46), z: r2(hallById('H6').floor + 6), need: 'noon' },
	{ id: 'F-S5', name: 'the Seed Vault', site: 'HG7', hall: 'HG7', need: 'sporefall' },
];
// The ring's door's stair leads to a sealed reliquary under the plateau: a small hall in F2 (the stair is a shaft).
HALLS.push({ id: 'RQ', kind: 'hall', zone: 'F2', name: 'the Deepfarmer reliquary', x: RING_OF_CAPS.x + 16, y: RING_OF_CAPS.y + 10, floor: r2(hAt(RING_OF_CAPS.x, RING_OF_CAPS.y) - 22), rx: 8, ry: 6, h: 5, secret: 'F-S1', light: 'none' });
SHAFTS.push({ id: 'ring_stair', kind: 'stair', x: RING_OF_CAPS.x + 4, y: RING_OF_CAPS.y + 2, r: 2.6, z0: r2(hAt(RING_OF_CAPS.x, RING_OF_CAPS.y) - 22), z1: r2(hAt(RING_OF_CAPS.x, RING_OF_CAPS.y) - 1.2), hall: 'RQ', how: 'altar', sealed: true });
tubesDense.push({ id: 'RQ-stair', kind: 'tube', zone: 'F2', r: 2.8, pts: densify([[RING_OF_CAPS.x + 4, RING_OF_CAPS.y + 2, hAt(RING_OF_CAPS.x, RING_OF_CAPS.y) - 22], [RING_OF_CAPS.x + 16, RING_OF_CAPS.y + 10, hAt(RING_OF_CAPS.x, RING_OF_CAPS.y) - 22]]) });

// ---------------------------------------------------------------- write
const C = c => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];
const ZC = 16, ZN = Math.ceil((N - 1) * STEP / ZC) + 1;
const zoneRows = [];
for (let j = 0; j < ZN; j++) { let row = ''; for (let i = 0; i < ZN; i++) row += zoneAt(X0 + i * ZC, Y0 + j * ZC); zoneRows.push(row); }
function writeLand() {
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'fungal_ground.bin'), Buffer.concat([hb, sb]));
	const ways = [];
	for (const h of HALLS) {
		const c = Math.cos((h.yaw || 0) * DEG), s = Math.sin((h.yaw || 0) * DEG), hl = Math.max(0, h.rx - h.ry);
		ways.push([r1(h.x - c * hl), r1(h.y - s * hl), r1(h.x + c * hl + 0.1), r1(h.y + s * hl), r1(Math.min(h.rx, h.ry) + 3)]);
	}
	for (const t of tubesDense) { const rr = (t.kind === 'tube' ? t.r : t.w / 2) + 3; for (let q = 0; q < t.pts.length - 1; q += 4) { const a = t.pts[q], b = t.pts[Math.min(t.pts.length - 1, q + 4)]; ways.push([a[0], a[1], b[0], b[1], r1(rr)]); } }
	for (const s of SHAFTS) ways.push([s.x, s.y, s.x + 0.1, s.y, r1(s.r + 2)]);
	const campRows = CAMPS.map(c => [c.x, c.y, (c.yaw + 180) % 360]);
	const land = {
		_comment: 'Generated by Tools/terrain/fungal.mjs (edit the script, not this file). The Fungal caverns\' surface as a runtime land (WRLand, cooks/build-biome-fungal.md 2.0-2.1): the Sinkhole Rim, a karst plateau on the Hollow Cape\'s west half, its ground the heightfield in fungal_ground.bin (4 m samples from x0, y0 in map metres: uint16 heights = offset + m * unitsPerMetre, then one surface byte each), built as procedural-mesh chunks on every machine. Game cm = map metres x 100 (the Sky land\'s frame). priority 1 and cuts: the Sky land\'s west downs and the Flue\'s outer crown are this land\'s while it is live (the cut boxes\' edges are on both grids; the samples on them are the Sky\'s own heights, so the two grounds share that ring; the Sky keeps a stepped ring about 300 m round the Flue\'s shaft). Colours (sRGB): zones by the zone grid letter (1 the Sinkhole Rim, s the Spine\'s foot, d the Desert\'s north strip, ~ the sea), surfaces by class (limestone pavement with its grykes, salt-grass, sea thrift, cliffs, shingle, sand, the Deepfarmers\' worn paths, moss round the doline lips, the Breath Vents\' spore crust, the Ring of Caps\' clean turf, the Spine\'s grey). underground.ways: every hall, tube and shaft of the caverns (no rescue there). camps: PlayerStarts tagged fungal on the three sinkhole rims, a safe zone each, the starter stations at the middle one. crossing: the Spore Crossing from the island\'s north shore to North Tip\'s cove.',
		id: 'fungal', biome: 'fungal', name: 'the Fungal caverns', priority: 1,
		origin: ORIGIN, mapCentre: MAPC,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 1200000, chunksPerFrame: 4,
		seaFloor: -16,
		heightfield: { file: 'fungal_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 40,
		zones: [
			{ id: 'F1', letter: '1', name: 'the Sinkhole Rim', ground: C('#A7A498'), groundAlt: C('#8E8C80'), rough: 0.88, region: 'fungal_f1', bed: 'fungal_f1', speck: { colors: [C('#7E8C5A'), C('#5E6058')], amount: 0.1, scale: 2.5 } },
			{ id: 'spine', letter: 's', name: "the Spine's foot", ground: C('#8A8780'), groundAlt: C('#6E6C66'), rough: 0.86, region: 'fungal_f1', bed: 'fungal_f1', speck: { colors: [C('#C9C2B0')], amount: 0.06, scale: 3 } },
			{ id: 'desert_edge', letter: 'd', name: "the Desert's edge", ground: C('#D8C08A'), groundAlt: C('#C8AE78'), rough: 0.94, region: 'fungal_f1', bed: 'fungal_f1', speck: { colors: [C('#E8D8A8'), C('#B89A62')], amount: 0.12, scale: 3 } },
			{ id: 'sea', letter: '~', name: 'the sea floor', ground: C('#1E2A2E'), groundAlt: C('#283438'), rough: 0.8 },
		],
		surfaces: [
			{ id: 'pave', class: SURF.pave, color: C('#A7A498'), rough: 0.84, speck: { colors: [C('#5A5850'), C('#C8C4B6')], amount: 0.16, scale: 1.1 } },
			{ id: 'grass', class: SURF.grass, color: C('#7E8C5A'), rough: 0.93, speck: { colors: [C('#9AA46A'), C('#5E6A40')], amount: 0.14, scale: 1.8 } },
			{ id: 'thrift', class: SURF.thrift, color: C('#748456'), rough: 0.93, speck: { colors: [C('#E6A6C0'), C('#F0D8E0')], amount: 0.22, scale: 1.2 } },
			{ id: 'cliff', class: SURF.cliff, color: C('#8E8B82'), rough: 0.84, speck: { colors: [C('#5E5C56'), C('#D8D4CC')], amount: 0.08, scale: 2 } },
			{ id: 'shingle', class: SURF.shingle, color: C('#77746C'), rough: 0.6, speck: { colors: [C('#A8A498'), C('#4E5F66')], amount: 0.2, scale: 1.4 } },
			{ id: 'sand', class: SURF.sand, color: C('#D8C08A'), rough: 0.95, speck: { colors: [C('#E8D8A8')], amount: 0.1, scale: 2 } },
			{ id: 'path', class: SURF.path, color: C('#7A6E5C'), rough: 0.96, speck: { colors: [C('#A7A498')], amount: 0.12, scale: 1.2 } },
			{ id: 'moss', class: SURF.moss, color: C('#5E7A3A'), rough: 0.95, speck: { colors: [C('#4A6A30'), C('#8AA05A')], amount: 0.2, scale: 1.4 } },
			{ id: 'crust', class: SURF.crust, color: C('#B8B8A8'), rough: 0.9, speck: { colors: [C('#9AD8C8'), C('#8E8C80')], amount: 0.14, scale: 1.2 } },
			{ id: 'ring', class: SURF.ring, color: C('#6E8A4E'), rough: 0.92, speck: { colors: [C('#8AA866')], amount: 0.1, scale: 2 } },
			{ id: 'spine', class: SURF.spine, color: C('#8A8780'), rough: 0.86 },
			{ id: 'sea', class: SURF.sea, color: C('#1E2A2E'), rough: 0.8 },
		],
		slopeRock: { color: C('#8E8B82'), fromDeg: 38, toDeg: 52 },
		underground: { letters: '', ways },
		buildFirst: campRows.map(([x, y]) => [x, y]),
		cuts: CUTS.map(([a, b, c, d]) => ({ land: 'sky', box: [a, b, c, d] })),
		camps: { tag: 'fungal', safeRadius: 40, list: campRows, stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
		crossing: { _comment: 'The Spore Crossing (cooks/build-biome-fungal.md 2.0.8): a longboat on the island\'s north shore and its twin beached in North Tip\'s cove under the sea cliff. Hold E to row across. island: metres on the island (settled to the shore at runtime); land: map metres.', island: [-600, -2150], islandFace: 180, land: [-1100, -9452], landFace: 270, holdSeconds: 2, fadeSeconds: 1.2, name: 'the Spore Crossing' },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'fungal.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}
writeLand();
{
	const tubesOut = tubesDense.map(t => ({ id: t.id, kind: t.kind, zone: t.zone, r: t.r, w: t.w, h: t.h, blend: t.blend, door: t.door, descent: t.descent, river: !!t.river, flooded: t.flooded, sealed: !!t.sealed, mole: !!t.mole, secret: t.secret, street: !!t.street, landings: !!t.landings, flue: !!t.flue, pts: t.pts }));
	const flueTube = tubesDense.find(t => t.flue);
	const mouthPt = flueTube.pts[flueTube.pts.length - 1];
	const out = {
		_comment: 'Generated by Tools/terrain/fungal.mjs (edit the script). The Fungal caverns\' contract (cooks/build-biome-fungal.md 2.2-2.14), parsed by FWRFungalData (WRFungalTypes.cpp). Map metres (+z up), world cm = map x 100 on the land\'s frame. halls: kind hall (an ellipsoid with a flat floor) | doline (an open-topped cylinder from its floor through the ground, rx its radius) | chasm (a deep hall whose walkways are at walk m, its floor the talus bottom) | grotto (a hall with a lake lake m deep) | dome (the Heart chamber); tubes: kind tube (round, r) | crawl (low and wide, w x h) | stair (a hewn passage, w x h, floor stepping); shafts: descent | mouth | sieve | stair. sites / kit / shrines / dens / resources / clouds / lines / springcaps / capfalls / foul / vents / water / arenas: placed by the game from these rows.',
		flue: { x: FLUE.x, y: FLUE.y, floor: flueFloorZ, r: 133, mouthYaw: r1(Math.atan2(-8789 - FLUE.y, -137 - FLUE.x) / DEG), mouthHalf: 0, mouthTop: 16 },
		halls: HALLS, tubes: tubesOut, shafts: SHAFTS, descents: DESCENTS, sites, shrines, runback, kit, routes: ROUTES, resources,
		clouds, lines, springcaps, capfalls, foul, vents, water, dens, denTables: DEN_TABLES, lore, secrets,
		great: GREAT, sieves: SIEVES, ringOfCaps: RING_OF_CAPS, breathVents: BREATH_VENTS, cloudMouth: CLOUD_MOUTH,
		arenas: {
			sporemother: { hall: 'G3', sacs },
			rot_king: { hall: 'R5', vats },
			weaver: { lair: 'H1', route: weaverRoute },
			mycelorn: { hall: 'HC', pool: { x: HC.x, y: HC.y, r: 20 }, terraces, gates, veins: { nodes: veinNodes, edges: veinEdges }, pods, altar: { x: terraces[1].x, y: terraces[1].y } },
		},
	};
	fs.writeFileSync(path.join(root, 'Data', 'fungal.json'), JSON.stringify(out) + '\n');
}
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.fungal);
		for (const s of sites) {
			const kind = s.camp ? 'camp' : s.landmark ? 'landmark' : s.kind === 'descent' ? 'cave' : null;
			if (!kind) continue;
			const [wx, wy, wz] = toWorld(s.x, s.y, s.z);
			dress.pois.push({ name: s.name, kind, x: wx, y: wy, z: wz, biome: 'fungal', fungal: true });
		}
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

// ---------------------------------------------------------------- preview (.lane-tmp, not committed)
{
	const img = Buffer.alloc(N * N * 3);
	const zc = { '1': [150, 148, 136], 's': [138, 135, 128], 'd': [216, 192, 138], '~': [24, 40, 56] };
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const k = j * N + i, x = X0 + i * STEP, y = Y0 + j * STEP;
		let c = (zc[zoneAt(x, y)] || [0, 0, 0]).slice();
		const s = SURFM[k];
		if (s === SURF.grass) c = [126, 140, 90]; else if (s === SURF.moss) c = [94, 122, 58]; else if (s === SURF.cliff) c = [110, 108, 100];
		else if (s === SURF.path) c = [122, 110, 92]; else if (s === SURF.crust) c = [160, 200, 190]; else if (s === SURF.hidden) c = owned(x, y) ? [255, 0, 255] : [60, 60, 70];
		const sh = Math.max(0.45, Math.min(1.5, 1 + (HF[k] - (HF[k - 1] ?? HF[k])) * 0.25 + HF[k] / 600));
		if (HF[k] < 0 && s !== SURF.hidden) c = [24, 40, 56];
		img[k * 3] = Math.min(255, c[0] * sh); img[k * 3 + 1] = Math.min(255, c[1] * sh); img[k * 3 + 2] = Math.min(255, c[2] * sh);
	}
	const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / STEP), cj = Math.round((y - Y0) / STEP); for (let a = -r; a <= r; a++) for (let b = -r; b <= r; b++) { const ii = ci + a, jj = cj + b; if (ii >= 0 && jj >= 0 && ii < N && jj < N) { const k = jj * N + ii; img[k * 3] = col[0]; img[k * 3 + 1] = col[1]; img[k * 3 + 2] = col[2]; } } };
	const zcol = { F2: [230, 200, 120], F3: [57, 230, 194], F4: [232, 160, 64], F5: [30, 140, 134], HG: [255, 120, 40] };
	for (const t of tubesDense) for (const p of t.pts) dot(p[0], p[1], 0, zcol[t.zone] || [255, 255, 255]);
	for (const h of HALLS) dot(h.x, h.y, Math.max(1, Math.round(Math.min(h.rx, h.ry) / 8)), zcol[h.zone] || [255, 255, 255]);
	for (const k of kit) dot(k.x, k.y, 1, [255, 255, 255]);
	for (const s of sites) dot(s.x, s.y, s.landmark ? 4 : 2, s.landmark ? [255, 80, 200] : [255, 255, 0]);
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
	fs.writeFileSync(path.join(root, '.lane-tmp', 'fungal.png'), png(N, N, img));
}
let landA = 0, maxH = -1e9; for (let k = 0; k < N * N; k++) { if (HF[k] > 0.5 && SURFM[k] !== SURF.hidden) landA++; maxH = Math.max(maxH, SURFM[k] !== SURF.hidden ? HF[k] : -1e9); }
const poiCount = sites.length + kit.length + shrines.length;
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, landKm2: +(landA * STEP * STEP / 1e6).toFixed(2), highest: Math.round(maxH), halls: HALLS.length, alcoves: alcoves.length, tubes: tubesDense.length, shafts: SHAFTS.length, sites: sites.length, kit: kit.length, pois: poiCount, clouds: clouds.length, springcaps: springcaps.length, capfalls: capfalls.length, dens: dens.length, deepcap: resources.deepcap.length }));
