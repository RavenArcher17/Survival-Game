// The NPC strongholds on the Old Island (Aaron 11-16, r-siege-pve: "Allow it to practice siegeing on NPC strongholds"; and
// q-stronghold-garrison: "make the NPC strongholds designed like elden ring areas, with small, medium, and boss enemies").
// Designs: showcase-notes cooks/wild-answers-1115.md 2.7 and cooks/build-wild-strongholds.md (2.2 the layout, 2.4 the
// garrison). Each fort is laid out here in its own frame and placed, once, off the island heightmap: the flattest open
// ground near its design point that is dry, off the lava and the ice, clear of every camp, arena, shrine, village, place
// and kit spot, with a gentle approach in front of its gate for siege engines (Electric Dreams' footprint test: the spot
// must hold the whole thing, never placed and then sunk). Every piece's, ladder's and soldier's height is written too, so
// every machine knows where the fort stands without a trace. Writes Data/strongholds.json (read by WRStronghold.cpp and
// WRGarrison.cpp).
//   node Tools/terrain/strongholds.mjs
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const SIZE = 4033, UPM = 32, HALFM = (SIZE - 1) / 2;
const raw = fs.readFileSync(path.join(root, "Art/Terrain/island_height.r16"));
const sample = (i, j) => (raw.readUInt16LE((Math.max(0, Math.min(SIZE - 1, j)) * SIZE + Math.max(0, Math.min(SIZE - 1, i))) * 2) - 32768) / UPM;
// The landscape's own surface between its 1 m vertices (metres, L_Island frame: +x east, +y south).
const h = (x, y) => {
	const fx = x + HALFM, fy = y + HALFM, i = Math.floor(fx), j = Math.floor(fy), u = fx - i, v = fy - j;
	const a = sample(i, j), b = sample(i + 1, j), c = sample(i, j + 1), d = sample(i + 1, j + 1);
	return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
};
const dressing = JSON.parse(fs.readFileSync(path.join(root, "Data/island_dressing.json"), "utf8"));
const places = JSON.parse(fs.readFileSync(path.join(root, "Data/island_places.json"), "utf8"));
const world = JSON.parse(fs.readFileSync(path.join(root, "Data/world.json"), "utf8"));
const cellKey = (x, y) => `${Math.round(x / 6)},${Math.round(y / 6)}`;
const lava = new Set(), ice = new Set();
for (let i = 0; i < dressing.hazards.lava.length; i += 3) lava.add(cellKey(dressing.hazards.lava[i] / 100, dressing.hazards.lava[i + 1] / 100));
for (let i = 0; i < dressing.hazards.ice.length; i += 3) ice.add(cellKey(dressing.hazards.ice[i] / 100, dressing.hazards.ice[i + 1] / 100));
const nearSet = (S, x, y, r) => { for (let dx = -r; dx <= r; dx += 6) for (let dy = -r; dy <= r; dy += 6) if (S.has(cellKey(x + dx, y + dy))) return true; return false; };

// Keep-outs (metres): camps (their safe zones), arenas, caves, villages, shrines, every place and kit spot.
const keepOut = [];
for (const p of dressing.pois) {
	const r = p.kind === "boss" ? 60 : p.kind === "mini-boss" ? 45 : p.kind === "camp" ? 90 : p.kind === "cave" ? 40 : 35;
	keepOut.push({ x: p.x / 100, y: p.y / 100, r, what: p.name });
}
for (const v of world.villages || []) {
	const c = dressing.pois.find(q => q.name === v.camp), b = world.biomes[v.biome];
	if (!c || !b || !b.centre) continue;
	const cx = c.x / 100, cy = c.y / 100, d = Math.hypot(b.centre[0] - cx, b.centre[1] - cy) || 1;
	keepOut.push({ x: cx + (b.centre[0] - cx) / d * v.inland, y: cy + (b.centre[1] - cy) / d * v.inland, r: v.radius / 100 + 60, what: v.id });
}
for (const s of (world.shrines && world.shrines.list) || []) if (s.at) keepOut.push({ x: s.at[0], y: s.at[1], r: 40, what: s.id });
for (const p of places.places) keepOut.push({ x: p.at[0], y: p.at[1], r: (p.clear || 20) + 50, what: p.id });
for (const k of places.kit) keepOut.push({ x: k.at[0], y: k.at[1], r: 40, what: k.kind });
for (const r of places.roads || []) for (const m of r.marks) keepOut.push({ x: m.at[0], y: m.at[1], r: 25, what: r.id });

// ================================================================ the fort's layout
// The fort's own frame, cm: +X out of the gate toward the approach, +Y to the left looking out. Plan (build-wild-strongholds 2.2):
//   X -3320 .. -1320  the inner ward (the warlord's arena), the land's building set, 5 x 5 cells
//   X -1320 .. -520   the keep, 2 x 3 cells, two storeys and a roof (the beacon)
//   X -1320 .. +1320  the bailey: palisade 6 x 5 segments, Y -1100 .. +1100; gate in the front's middle
//   X +4200           the outpost tower on the approach
// Heights: "g" pieces stand on the ground under them (embedded); "k" pieces on the keep/ward floor (floorTop, the same
// for both: the highest ground under them + 15 cm), plus their storey.
const SEG = 440, CELL = 400, STOREY = 300;
const BX = 1320, BY = 1100;          // the bailey's half extents
const KX0 = -1320, KX1 = -520;       // the keep
const WX0 = -3320, WX1 = -1320, WY = 1000;  // the ward
const OUTPOST_X = 4200;
const WALK_Z = 230;                  // the wall walk's floor over the ground (the palisade's points 1.25 m above it)
const DECK = 520;                    // a guard tower's deck top

