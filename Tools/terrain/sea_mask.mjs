// Where the sea is: a 4 m grid over every landscape on L_Island (the Old Island, 4033 samples 1 m apart centred on the
// origin, and Storm peaks north of it, Tools/terrain/storm_peaks.mjs), "sea" where the open ocean reaches (flood-filled
// in from the edge through ground below landAbove), everything else land. Where both landscapes cover a point the higher
// one is the ground (the north-west neck lies over the island's sea floor). The sea (AWRSea) is drawn only over sea cells,
// so it never shows through land where the far terrain isn't streamed in. Writes Data/sea_mask.json, rows of run lengths
// (sea, land, sea, ...). The water you swim in (UWRWaterSubsystem) is built on the same cells, on the map named here.
// Re-run after the terrain changes:  node Tools/terrain/sea_mask.mjs
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const SIZE = 4033;              // island heightmap samples per side, 1 m apart, centred on the world origin
const HALF_M = (SIZE - 1) / 2;
const CELL = 4;                 // m per mask cell
const LAND_ABOVE = 1.5;         // m: a cell whose lowest point is above this is dry land
const raw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16'));
const islandH = (xm, ym) => {   // metres from the origin
	const i = Math.round(xm + HALF_M), j = Math.round(ym + HALF_M);
	if (i < 0 || j < 0 || i >= SIZE || j >= SIZE) return null;
	return (raw.readUInt16LE((j * SIZE + i) * 2) - 32768) / 32;
};
let storm = null, SF = null;
try {
	storm = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'storm_height.r16'));
	SF = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'storm_peaks.json'), 'utf8')).frame;
} catch { storm = null; }
const stormH = (xm, ym) => {
	if (!storm) return null;
	const i = Math.round((xm - SF.x0) / SF.step), j = Math.round((ym - SF.y0) / SF.step);
	if (i < 0 || j < 0 || i >= SF.samples || j >= SF.samples) return null;
	return (storm.readUInt16LE((j * SF.samples + i) * 2) - 32768) / SF.unitsPerMetre;
};
// The grid covers both landscapes (metres).
const MINX = storm ? Math.min(-HALF_M, SF.x0) : -HALF_M, MINY = storm ? Math.min(-HALF_M, SF.y0) : -HALF_M;
const MAXX = storm ? Math.max(HALF_M, SF.x0 + (SF.samples - 1) * SF.step) : HALF_M, MAXY = HALF_M;
const W = Math.floor((MAXX - MINX) / CELL), N = Math.floor((MAXY - MINY) / CELL);
const ground = (xm, ym) => {
	const a = islandH(xm, ym), b = stormH(xm, ym);
	return a === null ? (b === null ? -50 : b) : b === null ? a : Math.max(a, b);
};
// Lowest ground in each cell.
const low = new Float32Array(W * N);
for (let j = 0; j < N; j++) {
	for (let i = 0; i < W; i++) {
		let m = Infinity;
		for (let y = 0; y <= CELL; y += 2) for (let x = 0; x <= CELL; x += 2) m = Math.min(m, ground(MINX + i * CELL + x, MINY + j * CELL + y));
		low[j * W + i] = m;
	}
}
// Flood the ocean in from the border through low cells (inland hollows below sea level stay land).
const sea = new Uint8Array(W * N);
const queue = [];
const border = [];
for (let k = 0; k < W; k++) border.push([k, 0], [k, N - 1]);
for (let k = 0; k < N; k++) border.push([0, k], [W - 1, k]);
for (const [i, j] of border) {
	const c = j * W + i;
	if (!sea[c] && low[c] < LAND_ABOVE) { sea[c] = 1; queue.push(c); }
}
for (let q = 0; q < queue.length; q++) {
	const c = queue[q], i = c % W, j = (c / W) | 0;
	for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
		const a = i + di, b = j + dj;
		if (a < 0 || b < 0 || a >= W || b >= N) continue;
		const d = b * W + a;
		if (!sea[d] && low[d] < LAND_ABOVE) { sea[d] = 1; queue.push(d); }
	}
}
// Rows of run lengths, starting with a (possibly empty) sea run.
const rows = [];
let seaCells = 0;
for (let j = 0; j < N; j++) {
	const runs = [];
	let cur = 1, len = 0;
	for (let i = 0; i < W; i++) {
		const v = sea[j * W + i];
		seaCells += v;
		if (v === cur) len++;
		else { runs.push(len); cur = v; len = 1; }
	}
	runs.push(len);
	rows.push(runs.join(' '));
}
const out = `{
  "_comment": "The open sea on a ${CELL} m grid (Tools/terrain/sea_mask.mjs, generated: edit the script, not this file). Each row (south to north, -Y first) is run lengths west to east (-X first), alternating sea, land, sea... starting with sea. Outside the grid is all sea.",
  "map": "L_Island", "cell": ${CELL * 100}, "originX": ${MINX * 100}, "originY": ${MINY * 100}, "width": ${W}, "height": ${N},
  "rows": [
${rows.map(r => '    "' + r + '"').join(',\n')}
  ]
}
`;
fs.writeFileSync(path.join(root, 'Data', 'sea_mask.json'), out);
console.log(`sea mask ${W} x ${N}: ${(100 * seaCells / (W * N)).toFixed(1)}% sea, ${(out.length / 1024).toFixed(0)} KB`);
