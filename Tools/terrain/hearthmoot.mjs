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
// Earthy, weathered versions of the path colours (slate, tile, thatch-like tones), so a quarter reads as its path's
// without looking painted; plaster is mostly limewash, only tinged with the path.
const QUARTER_LOOK = {
	lightning: { roof: [96, 98, 136], plaster: [214, 212, 214] }, wind: { roof: [132, 150, 162], plaster: [222, 222, 218] },
	sun: { roof: [176, 136, 66], plaster: [228, 212, 176] }, nature: { roof: [82, 112, 62], plaster: [214, 204, 176] },
	arcane: { roof: [112, 82, 124], plaster: [214, 206, 208] }, frost: { roof: [124, 148, 164], plaster: [222, 224, 222] },
	blood: { roof: [124, 50, 44], plaster: [204, 184, 166] }, tide: { roof: [62, 112, 116], plaster: [214, 216, 204] },
	necromancy: { roof: [86, 84, 92], plaster: [188, 184, 178] }, spore: { roof: [116, 114, 70], plaster: [208, 200, 170] },
	void: { roof: [70, 60, 82], plaster: [176, 170, 168] }, fire: { roof: [164, 84, 56], plaster: [214, 186, 156] },
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
const blocks = [], nightBlocks = [], solids = [], plants = [], fires = [];
const cm = (v) => Math.round(v * 100);
const CUBE = 0, CYL = 1, CONE = 2, WOOD = 0, STONE = 1, CLOTH = 2, TINT = 4;
const frame = (x, y, z, yaw) => ({
	x, y, z, yaw,
	at(lx, ly, lz) { const c = Math.cos(yaw * RAD), s = Math.sin(yaw * RAD); return [x + lx * c - ly * s, y + lx * s + ly * c, z + lz]; },
});
// Anything standing on the ground (its bottom at 0) goes SINK m into it, so a dip under it never shows a gap.
const SINK = 2;
// night: the block shows only after dusk (lit windows, lantern glass): WRRoundIsland.cpp draws those under a root it
// shows and hides with the daylight.
function block(F, mesh, mat, [lx, ly, lz], [sx, sy, sz], { roll = 0, pitch = 0, yawAdd = 0, colour = -1, solid = false, nosink = false, night = false } = {}) {
	if (!nosink && roll === 0 && pitch === 0 && Math.abs(lz - sz / 2) < 0.15 && mesh !== CONE) { lz -= SINK / 2; sz += SINK; }
	const [x, y, z] = F.at(lx, ly, lz);
	(night ? nightBlocks : blocks).push([mesh, mat, cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1), +pitch.toFixed(1), +roll.toFixed(1), colour]);
	if (solid) solids.push([cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1)]);
}
// A fire or a light: [kind, cx, cy, cz (cm), scale]. Kinds (WRRoundIsland.cpp): 0 the hearth (burns day and night, its
// light always on), 1 a brazier (lit at dusk, a light), 2 a lantern's light (dusk, no flame), 3 a torch (dusk, no light).
const FIRE = { hearth: 0, brazier: 1, lantern: 2, torch: 3 };
const fire = (kind, [x, y, z], scale = 1) => fires.push([kind, cm(x), cm(y), cm(z), +scale.toFixed(2)]);
const plant = (kind, variant, x, y, yaw, scale) => plants.push([kind, variant, cm(x), cm(y), cm(groundAt(x, y) - 0.15), Math.round(yaw), +scale.toFixed(2)]);
const trunkSolid = (x, y, h = 4) => solids.push([cm(x), cm(y), cm(groundAt(x, y) + h / 2), 70, 70, cm(h), 0]);
const slate = pal([62, 66, 74], 0, 0.7), iron = pal([40, 40, 44], 0.8, 0.45, 0, 2), darkWood = pal([58, 40, 26], 0, 0.9, 0, 1), beam = pal([70, 50, 34], 0, 0.9, 0, 1);
const glass = pal([30, 34, 40], 0, 0.2, 0, 3), ember = pal([170, 60, 18], 0, 0.7, 1.4, 0), straw = pal([196, 168, 92], 0, 0.95, 0, 1);
// A lit window after dusk (a few shades of candle and hearth light) and a lantern's glass.
const litWindow = [pal([255, 196, 120], 0, 0.5, 1.1, 3), pal([255, 176, 96], 0, 0.5, 0.9, 3), pal([255, 214, 150], 0, 0.5, 1.3, 3)];
const lanternGlow = pal([255, 188, 104], 0, 0.4, 3.2, 3), soot = pal([34, 30, 28], 0, 0.95);
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
		// The faces broken up: a battered plinth both sides, a string course under the walk, and on the inner face a
		// buttress every other length (the town side was one flat, sunlit sheet).
		for (const s of [-1, 1]) block(F, CUBE, STONE, [0, s * (WALL.thick / 2 + 0.35), 0.9], [len, 0.7, 1.8]);
		block(F, CUBE, STONE, [0, WALL.thick / 2 + 0.2, WALL.height - 1.6], [len, 0.4, 0.35], { nosink: true });
		block(F, CUBE, STONE, [0, -WALL.thick / 2 - 0.2, WALL.height - 1.2], [len, 0.4, 0.35], { nosink: true });
		if (k % 2 === 0) {
			block(F, CUBE, STONE, [0, WALL.thick / 2 + 0.75, (WALL.height - 2) / 2], [1.4, 1.5, WALL.height - 2], { solid: true });
			block(F, CUBE, STONE, [0, WALL.thick / 2 + 0.6, WALL.height - 2.4], [1.4, 1.2, 1.2], { pitch: 0, roll: 40, nosink: true });
		}
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
		// Both faces: merlons, a string course, arrow slits (the town side was a blank slab).
		for (const m of [-3.5, 0, 3.5]) block(F, CUBE, STONE, [side * (GATE_HALF + 5) + m, 5.6, 19.9], [1.6, 0.9, 1.4]);
		block(F, CUBE, STONE, [side * (GATE_HALF + 5), 0, 12], [10.5, 11.5, 0.45], { nosink: true });
		for (const yy of [-5.53, 5.53]) for (const zz of [6.5, 15]) for (const xx of [-2.2, 2.2]) block(F, CUBE, TINT, [side * (GATE_HALF + 5) + xx, yy, zz], [0.35, 0.06, 1.4], { colour: soot, nosink: true });
		block(F, CUBE, CLOTH, [side * (GATE_HALF + 5), -5.8, 12], [4, 0.2, 7], { colour: cloth });
	}
	block(F, CUBE, STONE, [0, 0, 13.5], [2 * GATE_HALF + 0.6, 11, 5], { solid: true });
	// The raised portcullis under the outer arch, its bars' feet showing.
	block(F, CUBE, TINT, [0, -3.5, 10.4], [2 * GATE_HALF - 0.4, 0.4, 1.2], { colour: iron });
	for (let b = -GATE_HALF + 1; b < GATE_HALF; b += 1.4) block(F, CUBE, TINT, [b, -3.5, 9.6], [0.16, 0.16, 1.2], { colour: iron, nosink: true });
	// The gates: two leaves of oak swung back against the passage walls, iron-banded.
	for (const side of [-1, 1]) {
		block(F, CUBE, WOOD, [side * (GATE_HALF - 0.3), -1.2, 5.2], [0.35, 6.4, 10.4], { colour: darkWood });
		for (const z of [2, 5.2, 8.4]) block(F, CUBE, TINT, [side * (GATE_HALF - 0.5), -1.2, z], [0.08, 6.5, 0.3], { colour: iron, nosink: true });
	}
	// The town side: an arch ring standing proud, lamps either side of the passage, and torches on the outer towers.
	block(F, CUBE, STONE, [0, 5.7, 12.2], [2 * GATE_HALF + 2.4, 0.6, 1.6], { nosink: true });
	for (const side of [-1, 1]) {
		block(F, CUBE, STONE, [side * (GATE_HALF + 0.6), 5.7, 5.5], [1.2, 0.6, 11], { solid: true });
		const L = frame(...F.at(side * (GATE_HALF + 2.2), 6.1, 0).slice(0, 2), CITY_TOP, deg);
		block(L, CUBE, TINT, [0, 0, 4.2], [0.5, 0.5, 0.7], { colour: iron, nosink: true });
		block(L, CUBE, TINT, [0, 0, 4.2], [0.56, 0.38, 0.5], { colour: lanternGlow, nosink: true, night: true });
		fire(FIRE.lantern, L.at(0, 0, 4.2), 1);
		block(F, CUBE, TINT, [side * (GATE_HALF + 2.2), -5.65, 7.0], [0.16, 0.5, 0.9], { colour: iron, nosink: true });
		fire(FIRE.torch, F.at(side * (GATE_HALF + 2.2), -6.0, 7.6), 1.3);
	}
}

