// The Ocean & reefs (cooks/biome-ocean.md, cooks/build-biome-ocean.md): writes
//   Data/lands/ocean.json + ocean_ground.bin  the coast's ground and sea floor as a runtime heightfield land (WRLand): the
//                                             Spine's east face, Saltgate, the Salt Terraces, the Pound's basin, the
//                                             Blowhole Shelf, the White Strand, the Gate Inlet, the kelp shelf, the reef
//                                             crest and slope, the Blue Wall and the open floor, camps
//   Data/ocean.json                           everything standing on it (parsed by FWROceanData, WROceanTypes.cpp):
//                                             landmarks, sites, kit POIs, decoration, caves, the Drowned Hall, nodes,
//                                             rips, blowholes, fleets, air pockets, arenas, routes, secrets, portal slots
//   Data/island_dressing.json "pois"          the map's place list (camps, landmarks)
// Run: node Tools/terrain/ocean.mjs   (prints the POI spacing check)
// Map metres: x east, y south (numbers grow south), z up; game cm = m x 100.
// THE COORDINATE RULE (build plan 2.1): the cook's coordinates are world-map's; this world puts the Ocean between the Sky's
// Chain Cliffs (y -7,955) and the Storm crown / Swamp estuary (y -5,700), so every cook y goes through Y(): x as written,
// y squeezed from -8,000..-5,400 into -7,955..-5,700.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const X0 = -600, Y0 = -7960, STEP = 5, N = 1001, UNITS = 16;
const SEED = 9203;
const Y = yd => -7955 + (yd + 8000) * 0.8673;
const SEA = 0.6;                                   // the drawn sea's mean surface (m)

// ---------------------------------------------------------------- noise (deterministic)
const hash = (x, y) => { let h = (x * 374761393 + y * 668265263 + SEED * 2147483647) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
	const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
	const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
	return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
const ridged = (x, y, oct = 4) => { let s = 0, a = 0.5, f = 1, w = 1, n = 0; for (let i = 0; i < oct; i++) { let v = 1 - Math.abs(vnoise(x * f, y * f) * 2 - 1); v *= v; v *= w; w = Math.min(1, v * 2); s += v * a; n += a; a *= 0.5; f *= 2.1; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const smin = (a, b, k) => { const h = clamp(0.5 + 0.5 * (b - a) / k, 0, 1); return lerp(b, a, h) - k * h * (1 - h); };
const interp = (pts, v) => {
	if (v <= pts[0][0]) return pts[0][1];
	for (let i = 1; i < pts.length; i++) if (v <= pts[i][0]) return lerp(pts[i - 1][1], pts[i][1], (v - pts[i - 1][0]) / (pts[i][0] - pts[i - 1][0]));
	return pts[pts.length - 1][1];
};
let rngState = 41011;
const rnd = () => { rngState = (rngState * 1103515245 + 12345) & 0x7fffffff; return rngState / 0x7fffffff; };
const r1 = v => +v.toFixed(1);
const C = hex => [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16)];
const distSeg = (px, py, ax, ay, bx, by) => { const dx = bx - ax, dy = by - ay, L = dx * dx + dy * dy; const t = L ? clamp(((px - ax) * dx + (py - ay) * dy) / L, 0, 1) : 0; return Math.hypot(px - ax - dx * t, py - ay - dy * t); };
const distPoly = (px, py, pts) => { let d = 1e9; for (let i = 0; i < pts.length - 1; i++) d = Math.min(d, distSeg(px, py, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])); return d; };

// ---------------------------------------------------------------- neighbours (read so the seams meet)
const readSky = () => {
	try {
		const b = fs.readFileSync(path.join(root, 'Data', 'lands', 'sky_ground.bin')); const n = 611;
		return (x, y) => { const i = Math.round((x + 1150) / 5), j = Math.round((y + 10100) / 5); if (i < 0 || j < 0 || i >= n || j >= n) return false; return b[n * n * 2 + j * n + i] !== 255; };
	} catch { return () => false; }
};
const skyHas = readSky();
const readStorm = () => {
	try {
		const b = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16')); const n = 2017;
		return (x, y) => { const i = Math.round((x + 2900) / 2), j = Math.round((y + 5936) / 2); if (i < 0 || j < 0 || i >= n || j >= n) return null; return (b.readUInt16LE((j * n + i) * 2) - 32768) / 16; };
	} catch { return () => null; }
};
const stormH = readStorm();

// ---------------------------------------------------------------- the shape of the coast
// The land runs between two straits: north (the Sky's Chain Cliffs plunge into a 25-40 m sea gap at y ~ -7,940) and south
// (the Storm crown rises from the sea at y ~ -5,700; east of it the Swamp's estuary). West the Spine falls into the sea
// where the Desert's ground goes later. The east coast is the cook's x ~ +1,850.
const NORTH = x => -7928 + 8 * fbm(x / 120, 3.3);                 // the north strait's shore
const SOUTH = x => (x < 1140 ? -5805 : -5745) + 10 * fbm(x / 140, 7.1);   // the south shore (the estuary east of x 1,140)
const COAST = y => {
	let c = 1850 + 46 * fbm(y / 260 + 4.4, 0.7) + 14 * fbm(y / 60, 2.2);
	if (y < Y(-6950)) c += 22 * ridged(y / 90, 1.9) - 10;                    // O2's shelf edge: ragged
	c -= 34 * Math.exp(-(((y - Y(-7640)) / 34) ** 2));                      // Barnacle Cove
	c += 22 * Math.exp(-(((y - Y(-6100)) / 110) ** 2));                     // the Turtle Beach's gentle bulge
	return c;
};
const WESTSEA = y => -330 + 18 * fbm(y / 150, 9.9);
// Saltgate: the pass through the Spine, a 60 m saddle at its crest (x ~ +60), falling east down a valley to the Gate Inlet.
const SG_Y = x => Y(-6700) + 22 * Math.sin(x / 260) - 0.04 * (x - 950);
const SG_FLOOR = x => interp([[-340, 18], [-200, 44], [-60, 58], [80, 62], [220, 56], [400, 40], [600, 22], [780, 9], [900, 3], [960, 1.4]], x);
const SG_W = x => interp([[-340, 120], [0, 90], [400, 110], [800, 160], [960, 200]], x);
// The Gate Inlet: tidal water from the pass's mouth (x 950) to the sea, beside camp 2, the Colossus face-down in it.
const INLET = [[930, SG_Y(930)], [1080, Y(-6740)], [1250, Y(-6735)], [1450, Y(-6720)], [1650, Y(-6705)], [1900, Y(-6700)]];
// The Sluiceway: the Pound's cut to the sea through its east gates.
const POUND = { x: 1000, y: Y(-7300), wall: 120, pool: 40 };
const SLUICE = [[POUND.x + 118, POUND.y + 6], [1300, POUND.y + 10], [1380, POUND.y + 4], [1560, POUND.y - 6], [1900, POUND.y - 14]];
// The coast road (a worn track from camp 1 north to camp 3, the Whalebone Arch on it, the chime beacons along it).
const ROAD = [[1640, Y(-5600)], [1600, Y(-5800)], [1430, Y(-6000)], [1250, Y(-6000)], [1300, Y(-6300)], [1500, Y(-6300)], [1640, Y(-6470)], [1600, Y(-6650)],
	[1380, Y(-6800)], [1420, Y(-7000)], [1560, Y(-7120)], [1600, Y(-7300)], [1520, Y(-7500)], [1560, Y(-7700)]];
// The sandbars off the strand and the rip gaps between them.
const RIPS = [Y(-5720), Y(-5980), Y(-6260), Y(-6520), Y(-6860)];
// The Long Dyke: tower stumps 200 m apart from the Dyke Head out to the reef, then curving north.
const DYKE = (() => {
	const pts = [[1860, Y(-6300)], [2550, Y(-6350)], [2600, Y(-6600)], [2650, Y(-6800)]];
	const out = []; let acc = 0; const seg = [];
	for (let i = 0; i < pts.length - 1; i++) seg.push(Math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]));
	const total = seg.reduce((a, b) => a + b, 0);
	for (let k = 0; k < 11; k++) {
		let d = k * total / 10, i = 0;
		while (i < seg.length - 1 && d > seg[i]) { d -= seg[i]; i++; }
		const t = d / seg[i];
		out.push([lerp(pts[i][0], pts[i + 1][0], t), lerp(pts[i][1], pts[i + 1][1], t)]);
	}
	return out;
})();

