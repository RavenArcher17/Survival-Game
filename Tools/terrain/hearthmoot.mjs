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
// What each quarter roofs with (11-22, "everything as good as the caves": the roofs were flat sheets of the path's
// colour). The patterns are M_BuildTinted's (Tools/make_build_materials_tinted.py): 5 clay pantiles, 6 thatch, 7 wooden
// shingles, 8 slate; plaster is 4 (limewashed render). The roof wears its material's own colour, only tinged with the
// path's.
const ROOF_KIND = { lightning: "slate", wind: "tile", sun: "thatch", nature: "thatch", arcane: "slate", frost: "shingle", blood: "tile",
	tide: "shingle", necromancy: "slate", spore: "thatch", void: "slate", fire: "tile" };
const ROOF_LOOK = { tile: { rgb: [164, 86, 56], pattern: 5, rough: 0.75 }, thatch: { rgb: [146, 124, 82], pattern: 6, rough: 0.97 },
	shingle: { rgb: [116, 102, 86], pattern: 7, rough: 0.9 }, slate: { rgb: [72, 76, 84], pattern: 8, rough: 0.6 } };
const PLASTER = 4;
for (const [path, look] of Object.entries(QUARTER_LOOK)) look.kind = ROOF_KIND[path];

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
const CUBE = 0, CYL = 1, CONE = 2, SPH = 4, AWN = 5, HAY = 6, BOAT = 7, WOOD = 0, STONE = 1, CLOTH = 2, TINT = 4;   // (5-7: generated shapes, make_meshes.py)
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
// Kinds 4 and 5 light rooms you walk into: a fireplace (burns always, a warm light) and a lamp (a light, always on).
FIRE.fireplace = 4; FIRE.lamp = 5;
// Floors you walk on indoors: no grass grows through them (WRRoundIsland StepCover): [cx, cy, half x, half y, yaw].
const floors = [];
const floorBox = (F, cx, cy, hx, hy) => { const [x, y] = F.at(cx, cy, 0); floors.push([cm(x), cm(y), cm(hx), cm(hy), +F.yaw.toFixed(1)]); };
// The city's people (WRRoundIsland spawns them on the server): walks (routes the townsfolk stroll), walkers on them,
// market sellers, and people who stand at their post (guards, stewards, idlers, the inn's patrons).
const people = { walks: [], walkers: [], sellers: [], standers: [] };
const FIRST_M = ["Aldric", "Bram", "Cedric", "Dunstan", "Edwin", "Fenn", "Garrick", "Hal", "Ivo", "Jory", "Kester", "Leof", "Merek", "Nyle", "Osric", "Piers", "Rowe", "Sim", "Tobin", "Wat"];
const FIRST_F = ["Alys", "Bryony", "Cate", "Della", "Edda", "Freya", "Gwen", "Hilde", "Isolde", "Joan", "Kit", "Linnet", "Maud", "Nell", "Odette", "Pell", "Rosa", "Sabine", "Tilda", "Wenna"];
// Everyday clothes (player_art.json townwear / towngown looks: linen, wool and leather; 11-22: the adventurer sets they
// wore showed skin windows and read as underwear). Six faces a body (WRHeroLook clamps past 5: R()*8 put most on F6/M6).
const OUTFITS = ["townwear", "townwear", "townwear_b", "townwear_c", "towngown", "towngown_b"];
// (round 8: the gowns are the gravecloth's cut, and on a man it's a bare-chested wrap: the hall's steward stood shirtless.
// A man drawn a gown wears a coat instead; no extra draws, so nobody else in the town changes.)
const manly = (o, sex) => sex === "m" && o.startsWith("towngown") ? (o === "towngown_b" ? "townwear_c" : "townwear_b") : o;
const person = (outfit) => { const sex = R() < 0.5 ? "m" : "f", o = manly(outfit || pick(OUTFITS), sex); return { sex, name: pick(sex === "m" ? FIRST_M : FIRST_F), face: Math.floor(R() * 6), build: Math.floor(R() * 3), outfit: o.startsWith("town") ? (R(), o) : o !== "iron" && R() < 0.4 ? o + "+" : o }; };
const yawOf = (F, lx, ly) => { const a = F.at(0, 0, 0), b = F.at(lx, ly, 0); return +(Math.atan2(b[1] - a[1], b[0] - a[0]) / RAD).toFixed(1); };
// [x, y, z (cm), yaw, role, name, sex, face, build, outfit]
const stand = (F, lx, ly, faceX, faceY, role, p, title) => { const [x, y, z] = F.at(lx, ly, 0); people.standers.push([cm(x), cm(y), cm(z), yawOf(F, faceX, faceY), role, title ? `${p.name} ${title}` : p.name, p.sex, p.face, p.build, p.outfit]); };

