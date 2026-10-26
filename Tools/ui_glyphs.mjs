// Vector glyphs for the journal's Paths, Skills and Map tabs (Data/UI/Glyphs/*.png), drawn from the mockups' SVG paths.
// Headless Edge rasterises them (canvas Path2D takes SVG path data as is) and hands back PNGs through the DOM.
//   node Tools/ui_glyphs.mjs
// Kinds:
//   duo   two white layers the UI tints per state: <id>_a.png (accent: t strokes, b and c fills) and <id>_p.png
//         (primary: s strokes, a fill, with c cut out so the accent shows through), as the mockups colour them.
//   mask  one white layer (+ fills / strokes, cut = punched out), tinted in the UI.
//   color one full-colour picture.
import fs from "fs";
import path from "path";
import os from "os";
import { execFileSync } from "child_process";

const root = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "..");
const out = path.join(root, "Data", "UI", "Glyphs");
fs.mkdirSync(out, { recursive: true });

// ---------- shape helpers (24-unit boxes, as in the mockups)
const fl = (x, y, k) => {
	const v = (m) => Math.round(m * k * 100) / 100;
	return "M" + x + " " + y + "c" + v(3) + " " + v(4) + " " + v(6) + " " + v(6) + " " + v(6) + " " + v(11) +
		"a" + v(6) + " " + v(6) + " 0 0 1 " + v(-12) + " 0c0 " + v(-3) + " " + v(2) + " " + v(-5) + " " + v(3) + " " + v(-6) +
		" 0 " + v(2) + " " + v(1) + " " + v(4) + " " + v(3) + " " + v(4) + " " + v(-1) + " " + v(-4) + " 0 " + v(-7) + " 0 " + v(-9) + "z";
};
const el = (cx, cy, rx, ry) => "M" + (cx - rx) + " " + cy + "a" + rx + " " + ry + " 0 1 0 " + (2 * rx) + " 0a" + rx + " " + ry + " 0 1 0 " + (-2 * rx) + " 0z";
const circ = (cx, cy, r) => el(cx, cy, r, r);
// A six-armed snowflake (strokes) of radius r with small chevrons on each arm.
const flake = (cx, cy, r) => {
	let d = "";
	for (let i = 0; i < 3; i++) {
		const a = (i * 60) * Math.PI / 180, dx = Math.sin(a) * r, dy = -Math.cos(a) * r;
		d += `M${(cx - dx).toFixed(2)} ${(cy - dy).toFixed(2)}L${(cx + dx).toFixed(2)} ${(cy + dy).toFixed(2)}`;
	}
	for (let i = 0; i < 6; i++) {
		const a = (i * 60) * Math.PI / 180, k = r * 0.62, w = r * 0.28;
		const px = cx + Math.sin(a) * k, py = cy - Math.cos(a) * k;
		const ox = Math.sin(a) * r * 0.82, oy = -Math.cos(a) * r * 0.82;
		const nx = Math.cos(a) * w, ny = Math.sin(a) * w;
		d += `M${(px - nx).toFixed(2)} ${(py - ny).toFixed(2)}L${(cx + ox).toFixed(2)} ${(cy + oy).toFixed(2)}L${(px + nx).toFixed(2)} ${(py + ny).toFixed(2)}`;
	}
	return d;
};
const shard = (cx, top, h, w) => `M${cx} ${top}l${w / 2} ${h * 0.32}l${-w / 2} ${h * 0.68}l${-w / 2} ${-h * 0.68}z`;

