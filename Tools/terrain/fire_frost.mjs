// The Volcanic and the Frozen tundra moved onto the round island (plan phase 3, slices 2 and 3; Raven 11-19 "Move on"
// after Hearthmoot, "the rest of the slices the same way"). The Old Island's west half (Volcanic) and east half
// (Frozen tundra) are mapped onto their wedges of the round island: on the Old Island each half runs from its coast in
// to the central ridge and from north to south; on the round island each wedge runs from its coast in to the meadow's
// edge and from one border to the other. So every Old Island point becomes (q, t): q 0 at its coast, 1 at the ridge
// (measured against the Old Island's real coastline on that row, Data/sea_mask.json), t -1 north .. 1 south; and lands
// at the same (q, t) of its wedge. The Volcanic's depth is bent so Vulkarn's crater lands on the volcano of the cape
// (round island layout), the Tundra's is near straight.
//   node Tools/terrain/fire_frost.mjs   -> Tools/terrain/fire_frost_layout.json, Docs/design/fire_frost_map.svg
// Exported for the ground (round_ground.mjs) and the content builders: toRound, fromRound, oldZoneAt, lavaAt, iceAt.
import fs from "fs";
import path from "path";
import { fileURLToPath, pathToFileURL } from "url";
import * as RI from "./round_island.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const read = (p) => JSON.parse(fs.readFileSync(path.join(root, p), "utf8"));
const { RAD, coastR, meadowR, byId, bearingOf, pt } = RI;

// ---- the Old Island's coastline: on each 4 m row, the land run round x = 0 (its west and east coasts) -------------
const SEA = read("Data/sea_mask.json");
const coastRows = (() => {
	const rows = [];
	for (let r = 0; r < SEA.height; r++) {
		const runs = String(SEA.rows[r]).trim().split(/[ ,]+/).filter(Boolean).map(Number);
		let x = SEA.originX, land = false, best = null;
		for (const n of runs) {
			const x1 = x + n * SEA.cell;
			if (land) { const span = [x, x1]; if (!best || (span[0] <= 0 && span[1] >= 0) || Math.abs((span[0] + span[1]) / 2) < Math.abs((best[0] + best[1]) / 2)) best = best && best[0] <= 0 && best[1] >= 0 ? best : span; }
			x = x1; land = !land;
		}
		rows.push(best);   // cm, or null
	}
	return rows;
})();
const ROW0 = SEA.originY, CELL = SEA.cell;
/** The Old Island's coast on the row at y (m), on that side (-1 west, +1 east): |x| in metres. Row by row the old coast
 *  jumps metres at a time (a jagged shore); everything mapped from it (each sub-zone's shapes, the ice cap) stepped with
 *  it, a sawtooth seen from the ground. So it is filled, smoothed over about 60 m of shore and read between rows. */
const coastSmooth = (() => {
	const out = {};
	for (const side of [-1, 1]) {
		const raw = coastRows.map((span) => (span ? Math.abs(side < 0 ? span[0] : span[1]) / 100 : null));
		let last = null;
		for (let i = 0; i < raw.length; i++) { if (raw[i] == null) raw[i] = last; else last = raw[i]; }
		last = null;
		for (let i = raw.length - 1; i >= 0; i--) { if (raw[i] == null) raw[i] = last; else last = raw[i]; }
		let arr = raw.map((v) => v ?? 1900);
		for (let pass = 0; pass < 3; pass++) {
			const R = 8, next = new Array(arr.length);
			for (let i = 0; i < arr.length; i++) { let t = 0, n = 0; for (let k = -R; k <= R; k++) { const j = i + k; if (j >= 0 && j < arr.length) { t += arr[j]; n++; } } next[i] = t / n; }
			arr = next;
		}
		out[side] = arr;
	}
	return out;
})();
function oldCoast(side, y) {
	const arr = coastSmooth[side < 0 ? -1 : 1], fr = Math.max(0, Math.min(arr.length - 1.001, (y * 100 - ROW0) / CELL)), i = Math.floor(fr);
	return arr[i] + (arr[i + 1] - arr[i]) * (fr - i);
}
const Y_EDGE = 2016;   // world.islandNorth/SouthEdge, m

