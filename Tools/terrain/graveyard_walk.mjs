// Grows the wildlife walk map (Data/island_walk.json, read by WRWildlife) over the Graveyard (Tools/terrain/graveyard.mjs).
// Every cell outside the Graveyard's own ground is kept exactly (the island, the forest, the sea); the square already
// reaches over the Graveyard (it grew south with the forest). The Graveyard's cells come from its heightfield the way
// forest_walk.mjs makes the forest's: ~ sea, x nobody (the chapel and Belfry arenas, the camps' cores, the crypt mouths
// and holes), # steeper than 35 deg or the Spine's face, f open flat ground (grazing), w ground by the shore, g ground.
// Only cells where the Graveyard's own ground shows (not hidden under the island's or the forest's).
// Run after forest_walk.mjs:   node Tools/terrain/graveyard_walk.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const walkPath = path.join(root, 'Data', 'island_walk.json');
const walk = JSON.parse(fs.readFileSync(walkPath, 'utf8'));
const land = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'lands', 'graveyard.json'), 'utf8'));
const content = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'graveyard.json'), 'utf8'));
const H = land.heightfield, G = land.zoneGrid;
const bin = fs.readFileSync(path.join(root, 'Data', 'lands', H.file));
const NS = H.samples;
const CELL = walk.cell / 100, ORIGIN = walk.origin / 100, N = walk.rows.length;
const at = (x, y) => {
	const i = Math.round((x - H.x0) / H.step), j = Math.round((y - H.y0) / H.step);
	if (i < 0 || j < 0 || i >= NS || j >= NS) return null;
	const k = j * NS + i;
	return { h: (bin.readUInt16LE(k * 2) - H.offset) / H.unitsPerMetre, hidden: bin[NS * NS * 2 + k] === 255 };
};
const zone = (x, y) => {
	const i = Math.floor((x - G.x0) / G.cell), j = Math.floor((y - G.y0) / G.cell);
	return i < 0 || j < 0 || i >= G.width || j >= G.height ? '~' : G.rows[j][i];
};
const keepOut = [
	...content.sites.filter(s => s.arena).map(s => [s.x, s.y, s.kind === 'chapel' ? 70 : 32]),
	...content.sites.filter(s => s.camp).map(s => [s.x, s.y, 40]),
	...content.crypts.map(c => [c.x, c.y, 14]), ...content.holes.map(h => [h.x, h.y, 12]),
];
const cell = (x, y) => {
	const c = at(x, y);
	if (!c || c.hidden) return null;
	if (c.h < 1.5) return '~';
	if (keepOut.some(([kx, ky, r]) => Math.hypot(kx - x, ky - y) < r)) return 'x';
	const z = zone(x, y);
	if (z === 's' || z === '~') return z === 's' ? '#' : '~';
	let lo = Infinity, hi = -Infinity, sea = false;
	for (let a = -14; a <= 14; a += 7) for (let b = -14; b <= 14; b += 7) { const q = at(x + a, y + b); if (!q) continue; lo = Math.min(lo, q.h); hi = Math.max(hi, q.h); if (q.h < 1.5) sea = true; }
	const slope = (hi - lo) / 28;
	if (slope > 0.7) return '#';
	if (sea) return 'w';
	return slope < 0.12 ? 'f' : 'g';
};
const rows = [];
let made = 0;
for (let j = 0; j < N; j++) {
	let r = '';
	for (let i = 0; i < N; i++) {
		const x = ORIGIN + (i + 0.5) * CELL, y = ORIGIN + (j + 0.5) * CELL;
		const old = walk.rows[j][i];
		// Only where the island's map has sea (the Graveyard never takes a cell the island or the forest own).
		if (old !== '~') { r += old; continue; }
		const c = cell(x, y);
		if (c && c !== '~') made++;
		r += c ?? old;
	}
	rows.push(r);
}
const camps = [...(walk.camps || [])];
for (const s of content.sites.filter(s => s.camp)) {
	const p = { x: Math.round(s.x * 100), y: Math.round(s.y * 100) };
	if (!camps.some(o => o.x === p.x && o.y === p.y)) camps.push(p);
}
const out = { ...walk, _comment: walk._comment.replace(/ Grown over the Graveyard.*$/, '') + ' Grown over the Graveyard by Tools/terrain/graveyard_walk.mjs (its cells from Data/lands/graveyard_ground.bin where the island\'s map had sea).', camps, rows };
fs.writeFileSync(walkPath, JSON.stringify(out, null, 1).replace(/\n\s+(?=[\d"-])/g, ' ').replace(/"rows": \[ /, '"rows": [\n  ') + '\n');
console.log(`walk map: ${made} Graveyard land cells`);
