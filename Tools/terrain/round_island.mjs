// The round island's layout for Raven's sign-off (plan phase 3 "One big world", step 1: "Lay the pie out to scale").
// Nine surface lands round Hearthmoot, each land's share of the island = its share of Aaron's surface sizes
// (Docs/design/cooks/world-map.md 2.2), every land scaled by the same amount (about double the area: Raven's "bigger,
// 10 to 12 km across"). The sky islands over the north, the Crystal caves under the north-east, the Fungal caverns
// under the south-west, the Ocean all round.
// Raven 11-19 ("the pie looks too unnatural"), from the sketches: borders that wander (ridges, rivers, valleys) instead
// of straight spokes; a rugged coast (capes, bays, islets); Hearthmoot in a green meadow with a lake on the Ocean side,
// so no land narrows to a point at the city; neighbouring lands' roads join, four main roads cross the meadow.
// Writes:
//   Tools/terrain/round_island_layout.json   every number in metres (x east, y south, bearings clockwise from north)
//   Docs/design/round_island_map.svg         the map to scale
//   (stdout) the table for the plan doc
//   node Tools/terrain/round_island.mjs [--widget <file>]   --widget also writes the plan doc's drawing module
// Imported (Tools/terrain/round_ground.mjs builds the ground from it), it writes nothing and exports the layout's shapes.
import fs from "fs";
import path from "path";
import { fileURLToPath, pathToFileURL } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

// ---- inputs ------------------------------------------------------------------------------------------------------
// Aaron's surface sizes in km² (world-map.md 2.2, belts B2 to B6). Clockwise from the north-west: the sky over the north
// is thickest over Storm, Desert and Forest; the Crystal caves are under Forest and Tundra; the Fungal caverns are under
// the Graveyard and the Wasteland; the Ocean keeps a shore slice in the south. Opposites face each other across the
// city (Sun–Tide, Nature–Necromancy, Fire–Frost, Lightning–Blood).
const LANDS = [
	{ id: "storm_peaks", name: "Storm peaks", path: "lightning", aaron: 5.5, colour: "#7d86b8" },
	{ id: "desert", name: "Desert", path: "sun", aaron: 6.5, colour: "#d9b46a" },
	{ id: "forest", name: "Ancient forest", path: "nature", aaron: 7.0, colour: "#5f9a4e" },
	{ id: "tundra", name: "Frozen tundra", path: "frost", aaron: 5.0, colour: "#a9cfe3" },
	{ id: "swamp", name: "Swamp", path: "blood", aaron: 4.0, colour: "#7a6a4a" },
	{ id: "ocean", name: "Ocean & reefs", path: "tide", aaron: 3.0, colour: "#4fb0b8" },
	{ id: "graveyard", name: "Graveyard", path: "necromancy", aaron: 2.5, colour: "#8d8a94" },
	{ id: "wasteland", name: "Corrupted wasteland", path: "void", aaron: 6.0, colour: "#8a6a9e" },
	{ id: "volcanic", name: "Volcanic", path: "fire", aaron: 5.0, colour: "#c0603e" },
];
const SCALE_AREA = 2.0;            // every land's area x2 (Raven 11-18: "roughly double the area")
const CITY_WALL = 300;             // Hearthmoot's walls: 600 m across
const MEADOW = 1450;               // the meadow round the walls (the Commons): its mean radius, a slow uneven edge
const MEADOW_WOBBLE = 0.2;         // how uneven the meadow's edge is (fraction of its radius)
const FIRST_DEG = -76.85;          // where the ring starts (Storm's west edge): puts the Storm-Desert-Forest block on north
// The lake in the meadow, on the Ocean side (Raven 11-19), its stream running out through the Ocean shore to the sea.
const LAKE = { bearing: 162, inFromMeadowEdge: 420, along: 480, across: 300, km2: 0.44 };
// The coast: a few slow waves plus capes and bays where a land's look wants one (bearing, width in degrees, amount as a
// fraction of the radius), the same every run.
const WAVES = [[3, 0.022, 0.7], [5, 0.014, 2.1], [8, 0.008, 4.4], [13, 0.004, 1.3]];
const FEATURES = [
	[266.6, 6.0, 0.12, "the volcano cape: the Volcanic juts out to sea"],
	[161.8, 7.5, -0.11, "the reef bay on the Ocean shore, islets across its mouth"],
	[315.3, 3.0, 0.08, "a cliff cape below the Storm peaks, sea stacks off it"],
	[94.2, 6.0, 0.05, "an icy headland on the Tundra"],
	[138.4, 5.0, 0.045, "the Swamp's muddy delta"],
	[355.0, 14.0, 0.03, "the Desert's long dune coast"],
	[61.0, 4.0, -0.05, "a forest inlet"],
];
const COAST_NOISE = 0.06;          // a slow wobble on top, so no stretch of coast is a clean arc
// Islets off the coast: bearing, metres out past the coast, radius.
const ISLETS = [[157.8, 620, 170], [165.8, 860, 120], [161.8, 1120, 90], [152.8, 980, 70], [319.3, 380, 90], [322.3, 560, 60], [253.6, 760, 150], [105.2, 480, 80], [341.0, 520, 70]];
// Borders that wander (Raven 11-19): each place is moved sideways by a slow, smooth amount, so borders bend along
// ridges, rivers and valleys; the build blends the two lands over BLEND metres either side of a border.
const WARP = 950, WARP_SCALE = 2600, BLEND = 200;
// Roads that join (Raven 11-19): each land's road runs in from its middle camp to a crossroads shared with its
// neighbours, in the inland band; four main roads cross the meadow to the city's four gates.
const GROUPS = [["storm_peaks", "desert"], ["forest", "tundra"], ["swamp", "ocean"], ["graveyard", "wasteland", "volcanic"]];
const CROSSROADS_OUT = 700;        // a crossroads sits this far out from the meadow's edge (in the inland band)
const ROAD_TURN = 0.3;             // a land's road runs down its own middle until this deep, then turns for the crossroads
// Slices 2 and 3 (11-19): the Volcanic's and the Tundra's own content moved from the Old Island (Tools/terrain/
// fire_frost.mjs) sets their roads and their majors. The Volcanic's road goes round the south of the cape's volcano,
// over the lava river on the Fallen Smith's hammer; the Tundra's keeps north of the Great Frozen Lake (the belt of
// old content between Skaldrith's bowl and the lake is too full for a road).
// Their majors sit where the Old Island had them: Vulkarn in the volcano's crater, Skaldrith in his glacier bowl.
const ROAD_VIA = {
	volcanic: [[-5760, 640], [-5420, 860], [-5050, 845], [-4760, 905], [-4640, 1085], [-4100, 1220], [-3100, 1350], [-2200, 1520]],
	tundra: [[5060, 590], [4700, 690], [4250, 700], [3600, 430], [2900, -160]],
};
const BOSS_AT = { volcanic: [-5212, 310], tundra: [4425, 232] };   // fire_frost.mjs: Vulkarn's crater and Skaldrith's bowl, moved
const SHALLOWS = 300, REEFS = 1200;  // off the coast: calm shallows by the camps, reefs, then deep water
const MAP_HALF = 8000;             // the level's square: 16 km, the deep sea round the island
const CELL = 40;                   // the grid that measures every land's area