// ---------------------------------------------------------------- the ground
const BENCH = [[350, 60], [610, 58], [620, 44], [750, 44], [760, 32], [890, 32], [900, 22], [1030, 22], [1040, 14], [1170, 14], [1180, 8], [1300, 8]];
const benchH = x => {
	// Terraces: flat benches with 10 m risers (a riser is a smooth step, never a cliff of samples).
	for (let i = 0; i < BENCH.length - 1; i++) {
		const [ax, ah] = BENCH[i], [bx, bh] = BENCH[i + 1];
		if (x <= bx) return ah === bh ? ah : lerp(ah, bh, sstep(ax, bx, x));
	}
	return 8;
};
function ground(x, y) {
	const c = COAST(y);
	const d = c - x;                                       // >0 inland from the east coast
	const north = y - NORTH(x), south = SOUTH(x) - y, west = x - WESTSEA(y);
	const toEdge = Math.min(north, south, west);           // distance to the north / south / west shores (inland > 0)
	const shelf = y < Y(-6950);                            // O2 north of the cook's -6,950; O1 south of it
	// --- the sea (east of the coast): the kelp shelf, the reef, the Blue Wall, the open floor
	const sea = (xx) => {
		let h;
		if (xx < 2500) h = lerp(-0.5, -25, Math.pow(sstep(c, 2500, xx), 0.85)) + 2.2 * fbm(xx / 70, y / 70);
		else if (xx < 2800) {
			const crest = 2560 + 50 * fbm(y / 300, 5.5);
			h = -0.5 - 4.5 * Math.min(1, Math.abs(xx - crest) / 140) + 1.0 * ridged(xx / 25, y / 25);
			h = Math.max(h, lerp(-25, -3, sstep(2480, 2560, xx)));
		}
		else if (xx < 3500) h = lerp(-2, -35, sstep(2800, 3500, xx)) + 2.5 * fbm(xx / 40, y / 40);
		else if (xx < 3700) h = lerp(-35, -70, sstep(3500 + 30 * fbm(y / 200, 1.1), 3620 + 30 * fbm(y / 200, 1.1), xx));
		else h = lerp(-70, -90, sstep(3700, 4400, xx)) + 2 * fbm(xx / 120, y / 120);
		// The kelp shelf's sand channels and the reef flat's lagoons (behind the crest: 2-6 m deep).
		if (xx > 2300 && xx < 2540) h = Math.min(h, -3 - 3 * (0.5 + 0.5 * fbm(xx / 60, y / 60)));
		return h;
	};
	if (d < 0) {
		let h = sea(x);
		// The strand's bars: two low ridges 60 and 120 m out, cut by the rip gaps.
		if (!shelf && d > -170) {
			const bar = Math.exp(-(((d + 60) / 18) ** 2)) * 1.6 + Math.exp(-(((d + 120) / 22) ** 2)) * 1.2;
			const gap = RIPS.some(ry => Math.abs(y - ry) < 6);
			h = Math.max(h, lerp(0.4, -3.2, sstep(0, 170, -d)) + (gap ? -0.8 : bar));
		}
		// The shelf's wave-cut platform runs out under the sea a little, then a 2-4 m step down.
		if (shelf && d > -40) h = Math.max(h, lerp(-0.6, -4.5, sstep(0, 40, -d)) - 0.8 * ridged(x / 12, y / 12));
		return { h: Math.max(h, toEdge < 0 ? -13 : -100), sea: true, d };
	}
	// --- the land
	let h;
	if (x > 1300 || (shelf && x > 1100)) {
		if (shelf) {
			// O2: a wave-cut platform of limestone 0.4-3 m, its surge channels and pools.
			h = 0.9 + 2.0 * sstep(0, 260, d) + 6.0 * sstep(230, 300, d) + 0.6 * fbm(x / 30, y / 30) + 0.5 * ridged(x / 9, y / 9);   // the platform, then a raised beach at ~9 m
			const chan = Math.abs(fbm(x / 90 + 2.2, y / 26 + 7.3));
			if (chan < 0.05 && d < 260) h -= 3.0 * (1 - chan / 0.05);                          // surge channels run inland
			const pool = vnoise(x / 22, y / 22);
			if (pool > 0.78 && d > 20) h -= 1.4 * sstep(0.78, 0.9, pool);                       // pools
		} else {
			// O1: beach 0-3 m over 100 m, then dunes 3-9 m, to the terrace foot.
			const beach = 0.3 + 2.7 * Math.pow(sstep(0, 110, d), 0.9);
			const dunes = 3 + 6 * Math.pow(Math.max(0, fbm(x / 80, y / 60) * 0.5 + 0.5), 1.3) * sstep(90, 170, d);
			h = d < 110 ? beach : lerp(beach, dunes, sstep(100, 160, d));
			h = Math.max(h, lerp(8, h, sstep(1250, 1320, x)));                                // meets the terrace foot
		}
	} else if (x > 350) {
		h = benchH(x) + 0.35 * fbm(x / 40, y / 40);
	} else {
		// The Spine: up from the terraces' top to a ridge of 380-520 m, down its west face into the sea.
		const crest = 60 + 30 * fbm(y / 500, 0.4);
		const top = 440 + 70 * fbm(y / 400, 3.3) + 60 * ridged(x / 160, y / 160);
		h = x > crest ? lerp(top, 60, Math.pow(sstep(crest, 350, x), 0.8)) : lerp(-12, top, Math.pow(sstep(WESTSEA(y), crest, x), 0.55));
		h += 18 * ridged(x / 45, y / 45) * sstep(80, 200, h);
	}
	// Saltgate's valley and pass (the 60 m saddle at the crest, down east to the inlet).
	const sgy = SG_Y(x), w = SG_W(x) / 2, dy = Math.abs(y - sgy);
	if (x < 980) {
		const floor = SG_FLOOR(x) + 0.6 * fbm(x / 25, y / 25);
		const wall = floor + Math.max(0, dy - w) * (x < 400 ? 1.4 : 0.35);
		h = smin(h, wall, 6);
	}
	// The Gate Inlet: tidal water and its salt marsh.
	const di = distPoly(x, y, INLET);
	if (di < 120 && x > 900) {
		const bed = di < 45 ? -1.6 + 0.8 * (di / 45) : lerp(-0.8, 0.9, sstep(45, 75, di));
		h = di < 75 ? Math.min(h, bed) : lerp(Math.min(h, 1.0 + 0.3 * fbm(x / 20, y / 20)), h, sstep(75, 120, di));
	}
	// The Pound: a lagoon dug into the terraces, walled (the wall is scenery): the flat at -0.1 m, the pool 8 m deep.
	const rp = Math.hypot(x - POUND.x, y - POUND.y);
	if (rp < POUND.wall + 18) {
		const flat = -0.1 + 0.15 * fbm(x / 15, y / 15);
		const pool = lerp(-8, flat, sstep(POUND.pool - 12, POUND.pool + 6, rp));
		h = rp < POUND.wall ? pool : lerp(pool, h, sstep(POUND.wall, POUND.wall + 18, rp));
	}
	// The Sluiceway: 8 m wide, 6 m deep at mean, its banks cut steep.
	const ds = distPoly(x, y, SLUICE);
	if (ds < 18 && x > POUND.x + 110) h = Math.min(h, ds < 4 ? -5.6 : lerp(-5.6, h, sstep(4, 18, ds)));
	// The Blowhole Grotto's sea mouth and the shelf edge cave mouths are scenery; the ground stays whole over them.
	// The shores: north and south straits, the west sea.
	if (toEdge < 60) h = lerp(Math.min(h, -10), h, sstep(-10, 60, toEdge));
	return { h, sea: false, d };
}

const SURF = { zone: 0, sand: 1, wetsand: 2, shelf: 3, wetrock: 4, pool: 5, terrace: 6, salt: 7, shallows: 8, kelpfloor: 9, barren: 10, reef: 11, slope: 12, wall: 13, ooze: 14, mud: 15, marsh: 16, road: 17, granite: 18, oyster: 19, dune: 20, hidden: 255 };
const HF = new Float32Array(N * N), SURFM = new Uint8Array(N * N);
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const x = X0 + i * STEP, y = Y0 + j * STEP, k = j * N + i;
	if (y > -5640 || skyHas(x, y)) { HF[k] = -14; SURFM[k] = SURF.hidden; continue; }
	const g = ground(x, y);
	let h = g.h;
	// Inside the Storm crown's frame, its ground is the ground wherever it stands at or above ours (its north face).
	const sh = x < 1132 && y > -5936 ? stormH(x, y) : null;
	if (sh !== null && (sh >= h - 0.5 || y > -5700)) { HF[k] = h; SURFM[k] = SURF.hidden; continue; }
	HF[k] = h;
	let s = SURF.zone;
	if (g.sea) {
		if (h > -3) s = x > 2450 ? SURF.reef : SURF.shallows;
		else if (x < 2500) s = vnoise(x / 70, y / 70) > 0.74 ? SURF.barren : SURF.kelpfloor;
		else if (x < 2820) s = SURF.reef;
		else if (x < 3500) s = SURF.slope;
		else if (x < 3700) s = SURF.wall;
		else s = SURF.ooze;
		if (y > -5800 && x > 1140 && x < 2000) s = SURF.oyster;                                     // the estuary's brackish mud
	} else {
		const rp = Math.hypot(x - POUND.x, y - POUND.y);
		const shelf = y < Y(-6950) && x > 1100;
		if (rp < POUND.wall) s = SURF.mud;
		else if (h < SEA + 1.2 && h > SEA - 2) s = shelf ? SURF.wetrock : SURF.wetsand;           // the intertidal band
		else if (distPoly(x, y, INLET) < 110 && x > 900) s = SURF.marsh;
		else if (shelf) s = vnoise(x / 22, y / 22) > 0.78 ? SURF.pool : SURF.shelf;
		else if (x > 1300) s = g.d < 105 ? SURF.sand : SURF.dune;
		else if (x > 350) s = x > 700 && x < 1180 && y > Y(-6150) && y < Y(-5650) && Math.abs(h - Math.round(h)) < 2 ? SURF.salt : SURF.terrace;
		else s = SURF.granite;
		if (y > -5820 && x > 1140 && h < 2) s = SURF.oyster;
	}
	SURFM[k] = s;
}
const hAt = (x, y) => {
	const fi = (x - X0) / STEP, fj = (y - Y0) / STEP, i = clamp(Math.floor(fi), 0, N - 2), j = clamp(Math.floor(fj), 0, N - 2);
	const tx = clamp(fi - i, 0, 1), ty = clamp(fj - j, 0, 1);
	const a = HF[j * N + i], b = HF[j * N + i + 1], c = HF[(j + 1) * N + i], d = HF[(j + 1) * N + i + 1];
	return lerp(lerp(a, b, tx), lerp(c, d, tx), ty);
};
const isSea = (x, y) => hAt(x, y) < SEA - 0.3;
// Openings the caves need in the ground (build plan 2.5): the mine's adit, the Horn's vent, Lazarus Deep's mouth in the
// Blue Wall; the scenery's rock collars cover the holes' edges.
{
	const holes = [[336 - 1, Y(-7600), 4.5], [COAST(Y(-7700)) - 42, Y(-7700), 2.2], [3590 + 6, Y(-7500), 6]];
	for (const [hx, hy, hr] of holes) for (let j = 0; j < N; j++) { const y = Y0 + j * STEP; if (Math.abs(y - hy) > hr + STEP) continue; for (let i = 0; i < N; i++) { const x = X0 + i * STEP; if (Math.hypot(x - hx, y - hy) < hr + STEP * 0.5) SURFM[j * N + i] = SURF.hidden; } }
}
// The coast road, worn into the ground.
{
	for (let s = 0; s < ROAD.length - 1; s++) {
		const [ax, ay] = ROAD[s], [bx, by] = ROAD[s + 1], L = Math.hypot(bx - ax, by - ay);
		for (let t = 0; t <= L; t += 2) {
			const px = ax + (bx - ax) * t / L, py = ay + (by - ay) * t / L;
			for (let dj = -4; dj <= 4; dj += STEP) for (let di = -4; di <= 4; di += STEP) {
				const i = Math.round((px + di - X0) / STEP), j = Math.round((py + dj - Y0) / STEP);
				if (i < 0 || j < 0 || i >= N || j >= N) continue;
				const k = j * N + i;
				if (SURFM[k] !== SURF.hidden && HF[k] > SEA + 1) SURFM[k] = SURF.road;
			}
		}
	}
}

