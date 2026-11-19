// The round island's layout for Raven's sign-off (plan phase 3 "One big world", step 1: "Lay the pie out to scale").
// Nine surface lands as uneven slices round Hearthmoot, each land's share of the island = its share of Aaron's surface
// sizes (Docs/design/cooks/world-map.md 2.2), every land scaled by the same amount (about double the area: Raven's
// "bigger, 10 to 12 km across"). The sky islands over the north, the Crystal caves under the north-east, the Fungal
// caverns under the south-west, the Ocean all round. Writes:
//   Tools/terrain/round_island_layout.json   every number in metres (x east, y south, bearings clockwise from north)
//   Docs/design/round_island_map.svg         the map to scale
//   (stdout) the table for the plan doc
//   node Tools/terrain/round_island.mjs [--widget <file>]   --widget also writes the plan doc's drawing module
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

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
const COMMONS = 650;               // the guarded ring round the walls where the roads meet: no wild spawns
const FIRST_DEG = -76.85;          // where the ring starts (Storm's west edge): puts the Storm-Desert-Forest block on north
// The coast: a few slow waves so it reads as land, not a disc (about ±4% of the radius), the same every run.
const WAVES = [[3, 0.022, 0.7], [5, 0.014, 2.1], [8, 0.008, 4.4], [13, 0.004, 1.3]];
const coastShape = (deg) => 1 + WAVES.reduce((s, [k, a, p]) => s + a * Math.sin(k * deg * Math.PI / 180 + p), 0);
const SHALLOWS = 300, REEFS = 1200;  // off the coast: calm shallows by the camps, reefs, then deep water
const MAP_HALF = 8000;             // the level's square: 16 km, the deep sea round the island

// ---- the coast radius from the areas ----------------------------------------------------------------------------
const totalAaron = LANDS.reduce((s, l) => s + l.aaron, 0);
const landTarget = totalAaron * SCALE_AREA * 1e6;  // m²
// ∫ ½ (R0² s(θ)² − COMMONS²) dθ over the circle = landTarget  →  R0
const STEPS = 7200;
const dTh = 2 * Math.PI / STEPS;
let s2 = 0;
for (let i = 0; i < STEPS; i++) s2 += coastShape(i * 360 / STEPS) ** 2;
const R0 = Math.sqrt((landTarget / dTh / 0.5 / 1 + STEPS * COMMONS ** 2) / s2);
const coastR = (deg) => R0 * coastShape(deg);

// Slice edges: walk round from FIRST_DEG adding area until each land has its share.
const slices = [];
{
	let deg = FIRST_DEG;
	const step = 0.01;
	for (const land of LANDS) {
		const want = land.aaron * SCALE_AREA * 1e6;
		let got = 0, start = deg;
		while (got < want) {
			const r = coastR(deg + step / 2);
			got += 0.5 * (r * r - COMMONS * COMMONS) * step * Math.PI / 180;
			deg += step;
		}
		slices.push({ ...land, start, end: deg, area: got });
	}
}
const norm = (d) => ((d % 360) + 360) % 360;
const xy = (deg, r) => [Math.round(r * Math.sin(deg * Math.PI / 180)), Math.round(-r * Math.cos(deg * Math.PI / 180))];
// Difficulty by distance from the coast (Raven 11-18: easy at the coast, harder inland, guarded roads): three bands
// between the Commons and the coast, of equal depth wherever you are.
const band = (deg, f) => COMMONS + (coastR(deg) - COMMONS) * f;
const BANDS = [{ name: "inland (hard)", from: 0, to: 1 / 3 }, { name: "midlands", from: 1 / 3, to: 2 / 3 }, { name: "coast (easy)", from: 2 / 3, to: 1 }];

