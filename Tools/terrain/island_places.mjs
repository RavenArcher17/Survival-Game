// The Fire and Frost places on the Old Island (cooks/cookpass-world.md 3.2-3.6): sub-zones, landmarks, story sites, the
// two waymark roads, the hazards' spots and the kit POIs. Hand places are written here in metres (L_Island frame: +x
// east, +y south) and settled onto dry, gentle ground off the heightmap; kit POIs are dealt by the coverage rule (every
// walkable spot within 125 m of a place, kit places at least 150 m apart). Writes Data/island_places.json, read at
// runtime by UWRIslandPlaces (WRIslandPlaces.cpp), which builds every place on every machine.
//   node Tools/terrain/island_places.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SIZE = 4033, UPM = 32, HALFM = (SIZE - 1) / 2;
const raw = fs.readFileSync(path.join(root, "Art/Terrain/island_height.r16"));
const h = (x, y) => {
	const i = Math.max(0, Math.min(SIZE - 1, Math.round(x + HALFM))), j = Math.max(0, Math.min(SIZE - 1, Math.round(y + HALFM)));
	return (raw.readUInt16LE((j * SIZE + i) * 2) - 32768) / UPM;
};
const slope = (x, y) => Math.max(Math.abs(h(x + 3, y) - h(x - 3, y)), Math.abs(h(x, y + 3) - h(x, y - 3))) / 6;
const dressing = JSON.parse(fs.readFileSync(path.join(root, "Data/island_dressing.json"), "utf8"));
const world = JSON.parse(fs.readFileSync(path.join(root, "Data/world.json"), "utf8"));
const walk = JSON.parse(fs.readFileSync(path.join(root, "Data/island_walk.json"), "utf8"));
const cellKey = (x, y) => `${Math.round(x / 6)},${Math.round(y / 6)}`;   // 6 m cells, as the dressing's hazard cells
const lava = new Set(), ice = new Set();
for (let i = 0; i < dressing.hazards.lava.length; i += 3) lava.add(cellKey(dressing.hazards.lava[i] / 100, dressing.hazards.lava[i + 1] / 100));
for (let i = 0; i < dressing.hazards.ice.length; i += 3) ice.add(cellKey(dressing.hazards.ice[i] / 100, dressing.hazards.ice[i + 1] / 100));
const nearSet = (S, x, y, r) => { for (let dx = -r; dx <= r; dx += 6) for (let dy = -r; dy <= r; dy += 6) if (S.has(cellKey(x + dx, y + dy))) return true; return false; };
const isLava = (x, y, r = 0) => nearSet(lava, x, y, r);
const isIce = (x, y, r = 0) => nearSet(ice, x, y, r);
const round = v => Math.round(v * 10) / 10;

// Everything the places keep clear of: arenas, shrines, villages, camps, caves (metres, radius).
const keepOut = [];
for (const p of dressing.pois) {
	const x = p.x / 100, y = p.y / 100;
	if (Math.abs(x) > 2000 || Math.abs(y) > 2016) continue;
	const r = p.kind === "boss" ? 45 : p.kind === "mini-boss" ? 32 : p.kind === "camp" ? 40 : p.kind === "cave" ? 30 : 25;
	keepOut.push({ x, y, r, what: p.name });
}
const biomeCentre = id => world.biomes[id].centre;
const camp = name => { const p = dressing.pois.find(q => q.name === name); return [p.x / 100, p.y / 100]; };
const villages = {};
for (const v of world.villages) {
	const [cx, cy] = camp(v.camp), [bx, by] = biomeCentre(v.biome);
	const d = Math.hypot(bx - cx, by - cy);
	villages[v.id] = [cx + (bx - cx) / d * v.inland, cy + (by - cy) / d * v.inland];
	keepOut.push({ x: villages[v.id][0], y: villages[v.id][1], r: v.radius / 100 + 20, what: v.name });
}
for (const s of world.shrines.list) if (s.at && Math.abs(s.at[0]) < 2000 && Math.abs(s.at[1]) < 2016) keepOut.push({ x: s.at[0], y: s.at[1], r: 16, what: s.id });
const clearOf = (x, y, extra = 0, skip = "") => keepOut.every(k => k.what === skip || Math.hypot(k.x - x, k.y - y) > k.r + extra);

