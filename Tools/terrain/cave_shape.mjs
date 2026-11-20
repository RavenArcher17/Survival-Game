// One underground cave of a land (island.json "caves"; WRLand's FWRLandCave::Air is the same formula): a passage, a
// flat-floored tunnel along a polyline whose floor goes down from the mouth, into a hall, a flat-floored ellipsoid, the
// two joined smoothly. Map metres, z absolute. Negative inside the cave's air. The game adds up to CAVE.noise m of
// rock noise to the walls (less on the floor), so anything that must stay clear of the air allows for it.
export const CAVE = {
	halfW: 2.6,      // the passage's half-width at its widest
	tubeB: 2.8,      // its vertical half-axis
	tubeLift: 1.6,   // the cross-section's centre over the floor (the floor cuts it: ~4.3 m wide, 4.4 m high)
	hallLift: 1.0,   // the hall's centre over its floor
	hallB: 5.6,      // its vertical half-axis (ceiling 6.6 m over the floor)
	join: 2.0,       // the smooth join (m)
	noise: 0.5,
};

const smin = (a, b, k) => { const h = Math.max(k - Math.abs(a - b), 0) / k; return Math.min(a, b) - h * h * k / 4; };

/** The passage's floor and its distance across at a map point: { f, lat, s } (s: metres along from the first point). */
export function passageAt(c, x, y) {
	let best = Infinity, f = 0, s = 0, acc = 0;
	for (let i = 1; i < c.pts.length; i++) {
		const [ax, ay, af] = c.pts[i - 1], [bx, by, bf] = c.pts[i];
		const dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy, L = Math.sqrt(L2);
		const t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / L2));
		const d = Math.hypot(x - ax - dx * t, y - ay - dy * t);
		if (d < best) { best = d; f = af + (bf - af) * t; s = acc + L * t; }
		acc += L;
	}
	return { f, lat: best, s };
}

export function caveAir(c, x, y, z) {
	const p = passageAt(c, x, y);
	const tube = Math.max((Math.hypot(p.lat / CAVE.halfW, (z - p.f - CAVE.tubeLift) / CAVE.tubeB) - 1) * Math.min(CAVE.halfW, CAVE.tubeB), p.f - z);
	const [hx, hy, hf] = c.hall, [dx, dy] = c.dir, [rx, ry] = c.hallR;
	const u = (x - hx) * dx + (y - hy) * dy, v = -(x - hx) * dy + (y - hy) * dx;
	const hall = Math.max((Math.hypot(u / rx, v / ry, (z - hf - CAVE.hallLift) / CAVE.hallB) - 1) * Math.min(rx, ry, CAVE.hallB), hf - z);
	return smin(tube, hall, CAVE.join);
}
