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
import * as FF from "./fire_frost.mjs";

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
// The Volcanic's volcano where Vulkarn's crater maps (Tools/terrain/fire_frost.mjs): a steep cone on a broad low shield.
// The crater: a steep inner wall (power wallPower) down to a flat floor (Vulkarn's ground), craterDepth below the cone
// at the rim, with a lip; breached on the side facing breachTo (the crater stair, Emberwick), where the wall runs out
// into a ramp a body can walk.
const VOLCANO = { bearing: 266.6, inFromCoast: FF.CRATER_IN, height: 330, radius: 700, crater: 165, craterDepth: 80, lip: 12, wallPower: 2.5,
	breachTo: [-5098, 205], shield: 1300, shieldHeight: 32 };

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

// The Volcanic and the Frozen tundra (slices 2 and 3): each sub-zone's own ground, blended by where the point maps on
// the Old Island (old x is how far from the ridge, old y north or south; Tools/terrain/fire_frost.mjs), so the sub-zones
// run into each other with no seams. The new inner ground past the old ridge (V6, T6) steps down to the meadow.
const BOWL = FF.toRound("tundra", [804, 0]), GREAT_LAKE = FF.toRound("tundra", [900, -1250]);
// A soft minimum (k m of rounding where a and b cross): crater crests and cone rims without a knife edge.
const smin = (a, b, k) => { const t = Math.max(0, Math.min(1, 0.5 + 0.5 * (b - a) / k)); return lerp(b, a, t) - k * t * (1 - t); };
const FFL = JSON.parse(fs.readFileSync(path.join(root, "Tools/terrain/fire_frost_layout.json"), "utf8"));
// Where nothing big may rise: the roads, the lava river, villages, camps, every place, site, cave and shrine.
const KEEP_CLEAR = (() => {
	const pts = [];
	const add = (p, r) => p && pts.push([p[0], p[1], r]);
	for (const id of ["volcanic", "tundra"]) {
		const L = FFL.lands[id];
		for (let i = 0; i < L.road.length; i += 2) add(L.road[i], 70);
		add(L.village, 170); add(L.mouthShrine, 80); add(L.champion, 80);
		for (const c of L.camps) add(c, 120);
	}
	for (const p of FF.LAVA_RIVER) add(p, 90);
	for (const k of ["places", "sites", "caves", "shrines"]) for (const o of FFL[k] || []) if (Array.isArray(o.at)) add(o.at, k === "sites" ? 160 : 70);
	return pts;
})();
const clearOf = (x, y, r) => KEEP_CLEAR.every(([px, py, pr]) => Math.hypot(x - px, y - py) > pr + r);
const rng = (seed) => () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
const inLand = (id, x, y) => { const d = bearingOf(x, y), S = FF.SIDE[id], r = Math.hypot(x, y); return d > S.lo + 1.5 && d < S.hi - 1.5 && r < coastAt(d) - 140 && r > meadowAt(d) + MEADOW_RISE; };
/** Scattered round features (cinder cones, pingos): n of them in a land's sub-zones, clear of KEEP_CLEAR and of each other. */
function scatter(id, zones, n, seed, make) {
	const R = rng(seed), out = [];
	for (let tries = 0; out.length < n && tries < 6000; tries++) {
		const x = FF.SIDE[id].side * (1200 + R() * 5200), y = -2600 + R() * 5200;
		if (!inLand(id, x, y) || !zones.includes(FF.subZoneAt(id, [x, y]))) continue;
		const f = make(R, [x, y]);
		if (!clearOf(x, y, f.r * 0.8) || out.some((o) => Math.hypot(o.at[0] - x, o.at[1] - y) < (o.r + f.r) * 1.15)) continue;
		if (Math.hypot(x - volcano[0], y - volcano[1]) < VOLCANO.radius * 0.62) continue;
		out.push(f);
	}
	return out;
}
// Cinder cones: on the volcano's lower flanks and out on the lava fields and the terraces (the volcano's children).
const CONES = [
	...scatter("volcanic", ["V5", "V2"], 4, 71, (R, at) => { const r = 170 + R() * 70; return { at, r, h: r * (0.4 + R() * 0.08), cr: r * 0.34, cd: 0 }; }),
	...scatter("volcanic", ["V6", "V7"], 5, 72, (R, at) => { const r = 150 + R() * 80; return { at, r, h: r * (0.34 + R() * 0.1), cr: r * 0.33, cd: 0 }; }),
].map((c) => ({ ...c, cd: c.h * 0.55 }));
// Maars: explosion craters sunk below the ground, a low ring of ejecta round each (the terraces and the ash woods).
// (Kept well in from the coast, so no crater sinks below the sea.)
const MAARS = scatter("volcanic", ["V6", "V7", "V2"], 6, 74, (R, at) => { const r = 130 + R() * 70; return { at, r, depth: 30 + R() * 18 }; })
	.filter((m) => Math.hypot(...m.at) < coastAt(bearingOf(...m.at)) - m.r - 450).slice(0, 4);