/** The flattest dry spot within R of (x, y) (lava, sea and keep-outs excluded); the nearest wins a tie. */
function settle(x, y, R = 30, opts = {}) {
	const { maxSlope = 0.35, allowIce = true, extra = 0, skip = "", quiet = false } = opts;
	let best = null, bestScore = 1e9;
	for (let r = 0; r <= R; r += 3) for (let a = 0; a < 360; a += r === 0 ? 360 : Math.max(6, 360 / (r / 2))) {
		const px = x + r * Math.cos(a * Math.PI / 180), py = y + r * Math.sin(a * Math.PI / 180);
		if (h(px, py) < 3 || isLava(px, py, 6) || (!allowIce && isIce(px, py)) || !clearOf(px, py, extra, skip)) continue;
		const s = slope(px, py);
		if (s > maxSlope) continue;
		const score = s + r / (R + 1) * 0.15;
		if (score < bestScore) { bestScore = score; best = [round(px), round(py)]; }
	}
	if (!best && quiet) return null;
	if (!best) console.warn(`settle: nothing within ${R} m of ${x}, ${y}; kept as written`);
	return best || [x, y];
}

// ---------------------------------------------------------------------------------------------------- sub-zones
// Tested in order: rings first (they lie over the rectangles). Metres.
const zones = [
	{ id: "V5", name: "The Crater Highlands", biome: "volcanic", ring: [-786, 0, 0, 520], bed: "ff.V5", colour: "FF8A3A" },
	{ id: "T5", name: "The Glacier Plateau", biome: "tundra", ring: [804, 0, 0, 340], bed: "ff.T5", colour: "BFE6FF" },
	{ id: "V1", name: "The Ash Plains", biome: "volcanic", rect: [-2100, -2100, -1250, 2100], bed: "ff.V1", colour: "D8D2C4" },
	{ id: "V2", name: "The Lava Fields", biome: "volcanic", rect: [-1250, 450, -150, 2100], bed: "ff.V2", colour: "FF6A1A" },
	{ id: "V3", name: "The Obsidian Ridges", biome: "volcanic", rect: [-1250, -2100, -800, -350], bed: "ff.V3", colour: "9A8FB0" },
	{ id: "V4", name: "The Solfatara", biome: "volcanic", rect: [-800, -2100, -150, -350], bed: "ff.V4", colour: "D9C230" },
	{ id: "V5", name: "The Crater Highlands", biome: "volcanic", rect: [-1250, -350, -150, 450], bed: "ff.V5", colour: "FF8A3A" },
	{ id: "T1", name: "The Snow Flats", biome: "tundra", rect: [1250, -2100, 2100, 2100], bed: "ff.T1", colour: "EEF2F5" },
	{ id: "T2", name: "The Pine Hollows", biome: "tundra", rect: [150, 450, 1250, 2100], bed: "ff.T2", colour: "9DB2C8" },
	{ id: "T3", name: "The Frozen Lakes", biome: "tundra", rect: [150, -2100, 1250, -550], bed: "ff.T3", colour: "9FD4E0" },
	{ id: "T4", name: "The Ice Shelves", biome: "tundra", rect: [150, -550, 1250, 450], bed: "ff.T4", colour: "5FA8D3" },
];
const zoneAt = (x, y) => {
	for (const z of zones) {
		if (z.ring) { const d = Math.hypot(x - z.ring[0], y - z.ring[1]); if (d >= z.ring[2] && d <= z.ring[3]) return z.id; }
		else if (x >= z.rect[0] && y >= z.rect[1] && x <= z.rect[2] && y <= z.rect[3]) return z.id;
	}
	return "";
};

// ---------------------------------------------------------------------------------------------------- hand places
// kind (the builder in WRIslandPlacesLook.cpp), id, name, at, face (a point it faces, or yaw), settle radius, landmark.
const places = [];
const add = (p, settleR = 30, opts = {}) => {
	const at = settleR > 0 ? settle(p.at[0], p.at[1], settleR, opts) : p.at;
	const out = { ...p, at };
	if (p.faceAt) out.yaw = round(Math.atan2(p.faceAt[1] - at[1], p.faceAt[0] - at[0]) * 180 / Math.PI);
	delete out.faceAt;
	out.zone = zoneAt(at[0], at[1]);
	places.push(out);
	keepOut.push({ x: at[0], y: at[1], r: p.clear ?? 30, what: p.id });
	return out;
};

