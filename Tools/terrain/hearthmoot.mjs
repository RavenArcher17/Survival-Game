// Hearthmoot and its meadow (plan phase 3, slice 1, with Raven's 11-19 quality pass: "go ahead with what you
// recommend"). The city on its hill in the middle of the round island, in the game's own building look (FWRBuildLook
// blocks: coursed stone, timber, plaster and roofs tinted per path), laid out along the shared plan's streets
// (Tools/terrain/hearthmoot_plan.mjs): the curtain wall with twelve towers and four gatehouses; the hearth and a ring of
// market stalls on the plaza; a hall and a green with a well for each of the twelve quarters (Raven's answers, in the
// ring's order); then houses of every kind (cottages, half-timbered houses, tall townhouses, L-shaped houses, workshops,
// tower houses, walled gardens) fronting the streets on plots of their own, roofed in their quarter's colour, trees
// along the avenues. Out on the meadow: its trees in groves, hedgerows round the field blocks, a farmstead by each main
// road's fields, two windmills, reeds in the lake's shallows, the jetty off the beach and a footbridge over the stream;
// the forts on the main roads and a waymark at each crossroads.
// Writes Data/hearthmoot.json: blocks (what you see), solids (what you bump into), plants (trees and shrubs drawn with
// the plant meshes: WRRoundIsland.cpp picks them). Built on every machine, nothing replicated. Metres here, cm in the file.
//   node Tools/terrain/hearthmoot.mjs        (after round_ground.mjs: it stands everything on the ground's heights)
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import * as RI from "./round_island.mjs";
import * as PLAN from "./hearthmoot_plan.mjs";

const { quarters, trunks, CITY_WALL, RAD, norm, dAng, pt, bearingOf, lakeOutline, inPoly } = RI;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

// ---- the ground's heights ---------------------------------------------------------------------------------------------
const land = JSON.parse(fs.readFileSync(path.join(root, "Data/lands/island.json"), "utf8"));
const HF = land.heightfield, N = HF.samples, LAKE_LEVEL = land.lake.level;
const raw = zlib.inflateSync(fs.readFileSync(path.join(root, "Data/lands", HF.file)));
const sample = (i, j) => (raw.readUInt16LE((Math.max(0, Math.min(N - 1, j)) * N + Math.max(0, Math.min(N - 1, i))) * 2) - HF.offset) / HF.unitsPerMetre;
const groundAt = (x, y) => {
	const fi = (x - HF.x0) / HF.step, fj = (y - HF.y0) / HF.step, i = Math.floor(fi), j = Math.floor(fj), u = fi - i, v = fj - j;
	return (sample(i, j) * (1 - u) + sample(i + 1, j) * u) * (1 - v) + (sample(i, j + 1) * (1 - u) + sample(i + 1, j + 1) * u) * v;
};
const CITY_TOP = groundAt(0, 0);

// ---- inputs ---------------------------------------------------------------------------------------------------------
const WALL = { r: CITY_WALL, height: 11, thick: 4, segment: 6 };
const GATE_HALF = 7;
// Each path's quarter: its roof colour (its path's) and the plaster its houses wear.
const QUARTER_LOOK = {
	lightning: { roof: [118, 108, 196], plaster: [200, 200, 212] }, wind: { roof: [176, 204, 230], plaster: [226, 228, 234] },
	sun: { roof: [206, 160, 58], plaster: [228, 208, 164] }, nature: { roof: [76, 128, 62], plaster: [206, 196, 166] },
	arcane: { roof: [156, 80, 178], plaster: [206, 196, 218] }, frost: { roof: [140, 188, 220], plaster: [218, 226, 234] },
	blood: { roof: [122, 34, 38], plaster: [184, 158, 148] }, tide: { roof: [44, 132, 142], plaster: [208, 216, 208] },
	necromancy: { roof: [96, 88, 108], plaster: [158, 154, 158] }, spore: { roof: [116, 124, 60], plaster: [196, 190, 156] },
	void: { roof: [62, 46, 82], plaster: [140, 132, 146] }, fire: { roof: [188, 72, 42], plaster: [196, 158, 126] },
};