// ---- each land's places on the layout ---------------------------------------------------------------------------
for (const s of slices) {
	const mid = (s.start + s.end) / 2, span = s.end - s.start;
	s.centre = mid;
	// Three wake-up camps on the coast (your path picks the land you wake in, at one of three camps); the middle one
	// starts the gate road.
	s.camps = [0.22, 0.5, 0.78].map((f) => { const d = s.start + span * f; return xy(d, coastR(d) - 220); });
	s.gate = xy(mid, CITY_WALL);
	// The land's half of its old pass becomes its gate road's works (Raven 11-18): the mouth shrine where the road
	// leaves the coast band, the champion where it enters the inland band, the fort at the Commons' edge.
	s.mouthShrine = xy(mid, band(mid, 2 / 3));
	s.champion = xy(mid, band(mid, 1 / 3) - 120);
	s.gateFort = xy(mid, COMMONS + 60);
	// The major boss off the road (Raven: bosses off the roads), deep inland, to the wider side of the slice.
	const off = Math.min(span * 0.32, 14);
	s.bossLair = xy(mid + off, band(mid + off, 0.24));
	s.coastArc = (() => { let L = 0; for (let d = s.start; d < s.end; d += 0.1) L += coastR(d) * 0.1 * Math.PI / 180; return L; })();
	s.bandArea = BANDS.map((b) => { let A = 0; for (let d = s.start; d < s.end; d += 0.05) { const r1 = band(d, b.from), r2 = band(d, b.to); A += 0.5 * (r2 * r2 - r1 * r1) * 0.05 * Math.PI / 180; } return A; });
}
const byId = Object.fromEntries(slices.map((s) => [s.id, s]));

// ---- the layers ---------------------------------------------------------------------------------------------------
// Each footprint is an annular sector sized to Aaron's area x2: the sky islands 2.0 km² of islands in ~5 km² of air,
// the Fungal caverns ~3 km² below, the Crystal caves ~2 km² below (world-map.md 2.2).
const sector = (from, to, r1, areaKm2) => {
	const th = (norm(to - from) || 360) * Math.PI / 180;
	const r2 = Math.sqrt(2 * areaKm2 * 1e6 / th + r1 * r1);
	return { from: norm(from), to: norm(to), r1: Math.round(r1), r2: Math.round(r2), areaKm2 };
};
const sky = sector(byId.storm_peaks.centre, byId.forest.centre, 1900, 5 * SCALE_AREA);
sky.islandsKm2 = 2.0 * SCALE_AREA;
sky.height = [500, 1300];
sky.note = "thickest over Storm peaks, the Desert and the Forest; a few more over other slices";
const crystal = sector(byId.forest.start + 6, byId.tundra.end - 4, 1500, 2 * SCALE_AREA);
crystal.note = "under the Forest and the Tundra; deeper halls hold harder creatures and better ore";
const fungal = sector(byId.graveyard.start + 2, byId.wasteland.end - 6, 1500, 3 * SCALE_AREA);
fungal.note = "under the Graveyard and the Wasteland, across the island from the sky (Wind and Spore are opposites)";
// The Severance (the old Void–Arcane pass) becomes the way down into the Crystal caves at the Forest–Tundra edge; the
// Flue (the 740 m shaft) rises from the Fungal floor to the south-west surface, with one sky island anchored over it.
const severanceDeg = byId.forest.end, flueDeg = (byId.graveyard.end + byId.wasteland.start) / 2 + 12;
const layers = {
	sky, crystal, fungal,
	severance: { at: xy(severanceDeg, band(severanceDeg, 0.5)), note: "a rift-cut gorge down into the Crystal caves" },
	flue: { at: xy(flueDeg, band(flueDeg, 0.3)), note: "the shaft up from the Fungal caverns, a sky island anchored over it" },
	// Wind, Spore and Arcane players wake inside their layer (Raven Q18), each start near the coast above or below.
	starts: {
		wind: { at: xy(byId.desert.start, coastR(byId.desert.start) - 700), height: 350, note: "a low starter sky island over the north coast: ropes, ledges and updrafts down" },
		spore: { at: xy(byId.wasteland.start + 10, coastR(byId.wasteland.start + 10) - 900), depth: 60, note: "a shallow cavern under the south-west coast; no markings out" },
		arcane: { at: xy(byId.tundra.start + 12, coastR(byId.tundra.start + 12) - 900), depth: 60, note: "a shallow crystal hall under the north-east coast; no markings out" },
	},
};