// A wall from a to b (the frame's own metres) with openings [{at: metres from a, w, z0, z1}]: solid pieces between
// them, a sill under each and a lintel over; thick, h tall, on the floor.
function wall(F, [ax, ay], [bx, by], thick, h, opens = [], opts = {}) {
	const L = Math.hypot(bx - ax, by - ay), yawAdd = Math.atan2(by - ay, bx - ax) / RAD;
	const W = frame(...F.at(ax, ay, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	const os = [...opens].sort((p, q) => p.at - q.at);
	let x = 0;
	const piece = (x0, x1, z0, z1) => { if (x1 - x0 < 0.05 || z1 - z0 < 0.05) return; block(W, CUBE, opts.mat ?? STONE, [(x0 + x1) / 2, 0, (z0 + z1) / 2], [x1 - x0, thick, z1 - z0], { solid: true, colour: opts.colour ?? -1, nosink: z0 > 0.01 }); };
	for (const o of os) {
		piece(x, o.at - o.w / 2, 0, h);
		piece(o.at - o.w / 2, o.at + o.w / 2, 0, o.z0);
		piece(o.at - o.w / 2, o.at + o.w / 2, o.z1, h);
		// The opening's timber frame (a sill and the head), both faces.
		if (o.z0 > 0.3) block(W, CUBE, TINT, [o.at, 0, o.z0 + 0.04], [o.w + 0.2, thick + 0.12, 0.1], { colour: beam, nosink: true });
		block(W, CUBE, TINT, [o.at, 0, o.z1 - 0.06], [o.w + 0.2, thick + 0.1, 0.14], { colour: beam, nosink: true });
		x = o.at + o.w / 2;
	}
	piece(x, L, 0, h);
}
// A roof you can stand under: two pitched slabs (45 degrees) over a room halfD deep each side of its ridge, the ridge
// along the frame's x from x0 to x1 at wall top z; gable ends filled at both ends (a diamond kept inside the end
// walls' thickness, so its lower half never shows indoors). Tile courses on the slabs.
function openRoof(F, x0, x1, cy, halfD, z, roof, endThick) {
	const over = 0.6, run = halfD + over, slab = run * Math.SQRT2, len = x1 - x0 + 2 * over, cx = (x0 + x1) / 2, th = 0.3;
	for (const sd of [-1, 1]) {
		// Its plane rises from the eave (y = sd * run, z - over) to the ridge (y = 0, z + halfD).
		const my = cy + sd * run / 2, mz = z + (halfD - over) / 2;
		const nY = sd / Math.SQRT2, nZ = 1 / Math.SQRT2;
		block(F, CUBE, TINT, [cx, my + nY * th / 2, mz + nZ * th / 2], [len, slab, th], { roll: sd * 45, colour: roof, nosink: true });
	}
	// (The roof's own pattern draws its courses: the darker strips laid on it went.)
	block(F, CUBE, TINT, [cx, cy, z + halfD + 0.3], [len, 0.42, 0.42], { roll: 45, colour: darker(roof, 0.85), nosink: true });
	for (const ex of [x0, x1]) block(F, CUBE, STONE, [ex, cy, z], [endThick - 0.06, halfD * Math.SQRT2, halfD * Math.SQRT2], { roll: 45, nosink: true });
	// (round 9: indoors the roof was a bare slab) its trusses: a tie beam across the walls' tops, a king post up to the
	// ridge, a rafter under each slab, and a purlin along each side and the ridge.
	const bays = Math.max(2, Math.round((x1 - x0) / 3.6)), wood = darkWood;
	for (let b = 1; b < bays; b++) {
		const x = x0 + (x1 - x0) * b / bays;
		block(F, CUBE, TINT, [x, cy, z - 0.18], [0.3, 2 * halfD, 0.36], { colour: wood, nosink: true });
		block(F, CUBE, TINT, [x, cy, z + halfD / 2 - 0.2], [0.24, 0.24, halfD - 0.3], { colour: wood, nosink: true });
		for (const sd of [-1, 1]) {
			block(F, CUBE, TINT, [x, cy + sd * (halfD / 2 + 0.12), z + halfD / 2 - 0.12], [0.24, halfD * Math.SQRT2 - 0.3, 0.26], { roll: sd * 45, colour: wood, nosink: true });
			block(F, CUBE, TINT, [x, cy + sd * halfD * 0.42, z + halfD * 0.22 - 0.05], [0.16, halfD * 0.62, 0.18], { roll: -sd * 30, colour: wood, nosink: true });
		}
	}
	for (const sd of [-1, 1]) block(F, CUBE, TINT, [cx, cy + sd * (halfD * 0.5 + 0.12), z + halfD * 0.5 - 0.42], [x1 - x0 - 0.2, 0.22, 0.24], { roll: sd * 45, colour: wood, nosink: true });
	block(F, CUBE, TINT, [cx, cy, z + halfD - 0.45], [x1 - x0 - 0.2, 0.24, 0.26], { colour: wood, nosink: true });
}
const floorWood = pal([90, 74, 58], 0, 0.9, 0, 1), cloth2 = pal([120, 36, 34], 0, 0.95, 0, 9);
function table(F, x, y, len, wid, along = 0) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + along);
	block(G, CUBE, TINT, [0, 0, 0.92], [len, wid, 0.1], { colour: darkWood, nosink: true, solid: true });
	for (const sx of [-1, 1]) for (const sy of [-1, 1]) block(G, CUBE, TINT, [sx * (len / 2 - 0.2), sy * (wid / 2 - 0.15), 0.46], [0.12, 0.12, 0.88], { colour: darkWood, nosink: true });
	// Benches: a seat plank on trestle boards (11-22: they were solid boxes).
	for (const sy of [-1, 1]) {
		block(G, CUBE, TINT, [0, sy * (wid / 2 + 0.45), 0.45], [len - 0.4, 0.34, 0.06], { colour: beam, nosink: true });
		const n = Math.max(2, Math.round(len / 3));
		for (let k = 0; k < n; k++) block(G, CUBE, TINT, [-(len - 1) / 2 + k * (len - 1) / (n - 1), sy * (wid / 2 + 0.45), 0.21], [0.06, 0.3, 0.42], { colour: darkWood, nosink: true });
	}
}
// A bench: a seat plank on two legs and a stretcher (11-22: benches were solid boxes).
function bench(F, x, y, len, along = 0) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + along);
	block(G, CUBE, TINT, [0, 0, 0.44], [len, 0.36, 0.06], { colour: beam, nosink: true });
	for (const sx of [-1, 1]) block(G, CUBE, TINT, [sx * (len / 2 - 0.25), 0, 0.21], [0.07, 0.32, 0.42], { colour: darkWood, nosink: true });
	block(G, CUBE, TINT, [0, 0, 0.13], [len - 0.5, 0.05, 0.06], { colour: darkWood, nosink: true });
}
// A door in a wall face (y = the face): boards on two ledges, a frame of jambs and a lintel, an iron latch (11-22: one
// plank slab stuck out from the wall).
function door(F, x, yFace, w, h) {
	const n = Math.max(3, Math.round(w / 0.28)), bw = w / n;
	for (let k = 0; k < n; k++) block(F, CUBE, TINT, [x - w / 2 + bw * (k + 0.5), yFace + 0.04, h / 2], [bw - 0.012, 0.06, h], { colour: darkWood, nosink: true });
	for (const z of [0.35, h - 0.4]) block(F, CUBE, TINT, [x, yFace + 0.08, z], [w - 0.1, 0.03, 0.14], { colour: darkWood, nosink: true });
	for (const sx of [-1, 1]) block(F, CUBE, TINT, [x + sx * (w / 2 + 0.07), yFace + 0.06, (h + 0.12) / 2], [0.14, 0.14, h + 0.12], { colour: beam, nosink: true });
	block(F, CUBE, TINT, [x, yFace + 0.06, h + 0.12], [w + 0.34, 0.16, 0.18], { colour: beam, nosink: true });
	block(F, CUBE, TINT, [x + w * 0.32, yFace + 0.1, h * 0.5], [0.05, 0.03, 0.12], { colour: iron, nosink: true });
}
// A three-legged stool: a round seat on splayed legs (11-22: a solid cylinder read as a log stump).
function stool(F, x, y, h = 0.46) {
	block(F, CYL, TINT, [x, y, h], [0.42, 0.42, 0.05], { colour: beam, nosink: true });
	for (let k = 0; k < 3; k++) {
		const a = k * 120 + 20;
		block(F, CUBE, TINT, [x + Math.cos(a * RAD) * 0.15, y + Math.sin(a * RAD) * 0.15, h / 2 - 0.01], [0.05, 0.05, h + 0.02], { yawAdd: a, pitch: 9, colour: darkWood, nosink: true });
	}
}
// A round tavern table: a top on a post and crossed feet, a tankard or two and a plate on it (its own random
// numbers, so the rest of the city keeps its layout).
const RT = PLAN.rng(20261122);
function roundTable(F, x, y) {
	block(F, CYL, TINT, [x, y, 0.88], [1.3, 1.3, 0.06], { colour: darkWood, nosink: true, solid: true });
	block(F, CYL, TINT, [x, y, 0.44], [0.14, 0.14, 0.84], { colour: darkWood, nosink: true });
	for (const a of [0, 90]) block(F, CUBE, TINT, [x, y, 0.05], [1.0, 0.1, 0.1], { yawAdd: a + 20, colour: darkWood, nosink: true });
	const n = 1 + Math.floor(RT() * 2);
	for (let k = 0; k < n; k++) {
		const a = (RT() * 360) * RAD, r = 0.25 + RT() * 0.25, tx = x + Math.cos(a) * r, ty = y + Math.sin(a) * r;
		block(F, CYL, TINT, [tx, ty, 0.98], [0.1, 0.1, 0.14], { colour: k ? pal([120, 90, 60], 0, 0.6, 0, 3) : darkWood, nosink: true });
		block(F, CUBE, TINT, [tx + Math.cos(a) * 0.07, ty + Math.sin(a) * 0.07, 0.98], [0.03, 0.015, 0.09], { yawAdd: a / RAD, colour: iron, nosink: true });
	}
	if (RT() < 0.6) block(F, CYL, TINT, [x - 0.2, y + 0.15, 0.915], [0.26, 0.26, 0.015], { colour: pal([150, 140, 124], 0, 0.5, 0, 3), nosink: true });
}
function lampHang(F, x, y, z, top) {
	block(F, CUBE, TINT, [x, y, (z + top) / 2], [0.05, 0.05, top - z], { colour: iron, nosink: true });
	block(F, CYL, TINT, [x, y, z], [1.4, 1.4, 0.08], { colour: iron, nosink: true });
	for (let k = 0; k < 6; k++) { const a = k * 60 * RAD; block(F, CYL, TINT, [x + Math.cos(a) * 0.62, y + Math.sin(a) * 0.62, z + 0.14], [0.07, 0.07, 0.2], { colour: lanternGlow, nosink: true }); }
	fire(FIRE.lamp, F.at(x, y, z - 0.3), 1);
}
const plant = (kind, variant, x, y, yaw, scale) => plants.push([kind, variant, cm(x), cm(y), cm(groundAt(x, y) - 0.15), Math.round(yaw), +scale.toFixed(2)]);
const trunkSolid = (x, y, h = 4) => solids.push([cm(x), cm(y), cm(groundAt(x, y) + h / 2), 70, 70, cm(h), 0]);
const slate = pal([62, 66, 74], 0, 0.6, 0, 8), iron = pal([40, 40, 44], 0.8, 0.45, 0, 2), darkWood = pal([58, 40, 26], 0, 0.9, 0, 1), beam = pal([70, 50, 34], 0, 0.9, 0, 1);
const glass = pal([30, 34, 40], 0, 0.2, 0, 3), ember = pal([170, 60, 18], 0, 0.7, 1.4, 0), straw = pal([196, 168, 92], 0, 0.95, 0, 1);
// A lit window after dusk: the room behind it, a dim warm glow filling the pane (a few shades of candle and hearth light),
// brighter on the side its lamp or fire is; now and then a curtain half drawn, the light through its cloth. (11-22 round
// 6: every lit window was one flat panel at full glow, so the town's windows read as glowing boxes.) A lantern's glass.
const roomGlow = [pal([236, 150, 78], 0, 0.6, 0.38, 0), pal([214, 128, 64], 0, 0.6, 0.3, 0), pal([244, 172, 104], 0, 0.6, 0.45, 0)];
const lampGlow = [pal([255, 204, 132], 0, 0.5, 1.05, 0), pal([255, 184, 104], 0, 0.5, 0.85, 0), pal([255, 220, 160], 0, 0.5, 1.25, 0)];
const curtainLit = [pal([176, 72, 40], 0, 0.95, 0.22, 9), pal([150, 112, 56], 0, 0.95, 0.2, 9), pal([96, 104, 120], 0, 0.95, 0.16, 9)];
const lanternGlow = pal([255, 188, 104], 0, 0.4, 3.2, 3), soot = pal([34, 30, 28], 0, 0.95);
const reedGreen = pal([96, 112, 58], 0, 0.9, 0, 1), reedHead = pal([92, 64, 40], 0, 0.95, 0, 1), cloth = pal([150, 40, 40], 0, 0.95, 0, 9);
const gateBearings = trunks.map((t) => t.bearing);
const nearGate = (deg, r, clear) => gateBearings.some((g) => Math.abs(dAng(deg, g)) * RAD * r < clear);
const polar = (deg, r) => pt(deg, r);
const R = PLAN.rng(20261119), RW = PLAN.rng(20261122);   // RW: the lit windows' own details
const RS = PLAN.rng(20261123);   // RS: the back and end walls' windows (round 8), so no house moves
const pick = (arr) => arr[Math.floor(R() * arr.length)];
const tone = (rgb, amount = 0.12) => { const k = 1 + (R() * 2 - 1) * amount; return rgb.map((v) => Math.max(0, Math.min(255, v * k))); };
const weathered = (rgb) => { const g = rgb[0] * 0.3 + rgb[1] * 0.59 + rgb[2] * 0.11; return rgb.map((v) => v * 0.72 + g * 0.28); };
// A roof's palette row: its kind's own colour tinged 18% with the look's (the path's) roof colour, a little varied.
const roofPal = (look, jitter = 0.12) => {
	const K = ROOF_LOOK[look.kind || "tile"], mix = K.rgb.map((v, i) => v * 0.82 + weathered(look.roof)[i] * 0.18);
	return pal(jitter ? tone(mix, jitter) : mix, 0, K.rough, 0, K.pattern);
};
// (a shade under paper white: at full strength the limewash blew out to flat white in the sun)
const plasterPal = (rgb, jitter = 0.06) => { const c = rgb.map((v) => v * 0.86); return pal(jitter ? tone(c, jitter) : c, 0, 0.92, 0, PLASTER); };
// The darker row of a roof's own kind (its ridge).
const darker = (row, k = 0.8) => pal(palette[row].slice(0, 3).map((v) => v * k), 0, palette[row][4], 0, palette[row][6]);
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
	const flameA = pal([255, 150, 50], 0, 0.6, 2.4, 3), flameB = pal([255, 210, 120], 0, 0.6, 3.2, 3);
	for (let k = 0; k < 3; k++) {
		block(F, CUBE, TINT, [0, 0, 1.45], [1.0, 0.03, 1.1], { yawAdd: k * 60, colour: flameA, nosink: true });
		block(F, CUBE, TINT, [0, 0, 2.05], [0.55, 0.03, 0.9], { yawAdd: k * 60 + 30, colour: flameB, nosink: true });
	}
	// People warm themselves round it.
	for (let k = 0; k < 5; k++) { const a = 30 + k * 72 + R() * 20, [hx, hy] = polar(a, 8.6); stand(frame(hx, hy, CITY_TOP, a), 0, 0, 0, 1, "townsfolk", person()); }
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
	// (11-22, quick3: at 118-158 they read near white in the sun, a tiled floor; old worn sandstone and grit, greyer)
	const flags = [[124, 120, 112], [112, 108, 100], [132, 126, 114], [104, 102, 98], [118, 112, 102], [96, 92, 86], [128, 124, 118]].map((c) => pal(c, 0, 0.92, 0, 0));
	// The plaza paved: rings of flagstones round the hearth, each ring its own width (55-85 cm) and cut into stones of
	// their own lengths (50-115 cm), each a little turned and raised or sunk, laid near flush with the ground (they stood a hand proud and read as loose tiles) with thin dark
	// joints between (the ground's own plaza colour shows in them). (11-22: they were 1.7 x 1.5 m slabs of one size in
	// even rings, which read as a board, not a square.)
	for (let r = 7.2; r < PLAN.PLAZA_R - 0.4;) {
		const ring = 0.55 + R() * 0.3, rc = r + ring / 2, circ = 2 * Math.PI * rc, a0 = R() * 360;
		for (let used = 0; used < circ - 0.3;) {
			const len = Math.min(0.5 + R() * 0.65, circ - used), mid = a0 + (used + len / 2) / circ * 360, [px, py] = polar(mid, rc + (R() - 0.5) * 0.04);
			block(frame(px, py, CITY_TOP, mid + (R() - 0.5) * 3), CUBE, TINT, [0, 0, -0.065 + (R() - 0.5) * 0.02], [len - 0.022, ring - 0.022, 0.16], { nosink: true, colour: pick(flags) });   // (joints 2 cm: at 5 cm the stones read as loose tiles)
			used += len;
		}
		r += ring;
	}
	reserveCircle([0, 0], 24);
	// What each stall sells (world.json trade): table and trade.
	const WARES = [["market_bread", "baker"], ["market_produce", "grocer"], ["market_tools", "toolmaker"], ["market_fish", "fishmonger"], ["market_cloth", "clothier"], ["market_food", "cook"], ["market_fletcher", "fletcher"], ["market_potter", "potter"], ["market_herbs", "herbwife"]];
	// Market stalls in a ring, their counters facing the hearth, awnings in the twelve paths' colours.
	const n = 18;
	for (let k = 0; k < n; k++) {
		const deg = k * 360 / n + 9;
		if (nearGate(deg, 31, 6)) continue;
		// (11-22: a ring of identical boxes under flat slabs. Now each stands a little off the ring and turned its own
		// way: posts, a plank counter on trestles, a canvas awning sloping out with a scalloped valance, and its
		// trade's goods laid out on it in baskets and on boards.)
		const rr = 31 + (R() - 0.5) * 2.4, dg = deg + (R() - 0.5) * 4;
		const [x, y] = polar(dg, rr), F2 = frame(x, y, CITY_TOP, dg + (R() - 0.5) * 14);
		// (dyed canvas, the path's colour strong: mixed pale it blew out to white boxes in the sun)
		const canvas = pal(tone(QUARTER_LOOK[quarters[k % 12].path].roof.map((v, i) => (v * 0.85 + [150, 140, 116][i] * 0.15) * 0.9), 0.08), 0, 0.95, 0, 9);
		const [table, trade] = WARES[people.sellers.length % WARES.length];
		for (const [px, py] of [[-1.4, -1], [1.4, -1], [-1.4, 1], [1.4, 1]]) block(F2, CYL, WOOD, [px, py, 1.4], [0.13, 0.13, 2.8], { colour: darkWood });
		// the counter: a thick top on two trestles, a front of boards
		block(F2, CUBE, TINT, [0, 0.8, 0.98], [3.0, 0.85, 0.07], { colour: floorWood, nosink: true });
		{ const [cx, cy, cz] = F2.at(0, 0.8, 0.5); solids.push([cm(cx), cm(cy), cm(cz), cm(3.0), cm(0.85), cm(1.0), +F2.yaw.toFixed(1)]); }   // (the counter solid to the ground)
		for (const tx of [-1.1, 1.1]) for (const ty of [0.5, 1.1]) block(F2, CUBE, TINT, [tx, ty, 0.47], [0.08, 0.08, 0.95], { colour: beam, nosink: true, pitch: ty < 0.8 ? -6 : 6 });
		for (let b = 0; b < 6; b++) block(F2, CUBE, TINT, [-1.25 + b * 0.5, 1.2, 0.5], [0.47, 0.04, 0.85], { colour: pick([beam, darkWood, floorWood]), nosink: true });
		// the awning and its valance
		// (round 8: a flat slab and a strip; now the canvas sags between the posts, its valance's hem scalloped)
		block(F2, AWN, TINT, [0, 0.15, 2.62], [3.5, 2.7, 0.5], { roll: -12, colour: canvas, nosink: true });
		// the goods
		const goods = {
			// (11-22: loaves and fruit were cylinders on their sides and read as logs: domed loaves, round fruit)
			baker: () => { for (let g = 0; g < 9; g++) block(F2, SPH, TINT, [-1.1 + (g % 5) * 0.5 + (R() - 0.5) * 0.1, 0.65 + Math.floor(g / 5) * 0.3, 1.06], [0.3, 0.17, 0.13], { yawAdd: (R() - 0.5) * 30, colour: pal(tone([150, 98, 52], 0.12), 0, 0.85), nosink: true }); },
			grocer: () => { for (let c = 0; c < 3; c++) { block(F2, CUBE, TINT, [-0.9 + c * 0.9, 0.8, 1.13], [0.6, 0.45, 0.22], { colour: beam, nosink: true }); for (let g = 0; g < 8; g++) block(F2, SPH, TINT, [-1.12 + c * 0.9 + (g % 4) * 0.14, 0.7 + Math.floor(g / 4) * 0.18, 1.29], [0.12, 0.12, 0.11], { colour: pal(tone(pick([[92, 128, 52], [150, 48, 36], [176, 140, 60]]), 0.1), 0, 0.6), nosink: true }); } },
			toolmaker: () => { for (let g = 0; g < 5; g++) { block(F2, CUBE, TINT, [-1 + g * 0.5, 0.75, 1.03], [0.06, 0.5, 0.03], { colour: beam, nosink: true }); block(F2, CUBE, TINT, [-1 + g * 0.5, 0.52, 1.04], [0.16, 0.08, 0.05], { colour: iron, nosink: true }); } },
			fishmonger: () => { block(F2, CUBE, TINT, [0, 0.8, 1.03], [2.2, 0.6, 0.03], { colour: pal([120, 126, 128], 0, 0.5), nosink: true }); for (let g = 0; g < 7; g++) block(F2, CYL, TINT, [-0.9 + g * 0.3, 0.8, 1.08], [0.08, 0.14, 0.5], { pitch: 90, yawAdd: 90 + (R() - 0.5) * 20, colour: pal(tone([150, 156, 160], 0.08), 0.3, 0.3), nosink: true }); },
			clothier: () => { for (let g = 0; g < 6; g++) block(F2, CUBE, TINT, [-1.1 + (g % 3) * 0.75, 0.65 + Math.floor(g / 3) * 0.35, 1.07 + Math.floor(g / 3) * 0.06], [0.6, 0.3, 0.12], { colour: pal(tone(pick([[120, 40, 34], [60, 80, 120], [150, 130, 80], [70, 96, 60]]), 0.1), 0, 0.95, 0, 9), nosink: true }); },
			cook: () => { for (let g = 0; g < 3; g++) block(F2, CYL, TINT, [-0.8 + g * 0.8, 0.8, 1.15], [0.42, 0.42, 0.3], { colour: iron, nosink: true }); },
			fletcher: () => { for (let g = 0; g < 14; g++) block(F2, CYL, TINT, [-1.1 + g * 0.16, 0.8, 1.04], [0.02, 0.02, 0.75], { pitch: 90, yawAdd: 90, colour: beam, nosink: true }); },
			potter: () => { for (let g = 0; g < 6; g++) { const h = 0.22 + R() * 0.2; block(F2, CYL, TINT, [-1.1 + g * 0.44, 0.8, 1.01 + h / 2], [0.24, 0.24, h], { colour: pal(tone([164, 96, 62], 0.12), 0, 0.7), nosink: true }); } },
			herbwife: () => { for (let g = 0; g < 8; g++) block(F2, CYL, TINT, [-1.2 + g * 0.34, 1.35, 2.15], [0.12, 0.12, 0.35], { colour: pal(tone([86, 104, 52], 0.15), 0, 0.95), nosink: true }); for (let c = 0; c < 2; c++) block(F2, CYL, TINT, [-0.5 + c, 0.8, 1.1], [0.5, 0.5, 0.18], { colour: straw, nosink: true }); },
		};
		(goods[trade] || goods.baker)();
		if (R() < 0.6) block(F2, CYL, WOOD, [1.9, -0.6, 0.45], [0.6, 0.6, 0.9]);
		reserve({ c: [x, y], u: [Math.cos(deg * RAD), Math.sin(deg * RAD)], hw: 2, hd: 1.6 });
		{
			const p = person(pick(["townwear", "townwear_b", "towngown"])), [sx, sy, sz] = F2.at(0, -0.35, 0);
			people.sellers.push([cm(sx), cm(sy), cm(sz), yawOf(F2, 0, 1), table, `${p.name} the ${trade}`, p.sex, p.face, p.build, p.outfit]);
		}
	}
	// Handcarts and crates between the stalls, a notice board by the hearth.
	for (let k = 0; k < 6; k++) {
		const deg = k * 60 + 30;
		if (nearGate(deg, 25, 5)) continue;
		const [x, y] = polar(deg, 25), C = frame(x, y, CITY_TOP, deg + 20);
		block(C, CUBE, WOOD, [0, 0, 0.75], [2.2, 1.3, 0.5], { colour: beam, solid: true });
		for (const sy of [-1, 1]) block(C, CYL, WOOD, [-0.3, sy * 0.72, 0.5], [1.0, 1.0, 0.1], { roll: 90, colour: darkWood, nosink: true });
		block(C, CUBE, WOOD, [1.6, 0, 0.6], [1.2, 0.1, 0.1], { colour: beam, nosink: true });
		for (let c = 0; c < 3; c++) block(C, R() < 0.5 ? CUBE : CYL, WOOD, [-0.6 + c * 0.6, 0, 1.2], [0.5, 0.5, 0.45], { colour: c % 2 ? straw : -1, nosink: true });
		reserveCircle([x, y], 1.8);
	}
	{
		const B = frame(...polar(200, 11), CITY_TOP, 200);
		for (const sx of [-0.9, 0.9]) block(B, CYL, WOOD, [sx, 0, 1.2], [0.14, 0.14, 2.4], { colour: darkWood });
		block(B, CUBE, WOOD, [0, 0.05, 1.7], [2.0, 0.08, 1.1], { colour: beam, nosink: true });
		for (let k = 0; k < 5; k++) block(B, CUBE, TINT, [-0.6 + k * 0.3, 0.1, 1.6 + (k % 2) * 0.25], [0.24, 0.02, 0.32], { colour: pal([214, 204, 180], 0, 0.95), nosink: true });
	}
	// Round the plaza's edge: trees with benches under them, but not across an avenue's mouth.
	for (let k = 0; k < 16; k++) {
		const deg = k * 22.5 + 11;
		if (nearGate(deg, 40, 9)) continue;
		const [x, y] = polar(deg, 40.5);
		plant(0, Math.floor(R() * 8), x, y, R() * 360, 0.42 + 0.08 * R());
		trunkSolid(x, y);
		bench(frame(...polar(deg, 38), CITY_TOP, deg), 0, 0, 2.2);
		reserveCircle([x, y], 2);
	}
}