// ---- blocks, solids, plants --------------------------------------------------------------------------------------------
// A block: [mesh 0 cube 1 cylinder 2 cone, mat 0 wood 1 stone 2 cloth 3 trim 4 tinted, cx, cy, cz, sx, sy, sz, yaw,
// pitch, roll, palette index] (cm, degrees); palette rows [r, g, b, metal, rough, glow, pattern]. A plant: [kind 0 tree
// 1 sapling 2 shrub, variant, cx, cy, cz, yaw, scale].
const palette = [], palIndex = new Map();
const pal = (rgb, metal = 0, rough = 0.85, glow = 0, pattern = 0) => {
	const key = [...rgb.map(Math.round), metal, rough, glow, pattern].join(",");
	if (!palIndex.has(key)) { palIndex.set(key, palette.length); palette.push([...rgb.map(Math.round), metal, rough, glow, pattern]); }
	return palIndex.get(key);
};
const blocks = [], solids = [], plants = [];
const cm = (v) => Math.round(v * 100);
const CUBE = 0, CYL = 1, CONE = 2, WOOD = 0, STONE = 1, CLOTH = 2, TINT = 4;
const frame = (x, y, z, yaw) => ({
	x, y, z, yaw,
	at(lx, ly, lz) { const c = Math.cos(yaw * RAD), s = Math.sin(yaw * RAD); return [x + lx * c - ly * s, y + lx * s + ly * c, z + lz]; },
});
// Anything standing on the ground (its bottom at 0) goes SINK m into it, so a dip under it never shows a gap.
const SINK = 2;
function block(F, mesh, mat, [lx, ly, lz], [sx, sy, sz], { roll = 0, pitch = 0, yawAdd = 0, colour = -1, solid = false, nosink = false } = {}) {
	if (!nosink && roll === 0 && pitch === 0 && Math.abs(lz - sz / 2) < 0.15 && mesh !== CONE) { lz -= SINK / 2; sz += SINK; }
	const [x, y, z] = F.at(lx, ly, lz);
	blocks.push([mesh, mat, cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1), +pitch.toFixed(1), +roll.toFixed(1), colour]);
	if (solid) solids.push([cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1)]);
}
const plant = (kind, variant, x, y, yaw, scale) => plants.push([kind, variant, cm(x), cm(y), cm(groundAt(x, y) - 0.15), Math.round(yaw), +scale.toFixed(2)]);
const trunkSolid = (x, y, h = 4) => solids.push([cm(x), cm(y), cm(groundAt(x, y) + h / 2), 70, 70, cm(h), 0]);
const slate = pal([62, 66, 74], 0, 0.7), iron = pal([40, 40, 44], 0.8, 0.45, 0, 2), darkWood = pal([58, 40, 26], 0, 0.9, 0, 1), beam = pal([70, 50, 34], 0, 0.9, 0, 1);
const glass = pal([30, 34, 40], 0, 0.2, 0, 3), ember = pal([255, 120, 30], 0, 0.6, 6, 3), straw = pal([196, 168, 92], 0, 0.95, 0, 1);
const reedGreen = pal([96, 112, 58], 0, 0.9, 0, 1), reedHead = pal([92, 64, 40], 0, 0.95, 0, 1), cloth = pal([150, 40, 40], 0, 0.9);
const gateBearings = trunks.map((t) => t.bearing);
const nearGate = (deg, r, clear) => gateBearings.some((g) => Math.abs(dAng(deg, g)) * RAD * r < clear);
const polar = (deg, r) => pt(deg, r);
const R = PLAN.rng(20261119);
const pick = (arr) => arr[Math.floor(R() * arr.length)];
const tone = (rgb, amount = 0.12) => { const k = 1 + (R() * 2 - 1) * amount; return rgb.map((v) => Math.max(0, Math.min(255, v * k))); };
const weathered = (rgb) => { const g = rgb[0] * 0.3 + rgb[1] * 0.59 + rgb[2] * 0.11; return rgb.map((v) => v * 0.72 + g * 0.28); };
const quarterOf = (deg) => quarters.find((q) => { const a = norm(deg - q.from), span = norm(q.to - q.from) || 30; return a < span; }) || quarters[0];

// ---- footprints: what stands where (rectangles), so nothing overlaps -----------------------------------------------
const taken = [];   // { c:[x,y], u:[ux,uy], hw, hd }
const corners = (b) => { const v = [-b.u[1], b.u[0]]; return [[1, 1], [1, -1], [-1, -1], [-1, 1]].map(([a, c]) => [b.c[0] + b.u[0] * b.hw * a + v[0] * b.hd * c, b.c[1] + b.u[1] * b.hw * a + v[1] * b.hd * c]); };
function overlaps(a, b, gap) {
	const axes = [a.u, [-a.u[1], a.u[0]], b.u, [-b.u[1], b.u[0]]], ca = corners({ ...a, hw: a.hw + gap / 2, hd: a.hd + gap / 2 }), cb = corners({ ...b, hw: b.hw + gap / 2, hd: b.hd + gap / 2 });
	for (const ax of axes) {
		const pa = ca.map((p) => p[0] * ax[0] + p[1] * ax[1]), pb = cb.map((p) => p[0] * ax[0] + p[1] * ax[1]);
		if (Math.max(...pa) < Math.min(...pb) || Math.max(...pb) < Math.min(...pa)) return false;
	}
	return true;
}
const free = (b, gap = 1.2) => !taken.some((t) => overlaps(b, t, gap));
const reserve = (b) => taken.push(b);
const reserveCircle = ([x, y], r) => reserve({ c: [x, y], u: [1, 0], hw: r, hd: r });
const offStreets = (b, margin = 0.6) => corners(b).concat([b.c]).every((p) => PLAN.nearestStreet(p).edge > margin);
const insideCity = (b) => corners(b).every((p) => { const r = Math.hypot(...p); return r < PLAN.CITY_INNER && r > PLAN.PLAZA_R + 6; });

// ---- the curtain wall, its towers and gatehouses ---------------------------------------------------------------------
{
	const n = Math.round(2 * Math.PI * WALL.r / WALL.segment), step = 360 / n;
	for (let k = 0; k < n; k++) {
		const deg = k * step;
		if (nearGate(deg, WALL.r, GATE_HALF + 6)) continue;
		const [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg), len = 2 * Math.PI * WALL.r / n + 0.15;
		block(F, CUBE, STONE, [0, 0, WALL.height / 2], [len, WALL.thick, WALL.height], { solid: true });
		block(F, CUBE, STONE, [-len / 4, -WALL.thick / 2 + 0.4, WALL.height + 0.7], [len / 2 - 0.6, 0.8, 1.4]);
		block(F, CUBE, STONE, [0, WALL.thick / 2 - 0.25, WALL.height + 0.45], [len, 0.5, 0.9]);
	}
}
for (const q of quarters) {
	const deg = q.from;
	if (nearGate(deg, WALL.r, 26)) continue;
	const [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg);
	block(F, CYL, STONE, [0, 0, 8.5], [14, 14, 17], { solid: true });
	block(F, CYL, STONE, [0, 0, 17.4], [15, 15, 0.8]);
	block(F, CONE, TINT, [0, 0, 21.8], [15.6, 15.6, 8], { colour: slate });
}
for (const t of trunks) {
	const deg = t.bearing, [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg);
	for (const side of [-1, 1]) {
		block(F, CUBE, STONE, [side * (GATE_HALF + 5), 0, 9], [10, 11, 18], { solid: true });
		block(F, CUBE, STONE, [side * (GATE_HALF + 5), 0, 18.6], [11, 12, 1.2]);
		for (const m of [-3.5, 0, 3.5]) block(F, CUBE, STONE, [side * (GATE_HALF + 5) + m, -5.6, 19.9], [1.6, 0.9, 1.4]);
		block(F, CUBE, CLOTH, [side * (GATE_HALF + 5), -5.8, 12], [4, 0.2, 7], { colour: cloth });
	}
	block(F, CUBE, STONE, [0, 0, 13.5], [2 * GATE_HALF + 0.6, 11, 5], { solid: true });
	block(F, CUBE, TINT, [0, -3.5, 10.4], [2 * GATE_HALF - 0.4, 0.4, 1.2], { colour: iron });
}