// ---- noise and geometry --------------------------------------------------------------------------------------------
const RAD = Math.PI / 180;
function hsh(ix, iy, s) { let n = (Math.imul(ix, 374761393) + Math.imul(iy, 668265263) + Math.imul(s, 1442695041)) | 0; n = Math.imul(n ^ (n >>> 13), 1274126177); n ^= n >>> 16; return (n >>> 0) / 4294967296; }
function vn(x, y, s) {
	const ix = Math.floor(x), iy = Math.floor(y), fx = x - ix, fy = y - iy, u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
	const a = hsh(ix, iy, s), b = hsh(ix + 1, iy, s), c = hsh(ix, iy + 1, s), d = hsh(ix + 1, iy + 1, s);
	return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
function fbm(x, y, s, o) { let t = 0, a = 1, f = 1, n = 0; for (let i = 0; i < o; i++) { t += a * (vn(x * f, y * f, s + i * 101) - 0.5); n += a; a *= 0.5; f *= 2.03; } return 2 * t / n; }
const norm = (d) => ((d % 360) + 360) % 360;
const dAng = (a, b) => norm(a - b + 180) - 180;
const bearingOf = (x, y) => norm(Math.atan2(x, -y) / RAD);
const pt = (deg, r) => [r * Math.sin(deg * RAD), -r * Math.cos(deg * RAD)];
const xy = (deg, r) => pt(deg, r).map(Math.round);
const rnd = (p) => p.map(Math.round);
const gauss = (deg, at, w) => Math.exp(-((dAng(deg, at) / w) ** 2));
const ring = (deg, k, s) => fbm(Math.cos(deg * RAD) * k + 20, Math.sin(deg * RAD) * k + 20, s, 4);
const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);

// ---- the coast and the meadow ---------------------------------------------------------------------------------------
const coastShape = (deg) => 1 + WAVES.reduce((s, [k, a, p]) => s + a * Math.sin(k * deg * RAD + p), 0) + COAST_NOISE * ring(deg, 2.5, 7)
	+ FEATURES.reduce((s, [at, w, amt]) => s + amt * gauss(deg, at, w), 0);
const meadowR = (deg) => MEADOW * (1 + MEADOW_WOBBLE * ring(deg, 1.6, 13));
const totalAaron = LANDS.reduce((s, l) => s + l.aaron, 0);
const landTarget = totalAaron * SCALE_AREA * 1e6;  // m²
// ∫ ½ (R0² s(θ)² − meadow(θ)²) dθ over the circle = landTarget  →  R0
let s2 = 0, m2 = 0;
{ const n = 7200, dt = 2 * Math.PI / n; for (let i = 0; i < n; i++) { const d = i * 360 / n; s2 += 0.5 * coastShape(d) ** 2 * dt; m2 += 0.5 * meadowR(d) ** 2 * dt; } }
const R0 = Math.sqrt((landTarget + m2) / s2);
const coastR = (deg) => R0 * coastShape(deg);
const depth = (deg, f) => meadowR(deg) + (coastR(deg) - meadowR(deg)) * f;      // f: 0 at the meadow's edge, 1 at the coast
const depthAt = ([x, y]) => { const d = bearingOf(x, y); return (Math.hypot(x, y) - meadowR(d)) / (coastR(d) - meadowR(d)); };
const BANDS = [{ name: "inland (hard)", from: 0, to: 1 / 3 }, { name: "midlands", from: 1 / 3, to: 2 / 3 }, { name: "coast (easy)", from: 2 / 3, to: 1 }];

// ---- the wandering borders, fitted so every land keeps its size ---------------------------------------------------
const warpDeg = (x, y, r) => WARP * fbm(x / WARP_SCALE + 3.1, y / WARP_SCALE + 7.7, 11, 4) / Math.max(r, 1000) / RAD;
// The point at distance r whose moved bearing is `deg` (a land's middle line, or a border when deg is an edge).
const along = (deg, r) => { let th = deg; for (let j = 0; j < 10; j++) { const [x, y] = pt(th, r); th = deg - warpDeg(x, y, r); } return pt(th, r); };
let spans = LANDS.map((l) => l.aaron / totalAaron * 360);
const edgeOf = (i) => FIRST_DEG + spans.slice(0, i).reduce((s, v) => s + v, 0);
const sliceOf = (deg) => { const d = norm(deg - FIRST_DEG); let acc = 0; for (let i = 0; i < LANDS.length; i++) { acc += spans[i]; if (d < acc) return i; } return LANDS.length - 1; };
const ownerAt = ([x, y]) => { const r = Math.hypot(x, y), d = bearingOf(x, y); if (r < meadowR(d) || r > coastR(d)) return -1; return sliceOf(d + warpDeg(x, y, r)); };
const cells = [];
for (let y = -MAP_HALF + CELL / 2; y < MAP_HALF; y += CELL) for (let x = -MAP_HALF + CELL / 2; x < MAP_HALF; x += CELL) {
	const r = Math.hypot(x, y), d = bearingOf(x, y);
	if (r < meadowR(d) || r > coastR(d)) continue;
	cells.push({ x, y, wd: d + warpDeg(x, y, r), f: (r - meadowR(d)) / (coastR(d) - meadowR(d)) });
}
const areas = () => { const a = LANDS.map(() => 0); for (const c of cells) a[sliceOf(c.wd)] += CELL * CELL; return a; };
const cellTotal = cells.length * CELL * CELL;
for (let it = 0; it < 16; it++) {
	const a = areas();
	spans = spans.map((s, i) => s * Math.pow(LANDS[i].aaron / totalAaron * cellTotal / Math.max(a[i], 1), 0.8));
	const k = 360 / spans.reduce((s, v) => s + v, 0); spans = spans.map((s) => s * k);
}
// Each border as a line from the meadow's edge to the coast.
const borderLine = (edgeDeg) => {
	const o = [];
	for (let r = 900; r < MAP_HALF; r += 25) { const p = along(edgeDeg, r), d = bearingOf(...p); if (r >= meadowR(d) && r <= coastR(d)) o.push(p); else if (o.length) break; }
	return o;
};
const borders = LANDS.map((_, i) => borderLine(edgeOf(i)));