// ---------------------------------------------------------------- the places (cook 2.3, 2.4, 2.5, 2.7, 2.11)
const at = (x, yd, extra = {}) => ({ x, y: Y(yd), ...extra });
const place = (o) => { o.z = o.z ?? r1(hAt(o.x, o.y)); o.x = r1(o.x); o.y = r1(o.y); return o; };
const landmarks = [
	{ id: 'pound', kind: 'pound', name: 'the Pound', ...at(1000, -7300), yaw: 0, size: 240 },
	{ id: 'colossus', kind: 'colossus', name: 'the Drowned Colossus', ...at(1000, -6760), yaw: 254, size: 60 },
	{ id: 'beacon_stack', kind: 'beacon_stack', name: 'the Beacon Stack', ...at(1780, -7250), yaw: 90 },
	{ id: 'spouting_horn', kind: 'spouting_horn', name: 'the Spouting Horn', x: COAST(Y(-7700)) - 42, y: Y(-7700), yaw: 90 },
	{ id: 'dyke_head', kind: 'dyke_head', name: 'the Dyke Head', ...at(1840, -6300), yaw: 90 },
	{ id: 'reef_lighthouse', kind: 'reef_lighthouse', name: 'the Reef Lighthouse', ...at(2700, -7200), yaw: 95 },
	{ id: 'bell_tower', kind: 'bell_tower', name: 'the Bell Tower', ...at(2610, -6450), yaw: 0 },
	{ id: 'long_dyke', kind: 'long_dyke', name: 'the Long Dyke', x: DYKE[0][0], y: DYKE[0][1], yaw: 0 },
].map(place);
// The Beacon Stack stands 120 m off the shelf in the sea: its foot is the sea floor there.
const stack = landmarks.find(l => l.id === 'beacon_stack'); stack.x = r1(COAST(stack.y) + 120); stack.z = r1(hAt(stack.x, stack.y));
landmarks.find(l => l.id === 'bell_tower').z = r1(hAt(2610, Y(-6450)));

const sites = [
	// --- land (cook 2.4, 18)
	{ id: 'tsunami_line', kind: 'tsunami_line', name: 'the Tsunami Line', zone: 'O3', ...at(1200, -6700) },
	{ id: 'upper_town', kind: 'upper_town', name: 'the Upper Town', zone: 'O3', ...at(900, -6200), yaw: 90 },
	{ id: 'salt_pans', kind: 'salt_pans', name: 'the Salt Pans', zone: 'O3', ...at(850, -5900), yaw: 90 },
	{ id: 'harpooners_hall', kind: 'harpooners_hall', name: "Harpooners' Hall", zone: 'O3', ...at(780, -7020), yaw: 0 },
	{ id: 'chain_anchors', kind: 'chain_anchors', name: 'the Chain Anchors', zone: 'O3', ...at(1000, -7300) },
	{ id: 'sunwatch_mirrors', kind: 'sunwatch_mirrors', name: 'the Sunwatch Mirrors', zone: 'O3', ...at(880, -6860), yaw: 0 },
	{ id: 'sluiceway_bridge', kind: 'sluiceway_bridge', name: 'the Sluiceway Bridge', zone: 'O2', ...at(1380, -7260), yaw: 0 },
	{ id: 'barnacle_cove', kind: 'barnacle_cove', name: 'Barnacle Cove', zone: 'O2', ...at(1720, -7640), yaw: 90 },
	{ id: 'ladle_rack', kind: 'ladle_rack', name: 'the Ladle Rack', zone: 'O2', ...at(1760, -7380), yaw: 90 },
	{ id: 'gullery', kind: 'gullery', name: 'the Gullery', zone: 'O2', ...at(1500, -7880), yaw: 0 },
	{ id: 'tide_clock', kind: 'tide_clock', name: 'the Tide Clock', zone: 'O2', ...at(1640, -7100), yaw: 90 },
	{ id: 'turtle_beach', kind: 'turtle_beach', name: 'the Turtle Beach', zone: 'O1', ...at(1800, -6100), yaw: 90 },
	{ id: 'rip_beach', kind: 'rip_beach', name: 'the Rip Beach', zone: 'O1', ...at(1820, -5720), yaw: 90 },
	{ id: 'driftwood_chapel', kind: 'driftwood_chapel', name: 'the Driftwood Chapel', zone: 'O1', ...at(1500, -6300), yaw: 90 },
	{ id: 'kelp_pits', kind: 'kelp_pits', name: 'the Kelp Pits', zone: 'O1', ...at(1450, -5900), yaw: 90 },
	{ id: 'shipwrights_slip', kind: 'shipwrights_slip', name: "the Shipwright's Slip", zone: 'O1', ...at(1660, -6560), yaw: 0 },
	{ id: 'whalebone_arch', kind: 'whalebone_arch', name: 'the Whalebone Arch', zone: 'O1', ...at(1250, -6000), yaw: 90 },
	{ id: 'mangrove_mouth', kind: 'mangrove_mouth', name: 'Mangrove Mouth', zone: 'O1', ...at(1600, -5460), yaw: 90 },
	{ id: 'gate_of_shells', kind: 'gate_of_shells', name: 'the Gate of Shells', zone: 'O3', ...at(1000, -6620), yaw: 270 },
	{ id: 'village', kind: 'village', name: 'Saltkin Steps', zone: 'O3', ...at(830, -6330), yaw: 90 },
	// --- sea (cook 2.4, 9)
	{ id: 'sunken_market', kind: 'sunken_market', name: 'the Sunken Market', zone: 'O5', ...at(2520, -6520), yaw: 90 },
	{ id: 'kelp_cathedral', kind: 'kelp_cathedral', name: 'the Kelp Cathedral', zone: 'O4', ...at(2200, -7000) },
	{ id: 'otter_barren', kind: 'otter_barren', name: 'the Otter Barren', zone: 'O4', ...at(2100, -5950) },
	{ id: 'mooring_stones', kind: 'mooring_stones', name: 'the Mooring Stones', zone: 'O4', ...at(2000, -6900) },
	{ id: 'impaled_ship', kind: 'impaled_ship', name: 'the Impaled Ship', zone: 'O5', ...at(2800, -6000), yaw: 70 },
	{ id: 'sunwatch_barque', kind: 'sunwatch_barque', name: 'the Sunwatch Barque', zone: 'O5', ...at(2950, -7420), yaw: 140 },
	{ id: 'clam_garden', kind: 'clam_garden', name: 'the Clam Garden', zone: 'O5', ...at(2420, -7560) },
	{ id: 'whale_fall', kind: 'whale_fall', name: 'the Whale Fall', zone: 'O5', ...at(3620, -6880), yaw: 20 },
	{ id: 'colossus_hand', kind: 'colossus_hand', name: 'the Coral Colossus Hand', zone: 'O5', ...at(2700, -6650), yaw: 200 },
	// --- shrines (cook 2.11; the shrine rows in world.json stand on these)
	{ id: 'shrine_mirror_pool', kind: 'shrine_mirror_pool', name: 'the Mirror Pool', zone: 'O2', ...at(1620, -7060) },
	{ id: 'shrine_driftwood', kind: 'shrine_driftwood', name: 'the Driftwood Shrine', zone: 'O1', ...at(1540, -6310) },
	{ id: 'shrine_gate', kind: 'shrine_gate', name: 'the Gate Shrine', zone: 'O3', ...at(960, -6620) },
	{ id: 'shrine_kelp_stack', kind: 'shrine_kelp_stack', name: 'the Kelp Stack Shrine', zone: 'O4', ...at(2150, -6750) },
	{ id: 'shrine_lighthouse', kind: 'shrine_lighthouse', name: 'the Lighthouse Shrine', zone: 'O5', ...at(2700, -7200) },
	// --- Hae-won's stilt house over the Mirror Pool; the Pound's run-back stone; Siren's Hollow's shore mouth
	{ id: 'haewon_house', kind: 'haewon_house', name: "Hae-won's house", zone: 'O2', ...at(1600, -7030), yaw: 90 },
	{ id: 'pound_runback', kind: 'runback', name: "the Pound's run-back stone", zone: 'O3', x: POUND.x, y: POUND.y - POUND.wall - 50 },
	{ id: 'sirens_hollow_mouth', kind: 'cave_mouth', name: "Siren's Hollow", zone: 'O2', ...at(1930, -7700), yaw: 270 },
	// --- camps (KEPT coordinates): a driftwood windbreak, a fire ring, the newcomers' wreck
	{ id: 'camp_strand', kind: 'camp', name: 'the Strand camp', zone: 'O1', camp: true, ...at(1650, -5800), yaw: 90 },
	{ id: 'camp_inlet', kind: 'camp', name: 'the Inlet camp', zone: 'O1', camp: true, ...at(1700, -6650), yaw: 90 },
	{ id: 'camp_shelf', kind: 'camp', name: 'the Shelf camp', zone: 'O2', camp: true, ...at(1550, -7500), yaw: 90 },
].map(place);
// Camps sit on dunes / the shelf top above the Tsunami Line: lift them to the nearest ground at 6 m+ landward.
for (const s of sites.filter(s => s.camp)) {
	// The nearest flat dune or shelf-top ground at 6.5-10 m (rings outward), away from the inlet and the Pound.
	let best = null;
	for (let r = 0; r < 400 && !best; r += 5) for (let a = 0; a < 360 && !best; a += 10) {
		const x = s.x + Math.cos(a * Math.PI / 180) * r, y = s.y + Math.sin(a * Math.PI / 180) * r, h = hAt(x, y);
		const flat = Math.abs(hAt(x + 6, y) - hAt(x - 6, y)) + Math.abs(hAt(x, y + 6) - hAt(x, y - 6)) < 1.6;
		if (h >= 6.5 && h <= 10.5 && flat && distPoly(x, y, INLET) > 130 && Math.hypot(x - POUND.x, y - POUND.y) > POUND.wall + 60) best = [x, y];
	}
	if (best) [s.x, s.y] = [r1(best[0]), r1(best[1])];
	s.z = r1(hAt(s.x, s.y));
}
for (const id of ['sunken_market', 'kelp_cathedral', 'otter_barren', 'mooring_stones', 'impaled_ship', 'sunwatch_barque', 'clam_garden', 'whale_fall', 'colossus_hand', 'shrine_kelp_stack', 'shrine_lighthouse']) {
	const s = sites.find(q => q.id === id); s.sea = true; s.z = r1(hAt(s.x, s.y));
}
// The Kelp Stack shrine stands on a 14 m sea stack you climb out of the water to; the lighthouse's on the crest.
sites.find(s => s.id === 'shrine_kelp_stack').stack = 14;