// Volcanic landmarks.
add({ kind: "crater_plume", id: "crater_plume", line: "Vulkarn's forge breathes smoke over the whole island.", name: "Vulkarn's Crater", at: [-786, 0], landmark: true, clear: 0 }, 0);
add({ kind: "bellows_stacks", id: "bellows", line: "Three spatter chimneys the smiths worked as bellows. They still breathe fire in turn.", name: "The Bellows Stacks", at: [-620, -760], faceAt: [-1133, -341], landmark: true, clear: 40 }, 40);
// The Fallen Smith lies south of the lava river; his hammer reaches north across it (the span is measured below).
const smith = add({ kind: "fallen_smith", id: "fallen_smith", line: "A forge giant split by frost, Rime-folk spearheads still in his skull. His hammer is the only bridge over the river.", name: "The Fallen Smith", at: [-880, 990], landmark: true, clear: 45 }, 14);
add({ kind: "glasswall", id: "glasswall", line: "A cliff of black glass. Something is cut into the ledge nine metres up.", name: "Glasswall", at: [-1180, -985], faceAt: [-1180, -900], landmark: true, clear: 40 }, 25);
add({ kind: "blue_fire", id: "blue_fire", line: "Sulfur vents the miners worked. At night their fire burns blue.", name: "The Blue Fire", at: [-430, -1160], landmark: true, clear: 30 }, 40);
add({ kind: "slag_light", id: "slag_light", line: "The smiths' beacon on the west headland. It burns again once the Kipuka's shrine is kindled.", name: "The Slag Light", at: [-1560, 120], faceAt: [-2000, 120], landmark: true, clear: 20 }, 50, { maxSlope: 0.45 });
add({ kind: "kipuka", id: "kipuka", line: "Old forest the lava flowed around: the only living green in the Volcanic.", name: "Ash Pine Kipuka", at: [-1285, 610], clearing: world.shrines.list.find(q => q.id === "kipuka").at, landmark: true, clear: 70 }, 20, { maxSlope: 0.5, skip: "kipuka" });
add({ kind: "wisp_spire", id: "wisp_spire", line: "A twisted spire behind Ignessa's ring, crowned with her wisps.", name: "Wisp Spire", at: [-1036, 742], faceAt: [-1036, 784], landmark: true, clear: 12 }, 8, { maxSlope: 0.6, skip: "Ignessa" });
// Tundra landmarks.
const rim = villages.rimehold;
const gateDir = (() => { const dx = 804 - rim[0], dy = 0 - rim[1], d = Math.hypot(dx, dy); return [dx / d, dy / d]; })();
add({ kind: "tusk_gate", id: "tusk_gate", line: "The Rime-folk's gate of mammoth tusks, facing the glacier.", name: "The Tusk Gate", at: [rim[0] + gateDir[0] * 72, rim[1] + gateDir[1] * 72], faceAt: [804, 0], landmark: true, clear: 14 }, 12, { maxSlope: 0.4 });
add({ kind: "great_inuksuk", id: "great_inuksuk", line: "Nine metres of stacked stone. Its arm points at the glacier.", name: "The Great Inuksuk", at: [1150, 120], faceAt: [804, 0], landmark: true, clear: 20 }, 30);
add({ kind: "frozen_whaler", id: "frozen_whaler", line: "A Rime-folk longboat locked in the ice from when the shelf was sea.", name: "The Frozen Whaler", at: [520, -420], faceAt: [700, -560], landmark: true, clear: 30 }, 40, { maxSlope: 0.45 });
add({ kind: "weeping_seracs", id: "weeping_seracs", line: "A wall of blue ice towers. Listen for the crack before one falls.", name: "The Weeping Seracs", at: [640, 250], faceAt: [804, 0], landmark: true, clear: 45 }, 30, { maxSlope: 0.45 });
add({ kind: "fox_pingo", id: "fox_pingo", line: "An ice-cored mound where the aurora foxes gather.", name: "The Fox Circle Pingo", at: [1450, 900], landmark: true, clear: 25 }, 60);
add({ kind: "pale_pines", id: "pale_pines", line: "Dead white pines round the Pale Hunter's ground. No light reaches here.", name: "The Pale Pines", at: [1034, 834], landmark: true, clear: 0 }, 0);
add({ kind: "ice_arch", id: "ice_arch", line: "A natural ice bridge over the river ice. Something lives beneath it.", name: "The Ice Arch", at: [1064, -766], faceAt: [1400, -766], landmark: true, clear: 0 }, 0);