// ---- each quarter: its hall facing the ring street, its green with a well and trees ------------------------------------
const wellAt = ([x, y]) => {
	const F = frame(x, y, CITY_TOP, R() * 360);
	block(F, CYL, STONE, [0, 0, 0.45], [2.2, 2.2, 0.9], { solid: true });
	block(F, CYL, TINT, [0, 0, 0.92], [1.6, 1.6, 0.05], { colour: pal([24, 46, 58], 0, 0.15, 0, 3) });
	for (const s of [-1, 1]) block(F, CUBE, WOOD, [s * 1.05, 0, 1.4], [0.18, 0.18, 2.8]);
	// An open roof of two slabs meeting at a ridge (a block turned 45 degrees here hung its lower half between the posts
	// like a cube balanced on its edge), the windlass across the posts, its rope and bucket.
	for (const sd of [-1, 1]) block(F, CUBE, TINT, [0, sd * 0.44, 3.04], [2.9, 1.3, 0.07], { roll: sd * 45, colour: slate, nosink: true });
	block(F, CUBE, TINT, [0, 0, 3.5], [2.95, 0.16, 0.16], { roll: 45, colour: slate, nosink: true });
	block(F, CYL, WOOD, [0, 0, 2.1], [0.16, 0.16, 2.1], { pitch: 90, colour: beam, nosink: true });
	block(F, CYL, TINT, [0.35, 0, 1.6], [0.03, 0.03, 1.0], { colour: pal([150, 128, 92], 0, 0.95), nosink: true });
	block(F, CYL, WOOD, [0.35, 0, 1.0], [0.34, 0.34, 0.32], { colour: darkWood, nosink: true });
};
// Boards up a room's walls to dado height, a rail along their top (round 9: the rooms were bare walls to the floor):
// along the frame's x at y (the wall's inner face, facing in = -sign(y)), from x0 to x1, gaps [{at, w}] for the doors.
function wainscot(F, x0, x1, y, h, gaps = []) {
	const inward = -Math.sign(y), pieces = [];
	let a = x0;
	for (const g of [...gaps].sort((p, q) => p.at - q.at)) { pieces.push([a, g.at - g.w / 2]); a = g.at + g.w / 2; }
	pieces.push([a, x1]);
	for (const [p, q] of pieces) {
		if (q - p < 0.2) continue;
		block(F, CUBE, TINT, [(p + q) / 2, y + inward * 0.04, h / 2], [q - p, 0.06, h], { colour: pal([74, 52, 34], 0, 0.85, 0, 1), nosink: true });
		block(F, CUBE, TINT, [(p + q) / 2, y + inward * 0.08, h + 0.04], [q - p, 0.1, 0.1], { colour: beam, nosink: true });
		block(F, CUBE, TINT, [(p + q) / 2, y + inward * 0.07, 0.08], [q - p, 0.08, 0.16], { colour: darkWood, nosink: true });
	}
}
function wainscotX(F, y0, y1, x, h, gaps = []) {
	const G = frame(...F.at(0, 0, 0).slice(0, 2), F.z, F.yaw + 90);
	wainscot(G, y0, y1, -x, h, gaps);
}
// A quarter's hall you can walk into: one long room (22 x 14 m, walls 9 m) with a door and windows open to the street,
// a timber floor, two long tables with benches, a fireplace on the back wall, a dais with the steward's chair, banners
// in the path's colour, two hanging lamps; its bell tower at the end. Its steward stands at the dais.
function hallInside(F, look, roof, q) {
	const H = 9, T = 0.6, X0 = -9, X1 = 13, HY = 7;
	// the path's banners in its own colour, woven (the roof's colour row is a roof's pattern now)
	const banner = pal(weathered(look.roof), 0, 0.95, 0, 9);
	block(F, CUBE, TINT, [2, 0, 0.1], [X1 - X0 - 0.4, 2 * HY - 0.4, 0.2], { colour: floorWood, nosink: true });
	floorBox(F, 2, 0, (X1 - X0) / 2, HY);
	wall(F, [X0, HY], [X1, HY], T, H, [{ at: 11, w: 2.6, z0: 0, z1: 4.2 }, { at: 4, w: 1.4, z0: 3.2, z1: 6.2 }, { at: 18, w: 1.4, z0: 3.2, z1: 6.2 }]);
	wall(F, [X0, -HY], [X1, -HY], T, H, [{ at: 4, w: 1.4, z0: 3.2, z1: 6.2 }, { at: 18, w: 1.4, z0: 3.2, z1: 6.2 }]);
	wall(F, [X0, -HY], [X0, HY], T, H);
	wall(F, [X1, -HY], [X1, HY], T, H, [{ at: 4.2, w: 1.4, z0: 3.2, z1: 6.2 }, { at: 9.8, w: 1.4, z0: 3.2, z1: 6.2 }]);
	openRoof(F, X0, X1, 0, HY, H, roof, T);
	// The bell tower at the far end, outside the room.
	block(F, CUBE, STONE, [-13, 0, 10], [8, 8, 20], { solid: true });
	block(F, CONE, TINT, [-13, 0, 24], [9.4, 9.4, 8], { colour: roof });
	// The door (swung in against the wall), the steps, the path's banner on a pole by the door.
	block(F, CUBE, WOOD, [0.75, HY - 1.5, 2.1], [0.14, 2.5, 4.1], { colour: darkWood, nosink: true });
	block(F, CUBE, STONE, [2, HY + 1.4, 0.25], [6, 2.4, 0.5]);
	block(F, CYL, WOOD, [-5, HY + 1.6, 5], [0.35, 0.35, 10]);
	block(F, CUBE, CLOTH, [-3.9, HY + 1.6, 7.6], [2, 0.12, 4], { colour: banner });
	// Inside: the long tables, the fireplace, the dais, banners, lamps.
	for (const ty of [-3, 3]) table(F, 1, ty, 12, 1.3);
	// (round 9) the walls panelled to 1.4 m, a runner down the floor between the tables, candles on them, a sideboard of
	// plates by the dais, the path's shields hung along the walls
	wainscot(F, X0 + T / 2, X1 - T / 2, HY - T / 2, 1.4, [{ at: X0 + 11, w: 2.6 }]);
	wainscot(F, X0 + T / 2, X1 - T / 2, -HY + T / 2, 1.4, [{ at: 2, w: 4.4 }]);
	wainscotX(F, -HY + T / 2, HY - T / 2, X0 + T / 2, 1.4);
	block(F, CUBE, TINT, [1, 0, 0.215], [13, 2.2, 0.03], { colour: pal([150, 120, 70], 0, 0.97, 0, 9), nosink: true });
	for (const ty of [-3, 3]) for (const tx of [-3.5, 1, 5.5]) {
		block(F, CYL, TINT, [tx, ty, 1.05], [0.16, 0.16, 0.16], { colour: iron, nosink: true });
		block(F, CYL, TINT, [tx, ty, 1.2], [0.07, 0.07, 0.22], { colour: pal([236, 226, 200], 0, 0.6, 0, 3), nosink: true });
		block(F, CYL, TINT, [tx, ty, 1.35], [0.04, 0.04, 0.08], { colour: lanternGlow, nosink: true, night: true });
	}
	block(F, CUBE, TINT, [X1 - 4.6, HY - 1.0, 0.5], [2.6, 0.7, 1.0], { colour: darkWood, nosink: true, solid: true });
	block(F, CUBE, TINT, [X1 - 4.6, HY - 1.0, 1.03], [2.7, 0.75, 0.06], { colour: beam, nosink: true });
	for (let k = 0; k < 4; k++) block(F, CYL, TINT, [X1 - 5.6 + k * 0.65, HY - 0.75, 1.32], [0.5, 0.5, 0.03], { pitch: 75, colour: pal([150, 140, 124], 0.3, 0.45, 0, 3), nosink: true });
	for (const sx of [-7.5, -1.5, 5.5]) for (const sd of [-1, 1]) {
		block(F, CYL, TINT, [sx, sd * (HY - T / 2 - 0.06), 3.0], [0.9, 0.9, 0.08], { pitch: 90, yawAdd: 90, colour: banner, nosink: true });
		block(F, CYL, TINT, [sx, sd * (HY - T / 2 - 0.1), 3.0], [0.22, 0.22, 0.06], { pitch: 90, yawAdd: 90, colour: iron, nosink: true });
	}
	// The fireplace: two jambs and a lintel round the firebox, a mantel shelf, the chimney breast above (11-22: one
	// stone block with a glowing panel in it).
	for (const jx of [-1.45, 1.45]) block(F, CUBE, STONE, [2 + jx, -HY + 0.9, 0.95], [1.3, 1.2, 1.9], { solid: true, nosink: true });
	block(F, CUBE, STONE, [2, -HY + 0.9, 2.15], [4.2, 1.2, 0.5], { nosink: true });
	block(F, CUBE, TINT, [2, -HY + 1.55, 2.47], [4.5, 0.4, 0.12], { colour: darkWood, nosink: true });
	block(F, CUBE, STONE, [2, -HY + 0.75, 5.7], [3.6, 0.9, 6.6], { solid: true, nosink: true });
	block(F, CUBE, STONE, [2, -HY + 0.35, 0.95], [1.6, 0.5, 1.9], { nosink: true });
	block(F, CUBE, TINT, [2, -HY + 0.62, 0.95], [1.6, 0.04, 1.8], { colour: soot, nosink: true });
	block(F, CUBE, STONE, [2, -HY + 2.0, 0.3], [3.2, 1.0, 0.2], { nosink: true });
	for (let k = 0; k < 3; k++) block(F, CYL, WOOD, [1.6 + k * 0.4, -HY + 1.05, 0.28], [0.2, 0.2, 1.1], { pitch: 90, yawAdd: 15 * (k - 1), colour: beam, nosink: true });
	fire(FIRE.fireplace, F.at(2, -HY + 1.05, 0.35), 0.6);
	block(F, CUBE, TINT, [X1 - 1.8, 0, 0.3], [3, 2 * HY - 1.4, 0.4], { colour: floorWood, nosink: true, solid: true });
	block(F, CUBE, TINT, [X1 - 1.2, 0, 1.3], [0.8, 1.0, 1.6], { colour: darkWood, nosink: true });
	block(F, CUBE, TINT, [X1 - 0.85, 0, 2.6], [0.15, 1.0, 1.4], { colour: darkWood, nosink: true });
	for (const by of [-HY + 0.36, HY - 0.36]) for (const bx of [-2, 6]) block(F, CUBE, CLOTH, [bx, by, 5.4], [1.4, 0.06, 3.2], { colour: banner, nosink: true });
	lampHang(F, -2, 0, 6.2, H + HY - 0.3); lampHang(F, 6, 0, 6.2, H + HY - 0.3);
	stand(F, X1 - 2.6, 0, -1, 0, "townsfolk", person("townwear"), `steward of the ${q.path} quarter`);
}
const quarterGreens = [];
for (const q of quarters) {
	const look = QUARTER_LOOK[q.path], roof = roofPal(look, 0), mid = norm(q.from + 15);
	// The hall: outside the ring street at the quarter's middle (beside an avenue if one runs there).
	let placed = false;
	for (const [rr, off] of [170, 200, 130, 235].flatMap((r) => [0, 6, -6, 10, -10, 13, -13].map((o) => [r, o]))) {
		if (placed) break;
		const deg = mid + off, ns = PLAN.nearestStreet(polar(deg, rr));
		const dir = [ns.at[0] - polar(deg, rr)[0], ns.at[1] - polar(deg, rr)[1]], L = Math.hypot(...dir) || 1;
		const d = [dir[0] / L, dir[1] / L], yaw = Math.atan2(-d[0], d[1]) / RAD;
		const c = [ns.at[0] - d[0] * (ns.street.half + 2 + 7), ns.at[1] - d[1] * (ns.street.half + 2 + 7)];
		const fp = { c, u: [Math.cos(yaw * RAD), Math.sin(yaw * RAD)], hw: 17, hd: 8 };
		if (!free(fp, 2) || !offStreets(fp) || !insideCity(fp)) continue;
		reserve(fp);
		const F = frame(c[0], c[1], CITY_TOP, yaw);
		hallInside(F, look, roof, q);
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

// ---- the inn on the plaza's edge, facing the hearth: the Hearth and Kettle ----------------------------------------------
// One tall room you walk into (18 x 12 m, walls 6 m): the bar along the back with the innkeeper behind it and barrels on
// the shelves, round tables and stools, a fireplace, lamps; a hanging sign and lanterns at the door.
{
	let placed = false;
	for (const rr of [57, 60, 64, 68]) {
		if (placed) break;
		for (let deg = 3; deg < 360 && !placed; deg += 4) {
			const c = polar(deg, rr), fp = { c, u: [Math.cos(deg * RAD), Math.sin(deg * RAD)], hw: 9.8, hd: 7.6 };
			if (!free(fp, 1.5) || !offStreets(fp, 0.5) || !insideCity(fp)) continue;
			reserve(fp);
			placed = true;
			const F = frame(c[0], c[1], CITY_TOP, deg), H = 6, X = 9, Y = 6, T = 0.6;
			const plaster = plasterPal([226, 218, 200], 0), roof = roofPal({ roof: [150, 84, 56], kind: "tile" }, 0);
			block(F, CUBE, TINT, [0, 0, 0.1], [2 * X - 0.4, 2 * Y - 0.4, 0.2], { colour: floorWood, nosink: true });
			floorBox(F, 0, 0, X, Y);
			wall(F, [-X, Y], [X, Y], T, H, [{ at: 6, w: 2.4, z0: 0, z1: 3.6 }, { at: 2, w: 1.4, z0: 1.3, z1: 3.1 }, { at: 11, w: 1.4, z0: 1.3, z1: 3.1 }, { at: 15, w: 1.4, z0: 1.3, z1: 3.1 }], { mat: TINT, colour: plaster });
			wall(F, [-X, -Y], [X, -Y], T, H, [{ at: 4, w: 1.4, z0: 1.6, z1: 3.1 }, { at: 14, w: 1.4, z0: 1.6, z1: 3.1 }], { mat: TINT, colour: plaster });
			wall(F, [-X, -Y], [-X, Y], T, H, [{ at: 6, w: 1.4, z0: 1.3, z1: 3.1 }], { mat: TINT, colour: plaster });
			wall(F, [X, -Y], [X, Y], T, H, [], { mat: TINT, colour: plaster });
			// Corner posts and a head beam (no posts across the door or the windows).
			for (const sx of [-1, 1]) for (const sy of [-1, 1]) block(F, CUBE, TINT, [sx * (X + 0.32), sy * (Y + 0.32), H / 2], [0.3, 0.3, H], { colour: beam });
			block(F, CUBE, TINT, [0, Y + 0.33, H - 0.15], [2 * X + 0.6, 0.12, 0.3], { colour: beam, nosink: true });
			openRoof(F, -X, X, 0, Y, H, roof, T);
			// The bar, its shelves of barrels and jugs, the innkeeper behind it.
			block(F, CUBE, TINT, [-1, -2.9, 0.55], [9, 0.8, 1.1], { colour: darkWood, solid: true, nosink: true });
			block(F, CUBE, TINT, [-1, -2.9, 1.14], [9.3, 0.95, 0.08], { colour: beam, nosink: true });
			for (const z of [1.0, 2.0]) block(F, CUBE, TINT, [-1, -Y + 0.55, z], [9, 0.5, 0.08], { colour: beam, nosink: true });
			for (let k = 0; k < 8; k++) {
				block(F, CYL, WOOD, [-4.8 + k * 1.1, -Y + 0.6, 0.45], [0.75, 0.75, 0.85], { pitch: 90, colour: k % 2 ? beam : darkWood, nosink: true });
				for (const h of [-0.27, 0.27]) block(F, CYL, TINT, [-4.8 + k * 1.1 + h, -Y + 0.6, 0.45], [0.78, 0.78, 0.05], { pitch: 90, colour: iron, nosink: true });
			}
			for (let k = 0; k < 9; k++) block(F, CYL, TINT, [-4.9 + k * 1.0, -Y + 0.55, 2.2], [0.22, 0.22, 0.32], { colour: pick([glass, straw, pal([120, 90, 60], 0, 0.6, 0, 3)]), nosink: true });
			{
				const p = person("towngown"), [ix, iy, iz] = F.at(-1, -4.3, 0);
				people.sellers.push([cm(ix), cm(iy), cm(iz), yawOf(F, 0, 1), "inn", `${p.name} the innkeeper`, p.sex, p.face, p.build, p.outfit]);
			}
			// The fireplace on the east wall, the round tables and stools, the lamps.
			block(F, CUBE, STONE, [X - 0.9, 1.5, 3], [1.2, 3.6, 6], { solid: true, nosink: true });
			block(F, CUBE, TINT, [X - 1.52, 1.5, 0.85], [0.06, 2.0, 1.5], { colour: soot, nosink: true });
			block(F, CUBE, STONE, [X - 2.0, 1.5, 0.12], [1.0, 2.8, 0.16], { nosink: true });
			fire(FIRE.fireplace, F.at(X - 2.0, 1.5, 0.45), 0.6);
			for (const [tx, ty] of [[-5.5, 2.6], [-1, 3.0], [3.4, 2.4], [-6.5, -0.6]]) {
				roundTable(F, tx, ty);
				R(); // (the old jug's draw, kept so the city built after the inn keeps its layout)
				for (let k = 0; k < 3; k++) { const a = (k * 120 + 30) * RAD; stool(F, tx + Math.cos(a) * 1.0, ty + Math.sin(a) * 1.0); }
			}
			// A patron at each table, on the side its stools leave free.
			for (const [tx, ty, a] of [[-5.5, 2.6, 90], [-1, 3.0, 90], [3.4, 2.4, 90], [-6.5, -0.6, 330]]) { const px = tx + Math.cos(a * RAD) * 1.3, py = ty + Math.sin(a * RAD) * 1.3; stand(F, px, py, tx - px, ty - py, "townsfolk", person()); }
			lampHang(F, -4, 0.5, 4.0, H + Y - 0.3); lampHang(F, 3, 0.5, 4.0, H + Y - 0.3);
			// (round 9) the walls panelled to 1.2 m, a hearthstone apron, hams and bunches of herbs hung from the tie beams,
			// a candle on each table, a cask on its cradle at the bar's end, a rag rug before the fire
			wainscot(F, -X + T / 2, X - T / 2, Y - T / 2, 1.2, [{ at: -X + 6, w: 2.4 }]);
			wainscot(F, -X + T / 2, X - T / 2, -Y + T / 2, 1.2);
			wainscotX(F, -Y + T / 2, Y - T / 2, -X + T / 2, 1.2);
			for (const [hx, hy] of [[-5.4, -1.2], [-5.4, -1.9], [-1.8, -1.0], [1.8, -0.6], [5.4, 0.8]]) {
				const top = H - 0.36, ham = hx < -3 && hy > -1.5;
				block(F, CUBE, TINT, [hx, hy, top - 0.45], [0.02, 0.02, 0.9], { colour: pal([150, 128, 92], 0, 0.95), nosink: true });
				if (ham) block(F, SPH, TINT, [hx, hy, top - 1.15], [0.3, 0.24, 0.55], { colour: pal([84, 46, 30], 0, 0.7, 0, 0), nosink: true });
				else for (let b = 0; b < 3; b++) block(F, CYL, TINT, [hx + (b - 1) * 0.1, hy, top - 1.05], [0.09, 0.09, 0.42], { roll: (b - 1) * 14, colour: pal([88, 84, 52], 0, 0.95, 0, 9), nosink: true });
			}
			for (const [tx, ty] of [[-5.5, 2.6], [-1, 3.0], [3.4, 2.4], [-6.5, -0.6]]) {
				block(F, CYL, TINT, [tx + 0.15, ty - 0.2, 0.97], [0.07, 0.07, 0.16], { colour: pal([236, 226, 200], 0, 0.6, 0, 3), nosink: true });
				block(F, CYL, TINT, [tx + 0.15, ty - 0.2, 1.08], [0.04, 0.04, 0.07], { colour: lanternGlow, nosink: true, night: true });
			}
			block(F, CUBE, STONE, [X - 2.7, 1.5, 0.21], [2.0, 3.4, 0.04], { nosink: true });
			block(F, CUBE, TINT, [X - 4.6, 1.5, 0.212], [1.7, 2.5, 0.02], { colour: pal([52, 34, 28], 0, 0.97, 0, 9), nosink: true });
			block(F, CUBE, TINT, [X - 4.6, 1.5, 0.222], [1.4, 2.2, 0.02], { colour: pal([92, 46, 36], 0, 0.97, 0, 9), nosink: true });
			block(F, CUBE, TINT, [4.3, -2.9, 0.45], [0.9, 0.9, 0.12], { colour: darkWood, nosink: true });
			block(F, CYL, WOOD, [4.3, -2.9, 0.95], [0.95, 0.95, 1.2], { pitch: 90, yawAdd: 90, colour: beam, nosink: true });
			for (const hx of [-0.4, 0.4]) block(F, CYL, TINT, [4.3, -2.9 + hx, 0.95], [0.98, 0.98, 0.05], { pitch: 90, yawAdd: 90, colour: iron, nosink: true });
			// Outside: the sign on its bracket, a lantern either side of the door, a bench.
			block(F, CUBE, TINT, [-3, Y + 0.9, 4.7], [0.08, 1.6, 0.08], { colour: iron, nosink: true });
			block(F, CUBE, TINT, [-3, Y + 1.4, 4.05], [0.06, 1.0, 0.8], { colour: darkWood, nosink: true });
			block(F, CYL, TINT, [-3.05, Y + 1.4, 4.05], [0.4, 0.4, 0.05], { pitch: 90, colour: iron, nosink: true });
			for (const sx of [-4.6, -1.4]) {
				block(F, CUBE, TINT, [sx, Y + 0.45, 2.9], [0.36, 0.36, 0.5], { colour: iron, nosink: true });
				block(F, CUBE, TINT, [sx, Y + 0.45, 2.9], [0.4, 0.26, 0.38], { colour: lanternGlow, nosink: true, night: true });
				fire(FIRE.lantern, F.at(sx, Y + 0.45, 2.9), 1);
			}
			bench(F, 3.5, Y + 1.1, 3);
			console.log(`the inn at ${c.map(Math.round)} (r ${rr}, bearing ${deg})`);
		}
	}
	if (!placed) console.warn("no room for the inn");
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
		// The lantern: an iron frame (plates top and bottom, four corner bars) with glass panes and a candle, a cap and a
		// ring; after dusk the panes glow. (11-22: it was a solid iron box, a black block on a pole by day.)
		block(G, CUBE, TINT, [0.85, 0, 2.82], [0.38, 0.38, 0.04], { colour: iron, nosink: true });
		block(G, CUBE, TINT, [0.85, 0, 3.29], [0.4, 0.4, 0.04], { colour: iron, nosink: true });
		for (const [cx, cy] of [[-1, -1], [-1, 1], [1, -1], [1, 1]]) block(G, CUBE, TINT, [0.85 + cx * 0.17, cy * 0.17, 3.055], [0.035, 0.035, 0.46], { colour: iron, nosink: true });
		for (const [ox, oy, sx, sy] of [[0.165, 0, 0.012, 0.31], [-0.165, 0, 0.012, 0.31], [0, 0.165, 0.31, 0.012], [0, -0.165, 0.31, 0.012]]) {
			block(G, CUBE, TINT, [0.85 + ox, oy, 3.055], [sx, sy, 0.42], { colour: glass, nosink: true });
			block(G, CUBE, TINT, [0.85 + ox * 1.03, oy * 1.03, 3.055], [sx + 0.004, sy + 0.004, 0.42], { colour: lanternGlow, nosink: true, night: true });
		}
		block(G, CYL, TINT, [0.85, 0, 2.92], [0.06, 0.06, 0.13], { colour: pal([214, 200, 168], 0, 0.6), nosink: true });
		block(G, CONE, TINT, [0.85, 0, 3.43], [0.48, 0.48, 0.24], { colour: iron });
		block(G, CYL, TINT, [0.85, 0, 3.6], [0.1, 0.1, 0.03], { colour: iron, nosink: true });
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
			if (R() < 0.42) {
				// the room's glow, the lamp's side brighter (behind the mullion and the frame), now and then a curtain (their
				// own random stream: drawing on the town's would move every house after the first lit window)
				block(F, CUBE, TINT, [x, d / 2 + 0.105, z], [0.9, 0.02, h - 0.06], { colour: pick(roomGlow), nosink: true, night: true });
				const side = RW() < 0.5 ? -1 : 1, lamp = 0.3 + 0.2 * RW();
				block(F, CUBE, TINT, [x + side * (0.45 - lamp / 2), d / 2 + 0.112, z - 0.12 * h], [lamp, 0.02, h * (0.45 + 0.2 * RW())], { colour: lampGlow[Math.floor(RW() * lampGlow.length)], nosink: true, night: true });
				if (RW() < 0.3) block(F, CUBE, TINT, [x - side * 0.27, d / 2 + 0.118, z + 0.02], [0.34, 0.02, h - 0.1], { colour: curtainLit[Math.floor(RW() * curtainLit.length)], nosink: true, night: true });
			}
		}
		if (R() < 0.35) block(F, CUBE, WOOD, [x + 0.7, d / 2 + 0.06, z], [0.4, 0.1, h + 0.1], { colour: darkWood });
	}
}
// Windows on the back wall (and, endWindow, on a gable end) too (round 8: every house showed the street its windows and
// the rest of the town blank walls). The same frame and sill and glass, now and then a lit room after dusk; on their own
// random stream (RS).
function backWindows(F, w, d, z, n, h = 1.2) {
	for (let i = 0; i < n; i++) {
		if (RS() < 0.15) continue;
		const x = -w / 2 + w * (i + 0.5) / n;
		block(F, CUBE, TINT, [x, -d / 2 - 0.05, z], [0.95, 0.12, h], { colour: glass, nosink: true });
		block(F, CUBE, TINT, [x, -d / 2 - 0.07, z + h / 2 + 0.06], [1.15, 0.14, 0.12], { colour: beam, nosink: true });
		block(F, CUBE, TINT, [x, -d / 2 - 0.12, z - h / 2 - 0.06], [1.2, 0.24, 0.12], { colour: beam, nosink: true });
		if (RS() < 0.35) block(F, CUBE, TINT, [x, -d / 2 - 0.105, z], [0.9, 0.02, h - 0.06], { colour: roomGlow[Math.floor(RS() * roomGlow.length)], nosink: true, night: true });
	}
}
function endWindow(F, w, z, side, h = 1.1) {
	if (RS() < 0.2) return;
	const x = side * (w / 2 + 0.05);
	block(F, CUBE, TINT, [x, 0, z], [0.12, 0.9, h], { colour: glass, nosink: true });
	block(F, CUBE, TINT, [x + side * 0.02, 0, z + h / 2 + 0.06], [0.14, 1.1, 0.12], { colour: beam, nosink: true });
	block(F, CUBE, TINT, [x + side * 0.07, 0, z - h / 2 - 0.06], [0.24, 1.15, 0.12], { colour: beam, nosink: true });
	if (RS() < 0.35) block(F, CUBE, TINT, [x + side * 0.055, 0, z], [0.02, 0.85, h - 0.06], { colour: roomGlow[Math.floor(RS() * roomGlow.length)], nosink: true, night: true });
}
// A gable roof is one block turned 45 degrees: its upper half the roof, its lower half hidden in the house, so it may
// not reach past the gable walls (its lower half would show there); the eaves overhang a little either side.
function gableRoof(F, w, d, z, roof, ox = 0, oy = 0, along = 0) {
	block(F, CUBE, TINT, [ox, oy, z], along ? [w * 0.78, d - 0.04, w * 0.78] : [w - 0.04, d * 0.78, d * 0.78], along ? { pitch: 45, colour: roof } : { roll: 45, colour: roof });
	// A ridge cap (the roof's own pattern draws its courses: the darker strips laid on it went, 11-22).
	const a = (along ? w : d) * 0.78, hd = a / Math.SQRT2, len = (along ? d : w) - 0.02, dark = darker(roof);
	block(F, CUBE, TINT, [ox, oy, z + hd + 0.02], along ? [0.3, len, 0.3] : [len, 0.3, 0.3], along ? { pitch: 45, colour: dark, nosink: true } : { roll: 45, colour: dark, nosink: true });
}
function halfTimber(F, w, d, z0, h) {
	// Posts at the corners and between, a sill beam and a head beam, on the street face and the ends.
	for (const x of [-w / 2, -w / 6, w / 6, w / 2]) block(F, CUBE, TINT, [x, d / 2 + 0.06, z0 + h / 2], [0.22, 0.12, h], { colour: beam });
	for (const zz of [z0 + 0.1, z0 + h - 0.1]) block(F, CUBE, TINT, [0, d / 2 + 0.06, zz], [w, 0.12, 0.2], { colour: beam });
	for (const s of [-1, 1]) for (const y of [-d / 2, 0, d / 2]) block(F, CUBE, TINT, [s * (w / 2 + 0.06), y, z0 + h / 2], [0.12, 0.22, h], { colour: beam });
}
// A plastered house stands on a stone footing: a plinth course a hand proud of the wall (11-22: plaster boxes met the
// grass in a hard line).
const plinth = (F, w, d, ox = 0, oy = 0) => block(F, CUBE, STONE, [ox, oy, 0.28], [w + 0.1, d + 0.1, 0.56], { nosink: false });
function building(kind, F, w, d, look) {
	const roof = roofPal(look), plaster = plasterPal(look.plaster), useStone = R() < 0.45;
	const wallMat = (ground) => (ground && useStone ? [STONE, -1] : [TINT, plaster]);
	switch (kind) {
		case "cottage": {
			const h = 3.8 + R() * 0.8, [m, c] = wallMat(true);
			block(F, CUBE, m, [0, 0, h / 2], [w, d, h], { solid: true, colour: c });
			if (m !== STONE) plinth(F, w, d);
			gableRoof(F, w, d, h, roof);
			door(F, -w / 4, d / 2, 1.1, 2.15);
			windows(F, w, d, h * 0.55, 2);
			backWindows(F, w, d, h * 0.55, 2); endWindow(F, w, h * 0.55, -1); endWindow(F, w, h * 0.55, 1);
			if (R() < 0.7) block(F, CUBE, STONE, [w / 3, -d / 6, h + d * 0.32], [0.9, 0.9, d * 0.6 + 1]);
			return h + d * 0.5;
		}
		case "house": {
			const h1 = 3.6, h2 = 3.2, [m] = wallMat(true);
			block(F, CUBE, m === STONE ? STONE : TINT, [0, 0, h1 / 2], [w, d, h1], { solid: true, colour: m === STONE ? -1 : plaster });
			if (m !== STONE) plinth(F, w, d);
			// The upper storey jettied out over the street and the yard, not at the gable ends: there the roof's hidden half
			// (one block turned 45 degrees) reached below the jetty and showed as a white wedge over the ground floor.
			block(F, CUBE, TINT, [0, 0, h1 + h2 / 2], [w, d + 0.5, h2], { solid: true, colour: plaster });
			halfTimber(F, w, d + 0.5, h1, h2);
			gableRoof(F, w, d + 0.5, h1 + h2, roof);
			door(F, -w / 4, d / 2, 1.2, 2.15);
			windows(F, w, d, 2.1, 2); windows(F, w, d + 0.5, h1 + 1.6, 3);
			backWindows(F, w, d, 2.1, 2); backWindows(F, w, d + 0.5, h1 + 1.6, 3); endWindow(F, w, 2.1, -1); endWindow(F, w, 2.1, 1);
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
			door(F, 0, d / 2, 1.2, 2.25);
			for (let s = 1; s < n; s++) { windows(F, w, d, s * hs + 1.7, 2); backWindows(F, w, d, s * hs + 1.7, 2); }
			return n * hs + w * 0.5;
		}
		case "lhouse": {
			const h = 3.8 + R() * 0.6, [m, c] = wallMat(true), ww = d, wd = w * 0.55, side = R() < 0.5 ? -1 : 1;
			block(F, CUBE, m, [0, 0, h / 2], [w, d, h], { solid: true, colour: c });
			gableRoof(F, w, d, h, roof);
			// The wing runs back from one end, its own roof across the main one's.
			block(F, CUBE, m, [side * (w / 2 - ww / 2), -d / 2 - wd / 2 + 0.2, h / 2], [ww, wd, h], { solid: true, colour: c });
			if (m !== STONE) { plinth(F, w, d); plinth(F, ww, wd, side * (w / 2 - ww / 2), -d / 2 - wd / 2 + 0.2); }
			gableRoof(F, ww, wd + 0.4, h, roof, side * (w / 2 - ww / 2), -d / 2 - wd / 2 + 0.2, 1);
			door(F, -side * w / 4, d / 2, 1.1, 2.15);
			windows(F, w, d, h * 0.55, 3);
			endWindow(F, w, h * 0.55, -side);
			return h + d * 0.5;
		}
		case "workshop": {
			const h = 4.6;
			block(F, CUBE, STONE, [0, 0, h / 2], [w, d, h], { solid: true });
			gableRoof(F, w, d, h, roof);
			for (const s2 of [-1, 1]) door(F, w / 6 + s2 * 0.85, d / 2, 1.6, 3.0);
			// A lean-to on one end and the yard's crates and barrels.
			block(F, CUBE, WOOD, [-w / 2 - 1.6, 0, 1.4], [3.2, d - 1, 2.8], { colour: beam });
			block(F, CUBE, TINT, [-w / 2 - 1.6, 0, 3.0], [3.8, d - 0.4, 0.15], { roll: 0, pitch: -14, colour: slate });
			for (let c = 0; c < 4; c++) block(F, R() < 0.5 ? CUBE : CYL, WOOD, [w / 2 - 1 - c * 1.1, d / 2 + 1.1, 0.45], [0.8, 0.8, 0.9]);
			windows(F, w, d, 2.8, 2);
			backWindows(F, w, d, 2.8, 2); endWindow(F, w, 2.8, 1, 1.3);
			return h + d * 0.5;
		}
		case "tower": {
			// Round, under its conical roof (a round cone on a square tower read wrong, 11-22).
			const h = 12 + R() * 3, D = (w + d) / 2;
			block(F, CYL, STONE, [0, 0, h / 2], [D, D, h], { solid: true });
			block(F, CONE, TINT, [0, 0, h + 3.2], [D * 1.3, D * 1.3, 6.4], { colour: roof });
			block(F, CUBE, WOOD, [0, D / 2 - 0.02, 1.15], [1.2, 0.2, 2.3], { colour: darkWood });
			for (let z = 4; z < h - 1; z += 3.5) windows(F, D, D - 0.12, z, 1, 1);
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
			bench(F, w / 4, 0, 2, 90);
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
	const z = groundAt(...f.at), F = frame(f.at[0], f.at[1], z, f.yaw), look = { roof: [128, 96, 64], plaster: [214, 204, 180], kind: "thatch" };
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
	// (as long as the walls: longer, the roof's hidden half showed past the gables as a big diamond)
	gableRoof(B, 15.8, 10.6, 6.1, pal([146, 124, 82], 0, 0.97, 0, 6));
	for (const s of [-1, 1]) block(B, CUBE, WOOD, [s * 1.05, 5.02, 2.4], [2.0, 0.16, 3.6], { colour: darkWood, nosink: true });
	block(B, CUBE, TINT, [0, 5.06, 4.25], [4.4, 0.18, 0.3], { colour: beam, nosink: true });
	block(B, CUBE, WOOD, [5, 5.02, 4.7], [1.4, 0.16, 1.2], { colour: darkWood, nosink: true });
	// (round 8: yellow cylinders; beehives of hay in the thatch's straw)
	for (let k = 0; k < 3; k++) { const [hx, hy] = F.at(-2 + k * 3.2, 9, 0); block(frame(hx, hy, groundAt(hx, hy), k * 47), HAY, TINT, [0, 0, 1.4], [2.9, 2.9, 3.1], { colour: pal([168, 138, 76], 0, 0.97, 0, 6), solid: true, nosink: true }); }
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
	const hub = [0, 3.9, 13.2], sailCloth = pal([196, 186, 166], 0, 0.95, 0, 9), turn = 17;
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
// Each clump of reeds: real reed grass (plants kind 3: the MW pack's grass clumps drawn tall, WRRoundIsland BuildPlants)
// and here and there a bulrush, a thin stem with its brown head. (11-22: they were bare cylinders with cylinder heads.)
for (const [x, y, n] of PLAN.reeds) {
	for (let k = 0; k < Math.ceil(n / 3); k++) {
		const a = R() * 360, rr = R() * 1.8, px = x + Math.cos(a * RAD) * rr, py = y + Math.sin(a * RAD) * rr;
		plants.push([3, Math.floor(R() * 4), cm(px), cm(py), cm(Math.min(groundAt(px, py), LAKE_LEVEL) - 0.1), Math.round(R() * 360), +(0.7 + R() * 0.45).toFixed(2)]);
		stalks++;
	}
	for (let k = 0; k < Math.ceil(n / 6); k++) {
		const a = R() * 360, rr = R() * 1.6, px = x + Math.cos(a * RAD) * rr, py = y + Math.sin(a * RAD) * rr, h = 1.5 + R() * 0.8, base = Math.min(groundAt(px, py), LAKE_LEVEL);
		const F = frame(px, py, base, R() * 360), tilt = { pitch: (R() - 0.5) * 12, roll: (R() - 0.5) * 12 };
		block(F, CYL, TINT, [0, 0, h / 2 + 0.01], [0.025, 0.025, h], { ...tilt, colour: reedGreen, nosink: true });
		block(F, CYL, TINT, [0, 0, h - 0.12], [0.07, 0.07, 0.24], { ...tilt, colour: reedHead, nosink: true });
	}
}
// The stream's reeds (round 9: it ran between bare banks): clumps of reed grass at its water's edge, in stands with gaps
// between, a bulrush now and then; on their own random stream (RD), so nothing else moves.
{
	const RD = PLAN.rng(20261125), SR = RI.stream;
	let L = 0;
	for (let q = 1; q < SR.length; q++) {
		const [ax, ay] = SR[q - 1], [bx, by] = SR[q], len = Math.hypot(bx - ax, by - ay) || 1, ux = (bx - ax) / len, uy = (by - ay) / len;
		for (let t = 0; t < len; t += 2.2) {
			const cx = ax + ux * t, cy = ay + uy * t, along = L + t;
			if (along < 30) continue;   // (the outlet's own reeds are the lake's)
			for (const side of [-1, 1]) {
				const stand = Math.sin(along / 37 + side * 1.7) + 0.6 * Math.sin(along / 13 + side * 4.1);
				if (stand < 0.55 || RD() < 0.35) continue;
				const off = 3.1 + RD() * 1.4, px = cx - uy * side * off, py = cy + ux * side * off;
				if (PLAN.roads.some((rd) => PLAN.lineDist([px, py], rd.pts).d < rd.half + 3)) continue;
				const g = groundAt(px, py);
				plants.push([3, Math.floor(RD() * 4), cm(px), cm(py), cm(g - 0.1), Math.round(RD() * 360), +(0.6 + RD() * 0.45).toFixed(2)]);
				stalks++;
				if (RD() < 0.18) {
					const h = 1.4 + RD() * 0.7, F = frame(px + (RD() - 0.5), py + (RD() - 0.5), g, RD() * 360), tilt = { pitch: (RD() - 0.5) * 12, roll: (RD() - 0.5) * 12 };
					block(F, CYL, TINT, [0, 0, h / 2 + 0.01], [0.025, 0.025, h], { ...tilt, colour: reedGreen, nosink: true });
					block(F, CYL, TINT, [0, 0, h - 0.12], [0.07, 0.07, 0.24], { ...tilt, colour: reedHead, nosink: true });
				}
			}
		}
		L += len;
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
	// (round 8: three boxes; a clinker hull, thwarts and keel, riding a little down by the stern)
	block(Bt, BOAT, WOOD, [0, 0, 0.12], [3.8, 1.3, 0.6], { colour: pal([92, 64, 42], 0, 0.9, 0, 1), nosink: true, pitch: 1.5 });
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
	// What it stands on (11-22: the deck floated over the banks): two stringers under the planks, a pair of piles at
	// every rail post down to the bed, and a stone abutment under each end.
	for (const side of [-1, 1]) block(F, CUBE, WOOD, [0, side * 0.9, -0.15], [14.4, 0.2, 0.22], { colour: darkWood, nosink: true });
	for (let x = -7; x <= 7; x += 3.5) for (const side of [-1, 1]) {
		const [px, py] = F.at(x, side * 0.9, 0), bed = groundAt(px, py), h = deckZ - 0.25 - bed + 0.4;
		if (h > 0.2) block(frame(px, py, bed - 0.4, yaw), CYL, WOOD, [0, 0, h / 2], [0.22, 0.22, h], { colour: darkWood, nosink: true });
	}
	for (const e of [-1, 1]) { const [ax, ay] = F.at(e * 7.6, 0, 0), g = groundAt(ax, ay); block(frame(ax, ay, g, yaw), CUBE, STONE, [0, 0, (deckZ - g) / 2 - 0.1], [1.4, 2.8, deckZ - g + 0.3], { nosink: true }); }
}

// ---- the forts (round 9) ---------------------------------------------------------------------------------------------
// The four were one box fort four times: the same square of walls, cones on its towers, a barracks box and a pole in an
// empty yard. Each keeps its square and the road through its two gates, and now has its own build from the lands its
// road runs to: the Storm Peaks and Desert road's of warm sandstone, square towers and a keep with a beacon on it; the
// Forest and Tundra road's of grey granite under timber hoardings and shingle cones, a log barracks, a stable and a
// woodpile; the Swamp and Ocean road's low drum towers of pale weathered stone, a beacon tower, nets drying and a boat
// turned over on trestles; the Graveyard, Wasteland and Volcanic road's of dark basalt, tall square towers with turrets
// on their corners, a smithy at its forge and racks of spears. Every yard: barracks with doors, windows and a chimney, a
// stair up to the wall walk, a well, the watch at the gates. Their own random stream (RF), so nothing else moves.
const RF = PLAN.rng(20261124);
const pickF = (arr) => arr[Math.floor(RF() * arr.length)];
const fortGuards = [];   // [frame, x, y, face x, face y, role, outfit]: added with the city's people (their draws last)
const FORT_STYLE = {
	sand: { S: 36, Hh: 8, T: 3, gate: 5, tint: [255, 226, 186], roof: pal([150, 84, 56], 0, 0.75, 0, 5), banner: pal([40, 62, 120], 0, 0.95, 0, 9), towers: "square", tw: 8.2, th: 12.5 },
	granite: { S: 36, Hh: 7, T: 3, gate: 5, tint: [208, 214, 212], roof: pal([104, 92, 78], 0, 0.9, 0, 7), banner: pal([52, 92, 50], 0, 0.95, 0, 9), towers: "hoard", tw: 8.6, th: 10 },
	shore: { S: 36, Hh: 7, T: 3, gate: 5, tint: [232, 236, 226], roof: pal([72, 76, 84], 0, 0.6, 0, 8), banner: pal([40, 104, 112], 0, 0.95, 0, 9), towers: "drum", tw: 10.5, th: 9 },
	basalt: { S: 36, Hh: 10, T: 3.2, gate: 5, tint: [150, 142, 140], roof: pal([52, 50, 54], 0, 0.6, 0, 8), banner: pal([112, 24, 22], 0, 0.95, 0, 9), towers: "turret", tw: 8.6, th: 16 },
};
const fortStyleOf = (t) => t.lands.includes("storm_peaks") ? "sand" : t.lands.includes("forest") ? "granite" : t.lands.includes("swamp") ? "shore" : "basalt";
// A row of merlons from (x0, y0) to (x1, y1) at z (their foot), and a ring of them round (cx, cy).
function merlonRow(F, x0, y0, x1, y1, z, mat, colour, step = 2.6) {
	const L = Math.hypot(x1 - x0, y1 - y0), n = Math.max(1, Math.round(L / step)), ya = Math.atan2(y1 - y0, x1 - x0) / RAD;
	for (let k = 0; k <= n; k++) { const u = k / n; block(F, CUBE, mat, [x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, z + 0.65], [1.2, 0.7, 1.3], { colour, yawAdd: ya, nosink: true }); }
}
function merlonRing(F, cx, cy, r, z, colour) {
	const n = Math.max(6, Math.round(2 * Math.PI * r / 2.3));
	for (let m = 0; m < n; m++) { const a = (m + 0.5) * 360 / n; block(F, CUBE, STONE, [cx + Math.cos(a * RAD) * r, cy + Math.sin(a * RAD) * r, z + 0.65], [1.2, 0.7, 1.3], { colour, yawAdd: a + 90, nosink: true }); }
}
// Arrow slits on a round tower (a dark slot facing out every 60 degrees, two storeys) or along a flat face.
function slitsRound(F, cx, cy, d, zs) { for (const z of zs) for (let m = 0; m < 6; m++) { const a = m * 60 + 30; block(F, CUBE, TINT, [cx + Math.cos(a * RAD) * (d / 2 + 0.01), cy + Math.sin(a * RAD) * (d / 2 + 0.01), z], [0.08, 0.36, 1.4], { yawAdd: a, colour: soot, nosink: true }); } }
// A round tower: a battered foot, a string course, a corbelled parapet a hand proud of it (its corbels under it).
function roundTower(F, cx, cy, d, h, tint, { parapet = true, foot = 1.4 } = {}) {
	block(F, CYL, STONE, [cx, cy, h / 2], [d, d, h], { solid: true, colour: tint });
	block(F, CYL, STONE, [cx, cy, 1.2], [d + foot, d + foot, 2.4], { colour: tint });
	block(F, CYL, STONE, [cx, cy, h * 0.55], [d + 0.3, d + 0.3, 0.35], { colour: tint, nosink: true });
	if (!parapet) return;
	block(F, CYL, STONE, [cx, cy, h + 0.4], [d + 0.9, d + 0.9, 0.8], { colour: tint, nosink: true });
	const n = Math.round(d * 1.7);
	for (let m = 0; m < n; m++) { const a = m * 360 / n; block(F, CUBE, STONE, [cx + Math.cos(a * RAD) * (d / 2 + 0.2), cy + Math.sin(a * RAD) * (d / 2 + 0.2), h - 0.4], [0.45, 0.5, 0.8], { colour: tint, yawAdd: a, nosink: true }); }
	merlonRing(F, cx, cy, d / 2 + 0.05, h + 0.8, tint);
}
// A square tower (or the keep): a battered plinth, a string course, a corbelled parapet, merlons on all four sides.
function squareTower(F, cx, cy, w, h, tint) {
	const G = frame(...F.at(cx, cy, 0).slice(0, 2), F.z, F.yaw);
	block(G, CUBE, STONE, [0, 0, h / 2], [w, w, h], { solid: true, colour: tint });
	block(G, CUBE, STONE, [0, 0, 1.1], [w + 1.0, w + 1.0, 2.2], { colour: tint });
	block(G, CUBE, STONE, [0, 0, h * 0.55], [w + 0.3, w + 0.3, 0.35], { colour: tint, nosink: true });
	block(G, CUBE, STONE, [0, 0, h + 0.4], [w + 0.9, w + 0.9, 0.8], { colour: tint, nosink: true });
	const e = w / 2 + 0.2, n = Math.max(3, Math.round(w / 1.1));
	for (const [ux, uy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
		for (let k = 0; k < n; k++) { const t = -w / 2 + 0.4 + (w - 0.8) * k / (n - 1); block(G, CUBE, STONE, [ux ? ux * e : t, uy ? uy * e : t, h - 0.4], [0.45, 0.45, 0.8], { colour: tint, nosink: true }); }
		const m = (w + 0.9) / 2 - 0.35;
		merlonRow(G, ux ? ux * m : -m, uy ? uy * m : -m, ux ? ux * m : m, uy ? uy * m : m, h + 0.8, STONE, tint, 2.2);
		for (const z of [h * 0.3, h * 0.72]) block(G, CUBE, TINT, [ux * (w / 2 + 0.01), uy * (w / 2 + 0.01), z], ux ? [0.08, 0.36, 1.4] : [0.36, 0.08, 1.4], { colour: soot, nosink: true });
	}
	return G;
}
// A turret corbelled out from a square tower's top corner, under its own spire.
function turret(G, sx, sy, w, h, tint, roof) {
	const cx = sx * (w / 2 + 0.35), cy = sy * (w / 2 + 0.35);
	block(G, CONE, STONE, [cx, cy, h - 1.0], [2.5, 2.5, 2.0], { pitch: 180, colour: tint, nosink: true });
	block(G, CYL, STONE, [cx, cy, h + 1.0], [2.5, 2.5, 4.0], { colour: tint, nosink: true });
	block(G, CONE, TINT, [cx, cy, h + 4.6], [3.1, 3.1, 3.4], { colour: roof });
	block(G, CYL, TINT, [cx, cy, h + 6.7], [0.08, 0.08, 1.0], { colour: iron, nosink: true });
	block(G, CUBE, TINT, [cx + sx * 1.26, cy + sy * 1.26, h + 1.3], [0.08, 0.3, 1.0], { yawAdd: Math.atan2(sy, sx) / RAD, colour: soot, nosink: true });
}
// A cone roof with an iron finial and a pennant over it in the fort's colour.
function coneRoof(F, cx, cy, d, z, h, roof, banner) {
	block(F, CONE, TINT, [cx, cy, z + h / 2], [d, d, h], { colour: roof });
	block(F, CYL, TINT, [cx, cy, z + h + 1.6], [0.12, 0.12, 3.4], { colour: iron, nosink: true });
	block(F, CUBE, CLOTH, [cx + 0.75, cy, z + h + 2.7], [1.5, 0.05, 0.8], { colour: banner, nosink: true });
}
// Windows on a building's front (+y face) on the fort's own random stream, now and then lit after dusk.
function fortWindows(B, w, d, z, n, h = 1.1) {
	for (let i = 0; i < n; i++) {
		const x = -w / 2 + w * (i + 0.5) / n;
		block(B, CUBE, TINT, [x, d / 2 + 0.05, z], [0.85, 0.12, h], { colour: glass, nosink: true });
		block(B, CUBE, TINT, [x, d / 2 + 0.07, z + h / 2 + 0.06], [1.05, 0.14, 0.12], { colour: beam, nosink: true });
		block(B, CUBE, TINT, [x, d / 2 + 0.12, z - h / 2 - 0.06], [1.1, 0.24, 0.12], { colour: beam, nosink: true });
		block(B, CUBE, TINT, [x, d / 2 + 0.09, z], [0.07, 0.1, h], { colour: beam, nosink: true });
		if (RF() < 0.55) block(B, CUBE, TINT, [x, d / 2 + 0.105, z], [0.8, 0.02, h - 0.06], { colour: roomGlow[Math.floor(RF() * roomGlow.length)], nosink: true, night: true });
	}
}
// The barracks along the wall: stone (or logs), a gable roof, two chimneys, doors and windows to the yard, a bench and a
// lantern by the door.
function barracks(F, cx, cy, w, d, h, st, logs) {
	const B = frame(...F.at(cx, cy, 0).slice(0, 2), F.z, F.yaw);
	if (logs) {
		block(B, CUBE, TINT, [0, 0, h / 2], [w - 0.3, d - 0.3, h], { solid: true, colour: darkWood });
		for (let z = 0.2, k = 0; z < h; z += 0.36, k++) {
			const c = k % 3 === 1 ? beam : darkWood;
			for (const sy of [-1, 1]) block(B, CYL, WOOD, [0, sy * d / 2, z], [0.38, 0.38, w + 0.8], { pitch: 90, colour: c, nosink: true });
			for (const sx of [-1, 1]) block(B, CYL, WOOD, [sx * w / 2, 0, z + 0.18], [0.38, 0.38, d + 0.8], { pitch: 90, yawAdd: 90, colour: c, nosink: true });
		}
	} else {
		block(B, CUBE, STONE, [0, 0, h / 2], [w, d, h], { solid: true, colour: st.tintPal });
		block(B, CUBE, STONE, [0, 0, 0.3], [w + 0.2, d + 0.2, 0.6], { colour: st.tintPal });
	}
	gableRoof(B, w, d, h, st.roof);
	const fd = logs ? d + 0.42 : d;   // (its front face: the logs stand proud of the box, the doors and windows on them)
	for (const sx of [-1, 1]) {
		block(B, CUBE, STONE, [sx * w / 3, -d / 4, h + 1.4], [1.0, 1.0, 4.2], { colour: st.tintPal, nosink: true });
		block(B, CUBE, STONE, [sx * w / 3, -d / 4, h + 3.55], [1.25, 1.25, 0.25], { colour: st.tintPal, nosink: true });
	}
	for (const dx of [-w / 4, w / 4]) door(B, dx, fd / 2, 1.2, 2.3);
	fortWindows(B, w, fd, 2.3, 4);
	bench(B, -w / 4 + 2.2, fd / 2 + 0.6, 2.2);
	block(B, CUBE, TINT, [w / 4 + 1.1, fd / 2 + 0.25, 2.6], [0.3, 0.3, 0.42], { colour: iron, nosink: true });
	block(B, CUBE, TINT, [w / 4 + 1.1, fd / 2 + 0.25, 2.6], [0.34, 0.2, 0.32], { colour: lanternGlow, nosink: true, night: true });
	fire(FIRE.lantern, B.at(w / 4 + 1.1, fd / 2 + 0.25, 2.6), 0.8);
}
// A well with a stone kerb, its roof on two posts and a windlass (a fort's: on its own ground).
function fortWell(F, x, y) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + RF() * 90);
	block(G, CYL, STONE, [0, 0, 0.45], [2.2, 2.2, 0.9], { solid: true });
	block(G, CYL, TINT, [0, 0, 0.92], [1.6, 1.6, 0.05], { colour: pal([24, 46, 58], 0, 0.15, 0, 3) });
	for (const sx of [-1, 1]) block(G, CUBE, WOOD, [sx * 1.05, 0, 1.4], [0.18, 0.18, 2.8]);
	for (const sd of [-1, 1]) block(G, CUBE, TINT, [0, sd * 0.44, 3.04], [2.9, 1.3, 0.07], { roll: sd * 45, colour: slate, nosink: true });
	block(G, CYL, WOOD, [0, 0, 2.1], [0.16, 0.16, 2.1], { pitch: 90, colour: beam, nosink: true });
	block(G, CYL, TINT, [0.35, 0, 1.6], [0.03, 0.03, 1.0], { colour: pal([150, 128, 92], 0, 0.95), nosink: true });
	block(G, CYL, WOOD, [0.35, 0, 1.0], [0.34, 0.34, 0.32], { colour: darkWood, nosink: true });
}
// The yard's things.
function crates(F, x, y) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + (RF() - 0.5) * 30);
	const n = 3 + Math.floor(RF() * 3);
	for (let k = 0; k < n; k++) {
		const sz = 0.7 + RF() * 0.35, up = k >= 3;
		block(G, CUBE, WOOD, [(k % 3) * 1.0 - 1 + (up ? 0.5 : 0), (RF() - 0.5) * 0.3, up ? 1.0 + sz / 2 : sz / 2], [sz, sz, sz], { yawAdd: (RF() - 0.5) * 20, colour: pickF([beam, darkWood, pal([120, 92, 60], 0, 0.9, 0, 1)]), solid: !up, nosink: up });
	}
	for (let k = 0; k < 2; k++) {
		const bx = 2.0 + k * 0.85;
		block(G, CYL, WOOD, [bx, 0.2, 0.48], [0.72, 0.72, 0.95], { colour: k ? beam : darkWood, solid: true });
		for (const hz of [0.18, 0.78]) block(G, CYL, TINT, [bx, 0.2, hz], [0.75, 0.75, 0.05], { colour: iron, nosink: true });
	}
}
function butt(F, x, y, yawAdd) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	for (const sx of [-0.5, 0.5]) block(G, CUBE, WOOD, [sx, 0.3, 0.75], [0.12, 0.12, 1.6], { pitch: -14, colour: darkWood, nosink: true });
	block(G, CYL, TINT, [0, 0, 1.15], [1.3, 1.3, 0.3], { pitch: 90, colour: straw, nosink: true });
	block(G, CYL, TINT, [0, -0.16, 1.15], [0.8, 0.8, 0.02], { pitch: 90, colour: cloth, nosink: true });
	block(G, CYL, TINT, [0, -0.17, 1.15], [0.5, 0.5, 0.02], { pitch: 90, colour: straw, nosink: true });
	block(G, CYL, TINT, [0, -0.18, 1.15], [0.18, 0.18, 0.02], { pitch: 90, colour: cloth, nosink: true });
	for (let k = 0; k < 2; k++) block(G, CYL, TINT, [(RF() - 0.5) * 0.6, -0.45, 1.1 + (RF() - 0.5) * 0.5], [0.03, 0.03, 0.7], { pitch: 90 - 8, yawAdd: 90 + (RF() - 0.5) * 10, colour: beam, nosink: true });
}
function rack(F, x, y, yawAdd, shield) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	for (const sx of [-1.1, 1.1]) block(G, CUBE, WOOD, [sx, 0, 0.8], [0.14, 0.14, 1.6], { colour: darkWood });
	for (const z of [0.5, 1.45]) block(G, CUBE, WOOD, [0, 0, z], [2.4, 0.12, 0.1], { colour: beam, nosink: true });
	for (let k = 0; k < 5; k++) {
		block(G, CYL, TINT, [-0.8 + k * 0.4, 0.12, 1.25], [0.05, 0.05, 2.5], { pitch: 8, colour: beam, nosink: true });
		block(G, CONE, TINT, [-0.8 + k * 0.4, 0.12 - 0.17, 2.55], [0.09, 0.09, 0.3], { pitch: 8, colour: iron, nosink: true });
	}
	if (shield) for (const sx of [-1.75, 1.75]) block(G, CYL, TINT, [sx, 0.15, 0.6], [0.75, 0.75, 0.06], { pitch: 90, roll: 8, colour: shield, nosink: true });
}
function woodpile(F, x, y, len, yawAdd) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	for (let row = 0; row < 4; row++) for (let k = 0; k < 7 - row; k++) block(G, CYL, WOOD, [(k - (6 - row) / 2) * 0.36, 0, 0.18 + row * 0.31], [0.34, 0.34, len], { pitch: 90, yawAdd: 90, colour: k % 2 ? beam : darkWood, nosink: true });
	solids.push([...G.at(0, 0, 0.7).map(cm), cm(2.6), cm(len), cm(1.4), +G.yaw.toFixed(1)]);
}
function stable(F, x0, x1, yWall, depth, st) {
	const mid = (x0 + x1) / 2, len = x1 - x0, yF = yWall - depth, hi = 4.0, lo = 2.7;
	for (let x = x0; x <= x1 + 0.01; x += len / 3) block(F, CUBE, WOOD, [x, yF, lo / 2], [0.22, 0.22, lo], { colour: darkWood });
	const run = Math.hypot(depth, hi - lo), ang = Math.atan2(hi - lo, depth) / RAD;
	block(F, CUBE, TINT, [mid, yF + depth / 2 - 0.2, (hi + lo) / 2 + 0.1], [len + 0.8, run + 0.9, 0.14], { roll: ang, colour: st.roof, nosink: true });
	block(F, CUBE, TINT, [mid, yF, lo - 0.05], [len + 0.3, 0.2, 0.22], { colour: beam, nosink: true });
	for (let x = x0 + 2.5; x < x1 - 1; x += 2.5) block(F, CUBE, TINT, [x, yF + depth / 2, 0.65], [0.08, depth - 0.3, 1.3], { colour: beam, nosink: true });
	for (let k = 0; k < 4; k++) block(F, CUBE, TINT, [x0 + 1.2 + k * 0.9 + (k > 1 ? 4 : 0), yWall - 0.6, 0.35 + (k === 1 ? 0.5 : 0)], [0.85, 0.5, 0.45], { yawAdd: (RF() - 0.5) * 20, colour: straw, nosink: true });
	const T0 = frame(...F.at(mid, yF - 0.9, 0).slice(0, 2), F.z, F.yaw);
	block(T0, CUBE, WOOD, [0, 0, 0.32], [2.6, 0.7, 0.64], { colour: darkWood, solid: true });
	block(T0, CUBE, TINT, [0, 0, 0.62], [2.4, 0.5, 0.02], { colour: pal([24, 46, 58], 0, 0.15, 0, 3), nosink: true });
}
function smithy(F, x, y, st) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw);
	block(G, CUBE, STONE, [0, 0, 0.5], [1.8, 1.6, 1.0], { solid: true, colour: st.tintPal });
	block(G, CUBE, TINT, [0, 0, 1.02], [1.3, 1.1, 0.06], { colour: ember, nosink: true });
	block(G, CUBE, STONE, [0, 0.2, 2.4], [1.9, 1.3, 0.9], { pitch: 0, colour: st.tintPal, nosink: true });
	block(G, CUBE, STONE, [0, 0.4, 4.0], [0.9, 0.9, 2.6], { colour: st.tintPal, nosink: true });
	for (const sx of [-0.8, 0.8]) block(G, CUBE, STONE, [sx, 0.55, 1.45], [0.3, 0.4, 1.0], { colour: st.tintPal, nosink: true });
	fire(FIRE.fireplace, G.at(0, 0, 1.15), 0.5);
	block(G, CYL, WOOD, [0.2, -1.9, 0.3], [0.7, 0.7, 0.6], { colour: darkWood });
	block(G, CUBE, TINT, [0.2, -1.9, 0.72], [0.7, 0.28, 0.24], { colour: iron, nosink: true });
	block(G, CONE, TINT, [0.67, -1.9, 0.75], [0.18, 0.18, 0.3], { pitch: 90, colour: iron, nosink: true });
	block(G, CYL, WOOD, [1.6, -1.3, 0.45], [0.75, 0.75, 0.9], { colour: beam, solid: true });
	block(G, CYL, TINT, [1.6, -1.3, 0.88], [0.6, 0.6, 0.02], { colour: pal([24, 34, 40], 0, 0.15, 0, 3), nosink: true });
	block(G, CUBE, TINT, [-1.5, -0.6, 0.9], [0.9, 0.5, 0.06], { pitch: -60, colour: pal([92, 60, 36], 0, 0.9, 0, 1), nosink: true });
	// its open shed: four posts and a pent roof
	for (const sx of [-2.2, 2.2]) for (const sy of [-2.8, 1.2]) block(G, CUBE, WOOD, [sx, sy, 1.5], [0.2, 0.2, 3.0], { colour: darkWood });
	block(G, CUBE, TINT, [0, -0.8, 3.15], [5.2, 4.8, 0.14], { roll: -9, colour: st.roof, nosink: true });
	return G;
}
function cart(F, x, y, yawAdd) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	block(G, CUBE, WOOD, [0, 0, 0.95], [2.4, 1.3, 0.1], { colour: beam, nosink: true });
	for (const sy of [-0.62, 0.62]) block(G, CUBE, WOOD, [0, sy, 1.2], [2.4, 0.06, 0.45], { colour: darkWood, nosink: true });
	for (const sx of [-1.18, 1.18]) block(G, CUBE, WOOD, [sx, 0, 1.2], [0.06, 1.3, 0.45], { colour: darkWood, nosink: true });
	for (const sy of [-0.78, 0.78]) {
		block(G, CYL, TINT, [0.2, sy, 0.62], [1.24, 1.24, 0.08], { roll: 90, colour: darkWood, nosink: true });
		block(G, CYL, TINT, [0.2, sy * 1.02, 0.62], [0.25, 0.25, 0.14], { roll: 90, colour: iron, nosink: true });
		block(G, CUBE, WOOD, [-2.3, sy * 0.45, 0.62], [2.4, 0.08, 0.08], { pitch: -8, colour: beam, nosink: true });
	}
	block(G, CUBE, TINT, [0.3, 0, 1.25], [1.2, 0.9, 0.5], { colour: pal([120, 102, 70], 0, 0.95, 0, 9), nosink: true });
	solids.push([...G.at(0, 0, 0.8).map(cm), cm(2.5), cm(1.6), cm(1.6), +G.yaw.toFixed(1)]);
}
function netRack(F, x, y, yawAdd) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	for (const sx of [-1.8, 0, 1.8]) block(G, CUBE, WOOD, [sx, 0, 1.2], [0.12, 0.12, 2.4], { colour: darkWood });
	block(G, CYL, WOOD, [0, 0, 2.35], [0.1, 0.1, 3.9], { pitch: 90, yawAdd: 0, colour: beam, nosink: true });
	// each net a mesh of cords (a solid sheet read as a signboard), sagging a little
	const cord = pal([120, 110, 88], 0, 0.95, 0, 9);
	for (const [ox, w, drop] of [[-0.9, 1.7, 1.5], [0.9, 1.6, 1.8]]) {
		for (let x = -w / 2; x <= w / 2 + 0.01; x += 0.17) block(G, CUBE, TINT, [ox + x, 0.03, 2.3 - drop / 2], [0.018, 0.018, drop], { colour: cord, nosink: true });
		for (let zz = 0.15; zz < drop; zz += 0.17) block(G, CUBE, TINT, [ox, 0.03 + 0.04 * Math.sin(Math.PI * zz / drop), 2.3 - zz], [w, 0.018, 0.018], { colour: cord, nosink: true });
	}
	for (let k = 0; k < 5; k++) block(G, SPH, TINT, [-1.6 + k * 0.8, 0.07, 0.75 + (k % 2) * 0.3], [0.16, 0.16, 0.16], { colour: pal([196, 160, 90], 0, 0.5, 0, 3), nosink: true });
}
function upturnedBoat(F, x, y, yawAdd) {
	const G = frame(...F.at(x, y, 0).slice(0, 2), F.z, F.yaw + yawAdd);
	for (const sx of [-1.1, 1.1]) {
		block(G, CUBE, WOOD, [sx, 0, 0.75], [0.12, 1.3, 0.12], { colour: darkWood, nosink: true });
		for (const sy of [-0.55, 0.55]) block(G, CUBE, WOOD, [sx, sy, 0.38], [0.1, 0.1, 0.78], { roll: sy > 0 ? 12 : -12, colour: darkWood, nosink: true });
	}
	block(G, BOAT, WOOD, [0, 0, 1.15], [3.8, 1.3, 0.6], { roll: 180, colour: pal([92, 64, 42], 0, 0.9, 0, 1), nosink: true });
}
function fortAt(t) {
	const style = fortStyleOf(t), st = { ...FORT_STYLE[style] };
	st.tintPal = pal(st.tint, 0, 0.9, 0, 0);
	const { S, Hh, T, gate } = st, tint = st.tintPal;
	const [fx, fy] = t.fort, z = groundAt(fx, fy), F = frame(fx, fy, z, t.bearing - 90);
	// The curtain: four walls (the two ends open at the gate), a battered plinth outside, slits along them; the walk on top.
	const wallPiece = (cx, cy, lx, ly) => {
		block(F, CUBE, STONE, [cx, cy, Hh / 2], [lx, ly, Hh], { solid: true, colour: tint });
		block(F, CUBE, STONE, [cx, cy, 0.8], [lx + (lx > ly ? 0 : 1.0), ly + (ly > lx ? 0 : 1.0), 1.6], { colour: tint });
	};
	for (const side of [-1, 1]) {
		wallPiece(0, side * S / 2, S, T);
		for (const end of [-1, 1]) wallPiece(end * S / 2, side * (gate + (S / 2 - gate) / 2), T, S / 2 - gate);
		for (let m = -S / 2 + 7; m <= S / 2 - 7; m += 5.5) block(F, CUBE, TINT, [m, side * (S / 2 + T / 2 + 0.01), 3.6], [0.36, 0.08, 1.4], { colour: soot, nosink: true });
		if (style === "granite") {
			// a timber breastwork on the wall walk, posts along it, instead of stone merlons
			block(F, CUBE, TINT, [0, side * (S / 2 + 1.05), Hh + 0.7], [S - 6, 0.25, 1.4], { colour: beam, nosink: true });
			for (let m = -S / 2 + 4; m <= S / 2 - 4; m += 2.5) block(F, CUBE, TINT, [m, side * (S / 2 + 1.25), Hh + 0.9], [0.22, 0.22, 1.8], { colour: darkWood, nosink: true });
		} else merlonRow(F, -S / 2 + 4, side * (S / 2 + 1.1), S / 2 - 4, side * (S / 2 + 1.1), Hh, STONE, tint);
		for (const end of [-1, 1]) if (style !== "granite") merlonRow(F, end * (S / 2 + 1.1), side * (gate + 2.4), end * (S / 2 + 1.1), side * (S / 2 - 4.5), Hh, STONE, tint);
	}
	// The gatehouses: the opening 5.4 m high under the wall's head, a portcullis's teeth showing under it, the leaves
	// swung open against the passage, a torch either side outside and the fort's banners over the arch.
	for (const end of [-1, 1]) {
		const top = Hh + 3, low = 5.4;
		block(F, CUBE, STONE, [end * S / 2, 0, (low + top) / 2], [T + 0.4, 2 * gate + 0.4, top - low], { solid: true, colour: tint, nosink: true });
		for (const m of [-3.9, -1.3, 1.3, 3.9]) block(F, CUBE, STONE, [end * S / 2 + end * 0.2, m, top + 0.6], [T + 0.2, 1.3, 1.2], { colour: tint, nosink: true });
		for (let k = 0; k < 13; k++) block(F, CUBE, TINT, [end * S / 2, -gate + 0.35 + k * (2 * gate - 0.7) / 12, low - 0.35], [0.12, 0.12, 0.9], { colour: iron, nosink: true });
		block(F, CUBE, TINT, [end * S / 2, 0, low - 0.15], [0.14, 2 * gate - 0.4, 0.14], { colour: iron, nosink: true });
		for (const sd of [-1, 1]) {
			block(F, CUBE, WOOD, [end * (S / 2 - T / 2 - gate / 2), sd * (gate - 0.25), 2.7], [gate - 0.2, 0.3, 5.3], { colour: darkWood });
			for (const zz of [1.0, 4.4]) block(F, CUBE, TINT, [end * (S / 2 - T / 2 - gate / 2), sd * (gate - 0.42), zz], [gate - 0.5, 0.05, 0.16], { colour: iron, nosink: true });
			fire(FIRE.torch, F.at(end * (S / 2 + T / 2 + 0.35), sd * (gate + 1.1), 4.0), 1.2);
			block(F, CUBE, TINT, [end * (S / 2 + T / 2 + 0.12), sd * (gate + 1.1), 3.5], [0.3, 0.16, 0.9], { colour: iron, nosink: true });
			block(F, CUBE, CLOTH, [end * (S / 2 + T / 2 + 0.25), sd * 2.4, low + 1.95], [0.05, 1.3, 3.0], { colour: st.banner, nosink: true });
			block(F, CUBE, TINT, [end * (S / 2 + T / 2 + 0.25), sd * 2.4, low + 3.5], [0.08, 1.5, 0.08], { colour: iron, nosink: true });
		}
		fire(FIRE.brazier, F.at(end * S / 2, 0, top + 0.05), 0.7);
	}
	// The towers on the corners, each fort's own.
	for (const ex of [-1, 1]) for (const sy of [-1, 1]) {
		const cx = ex * S / 2, cy = sy * S / 2;
		if (st.towers === "square") {
			squareTower(F, cx, cy, st.tw, st.th, tint);
			block(F, CYL, TINT, [cx, cy, st.th + 3.2], [0.14, 0.14, 4.4], { colour: darkWood, nosink: true });
			block(F, CUBE, CLOTH, [cx + 0.9, cy, st.th + 4.6], [1.8, 0.06, 1.1], { colour: st.banner, nosink: true });
		} else if (st.towers === "hoard") {
			roundTower(F, cx, cy, st.tw, st.th, tint, { parapet: false });
			// the timber hoarding: a gallery of boards round the top on beams run out of the stone, slots in it, and a
			// shingle cone over all
			for (let m = 0; m < 12; m++) { const a = m * 30; block(F, CUBE, TINT, [cx + Math.cos(a * RAD) * (st.tw / 2 + 0.3), cy + Math.sin(a * RAD) * (st.tw / 2 + 0.3), st.th - 0.15], [1.4, 0.24, 0.3], { yawAdd: a, colour: darkWood, nosink: true }); }
			block(F, CYL, TINT, [cx, cy, st.th + 1.35], [st.tw + 1.5, st.tw + 1.5, 2.7], { colour: beam, nosink: true });
			for (let m = 0; m < 16; m++) { const a = m * 22.5; block(F, CUBE, TINT, [cx + Math.cos(a * RAD) * (st.tw / 2 + 0.76), cy + Math.sin(a * RAD) * (st.tw / 2 + 0.76), st.th + 1.35], [0.12, 0.2, 2.7], { yawAdd: a, colour: darkWood, nosink: true }); }
			slitsRound(F, cx, cy, st.tw + 1.52, [st.th + 1.5]);
			coneRoof(F, cx, cy, st.tw + 3.0, st.th + 2.65, st.tw * 0.95, st.roof, st.banner);
		} else if (st.towers === "drum") {
			if (ex === 1 && sy === 1) {
				// the beacon tower: tall and slender, its lantern room at the top lit at dusk
				const bd = 7, bh = 21;
				roundTower(F, cx, cy, bd, bh, tint, { parapet: false, foot: 2.0 });
				for (const zz of [7, 14]) block(F, CYL, STONE, [cx, cy, zz], [bd + 0.35, bd + 0.35, 0.35], { colour: tint, nosink: true });
				block(F, CYL, STONE, [cx, cy, bh + 0.3], [bd + 1.2, bd + 1.2, 0.6], { colour: tint, nosink: true });
				for (let m = 0; m < 8; m++) { const a = m * 45 + 22.5; block(F, CUBE, TINT, [cx + Math.cos(a * RAD) * 2.4, cy + Math.sin(a * RAD) * 2.4, bh + 2.1], [0.22, 0.22, 3.0], { colour: iron, nosink: true }); }
				block(F, CYL, TINT, [cx, cy, bh + 2.1], [4.6, 4.6, 2.8], { colour: glass, nosink: true });
				block(F, CYL, TINT, [cx, cy, bh + 2.1], [4.5, 4.5, 2.7], { colour: lanternGlow, nosink: true, night: true });
				block(F, CYL, TINT, [cx, cy, bh + 3.65], [5.6, 5.6, 0.3], { colour: iron, nosink: true });
				coneRoof(F, cx, cy, 5.8, bh + 3.8, 2.8, st.roof, st.banner);
				fire(FIRE.brazier, F.at(cx, cy, bh + 1.2), 1.6);
				slitsRound(F, cx, cy, bd, [4, 10, 16]);
			} else {
				roundTower(F, cx, cy, st.tw, st.th, tint, { foot: 2.2 });
				slitsRound(F, cx, cy, st.tw, [3.5]);
				block(F, CYL, TINT, [cx, cy, st.th + 3.0], [0.14, 0.14, 4.0], { colour: darkWood, nosink: true });
				block(F, CUBE, CLOTH, [cx + 0.9, cy, st.th + 4.3], [1.8, 0.06, 1.1], { colour: st.banner, nosink: true });
			}
		} else {
			const G = squareTower(F, cx, cy, st.tw, st.th, tint);
			for (const tx of [-1, 1]) for (const ty of [-1, 1]) turret(G, tx, ty, st.tw, st.th, tint, st.roof);
		}
	}
	// The yard. Barracks along one wall, a stair up the other to the wall walk, a well; the rest each fort's own.
	const inner = S / 2 - T / 2, flip = 1;
	barracks(F, 0, -(inner - 0.2 - 3.6), 22, 7.2, style === "basalt" ? 5.2 : 4.4, st, style === "granite");
	{
		// (the stair's head stops short of the corner tower; each step a solid you climb)
		const steps = Math.ceil(Hh / 0.34), rise = Hh / steps, y0 = inner - 0.85, L = steps * 0.48, x0 = 12.4 - L;
		for (let k = 0; k < steps; k++) {
			const top = (k + 1) * rise;
			block(F, CUBE, STONE, [x0 + 0.24 + k * 0.48, y0, top / 2], [0.5, 1.6, top], { solid: true, colour: tint });
		}
		block(F, CUBE, TINT, [x0 + L / 2, y0 - 0.78, Hh / 2 + 0.95], [Math.hypot(L, Hh), 0.08, 0.08], { pitch: Math.atan2(Hh, L) / RAD, colour: darkWood, nosink: true });
		for (const u of [0.15, 0.5, 0.85]) block(F, CUBE, TINT, [x0 + L * u, y0 - 0.78, Hh * u + 0.48], [0.08, 0.08, 0.95], { colour: darkWood, nosink: true });
	}
	fortWell(F, style === "shore" ? -3 : style === "basalt" ? -1 : 4, 7.5 * flip);
	if (style === "sand") {
		// the keep: a square tower in the yard's corner, its door to the yard, a beacon in an iron basket on its roof
		const G = squareTower(F, -9, 10.2, 10, 17, tint);
		const G2 = frame(...G.at(0, 0, 0).slice(0, 2), F.z, F.yaw + 180);
		door(G2, 0, 5, 1.6, 2.8);
		block(G2, CUBE, STONE, [0, 5.8, 0.2], [3.2, 1.4, 0.4], { colour: tint });
		block(G, CYL, STONE, [0, 0, 17.6], [2.2, 2.2, 1.2], { colour: tint, nosink: true });
		block(G, CYL, TINT, [0, 0, 18.6], [1.8, 1.8, 0.9], { colour: iron, nosink: true });
		fire(FIRE.brazier, G.at(0, 0, 19.2), 1.5);
		block(G, CYL, TINT, [3.6, 3.6, 21], [0.18, 0.18, 7], { colour: darkWood, nosink: true });
		block(G, CUBE, CLOTH, [3.6 + 1.3, 3.6, 22.8], [2.6, 0.06, 1.6], { colour: st.banner, nosink: true });
		for (const zz of [6, 11]) block(G2, CUBE, TINT, [0, 5.01, zz], [0.7, 0.08, 1.3], { colour: soot, nosink: true });
		butt(F, 8.5, 11.5, 0); butt(F, 11.5, 11.5, 0);
		rack(F, 10, 6.5, 0, st.banner);
		crates(F, -0.5, 13.0);
	} else if (style === "granite") {
		stable(F, -12.4, -3.5, inner - 0.2, 3.6, st);
		woodpile(F, 9.5, 12.8, 2.6, 0); woodpile(F, 9.5, 10.2, 2.6, 0);
		crates(F, -10, 6.5);
		cart(F, 3, 11.8, 8);
	} else if (style === "shore") {
		netRack(F, -10.5, 9.5, 0); netRack(F, -10.5, 6.2, 0);
		upturnedBoat(F, 5, 10, 0);
		crates(F, 9.5, 6.2);
		for (let k = 0; k < 4; k++) block(F, CYL, TINT, [-5 + k * 0.7, 13.6, 0.3], [0.55, 0.55, 0.6], { colour: pal([120, 96, 62], 0, 0.95, 0, 9), solid: k === 0 });
	} else {
		smithy(F, -9.5, 10.5, st);
		rack(F, 3.5, 12.6, 0, st.banner); rack(F, 7.5, 12.6, 0, st.banner);
		butt(F, 11, 9.5, 0);
		cart(F, 8, 6.8, -5);
		crates(F, -4, 13.3);
		// a spiked palisade of stakes in front of each gate's flanks, outside
		for (const end of [-1, 1]) for (const sd of [-1, 1]) for (let k = 0; k < 6; k++) block(F, CYL, WOOD, [end * (S / 2 + 4.2 + (k % 2) * 0.5), sd * (gate + 2.5 + k * 1.3), 1.0], [0.24, 0.24, 2.4], { pitch: end * 28, colour: darkWood, nosink: true });
	}
	// The watch: two at each gate inside, one on the wall walk at the head of the stair; a smith at the forge.
	for (const end of [-1, 1]) for (const sd of [-1, 1]) fortGuards.push([F, end * (S / 2 - T / 2 - gate - 0.8), sd * (gate - 1.6), end, 0, "guard", "iron"]);
	fortGuards.push([frame(fx, fy, z + Hh, F.yaw), 4, S / 2, 0, 1, "guard", "iron"]);
	if (style === "basalt") fortGuards.push([F, -9.5, 8.0, 0, 1, "townsfolk", "townwear_b"]);
}
// ---- a fort on each main road at the meadow's edge (the road runs through it), a waymark at each crossroads -------------
for (const t of trunks) {
	fortAt(t);
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

// ---- the city's people: routes the townsfolk stroll and who walks them; the watch at the gates; neighbours on the greens
{
	const sample = (pts, closed, minR) => {
		const out = [];
		const ring = closed ? [...pts, pts[0]] : pts;
		for (let i = 1; i < ring.length; i++) {
			const a = ring[i - 1], b = ring[i], L = Math.hypot(b[0] - a[0], b[1] - a[1]), n = Math.max(1, Math.round(L / 6));
			for (let k = 0; k < n; k++) { const p = [a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n]; if (Math.hypot(...p) >= minR) out.push(p); }
		}
		if (!closed) { const e = ring[ring.length - 1]; if (Math.hypot(...e) >= minR) out.push(e); }
		return out;
	};
	const addWalk = (pts, closed, walkers) => {
		if (pts.length < 3) return;
		const w = people.walks.length;
		people.walks.push({ closed, pts: pts.map((p) => [cm(p[0]), cm(p[1])]) });
		for (let k = 0; k < walkers; k++) { const p = person(); people.walkers.push([w, Math.floor(R() * pts.length), R() < 0.5 ? 1 : -1, p.name, p.sex, p.face, p.build, p.outfit]); }
	};
	const plazaLoop = []; for (let k = 0; k < 40; k++) plazaLoop.push(polar(k * 9, 35.3));
	addWalk(plazaLoop, true, 6);
	for (const st of PLAN.streets) {
		if (st.kind === "ring") addWalk(sample(st.pts, true, 0), true, 8);
		else if (st.kind === "alley") addWalk(sample(st.pts, true, 0), true, 3);
		else if (st.kind === "avenue") addWalk(sample(st.pts, false, 47), false, 2);
	}
	const lanes = PLAN.streets.filter((st) => st.kind === "lane");
	for (let i = 0; i < lanes.length; i += 6) addWalk(sample(lanes[i].pts, false, 47), false, 1);
	// Two of the watch at each gate, on the town side of the passage, looking out along the road.
	for (const t of trunks) {
		const F = frame(...polar(t.bearing, WALL.r), CITY_TOP, t.bearing);
		for (const sd of [-1, 1]) stand(F, sd * (GATE_HALF - 1.2), 7.2, 0, -1, "guard", person("iron"), "");
	}
	// A neighbour by the well on every other green.
	quarterGreens.forEach((c, i) => { if (i % 2 === 0) stand(frame(c[0] + 2.4, c[1] + 0.8, CITY_TOP, 0), 0, 0, -1, -0.3, "townsfolk", person()); });
	// The forts' watch (round 9: the forts stood empty).
	for (const [F, x, y, fxx, fyy, role, outfit] of fortGuards) stand(F, x, y, fxx, fyy, role, person(outfit), role === "guard" ? "of the fort's watch" : "the smith");
}

fs.writeFileSync(path.join(root, "Data/hearthmoot.json"), JSON.stringify({
	_comment: "Generated by Tools/terrain/hearthmoot.mjs from the shared plan (Tools/terrain/hearthmoot_plan.mjs; edit the scripts, not this file). Hearthmoot and its meadow, built on every machine by WRRoundIsland.cpp (no replication). blocks: [mesh 0 cube / 1 cylinder / 2 cone / 4 sphere / 5 awning / 6 haystack / 7 rowboat, mat 0 wood / 1 stone / 2 cloth / 3 trim / 4 tinted, cx, cy, cz, sx, sy, sz (game cm), yaw, pitch, roll (degrees), palette index or -1]; palette rows [r, g, b (sRGB), metal, rough, glow, pattern]; nightBlocks: the same, shown only after dusk (lit windows, lantern glass); fires: [kind 0 hearth / 1 brazier / 2 lantern light / 3 torch / 4 fireplace / 5 lamp, cx, cy, cz, scale]; floors: [cx, cy, half x, half y, yaw] rooms you walk into (no grass); people: walks {closed, pts [[x, y]]}, walkers [walk, start point, direction, name, sex, face, build, outfit], sellers [x, y, z, yaw, trade table, name, sex, face, build, outfit], standers [x, y, z, yaw, role, name, sex, face, build, outfit]; solids: [cx, cy, cz, sx, sy, sz, yaw] boxes you bump into; plants: [kind 0 tree / 1 sapling / 2 shrub, variant, cx, cy, cz (its base), yaw, scale].",
	cityTop: CITY_TOP, palette, blocks, nightBlocks, fires, solids, plants, floors, people,
}));
console.log(`Hearthmoot: ${blocks.length} blocks, ${nightBlocks.length} night blocks, ${fires.length} fires (${lanterns} street lanterns), ${solids.length} solids, ${plants.length} plants (${PLAN.trees.length} meadow trees, ${hedge} hedge shrubs), ${stalks} reeds, ${palette.length} colours`);
console.log(`people: ${people.walkers.length} walkers on ${people.walks.length} walks, ${people.sellers.length} sellers, ${people.standers.length} standing; ${floors.length} floors`);
console.log(`buildings: ${Object.entries(counts).map(([k, v]) => `${v} ${k}`).join(", ")}; ${quarterGreens.length} greens; fields ${PLAN.fields.length}, farms ${PLAN.farms.length}, windmills ${PLAN.windmills.length}`);