// ---- the wedges -----------------------------------------------------------------------------------------------------
// An Old Island point keeps its distance in from its own coast (x DEPTH) and its distance north or south of the half's
// middle (x ACROSS, as arc length round the wedge's middle bearing at that radius). Raven 11-20 ("make sure they look
// like they are supposed to"): the old sub-zones are stretched in (DEPTH) to fill about 70% of each wedge, so none is
// a sliver; the new ground past the old ridge is two sub-zones of its own, the outer one (V7 the Ashfall Woods, T7 the
// Mammoth Steppe) and the inner one down to the meadow (V6 the Cinder Terraces, T6 the Thaw). The Volcanic's middle
// bearing is its cape volcano's, which stands where Vulkarn's crater maps (round_ground.mjs takes VOLCANO from here).
const ACROSS = 0.75;
const SIDE = {
	volcanic: { side: -1, land: byId.volcanic, mid: 266.6, lo: 247.5, hi: 281, crater: [-786, 0], depth: +(process.env.FF_DEPTH_V || 1.75), split: +(process.env.FF_SPLIT_V || 620) },
	tundra: { side: 1, land: byId.tundra, mid: 93, lo: 75, hi: 111.5, crater: [804, 0], depth: +(process.env.FF_DEPTH_T || 1.5), split: +(process.env.FF_SPLIT_T || 760) },
};
const COAST_IN = 40;
// Bearing off the middle for an arc s (m) at radius r: north of the middle (s < 0) is the higher bearing on the west
// side, the lower on the east.
const bearingFor = (S, s, r) => Math.max(S.lo, Math.min(S.hi, S.mid + S.side * (s / r) / RAD));
const arcFor = (S, deg, r) => S.side * RI.dAng(deg, S.mid) * RAD * r;
/** Old Island (x, y) m -> round island (x, y) m. */
export function toRound(id, [x, y]) {
	const S = SIDE[id], inFromOld = Math.max(0, oldCoast(S.side, y) - S.side * x), s = y * ACROSS;
	// The radius for this depth depends on the bearing (the coast wanders), and the bearing on the radius: two passes.
	let deg = S.mid, r = coastR(deg) - COAST_IN - inFromOld * S.depth;
	for (let k = 0; k < 3; k++) { deg = bearingFor(S, s, Math.max(r, 400)); r = coastR(deg) - COAST_IN - inFromOld * S.depth; }
	return pt(deg, r);
}
/** Round island (x, y) m -> Old Island (x, y) m; beyond is how far past the old ridge (m of new ground, 0 if not). */
export function fromRound(id, [X, Y]) {
	const S = SIDE[id], deg = bearingOf(X, Y), r = Math.hypot(X, Y), s = arcFor(S, deg, r), y = Math.max(-Y_EDGE, Math.min(Y_EDGE, s / ACROSS));
	const inFrom = (coastR(deg) - COAST_IN - r) / S.depth, c = oldCoast(S.side, y), ax = c - inFrom;
	return { at: [S.side * Math.max(0, ax), y], beyond: Math.max(0, -ax) * S.depth, past: -ax * S.depth, inside: Math.abs(s / ACROSS) <= Y_EDGE && inFrom >= -30 && ax >= 0 };
}
export const CRATER_IN = (() => { const c = oldCoast(-1, 0); return COAST_IN + (c - 786) * SIDE.volcanic.depth; })();   // m in from the coast at the volcano's bearing