// ---- the plaza: the hearth, braziers, and the market ring ------------------------------------------------------------
{
	const F = frame(0, 0, CITY_TOP, 0);
	block(F, CYL, STONE, [0, 0, 0.6], [12, 12, 1.2], { solid: true });
	block(F, CYL, STONE, [0, 0, 1.25], [10, 10, 0.3]);
	block(F, CYL, TINT, [0, 0, 2.6], [4, 4, 3], { colour: ember });
	block(F, CONE, TINT, [0, 0, 5.4], [3.2, 3.2, 3], { colour: ember });
	for (let k = 0; k < 4; k++) {
		const [bx, by] = polar(45 + 90 * k, 18), G = frame(bx, by, CITY_TOP, 0);
		block(G, CYL, STONE, [0, 0, 1], [1.4, 1.4, 2], { solid: true });
		block(G, CYL, TINT, [0, 0, 2.3], [1.2, 1.2, 0.6], { colour: ember });
	}
	reserveCircle([0, 0], 24);
	// Market stalls in a ring, their counters facing the hearth, awnings in the twelve paths' colours.
	const n = 18;
	for (let k = 0; k < n; k++) {
		const deg = k * 360 / n + 9;
		if (nearGate(deg, 31, 6)) continue;
		const [x, y] = polar(deg, 31), F2 = frame(x, y, CITY_TOP, deg), awning = pal(tone(QUARTER_LOOK[quarters[k % 12].path].roof, 0.08), 0, 0.9);
		for (const [px, py] of [[-1.4, -1], [1.4, -1], [-1.4, 1], [1.4, 1]]) block(F2, CYL, WOOD, [px, py, 1.4], [0.14, 0.14, 2.8]);
		block(F2, CUBE, WOOD, [0, 0.8, 0.5], [2.8, 0.7, 1], { solid: true, colour: beam });
		block(F2, CUBE, TINT, [0, 0, 2.85], [3.4, 2.6, 0.1], { roll: -12, colour: awning });
		for (let c = 0; c < 3; c++) if (R() < 0.7) block(F2, R() < 0.5 ? CUBE : CYL, WOOD, [-0.9 + c * 0.9, 0.85, 1.2], [0.5, 0.5, 0.4]);
		if (R() < 0.6) block(F2, CYL, WOOD, [1.9, -0.6, 0.45], [0.6, 0.6, 0.9]);
		reserve({ c: [x, y], u: [Math.cos(deg * RAD), Math.sin(deg * RAD)], hw: 2, hd: 1.6 });
	}
	// Round the plaza's edge: trees with benches under them, but not across an avenue's mouth.
	for (let k = 0; k < 16; k++) {
		const deg = k * 22.5 + 11;
		if (nearGate(deg, 40, 9)) continue;
		const [x, y] = polar(deg, 40.5);
		plant(0, Math.floor(R() * 8), x, y, R() * 360, 0.42 + 0.08 * R());
		trunkSolid(x, y);
		block(frame(...polar(deg, 38), CITY_TOP, deg), CUBE, WOOD, [0, 0, 0.25], [2.2, 0.55, 0.5], { colour: beam });
		reserveCircle([x, y], 2);
	}
}

