// The Crystal caves: the east half of belt B6, east of the Spine and across the Severance from the Corrupted wasteland
// (cooks/biome-crystal.md 2.1-2.11, 2.20; cooks/build-biome-crystal.md 2.0-2.13; cooks/world-map.md 2.2). Map metres:
// +x east, +y south. The land sits on the Wasteland's frame (origin (0, 900000), mapCentre (-850, +5,300): build cook 2.0.2)
// so the Severance runs from one land into the other; the B6 belt moves to real metres as one piece later. Writes
//   Data/lands/crystal.json        the runtime land (WRLand): frame, heightfield, zone grid, surfaces, cave ways, camps,
//                                  the Chime Crossing
//   Data/lands/crystal_ground.bin  uint16 heights (32768 + m * 16) row by row, then one surface byte per sample
//   Data/crystal.json              THE contract parsed into FWRCrystalData (Source/Wildreach/WRCrystalTypes.cpp): the cave
//                                  network (halls, tubes, passages, shafts, pools), sites, kit POIs, shrines, roads and
//                                  chime stones, domes, vents, ward fields and keystones, chandeliers, thin floors,
//                                  resources, lore, arenas, run-back stones; generated, edit this script
//   Data/island_dressing.json      only its "pois" (the map's place list): camps, descents, landmarks
//   .lane-tmp/crystal.png          a shaded layout preview (1 px = 4 m; not committed)
// node Tools/terrain/crystal.mjs
//
// The caves are air shapes (build cook 2.3): every hall an ellipsoid with a flat floor, every tube a swept circle with a
// flat floor and old-lava benches, every Wardcarver passage a hewn box section, every shaft a cylinder. The game unions
// them and meshes the boundary (WRCrystalCaves.cpp, surface nets), so every junction is closed by construction. This
// script checks that no cave roof comes within 5 m of the ground (or the sea floor) except where a descent opens it.
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';
import * as Waste from './wasteland_ground.mjs';

const t0 = Date.now();
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
export const X0 = 1400, Y0 = 4520, STEP = 4, N = 491, UNITS = 16;
const X1 = X0 + (N - 1) * STEP, Y1 = Y0 + (N - 1) * STEP;
const SEED = 7219;
const ORIGIN = Waste.W.origin, MAPC = Waste.W.mapCentre;
const toWorld = (x, y, z) => [Math.round(ORIGIN[0] + (x - MAPC[0]) * 100), Math.round(ORIGIN[1] + (y - MAPC[1]) * 100), Math.round(ORIGIN[2] + z * 100)];

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
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 7219;
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
const polyDist = (x, y, P) => { let b = 1e9; for (let i = 0, j = P.length - 1; i < P.length; j = i++) b = Math.min(b, distSeg(x, y, P[j][0], P[j][1], P[i][0], P[i][1]).d); return inPoly(x, y, P) ? b : -b; };

// ---------------------------------------------------------------- the coast (crystal 2.1: black basalt sea cliffs)
// Drawn by hand round the plateau, then roughened. The west side between y +5,250 and +5,600 lies inside the
// Wasteland's Spine tip (its ground stands there); north and south of it the Spine's east face drops into the sea.
const COAST_RAW = [
	[1600, 4650], [1700, 4615], [1850, 4628], [2000, 4602], [2150, 4618], [2300, 4632], [2385, 4655],
	[2428, 4705], [2446, 4800], [2440, 4950], [2452, 5100], [2440, 5250], [2456, 5400], [2446, 5550], [2452, 5700], [2441, 5850], [2431, 6000], [2402, 6120], [2342, 6232],
	[2262, 6312], [2160, 6356], [2050, 6392], [1920, 6420], [1780, 6432], [1650, 6420],
	[1566, 6350], [1544, 6100], [1548, 5800], [1522, 5640], [1398, 5560], [1398, 5300], [1522, 5250], [1546, 5000], [1556, 4800], [1580, 4690],
];
const COAST = [];
for (let i = 0; i < COAST_RAW.length; i++) {
	const a = COAST_RAW[i], b = COAST_RAW[(i + 1) % COAST_RAW.length];
	const L = Math.hypot(b[0] - a[0], b[1] - a[1]), n = Math.max(1, Math.round(L / 12));
	const nx = -(b[1] - a[1]) / L, ny = (b[0] - a[0]) / L;
	for (let k = 0; k < n; k++) {
		const t = k / n, x = lerp(a[0], b[0], t), y = lerp(a[1], b[1], t);
		// The gorge strip and the Wasteland seam stay exactly where drawn; the sea cliffs wander.
		const wild = x < 1530 && y > 5240 && y < 5650 ? 0 : 1;
		const off = wild * (14 * fbm(x / 160, y / 160, 4) + 5 * fbm(x / 35 + 9, y / 35, 3));
		COAST.push([x + nx * off, y + ny * off]);
	}
}

// Camps (crystal 2.1, KEPT positions): each at the head of a geo (a long narrow sea inlet cut into the basalt cliff, a
// geode the sea broke open), its castaway wreck in the cove below, cliff stairs down.
const CAMPS = [
	{ id: 'K1', name: 'the North Geo', x: 2250, y: 4900, mouth: [2445, 4905], w: 46, yaw: 0 },
	{ id: 'K2', name: 'the Bell Geo', x: 2200, y: 5500, mouth: [2450, 5492], w: 52, yaw: 0 },
	{ id: 'K3', name: 'the Long Geo', x: 2010, y: 6010, mouth: [2300, 6262], w: 44, yaw: 41 },
];
// The geo channel: from the camp (its head, the beach) out to the sea mouth; the head's beach is the cove.
const geoAt = (x, y) => {
	let best = null;
	for (const c of CAMPS) {
		const hx = c.x + Math.cos(c.yaw * DEG) * 26, hy = c.y + Math.sin(c.yaw * DEG) * 26;
		const r = distSeg(x, y, hx, hy, c.mouth[0], c.mouth[1]);
		const w = c.w * (0.55 + 0.45 * r.t) * (1 + 0.12 * fbm(x / 20, y / 20, 2));
		const edge = r.d - w / 2;
		if (!best || edge < best.edge) best = { edge, t: r.t, camp: c, head: [hx, hy] };
	}
	return best;
};
// L3 the Split Geode (crystal 2.3): a bay bitten out of the east cliff where the sea split a geode in half; its half
// shell (WRCrystalScenery SplitGeode) lines the bay's back wall.
const SPLIT = { x: 2452, y: 5750, r: 44 };
const coastDist = (x, y) => Math.min(polyDist(x, y, COAST), Math.hypot(x - SPLIT.x, y - SPLIT.y) - SPLIT.r);

// ---------------------------------------------------------------- the Severance (world-map 2.4; crystal 2.1, 2.3 L2)
// The Wasteland's gorge falls from 200 m at its west mouth to the sea; here the Crystal side holds its floor at ~20 m
// from x +1,420 (where the two floors meet exactly) to the mouth at +1,860, then a valley climbs onto the plateau.
const GORGE = { x0: 1400, x1: 1860, xTop: 2070, y0: 5430, y1: 5452, half: 36, walls: 30 };
const gorgeY = x => lerp(GORGE.y0, GORGE.y1, Math.min(1, Math.max(0, (x - GORGE.x0) / (GORGE.xTop - GORGE.x0))));
const SEAM_FLOOR = Waste.height(GORGE.x0, GORGE.y0);
const gorgeFloor = x => {
	if (x <= GORGE.x0) return SEAM_FLOOR;
	if (x <= 1560) return lerp(SEAM_FLOOR, 20, sstep(GORGE.x0, 1560, x));
	if (x <= GORGE.x1) return 20 + 1.5 * sstep(1560, GORGE.x1, x);
	return 21.5;   // the valley rises from here (base heights take over)
};

// ---------------------------------------------------------------- the plateau and the Spine (crystal 2.1)
// Spine crest along the west edge (north and south of the Wasteland's tip): x by y, its height; it falls east to the
// plateau at its foot (~150 m) and west into the sea.
const SPINE = [[4600, 1640, 250], [4800, 1626, 290], [5000, 1618, 320], [5200, 1606, 330], [5400, 1600, 300], [5600, 1612, 320], [5800, 1620, 330], [6000, 1622, 315], [6200, 1626, 280], [6440, 1640, 230]];
const spineX = y => interp(SPINE.map(s => [s[0], s[1]]), y) + 14 * fbm(y / 240, 3.3, 3);
const spineH = y => interp(SPINE.map(s => [s[0], s[2]]), y) + 18 * fbm(y / 300, 8.1, 3);
// The plateau tilts east to the sea cliffs, its basalt flows stepped into terraces.
const plateauBase = x => interp([[1640, 152], [1720, 148], [1850, 134], [2000, 120], [2150, 108], [2300, 98], [2460, 90]], x);
function terrace(x, y) {
	const v = (fbm(x / 210, y / 210, 4) * 0.5 + 0.5) * 7;
	const k = Math.floor(v), f = v - k;
	return (k + sstep(0.78, 0.98, f)) * 1.6;
}

// Domes: collapsed geode domes on the plateau, open to the sky, their bowls lined with amethyst grit (crystal 2.1: the
// surface reads by its holes; 3.5: 14 domes, 4 of them descents). Ten bowls here; the four descents below.
const DOMES = [
	{ id: 'dome_n1', x: 1965, y: 4735, r: 22, d: 10 }, { id: 'dome_n2', x: 2335, y: 4775, r: 18, d: 8 },
	{ id: 'dome_hammer', x: 2045, y: 5072, r: 26, d: 11, site: 'S3' }, { id: 'dome_ridge', x: 2232, y: 5328, r: 20, d: 9 },
	{ id: 'dome_gorge', x: 1985, y: 5330, r: 17, d: 8 }, { id: 'dome_bell', x: 2330, y: 5600, r: 24, d: 10 },
	{ id: 'dome_wax', x: 2160, y: 5810, r: 30, d: 12 }, { id: 'dome_s1', x: 1905, y: 5910, r: 19, d: 9 },
	{ id: 'dome_s2', x: 2285, y: 6075, r: 21, d: 9 }, { id: 'dome_s3', x: 1990, y: 6195, r: 36, d: 13 },
	{ id: 'dome_rim', x: 2040, y: 4880, r: 15, d: 7 },
];
const bowlDrop = (x, y) => {
	let drop = 0;
	for (const d of DOMES) {
		const q = Math.hypot(x - d.x, y - d.y) / (d.r * (1 + 0.08 * fbm(x / 9, y / 9, 2)));
		if (q < 1.25) drop = Math.max(drop, d.d * (1 - sstep(0.55, 1.08, q)) + 1.2 * (1 - sstep(1.0, 1.25, q)) * sstep(0.9, 1.0, q) * -1);
	}
	return drop;
};