// ---- the Old Island's content ---------------------------------------------------------------------------------------
const PLACES = read("Data/island_places.json"), DRESS = read("Data/island_dressing.json"), WORLD = read("Data/world.json");
const sideOf = (x) => (x < 0 ? "volcanic" : "tundra");
// Sub-zones the Old Island had too small for a land of their own (the world-map's 0.6 km² floor): the Obsidian Ridges
// take more of the Solfatara's side of the ridge, the Glacier Plateau a wider ring round Skaldrith's bowl.
const ZONE_FIX = { T5: { ring: [804, 0, 0, 470] }, V3: { rect: [-1250, -2100, -640, -350] }, V4: { rect: [-640, -2100, -150, -350] } };
const ZONES = PLACES.zones.map((z) => (ZONE_FIX[z.id] && (z.ring ? ZONE_FIX[z.id].ring : ZONE_FIX[z.id].rect) ? { ...z, ...ZONE_FIX[z.id] } : z));
/** The Old Island sub-zone at (x, y) m (the places file's rings first, then its rects; ZONE_FIX applied). */
export function oldZoneAt([x, y]) {
	for (const z of ZONES) {
		if (z.ring) { const [cx, cy, r0, r1] = z.ring, d = Math.hypot(x - cx, y - cy); if (d >= r0 && d <= r1) return z.id; }
		else if (z.rect) { const [x0, y0, x1, y1] = z.rect; if (x >= x0 && x <= x1 && y >= y0 && y <= y1) return z.id; }
	}
	return null;
}
const cellSet = (arr) => { const s = new Set(); for (let i = 0; i + 2 < arr.length; i += 3) s.add(`${Math.round(arr[i] / 600)},${Math.round(arr[i + 1] / 600)}`); return s; };
const LAVA = cellSet(DRESS.hazards.lava), ICE = cellSet(DRESS.hazards.ice);
const inCells = (set, [x, y]) => set.has(`${Math.round(x * 100 / 600)},${Math.round(y * 100 / 600)}`);
/** Is the round island point (m) on the moved lava flow / ice sheet? */
export const iceAt = (XY) => { const o = fromRound("tundra", XY); return o.inside && o.beyond === 0 && inCells(ICE, o.at); };
/** The Old Island point under a round island point, held inside its half. */
export function oldUnder(id, XY) { const o = fromRound(id, XY); return [SIDE[id].side * Math.max(160, Math.abs(o.at[0])), o.at[1]]; }   // (the old zones stop 150 m short of the ridge, at its passes)
/** The sub-zone at a round island point of the Volcanic or the Tundra: V1-V5 / T1-T5 where the Old Island's ground
 *  lands, V6 / T6 on the new inner ground past its ridge (more than INNER_FROM m past it). The Crater Highlands are the
 *  volcano's (within CRATER_REACH m of Vulkarn's crater): past it their old band (it ran on to the ridge) goes to the
 *  Solfatara on its north side and the Lava Fields on its south. */
export const INNER_FROM = 60;
export const CRATER_REACH = 1100;
/** A round island point moved a little (two scales: ~160 m bends over ~900 m, ~60 m tongues over ~160 m) before it
 *  is looked up in the Old Island's zones, so their straight edges come out as ragged, wandering lines. The ground's
 *  shapes (round_ground.mjs landV / landT) and its surfaces both use it, so they agree. */
export const warpXY = ([x, y]) => [
	x + 160 * RI.fbm(x / 900, y / 900, 501, 3) + 60 * RI.fbm(x / 160, y / 160, 503, 2),
	y + 160 * RI.fbm(x / 900, y / 900, 502, 3) + 60 * RI.fbm(x / 160, y / 160, 504, 2),
];
/** How far past the old ridge a round island point lies (m; negative on the old ground), the line wandering ~250 m in
 *  and out over ~1.4 km. */
