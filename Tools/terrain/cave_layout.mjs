// Cave layouts (11-20, Raven: "Build them" -- caves worth exploring, not a tunnel and a room): each a small network of
// shapes (cave_shape.mjs) laid from its mouth by a seeded hand, then dressed with formations, lights and the spots its
// contents stand on. round_ground.mjs sites the caves, raises the ground where their rock is thin, adds the skylights
// and cuts the mouths; WRLandCaves builds the rock and the formations, AWRCave and the world systems the contents.
//
// A cave: the entrance winding down to an entry chamber; on to the big hall either down a slope or off a ledge (3-3.6 m
// drop) with a ramp that loops back up to it; a crawl off the way to a hidden pocket (the cache); a squeeze from the
// hall down into the deep chamber (the best ore, the burrowers' den); sometimes a collapsed skylight over the hall.
// A tube's room: its passage from the corridor down into the room, and a crawl to a pocket.
import { caveAir, airTop } from "./cave_shape.mjs";
import { caveFloorAt } from "./cave_check.mjs";

export const rng = (seed) => { let s = (seed >>> 0) || 1; return () => { s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0; return s / 4294967296; }; };
const V = (a) => [Math.cos(a), Math.sin(a)];
const add = (p, d, k) => [p[0] + d[0] * k, p[1] + d[1] * k];
const segDist = (x, y, a, b) => { const dx = b[0] - a[0], dy = b[1] - a[1], t = Math.max(0, Math.min(1, ((x - a[0]) * dx + (y - a[1]) * dy) / Math.max(1e-6, dx * dx + dy * dy))); return Math.hypot(x - a[0] - dx * t, y - a[1] - dy * t); };
/** An ellipse's radius in a direction (rad), its long axis at yaw (rad). */
const radiusAlong = (r, yaw, a) => 1 / Math.hypot(Math.cos(a - yaw) / r[0], Math.sin(a - yaw) / r[1]);
const at = (h, u, v) => { const a = h.yaw * Math.PI / 180; return [h.c[0] + u * Math.cos(a) - v * Math.sin(a), h.c[1] + u * Math.sin(a) + v * Math.cos(a)]; };

export const ROOF = 2.2, GRADE = 0.36;
/** The point just inside a chamber's wall (at its floor) on the way from its centre toward a point. A passage coming
 *  down into a chamber reaches the chamber's floor there: inside, the floor is the chamber's, so a passage still up off
 *  it at the wall left a step you could drop down but never climb back. */
const wallPoint = (c, r, yawRad, toward) => { const a = Math.atan2(toward[1] - c[1], toward[0] - c[0]); return add(c, V(a), radiusAlong(r, yawRad, a) * 0.96); };
/** FLAT m past a chamber's wall the passage still runs at the chamber's floor before it climbs: where two shapes join
 *  the smooth union dips the floor a little, and a climb starting right there left a lip too steep to walk up. */
const FLAT = 2.5;
const beyond = (p, from, k) => { const L = Math.hypot(from[0] - p[0], from[1] - p[1]) || 1; return [p[0] + (from[0] - p[0]) / L * k, p[1] + (from[1] - p[1]) / L * k]; };
const dist = (a, b) => Math.hypot(b[0] - a[0], b[1] - a[1]);
/** The length walked along a passage's points from i0 to i1. */
const runLen = (pts, i0, i1) => { let s = 0; for (let i = i0 + 1; i <= i1; i++) s += dist(pts[i - 1], pts[i]); return s; };
/** A passage's floors by the length walked: to point i0 at z0, from i1 on at z1, in proportion between, so its grade is
 *  even however its points fall. (A floor halfway by point count, with the points unevenly spaced, made a squeeze that
 *  dropped 2.9 m in 2 m: down it you fell, back up it you couldn't.) */
const graded = (pts, i0, i1, z0, z1) => { const L = runLen(pts, i0, i1) || 1; let s = 0; return pts.map((p, i) => { if (i > i0 && i <= i1) s += dist(pts[i - 1], p); return i <= i0 ? z0 : i >= i1 ? z1 : z0 + (z1 - z0) * s / L; }); };

/** The lowest ground over a chamber's footprint grown by g m. */
function groundMin(H, c, r, yaw, g) {
	let m = Infinity;
	const ca = Math.cos(yaw), sa = Math.sin(yaw);
	for (let u = -(r[0] + g); u <= r[0] + g; u += 3) for (let v = -(r[1] + g); v <= r[1] + g; v += 3) {
		if ((u / (r[0] + g)) ** 2 + (v / (r[1] + g)) ** 2 > 1) continue;
		m = Math.min(m, H(c[0] + u * ca - v * sa, c[1] + u * sa + v * ca));
	}
	return m;
}