function layout(kit, look) {
	const P = [];
	const g = (piece, role, x, y, yaw, extra = {}) => P.push({ piece, role, x, y, yaw, base: "g", lift: 0, embed: true, ...extra });
	const k = (piece, role, x, y, yaw, lift, extra = {}) => P.push({ piece, role, x, y, yaw, base: "k", lift, embed: false, ...extra });
	// --- the bailey wall. Front (the gate in the middle, between the gate towers).
	for (const y of [-880, -440, 0, 440, 880]) {
		if (y === 0) g("palisade_gate", "gate", BX, 0, 90, { inward: [-1, 0] });
		else g("palisade_wall", "palisade", BX, y, 90);
	}
	// Sides (Y = +-1100), front to back. The breach: the two +Y segments by the rear corner are down (the side path).
	for (const x of [1100, 660, 220, -220, -660, -1100]) {
		g("palisade_wall", "palisade", x, -BY, 0);
		if (x > -600) g("palisade_wall", "palisade", x, BY, 0);
	}
	// Towers: the gate's two (a bolt turret on each deck) and the four corners (archers' posts, ladders up).
	g("guard_tower", "tower", BX, -220, 0, { turret: true });
	g("guard_tower", "tower", BX, 220, 0, { turret: true });
	for (const [x, y] of [[BX, -BY], [BX, BY], [-BX, -BY], [-BX, BY]]) g("guard_tower", "tower", x, y, 0);
	// The wall walk either side of the gate: plank floors on posts, WALK_Z up.
	for (const y of [-900, -540, 540, 900]) {
		g("wood_floor", "walk", BX - 220, y, 0, { embed: false, lift: WALK_Z - 20 });
		for (const dy of [-170, 170]) g("wood_pillar", "walk_post", BX - 400, y + dy, 0, { embed: false, lift: WALK_Z - 20 - 300 });
	}
	// --- the keep: six foundations, the hall (storey 1), the upper floor (storey 2), the roof (parapet, beacon, banner).
	const KC = [[-1120, -400], [-1120, 0], [-1120, 400], [-720, -400], [-720, 0], [-720, 400]];
	for (const [x, y] of KC) k(`${kit}_foundation`, "keep_floor", x, y, 0, -100);
	// Storey 1 edges. Back (X -1320): the boss door in the middle. Front (X -520): the barred door (the siege door).
	k(`${kit}_wall`, "keep_wall", KX0, -400, 90, 0); k(`${kit}_doorway`, "keep_wall", KX0, 0, 90, 0); k(`${kit}_wall`, "keep_wall", KX0, 400, 90, 0);
	k(`${kit}_wall`, "keep_wall", KX1, -400, 90, 0); k(`${kit}_door`, "keep_door", KX1, 0, 90, 0); k(`${kit}_wall`, "keep_wall", KX1, 400, 90, 0);
	for (const x of [-1120, -720]) { k(`${kit}_wall`, "keep_wall", x, -600, 0, 0); k(`${kit}_wall`, "keep_wall", x, 600, 0, 0); }
	// Stairs inside: hall -> upper floor (cell -1120,+400), upper floor -> roof (cell -1120,-400), both rising toward +X.
	k(`${kit}_stairs`, "keep_stairs", -1120, 400, 0, 0);
	k(`${kit}_stairs`, "keep_stairs", -1120, -400, 0, STOREY);
	// Storey 2 floor (no floor over the hall stairs), its walls: windows on the front (the yard), a doorway on the +Y side
	// where the outside stair lands.
	for (const [x, y] of KC) if (!(x === -1120 && y === 400)) k(`${kit}_floor`, "keep_upper", x, y, 0, STOREY);
	for (const y of [-400, 0, 400]) k(`${kit}_wall`, "keep_upper", KX0, y, 90, STOREY);
	k(`${kit}_window`, "keep_upper", KX1, -400, 90, STOREY); k(`${kit}_window`, "keep_upper", KX1, 0, 90, STOREY); k(`${kit}_wall`, "keep_upper", KX1, 400, 90, STOREY);
	k(`${kit}_wall`, "keep_upper", -1120, -600, 0, STOREY); k(`${kit}_wall`, "keep_upper", -720, -600, 0, STOREY);
	k(`${kit}_wall`, "keep_upper", -1120, 600, 0, STOREY); k(`${kit}_doorway`, "keep_upper", -720, 600, 0, STOREY);
	// The roof: floor (no floor over the roof stairs), a half-wall parapet all round, the beacon and the banner.
	for (const [x, y] of KC) if (!(x === -1120 && y === -400)) k(`${kit}_floor`, "keep_roof", x, y, 0, 2 * STOREY);
	for (const y of [-400, 0, 400]) { k(`${kit}_halfwall`, "keep_roof", KX0, y, 90, 2 * STOREY); k(`${kit}_halfwall`, "keep_roof", KX1, y, 90, 2 * STOREY); }
	for (const x of [-1120, -720]) { k(`${kit}_halfwall`, "keep_roof", x, -600, 0, 2 * STOREY); k(`${kit}_halfwall`, "keep_roof", x, 600, 0, 2 * STOREY); }
	k("brazier", "beacon", -720, 0, 0, 2 * STOREY + 20);
	k("banner", "banner", -620, 380, 180, 2 * STOREY + 20);
	// The war chest on the upper floor against the back wall; the keep warden's post in front of it.
	k("storage_chest", "war_chest", -1240, 0, 0, STOREY + 20);
	// The way up on the +Y side to the upper doorway: a landing on posts, a fixed ladder up to it from the yard (a ladder
	// takes any height the plinth stands above the yard; the posts reach down to the ground, see place()).
	k(`${kit}_floor`, "keep_landing", -720, 800, 0, STOREY);
	for (const x of [-900, -540]) k(`${kit}_pillar`, "keep_landing_post", x, 980, 0, 0, { post: true });
	// --- the inner ward: a floor of the set's foundations, its walls (the postern in the back by the +Y corner), lamps,
	// the warlord's seat, the banner over the boss door.
	for (let x = WX0 + 200; x < WX1; x += CELL) for (let y = -WY + 200; y < WY; y += CELL) k(`${kit}_foundation`, "ward_floor", x, y, 0, -100);
	for (let y = -WY + 200; y < WY; y += CELL) {
		if (y === 800) k(`${kit}_door`, "postern", WX0, y, 90, 0, { inward: [1, 0] });
		else k(`${kit}_wall`, "ward_wall", WX0, y, 90, 0);
	}
	for (let x = WX0 + 200; x < WX1; x += CELL) { k(`${kit}_wall`, "ward_wall", x, -WY, 0, 0); k(`${kit}_wall`, "ward_wall", x, WY, 0, 0); }
	for (const y of [-800, 800]) k(`${kit}_wall`, "ward_wall", WX1 - 40, y, 90, 0);
	for (const [x, y] of [[WX0 + 160, -WY + 160], [WX0 + 160, WY - 160], [WX1 - 200, -WY + 160], [WX1 - 200, WY - 160]]) k(look.lamp, "ward_decor", x, y, 0, 0);
	k(look.seat, "ward_decor", WX0 + 140, 0, 0, 0);
	if (look.seatBack) k(look.seatBack, "ward_decor", WX0 + 60, 0, 0, 40);
	k("banner", "banner", WX1 - 120, -260, 0, 0);
	k("banner", "banner", WX1 - 120, 260, 0, 0);
	// --- the yard. Barracks (two wood cells against the -Y wall, doorway on the yard, a flat plank roof, beds in it).
	for (const x of [-320, 80]) g("wood_foundation", "hall", x, -850, 0, { lift: -100 + 15, embed: false, flat: true });
	for (const x of [-320, 80]) g("wood_wall", "hall", x, -1050, 0, { lift: 15, embed: false, flat: true });
	g("wood_wall", "hall", -320, -650, 0, { lift: 15, embed: false, flat: true });
	g("wood_doorway", "hall", 80, -650, 0, { lift: 15, embed: false, flat: true });
	for (const x of [-520, 280]) g("wood_wall", "hall", x, -850, 90, { lift: 15, embed: false, flat: true });
	for (const x of [-320, 80]) g("wood_floor", "hall", x, -850, 0, { lift: 15 + STOREY, embed: false, flat: true });
	for (const x of [-380, -40]) g("fur_bed", "hall", x, -960, 0, { lift: 15, embed: false, flat: true });
	// The campfire with its benches (the footmen's ease), torches by the keep door and on the wall walk.
	g("campfire", "decor", 420, -250, 0, { embed: false });
	g("bench", "decor", 420, -440, 0, { embed: false });
	g("bench", "decor", 420, -60, 180, { embed: false });
	g("torch", "decor", -430, -260, 0, { embed: false });
	g("torch", "decor", -430, 260, 0, { embed: false });
	for (const y of [-900, 900]) g("torch", "decor", BX - 380, y, 0, { embed: false, lift: WALK_Z });
	// The land's work in the yard's +Y front quarter.
	for (const w of look.work) g(w.piece, "decor", w.x, w.y, w.yaw || 0, { embed: false });
	// The kennel by the breach.
	g("lean_to", "decor", -1150, 860, 90, { embed: false });
	// Banners outside the gate: whose fort it is, seen from the road.
	g("banner", "banner", BX + 140, -470, 0, { embed: false });
	g("banner", "banner", BX + 140, 470, 0, { embed: false });
	// --- the outpost on the approach: its tower and a brazier.
	g("guard_tower", "outpost", OUTPOST_X, 0, 0);
	g("brazier", "decor", OUTPOST_X - 120, 330, 0, { embed: false });
	return P;
}