// ---- Hearthmoot: 12 quarters, one per path, each land's quarter behind its own gate where it can be ---------------
// The three layer paths take the quarters between the lands their layer lies under or over.
const quarterOrder = ["lightning", "wind", "sun", "nature", "arcane", "frost", "blood", "tide", "necromancy", "spore", "void", "fire"];
const quarters = quarterOrder.map((p, i) => ({ path: p, from: norm(FIRST_DEG + i * 30), to: norm(FIRST_DEG + (i + 1) * 30) }));

// ---- out ------------------------------------------------------------------------------------------------------------
const coast = []; for (let d = 0; d < 360; d += 2) coast.push(xy(d, coastR(d)));
const layout = {
	_comment: "The round island's layout (plan phase 3 step 1, for Raven's sign-off). Made by Tools/terrain/round_island.mjs: change that, not this. Metres; x east, y south; bearings in degrees clockwise from north. Land areas = Aaron's surface sizes (world-map.md 2.2) x " + SCALE_AREA + ".",
	coastRadius: { mean: Math.round(R0), min: Math.round(Math.min(...coast.map(([x, y]) => Math.hypot(x, y)))), max: Math.round(Math.max(...coast.map(([x, y]) => Math.hypot(x, y)))) },
	across: Math.round(2 * R0),
	landKm2: +(landTarget / 1e6).toFixed(1),
	hearthmoot: { wallRadius: CITY_WALL, commonsRadius: COMMONS, quarters },
	bands: BANDS.map((b) => ({ name: b.name, fromFraction: +b.from.toFixed(3), toFraction: +b.to.toFixed(3) })),
	sea: { shallows: SHALLOWS, reefs: REEFS, mapHalf: MAP_HALF, thalassor: xy(byId.ocean.centre, coastR(byId.ocean.centre) + 1500) },
	lands: slices.map((s) => ({
		id: s.id, name: s.name, path: s.path, aaronKm2: s.aaron, km2: +(s.area / 1e6).toFixed(2),
		fromDeg: +norm(s.start).toFixed(2), toDeg: +norm(s.end).toFixed(2), centreDeg: +norm(s.centre).toFixed(2), spanDeg: +(s.end - s.start).toFixed(2),
		coastKm: +(s.coastArc / 1000).toFixed(2), bandKm2: s.bandArea.map((a) => +(a / 1e6).toFixed(2)),
		camps: s.camps, gate: s.gate, gateFort: s.gateFort, champion: s.champion, mouthShrine: s.mouthShrine, bossLair: s.bossLair,
	})),
	layers,
	coast,
};
fs.writeFileSync(path.join(root, "Tools/terrain/round_island_layout.json"), JSON.stringify(layout, null, 1));