/** Lays a cave from its mouth: { id, kind: "cave" | "room", mouth: [x, y, floor], dir: [dx, dy], seed, H, ok(x, y, r) }.
 *  Null when a chamber would fall where ok() says no. */
export function layoutCave(o) {
	const { kind, mouth, dir, seed, H, ok } = o;
	const R = rng(seed), rr = (a, b) => a + (b - a) * R();
	const room = kind === "room";
	const [mx, my, z0] = mouth, head = Math.atan2(dir[1], dir[0]);
	const shapes = [];
	const hall = (role, c, r, yawRad, h, f) => { const s = { k: "hall", role, c: [c[0], c[1], f], r, yaw: yawRad * 180 / Math.PI, h }; shapes.push(s); return s; };
	const tube = (role, p) => { const s = { k: "tube", role, p }; shapes.push(s); return s; };
	const coverFloor = (c, r, yawRad, h) => groundMin(H, c, r, yawRad, 2) - h - ROOF;

	// The entrance, winding in and down to the first chamber.
	const entLen = room ? rr(14, 18) : rr(18, 24);
	const m0 = add([mx, my], dir, -3), m1 = add([mx, my], dir, 4);
	const w1 = head + rr(-0.35, 0.35), m2 = add(m1, V(w1), entLen * 0.5);
	const headA = w1 + rr(-0.35, 0.35);
	const rA = room ? [rr(8, 10), rr(7, 8.5)] : [rr(6.5, 8.5), rr(5.5, 7)], hA = room ? rr(5.5, 6.5) : rr(4.5, 5.5);
	const cA = add(m2, V(headA), entLen * 0.5 + rA[0] - 1.5);
	if (!ok(cA[0], cA[1], rA[0])) return null;
	const eA = wallPoint(cA, rA, headA, m2), eA2 = beyond(eA, m2, FLAT), outLen = Math.hypot(m2[0] - m1[0], m2[1] - m1[1]) + Math.hypot(eA2[0] - m2[0], eA2[1] - m2[1]);
	let zA = Math.min(z0 - rr(3, 5), coverFloor(cA, rA, headA, hA));
	zA = Math.max(zA, z0 - GRADE * outLen);
	tube("entrance", [[...m0, z0, 2.3, 4.2], [...m1, z0, 2.5, 4.4], [...m2, z0 + (zA - z0) * Math.hypot(m2[0] - m1[0], m2[1] - m1[1]) / outLen, 2.2, 3.9], [...eA2, zA, 2.4, 4.2], [...eA, zA, 2.4, 4.2], [...cA, zA, 2.4, 4.2]]);
	const A = hall(room ? "room" : "entry", cA, rA, headA, hA, zA);
	const cave = { kind, mouth, dir, shapes, main: A, entLen };
	// A crawl to a pocket: a narrow way (1.35-1.6 m half-width, 2.5-2.8 m high) to a small chamber.
	// (It runs level for its first FLAT m: leaving a passage's side it would otherwise cut a channel into that passage's
	// floor, a step down and back up for anyone walking past.)
	const pocket = (from, zFrom, a) => {
		const len = rr(8, 12), rP = [rr(3.2, 3.8), rr(2.6, 3.2)], hP = rr(3.3, 3.8);
		const end = add(from, V(a), len), cP = add(end, V(a), rP[0] - 0.8);
		if (!ok(cP[0], cP[1], rP[0])) return null;
		const midP = add(from, V(a + rr(-0.25, 0.25)), len * 0.5), eP = wallPoint(cP, rP, a, midP), eP2 = beyond(eP, midP, FLAT * 0.6);
		const pts = [from, beyond(from, midP, FLAT), midP, eP2, eP, cP], run = runLen(pts, 1, 3);
		let zP = Math.min(zFrom + rr(-0.8, 1.0), coverFloor(cP, rP, a, hP));
		zP = Math.min(Math.max(zP, zFrom - GRADE * run), zFrom + GRADE * run);
		const fl = graded(pts, 1, 3, zFrom, zP), W = [1.6, 1.6, 1.5, 1.6, 1.6, 1.6], Ht = [3.1, 3.1, 2.9, 3.1, 3.1, 3.1];
		tube("crawl", pts.map((p, i) => [...p, fl[i], W[i], Ht[i]]));
		return hall("pocket", cP, rP, a, hP, zP);
	};
	if (room) {
		// The room's crawl leaves from its far side.
		const side = R() < 0.5 ? 1 : -1, a = headA + side * rr(0.9, 1.5);
		if (!pocket(add(cA, V(a), radiusAlong(rA, headA, a) - 1), zA, a)) return null;
		return cave;
	}
	// The big hall, down a slope or off a ledge.
	// (A slope gets the longer way: its grade stays walkable with the level runs at its ends and its landing.)
	const headB = headA + rr(-0.9, 0.9), wantLedge = R() < 0.6;
	const rB = [rr(10, 13), rr(8, 10.5)], hB = rr(6.5, 8.5), gap = wantLedge ? rr(10, 15) : rr(15, 19);
	const cB = add(cA, V(headB), radiusAlong(rA, headA, headB) + gap + rB[0]);
	if (!ok(cB[0], cB[1], rB[0])) return null;
	const u = V(headB), aOut = add(cA, u, radiusAlong(rA, headA, headB) - 1), bIn = add(cB, u, -(rB[0] - 0.5));
	const sideSign = R() < 0.5 ? 1 : -1, v = V(headB + sideSign * Math.PI / 2);
	const mid = add(add(aOut, u, gap * 0.5), v, rr(-2.5, 2.5));
	const coverB = coverFloor(cB, rB, headB, hB);
	const ledge = wantLedge && coverB >= zA - 4.1;
	let zB, midFloor;
	if (ledge) {
		// The way ends at a ledge over the hall (a 3-3.6 m drop), and a ramp leaves it before the edge, loops round
		// outside the hall's wall and comes down into the hall along it: down by the drop, back up by the ramp.
		midFloor = zA - 0.3;
		tube("ledge", [[...aOut, zA, 2.3, 4], [...mid, midFloor, 2.2, 3.8], [...bIn, zA - 0.5, 2.4, 4]]);
		// (The ramp reaches the hall's floor where it meets the hall's wall: inside the hall its floor is the hall's, so a
		// ramp still up off it there left a step no one could climb.)
		const J = add(bIn, u, -7), J2 = add(J, v, 5);
		const K = add(add(cB, u, -0.6 * rB[0]), v, 0.8 * rB[1]), K2 = add(add(cB, u, -0.6 * rB[0] + 2.5), v, 0.6 * rB[1]);
		const Kf = beyond(K, J2, FLAT);
		const drop = Math.min(rr(3, 3.6), GRADE * dist(J2, Kf) + 0.3);
		zB = zA - 0.5 - drop;
		tube("ramp", [[...J, zA - 0.4, 2.0, 3.6], [...J2, zA - 0.8, 2.0, 3.6], [...Kf, zB, 2.1, 3.6], [...K, zB, 2.1, 3.6], [...K2, zB, 2.2, 3.6]]);
	} else {
		// Level from the entry chamber, down to a landing where the crawl leaves (level across its mouth: a crawl leaving a
		// sloping floor cut a lip into it too steep to walk up), down again to the hall, level into it.
		const a1 = beyond(aOut, mid, FLAT + 1), b1 = beyond(bIn, mid, FLAT), m0 = beyond(mid, a1, 1.8), m1 = beyond(mid, b1, 1.8);
		const s1 = dist(a1, m0), s2 = dist(m1, b1);
		zB = Math.max(Math.min(zA - rr(1.5, 3.5), coverB), zA - GRADE * (s1 + s2));
		midFloor = zA - (zA - zB) * s1 / (s1 + s2);
		tube("slope", [[...aOut, zA, 2.3, 4], [...a1, zA, 2.3, 4], [...m0, midFloor, 2.2, 3.8], [...mid, midFloor, 2.2, 3.8], [...m1, midFloor, 2.2, 3.8], [...b1, zB, 2.4, 4.2], [...bIn, zB, 2.4, 4.2], [...cB, zB, 2.4, 4.2]]);
	}
	const B = hall("hall", cB, rB, headB, hB, zB);
	cave.main = B;
	cave.ledge = ledge;
	// The pocket, off the way to the hall on the side away from the ramp.
	if (!pocket(mid, midFloor, headB - sideSign * rr(1.2, 1.9))) return null;
	// The deep chamber, through a squeeze from the hall's far side.
	const headC = headB + rr(-1.1, 1.1);
	const rC = [rr(6, 8), rr(5, 6.5)], hC = rr(4.5, 6), sq = rr(10, 13);
	const bOut = add(cB, V(headC), radiusAlong(rB, headB, headC) - 0.8), sEnd = add(bOut, V(headC), sq), cC = add(sEnd, V(headC), rC[0] - 1);
	if (!ok(cC[0], cC[1], rC[0])) return null;
	const sMid = add(bOut, V(headC + rr(-0.3, 0.3)), sq * 0.5);
	const sPts = [bOut, beyond(bOut, sMid, FLAT), sMid, beyond(sEnd, sMid, FLAT), sEnd, cC], sRun = runLen(sPts, 1, 3);
	let zC = Math.min(zB - rr(1.5, 3), coverFloor(cC, rC, headC, hC));
	zC = Math.max(zC, zB - GRADE * sRun);
	const sFl = graded(sPts, 1, 3, zB, zC), sW = [1.6, 1.6, 1.5, 1.6, 1.6, 1.9], sH = [3.1, 3.1, 2.9, 3.0, 3.0, 3.3];
	tube("squeeze", sPts.map((p, i) => [...p, sFl[i], sW[i], sH[i]]));
	hall("deep", cC, rC, headC, hC, zC);
	return cave;
}

