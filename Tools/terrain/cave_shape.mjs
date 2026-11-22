// An underground cave's air (land file "caves"; WRLand's FWRLandCave::Air is the same formula), the union of its shapes
// joined smoothly. Map metres, z absolute; negative inside the air. Each shape's floor is flat or sloping (never a bowl).
//   tube   { k: "tube", p: [[x, y, floor, halfWidth, height], ...] }  a passage: its cross-section an ellipse cut flat by
//          its floor (about 1.6 x halfWidth wide at the floor, height high), each between its points varying linearly
//   hall   { k: "hall", c: [x, y, floor], r: [rx, ry], yaw: deg, h: height }  a chamber: an ellipsoid cut flat by its floor
//   shaft  { k: "shaft", c: [x, y], r, z: [bottom, top] }  a vertical shaft (a collapsed skylight up through the ground)
// The game adds rock noise to the walls (up to CAVE.noise m in the big shapes, less in narrow ones and on the floors), so
// anything that must stay clear of the air allows for it.
export const CAVE = { join: 1.5, noise: 0.7 };

const smin = (a, b, k) => { const h = Math.max(k - Math.abs(a - b), 0) / k; return Math.min(a, b) - h * h * k / 4; };
const sm = (e0, e1, x) => { const t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); };
/** A lava bench (the Volcanic's caves: c.bench = [height above the floor, depth], m): below it the way is narrower by
 *  depth, so a shelf runs along the walls where the flow's level once stood. Only in the wide shapes (not a crawl or a
 *  squeeze). WRLand's FWRLandCaveShape::Air is the same. */
export const benchCut = (bench, w, zRel) => bench ? bench[1] * sm(1.7, 2.1, w) * (1 - sm(bench[0] - 0.15, bench[0] + 0.05, zRel)) : 0;

export function tubeAir(t, x, y, z, bench) {
	// The cross-section of the passage's nearest point (in plan): its floor runs on through every bend. (Taken segment by
	// segment, each segment's rounded end carried its own floor a couple of metres past a joint: where the floor changed
	// grade there, a pit and a step you could drop down but not climb.)
	let near = Infinity, f = 0, w = 1, h = 1;
	for (let i = 1; i < t.p.length; i++) {
		const [ax, ay, af, aw, ah] = t.p[i - 1], [bx, by, bf, bw, bh] = t.p[i];
		const dx = bx - ax, dy = by - ay, L2 = Math.max(1e-6, dx * dx + dy * dy);
		const u = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / L2));
		const lat = Math.hypot(x - ax - dx * u, y - ay - dy * u);
		if (lat < near) { near = lat; f = af + (bf - af) * u; w = aw + (bw - aw) * u; h = ah + (bh - ah) * u; }
	}
	const b = 0.62 * h, l = h - b, we = w - benchCut(bench, w, z - f);
	return [Math.max((Math.hypot(near / we, (z - f - l) / b) - 1) * Math.min(we, b), f - z), f, w];
}

export function hallAir(s, x, y, z, bench) {
	const [cx, cy, f] = s.c, a = s.yaw * Math.PI / 180, ca = Math.cos(a), sa = Math.sin(a);
	const u = (x - cx) * ca + (y - cy) * sa, v = -(x - cx) * sa + (y - cy) * ca, b = 0.85 * s.h, l = s.h - b;
	const cut = benchCut(bench, Math.min(s.r[0], s.r[1]), z - f), r0 = s.r[0] - cut, r1 = s.r[1] - cut;
	return [Math.max((Math.hypot(u / r0, v / r1, (z - f - l) / b) - 1) * Math.min(r0, r1, b), f - z), f, Math.min(s.r[0], s.r[1])];
}

export function shaftAir(s, x, y, z) {
	return [Math.max(Math.hypot(x - s.c[0], y - s.c[1]) - s.r, s.z[0] - z, z - s.z[1]), s.z[0], s.r];
}

/** The debris cone under a skylight (a shaft's talus = [floor, height, radius], m): the fallen roof heaped on the floor
 *  below the hole, a gentle cone (walkable), rock added into the air. WRLand's FWRLandCave::Air is the same. */
export function talusCut(c, x, y, z, v) {
	for (const s of c.shapes) {
		if (s.k !== "shaft" || !s.talus) continue;
		const [f, h, r] = s.talus, rho = Math.hypot(x - s.c[0], y - s.c[1]);
		const d = ((z - f) / h + rho / r - 1) * h / Math.hypot(1, h / r);
		const k = 0.4, hh = Math.max(k - Math.abs(v + d), 0) / k;
		v = Math.max(v, -d) + hh * hh * k / 4;
	}
	return v;
}

export const shapeAir = (s, x, y, z, bench) => s.k === "tube" ? tubeAir(s, x, y, z, bench) : s.k === "hall" ? hallAir(s, x, y, z, bench) : shaftAir(s, x, y, z);

/** The cave's air at a point; out (optional) gets the floor and the size of the nearest shape. */
export function caveAir(c, x, y, z, out) {
	let v = Infinity, best = Infinity;
	for (const s of c.shapes) {
		const [d, f, w] = shapeAir(s, x, y, z, c.bench);
		if (d < best) { best = d; if (out) { out.floor = f; out.width = w; } }
		v = v === Infinity ? d : smin(v, d, CAVE.join);
	}
	return talusCut(c, x, y, z, v);
}

/** The top of the air over a map point (the ceiling), from a floor height up; -Infinity if no air there. */
export function airTop(c, x, y, from) {
	let z = from + 0.2;
	if (caveAir(c, x, y, z) >= 0) return -Infinity;
	while (z < from + 40 && caveAir(c, x, y, z + 0.25) < 0) z += 0.25;
	return z;
}