// ---- each quarter: its hall facing the ring street, its green with a well and trees ------------------------------------
const wellAt = ([x, y]) => {
	const F = frame(x, y, CITY_TOP, R() * 360);
	block(F, CYL, STONE, [0, 0, 0.45], [2.2, 2.2, 0.9], { solid: true });
	block(F, CYL, TINT, [0, 0, 0.92], [1.6, 1.6, 0.05], { colour: pal([24, 46, 58], 0, 0.15, 0, 3) });
	for (const s of [-1, 1]) block(F, CUBE, WOOD, [s * 1.05, 0, 1.4], [0.18, 0.18, 2.8]);
	block(F, CUBE, TINT, [0, 0, 2.95], [2.8, 1.6, 1.6], { roll: 45, colour: slate });
};
const quarterGreens = [];
for (const q of quarters) {
	const look = QUARTER_LOOK[q.path], roof = pal(weathered(look.roof), 0, 0.8), mid = norm(q.from + 15);
	// The hall: outside the ring street at the quarter's middle (beside an avenue if one runs there).
	let placed = false;
	for (const [rr, off] of [170, 200, 130, 235].flatMap((r) => [0, 6, -6, 10, -10, 13, -13].map((o) => [r, o]))) {
		if (placed) break;
		const deg = mid + off, ns = PLAN.nearestStreet(polar(deg, rr));
		const dir = [ns.at[0] - polar(deg, rr)[0], ns.at[1] - polar(deg, rr)[1]], L = Math.hypot(...dir) || 1;
		const d = [dir[0] / L, dir[1] / L], yaw = Math.atan2(-d[0], d[1]) / RAD;
		const c = [ns.at[0] - d[0] * (ns.street.half + 2 + 7), ns.at[1] - d[1] * (ns.street.half + 2 + 7)];
		const fp = { c, u: [Math.cos(yaw * RAD), Math.sin(yaw * RAD)], hw: 16, hd: 8 };
		if (!free(fp, 2) || !offStreets(fp) || !insideCity(fp)) continue;
		reserve(fp);
		const F = frame(c[0], c[1], CITY_TOP, yaw);
		block(F, CUBE, STONE, [2.5, 0, 6], [22, 14, 12], { solid: true });
		block(F, CUBE, TINT, [2.5, 0, 12], [22.8, 10.6, 10.6], { roll: 45, colour: roof });
		block(F, CUBE, STONE, [-12.5, 0, 10], [8, 8, 20], { solid: true });
		block(F, CONE, TINT, [-12.5, 0, 24], [9.4, 9.4, 8], { colour: roof });
		block(F, CUBE, WOOD, [2.5, 7.05, 2.4], [3.2, 0.3, 4.8], { colour: darkWood });
		block(F, CUBE, STONE, [2.5, 8.2, 0.25], [6, 2.4, 0.5]);
		for (const wx of [-4, 9]) block(F, CUBE, TINT, [wx, 7.05, 6.5], [1.4, 0.12, 3.2], { colour: glass });
		block(F, CYL, WOOD, [-7, 8.5, 5], [0.35, 0.35, 10]);
		block(F, CUBE, CLOTH, [-5.9, 8.5, 7.6], [2, 0.12, 4], { colour: roof });
		placed = true;
		break;
	}
	if (!placed) console.warn(`no room for the ${q.path} quarter's hall`);
	// The green: an open square of grass with a well and a couple of trees, set back off a lane or alley.
	for (const rr of [205, 240, 120, 260]) {
		const deg = mid + (R() - 0.5) * 12, c = polar(deg, rr), fp = { c, u: [1, 0], hw: 13, hd: 13 };
		if (!free(fp, 1) || !offStreets(fp, 0.3) || !insideCity(fp)) continue;
		reserve(fp);
		wellAt(c);
		for (let t = 0; t < 3; t++) { const a = R() * 360, p = [c[0] + Math.cos(a * RAD) * (6 + 4 * R()), c[1] + Math.sin(a * RAD) * (6 + 4 * R())]; plant(0, Math.floor(R() * 8), p[0], p[1], R() * 360, 0.5 + 0.2 * R()); trunkSolid(p[0], p[1]); }
		for (const s of [-1, 1]) block(frame(c[0] + s * 4, c[1] + 3, CITY_TOP, 0), CUBE, WOOD, [0, 0, 0.25], [2, 0.5, 0.5], { colour: beam });
		quarterGreens.push(c);
		break;
	}
}

// ---- trees along the avenues -------------------------------------------------------------------------------------------
for (const st of PLAN.streets.filter((s) => s.kind === "avenue")) {
	let L = 0;
	for (let i = 1; i < st.pts.length; i++) {
		const a = st.pts[i - 1], b = st.pts[i], seg = Math.hypot(b[0] - a[0], b[1] - a[1]), d = [(b[0] - a[0]) / seg, (b[1] - a[1]) / seg];
		L += seg;
		if (L < 22) continue;
		L = 0;
		const r = Math.hypot(...b);
		if (r > 270 || r < PLAN.PLAZA_R + 14 || Math.abs(r - 150) < 12) continue;
		for (const s of [-1, 1]) {
			const p = [b[0] - d[1] * s * (st.half + 2.2), b[1] + d[0] * s * (st.half + 2.2)];
			if (!free({ c: p, u: [1, 0], hw: 1.5, hd: 1.5 }, 0.5)) continue;
			reserveCircle(p, 1.6);
			plant(0, Math.floor(R() * 8), p[0], p[1], R() * 360, 0.48 + 0.1 * R());
			trunkSolid(p[0], p[1]);
		}
	}
}