// The vista stack (kit, crystal 2.4): the highest basalt stack, +140, steps cut up its side.
const VISTA = { x: 2200, y: 5250, r: 11, top: 142 };
// The Lumenhorn pool (kit): a spring-fed pool in a basalt hollow.
const POOL = { x: 2118, y: 5418, rx: 18, ry: 12, z: 0 };

function surfaceBase(x, y) {
	// The plateau, rising into the Spine's east face along the west edge.
	const sx = spineX(y), sh = spineH(y);
	let h = plateauBase(x) + 7 * fbm(x / 380, y / 380, 4) + terrace(x, y) + 0.5 * fbm(x / 9, y / 9, 2);
	const foot = sx + 115;
	if (x < foot) {
		const t = sstep(foot, sx, x);
		h = lerp(h, sh + 10 * fbm(x / 60, y / 60, 3), Math.pow(t, 1.35));
	}
	if (x < sx) {
		// The west face: a wall into the sea (the Wasteland's Spine stands here in its tip's stretch).
		const t = sstep(sx, sx - 75, x);
		h = lerp(sh, -16, t);
	}
	// The Severance valley and gorge.
	const gy = gorgeY(x), dy = Math.abs(y - gy);
	if (x > GORGE.x0 - 10 && x < GORGE.xTop + 60) {
		const halfW = GORGE.half + (x > GORGE.x1 ? (x - GORGE.x1) * 0.22 : 0) + 6 * fbm(x / 40, 1.7, 2);
		const walls = GORGE.walls + (x > GORGE.x1 ? (x - GORGE.x1) * 0.6 : 0);
		let floor = gorgeFloor(x);
		if (x > GORGE.x1) floor = lerp(21.5, plateauBase(x) + 2, sstep(GORGE.x1, GORGE.xTop + 40, x));
		const w = 1 - sstep(halfW, halfW + walls, dy);
		const fade = 1 - sstep(GORGE.xTop, GORGE.xTop + 60, x);
		if (w > 0) h = lerp(h, Math.min(h, floor + 0.6 * fbm(x / 14, y / 14, 2) + 0.012 * dy * dy * 0), w * fade);
	}
	// Domes.
	h -= bowlDrop(x, y);
	// The vista stack.
	const dv = Math.hypot(x - VISTA.x, y - VISTA.y);
	if (dv < VISTA.r + 6) h = lerp(h, VISTA.top + 0.4 * fbm(x, y, 2), 1 - sstep(VISTA.r, VISTA.r + 6, dv));
	// The Lumenhorn pool's basin.
	const qp = Math.hypot((x - POOL.x) / POOL.rx, (y - POOL.y) / POOL.ry);
	if (qp < 1.6) h -= 2.4 * (1 - sstep(0.7, 1.6, qp));
	return h;
}
POOL.z = 0;   // set after the heightfield (its water surface)

// The camps' coves: the geo's head is a black shingle beach 2 m over the sea; the cliff stairs climb from it.
function coastShape(x, y, h) {
	const s = coastDist(x, y);
	const g = geoAt(x, y);
	// Sea cliffs: the plateau stops in a 6 m wide wall of columns; under it a boulder apron, then the sea floor.
	let out = h;
	if (s < 7) out = s >= 0 ? lerp(-2.5, h, Math.pow(sstep(0, 7, s), 0.6)) : lerp(-2.5, -16, sstep(0, 45, -s));
	// The geos: cut the cliff into the plateau, a narrow sea channel with a shingle beach at its head.
	if (g.edge < 8) {
		const head = Math.hypot(x - g.head[0], y - g.head[1]);
		const beach = 1 - sstep(16, 30, head);
		const floor = lerp(-9 * sstep(0, 0.5, g.t), 1.8 + 0.3 * fbm(x / 6, y / 6, 2), beach);
		out = Math.min(out, lerp(floor, out, Math.pow(sstep(0, 8, g.edge), 0.6)));
	}
	return out;
}

