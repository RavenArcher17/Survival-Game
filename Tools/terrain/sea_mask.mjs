// Where the sea is: a 4 m grid over the island's heightmap, "sea" where the open ocean reaches (flood-filled in from the
// edge through ground below landAbove), everything else land. The sea (AWRSea) is drawn only over sea cells, so it never
// shows through the middle of the island where the far terrain isn't streamed in. Writes Data/sea_mask.json, rows of
// run lengths (sea, land, sea, ...). Re-run after the terrain changes:  node Tools/terrain/sea_mask.mjs
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const SIZE = 4033;              // heightmap samples per side, 1 m apart, centred on the world origin
const CELL = 4;                 // m per mask cell
const LAND_ABOVE = 1.5;         // m: a cell whose lowest point is above this is dry land
const N = Math.floor((SIZE - 1) / CELL);  // 1008
const raw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16'));
const height = (x, y) => (raw.readUInt16LE((y * SIZE + x) * 2) - 32768) / 32;

// Lowest ground in each cell.
const low = new Float32Array(N * N);
for (let j = 0; j < N; j++) {
	for (let i = 0; i < N; i++) {
		let m = Infinity;
		for (let y = j * CELL; y <= j * CELL + CELL; y++) for (let x = i * CELL; x <= i * CELL + CELL; x++) m = Math.min(m, height(x, y));
		low[j * N + i] = m;
	}
}
// Flood the ocean in from the border through low cells (inland hollows below sea level stay land).
const sea = new Uint8Array(N * N);
const queue = [];
for (let k = 0; k < N; k++) for (const [i, j] of [[k, 0], [k, N - 1], [0, k], [N - 1, k]]) {
	const c = j * N + i;
	if (!sea[c] && low[c] < LAND_ABOVE) { sea[c] = 1; queue.push(c); }
}
for (let q = 0; q < queue.length; q++) {
	const c = queue[q], i = c % N, j = (c / N) | 0;
	for (const [di, dj] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
		const a = i + di, b = j + dj;
		if (a < 0 || b < 0 || a >= N || b >= N) continue;
		const d = b * N + a;
		if (!sea[d] && low[d] < LAND_ABOVE) { sea[d] = 1; queue.push(d); }
	}
}
// Rows of run lengths, starting with a (possibly empty) sea run.
const rows = [];
let seaCells = 0;
for (let j = 0; j < N; j++) {
	const runs = [];
	let cur = 1, len = 0;
	for (let i = 0; i < N; i++) {
		const v = sea[j * N + i];
		seaCells += v;
		if (v === cur) len++;
		else { runs.push(len); cur = v; len = 1; }
	}
	runs.push(len);
	rows.push(runs.join(' '));
}
const half = (SIZE - 1) * 100 / 2;
const out = `{
  "_comment": "The open sea on a ${CELL} m grid (Tools/terrain/sea_mask.mjs, generated: edit the script, not this file). Each row (south to north, -Y first) is run lengths west to east (-X first), alternating sea, land, sea... starting with sea. Outside the grid is all sea.",
  "cell": ${CELL * 100}, "originX": ${-half}, "originY": ${-half}, "width": ${N}, "height": ${N},
  "rows": [
${rows.map(r => '    "' + r + '"').join(',\n')}
  ]
}
`;
fs.writeFileSync(path.join(root, 'Data', 'sea_mask.json'), out);
console.log(`sea mask ${N} x ${N}: ${(100 * seaCells / (N * N)).toFixed(1)}% sea, ${(out.length / 1024).toFixed(0)} KB`);