// Volcanic story sites.
add({ kind: "plough_trench", id: "plough_trench", name: "The Plough Trench", at: [-1563, 505], from: [-1572, 400], to: [-1556, 612], clear: 20 }, 0);
add({ kind: "miners_grave", id: "miners_grave", name: "The Miners' Grave", at: [-470, -1110], faceAt: [-900, -800], clear: 22 }, 40);
add({ kind: "miners_camp", id: "miners_camp", name: "The Miners' Camp", at: [-380, -1240], faceAt: [-430, -1160], clear: 18 }, 40);
add({ kind: "charred_caravan", id: "charred_caravan", name: "The Charred Caravan", at: [-1450, -700], faceAt: [0, -700], clear: 22 }, 40);
add({ kind: "tree_molds", id: "tree_molds", name: "The Tree-Mold Field", at: [-1100, 500], clear: 30 }, 40);
add({ kind: "crater_stair", id: "crater_stair", name: "The Crater Stair", at: [-1050, -120], faceAt: [-786, 0], clear: 20 }, 25, { maxSlope: 0.6 });
add({ kind: "wardens_quarry", id: "wardens_quarry", name: "The Warden's Quarry", at: [-1066 - 38, -716 - 10], faceAt: [-1066, -716], clear: 14 }, 10, { maxSlope: 0.45, skip: "Obsidian Warden" });
add({ kind: "shard_field", id: "shard_field", name: "The Shard Field", at: [-1350, -1100], clear: 45 }, 40);
add({ kind: "ridge_watch", id: "ridge_watch", name: "The Ridge Watch", at: [-420, -1330], faceAt: [-13, -1296], clear: 14 }, 40, { maxSlope: 0.3 });
add({ kind: "kilnmite_hill", id: "kilnmite_hill", name: "The Kilnmite Hill", at: [-1300, -1260], clear: 20 }, 40);
add({ kind: "acid_pool", id: "acid_pool", name: "The Acid Pool", at: [-470, -980], clear: 22 }, 60, { maxSlope: 0.2 });
add({ kind: "steam_ground", id: "steam_ground_v", name: "The Steam Ground", at: [-520, -620], clear: 22 }, 40);
// Tundra story sites.
add({ kind: "tusker_wallow", id: "tusker_wallow", name: "Old Tusker's Wallow", at: [1050, 300], clear: 20 }, 20);
add({ kind: "tusk_midden", id: "tusk_midden", name: "The Tusk Midden", at: [1450, -900], faceAt: [1600, -900], clear: 18 }, 50, { maxSlope: 0.5 });
add({ kind: "sledge_line", id: "sledge_line", name: "The Sledge Wreck Line", at: [1450, 650], faceAt: [1700, 700], clear: 25 }, 50);
add({ kind: "fishing_camp", id: "fishing_camp", name: "The Ice-Fishing Camp", at: [760, -1000], clear: 18 }, 60, { maxSlope: 0.15 });
add({ kind: "drowned_sledge", id: "drowned_sledge", name: "The Drowned Sledge", at: [1150, -850], clear: 15 }, 60, { maxSlope: 0.15 });
add({ kind: "hunter_camp", id: "hunter_camp", name: "The Pale Hunter's Camp", at: [990, 905], faceAt: [1034, 834], clear: 14 }, 15, { maxSlope: 0.5 });
add({ kind: "hunters_blind", id: "hunters_blind", name: "The Hunter's Blind", at: [700, 1300], faceAt: [600, 1300], clear: 12 }, 40);
add({ kind: "giant_pine", id: "giant_pine", name: "The Fallen Giant Pine", at: [600, 700], faceAt: [640, 740], clear: 25 }, 40);
add({ kind: "glacimaw_boneyard", id: "glacimaw_boneyard", name: "The Glacimaw Boneyard", at: [420, 500], clear: 18 }, 90);
add({ kind: "claw_scars", id: "claw_scars", name: "The Wyrm Claw Scars", at: [650, -300], faceAt: [804, 0], clear: 12 }, 30, { maxSlope: 0.6 });
add({ kind: "watch_cairn", id: "watch_cairn", name: "The Watch Cairn", at: [380, -720], faceAt: [-13, -1296], clear: 12 }, 110, { maxSlope: 0.35 });
add({ kind: "moulin", id: "moulin", name: "The Moulin of Voices", at: [560, -330], clear: 12 }, 30, { maxSlope: 0.3 });
add({ kind: "crevasse_maze", id: "crevasse_maze", name: "The Crevasse Maze", at: [440, -70], faceAt: [450, 60], clear: 60 }, 30, { maxSlope: 0.3, skip: "crevasse" });
add({ kind: "steam_ground", id: "steam_ground_t", name: "The Steam Ground", at: [180, 1250], clear: 22 }, 60, { maxSlope: 0.45 });

