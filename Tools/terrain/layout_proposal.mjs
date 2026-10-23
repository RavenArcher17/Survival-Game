// M3 step 1: first-playable landmass proposal for Raven's sign-off (design: Map tab).
// Generates a 4x4 km heightfield at 4 m/px, renders a shaded, annotated preview PNG and a JSON summary.
// No terrain is imported into Unreal until the layout is approved.
//   node Tools/terrain/layout_proposal.mjs [outDir] [seed] [--export path/to/height.r16]
// --export also writes the full-resolution heightmap: 4033 x 4033 samples at 1 m, 16-bit little-endian,
// value = 32768 + metres * 32 (import with landscape Z scale 400; see Tools/terrain/import_island.py).
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';

const outDir = process.argv[2] ?? 'Docs/terrain';
const seed = +(process.argv[3] ?? 7);
const exportIdx = process.argv.indexOf('--export');
const exportPath = exportIdx > 0 ? process.argv[exportIdx + 1] : null;
export const HEIGHT_UNITS_PER_METRE = 32;
const SIZE = 4032;            // metres
const RES = 1008;             // pixels (4 m/px); final export is 4033^2 at 1 m after sign-off
const M = SIZE / RES;

// ---------- noise ----------
const hash = (x, y) => { let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0; h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967295; };
const smooth = t => t * t * (3 - 2 * t);
function vnoise(x, y) {
  const xi = Math.floor(x), yi = Math.floor(y), xf = smooth(x - xi), yf = smooth(y - yi);
  const a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
  return (a + (b - a) * xf) + ((c + (d - c) * xf) - (a + (b - a) * xf)) * yf;
}
const fbm = (x, y, oct = 5) => { let s = 0, a = 0.5, f = 1, n = 0; for (let i = 0; i < oct; i++) { s += a * (vnoise(x * f, y * f) * 2 - 1); n += a; a *= 0.5; f *= 2.03; } return s / n; };
const sstep = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };

// ---------- layout features (metres) ----------
const ridgeX = y => SIZE / 2 + 170 * fbm(y / 900, 3.1, 3);
const PASSES = [{ name: 'North pass', y: 720 }, { name: 'South pass', y: 3310 }];
const VOLCANO = { x: 1230, y: 2016 }, GLACIER = { x: 2820, y: 2016 };
const COAST_IN = 380;         // land starts this far from the map edge (with noise)
// Volcanic side: one water river (north) and one lava river (south), decided 2026-10-23. Tundra rivers are icy water.
const RIVERS = [
  { kind: 'water', pts: [[1760, 1250], [1350, 1150], [900, 1320], [450, 1150], [80, 1230]] },
  { kind: 'lava', pts: [[1700, 2900], [1250, 3050], [700, 2850], [80, 2950]] },
  { kind: 'ice', pts: [[2300, 1300], [2800, 1150], [3300, 1350], [3950, 1250]] },
  { kind: 'ice', pts: [[2330, 2850], [2900, 3000], [3450, 2800], [3950, 2900]] },
];
const SPAWN_X = 520;
// Middle spawns moved diagonally toward a pass (west toward north, east toward south) so both passes
// get traffic, while staying in the outer lowlands and clear of the rivers.
const SPAWNS = [];
for (const [wy, ey] of [[720, 720], [1500, 2530], [3310, 3310]]) {
  SPAWNS.push({ biome: 'volcanic', x: SPAWN_X, y: wy });
  SPAWNS.push({ biome: 'tundra', x: SIZE - SPAWN_X, y: ey });
}

function distToPolyline(px, py, pts) {
  let best = 1e9;
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1];
    const dx = bx - ax, dy = by - ay, t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
    best = Math.min(best, Math.hypot(px - (ax + t * dx), py - (ay + t * dy)));
  }
  return best;
}