// ---- the houses: every kind, fronting the nearest street on a plot of its own -----------------------------------------
const KINDS = [["cottage", 30], ["house", 34], ["townhouse", 12], ["lhouse", 9], ["workshop", 8], ["tower", 4], ["garden", 3]];
const pickKind = () => { let t = R() * 100; for (const [k, w] of KINDS) { if ((t -= w) < 0) return k; } return "house"; };
const SIZES = { cottage: [[7, 9], [6, 7.5]], house: [[9, 12], [7, 9]], townhouse: [[6, 7.5], [9, 11.5]], lhouse: [[10, 13], [7, 8.5]], workshop: [[12, 15.5], [8.5, 10]], tower: [[6, 7], [6, 7]], garden: [[11, 15], [10, 13]] };
function windows(F, w, d, z, n, h = 1.2) { for (let i = 0; i < n; i++) { const x = -w / 2 + w * (i + 0.5) / n; if (R() < 0.85) block(F, CUBE, TINT, [x, d / 2 + 0.05, z], [0.95, 0.12, h], { colour: glass }); if (R() < 0.35) block(F, CUBE, WOOD, [x + 0.7, d / 2 + 0.06, z], [0.4, 0.1, h + 0.1], { colour: darkWood }); } }
// A gable roof is one block turned 45 degrees: its upper half the roof, its lower half hidden in the house, so it may
// not reach past the gable walls (its lower half would show there); the eaves overhang a little either side.
function gableRoof(F, w, d, z, roof, ox = 0, oy = 0, along = 0) { block(F, CUBE, TINT, [ox, oy, z], along ? [w * 0.78, d - 0.04, w * 0.78] : [w - 0.04, d * 0.78, d * 0.78], along ? { pitch: 45, colour: roof } : { roll: 45, colour: roof }); }
function halfTimber(F, w, d, z0, h) {
	// Posts at the corners and between, a sill beam and a head beam, on the street face and the ends.
	for (const x of [-w / 2, -w / 6, w / 6, w / 2]) block(F, CUBE, TINT, [x, d / 2 + 0.06, z0 + h / 2], [0.22, 0.12, h], { colour: beam });
	for (const zz of [z0 + 0.1, z0 + h - 0.1]) block(F, CUBE, TINT, [0, d / 2 + 0.06, zz], [w, 0.12, 0.2], { colour: beam });
	for (const s of [-1, 1]) for (const y of [-d / 2, 0, d / 2]) block(F, CUBE, TINT, [s * (w / 2 + 0.06), y, z0 + h / 2], [0.12, 0.22, h], { colour: beam });
}
function building(kind, F, w, d, look) {
	const roof = pal(tone(weathered(look.roof)), 0, 0.8), plaster = pal(tone(look.plaster, 0.06), 0, 0.92), useStone = R() < 0.45;
	const wallMat = (ground) => (ground && useStone ? [STONE, -1] : [TINT, plaster]);
	switch (kind) {
		case "cottage": {
			const h = 3.8 + R() * 0.8, [m, c] = wallMat(true);
			block(F, CUBE, m, [0, 0, h / 2], [w, d, h], { solid: true, colour: c });
			gableRoof(F, w, d, h, roof);
			block(F, CUBE, WOOD, [-w / 4, d / 2 + 0.05, 1.1], [1.2, 0.2, 2.2], { colour: darkWood });
			windows(F, w, d, h * 0.55, 2);
			if (R() < 0.7) block(F, CUBE, STONE, [w / 3, -d / 6, h + d * 0.32], [0.9, 0.9, d * 0.6 + 1]);
			return h + d * 0.5;
		}
		case "house": {
			const h1 = 3.6, h2 = 3.2, [m] = wallMat(true);
			block(F, CUBE, m === STONE ? STONE : TINT, [0, 0, h1 / 2], [w, d, h1], { solid: true, colour: m === STONE ? -1 : plaster });
			block(F, CUBE, TINT, [0, 0, h1 + h2 / 2], [w + 0.5, d + 0.5, h2], { solid: true, colour: plaster });   // the jettied upper storey
			halfTimber(F, w + 0.5, d + 0.5, h1, h2);
			gableRoof(F, w + 0.5, d + 0.5, h1 + h2, roof);
			block(F, CUBE, WOOD, [-w / 4, d / 2 + 0.05, 1.1], [1.3, 0.2, 2.2], { colour: darkWood });
			windows(F, w, d, 2.1, 2); windows(F, w + 0.5, d + 0.5, h1 + 1.6, 3);
			if (R() < 0.6) block(F, CUBE, STONE, [-w / 3, 0, h1 + h2 + d * 0.3], [0.9, 0.9, d * 0.6 + 1.2]);
			return h1 + h2 + d * 0.5;
		}
		case "townhouse": {
			const hs = 3.3, n = 3;
			block(F, CUBE, STONE, [0, 0, hs / 2], [w, d, hs], { solid: true });
			block(F, CUBE, TINT, [0, 0, hs + (n - 1) * hs / 2], [w, d, (n - 1) * hs], { solid: true, colour: plaster });
			for (let s = 1; s < n; s++) halfTimber(F, w, d, s * hs, hs);
			// The gable faces the street: the ridge runs back from it.
			gableRoof(F, w, d, n * hs, roof, 0, 0, 1);
			block(F, CUBE, WOOD, [0, d / 2 + 0.05, 1.15], [1.3, 0.2, 2.3], { colour: darkWood });
			for (let s = 1; s < n; s++) windows(F, w, d, s * hs + 1.7, 2);
			return n * hs + w * 0.5;
		}
		case "lhouse": {
			const h = 3.8 + R() * 0.6, [m, c] = wallMat(true), ww = d, wd = w * 0.55, side = R() < 0.5 ? -1 : 1;
			block(F, CUBE, m, [0, 0, h / 2], [w, d, h], { solid: true, colour: c });
			gableRoof(F, w, d, h, roof);
			// The wing runs back from one end, its own roof across the main one's.
			block(F, CUBE, m, [side * (w / 2 - ww / 2), -d / 2 - wd / 2 + 0.2, h / 2], [ww, wd, h], { solid: true, colour: c });
			gableRoof(F, ww, wd + 0.4, h, roof, side * (w / 2 - ww / 2), -d / 2 - wd / 2 + 0.2, 1);
			block(F, CUBE, WOOD, [-side * w / 4, d / 2 + 0.05, 1.1], [1.2, 0.2, 2.2], { colour: darkWood });
			windows(F, w, d, h * 0.55, 3);
			return h + d * 0.5;
		}
		case "workshop": {
			const h = 4.6;
			block(F, CUBE, STONE, [0, 0, h / 2], [w, d, h], { solid: true });
			gableRoof(F, w, d, h, roof);
			block(F, CUBE, WOOD, [w / 6, d / 2 + 0.06, 1.6], [3.4, 0.2, 3.2], { colour: darkWood });
			// A lean-to on one end and the yard's crates and barrels.
			block(F, CUBE, WOOD, [-w / 2 - 1.6, 0, 1.4], [3.2, d - 1, 2.8], { colour: beam });
			block(F, CUBE, TINT, [-w / 2 - 1.6, 0, 3.0], [3.8, d - 0.4, 0.15], { roll: 0, pitch: -14, colour: slate });
			for (let c = 0; c < 4; c++) block(F, R() < 0.5 ? CUBE : CYL, WOOD, [w / 2 - 1 - c * 1.1, d / 2 + 1.1, 0.45], [0.8, 0.8, 0.9]);
			windows(F, w, d, 2.8, 2);
			return h + d * 0.5;
		}
		case "tower": {
			const h = 12 + R() * 3;
			block(F, CUBE, STONE, [0, 0, h / 2], [w, d, h], { solid: true });
			block(F, CONE, TINT, [0, 0, h + 3.2], [w * 1.3, d * 1.3, 6.4], { colour: roof });
			block(F, CUBE, WOOD, [0, d / 2 + 0.05, 1.15], [1.2, 0.2, 2.3], { colour: darkWood });
			for (let z = 4; z < h - 1; z += 3.5) windows(F, w, d, z, 1, 1);
			return h + 6;
		}
		case "garden": {
			// A walled garden: low walls with a gate, a tree, beds and a bench.
			for (const s of [-1, 1]) block(F, CUBE, STONE, [0, -s * d / 2, 0.7], [w, 0.6, 1.4], { solid: true });
			block(F, CUBE, STONE, [-w / 2, 0, 0.7], [0.6, d, 1.4], { solid: true });
			for (const s of [-1, 1]) block(F, CUBE, STONE, [w / 2, s * (d / 4 + 0.6), 0.7], [0.6, d / 2 - 1.2, 1.4], { solid: true });
			const [tx, ty] = F.at(-w / 5, 0, 0);
			plant(R() < 0.5 ? 1 : 0, Math.floor(R() * 8), tx, ty, R() * 360, 0.45 + 0.15 * R());
			for (const s of [-1, 1]) block(F, CUBE, TINT, [w / 6, s * d / 4, 0.18], [w / 3, 1.4, 0.36], { colour: pal([60, 44, 30], 0, 0.95) });
			block(F, CUBE, WOOD, [w / 4, 0, 0.25], [0.5, 2, 0.5], { colour: beam });
			return 2;
		}
	}
	return 0;
}
const counts = {};
{
	const cands = [];
	for (let y = -PLAN.CITY_INNER; y <= PLAN.CITY_INNER; y += 6.5) for (let x = -PLAN.CITY_INNER; x <= PLAN.CITY_INNER; x += 6.5) {
		const p = [x + (R() - 0.5) * 5, y + (R() - 0.5) * 5], r = Math.hypot(...p);
		if (r > PLAN.PLAZA_R + 8 && r < PLAN.CITY_INNER - 4) cands.push(p);
	}
	for (let i = cands.length - 1; i > 0; i--) { const j = Math.floor(R() * (i + 1)); [cands[i], cands[j]] = [cands[j], cands[i]]; }
	for (const p of cands) {
		const ns = PLAN.nearestStreet(p);
		if (ns.edge < 1 || ns.edge > 24) continue;
		const kind = pickKind(), [[w0, w1], [d0, d1]] = SIZES[kind], w = w0 + (w1 - w0) * R(), d = d0 + (d1 - d0) * R();
		const dir = [ns.at[0] - p[0], ns.at[1] - p[1]], L = Math.hypot(...dir), u = [dir[0] / L, dir[1] / L];
		const yaw = Math.atan2(-u[0], u[1]) / RAD, back = ns.street.half + 0.8 + R() * 1.6 + d / 2;
		const c = [ns.at[0] - u[0] * back, ns.at[1] - u[1] * back];
		const fp = { c, u: [Math.cos(yaw * RAD), Math.sin(yaw * RAD)], hw: w / 2 + (kind === "lhouse" ? 0 : 0), hd: d / 2 + (kind === "lhouse" ? w * 0.28 : 0) };
		const fpCheck = kind === "lhouse" ? { ...fp, c: [c[0] + u[0] * -w * 0.28, c[1] + u[1] * -w * 0.28] } : fp;
		if (!insideCity(fpCheck) || !offStreets(fpCheck, 0.4) || !free(fpCheck, kind === "garden" ? 1.5 : 1.0)) continue;
		reserve(fpCheck);
		building(kind, frame(c[0], c[1], CITY_TOP, yaw), w, d, QUARTER_LOOK[quarterOf(bearingOf(...c)).path]);
		counts[kind] = (counts[kind] || 0) + 1;
	}
}

