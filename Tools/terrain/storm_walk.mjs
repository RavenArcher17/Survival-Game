// Grows the wildlife walk map (Data/island_walk.json, read by WRWildlife) from the Old Island's square to one that also
// covers Storm peaks (Tools/terrain/storm_peaks.mjs, cooks/build-biome-lightning.md 2.4). The island's cells are kept
// exactly as Tools/terrain/walkmap.mjs made them; the new land's cells come from its heightmap: ~ sea, x nobody (the
// boss arenas, the camps' cores), # steeper than 35 deg or a gorge wall (flyers cross it), f open flat ground (grazing),
// w ground by the shore, g ground. The grid stays square and aligned with the island's cells.
// Run after walkmap.mjs or storm_peaks.mjs:  node Tools/terrain/storm_walk.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const walkPath = path.join(root, 'Data', 'island_walk.json');
const walk = JSON.parse(fs.readFileSync(walkPath, 'utf8'));
const storm = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'storm_peaks.json'), 'utf8'));
const raw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16'));
const F = storm.frame;
const CELL = walk.cell / 100;                  // 32 m
const ISL_ORIGIN = -2016, ISL_N = 126;
// The island's own rows: either the file as walkmap.mjs wrote it, or cut back out of an extended one.
let islandRows;
if (walk.rows.length === ISL_N && walk.origin === ISL_ORIGIN * 100) islandRows = walk.rows;
else {
	const off = Math.round((ISL_ORIGIN * 100 - walk.origin) / walk.cell);
	islandRows = walk.rows.slice(off, off + ISL_N).map(r => r.slice(off, off + ISL_N));
}
const K = Math.ceil((ISL_ORIGIN - F.y0) / CELL);    // cells added north (and west) of the island's corner
const ORIGIN = ISL_ORIGIN - K * CELL;
// The Swamp (Tools/terrain/swamp.mjs) east of Storm peaks: the square grows east (and so south, it stays square) to its
// land's east edge.
let swamp = null, SW = null;
try {
	SW = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'swamp.json'), 'utf8'));
	swamp = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'swamp_height.r16'));
} catch { swamp = null; SW = null; }
const swampZone = (x, y) => {
	if (!SW) return '~';
	const Z = SW.zones, i = Math.floor((x - Z.x0) / Z.cell), j = Math.floor((y - Z.y0) / Z.cell);
	return i < 0 || j < 0 || i >= Z.width || j >= Z.height ? '~' : Z.rows[j][i];
};
const sh = (x, y) => {
	const G = SW.frame, i = Math.max(0, Math.min(G.samples - 1, Math.round((x - G.x0) / G.step))), j = Math.max(0, Math.min(G.samples - 1, Math.round((y - G.y0) / G.step)));
	return (swamp.readUInt16LE((j * G.samples + i) * 2) - 32768) / G.unitsPerMetre;
};
let eastEdge = ISL_ORIGIN + ISL_N * CELL;
if (SW) for (let j = 0; j < SW.zones.height; j++) { const r = SW.zones.rows[j]; for (let i = r.length - 1; i >= 0; i--) if (r[i] !== '~') { eastEdge = Math.max(eastEdge, SW.zones.x0 + (i + 1) * SW.zones.cell + 64); break; } }
const N = Math.max(K + ISL_N, Math.ceil((eastEdge - ORIGIN) / CELL));
const h = (x, y) => {
	const i = Math.max(0, Math.min(F.samples - 1, Math.round((x - F.x0) / F.step))), j = Math.max(0, Math.min(F.samples - 1, Math.round((y - F.y0) / F.step)));
	return (raw.readUInt16LE((j * F.samples + i) * 2) - 32768) / F.unitsPerMetre;
};
const inStorm = (x, y) => x >= F.x0 && y >= F.y0 && x <= F.x0 + (F.samples - 1) * F.step && y <= F.y0 + (F.samples - 1) * F.step;
const keepOut = [...Object.values(storm.arenas).map(a => [a.x, a.y, 90]), ...storm.sites.filter(s => s.kind === 'camp').map(s => [s.x, s.y, 40])];
const swampKeep = SW ? [[SW.arenas.vharra.x, SW.arenas.vharra.y, 90], [SW.arenas.mother_maw.x, SW.arenas.mother_maw.y, 45], [SW.arenas.hex_warden.x, SW.arenas.hex_warden.y, 40], ...SW.camps.map(c => [c.x, c.y, 40])] : [];
const rows = [];
for (let j = 0; j < N; j++) {
	let row = '';
	for (let i = 0; i < N; i++) {
		const ii = i - K, jj = j - K;
		const x = ORIGIN + (i + 0.5) * CELL, y = ORIGIN + (j + 0.5) * CELL;
		// The Swamp's land (its frozen-bog neck over the island's sea too): shallow water is wading ground for its
		// creatures ('w'), deep channels and the open sea are '~'.
		if (swamp && swampZone(x, y) !== '~') {
			if (swampKeep.some(([kx, ky, r]) => Math.hypot(kx - x, ky - y) < r)) { row += 'x'; continue; }
			let lo = Infinity, hi = -Infinity;
			for (let a = -14; a <= 14; a += 7) for (let b = -14; b <= 14; b += 7) { const v = sh(x + a, y + b); lo = Math.min(lo, v); hi = Math.max(hi, v); }
			const c = sh(x, y);
			if (c < -2.5) { row += '~'; continue; }
			const slope = (hi - lo) / 28;
			if (slope > 0.7) { row += '#'; continue; }
			if (c < 0.9) { row += 'w'; continue; }
			row += slope < 0.12 ? 'f' : 'g';
			continue;
		}
		// The island's square, and anything south of its north edge: the island's own map (sea outside it).
		if (y >= ISL_ORIGIN) { row += ii >= 0 && jj >= 0 && ii < ISL_N && jj < ISL_N ? islandRows[jj][ii] : '~'; continue; }
		if (!inStorm(x, y)) { row += '~'; continue; }
		let lo = Infinity, hi = -Infinity, sea = false;
		for (let a = -14; a <= 14; a += 7) for (let b = -14; b <= 14; b += 7) { const v = h(x + a, y + b); lo = Math.min(lo, v); hi = Math.max(hi, v); if (v < 1.5) sea = true; }
		const c = h(x, y);
		if (c < 1.5) { row += '~'; continue; }
		if (keepOut.some(([kx, ky, r]) => Math.hypot(kx - x, ky - y) < r)) { row += 'x'; continue; }
		const slope = (hi - lo) / 28;
		if (slope > 0.7) { row += '#'; continue; }
		if (sea) { row += 'w'; continue; }
		row += slope < 0.12 ? 'f' : 'g';
	}
	rows.push(row);
}
const camps = (walk.camps || []).filter(c => c.y > ISL_ORIGIN * 100);
for (const s of storm.sites.filter(s => s.kind === 'camp')) camps.push({ x: Math.round(s.x * 100), y: Math.round(s.y * 100) });
if (SW) for (const c of SW.camps) camps.push({ x: Math.round(c.x * 100), y: Math.round(c.y * 100) });
const out = {
	...walk,
	_comment: walk._comment.replace(/ Grown to cover Storm peaks.*$/, '') + ' Grown to cover Storm peaks by Tools/terrain/storm_walk.mjs (cells north of the island\'s edge come from Art/Terrain/storm_height.r16; storm_peaks creatures keep to them, island creatures to the island).',
	origin: ORIGIN * 100, camps, rows,
};
fs.writeFileSync(walkPath, JSON.stringify(out, null, 1).replace(/\n\s+(?=[\d"-])/g, ' ').replace(/"rows": \[ /, '"rows": [\n  ') + '\n');
let land = 0; for (const r of rows) for (const ch of r) if (ch !== '~') land++;
console.log(`walk map ${N} x ${N} from ${ORIGIN} m: ${land} land cells`);