function maarH(m, x, y) {
	const d = Math.hypot(x - m.at[0], y - m.at[1]);
	if (d > m.r + 120) return 0;
	const bowl = d < m.r ? -m.depth * (1 - Math.pow(d / m.r, 3)) : 0;
	return bowl + 9 * Math.exp(-(((d - m.r) / 45) ** 2));
}
function coneH(c, x, y) {
	const d = Math.hypot(x - c.at[0], y - c.at[1]);
	if (d >= c.r) return 0;
	const outer = c.h * Math.pow(1 - d / c.r, 1.25), rim = c.h * Math.pow(1 - c.cr / c.r, 1.25);
	return Math.max(0, smin(outer, rim - c.cd + c.cd * Math.pow(d / c.cr, 2.4), 5));
}
// Pingos: ice-cored hills on the snow flats and the thaw, a few with their tops fallen in. The Fox Pingo is the biggest.
const FOX = (FFL.places.find((p) => p.id === "fox_pingo") || {}).at;
const PINGOS = [
	...(FOX ? [{ at: [FOX[0] + 40, FOX[1] - 30], r: 150, h: 45, cr: 34, cd: 14 }] : []),
	...scatter("tundra", ["T1", "T6", "T7"], 7, 81, (R, at) => { const r = 80 + R() * 60; const fallen = R() < 0.4; return { at, r, h: r * (0.3 + R() * 0.1), cr: fallen ? r * 0.24 : 0, cd: fallen ? 12 : 0 }; }),
];
function pingoH(p, x, y) {
	const d = Math.hypot(x - p.at[0], y - p.at[1]);
	if (d >= p.r) return 0;
	let h = p.h * Math.pow(1 - (d / p.r) ** 2, 1.6);
	if (p.cr && d < p.cr) h -= p.cd * (1 - (d / p.cr) ** 2);
	return h;
}
// The lava breaks out of the volcano's roots: a rocky rise at the river's head, the channel cut into its west face.
const LAVA_HEAD = (() => { const [a, b] = FF.LAVA_RIVER, L = Math.hypot(a[0] - b[0], a[1] - b[1]); return [a[0] + (a[0] - b[0]) / L * 55, a[1] + (a[1] - b[1]) / L * 55]; })();
// Voronoi edges (patterned ground: frost polygons ~cell m across): 0 on an edge, 1 well inside a polygon.
function polyEdge(x, y, cell, seed) {
	const gx = Math.floor(x / cell), gy = Math.floor(y / cell);
	let d1 = 1e9, d2 = 1e9;
	for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) {
		const cx = gx + i, cy = gy + j;
		let hsh = (Math.imul(cx, 374761393) + Math.imul(cy, 668265263) + seed) | 0;
		hsh = Math.imul(hsh ^ (hsh >>> 13), 1274126177);
		const px = (cx + ((hsh & 1023) / 1023) * 0.8 + 0.1) * cell, py = (cy + (((hsh >>> 10) & 1023) / 1023) * 0.8 + 0.1) * cell;
		const d = Math.hypot(x - px, y - py);
		if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
	}
	return Math.min(1, (d2 - d1) / 5);
}
// The Thaw's braided meltwater channels and frozen ponds, and the Great Frozen Lake's pressure ridges (0..1), shared
// with the surfaces below.
const thawChan = (x, y) => Math.max(0, 1 - Math.abs(fbm(x / 380, y / 240, 1068 + 30, 3)) * 40);
const thawPond = (x, y) => smooth(-0.58, -0.66, fbm(x / 60, y / 60, 1068 + 31, 2));
const lakeRidge = (x, y) => Math.max(0, 1 - Math.abs(Math.sin((x * 0.8 - y * 0.6) / 70 + 1.2 * fbm(x / 400, y / 400, 1068 + 34, 2))) / 0.05) * smooth(0.0, 0.25, fbm(x / 300, y / 300, 1068 + 49, 2));
// The Volcanic's middle sub-zones (the lava fields, the obsidian ridges, the solfatara) shape the ground where those
// zones really lie: each zone's share of the ground round a point, from a 16 m grid of the zones blurred over ~130 m and
// read smoothly, so a zone's shapes fade in across its edge instead of stepping.
const VZONES = (() => {
	const cell = 16, x0 = -6640, y0 = -3200, nx = Math.ceil(5800 / cell) + 1, ny = Math.ceil(6400 / cell) + 1, keys = ["V2", "V3", "V4"];
	const g = keys.map(() => new Float32Array(nx * ny));
	for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
		const k = keys.indexOf(FF.subZoneAt("volcanic", [x0 + i * cell, y0 + j * cell]));
		if (k >= 0) g[k][j * nx + i] = 1;
	}
	const blur = (a, R) => {
		const t = new Float32Array(a.length);
		for (let j = 0; j < ny; j++) { let acc = 0; for (let i = -R; i <= R; i++) acc += a[j * nx + Math.max(0, Math.min(nx - 1, i))]; for (let i = 0; i < nx; i++) { t[j * nx + i] = acc / (2 * R + 1); acc += a[j * nx + Math.min(nx - 1, i + R + 1)] - a[j * nx + Math.max(0, i - R)]; } }
		for (let i = 0; i < nx; i++) { let acc = 0; for (let j = -R; j <= R; j++) acc += t[Math.max(0, Math.min(ny - 1, j)) * nx + i]; for (let j = 0; j < ny; j++) { a[j * nx + i] = acc / (2 * R + 1); acc += t[Math.min(ny - 1, j + R + 1) * nx + i] - t[Math.max(0, j - R) * nx + i]; } }
	};
	for (const a of g) { blur(a, 4); blur(a, 4); }
	return (x, y) => {
		const fx = Math.max(0, Math.min(nx - 1.001, (x - x0) / cell)), fy = Math.max(0, Math.min(ny - 1.001, (y - y0) / cell)), i = Math.floor(fx), j = Math.floor(fy), tx = fx - i, ty = fy - j;
		return g.map((a) => { const k = j * nx + i; return a[k] * (1 - tx) * (1 - ty) + a[k + 1] * tx * (1 - ty) + a[k + nx] * (1 - tx) * ty + a[k + nx + 1] * tx * ty; });
	};
})();
// Hornitos: small spatter cones on the lava fields, a few metres high with a throat at the top.
const HORNITOS = scatter("volcanic", ["V2"], 16, 73, (R, at) => { const r = 20 + R() * 12; return { at, r, h: r * (0.32 + R() * 0.12), cr: r * 0.28, cd: r * 0.2 }; });
// Nunataks: dark rock knobs standing out of the glacier plateau's ice.
const NUNATAKS = [40, 150, 230, 320].map((a, i) => { const t = a * Math.PI / 180, d = 330 + 60 * (i % 2); return { at: [BOWL[0] + Math.cos(t) * d, BOWL[1] + Math.sin(t) * d], r: 70 + 12 * (i % 3), h: 36 + 8 * (i % 2) }; });
// The Frozen Lakes are lakes: flat basins of ice in the hollows (0..1), besides the Great Frozen Lake.
// The Great Frozen Lake's shore: an ellipse bent by noise into bays and points (not a ring from above).
const lakeD = (x, y) => Math.hypot((x - GREAT_LAKE[0]) / 1.25, y - GREAT_LAKE[1]) + 75 * fbm(x / 230, y / 230, 1068 + 47, 2) + 22 * fbm(x / 70, y / 70, 1068 + 48, 2);
const t3Lake = (x, y) => smooth(-0.2, -0.28, fbm(x / 430, y / 430, 1068 + 42, 2));
// Where the new ground's outer sub-zone meets its inner one (same line as fire_frost.mjs subZoneAt): 0 outer, 1 inner.
const innerOf = (id, x, y, b) => smooth(-120, 120, b - (FF.SIDE[id].split + 180 * fbm(x / 1100, y / 1100, 507, 2)));
function landV(x, y, c) {
	const o = FF.fromRound("volcanic", FF.warpXY([x, y])), ax = Math.abs(o.at[0]), s = 1240;
	const base = 8 + 40 * smooth(0, 0.35, c) * (1 - 0.65 * smooth(0.75, 1, c));
	const b = FF.beyondAt("volcanic", [x, y]), wNew = smooth(0, 320, b), rest = 1 - wNew, inner = innerOf("volcanic", x, y, b);
	const w6 = wNew * inner, w7 = wNew * (1 - inner);
	const wc = smooth(1150, 1400, ax) * rest, rem = rest - wc;
	const zw = VZONES(x, y);
	const w2 = rem * zw[0], w3 = rem * zw[1], w4 = rem * zw[2], w5 = Math.max(0, rem - w2 - w3 - w4);
	// V1 the Ash Plains: wind-built ash dunes, a long windward slope and a steep lee, over a slow swell.
	const ph = (x * 0.6 + y * 0.8) / 18 + 3 * fbm(x / 300, y / 300, s, 2), fr = ph / (2 * Math.PI) - Math.floor(ph / (2 * Math.PI));
	const ash = 7 * smooth(0, 0.78, fr) * (1 - smooth(0.78, 1, fr)) * (0.55 + 0.45 * fbm(x / 260, y / 260, s + 11, 2)) + 6 * fbm(x / 500, y / 500, s + 1, 3);
	// V2 the Lava Fields: overlapping flow sheets, each ending in a steep front a few metres high (elongated down the
	// slope, west to the sea), with tumuli (pressure blisters) on them.
	const wx = x + 140 * fbm(x / 300, y / 300, s + 20, 2), wy = y + 140 * fbm(x / 300, y / 300, s + 25, 2);
	const F = fbm(wx / 460, wy / 250, s + 21, 3);
	let lobes = 0;
	for (const t of [-0.3, -0.12, 0.06, 0.24]) lobes += smooth(t - 0.007, t + 0.007, F);
	const tum = 2.6 * Math.pow(Math.max(0, fbm(x / 34, y / 34, s + 22, 2) - 0.18) / 0.82, 1.3);
	const flow = 7 * lobes + 1.6 * tum + 5 * fbm(x / 400, y / 400, s + 3, 3);
	// V3 the Obsidian Ridges: tall, sharp-crested ridges with glassy faces, smaller blades between them.
	const rn = fbm(x / 270, y / 270, s + 4, 4);
	const ridges = 92 * Math.pow(Math.max(0, 1 - Math.abs(rn) * 2.6), 2.6) + 7 * Math.pow(Math.max(0, 1 - Math.abs(fbm(x / 80, y / 80, s + 23, 2)) * 2.8), 2) + 3 * fbm(x / 120, y / 120, s + 5, 2);
	// V4 the Solfatara: a sunken, steaming basin dotted with fumarole mounds and boiling pools.
	const mounds = 8 * Math.pow(Math.max(0, fbm(x / 46, y / 46, s + 24, 2) - 0.16) / 0.84, 1.1);
	const solf = -32 * smooth(0.15, 0.85, zw[2]) + mounds + 5 * fbm(x / 350, y / 350, s + 6, 3) - 1.5 * smooth(-0.42, -0.5, fbm(x / 60, y / 60, s + 27, 2));
	// V6 the Cinder Terraces: broad steps down toward the meadow, each with a crisp riser.
	const v = (fbm(x / 1100, y / 1100, s + 8, 3) + 1) * 2.5 + o.beyond / 500, vf = v - Math.floor(v);
	const terraces = 16 * (Math.floor(v) + smooth(0.46, 0.54, vf)) - 16 * o.beyond / 500 + 4 * fbm(x / 160, y / 160, s + 9, 2);
	// V7 the Ashfall Woods: an old forest smothered by a fall of ash: soft drifted swells, hummocks over fallen trunks.
	const woods = 6 * fbm(x / 340, y / 340, s + 40, 3) + 1.3 * Math.abs(fbm(x / 40, y / 40, s + 41, 2)) + 1.6 * Math.max(0, 1 - Math.abs(fbm(x / 90, y / 160, s + 42, 2)) * 9);
	let h = base + wc * ash + w2 * flow + w3 * ridges + w4 * solf + w5 * 5 * fbm(x / 300, y / 300, s + 10, 3) + w6 * terraces + w7 * woods;
	for (const cn of CONES) h += coneH(cn, x, y);
	for (const hn of HORNITOS) h += coneH(hn, x, y);
	for (const m of MAARS) h += maarH(m, x, y);
	h = Math.max(h, 2.5);   // hollows never sink below the sea
	const dh = Math.hypot(x - LAVA_HEAD[0], y - LAVA_HEAD[1]);
	if (dh < 170) h += 26 * Math.pow(1 - dh / 170, 1.4) * (1 + 0.25 * fbm(x / 30, y / 30, s + 26, 2));
	return h;
}
function landT(x, y, c) {
	const o = FF.fromRound("tundra", FF.warpXY([x, y])), ax = Math.abs(o.at[0]), oy = o.at[1], s = 1068;
	const base = 10 + 46 * smooth(0, 0.35, c) * (1 - 0.6 * smooth(0.75, 1, c));
	const b = FF.beyondAt("tundra", [x, y]), wNew = smooth(0, 320, b), rest = 1 - wNew, inner = innerOf("tundra", x, y, b);
	const w6 = wNew * inner, w7 = wNew * (1 - inner);
	const wc = smooth(1150, 1400, ax) * rest, rem = rest - wc;
	// (The bands are bent a little more than the zones, so the ice shelves' fronts wander instead of running straight.)
	const oyb = oy + 220 * fbm(x / 1300, y / 1300, s + 46, 2), north = smooth(-450, -650, oyb), south = smooth(350, 550, oyb);
	const w3 = rem * north, w2 = rem * south, w4 = Math.max(0, rem * (1 - north - south));
	// T1 the Snow Flats: wind-smoothed swells, wind-scoured rock knolls (erratics), frost polygons.
	const flats = 3 * fbm(x / 400, y / 400, s, 3) + 4 * Math.max(0, fbm(x / 45, y / 45, s + 1, 2) - 0.25);
	// T2 the Pine Hollows: rolling ground with kettle hollows among the trees.
	const pines = 24 * fbm(x / 320, y / 320, s + 2, 4) - 14 * smooth(0.12, 0.34, fbm(x / 170, y / 170, s + 9, 2));
	// T3 the Frozen Lakes: low ground with lakes of ice in its hollows.
	const lakes = 16 * fbm(x / 520, y / 520, s + 3, 3);
	// T4 the Ice Shelves: raised, smooth ice that ends in cliffs here and ramps there, crossed by crevasse fields
	// running across its flow (it flows to the coast).
	const across = (x * Math.cos(0.2) + y * Math.sin(0.2)) / 22 + 2.2 * fbm(x / 240, y / 240, s + 4, 2);
	const crev = smooth(0, 1, 1 - Math.abs(Math.sin(across)) / 0.28) * smooth(0.02, 0.3, fbm(x / 260, y / 260, s + 5, 2));
	const ramp = smooth(0.05, 0.35, fbm(x / 320, y / 320, s + 10, 2)), wq = w4 / Math.max(rem, 1e-3);
	const shelfOn = smooth(0.5 - lerp(0.08, 0.3, ramp), 0.5 + lerp(0.08, 0.3, ramp), wq);
	const shelf = (58 + 6 * fbm(x / 600, y / 600, s + 6, 3)) * shelfOn - 18 * crev * shelfOn;
	// T6 the Thaw: hummocky moss, braided meltwater channels of gravel, frozen ponds in the hollows.
	const chan = thawChan(x, y), pond = thawPond(x, y);
	const thaw = 9 * fbm(x / 450, y / 450, s + 7, 3) + 1.6 * Math.abs(fbm(x / 26, y / 26, s + 32, 2)) - 2.2 * chan - 1.8 * pond;
	// T7 the Mammoth Steppe: long, low, wind-laid swells of loess under dry grass, cut by a few shallow dry gullies.
	const gully = Math.max(0, 1 - Math.abs(fbm(x / 520, y / 300, s + 43, 3)) * 22);
	const steppe = 14 * fbm(x / 700, y / 700, s + 40, 3) + 3.5 * Math.sin((x * 0.3 - y * 0.95) / 60 + 2 * fbm(x / 400, y / 400, s + 41, 2)) - 9 * gully;
	let h = base + wc * flats + w2 * pines + w3 * lakes + rem * shelf + w6 * thaw + w7 * steppe;
	// The lakes of the Frozen Lakes: flat ice a metre over the land's base.
	const tl = t3Lake(x, y) * smooth(0.35, 0.75, w3);
	if (tl > 0) h = lerp(h, base + 1, tl);
	// Frost polygons on the flats and the thaw: their cracks sink a little.
	if (wc + w6 > 0.2) h -= 1.0 * (1 - polyEdge(x, y, 26, 401)) * smooth(0.0, 0.3, fbm(x / 240, y / 240, s + 33, 2)) * (wc + w6);
	for (const p of PINGOS) h += pingoH(p, x, y);
	// The glacier plateau round Skaldrith's bowl, and the bowl: a cirque, its headwall on the inland (west) side, open
	// to the east where the ice spills toward the coast; dark nunataks stand out of its ice.
	const db = Math.hypot(x - BOWL[0], y - BOWL[1]), cosE = db > 1 ? (x - BOWL[0]) / db : 0;
	h += 85 * (1 - smooth(320, 620, db)) * rest;
	{
		const east = Math.max(0, cosE) ** 2, R = 255 * (1 + 0.9 * east), p = 4 - 2.4 * east;
		if (db < R) h -= 95 * (1 - Math.pow(db / R, p));
	}
	h += 45 * Math.exp(-(((db - 262) / 45) ** 2)) * Math.max(0, -cosE) ** 1.2 * rest;
	for (const n of NUNATAKS) { const d = Math.hypot(x - n.at[0], y - n.at[1]); if (d < n.r) h += n.h * Math.pow(1 - (d / n.r) ** 2, 1.4) * (1 + 0.2 * fbm(x / 14, y / 14, s + 44, 2)); }
	// The Great Frozen Lake: a flat basin; pressure ridges where the ice pushed up.
	const dl = lakeD(x, y);
	if (dl < 300) h = lerp(h, Math.min(h, base + 2), 1 - smooth(200, 300, dl));
	if (dl < 205) h += 0.9 * lakeRidge(x, y);
	return Math.max(h, 2.5);   // kettle hollows never sink below the sea
}

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
		case "tundra": return landT(x, y, c);
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
		case "volcanic": return landV(x, y, c);
	}
	return 10 + 50 * p;
}