// ---------------------------------------------------------------- the cave network (build cook 2.3, 2.4)
// Halls: kind geode | beams | prism | chamber | vault | alcove; zone C2..C5, V; floor (m), rx, ry, h (m), yaw (deg).
const HALLS = [
	// C2 the Geode Halls (north and centre).
	{ id: 'G1', kind: 'geode', zone: 'C2', name: 'the North Dome', x: 2150, y: 4762, floor: 70, rx: 28, ry: 24, h: 24, yaw: 20 },
	{ id: 'G1b', kind: 'geode', zone: 'C2', name: 'the cup under the North Dome', x: 2168, y: 4748, floor: 50, rx: 10, ry: 9, h: 9, yaw: 0, cup: true },
	{ id: 'G2', kind: 'geode', zone: 'C2', name: 'the Singing School', x: 2210, y: 4862, floor: 52, rx: 20, ry: 18, h: 18, yaw: -10, site: 'H1' },
	{ id: 'G7', kind: 'geode', zone: 'C2', name: 'the Upper Geode', x: 2010, y: 4930, floor: 58, rx: 18, ry: 17, h: 16, yaw: 35 },
	{ id: 'G3', kind: 'geode', zone: 'C2', name: 'the Hall of the Geode Shrine', x: 2100, y: 5020, floor: 30, rx: 30, ry: 26, h: 24, yaw: -25, shrine: 'geode' },
	{ id: 'G3b', kind: 'geode', zone: 'C2', name: 'the cup under the shrine', x: 2122, y: 5008, floor: 11, rx: 12, ry: 11, h: 10, yaw: 0, cup: true },
	{ id: 'G4', kind: 'geode', zone: 'C2', name: 'the Bell Throat', x: 2300, y: 5150, floor: 22, rx: 24, ry: 22, h: 20, yaw: 10 },
	{ id: 'G5', kind: 'geode', zone: 'C2', name: 'the Broken Nest', x: 1960, y: 5200, floor: -8, rx: 20, ry: 19, h: 16, yaw: 0, site: 'H2' },
	{ id: 'G6', kind: 'geode', zone: 'C2', name: 'the Agate Rinds', x: 2062, y: 5252, floor: -18, rx: 32, ry: 26, h: 22, yaw: 15, gallery: 'O2' },
	// C4 the Echo Tunnels: the junctions are tube crossings (no hall); the Guano Mines are a geode under the big roost.
	{ id: 'H6', kind: 'geode', zone: 'C4', name: 'the Guano Mines', x: 2062, y: 5822, floor: -30, rx: 18, ry: 16, h: 16, yaw: 0, site: 'H6' },
	// C3 the Crystal Forests (south).
	{ id: 'B6', kind: 'geode', zone: 'C3', name: 'the Sinking Geode', x: 2002, y: 5650, floor: 30, rx: 20, ry: 18, h: 18, yaw: 0 },
	{ id: 'B6b', kind: 'geode', zone: 'C3', name: 'the cup under the Sinking Geode', x: 2012, y: 5660, floor: 12, rx: 10, ry: 9, h: 9, yaw: 0, cup: true },
	{ id: 'B2', kind: 'geode', zone: 'C3', name: 'the Steam Baths', x: 2202, y: 5702, floor: -8, rx: 22, ry: 20, h: 16, yaw: 0, site: 'H3' },
	{ id: 'B1', kind: 'beams', zone: 'C3', name: 'the Hall of Beams', x: 2052, y: 5952, floor: -55, rx: 90, ry: 45, h: 50, yaw: 4, shrine: 'beam', landmark: 'L4' },
	{ id: 'B4', kind: 'geode', zone: 'C3', name: "the Tyrant's Wallow", x: 2172, y: 6072, floor: -40, rx: 26, ry: 22, h: 18, yaw: 0, lair: 'geode_tyrant' },
	{ id: 'B3', kind: 'geode', zone: 'C3', name: 'the Chandelier Gallery', x: 1950, y: 6150, floor: -30, rx: 24, ry: 8, h: 16, yaw: 0, site: 'H4' },
	{ id: 'B5', kind: 'geode', zone: 'C3', name: 'the South Steam Hall', x: 2100, y: 6210, floor: 2, rx: 22, ry: 21, h: 18, yaw: 0 },
	// C5 the Ward Chambers (west, under the Spine).
	{ id: 'W1', kind: 'chamber', zone: 'C5', name: 'the Severance Gate', x: 1800, y: 5370, floor: 18, rx: 7, ry: 5.5, h: 8, yaw: 0, site: 'H8' },
	{ id: 'W2', kind: 'chamber', zone: 'C5', name: 'the Great Hexafoil', x: 1760, y: 5650, floor: -90, rx: 8, ry: 8, h: 9, yaw: 0, shrine: 'great' },
	{ id: 'W3', kind: 'chamber', zone: 'C5', name: "the Lens Grinder's Room", x: 1690, y: 5110, floor: -108, rx: 6, ry: 4.5, h: 6, yaw: -30, site: 'H7' },
	{ id: 'W4', kind: 'chamber', zone: 'C5', name: 'the Archive of Wards', x: 1640, y: 5190, floor: -110, rx: 20, ry: 20, h: 18, yaw: 0, arena: 'mirrored_twins' },
	{ id: 'W5', kind: 'prism', zone: 'C5', name: 'the Prism Hall', x: 1790, y: 5300, floor: -120, rx: 45, ry: 45, h: 50, yaw: 0, arena: 'lyrathis' },
	{ id: 'W6', kind: 'geode', zone: 'C5', name: 'the Hall of the Door', x: 1640, y: 5850, floor: -150, rx: 25, ry: 25, h: 20, yaw: 0, arena: 'echo_warden' },
	{ id: 'W7', kind: 'geode', zone: 'C5', name: 'the Deep Geode', x: 1502, y: 6050, floor: -160, rx: 26, ry: 24, h: 22, yaw: 20, gallery: 'O4' },
	// The Silent Vault (crystal 2.5): six hewn rooms in a line, descending south.
	{ id: 'V1', kind: 'vault', zone: 'V', name: 'the Antechamber', x: 1600, y: 5905, floor: -165, rx: 6, ry: 5, h: 6, yaw: 90 },
	{ id: 'V2', kind: 'vault', zone: 'V', name: 'the Choir Benches', x: 1600, y: 5937, floor: -168, rx: 12, ry: 10, h: 9, yaw: 90 },
	{ id: 'V3', kind: 'vault', zone: 'V', name: 'the Archive of Forks', x: 1600, y: 5973, floor: -171, rx: 8, ry: 7, h: 7, yaw: 90 },
	{ id: 'V4', kind: 'vault', zone: 'V', name: 'the Lake', x: 1600, y: 6020, floor: -174, rx: 26, ry: 12, h: 14, yaw: 90, lake: true },
	{ id: 'V5', kind: 'vault', zone: 'V', name: "the Sovereigns' Mould", x: 1600, y: 6068, floor: -177, rx: 10, ry: 8, h: 9, yaw: 90 },
	{ id: 'V6', kind: 'vault', zone: 'V', name: 'the Last Carver', x: 1600, y: 6098, floor: -180, rx: 6, ry: 6, h: 7, yaw: 90 },
];
const hallById = id => HALLS.find(h => h.id === id);
// Where a tube meets a hall it ends at the hall's floor, a little inside its wall (the air shapes overlap).
const into = (id, toward, inset = 0.72) => {
	const h = hallById(id);
	const dx = toward[0] - h.x, dy = toward[1] - h.y, L = Math.hypot(dx, dy) || 1;
	const r = Math.min(h.rx, h.ry) * inset;
	return [r1(h.x + dx / L * r), r1(h.y + dy / L * r), h.floor];
};
// Tubes: kind tube (lava tube, radius r) | passage (hewn, w x h) | ramp (a tube open to the sky at its top end).
// pts: [x, y, floor] control points; densified below on a Catmull-Rom curve every 3 m.
const TUBES = [];
const tube = (id, kind, zone, ends, mids, size, extra = {}) => {
	// With no middle points the first end aims at the second (a hall's centre when it is a hall: a bare id there gave a
	// NaN point, and the West Door, the Lens Grinder's door and the Vault's own doors had no passage).
	const toward = mids[0] || (typeof ends[1] === 'string' ? [hallById(ends[1]).x, hallById(ends[1]).y, hallById(ends[1]).floor] : ends[1]);
	const a = typeof ends[0] === 'string' ? into(ends[0], toward) : ends[0];
	const b = typeof ends[1] === 'string' ? into(ends[1], mids.length ? mids[mids.length - 1] : a) : ends[1];
	TUBES.push({ id, kind, zone, ctrl: [a, ...mids, b], ...size, ...extra });
};
// C2.
tube('G1-G2', 'tube', 'C2', ['G1', 'G2'], [[2182, 4812, 61]], { r: 4.5 });
tube('G1-G7', 'tube', 'C2', ['G1', 'G7'], [[2080, 4848, 64]], { r: 4.2 });
tube('G1b-G2', 'tube', 'C2', ['G1b', 'G2'], [[2196, 4802, 51]], { r: 3.4 });
tube('G7-G3', 'tube', 'C2', ['G7', 'G3'], [[2042, 4982, 45]], { r: 4.5 });
tube('G3-G4', 'tube', 'C2', ['G3', 'G4'], [[2200, 5090, 26]], { r: 5 });
tube('G3b-G5', 'tube', 'C2', ['G3b', 'G5'], [[2032, 5100, 2]], { r: 3.4 });
tube('G3-G6', 'tube', 'C2', ['G3', 'G6'], [[2140, 5140, 8]], { r: 5 });
tube('G6-G5', 'tube', 'C2', ['G6', 'G5'], [[2010, 5226, -12]], { r: 4.5 });
// The throat: the Geode Halls' violet fades into the Echo Tunnels' black over its 40 m descent (crystal 2.2 blends).
tube('G6-J2', 'tube', 'C4', ['G6', [2000, 5556, -20]], [[2046, 5400, -20]], { r: 6 }, { blend: 'C2' });
tube('G4-J3', 'tube', 'C4', ['G4', [2300, 5606, -32]], [[2250, 5300, 5], [2165, 5470, -20], [2205, 5565, -28]], { r: 5 });
// L5 the Organ (crystal 2.3): one straight tube, 850 m, eleven vents to the plateau.
tube('organ', 'tube', 'C4', [[1450, 5500, -2], [2318, 5608, -33]], [[1700, 5520, -10], [2000, 5556, -20], [2300, 5606, -32]], { r: 7 }, { benches: true, landmark: 'L5' });
tube('B6-J2', 'tube', 'C3', ['B6', [2000, 5560, -20]], [[2082, 5662, 18], [2118, 5600, 2]], { r: 5 });
tube('B6b-J2', 'tube', 'C3', ['B6b', [2004, 5562, -20]], [[2040, 5602, -2]], { r: 3.4 });
tube('J3-B2', 'tube', 'C3', [[2300, 5606, -32], 'B2'], [[2252, 5662, -20]], { r: 5 });
// O3 the Silver Tube.
tube('silver', 'tube', 'C4', ['B2', 'B1'], [[2266, 5792, -40], [2240, 5880, -52]], { r: 5 }, { gallery: 'O3' });
tube('J2-H6', 'tube', 'C4', [[2000, 5556, -20], 'H6'], [[2030, 5700, -26]], { r: 4.5 });
tube('H6-B1', 'tube', 'C3', ['H6', [2040, 5918, -55]], [[2052, 5880, -45]], { r: 4 });
tube('B1-B4', 'tube', 'C3', [[2126, 5982, -55], 'B4'], [[2150, 6030, -48]], { r: 5 });
tube('B1-B3', 'tube', 'C3', [[1980, 5982, -55], 'B3'], [[1962, 6066, -42]], { r: 5 });
tube('B4-B5', 'tube', 'C3', ['B4', 'B5'], [[2112, 6128, -30]], { r: 5 });
// C5: the Wardcarvers' hewn ways.
tube('J1-W2', 'passage', 'C5', [[1700, 5522, -10], 'W2'], [[1660, 5600, -50]], { w: 6, h: 7 });
tube('W2-W4', 'passage', 'C5', ['W2', 'W4'], [[1700, 5420, -100]], { w: 6, h: 7 });
tube('W4-W3', 'passage', 'C5', ['W4', 'W3'], [], { w: 5, h: 5 });
tube('W4-W5', 'passage', 'C5', ['W4', 'W5'], [], { w: 6, h: 8 }, { door: 'west' });
tube('W2-W6', 'passage', 'C5', ['W2', 'W6'], [[1700, 5760, -125]], { w: 6, h: 7 });
tube('W6-W7', 'tube', 'C5', ['W6', 'W7'], [[1560, 5960, -156]], { r: 5 });
// The Severance: the gate in the gorge's south wall to the guard room; the stair down; the Severance Door.
tube('gate', 'passage', 'C5', [[1800, 5404, 20.4], 'W1'], [], { w: 7, h: 9 }, { door: 'gate' });
tube('severance_door', 'passage', 'C5', [[1801, 5366, -120], 'W5'], [], { w: 6, h: 8 }, { door: 'severance' });
// The Vault (crystal 2.5): the Vault Door from the Hall of the Door, then the rooms in their line.
tube('vault_door', 'passage', 'V', [[1628, 5873, -150], 'V1'], [], { w: 6, h: 6.5 }, { door: 'vault' });
tube('V1-V2', 'passage', 'V', ['V1', 'V2'], [], { w: 3.5, h: 4 });
tube('V2-V3', 'passage', 'V', ['V2', 'V3'], [], { w: 3.5, h: 4 });
tube('V3-V4', 'passage', 'V', ['V3', 'V4'], [], { w: 3.5, h: 4 });
tube('V4-V5', 'passage', 'V', ['V4', 'V5'], [], { w: 3.5, h: 4 });
tube('V5-V6', 'passage', 'V', ['V5', 'V6'], [], { w: 3.5, h: 4 });
// D3 the Sinking Geode's ramp (crystal 2.4): a collapsing dome you walk down on its fallen rind, open to the sky above.
tube('D3', 'ramp', 'C3', [[1958, 5546, 0], 'B6'], [[1974, 5596, 68]], { r: 8 }, { descent: 'D3' });

// Shafts: vertical cylinders. descent (a way down from the plateau, its stair built in it), stair (the Severance
// stair), drop (a thin floor's hole into a cup), vent (the Organ's skylights), light (the Prism Crown's shafts).
const SHAFTS = [];
// Densify the tubes now (heights of the surface come later; the ramp's top is set from the ground).
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
const SURF = { ground: 0, pave: 1, grass: 2, heather: 3, cliff: 4, shingle: 5, gorge: 6, path: 7, dome: 8, sea: 9, spine: 10, hidden: 255 };
const idx = (i, j) => j * N + i;
const hRaw = (x, y) => coastShape(x, y, surfaceBase(x, y));
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) HF[idx(i, j)] = hRaw(X0 + i * STEP, Y0 + j * STEP);
// The seam with the Wasteland: west of x +1,600 only the gorge's floor strip is this land's (where its floor is above
// the Wasteland's falling one); at its west end the two floors share the seam samples exactly. Between +1,600 and
// +1,720 (its Spine tip's east fall) this land covers the Wasteland's ground: never below it.
const hidden = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = idx(i, j);
	const inWasteBox = y > 5236 && y < 5640;
	if (inWasteBox && x < 1720) {
		const w = Waste.height(x, y);
		const dy = Math.abs(y - gorgeY(x));
		if (x < 1600) {
			if (dy < GORGE.half + 8 && w < HF[k] + 30) { HF[k] = Math.max(HF[k], w + 0.3); if (x <= GORGE.x0 + 0.5) HF[k] = w; }
			else hidden[k] = 1;
		} else if (w > -4) HF[k] = Math.max(HF[k], w + 0.3);
	}
	if (x < X0 + 0.5 && !(inWasteBox && Math.abs(y - gorgeY(x)) < GORGE.half + 8)) hidden[k] = 1;
	// Far sea floor: nothing to see under 30 m of open water beyond the cliffs' aprons.
	if (coastDist(x, y) < -90) hidden[k] = 1;
}
const hAt = (x, y) => {
	const fi = Math.min(N - 1.001, Math.max(0, (x - X0) / STEP)), fj = Math.min(N - 1.001, Math.max(0, (y - Y0) / STEP));
	const i = Math.floor(fi), j = Math.floor(fj), tx = fi - i, ty = fj - j;
	return lerp(lerp(HF[idx(i, j)], HF[idx(i + 1, j)], tx), lerp(HF[idx(i, j + 1)], HF[idx(i + 1, j + 1)], tx), ty);
};
const slopeAt = (x, y) => Math.hypot(hAt(x + 2, y) - hAt(x - 2, y), hAt(x, y + 2) - hAt(x, y - 2)) / 4;
POOL.z = r2(hAt(POOL.x, POOL.y) + 1.6);