// ---- the plaza: the hearth, braziers, and the market ring ------------------------------------------------------------
{
	const F = frame(0, 0, CITY_TOP, 0);
	// The hearth: two steps of dressed stone, a kerb of big stones round the fire pit, a bed of embers, split logs
	// stacked in a cone, and the game's own hearth fire burning on them day and night (WRRoundIsland.cpp, fires).
	// Low enough that the fire shows over its kerb from where you stand on the plaza.
	block(F, CYL, STONE, [0, 0, 0.2], [13, 13, 0.4], { solid: true });
	block(F, CYL, STONE, [0, 0, 0.55], [10.4, 10.4, 0.3], { solid: true, nosink: true });
	for (let k = 0; k < 14; k++) {
		const a = k * 360 / 14 + R() * 6, [kx, ky] = polar(a, 3.6);
		block(frame(kx, ky, CITY_TOP + 0.7, a), CUBE, STONE, [0, 0, 0.25], [1.4, 0.9, 0.48 + R() * 0.14], { nosink: true });
	}
	block(F, CYL, TINT, [0, 0, 0.72], [6.2, 6.2, 0.06], { colour: soot, nosink: true });
	block(F, CYL, TINT, [0, 0, 0.78], [4.2, 4.2, 0.08], { colour: ember, nosink: true });
	for (let k = 0; k < 9; k++) {
		// (A frame turned to a bearing has x along the circle and y in toward the middle; a positive roll tips a
		// block's top toward +y, so the logs lean in over the fire.)
		const a = k * 40 + R() * 12, [lx, ly] = polar(a, 1.45), G = frame(lx, ly, CITY_TOP, a);
		block(G, CYL, WOOD, [0, 0, 1.78], [0.32, 0.32, 2.4], { roll: 30 + R() * 8, colour: k % 3 ? darkWood : beam, nosink: true });
	}
	fire(FIRE.hearth, [0, 0, CITY_TOP + 1.05], 2.6);
	// Four braziers round it on iron legs, lit at dusk.
	for (let k = 0; k < 4; k++) {
		const [bx, by] = polar(45 + 90 * k, 16), G = frame(bx, by, CITY_TOP, 45 + 90 * k);
		block(G, CYL, STONE, [0, 0, 0.25], [1.6, 1.6, 0.5], { solid: true });
		for (let l = 0; l < 3; l++) { const a = l * 120; block(frame(...G.at(Math.cos(a * RAD) * 0.42, Math.sin(a * RAD) * 0.42, 0).slice(0, 2), CITY_TOP, G.yaw + a + 90), CUBE, TINT, [0, 0, 1.05], [0.1, 0.1, 1.3], { roll: 12, colour: iron, nosink: true }); }
		block(G, CONE, TINT, [0, 0, 1.78], [1.5, 1.5, 0.6], { pitch: 180, colour: iron });
		block(G, CYL, TINT, [0, 0, 2.06], [1.42, 1.42, 0.06], { colour: soot, nosink: true });
		block(G, CYL, TINT, [0, 0, 2.08], [1.0, 1.0, 0.05], { colour: ember, nosink: true });
		fire(FIRE.brazier, G.at(0, 0, 2.08), 0.8);
	}
	const flags = [[150, 144, 132], [136, 130, 120], [158, 150, 136], [126, 122, 116], [144, 136, 122], [118, 112, 104]].map((c) => pal(c, 0, 0.92, 0, 0));
	// The plaza paved: rings of flagstones round the hearth, each ring cut into stones about 1.7 m long, laid a hand
	// above the ground with thin dark joints between (the ground's own plaza colour shows in them).
	for (let r = 7.5; r < PLAN.PLAZA_R - 0.6; r += 1.6) {
		const n = Math.max(8, Math.round(2 * Math.PI * r / 1.7)), off = R() * 360;
		for (let s = 0; s < n; s++) {
			const a = off + s * 360 / n, [px, py] = polar(a, r), w = 2 * Math.PI * r / n - 0.07;
			block(frame(px, py, CITY_TOP, a), CUBE, TINT, [0, 0, -0.04], [w, 1.52, 0.16], { nosink: true, colour: pick(flags) });
		}
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
	// Tried at several depths and across the quarter's width, so every quarter gets one.
	let greened = false;
	for (const [rr, off] of [205, 240, 120, 260, 180, 150, 225].flatMap((r) => [0, 5, -5, 9, -9, 12, -12].map((o) => [r, o]))) {
		const deg = mid + off, c = polar(deg, rr), fp = { c, u: [1, 0], hw: 13, hd: 13 };
		if (!free(fp, 1) || !offStreets(fp, 0.3) || !insideCity(fp)) continue;
		greened = true;
		reserve(fp);
		wellAt(c);
		for (let t = 0; t < 3; t++) { const a = R() * 360, p = [c[0] + Math.cos(a * RAD) * (6 + 4 * R()), c[1] + Math.sin(a * RAD) * (6 + 4 * R())]; plant(0, Math.floor(R() * 8), p[0], p[1], R() * 360, 0.5 + 0.2 * R()); trunkSolid(p[0], p[1]); }
		for (const s of [-1, 1]) block(frame(c[0] + s * 4, c[1] + 3, CITY_TOP, 0), CUBE, WOOD, [0, 0, 0.25], [2, 0.5, 0.5], { colour: beam });
		quarterGreens.push(c);
		break;
	}
	if (!greened) console.warn(`no room for the ${q.path} quarter's green`);
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

// ---- street lanterns along the avenues and the ring street: a post, an arm, a lantern lit at dusk -----------------------
let lanterns = 0;
for (const st of PLAN.streets.filter((s) => s.kind === "avenue" || s.kind === "ring")) {
	let L = 0, side = 1;
	for (let i = 1; i < st.pts.length; i++) {
		const a = st.pts[i - 1], b = st.pts[i], seg = Math.hypot(b[0] - a[0], b[1] - a[1]), d = [(b[0] - a[0]) / seg, (b[1] - a[1]) / seg];
		L += seg;
		if (L < 26) continue;
		const r = Math.hypot(...b);
		if (r > 262 || r < PLAN.PLAZA_R + 8) continue;
		const p = [b[0] - d[1] * side * (st.half + 0.9), b[1] + d[0] * side * (st.half + 0.9)];
		if (!free({ c: p, u: [1, 0], hw: 0.6, hd: 0.6 }, 0.3)) continue;
		L = 0;
		reserveCircle(p, 0.7);
		// Its x toward the street's middle: the arm reaches out over the street.
		const G = frame(p[0], p[1], CITY_TOP, Math.atan2(-d[0] * side, d[1] * side) / RAD);
		block(G, CYL, STONE, [0, 0, 0.2], [0.5, 0.5, 0.4]);
		block(G, CYL, WOOD, [0, 0, 1.9], [0.18, 0.18, 3.4], { colour: darkWood, nosink: true });
		block(G, CUBE, TINT, [0.45, 0, 3.45], [0.95, 0.08, 0.08], { colour: iron, nosink: true });
		block(G, CUBE, TINT, [0.85, 0, 3.05], [0.36, 0.36, 0.5], { colour: iron, nosink: true });
		block(G, CONE, TINT, [0.85, 0, 3.42], [0.46, 0.46, 0.24], { colour: iron });
		block(G, CUBE, TINT, [0.85, 0, 3.05], [0.4, 0.26, 0.38], { colour: lanternGlow, nosink: true, night: true });
		block(G, CUBE, TINT, [0.85, 0, 3.05], [0.26, 0.4, 0.38], { colour: lanternGlow, nosink: true, night: true });
		if (lanterns % 4 === 0) fire(FIRE.lantern, G.at(0.85, 0, 3.0), 1);
		lanterns++;
		side = -side;
	}
}

// ---- the houses: every kind, fronting the nearest street on a plot of its own -----------------------------------------
const KINDS = [["cottage", 30], ["house", 34], ["townhouse", 12], ["lhouse", 9], ["workshop", 8], ["tower", 4], ["garden", 3]];
const pickKind = () => { let t = R() * 100; for (const [k, w] of KINDS) { if ((t -= w) < 0) return k; } return "house"; };
const SIZES = { cottage: [[7, 9], [6, 7.5]], house: [[9, 12], [7, 9]], townhouse: [[6, 7.5], [9, 11.5]], lhouse: [[10, 13], [7, 8.5]], workshop: [[12, 15.5], [8.5, 10]], tower: [[6, 7], [6, 7]], garden: [[11, 15], [10, 13]] };
// Windows on the street face: dark glass in a timber frame with a sill, now and then a shutter; after dusk about two
// in five of them glow with the light of the room behind.
function windows(F, w, d, z, n, h = 1.2) {
	for (let i = 0; i < n; i++) {
		const x = -w / 2 + w * (i + 0.5) / n;
		if (R() < 0.85) {
			block(F, CUBE, TINT, [x, d / 2 + 0.05, z], [0.95, 0.12, h], { colour: glass });
			block(F, CUBE, TINT, [x, d / 2 + 0.07, z + h / 2 + 0.06], [1.15, 0.14, 0.12], { colour: beam, nosink: true });
			block(F, CUBE, TINT, [x, d / 2 + 0.12, z - h / 2 - 0.06], [1.2, 0.24, 0.12], { colour: beam, nosink: true });
			block(F, CUBE, TINT, [x, d / 2 + 0.09, z], [0.08, 0.1, h], { colour: beam, nosink: true });
			if (R() < 0.42) block(F, CUBE, TINT, [x, d / 2 + 0.115, z], [0.9, 0.02, h - 0.06], { colour: pick(litWindow), nosink: true, night: true });
		}
		if (R() < 0.35) block(F, CUBE, WOOD, [x + 0.7, d / 2 + 0.06, z], [0.4, 0.1, h + 0.1], { colour: darkWood });
	}
}
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
	// Weatherboarded: a stone footing, boards in two tones, corner posts, big double doors with a hayloft door above,
	// and a thatch-coloured roof.
	const boards = pal([104, 76, 52], 0, 0.9, 0, 1);
	block(B, CUBE, STONE, [0, 0, 0.6], [16, 10, 1.2], { solid: true });
	block(B, CUBE, TINT, [0, 0, 3.6], [15.8, 9.8, 4.8], { solid: true, colour: boards, nosink: true });
	for (let x = -7.2; x <= 7.2; x += 0.9) block(B, CUBE, TINT, [x, 4.93, 3.6], [0.12, 0.08, 4.8], { colour: beam, nosink: true });
	for (const x of [-7.95, 7.95]) for (const y of [-4.95, 4.95]) block(B, CUBE, TINT, [x, y, 3.6], [0.35, 0.35, 4.9], { colour: darkWood, nosink: true });
	block(B, CUBE, TINT, [0, 0, 6.06], [16.2, 10.2, 0.25], { colour: darkWood, nosink: true });
	gableRoof(B, 16.6, 10.6, 6.1, pal([112, 94, 64], 0, 0.95, 0, 1));
	for (const s of [-1, 1]) block(B, CUBE, WOOD, [s * 1.05, 5.02, 2.4], [2.0, 0.16, 3.6], { colour: darkWood, nosink: true });
	block(B, CUBE, TINT, [0, 5.06, 4.25], [4.4, 0.18, 0.3], { colour: beam, nosink: true });
	block(B, CUBE, WOOD, [5, 5.02, 4.7], [1.4, 0.16, 1.2], { colour: darkWood, nosink: true });
	for (let k = 0; k < 3; k++) { const [hx, hy] = F.at(-2 + k * 3.2, 9, 0); block(frame(hx, hy, groundAt(hx, hy), 0), CYL, TINT, [0, 0, 1.2], [2.6, 2.6, 2.4], { colour: straw, solid: true }); }
	for (let k = 0; k <= 10; k++) { const [px, py] = F.at(-16 + k * 3.2, 14, 0); block(frame(px, py, groundAt(px, py), f.yaw), CUBE, WOOD, [0, 0, 0.6], [0.16, 0.16, 1.2], { colour: beam }); }
	for (let k = 0; k < 4; k++) { const [tx, ty] = F.at(-20 + k * 5 + R() * 2, -12 - R() * 6, 0); plant(0, Math.floor(R() * 8), tx, ty, R() * 360, 0.8 + 0.25 * R()); trunkSolid(tx, ty); }
}
for (const m of PLAN.windmills) {
	// A tower mill: a tapering stone tower in three courses, a gallery, a timber cap, and four sails on a hub facing the
	// wind (its front is the frame's +y). A sail is a stock from the hub with a lattice frame and its canvas on one side.
	const z = groundAt(...m.at), F = frame(m.at[0], m.at[1], z, m.yaw);
	block(F, CYL, STONE, [0, 0, 2.5], [8, 8, 5], { solid: true });
	block(F, CYL, STONE, [0, 0, 6.6], [7.2, 7.2, 3.4], { nosink: true });
	block(F, CYL, STONE, [0, 0, 9.9], [6.4, 6.4, 3.2], { nosink: true });
	block(F, CYL, WOOD, [0, 0, 11.65], [8.2, 8.2, 0.3], { colour: beam, nosink: true });
	for (let k = 0; k < 16; k++) { const a = k * 22.5; block(F, CUBE, WOOD, [Math.cos(a * RAD) * 3.95, Math.sin(a * RAD) * 3.95, 12.3], [0.1, 0.1, 1], { colour: beam, nosink: true }); }
	block(F, CUBE, WOOD, [0, 0, 13.1], [5.2, 6.4, 2.6], { colour: pal([112, 84, 58], 0, 0.9, 0, 1), nosink: true });
	gableRoof(F, 5.2, 6.8, 14.4, pal([74, 62, 54], 0, 0.85), 0, 0, 1);
	block(F, CUBE, WOOD, [0, 4.05, 1.15], [1.4, 0.2, 2.3], { colour: darkWood });
	windows(F, 4, 7.4, 6.8, 1, 1);
	const hub = [0, 3.9, 13.2], sailCloth = pal([214, 204, 182], 0, 0.95), turn = 17;
	block(F, CYL, WOOD, hub, [0.8, 0.8, 1.4], { roll: 90, colour: darkWood, nosink: true });
	for (let k = 0; k < 4; k++) {
		// pitch tilts a block's x up toward z: the sail's length points out from the hub at angle a round it.
		const a = 90 * k + turn, c = Math.cos(a * RAD), s = Math.sin(a * RAD), along = (r) => [hub[0] + c * r, hub[1] + 0.35, hub[2] + s * r];
		block(F, CUBE, WOOD, along(5.6), [11.2, 0.22, 0.32], { pitch: a, colour: beam, nosink: true });
		const sideOff = (r, o) => [hub[0] + c * r - s * o, hub[1] + 0.45, hub[2] + s * r + c * o];
		block(F, CUBE, TINT, sideOff(6.4, 0.95), [8.4, 0.06, 1.7], { pitch: a, colour: sailCloth, nosink: true });
		block(F, CUBE, WOOD, sideOff(6.4, 1.85), [8.6, 0.12, 0.12], { pitch: a, colour: beam, nosink: true });
		for (let r = 2.4; r <= 10.6; r += 1.37) block(F, CUBE, WOOD, sideOff(r, 0.95), [0.1, 0.12, 1.9], { pitch: a, colour: beam, nosink: true });
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
		for (const m of [-3.9, -1.3, 1.3, 3.9]) block(F, CUBE, STONE, [end * S / 2 + end * 0.2, m, Hh + 3.6], [T + 0.2, 1.3, 1.2], { nosink: true });
		// Its gates swung open against the passage, and a torch either side of the arch on the outer face.
		for (const s of [-1, 1]) {
			block(F, CUBE, WOOD, [end * (S / 2 - T / 2 - gate / 2), s * (gate - 0.25), 3.2], [gate - 0.2, 0.3, 6.4], { colour: darkWood });
			fire(FIRE.torch, F.at(end * (S / 2 + T / 2 + 0.35), s * (gate + 1.1), 4.6), 1.2);
			block(F, CUBE, TINT, [end * (S / 2 + T / 2 + 0.12), s * (gate + 1.1), 4.1], [0.3, 0.16, 0.9], { colour: iron, nosink: true });
		}
	}
	// The towers: arrow slits on their outer faces, a pennant over each, the fort's braziers on the gate tops.
	for (const ex of [-1, 1]) for (const sy of [-1, 1]) {
		for (const zz of [3.5, 7]) {
			block(F, CUBE, TINT, [ex * (S / 2 + 4.02), sy * S / 2, zz], [0.06, 0.4, 1.4], { colour: soot, nosink: true });
			block(F, CUBE, TINT, [ex * S / 2, sy * (S / 2 + 4.02), zz], [0.4, 0.06, 1.4], { colour: soot, nosink: true });
		}
		block(F, CYL, WOOD, [ex * S / 2, sy * S / 2, 17.4], [0.16, 0.16, 4], { colour: darkWood, nosink: true });
		block(F, CUBE, CLOTH, [ex * S / 2 + 0.9, sy * S / 2, 18.6], [1.8, 0.06, 1.1], { colour: cloth, nosink: true });
	}
	for (const end of [-1, 1]) fire(FIRE.brazier, F.at(end * S / 2, 0, Hh + 3.05), 0.7);
	// Inside: a barracks along one wall, a well, a banner.
	block(F, CUBE, STONE, [0, -S / 2 + 5.5, 2.2], [S - 12, 7, 4.4], { solid: true });
	gableRoof(F, S - 12, 7, 4.4, slate, 0, -S / 2 + 5.5);
	block(F, CYL, WOOD, [6, S / 2 - 5, 6], [0.35, 0.35, 12]);
	block(F, CUBE, CLOTH, [6.9, S / 2 - 5, 9.5], [1.8, 0.12, 4], { colour: cloth });
	// The waymark: a standing stone on a low cairn beside the crossroads, and a signpost with an arm down every road
	// that meets there (to Hearthmoot, and to each land's road), a lantern on top lit at dusk.
	const [cx, cy] = t.crossroads;
	const offRoad = (p, m) => PLAN.roads.every((rd) => PLAN.lineDist(p, rd.pts).d > rd.half + m);
	const spotNear = (r0, not) => { for (let rr = r0; rr < r0 + 30; rr += 2) for (let a = 0; a < 360; a += 15) { const p = [cx + Math.cos(a * RAD) * rr, cy + Math.sin(a * RAD) * rr]; if (offRoad(p, 2.5) && (!not || Math.hypot(p[0] - not[0], p[1] - not[1]) > 5)) return p; } return [cx + r0, cy]; };
	const [sx0, sy0] = spotNear(9), [px0, py0] = spotNear(7, [sx0, sy0]), G = frame(sx0, sy0, groundAt(sx0, sy0), t.bearing);
	for (let k = 0; k < 9; k++) { const a = k * 40, rr = 1.6 + R() * 0.6; block(frame(sx0 + Math.cos(a * RAD) * rr, sy0 + Math.sin(a * RAD) * rr, G.z, a), CUBE, STONE, [0, 0, 0.3], [0.9 + R() * 0.4, 0.7, 0.6 + R() * 0.3], { roll: (R() - 0.5) * 16 }); }
	block(G, CUBE, STONE, [0, 0, 1.6], [1.3, 0.9, 3.2], { solid: true, roll: 3 });
	block(G, CUBE, STONE, [0, 0.05, 3.55], [1.0, 0.75, 0.9], { roll: -4, nosink: true, pitch: 5 });
	const P = frame(px0, py0, groundAt(px0, py0), 0), dirs = [];
	const toward = (p) => Math.atan2(p[1] - py0, p[0] - px0) / RAD;
	dirs.push(toward(t.points[Math.min(3, t.points.length - 1)]));   // the main road on to Hearthmoot
	for (const l of RI.lands) {
		if (!l.road || l.road.length < 2) continue;
		const ends = [l.road[0], l.road[l.road.length - 1]];
		const near = ends.map((e) => Math.hypot(e[0] - cx, e[1] - cy));
		if (Math.min(...near) > 60) continue;
		const pts = near[0] < near[1] ? l.road : [...l.road].reverse();
		dirs.push(toward(pts[Math.min(4, pts.length - 1)]));
	}
	block(P, CYL, WOOD, [0, 0, 1.9], [0.22, 0.22, 3.8], { colour: darkWood });
	dirs.forEach((a, i) => {
		const z = 3.1 - i * 0.42, c = Math.cos(a * RAD), s = Math.sin(a * RAD);
		block(frame(px0 + c * 0.75, py0 + s * 0.75, P.z, a), CUBE, TINT, [0, 0, z], [1.3, 0.06, 0.3], { colour: pal([150, 120, 84], 0, 0.9, 0, 1), nosink: true });
		block(frame(px0 + c * 1.45, py0 + s * 1.45, P.z, a + 45), CUBE, TINT, [0, 0, z], [0.21, 0.21, 0.3], { colour: pal([150, 120, 84], 0, 0.9, 0, 1), nosink: true });
	});
	block(P, CUBE, TINT, [0, 0, 3.95], [0.32, 0.32, 0.4], { colour: iron, nosink: true });
	block(P, CUBE, TINT, [0, 0, 3.95], [0.36, 0.22, 0.32], { colour: lanternGlow, nosink: true, night: true });
	block(P, CONE, TINT, [0, 0, 4.27], [0.42, 0.42, 0.24], { colour: iron });
}

fs.writeFileSync(path.join(root, "Data/hearthmoot.json"), JSON.stringify({
	_comment: "Generated by Tools/terrain/hearthmoot.mjs from the shared plan (Tools/terrain/hearthmoot_plan.mjs; edit the scripts, not this file). Hearthmoot and its meadow, built on every machine by WRRoundIsland.cpp (no replication). blocks: [mesh 0 cube / 1 cylinder / 2 cone, mat 0 wood / 1 stone / 2 cloth / 3 trim / 4 tinted, cx, cy, cz, sx, sy, sz (game cm), yaw, pitch, roll (degrees), palette index or -1]; palette rows [r, g, b (sRGB), metal, rough, glow, pattern]; nightBlocks: the same, shown only after dusk (lit windows, lantern glass); fires: [kind 0 hearth / 1 brazier / 2 lantern light / 3 torch, cx, cy, cz, scale]; solids: [cx, cy, cz, sx, sy, sz, yaw] boxes you bump into; plants: [kind 0 tree / 1 sapling / 2 shrub, variant, cx, cy, cz (its base), yaw, scale].",
	cityTop: CITY_TOP, palette, blocks, nightBlocks, fires, solids, plants,
}));
console.log(`Hearthmoot: ${blocks.length} blocks, ${nightBlocks.length} night blocks, ${fires.length} fires (${lanterns} street lanterns), ${solids.length} solids, ${plants.length} plants (${PLAN.trees.length} meadow trees, ${hedge} hedge shrubs), ${stalks} reeds, ${palette.length} colours`);
console.log(`buildings: ${Object.entries(counts).map(([k, v]) => `${v} ${k}`).join(", ")}; ${quarterGreens.length} greens; fields ${PLAN.fields.length}, farms ${PLAN.farms.length}, windmills ${PLAN.windmills.length}`);