// The Tsunami Line: nine stones on the 6 m contour from the cook's (1150, -5600) to (1250, -7800).
const TSUNAMI = 6.0;
const stones = [];
for (let k = 0; k < 9; k++) {
	const yd = lerp(-5600, -7800, k / 8), y = Y(yd);
	let x = lerp(1150, 1250, k / 8);
	// Walk along x to where the ground crosses the contour (seaward of the terraces: the dune / shelf rise).
	// The easternmost place the ground falls through the contour and stays under it seaward for 40 m (the last rise
	// before the sea: the dune crest's back, the shelf's riser).
	let best = x;
	for (let xx = 700; xx < 1900; xx += 1) {
		if (hAt(xx, y) >= TSUNAMI && hAt(xx + 1, y) < TSUNAMI) {
			let ok = true; for (let t = 2; t < 40; t += 2) if (hAt(xx + t, y) >= TSUNAMI) { ok = false; break; }
			if (ok) best = xx;
		}
	}
	stones.push([r1(best), r1(y), r1(hAt(best, y)), r1(rnd() * 20 - 10)]);
}

// ---------------------------------------------------------------- caves (cook 2.7) and the Drowned Hall (2.5)
// Each cave is rock round its rooms (build plan 2.5): the Saltkin Mine is an adit into the Spine's face (its rooms under the
// hill), the Blowhole Grotto lies under the shelf beneath the Spouting Horn with its sea mouth in the shelf edge, the Pearl
// Cave and Siren's Hollow are inside rock outcrops (a reef mound, a headland), Lazarus Deep goes into the Blue Wall.
const hornY = Y(-7700), hornX = COAST(hornY) - 42;
const caves = [
	{ id: 'blowhole_grotto', name: 'the Blowhole Grotto', zone: 'O2', x: hornX, y: hornY, floor: -5, roof: 5.2, rooms: [[0, 0, 14], [16, 8, 10], [30, 2, 8]], mouth: { dx: 40, dy: 2, yaw: 90, w: 8, h: 5 }, ore: ['copper_ore', 'tin_ore'], creatures: { tidecoil: 2, spinecrab: 3 }, kind: 'under' },
	{ id: 'saltkin_mine', name: 'the Saltkin Mine', zone: 'O3', x: 336, y: Y(-7600), floor: 60, roof: 6, rooms: [[-30, 0, 9], [-70, 6, 10], [-110, -4, 11], [-140, 14, 12]], mouth: { dx: 0, dy: 0, yaw: 90, w: 5, h: 5 }, ore: ['copper_ore', 'iron_ore', 'iron_ore', 'iron_ore'], creatures: { spinecrab: 3 }, dry: true, kind: 'adit' },
	{ id: 'pearl_cave', name: 'the Pearl Cave', zone: 'O4', x: 2050, y: Y(-6200), floor: -14, roof: 8, rooms: [[0, 0, 12], [16, 10, 8], [26, 22, 6]], mouth: { dx: -16, dy: 0, yaw: 270, w: 7, h: 6 }, ore: ['tin_ore', 'gem_topaz'], creatures: { pearldiver: 4, tidecoil: 2 }, airPocket: [0, 0, -9, 10], kind: 'mound' },
	{ id: 'sirens_hollow', name: "Siren's Hollow", zone: 'O2', x: 1950, y: Y(-7700), floor: -20, roof: 36, rooms: [[0, 0, 30]], mouth: { dx: -34, dy: 0, yaw: 270, w: 12, h: 7 }, sea: { dx: 34, dy: 0, z: -10, w: 10, h: 7 }, ore: ['iron_ore', 'iron_ore'], creatures: {}, arena: 'siren_eel', ledge: 4, islands: [[-14, -8, 4], [12, -14, 3.5], [18, 10, 4], [-6, 16, 3], [0, 0, 5]], kind: 'mound' },
	{ id: 'lazarus_deep', name: 'Lazarus Deep', zone: 'O5', x: 3590, y: Y(-7500), floor: -60, roof: 8, rooms: [[-20, 0, 12], [-46, 6, 10], [-70, 0, 12]], mouth: { dx: 0, dy: 0, yaw: 90, w: 9, h: 7 }, ore: ['iron_ore', 'iron_ore', 'iron_ore', 'iron_ore', 'gem_topaz', 'gem_topaz'], creatures: { reefjaw: 2, tidecoil: 4 }, legendary: 'fish_lazarus_fish', side: [-36, 14, -40], kind: 'wall' },
].map(c => { c.x = r1(c.x); c.y = r1(c.y); c.z = r1(c.floor ?? hAt(c.x, c.y)); return c; });

const hallC = { x: 2600, y: Y(-6500) };
const hall = {
	at: [hallC.x, r1(hallC.y)], top: -6, deep: -22,
	rooms: [
		{ id: 'bell_chamber', name: 'the Bell Chamber', dx: 10, dy: 50, z: -4, r: 5, air: true },
		{ id: 'air_dome', name: 'the Air Dome', dx: 0, dy: 0, z: -14, r: 20, air: 'dome', airTop: -6, waterTop: -13 },
		{ id: 'kings_tower', name: "the King's Tower", dx: -26, dy: -12, z: -8, r: 6, air: true },
		{ id: 'sluice_works', name: 'the Sluice Works', dx: 34, dy: -22, z: -18, r: 10 },
		{ id: 'pearl_beds', name: 'the Pearl Beds', dx: -30, dy: 24, z: -16, r: 11 },
		{ id: 'tidecoil_warren', name: 'the Tidecoil Warren', dx: 38, dy: 28, z: -17, r: 12 },
		{ id: 'archive', name: 'the Archive', dx: -8, dy: -22, z: -7, r: 6, air: true },
		{ id: 'low_gate_hall', name: 'the Low Gate Hall', dx: -60, dy: 0, z: -9, r: 8 },
	],
	ways: [
		{ id: 'bell_dive', kind: 'stair', from: [2610, r1(Y(-6450)), 0.6], to: 'bell_chamber' },
		{ id: 'low_gate', kind: 'gate', at: [2520 + 44, r1(Y(-6520)), -3], to: 'low_gate_hall', kingTide: true },
		{ id: 'pearl_culvert', kind: 'culvert', from: 'pearl_cave', to: 'pearl_beds', length: 40, pearldiver: 20 },
		{ id: 'sluice_door', kind: 'oneway', from: 'sluice_works', to: [r1(DYKE[6][0]), r1(DYKE[6][1]), -1] },
	],
};

