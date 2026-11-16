// The NPC strongholds on the Old Island (Aaron 11-16, r-siege-pve: "Allow it to practice siegeing on NPC strongholds";
// showcase-notes cooks/wild-answers-1115.md 2.7). Each fort is placed here, once, off the island heightmap: the flattest
// open ground near its design point that is dry, off the lava and the ice, clear of every camp, arena, shrine, village,
// place and kit spot, with a gentle approach in front of its gate for siege engines (Electric Dreams' footprint test:
// the spot must hold the whole thing, never placed and then sunk). Every piece's ground height is written too, so every
// machine knows where the fort stands without a trace. Writes Data/strongholds.json (read by WRStronghold.cpp).
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
// Villages stand inland of their camp toward their land's centre (as island_places.mjs places them).
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
const clearOf = (x, y, extra) => keepOut.every(k => Math.hypot(k.x - x, k.y - y) > k.r + extra);

// ---- the fort's layout (cm, the fort's own frame: +X out of the gate) ----
const SEG = 440, N = 16;                                     // palisade segments (the piece's own length), round the ring
const R = SEG / (2 * Math.sin(Math.PI / N));                 // the ring's corners
const APO = R * Math.cos(Math.PI / N);                       // the middle of each segment
const KEEP_X = -420;                                         // the keep's middle (2 x 2 stone foundations), toward the back
function layout() {
	const parts = [];
	for (let s = 0; s < N; s++) {
		const a = (s / N) * 2 * Math.PI;                       // segment s faces out along a; s = 0 is the gate (+X)
		parts.push({ piece: s === 0 ? "palisade_gate" : "palisade_wall", role: s === 0 ? "gate" : "palisade", x: Math.cos(a) * APO, y: Math.sin(a) * APO, yaw: a * 180 / Math.PI + 90 });
	}
	for (const sgn of [-1, 1]) {
		const a = sgn * Math.PI / N;                           // the corners either side of the gate: the gate towers
		parts.push({ piece: "guard_tower", role: "tower", x: Math.cos(a) * R, y: Math.sin(a) * R, yaw: a * 180 / Math.PI });
	}
	for (const fx of [-200, 200]) for (const fy of [-200, 200]) parts.push({ piece: "stone_foundation", role: "keep_floor", x: KEEP_X + fx, y: fy, yaw: 0 });
	// The keep's walls on the foundations' outer edges; its door on the gate side, the south cell.
	const edges = [
		[KEEP_X + 400, -200, 90, "keep_door"], [KEEP_X + 400, 200, 90, "keep_wall"],
		[KEEP_X - 400, -200, 90, "keep_wall"], [KEEP_X - 400, 200, 90, "keep_wall"],
		[KEEP_X - 200, -400, 0, "keep_wall"], [KEEP_X + 200, -400, 0, "keep_wall"],
		[KEEP_X - 200, 400, 0, "keep_wall"], [KEEP_X + 200, 400, 0, "keep_wall"]];
	for (const [x, y, yaw, role] of edges) parts.push({ piece: role === "keep_door" ? "reinforced_door" : "stone_wall", role, x, y, yaw, onFloor: true });
	// The war chest against the back wall of the keep (north cell); the turrets on the towers' decks.
	parts.push({ piece: "storage_chest", role: "war_chest", x: KEEP_X - 300, y: 200, yaw: 0, onFloor: true });
	return parts;
}

/** Score a centre: the height spread under the whole footprint (ring + a 2 m margin), and the approach's. */
function fit(cx, cy, yawDeg) {
	let lo = 1e9, hi = -1e9;
	const Rm = R / 100 + 2;
	for (let dx = -Rm; dx <= Rm; dx += 2) for (let dy = -Rm; dy <= Rm; dy += 2) {
		if (Math.hypot(dx, dy) > Rm) continue;
		const z = h(cx + dx, cy + dy);
		lo = Math.min(lo, z); hi = Math.max(hi, z);
	}
	// The approach: 15-55 m out of the gate in a 50 m wide lane (where engines stand and roll).
	const a = yawDeg * Math.PI / 180, ux = Math.cos(a), uy = Math.sin(a);
	let alo = 1e9, ahi = -1e9, steep = 0;
	for (let t = 15; t <= 55; t += 4) for (let s = -25; s <= 25; s += 5) {
		const px = cx + ux * t - uy * s, py = cy + uy * t + ux * s;
		const z = h(px, py);
		alo = Math.min(alo, z); ahi = Math.max(ahi, z);
		steep = Math.max(steep, Math.abs(h(px + 2, py) - h(px - 2, py)) / 4, Math.abs(h(px, py + 2) - h(px, py - 2)) / 4);
	}
	return { spread: hi - lo, low: lo, approach: ahi - alo, steep };
}