// ---------- path nodes (fire from the Paths mockup; frost drawn to match)
const fire = {
	fire_bolt: { a: "M4 20c4-1 8-5 10-9l-3-1 6-7-1 6 3 1c-3 6-8 10-15 10z", b: "M4.6 13.2a1.4 1.4 0 1 1 0 2.8 1.4 1.4 0 0 1 0-2.8zM8.4 9.4a1 1 0 1 1 0 2 1 1 0 0 1 0-2z" },
	heat_capacity: { t: "M9.5 3h5M10.5 3v5.6a6.5 6.5 0 1 0 3 0V3", a: fl(12, 10.6, 0.46) },
	ignite_chance: { a: "M12 2l2.2 7.8L22 12l-7.8 2.2L12 22l-2.2-7.8L2 12l7.8-2.2z", c: "M12 9.6a2.4 2.4 0 1 1 0 4.8 2.4 2.4 0 0 1 0-4.8z" },
	flame_slash: { b: "M5 19L17 7l2-4-4 2L3 17z", a: "M13.5 13.5c3 .3 5.8 2.8 6.5 7-2.3-1.5-4.3-2.2-6.8-2.2 1-1.4 1.1-3.2.3-4.8z" },
	spreading_flames: { a: fl(12, 3, 0.78), b: fl(5, 11, 0.42) + fl(19, 11, 0.42), t: "M3.5 21h17" },
	searing_heat: { a: fl(12, 5.5, 0.8), t: "M4.5 4c1.3 1.3-1.3 2.7 0 4s-1.3 2.7 0 4M19.5 4c1.3 1.3-1.3 2.7 0 4s-1.3 2.7 0 4" },
	fire_wall: { a: "M3.5 15C3 12.4 4.6 10.8 5.8 9.4c.3 1.6 1 2.4 2 2.7-.2-2.8 1-5.2 3-6.8-.2 2.5.8 4.2 2 5.2.2-1.6 1-2.6 2.2-3.3-.2 2 .6 3.5 1.8 4.3.2-1 .8-1.8 1.6-2.2 1 1.6 1.9 3.5 1.6 5.7z", b: "M3 16h8.5v2.3H3zM12.5 16H21v2.3h-8.5zM3 19.2h4.2v2.3H3zM8.2 19.2h7.6v2.3H8.2zM16.8 19.2H21v2.3h-4.2z" },
	long_burn: { t: "M7 3h10M7 21h10M8 3c0 4.6 3.4 6 3.4 9S8 16.4 8 21M16 3c0 4.6-3.4 6-3.4 9s3.4 4.4 3.4 9", a: fl(12, 14.2, 0.36) },
	fuel_the_fire: { b: "M3.2 18.6l16.6-3.9.9 2.6-16.6 3.9zM20.8 18.6L4.2 14.7l-.9 2.6 16.6 3.9z", a: fl(12, 2.5, 0.72) },
	wildfire: { a: "M12 1.5l1.9 4.6 3.6-2.9-.4 4.6 4.6-.5-2.8 3.7 3.6 2.6-4.4 1.2 1.6 4.4-4.3-1.8L12 22.5l-1.4-5.1-4.3 1.8 1.6-4.4-4.4-1.2 3.6-2.6-2.8-3.7 4.6.5-.4-4.6 3.6 2.9z", c: fl(12, 8, 0.45) },
	explosive_bolt: { a: "M12 2l2 5.5 5-3-2.4 5.3L22 12l-5.4 2.2 2.4 5.3-5-3L12 22l-2-5.5-5 3 2.4-5.3L2 12l5.4-2.2L5 4.5l5 3z", c: "M12 9a3 3 0 1 1 0 6 3 3 0 0 1 0-6z" },
	shockwave: { t: "M4.5 19.5A10.6 10.6 0 0 1 4.5 4.5M19.5 4.5A10.6 10.6 0 0 1 19.5 19.5", s: "M6.5 17.5A7.8 7.8 0 0 1 6.5 6.5M17.5 6.5A7.8 7.8 0 0 1 17.5 17.5", a: "M12 9.6a2.4 2.4 0 1 1 0 4.8 2.4 2.4 0 0 1 0-4.8z" },
	lava_pool: { a: "M2.5 17.5a9.5 3.5 0 1 0 19 0 9.5 3.5 0 1 0-19 0z", b: "M8 10.5a1.7 1.7 0 1 1 0 3.4 1.7 1.7 0 0 1 0-3.4zM14.5 7.5a2.3 2.3 0 1 1 0 4.6 2.3 2.3 0 0 1 0-4.6zM11.2 3a1.2 1.2 0 1 1 0 2.4 1.2 1.2 0 0 1 0-2.4z" },
	crushing_blast: { t: "M2.5 20.5h19M8 18.2l-2.6-2M16 18.2l2.6-2", a: "M7.5 2.5h9V9H20l-8 8-8-8h3.5z" },
	overheat: { t: "M3.5 18a8.5 8.5 0 0 1 17 0M4.7 13.8l1.3.75M12 9.5V11M19.3 13.8l-1.3.75", s: "M12 18l5.5-5.5", a: "M12 16a2 2 0 1 1 0 4 2 2 0 0 1 0-4z", c: "M19.5 1l.9 2.5 2.6.9-2.6.9-.9 2.6-.9-2.6-2.6-.9 2.6-.9z" },
	meteor: { s: "M13 11.8L20.5 4.3M14.8 15.2L21 9M9.8 10.4L15.8 4.4", a: "M9 10.5a5.5 5.5 0 1 1 0 11 5.5 5.5 0 0 1 0-11z", c: "M7.3 14.2a1.3 1.3 0 1 1 0 2.6 1.3 1.3 0 0 1 0-2.6zM10.8 17.6a.9.9 0 1 1 0 1.8.9.9 0 0 1 0-1.8z" },
	heated_blade: { s: "M6 5c1.2 1.2-1.2 2.5 0 3.7s-1.2 2.5 0 3.7M18 5c1.2 1.2-1.2 2.5 0 3.7s-1.2 2.5 0 3.7", b: "M12 2l2 3v10.5h-4V5zM7 15.5h10v2H7zM11 17.5h2V22h-2z", a: "M12 6.5l.9 1.4v7.6h-1.8V7.9z" },
	molten_edge: { b: "M5 19L17 7l2-4-4 2L3 17z", a: "M13.2 12.6c.9 1.2 1.4 2 1.4 3a1.4 1.4 0 0 1-2.8 0c0-1 .5-1.8 1.4-3zM17.4 9.6c.7.9 1 1.5 1 2.2a1 1 0 0 1-2 0c0-.7.3-1.3 1-2.2zM9 16.6c.7.9 1 1.5 1 2.2a1 1 0 0 1-2 0c0-.7.3-1.3 1-2.2z" },
	bellows: { t: "M2.5 8.5H11a2.5 2.5 0 1 0-2.5-2.5M2.5 12.5h10M2.5 16.5H10a2.5 2.5 0 1 1-2.5 2.5", a: fl(18, 5, 0.55) },
	tempered: { a: "M2.5 6.5h13c1.2 2.4 3.4 3.5 6 3.5v1.5H16c-1 0-1.5.7-1.5 1.5v2.5H9V13c0-1-.7-1.5-1.5-1.5C4.7 11.5 2.8 9.5 2.5 6.5zM6.5 15.5h11l1.5 5.5H5z", c: "M19.5 1.5l.8 2 2 .8-2 .8-.8 2-.8-2-2-.8 2-.8z" },
	searing_parry: { b: "M12 2.5l8 3V11c0 5.5-3.5 9-8 10.5-4.5-1.5-8-5-8-10.5V5.5z", a: fl(12, 7, 0.55) },
	molten_cleave: { t: "M4 21L14.5 8", b: "M10.5 6.5L14 3c2.5 1 5.5 1 8-.5-.5 3.5-2 6.5-4.5 8.5z", a: "M11 20.2a5.5 1.8 0 1 0 11 0 5.5 1.8 0 1 0-11 0zM17.5 11.5c.8 1.1 1.3 1.9 1.3 2.8a1.3 1.3 0 0 1-2.6 0c0-.9.5-1.7 1.3-2.8z" },
};
const frost = {
	frost_shard: { a: shard(13, 2, 20, 8), b: shard(6, 9, 10, 4.4), t: "M13 2v20" },
	chill_capacity: { t: "M9.5 3h5M10.5 3v5.6a6.5 6.5 0 1 0 3 0V3", a: shard(12, 11, 9, 5.2) },
	frostbite: { s: flake(12, 12, 8.5), t: "M3 4c1.3 1.3-1.3 2.7 0 4s-1.3 2.7 0 4M21 4c1.3 1.3-1.3 2.7 0 4s-1.3 2.7 0 4" },
	ice_slash: { b: "M5 19L17 7l2-4-4 2L3 17z", a: shard(17, 11, 11, 5.5) },
	deep_freeze: { t: "M12 2.5a9.5 9.5 0 1 1 0 19 9.5 9.5 0 0 1 0-19z", s: flake(12, 12, 6) },
	brittle: { a: shard(12, 2, 20, 11), c: "M12.6 5.5l-2 5 2.4 2.6-2 4.4.5.2 2.4-4.6-2.3-2.6 1.6-4.8z" },
	ice_lance: { t: "M3.5 20.5L14 10", a: "M12.5 8.5L21.5 2.5l-6 9z", b: "M10 9l5 5-1.4 1.4-5-5z" },
	hard_break: { t: "M2.5 20.5h19M8 18.2l-2.6-2M16 18.2l2.6-2", a: shard(12, 1.5, 15, 9) },
	cold_return: { s: "M18.5 8.5A7.5 7.5 0 1 0 19.5 13", b: "M15 3.5l5.5.5-.5 5.5z", a: shard(12, 8, 9, 5) },
	chain_shatter: { a: "M12 1.5l1.9 4.6 3.6-2.9-.4 4.6 4.6-.5-2.8 3.7 3.6 2.6-4.4 1.2 1.6 4.4-4.3-1.8L12 22.5l-1.4-5.1-4.3 1.8 1.6-4.4-4.4-1.2 3.6-2.6-2.8-3.7 4.6.5-.4-4.6 3.6 2.9z", c: shard(12, 7.5, 9, 5) },
	frost_nova: { a: "M12 2l2 5.5 5-3-2.4 5.3L22 12l-5.4 2.2 2.4 5.3-5-3L12 22l-2-5.5-5 3 2.4-5.3L2 12l5.4-2.2L5 4.5l5 3z", c: shard(12, 8.5, 7, 4.4) },
	biting_cold: { t: "M2.5 8.5H11a2.5 2.5 0 1 0-2.5-2.5M2.5 12.5h10M2.5 16.5H10a2.5 2.5 0 1 1-2.5 2.5", s: flake(17.5, 12, 4.5) },
	wide_storm: { t: "M4.5 19.5A10.6 10.6 0 0 1 4.5 4.5M19.5 4.5A10.6 10.6 0 0 1 19.5 19.5", s: "M6.5 17.5A7.8 7.8 0 0 1 6.5 6.5M17.5 6.5A7.8 7.8 0 0 1 17.5 17.5", a: shard(12, 8, 8, 4.6) },
	ice_wall: { a: "M3.5 15l2.2-7 2.2 7zM9.7 15L12 3.5l2.3 11.5zM16.1 15l2.2-7 2.2 7z", b: "M3 16h8.5v2.3H3zM12.5 16H21v2.3h-8.5zM3 19.2h4.2v2.3H3zM8.2 19.2h7.6v2.3H8.2zM16.8 19.2H21v2.3h-4.2z" },
	whiteout: { s: "M2.5 12c2.5-4 5.8-6 9.5-6s7 2 9.5 6c-2.5 4-5.8 6-9.5 6s-7-2-9.5-6z", a: "M12 9.5a2.5 2.5 0 1 1 0 5 2.5 2.5 0 0 1 0-5z", b: circ(4, 4, 1.3) + circ(19.5, 3.5, 1) + circ(20, 20, 1.3) + circ(4.5, 20.5, 1) },
	blizzard: { t: "M2.5 7h9.5a2.5 2.5 0 1 0-2.5-2.5M2.5 11h6M2.5 15h9.5a2.5 2.5 0 1 1-2.5 2.5", s: flake(17.5, 13, 4.5) + flake(19.5, 4, 2.2) },
	keen_edge: { b: "M12 2l2 3v10.5h-4V5zM7 15.5h10v2H7zM11 17.5h2V22h-2z", a: "M12 6.5l.9 1.4v7.6h-1.8V7.9z", c: "M19.5 1.5l.8 2 2 .8-2 .8-.8 2-.8-2-2-.8 2-.8zM4.5 6l.6 1.5 1.5.6-1.5.6-.6 1.5-.6-1.5-1.5-.6 1.5-.6z" },
	flurry: { s: "M4 13.5L13.5 4M5.5 18.5L18.5 5.5M10.5 20L20 10.5", b: shard(19, 14, 8, 4) },
	light_step: { a: "M20 3.5c-6 .5-11 4.5-12.5 10.5L6 20l1 .5 2.6-4.3c5.4-1 9.4-6.2 10.4-12.7z", t: "M3 21h10" },
	chilling_touch: { a: "M8 21v-6.5L5.5 11c-.8-1.1.8-2.4 1.7-1.4L9 11.5V4.5a1.2 1.2 0 0 1 2.4 0V10h.6V3.2a1.2 1.2 0 0 1 2.4 0V10h.6V4.5a1.2 1.2 0 0 1 2.4 0V11h.4V7a1.2 1.2 0 0 1 2.4 0v7.5c0 3.5-2 6.5-5 6.5z", c: shard(14.2, 12, 6, 3.6) },
	cold_riposte: { b: "M12 2.5l8 3V11c0 5.5-3.5 9-8 10.5-4.5-1.5-8-5-8-10.5V5.5z", a: shard(12, 6, 11, 6.4) },
	frozen_moment: { t: "M7 3h10M7 21h10M8 3c0 4.6 3.4 6 3.4 9S8 16.4 8 21M16 3c0 4.6-3.4 6-3.4 9s3.4 4.4 3.4 9", a: shard(12, 13.5, 6.5, 4) },
};
// ---------- life skills (Skills mockup)
const skills = {
	skill_mining: { t: "M16 8L3.5 20.5", a: "M8 3Q20.5 2.5 21 16Q16 8 8 3z" },
	skill_woodcutting: { t: "M4 21L15.2 3.8", a: "M14.4 4.9L21.6 6.5Q21.5 11.5 16.8 14.1L12.3 8.2z" },
	skill_fishing: { s: "M14.5 2v10.5a4.75 4.75 0 0 1-9.5 0V11M5 11l2.4 2.4", t: "M3 21.5c1.5 0 2-1 3.5-1s2 1 3.5 1 2-1 3.5-1 2 1 3.5 1 2-1 3.5-1" },
	skill_tracking: { a: el(8, 19.5, 1.9, 2.7) + el(13, 15, 1.9, 2.7) + el(9.5, 9.8, 1.9, 2.7) + el(14.5, 5, 1.9, 2.7) },
	skill_smithing: { a: "M2.5 6.5h13c1.2 2.4 3.4 3.5 6 3.5v1.5H16c-1 0-1.5.7-1.5 1.5v2.5H9V13c0-1-.7-1.5-1.5-1.5C4.7 11.5 2.8 9.5 2.5 6.5zM6.5 15.5h11l1.5 5.5H5z", c: "M19.5 1.5l.8 2 2 .8-2 .8-.8 2-.8-2-2-.8 2-.8z" },
	skill_armorcraft: { a: "M7 3l2.5 1.5h5L17 3l4 3-2 4.5V20c-2 1-4.5 1.5-7 1.5S7 21 5 20V10.5L3 6z", c: "M11.2 8h1.6v10.5h-1.6z" },
	skill_alchemy: { t: "M9.5 2.5h5M10.5 2.5v6.2L4.6 19.5c-.8 1.6.3 3.3 2.1 3.3h10.6c1.8 0 2.9-1.7 2.1-3.3l-5.9-10.8V2.5", a: "M7.2 15h9.6l2.6 4.6c.7 1.4-.3 3-1.9 3H6.5c-1.6 0-2.6-1.6-1.9-3z", c: "M11 11.5a1 1 0 1 1 0 2 1 1 0 0 1 0-2zM13.4 9.2a.8.8 0 1 1 0 1.6.8.8 0 0 1 0-1.6z" },
	skill_cooking: { t: "M9.5 4c-.8-.8.8-1.6 0-2.4M14.5 4c-.8-.8.8-1.6 0-2.4", a: "M3 6h18v1.8H3zM4.5 7.8h15V9c0 3.8-3.2 6.8-7.5 6.8S4.5 12.8 4.5 9z", c: "M8 18c.8 1 1.5 1.6 1.5 2.4a1.5 1.5 0 0 1-3 0c0-.8.7-1.4 1.5-2.4zM12 17.3c1 1.2 1.8 1.9 1.8 2.9a1.8 1.8 0 0 1-3.6 0c0-1 .8-1.7 1.8-2.9zM16 18c.8 1 1.5 1.6 1.5 2.4a1.5 1.5 0 0 1-3 0c0-.8.7-1.4 1.5-2.4z" },
	skill_farming: { t: "M12 21.5V11", a: "M12 13C12 8.2 8.6 5.5 3.5 5.5c0 4.5 3.5 7.5 8.5 7.5zM12 11c0-4.5 3.5-7.5 8.5-7.5 0 4.5-3.5 7.5-8.5 7.5z", b: "M5 21.5c1.5-2 4-3 7-3s5.5 1 7 3z" },
	skill_husbandry: { a: "M12 3c3 0 5.5 4.5 5.5 8.5S15 17 12 17s-5.5-1.5-5.5-5.5S9 3 12 3z", c: "M3 15.5c2 3.5 5.5 5 9 5s7-1.5 9-5c-3 1.2-6 1.8-9 1.8s-6-.6-9-1.8z" },
	skill_building: { a: "M3 11L12 3.5 21 11v1.5h-2V21H5v-8.5H3z", c: "M10 15h4v6h-4z" },
	skill_engineering: { a: "M10.5 2h3l.5 2.6 1.8.8 2.2-1.5 2.1 2.1-1.5 2.2.8 1.8L22 10.5v3l-2.6.5-.8 1.8 1.5 2.2-2.1 2.1-2.2-1.5-1.8.8-.5 2.6h-3l-.5-2.6-1.8-.8-2.2 1.5-2.1-2.1 1.5-2.2-.8-1.8L2 13.5v-3l2.6-.5.8-1.8-1.5-2.2 2.1-2.1 2.2 1.5 1.8-.8zM12 8.5a3.5 3.5 0 1 1 0 7 3.5 3.5 0 0 1 0-7z" },
	skill_taming: { a: "M12 12.5c3 0 5.5 2.6 5.5 5.2 0 2-1.8 2.9-3.2 2.5-1-.3-1.6-.6-2.3-.6s-1.3.3-2.3.6c-1.4.4-3.2-.5-3.2-2.5 0-2.6 2.5-5.2 5.5-5.2z" + el(6, 10.5, 1.9, 2.5) + el(9.8, 6.3, 1.9, 2.5) + el(14.2, 6.3, 1.9, 2.5) + el(18, 10.5, 1.9, 2.5) },
	skill_swimming: { s: "M2.5 9c2 0 2.5-1.5 4.75-1.5S9.75 9 12 9s2.5-1.5 4.75-1.5S19.25 9 21.5 9M2.5 14c2 0 2.5-1.5 4.75-1.5S9.75 14 12 14s2.5-1.5 4.75-1.5S19.25 14 21.5 14M2.5 19c2 0 2.5-1.5 4.75-1.5S9.75 19 12 19s2.5-1.5 4.75-1.5S19.25 19 21.5 19" },
	skill_sneaking: { s: "M2.5 12c2.5-4 5.8-6 9.5-6s7 2 9.5 6c-2.5 4-5.8 6-9.5 6s-7-2-9.5-6z", a: "M12 9.5a2.5 2.5 0 1 1 0 5 2.5 2.5 0 0 1 0-5z", c: "M3.6 19.2L19.2 3.6l1.2 1.2L4.8 20.4z" },
};