// ---------------------------------------------------------------- kit POIs (cook 2.4 table): Poisson >= 150 m per sub-zone
const zoneOf = (x, y) => {
	if (x > 2500) return 'O5';
	if (x > COAST(y)) return 'O4';
	if (x < 1300 && !(y < Y(-6950) && x > 1100)) return 'O3';
	return y < Y(-6950) ? 'O2' : 'O1';
};
const pois = [...landmarks.map(l => [l.x, l.y]), ...sites.map(s => [s.x, s.y]), ...caves.map(c => [c.x, c.y]), ...DYKE];
const near = (x, y, r) => pois.some(([px, py]) => (px - x) ** 2 + (py - y) ** 2 < r * r);
const KIT = [
	// kind, zones, variants, count
	['chime_beacon', ['O1', 'O2', 'O3'], 3, 9, 'road'],
	['castaway_hull', ['O1', 'O2'], 3, 4, 'beach'],
	['tidepool_cluster', ['O2'], 3, 4],
	['blowhole_field', ['O2'], 2, 3],
	['fish_weir', ['O1', 'O2'], 2, 3, 'beach'],
	['saltpan_terrace', ['O3'], 2, 3],
	['tsunami_outlier', ['O3', 'O1'], 2, 2, 'below'],
	['kill_scene', ['O1', 'O2'], 2, 3],
	['driftwood_shelter', ['O1'], 2, 2],
	['coral_head', ['O4', 'O5'], 3, 5],
	['drowned_house', ['O4'], 3, 3],
	['manowar_fleet', ['O5'], 1, 2],
	['anchor_chain', ['O4', 'O5'], 2, 3],
	['sea_stack', ['O4'], 3, 3],
	['cleaning_station', ['O5'], 2, 2],
];
const kit = [];
const candidate = (kind, zones, rule) => {
	for (let tries = 0; tries < 4000; tries++) {
		let x, y;
		if (rule === 'road') { const s = Math.floor(rnd() * (ROAD.length - 1)), t = rnd(); x = lerp(ROAD[s][0], ROAD[s + 1][0], t) + (rnd() - 0.5) * 10; y = lerp(ROAD[s][1], ROAD[s + 1][1], t); }
		else { x = lerp(-50, 3650, rnd()); y = lerp(-7900, -5780, rnd()); }
		const z = zoneOf(x, y), h = hAt(x, y);
		if (!zones.includes(z)) continue;
		if (rule === 'beach' && !(h > 0.8 && h < 3.5)) continue;
		if (rule === 'below' && !(h > 2 && h < TSUNAMI - 0.8)) continue;
		if (['O4', 'O5'].includes(z) ? h > -2 : h < 1.2) continue;
		if (z === 'O3' && h > 70) continue;
		if (near(x, y, 150)) continue;
		return { x, y };
	}
	return null;
};
// The shell-chime beacons stand every 120-200 m along the coast road (the Saltkin waymark), leaning a little seaward.
{
	let carry = 60;
	for (let s = 0; s < ROAD.length - 1; s++) {
		const L = Math.hypot(ROAD[s + 1][0] - ROAD[s][0], ROAD[s + 1][1] - ROAD[s][1]);
		for (let t = carry; t < L; t += 150 + rnd() * 50) {
			const x = lerp(ROAD[s][0], ROAD[s + 1][0], t / L) + 6, y = lerp(ROAD[s][1], ROAD[s + 1][1], t / L);
			carry = 0;
			if (sites.some(q => Math.hypot(q.x - x, q.y - y) < 40)) continue;
			const o = { kind: 'chime_beacon', variant: Math.floor(rnd() * 3), x: r1(x), y: r1(y), z: r1(hAt(x, y)), yaw: 90, lean: 4, zone: zoneOf(x, y), route: 'coastRoad' };
			kit.push(o); pois.push([o.x, o.y]);
		}
	}
}
for (const [kind, zones, variants, count, rule] of KIT.filter(k => k[0] !== 'chime_beacon')) {
	for (let c = 0; c < count; c++) {
		const p = candidate(kind, zones, rule);
		if (!p) { console.error(`kit ${kind}: no room for ${c + 1}`); continue; }
		const o = { kind, variant: Math.floor(rnd() * variants), x: r1(p.x), y: r1(p.y), z: r1(hAt(p.x, p.y)), yaw: Math.round(rnd() * 360), zone: zoneOf(p.x, p.y) };
		if (kind === 'chime_beacon') { const d = COAST(p.y) - p.x; o.yaw = 90; o.lean = 4; }
		kit.push(o); pois.push([o.x, o.y]);
	}
}
// The route rule: along the coast road, the dyke top and the three dive lines, any point > 125 m from every POI gets one.
const ROUTES = {
	coastRoad: ROAD,
	dykeTop: DYKE,
	diveCathedral: [[1700, Y(-6650)], [2200, Y(-7000)]],
	diveClams: [[1550, Y(-7500)], [2420, Y(-7560)]],
	diveWhaleFall: [[2700, Y(-7200)], [3620, Y(-6880)]],
};
for (const [name, pts] of Object.entries(ROUTES)) {
	for (let s = 0; s < pts.length - 1; s++) {
		const L = Math.hypot(pts[s + 1][0] - pts[s][0], pts[s + 1][1] - pts[s][1]);
		for (let t = 0; t <= L; t += 20) {
			const x = lerp(pts[s][0], pts[s + 1][0], t / L), y = lerp(pts[s][1], pts[s + 1][1], t / L);
			if (near(x, y, 125)) continue;
			const z = zoneOf(x, y), h = hAt(x, y);
			const kind = ['O4', 'O5'].includes(z) || h < 0 ? (z === 'O5' ? 'coral_head' : 'anchor_chain') : (name === 'coastRoad' ? 'chime_beacon' : 'castaway_hull');
			const o = { kind, variant: Math.floor(rnd() * 2), x: r1(x), y: r1(y), z: r1(h), yaw: name === 'coastRoad' ? 90 : Math.round(rnd() * 360), zone: z, route: name };
			if (kind === 'chime_beacon') o.lean = 4;
			kit.push(o); pois.push([x, y]);
		}
	}
}

// The density rule (world-map: a POI every 150-250 m): anywhere in the five sub-zones still more than 200 m from every
// place gets the kit piece that fits its ground.
{
	const fillKind = (z, h) => z === 'O5' ? (h < -30 ? 'anchor_chain' : 'coral_head') : z === 'O4' ? (rnd() < 0.5 ? 'drowned_house' : 'anchor_chain')
		: z === 'O3' ? (h > TSUNAMI ? 'saltpan_terrace' : 'tsunami_outlier') : z === 'O2' ? (rnd() < 0.5 ? 'tidepool_cluster' : 'castaway_hull') : (h < 3 ? 'fish_weir' : 'driftwood_shelter');
	for (let pass = 0; pass < 3; pass++) for (let y = -7900; y < -5800; y += 30) for (let x = -100; x < 3650; x += 30) {
		const h = hAt(x, y), z = zoneOf(x, y);
		if (x < 350 && h > 70) continue;                                 // the Spine's high ground: the Desert's later, no POIs
		if (h < -60 || (x < 1900 && h < SEA - 0.3 && z !== 'O4')) continue;
		if (near(x, y, 180)) continue;
		const kind = fillKind(z, h);
		const o = { kind, variant: Math.floor(rnd() * 2), x: r1(x), y: r1(y), z: r1(h), yaw: Math.round(rnd() * 360), zone: z, fill: true };
		kit.push(o); pois.push([x, y]);
	}
}

// ---------------------------------------------------------------- decoration (build plan 2.2's rule): instances by kind
const decor = { grass: [], driftwood: [], tamarisk: [], wall: [], kelp: [], coral: [], clam: [], rock: [], barnacle: [], urchin: [], mangrove: [] };
const nearSite = (x, y, r) => sites.some(s => (s.x - x) ** 2 + (s.y - y) ** 2 < r * r) || landmarks.some(s => (s.x - x) ** 2 + (s.y - y) ** 2 < r * r);
for (let j = 0; j < N; j += 1) for (let i = 0; i < N; i += 1) {
	const k = j * N + i; if (SURFM[k] === SURF.hidden) continue;
	const x = X0 + i * STEP + (rnd() - 0.5) * STEP, y = Y0 + j * STEP + (rnd() - 0.5) * STEP, h = hAt(x, y), s = SURFM[k];
	if (nearSite(x, y, 12)) continue;
	const r = rnd();
	if (s === SURF.dune && r < 0.22) decor.grass.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.7 + rnd() * 0.6)]);
	else if ((s === SURF.sand || s === SURF.wetsand) && h > SEA + 1.0 && h < SEA + 1.9 && r < 0.05) decor.driftwood.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.6 + rnd() * 0.9)]);
	else if (s === SURF.terrace && r < 0.025) decor.tamarisk.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.7 + rnd() * 0.5)]);
	else if (s === SURF.granite && r < 0.012) decor.rock.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(1 + rnd() * 2.5)]);
	else if ((s === SURF.wetrock || s === SURF.shelf) && h < SEA + 1.2 && r < 0.03) decor.barnacle.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.6 + rnd())]);
	else if (s === SURF.kelpfloor && h < -3 && h > -25 && r < 0.035 && vnoise(x / 50, y / 50) > 0.35) decor.kelp.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(Math.min(30, -h + SEA + 1.5))]);
	else if (s === SURF.barren && r < 0.05) decor.urchin.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), 1]);
	else if ((s === SURF.reef || s === SURF.shallows) && x > 2300 && h < -1 && r < 0.008) decor.clam.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(1 + rnd())]);
	else if ((s === SURF.reef || s === SURF.slope) && r < (s === SURF.reef ? 0.05 : 0.025)) decor.coral.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.6 + rnd() * 1.4), Math.floor(rnd() * 6)]);
	else if (s === SURF.oyster && r < 0.02 && y > -5790) decor.mangrove.push([r1(x), r1(y), r1(h), Math.round(rnd() * 360), r1(0.8 + rnd() * 0.6)]);
}
// Dry-stone walls along the terrace bench edges (the riser tops), in runs with gaps.
for (const [bx] of BENCH.filter((b, k) => k % 2 === 1)) {
	for (let y = -7880; y < -5850; y += 26) {
		if (rnd() < 0.3) continue;
		const x = bx - 4;
		if (Math.abs(y - SG_Y(x)) < 70 || Math.hypot(x - POUND.x, y - POUND.y) < POUND.wall + 30) continue;
		decor.wall.push([r1(x), r1(y), r1(hAt(x, y)), r1(x), r1(y + 22), r1(hAt(x, y + 22))]);
	}
}