// ---- each land ---------------------------------------------------------------------------------------------------------
const lands = LANDS.map((land, i) => {
	const from = edgeOf(i), to = from + spans[i], mid = from + spans[i] / 2;
	const inB = borders[i], outB = borders[(i + 1) % LANDS.length];
	let cFrom = bearingOf(...inB[inB.length - 1]), cTo = bearingOf(...outB[outB.length - 1]); while (cTo < cFrom) cTo += 360;
	let mFrom = bearingOf(...inB[0]), mTo = bearingOf(...outB[0]); while (mTo < mFrom) mTo += 360;
	const polygon = [...inB];
	for (let d = cFrom + 1; d < cTo; d += 1) polygon.push(pt(d, coastR(d)));
	polygon.push(...outB.slice().reverse());
	for (let d = mTo - 2; d > mFrom; d -= 2) polygon.push(pt(d, meadowR(d)));
	const polyArea = Math.abs(polygon.reduce((s, p, j) => { const q = polygon[(j + 1) % polygon.length]; return s + p[0] * q[1] - q[0] * p[1]; }, 0)) / 2;
	let coastLen = 0; for (let d = cFrom; d < cTo; d += 0.1) coastLen += coastR(d) * 0.1 * RAD;
	return { ...land, i, from, to, mid, spanDeg: spans[i], coastFrom: cFrom, coastTo: cTo, polygon, polyArea, coastLen };
});
{
	const a = areas(), sx = LANDS.map(() => 0), sy = LANDS.map(() => 0), bandA = LANDS.map(() => [0, 0, 0]);
	for (const c of cells) { const o = sliceOf(c.wd); sx[o] += c.x; sy[o] += c.y; bandA[o][Math.min(2, Math.floor(c.f * 3))] += CELL * CELL; }
	lands.forEach((s, i) => { s.area = a[i]; s.label = [sx[i] / (a[i] / CELL / CELL), sy[i] / (a[i] / CELL / CELL)]; s.bandArea = bandA[i]; });
}
const byId = Object.fromEntries(lands.map((s) => [s.id, s]));
// Three wake-up camps on each land's coast (your path picks the land you wake in, at one of three camps).
// The middle camp sits on the land's middle line, where its road starts.
for (const s of lands) {
	s.camps = [0.22, 0.5, 0.78].map((f) => { const d = s.coastFrom + (s.coastTo - s.coastFrom) * f; return pt(d, coastR(d) - 220); });
	let r = coastR(norm(s.mid)) - 220; for (let j = 0; j < 4; j++) r = coastR(bearingOf(...along(s.mid, r))) - 220;
	s.camps[1] = along(s.mid, r);
}

// ---- the lake in the meadow and its stream ------------------------------------------------------------------------
const lakeAt = pt(LAKE.bearing, meadowR(LAKE.bearing) - LAKE.inFromMeadowEdge);
const lakeOutline = [];
{
	const t = LAKE.bearing * RAD, tan = [Math.cos(t), Math.sin(t)], rad = [Math.sin(t), -Math.cos(t)];
	// A natural shore (Raven 11-19's quality pass): bays and points on the oval, then scaled back to LAKE.km2.
	const local = [];
	for (let a = 0; a < 360; a += 3) {
		const c = Math.cos(a * RAD), s = Math.sin(a * RAD);
		const rr = 1 / Math.sqrt((c / LAKE.along) ** 2 + (s / LAKE.across) ** 2) * (1 + 0.24 * fbm(c * 1.1 + 60, s * 1.1 + 60, 21, 3) + 0.07 * fbm(c * 3.4 + 90, s * 3.4 + 90, 23, 2));
		local.push([rr * c, rr * s]);
	}
	const area0 = Math.abs(local.reduce((t, p, j) => { const q = local[(j + 1) % local.length]; return t + p[0] * q[1] - q[0] * p[1]; }, 0)) / 2;
	const k = Math.sqrt(LAKE.km2 * 1e6 / area0);
	for (const [u, v] of local) lakeOutline.push([lakeAt[0] + k * (u * tan[0] + v * rad[0]), lakeAt[1] + k * (u * tan[1] + v * rad[1])]);
}
{
}
const inPoly = (p, poly) => { let ins = false; for (let j = 0, k = poly.length - 1; j < poly.length; k = j++) { const [xi, yi] = poly[j], [xk, yk] = poly[k]; if ((yi > p[1]) !== (yk > p[1]) && p[0] < (xk - xi) * (p[1] - yi) / (yk - yi) + xi) ins = !ins; } return ins; };
const lakeArea = Math.abs(lakeOutline.reduce((s, p, j) => { const q = lakeOutline[(j + 1) % lakeOutline.length]; return s + p[0] * q[1] - q[0] * p[1]; }, 0)) / 2;
const lakeOk = lakeOutline.every((p) => Math.hypot(...p) < meadowR(bearingOf(...p)) - 60 && Math.hypot(...p) > CITY_WALL + 150);
// The stream: out of the lake's sea side, through the Ocean shore, into the reef bay; it bends as it goes.
const stream = [];
{
	const r0 = Math.hypot(...lakeAt) + LAKE.across * 0.9;
	for (let r = r0; ; r += 40) { const d = LAKE.bearing + 4 * Math.sin((r - r0) / 650) + 2.5 * fbm(r / 900, 3.3, 29, 2); stream.push(pt(d, r)); if (r > coastR(d) + 60) break; }
}
let streamLen = 0; for (let j = 1; j < stream.length; j++) streamLen += dist(stream[j - 1], stream[j]);