function heightAt(x, y) {
  // Rounded-square island (superellipse) with large bays and headlands.
  const dx = Math.abs(x - SIZE / 2), dy = Math.abs(y - SIZE / 2), P = 3.2;
  const rho = (dx ** P + dy ** P) ** (1 / P);
  const edge = SIZE / 2 - rho + 230 * fbm(x / 750, y / 750, 4) + 70 * fbm(x / 180, y / 180, 3);
  const land = sstep(COAST_IN - 220, COAST_IN + 160, edge);
  if (land <= 0.001) return { h: -12, land: 0 };

  const rx = ridgeX(y), r = Math.abs(x - rx);
  let saddle = 1;
  for (const p of PASSES) saddle = Math.min(saddle, 1 - 0.9 * Math.exp(-(((y - p.y) / 230) ** 2)));
  const ridge = (640 * Math.exp(-((r / 270) ** 2)) + 90 * Math.max(0, fbm(x / 160, y / 160, 4)) * Math.exp(-((r / 400) ** 2))) * saddle;
  const rise = 140 * Math.max(0, 1 - r / 1900);
  const hills = 55 * fbm(x / 650, y / 650, 5) + 18 * fbm(x / 120, y / 120, 3);

  const dv = Math.hypot(x - VOLCANO.x, y - VOLCANO.y);
  const volcano = 430 * Math.max(0, 1 - dv / 720) ** 1.6 - 150 * Math.exp(-((dv / 120) ** 2));
  const dg = Math.hypot(x - GLACIER.x, y - GLACIER.y);
  const glacier = 190 * sstep(1050, 380, dg) + 10 * fbm(x / 90, y / 90, 2);

  let h = 18 + hills + rise + ridge + (x < rx ? volcano : glacier);
  // Rivers meander: sample the polyline through a warped position.
  let river = 1e9, riverKind = null;
  const wx = x + 110 * fbm(x / 260, y / 260 + 9, 3), wy = y + 110 * fbm(x / 260 + 4, y / 260, 3);
  for (const rv of RIVERS) {
    const d = distToPolyline(wx, wy, rv.pts);
    if (d < river) { river = d; riverKind = rv.kind; }
  }
  // Wide, shallow, fordable channel: 6 m deep with ~90 m gentle banks (a deep trench trapped the walk test).
  if (river < 90) h -= 6 * (1 - river / 90) ** 2;
  return { h: h * land - 12 * (1 - land), land, r, rx, river, riverKind, dv, dg };
}

// ---------- generate ----------
const H = new Float32Array(RES * RES);
const info = new Array(RES * RES);
for (let j = 0; j < RES; j++) for (let i = 0; i < RES; i++) {
  const s = heightAt((i + 0.5) * M, (j + 0.5) * M);
  H[j * RES + i] = s.h; info[j * RES + i] = s;
}
// Spawns sit on the outer lowlands: scan in from each edge to solid land, then 150 m further.
for (const s of SPAWNS) {
  const j = Math.round(s.y / M);
  const west = s.biome === 'volcanic';
  for (let step = 0; step < RES / 2; step++) {
    const i = west ? step : RES - 1 - step;
    if (info[j * RES + i].land > 0.9) { s.x = (i + 0.5) * M + (west ? 150 : -150); break; }
  }
}

const slopeAt = (i, j) => {
  const g = (a, b) => H[Math.min(RES - 1, Math.max(0, b)) * RES + Math.min(RES - 1, Math.max(0, a))];
  return Math.hypot(g(i + 1, j) - g(i - 1, j), g(i, j + 1) - g(i, j - 1)) / (2 * M);
};

// Walkability check: slope <= 0.7 (35 deg) counts as walkable. Find the ridge crossing points.
const crossings = [];
for (let j = 0; j < RES; j += 2) {
  const y = (j + 0.5) * M, ci = Math.round(ridgeX(y) / M);
  let ok = true;
  for (let i = ci - 60; i <= ci + 60; i++) if (slopeAt(i, j) > 0.7) { ok = false; break; }
  if (ok && info[j * RES + ci].land > 0.5) crossings.push(Math.round(y));
}