// ---------------------------------------------------------------- descents (crystal 2.4: D1-D4) and the Organ's vents
const surfZ = (x, y) => hAt(x, y);
const DESCENTS = [
	{ id: 'D1', name: 'the North Dome', x: 2150, y: 4762, r: 15, hall: 'G1', how: 'lip_stairs' },
	{ id: 'D2', name: 'the Bell Throat', x: 2300, y: 5150, r: 7.5, hall: 'G4', how: 'bell_stair' },
	{ id: 'D3', name: 'the Sinking Geode', x: 1958, y: 5546, r: 20, hall: 'B6', how: 'rind_ramp' },
	{ id: 'D4', name: 'the South Steam Vent', x: 2100, y: 6210, r: 9, hall: 'B5', how: 'vent_steps' },
];
for (const d of DESCENTS) {
	d.z = r2(surfZ(d.x, d.y));
	if (d.how === 'rind_ramp') continue;
	const h = hallById(d.hall);
	// The shaft from the hall's roof up to just under the ground (the hole and its kerb cover the join).
	SHAFTS.push({ id: d.id, kind: 'descent', x: d.x, y: d.y, r: d.r, z0: r2(h.floor + h.h * 0.6), z1: r2(d.z - 0.6), hall: d.hall, how: d.how });
}
// D3's ramp starts in its collapsed dome at the ground.
{
	const ramp = TUBES.find(t => t.id === 'D3');
	ramp.ctrl[0][2] = r2(surfZ(ramp.ctrl[0][0], ramp.ctrl[0][1]) - 2);
	ramp.ctrl[1][2] = r2(lerp(ramp.ctrl[0][2], hallById('B6').floor, 0.45));
}
// The Organ's eleven vents (crystal 2.3 L5): skylights from the tube's roof to the plateau, along its eastern 600 m.
const VENTS = [];
for (let v = 0; v < 11; v++) {
	const x = 1740 + v * 56;
	const p = (() => { const o = TUBES.find(t => t.id === 'organ').ctrl; let best = o[0]; for (let i = 0; i + 1 < o.length; i++) if (x >= o[i][0] && x <= o[i + 1][0]) { const t = (x - o[i][0]) / (o[i + 1][0] - o[i][0]); best = [x, lerp(o[i][1], o[i + 1][1], t), lerp(o[i][2], o[i + 1][2], t)]; } return best; })();
	const vy = p[1] + 2.5 * Math.sin(v * 1.7);
	const z = surfZ(x, vy);
	VENTS.push({ id: `vent${v + 1}`, x: r1(x), y: r1(vy), z: r2(z), r: 3.2 });
	SHAFTS.push({ id: `vent${v + 1}`, kind: 'vent', x: r1(x), y: r1(vy), r: 3.2, z0: r2(p[2] + 7), z1: r2(z - 0.6) });
}
// The Severance stair (W1s): the Wardcarvers' spiral from the guard room's floor to the Prism Hall's level.
SHAFTS.push({ id: 'W1s', kind: 'stair', x: 1801, y: 5371, r: 6.5, z0: -120, z1: 18.6, hall: 'W1', how: 'spiral' });
// The thin floors' drops into their cups (crystal 2.8: only over a real lower geode).
const THIN = [
	{ id: 'TF_G1', hall: 'G1', cup: 'G1b', x: 2168, y: 4748, r: 4 },
	{ id: 'TF_G3', hall: 'G3', cup: 'G3b', x: 2122, y: 5008, r: 4 },
	{ id: 'TF_B6', hall: 'B6', cup: 'B6b', x: 2012, y: 5660, r: 4 },
];
for (const t of THIN) {
	const top = hallById(t.hall), cup = hallById(t.cup);
	t.z = top.floor;
	t.drop = r2(top.floor - cup.floor);
	SHAFTS.push({ id: t.id, kind: 'drop', x: t.x, y: t.y, r: t.r + 0.3, z0: r2(cup.floor + cup.h * 0.5), z1: r2(top.floor + 0.4) });
}
// The Prism Crown's eight light shafts (crystal 2.3 L1, 2.14.1): from the dome at r 30 m straight up to each spire.
const CROWN = { x: 1790, y: 5300, r: 30 };
const spires = [];
for (let s = 0; s < 8; s++) {
	const a = (s * 45 + 22.5) * DEG;
	const x = CROWN.x + Math.cos(a) * CROWN.r, y = CROWN.y + Math.sin(a) * CROWN.r;
	const z = surfZ(x, y);
	spires.push({ i: s, x: r1(x), y: r1(y), z: r2(z), h: r1(42 + 18 * ((s * 5) % 8) / 7), tilt: r1(2 + 6 * ((s * 3) % 8) / 7), yaw: r1(s * 45 + 22.5 + 180) });
	const hall = hallById('W5');
	SHAFTS.push({ id: `light${s + 1}`, kind: 'light', x: r1(x), y: r1(y), r: 1.4, z0: r2(hall.floor + hall.h * 0.7), z1: r2(z - 1.2) });
}

// ---------------------------------------------------------------- hidden samples: the holes
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = idx(i, j);
	for (const d of DESCENTS) {
		if (d.how === 'rind_ramp') continue;
		// The hole is smaller than the shaft (its lip overhangs the shaft wall; the kerb covers the edge).
		if (Math.hypot(x - d.x, y - d.y) < Math.max(2.2, d.r - 5.5)) hidden[k] = 1;
	}
	for (const v of VENTS) if (Math.hypot(x - v.x, y - v.y) < 1.9) hidden[k] = 1;
}
// D3: the ground is open wherever the ramp's roof would stand above it.
const tubesDense = TUBES.map(t => ({ ...t, pts: densify(t.ctrl) }));
{
	const ramp = tubesDense.find(t => t.id === 'D3');
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const x = X0 + i * STEP, y = Y0 + j * STEP;
		const r = distLine(x, y, ramp.pts);
		if (r.d > ramp.r - 2.5) continue;
		const p = ramp.pts[r.s], q = ramp.pts[r.s + 1];
		const floor = lerp(p[2], q[2], r.t);
		if (floor + ramp.r * 1.55 > HF[idx(i, j)] - 1.0) hidden[idx(i, j)] = 1;
	}
}

// ---------------------------------------------------------------- zones and surfaces
const zoneAt = (x, y) => {
	if (coastDist(x, y) < -3 || geoAt(x, y).edge < -2) return '~';
	const gy = gorgeY(x);
	if (x < GORGE.xTop && Math.abs(y - gy) < GORGE.half + 20 + (x > GORGE.x1 ? (x - GORGE.x1) * 0.22 : 0)) return 'g';
	if (x < spineX(y) + 70) return 's';
	return '1';
};
const ROADS = [
	{ id: 'shrine_road', name: 'the Chime Road', pts: [[2335, 5205], [2280, 5232], [2220, 5262], [2160, 5300], [2100, 5332], [2050, 5372], [2010, 5420], [1960, 5440], [1900, 5444], [1866, 5442]] },
	{ id: 'north_road', name: 'the North Geo path', pts: [[2236, 4900], [2210, 4850], [2180, 4800], [2162, 4784]] },
	{ id: 'bell_road', name: 'the Bell Geo path', pts: [[2242, 4908], [2270, 4980], [2290, 5060], [2302, 5128], [2318, 5168], [2335, 5205]] },
	{ id: 'mid_road', name: 'the Bell Geo path', pts: [[2186, 5500], [2140, 5470], [2090, 5440], [2050, 5400]] },
	{ id: 'ramp_road', name: 'the Sinking Geode path', pts: [[2186, 5510], [2130, 5540], [2060, 5552], [1990, 5552], [1972, 5548]] },
	{ id: 'south_road', name: 'the South path', pts: [[2196, 5512], [2215, 5600], [2240, 5700], [2250, 5800], [2248, 5880], [2210, 5960], [2160, 6050], [2120, 6140], [2108, 6190]] },
	{ id: 'long_geo_road', name: 'the Long Geo path', pts: [[2028, 6022], [2060, 6080], [2090, 6150], [2100, 6185]] },
	{ id: 'runs_road', name: 'the Alicanto Runs', pts: [[2118, 6232], [2140, 6270], [2150, 6300]] },
];
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = idx(i, j);
	if (hidden[k]) { SURFM[k] = SURF.hidden; continue; }
	const z = zoneAt(x, y), h = HF[k], s = slopeAt(x, y);
	let c = SURF.ground;
	if (z === '~' || h < -1) c = SURF.sea;
	else if (z === 'g') c = SURF.gorge;
	else if (h < 3.5 && geoAt(x, y).edge < 10) c = SURF.shingle;
	else if (s > 0.9) c = SURF.cliff;
	else if (z === 's') c = s > 0.45 ? SURF.cliff : SURF.spine;
	else {
		const n = fbm(x / 26, y / 26, 3), m = fbm(x / 11 + 5, y / 11, 2);
		// Black hex pavement, salt-grass and sea campion only in its cracks and hollows, heather on the higher ground.
		c = n > 0.34 ? SURF.grass : (n < -0.38 && h > 112 ? SURF.heather : SURF.pave);
		if (m > 0.62) c = SURF.grass;
	}
	if (c !== SURF.sea && c !== SURF.cliff) {
		for (const d of DOMES) if (Math.hypot(x - d.x, y - d.y) < d.r * 0.92) c = SURF.dome;
		for (const R of ROADS) if (distLine(x, y, R.pts).d < 1.8 + 0.6 * fbm(x / 5, y / 5, 2)) c = SURF.path;
	}
	SURFM[k] = c;
}
// The gorge's walls are grey-bleached where the Void touched them (crystal 2.2 Void border): the gorge surface reaches
// up its walls; its slope rock is the same grey (the land's slopeRock stays basalt elsewhere: zone colours).