// ---- roads that join ---------------------------------------------------------------------------------------------------
const bez = (p0, p1, p2, n = 40) => Array.from({ length: n + 1 }, (_, j) => { const t = j / n, a = (1 - t) ** 2, b = 2 * (1 - t) * t, c = t * t; return [a * p0[0] + b * p1[0] + c * p2[0], a * p0[1] + b * p1[1] + c * p2[1]]; });
const lakeHalfSpan = Math.atan2(LAKE.along, Math.hypot(...lakeAt)) / RAD + 12;
const trunks = GROUPS.map((ids) => {
	const ls = ids.map((id) => byId[id]);
	let sx = 0, sy = 0; ls.forEach((s) => { sx += Math.sin(s.mid * RAD); sy += Math.cos(s.mid * RAD); });
	let ga = norm(Math.atan2(sx, sy) / RAD);
	// A main road never crosses the lake: one that would is moved round its nearer side.
	const off = dAng(ga, LAKE.bearing); if (Math.abs(off) < lakeHalfSpan) ga = norm(LAKE.bearing + Math.sign(off || -1) * lakeHalfSpan);
	const fortR = meadowR(ga), jr = fortR + CROSSROADS_OUT, crossroads = pt(ga, jr), fort = pt(ga, fortR + 40), gate = pt(ga, CITY_WALL);
	const points = [...bez(crossroads, pt(ga + 5, (jr + fortR) / 2), fort, 20), ...bez(fort, pt(ga, (fortR + CITY_WALL) / 2), gate, 12).slice(1)];
	for (const s of ls) {
		if (ROAD_VIA[s.id]) {
			// Through its waypoints: a Catmull-Rom curve from the middle camp to the crossroads, a point every ~40 m.
			const K = [s.camps[1], ...ROAD_VIA[s.id], crossroads], out = [K[0]];
			for (let k = 0; k + 1 < K.length; k++) {
				const p0 = K[Math.max(0, k - 1)], p1 = K[k], p2 = K[k + 1], p3 = K[Math.min(K.length - 1, k + 2)], n = Math.max(2, Math.round(dist(p1, p2) / 40));
				for (let j = 1; j <= n; j++) {
					const t = j / n, t2 = t * t, t3 = t2 * t;
					out.push([0, 1].map((c) => 0.5 * (2 * p1[c] + (-p0[c] + p2[c]) * t + (2 * p0[c] - 5 * p1[c] + 4 * p2[c] - p3[c]) * t2 + (-p0[c] + 3 * p1[c] - 3 * p2[c] + p3[c]) * t3)));
				}
			}
			s.road = out;
			s.roadLen = s.road.reduce((t, p, j) => j ? t + dist(s.road[j - 1], p) : 0, 0);
			s.trunk = ids;
			continue;
		}
		const road = [];
		for (let r = Math.hypot(...s.camps[1]); r > jr; r -= 50) { const p = along(s.mid, r); road.push(p); if (depthAt(p) <= ROAD_TURN) break; }
		const last = road[road.length - 1], lr = Math.hypot(...last);
		s.road = [...road, ...bez(last, along(s.mid, lr - 0.6 * (lr - jr)), crossroads, 20).slice(1)];
		s.roadLen = s.road.reduce((t, p, j) => j ? t + dist(s.road[j - 1], p) : 0, 0);
		s.trunk = ids;
	}
	return { lands: ids, bearing: ga, crossroads, fort, gate, points };
});
const allRoads = [...lands.map((s) => s.road), ...trunks.map((t) => t.points)];
const roadsInLake = allRoads.flat().filter((p) => inPoly(p, lakeOutline)).length;
// On each land's own road, its old pass's works (Raven 11-18): the mouth shrine where it leaves the coast band, the
// champion where it enters the inland band; the forts now stand on the main roads at the meadow's edge, one per road.
for (const s of lands) {
	const firstBelow = (f) => s.road.findIndex((p) => depthAt(p) <= f);
	s.mouthShrine = s.road[firstBelow(2 / 3)];
	s.champion = s.road[Math.min(s.road.length - 1, firstBelow(1 / 3) + 2)];
	// On a road that bends near a border, each work goes to its own land's stretch nearest its band's depth, at least
	// OWN_CLEAR m inside its own land all round (not on the border's blend).
	const OWN_CLEAR = 80;
	const deepOwn = (q) => [0, 40, OWN_CLEAR].every((rr) => Array.from({ length: 12 }, (_, k) => k * Math.PI / 6).every((t) => ownerAt([q[0] + rr * Math.cos(t), q[1] + rr * Math.sin(t)]) === s.i));
	const ownNear = (f, p) => { if (deepOwn(p)) return p; const own = s.road.filter(deepOwn); return own.reduce((b, q) => Math.abs(depthAt(q) - f) < Math.abs(depthAt(b) - f) ? q : b, own[0]); };
	s.mouthShrine = ownNear(2 / 3, s.mouthShrine);
	s.champion = ownNear(1 / 3, s.champion);
	s.roadOwn = s.road.filter((p) => ownerAt(p) === s.i).length / s.road.length;
	s.worksOwn = ownerAt(s.mouthShrine) === s.i && ownerAt(s.champion) === s.i;
}
// The major boss off the roads (Raven: bosses off the roads), deep inland: the spot in the land's inland band farthest
// from every road.
const nearestRoad = (p) => Math.min(...allRoads.flat().map((q) => dist(p, q)));
// It also keeps clear of its land's borders and of the other lairs, so no two bosses sit across a border from each other.
const placed = [];
for (const s of lands) {
	const edges = [...borders[s.i], ...borders[(s.i + 1) % LANDS.length]];
	let best = null;
	for (let k = -0.4; k <= 0.4001; k += 0.04) for (const f of [0.12, 0.17, 0.22, 0.27, 0.32]) {
		const d = s.mid + k * s.spanDeg, r = depth(norm(d), f), p = along(d, r);
		if (ownerAt(p) !== s.i) continue;
		const road = nearestRoad(p), edge = Math.min(...edges.map((q) => dist(p, q))), other = placed.length ? Math.min(...placed.map((q) => dist(p, q))) : 1e9;
		const score = Math.min(road, 1.5 * edge, 0.45 * other); if (!best || score > best.score) best = { p, score, road, edge };
	}
	s.bossLair = best.p; s.bossFromRoad = best.road; s.bossFromBorder = best.edge;
	if (BOSS_AT[s.id]) { s.bossLair = BOSS_AT[s.id]; s.bossFromRoad = nearestRoad(s.bossLair); s.bossFromBorder = Math.min(...edges.map((q) => dist(s.bossLair, q))); }
	placed.push(s.bossLair);
}
const campsOk = lands.every((s) => s.camps.every((c) => ownerAt(c) === s.i));