/** The ground each shape needs over it ([x, y, z]): a chamber's ceiling + ROOF, a passage's + 1.8 m, the rock's noise
 *  allowed for; the entrance's first metres (and a room's way under its corridor) are left to the brow. */
export function coverNeeds(c, skipEntrance) {
	const out = [];
	for (const s of c.shapes) {
		if (s.k === "hall") {
			const b = 0.85 * s.h, l = s.h - s.h * 0.85;
			for (let u = -s.r[0] * 1.1; u <= s.r[0] * 1.1; u += 2.5) for (let v = -s.r[1] * 1.1; v <= s.r[1] * 1.1; v += 2.5) {
				const q = 1 - (u / (s.r[0] * 1.1)) ** 2 - (v / (s.r[1] * 1.1)) ** 2;
				if (q < 0) continue;
				const [x, y] = at(s, u, v);
				out.push([x, y, s.c[2] + l + b * Math.sqrt(Math.max(0, 1 - (u / s.r[0]) ** 2 - (v / s.r[1]) ** 2)) + ROOF + 0.5]);
			}
		} else if (s.k === "tube") {
			let acc = -3;
			for (let i = 1; i < s.p.length; i++) {
				const [ax, ay, af, aw, ah] = s.p[i - 1], [bx, by, bf, bw, bh] = s.p[i], L = Math.hypot(bx - ax, by - ay);
				for (let t = 0; t <= L; t += 2) {
					const k = t / L, x = ax + (bx - ax) * k, y = ay + (by - ay) * k, f = af + (bf - af) * k, w = aw + (bw - aw) * k, h = ah + (bh - ah) * k;
					if (s.role === "entrance" && acc + t < skipEntrance) continue;
					const nx = -(by - ay) / L, ny = (bx - ax) / L;
					for (const lat of [-w * 0.8, 0, w * 0.8]) out.push([x + nx * lat, y + ny * lat, f + h + 1.8 + 0.4]);
				}
				acc += L;
			}
		}
	}
	return out;
}