// ---------------------------------------------------------------- the check: no cave roof near the ground
const caveTops = [];
for (const h of HALLS) {
	for (let a = 0; a < 16; a++) for (const f of [0, 0.5, 0.85]) {
		const x = h.x + Math.cos(a * 22.5 * DEG) * h.rx * f, y = h.y + Math.sin(a * 22.5 * DEG) * h.ry * f;
		const top = h.floor + h.h * Math.sqrt(Math.max(0, 1 - f * f));
		caveTops.push({ id: h.id, x, y, top });
	}
}
for (const t of tubesDense) {
	if (t.kind === 'ramp' || t.id === 'gate') continue;
	for (const p of t.pts) caveTops.push({ id: t.id, x: p[0], y: p[1], top: p[2] + (t.kind === 'passage' ? t.h : t.r * 1.55) });
}
let worst = 1e9, worstAt = null, bad = 0; const badIds = {};
for (const c of caveTops) {
	const inWaste = c.y > 5236 && c.y < 5640 && c.x < 1720;
	const g = Math.max(hAt(c.x, c.y), coastDist(c.x, c.y) < 0 ? -16 : -1e9, inWaste ? Waste.height(c.x, c.y) : -1e9);
	const cover = g - c.top;
	if (cover < worst) { worst = cover; worstAt = c; }
	if (cover < 5) { bad++; badIds[c.id] = Math.min(badIds[c.id] ?? 1e9, +cover.toFixed(1)); }
}
if (bad) console.error(`WARNING: ${bad} cave points under 5 m of rock; worst ${worst.toFixed(1)} m at ${worstAt.id} (${worstAt.x.toFixed(0)}, ${worstAt.y.toFixed(0)}) ${JSON.stringify(badIds)}`);

// ---------------------------------------------------------------- places (crystal 2.3, 2.4, 2.11, 2.20)
const site = (id, kind, name, x, y, extra = {}) => ({ id, kind, name, x: r1(x), y: r1(y), z: r2(extra.z ?? hAt(x, y)), yaw: extra.yaw ?? 0, ...extra });
const sites = [];
for (const c of CAMPS) sites.push(site(c.id, 'camp', c.name, c.x, c.y, { yaw: c.yaw, camp: true, cove: [r1(c.x + Math.cos(c.yaw * DEG) * 26), r1(c.y + Math.sin(c.yaw * DEG) * 26), r2(hAt(c.x + Math.cos(c.yaw * DEG) * 26, c.y + Math.sin(c.yaw * DEG) * 26))] }));
sites.push(site('L1', 'prism_crown', 'the Prism Crown', CROWN.x, CROWN.y, { landmark: true, spires }));
sites.push(site('L2', 'giantess', 'the Hollowed Giantess', 1838, gorgeY(1838), { yaw: 180, landmark: true }));
sites.push(site('L3', 'split_geode', 'the Split Geode', SPLIT.x, SPLIT.y, { yaw: 0, landmark: true, z: 0 }));
for (const d of DESCENTS) sites.push(site(d.id, 'descent', d.name, d.x, d.y, { z: d.z, how: d.how, hall: d.hall, r: d.r }));
// S1 the Watch-Cairn Line: seven cairns along the Spine's foot aimed west at the Severance; the southern four torn out.
const cairns = [];
for (let c = 0; c < 7; c++) {
	const y = 4800 + c * (1400 / 6) + (c >= 3 ? 40 : 0);
	const x = spineX(y) + 125 + 8 * fbm(y / 50, 1, 2);
	cairns.push({ i: c, x: r1(x), y: r1(y), z: r2(hAt(x, y)), torn: c >= 3, yaw: yawOf(1800 - x, gorgeY(1800) - y) });
}
sites.push(site('S1', 'watch_cairns', 'the Watch-Cairn Line', cairns[3].x, cairns[3].y, { hand: true, cairns }));
sites.push(site('S2', 'ringing_wreck', 'the Ringing Wreck', 2392, 4906, { yaw: 170, hand: true, z: 0 }));
sites.push(site('S3', 'dropped_hammer', 'the Dome of the Dropped Hammer', 2045, 5072, { hand: true }));
sites.push(site('S4', 'wax_fields', 'the Wax Fields', 2252, 5902, { hand: true }));
sites.push(site('S5', 'alicanto_runs', 'the Alicanto Runs', 2150, 6300, { hand: true, yaw: 100 }));
sites.push(site('S6', 'chime_garden', 'the Chime Garden', 2352, 5352, { hand: true }));
// Cave hand sites placed in their halls (H1 G2, H2 G5, H3 B2, H4 B3, H6, H7 W3, H8 W1) and H5 on the Organ's wall.
for (const h of HALLS) if (h.site) sites.push(site(h.site, h.site.toLowerCase(), h.name, h.x, h.y, { z: h.floor, hall: h.id, hand: true, cave: true }));
{
	const o = tubesDense.find(t => t.id === 'organ').pts;
	const p = o.reduce((b, q) => Math.abs(q[0] - 1805) < Math.abs(b[0] - 1805) ? q : b);
	sites.push(site('H5', 'tally_wall', 'the Tally Wall', p[0], p[1], { z: p[2], tube: 'organ', hand: true, cave: true, yaw: 90 }));
}
// The Silent Vault's six rooms (crystal 2.5).
for (const h of HALLS) if (h.kind === 'vault') sites.push(site(h.id, 'vault_' + h.id.toLowerCase(), h.name, h.x, h.y, { z: h.floor, hall: h.id, hand: true, cave: true, vault: true }));

// Shrines (crystal 2.11): five; the Great Shrine in the Hexafoil chamber has no guard.
const shrines = [
	{ id: 'crystal_hexafoil', name: 'the Great Shrine of the Hexafoil', x: 1760, y: 5650, z: -90, great: true, hall: 'W2' },
	{ id: 'crystal_cliff_bell', name: 'the Cliff Bell Shrine', x: 2335, y: 5205, z: r2(hAt(2335, 5205)) },
	{ id: 'crystal_geode', name: 'the Geode Shrine', x: 2078, y: 5004, z: 30.9, hall: 'G3' },
	{ id: 'crystal_beam', name: 'the Beam Shrine', x: 2052, y: 5952, z: -34.6, hall: 'B1', beam: true },
	{ id: 'crystal_severance', name: 'the Severance Shrine', x: 1851, y: r1(gorgeY(1851)), z: r2(hAt(1851, gorgeY(1851))), cage: 'L2' },
];
// Run-back stones (crystal 2.21.8 contested bosses: one outside each door of a major arena; one for each mini).
const runback = [
	{ boss: 'lyrathis', door: 'west', x: 1690, y: 5228, z: -110 },
	{ boss: 'lyrathis', door: 'severance', x: 1803, y: 5385, z: 18.4 },
	{ boss: 'mirrored_twins', x: 1695, y: 5422, z: -100 },
	{ boss: 'echo_warden', x: 1706, y: 5764, z: -125 },
	{ boss: 'geode_tyrant', x: 2132, y: 6002, z: -52 },
];

// ---------------------------------------------------------------- kit POIs: surface and the caves' 150 / 125 m rule
const kit = [];
kit.push({ id: 'k_out1', kind: 'resonant_outcrop', x: 2026, y: 4890, z: r2(hAt(2026, 4890)), surface: true, dome: 'dome_rim' });
kit.push({ id: 'k_out2', kind: 'resonant_outcrop', x: 2318, y: 5588, z: r2(hAt(2318, 5588)), surface: true, dome: 'dome_bell' });
kit.push({ id: 'k_ledge', kind: 'druseback_ledge', x: 2396, y: 5986, z: r2(hAt(2396, 5986)), surface: true });
kit.push({ id: 'k_pool', kind: 'lumenhorn_pool', x: POOL.x, y: POOL.y, z: POOL.z, surface: true, rx: POOL.rx, ry: POOL.ry });
kit.push({ id: 'k_vista', kind: 'vista', x: VISTA.x, y: VISTA.y, z: VISTA.top, surface: true, reveal: 400 });
// Cave kit kinds in the cook's counts (crystal 2.4: 26), handed out round the network in this order; a hall that's a
// natural home takes its own first, then alcoves cut off the tubes wherever the route runs 125 m from any place.
const CAVE_KIT = [
	['resonant_outcrop', 'G1'], ['resonant_outcrop', 'G7'], ['resonant_outcrop', 'G3'], ['resonant_outcrop', 'G4'], ['resonant_outcrop', 'W7'], ['resonant_outcrop', null],
	['nest_geode', 'G6'], ['nest_geode', 'G5'], ['nest_geode', null], ['nest_geode', null],
	['roost', 'H6'], ['roost', null], ['roost', null],
	['chime_cache', null], ['chime_cache', null], ['chime_cache', null],
	['scald_basin', 'B5'], ['scald_basin', null],
	['chimecrab_ledge', 'G4'], ['chimecrab_ledge', null],
	['druseback_wallow', 'B4'], ['druseback_wallow', null],
	['cave_lake', null], ['cave_lake', null],
	['lumenhorn_hall', null],
	['vista_window', null],
];
const pois = [...sites.filter(s => s.cave || s.hall).map(s => [s.x, s.y]), ...HALLS.map(h => [h.x, h.y])];
const queue = [];
for (const [kind, hall] of CAVE_KIT) {
	if (hall) {
		const h = hallById(hall);
		const a = (kit.length * 77) % 360 * DEG;
		kit.push({ id: `k_${kind}_${hall}`, kind, x: r1(h.x + Math.cos(a) * h.rx * 0.55), y: r1(h.y + Math.sin(a) * h.ry * 0.55), z: h.floor, hall, cave: true });
	} else queue.push(kind);
}
// The alcoves: a little geode cut beside the tube (round a bend, below a lip: the triangle rule), its kit inside.
const alcoves = [];
const nearPoi = (x, y) => Math.min(...pois.map(p => Math.hypot(p[0] - x, p[1] - y)), ...alcoves.map(a => Math.hypot(a.x - x, a.y - y)));
const routeTubes = tubesDense.filter(t => t.kind !== 'ramp' && t.zone !== 'V' && !t.id.startsWith('V'));
let qi = 0, side = 1;
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
		const rad = t.kind === 'passage' ? 0 : t.r;
		const kind = queue[qi % Math.max(1, queue.length)] || 'resonant_outcrop';
		const big = kind === 'cave_lake' || kind === 'lumenhorn_hall' || kind === 'druseback_wallow';
		const ar = big ? 13 : 8;
		const off = rad + ar * 0.75 + (t.kind === 'passage' ? t.w / 2 : 0);
		const ax2 = bx + nx * off, ay2 = by + ny * off;
		const id = `A${alcoves.length + 1}`;
		const hall = { id, kind: t.zone === 'C5' ? 'chamber' : 'alcove', zone: t.zone, name: 'an alcove', x: r1(ax2), y: r1(ay2), floor: r2(bz + (kind === 'cave_lake' ? -1.5 : 0.2)), rx: ar, ry: ar * 0.85, h: big ? 11 : 7, yaw: yawOf(nx, ny), alcove: true, tube: t.id };
		if (t.zone === 'C5') { hall.rx = 7; hall.ry = 5; hall.h = 5; }
		HALLS.push(hall);
		alcoves.push(hall);
		kit.push({ id: `k_${kind}_${id}`, kind, x: hall.x, y: hall.y, z: hall.floor, hall: id, cave: true });
		qi++;
		acc = 0;
	}
}
// Every alcove opens off its tube with a short throat (the air shapes overlap: the tube's and the alcove's).
for (const a of alcoves) {
	const t = tubesDense.find(q => q.id === a.tube);
	let best = t.pts[0], bd = 1e9;
	for (const p of t.pts) { const d = Math.hypot(p[0] - a.x, p[1] - a.y); if (d < bd) { bd = d; best = p; } }
	const w = t.kind === 'passage' ? 3.5 : Math.min(3.5, t.r * 0.7);
	tubesDense.push({ id: `${a.id}-throat`, kind: 'tube', zone: a.zone, r: w, pts: densify([[best[0], best[1], best[2]], [a.x, a.y, a.floor]]) });
}

