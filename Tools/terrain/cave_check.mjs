// Can a survivor walk every passage of a cave, both ways? Each passage is probed along its centre line through the cave's
// air with the game's own rock noise (WRLandCaves.cpp LcAir: its hash value noise, ported here; change one, change the
// other). Flags slopes past ~40 deg over a quarter metre (the capsule takes 0.45 m steps and ~44 deg; the noise under its
// edge eats a little), roofs under 2.1 m, and ways under 1.1 m wide at the knee. The ledge's own drop is one-way by
// design (the ramp is the way back up), so its last 4 m are left out.
// round_ground.mjs logs it for every cave; on its own: node Tools/terrain/cave_check.mjs [caveId ...]
import { shapeAir, talusCut, CAVE } from "./cave_shape.mjs";

const smin = (a, b, k) => { const h = Math.max(k - Math.abs(a - b), 0) / k; return Math.min(a, b) - h * h * k / 4; };
const sm = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
const hash = (a, b) => { let v = (Math.imul(a, 0x8da6b343) ^ Math.imul(b, 0xd8163841) ^ 0x2c1b3) >>> 0; v = (v ^ (v >>> 13)) >>> 0; v = Math.imul(v, 0x5bd1e995) >>> 0; v = (v ^ (v >>> 15)) >>> 0; return (v & 0xffff) / 32767.5 - 1; };
const n2 = (x, y) => {
	const x0 = Math.floor(x), y0 = Math.floor(y), fx = x - x0, fy = y - y0, sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
	const l = (a, b, t) => a + (b - a) * t;
	return l(l(hash(x0, y0), hash(x0 + 1, y0), sx), l(hash(x0, y0 + 1), hash(x0 + 1, y0 + 1), sx), sy);
};
const n3 = (x, y, z) => n2(x + z * 0.37, y - z * 0.61) * 0.6 + n2(y * 0.73 + 11, z + x * 0.29) * 0.4;

/** The cave's air at a point (map m) as the game builds it: negative inside. */
export function gameAir(c, x, y, z) {
	let v = Infinity, best = Infinity, floor = 0, width = 1;
	for (const s of c.shapes) {
		const [d, f, w] = shapeAir(s, x, y, z, c.bench);
		if (d < best) { best = d; floor = f; width = w; }
		v = v === Infinity ? d : smin(v, d, CAVE.join);
	}
	v = talusCut(c, x, y, z, v);
	const wall = Math.min(0.7, Math.max(0.18, 0.22 * width)), amp = 0.12 + (wall - 0.12) * sm(0.4, 1.6, z - floor);
	return v + amp * (n3(x / 2.2, y / 2.2, z / 2.2) + 0.35 * n3(x / 0.7 + 5, y / 0.7 + 1, z / 0.7 + 3));
}

/** The crag over a cave's mouth (land file "brow"; WRLandCaves.cpp LcBrow, the same): rock where negative, m. */
export function cragAt(c, x, y, z) {
	if (!c.brow) return Infinity;
	const [bx, by, bz, rx, ry, rz, yaw] = c.brow, a = yaw * Math.PI / 180, ca = Math.cos(a), sa = Math.sin(a);
	const egg = (cx, cy, cz, sx, sy, sz) => { const dx = x - cx, dy = y - cy, dz = z - cz; return (Math.hypot((dx * ca + dy * sa) / sx, (-dx * sa + dy * ca) / sy, dz / sz) - 1) * Math.min(sx, sy, sz); };
	const side = yaw * 0.1 - Math.floor(yaw * 0.1) < 0.5 ? 1 : -1;
	const lx = bx + ca * rx * 0.45 - sa * ry * 0.55 * side, ly = by + sa * rx * 0.45 + ca * ry * 0.55 * side, lz = bz + rz * 0.35;
	const e1 = egg(bx, by, bz, rx, ry, rz), e2 = egg(lx, ly, lz, rx * 0.62, ry * 0.62, rz * 0.62), hj = Math.max(1 - Math.abs(e1 - e2), 0);
	return Math.min(e1, e2) - hj * hj * 0.25 + 0.5 * n3(x / 2, y / 2, z / 2) + 0.25 * n3(x / 0.6 + 3, y / 0.6 + 9, z / 0.6 + 2) - 0.4 * Math.abs(n3(x / 1.2 + 11, y / 1.2 + 4, z / 1.2 + 7));
}

/** The way in from outside, ground, crag and cave together as the game joins them (H: the ground's height): from 10 m
 *  out to 4 m in along the mouth's line, the steepest step (empty: walkable). */
