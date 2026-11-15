// Grows the wildlife walk map (Data/island_walk.json, read by WRWildlife) south over the Ancient forest
// (Tools/terrain/forest.mjs). Every cell already in the file is kept exactly; the square grows south (and so east, it
// stays square, same corner and cell size) to the forest's south edge. The forest's cells come from its heightmap the
// way storm_walk.mjs makes Storm peaks': ~ sea, x nobody (the boss arenas, the camps' cores, the Hollow Root's carved
// rooms and chasms), # steeper than 35 deg or the Spine's cliff face (flyers cross it), f open flat ground (grazing),
// w ground by the shore, g ground. North of the island's south edge only the neck is taken from the forest: cells the
// island's map has as sea where the forest's ground rises above it.
// Run after storm_walk.mjs: storm_walk rebuilds the square from the island's rows, so run this after it.
//   node Tools/terrain/forest_walk.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const walkPath = path.join(root, 'Data', 'island_walk.json');
const walk = JSON.parse(fs.readFileSync(walkPath, 'utf8'));
const forest = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'forest.json'), 'utf8'));
const raw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'forest_height.r16'));
const F = forest.frame, Z = forest.zones;
const CELL = walk.cell / 100;                  // 32 m
const ORIGIN = walk.origin / 100;              // the square's corner stays where it is
const OLD_N = walk.rows.length;
const EDGE = F.islandEdge;                     // the island's south edge (m): south of it is forest only
const SOUTH = F.y0 + (F.samples - 1) * F.step;
const N = Math.max(OLD_N, Math.ceil((SOUTH - ORIGIN) / CELL));
const h = (x, y) => {
	const i = Math.max(0, Math.min(F.samples - 1, Math.round((x - F.x0) / F.step))), j = Math.max(0, Math.min(F.samples - 1, Math.round((y - F.y0) / F.step)));
	return (raw.readUInt16LE((j * F.samples + i) * 2) - 32768) / F.unitsPerMetre;
};
const inForest = (x, y) => x >= F.x0 && y >= F.y0 && x <= F.x0 + (F.samples - 1) * F.step && y <= SOUTH;
const zone = (x, y) => {
	const i = Math.floor((x - Z.x0) / Z.cell), j = Math.floor((y - Z.y0) / Z.cell);
	return i < 0 || j < 0 || i >= Z.width || j >= Z.height ? '~' : Z.rows[j][i];
};
const A = forest.arenas;
const keepOut = [[A.sylvaroth.x, A.sylvaroth.y, 90], [A.grey_matron.x, A.grey_matron.y, 90], [A.old_gnarl.x, A.old_gnarl.y, 90],
	[A.bramblequeen.x, A.bramblequeen.y, 45], ...forest.camps.map(c => [c.x, c.y, 40])];
// The Hollow Root's rooms: a cell any part of which comes within a room's half size + 8 m of its centre.
const nearRoom = (x, y) => (forest.hollow || []).some(r => {
	const dx = Math.max(0, Math.abs(r.x - x) - CELL / 2), dy = Math.max(0, Math.abs(r.y - y) - CELL / 2);
	return Math.hypot(dx, dy) < Math.max(r.hx, r.hy) + 8;
});
// One forest cell from its heightmap (null: nothing there, keep what the map had).
const forestCell = (x, y) => {
	let lo = Infinity, hi = -Infinity, sea = false;
	for (let a = -14; a <= 14; a += 7) for (let b = -14; b <= 14; b += 7) { const v = h(x + a, y + b); lo = Math.min(lo, v); hi = Math.max(hi, v); if (v < 1.5) sea = true; }
	const c = h(x, y);
	if (c < 1.5) return '~';
	if (keepOut.some(([kx, ky, r]) => Math.hypot(kx - x, ky - y) < r)) return 'x';
	const z = zone(x, y);
	if (z === 'h' || nearRoom(x, y)) return 'x';
	if (z === 's') return '#';
	const slope = (hi - lo) / 28;
	if (slope > 0.7) return '#';
	if (sea) return 'w';
	return slope < 0.12 ? 'f' : 'g';
};
const rows = [];
let neck = 0;
for (let j = 0; j < N; j++) {
	let row = '';
	for (let i = 0; i < N; i++) {
		const x = ORIGIN + (i + 0.5) * CELL, y = ORIGIN + (j + 0.5) * CELL;
		const old = i < OLD_N && j < OLD_N ? walk.rows[j][i] : '~';
		if (!inForest(x, y)) { row += old; continue; }
		// South of the island's edge the forest is the only land; north of it only the neck over the island's sea.
		if (y >= EDGE) { row += forestCell(x, y); continue; }
		if (old === '~' && h(x, y) > 1.5) { const c = forestCell(x, y); if (c !== '~') neck++; row += c; continue; }
		row += old;
	}
	rows.push(row);
}
const camps = [...(walk.camps || [])];
for (const c of forest.camps) {
	const p = { x: Math.round(c.x * 100), y: Math.round(c.y * 100) };
	if (!camps.some(o => o.x === p.x && o.y === p.y)) camps.push(p);
}
const out = {
	...walk,
	_comment: walk._comment.replace(/ Grown south to cover the Ancient forest.*$/, '') + ' Grown south to cover the Ancient forest by Tools/terrain/forest_walk.mjs (cells south of the island\'s south edge, and its neck over the island\'s sea, come from Art/Terrain/forest_height.r16; the Hollow Root is kept clear).',
	camps, rows,
};
fs.writeFileSync(walkPath, JSON.stringify(out, null, 1).replace(/\n\s+(?=[\d"-])/g, ' ').replace(/"rows": \[ /, '"rows": [\n  ') + '\n');
let land = 0; for (const r of rows) for (const ch of r) if (ch !== '~') land++;
console.log(`walk map ${N} x ${N} from ${ORIGIN} m: ${land} land cells (${neck} on the forest's neck)`);