// Fixed ladders (AWRSiegeLadder owned by the fort): foot, facing (yaw: toward the climber), length, lean; against = the
// role + position of the piece it leans on.
function ladders() {
	const L = [];
	const towerLadder = (tx, ty, fx, fy, yaw, lift = 0) => L.push({ x: fx, y: fy, yaw, length: Math.round(Math.hypot(DECK - lift, 70)), footOut: 70, againstAt: [tx, ty], lift });
	// The front corners' ladders stand on the wall walk (walk -> deck: the archers' climb); the rear ones on the yard.
	towerLadder(BX, -BY, BX - 270, -BY + 120, 180, WALK_Z);
	towerLadder(BX, BY, BX - 270, BY - 120, 180, WALK_Z);
	towerLadder(-BX, -BY, -BX + 270, -BY + 120, 0);
	towerLadder(-BX, BY, -BX + 270, BY - 120, 0);
	towerLadder(OUTPOST_X, 0, OUTPOST_X - 270, 0, 180);
	// Up to the wall walk from the yard, at its two inner ends.
	for (const y of [-900, 900]) L.push({ x: BX - 420 - 70, y, yaw: 180, length: Math.round(Math.hypot(WALK_Z, 70)), footOut: 70, againstAt: [BX - 220, y], lift: 0 });
	// Up to the keep's landing (the on-foot way into the keep), from the yard.
	L.push({ x: KX1 + 70, y: 800, yaw: 0, length: 400, footOut: 70, againstAt: [-720, 800], lift: 0 });
	return L;
}