// The Fallen Smith's hammer: his arm reaches from the skeleton to the lava's south bank, the haft spans the river and
// the head sinks into the north bank (bridge.from = the haft's south end, bridge.to = its north end, metres).
{
	const [sx, sy] = smith.at;
	let near = null, far = null;
	for (let y = sy - 10; y > sy - 220; y -= 2) {
		const l = isLava(sx, y);
		if (l && near === null) near = y + 2;
		if (near !== null && !l && !isLava(sx, y - 6) && !isLava(sx, y - 12)) { far = y; break; }
	}
	if (near === null || far === null) console.warn("fallen smith: no river crossing found north of the skeleton");
	else smith.bridge = { from: [sx, round(near + 5)], to: [sx, round(far - 6)] };
}

// ---------------------------------------------------------------------------------------------------- roads
const road = (pts, step) => {
	const out = [];
	for (let i = 1; i < pts.length; i++) {
		const [ax, ay] = pts[i - 1], [bx, by] = pts[i], L = Math.hypot(bx - ax, by - ay), n = Math.max(1, Math.round(L / step));
		for (let k = i === 1 ? 0 : 1; k <= n; k++) out.push([ax + (bx - ax) * k / n, ay + (by - ay) * k / n]);
	}
	return out;
};
const waymarks = (kind, pts, step) => {
	// A mark that would stand in a camp, a village or on a shrine is left out: the road meets them at their edge.
	const raw = road(pts, step).map(([x, y]) => settle(x, y, 22, { maxSlope: 0.45, quiet: true })).filter(Boolean);
	return raw.map((p, i) => {
		const n = raw[Math.min(raw.length - 1, i + 1)], b = raw[Math.max(0, i - 1)];
		const to = i < raw.length - 1 ? n : [p[0] + (p[0] - b[0]), p[1] + (p[1] - b[1])];
		return { kind, at: p, yaw: round(Math.atan2(to[1] - p[1], to[0] - p[0]) * 180 / Math.PI), zone: zoneAt(p[0], p[1]) };
	});
};
const ember = villages.emberwick;
const roads = [
	{ id: "slag_road", name: "The Slag-Cairn Road", marks: waymarks("slag_cairn", [camp("Volcanic camp 2"), [-900, 1180], [-1200, 690], [-1100, 430], [-1080, -100], ember, [-1450, -640], camp("Volcanic camp 1")], 175) },
	{ id: "inuksuk_road", name: "The Inuksuk Road", marks: waymarks("inuksuk", [camp("Tundra camp 3"), [1180, 900], rim, [1150, 160], [1180, -300], [1150, -700], camp("Tundra camp 2")], 200) },
];
for (const r of roads) for (const m of r.marks) keepOut.push({ x: m.at[0], y: m.at[1], r: 10, what: r.id });