// ---------- render ----------
const img = Buffer.alloc(RES * RES * 3);
const mix = (a, b, t) => a.map((v, k) => v + (b[k] - v) * t);
for (let j = 0; j < RES; j++) for (let i = 0; i < RES; i++) {
  const k = j * RES + i, s = info[k], h = H[k];
  let c;
  if (s.land < 0.2) c = mix([28, 70, 120], [60, 110, 150], Math.max(0, s.land * 5));
  else {
    const west = (i + 0.5) * M < s.rx;
    const zone = s.r < 600 ? 2 : s.r < 1300 ? 1 : 0;
    const vol = [[96, 86, 78], [74, 62, 56], [52, 40, 38]], tun = [[214, 222, 228], [190, 204, 216], [168, 186, 204]];
    c = (west ? vol : tun)[zone];
    if (west && s.dv < 150) c = [210, 90, 30];                  // crater / lava
    if (!west && s.dg < 520) c = mix(c, [200, 235, 250], 0.6);  // glacier
    if (slopeAt(i, j) > 0.7) c = mix(c, [120, 116, 112], 0.7);  // cliff / impassable rock
    if (s.river < 14) c = s.riverKind === 'lava' ? [220, 80, 25] : s.riverKind === 'water' ? [70, 130, 190] : [120, 170, 210];
  }
  // Hillshade (light from the north-west).
  const dzdx = (H[j * RES + Math.min(RES - 1, i + 1)] - H[j * RES + Math.max(0, i - 1)]) / (2 * M);
  const dzdy = (H[Math.min(RES - 1, j + 1) * RES + i] - H[Math.max(0, j - 1) * RES + i]) / (2 * M);
  const shade = Math.max(0.35, Math.min(1.25, 0.95 + (-dzdx * 0.7 - dzdy * 0.7) * 0.9));
  img[k * 3] = Math.min(255, c[0] * shade); img[k * 3 + 1] = Math.min(255, c[1] * shade); img[k * 3 + 2] = Math.min(255, c[2] * shade);
}
const dot = (x, y, r, col) => {
  const ci = Math.round(x / M), cj = Math.round(y / M);
  for (let j = cj - r; j <= cj + r; j++) for (let i = ci - r; i <= ci + r; i++)
    if (i >= 0 && j >= 0 && i < RES && j < RES && (i - ci) ** 2 + (j - cj) ** 2 <= r * r) img.set(col, (j * RES + i) * 3);
};
const ring = (x, y, r, col) => { for (let a = 0; a < 360; a += 1) dot(x + Math.cos(a * Math.PI / 180) * r * M, y + Math.sin(a * Math.PI / 180) * r * M, 1, col); };
for (const s of SPAWNS) { dot(s.x, s.y, 9, [255, 255, 255]); dot(s.x, s.y, 6, [230, 40, 40]); }
for (const p of PASSES) ring(ridgeX(p.y), p.y, 22, [255, 220, 40]);
// Boss arenas (highlands, against the ridge) and mini-boss sites (midlands): proposals.
const BOSSES = [{ name: 'Vulkarn (forge cavern)', x: 1230, y: 2016, biome: 'volcanic' }, { name: 'Skaldrith (mountain bowl)', x: 2820, y: 2016, biome: 'tundra' }];
const MINIS = [{ name: 'Obsidian Warden', x: 950, y: 1300 }, { name: 'Ignessa', x: 980, y: 2800 }, { name: 'Grimjaw', x: 3080, y: 1250 }, { name: 'Pale Hunter', x: 3050, y: 2850 }];
for (const b of BOSSES) { ring(b.x, b.y, 30, [170, 60, 220]); ring(b.x, b.y, 28, [170, 60, 220]); }
for (const m of MINIS) { dot(m.x, m.y, 8, [20, 20, 20]); dot(m.x, m.y, 5, [255, 150, 30]); }

function png(w, h, rgb) {
  const crcTable = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
  const crc = buf => { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3); }
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}

fs.mkdirSync(outDir, { recursive: true });
fs.writeFileSync(path.join(outDir, 'layout_proposal.png'), png(RES, RES, img));

// ---------- summary ----------
const WALK = 4.5; // m/s (walk speed 450 cm/s)
const spawnInfo = SPAWNS.map(s => {
  const d = Math.min(...PASSES.map(p => Math.hypot(ridgeX(p.y) - s.x, p.y - s.y)));
  return { ...s, nearestPassMetres: Math.round(d), walkMinutes: +(d / WALK / 60).toFixed(1) };
});
let maxH = -1e9, landPx = 0;
for (let k = 0; k < H.length; k++) { maxH = Math.max(maxH, H[k]); if (info[k].land > 0.5) landPx++; }
const summary = {
  seed, sizeMetres: SIZE, previewMetresPerPixel: M,
  landAreaKm2: +(landPx * M * M / 1e6).toFixed(2),
  highestPointMetres: Math.round(maxH),
  passes: PASSES.map(p => ({ ...p, x: Math.round(ridgeX(p.y)) })),
  walkableRidgeCrossingsY: [...new Set(crossings.map(y => Math.round(y / 100) * 100))],
  spawns: spawnInfo,
  bosses: BOSSES, miniBosses: MINIS,
};
fs.writeFileSync(path.join(outDir, 'layout_proposal.json'), JSON.stringify(summary, null, 2) + '\n');

if (exportPath) {
  // Full resolution: one sample per metre, same height function as the preview.
  const N = SIZE + 1;
  const out = Buffer.alloc(N * N * 2);
  const t0 = Date.now();
  for (let j = 0; j < N; j++) {
    for (let i = 0; i < N; i++) {
      const v = Math.round(32768 + heightAt(i, j).h * HEIGHT_UNITS_PER_METRE);
      out.writeUInt16LE(Math.max(0, Math.min(65535, v)), (j * N + i) * 2);
    }
    if (j % 500 === 0) console.error(`export row ${j}/${N} (${((Date.now() - t0) / 1000).toFixed(0)} s)`);
  }
  fs.mkdirSync(path.dirname(exportPath), { recursive: true });
  fs.writeFileSync(exportPath, out);
  console.error(`wrote ${exportPath} (${N}x${N}) in ${((Date.now() - t0) / 1000).toFixed(0)} s`);
}
console.log(JSON.stringify({ land: summary.landAreaKm2, highest: summary.highestPointMetres, crossings: summary.walkableRidgeCrossingsY, walk: spawnInfo.map(s => `${s.biome}@${s.y}: ${s.walkMinutes} min`) }, null, 1));