// ---- the meadow: its trees, the hedgerows round the field blocks, the farmsteads and the windmills ------------------------
for (const t of PLAN.trees) {
	plant(t.kind === "tree" ? 0 : 1, t.variant, t.at[0], t.at[1], t.yaw, t.scale);
	if (t.kind === "tree") trunkSolid(t.at[0], t.at[1]);
}
let hedge = 0;
for (const b of PLAN.fieldBlocks) {
	const ring = [...b.poly, b.poly[0]];
	for (let i = 1; i < ring.length; i++) {
		const a = ring[i - 1], c = ring[i], L = Math.hypot(c[0] - a[0], c[1] - a[1]);
		for (let s = 0; s < L; s += 2.6 + R() * 1.2) {
			if (R() < 0.06) { s += 8; continue; }   // gaps where a gate or a gap in the hedge lets you through
			const x = a[0] + (c[0] - a[0]) * s / L, y = a[1] + (c[1] - a[1]) * s / L;
			plant(2, Math.floor(R() * 10), x, y, R() * 360, 0.7 + 0.5 * R());
			hedge++;
			if (R() < 0.04) { plant(0, Math.floor(R() * 8), x, y, R() * 360, 0.7 + 0.3 * R()); trunkSolid(x, y); }
		}
	}
}
for (const f of PLAN.farms) {
	const z = groundAt(...f.at), F = frame(f.at[0], f.at[1], z, f.yaw), look = { roof: [128, 96, 64], plaster: [214, 204, 180] };
	building("house", frame(...F.at(-9, 0, 0).slice(0, 2), z, f.yaw), 10, 8, look);
	// The barn: big, timber, a stone footing; haystacks and a fenced yard.
	const B = frame(...F.at(10, -4, 0).slice(0, 2), groundAt(...F.at(10, -4, 0).slice(0, 2)), f.yaw + 90);
	block(B, CUBE, STONE, [0, 0, 0.5], [16, 10, 1], { solid: true });
	block(B, CUBE, WOOD, [0, 0, 3.5], [16, 10, 5], { solid: true, colour: beam });
	gableRoof(B, 16, 10, 6, pal([84, 70, 60], 0, 0.85));
	block(B, CUBE, WOOD, [0, 5.06, 2.3], [4, 0.2, 4.2], { colour: darkWood });
	for (let k = 0; k < 3; k++) { const [hx, hy] = F.at(-2 + k * 3.2, 9, 0); block(frame(hx, hy, groundAt(hx, hy), 0), CYL, TINT, [0, 0, 1.2], [2.6, 2.6, 2.4], { colour: straw, solid: true }); }
	for (let k = 0; k <= 10; k++) { const [px, py] = F.at(-16 + k * 3.2, 14, 0); block(frame(px, py, groundAt(px, py), f.yaw), CUBE, WOOD, [0, 0, 0.6], [0.16, 0.16, 1.2], { colour: beam }); }
	for (let k = 0; k < 4; k++) { const [tx, ty] = F.at(-20 + k * 5 + R() * 2, -12 - R() * 6, 0); plant(0, Math.floor(R() * 8), tx, ty, R() * 360, 0.8 + 0.25 * R()); trunkSolid(tx, ty); }
}
for (const m of PLAN.windmills) {
	const z = groundAt(...m.at), F = frame(m.at[0], m.at[1], z, m.yaw);
	block(F, CYL, STONE, [0, 0, 6], [7.5, 7.5, 12], { solid: true });
	block(F, CYL, STONE, [0, 0, 12.2], [7, 7, 0.4]);
	block(F, CONE, TINT, [0, 0, 14.4], [7.6, 7.6, 4.4], { colour: pal([80, 62, 46], 0, 0.85) });
	block(F, CUBE, WOOD, [0, 4.1, 1.15], [1.4, 0.2, 2.3], { colour: darkWood });
	block(F, CYL, WOOD, [0, 4.4, 12.6], [0.6, 0.6, 1.6], { roll: 90, colour: beam });
	for (let k = 0; k < 4; k++) {
		const a = 45 + 90 * k + 17;
		const [sx, sz] = [Math.cos(a * RAD) * 5.6, Math.sin(a * RAD) * 5.6];
		block(F, CUBE, WOOD, [sx, 5.2, 12.6 + sz], [11, 0.15, 1.9], { pitch: 0, roll: 0, yawAdd: 0, colour: pal([196, 186, 160], 0, 0.95) });
		blocks[blocks.length - 1][9] = +(-a).toFixed(1);   // the sail turned round the hub (pitch about the hub's axis)
	}
}