// ---------------------------------------------------------------- resources (cook 2.7): nodes the server spawns
const resources = [];
const node = (item, x, y, z, extra = {}) => resources.push({ item, x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), ...extra });
// Tidecoral: reef heads (O5), the Colossus Hand's 20, O4's deep edge; underwater only.
for (const c of decor.coral.filter((c, k) => k % 7 === 0).slice(0, 140)) node('tidecoral', c[0], c[1], c[2], { under: true });
{ const h = sites.find(s => s.id === 'colossus_hand'); for (let k = 0; k < 20; k++) { const a = rnd() * 6.283, r = 3 + rnd() * 9; node('tidecoral', h.x + Math.cos(a) * r, h.y + Math.sin(a) * r, null, { under: true }); } }
// Giant clams (S7's forty, the reef flats), oysters at Mangrove Mouth, sea-glass on the beaches, driftwood, urchins.
{ const g = sites.find(s => s.id === 'clam_garden'); for (let k = 0; k < 40; k++) { const a = rnd() * 6.283, r = 2 + rnd() * 22; node('giant_clam', g.x + Math.cos(a) * r, g.y + Math.sin(a) * r, null, { under: true, size: r1(1 + rnd()) }); } }
for (const c of decor.clam.slice(0, 40)) node('giant_clam', c[0], c[1], c[2], { under: true, size: c[4] });
{ const m = sites.find(s => s.id === 'mangrove_mouth'); for (let k = 0; k < 14; k++) node('oyster', m.x + (rnd() - 0.5) * 70, m.y + (rnd() - 0.5) * 40); }
for (let k = 0; k < 60; k++) { const y = lerp(-7850, -5800, rnd()), x = COAST(y) - 8 - rnd() * 60; if (hAt(x, y) > 0.8 && hAt(x, y) < 3) node('sea_glass', x, y); }
for (const d of decor.driftwood.filter((d, k) => k % 4 === 0).slice(0, 90)) node('driftwood', d[0], d[1], d[2]);
for (const u of decor.urchin.filter((u, k) => k % 3 === 0).slice(0, 40)) node('urchin', u[0], u[1], u[2], { under: true });
// Salt: the Salt Pans' sixteen (on the pans), and the kelp you pick by hand in O4; sea kelp and sea grapes wild on the
// reef flat and shelf (the crops: they give their seeds too).
{ const p = sites.find(s => s.id === 'salt_pans'); for (let k = 0; k < 16; k++) node('salt', p.x - 120 + (k % 4) * 70 + (rnd() - 0.5) * 20, p.y - 150 + Math.floor(k / 4) * 90 + (rnd() - 0.5) * 20); }
for (const kp of decor.kelp.filter((k2, k) => k % 9 === 0).slice(0, 80)) node('sea_kelp', kp[0], kp[1], kp[2], { under: true, wild: true });
for (let k = 0; k < 50; k++) { const x = lerp(2320, 2520, rnd()), y = lerp(-7800, -5900, rnd()), h = hAt(x, y); if (h < -1 && h > -6) node('sea_grapes', x, y, h, { under: true, wild: true }); }
{ const ca = sites.find(s => s.id === 'chain_anchors'); for (let k = 0; k < 4; k++) { const a = k / 4 * 6.283 + 0.4; node('bronze_scrap', ca.x + Math.cos(a) * 127, ca.y + Math.sin(a) * 127, null, { yield: 6 }); } }
{ const kp = sites.find(s => s.id === 'kelp_pits'); for (let k = 0; k < 6; k++) node('kelp_ash', kp.x + (k % 3 - 1) * 6, kp.y + (Math.floor(k / 3) - 0.5) * 7); }
{ const wf = sites.find(s => s.id === 'whale_fall'); for (let k = 0; k < 4; k++) node('bone', wf.x + (k - 1.5) * 4, wf.y + (k % 2) * 3, null, { under: true }); }
{ const w = hall.rooms.find(r => r.id === 'tidecoil_warren'); for (let k = 0; k < 3; k++) node('iron_ore', hallC.x + w.dx + (k - 1) * 4, hallC.y + w.dy + 5, w.z + 0.2, { hall: true }); }
{ const p = hall.rooms.find(r => r.id === 'pearl_beds'); for (let k = 0; k < 8; k++) { const a = k / 8 * 6.283; node('giant_clam', hallC.x + p.dx + Math.cos(a) * 7, hallC.y + p.dy + Math.sin(a) * 7, p.z + 0.1, { under: true, hall: true, size: 1.6 }); } }
// Ores in the caves (cook 2.7) and nails in the wrecks.
for (const c of caves) c.ore.forEach((ore, k) => { const room = c.rooms[k % c.rooms.length]; node(ore, c.x + room[0] + (rnd() - 0.5) * room[2], c.y + room[1] + (rnd() - 0.5) * room[2], c.z + 0.2, { cave: c.id }); });

// ---------------------------------------------------------------- what the places pay (cook 2.4, 2.5, 2.20): spots you hold E
// at and caches you open. Spot kinds: take (once per survivor), stone (a tsunami stone to read), anchor (the Kelp
// Cathedral's anchor stone, Hae-won's first), bell (the drowned bell's rope), key (the Sluice key), beacon (the Beacon
// Stack), nest (eggs, every day), mirror (the Sunwatch tripods' line).
const spots = [], caches = [];
const S = id => sites.find(q => q.id === id);
const spot = (kind, id, x, y, z, title, verb, seconds, gives = {}, lore = '', respawnHours = 0) => spots.push({ kind, id, x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), title, verb, seconds, gives, lore, respawnHours });
const cache = (x, y, z, yaw, title, items, lore, refillHours) => caches.push({ x: r1(x), y: r1(y), z: r1(z ?? hAt(x, y)), yaw, title, items, lore, refillHours });
{
	const mid = stones[4];
	spot('take', 'tide_stone_rubbing', mid[0] - 2, mid[1], mid[2], 'A tsunami stone', 'Take a rubbing', 3, { lore_tide_stone_rubbing: 1 });
	stones.forEach((st, k) => spot('stone', `tsunami_stone_${k}`, st[0], st[1], st[2], `Tsunami stone ${k + 1} of 9`, 'Read the stone', 1.5));
	const ut = S('upper_town'); cache(ut.x + 6, ut.y + 4, null, 90, 'A Saltkin larder', { salt: 6, rope: 4, fishing_rod: 1 }, 'Salt jars and nets, a meal still on the table.', 72);
	const hh = S('harpooners_hall'); spot('take', 'harpooners_tally', hh.x + 4, hh.y, hh.z, "Harpooners' Hall", 'Take a worn harpoon from the rack', 3, { worn_harpoon: 1, lore_harpooners_tally_cord: 1, bp_whalers_harpoon: 1 });
	const sm = S('sunwatch_mirrors'); spot('take', 'sunwatch_mirror_shard', sm.x, sm.y + 3, sm.z, 'The Sunwatch Mirrors', 'Take the cracked mirror', 3, { lore_sunwatch_mirror_shard: 1, bronze_sheet: 1 });
	['a', 'b', 'c'].forEach((t, k) => spot('mirror', `mirror_tripod_${t}`, sm.x + (k - 1) * 9, sm.y - 6 + (k % 2) * 4, null, 'A Sunwatch tripod mirror', 'Turn the mirror', 2));
	const sb = S('sluiceway_bridge'); cache(sb.x + 3, sb.y + 5, null, 0, 'The fallen span', { iron_nails: 4 }, 'Iron nails, bronze-headed, in a split beam.', 0);
	const lr = S('ladle_rack'); spot('take', 'bottomless_ladle', lr.x, lr.y, lr.z, 'The Ladle Rack', 'Take down a ladle', 2, { lore_bottomless_ladle: 1, bronze_ingot: 2 });
	const gl = S('gullery'); spot('nest', 'gullery_eggs', gl.x + 8, gl.y + 6, null, 'Tidefowl burrows', 'Take eggs', 2, { tidefowl_egg: 2 }, '', 24);
	cache(gl.x - 10, gl.y + 4, null, 0, "A Gullraider's hoard", { sea_glass: 4, copper_ingot: 1, gem_topaz: 1 }, 'Rings, a compass, a child\'s carved boat: things taken from bags.', 96);
	const dc = S('driftwood_chapel'); spot('take', 'votive_boat', dc.x, dc.y, dc.z, 'The Driftwood Chapel', 'Take a votive boat', 2, { lore_votive_boat: 1, sea_glass: 6 });
	const kp = S('kelp_pits'); spot('take', 'glassmakers_scrap', kp.x + 5, kp.y, kp.z, 'The Kelp Pits', 'Take the scrap from the rake', 2, { bp_kelp_glass: 1 });
	const ss = S('shipwrights_slip'); spot('take', 'fittings_scrap', ss.x, ss.y + 4, ss.z, "The Shipwright's Slip", 'Take the fittings sketch', 2, { bp_tidecoral_fittings: 1, tidecoral: 3 });
	const sk = S('sunken_market'); cache(sk.x, sk.y, null, 0, 'A market amphora', { sea_glass: 3, copper_ore: 3, clay: 4 }, 'Amphorae in rows, sealed with wax and a fish stamp.', 72);
	const kc = S('kelp_cathedral'); spot('anchor', 'cathedral_anchor', kc.x, kc.y, kc.z + 0.8, 'The Kelp Cathedral\'s anchor stone', 'Touch the anchor stone', 1);
	const is = S('impaled_ship'); cache(is.x - 4, is.y + 2, is.z + 3.5, 70, "The captain's chest", { iron_ore: 6, rope: 3, linen: 3, lore_ships_log: 1 }, 'The log\'s last page is water-stained to nothing but one line.', 0);
	const sbq = S('sunwatch_barque'); cache(sbq.x, sbq.y, null, 140, 'A cedar chest under the prow', { bronze_ingot: 3, sunstone: 1, gold_leaf: 2 }, 'Sun-disc stamps on the lid, coral grown over the hasp.', 0);
	const wf = S('whale_fall'); cache(wf.x + 6, wf.y - 3, null, 20, "A diver's sealed chest", { steel_plate: 2, gem_ruby: 1, upgrade_shard: 1 }, 'A newcomer diver\'s helmet lies beside it, the glass cracked.', 0);
	const hallSpot = (kind, id, room, title, verb, secs, gives) => { const r = hall.rooms.find(q => q.id === room); spot(kind, id, hallC.x + r.dx, hallC.y + r.dy, r.z, title, verb, secs, gives); };
	hallSpot('bell', 'drowned_bell', 'bell_chamber', 'The drowned bell', 'Pull the bell rope', 1.5, {});
	hallSpot('take', 'broken_key_chain', 'kings_tower', "The king's bed", 'Take the broken chain', 2, { lore_broken_key_chain: 1 });
	hallSpot('key', 'sluice_key', 'sluice_works', 'The sluice lever', 'Take the key from the lock', 3, { sluice_key: 1 });
	hallSpot('take', 'tablet_of_the_pound', 'archive', 'The Archive', 'Take the tablet', 2, { lore_tablet_of_the_pound: 1, bp_sun_mirror: 1 });
	const bs = landmarks.find(l => l.id === 'beacon_stack'); spot('beacon', 'beacon_stack', bs.x, bs.y, bs.z + 70, 'The Beacon Stack', 'Light the beacon (4 wood)', 3);
}

