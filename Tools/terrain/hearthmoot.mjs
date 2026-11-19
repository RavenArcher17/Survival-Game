// Hearthmoot, first pass (plan phase 3, slice 1: the city, its meadow, the lake and the main roads). The city on its
// hill in the middle of the round island, built from the game's own building look (FWRBuildLook blocks: coursed stone,
// timber, tinted roofs), so it reads like the forts players build: a curtain wall 600 m across with twelve towers and
// four gatehouses where the main roads arrive, the hearth on the central plaza, a ring street, and twelve quarters (one
// per path, Raven's answers: in the ring's order) of houses roofed in their path's colour, each with its hall (where its
// path master will stand). Out at the meadow's edge, a fort on each main road; a waymark at each crossroads.
// Writes Data/hearthmoot.json: "blocks" (what you see) and "solids" (what you bump into), both built on every machine by
// WRRoundIsland.cpp (no replication). Every number in metres here; the file is in game cm.
//   node Tools/terrain/hearthmoot.mjs        (after round_ground.mjs: it reads the ground's heights)
import fs from "fs";
import path from "path";
import zlib from "zlib";
import { fileURLToPath } from "url";
import * as RI from "./round_island.mjs";

const { quarters, trunks, CITY_WALL, RAD, norm, dAng, pt } = RI;
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");

// ---- the ground's heights (Data/lands/island.json + its bin) ------------------------------------------------------
const land = JSON.parse(fs.readFileSync(path.join(root, "Data/lands/island.json"), "utf8"));
const HF = land.heightfield, N = HF.samples;
const raw = zlib.inflateSync(fs.readFileSync(path.join(root, "Data/lands", HF.file)));
const sample = (i, j) => (raw.readUInt16LE((Math.max(0, Math.min(N - 1, j)) * N + Math.max(0, Math.min(N - 1, i))) * 2) - HF.offset) / HF.unitsPerMetre;
const groundAt = (x, y) => {
	const fi = (x - HF.x0) / HF.step, fj = (y - HF.y0) / HF.step, i = Math.floor(fi), j = Math.floor(fj), u = fi - i, v = fj - j;
	return (sample(i, j) * (1 - u) + sample(i + 1, j) * u) * (1 - v) + (sample(i, j + 1) * (1 - u) + sample(i + 1, j + 1) * u) * v;
};
const CITY_TOP = groundAt(0, 0);

// ---- inputs ---------------------------------------------------------------------------------------------------------
const WALL = { r: CITY_WALL, height: 11, thick: 4, segment: 6 };
const GATE_HALF = 7;                 // m: half the gap in the wall where a main road comes in
const ROWS = [74, 100, 126, 174, 200, 226, 252, 276];   // house rows (m from the centre); the ring street at 150
const AVENUE = 9;                    // m either side of a gate's avenue kept clear of houses
const LANE = 5;                      // m either side of a quarter's edge kept clear (the lanes between quarters)
// Each path's colour: its quarter's roofs, its hall's banner (linear-ish sRGB, as the tinted look takes it).
const PATH_COLOURS = {
	lightning: [118, 108, 196], wind: [186, 210, 232], sun: [214, 166, 58], nature: [74, 136, 60], arcane: [164, 78, 184], frost: [146, 196, 226],
	blood: [128, 30, 36], tide: [40, 136, 148], necromancy: [106, 94, 118], spore: [118, 128, 58], void: [58, 40, 78], fire: [196, 70, 40],
};

