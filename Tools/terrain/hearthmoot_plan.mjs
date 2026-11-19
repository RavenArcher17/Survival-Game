// Hearthmoot and its meadow, the plan both builders share (plan phase 3, slice 1 quality pass, Raven 11-19: "go ahead
// with what you recommend": a city that feels lived in, a meadow and lake that look natural). Pure shapes from the
// signed-off layout (no heights): round_ground.mjs paints and flattens the ground from it (cobbled streets, grass yards,
// fields, the beach, the bridges' channels); hearthmoot.mjs stands the buildings, plants and props on that ground.
//   streets   the city's avenues (gate to plaza), the ring street, lanes, alleys and the lane inside the wall
//   fields    strip fields in blocks beside each main road, a crop each; hedgerows round each block
//   farms     a farmstead by each main road's fields; two windmills
//   trees     the meadow's trees, in groves and alone (Poisson spacing, clear of everything else)
//   reeds     clumps along the lake's shallows; beach (the city side of the lake), jetty, bridges (roads over the stream)
import * as RI from "./round_island.mjs";

const { trunks, lands, stream, lakeOutline, lakeAt, meadowR, CITY_WALL, quarters, pt, bearingOf, norm, dAng, RAD, fbm, inPoly } = RI;

export const PLAZA_R = 44;          // the plaza (the hearth, the market ring, trees and benches round its edge)
export const CITY_INNER = 285;      // buildings stay inside this (the wall's inner face is at 298, the wall lane at 289)
export const HILL_FOOT = 470;       // the city's hill meets the meadow about here: fields and trees stay out of it

// ---- small helpers ------------------------------------------------------------------------------------------------
export function rng(seed) { let s = seed >>> 0 || 1; return () => { s = (Math.imul(s, 1664525) + 1013904223) >>> 0; return s / 4294967296; }; }
export const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };
export function segDist([px, py], [ax, ay], [bx, by]) {
	const L2 = (bx - ax) ** 2 + (by - ay) ** 2 || 1e-9, t = Math.min(1, Math.max(0, ((px - ax) * (bx - ax) + (py - ay) * (by - ay)) / L2));
	return { d: Math.hypot(px - ax - t * (bx - ax), py - ay - t * (by - ay)), t, at: [ax + t * (bx - ax), ay + t * (by - ay)] };
}
export function lineDist(p, pts) {
	let best = { d: 1e9, at: pts[0], seg: 0 };
	for (let i = 1; i < pts.length; i++) { const r = segDist(p, pts[i - 1], pts[i]); if (r.d < best.d) best = { d: r.d, at: r.at, seg: i - 1 }; }
	return best;
}
function segCross(a, b, c, d) {
	const r = [b[0] - a[0], b[1] - a[1]], s = [d[0] - c[0], d[1] - c[1]], den = r[0] * s[1] - r[1] * s[0];
	if (Math.abs(den) < 1e-9) return null;
	const t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den, u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den;
	return t >= 0 && t <= 1 && u >= 0 && u <= 1 ? [a[0] + t * r[0], a[1] + t * r[1]] : null;
}

// ---- the city's streets ---------------------------------------------------------------------------------------------
// Avenues from each gate to the plaza, bending a little; a ring street at about 150 m and alleys at about 100 and 222 m,
// all wandering a few metres; lanes in from the ring to the plaza on the quarters' edges and out to the wall lane on
// their middles (none where an avenue already runs). Each: { kind, half (m), pts }.
const gates = trunks.map((t) => t.bearing);
const nearAvenue = (deg, r, clear) => gates.some((g) => Math.abs(dAng(deg, g)) * RAD * r < clear);
const wobble = (deg, amp, seed) => amp * fbm(Math.cos(deg * RAD) * 1.7 + seed, Math.sin(deg * RAD) * 1.7 + seed, seed, 3);
const ringLine = (r, amp, seed) => { const o = []; for (let d = 0; d <= 360; d += 2) o.push(pt(d, r + wobble(d, amp, seed))); return o; };
const radial = (deg, r0, r1, amp, seed) => { const o = []; const n = Math.max(2, Math.ceil(Math.abs(r1 - r0) / 8)); for (let i = 0; i <= n; i++) { const r = r0 + (r1 - r0) * i / n, bend = amp * Math.sin(Math.PI * i / n) + 0.4 * amp * fbm(r / 60, seed, seed, 2); o.push(pt(deg + bend / Math.max(r, 1) / RAD, r)); } return o; };
export const streets = [
	...gates.map((g, i) => ({ kind: "avenue", half: 5.5, pts: radial(g, CITY_WALL, PLAZA_R - 4, 6 * (i % 2 ? 1 : -1), 31 + i) })),
	{ kind: "ring", half: 4, pts: ringLine(150, 7, 41) },
	{ kind: "alley", half: 2, pts: ringLine(100, 5, 43) },
	{ kind: "alley", half: 2, pts: ringLine(222, 6, 47) },
	{ kind: "wall", half: 4, pts: ringLine(289, 0, 49) },
	...quarters.filter((q) => !nearAvenue(q.from, 150, 16)).map((q, i) => ({ kind: "lane", half: 2.8, pts: radial(q.from, PLAZA_R + 2, 150, 4, 51 + i) })),
	...quarters.map((q) => norm(q.from + 15)).filter((m) => !nearAvenue(m, 220, 16)).map((m, i) => ({ kind: "lane", half: 2.8, pts: radial(m, 150, 289, 5, 61 + i) })),
];
/** The nearest street to a point and how far it is past that street's edge (negative: on it). */
export function nearestStreet(p) {
	let best = null;
	for (const s of streets) { const r = lineDist(p, s.pts); const edge = r.d - s.half; if (!best || edge < best.edge) best = { street: s, edge, at: r.at, d: r.d }; }
	return best;
}