// ---- the layers ---------------------------------------------------------------------------------------------------
// Each footprint is an annular sector sized to Aaron's area x2: the sky islands 2.0 km² of islands in ~5 km² of air,
// the Fungal caverns ~3 km² below, the Crystal caves ~2 km² below (world-map.md 2.2).
const sector = (from, to, r1, areaKm2) => {
	const th = (norm(to - from) || 360) * RAD;
	const r2 = Math.sqrt(2 * areaKm2 * 1e6 / th + r1 * r1);
	return { from: +norm(from).toFixed(1), to: +norm(to).toFixed(1), r1: Math.round(r1), r2: Math.round(r2), areaKm2 };
};
const sky = sector(byId.storm_peaks.mid, byId.forest.mid, MEADOW + 450, 5 * SCALE_AREA);
sky.islandsKm2 = 2.0 * SCALE_AREA;
sky.height = [500, 1300];
sky.note = "thickest over Storm peaks, the Desert and the Forest; a few more over other lands";
// The caverns (Raven 11-19: "make the cavern layers larger"): twice what they were (Aaron's 2 and 3 km² x4), each an
// uneven footprint with rounded ends rather than a straight-edged slice, kept 400 m in from the coast.
const CRYSTAL_KM2 = 8, FUNGAL_KM2 = 12;
const polyArea = (o) => Math.abs(o.reduce((s, p, j) => { const q = o[(j + 1) % o.length]; return s + p[0] * q[1] - q[0] * p[1]; }, 0)) / 2;
const cavern = (from, to, r1, km2, seed) => {
	if (to < from) to += 360;
	const shape = (r2) => {
		const outerR = (d) => Math.min(r2 * (1 + 0.08 * ring(d, 2.2, seed)), coastR(norm(d)) - 400);
		const innerR = (d) => r1 * (1 + 0.08 * ring(d, 2.2, seed + 1));
		const o = [];
		for (let d = from; d <= to + 1e-6; d += 1) o.push(pt(d, outerR(d)));
		for (let t = 0.1; t < 0.95; t += 0.1) { const r = outerR(to) + (innerR(to) - outerR(to)) * t; o.push(pt(to + 6 * Math.sin(Math.PI * t) * 2000 / r, r)); }
		for (let d = to; d >= from - 1e-6; d -= 2) o.push(pt(d, innerR(d)));
		for (let t = 0.1; t < 0.95; t += 0.1) { const r = innerR(from) + (outerR(from) - innerR(from)) * t; o.push(pt(from - 6 * Math.sin(Math.PI * t) * 2000 / r, r)); }
		return o;
	};
	let lo = r1 + 100, hi = 7000;
	for (let i = 0; i < 40; i++) { const m = (lo + hi) / 2; if (polyArea(shape(m)) < km2 * 1e6) lo = m; else hi = m; }
	const outline = shape(lo);
	return { from: +norm(from).toFixed(1), to: +norm(to).toFixed(1), r1: Math.round(r1), r2: Math.round(lo), areaKm2: +(polyArea(outline) / 1e6).toFixed(2), outline: outline.map(rnd) };
};
const crystal = cavern(byId.forest.from + 6, byId.tundra.to - 4, MEADOW + 300, CRYSTAL_KM2, 41);
crystal.note = "under the Forest and the Tundra; deeper halls hold harder creatures and better ore";
const fungal = cavern(byId.graveyard.from - 2, byId.wasteland.to + 2, MEADOW + 300, FUNGAL_KM2, 43);
fungal.note = "under the Graveyard and the Wasteland, across the island from the sky (Wind and Spore are opposites)";
// The Severance (the old Void–Arcane pass) becomes the way down into the Crystal caves on the Forest–Tundra border; the
// Flue (the 740 m shaft) rises from the Fungal floor to the south-west surface, with one sky island anchored over it.
const onBorder = (line, f) => line.reduce((b, p) => Math.abs(depthAt(p) - f) < Math.abs(depthAt(b) - f) ? p : b, line[0]);
const flueDeg = byId.wasteland.from + 12;
const starts = {
	wind: { at: xy(byId.desert.coastFrom, coastR(byId.desert.coastFrom) - 700), height: 350, note: "a low starter sky island over the north coast: ropes, ledges and updrafts down" },
	spore: { at: rnd(along(byId.wasteland.from + 10, coastR(norm(byId.wasteland.coastFrom + 10)) - 900)), depth: 60, note: "a shallow cavern under the south-west coast; no markings out" },
	arcane: { at: rnd(along(byId.tundra.from + 12, coastR(norm(byId.tundra.coastFrom + 12)) - 900)), depth: 60, note: "a shallow crystal hall under the north-east coast; no markings out" },
};
const layers = {
	sky, crystal, fungal,
	severance: { at: rnd(onBorder(borders[byId.tundra.i], 0.5)), note: "a rift-cut gorge down into the Crystal caves, on the Forest–Tundra border" },
	flue: { at: rnd(along(flueDeg, depth(norm(flueDeg), 0.3))), note: "the shaft up from the Fungal caverns, a sky island anchored over it" },
	starts,
};

// ---- Hearthmoot: 12 quarters, one per path, in the ring's order; four gates where the main roads arrive -----------
// The three layer paths take the quarters between the lands their layer lies under or over.
const quarterOrder = ["lightning", "wind", "sun", "nature", "arcane", "frost", "blood", "tide", "necromancy", "spore", "void", "fire"];
const quarters = quarterOrder.map((p, i) => ({ path: p, from: norm(FIRST_DEG + i * 30), to: norm(FIRST_DEG + (i + 1) * 30) }));
let meadowArea = 0; { const n = 3600; for (let i = 0; i < n; i++) meadowArea += 0.5 * meadowR(i * 360 / n) ** 2 * 2 * Math.PI / n; }

