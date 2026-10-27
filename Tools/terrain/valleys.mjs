// Where the valley mist gathers: the island's hollows, from the heightmap. Writes Data/valley_fog.json, which the sky
// (AWRSky) turns into local fog volumes on each client (thickest at dawn). Re-run after the terrain changes:
//   node Tools/terrain/valleys.mjs
// A spot counts as a valley when the ground in a ring around it stands well above it (concavity); the deepest are
// kept, spread out. Coordinates are world cm (landscape centred on the origin, 1 m per sample, as import_island.py).
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const SIZE = 4033;
const raw = fs.readFileSync(path.join(root, 'Art', 'Terrain', 'island_height.r16'));
const height = (x, y) => {
	x = Math.max(0, Math.min(SIZE - 1, Math.round(x))); y = Math.max(0, Math.min(SIZE - 1, Math.round(y)));
	return (raw.readUInt16LE((y * SIZE + x) * 2) - 32768) / 32;
};
const RING = 150;          // m: the ring the hollow is measured against
const MIN_DEPTH = 10;      // m: how far below its ring a spot must sit
const SPACING = 380;       // m between kept spots
const MAX = 48;
const candidates = [];
for (let y = 100; y < SIZE - 100; y += 40) {
	for (let x = 100; x < SIZE - 100; x += 40) {
		const h = height(x, y);
		if (h < 1.5) continue;  // the sea and the beach have the sea's own haze
		let sum = 0;
		for (let k = 0; k < 12; k++) {
			const a = k * Math.PI / 6;
			sum += Math.max(0, height(x + Math.cos(a) * RING, y + Math.sin(a) * RING));
		}
		const depth = sum / 12 - h;
		if (depth >= MIN_DEPTH) candidates.push({ x, y, h, depth });
	}
}
candidates.sort((a, b) => b.depth - a.depth);
const kept = [];
for (const c of candidates) {
	if (kept.length >= MAX) break;
	if (kept.every(k => Math.hypot(k.x - c.x, k.y - c.y) >= SPACING)) kept.push(c);
}
const out = {
	_comment: 'Valley mist spots from Tools/terrain/valleys.mjs (generated: edit the script, not this file). x, y, z = the hollow\'s floor (world cm), radius cm, depth m below the ground around it.',
	valleys: kept.map(k => ({
		x: Math.round(k.x * 100 - 201600), y: Math.round(k.y * 100 - 201600), z: Math.round(k.h * 100),
		radius: Math.round(Math.min(30000, 15000 + k.depth * 300)), depth: Math.round(k.depth),
	})),
};
// One valley per line, so a re-run diffs readably.
const lines = out.valleys.map(v => '    ' + JSON.stringify(v));
fs.writeFileSync(path.join(root, 'Data', 'valley_fog.json'),
	`{\n  "_comment": ${JSON.stringify(out._comment)},\n  "valleys": [\n${lines.join(',\n')}\n  ]\n}\n`);
console.log(`${candidates.length} hollow samples, kept ${kept.length}; deepest ${kept[0]?.depth.toFixed(0)} m, shallowest ${kept.at(-1)?.depth.toFixed(0)} m`);