export const beyondAt = (id, XY) => fromRound(id, warpXY(XY)).past - 260 * RI.fbm(XY[0] / 1400, XY[1] / 1400, 505, 2);
/** Where the new ground's outer sub-zone (V7, T7) gives way to the inner one (V6, T6): m past the old ridge, wandering. */
export function subZoneAt(id, XY0) {
	const XY = warpXY(XY0), b = beyondAt(id, XY0);
	if (b > INNER_FROM) return b > SIDE[id].split + 180 * RI.fbm(XY0[0] / 1100, XY0[1] / 1100, 507, 2) ? (id === "volcanic" ? "V6" : "T6") : (id === "volcanic" ? "V7" : "T7");
	const o = oldUnder(id, XY), z = oldZoneAt(o) || (id === "volcanic" ? "V1" : "T1");
	if (z === "V5" && Math.hypot(XY[0] - CRATER_AT[0], XY[1] - CRATER_AT[1]) > CRATER_REACH) return o[1] < 50 ? "V4" : "V2";
	return z;
}
// The lava river: the Old Island's flow on its own line (it ran off the central ridge, now the inner end of the flow,
// where it breaks out of a tube mouth: the Kiln Roads carry it under the volcano), west down to the sea. Its centre
// line from the flow's cells, averaged every 60 m along it, then on past the coast. Map metres.
export const LAVA_RIVER = (() => {
	const cells = [];
	for (let i = 0; i + 2 < DRESS.hazards.lava.length; i += 3) cells.push([DRESS.hazards.lava[i] / 100, DRESS.hazards.lava[i + 1] / 100]);
	const xs = cells.map((c) => c[0]), x0 = Math.min(...xs), x1 = Math.max(...xs), pts = [];
	for (let x = x1; x >= x0; x -= 60) {
		const near = cells.filter((c) => c[0] <= x && c[0] > x - 60);
		if (near.length) pts.push(toRound("volcanic", [x - 30, near.reduce((t, c) => t + c[1], 0) / near.length]));
	}
	// Smooth it a little, then carry on west past the coast.
	const sm = pts.map((p, i) => i === 0 || i === pts.length - 1 ? p : [0, 1].map((k) => (pts[i - 1][k] + 2 * p[k] + pts[i + 1][k]) / 4));
	const last = sm[sm.length - 1], prev = sm[sm.length - 2], dir = [last[0] - prev[0], last[1] - prev[1]], L = Math.hypot(...dir);
	for (let d = 80; ; d += 80) { const p = [last[0] + dir[0] / L * d, last[1] + dir[1] / L * d]; sm.push(p); if (Math.hypot(...p) > coastR(bearingOf(...p)) + 40) break; }
	return sm;
})();
export const LAVA_WIDTH = 16;   // m across the molten channel
// The villages where the Old Island had them (each beside its wake camp, a little in from the coast); Emberwick a
// little further down off the volcano's steeper flank.
export const VILLAGES = (() => {
	const D = DRESS.pois, camp = (n) => { const p = D.find((q) => q.name === n); return [p.x / 100, p.y / 100]; };
	const out = {};
	for (const v of WORLD.villages) {
		const c = camp(v.camp), cen = WORLD.biomes[v.biome].centre, dd = Math.hypot(cen[0] - c[0], cen[1] - c[1]);
		const old = [c[0] + (cen[0] - c[0]) / dd * v.inland, c[1] + (cen[1] - c[1]) / dd * v.inland];
		let at = toRound(v.biome, old);
		out[v.biome] = { id: v.id, name: v.name, old, at };
	}
	return out;
})();
export { SIDE };
const CRATER_AT = toRound("volcanic", [-786, 0]);
// Where the road crosses the lava river: the Fallen Smith lies beside it, his hammer haft the bridge.
export const SMITH_CROSSING = (() => {
	const road = byId.volcanic.road;
	let best = null;
	for (let i = 1; i < road.length; i++) for (let j = 1; j < LAVA_RIVER.length; j++) {
		const [a, b] = [road[i - 1], road[i]], [c, d] = [LAVA_RIVER[j - 1], LAVA_RIVER[j]];
		const den = (b[0] - a[0]) * (d[1] - c[1]) - (b[1] - a[1]) * (d[0] - c[0]); if (Math.abs(den) < 1e-9) continue;
		const t = ((c[0] - a[0]) * (d[1] - c[1]) - (c[1] - a[1]) * (d[0] - c[0])) / den, u = ((c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0])) / den;
		if (t >= 0 && t <= 1 && u >= 0 && u <= 1) best = { at: [a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])], road: [b[0] - a[0], b[1] - a[1]] };
	}
	return best;
})();
// Hand moves after the mapping (map metres, by place id or site name): a mini-boss off the road (Raven: bosses off the
// roads) with its spire, the Fallen Smith at the crossing.
const NUDGE = {
	Ignessa: { to: [-4500, 1090] }, wisp_spire: { to: [-4530, 1065] },
	// The Pale Hunter keeps to the deep pines, off the road that now runs past Rimehold.
	"Pale Hunter": { to: [4250, 1150] }, pale_pines: { to: [4220, 1125] },
	fallen_smith: SMITH_CROSSING ? (() => { const L = Math.hypot(...SMITH_CROSSING.road), n = [-SMITH_CROSSING.road[1] / L, SMITH_CROSSING.road[0] / L]; return { to: [SMITH_CROSSING.at[0] + n[0] * 32, SMITH_CROSSING.at[1] + n[1] * 32] }; })() : null,
};
export const nudged = (key, at) => { const n = NUDGE[key]; if (!n) return at; return n.to ? n.to : [at[0] + n[0], at[1] + n[1]]; };