// ---- the map ----------------------------------------------------------------------------------------------------------
// One SVG for the repo (Docs/design) and, with --widget, the same drawing as the plan doc's module (every label typed
// out with its own id, as the doc's drawings need).
const W = 760, PAD = 20, VIEW = 7100;               // the drawing shows ±7.1 km
const sc = (W - 2 * PAD) / (2 * VIEW);
const P = ([x, y]) => [Math.round(W / 2 + x * sc), Math.round(PAD + VIEW * sc + y * sc) + 40];
const pathOf = (pts, close = true) => "M" + pts.map((p) => P(p).join(",")).join("L") + (close ? "Z" : "");
const ringPts = (f, from = 0, to = 360, stepDeg = 1) => { const o = []; for (let d = from; d <= to + 1e-6; d += stepDeg) o.push(xy(d, f(d))); return o; };
const slicePath = (s) => {
	const outer = ringPts(coastR, s.start, s.end, 1.5);
	const inner = ringPts(() => COMMONS, s.start, s.end, 4).reverse();
	return pathOf([...outer, ...inner]);
};
const sectorPath = (L) => {
	const to = L.to < L.from ? L.to + 360 : L.to;
	return pathOf([...ringPts(() => L.r2, L.from, to, 3), ...ringPts(() => L.r1, L.from, to, 3).reverse()]);
};
const H = Math.round(40 + PAD * 2 + 2 * VIEW * sc + 120);
const shapes = [];
shapes.push(`<rect x='0' y='40' width='${W}' height='${Math.round(2 * PAD + 2 * VIEW * sc)}' fill='#2f6f8f' opacity='0.18'/>`);
shapes.push(`<path d='${pathOf(ringPts((d) => coastR(d) + REEFS, 0, 360, 3))}' fill='#4fb0b8' opacity='0.18'/>`);
shapes.push(`<path d='${pathOf(ringPts((d) => coastR(d) + SHALLOWS, 0, 360, 3))}' fill='#7fd0d0' opacity='0.35'/>`);
for (const s of slices) shapes.push(`<g data-claude-anchor='land-${s.id}'><path d='${slicePath(s)}' fill='${s.colour}' opacity='0.85' stroke='#ffffff' strokeWidth='1.2'/></g>`);
for (const f of [1 / 3, 2 / 3]) shapes.push(`<path d='${pathOf(ringPts((d) => band(d, f), 0, 360, 4))}' fill='none' stroke='#000000' strokeOpacity='0.35' strokeDasharray='4 4'/>`);
shapes.push(`<g data-claude-anchor='layer-sky'><path d='${sectorPath(sky)}' fill='none' stroke='#ffffff' strokeWidth='2.2' strokeDasharray='7 4'/></g>`);
shapes.push(`<g data-claude-anchor='layer-crystal'><path d='${sectorPath(crystal)}' fill='#b06fe0' fillOpacity='0.18' stroke='#7a3fb0' strokeWidth='2' strokeDasharray='2 3'/></g>`);
shapes.push(`<g data-claude-anchor='layer-fungal'><path d='${sectorPath(fungal)}' fill='#3fb08a' fillOpacity='0.18' stroke='#1f7a5a' strokeWidth='2' strokeDasharray='2 3'/></g>`);
shapes.push(`<g data-claude-anchor='commons'><circle cx='${P([0, 0])[0]}' cy='${P([0, 0])[1]}' r='${(COMMONS * sc).toFixed(1)}' fill='#e8dcc0' stroke='#8a7a50'/><circle cx='${P([0, 0])[0]}' cy='${P([0, 0])[1]}' r='${(CITY_WALL * sc).toFixed(1)}' fill='#c9b27a' stroke='#5a4a20' strokeWidth='2'/></g>`);
for (const s of slices) {
	const [cx, cy] = P(s.camps[1]), [gx, gy] = P(s.gate);
	shapes.push(`<line x1='${cx}' y1='${cy}' x2='${gx}' y2='${gy}' stroke='#3a2a10' strokeWidth='1.6'/>`);
	for (const c of s.camps) { const [x, y] = P(c); shapes.push(`<circle cx='${x}' cy='${y}' r='3.2' fill='#ffffff' stroke='#000000' strokeWidth='0.8'/>`); }
	const [bx, by] = P(s.bossLair); shapes.push(`<path d='M${bx},${by - 5}L${bx + 5},${by + 4}L${bx - 5},${by + 4}Z' fill='#b02020' stroke='#ffffff' strokeWidth='0.8'/>`);
	const [fx, fy] = P(s.gateFort); shapes.push(`<rect x='${fx - 2.5}' y='${fy - 2.5}' width='5' height='5' fill='#3a2a10'/>`);
}
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
T("title", [24, 26], `The round island to scale: ${(2 * R0 / 1000).toFixed(1)} km across, nine lands round Hearthmoot`, 15, 600, "start", "var(--cds-text-primary)", 0);
for (const s of slices) {
	const lp = P(xy(s.centre, band(s.centre, s.end - s.start < 30 ? 0.8 : 0.56)));
	T(`name-${s.id}`, [lp[0], lp[1] - 4], s.name, 12, 700);
	T(`size-${s.id}`, [lp[0], lp[1] + 11], `${(s.area / 1e6).toFixed(1)} km²`, 11, 400);
}
T("hearthmoot", [P([0, 0])[0], P([0, 0])[1] + 4], "Hearthmoot", 11, 700);
T("sky-label", (([x, y]) => [x, y])(P(xy(norm(byId.desert.centre), sky.r2 + 220))), "sky islands above", 11, 600, "middle", "#1a3a5a");
T("crystal-label", P(xy(62, coastR(62) + 750)), "Crystal caves below", 11, 700, "middle", "#5a2a8a");
T("fungal-label", P(xy(222, coastR(222) + 750)), "Fungal caverns below", 11, 700, "middle", "#1a5a3a");
T("severance", (([x, y]) => [x + 8, y + 4])(P(layers.severance.at)), "the Severance", 10, 600, "start");
T("flue", (([x, y]) => [x + 8, y + 4])(P(layers.flue.at)), "the Flue", 10, 600, "start");
T("thalassor", (([x, y]) => [x + 9, y + 4])(P(layout.sea.thalassor)), "Thalassor's waters", 10, 600, "start");
T("scale", [PAD + 6, sbY - 7], "2 km", 11, 600, "start");
T("north", [W - 40, 98], "N", 12, 700);
// Key, under the map.
const ky = Math.round(40 + 2 * PAD + 2 * VIEW * sc + 22);
T("key-1", [24, ky], "White dots: the three wake-up camps per land. Lines: each land's road from its middle camp to its gate.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-2", [24, ky + 18], "Squares: gate forts at the guarded Commons. Red triangles: major boss lairs, off the roads, deep inland.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-3", [24, ky + 36], "Dashed rings: coast (easy), midlands, inland (hard). Yellow diamonds: Wind, Spore and Arcane starts.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-4", [24, ky + 54], "White dashes: sky islands overhead. Purple and green dots: Crystal caves and Fungal caverns below.", 11, 400, "start", "var(--cds-text-primary)", 0);
T("key-5", [24, ky + 72], "Pale ring: calm shallows (300 m); wider ring: reefs (1.2 km); then deep sea.", 11, 400, "start", "var(--cds-text-primary)", 0);