// ---- the lake: reeds in the shallows, the jetty off the beach and a moored boat, the footbridge over the stream ---------
let stalks = 0;
for (const [x, y, n] of PLAN.reeds) {
	for (let k = 0; k < n; k++) {
		const a = R() * 360, rr = R() * 1.6, px = x + Math.cos(a * RAD) * rr, py = y + Math.sin(a * RAD) * rr, h = 1.4 + R() * 0.9, base = Math.min(groundAt(px, py), LAKE_LEVEL);
		const F = frame(px, py, base, R() * 360);
		block(F, CYL, TINT, [0, 0, h / 2 + 0.01], [0.06, 0.06, h], { pitch: (R() - 0.5) * 14, roll: (R() - 0.5) * 14, colour: reedGreen, nosink: true });
		if (R() < 0.45) block(F, CYL, TINT, [0, 0, h + 0.1], [0.11, 0.11, 0.28], { colour: reedHead });
		stalks++;
	}
}
{
	const J = PLAN.jetty, deckZ = LAKE_LEVEL + 0.75, yaw = Math.atan2(J.dir[1], J.dir[0]) / RAD;
	const start = [J.at[0] - J.dir[0] * 3, J.at[1] - J.dir[1] * 3];
	const F = frame(start[0], start[1], deckZ, yaw);
	for (let s = 0; s < J.length; s += 0.34) block(F, CUBE, WOOD, [s, 0, 0], [0.3, J.width, 0.1], { colour: pal(tone([112, 84, 58], 0.1), 0, 0.92, 0, 1), nosink: true });
	solids.push([...F.at(J.length / 2, 0, -0.05).map(cm), cm(J.length), cm(J.width), 20, +yaw.toFixed(1)]);
	for (let s = 1.5; s < J.length; s += 3) for (const side of [-1, 1]) {
		const [px, py] = F.at(s, side * (J.width / 2 - 0.15), 0), bed = groundAt(px, py), h = deckZ - bed + 0.6;
		block(frame(px, py, bed, 0), CYL, WOOD, [0, 0, h / 2 - 0.2], [0.24, 0.24, h], { colour: beam, nosink: true });
	}
	// A rowboat tied up beside the end.
	const [bx, by] = F.at(J.length - 4, J.width / 2 + 1.4, 0), Bt = frame(bx, by, LAKE_LEVEL, yaw);
	block(Bt, CUBE, WOOD, [0, 0, 0.1], [3.8, 1.3, 0.5], { colour: pal([92, 64, 42], 0, 0.9, 0, 1), nosink: true });
	for (const s of [-1, 1]) block(Bt, CUBE, WOOD, [0, s * 0.62, 0.3], [3.9, 0.1, 0.5], { colour: beam, nosink: true });
	block(Bt, CUBE, WOOD, [0, 0, 0.32], [0.3, 1.2, 0.08], { colour: beam, nosink: true });
}
{
	const Fb = PLAN.footbridge, yaw = Math.atan2(Fb.along[0], -Fb.along[1]) / RAD;   // across the stream
	const ends = [-1, 1].map((s) => [Fb.at[0] - Fb.along[1] * s * 7, Fb.at[1] + Fb.along[0] * s * 7]);
	const deckZ = Math.max(...ends.map((e) => groundAt(...e))) + 0.35, F = frame(Fb.at[0], Fb.at[1], deckZ, yaw);
	for (let s = -7; s <= 7; s += 0.34) block(F, CUBE, WOOD, [s, 0, 0], [0.3, 2.4, 0.1], { colour: pal([112, 84, 58], 0, 0.92, 0, 1), nosink: true });
	solids.push([...F.at(0, 0, -0.05).map(cm), 1450, 240, 20, +yaw.toFixed(1)]);
	for (const side of [-1, 1]) {
		block(F, CUBE, WOOD, [0, side * 1.15, 0.95], [14.2, 0.12, 0.12], { colour: beam });
		for (let s = -7; s <= 7; s += 3.5) block(F, CUBE, WOOD, [s, side * 1.15, 0.5], [0.14, 0.14, 1], { colour: beam, nosink: true });
	}
}