const glyphs = [];
for (const [id, g] of Object.entries(fire)) glyphs.push({ id: "path_" + id, kind: "duo", px: 64, g, tw: 1.8, sw: 1.8 });
for (const [id, g] of Object.entries(frost)) glyphs.push({ id: "path_" + id, kind: "duo", px: 64, g, tw: 1.8, sw: 1.8 });
for (const [id, g] of Object.entries(skills)) glyphs.push({ id, kind: "duo", px: 64, g, tw: 2.2, sw: 2 });

// ---------- single-colour marks (tinted in the UI)
const F = (d, rule) => ({ d, fill: rule || "nonzero" });
const S = (d, w) => ({ d, stroke: w });
const CUT = (op) => ({ ...op, cut: true });
const masks = {
	flame: [F("M12 2c3 4 6 6 6 11a6 6 0 0 1-12 0c0-3 2-5 3-6 0 2 1 4 3 4-1-4 0-7 0-9z")],
	snowflake: [S("M12 3v18M4.2 7.5l15.6 9M4.2 16.5l15.6-9M9.5 4.5L12 7l2.5-2.5M9.5 19.5L12 17l2.5 2.5", 2)],
	lock: [S("M8 11V8a4 4 0 0 1 8 0v3", 2.8), F("M5.5 11h13v10h-13z")],
	check: [S("M5 12.5l4.5 4.5L19 7.5", 3.4)],
	plus: [S("M12 5v14M5 12h14", 3.6)],
	diamond: [F("M12 1.5l10.5 10.5L12 22.5 1.5 12z")],
	diamond_line: [S("M12 2.5l9.5 9.5L12 21.5 2.5 12z", 2.2)],
	camp: [F("M3 20L12 4l9 16z"), CUT(F("M12 12l-3 8h6z"))],
	cave: [F("M2 21v-8a10 10 0 0 1 20 0v8h-6v-6a4 4 0 0 0-8 0v6z")],
	skull: [F("M12 3c-5 0-8 3.5-8 8 0 3 1.5 4.5 3 5.5V20h10v-3.5c1.5-1 3-2.5 3-5.5 0-4.5-3-8-8-8z"), CUT(F(circ(9, 11, 1.9) + circ(15, 11, 1.9))), CUT(S("M10 17v3.5M14 17v3.5", 1.2))],
	house: [F("M2.5 11.5L12 3.5l9.5 8V21h-6.5v-6h-6v6H2.5z")],
	paw: [F(circ(6.5, 10, 2.3) + circ(10.5, 5.8, 2.3) + circ(15.5, 6, 2.3) + circ(19, 10.4, 2.2)), F("M7.5 17.5c0-3.2 2.4-6 5-6s5 2.8 5 5.6c0 2.6-1.9 3.6-3.5 3.1-1-.3-1.5-.6-1.5-.6s-.6.4-1.7.7c-1.6.4-3.3-.4-3.3-2.8z")],
	person: [F(circ(12, 7.5, 4)), F("M4 21c0-4.5 3.6-8 8-8s8 3.5 8 8z")],
	mountain: [F("M3 20l6-11 4 6 2.5-3.5L21 20z")],
	flag: [S("M6 3v18.5", 2.2), F("M7 3.5h12l-3.2 4.5L19 12.5H7z")],
	target: [S(circ(12, 12, 6), 2), F(circ(12, 12, 2)), S("M12 2v4M12 18v4M2 12h4M18 12h4", 2)],
	mouse_left: [S("M12 2.5a6 6 0 0 1 6 6v7a6 6 0 0 1-12 0v-7a6 6 0 0 1 6-6zM6 10h12M12 2.5V10", 1.6), F("M12 2.8a5.7 5.7 0 0 0-5.7 5.7V10H12z")],
	mouse_wheel: [S("M12 2.5a6 6 0 0 1 6 6v7a6 6 0 0 1-12 0v-7a6 6 0 0 1 6-6zM6 10h4.6M13.4 10H18", 1.6), F("M12 5a1.4 1.4 0 0 1 1.4 1.4v2.7a1.4 1.4 0 0 1-2.8 0V6.4A1.4 1.4 0 0 1 12 5z")],
	mouse_right: [S("M12 2.5a6 6 0 0 1 6 6v7a6 6 0 0 1-12 0v-7a6 6 0 0 1 6-6zM6 10h12M12 2.5V10", 1.6), F("M12 2.8a5.7 5.7 0 0 1 5.7 5.7V10H12z")],
	ring: [S(circ(12, 12, 10.5), 1.2)],
};
const maskPx = { flame: 48, snowflake: 48, lock: 32, check: 32, plus: 32, diamond: 32, diamond_line: 32, ring: 96, mouse_left: 40, mouse_wheel: 40, mouse_right: 40 };
for (const [id, ops] of Object.entries(masks)) glyphs.push({ id: id, kind: "mask", px: maskPx[id] || 40, ops });