// ---------------------------------------------------------------- keystone chimes, waymark chimes, chandeliers, pools
// Keystone chime per hall (crystal 2.1): at its far end from the first way in, under a crystal cluster.
const tubeEndsAt = id => tubesDense.filter(t => !t.id.endsWith('-throat')).flatMap(t => [t.pts[0], t.pts[t.pts.length - 1]]).filter(p => { const h = hallById(id); return Math.hypot((p[0] - h.x) / h.rx, (p[1] - h.y) / h.ry) < 1.05; });
for (const h of HALLS) {
	if (h.kind === 'vault' || h.cup || h.alcove || h.kind === 'chamber') continue;
	const ends = tubeEndsAt(h.id);
	let dx = 1, dy = 0;
	if (ends.length) { dx = h.x - ends[0][0]; dy = h.y - ends[0][1]; const L = Math.hypot(dx, dy) || 1; dx /= L; dy /= L; }
	const k = 0.78;
	h.keystone = [r1(h.x + dx * h.rx * k), r1(h.y + dy * h.ry * k), h.floor];
}
// Waymark chime stones every 150 m along the surface roads and the cave tubes (world-map waymark: the Wardcarvers').
const chimes = [];
const walkEvery = (pts, every, fn, start = every / 2) => { let acc = 0, next = start; for (let i = 1; i < pts.length; i++) { const a = pts[i - 1], b = pts[i]; const L = Math.hypot(b[0] - a[0], b[1] - a[1]); while (acc + L >= next) { const t = (next - acc) / L; fn(lerp(a[0], b[0], t), lerp(a[1], b[1], t), a.length > 2 ? lerp(a[2], b[2], t) : null, yawOf(b[0] - a[0], b[1] - a[1])); next += every; } acc += L; } };
for (const R of ROADS) walkEvery(R.pts, 150, (x, y, z, yaw) => chimes.push({ x: r1(x + Math.cos((yaw + 90) * DEG) * 3), y: r1(y + Math.sin((yaw + 90) * DEG) * 3), z: r2(hAt(x, y)), yaw, surface: true }), 60);
for (const t of tubesDense) {
	if (t.kind === 'ramp' || t.id.endsWith('-throat') || t.zone === 'V') continue;
	const side = t.kind === 'passage' ? t.w / 2 - 0.4 : t.r * 0.78;
	walkEvery(t.pts, 150, (x, y, z, yaw) => chimes.push({ x: r1(x + Math.cos((yaw + 90) * DEG) * side), y: r1(y + Math.sin((yaw + 90) * DEG) * side), z: r2(z), yaw, tube: t.id }), 75);
}
// Shard chandeliers (crystal 2.8): the Chandelier Gallery's six (one fallen: H4), one each in two C2 halls; on the
// Tyrant's route so the player can drop them on it.
const chandeliers = [];
{
	const b3 = hallById('B3');
	for (let c = 0; c < 6; c++) chandeliers.push({ id: `ch${c + 1}`, hall: 'B3', x: r1(b3.x - 18 + c * 7.2), y: r1(b3.y + (c % 2 ? 1.5 : -1.5)), floor: b3.floor, top: r2(b3.floor + b3.h * 0.96), fallen: c === 5 });
	for (const id of ['G7', 'G6']) { const h = hallById(id); chandeliers.push({ id: `ch_${id}`, hall: id, x: r1(h.x + 4), y: r1(h.y - 3), floor: h.floor, top: r2(h.floor + h.h * 0.97), fallen: false }); }
	const b1 = hallById('B1');
	chandeliers.push({ id: 'ch_B1', hall: 'B1', x: r1(b1.x + 52), y: r1(b1.y + 18), floor: b1.floor, top: r2(b1.floor + b1.h * 0.78), fallen: false });
}
// Scalding pools (crystal 2.8): basins cut into the floor of the C3 halls beside the beam routes; teal water.
const pools = [];
const pool = (hall, fx, fy, rx, ry, kind = 'scald') => { const h = hallById(hall); pools.push({ hall, kind, x: r1(h.x + fx * h.rx), y: r1(h.y + fy * h.ry), rx, ry, z: r2(h.floor - 0.35), depth: 1.3 }); };
pool('B1', -0.55, -0.25, 13, 8); pool('B1', -0.2, 0.3, 16, 9); pool('B1', 0.2, -0.3, 12, 10); pool('B1', 0.52, 0.22, 15, 8); pool('B1', 0.05, 0.0, 9, 6);
pool('B2', 0.0, 0.1, 7, 5.5); pool('B2', -0.5, -0.35, 4, 3); pool('B2', 0.45, -0.4, 4, 3.2);
pool('B4', 0.1, 0.0, 9, 7, 'wallow');
pool('B5', -0.35, 0.35, 6, 4.5);
// The cave lakes and the Vault's lake (cold, still: not scalding).
for (const k of kit) if (k.kind === 'cave_lake') { const h = hallById(k.hall); pools.push({ hall: k.hall, kind: 'lake', x: h.x, y: h.y, rx: h.rx * 0.7, ry: h.ry * 0.6, z: r2(h.floor - 0.3), depth: 2.4 }); }
{ const v4 = hallById('V4'); pools.push({ hall: 'V4', kind: 'lake', x: v4.x, y: v4.y + 3, rx: 9, ry: 22, z: r2(v4.floor - 0.3), depth: 3.0 }); }
// The selenite beams of the Hall of Beams (crystal 2.3 L4): three great beams bridging the width 20 m over the pools,
// walkable; the scenery adds the thicket (built in the game from the hall's seed).
const beams = [];
{
	const b1 = hallById('B1');
	for (const [fx, w] of [[-0.42, 1.35], [0.0, 1.45], [0.4, 1.3]]) {
		const x = b1.x + fx * b1.rx;
		beams.push({ hall: 'B1', a: [r1(x - 6), r1(b1.y - b1.ry * 0.86), r2(b1.floor + 18.5)], b: [r1(x + 5), r1(b1.y + b1.ry * 0.86), r2(b1.floor + 21.5)], thick: w, walk: true });
	}
}

// ---------------------------------------------------------------- ward fields and their keystones (crystal 2.8)
// 34 fields: every Ward Chamber threshold, the Severance Gate, the Vault's inner doors, the chime-junction caches, the
// vault caches; pattern A live normally, B only in the Resonance (12 drop, 12 rise: the ward shift).
const wards = [];
const ward = (id, at, yaw, w, h, key, pattern = 'AB', extra = {}) => wards.push({ id, x: r1(at[0]), y: r1(at[1]), z: r2(at[2]), yaw: Math.round(yaw), w, h, key: [r1(key[0]), r1(key[1]), r2(key[2])], pattern, ...extra });
// A passage's ends: a field 3 m inside each end, its keystone above the door on the chamber's side.
for (const t of tubesDense) {
	if (t.kind !== 'passage' || t.id.endsWith('-throat')) continue;
	const ends = [[t.pts[0], t.pts[Math.min(2, t.pts.length - 1)]], [t.pts[t.pts.length - 1], t.pts[Math.max(0, t.pts.length - 3)]]];
	ends.forEach(([p, q], e) => {
		if (t.id === 'severance_door' && e === 0) return;   // the stair's foot: the far end is the hall's door
		const dx = q[0] - p[0], dy = q[1] - p[1], L = Math.hypot(dx, dy) || 1;
		const at = [p[0] + dx / L * 3, p[1] + dy / L * 3, lerp(p[2], q[2], 3 / Math.max(3, L))];
		const key = [p[0] - dx / L * 1.5, p[1] - dy / L * 1.5, at[2] + t.h + 1.2];
		const n = wards.length;
		const pattern = t.id === 'gate' || t.id === 'vault_door' || t.zone === 'V' ? 'AB' : n % 5 === 1 ? 'A' : n % 5 === 3 ? 'B' : 'AB';
		ward(`w_${t.id}_${e}`, at, yawOf(dx, dy), t.w, t.h, key, pattern, { tube: t.id, door: t.door || '' });
	});
}
// The Severance Gate's keystone is high above the door on the gorge face (an arrow or a spell: crystal 2.4 H8).
{ const g = wards.find(w => w.id === 'w_gate_0'); if (g) { g.key = [1800, 5406.5, 33]; g.gate = true; } }
// The chime-junction caches: a ward across each alcove's throat, its keystone across the tube (a shot from the tube).
for (const k of kit) {
	if (k.kind !== 'chime_cache' || !k.hall) continue;
	const a = hallById(k.hall);
	const th = tubesDense.find(t => t.id === `${a.id}-throat`);
	if (!th) continue;
	const p = th.pts[0], q = th.pts[th.pts.length - 1];
	const dx = q[0] - p[0], dy = q[1] - p[1], L = Math.hypot(dx, dy) || 1;
	const mid = [lerp(p[0], q[0], 0.6), lerp(p[1], q[1], 0.6), lerp(p[2], q[2], 0.6)];
	ward(`w_cache_${a.id}`, mid, yawOf(dx, dy), 7.5, 6, [p[0] - dx / L * 4, p[1] - dy / L * 4, p[2] + 6.5], wards.length % 2 ? 'A' : 'AB', { cache: k.id });
}
// Ward shift fields that rise only in the Resonance (pattern B): across tube mouths into the C5 halls and the C2 junctions.
const shiftSpots = ['W6-W7', 'G3-G6', 'G6-J2', 'J3-B2', 'B1-B3', 'G4-J3', 'B4-B5', 'J2-H6', 'G7-G3', 'G1-G7', 'silver', 'B6-J2'];
for (const id of shiftSpots) {
	if (wards.length >= 34) break;
	const t = tubesDense.find(q => q.id === id);
	if (!t) continue;
	const i = Math.floor(t.pts.length * 0.3), p = t.pts[i], q = t.pts[i + 1];
	const dx = q[0] - p[0], dy = q[1] - p[1];
	const nx = -dy, ny = dx, L = Math.hypot(nx, ny) || 1;
	ward(`w_shift_${id}`, p, yawOf(dx, dy), 2 * t.r * 0.95, t.r * 1.5, [p[0] + nx / L * t.r * 0.8, p[1] + ny / L * t.r * 0.8, p[2] + t.r * 1.2], 'B', { tube: id, shift: true });
}