/** A skylight (a collapsed roof) over a chamber where the drop onto the fallen roof's rubble is at most 14 m: a shaft from just under the ceiling to over the ground. Tries the big hall, then the others. Returns the
 *  shaft (also pushed into the shapes) or null. */
export function addSkylight(c, H, seed, chance) {
	const R = rng(seed ^ 0x2f17);
	if (R() > chance) return null;
	const order = ["hall", "entry", "deep"].map((r) => c.shapes.find((s) => s.role === r)).filter(Boolean);
	// The walking lines (every passage's line, and in each chamber from where the passages meet it to its middle): the
	// fallen roof's rubble is solid, so the shaft stands off them.
	const lanes = [];
	for (const t of c.shapes.filter((q) => q.k === "tube")) for (let i = 1; i < t.p.length; i++) lanes.push([t.p[i - 1], t.p[i]]);
	for (const h of c.shapes.filter((q) => q.k === "hall")) for (const t of c.shapes.filter((q) => q.k === "tube")) for (const p of t.p) if (Math.hypot((p[0] - h.c[0]) / h.r[0], (p[1] - h.c[1]) / h.r[1]) < 1.15) lanes.push([p, h.c]);
	const laneDist = (x, y) => lanes.reduce((m, [a, b]) => Math.min(m, segDist(x, y, a, b)), Infinity);
	for (let tries = 0; tries < 60; tries++) {
		const B = order[Math.floor(tries / 20)];
		if (!B) break;
		const a = R() * Math.PI * 2, f = 0.35 + 0.4 * R(), [x, y] = at(B, Math.cos(a) * B.r[0] * f, Math.sin(a) * B.r[1] * f);
		const r = 2.2 + 0.8 * R();
		// The fallen roof heaped under it: a debris cone 2.2-2.8 m high, as steep as loose rock lies (about 30 deg: still
		// walkable), wider than the hole. (At 1.2 m over 4 m it lay flat, and the sunlit patch on it read as a sheet of paper.)
		const th = 2.2 + 0.6 * R(), tr = Math.max(r + 1.6, th * 1.75);
		const g = H(x, y), top = airTop(c, x, y, B.c[2]);
		// (the walking lines may pass over its foot, a gentle slope, not its steep middle)
		if (laneDist(x, y) < tr - 0.6) continue;
		// (3 m of rock over the chamber at least: with 1.5 m the hole's rim, most of it the ground's own skin, looked as thin
		// as torn paper from below)
		if (!(top > -Infinity) || g - (B.c[2] + th) > 14 || g - top < 3) continue;
		let lo = Infinity;
		for (let k = 0; k < 8; k++) lo = Math.min(lo, H(x + Math.cos(k * Math.PI / 4) * (r + 2), y + Math.sin(k * Math.PI / 4) * (r + 2)));
		const s = { k: "shaft", role: "skylight", under: B.role, c: [x, y], r, z: [top - 1.5, Math.max(g, lo) + 3], talus: [B.c[2], +th.toFixed(2), +tr.toFixed(2)] };
		c.shapes.push(s);
		return s;
	}
	return null;
}