// ---------- full colour
glyphs.push({ id: "map_player", kind: "color", px: 64, box: 24, ops: [
	{ d: "M12 1.5l7.5 20-7.5-4.6-7.5 4.6z", fill: "#f0c878", stroke: "#2a1a08", w: 1.4 },
	{ d: "M12 1.5v15.4l7.5 4.6z", fill: "#b8873e" },
] });
glyphs.push({ id: "map_compass", kind: "color", px: 200, box: 100, ops: [
	{ d: circ(50, 50, 31), fill: "rgba(12,14,16,0.72)", stroke: "#8e6b38", w: 2 },
	{ d: circ(50, 50, 25), stroke: "#8e6b38", w: 1, dash: [1.5, 3] },
	{ d: "M50 50L61 39M50 50L61 61M50 50L39 61M50 50L39 39", stroke: "#6e5530", w: 2.4, cap: "round" },
	{ d: "M50 14L55 45L50 50z", fill: "#f0c878" }, { d: "M50 14L45 45L50 50z", fill: "#b8873e" },
	{ d: "M50 86L55 55L50 50z", fill: "#8e6b38" }, { d: "M50 86L45 55L50 50z", fill: "#5a4426" },
	{ d: "M86 50L55 45L50 50z", fill: "#8e6b38" }, { d: "M86 50L55 55L50 50z", fill: "#5a4426" },
	{ d: "M14 50L45 45L50 50z", fill: "#8e6b38" }, { d: "M14 50L45 55L50 50z", fill: "#5a4426" },
	{ d: circ(50, 50, 3.5), fill: "#cfa35e", stroke: "#1a1206", w: 1 },
] });
glyphs.push({ id: "map_pin", kind: "color", px: 48, box: 30, ops: [
	{ d: "M15 1.5a8.5 8.5 0 0 0-8.5 8.5c0 6.5 8.5 18.5 8.5 18.5s8.5-12 8.5-18.5A8.5 8.5 0 0 0 15 1.5z", fill: "#cfa35e", stroke: "#1a1206", w: 1.4 },
	{ d: circ(15, 10, 3.2), fill: "#1a1206" },
] });