// ---------------------------------------------------------------- resources (crystal 2.6, 2.7)
const resources = { resonant: [], dust: [], selenite: [], spar: [], amethyst: [], saltpetre: [], ochre: [], wax: [], copper: [], tin: [], iron: [], coal: [], silver: [], star: [], gems: [], rhubarb: [], glowmelon: [], singing: [] };
const onWall = (h, a, f = 0.86) => [r1(h.x + Math.cos(a) * h.rx * f), r1(h.y + Math.sin(a) * h.ry * f), h.floor];
// 48 resonant crystal nodes in the C2 and C5 linings, richest in G7 (the Upper Geode), plus two surface outcrops.
{
	const homes = [['G7', 8], ['G1', 5], ['G2', 4], ['G3', 6], ['G4', 4], ['G5', 3], ['G6', 5], ['W4', 3], ['W6', 3], ['W7', 4], ['W2', 1], ['W5', 2]];
	for (const [id, n] of homes) { const h = hallById(id); for (let i = 0; i < n; i++) resources.resonant.push([...onWall(h, (i / n) * 2 * Math.PI + 0.4), id]); }
	for (const k of kit) if (k.kind === 'resonant_outcrop' && k.surface) resources.resonant.push([k.x, k.y, k.z, k.id]);
}
for (const id of ['G1', 'G3', 'G5', 'G6']) { const h = hallById(id); for (let i = 0; i < 2; i++) resources.dust.push([...onWall(h, i * 2.4 + 1, 0.5), id]); }
for (const id of ['B1', 'B1', 'B1', 'B2', 'B4', 'B5', 'B3']) { const h = hallById(id); resources.selenite.push([...onWall(h, resources.selenite.length * 1.3, 0.84), id]); }
for (const id of ['W4', 'W3', 'W2', 'W6', 'W7', 'W1']) { const h = hallById(id); resources.spar.push([...onWall(h, resources.spar.length * 2.1 + 0.5, 0.7), id]); }
{ const t = tubesDense.find(q => q.id === 'silver'); for (let i = 0; i < 4; i++) { const p = t.pts[Math.floor((i + 0.5) / 4 * t.pts.length)]; resources.amethyst.push([p[0], p[1], p[2], 'silver']); resources.silver.push([r1(p[0] + 2), r1(p[1] + 1), p[2], 'silver']); } }
{ const h = hallById('H6'); for (let i = 0; i < 4; i++) resources.saltpetre.push([...onWall(h, i * 1.6, 0.6), 'H6']); }
for (const k of kit) if (k.kind === 'roost') resources.saltpetre.push([k.x, k.y, k.z, k.id]);
{ const o = tubesDense.find(t => t.id === 'organ').pts; for (let i = 1; i <= 6; i++) { const p = o[Math.floor(i / 7 * o.length)]; resources.ochre.push([p[0], p[1], p[2], 'organ']); } }
for (let i = 0; i < 6; i++) resources.wax.push([r1(2252 + Math.cos(i) * 14), r1(5902 + Math.sin(i) * 10), r2(hAt(2252 + Math.cos(i) * 14, 5902 + Math.sin(i) * 10)), 'S4']);
{ const v1 = hallById('V1'); resources.wax.push([v1.x + 3, v1.y, v1.floor, 'V1']); }
for (const id of ['G1', 'G7', 'G2']) { const h = hallById(id); resources.copper.push([...onWall(h, 2.2, 0.8), id]); resources.tin.push([...onWall(h, 4.0, 0.8), id]); }
{ const h = hallById('G6'); for (let i = 0; i < 4; i++) { resources.iron.push([...onWall(h, i * 1.5, 0.82), 'G6']); } for (let i = 0; i < 3; i++) resources.coal.push([...onWall(h, i * 2 + 0.7, 0.8), 'G6']); for (let i = 0; i < 2; i++) resources.gems.push([...onWall(h, i * 3 + 1.2, 0.7), 'G6']); }
{ const t = tubesDense.find(q => q.id === 'silver'); for (let i = 0; i < 3; i++) { const p = t.pts[Math.floor((i + 0.2) / 3 * t.pts.length)]; resources.iron.push([r1(p[0] - 2), p[1], p[2], 'silver']); } }
{ const h = hallById('W7'); for (let i = 0; i < 4; i++) resources.star.push([...onWall(h, i * 1.57 + 0.3, 0.78), 'W7']); }
// Crops: candle rhubarb wild at the Wax Fields; glowmelon in the Steam Baths' warm dark (both need dark).
for (let i = 0; i < 14; i++) { const a = i * 2.39996, r = 6 + 2.2 * Math.sqrt(i) * 3; const x = 2252 + Math.cos(a) * r, y = 5902 + Math.sin(a) * r * 0.8; resources.rhubarb.push([r1(x), r1(y), r2(hAt(x, y)), 'S4']); }
{ const h = hallById('B2'); for (let i = 0; i < 5; i++) resources.glowmelon.push([...onWall(h, i * 1.25 + 2.5, 0.72), 'B2']); }
// The six singing geodes (crystal 2.10): sealed geodes that split only in the Resonance.
for (const id of ['G1', 'G3', 'G6', 'B1', 'W6', 'W7']) { const h = hallById(id); resources.singing.push([...onWall(h, 5.1, 0.9), id]); }

// ---------------------------------------------------------------- lore (crystal 2.20)
const lore = [
	{ item: 'lore_watch_tally', site: 'S1' }, { item: 'lore_childs_tuning_fork', site: 'H1' }, { item: 'lore_count_of_the_quiet', site: 'H5' },
	{ item: 'lore_order_of_the_door', site: 'H8' }, { item: 'lore_two_plates', site: 'V5' }, { item: 'lore_wax_seal', site: 'V2' },
	{ item: 'lore_miners_last_page', site: 'S5' },
];

// ---------------------------------------------------------------- the content file
const caveZoneOf = (x, y, z) => { for (const h of HALLS) { const q = Math.hypot((x - h.x) / h.rx, (y - h.y) / h.ry); if (q < 1.05 && z > h.floor - 3 && z < h.floor + h.h + 2) return h.zone; } return null; };
const contentOut = {
	_comment: 'Generated by Tools/terrain/crystal.mjs (edit the script, not this file). The Crystal caves content (cooks/biome-crystal.md, cooks/build-biome-crystal.md), parsed by FWRCrystalData (WRCrystalTypes.cpp). Positions are map metres on the Crystal land\'s frame (Data/lands/crystal.json origin / mapCentre: x east, y south), z metres above the sea. halls: air shapes (geode/beams/prism/alcove: an ellipsoid of radii rx, ry, h/1.2 with a flat floor; chamber/vault: a hewn box rx, ry half sizes, h high); tubes: [x,y,floor] every 3 m (tube: radius r, flat floor 0.55 r under the axis, benches if set; passage: w x h hewn; ramp: open to the sky at its top); shafts: cylinders (descent, stair, vent, drop, light); pools: basins under hall floors (scald, wallow, lake: water at z); beams: the walkable selenite beams; wards: fields (centre, yaw of their normal, w x h, key = the keystone plate, pattern A/B/AB); chandeliers: hanging point; thin: thin floors over cups; chimes: waymark chime stones; resources: node points per kind [x,y,z,of].',
	land: 'crystal',
	frame: { x0: X0, y0: Y0, x1: X1, y1: Y1, step: STEP },
	gorge: { x0: GORGE.x0, x1: GORGE.x1, xTop: GORGE.xTop, y0: GORGE.y0, y1: GORGE.y1, half: GORGE.half },
	halls: HALLS.map(h => ({ id: h.id, kind: h.kind, zone: h.zone, name: h.name, x: h.x, y: h.y, floor: h.floor, rx: h.rx, ry: h.ry, h: h.h, yaw: h.yaw, cup: !!h.cup, alcove: !!h.alcove, site: h.site || '', shrine: h.shrine || '', arena: h.arena || '', lair: h.lair || '', gallery: h.gallery || '', landmark: h.landmark || '', lake: !!h.lake, keystone: h.keystone || null })),
	tubes: tubesDense.map(t => ({ id: t.id, kind: t.kind, zone: t.zone, r: t.r || 0, w: t.w || 0, h: t.h || 0, benches: !!t.benches, blend: t.blend || '', door: t.door || '', descent: t.descent || '', gallery: t.gallery || '', pts: t.pts })),
	shafts: SHAFTS,
	descents: DESCENTS,
	vents: VENTS,
	domes: DOMES.map(d => ({ ...d, z: r2(hAt(d.x, d.y)), rim: r2(hAt(d.x + d.r * 1.15, d.y)) })),
	pool: POOL,
	vista: VISTA,
	sites,
	shrines,
	runback,
	kit,
	roads: ROADS.map(r => ({ id: r.id, name: r.name, pts: r.pts.map(([x, y]) => [x, y, r2(hAt(x, y))]) })),
	chimes,
	chandeliers,
	pools,
	beams,
	thin: THIN,
	wards,
	resources,
	lore,
};
fs.writeFileSync(path.join(root, 'Data', 'crystal.json'), JSON.stringify(contentOut) + '\n');