/** Formations, lights and the contents' spots, from the final ground. */
export function dressCave(c, H, biome, seed) {
	const R = rng(seed ^ 0x5bd1), rr = (a, b) => a + (b - a) * R();
	const ice = biome === "tundra";
	const feats = [], lights = [], spots = { ore: [] };
	const halls = c.shapes.filter((s) => s.k === "hall"), tubes = c.shapes.filter((s) => s.k === "tube");
	const role = (r) => halls.find((h) => h.role === r);
	// Where survivors walk: every passage's line, and in each chamber the lines from where passages meet it to its middle.
	const lanes = [];
	for (const t of tubes) for (let i = 1; i < t.p.length; i++) lanes.push([t.p[i - 1], t.p[i]]);
	for (const h of halls) for (const t of tubes) for (const p of t.p) if (Math.hypot((p[0] - h.c[0]) / h.r[0], (p[1] - h.c[1]) / h.r[1]) < 1.15) lanes.push([p, h.c]);
	const laneDist = (x, y) => lanes.reduce((m, [a, b]) => Math.min(m, segDist(x, y, a, b)), Infinity);
	const used = [];
	const free = (x, y, r) => used.every(([ux, uy, ur]) => Math.hypot(x - ux, y - uy) > r + ur);
	const inHall = (h, f0, f1) => { const a = R() * Math.PI * 2, f = f0 + (f1 - f0) * Math.sqrt(R()); return at(h, Math.cos(a) * h.r[0] * f, Math.sin(a) * h.r[1] * f); };
	const spot = (h, f0, f1, lane, r) => { for (let k = 0; k < 40; k++) { const [x, y] = inHall(h, f0, f1); if (laneDist(x, y) > lane && free(x, y, r)) { used.push([x, y, r]); return [x, y, h.c[2]]; } } const [x, y] = inHall(h, f0, f1); used.push([x, y, r]); return [x, y, h.c[2]]; };
	const r2 = (v) => Math.round(v * 100) / 100;
	const feat = (kind, x, y, z, a, b, yaw = 0) => feats.push([kind, r2(x), r2(y), r2(z), r2(a), r2(b), Math.round(yaw)]);
	const light = (x, y, z, col, intensity, radius, day = 0) => lights.push([r2(x), r2(y), r2(z), ...col, intensity, radius, ...(day ? [1] : [])]);
	const pocket = role("pocket");
	if (c.kind === "room") {
		const room = role("room");
		spots.cache = [...spot(pocket, 0, 0.3, 0, 1.2), Math.round(Math.atan2(room.c[1] - pocket.c[1], room.c[0] - pocket.c[0]) * 180 / Math.PI)];
		// The room's ore (as much as the tube's data gives it, up to 8): along its walls, off the ways.
		for (let k = 0; k < 8; k++) spots.ore.push(spot(room, 0.55, 0.85, 1.8, 1.4));
	} else {
		const B = role("hall"), Dp = role("deep"), A = role("entry");
		// The contents first (they get the room): AWRCave's ores in order (iron, iron | tin, copper, sulfur or lichen |
		// saltpetre, clay), the best deepest; the gas (or the slick ice) in the hall, off the ways; the den in the deep
		// chamber; the cache in the pocket.
		for (const h of [Dp, Dp, B, B, B, A, A]) spots.ore.push(spot(h, 0.5, 0.85, 1.8, 1.4));
		spots.star = spot(Dp, 0.3, 0.7, 1.5, 1.4);
		spots.hazard = spot(B, 0.4, 0.75, ice ? 1.0 : 3.6, ice ? 3.2 : 2.8);
		spots.den = [...Dp.c];
		spots.cache = [...spot(pocket, 0, 0.3, 0, 1.2), 0];
	}
	// Formations in every chamber: drips or icicles from the ceiling, stalagmites off the ways, a column or two in the
	// big ones, rubble against the walls.
	for (const h of halls) {
		const area = Math.PI * h.r[0] * h.r[1], f = h.c[2];
		for (let k = Math.round(area * (ice ? 0.06 : 0.045)); k > 0; k--) {
			// (Not under a skylight: there the air runs on up out of the ground, and a drip hung from it stood over the hole.)
			const [x, y] = inHall(h, 0, 0.85), top = airTop(c, x, y, f);
			if (!(top > -Infinity) || top - f < 3 || top > H(x, y) - 1.5) continue;
			const L = ice ? rr(0.6, Math.min(2.2, (top - f) * 0.35)) : rr(0.3, Math.min(1.2, (top - f) * 0.25));
			feat(ice ? "icicle" : "drip", x, y, top + 0.9, L + 0.9, ice ? rr(0.12, 0.3) : rr(0.08, 0.2));
		}
		for (let k = Math.round(area * 0.012) + (h.role === "pocket" ? 1 : 0); k > 0; k--) {
			const r = rr(0.22, 0.5), [x, y] = inHall(h, 0.2, 0.9);
			if (laneDist(x, y) < 2.2 + r || !free(x, y, r + 0.4)) continue;
			used.push([x, y, r + 0.4]);
			feat(ice ? "icemite" : "mite", x, y, f - 0.3, rr(0.5, 1.8) + 0.3, r);
		}
		if (h.r[0] > 9) for (let k = R() < 0.5 ? 1 : 2; k > 0; k--) {
			const [x, y] = inHall(h, 0.4, 0.8), top = airTop(c, x, y, f), r = rr(0.45, 0.8);
			if (!(top > -Infinity) || top - f < 4 || top - f > 9 || top > H(x, y) - 1.5 || laneDist(x, y) < 3 || !free(x, y, r + 0.5)) continue;
			used.push([x, y, r + 0.5]);
			feat(ice ? "icecolumn" : "column", x, y, f - 0.3, top - f + 1.2, r);
		}
		for (let cl = h.role === "pocket" ? 1 : 2; cl > 0; cl--) {
			const [x, y] = inHall(h, 0.86, 0.96);
			for (let k = 3 + Math.floor(R() * 4); k > 0; k--) {
				const s = rr(0.4, 1.1), px = x + rr(-1.5, 1.5), py = y + rr(-1.5, 1.5);
				if (laneDist(px, py) > 1.6 + s * 0.5) feat("rock", px, py, f, s, s * rr(0.5, 0.8), R() * 360);
			}
		}
	}
	const shaftSky = c.shapes.find((q) => q.k === "shaft" && q.talus);
	const nearCone = (x, y) => !!shaftSky && Math.hypot(x - shaftSky.c[0], y - shaftSky.c[1]) < shaftSky.talus[2] + 1.2;
	if (c.kind !== "room") {
		const B = role("hall"), Dp = role("deep");
		if (!ice) {
			// The Volcanic's: cracks still glowing in the hall and the deep chamber (a zig-zag of short lit seams and an
			// ember light), a lava bench along one of the hall's walls, sulfur crusting round the gas.
			for (const h of [B, Dp]) for (let n = h === B ? 2 : 1; n > 0; n--) {
				let [x, y] = spot(h, 0.2, 0.7, 2.5, 2.5), a = R() * 360;
				const mid = [x, y];
				for (let k = 0; k < 5; k++) {
					const l = rr(1.0, 1.6), w = rr(0.06, 0.12), x1 = x + Math.cos(a * Math.PI / 180) * l, y1 = y + Math.sin(a * Math.PI / 180) * l;
					// (not up a skylight's debris cone: the fallen roof has no seam in it)
					if (!nearCone(x, y) && !nearCone(x1, y1)) feat("crack", x, y, h.c[2] + 0.04, l, w, a);
					x = x1; y = y1; a += rr(-55, 55);
				}
				light(mid[0], mid[1], h.c[2] + 1.2, [1, 0.42, 0.12], 2600, 11);
			}
			const a0 = R() * Math.PI * 2;
			for (let k = 0; k < 7; k++) {
				const a = a0 + k * 0.16, [x, y] = at(B, Math.cos(a) * B.r[0] * 0.9, Math.sin(a) * B.r[1] * 0.9);
				if (laneDist(x, y) > 2) feat("bench", x, y, B.c[2], rr(2.2, 2.8), rr(0.9, 1.2), (a + Math.PI / 2) * 180 / Math.PI + B.yaw);
			}
			for (let k = 3 + Math.floor(R() * 3); k > 0; k--) feat("sulfur", spots.hazard[0] + rr(-2.5, 2.5), spots.hazard[1] + rr(-2.5, 2.5), B.c[2] + 0.03, rr(0.4, 0.9), 0);
			light(pocket.c[0], pocket.c[1], pocket.c[2] + 1.2, [1, 0.45, 0.15], 500, 6);
		} else {
			// The Tundra's: a frozen fall down the hall's far wall with its frozen pool (the slick ice), a clear-ice wall in
			// the deep chamber with old bones in it, blue glows.
			const entry = role("entry").c, away = Math.atan2(B.c[1] - entry[1], B.c[0] - entry[0]) + rr(-0.6, 0.6);
			const yawR = B.yaw * Math.PI / 180, fr = radiusAlong(B.r, yawR, away) * 0.9, [fx, fy] = add(B.c, V(away), fr);
			const top = airTop(c, ...add(B.c, V(away), fr * 0.92), B.c[2]);
			if (top > -Infinity) for (let k = 0; k < 8; k++) {
				const a = away + Math.PI / 2, off = rr(-1.8, 1.8), px = fx + Math.cos(a) * off + Math.cos(away) * rr(-0.5, 0.3), py = fy + Math.sin(a) * off + Math.sin(away) * rr(-0.5, 0.3);
				feat("icefall", px, py, B.c[2] - 0.2, top - B.c[2] + 1.0, rr(0.35, 0.8));
			}
			const [px, py] = add(B.c, V(away), fr * 0.6);
			spots.hazard = [px, py, B.c[2]];
			feat("pool", px, py, B.c[2] + 0.03, rr(2.4, 3.2), 0);
			light(fx, fy, B.c[2] + 2.5, [0.45, 0.7, 1], 3000, 15);
			const wa = R() * Math.PI * 2, [tx, ty] = at(Dp, Math.cos(wa) * Dp.r[0] * 0.88, Math.sin(wa) * Dp.r[1] * 0.88);
			feat("trapped", tx, ty, Dp.c[2], 1, 0, Math.atan2(Dp.c[1] - ty, Dp.c[0] - tx) * 180 / Math.PI);
			light(Dp.c[0], Dp.c[1], Dp.c[2] + 2.5, [0.4, 0.65, 1], 2200, 11);
			light(pocket.c[0], pocket.c[1], pocket.c[2] + 1.2, [0.45, 0.7, 1], 400, 5);
		}
	} else {
		const room = role("room");
		light(room.c[0], room.c[1], room.c[2] + 3, ice ? [0.4, 0.65, 1] : [1, 0.45, 0.15], 5000, 18);
	}
	// Breakdown: blocks fallen from the roof lying in the big chambers (solid, off the ways); gravel over every floor and
	// along the passages' sides (no collision: small enough to walk over); in the lava tubes a puddle or two where
	// water seeped in and pooled in the floor's hollows.
	for (const h of halls) {
		if (h.r[0] > 6) for (let k = 2 + Math.floor(R() * 3); k > 0; k--) {
			const s = rr(1.2, 2.4), [x, y] = inHall(h, 0.35, 0.85);
			if (laneDist(x, y) < 2.4 + s * 0.5 || !free(x, y, s * 0.6)) continue;
			used.push([x, y, s * 0.6]);
			feat("rock", x, y, h.c[2], s, s * rr(0.4, 0.65), R() * 360);
		}
		for (let k = Math.round(h.r[0] * h.r[1] * 0.3); k > 0; k--) {
			const [x, y] = inHall(h, 0.05, 0.95), s = rr(0.08, 0.28);
			if (laneDist(x, y) > 0.6) feat("pebble", x, y, h.c[2], s, s * rr(0.45, 0.8), R() * 360);
		}
		if (!ice && h.r[0] > 5 && R() < 0.75) {
			// (in the dark, not under a skylight: in the sunlight one mirrored it like a metal plate; a few tries for a spot)
			const sky = c.shapes.find((q) => q.k === "shaft");
			for (let t = 0; t < 8; t++) {
				const [x, y] = inHall(h, 0.15, 0.7);
				if (!free(x, y, 2.2) || (sky && Math.hypot(x - sky.c[0], y - sky.c[1]) < sky.r + 5)) continue;
				used.push([x, y, 2.2]);
				// The water's level: the floor's own (the game's rock noise and all) sampled over the footprint, filled to its
				// lowest half, so the water stands level in the hollows and its edge is wherever the rock rises out of it.
				// (1-2 m across: smaller, the few triangles it filled drew it as a sharp-cornered hole.)
				const A = rr(1.0, 2.0), Bq = rr(0.7, 1.3), yaw = R() * 360, ya = yaw * Math.PI / 180, fl = [];
				for (let i = -3; i <= 3; i++) for (let j = -3; j <= 3; j++) {
					const u = i / 3, v = j / 3;
					if (u * u + v * v > 1) continue;
					const z = caveFloorAt(c, x + Math.cos(ya) * u * A - Math.sin(ya) * v * Bq, y + Math.sin(ya) * u * A + Math.cos(ya) * v * Bq, h.c[2]);
					if (!isNaN(z)) fl.push(z);
				}
				fl.sort((p, q) => p - q);
				const level = fl.length ? fl[Math.floor(fl.length * 0.5)] + 0.01 : h.c[2];
				feat("puddle", x, y, level, A, Bq, yaw);
				break;
			}
		}
	}
	for (const t of tubes) for (let i = 1; i < t.p.length; i++) {
		const [ax, ay, af, aw] = t.p[i - 1], [bx, by, bf] = t.p[i], len = Math.hypot(bx - ax, by - ay);
		if (len < 0.5) continue;
		const nx = -(by - ay) / len, ny = (bx - ax) / len;
		for (let d = rr(0, 1.5); d < len; d += rr(0.8, 2.2)) {
			const side = R() < 0.5 ? -1 : 1, off = side * aw * rr(0.45, 0.72), k = d / len, s = rr(0.08, 0.24);
			feat("pebble", ax + (bx - ax) * k + nx * off, ay + (by - ay) * k + ny * off, af + (bf - af) * k, s, s * rr(0.45, 0.8), R() * 360);
		}
	}
	// Under a skylight: the rubble of the fallen roof, and the rim's rocks up on the ground round its opening.
	const sky = c.shapes.find((s) => s.k === "shaft");
	if (sky) {
		const B = role(sky.under);
		// The day's light thrown about the chamber off the sunlit cone (by day only: the game scales it by the daylight).
		// Without it the chamber was black round a sunlit patch, and the eye's adjustment to the dark blew the patch out
		// to a flat white shape.
		const [tf, th, tr] = sky.talus, onCone = (d) => tf + th * Math.max(0, 1 - d / tr);
		light(sky.c[0], sky.c[1], tf + th + 1.4, [1, 0.9, 0.78], 9000, sky.r + 12, 1);
		// On the debris cone: the roof's slabs, big ones half sunk in it, then stones, then gravel down its sides (each on
		// the cone's real surface, the rock's noise and all); ferns where the light falls on it.
		const onFloor = (x, y, d) => { const z = caveFloorAt(c, x, y, onCone(d)); return isNaN(z) ? onCone(d) : z; };
		const drop = (kind, d0, d1, s0, s1, h0, h1, sink) => {
			const a = R() * Math.PI * 2, d = d0 + (d1 - d0) * Math.sqrt(R()), x = sky.c[0] + Math.cos(a) * d, y = sky.c[1] + Math.sin(a) * d, s = rr(s0, s1);
			feat(kind, x, y, onFloor(x, y, d) - sink * s, s, s * rr(h0, h1), R() * 360);
		};
		for (let k = 0; k < 5; k++) drop("rock", 0, tr * 0.8, 1.0, 1.7, 0.45, 0.7, 0.18);
		for (let k = 0; k < 10; k++) drop("rock", 0, tr, 0.35, 0.85, 0.4, 0.75, 0.05);
		for (let k = 0; k < 34; k++) drop("pebble", 0, tr + 0.8, 0.1, 0.32, 0.45, 0.8, 0);
		for (let k = 0; k < 7; k++) { const a = R() * Math.PI * 2, d = rr(0.3, 1.05) * sky.r, x = sky.c[0] + Math.cos(a) * d, y = sky.c[1] + Math.sin(a) * d; feat("fern", x, y, onFloor(x, y, d), rr(0.55, 0.95), 0, R() * 360); }
		for (let k = 0; k < 9; k++) {
			const a = k * Math.PI * 2 / 9 + rr(-0.2, 0.2), d = sky.r + rr(0.8, 2.2), x = sky.c[0] + Math.cos(a) * d, y = sky.c[1] + Math.sin(a) * d;
			feat("rimrock", x, y, H(x, y), rr(0.7, 1.6), rr(0.5, 0.9), R() * 360);
		}
	}
	return { feats, lights, spots };
}