// ---------- render in headless Edge
const page = `<!doctype html><html><body><pre id="out"></pre><script>
const G = ${JSON.stringify(glyphs)};
const res = {};
function canvas(px) { const c = document.createElement("canvas"); c.width = px; c.height = px; return c; }
function draw(g, ops, box) {
	g.save(); g.scale(g.canvas.width / box, g.canvas.height / box);
	for (const o of ops) {
		const p = new Path2D(o.d);
		g.globalCompositeOperation = o.cut ? "destination-out" : "source-over";
		g.lineCap = o.cap || "round"; g.lineJoin = "round";
		if (o.fill !== undefined && o.fill !== null) { g.fillStyle = typeof o.fill === "string" && o.fill[0] !== "n" && o.fill[0] !== "e" ? o.fill : "#fff"; g.fill(p, o.fill === "evenodd" || o.rule === "evenodd" ? "evenodd" : "nonzero"); }
		if (o.stroke !== undefined) { g.setLineDash(o.dash || []); g.strokeStyle = typeof o.stroke === "string" ? o.stroke : "#fff"; g.lineWidth = typeof o.stroke === "number" ? o.stroke : o.w; g.stroke(p); g.setLineDash([]); }
	}
	g.restore();
}
for (const s of G) {
	if (s.kind === "duo") {
		const g = s.g;
		const a = canvas(s.px), p = canvas(s.px);
		const opsA = [], opsP = [];
		if (g.t) opsA.push({ d: g.t, stroke: s.tw });
		if (g.b) opsA.push({ d: g.b, fill: "nonzero" });
		if (g.c) opsA.push({ d: g.c, fill: "nonzero" });
		if (g.s) opsP.push({ d: g.s, stroke: s.sw });
		if (g.a) opsP.push({ d: g.a, fill: "evenodd" });
		if (g.c) opsP.push({ d: g.c, fill: "nonzero", cut: true });
		draw(a.getContext("2d"), opsA, 24); draw(p.getContext("2d"), opsP, 24);
		res[s.id + "_a"] = a.toDataURL("image/png"); res[s.id + "_p"] = p.toDataURL("image/png");
	} else if (s.kind === "mask") {
		const c = canvas(s.px); draw(c.getContext("2d"), s.ops, 24); res[s.id] = c.toDataURL("image/png");
	} else {
		const c = canvas(s.px); draw(c.getContext("2d"), s.ops.map((o) => ({ ...o, stroke: o.stroke || (o.w ? o.stroke : undefined) })), s.box); res[s.id] = c.toDataURL("image/png");
	}
}
document.getElementById("out").textContent = JSON.stringify(res);
</script></body></html>`;
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "wr-glyphs-"));
const html = path.join(tmp, "glyphs.html");
fs.writeFileSync(html, page);
const edge = ["C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe", "C:/Program Files/Google/Chrome/Application/chrome.exe"].find((p) => fs.existsSync(p));
const dom = execFileSync(edge, ["--headless=new", "--disable-gpu", "--no-first-run", "--user-data-dir=" + path.join(tmp, "profile"), "--dump-dom", "file:///" + html.replace(/\\/g, "/")], { encoding: "utf8", maxBuffer: 64 << 20 });
const json = dom.slice(dom.indexOf('<pre id="out">') + 14, dom.indexOf("</pre>")).replace(/&quot;/g, '"').replace(/&amp;/g, "&");
const res = JSON.parse(json);
let n = 0;
for (const [id, url] of Object.entries(res)) { fs.writeFileSync(path.join(out, id + ".png"), Buffer.from(url.split(",")[1], "base64")); n++; }
fs.rmSync(tmp, { recursive: true, force: true });
console.log(`${n} glyph images in ${out}`);