// ---- a fort on each main road at the meadow's edge (the road runs through it), a waymark at each crossroads -------------
for (const t of trunks) {
	const [fx, fy] = t.fort, z = groundAt(fx, fy), F = frame(fx, fy, z, t.bearing - 90);
	const S = 36, Hh = 8, gate = 5, T = 3;
	for (const side of [-1, 1]) {
		block(F, CUBE, STONE, [0, side * S / 2, Hh / 2], [S, T, Hh], { solid: true });
		for (const end of [-1, 1]) block(F, CUBE, STONE, [end * S / 2, side * (gate + (S / 2 - gate) / 2), Hh / 2], [T, S / 2 - gate, Hh], { solid: true });
		for (const end of [-1, 1]) {
			block(F, CUBE, STONE, [end * S / 2, side * S / 2, 5.5], [8, 8, 11], { solid: true });
			for (const m of [-2.5, 2.5]) block(F, CUBE, STONE, [end * S / 2 + m, side * S / 2 + side * 3.6, 11.7], [1.4, 0.8, 1.4]);
			block(F, CONE, TINT, [end * S / 2, side * S / 2, 13.6], [9, 9, 4], { colour: slate });
		}
		// Merlons along the long walls.
		for (let m = -S / 2 + 3; m < S / 2 - 2; m += 2.6) block(F, CUBE, STONE, [m, side * (S / 2 + 1.1), Hh + 0.6], [1.2, 0.7, 1.2]);
	}
	for (const end of [-1, 1]) {
		block(F, CUBE, STONE, [end * S / 2, 0, Hh + 1.5], [T + 0.4, 2 * gate + 0.4, 3], { solid: true });
		block(F, CUBE, TINT, [end * S / 2, 0, Hh - 0.6], [0.3, 2 * gate - 0.4, 1], { colour: iron });
	}
	// Inside: a barracks along one wall, a well, a banner.
	block(F, CUBE, STONE, [0, -S / 2 + 5.5, 2.2], [S - 12, 7, 4.4], { solid: true });
	gableRoof(F, S - 12, 7, 4.4, slate, 0, -S / 2 + 5.5);
	block(F, CYL, WOOD, [6, S / 2 - 5, 6], [0.35, 0.35, 12]);
	block(F, CUBE, CLOTH, [6.9, S / 2 - 5, 9.5], [1.8, 0.12, 4], { colour: cloth });
	const [cx, cy] = t.crossroads, G = frame(cx, cy, groundAt(cx, cy), t.bearing);
	block(G, CUBE, STONE, [0, 0, 2.2], [1.2, 1.2, 4.4], { solid: true });
	block(G, CUBE, STONE, [0, 0, 4.7], [1.6, 1.6, 0.6]);
	for (const [a, l] of [[0, 1.8], [100, 1.5], [-110, 1.6]]) block(G, CUBE, WOOD, [0, 0, 3.3], [l, 0.12, 0.35], { yawAdd: a, colour: beam });
}

fs.writeFileSync(path.join(root, "Data/hearthmoot.json"), JSON.stringify({
	_comment: "Generated by Tools/terrain/hearthmoot.mjs from the shared plan (Tools/terrain/hearthmoot_plan.mjs; edit the scripts, not this file). Hearthmoot and its meadow, built on every machine by WRRoundIsland.cpp (no replication). blocks: [mesh 0 cube / 1 cylinder / 2 cone, mat 0 wood / 1 stone / 2 cloth / 3 trim / 4 tinted, cx, cy, cz, sx, sy, sz (game cm), yaw, pitch, roll (degrees), palette index or -1]; palette rows [r, g, b (sRGB), metal, rough, glow, pattern]; solids: [cx, cy, cz, sx, sy, sz, yaw] boxes you bump into; plants: [kind 0 tree / 1 sapling / 2 shrub, variant, cx, cy, cz (its base), yaw, scale].",
	cityTop: CITY_TOP, palette, blocks, solids, plants,
}));
console.log(`Hearthmoot: ${blocks.length} blocks, ${solids.length} solids, ${plants.length} plants (${PLAN.trees.length} meadow trees, ${hedge} hedge shrubs), ${stalks} reeds, ${palette.length} colours`);
console.log(`buildings: ${Object.entries(counts).map(([k, v]) => `${v} ${k}`).join(", ")}; ${quarterGreens.length} greens; fields ${PLAN.fields.length}, farms ${PLAN.farms.length}, windmills ${PLAN.windmills.length}`);