// ---- the blocks ---------------------------------------------------------------------------------------------------
// A block: [mesh 0 cube 1 cylinder 2 cone, mat 0 wood 1 stone 2 cloth 3 trim 4 tinted, cx, cy, cz, sx, sy, sz, yaw,
// pitch, roll, palette index] in game cm and degrees; palette rows [r, g, b, metal, rough, glow, pattern].
const palette = [], palIndex = new Map();
const pal = (rgb, metal = 0, rough = 0.85, glow = 0, pattern = 0) => {
	const key = [...rgb, metal, rough, glow, pattern].join(",");
	if (!palIndex.has(key)) { palIndex.set(key, palette.length); palette.push([...rgb, metal, rough, glow, pattern]); }
	return palIndex.get(key);
};
const blocks = [], solids = [];
const cm = (v) => Math.round(v * 100);
// A frame: the origin (map metres x, y and z), yaw in degrees; place(...) puts a block in its local metres.
const frame = (x, y, z, yaw) => ({
	x, y, z, yaw,
	at(lx, ly, lz) { const c = Math.cos(yaw * RAD), s = Math.sin(yaw * RAD); return [x + lx * c - ly * s, y + lx * s + ly * c, z + lz]; },
});
// Anything standing on the ground (its bottom at 0) goes SINK m into it, so a dip under it never shows a gap.
const SINK = 2;
function block(F, mesh, mat, [lx, ly, lz], [sx, sy, sz], { roll = 0, pitch = 0, yawAdd = 0, colour = -1, solid = false } = {}) {
	if (roll === 0 && Math.abs(lz - sz / 2) < 0.15 && mesh !== CONE) { lz -= SINK / 2; sz += SINK; }
	const [x, y, z] = F.at(lx, ly, lz);
	blocks.push([mesh, mat, cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1), pitch, roll, colour]);
	if (solid) solids.push([cm(x), cm(y), cm(z), cm(sx), cm(sy), cm(sz), +(F.yaw + yawAdd).toFixed(1)]);
}
const CUBE = 0, CYL = 1, CONE = 2, WOOD = 0, STONE = 1, CLOTH = 2, TINT = 4;
const slate = pal([62, 66, 74], 0, 0.7), iron = pal([40, 40, 44], 0.8, 0.45, 0, 2), darkWood = pal([58, 40, 26], 0, 0.9, 0, 1);
const glass = pal([30, 34, 40], 0, 0.2, 0, 3), ember = pal([255, 120, 30], 0, 0.6, 6, 3);
let seed = 12345;
const rand = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
const gateBearings = trunks.map((t) => t.bearing);
const nearGate = (deg, r, clear) => gateBearings.some((g) => Math.abs(dAng(deg, g)) * RAD * r < clear);
const polar = (deg, r) => pt(deg, r);