// ---- the lake: its beach (the city side), the jetty, reeds in the shallows -------------------------------------------
const towardCity = Math.atan2(-lakeAt[1], -lakeAt[0]);
const shoreAngle = (p) => Math.atan2(p[1] - lakeAt[1], p[0] - lakeAt[0]);
const angDiff = (a, b) => Math.abs(((a - b + 3 * Math.PI) % (2 * Math.PI)) - Math.PI);
/** Shore points on the beach (within 40 degrees of the side facing the city). */
export const beach = lakeOutline.filter((p) => angDiff(shoreAngle(p), towardCity) < 40 * RAD);
export const jetty = (() => {
	let best = beach[0];
	for (const p of beach) if (angDiff(shoreAngle(p), towardCity) < angDiff(shoreAngle(best), towardCity)) best = p;
	const dir = [lakeAt[0] - best[0], lakeAt[1] - best[1]], L = Math.hypot(...dir);
	return { at: best, dir: [dir[0] / L, dir[1] / L], length: 34, width: 3.6 };
})();
export const reeds = (() => {
	const r = rng(77), o = [];
	for (let i = 0; i < lakeOutline.length; i++) {
		const p = lakeOutline[i];
		if (angDiff(shoreAngle(p), towardCity) < 46 * RAD) continue;                      // the beach stays open
		if (r() < 0.35) continue;
		const inward = [lakeAt[0] - p[0], lakeAt[1] - p[1]], L = Math.hypot(...inward), into = 2 + 5 * r();
		o.push([p[0] + inward[0] / L * into, p[1] + inward[1] / L * into, 6 + Math.floor(r() * 10)]);
	}
	return o;
})();

// ---- the roads (the lands' own and the main roads) and where they cross the stream: a bridge each -----------------------
export const roads = [...lands.map((l) => ({ id: l.id, pts: l.road, half: 4 })), ...trunks.map((t) => ({ id: "main_" + t.lands.join("_"), pts: t.points, half: 6 }))];
export const bridges = (() => {
	const o = [];
	for (const rd of roads) for (let i = 1; i < rd.pts.length; i++) for (let j = 1; j < stream.length; j++) {
		const x = segCross(rd.pts[i - 1], rd.pts[i], stream[j - 1], stream[j]);
		if (x && !o.some((b) => Math.hypot(b.at[0] - x[0], b.at[1] - x[1]) < 30)) {
			const d = [rd.pts[i][0] - rd.pts[i - 1][0], rd.pts[i][1] - rd.pts[i - 1][1]], L = Math.hypot(...d);
			o.push({ road: rd.id, at: x, dir: [d[0] / L, d[1] / L], half: rd.half });
		}
	}
	return o;
})();
/** A footbridge where the stream leaves the lake (no road crosses it: the roads run beside it). */
export const footbridge = (() => {
	let i = 0, L = 0;
	while (i + 1 < stream.length && L < 70) { L += Math.hypot(stream[i + 1][0] - stream[i][0], stream[i + 1][1] - stream[i][1]); i++; }
	const a = stream[i - 1], b = stream[i], d = [b[0] - a[0], b[1] - a[1]], n = Math.hypot(...d);
	return { at: b, along: [d[0] / n, d[1] / n] };
})();

