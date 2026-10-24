// Point creatures in Data/creatures.json at their imported meshes + animations.
//   node Tools/art/wire_creatures.mjs id=Folder[:meshYaw] [id=Folder ...]
// Assumes the rig_quadruped.py naming: mesh <base>, animations <base>Rig_<Action>, base = lower-case Folder.
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..", "..");
const file = path.join(root, "Data/creatures.json");
let text = fs.readFileSync(file, "utf8");
for (const arg of process.argv.slice(2)) {
	const [id, rest] = arg.split("=");
	const [folder, yaw = "0"] = rest.split(":");
	const base = folder.toLowerCase();
	const dir = `/Game/Creatures/${folder}/`;
	if (!fs.existsSync(path.join(root, "Content/Creatures", folder, `${base}.uasset`))) { console.log("skip (not imported):", id); continue; }
	const anim = a => `${dir}${base}Rig_${a}.${base}Rig_${a}`;
	const art = `"mesh": "${dir}${base}.${base}", "meshYaw": ${Number(yaw)},\n      "anims": { "idle": "${anim("Idle")}", "walk": "${anim("Walk")}", "run": "${anim("Run")}", "attack": "${anim("Attack")}", "hit": "${anim("Hit")}", "death": "${anim("Death")}" }`;
	// Find this creature's object (one entry per id) and replace any old art fields, then append the new ones.
	const start = text.indexOf(`{ "id": "${id}"`);
	if (start < 0) { console.log("no such creature:", id); continue; }
	let depth = 0, end = start;
	for (; end < text.length; end++) { if (text[end] === "{") depth++; else if (text[end] === "}" && --depth === 0) break; }
	let entry = text.slice(start, end);
	entry = entry.replace(/,\s*"mesh":[\s\S]*$/, "");
	entry = entry.replace(/\s*$/, "") + `,\n      ${art} `;
	text = text.slice(0, start) + entry + text.slice(end);
	console.log("wired", id, "->", dir);
}
JSON.parse(text);
fs.writeFileSync(file, text);