const islets = ISLETS.map(([d, out, size]) => { const at = pt(d, coastR(d) + out); return { at: rnd(at), radius: size, land: LANDS[sliceOf(d + warpDeg(at[0], at[1], Math.hypot(...at)))].id }; });
export {
	LANDS, GROUPS, FIRST_DEG, SCALE_AREA, CITY_WALL, MEADOW, LAKE, FEATURES, SHALLOWS, REEFS, MAP_HALF, BLEND, RAD,
	hsh, vn, fbm, norm, dAng, bearingOf, pt, gauss, ring, dist, inPoly,
	coastShape, coastR, meadowR, depth, depthAt, warpDeg, along, edgeOf, sliceOf, ownerAt, R0,
	lands, byId, borders, lakeAt, lakeOutline, stream, trunks, layers, islets, quarters,
};
const MAIN = process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href;
if (MAIN) {
// ---- out ------------------------------------------------------------------------------------------------------------
const coast = []; for (let d = 0; d < 360; d += 1) coast.push(xy(d, coastR(d)));
const meadowEdge = []; for (let d = 0; d < 360; d += 2) meadowEdge.push(xy(d, meadowR(d)));
const extent = Math.max(...coast.map((p) => Math.hypot(...p)));
const layout = {
	_comment: "The round island's layout (plan phase 3 step 1, for Raven's sign-off). Made by Tools/terrain/round_island.mjs: change that, not this. Metres; x east, y south; bearings in degrees clockwise from north. Land areas = Aaron's surface sizes (world-map.md 2.2) x " + SCALE_AREA + ". Borders wander and blend over blendMetres either side; a land's polygon is its exact shape before the blend.",
	coastRadius: { mean: Math.round(R0), min: Math.round(Math.min(...coast.map((p) => Math.hypot(...p)))), max: Math.round(extent) },
	across: Math.round(2 * R0),
	landKm2: +(landTarget / 1e6).toFixed(1),
	blendMetres: BLEND,
	hearthmoot: {
		wallRadius: CITY_WALL, quarters,
		gates: trunks.map((t) => ({ bearing: +t.bearing.toFixed(1), at: rnd(t.gate), for: t.lands })),
		meadow: { meanRadius: MEADOW, km2: +(meadowArea / 1e6).toFixed(2), edge: meadowEdge, note: "the Commons: a green meadow round the walls where the main roads arrive; guarded, no wild creatures, nothing owned (Raven's answers)" },
		lake: { at: rnd(lakeAt), km2: +(lakeArea / 1e6).toFixed(2), outline: lakeOutline.map(rnd), stream: stream.map(rnd), streamKm: +(streamLen / 1000).toFixed(2), note: "in the meadow on the Ocean side; its stream runs out through the Ocean shore into the reef bay" },
	},
	bands: BANDS.map((b) => ({ name: b.name, fromFraction: +b.from.toFixed(3), toFraction: +b.to.toFixed(3), note: "fractions of the way from the meadow's edge (0) to the coast (1)" })),
	sea: { shallows: SHALLOWS, reefs: REEFS, mapHalf: MAP_HALF, thalassor: xy(byId.ocean.mid, coastR(norm(byId.ocean.mid)) + 1500), features: FEATURES.map(([b, w, a, note]) => ({ bearing: b, note })) },
	roads: trunks.map((t) => ({ lands: t.lands, bearing: +t.bearing.toFixed(1), crossroads: rnd(t.crossroads), fort: rnd(t.fort), gate: rnd(t.gate), points: t.points.map(rnd) })),
	lands: lands.map((s) => ({
		id: s.id, name: s.name, path: s.path, aaronKm2: s.aaron, km2: +(s.area / 1e6).toFixed(2),
		centreDeg: +norm(s.mid).toFixed(2), coastFromDeg: +norm(s.coastFrom).toFixed(2), coastToDeg: +norm(s.coastTo).toFixed(2),
		coastKm: +(s.coastLen / 1000).toFixed(2), bandKm2: s.bandArea.map((a) => +(a / 1e6).toFixed(2)),
		camps: s.camps.map(rnd), road: s.road.map(rnd), roadKm: +(s.roadLen / 1000).toFixed(2), mainRoad: s.trunk,
		champion: rnd(s.champion), mouthShrine: rnd(s.mouthShrine), bossLair: rnd(s.bossLair),
		polygon: s.polygon.map(rnd),
	})),
	borders: lands.map((s, i) => ({ between: [LANDS[(i + LANDS.length - 1) % LANDS.length].id, s.id], points: borders[i].filter((_, j) => j % 2 === 0 || j === borders[i].length - 1).map(rnd) })),
	islets,
	layers,
	coast,
};
fs.writeFileSync(path.join(root, "Tools/terrain/round_island_layout.json"), JSON.stringify(layout));

// ---- the map ----------------------------------------------------------------------------------------------------------
// One SVG for the repo (Docs/design) and, with --widget, the same drawing as the plan doc's module (every label typed
// out with its own id, as the doc's drawings need).
const W = 760, PAD = 20, VIEW = 7100;               // the drawing shows ±7.1 km
const sc = (W - 2 * PAD) / (2 * VIEW);
const P = ([x, y]) => [Math.round(W / 2 + x * sc), Math.round(PAD + VIEW * sc + y * sc) + 40];
// Lines are thinned to what shows at this scale (no point more than half a pixel off), so the drawing stays small.
const thin = (pts, tol = 0.5) => {
	if (pts.length < 3) return pts;
	const keep = new Uint8Array(pts.length); keep[0] = keep[pts.length - 1] = 1;
	const stack = [[0, pts.length - 1]];
	while (stack.length) {
		const [i, j] = stack.pop(), [ax, ay] = pts[i], [bx, by] = pts[j], L = Math.hypot(bx - ax, by - ay);
		let far = -1, fd = tol;
		for (let k = i + 1; k < j; k++) { const d = L < 1e-6 ? Math.hypot(pts[k][0] - ax, pts[k][1] - ay) : Math.abs((bx - ax) * (ay - pts[k][1]) - (ax - pts[k][0]) * (by - ay)) / L; if (d > fd) { fd = d; far = k; } }
		if (far >= 0) { keep[far] = 1; stack.push([i, far], [far, j]); }
	}
	return pts.filter((_, k) => keep[k]);
};
const pathOf = (pts, close = true) => { const px = thin(pts.map((p) => [W / 2 + p[0] * sc, PAD + VIEW * sc + p[1] * sc + 40])); let o = "", last = ""; for (const p of px) { const s = Math.round(p[0]) + "," + Math.round(p[1]); if (s !== last) o += (o ? "L" : "M") + s; last = s; } return o + (close ? "Z" : ""); };
const ringPts = (f, from = 0, to = 360, stepDeg = 1) => { const o = []; for (let d = from; d <= to + 1e-6; d += stepDeg) o.push(pt(d, f(norm(d)))); return o; };
const sectorPath = (L) => {
	const to = L.to < L.from ? L.to + 360 : L.to;
	return pathOf([...ringPts(() => L.r2, L.from, to, 3), ...ringPts(() => L.r1, L.from, to, 3).reverse()]);
};
const H = Math.round(40 + PAD * 2 + 2 * VIEW * sc + 120);
const shapes = [];
shapes.push(`<rect x='0' y='40' width='${W}' height='${Math.round(2 * PAD + 2 * VIEW * sc)}' fill='#2f6f8f' opacity='0.18'/>`);
shapes.push(`<path d='${pathOf(ringPts((d) => coastR(d) + REEFS, 0, 360, 2))}' fill='#4fb0b8' opacity='0.18'/>`);
shapes.push(`<path d='${pathOf(ringPts((d) => coastR(d) + SHALLOWS, 0, 360, 2))}' fill='#7fd0d0' opacity='0.35'/>`);
for (const s of islets) { const pts = []; for (let a = 0; a < 360; a += 30) pts.push([s.at[0] + s.radius * (1 + 0.3 * Math.sin(a * 0.07 * s.radius)) * Math.sin(a * RAD), s.at[1] - s.radius * (1 + 0.3 * Math.sin(a * 0.07 * s.radius)) * Math.cos(a * RAD)]); shapes.push(`<path d='${pathOf(pts)}' fill='${byId[s.land].colour}' stroke='#000000' strokeOpacity='0.4'/>`); }
for (const s of lands) shapes.push(`<g data-claude-anchor='land-${s.id}'><path d='${pathOf(s.polygon)}' fill='${s.colour}' opacity='0.85' stroke='#ffffff' strokeWidth='1' strokeOpacity='0.7'/></g>`);
shapes.push(`<path d='${pathOf(ringPts(coastR, 0, 360, 1))}' fill='none' stroke='#1a3a4a' strokeWidth='1.2' strokeOpacity='0.6'/>`);
for (const f of [1 / 3, 2 / 3]) shapes.push(`<path d='${pathOf(ringPts((d) => depth(d, f), 0, 360, 3))}' fill='none' stroke='#000000' strokeOpacity='0.35' strokeDasharray='4 4'/>`);
shapes.push(`<g data-claude-anchor='meadow'><path d='${pathOf(ringPts(meadowR, 0, 360, 2))}' fill='#a8d080' stroke='#4f7a2a' strokeWidth='1.2'/><path d='${pathOf(lakeOutline)}' fill='#5aa9c9' stroke='#2f6f8f'/></g>`);
shapes.push(`<path d='${pathOf(stream, false)}' fill='none' stroke='#3f8fbf' strokeWidth='2'/>`);
shapes.push(`<g data-claude-anchor='layer-sky'><path d='${sectorPath(sky)}' fill='none' stroke='#ffffff' strokeWidth='2.2' strokeDasharray='7 4'/></g>`);
shapes.push(`<g data-claude-anchor='layer-crystal'><path d='${pathOf(crystal.outline)}' fill='#b06fe0' fillOpacity='0.18' stroke='#7a3fb0' strokeWidth='2' strokeDasharray='2 3'/></g>`);
shapes.push(`<g data-claude-anchor='layer-fungal'><path d='${pathOf(fungal.outline)}' fill='#3fb08a' fillOpacity='0.18' stroke='#1f7a5a' strokeWidth='2' strokeDasharray='2 3'/></g>`);
for (const s of lands) shapes.push(`<path d='${pathOf(s.road, false)}' fill='none' stroke='#3a2a10' strokeWidth='1.5'/>`);
for (const t of trunks) shapes.push(`<path d='${pathOf(t.points, false)}' fill='none' stroke='#3a2a10' strokeWidth='3'/>`);
shapes.push(`<g data-claude-anchor='hearthmoot'><circle cx='${P([0, 0])[0]}' cy='${P([0, 0])[1]}' r='${(CITY_WALL * sc).toFixed(1)}' fill='#c9b27a' stroke='#5a4a20' strokeWidth='2'/></g>`);
for (const s of lands) {
	for (const c of s.camps) { const [x, y] = P(c); shapes.push(`<circle cx='${x}' cy='${y}' r='3.2' fill='#ffffff' stroke='#000000' strokeWidth='0.8'/>`); }
	const [bx, by] = P(s.bossLair); shapes.push(`<path d='M${bx},${by - 5}L${bx + 5},${by + 4}L${bx - 5},${by + 4}Z' fill='#b02020' stroke='#ffffff' strokeWidth='0.8'/>`);
}
for (const t of trunks) { const [fx, fy] = P(t.fort); shapes.push(`<rect x='${fx - 3}' y='${fy - 3}' width='6' height='6' fill='#3a2a10' stroke='#ffffff' strokeWidth='0.8'/>`); }
for (const k of ["severance", "flue"]) { const [x, y] = P(layers[k].at); shapes.push(`<circle cx='${x}' cy='${y}' r='4.5' fill='none' stroke='#000000' strokeWidth='1.6'/>`); }
for (const k of ["wind", "spore", "arcane"]) { const [x, y] = P(layers.starts[k].at); shapes.push(`<path d='M${x},${y - 6}L${x + 6},${y}L${x},${y + 6}L${x - 6},${y}Z' fill='#ffd84a' stroke='#000000' strokeWidth='0.8'/>`); }
{ const [x, y] = P(layout.sea.thalassor); shapes.push(`<circle cx='${x}' cy='${y}' r='5' fill='#1f4f8f' stroke='#ffffff'/>`); }
// Scale bar (2 km) and north arrow, bottom left / top right.
const sbY = Math.round(40 + 2 * PAD + 2 * VIEW * sc - 14);
shapes.push(`<line x1='${PAD + 6}' y1='${sbY}' x2='${(PAD + 6 + 2000 * sc).toFixed(1)}' y2='${sbY}' stroke='#000000' strokeWidth='3'/>`);
shapes.push(`<path d='M${W - 40},${62}L${W - 33},${82}L${W - 47},${82}Z' fill='#000000'/>`);

// Labels: typed out one by one (the doc's drawings need literal words with their own ids).
const labels = [];
const T = (id, [x, y], rawWords, size = 12, weight = 600, anchor = "middle", fill = "var(--cds-text-primary)", halo = 3, words = rawWords.replace(/&/g, "&amp;")) =>
	labels.push(halo > 0
		// On the map: dark words on a white halo in either theme (the map's colours don't change with the theme).
		? `<text data-claude-text-id='${id}' x='${x}' y='${y}' fontSize='${size}' fontWeight='${weight}' textAnchor='${anchor}' fill='${fill === "var(--cds-text-primary)" ? "#1a1a1a" : fill}' stroke='#ffffff' strokeWidth='${halo}' strokeOpacity='0.85' paintOrder='stroke'>${words}</text>`
		: `<text data-claude-text-id='${id}' x='${x}' y='${y}' fontSize='${size}' fontWeight='${weight}' textAnchor='${anchor}' fill='${fill}'>${words}</text>`);
T("title", [24, 26], `The round island to scale: ${(2 * R0 / 1000).toFixed(1)} km across, nine lands round Hearthmoot's meadow`, 15, 600, "start", "var(--cds-text-primary)", 0);
// Land names at each land's middle, nudged apart where two would overlap.
const lb = lands.map((s) => { const [x, y] = P(s.label); return { x, y, w: Math.max(s.name.length * 6.6, 48) + 8, h: 30 }; });
{ const [x, y] = P(pt(norm(byId.desert.mid), sky.r2 + 160)); lb.push({ x, y, w: 112, h: 16 }); }
for (let it = 0; it < 80; it++) for (let a = 0; a < lb.length; a++) for (let b = a + 1; b < lb.length; b++) {
	const A = lb[a], B = lb[b], ovx = (A.w + B.w) / 2 - Math.abs(A.x - B.x), ovy = (A.h + B.h) / 2 - Math.abs(A.y - B.y);
	if (ovx > 0 && ovy > 0) { if (ovx < ovy) { const m = (A.x < B.x ? -1 : 1) * ovx / 2; A.x += m; B.x -= m; } else { const m = (A.y < B.y ? -1 : 1) * ovy / 2; A.y += m; B.y -= m; } }
}
lands.forEach((s, i) => {
	const x = Math.round(lb[i].x), y = Math.round(lb[i].y);
	T(`name-${s.id}`, [x, y - 4], s.name, 12, 700);
	T(`size-${s.id}`, [x, y + 11], `${(s.area / 1e6).toFixed(1)} km²`, 11, 400);
});
T("hearthmoot", [P([0, 0])[0], P([0, 0])[1] + 26], "Hearthmoot", 11, 700);
T("meadow-label", P(pt(290, 1000)), "the meadow", 10, 600, "middle", "#2f5a1a");
T("lake-label", (([x, y]) => [x, y + 4])(P(lakeAt)), "lake", 10, 600, "middle", "#1a3a5a");
T("sky-label", [Math.round(lb[lands.length].x), Math.round(lb[lands.length].y)], "sky islands above", 11, 600, "middle", "#1a3a5a");
T("crystal-label", P(pt(62, coastR(62) + 750)), "Crystal caves below", 11, 700, "middle", "#5a2a8a");
T("fungal-label", P(pt(222, coastR(222) + 750)), "Fungal caverns below", 11, 700, "middle", "#1a5a3a");
T("severance", (([x, y]) => [x + 8, y + 4])(P(layers.severance.at)), "the Severance", 10, 600, "start");
T("flue", (([x, y]) => [x + 8, y + 4])(P(layers.flue.at)), "the Flue", 10, 600, "start");
T("thalassor", (([x, y]) => [x + 9, y + 4])(P(layout.sea.thalassor)), "Thalassor's waters", 10, 600, "start");
T("scale", [PAD + 6, sbY - 7], "2 km", 11, 600, "start");
T("north", [W - 40, 98], "N", 12, 700);
// Key, under the map.
const ky = Math.round(40 + 2 * PAD + 2 * VIEW * sc + 22);
T("key-1", [24, ky], "White dots: three wake-up camps per land. Thin lines: each land's road in to a crossroads it shares.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-2", [24, ky + 18], "Thick lines: four main roads across the meadow to the city's gates; squares: their forts at the meadow's edge.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-3", [24, ky + 36], "Red triangles: major boss lairs, off the roads, deep inland. Dashed lines: coast (easy), midlands, inland (hard).", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-4", [24, ky + 54], "Yellow diamonds: Wind, Spore and Arcane starts. White dashes: sky islands. Purple and green dots: caves below.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-5", [24, ky + 72], "Pale ring: calm shallows (300 m); wider ring: reefs (1.2 km); then deep sea. Blue line: the lake's stream.", 11, 400, "start", "var(--cds-text-primary)", 0);

const svgBody = `<svg viewBox='0 0 ${W} ${H}' role='img' aria-label='The round island to scale: nine lands round Hearthmoot and its meadow' fontFamily='sans-serif'>${shapes.join("")}${labels.join("")}</svg>`;
const xmlSvg = svgBody.replace(/ ([a-z]+)([A-Z][a-z]+)='/g, (m, a, b) => a + b === "viewBox" ? m : ` ${a}-${b.toLowerCase()}='`).replace("<svg ", "<svg xmlns='http://www.w3.org/2000/svg' ")
	.replace(/ data-claude-[a-z-]+='[^']*'/g, "").replace(/var\(--cds-text-primary\)/g, "#1a1a1a").replace(/ & /g, " &amp; ");
fs.writeFileSync(path.join(root, "Docs/design/round_island_map.svg"), xmlSvg);
const widgetArg = process.argv.indexOf("--widget");
if (widgetArg > 0) fs.writeFileSync(process.argv[widgetArg + 1], `export default () => ${svgBody};`);

// ---- the table ----------------------------------------------------------------------------------------------------------
console.log(`island ${(2 * R0 / 1000).toFixed(2)} km across, widest ${(2 * extent / 1000).toFixed(1)} km at the volcano cape (coast ${layout.coastRadius.min}-${layout.coastRadius.max} m from the centre); land ${layout.landKm2} km² (grid ${(cellTotal / 1e6).toFixed(1)}) + the meadow ${(meadowArea / 1e6).toFixed(2)} km² (lake ${(lakeArea / 1e6).toFixed(2)} km², stream ${(streamLen / 1000).toFixed(1)} km)`);
console.log(`checks: lake inside the meadow ${lakeOk}; road points in the lake ${roadsInLake}; every camp on its own land ${campsOk}; every shrine and champion on its own land ${lands.every((s) => s.worksOwn)}`);
console.log("land | path | Aaron km² | now km² (outline) | coast | coast km | coast/mid/inland km² | own road km (on own land) | boss from a road");
for (const s of lands) console.log(`${s.name} | ${s.path} | ${s.aaron} | ${(s.area / 1e6).toFixed(2)} (${(s.polyArea / 1e6).toFixed(2)}) | ${norm(s.coastFrom).toFixed(0)}-${norm(s.coastTo).toFixed(0)} | ${(s.coastLen / 1000).toFixed(1)} | ${s.bandArea.slice().reverse().map((a) => (a / 1e6).toFixed(1)).join(" / ")} | ${(s.roadLen / 1000).toFixed(1)} (${Math.round(s.roadOwn * 100)}%) | ${Math.round(s.bossFromRoad)} m (border ${Math.round(s.bossFromBorder)} m)`);
for (const t of trunks) console.log(`main road ${t.lands.join("+")}: bearing ${t.bearing.toFixed(0)}, crossroads ${Math.round(Math.hypot(...t.crossroads))} m out, fort ${Math.round(Math.hypot(...t.fort))} m`);
for (const k of ["sky", "crystal", "fungal"]) { const L = layers[k]; console.log(`${k}: ${L.from.toFixed(0)}-${L.to.toFixed(0)} deg, ${L.r1}-${L.r2} m from the centre, ${L.areaKm2} km²`); }
console.log(`checks: the Severance over the Crystal caves ${inPoly(layers.severance.at, crystal.outline)}; the Flue over the Fungal caverns ${inPoly(layers.flue.at, fungal.outline)}; caverns 400 m in from the coast ${[...crystal.outline, ...fungal.outline].every((p) => Math.hypot(...p) <= coastR(bearingOf(...p)) - 390)}`);
}