// ---- 1. every sample's shape: the lands blended across their borders, the meadow, the coast and the sea floor ------
const H = new Float32Array(N * N), S = new Uint8Array(N * N), OWN = new Int8Array(N * N);
const SURF = { bed: 1, bank: 2, sand: 3, shore_rock: 6, road: 7, main_road: 10, plaza: 11, lake_bed: 12, cobble: 13,
	wheat: 14, wheat2: 15, barley: 16, barley2: 17, greens: 18, greens2: 19, ploughed: 20, ploughed2: 21, fallow: 22, fallow2: 23,
	// The Volcanic's and the Tundra's (slices 2 and 3).
	ash: 24, black_sand: 25, pahoehoe: 26, aa: 27, obsidian: 28, sulfur: 29, steam_ground: 30, acid: 31, basalt: 32, scoria: 33,
	cinder: 34, cinder_grass: 35, kipuka: 36, lava: 37, lava_crust: 38, snow: 39, snow_forest: 40, lake_ice: 41, glacier: 42,
	crevasse: 43, thaw_moss: 44, shingle: 45, ice: 46, oxide: 47, snow_crack: 48, moss_crack: 49,
	// The Ashfall Woods and the Mammoth Steppe (11-20), and each old sub-zone's own touches.
	steppe: 50, ash_pale: 51, charcoal: 52, lichen_rock: 53, pumice: 54, moraine: 55, copper: 56 };
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
		if (dv < VOLCANO.shield) {
			h += VOLCANO.shieldHeight * Math.pow(1 - dv / VOLCANO.shield, 1.2);
			if (dv < VOLCANO.radius) {
				const cone = (d) => VOLCANO.height * Math.pow(Math.max(0, 1 - d / VOLCANO.radius), 1.6), R = VOLCANO.crater;
				const ba = Math.atan2(VOLCANO.breachTo[1] - volcano[1], VOLCANO.breachTo[0] - volcano[0]);
				const breach = Math.pow(Math.max(0, Math.cos(Math.atan2(y - volcano[1], x - volcano[0]) - ba)), 10);
				const floor = cone(R) - VOLCANO.craterDepth, rim = cone(R) + VOLCANO.lip;
				const lip = VOLCANO.lip * Math.exp(-(((dv - R) / 70) ** 2)) * (1 - breach);
				const wall = floor + (rim - floor) * Math.pow(dv / (R * (1 + 1.2 * breach)), VOLCANO.wallPower - 1.7 * breach);
				// Radial gullies down its flanks (rain on loose cinder), wandering a little.
				const th = Math.atan2(y - volcano[1], x - volcano[0]) + 0.3 * fbm(x / 260, y / 260, 91, 2);
				const gully = Math.pow(1 - Math.abs(Math.sin(th * 13)), 3.5) * (5 + 4 * fbm(th * 3, 1.7, 92, 2))
					* smooth(R + 25, R + 110, dv) * (1 - smooth(VOLCANO.radius * 0.72, VOLCANO.radius * 1.02, dv));
				h += (smin(cone(dv) + lip, wall, 12) - gully) * (1 + 0.07 * fbm(x / 110, y / 110, 77, 3));
			}
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
// The lava river (fire_frost.mjs LAVA_RIVER): its surface level down along it (from the tube mouth at its inner end to
// the sea, never rising), a channel under it with low levees of crust, its banks cut back.
const lavaLine = (() => {
	const pts = FF.LAVA_RIVER, out = [];
	let lv = 1e9;
	for (let i = 0; i < pts.length; i++) {
		const g = sampleH(...pts[i]), along = i ? Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]) : 0;
		lv = Math.min(lv - 0.015 * along, g - 1.2);
		out.push([pts[i][0], pts[i][1], Math.max(0.6, lv)]);
	}
	return out;
})();
{
	const D = new Float32Array(N * N).fill(1e9), A = new Float32Array(N * N), half = FF.LAVA_WIDTH / 2;
	const len = raster(FF.LAVA_RIVER, 90, D, A);
	const cum = [0]; for (let i = 1; i < lavaLine.length; i++) cum.push(cum[i - 1] + Math.hypot(lavaLine[i][0] - lavaLine[i - 1][0], lavaLine[i][1] - lavaLine[i - 1][1]));
	const levelAt = (a) => { let i = 1; while (i < cum.length - 1 && cum[i] < a) i++; const t = Math.min(1, Math.max(0, (a - cum[i - 1]) / (cum[i] - cum[i - 1] || 1))); return lerp(lavaLine[i - 1][2], lavaLine[i][2], t); };
	for (let k = 0; k < N * N; k++) {
		if (D[k] >= 90) continue;
		const d = D[k], lv = levelAt(A[k]);
		const want = d < half ? lv - 0.8 : d < half + 6 ? lv + 0.5 + (d - half) * 0.12 : lv + 1.2 + (d - half - 6) * 0.32;
		const xk = -HALF + (k % N) * STEP, yk = -HALF + Math.floor(k / N) * STEP;
		const seaward = Math.hypot(xk, yk) > coastAt(bearingOf(xk, yk)) - 4;
		if (seaward) { H[k] = Math.min(H[k], want); continue; }
		if (d < half + 6 || want < H[k]) H[k] = d < half + 6 ? want : Math.min(H[k], want);
		if (d < half) S[k] = SURF.lava; else if (d < half + 7) S[k] = SURF.lava_crust;
	}
	log(`lava river: ${Math.round(len)} m, ${lavaLine[0][2].toFixed(1)} m at its mouth down to ${lavaLine[lavaLine.length - 1][2].toFixed(1)} m`);
}
// The two lands' ground by sub-zone (where nothing more particular lies yet: beds, sand and the lava keep theirs).
// The Cinder Terraces green over as they step down to the meadow.
const grassy = (x, y) => { const deg = bearingOf(x, y), cc = Math.min(1, Math.max(0, 1 - (Math.hypot(x, y) - meadowAt(deg)) / (coastAt(deg) - meadowAt(deg)))); return fbm(x / 140, y / 140, 85, 3) > 0.1 + 0.4 * (1 - smooth(0.7, 1, cc)); };
const KIPUKA = FF.toRound("volcanic", [-1285, 610]), ACID = FF.toRound("volcanic", [-704, -1166]);
const VOLC_AT = (() => { const r = coastAt(VOLCANO.bearing) - VOLCANO.inFromCoast; return pt(VOLCANO.bearing, r); })();
for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
	const k = j * N + i, own = OWN[k];
	if (own !== LAND_INDEX.volcanic && own !== LAND_INDEX.tundra) continue;
	const x = -HALF + i * STEP, y = -HALF + j * STEP;
	if (H[k] < 0) continue;
	const cur = S[k];
	if (cur === SURF.sand) { S[k] = own === LAND_INDEX.volcanic ? SURF.black_sand : SURF.shingle; continue; }
	if (cur === SURF.shore_rock) continue;
	if (cur !== 0 && cur !== SURF.bed && cur !== SURF.bank) continue;
	if (own === LAND_INDEX.volcanic) {
		const z = FF.subZoneAt("volcanic", [x, y]);
		if (Math.hypot(x - KIPUKA[0], y - KIPUKA[1]) < 150 + 25 * fbm(x / 60, y / 60, 81, 2)) { S[k] = SURF.kipuka; continue; }
		if (Math.hypot(x - ACID[0], y - ACID[1]) < 26) { S[k] = SURF.acid; H[k] -= 1.2; continue; }
		const n = fbm(x / 90, y / 90, 83, 3), dv = Math.hypot(x - VOLC_AT[0], y - VOLC_AT[1]);
		S[k] = z === "V1" ? (fbm(x / 70, y / 70, 97, 2) > 0.55 ? SURF.pumice : SURF.ash) : z === "V2" ? (n > 0.12 ? SURF.aa : SURF.pahoehoe)
			: z === "V3" ? (fbm(x / 50, y / 50, 98, 2) > 0.55 ? SURF.copper : SURF.obsidian)
			: z === "V4" ? (fbm(x / 60, y / 60, 1240 + 27, 2) < -0.6 ? SURF.acid
				: fbm(x / 46, y / 46, 1240 + 24, 2) + 0.12 * fbm(x / 9, y / 9, 98, 2) > 0.2 ? SURF.sulfur
				: Math.abs(fbm(x / 160, y / 38, 94, 2)) < 0.035 ? SURF.oxide : SURF.steam_ground)
			: z === "V5" ? (dv < VOLCANO.crater + 40 ? SURF.scoria : SURF.basalt)
			: z === "V7" ? (Math.abs(fbm((x * 0.8 + y * 0.6) / 30, (y * 0.8 - x * 0.6) / 110, 99, 2)) < 0.025 ? SURF.charcoal : SURF.ash_pale)
			: grassy(x, y) ? SURF.cinder_grass : SURF.cinder;
		for (const hn of HORNITOS) if (Math.hypot(x - hn.at[0], y - hn.at[1]) < hn.r * 0.95) { S[k] = SURF.scoria; break; }
		for (const cn of CONES) if (Math.hypot(x - cn.at[0], y - cn.at[1]) < cn.r * (0.8 + 0.12 * fbm(x / 30, y / 30, 95, 2))) { S[k] = SURF.scoria; break; }
		// Where the old ground gives way to the terraces, its dark rock breaks up in patches over ~400 m.
		if (z === "V7") { const b = FF.beyondAt("volcanic", [x, y]); if (fbm(x / 70, y / 70, 89, 2) > -0.35 + 0.95 * smooth(60, 260, b)) S[k] = n > 0.1 ? SURF.aa : SURF.basalt; }
	} else {
		const z = FF.subZoneAt("tundra", [x, y]), n = fbm(x / 110, y / 110, 87, 3);
		const dlk = lakeD(x, y);
		if (dlk < 210 && (lakeRidge(x, y) > 0.35 || (dlk > 175 && fbm(x / 40, y / 40, 96, 2) > 0.1))) { S[k] = SURF.snow; continue; }   // pressure ridges, and drifts along the shore   // pressure ridges and drifts on the ice
		if (dlk < 210) { S[k] = SURF.lake_ice; continue; }
		S[k] = z === "T1" ? (fbm(x / 45, y / 45, 1068 + 1, 2) > 0.45 ? SURF.lichen_rock : SURF.snow) : z === "T2" ? SURF.snow_forest
			: z === "T3" ? (t3Lake(x, y) > 0.5 ? (fbm(x / 50, y / 50, 96, 2) > 0.35 ? SURF.snow : SURF.lake_ice) : SURF.snow)
			: z === "T4" ? SURF.glacier
			: z === "T5" ? (() => {   // the cirque floor one sheet of blue ice (Skaldrith's arena); polished ice in streaks down the ice cap
				const db = Math.hypot(x - BOWL[0], y - BOWL[1]), th = Math.atan2(y - BOWL[1], x - BOWL[0]);
				// (Polished streaks only on the cirque's own walls: out on the cap a thin noise-contour streak, drawn 4 m at a
				// time, read as a jagged dark band.)
				return db < 205 + 15 * fbm(x / 40, y / 40, 1068 + 50, 2) || (db < 300 && Math.abs(fbm(th * 9, db / 140, 1068 + 51, 2)) < 0.07) ? SURF.ice : SURF.glacier;
			})()
			: z === "T7" ? (Math.max(0, 1 - Math.abs(fbm(x / 520, y / 300, 1068 + 43, 3)) * 22) > 0.4 ? SURF.shingle
				: n > -0.25 + 0.9 * (1 - smooth(60, 200, FF.beyondAt("tundra", [x, y]))) ? SURF.steppe : SURF.snow)   // old snow lies in its hollows
			: (n > -0.15 ? SURF.thaw_moss : SURF.snow);
		if (z === "T6" && thawPond(x, y) > 0.5) S[k] = SURF.lake_ice;
		else if (z === "T6" && thawChan(x, y) > 0.35) S[k] = SURF.shingle;
		else if ((z === "T1" || z === "T6") && polyEdge(x, y, 26, 401) < 0.6 && fbm(x / 240, y / 240, 1068 + 33, 2) > 0.05) S[k] = S[k] === SURF.thaw_moss ? SURF.moss_crack : SURF.snow_crack;
		if (z === "T4" && clearOf(x, y, 0) && (H[k] < sampleH(x + 8, y) - 2 || H[k] < sampleH(x - 8, y) - 2) && (H[k] < sampleH(x, y + 8) - 1 || H[k] < sampleH(x, y - 8) - 1)) S[k] = SURF.crevasse;
	}
}
// The landforms content may stand on (vents on the cones, a den in a pingo, the tube mouth at the lava head).
fs.mkdirSync(path.join(root, "Saved/RoundIsland"), { recursive: true });
fs.writeFileSync(path.join(root, "Saved/RoundIsland/fire_frost_features.json"), JSON.stringify({ cones: CONES.map((c) => ({ at: c.at.map(Math.round), r: Math.round(c.r), h: Math.round(c.h) })), maars: MAARS.map((m) => ({ at: m.at.map(Math.round), r: Math.round(m.r), depth: Math.round(m.depth) })), pingos: PINGOS.map((p) => ({ at: p.at.map(Math.round), r: Math.round(p.r), h: Math.round(p.h), fallen: !!p.cr })), lavaHead: LAVA_HEAD.map(Math.round), bowl: BOWL.map(Math.round), greatLake: GREAT_LAKE.map(Math.round), volcano: volcano.map(Math.round) }, null, 1));
log(`fire and frost ground done: ${CONES.length} cinder cones (${CONES.map((c) => Math.round(c.h) + " m").join(", ")}), ${MAARS.length} maars, ${PINGOS.length} pingos (${PINGOS.map((p) => Math.round(p.h) + " m").join(", ")})`);

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
for (const v of Object.values(FF.VILLAGES)) pad(v.at, 48, 90);   // a long, easy blend: no cut face round the village
// The Fire and Frost places that sit on open ground get a little level floor (their builders lay flat crusts, rings and
// camps); the ones made to sit in a landform keep it (the plume in the crater, the seracs and the whaler in the ice, the
// stair up the cone, the ridge glass, the pingo, the arch, the moulin, the smith in the lava, the kipuka's own rise).
{
	const KEEP = new Set(["crater_plume", "crater_stair", "weeping_seracs", "frozen_whaler", "glasswall", "fox_pingo", "ice_arch", "moulin", "fallen_smith", "kipuka", "crevasse_maze", "claw_scars", "ridge_watch", "wardens_quarry", "steam_ground", "acid_pool"]);
	let n = 0;
	for (const p of FFL.places) if (!KEEP.has(p.kind) && Array.isArray(p.at)) { pad(p.at, 18, 26); n++; }
	log(`places levelled: ${n}`);
}
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
// The Volcanic's and the Tundra's sub-zones have letters of their own (their zone ids V1-V6, T1-T6: the sub-zone
// banners and the ambient beds go by them).
const SUBLETTER = { V1: "a", V2: "b", V3: "c", V4: "d", V5: "e", V6: "f", T1: "g", T2: "h", T3: "i", T4: "j", T5: "k", T6: "l", V7: "m", T7: "n" };
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
		if (a === LAND_INDEX.volcanic || a === LAND_INDEX.tundra) { row += SUBLETTER[FF.subZoneAt(LANDS[a].id, [x, y])]; continue; }
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
	{ id: "V1", letter: "a", name: "the Ash Plains", slopeRock: [56, 52, 50], ground: [78, 79, 84], groundAlt: [68, 68, 72], layers: { soil: 0.45, stones: 0.4, sand: 0.15 } },
	{ id: "V2", letter: "b", name: "the Lava Fields", slopeRock: [34, 31, 30], ground: [38, 34, 33], groundAlt: [52, 44, 40], layers: { stones: 0.9, soil: 0.1 } },
	{ id: "V3", letter: "c", name: "the Obsidian Ridges", slopeRock: [14, 13, 18], ground: [28, 27, 34], groundAlt: [48, 44, 56], layers: { stones: 1 } },
	{ id: "V4", letter: "d", name: "the Solfatara", slopeRock: [150, 128, 96], ground: [168, 162, 146], groundAlt: [182, 172, 138], layers: { sand: 0.5, stones: 0.5 } },
	{ id: "V5", letter: "e", name: "the Crater Highlands", slopeRock: [40, 34, 32], ground: [48, 44, 42], groundAlt: [78, 52, 42], layers: { stones: 0.8, soil: 0.2 } },
	{ id: "V6", letter: "f", name: "the Cinder Terraces", slopeRock: [54, 46, 40], ground: [64, 54, 46], groundAlt: [80, 86, 56], layers: { soil: 0.5, stones: 0.4, grass: 0.1 } },
	{ id: "V7", letter: "m", name: "the Ashfall Woods", slopeRock: [70, 66, 62], ground: [118, 118, 122], groundAlt: [104, 104, 108], layers: { soil: 0.5, stones: 0.25, sand: 0.25 } },
	{ id: "T1", letter: "g", name: "the Snow Flats", slopeRock: [200, 216, 232], ground: [236, 240, 244], groundAlt: [220, 228, 236], layers: { sand: 0.9, stones: 0.1 } },
	{ id: "T2", letter: "h", name: "the Pine Hollows", slopeRock: [188, 204, 222], ground: [210, 218, 220], groundAlt: [196, 206, 204], layers: { sand: 0.6, litter: 0.4 } },
	{ id: "T3", letter: "i", name: "the Frozen Lakes", slopeRock: [214, 230, 242], ground: [222, 232, 240], groundAlt: [190, 214, 228], layers: { sand: 1 } },
	{ id: "T4", letter: "j", name: "the Ice Shelves", slopeRock: [214, 232, 246], ground: [196, 218, 236], groundAlt: [176, 204, 228], layers: { sand: 0.8, stones: 0.2 } },
	{ id: "T5", letter: "k", name: "the Glacier Plateau", slopeRock: [214, 232, 246], ground: [224, 234, 244], groundAlt: [206, 222, 238], layers: { sand: 0.9, stones: 0.1 } },
	{ id: "T6", letter: "l", name: "the Thaw", slopeRock: [104, 102, 96], ground: [150, 156, 118], groundAlt: [226, 232, 236], layers: { moss: 0.5, sand: 0.3, grass: 0.2 } },
	{ id: "T7", letter: "n", name: "the Mammoth Steppe", slopeRock: [120, 110, 92], ground: [166, 148, 100], groundAlt: [150, 136, 96], layers: { grass: 0.7, soil: 0.3 } },
	{ id: "meadow", letter: "M", name: "Hearthmoot's meadow", ground: [96, 150, 62], groundAlt: [124, 166, 70], layers: { grass: 0.85, soil: 0.15 } },
	{ id: "city", letter: "C", name: "Hearthmoot's walls", ground: [96, 148, 62], groundAlt: [120, 162, 70], layers: { grass: 0.8, soil: 0.2 } },
	{ id: "yards", letter: "Y", name: "Hearthmoot's yards", ground: [92, 140, 64], groundAlt: [112, 150, 70], layers: { grass: 0.85, soil: 0.15 } },
	{ id: "seabed", letter: "~", name: "the sea floor", ground: [176, 164, 124], groundAlt: [150, 144, 112], layers: { sand: 0.9, stones: 0.1 } },
	{ id: "deep", letter: "D", name: "the deep sea floor", ground: [34, 46, 56], groundAlt: [30, 40, 50], layers: { sand: 0.6, stones: 0.4 } },
];
// Which biome each zone letter is (the lands' first-pass letters, the Volcanic's and the Tundra's sub-zones, the meadow,
// the city, the yards, the sea floor).
const ZONE_BIOME = { "1": "storm_peaks", "2": "desert", "3": "forest", "4": "tundra", "5": "swamp", "6": "ocean", "7": "graveyard", "8": "wasteland", "9": "volcanic",
	a: "volcanic", b: "volcanic", c: "volcanic", d: "volcanic", e: "volcanic", f: "volcanic", g: "tundra", h: "tundra", i: "tundra", j: "tundra", k: "tundra", l: "tundra", m: "volcanic", n: "tundra", M: "meadow", C: "meadow", Y: "meadow", "~": "ocean", D: "ocean" };