// ---------------------------------------------------------------- hazards (cook 2.8)
const rips = RIPS.map(ry => ({ x: r1(COAST(ry) - 20), y: r1(ry), dir: 0, length: 120, width: 8 }));
const blowholes = [];
for (const f of kit.filter(k => k.kind === 'blowhole_field')) for (let v = 0; v < 3 + f.variant * 2; v++) { const a = rnd() * 6.283, r = 6 + rnd() * 18; blowholes.push({ x: r1(f.x + Math.cos(a) * r), y: r1(f.y + Math.sin(a) * r), z: r1(hAt(f.x + Math.cos(a) * r, f.y + Math.sin(a) * r)), horn: false }); }
{ const h = landmarks.find(l => l.id === 'spouting_horn'); blowholes.push({ x: h.x, y: h.y, z: h.z, horn: true, ledge: [r1(h.x - 14), r1(h.y + 6), r1(h.z + 18)] }); }
const surges = [];
for (let k = 0; k < 6; k++) { const y = lerp(Y(-7850), Y(-7000), (k + 0.5) / 6), x = COAST(y); surges.push({ x: r1(x - 10), y: r1(y), z: r1(hAt(x - 10, y)), length: 60, yaw: 180 }); }
const fleets = kit.filter(k => k.kind === 'manowar_fleet').map(k => ({ x: k.x, y: k.y, count: 6 + Math.floor(rnd() * 14) }));
const airPockets = [
	...caves.filter(c => c.airPocket).map(c => ({ cave: c.id, x: r1(c.x + c.airPocket[0]), y: r1(c.y + c.airPocket[1]), z: c.airPocket[2], r: c.airPocket[3] })),
	...hall.rooms.filter(r => r.air).map(r => ({ hall: r.id, x: r1(hallC.x + r.dx), y: r1(hallC.y + r.dy), z: r.air === 'dome' ? r.waterTop : r.z, top: r.air === 'dome' ? r.airTop : r.z + 4, r: r.r })),
];

// ---------------------------------------------------------------- arenas, routes (cook 2.13, 2.14)
const TYRANT_ROUTE = [[2700, Y(-7200)], [3300, Y(-7100)], [3620, Y(-6880)], [3550, Y(-6300)], [3400, Y(-5950)], [2800, Y(-6100)], [2300, Y(-6500)], [2000, Y(-6900)], [2100, Y(-6400)], [2550, Y(-6350)], [2650, Y(-6800)], [2700, Y(-7200)]].map(([x, y]) => [r1(x), r1(y), r1(Math.max(hAt(x, y) + 3, Math.min(-2, hAt(x, y) * 0.5)))]);
const arenas = {
	thalassor: { at: [POUND.x, r1(POUND.y)], wall: POUND.wall, wallTop: 30, walk: 4, pool: POUND.pool, flat: -0.1, waterP1: 0.65, rise: 4,
		runback: [POUND.x, r1(POUND.y - POUND.wall - 50), r1(hAt(POUND.x, POUND.y - POUND.wall - 50))],
		altar: [POUND.x - 30, r1(POUND.y - POUND.wall - 60), r1(hAt(POUND.x - 30, POUND.y - POUND.wall - 60))],
		stairs: [0, 270, 180].map(a => a), mirrors: [0, 270, 180], sluices: [55, 75, 95, 115], burst: 0,
		towers: Array.from({ length: 8 }, (_, k) => { const a = (k + 0.5) / 8 * 6.283, r = 76; return [r1(POUND.x + Math.cos(a) * r), r1(POUND.y + Math.sin(a) * r)]; }),
		bollards: Array.from({ length: 4 }, (_, k) => { const a = k / 4 * 6.283 + 0.4, r = 104; return [r1(POUND.x + Math.cos(a) * r), r1(POUND.y + Math.sin(a) * r)]; }) },
	old_barnacle: { at: [r1(sites.find(s => s.id === 'barnacle_cove').x), r1(sites.find(s => s.id === 'barnacle_cove').y)], radius: 35,
		spares: [{ kind: 'bell', dx: -14, dy: 12 }, { kind: 'carapace', dx: 12, dy: -16 }], rocks: [[-26, -8], [22, 18], [-6, -28]] },
	siren_eel: { cave: 'sirens_hollow' },
	reef_tyrant: { route: TYRANT_ROUTE, rest: [r1(sites.find(s => s.id === 'mooring_stones').x), r1(sites.find(s => s.id === 'mooring_stones').y)], restHours: [13, 15] },
};
const mooringStones = Array.from({ length: 8 }, (_, k) => { const s = sites.find(q => q.id === 'mooring_stones'); const a = k / 8 * 6.283; return [r1(s.x + Math.cos(a) * 18), r1(s.y + Math.sin(a) * 18), r1(hAt(s.x + Math.cos(a) * 18, s.y + Math.sin(a) * 18))]; });

// ---------------------------------------------------------------- secrets (cook 2.20), portal slots (brief)
const secrets = [
	{ id: 'O-S1', kind: 'bell_window', at: [2610 + 2, r1(Y(-6450)), -2], when: 'midnight_high', pays: { pearl: 3, '@gem': 1 } },
	{ id: 'O-S2', kind: 'dyke_vault', at: [r1(landmarks.find(l => l.id === 'dyke_head').x - 6), r1(landmarks.find(l => l.id === 'dyke_head').y), r1(landmarks.find(l => l.id === 'dyke_head').z)], needs: 'sluice_key', pays: { pearl_circlet: 1, gold_coin: 12, upgrade_shard: 2 } },
	{ id: 'O-S3', kind: 'horn_ledge', at: blowholes.find(b => b.horn).ledge, pays: { iron_ore: 4, harpoon_head: 1 } },
	{ id: 'O-S4', kind: 'mirror_line', at: [760, r1(Y(-6980)), r1(hAt(760, Y(-6980)))], when: 'noon_clear', pays: { sunstone: 3, sun_disc_decor: 1, bp_sun_mirror: 1 } },
	{ id: 'O-S5', kind: 'lazarus_ledge', at: [r1(caves.find(c => c.id === 'lazarus_deep').x - 6), r1(caves.find(c => c.id === 'lazarus_deep').y + 14), -40], needs: 'pearldiver', pays: { diving_helm: 1, steel_plate: 4 } },
	{ id: 'O-S6', kind: 'low_gate', at: hall.ways.find(w => w.id === 'low_gate').at, when: 'king_tide' },
];
const portalSlots = [{ rank: '1', x: 1330, y: r1(Y(-6150)), z: r1(hAt(1330, Y(-6150))) }, { rank: 'high', x: 2560, y: r1(Y(-7700)), z: r1(hAt(2560, Y(-7700))) }];

// ---------------------------------------------------------------- write Data/ocean.json
const out = {
	_comment: 'Generated by Tools/terrain/ocean.mjs (edit the script, not this file). Everything the Ocean builds over its ground (FWROceanData, WROceanTypes.cpp; cooks/build-biome-ocean.md 2.1-2.20). Map metres (game cm = m x 100); the cook\'s y squeezed by yMap. sea: the mean drawn surface (m). tsunami: the King Tide\'s safe line (its stones on the contour). decor rows [x, y, z, yaw, scale(, variant)]; walls [x1, y1, z1, x2, y2, z2]; kelp scale = height (m).',
	yMap: { a: -7955, k: 0.8673 }, sea: SEA, frame: [X0, Y0, X0 + (N - 1) * STEP, -5640],
	zones: { O1: 'the White Strand', O2: 'the Blowhole Shelf', O3: 'the Salt Terraces', O4: 'the Kelp Forest', O5: 'the Reef and the Drop' },
	coast: Array.from({ length: 50 }, (_, k) => { const y = lerp(-7930, -5760, k / 49); return [r1(COAST(y)), r1(y)]; }),
	landmarks, sites, kit, decor, caves, hall, resources,
	tsunami: { contour: TSUNAMI, stones },
	dyke: { towers: DYKE.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]), wallTop: -1.0 },
	sluiceway: SLUICE.map(([x, y]) => [r1(x), r1(y)]), inlet: INLET.map(([x, y]) => [r1(x), r1(y)]), road: ROAD.map(([x, y]) => [r1(x), r1(y), r1(hAt(x, y))]),
	saltgate: Array.from({ length: 14 }, (_, k) => { const x = lerp(-320, 960, k / 13); return [r1(x), r1(SG_Y(x)), r1(SG_FLOOR(x))]; }),
	rips, blowholes, surges, fleets, airPockets, arenas, mooringStones, routes: Object.fromEntries(Object.entries(ROUTES).map(([k, v]) => [k, v.map(([x, y]) => [r1(x), r1(y)])])),
	secrets, portalSlots, spots, caches,
};
fs.writeFileSync(path.join(root, 'Data', 'ocean.json'), JSON.stringify(out) + '\n');