// ---- fields: blocks of strip fields beside each main road, hedged round; a farmstead by each; two windmills ------------
const streamDist = (p) => lineDist(p, stream).d;
const roadDist = (p) => Math.min(...roads.map((r) => lineDist(p, r.pts).d - r.half));
const lakeShoreDist = (p) => (inPoly(p, lakeOutline) ? -1 : 1) * lineDist(p, [...lakeOutline, lakeOutline[0]]).d;
const inMeadow = (p, margin) => Math.hypot(...p) < meadowR(bearingOf(...p)) - margin;
const CROPS = ["wheat", "barley", "greens", "ploughed", "wheat", "fallow"];
export const fields = [], fieldBlocks = [], farms = [];
{
	const r = rng(91);
	trunks.forEach((t, n) => {
		const [fx, fy] = t.fort, along = [-fx, -fy], L = Math.hypot(fx, fy);
		const u = [along[0] / L, along[1] / L], v = [-u[1], u[0]];          // u: in along the road, v: across it
		for (const side of [-1, 1]) {
			const W = 36, Lf = 110, gap = 5, cols = 3, rows = 3, off = 34, start = 130;
			const block = [], ok = [];
			for (let c = 0; c < cols; c++) for (let k = 0; k < rows; k++) {
				const s0 = start + k * (Lf + gap), w0 = off + c * (W + gap);
				const corner = (s, w) => [fx + u[0] * s + v[0] * w * side, fy + u[1] * s + v[1] * w * side];
				const poly = [corner(s0, w0), corner(s0 + Lf, w0), corner(s0 + Lf, w0 + W), corner(s0, w0 + W)];
				const mid = corner(s0 + Lf / 2, w0 + W / 2);
				const clear = poly.every((p) => inMeadow(p, 60) && Math.hypot(...p) > HILL_FOOT && streamDist(p) > 25 && lakeShoreDist(p) > 50 && roadDist(p) > 12);
				if (!clear) continue;
				const f = { poly, mid, u, v: [v[0] * side, v[1] * side], crop: CROPS[Math.floor(r() * CROPS.length)] };
				fields.push(f);
				block.push(...poly);
				ok.push(f);
			}
			if (ok.length >= 2) {
				// The block's outline (the hull of its fields' corners): its hedgerow runs round it.
				const pts = block.map((p) => [(p[0] - fx) * u[0] + (p[1] - fy) * u[1], ((p[0] - fx) * v[0] + (p[1] - fy) * v[1]) * side]);
				const s0 = Math.min(...pts.map((p) => p[0])) - 3, s1 = Math.max(...pts.map((p) => p[0])) + 3, w0 = Math.min(...pts.map((p) => p[1])) - 3, w1 = Math.max(...pts.map((p) => p[1])) + 3;
				const corner = (s, w) => [fx + u[0] * s + v[0] * w * side, fy + u[1] * s + v[1] * w * side];
				fieldBlocks.push({ poly: [corner(s0, w0), corner(s1, w0), corner(s1, w1), corner(s0, w1)] });
				if (side === 1) {
					const at = corner((s0 + s1) / 2, w1 + 28);
					if (inMeadow(at, 50) && roadDist(at) > 20 && lakeShoreDist(at) > 40 && streamDist(at) > 30) farms.push({ at, yaw: Math.atan2(u[1], u[0]) / RAD });
				}
			}
		}
	});
}
export const windmills = (() => {
	const o = [];
	for (const b of [40, 250]) {
		for (let rr = 900; rr < 1300; rr += 40) {
			const at = pt(b, rr);
			if (!inMeadow(at, 80) || roadDist(at) < 40 || fields.some((f) => inPoly(at, f.poly) || Math.hypot(at[0] - f.mid[0], at[1] - f.mid[1]) < 90)) continue;
			o.push({ at, yaw: b + 30 });
			break;
		}
	}
	return o;
})();
const inField = (p, pad = 0) => fields.some((f) => inPoly(p, f.poly)) || (pad > 0 && fieldBlocks.some((b) => lineDist(p, [...b.poly, b.poly[0]]).d < pad));
export const fieldAt = (p) => fields.find((f) => inPoly(p, f.poly));

// ---- the meadow's trees: groves and lone trees, Poisson-spaced, clear of roads, fields, water, farms and the city --------
export const trees = (() => {
	const r = rng(123), o = [], grid = new Map(), cell = 7;
	const key = (x, y) => `${Math.floor(x / cell)},${Math.floor(y / cell)}`;
	const near = (x, y, d) => { const cx = Math.floor(x / cell), cy = Math.floor(y / cell); for (let i = -2; i <= 2; i++) for (let j = -2; j <= 2; j++) for (const q of grid.get(`${cx + i},${cy + j}`) || []) if (Math.hypot(q[0] - x, q[1] - y) < d) return true; return false; };
	for (let y = -1800; y <= 1800; y += 16) for (let x = -1800; x <= 1800; x += 16) {
		const px = x + (r() - 0.5) * 14, py = y + (r() - 0.5) * 14, p = [px, py], rad = Math.hypot(px, py);
		if (rad < HILL_FOOT - 30 || !inMeadow(p, 25)) continue;
		const grove = smooth(0.12, 0.42, fbm(px / 330, py / 330, 131, 3));
		if (r() > 0.015 + 0.3 * grove) continue;
		if (roadDist(p) < 12 || inField(p, 6) || lakeShoreDist(p) < 16 || streamDist(p) < 9) continue;
		if (trunks.some((t) => Math.hypot(t.fort[0] - px, t.fort[1] - py) < 60)) continue;
		if (farms.some((f) => Math.hypot(f.at[0] - px, f.at[1] - py) < 32) || windmills.some((w) => Math.hypot(w.at[0] - px, w.at[1] - py) < 22)) continue;
		if (near(px, py, grove > 0.5 ? 7 : 10)) continue;
		const kind = r() < 0.12 ? "sapling" : "tree";
		o.push({ kind, at: p, yaw: r() * 360, scale: kind === "tree" ? 0.8 + 0.4 * r() : 0.8 + 0.6 * r(), variant: Math.floor(r() * 8) });
		const k = key(px, py); if (!grid.has(k)) grid.set(k, []); grid.get(k).push(p);
	}
	return o;
})();