const MAIN = process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href;
if (MAIN) {
	const R = (p) => p.map((v) => Math.round(v));
	const move = (o) => { const id = sideOf(o.at[0]); return { ...o, land: id, old: o.at, at: R(nudged(o.id || o.name, toRound(id, o.at))) }; };
	const layout = {
		_comment: "Generated by Tools/terrain/fire_frost.mjs: the Old Island's Volcanic and Frozen tundra content (Data/island_places.json, island_dressing.json pois and caves, world.json villages, shrines, masters, hidden routes) moved onto the round island's wedges, map metres (x east, y south). old = where it was on the Old Island.",
		places: PLACES.places.map((p) => { const o = move(p); if (p.from) { o.from = R(toRound(o.land, p.from)); o.to = R(toRound(o.land, p.to)); } return o; }),
		kit: PLACES.kit.map(move),
		ground: PLACES.ground.map(move),
		hazards: {
			fumaroles: PLACES.hazards.fumaroles.map(move), geysers: PLACES.hazards.geysers.map(move), thinIce: PLACES.hazards.thinIce.map(move),
			katabatic: { centre: R(toRound("tundra", PLACES.hazards.katabatic.centre)), inner: Math.round(PLACES.hazards.katabatic.inner * 1.5), outer: Math.round(PLACES.hazards.katabatic.outer * 1.5) },
		},
		sites: DRESS.pois.filter((p) => /boss|mini|cave/.test(p.kind) && Math.abs(p.y) < 201600 && Math.abs(p.x) < 210000).map((p) => move({ kind: p.kind, name: p.name, at: [p.x / 100, p.y / 100] })),
		caves: DRESS.caves.map((c) => move({ biome: c.biome, variant: c.variant, yaw: c.yaw, at: [c.x / 100, c.y / 100] })),
		hidden: WORLD.hidden.filter((h) => /volcanic|tundra/.test(h.biome)).map((h) => ({ id: h.id, name: h.name, land: h.biome, old: h.route, route: h.route.map((p) => R(toRound(h.biome, p))) })),
		shrines: (WORLD.shrines.list || WORLD.shrines).filter((s) => /volcanic|tundra/.test(s.biome) && s.at).map((s) => ({ id: s.id, land: s.biome, old: s.at, at: R(toRound(s.biome, s.at)) })),
		lands: {},
	};
	// Per land: the round island's own camps and road (signed off), the village on the road where it leaves the coast
	// band (with the great shrine and the path master), Vulkarn on the volcano and Skaldrith in his bowl.
	for (const [id, S] of Object.entries(SIDE)) {
		const L = S.land, boss = R(toRound(id, S.crater));
		layout.lands[id] = { camps: L.camps.map((c) => R(Array.isArray(c) ? c : c.at)), road: L.road.map(R), village: R(VILLAGES[id].at), villageId: VILLAGES[id].id, boss, mouthShrine: R(L.mouthShrine), champion: R(L.champion), crossroads: R(L.road[L.road.length - 1]) };
		if (id === "volcanic") { layout.lands[id].lavaRiver = LAVA_RIVER.map(R); layout.lands[id].lavaWidth = LAVA_WIDTH; layout.lands[id].smithBridge = SMITH_CROSSING && R(SMITH_CROSSING.at); }
	}
	fs.writeFileSync(path.join(root, "Tools/terrain/fire_frost_layout.json"), JSON.stringify(layout, null, 1));

	// ---- the review map: each land on its own panel, sub-zones and the flow / sheet filled by the inverse map -------
	const COL = { V1: "#8f8a84", V2: "#3a3431", V3: "#4b4458", V4: "#c9b54a", V5: "#7a3b22", T1: "#eef2f5", T2: "#4c6b55", T3: "#9fd4e0", T4: "#7fb1d6", T5: "#d9e6f2", V6: "#6b5646", T6: "#9fb08f", V7: "#a9a8ab", T7: "#c2a66a" };
	const NEWZ = { V6: "Cinder Terraces (new)", T6: "The Thaw (new)", V7: "Ashfall Woods (new)", T7: "Mammoth Steppe (new)" };
	const panel = (id, ox, W) => {
		const L = SIDE[id].land, xs = L.polygon.map((p) => p[0]), ys = L.polygon.map((p) => p[1]);
		const x0 = Math.min(...xs) - 150, x1 = Math.max(...xs) + 150, y0 = Math.min(...ys) - 150, y1 = Math.max(...ys) + 150, sc = W / (x1 - x0), H = (y1 - y0) * sc;
		const P = ([x, y]) => [(ox + (x - x0) * sc).toFixed(1), (40 + (y - y0) * sc).toFixed(1)];
		const out = [];
		const step = 30;
		for (let y = y0; y < y1; y += step) for (let x = x0; x < x1; x += step) {
			const c = [x + step / 2, y + step / 2];
			if (!RI.inPoly(c, L.polygon)) continue;
			const o = fromRound(id, c);
			const z = subZoneAt(id, c), lava = false, ice = id === "tundra" && o.inside && inCells(ICE, o.at);
			const [px, py] = P([x, y]);
			out.push(`<rect x='${px}' y='${py}' width='${(step * sc + 0.4).toFixed(1)}' height='${(step * sc + 0.4).toFixed(1)}' fill='${lava ? "#ff6a1a" : ice ? "#bfe6f5" : COL[z] || "#666"}'/>`);
		}
		out.push(`<path d='M${L.polygon.map((p) => P(p).join(" ")).join(" L")} Z' fill='none' stroke='#000' stroke-width='1.2'/>`);
		const lay = layout.lands[id];
		const mark = (...a) => mark0(...a);
		out.push(`<polyline points='${lay.road.map((p) => P(p).join(",")).join(" ")}' fill='none' stroke='#d8c39a' stroke-width='3'/>`);
		if (lay.lavaRiver) out.push(`<polyline points='${lay.lavaRiver.map((p) => P(p).join(",")).join(" ")}' fill='none' stroke='#ff6a1a' stroke-width='4'/>`);
		mark(lay.mouthShrine, "#7df", 5, "", false); mark(lay.champion, "#e07000", 4, "champion", false);
		for (const k of layout.kit.filter((k) => k.land === id)) { const [px, py] = P(k.at); out.push(`<circle cx='${px}' cy='${py}' r='1.8' fill='#222'/>`); }
		for (const h of layout.hidden.filter((h) => h.land === id)) out.push(`<polyline points='${h.route.map((p) => P(p).join(",")).join(" ")}' fill='none' stroke='#ff3' stroke-width='2' stroke-dasharray='5 3'/>`);
		function mark0(at, fill, r, label, bold) { const [px, py] = P(at); out.push(`<circle cx='${px}' cy='${py}' r='${r}' fill='${fill}' stroke='#000' stroke-width='1'/>`); if (label) out.push(`<text x='${+px + r + 2}' y='${+py + 4}' font-size='${bold ? 12 : 10}' font-family='Segoe UI' font-weight='${bold ? 700 : 400}' fill='#000' stroke='#fff' stroke-width='3' paint-order='stroke'>${label}</text>`); }
		for (const p of layout.places.filter((p) => p.land === id)) mark(p.at, p.landmark ? "#fff" : "#ccc", p.landmark ? 5 : 3.5, p.name.replace(/^The /, ""), p.landmark);
		for (const s of layout.sites.filter((s) => s.land === id)) mark(s.at, s.kind === "cave" ? "#6b4a2a" : s.kind === "boss" ? "#c00" : "#e07000", s.kind === "cave" ? 4 : 6, s.kind === "cave" ? "" : s.name, true);
		for (const s of layout.shrines.filter((s) => s.land === id)) mark(s.at, "#7df", 4, "", false);
		for (const c of lay.camps) mark(c, "#2b8", 5, "camp", false);
		mark(lay.village, "#fd0", 7, id === "volcanic" ? "Emberwick" : "Rimehold", true);
		out.push(`<text x='${ox}' y='26' font-size='18' font-weight='700' font-family='Segoe UI'>${id === "volcanic" ? "The Volcanic (Fire)" : "The Frozen tundra (Frost)"}</text>`);
		return { svg: out.join(""), H };
	};
	const a = panel("volcanic", 20, 760), b = panel("tundra", 820, 620);
	const legend = Object.entries(COL).map(([k, c], i) => `<rect x='${20 + (i % 7) * 205}' y='${Math.max(a.H, b.H) + 60 + Math.floor(i / 7) * 22}' width='14' height='14' fill='${c}' stroke='#000'/><text x='${38 + (i % 7) * 205}' y='${Math.max(a.H, b.H) + 72 + Math.floor(i / 7) * 22}' font-size='11' font-family='Segoe UI'>${k} ${NEWZ[k] || PLACES.zones.find((z) => z.id === k)?.name.replace(/^The /, "") || ""}</text>`).join("");
	const H = Math.max(a.H, b.H) + 100;
	fs.writeFileSync(path.join(root, "Docs/design/fire_frost_map.svg"), `<svg xmlns='http://www.w3.org/2000/svg' width='1460' height='${Math.round(H)}' viewBox='0 0 1460 ${Math.round(H)}'><rect width='100%' height='100%' fill='#f4f1ea'/>${a.svg}${b.svg}${legend}</svg>`);
	// Overlaps: places closer than their clearings.
	const ps = layout.places;
	const tight = [];
	for (let i = 0; i < ps.length; i++) for (let j = i + 1; j < ps.length; j++) { const d = Math.hypot(ps[i].at[0] - ps[j].at[0], ps[i].at[1] - ps[j].at[1]); if (d < (ps[i].clear || 10) + (ps[j].clear || 10)) tight.push(`${ps[i].id}~${ps[j].id} ${Math.round(d)} m`); }
	console.log(`moved ${ps.length} places, ${layout.kit.length} kit, ${layout.sites.length} sites, ${layout.caves.length} caves, ${layout.shrines.length} shrines; boss ${JSON.stringify(layout.lands.volcanic.boss)} / ${JSON.stringify(layout.lands.tundra.boss)}`);
	console.log(tight.length ? `too close: ${tight.join(", ")}` : "no places closer than their clearings");
}