// ---------------------------------------------------------------- the runtime land (WRLand, Data/lands/crystal.json)
const C = c => [parseInt(c.slice(1, 3), 16), parseInt(c.slice(3, 5), 16), parseInt(c.slice(5, 7), 16)];
const ZC = 16, ZN = Math.ceil((N - 1) * STEP / ZC) + 1;
const zoneRows = [];
for (let j = 0; j < ZN; j++) { let row = ''; for (let i = 0; i < ZN; i++) row += zoneAt(X0 + (i + 0.5) * ZC, Y0 + (j + 0.5) * ZC); zoneRows.push(row); }
function writeLand() {
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'crystal_ground.bin'), Buffer.concat([hb, sb]));
	// Where a body is rightly under the ground (no rescue): every hall, tube, passage and shaft.
	const ways = [];
	for (const h of HALLS) {
		const c = Math.cos(h.yaw * DEG), s = Math.sin(h.yaw * DEG), hl = Math.max(0, h.rx - h.ry);
		ways.push([r1(h.x - c * hl), r1(h.y - s * hl), r1(h.x + c * hl + 0.1), r1(h.y + s * hl), r1(Math.min(h.rx, h.ry) + 3)]);
	}
	for (const t of tubesDense) { const rr = (t.kind === 'passage' ? t.w / 2 : t.r) + 3; for (let q = 0; q < t.pts.length - 1; q += 4) { const a = t.pts[q], b = t.pts[Math.min(t.pts.length - 1, q + 4)]; ways.push([a[0], a[1], b[0], b[1], r1(rr)]); } }
	for (const s of SHAFTS) if (s.kind !== 'light') ways.push([s.x, s.y, s.x + 0.1, s.y, r1(s.r + 2)]);
	const campRows = CAMPS.map(c => [c.x, c.y, c.yaw]);
	const land = {
		_comment: 'Generated by Tools/terrain/crystal.mjs (edit the script, not this file). The Crystal caves as a runtime land (WRLand, cooks/build-biome-crystal.md 2.0-2.2): its ground is the heightfield in crystal_ground.bin (4 m samples from x0, y0 in map metres: uint16 heights = offset + m * unitsPerMetre, then one surface byte each), built as procedural-mesh chunks on every machine. It sits on the Wasteland\'s frame (origin / mapCentre the same) so the Severance runs from one land into the other; priority 1: over the Wasteland\'s Spine tip and gorge floor this ground is the ground (it meets the Wasteland\'s gorge floor exactly at x +1,400). Colours (sRGB): zones by the zone grid letter (1 the Basalt Mouths, s the Spine, g the Severance, ~ the sea), surfaces by class (hex basalt pavement, salt-grass, heather, column cliffs, black shingle, the gorge\'s bleached grey, the chime roads\' grit, the domes\' amethyst grit, the Spine), rock on slopes. underground.ways: every hall, tube and shaft of the caves (no rescue there). camps: PlayerStarts tagged crystal at the heads of the three geos, a safe zone each, the starter stations at the middle one. crossing: the Chime Crossing from the island.',
		id: 'crystal', biome: 'crystal', name: 'the Crystal caves', priority: 1,
		origin: ORIGIN, mapCentre: MAPC,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 900000, chunksPerFrame: 4,
		seaFloor: -16,
		heightfield: { file: 'crystal_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 40,
		zones: [
			{ id: 'C1', letter: '1', name: 'the Basalt Mouths', ground: C('#26262B'), groundAlt: C('#1C1C20'), rough: 0.86, region: 'crystal_c1', bed: 'crystal_c1', speck: { colors: [C('#7E8C5A'), C('#C8C2B4')], amount: 0.08, scale: 3 } },
			{ id: 'spine', letter: 's', name: 'the Spine', ground: C('#2A2A30'), groundAlt: C('#34343A'), rough: 0.84, region: 'crystal_c1', bed: 'crystal_c1' },
			{ id: 'severance', letter: 'g', name: 'the Severance', ground: C('#8E8C92'), groundAlt: C('#6E6C72'), rough: 0.9, region: 'crystal_severance', bed: 'crystal_severance', speck: { colors: [C('#B9B4C4'), C('#5A5860')], amount: 0.1, scale: 4 } },
			{ id: 'sea', letter: '~', name: 'the sea floor', ground: C('#1E2226'), groundAlt: C('#2A3036'), rough: 0.8 },
		],
		surfaces: [
			{ id: 'pave', class: SURF.pave, color: C('#1F1F24'), rough: 0.78, speck: { colors: [C('#3A3A42'), C('#E8E4DA')], amount: 0.14, scale: 1.2 } },
			{ id: 'grass', class: SURF.grass, color: C('#7E8C5A'), rough: 0.92, speck: { colors: [C('#E9E2EE'), C('#5E6A40')], amount: 0.12, scale: 2 } },
			{ id: 'heather', class: SURF.heather, color: C('#5E4A48'), rough: 0.93, speck: { colors: [C('#8A5A78'), C('#4A3A30')], amount: 0.2, scale: 2 } },
			{ id: 'cliff', class: SURF.cliff, color: C('#1A1A1E'), rough: 0.82, speck: { colors: [C('#D8D4CC')], amount: 0.05, scale: 2 } },
			{ id: 'shingle', class: SURF.shingle, color: C('#2E3034'), rough: 0.55, speck: { colors: [C('#6B3FA0'), C('#55585E')], amount: 0.15, scale: 1.5 } },
			{ id: 'gorge', class: SURF.gorge, color: C('#8E8C92'), rough: 0.92, speck: { colors: [C('#B9B4C4'), C('#5A5860')], amount: 0.18, scale: 3 } },
			{ id: 'path', class: SURF.path, color: C('#4A4648'), rough: 0.95, speck: { colors: [C('#D8D0FF')], amount: 0.06, scale: 1.5 } },
			{ id: 'dome', class: SURF.dome, color: C('#3A2A4C'), rough: 0.6, speck: { colors: [C('#6B3FA0'), C('#EDEAF4')], amount: 0.3, scale: 1.2 } },
			{ id: 'sea', class: SURF.sea, color: C('#1E2226'), rough: 0.8 },
			{ id: 'spine', class: SURF.spine, color: C('#2A2A30'), rough: 0.84 },
		],
		slopeRock: { color: C('#1C1C20'), fromDeg: 36, toDeg: 50 },
		underground: { letters: '', ways },
		buildFirst: campRows.map(([x, y]) => [x, y]),
		camps: { tag: 'crystal', safeRadius: 40, list: campRows, stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
		crossing: { _comment: 'The Chime Crossing (cooks/build-biome-crystal.md 2.0.3): a longboat on the island\'s south-east shore and its twin beached in the Bell Geo\'s cove. Hold E to row across (a short dark crossing). island: metres on the island (settled to the shore at runtime); land: map metres.', island: [1500, 1300], islandFace: 0, land: CAMPS[1].x + 22 > 0 ? [r1(CAMPS[1].x + 24), r1(CAMPS[1].y + 4)] : [0, 0], landFace: 0, holdSeconds: 2, fadeSeconds: 1.2, name: 'the Chime Crossing' },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'crystal.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}
writeLand();
// The map's place list: camps, descents (caves), landmarks.
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.crystal);
		for (const s of sites) {
			const kind = s.camp ? 'camp' : s.landmark ? 'landmark' : s.kind === 'descent' ? 'cave' : null;
			if (!kind) continue;
			const [wx, wy, wz] = toWorld(s.x, s.y, s.z);
			dress.pois.push({ name: s.name, kind, x: wx, y: wy, z: wz, biome: 'crystal', crystal: true });
		}
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

// ---------------------------------------------------------------- preview (.lane-tmp, not committed)
{
	const img = Buffer.alloc(N * N * 3);
	const zc = { '1': [40, 40, 46], 's': [52, 52, 60], 'g': [142, 140, 146], '~': [24, 40, 56] };
	for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
		const k = j * N + i, x = X0 + i * STEP, y = Y0 + j * STEP;
		let c = (zc[zoneAt(x, y)] || [0, 0, 0]).slice();
		const s = SURFM[k];
		if (s === SURF.grass) c = [126, 140, 90]; else if (s === SURF.heather) c = [94, 74, 72]; else if (s === SURF.cliff) c = [26, 26, 30];
		else if (s === SURF.shingle) c = [70, 72, 78]; else if (s === SURF.path) c = [150, 140, 150]; else if (s === SURF.dome) c = [107, 63, 160]; else if (s === SURF.hidden) c = [255, 0, 255];
		const sh = Math.max(0.45, Math.min(1.5, 1 + (HF[k] - (HF[k - 1] ?? HF[k])) * 0.25 + HF[k] / 500));
		if (HF[k] < 0 && s !== SURF.hidden) c = [24, 40, 56];
		img[k * 3] = Math.min(255, c[0] * sh); img[k * 3 + 1] = Math.min(255, c[1] * sh); img[k * 3 + 2] = Math.min(255, c[2] * sh);
	}
	const dot = (x, y, r, col) => { const ci = Math.round((x - X0) / STEP), cj = Math.round((y - Y0) / STEP); for (let a = -r; a <= r; a++) for (let b = -r; b <= r; b++) { const ii = ci + a, jj = cj + b; if (ii >= 0 && jj >= 0 && ii < N && jj < N) { const k = jj * N + ii; img[k * 3] = col[0]; img[k * 3 + 1] = col[1]; img[k * 3 + 2] = col[2]; } } };
	for (const t of tubesDense) for (const p of t.pts) dot(p[0], p[1], 0, t.zone === 'C5' || t.zone === 'V' ? [255, 200, 80] : [255, 120, 60]);
	for (const h of HALLS) dot(h.x, h.y, Math.max(1, Math.round(Math.min(h.rx, h.ry) / 8)), h.zone === 'C2' ? [200, 150, 255] : h.zone === 'C3' ? [120, 230, 220] : h.zone === 'C4' ? [220, 80, 60] : [255, 255, 160]);
	for (const k of kit) dot(k.x, k.y, 1, [255, 255, 255]);
	for (const s of sites) dot(s.x, s.y, s.landmark ? 4 : 2, s.landmark ? [185, 167, 255] : [255, 255, 0]);
	for (const c of chimes) dot(c.x, c.y, 0, [255, 255, 255]);
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
	fs.writeFileSync(path.join(root, '.lane-tmp', 'crystal.png'), png(N, N, img));
}
let landA = 0, maxH = -1e9; for (let k = 0; k < N * N; k++) { if (HF[k] > 0.5 && SURFM[k] !== SURF.hidden) landA++; maxH = Math.max(maxH, HF[k]); }
console.log(JSON.stringify({ seconds: (Date.now() - t0) / 1000, landKm2: +(landA * STEP * STEP / 1e6).toFixed(2), highest: Math.round(maxH), seamFloor: +SEAM_FLOOR.toFixed(2), halls: HALLS.length, alcoves: alcoves.length, tubes: tubesDense.length, shafts: SHAFTS.length, sites: sites.length, kit: kit.length, wards: wards.length, chimes: chimes.length, chandeliers: chandeliers.length, pools: pools.length, resonant: resources.resonant.length }));
