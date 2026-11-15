// The Corrupted wasteland's ground in JS: a line-for-line port of FWRLandDef::BaseHeight / Height (Source/Wildreach/
// WRLand.cpp, the function land) reading Data/lands/wasteland.json, so another land's generator can meet it exactly at a
// seam (the Crystal caves at the Severance: Tools/terrain/crystal.mjs). Keep in step with WRLand.cpp.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
export const W = JSON.parse(fs.readFileSync(path.join(root, 'Data', 'lands', 'wasteland.json'), 'utf8'));
const f32 = Math.fround;
const smooth = (e0, e1, x) => { if (Math.abs(e0 - e1) < 1e-8) return x < e0 ? 0 : 1; const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;
function hashI(x, y, seed) {
	let h = (Math.imul(x | 0, 0x8da6b343) ^ Math.imul(y | 0, 0xd8163841) ^ Math.imul(seed | 0, 0xcb1ab31f)) >>> 0;
	h = (h ^ (h >>> 13)) >>> 0; h = Math.imul(h, 0x5bd1e995) >>> 0; h = (h ^ (h >>> 15)) >>> 0;
	return h;
}
const SEED = W.seed;
function noise(px, py) {
	const x0 = Math.floor(px), y0 = Math.floor(py), fx = px - x0, fy = py - y0;
	const v = (x, y) => (hashI(x, y, SEED) & 0xffff) / 32767.5 - 1;
	const sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
	return lerp(lerp(v(x0, y0), v(x0 + 1, y0), sx), lerp(v(x0, y0 + 1), v(x0 + 1, y0 + 1), sx), sy);
}
function distSeg(px, py, ax, ay, bx, by) {
	const dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
	const t = l2 > 0 ? Math.min(1, Math.max(0, ((px - ax) * dx + (py - ay) * dy) / l2)) : 0;
	return { d: Math.hypot(px - (ax + dx * t), py - (ay + dy * t)), t };
}
function distLine(px, py, L) { let b = 1e18, s = 0, tt = 0; for (let i = 0; i + 1 < L.length; i++) { const r = distSeg(px, py, L[i][0], L[i][1], L[i + 1][0], L[i + 1][1]); if (r.d < b) { b = r.d; s = i; tt = r.t; } } return { d: b, s, t: tt }; }
const inPoly = (x, y, P) => { let c = false; for (let i = 0, j = P.length - 1; i < P.length; j = i++) { const A = P[i], B = P[j]; if ((A[1] > y) !== (B[1] > y) && x < (B[0] - A[0]) * (y - A[1]) / (B[1] - A[1]) + A[0]) c = !c; } return c; };
const C = W.coast, HC = W.heightCurve, CR = W.crest;
const NS = W.noise.scale, NA = W.noise.amp, FS = W.noise.fineScale, FA = W.noise.fineAmp;
const CB = W.coastBeach, CSEA = W.coastSea, SF = W.seaFloor;
export function coastDistance(x, y) { let b = 1e18; for (let i = 0, j = C.length - 1; i < C.length; j = i++) b = Math.min(b, distSeg(x, y, C[j][0], C[j][1], C[i][0], C[i][1]).d); return inPoly(x, y, C) ? b : -b; }
export function baseHeight(x, y) {
	const D = distLine(x, y, CR).d;
	let H = HC[HC.length - 1][1];
	for (let i = 0; i + 1 < HC.length; i++) if (D <= HC[i + 1][0]) { H = lerp(HC[i][1], HC[i + 1][1], smooth(HC[i][0], HC[i + 1][0], D)); break; }
	if (D < HC[0][0]) H = HC[0][1];
	const rough = 1 + Math.min(1.5, Math.max(0, H / 150));
	H += noise(x / NS, y / NS) * NA * rough + noise(x / FS + 37.1, y / FS + 11.7) * FA;
	const S = coastDistance(x, y);
	if (S < CB) H = S >= 0 ? lerp(1.0, H, smooth(0, CB, S)) : lerp(1.0, SF, smooth(0, CSEA, -S));
	return H;
}
const FEAT = W.features.map(f => {
	const o = { kind: f.kind, id: f.id, A: f.at || f.a, B: f.b, pts: f.points, r: f.r || 0, width: f.width || 0, depth: f.depth || 0, walls: f.walls ?? 20, blend: f.blend ?? 30, lipDrop: f.lipDrop || 0 };
	if (o.kind === 'flatten') o.levels = [f.level ?? baseHeight(o.A[0], o.A[1])];
	else if (o.kind === 'road') o.levels = o.pts.map(p => baseHeight(p[0], p[1]));
	else if (o.kind === 'gorge') { const fa = baseHeight(o.A[0], o.A[1]) - o.depth; o.levels = [fa, Math.min(fa, baseHeight(o.B[0], o.B[1]) - o.depth)]; }
	else if (o.kind === 'hole') { let s = 0; for (let i = 0; i < 16; i++) { const a = 2 * Math.PI * i / 16; s += baseHeight(o.A[0] + Math.cos(a) * (o.r + 10), o.A[1] + Math.sin(a) * (o.r + 10)); } o.levels = [s / 16]; }
	return o;
});
export function height(x, y) {
	let H = baseHeight(x, y);
	for (const F of FEAT) {
		if (F.kind === 'flatten') { const d = Math.hypot(x - F.A[0], y - F.A[1]); if (d < F.r + F.blend) H = lerp(H, F.levels[0], 1 - smooth(F.r, F.r + F.blend, d)); }
		else if (F.kind === 'road') { const r = distLine(x, y, F.pts); const half = F.width / 2; if (r.d < half + F.blend) H = lerp(H, lerp(F.levels[r.s], F.levels[r.s + 1], r.t), 1 - smooth(half, half + F.blend, r.d)); }
		else if (F.kind === 'ravine') { const d = distSeg(x, y, F.A[0], F.A[1], F.B[0], F.B[1]).d; if (d < F.width / 2) H -= F.depth * (1 - smooth(F.width * 0.16, F.width / 2, d)); }
		else if (F.kind === 'gorge') { const r = distSeg(x, y, F.A[0], F.A[1], F.B[0], F.B[1]); const half = F.width / 2; if (r.d < half + F.walls) H = lerp(H, Math.min(H, lerp(F.levels[0], F.levels[1], r.t)), 1 - smooth(half, half + F.walls, r.d)); }
		else if (F.kind === 'hole') { const d = Math.hypot(x - F.A[0], y - F.A[1]); if (d < F.r + 12) H = lerp(F.levels[0] - F.lipDrop, H, smooth(F.r, F.r + 12, d)); }
	}
	return H;
}
export const gorge = FEAT.find(f => f.id === 'severance');
if (process.argv[1] && process.argv[1].endsWith('wasteland_ground.mjs')) {
	console.log('severance levels', gorge.levels.map(v => v.toFixed(1)));
	for (let x = 1100; x <= 1900; x += 50) { let r = `${x}:`; for (let y = 5300; y <= 5560; y += 20) r += ' ' + height(x, y).toFixed(0).padStart(4); console.log(r); }
}
