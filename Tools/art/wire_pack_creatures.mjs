// Point creatures at skeletal meshes from Fab packs (git-ignored, see .gitignore) with tinted materials.
//   node Tools/art/wire_pack_creatures.mjs
// Pack meshes face +Y, so meshYaw -90 turns them to the creature's forward (+X).
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const file = path.join(root, "Data/creatures.json");
let text = fs.readFileSync(file, "utf8");
const B = "/Game/QuadrapedCreatures/Barghest/";
const D = "/Game/QuadrapedCreatures/MountainDragon/";
const ref = (dir, sub, name) => `${dir}${sub}/${name}.${name}`;
const dragon = n => ref(D, "Animations", `ANIM_MOUNTAIN_DRAGON_${n}`);
const barghest = n => ref(B, "Animations", `BARGHEST_${n}`);

const wire = {
	gloomhowl: { mesh: ref(B, "Meshes", "SK_BARGHEST"), materials: ["/Game/Creatures/Gloomhowl/MI_Gloomhowl.MI_Gloomhowl"],
		anims: { idle: barghest("idleBreathe"), walk: barghest("walk"), run: barghest("run"), attack: barghest("biteAggressive"),
			hit: barghest("getHitNormal"), death: barghest("deathNormal") } },
	scorchwing: { mesh: ref(D, "Meshes", "SK_MOUNTAIN_DRAGON"), materials: ["/Game/Creatures/Scorchwing/MI_Scorchwing.MI_Scorchwing"],
		anims: { idle: dragon("idleBreathe"), walk: dragon("walk"), run: dragon("run"), fly: dragon("flyNormal"), hover: dragon("FlyStationary"),
			attack: dragon("bite"), hit: dragon("getHitFront"), death: dragon("death") } },
	skaldrith: { mesh: ref(D, "Meshes", "SK_MOUNTAIN_DRAGON"), materials: ["/Game/Creatures/Skaldrith/MI_Skaldrith.MI_Skaldrith"],
		anims: { idle: dragon("idleBreathe"), walk: dragon("walk"), run: dragon("run"), fly: dragon("flyNormal"), hover: dragon("FlyStationary"),
			attack: dragon("ClawsAttack2HitComboForward"), hit: dragon("getHitFront"), death: dragon("death") } },
};
for (const [id, w] of Object.entries(wire)) {
	if (!fs.existsSync(path.join(root, "Content", w.mesh.split(".")[0].replace("/Game/", "") + ".uasset"))) { console.log("skip (pack missing):", id); continue; }
	const start = text.indexOf(`{ "id": "${id}"`);
	if (start < 0) { console.log("no such creature:", id); continue; }
	let depth = 0, end = start;
	for (; end < text.length; end++) { if (text[end] === "{") depth++; else if (text[end] === "}" && --depth === 0) break; }
	let entry = text.slice(start, end).replace(/,\s*"mesh":[\s\S]*$/, "");
	const anims = Object.entries(w.anims).map(([k, v]) => `"${k}": "${v}"`).join(", ");
	entry = entry.replace(/\s*$/, "") + `,\n      "mesh": "${w.mesh}", "meshYaw": -90, "materials": ${JSON.stringify(w.materials)},\n      "anims": { ${anims} } `;
	text = text.slice(0, start) + entry + text.slice(end);
	console.log("wired", id);
}
JSON.parse(text);
fs.writeFileSync(file, text);