// ---------------------------------------------------------------- write the land
const ZC = 20, ZN = Math.ceil((N - 1) * STEP / ZC);
const zoneRows = [];
for (let j = 0; j < ZN; j++) {
	let row = '';
	for (let i = 0; i < ZN; i++) {
		const x = X0 + (i + 0.5) * ZC, y = Y0 + (j + 0.5) * ZC;
		if (y > -5640) { row += '~'; continue; }
		const z = zoneOf(x, y);
		row += x < 300 ? 'p' : { O1: 's', O2: 'h', O3: 't', O4: 'k', O5: 'r' }[z];
	}
	zoneRows.push(row);
}
const camps = sites.filter(s => s.camp);
{
	const hb = Buffer.alloc(N * N * 2), sb = Buffer.alloc(N * N);
	for (let k = 0; k < N * N; k++) { hb.writeUInt16LE(Math.max(1, Math.min(65535, Math.round(32768 + HF[k] * UNITS))), k * 2); sb[k] = SURFM[k]; }
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'ocean_ground.bin'), Buffer.concat([hb, sb]));
	const mid = [X0 + (N - 1) * STEP / 2, Y0 + (N - 1) * STEP / 2];
	const sp = (id, cls, hex, rough, speck) => ({ id, class: cls, color: C(hex), rough, ...(speck ? { speck } : {}) });
	const land = {
		_comment: 'Generated by Tools/terrain/ocean.mjs (edit the script, not this file). The Ocean & reefs\' ground and sea floor as a runtime heightfield land (WRLand; cooks/build-biome-ocean.md 2.1): 5 m samples (uint16 heights = offset + m x unitsPerMetre, then a surface byte each; 255 hidden: the Sky\'s, the Storm crown\'s and the Swamp\'s own ground). The sea floor is drawn (its colour by depth band is how the water reads its depth). Zones by the grid letter (s O1 the White Strand, h O2 the Blowhole Shelf, t O3 the Salt Terraces, k O4 the Kelp Forest, r O5 the Reef and the Drop, p the Spine), surfaces by class over them. camps: PlayerStarts tagged ocean (the Tide path), a safe zone each, the starter stations at the middle one. underground.ways: the sea caves and the Drowned Hall (no rescue there).',
		id: 'ocean', biome: 'ocean', name: 'the Ocean & reefs', priority: 0,
		origin: [mid[0] * 100, mid[1] * 100, 0], mapCentre: mid,
		seed: SEED, grid: STEP, chunk: 200, drawDistance: 1200000, chunksPerFrame: 4,
		seaFloor: -14,
		heightfield: { file: 'ocean_ground.bin', x0: X0, y0: Y0, step: STEP, samples: N, unitsPerMetre: UNITS, offset: 32768, surface: true },
		zoneGrid: { cell: ZC, x0: X0, y0: Y0, width: ZN, height: ZN, rows: zoneRows },
		zoneBlend: 60,
		zones: [
			{ id: 'O1', letter: 's', name: 'the White Strand', ground: C('#EDE8DC'), groundAlt: C('#E2DCCC'), rough: 0.9, bed: 'zone.O1', speck: { colors: [C('#A8A39A'), C('#D9CDB4')], amount: 0.05, scale: 2 } },
			{ id: 'O2', letter: 'h', name: 'the Blowhole Shelf', ground: C('#C9A46A'), groundAlt: C('#B8945E'), rough: 0.82, bed: 'zone.O2', speck: { colors: [C('#2A2A28'), C('#E8E0D0')], amount: 0.08, scale: 2 } },
			{ id: 'O3', letter: 't', name: 'the Salt Terraces', ground: C('#B98A52'), groundAlt: C('#A97E4C'), rough: 0.9, bed: 'zone.O3', speck: { colors: [C('#8C9A7A'), C('#F2EFE6')], amount: 0.12, scale: 3 } },
			{ id: 'O4', letter: 'k', name: 'the Kelp Forest', ground: C('#D8CDB0'), groundAlt: C('#C9BE9C'), rough: 0.9, bed: 'zone.O4' },
			{ id: 'O5', letter: 'r', name: 'the Reef and the Drop', ground: C('#EADFC8'), groundAlt: C('#E0CDB2'), rough: 0.85, bed: 'zone.O5' },
			{ id: 'spine', letter: 'p', name: 'the Spine', ground: C('#8E8A82'), groundAlt: C('#A49C8C'), rough: 0.88 },
		],
		surfaces: [
			sp('sand', SURF.sand, '#EDE8DC', 0.92, { colors: [C('#D9CDB4'), C('#FFFFFF')], amount: 0.1, scale: 1.5 }),
			sp('wetsand', SURF.wetsand, '#CFC7B5', 0.4),
			sp('shelf', SURF.shelf, '#C9A46A', 0.82, { colors: [C('#B08A50'), C('#E2C890')], amount: 0.2, scale: 2 }),
			sp('wetrock', SURF.wetrock, '#2A2A28', 0.35, { colors: [C('#4A5A3A'), C('#E8E0D0')], amount: 0.12, scale: 1.5 }),
			sp('pool', SURF.pool, '#3E8C7A', 0.3, { colors: [C('#5A2E6A'), C('#E07A4F')], amount: 0.06, scale: 1 }),
			sp('terrace', SURF.terrace, '#B98A52', 0.9, { colors: [C('#8C9A7A'), C('#9C7A48')], amount: 0.15, scale: 3 }),
			sp('salt', SURF.salt, '#F2EFE6', 0.6, { colors: [C('#D8D2C4')], amount: 0.2, scale: 1 }),
			sp('shallows', SURF.shallows, '#F4EFE2', 0.9, { colors: [C('#E3D8BE')], amount: 0.15, scale: 2 }),
			sp('kelpfloor', SURF.kelpfloor, '#9C9468', 0.9, { colors: [C('#8A7A2E'), C('#D8CDB0')], amount: 0.3, scale: 3 }),
			sp('barren', SURF.barren, '#7A6A7C', 0.85, { colors: [C('#5A2E6A'), C('#3A1E44')], amount: 0.4, scale: 1 }),
			sp('reef', SURF.reef, '#EADFC8', 0.8, { colors: [C('#E07A4F'), C('#E8A0A8'), C('#C9B48A')], amount: 0.45, scale: 1.5 }),
			sp('slope', SURF.slope, '#BFB49C', 0.85, { colors: [C('#E07A4F'), C('#8A8070')], amount: 0.25, scale: 2 }),
			sp('wall', SURF.wall, '#3E4448', 0.8, { colors: [C('#2A3036')], amount: 0.3, scale: 3 }),
			sp('ooze', SURF.ooze, '#1C2228', 0.95),
			sp('mud', SURF.mud, '#6E6450', 0.6, { colors: [C('#4E4A3A')], amount: 0.2, scale: 2 }),
			sp('marsh', SURF.marsh, '#8C9A6A', 0.85, { colors: [C('#F2EFE6'), C('#6E7A4A')], amount: 0.25, scale: 1.5 }),
			sp('road', SURF.road, '#CDBF9E', 0.92, { colors: [C('#A8A39A')], amount: 0.1, scale: 1 }),
			sp('granite', SURF.granite, '#8E8A82', 0.85, { colors: [C('#B98A52'), C('#6E6A62')], amount: 0.2, scale: 4 }),
			sp('oyster', SURF.oyster, '#5E5848', 0.55, { colors: [C('#BDB6A6'), C('#3E3A30')], amount: 0.3, scale: 1 }),
			sp('dune', SURF.dune, '#E6DECB', 0.92, { colors: [C('#8FA65A'), C('#D9CDB4')], amount: 0.18, scale: 2 }),
		],
		slopeRock: { color: C('#B49A70'), fromDeg: 38, toDeg: 52, ledge: C('#8C9A7A') },
		bands: [],
		underground: { letters: '', ways: [
			...caves.flatMap(c => c.rooms.map(([dx, dy, r]) => [r1(c.x + dx), r1(c.y + dy), r1(c.x + dx), r1(c.y + dy), r + 6])),
			...hall.rooms.map(r => [r1(hallC.x + r.dx), r1(hallC.y + r.dy), r1(hallC.x + r.dx), r1(hallC.y + r.dy), r.r + 6]),
		] },
		buildFirst: camps.map(c => [c.x, c.y]),
		camps: { tag: 'ocean', safeRadius: 35, list: camps.map(c => [c.x, c.y, 270]), stationsAt: 1, stations: ['forge', 'armorer', 'alchemy_table', 'workbench'], stationsBack: 25 },
	};
	fs.writeFileSync(path.join(root, 'Data', 'lands', 'ocean.json'), JSON.stringify(land) + '\n');
	console.error(`land ${N} x ${N} at ${STEP} m: ${((hb.length + sb.length) / 1048576).toFixed(2)} MB`);
}

// ---------------------------------------------------------------- the map's place list
{
	const dressPath = path.join(root, 'Data', 'island_dressing.json');
	try {
		const dress = JSON.parse(fs.readFileSync(dressPath, 'utf8'));
		dress.pois = (dress.pois || []).filter(p => !p.ocean);
		for (const s of [...sites.filter(s => s.camp), ...landmarks]) {
			dress.pois.push({ name: s.name, kind: s.camp ? 'camp' : 'landmark', x: Math.round(s.x * 100), y: Math.round(s.y * 100), z: Math.round(s.z * 100), biome: 'ocean', ocean: true });
		}
		fs.writeFileSync(dressPath, JSON.stringify(dress) + '\n');
	} catch (e) { console.error(`dressing pois not written: ${e.message}`); }
}

// ---------------------------------------------------------------- the spacing check (cook 2.4: a POI every 150-250 m)
{
	const all = [...landmarks, ...sites, ...kit, ...caves];
	const counts = {}; for (const p of all) { const z = p.zone ?? zoneOf(p.x, p.y); counts[z] = (counts[z] || 0) + 1; }
	let worst = 0;
	for (let y = -7880; y < -5820; y += 50) for (let x = -100; x < 3600; x += 50) {
		if (x < 350 && hAt(x, y) > 70) continue;
		if (hAt(x, y) < -60) continue;
		let d = 1e9; for (const p of all) d = Math.min(d, Math.hypot(p.x - x, p.y - y));
		worst = Math.max(worst, d);
	}
	console.error(`landmarks ${landmarks.length}, sites ${sites.length}, kit ${kit.length}, caves ${caves.length}; by zone ${JSON.stringify(counts)}; farthest from any POI ${worst.toFixed(0)} m`);
	console.error(`decor: ${Object.entries(decor).map(([k, v]) => `${k} ${v.length}`).join(', ')}; resources ${resources.length}; stones ${stones.map(s => s[2]).join('/')}`);
	console.error(`camps: ${camps.map(c => `${c.name} (${c.x}, ${c.y}, ${c.z} m)`).join('; ')}`);
}