// ================================================================ the forts' people (cooks/build-wild-strongholds.md 2.4)
const OUT_R = 2100;   // the patrol's ring outside the walls
const patrolRoute = [[BX + 600, -BY - 600], [BX + 600, BY + 600], [-1500, BY + 600], [WX0 - 600, WY + 600], [WX0 - 600, -WY - 600], [-1500, -BY - 600]];
const forts = [
	{ id: "emberwatch", near: [-1430, 330], search: 220, data: {
		name: "Emberwatch", biome: "volcanic", zone: "V1", faction: "Reaver",
		lore: "The Forge-wardens' watch over the Ash Plains, raised when the smiths still kept the Slag Light. The Cinder Reavers hold it now, forge-hands who stayed when the wardens left and took to raiding the coast camps. Their Reeve keeps the inner ward.",
		chest: { upgrade_shard: 4, iron_ingot: 10, obsidian: 6, sulfur_crystal: 6, crossbow_bolt: 20, flint: 10, stone_arrow: 30 },
		hounds: [{ creature: "cinderfang", count: 3, at: [-1120, 760] }],
		kit: "basalt_hall",
		look: { lamp: "basalt_hall_lamp", seat: "basalt_hall_table", seatBack: "basalt_hall_fireplace",
			work: [{ piece: "forge", x: 900, y: 700, yaw: 270 }, { piece: "basalt_hall_table", x: 650, y: 700, yaw: 0 }, { piece: "brazier", x: 1000, y: 920 }, { piece: "long_table", x: 400, y: 900, yaw: 0 }] },
		garrison: [
			{ id: "out_archer", rank: "archer", at: [OUTPOST_X, 0], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 2, build: 1 },
			{ id: "out_horn", rank: "horn", at: [OUTPOST_X - 200, 220], yaw: 0, weapon: "iron_sword", shield: true, armor: ["iron_head", "hide_armor", "hide_hands", "iron_legs"], sex: "m", face: 1, build: 2 },
			{ id: "patrol_a", rank: "footman", at: patrolRoute[0], route: 0, yaw: 90, weapon: "iron_spear", armor: ["iron_head", "iron_armor", "hide_hands", "hide_legs"], sex: "m", face: 3, build: 0 },
			{ id: "patrol_b", rank: "footman", at: patrolRoute[3], route: 3, yaw: 270, weapon: "iron_axe", shield: true, armor: ["bronze_head", "bronze_armor", "hide_hands", "bronze_legs"], sex: "f", face: 0, build: 2 },
			{ id: "corner_l", rank: "archer", at: [BX, BY], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 4, build: 1 },
			{ id: "corner_r", rank: "archer", at: [BX, -BY], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 5, build: 0 },
			{ id: "corner_breach", rank: "archer", at: [-BX, BY], deck: true, yaw: 90, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 6, build: 2 },
			{ id: "walk_l", rank: "archer", at: [BX - 220, 900], walk: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["iron_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 7, build: 1 },
			{ id: "walk_r", rank: "archer", at: [BX - 220, -900], walk: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["iron_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 8, build: 0 },
			{ id: "fire_1", rank: "footman", at: [420, -560], yaw: 90, weapon: "iron_sword", shield: true, armor: ["iron_head", "iron_armor", "iron_hands", "hide_legs"], sex: "m", face: 9, build: 1 },
			{ id: "fire_2", rank: "footman", at: [610, -230], yaw: 180, weapon: "iron_axe", armor: [null, "bronze_armor", "hide_hands", "bronze_legs"], sex: "f", face: 10, build: 1 },
			{ id: "fire_3", rank: "footman", at: [230, -230], yaw: 0, weapon: "iron_spear", armor: ["bronze_head", "hide_armor", "bronze_hands", "hide_legs"], sex: "m", face: 11, build: 2 },
			{ id: "yard_horn", rank: "horn", at: [80, -500], yaw: 0, weapon: "iron_sword", shield: true, armor: ["iron_head", "hide_armor", "hide_hands", "iron_legs"], sex: "f", face: 12, build: 0 },
			{ id: "captain", rank: "captain", name: "Varro, Reaver captain", at: [900, 0], yaw: 0, patrolYard: true, weapon: "iron_sword", shield: true, metal: "iron", armor: ["iron_head", "iron_armor", "iron_hands", "iron_legs"], sex: "m", face: 13, build: 2 },
			{ id: "warden", rank: "warden", name: "Ysolde of the Anvil", at: [-960, 0], upper: true, yaw: 0, weapon: "steel_greatsword", metal: "steel", armor: ["forgeplate_head", "forgeplate_armor", "forgeplate_hands", "forgeplate_legs"], sex: "f", face: 14, build: 2 },
			{ id: "warlord", rank: "warlord", warlord: "hask", at: [-2420, 0], ward: true, yaw: 0, weapon: "steel_great_hammer", metal: "steel", armor: ["forgeplate_ascendant_head", "forgeplate_ascendant_armor", "forgeplate_ascendant_hands", "forgeplate_ascendant_legs"], sex: "m", face: 15, build: 2 },
		] } },
	{ id: "rimehold", near: [1300, -870], search: 320, data: {
		name: "Rimehold", biome: "tundra", zone: "T1", faction: "Wrecker",
		lore: "The whalers' winter fort on the Snow Flats. When the sledges stopped coming the whalers stayed and turned wrecker; the Rime Wreckers hold it still, and their Wreck-Queen keeps the inner ward.",
		chest: { upgrade_shard: 4, iron_ingot: 10, lichen: 8, hide: 10, crossbow_bolt: 20, flint: 10, stone_arrow: 30 },
		hounds: [{ creature: "gloomhowl", count: 3, at: [-1120, 760] }],
		kit: "rime_lodge",
		look: { lamp: "standing_lantern", seat: "rime_lodge_bench", seatBack: "rime_lodge_tapestry",
			work: [{ piece: "cooking_pot", x: 820, y: 640 }, { piece: "cooking_pot", x: 1000, y: 640 }, { piece: "campfire", x: 910, y: 860 }, { piece: "long_table", x: 560, y: 760, yaw: 90 }, { piece: "rime_lodge_rug", x: 560, y: 760, yaw: 90 }] },
		garrison: [
			{ id: "out_archer", rank: "archer", at: [OUTPOST_X, 0], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 2, build: 2 },
			{ id: "out_horn", rank: "horn", at: [OUTPOST_X - 200, 220], yaw: 0, weapon: "iron_axe", armor: ["hide_head", "bronze_armor", "hide_hands", "hide_legs"], sex: "f", face: 3, build: 1 },
			{ id: "patrol_a", rank: "footman", at: patrolRoute[0], route: 0, yaw: 90, weapon: "iron_spear", armor: ["hide_head", "iron_armor", "hide_hands", "hide_legs"], sex: "f", face: 4, build: 0 },
			{ id: "patrol_b", rank: "footman", at: patrolRoute[3], route: 3, yaw: 270, weapon: "iron_spear", armor: ["bronze_head", "hide_armor", "bronze_hands", "hide_legs"], sex: "m", face: 5, build: 2 },
			{ id: "corner_l", rank: "archer", at: [BX, BY], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 6, build: 1 },
			{ id: "corner_r", rank: "archer", at: [BX, -BY], deck: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 7, build: 0 },
			{ id: "corner_breach", rank: "archer", at: [-BX, BY], deck: true, yaw: 90, bow: "hunting_bow", weapon: "iron_dagger", armor: ["hide_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 8, build: 2 },
			{ id: "walk_l", rank: "archer", at: [BX - 220, 900], walk: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["bronze_head", "hide_armor", "hide_hands", "hide_legs"], sex: "m", face: 9, build: 1 },
			{ id: "walk_r", rank: "archer", at: [BX - 220, -900], walk: true, yaw: 0, bow: "hunting_bow", weapon: "iron_dagger", armor: ["bronze_head", "hide_armor", "hide_hands", "hide_legs"], sex: "f", face: 10, build: 0 },
			{ id: "fire_1", rank: "footman", at: [420, -560], yaw: 90, weapon: "iron_axe", shield: true, armor: ["iron_head", "iron_armor", "hide_hands", "hide_legs"], sex: "m", face: 11, build: 2 },
			{ id: "fire_2", rank: "footman", at: [610, -230], yaw: 180, weapon: "iron_spear", armor: [null, "hide_armor", "hide_hands", "bronze_legs"], sex: "f", face: 12, build: 1 },
			{ id: "fire_3", rank: "footman", at: [230, -230], yaw: 0, weapon: "iron_sword", shield: true, armor: ["bronze_head", "bronze_armor", "hide_hands", "hide_legs"], sex: "m", face: 13, build: 0 },
			{ id: "yard_horn", rank: "horn", at: [80, -500], yaw: 0, weapon: "iron_spear", armor: ["hide_head", "hide_armor", "hide_hands", "iron_legs"], sex: "m", face: 14, build: 1 },
			{ id: "captain", rank: "captain", name: "Brannoc, Wrecker captain", at: [900, 0], yaw: 0, patrolYard: true, weapon: "iron_axe", shield: true, metal: "iron", armor: ["iron_head", "iron_armor", "iron_hands", "iron_legs"], sex: "m", face: 15, build: 2 },
			{ id: "warden", rank: "warden", name: "Halla the Flenser", at: [-960, 0], upper: true, yaw: 0, weapon: "steel_great_hammer", metal: "steel", armor: ["rimeguard_head", "rimeguard_armor", "rimeguard_hands", "rimeguard_legs"], sex: "f", face: 0, build: 2 },
			{ id: "warlord", rank: "warlord", warlord: "skarde", at: [-2420, 0], ward: true, yaw: 0, weapon: "steel_spear", metal: "steel", armor: ["rimeguard_ascendant_head", "rimeguard_ascendant_armor", "rimeguard_ascendant_hands", "rimeguard_ascendant_legs"], sex: "f", face: 1, build: 1 },
		] } },
];

// The yard captain's round: gate, the work, the keep stair, the fire.
const yardRoute = [[900, 0], [700, 600], [-300, 700], [-250, -300], [600, -350]];

/** Score a centre: the height spread under the bailey, under the keep + ward (they share one floor), and the approach's. */
function fit(cx, cy, yawDeg) {
	const a = yawDeg * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
	const at = (fx, fy) => [cx + (fx * c - fy * s) / 100, cy + (fx * s + fy * c) / 100];
	let lo = 1e9, hi = -1e9, klo = 1e9, khi = -1e9;
	for (let fx = -BX - 100; fx <= BX + 100; fx += 200) for (let fy = -BY - 100; fy <= BY + 100; fy += 200) {
		const [x, y] = at(fx, fy), z = h(x, y); lo = Math.min(lo, z); hi = Math.max(hi, z);
	}
	for (let fx = WX0 - 100; fx <= KX1 + 100; fx += 200) for (let fy = -WY - 100; fy <= WY + 100; fy += 200) {
		const [x, y] = at(fx, fy), z = h(x, y); klo = Math.min(klo, z); khi = Math.max(khi, z);
	}
	// Round the back of the ward (the run-back stone, the patrol) and the outpost: walkable.
	let steep = 0;
	for (const [fx, fy] of [[WX0 - 1000, 800], [WX0 - 600, 0], [OUTPOST_X, 0], [OUTPOST_X - 300, 300]]) {
		const [x, y] = at(fx, fy);
		steep = Math.max(steep, Math.abs(h(x + 2, y) - h(x - 2, y)) / 4, Math.abs(h(x, y + 2) - h(x, y - 2)) / 4);
	}
	// The approach: 15-60 m out of the gate in a 50 m wide lane (where engines stand and roll).
	let alo = 1e9, ahi = -1e9;
	for (let t = 15; t <= 60; t += 4) for (let sd = -25; sd <= 25; sd += 5) {
		const px = cx + c * (BX / 100 + t) - s * sd, py = cy + s * (BX / 100 + t) + c * sd;
		const z = h(px, py);
		alo = Math.min(alo, z); ahi = Math.max(ahi, z);
		steep = Math.max(steep, Math.abs(h(px + 2, py) - h(px - 2, py)) / 4, Math.abs(h(px, py + 2) - h(px, py - 2)) / 4);
	}
	return { spread: hi - lo, keep: khi - klo, keepTop: khi, approach: ahi - alo, steep };
}

function clearAll(cx, cy, yawDeg) {
	const a = yawDeg * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
	for (const [fx, fy] of [[0, 0], [WX0, 0], [-2300, 0], [OUTPOST_X, 0], [BX, BY], [BX, -BY], [WX0, WY], [WX0, -WY]]) {
		const x = cx + (fx * c - fy * s) / 100, y = cy + (fx * s + fy * c) / 100;
		if (h(x, y) < 4 || nearSet(lava, x, y, 30) || nearSet(ice, x, y, 24)) return false;
		if (!keepOut.every(k => Math.hypot(k.x - x, k.y - y) > k.r + 18)) return false;
	}
	return true;
}

function place(spec) {
	let best = null;
	for (let r = 0; r <= spec.search; r += 6) for (let a = 0; a < 360; a += r === 0 ? 360 : Math.max(4, 360 / (r / 3))) {
		const x = spec.near[0] + r * Math.cos(a * Math.PI / 180), y = spec.near[1] + r * Math.sin(a * Math.PI / 180);
		if (h(x, y) < 4) continue;
		for (let yaw = 0; yaw < 360; yaw += 22.5) {
			if (!clearAll(x, y, yaw)) continue;
			const f = fit(x, y, yaw);
			// The bailey follows the ground (every palisade post is embedded); the keep and the ward share one floor on a
			// plinth of the set's foundations (their slabs reach 2.5 m down), so they need it flatter. 20 degrees is the
			// engines' limit.
			if (f.spread > 5.0 || f.keep > 2.0 || f.steep > 0.36) continue;
			const score = f.spread * 1.5 + f.keep * 3 + f.approach * 0.3 + f.steep * 4 + r / spec.search * 0.6;
			if (!best || score < best.score) best = { x, y, yaw, score, ...f };
		}
	}
	if (!best) throw new Error(`no ground for ${spec.id}`);
	const a = best.yaw * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
	const W = (fx, fy) => { const wx = best.x + (fx * c - fy * s) / 100, wy = best.y + (fx * s + fy * c) / 100; return [wx, wy]; };
	const cm = v => Math.round(v * 1000) / 10;
	const yawOf = y => Math.round((((y + best.yaw) % 360) + 360) % 360 * 10) / 10;
	const floorTop = best.keepTop + 0.15;   // m: the keep's and the ward's floor
	const D = spec.data;
	const rows = layout(D.kit, D.look);
	// The landing's posts stand on the ground: each is a 3 m pillar under the landing; where the plinth stands higher
	// than that above the yard, a second one carries it down to the ground.
	for (const p of rows.filter(q => q.post)) {
		const [wx, wy] = W(p.x, p.y);
		const ground = h(wx, wy) * 100;
		p.base = "abs"; p.abs = floorTop * 100;     // the top post: from the floor's height to the landing
		if (ground < floorTop * 100 - 5) rows.push({ ...p, abs: floorTop * 100 - STOREY, embed: true, post: false });
	}
	// The postern: outside it a step of the set's floor at the ward's height, and a ladder down when the ground falls away
	// more than a stride (the shortcut must work both ways).
	const [pox, poy] = W(WX0 - 200, 800);
	const posternDrop = floorTop * 100 - h(pox, poy) * 100;
	if (posternDrop > 10) rows.push({ piece: `${D.kit}_floor`, role: "postern_step", x: WX0 - 200, y: 800, yaw: 0, base: "k", lift: -20, embed: false });
	const pieces = rows.map(p => {
		const [wx, wy] = W(p.x, p.y);
		let z;
		if (p.base === "abs") z = p.abs;
		else if (p.base === "k") z = floorTop * 100 + p.lift;
		else if (p.flat) z = h(...W(-120, -850)) * 100 + p.lift;   // the barracks stand level on their own ground
		else z = h(wx, wy) * 100 + p.lift;
		const out = { piece: p.piece, role: p.role, x: cm(wx), y: cm(wy), z: Math.round(z * 10) / 10, yaw: yawOf(p.yaw) };
		if (p.embed) out.embed = true;
		if (p.turret) out.turret = true;
		if (p.inward) out.inward = [Math.round((p.inward[0] * c - p.inward[1] * s) * 1000) / 1000, Math.round((p.inward[0] * s + p.inward[1] * c) * 1000) / 1000];
		return out;
	});
	// Barracks level: their ground under the middle of the hall.
	const L = ladders();
	if (posternDrop > 45) L.push({ x: WX0 - 400 - 70, y: 800, yaw: 180, length: 200, footOut: 70, againstAt: [WX0 - 200, 800], lift: 0 });
	const ladderRows = L.map(l => {
		const [wx, wy] = W(l.x, l.y);
		const [tx, ty] = W(l.againstAt[0], l.againstAt[1]);
		const against = pieces.findIndex(p => Math.hypot(p.x - tx * 100, p.y - ty * 100) < 30 && ["tower", "outpost", "walk", "keep_landing", "postern_step"].includes(p.role));
		const foot = h(wx, wy) * 100 + l.lift;
		const against_ = pieces[against];
		// The top hooks the deck's / the floor's edge: the length is from the foot to there.
		const topZ = !against_ ? foot + l.length : against_.role === "tower" || against_.role === "outpost" ? against_.z - 25 + DECK : against_.z + 20;
		const rise = Math.max(100, topZ - foot);
		return { x: cm(wx), y: cm(wy), z: Math.round(foot * 10) / 10, yaw: yawOf(l.yaw), length: Math.round(Math.hypot(rise, l.footOut)), footOut: l.footOut, against };
	});
	const soldier = g => {
		const [wx, wy] = W(g.at[0], g.at[1]);
		let z = h(wx, wy) * 100;
		if (g.deck) z = h(wx, wy) * 100 - 25 + DECK;
		if (g.walk) z = h(wx, wy) * 100 + WALK_Z;
		if (g.upper) z = floorTop * 100 + STOREY + 20;
		if (g.ward) z = floorTop * 100;
		const out = { id: g.id, rank: g.rank, x: cm(wx), y: cm(wy), z: Math.round(z * 10) / 10, yaw: yawOf(g.yaw),
			weapon: g.weapon, armor: g.armor.map(a => a || ""), sex: g.sex, face: g.face, build: g.build };
		if (g.name) out.name = g.name;
		if (g.shield) out.shield = true;
		if (g.bow) out.bow = g.bow;
		if (g.metal) out.metal = g.metal;
		if (g.warlord) out.warlord = g.warlord;
		const route = g.route !== undefined ? patrolRoute.slice(g.route).concat(patrolRoute.slice(0, g.route)) : g.patrolYard ? yardRoute : null;
		if (route) out.route = route.map(([fx, fy]) => { const [rx, ry] = W(fx, fy); return [cm(rx), cm(ry), Math.round(h(rx, ry) * 1000) / 10]; });
		return out;
	};
	const [wcx, wcy] = W((WX0 + WX1) / 2, 0);
	const [rbx, rby] = W(WX0 - 1000, 800);
	const fortOut = {
		id: spec.id, name: D.name, biome: D.biome, zone: D.zone, faction: D.faction, lore: D.lore,
		centre: [cm(best.x), cm(best.y), cm(h(best.x, best.y))], yaw: best.yaw,
		clearRadius: 4800,
		ward: { centre: [cm(wcx), cm(wcy), Math.round(floorTop * 1000) / 10], half: [(WX1 - WX0) / 2, WY] },
		runback: { at: [cm(rbx), cm(rby), cm(h(rbx, rby))], yaw: yawOf(0) },
		chest: D.chest,
		hounds: D.hounds.map(hd => { const [hx, hy] = W(hd.at[0], hd.at[1]); return { creature: hd.creature, count: hd.count, at: [cm(hx), cm(hy), cm(h(hx, hy))] }; }),
		garrison: D.garrison.map(soldier),
		ladders: ladderRows,
		pieces,
	};
	console.log(`${spec.id}: (${best.x.toFixed(0)}, ${best.y.toFixed(0)}) m, gate ${best.yaw} deg, bailey spread ${best.spread.toFixed(2)} m, keep+ward ${best.keep.toFixed(2)} m, approach ${best.approach.toFixed(2)} m, steepest ${best.steep.toFixed(2)}; ${pieces.length} pieces, ${ladderRows.length} ladders, ${fortOut.garrison.length} soldiers`);
	return fortOut;
}

// ================================================================ ranks and warlords (first drafts; build-wild-strongholds 2.4)
const ranks = {
	footman: { title: "footman", tier: "small", health: 160, stamina: 90, poise: 30, armor: 12, sight: 1800, cone: 120, hear: 1500, leash: 3000, walk: 160, run: 420,
		guard: 0.2, guardShield: 0.55, dodge: 0.15, heavy: 0.25, charged: 0.08, chain: 3, loot: { stone_arrow: 4, iron_ingot: 1, flint: 2 } },
	archer: { title: "archer", tier: "small", health: 110, stamina: 80, poise: 20, armor: 6, sight: 3500, cone: 140, hear: 1500, leash: 2000, walk: 160, run: 400,
		guard: 0.1, dodge: 0.2, heavy: 0.15, charged: 0, chain: 2, shot: 14, draw: 1.1, reload: 1.5, keepMin: 400, keepMax: 3200, loot: { stone_arrow: 8, flint: 2 } },
	horn: { title: "horn-blower", tier: "small", health: 140, stamina: 80, poise: 25, armor: 8, sight: 3000, cone: 140, hear: 1800, leash: 2500, walk: 160, run: 420,
		guard: 0.3, guardShield: 0.5, dodge: 0.1, heavy: 0.2, charged: 0, chain: 2, horn: 2.0, loot: { cooked_meat: 2, stone_arrow: 4 } },
	captain: { title: "captain", tier: "medium", health: 420, stamina: 140, poise: 60, armor: 22, sight: 2200, cone: 130, hear: 1800, leash: 3500, walk: 170, run: 440,
		guard: 0.35, guardShield: 0.6, dodge: 0.12, heavy: 0.3, charged: 0.12, chain: 3, kickAfter: 0.8, loot: { upgrade_shard: 1, iron_ingot: 4 }, rare: 0.25, rareItem: "weapon" },
	warden: { title: "keep warden", tier: "medium", health: 650, stamina: 160, poise: 80, armor: 28, sight: 1400, cone: 140, hear: 1200, leash: 1200, walk: 150, run: 400,
		guard: 0.15, dodge: 0.08, heavy: 0.4, charged: 0.2, chain: 2, loot: { upgrade_shard: 2, iron_ingot: 4 }, rare: 0.15, rareItem: "chest" },
	warlord: { title: "warlord", tier: "boss", health: 2600, stamina: 400, poise: 90, armor: 30, sight: 3000, cone: 360, hear: 3000, leash: 99999, walk: 170, run: 460,
		guard: 0.1, dodge: 0.06, heavy: 0.35, charged: 0.15, chain: 3, loot: {} },
};
const warlords = {
	hask: { name: "Hask, the Forge-Reeve", element: "fire", health: 2600, poise: 90, armor: 30, perFighter: 0.8, phase2At: 0.5, phase2Speed: 1.12, phase2Buildup: 12,
		firstKill: "steel_great_hammer", loot: { upgrade_shard: 4, iron_ingot: 6, obsidian: 4 },
		moves: [
			{ id: "anvil_leap", shape: "target", range: 1400, size: 260, windup: 1.0, hold: 0.3, active: 0.25, punish: 1.3, dmg: 55, poise: 120, impact: 4, cooldown: 7, weights: { close: 0, mid: 3, far: 3 }, clip: "heavy", tamePunish: true, sound: "stomp", fx: "stomp", maxPhase: 1 },
			{ id: "anvil_leap_late", shape: "target", range: 1400, size: 300, windup: 1.0, hold: 0.6, active: 0.25, punish: 1.3, dmg: 60, poise: 130, impact: 4, element: "fire", buildup: 20, cooldown: 7, weights: { close: 0, mid: 3, far: 3 }, clip: "heavy", tamePunish: true, sound: "stomp", fx: "stomp", hazard: 4, hazardRadius: 220, phase: 2 },
			{ id: "bellows_kick", shape: "cone", range: 220, size: 40, windup: 0.4, active: 0.15, punish: 0.8, dmg: 15, poise: 90, impact: 3, guardBreak: true, cooldown: 5, weights: { close: 3, mid: 0, far: 0 }, clip: "kick" },
			{ id: "quench_flare", shape: "self", range: 450, size: 450, windup: 1.3, active: 0.3, punish: 1.4, dmg: 50, poise: 150, impact: 5, unblockable: true, element: "fire", buildup: 40, transition: true, cooldown: 99, weights: { close: 0, mid: 0, far: 0 }, clip: "heavy", sound: "firewall", fx: "firewall" },
			{ id: "cinder_sweep", shape: "cone", range: 300, size: 70, windup: 0.7, hold: 0.5, active: 0.25, punish: 1.2, dmg: 45, poise: 100, impact: 4, element: "fire", buildup: 25, hazard: 4, hazardRadius: 200, cooldown: 6, weights: { close: 3, mid: 1, far: 0 }, clip: "heavy", phase: 2, sound: "firewall" },
			{ id: "forge_combo", shape: "cone", range: 260, size: 55, windup: 0.5, active: 0.2, punish: 0.6, dmg: 30, poise: 60, impact: 3, parryable: true, repeats: 3, gap: 0.55, holdOn: 3, hold: 0.45, cooldown: 6, weights: { close: 3, mid: 0, far: 0 }, clip: "light", phase: 2 },
		] },
	skarde: { name: "Skarde, the Wreck-Queen", element: "frost", health: 2400, poise: 80, armor: 26, perFighter: 0.8, phase2At: 0.5, phase2Speed: 1.1, phase2Buildup: 12,
		firstKill: "steel_spear", loot: { upgrade_shard: 4, iron_ingot: 6, hide: 6 },
		moves: [
			{ id: "harpoon_cast", shape: "throw", range: 2400, size: 40, windup: 1.0, active: 0.1, punish: 0.9, dmg: 40, poise: 70, impact: 3, element: "frost", buildup: 25, speed: 1000, cooldown: 6, weights: { close: 0, mid: 2, far: 3 }, clip: "throw", mesh: "/Game/Props/Spear/spear_game/StaticMeshes/spear_game.spear_game", meshAlong: "-Y", maxPhase: 1 },
			{ id: "harpoon_brace", shape: "throw", range: 2400, size: 40, windup: 1.0, active: 0.1, punish: 1.0, dmg: 40, poise: 70, impact: 3, element: "frost", buildup: 25, speed: 1000, repeats: 2, gap: 0.6, cooldown: 7, weights: { close: 0, mid: 2, far: 3 }, clip: "throw", mesh: "/Game/Props/Spear/spear_game/StaticMeshes/spear_game.spear_game", meshAlong: "-Y", phase: 2 },
			{ id: "ice_hook", shape: "cone", range: 320, size: 75, windup: 0.7, active: 0.2, punish: 1.0, dmg: 35, poise: 80, impact: 3, low: true, element: "frost", buildup: 15, cooldown: 5, weights: { close: 3, mid: 1, far: 0 }, clip: "heavy" },
			{ id: "whiteout", shape: "self", range: 450, size: 450, windup: 1.3, active: 0.3, punish: 1.4, dmg: 45, poise: 150, impact: 5, unblockable: true, element: "frost", buildup: 45, transition: true, cooldown: 99, weights: { close: 0, mid: 0, far: 0 }, clip: "cast", sound: "frostnova", fx: "frostnova" },
			{ id: "flense", shape: "cone", range: 300, size: 30, windup: 0.45, active: 0.15, punish: 1.2, dmg: 28, poise: 50, impact: 3, parryable: true, repeats: 3, gap: 0.45, holdOn: 3, hold: 0.5, cooldown: 6, weights: { close: 3, mid: 1, far: 0 }, clip: "light", phase: 2 },
			{ id: "tide_drag", shape: "line", range: 900, size: 120, windup: 1.0, active: 0.5, punish: 1.4, dmg: 50, poise: 140, impact: 4, unblockable: true, grab: true, element: "frost", buildup: 20, cooldown: 9, weights: { close: 0, mid: 3, far: 2 }, clip: "heavy", lunge: true, phase: 2 },
		] },
};

const out = {
	_comment: "Generated by Tools/terrain/strongholds.mjs (edit the script, not this file). The NPC strongholds on the Old Island (Aaron 11-16, r-siege-pve and q-stronghold-garrison; WRStronghold.cpp, WRGarrison.cpp; cooks/build-wild-strongholds.md): Elden-Ring-style forts of the game's own build pieces (the bailey of wood, the keep and the inner ward of the land's building set) that anyone may break on any world (PvE too), held by a garrison in three tiers. Positions in game cm (L_Island: +x east, +y south). Pieces: z is where the piece stands (the ground, or the keep/ward floor + its storey); embed: stands into the ground; inward: a barred door that opens from inside (gate, postern), its way in; turret: a bolt turret rides its deck. Roles: gate, palisade, tower, outpost, walk, walk_post, keep_floor, keep_wall, keep_door (the war chest opens once it or a keep wall falls), keep_stairs, keep_upper, keep_roof, keep_landing and keep_landing_post (the landing the ladder from the yard reaches), postern_step (outside the postern where the ground falls away), beacon (lit while the fort is held), banner (down once it's taken), war_chest, ward_floor, ward_wall, ward_decor, postern, hall, decor. ladders: fixed ladders (foot, yaw toward the climber, length, lean; against = the piece index they lean on). garrison: the soldiers (rank, post, route, weapon, shield, bow, armour head/chest/hands/legs, sex, face, build; warlord = its warlords row). hounds: the land's beasts kennelled at a spot. ranks: each rank's numbers. warlords: each warlord's numbers and moves (CAL 1.13 rows: shape cone/self/line/target/throw, range, size, windup, hold, active, punish, dmg, poise, impact, flags, phase/maxPhase, weights close/mid/far, clip = the hero clip it plays). chest: what the war chest holds, refilled when the fort rebuilds. All first drafts.",
	forts: forts.map(place),
	ranks,
	warlords,
};
fs.writeFileSync(path.join(root, "Data/strongholds.json"), JSON.stringify(out, null, 1) + "\n");
console.log("wrote Data/strongholds.json");