// Sastrugi: fields of wind-carved snow ridges on the open snow, every ridge lying along the prevailing katabatic wind off
// the glacier, so their long axis points at the bowl (a compass you read off the ground, Ghost of Tsushima's wind).
const sastrugi = [];
{
	let seed = 99;
	const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	for (let tries = 0; tries < 4000 && sastrugi.length < 16; tries++) {
		const x = 1150 + rnd() * 500, y = -1500 + rnd() * 3000;
		const z = zoneAt(x, y);
		if (z !== "T1" && z !== "T3") continue;
		if (h(x, y) < 6 || slope(x, y) > 0.2 || isIce(x, y) || !clearOf(x, y, 10)) continue;
		if (sastrugi.some(q => Math.hypot(q.at[0] - x, q.at[1] - y) < 160)) continue;
		const yaw = round(Math.atan2(0 - y, 804 - x) * 180 / Math.PI);
		sastrugi.push({ kind: "sastrugi", at: [round(x), round(y)], yaw, zone: z });
	}
}

// ---------------------------------------------------------------------------------------------------- hazards
// Thin ice: dark patches on the lake ice, 60-120 m apart, kept off the camps' and the shrine's ground.
const thinIce = [];
{
	const want = 8;
	let tries = 0, seed = 7;
	const rnd = () => { seed = (seed * 16807) % 2147483647; return seed / 2147483647; };
	while (thinIce.length < want && tries++ < 5000) {
		const x = 450 + rnd() * 1000, y = -1100 + rnd() * 600;
		if (!isIce(x, y) || !isIce(x, y, 18) || slope(x, y) > 0.12 || !clearOf(x, y, 10)) continue;
		if (thinIce.some(t => Math.hypot(t.at[0] - x, t.at[1] - y) < 70)) continue;
		thinIce.push({ at: [round(x), round(y)], r: round(6 + rnd() * 4) });
	}
}
// The drowned sledge's window points at the nearest thin patch (the story: read the colour).
const fumaroleFields = [ { at: [-560, -1020], n: 4 }, { at: [-560, -880], n: 3 }, { at: [-680, -480], n: 3 } ];
const fumaroles = [];
for (const f of fumaroleFields) {
	const c = settle(f.at[0], f.at[1], 60);
	for (let i = 0; i < f.n; i++) {
		const a = i * 137.5 * Math.PI / 180, r = 9 + i * 7;
		fumaroles.push({ at: settle(c[0] + r * Math.cos(a), c[1] + r * Math.sin(a), 8, { maxSlope: 0.45 }) });
	}
}
const geysers = [];
for (const g of [[-600, -1180], [-540, -1300], [-470, -880], [-720, -1300]]) geysers.push({ at: settle(g[0], g[1], 50, { maxSlope: 0.25 }) });
const katabatic = { centre: [804, 0], inner: 180, outer: 340 };