const land = {
	_comment: "Generated by Tools/terrain/round_ground.mjs from the signed-off layout (Tools/terrain/round_island.mjs; edit the scripts, not this file). The round island as one runtime heightfield land on its own map (map): 4 m samples in island_ground.bin (zlib: uint16 heights = offset + m x unitsPerMetre, then a surface byte each), built as procedural-mesh chunks, full detail near you and coarser far off (lod), with collision only near players. Zones by letter: one per land (each land's slice replaces its first-pass ground later), the meadow (M), Hearthmoot's hill top (C), the sea floor (~); zoneBiomes says which biome each letter is.",
	id: "island", biome: "meadow", name: "the round island", map: MAP,
	origin: [0, 0, 0], mapCentre: [0, 0], seed: 11, grid: STEP, chunk: 200, drawDistance: 1400000, chunksPerFrame: 6, seaFloor: SEA_FLOOR,
	heightfield: { file: "island_ground.bin", x0: -HALF, y0: -HALF, step: STEP, samples: N, unitsPerMetre: UNITS, offset: OFFSET, surface: true, compressed: "zlib", rawBytes: bytes.length },
	lod: { _comment: "Far detail levels (WRLand): a chunk this far (m) from your view uses every 2nd, 4th, 8th sample; collision only within collisionRange m of a player.", ranges: [700, 1600, 3600], collisionRange: 600 },
	zoneGrid: { cell: CELL, x0: -HALF, y0: -HALF, width: GW, height: GW, rows },
	zoneBlend: 40,
	zoneBiomes: ZONE_BIOME,
	groundMaterial: "/Game/Terrain/M_IslandGround.M_IslandGround",
	// Each zone carries its own biome: zoneBiomes is a JSON object, and the game matches its keys without case ("c" = "C").
	zones: ZONES.map((z) => ({ id: z.id, letter: z.letter, name: z.name, biome: ZONE_BIOME[z.letter], ground: z.ground, groundAlt: z.groundAlt, ...(z.slopeRock ? { slopeRock: z.slopeRock } : {}), rough: 0.9, ...(z.speck ? { speck: z.speck } : {}), layers: z.layers })),
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
		{ id: "ash", class: SURF.ash, color: [76, 77, 82], rough: 0.95, layers: { soil: 0.45, stones: 0.4, sand: 0.15 } },
		{ id: "black_sand", class: SURF.black_sand, color: [44, 42, 42], rough: 0.85, layers: { sand: 1 } },
		{ id: "pahoehoe", class: SURF.pahoehoe, color: [36, 33, 32], rough: 0.55, layers: { stones: 0.8, soil: 0.2 } },
		{ id: "aa", class: SURF.aa, color: [52, 43, 38], rough: 0.95, layers: { stones: 1 } },
		{ id: "obsidian", class: SURF.obsidian, color: [18, 17, 22], rough: 0.55, layers: { stones: 1 } },
		{ id: "sulfur", class: SURF.sulfur, color: [206, 188, 112], rough: 0.85, layers: { sand: 0.6, stones: 0.4 } },
		{ id: "steam_ground", class: SURF.steam_ground, color: [188, 182, 166], rough: 0.8, layers: { sand: 0.5, soil: 0.5 } },
		{ id: "acid", class: SURF.acid, color: [74, 160, 146], rough: 0.25, layers: { sand: 1 } },
		{ id: "basalt", class: SURF.basalt, color: [46, 44, 44], rough: 0.85, layers: { stones: 0.9, soil: 0.1 } },
		{ id: "scoria", class: SURF.scoria, color: [98, 50, 38], rough: 0.95, layers: { stones: 0.7, sand: 0.3 } },
		{ id: "cinder", class: SURF.cinder, color: [62, 52, 46], rough: 0.95, layers: { soil: 0.5, stones: 0.5 } },
		{ id: "cinder_grass", class: SURF.cinder_grass, color: [80, 90, 54], rough: 0.9, layers: { grass: 0.6, soil: 0.4 } },
		{ id: "kipuka", class: SURF.kipuka, color: [72, 90, 60], rough: 0.9, layers: { litter: 0.5, moss: 0.3, grass: 0.2 } },
		{ id: "lava", class: SURF.lava, color: [232, 92, 26], rough: 0.6, layers: { stones: 1 } },
		{ id: "lava_crust", class: SURF.lava_crust, color: [30, 25, 23], rough: 0.95, layers: { stones: 1 } },
		{ id: "snow", class: SURF.snow, color: [238, 242, 245], rough: 0.8, layers: { sand: 0.9, stones: 0.1 } },
		{ id: "snow_forest", class: SURF.snow_forest, color: [214, 222, 224], rough: 0.85, layers: { sand: 0.6, litter: 0.4 } },
		{ id: "lake_ice", class: SURF.lake_ice, color: [118, 156, 186], rough: 0.42, layers: { stones: 0.5, sand: 0.5 } },
		{ id: "glacier", class: SURF.glacier, color: [198, 222, 238], rough: 0.3, layers: { sand: 0.8, stones: 0.2 } },
		{ id: "crevasse", class: SURF.crevasse, color: [92, 148, 200], rough: 0.4, layers: { stones: 1 } },
		{ id: "thaw_moss", class: SURF.thaw_moss, color: [104, 106, 72], rough: 0.9, layers: { moss: 0.6, grass: 0.2, soil: 0.2 } },
		{ id: "shingle", class: SURF.shingle, color: [120, 124, 126], rough: 0.85, layers: { stones: 1 } },
		{ id: "ice", class: SURF.ice, color: [128, 174, 208], rough: 0.4, layers: { stones: 0.4, sand: 0.6 } },
		{ id: "steppe", class: SURF.steppe, color: [168, 148, 96], rough: 0.92, layers: { grass: 0.75, soil: 0.25 } },
		{ id: "ash_pale", class: SURF.ash_pale, color: [118, 118, 122], rough: 0.95, layers: { soil: 0.5, stones: 0.25, sand: 0.25 } },
		{ id: "charcoal", class: SURF.charcoal, color: [66, 62, 60], rough: 0.95, layers: { soil: 0.6, litter: 0.4 } },
		{ id: "lichen_rock", class: SURF.lichen_rock, color: [176, 150, 100], rough: 0.85, layers: { stones: 0.8, moss: 0.2 } },
		{ id: "pumice", class: SURF.pumice, color: [112, 109, 104], rough: 0.9, layers: { stones: 0.6, sand: 0.4 } },
		{ id: "moraine", class: SURF.moraine, color: [170, 172, 176], rough: 0.85, layers: { stones: 0.7, sand: 0.3 } },
		{ id: "copper", class: SURF.copper, color: [52, 84, 72], rough: 0.6, layers: { stones: 1 } },
		{ id: "oxide", class: SURF.oxide, color: [116, 66, 40], rough: 0.85, layers: { sand: 0.5, stones: 0.5 } },
		{ id: "snow_crack", class: SURF.snow_crack, color: [168, 184, 202], rough: 0.8, layers: { sand: 0.7, stones: 0.3 } },
		{ id: "moss_crack", class: SURF.moss_crack, color: [86, 90, 62], rough: 0.9, layers: { moss: 0.5, soil: 0.5 } },
	],
	slopeRock: { color: [88, 84, 80], fromDeg: 33, toDeg: 47, ledge: [96, 112, 70] },
	// The Volcanic's lava river (WRRoundIsland draws it, molten): its points with the surface level (m), and its width.
	lava: { width: FF.LAVA_WIDTH, river: lavaLine.filter((p) => Math.hypot(p[0], p[1]) < coastAt(bearingOf(p[0], p[1])) - 10).map((p) => [Math.round(p[0]), Math.round(p[1]), +p[2].toFixed(2)]) },
	lake: { level: LAKE_LEVEL, at: RI.lakeAt.map(Math.round), outline: lakeOutline.map((p) => p.map(Math.round)),
		// The stream's points with its water's level (m): 0.4 m over its bed, down from the lake to the sea.
		stream: (() => { let L = 0; return stream.map((p, i) => { if (i) L += Math.hypot(p[0] - stream[i - 1][0], p[1] - stream[i - 1][1]); const s = L / streamLen; return [Math.round(p[0]), Math.round(p[1]), +(LAKE_LEVEL - 0.6 - (LAKE_LEVEL - 0.6 + 1.2) * Math.pow(s, 0.85) + 0.4).toFixed(2)]; }); })() },
	buildFirst: [[0, 0], ...trunks.map((t) => t.gate.map(Math.round))],
	// Until each land's own camps are built (its slice), everyone starts in Hearthmoot (WRGameMode: world.cityStartTag).
	// Hearthmoot's camps (everyone whose path's land isn't built yet starts there), then each built land's three wake-up camps
	// on its coast (tagged with its biome: WRGameMode starts a new player of its path there), each looking out to sea, the
	// middle one with the starter stations.
	camps: { tag: "hearthmoot", safeRadius: 40, stationsAt: -1, list: [[0, 34, 0], [30, -17, 120], [-30, -17, 240]],
		groups: ["volcanic", "tundra"].map((id) => ({ tag: id, safeRadius: 40, stationsAt: 1, stations: ["forge", "armorer", "alchemy_table", "workbench"], stationsBack: 25,
			list: RI.byId[id].camps.map((c) => { const p = Array.isArray(c) ? c : c.at; return [Math.round(p[0]), Math.round(p[1]), Math.round(Math.atan2(p[1], p[0]) * 180 / Math.PI)]; }) })) },
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
