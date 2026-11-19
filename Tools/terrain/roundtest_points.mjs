// Points for Tools/roundtest.ps1 from Data/hearthmoot.json (game cm, "x y"): in front of the first market stall, and
// outside and inside the inn's and the nearest hall's doors. node Tools/terrain/roundtest_points.mjs -> JSON on stdout.
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const j = JSON.parse(fs.readFileSync(path.join(root, "Data/hearthmoot.json"), "utf8"));
const RAD = Math.PI / 180;
const at = (o, yaw, lx, ly) => { const c = Math.cos(yaw * RAD), s = Math.sin(yaw * RAD); return [o[0] + lx * c - ly * s, o[1] + lx * s + ly * c]; };
const xy = (p) => `${Math.round(p[0])} ${Math.round(p[1])}`;
const floors = j.floors.map(([cx, cy, hx, hy, yaw]) => ({ c: [cx, cy], hx, hy, yaw }));
const inn = floors.find((f) => Math.abs(f.hx - 900) < 50);
const hall = floors.filter((f) => Math.abs(f.hx - 1100) < 50).sort((a, b) => Math.hypot(...a.c) - Math.hypot(...b.c))[0];
// The seller stands behind the counter facing the hearth: their customer stands 2.2 m in front of them.
const s = j.people.sellers[0], sf = [s[0] + Math.cos(s[3] * RAD) * 220, s[1] + Math.sin(s[3] * RAD) * 220];
// The inn's door is at local (-3, 6) (its frame's +y faces the plaza); a hall's at local (2, 7) from its frame, whose
// floor centre is at local (2, 0).
const ho = at(hall.c, hall.yaw, -200, 0);
console.log(JSON.stringify({
	seller: xy(sf), sellerTable: s[4],
	// A walk "arrives" within 4 m: each inside point is over 4 m past its door (the inn's front wall is at y 6, the hall's
	// at 7; a hall's tables run across in front of its door, so its walk in ends against them), each outside one too.
	innOut: xy(at(inn.c, inn.yaw, -300, 1200)), innIn: xy(at(inn.c, inn.yaw, -300, 50)),
	hallOut: xy(at(ho, hall.yaw, 200, 1400)), hallIn: xy(at(ho, hall.yaw, 200, 250)),
}));