// ---------------------------------------------------------------------------------------------------- kit POIs
const kitRules = {
	V1: { smith_grave: 3, glowmoth_roost: 2, basalt_ring: 2, miner_leanto: 1, giant_tool: 1 },
	V2: { hornito: 3, basalt_ring: 2, glowmoth_roost: 2, giant_tool: 2, smith_grave: 1, sulfur_vent: 1 },
	V3: { smith_grave: 2, basalt_ring: 1, giant_tool: 1, glowmoth_roost: 1 },
	V4: { sulfur_vent: 3, hornito: 2, miner_leanto: 2, smith_grave: 1 },
	V5: { giant_tool: 2, basalt_ring: 2, smith_grave: 1, hornito: 1 },
	T1: { tent_ring: 3, erratic: 2, snowpuff_nest: 2, cache_pole: 2, small_pingo: 1, frozen_carcass: 1 },
	T2: { tent_ring: 2, cache_pole: 2, frozen_carcass: 2, erratic: 1 },
	T3: { small_pingo: 2, frozen_carcass: 2, cache_pole: 1, erratic: 1, tent_ring: 1 },
	T4: { erratic: 3, frozen_carcass: 2, small_pingo: 1 },
	T5: { erratic: 2, frozen_carcass: 1, cache_pole: 1 },
};
const taken = keepOut.map(k => [k.x, k.y]);
for (const q of sastrugi) taken.push(q.at);
const kit = [];
{
	const W = walk, cell = W.cell / 100, ox = W.origin / 100;
	const cands = [];
	for (const [jStr, row] of Object.entries(W.rows)) {
		const j = +jStr;
		for (let i = 0; i < row.length; i++) {
			const c = row[i];
			if (c !== "g" && c !== "f" && c !== "w") continue;
			const x = ox + (i + 0.5) * cell, y = ox + (j + 0.5) * cell;
			if (Math.abs(y) > 1900 || Math.abs(x) > 1900 || Math.abs(x) < 200) continue;
			cands.push([x, y]);
		}
	}
	// A fixed shuffle so the deal is the same each run.
	let seed = 1234567;
	const rnd = () => { seed = (seed * 48271) % 2147483647; return seed / 2147483647; };
	for (let i = cands.length - 1; i > 0; i--) { const k = Math.floor(rnd() * (i + 1)); [cands[i], cands[k]] = [cands[k], cands[i]]; }
	const deck = {}, dealt = {};
	for (const [z, w] of Object.entries(kitRules)) { deck[z] = []; for (const [k, n] of Object.entries(w)) for (let q = 0; q < n; q++) deck[z].push(k); dealt[z] = 0; }
	const nearest = (x, y) => { let d = 1e9; for (const [tx, ty] of taken) d = Math.min(d, Math.hypot(tx - x, ty - y)); return d; };
	for (const [x, y] of cands) {
		if (nearest(x, y) <= 125) continue;
		const spot = settle(x, y, 30, { maxSlope: 0.4, allowIce: false, quiet: true });
		if (!spot) continue;
		if (h(spot[0], spot[1]) < 3 || isIce(spot[0], spot[1]) || isLava(spot[0], spot[1], 10) || slope(spot[0], spot[1]) > 0.4) continue;
		if (nearest(spot[0], spot[1]) < 150 && kit.some(k => Math.hypot(k.at[0] - spot[0], k.at[1] - spot[1]) < 150)) continue;
		const z = zoneAt(spot[0], spot[1]);
		if (!deck[z]) continue;
		// The next kind in the zone's deck that isn't the same as a kit place within 400 m.
		let kind = null;
		for (let t = 0; t < deck[z].length; t++) {
			const k = deck[z][(dealt[z] + t) % deck[z].length];
			if (!kit.some(q => q.kind === k && Math.hypot(q.at[0] - spot[0], q.at[1] - spot[1]) < 400)) { kind = k; dealt[z] += t + 1; break; }
		}
		if (!kind) { kind = deck[z][dealt[z] % deck[z].length]; dealt[z]++; }
		kit.push({ kind, at: spot, yaw: Math.round(rnd() * 360), v: Math.floor(rnd() * 4), zone: z });
		taken.push(spot);
	}
}

// ---------------------------------------------------------------------------------------------------- write
const out = {
	_comment: "Generated by Tools/terrain/island_places.mjs (cooks/cookpass-world.md): the Fire and Frost places on the Old Island, in metres (+x east, +y south). zones are tested in order (rings first); places/roads/kit are built at runtime by UWRIslandPlaces on every machine (WRIslandPlaces*.cpp) on the ground traced under each; hazards are the server's (WRIslandHazards.cpp, numbers in tuning hazards.ff). Re-run the tool after the island or world.json's villages move.",
	zones, places, roads, kit, ground: sastrugi,
	hazards: { thinIce, fumaroles, geysers, katabatic },
};
fs.writeFileSync(path.join(root, "Data/island_places.json"), JSON.stringify(out, null, 1).replace(/\n\s+(-?[\d.]+),?\n\s+(-?[\d.]+)\n\s+\]/g, " $1, $2 ]"));
const byZone = {};
for (const p of [...places, ...kit, ...roads.flatMap(r => r.marks)]) byZone[p.zone] = (byZone[p.zone] || 0) + 1;
console.log(`places ${places.length}, waymarks ${roads.reduce((n, r) => n + r.marks.length, 0)}, kit ${kit.length}, thin ice ${thinIce.length}, fumaroles ${fumaroles.length}, geysers ${geysers.length}`);
console.log("by zone", byZone);
console.log("fallen smith bridge", JSON.stringify(smith.bridge));
for (const p of places) console.log(p.id.padEnd(18), p.at.join(", ").padEnd(16), p.zone, "z", h(p.at[0], p.at[1]).toFixed(0), "slope", slope(p.at[0], p.at[1]).toFixed(2));
