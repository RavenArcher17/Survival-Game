// Places the island's wildlife: spawn sites for every wild species in its own biome and elevation band, written to
// Data/island_spawns.json (read at server start by AWRCreatureSpawner::SpawnSitesFromData; packs wake when a player
// comes near). Same island model as dressing.mjs: the heightmap, the layout's camps and boss sites, the caves and
// hazard cells already in Data/island_dressing.json.
//   node Tools/terrain/spawns.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SIZE = 4033, UNITS_PER_METRE = 32, HALF = (SIZE - 1) * 100 / 2, RIDGE_X = 2016;
const raw = fs.readFileSync(path.join(root, "Art/Terrain/island_height.r16"));
const layout = JSON.parse(fs.readFileSync(path.join(root, "Docs/terrain/layout_proposal.json"), "utf8"));
const dressing = JSON.parse(fs.readFileSync(path.join(root, "Data/island_dressing.json"), "utf8"));
const creatures = JSON.parse(fs.readFileSync(path.join(root, "Data/creatures.json"), "utf8")).creatures;

const h = (x, y) => {
	const i = Math.max(0, Math.min(SIZE - 1, Math.round(x))), j = Math.max(0, Math.min(SIZE - 1, Math.round(y)));
	return (raw.readUInt16LE((j * SIZE + i) * 2) - 32768) / UNITS_PER_METRE;
};
const slope = (x, y) => Math.max(Math.abs(h(x + 2, y) - h(x - 2, y)), Math.abs(h(x, y + 2) - h(x, y - 2))) / 4;
function rng(seed) { return () => { seed |= 0; seed = (seed + 0x6d2b79f5) | 0; let t = Math.imul(seed ^ (seed >>> 15), 1 | seed); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const rand = rng(layout.seed * 104729 + 7);
const toMetres = (cm) => (cm + HALF) / 100;

// Keep-outs: camps (predators much further), boss arenas, mini-boss sites, caves.
const camps = layout.spawns.map(s => ({ x: s.x, y: s.y }));
const keepOut = [
	...layout.bosses.map(b => ({ x: b.x, y: b.y, r: 220 })),
	...layout.miniBosses.map(b => ({ x: b.x, y: b.y, r: 120 })),
	...dressing.caves.map(c => ({ x: toMetres(c.x), y: toMetres(c.y), r: 70 })),
];
// Hazard cells (lava river, slick ice) as a lookup grid; sites stay 12 m clear of them.
const CELL = dressing.hazards.cell / 100;
const hazard = new Set();
for (const kind of ["lava", "ice"]) {
	const a = dressing.hazards[kind];
	for (let i = 0; i < a.length; i += 3) hazard.add(`${Math.round(toMetres(a[i]) / CELL)},${Math.round(toMetres(a[i + 1]) / CELL)}`);
}
const nearHazard = (x, y) => {
	for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++) if (hazard.has(`${Math.round(x / CELL) + dx},${Math.round(y / CELL) + dy}`)) return true;
	return false;
};
const inTrench = (x, y) => (h(x + 40, y) + h(x - 40, y) + h(x, y + 40) + h(x, y - 40)) / 4 - h(x, y) > 3.2;

// Elevation bands by distance from the ridge (the coast is ~1,700 m out): lowlands by the camps, highlands by the ridge.
const BANDS = { lowland: [900, 1900], midland: [450, 1050], highland: [140, 620] };
// Sites per species and pack size. Critters and herds by the camps, hunters spread wider, flyers and giants fewer.
// Doubled for 0.5.0 (Raven: "still feels very sparse on the creatures"): about 160 sites, so walking anywhere you come
// within a pack's wake-up range (wildlife.activationRadius, 250 m) about once a minute. Predators grew a little less
// than grazers and critters, so the island fills with life rather than with fights.
const PLAN = {
	pumicehog: [16, 2], glowmoth: [12, 2], ashstrider: [12, 3], cinderfang: [9, 3], kilnmite: [6, 2],
	emberhorn: [10, 3], basaltback: [8, 2], scorchwing: [7, 1], magmaw: [5, 1],
	snowpuff: [16, 2], aurora_fox: [12, 1], gloomhowl: [9, 3], frostantler: [10, 3], woolly_drudge: [8, 2],
	rimeshell: [6, 2], glacimaw: [5, 1], rimeclaw: [5, 1],
};
const RADIUS = { critter: 300, herd: 650, pack_hunter: 500, ground_predator: 350, flyer: 450, burrower: 300 };
const RESPAWN = { passive: 120, neutral: 180, aggressive: 240 };
const CAMP_GAP = { passive: 90, neutral: 160, aggressive: 280 };  // metres from a player camp

// The packs placed in the map itself (Tools/terrain/import_island.py, always out): new sites keep clear of them too.
const mapPacks = [["cinderfang", 950, 1100], ["cinderfang", 1000, 2900], ["magmaw", 1450, 1700], ["gloomhowl", 3080, 1100], ["gloomhowl", 3050, 2950], ["rimeclaw", 2600, 2450]]
	.map(([creature, mx, my]) => ({ creature, mx, my }));
const sites = [];
for (const [id, [want, count]] of Object.entries(PLAN)) {
	const c = creatures.find(k => k.id === id);
	if (!c) throw new Error(`no creature ${id}`);
	const [near, far] = BANDS[c.tier] || BANDS.midland;
	const west = c.biome === "volcanic";
	let placed = 0, tries = 0;
	while (placed < want && tries++ < 40000) {
		const d = near + rand() * (far - near);
		const x = west ? RIDGE_X - d : RIDGE_X + d, y = 250 + rand() * (SIZE - 500);
		const z = h(x, y);
		if (z < 3 || slope(x, y) > (c.archetype === "flyer" ? 0.7 : 0.45) || inTrench(x, y) || nearHazard(x, y)) continue;
		if (keepOut.some(k => Math.hypot(k.x - x, k.y - y) < k.r)) continue;
		if (camps.some(k => Math.hypot(k.x - x, k.y - y) < CAMP_GAP[c.behavior])) continue;
		// Spread out: 95 m from any site, 210 m from one of the same kind.
		if (sites.concat(mapPacks).some(s => Math.hypot(s.mx - x, s.my - y) < (s.creature === id ? 210 : 95))) continue;
		sites.push({ creature: id, mx: x, my: y, x: Math.round(x * 100 - HALF), y: Math.round(y * 100 - HALF), z: Math.round(z * 100),
			count, radius: RADIUS[c.archetype] || 400, respawn: RESPAWN[c.behavior] || 180 });
		placed++;
	}
	if (placed < want) console.warn(`${id}: placed ${placed} of ${want}`);
}

const out = {
	_comment: "Generated by Tools/terrain/spawns.mjs from the heightmap and layout. One wildlife spawner per site (world cm); a pack spawns when a player comes within wildlife.activationRadius and fades out after wildlife.despawnSeconds alone (tuning.json).",
	map: "L_Island",
	sites: sites.map(({ mx, my, ...s }) => s),
};
fs.writeFileSync(path.join(root, "Data/island_spawns.json"), JSON.stringify(out, null, 1).replace(/\n {2,}/g, " ").replace(/\[\n /g, "[").replace(/\n \]/g, "]"));
const total = sites.reduce((n, s) => n + s.count, 0);
const by = {};
for (const s of sites) by[s.creature] = (by[s.creature] || 0) + s.count;
console.log(`${sites.length} sites, ${total} creatures:`, Object.entries(by).map(([k, v]) => `${k} ${v}`).join(", "));