// ---- the curtain wall: stone in 6 m lengths, merlons on its outer edge, gaps for the four gates ----------------------
{
	const n = Math.round(2 * Math.PI * WALL.r / WALL.segment), step = 360 / n;
	for (let k = 0; k < n; k++) {
		const deg = k * step;
		if (nearGate(deg, WALL.r, GATE_HALF + 6)) continue;
		const [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg);
		const len = 2 * Math.PI * WALL.r / n + 0.15;
		block(F, CUBE, STONE, [0, 0, WALL.height / 2 - 0.5], [len, WALL.thick, WALL.height + 1], { solid: true });
		block(F, CUBE, STONE, [-len / 4, -WALL.thick / 2 + 0.4, WALL.height + 0.7], [len / 2 - 0.6, 0.8, 1.4]);   // merlon (outer edge)
		block(F, CUBE, STONE, [0, WALL.thick / 2 - 0.25, WALL.height + 0.45], [len, 0.5, 0.9]);                 // the walk's inner parapet
	}
}
// ---- twelve round towers at the quarters' edges (none where a gatehouse stands) --------------------------------------
for (const q of quarters) {
	const deg = q.from;
	if (nearGate(deg, WALL.r, 26)) continue;
	const [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg);
	block(F, CYL, STONE, [0, 0, 8.5], [14, 14, 17], { solid: true });
	block(F, CYL, STONE, [0, 0, 17.4], [15, 15, 0.8]);
	block(F, CONE, TINT, [0, 0, 21.8], [15.6, 15.6, 8], { colour: slate });
}
// ---- the gatehouses: two square towers, the arch over the road, the portcullis raised, the path banners -----------
for (const t of trunks) {
	const deg = t.bearing, [x, y] = polar(deg, WALL.r), F = frame(x, y, CITY_TOP, deg);
	for (const side of [-1, 1]) {
		block(F, CUBE, STONE, [side * (GATE_HALF + 5), 0, 9], [10, 11, 18], { solid: true });
		block(F, CUBE, STONE, [side * (GATE_HALF + 5), 0, 18.6], [11, 12, 1.2]);
		for (const m of [-3.5, 0, 3.5]) block(F, CUBE, STONE, [side * (GATE_HALF + 5) + m, -5.6, 19.9], [1.6, 0.9, 1.4]);
	}
	block(F, CUBE, STONE, [0, 0, 13.5], [2 * GATE_HALF + 0.6, 11, 5], { solid: true });   // the arch over the road
	block(F, CUBE, TINT, [0, -3.5, 10.4], [2 * GATE_HALF - 0.4, 0.4, 1.2], { colour: iron });   // the portcullis, raised
	for (const side of [-1, 1]) block(F, CUBE, CLOTH, [side * (GATE_HALF + 5), -5.8, 12], [4, 0.2, 7], { colour: pal([150, 40, 40]) });
}
// ---- the plaza's hearth: a stone basin, the fire in it, four braziers round it ---------------------------------------
{
	const F = frame(0, 0, CITY_TOP, 0);
	block(F, CYL, STONE, [0, 0, 0.6], [12, 12, 1.2], { solid: true });
	block(F, CYL, STONE, [0, 0, 1.25], [10, 10, 0.3]);
	block(F, CYL, TINT, [0, 0, 2.6], [4, 4, 3], { colour: ember });
	block(F, CONE, TINT, [0, 0, 5.4], [3.2, 3.2, 3], { colour: ember });
	for (let k = 0; k < 4; k++) {
		const a = 45 + 90 * k, [bx, by] = polar(a, 22);
		const G = frame(bx, by, CITY_TOP, a);
		block(G, CYL, STONE, [0, 0, 1], [1.4, 1.4, 2], { solid: true });
		block(G, CYL, TINT, [0, 0, 2.3], [1.2, 1.2, 0.6], { colour: ember });
	}
}
// ---- the twelve quarters: a hall each, then houses along the rows, roofed in the path's colour ------------------------
let houses = 0;
for (const q of quarters) {
	const a0 = q.from, a1 = q.to < q.from ? q.to + 360 : q.to, mid = (a0 + a1) / 2, roof = pal(PATH_COLOURS[q.path], 0, 0.8);
	// The hall, on the quarter's middle between the 100 and 126 m rows (unless an avenue runs there: then beside it).
	let hallDeg = mid;
	if (nearGate(hallDeg, 113, AVENUE + 14)) hallDeg += 8;
	{
		const [x, y] = polar(hallDeg, 113), F = frame(x, y, CITY_TOP, hallDeg);
		block(F, CUBE, STONE, [0, 0, 6], [22, 14, 12], { solid: true });
		block(F, CUBE, TINT, [0, 0, 12], [22.8, 10.6, 10.6], { roll: 45, colour: roof });
		block(F, CUBE, STONE, [-13, 0, 10], [8, 8, 20], { solid: true });
		block(F, CONE, TINT, [-13, 0, 24], [9.4, 9.4, 8], { colour: roof });
		block(F, CUBE, WOOD, [0, 7.05, 2.4], [3.2, 0.3, 4.8], { colour: darkWood });
		block(F, CYL, WOOD, [6, 9.5, 5], [0.35, 0.35, 10]);
		block(F, CUBE, CLOTH, [7.1, 9.5, 7.6], [2, 0.12, 4], { colour: roof });
	}
	for (const r of ROWS) {
		const arc = (a1 - a0) * RAD * r;
		let s = LANE;
		while (true) {
			const w = 9 + 5 * rand(), gap = 1.5 + 3 * rand();
			if (s + w > arc - LANE) break;
			const deg = a0 + (s + w / 2) / r / RAD;
			s += w + gap;
			if (nearGate(deg, r, AVENUE + w / 2)) continue;
			if ((r === 100 || r === 126) && Math.abs(dAng(deg, hallDeg)) * RAD * r < 20 + w / 2) continue;
			const d = 7 + 3 * rand(), two = rand() < 0.55, h1 = two ? 4 : 5 + rand(), h2 = two ? 3.4 : 0, top = h1 + h2;
			const [x, y] = polar(deg, r), F = frame(x, y, CITY_TOP, deg);
			block(F, CUBE, STONE, [0, 0, h1 / 2], [w, d, h1], { solid: true });
			if (two) block(F, CUBE, WOOD, [0, 0, h1 + h2 / 2], [w - 0.3, d - 0.3, h2], { solid: true });
			block(F, CUBE, TINT, [0, 0, top], [w + 0.6, d * 0.72, d * 0.72], { roll: 45, colour: roof });
			block(F, CUBE, WOOD, [-w / 4, d / 2 + 0.05, 1.2], [1.4, 0.2, 2.4], { colour: darkWood });
			for (const wx of [w / 8, w * 3 / 8]) block(F, CUBE, TINT, [wx, d / 2 + 0.05, h1 * 0.6], [1, 0.12, 1.2], { colour: glass });
			if (two) for (const wx of [-w / 4, w / 4]) block(F, CUBE, TINT, [wx, d / 2 - 0.1, h1 + h2 * 0.55], [1, 0.12, 1.1], { colour: glass });
			if (rand() < 0.5) block(F, CUBE, STONE, [w / 3, -d / 6, top + d * 0.3], [1.1, 1.1, d * 0.6 + 1.2]);
			houses++;
		}
	}
}
// ---- a fort on each main road at the meadow's edge (the road runs through it), a waymark at each crossroads ----------
for (const t of trunks) {
	const [fx, fy] = t.fort, z = groundAt(fx, fy), F = frame(fx, fy, z, t.bearing - 90);   // local X: out along the road
	const S = 36, T = 3, Hh = 8, gate = 5;
	for (const side of [-1, 1]) {
		block(F, CUBE, STONE, [0, side * S / 2, Hh / 2], [S, T, Hh], { solid: true });                       // the long walls
		for (const end of [-1, 1]) block(F, CUBE, STONE, [end * S / 2, side * (gate + (S / 2 - gate) / 2), Hh / 2], [T, S / 2 - gate, Hh], { solid: true });
		for (const end of [-1, 1]) {
			block(F, CUBE, STONE, [end * S / 2, side * S / 2, 5.5], [8, 8, 11], { solid: true });            // corner towers
			for (const m of [-2.5, 2.5]) block(F, CUBE, STONE, [end * S / 2 + m, side * S / 2 + side * 3.6, 11.7], [1.4, 0.8, 1.4]);
		}
	}
	for (const end of [-1, 1]) block(F, CUBE, STONE, [end * S / 2, 0, Hh + 1.5], [T + 0.4, 2 * gate + 0.4, 3], { solid: true });   // over each gate
	block(F, CYL, WOOD, [0, S / 2 - 4, 6], [0.35, 0.35, 12]);
	block(F, CUBE, CLOTH, [0.9, S / 2 - 4, 9.5], [1.8, 0.12, 4], { colour: pal([150, 40, 40]) });
	const [cx, cy] = t.crossroads, G = frame(cx, cy, groundAt(cx, cy), t.bearing);
	block(G, CUBE, STONE, [0, 0, 2.2], [1.2, 1.2, 4.4], { solid: true });
	block(G, CUBE, STONE, [0, 0, 4.7], [1.6, 1.6, 0.6]);
}

fs.writeFileSync(path.join(root, "Data/hearthmoot.json"), JSON.stringify({
	_comment: "Generated by Tools/terrain/hearthmoot.mjs (edit the script, not this file). Hearthmoot, first pass: built on every machine by WRRoundIsland.cpp from the building look's blocks (no replication). blocks: [mesh 0 cube / 1 cylinder / 2 cone, mat 0 wood / 1 stone / 2 cloth / 3 trim / 4 tinted, cx, cy, cz, sx, sy, sz (game cm), yaw, pitch, roll (degrees), palette index or -1]; palette rows [r, g, b (sRGB), metal, rough, glow, pattern]; solids: [cx, cy, cz, sx, sy, sz, yaw] boxes you bump into.",
	cityTop: CITY_TOP, palette, blocks, solids,
}));
console.log(`Hearthmoot: ${blocks.length} blocks, ${solids.length} solids, ${houses} houses, ${palette.length} colours; the city's top at ${CITY_TOP.toFixed(1)} m`);
