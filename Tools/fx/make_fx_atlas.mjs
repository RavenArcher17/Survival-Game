// Paints the effects sprite atlas (Content/FX/T_FxAtlas): 4 x 4 cells of 256 px, each a sprite shape the particle
// system in WRFx.cpp picks by name (Data/fx.json "sprite"). Everything is drawn from maths and noise, so nothing is
// downloaded and the atlas can be remade at any size. Colours are linear (the texture is imported without sRGB).
//   node Tools/fx/make_fx_atlas.mjs [out.png] [cellPx]
// Then Tools/fx/make_fx_assets.py imports it and builds the two effect materials.
import fs from 'fs';
import path from 'path';
import zlib from 'zlib';

const out = process.argv[2] || path.join(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1')), '..', '..', 'Saved', 'FX', 'fx_atlas.png');
const CELL = Number(process.argv[3] || 256);
const GRID = 4;
const SIZE = CELL * GRID;
const SS = 3; // supersamples per axis: hard-edged chips get smooth edges

// ---------------------------------------------------------------- noise
function hash(x, y, seed) {
	let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0;
	h = (h ^ (h >>> 13)) * 1274126177 | 0;
	return ((h ^ (h >>> 16)) >>> 0) / 4294967295;
}
function smooth(t) { return t * t * (3 - 2 * t); }
function vnoise(x, y, seed) {
	const xi = Math.floor(x), yi = Math.floor(y);
	const tx = smooth(x - xi), ty = smooth(y - yi);
	const a = hash(xi, yi, seed), b = hash(xi + 1, yi, seed), c = hash(xi, yi + 1, seed), d = hash(xi + 1, yi + 1, seed);
	return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}
function fbm(x, y, seed, oct = 4) {
	let s = 0, amp = 0.5, f = 1, norm = 0;
	for (let i = 0; i < oct; i++) { s += amp * vnoise(x * f, y * f, seed + i * 17); norm += amp; amp *= 0.5; f *= 2.03; }
	return s / norm;
}
const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const sstep = (a, b, v) => { const t = clamp((v - a) / (b - a)); return t * t * (3 - 2 * t); };
const mix = (a, b, t) => a + (b - a) * t;
// Shapes keep clear of the cell edge so mip maps don't bleed one sprite into the next.
const edge = (r) => 1 - sstep(0.86, 0.97, r);

// Each cell: (x, y) in -1..1 with y = -1 at the top of the sprite (world up on a billboard); returns [r, g, b, a].
const cells = [
	// 0 dot: soft round puff of light (sparks' glow, magic motes, snowflakes far away)
	(x, y) => { const r = Math.hypot(x, y); return [1, 1, 1, Math.exp(-r * r * 4.5) * edge(r)]; },
	// 1 spark: a hot pin-point with a faint halo; stretched along its velocity it becomes a streak
	(x, y) => { const r = Math.hypot(x, y); return [1, 1, 1, clamp(Math.exp(-r * r * 18) + 0.3 * Math.exp(-r * r * 3.5)) * edge(r)]; },
	// 2 smoke: a lumpy puff, lighter on top so lit smoke reads as a volume
	(x, y) => smokeCell(x, y, 11, 0.5),
	// 3 smoke2: a more broken puff for dust
	(x, y) => smokeCell(x, y, 29, 0.85),
	// 4 flame: a tongue of fire, white-yellow core to red edges (coloured: tint it white)
	(x, y) => flameCell(x, y, 5, 1.0),
	// 5 flame2: a split, flickery tongue
	(x, y) => flameCell(x, y, 41, 1.35),
	// 6 flare: a four-point star (parry flash, crits, the glint on ice)
	(x, y) => {
		const r = Math.hypot(x, y);
		const a = Math.exp(-r * r * 9) + 0.75 * Math.exp(-(x * x * 90 + y * y * 1.6)) + 0.75 * Math.exp(-(y * y * 90 + x * x * 1.6))
			+ 0.25 * Math.exp(-((x - y) ** 2 * 60 + (x + y) ** 2 * 2.5)) + 0.25 * Math.exp(-((x + y) ** 2 * 60 + (x - y) ** 2 * 2.5));
		return [1, 1, 1, clamp(a) * edge(r)];
	},
	// 7 ring: a soft shock ring (frost nova, parry, landing dust from above)
	(x, y) => { const r = Math.hypot(x, y); return [1, 1, 1, Math.exp(-(((r - 0.72) / 0.1) ** 2)) * (0.75 + 0.25 * fbm(x * 4, y * 4, 7)) * edge(r)]; },
	// 8 chip_wood: a splinter with grain along its length
	(x, y) => {
		const c = Math.cos(0.45), s = Math.sin(0.45);
		const u = x * c - y * s, v = x * s + y * c;
		const taper = 1 - Math.abs(u) * 0.55;
		const half = 0.2 * taper + 0.05 * (fbm(u * 3, 0, 3) - 0.5);
		const inside = Math.abs(u) < 0.82 - 0.08 * fbm(v * 9, 1, 5) && Math.abs(v) < half;
		const grain = 0.78 + 0.16 * Math.sin(v * 70 + fbm(u * 2, v * 4, 9) * 7) + 0.1 * (fbm(u * 6, v * 20, 2) - 0.5);
		return inside ? [grain, grain * 0.97, grain * 0.93, 1] : [grain, grain, grain, 0];
	},
	// 9 chip_stone: a jagged pebble with lit and shaded facets
	(x, y) => {
		const ang = Math.atan2(y, x), r = Math.hypot(x, y);
		const n = 7, k = ((ang / (Math.PI * 2)) * n + n) % n, i = Math.floor(k), f = k - i;
		const rad = (j) => 0.55 + 0.3 * hash(j % n, 3, 91);
		const lim = mix(rad(i), rad(i + 1), f);
		const shade = 0.55 + 0.4 * hash(i, 5, 13) - 0.15 * (y + 1) * 0.5 + 0.12 * (fbm(x * 12, y * 12, 4) - 0.5);
		return r < lim * 0.92 ? [shade, shade, shade, 1] : [shade, shade, shade, 0];
	},
	// 10 leaf: a fibre blade / leaf bit with a darker midrib
	(x, y) => {
		const w = 0.34 * Math.sqrt(Math.max(0, 1 - (y / 0.85) ** 2)) * (1 + 0.15 * Math.sin(y * 9));
		const inside = Math.abs(x + 0.08 * Math.sin(y * 2.5)) < w;
		const tone = 0.82 - 0.25 * Math.exp(-((x + 0.08 * Math.sin(y * 2.5)) ** 2) * 900) + 0.1 * (fbm(x * 8, y * 8, 8) - 0.5);
		return [tone, tone, tone, inside ? 1 : 0];
	},
	// 11 shard: a long ice splinter with bright edges and a centre ridge
	(x, y) => {
		const d = Math.abs(x) / 0.26 + Math.abs(y) / 0.9;
		const ridge = Math.exp(-(x * x) * 600) * 0.35;
		const rim = sstep(0.7, 0.98, d) * 0.45;
		const t = clamp(0.55 + ridge + rim + 0.1 * fbm(x * 10, y * 4, 6));
		return [t, t, t, d < 1 ? 0.88 : 0];
	},
	// 12 flake: a small ragged flake (snow up close, ash)
	(x, y) => {
		const r = Math.hypot(x, y) * (1 + 0.45 * (fbm(x * 3 + 4, y * 3, 21) - 0.5));
		return [1, 1, 1, (1 - sstep(0.42, 0.72, r)) * edge(Math.hypot(x, y))];
	},
	// 13 droplet: a round drop with a highlight
	(x, y) => {
		const r = Math.hypot(x, y * 0.92);
		const hi = Math.exp(-(((x + 0.22) ** 2 + (y + 0.22) ** 2) * 30));
		const t = clamp(0.55 + 0.45 * hi - 0.15 * r);
		return [t, t, t, 1 - sstep(0.5, 0.6, r)];
	},
	// 14 glow: a wide soft halo (bolt cores, Hearth glow, light flashes)
	(x, y) => { const r = Math.hypot(x, y); return [1, 1, 1, clamp(Math.exp(-r * r * 2.4) * 0.85 + Math.exp(-r * r * 12) * 0.15) * edge(r)]; },
	// 15 wisp: a streak of mist (frost trails, the nova's breath, heat haze over lava)
	(x, y) => {
		const a = Math.exp(-((y * 2.4) ** 2)) * (1 - sstep(0.55, 0.95, Math.abs(x))) * (0.35 + 0.65 * fbm(x * 2.2 + 3, y * 5, 33));
		return [1, 1, 1, clamp(a * 1.25) * edge(Math.hypot(x, y))];
	},
];

function smokeCell(x, y, seed, broken) {
	const n = fbm(x * 2.2 + 10, y * 2.2, seed);
	const r = Math.hypot(x, y) * (1 + broken * 0.6 * (n - 0.5));
	const body = 1 - sstep(0.35, 0.86, r);
	const detail = 0.55 + 0.45 * fbm(x * 5, y * 5, seed + 3);
	const light = clamp(0.62 + 0.3 * (-y) * 0.5 + 0.25 * (fbm(x * 3, y * 3, seed + 9) - 0.5) + 0.12);
	return [light, light, light, clamp(body * detail * 1.15) * edge(Math.hypot(x, y))];
}

function flameCell(x, y, seed, wiggle) {
	// Height 0 at the top, 1 at the base; the tongue is widest a little above the base and pinches to a tip.
	const h = (y + 1) * 0.5;
	const sway = 0.18 * wiggle * (fbm(h * 2.5, 1, seed) - 0.5) * (1 - h);
	// teardrop: a pointed, slightly concave tip widening to a round base
	const width = h < 0.72 ? 0.6 * Math.pow(Math.max(0, h) / 0.72, 1.25) + 0.015 : 0.6 * Math.sqrt(Math.max(0, 1 - ((h - 0.72) / 0.26) ** 2)) + 0.015;
	const dx = (x - sway) / width;
	const n = fbm(x * 3 + 2, y * 4 - h * 3, seed + 4);
	const shape = (1 - sstep(0.5, 1.0, Math.abs(dx) + (n - 0.5) * 0.7 * wiggle * (1.2 - h))) * sstep(0.0, 0.12, h);
	const heat = clamp((1 - Math.abs(dx)) * 0.95 + h * 0.35 - 0.12);
	// white-yellow core -> orange -> deep red at the edges (linear)
	const r = 1, g = clamp(0.18 + 0.72 * Math.pow(heat, 1.8)), b = clamp(0.02 + 0.38 * Math.pow(heat, 4));
	return [r, g, b, clamp(shape * (0.6 + 0.6 * n))];
}

// ---------------------------------------------------------------- paint
const px = Buffer.alloc(SIZE * SIZE * 4);
for (let c = 0; c < cells.length; c++) {
	const ox = (c % GRID) * CELL, oy = Math.floor(c / GRID) * CELL;
	for (let j = 0; j < CELL; j++) {
		for (let i = 0; i < CELL; i++) {
			let r = 0, g = 0, b = 0, a = 0;
			for (let sy = 0; sy < SS; sy++) {
				for (let sx = 0; sx < SS; sx++) {
					const x = ((i + (sx + 0.5) / SS) / CELL) * 2 - 1;
					const y = ((j + (sy + 0.5) / SS) / CELL) * 2 - 1;
					const v = cells[c](x, y);
					// colour weighted by coverage, so edges don't pick up the transparent pixels' colour
					r += v[0] * v[3]; g += v[1] * v[3]; b += v[2] * v[3]; a += v[3];
				}
			}
			const n = SS * SS;
			const k = ((oy + j) * SIZE + ox + i) * 4;
			const inv = a > 1e-6 ? 1 / a : 0;
			const solo = cells[c](((i + 0.5) / CELL) * 2 - 1, ((j + 0.5) / CELL) * 2 - 1);
			px[k] = Math.round(clamp(a > 1e-6 ? r * inv : solo[0]) * 255);
			px[k + 1] = Math.round(clamp(a > 1e-6 ? g * inv : solo[1]) * 255);
			px[k + 2] = Math.round(clamp(a > 1e-6 ? b * inv : solo[2]) * 255);
			px[k + 3] = Math.round(clamp(a / n) * 255);
		}
	}
}

// ---------------------------------------------------------------- PNG
const crcTable = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
function crc32(buf) { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; }
function chunk(type, data) {
	const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
	const td = Buffer.concat([Buffer.from(type, 'ascii'), data]);
	const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
	return Buffer.concat([len, td, crc]);
}
const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(SIZE, 0); ihdr.writeUInt32BE(SIZE, 4); ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
const raw = Buffer.alloc((SIZE * 4 + 1) * SIZE);
for (let y = 0; y < SIZE; y++) { raw[y * (SIZE * 4 + 1)] = 0; px.copy(raw, y * (SIZE * 4 + 1) + 1, y * SIZE * 4, (y + 1) * SIZE * 4); }
const png = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, png);
console.log(`wrote ${out} (${SIZE}x${SIZE}, ${(png.length / 1024).toFixed(0)} KB)`);

// A preview over a dark backdrop (alpha made visible), for checking the shapes by eye.
if (process.env.FX_ATLAS_PREVIEW) {
	const prev = Buffer.alloc(raw.length);
	for (let y = 0; y < SIZE; y++) {
		prev[y * (SIZE * 4 + 1)] = 0;
		for (let x = 0; x < SIZE; x++) {
			const k = (y * SIZE + x) * 4, o = y * (SIZE * 4 + 1) + 1 + x * 4;
			const a = px[k + 3] / 255, bg = ((x >> 4) + (y >> 4)) & 1 ? 40 : 25;
			for (let c = 0; c < 3; c++) prev[o + c] = Math.round(px[k + c] * a + bg * (1 - a));
			prev[o + 3] = 255;
		}
	}
	const p = out.replace(/\.png$/, '_preview.png');
	fs.writeFileSync(p, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(prev)), chunk('IEND', Buffer.alloc(0))]));
	console.log(`wrote ${p}`);
}