export function checkMouth(c, H) {
	const [mx, my] = c.mouth, [dx, dy] = c.dir;
	const field = (x, y, z) => { const g = H(x, y) - z, b = -cragAt(c, x, y, z), hk = Math.max(0.5 - Math.abs(g - b), 0) / 0.5; return Math.min(gameAir(c, x, y, z), Math.max(g, b) + hk * hk * 0.5 * 0.25); };
	let prev = null, steep = { g: 0 };
	for (let d = -10; d <= 4; d += 0.25) {
		const x = mx + dx * d, y = my + dy * d;
		// (up from under both the ground and the way in's floor: under the hill the ground is the hill's top)
		let z = Math.min(H(x, y), c.mouth[2]) - 1.5;
		while (z < H(x, y) + 8 && field(x, y, z) >= 0) z += 0.02;
		if (prev !== null && Math.abs(z - prev) / 0.25 > Math.abs(steep.g)) steep = { g: (z - prev) / 0.25, at: [x, y, z] };
		prev = z;
	}
	return Math.abs(steep.g) > 0.84 ? [`mouth: grade ${steep.g.toFixed(2)} at (${steep.at.map((v) => v.toFixed(1)).join(", ")})`] : [];
}

/** The problems found walking the cave's passages (empty: all walkable). */
export function checkCave(c) {
	const air = (x, y, z) => gameAir(c, x, y, z);
	const floorAt = (x, y, guess) => {
		let z = guess + 1.0;
		if (air(x, y, z) >= 0) { while (z < guess + 4 && air(x, y, z) >= 0) z += 0.02; if (air(x, y, z) >= 0) return NaN; }
		while (z > guess - 6 && air(x, y, z - 0.02) < 0) z -= 0.02;
		return z;
	};
	const headroom = (x, y, f) => { let z = f + 0.05; while (z < f + 20 && air(x, y, z) < 0) z += 0.05; return z - f; };
	const across = (x, y, f, nx, ny) => { const go = (s) => { let d = 0; while (d < 6 && air(x + nx * d * s, y + ny * d * s, f + 0.6) < 0) d += 0.05; return d; }; return go(1) + go(-1); };
	const fmt = (p) => `(${p.map((v) => v.toFixed(1)).join(", ")})`;
	const issues = [];
	for (const t of c.shapes.filter((s) => s.k === "tube")) {
		const last = t.p[t.p.length - 1];
		let prev = null, steep = { g: 0 }, low = { h: 99 }, tight = { w: 99 };
		for (let i = 1; i < t.p.length; i++) {
			const [ax, ay, af] = t.p[i - 1], [bx, by, bf] = t.p[i], len = Math.hypot(bx - ax, by - ay) || 1e-6, ux = (bx - ax) / len, uy = (by - ay) / len;
			for (let d = i === 1 ? 0 : 0.25; d <= len; d += 0.25) {
				const x = ax + ux * d, y = ay + uy * d;
				if (t.role === "ledge" && Math.hypot(x - last[0], y - last[1]) < 4) { prev = null; continue; }
				const f = floorAt(x, y, af + (bf - af) * d / len);
				if (isNaN(f)) { issues.push(`${t.role}: no air at ${fmt([x, y])}`); prev = null; continue; }
				if (prev) { const g = (f - prev[2]) / Math.hypot(x - prev[0], y - prev[1]); if (Math.abs(g) > Math.abs(steep.g)) steep = { g, at: [x, y, f] }; }
				const h = headroom(x, y, f);
				if (h < low.h) low = { h, at: [x, y, f] };
				const w = across(x, y, f, -uy, ux);
				if (w < tight.w) tight = { w, at: [x, y, f] };
				prev = [x, y, f];
			}
		}
		if (Math.abs(steep.g) > 0.84) issues.push(`${t.role}: grade ${steep.g.toFixed(2)} at ${fmt(steep.at)}`);
		if (low.h < 2.1) issues.push(`${t.role}: roof ${low.h.toFixed(2)} m at ${fmt(low.at)}`);
		if (tight.w < 1.1) issues.push(`${t.role}: ${tight.w.toFixed(2)} m wide at ${fmt(tight.at)}`);
	}
	return issues;
}

if (process.argv[1]?.replace(/\\/g, "/").endsWith("/cave_check.mjs")) {
	const fs = await import("fs");
	const L = JSON.parse(fs.readFileSync(new URL("../../Data/lands/island.json", import.meta.url), "utf8"));
	const want = process.argv.slice(2);
	let bad = 0;
	for (const c of L.caves) {
		if (want.length && !want.includes(c.id)) continue;
		const issues = checkCave(c);
		bad += issues.length;
		console.log(`${c.id} (${c.name}): ${issues.length ? "\n  " + issues.join("\n  ") : "walkable"}`);
	}
	process.exitCode = bad ? 1 : 0;
}