const svgBody = `<svg viewBox='0 0 ${W} ${H}' role='img' aria-label='The round island to scale: nine lands round Hearthmoot' fontFamily='sans-serif'>${shapes.join("")}${labels.join("")}</svg>`;
const xmlSvg = svgBody.replace(/ ([a-z]+)([A-Z][a-z]+)='/g, (m, a, b) => a + b === "viewBox" ? m : ` ${a}-${b.toLowerCase()}='`).replace("<svg ", "<svg xmlns='http://www.w3.org/2000/svg' ")
	.replace(/ data-claude-[a-z-]+='[^']*'/g, "").replace(/var\(--cds-text-primary\)/g, "#1a1a1a").replace(/ & /g, " &amp; ");
fs.writeFileSync(path.join(root, "Docs/design/round_island_map.svg"), xmlSvg);
const widgetArg = process.argv.indexOf("--widget");
if (widgetArg > 0) fs.writeFileSync(process.argv[widgetArg + 1], `export default () => ${svgBody};`);

// ---- the table ----------------------------------------------------------------------------------------------------------
console.log(`island ${(2 * R0 / 1000).toFixed(2)} km across (coast ${layout.coastRadius.min}-${layout.coastRadius.max} m from the centre), land ${layout.landKm2} km² + Hearthmoot and its Commons ${(Math.PI * COMMONS * COMMONS / 1e6).toFixed(2)} km²`);
console.log("land | path | Aaron km² | now km² | bearing | span | coast km | coast/mid/inland km² | gate road km");
for (const s of slices) {
	const road = Math.hypot(s.camps[1][0] - s.gate[0], s.camps[1][1] - s.gate[1]) / 1000;
	console.log(`${s.name} | ${s.path} | ${s.aaron} | ${(s.area / 1e6).toFixed(1)} | ${norm(s.start).toFixed(0)}-${norm(s.end).toFixed(0)} | ${(s.end - s.start).toFixed(0)} | ${(s.coastArc / 1000).toFixed(1)} | ${s.bandArea.slice().reverse().map((a) => (a / 1e6).toFixed(1)).join(" / ")} | ${road.toFixed(1)}`);
}
for (const k of ["sky", "crystal", "fungal"]) { const L = layers[k]; console.log(`${k}: ${L.from.toFixed(0)}-${L.to.toFixed(0)} deg, ${L.r1}-${L.r2} m from the centre, ${L.areaKm2} km²`); }