function place(spec) {
	let best = null;
	for (let r = 0; r <= spec.search; r += 6) for (let a = 0; a < 360; a += r === 0 ? 360 : Math.max(4, 360 / (r / 3))) {
		const x = spec.near[0] + r * Math.cos(a * Math.PI / 180), y = spec.near[1] + r * Math.sin(a * Math.PI / 180);
		if (h(x, y) < 4 || nearSet(lava, x, y, 36) || (!spec.ice && nearSet(ice, x, y, 30)) || !clearOf(x, y, R / 100 + 10)) continue;
		for (let yaw = 0; yaw < 360; yaw += 22.5) {
			const f = fit(x, y, yaw);
			if (f.spread > 2.2 || f.steep > 0.36) continue;   // a 20 degree slope is the engines' limit
			const score = f.spread * 2 + f.approach * 0.3 + f.steep * 4 + r / spec.search * 0.6;
			if (!best || score < best.score) best = { x, y, yaw, score, ...f };
		}
	}
	if (!best) throw new Error(`no ground for ${spec.id}`);
	const a = best.yaw * Math.PI / 180, c = Math.cos(a), s = Math.sin(a);
	const parts = layout().map(p => {
		const wx = best.x + (p.x * c - p.y * s) / 100, wy = best.y + (p.x * s + p.y * c) / 100;
		return { ...p, x: Math.round(wx * 1000) / 10, y: Math.round(wy * 1000) / 10, z: Math.round(h(wx, wy) * 1000) / 10, yaw: Math.round(((p.yaw + best.yaw) % 360) * 10) / 10 };
	});
	// The keep stands level on its four foundations: their floor is the highest ground under them (the slabs sink 1.5 m).
	const floorZ = Math.max(...parts.filter(p => p.role === "keep_floor").map(p => p.z));
	for (const p of parts) if (p.role === "keep_floor" || p.onFloor) { p.z = Math.round(floorZ * 10) / 10; }
	for (const p of parts) delete p.onFloor;
	console.log(`${spec.id}: (${best.x.toFixed(0)}, ${best.y.toFixed(0)}) m, gate ${best.yaw} deg, ground spread ${best.spread.toFixed(2)} m, approach ${best.approach.toFixed(2)} m, steepest ${best.steep.toFixed(2)}`);
	return { ...spec.data, id: spec.id, centre: [Math.round(best.x * 1000) / 10, Math.round(best.y * 1000) / 10, Math.round(h(best.x, best.y) * 1000) / 10],
		yaw: best.yaw, ringRadius: Math.round(R), clearRadius: Math.round(R + 600), pieces: parts };
}

const forts = [
	place({ id: "emberwatch", near: [-1430, 330], search: 170, ice: false, data: {
		name: "Emberwatch", biome: "volcanic", zone: "V1",
		lore: "The Forge-wardens' watch over the Ash Plains, raised when the smiths still kept the Slag Light. Cinderfangs den in its yard now, and its bolt-throwers still wind themselves at anything that comes up the plain.",
		defenders: [{ creature: "cinderfang", count: 3 }, { creature: "magmaw", count: 1 }],
		chest: { upgrade_shard: 4, iron_ingot: 8, obsidian: 6, sulfur_crystal: 6, crossbow_bolt: 20, flint: 10 } } }),
	place({ id: "rimehold", near: [1300, -870], search: 120, ice: false, data: {
		name: "Rimehold", biome: "tundra", zone: "T1",
		lore: "The whalers' winter fort on the Snow Flats. The gloomhowls took it the winter the sledges stopped coming, and the bolt-throwers on its towers were never unloaded.",
		defenders: [{ creature: "gloomhowl", count: 3 }, { creature: "rimeclaw", count: 1 }],
		chest: { upgrade_shard: 4, iron_ingot: 8, lichen: 8, hide: 10, crossbow_bolt: 20, flint: 10 } } }),
];

const out = {
	_comment: "Generated by Tools/terrain/strongholds.mjs (edit the script, not this file). The NPC strongholds on the Old Island (Aaron 11-16, r-siege-pve; WRStronghold.cpp; cooks/wild-answers-1115.md 2.7): forts of the game's own build pieces that anyone may break on any world (PvE too) to practise sieging. Positions in game cm (L_Island: +x east, +y south); z is the landscape's ground under each piece (the keep's four foundations share the highest). Roles: gate, palisade, tower (a bolt turret rides each deck), keep_floor, keep_wall, keep_door (the chest opens once it or a keep wall falls), war_chest. defenders spawn in the yard and keep to it; chest = what the war chest holds, refilled when the fort rebuilds.",
	forts,
};
fs.writeFileSync(path.join(root, "Data/strongholds.json"), JSON.stringify(out, null, 1) + "\n");
console.log("wrote Data/strongholds.json");
