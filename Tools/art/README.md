# Creature art pipeline (all local, free)

Runs on the RTX 4070 Ti. Python env + models live in `C:\Dev\WildreachArt` (not in git: ~35 GB).
Env: `HF_HOME=C:\Dev\WildreachArt\hf`, `HF_HUB_OFFLINE=1`, `HY3DGEN_MODELS=C:\Dev\WildreachArt\models`, CUDA 12.6 `bin` on PATH.

Batch flow (what the four-legged set used):

1. Concepts: `python concept_batch.py batch_quadrupeds.json [--only id1,id2] [--seed 31]` -> `out\<id>_0..3.png` + `out\<id>_sheet.png`
   (SDXL base 1.0, commercial use OK). Keep prompts short: CLIP drops anything past 77 tokens. Pick standing, clean side views;
   crouched or front-on poses turn out squashed. A batch file can set `stance` and `negative` (`batch_hexapods.json` asks for
   six legs and bans "four legs"); SDXL miscounts legs, so use `--count 8` there and pick ones with every foot separate.
2. Shape + texture: `python build3d_batch.py id:index ...` -> `out\<id>_shape.glb`, `out\<id>_textured.glb`
   (Hunyuan3D-2 + Paint turbo, Tencent Hunyuan 3D 2.0 Community License: not licensed in the EU, UK or South Korea;
   commercial use allowed below 1M monthly users). Needs transformers 4.49 / diffusers 0.33.1 / huggingface_hub 0.30.2.
3. Rig + animations: `blender -b -P rig_quadruped.py -- out\<id>_textured.glb out\<id>.fbx --head +x --preview out\<id>_walk.png`
   (finds the 4 feet, aligns the body, builds the skeleton, auto-weights, bakes Idle/Walk/Run/Attack/Hit/Death).
   Right-facing concepts come out head +X. Three-quarter or left-facing ones land on any axis: check `_walk_0.png`
   (side view, head should be on the right) and re-run with `--head -x|+y|-y` as needed. Those also come out short-bodied:
   `--stretch 1.3` lengthens just the torso between the legs (Rimeclaw uses it). `--legs 6` rigs front / middle / rear
   pairs with a tripod gait (insects come out of SDXL with six legs). `--add-mid-legs` turns a four-legged model into a
   six-legged one by copying the front legs halfway back (SDXL never draws six-legged mammals: Cinderfang, Woolly drudge).
   `--gait hop` gives critters a bounding gait instead of a trot. `--legs 2` rigs birds (alternating steps with a
   waddle; feet that overlap mid-stride are split front/back).
   Worms and moths use `rig_special.py -- in.glb out.fbx --body worm|moth [--head ..] [--hover 4]`: a 10-bone spine
   that slithers and rears to strike, or a flat moth (concept seen from above) with hinged flapping wings that hovers.
4. Import: `WR_IMPORTS="C:\...\out\a.fbx=A;C:\...\out\b.fbx=B"` then
   `UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script=Tools/import_creature.py`.
   The folder name lower-cased must equal the FBX name (`aurora_fox.fbx=Aurora_fox`). To replace a creature, move its
   `Content/Creatures/<Name>` folder out of the project first (the importer skips existing folders: a re-import would keep
   the old texture and animations).
5. Wire: `node Tools/art/wire_creatures.mjs id=Folder[:meshYaw] ...` fills `mesh` / `anims` in `Data/creatures.json`.
6. Check in game: `powershell -File Tools\creatureshots.ps1 id1 id2 ...` (two `WRArtCam` angles per creature, burrowers kept visible).

Pack creatures (Fab, git-ignored): `node Tools/art/wire_pack_creatures.mjs` points Gloomhowl (Barghest), Scorchwing
and Skaldrith (Mountain Dragon) at the pack meshes and animations, with tinted materials from
`Tools/art/creature_tints.py` (`M_CreatureTint` + one instance each). Flyers can add `fly` / `hover` animations.

Props (static, no rig), e.g. the island trees:

1. `python concept_batch.py batch_trees.json --count 8` (the batch's `template` replaces the creature prompt), pick,
   copy picks to unique ids (`out\char_tree_a_0.png`), then `python build3d_batch.py char_tree_a:0 ...`.
2. Check with `blender -b -P render_preview.py -- C:\full\path\model.glb C:\full\path\out.png` (absolute paths: relative
   ones land in `C:\out`). Cut off a plinth or ground slab with `blender -b -P crop_bottom.py -- in.glb out.glb 0.08`.
3. `WR_IMPORTS="C:\...\out\pine_a_cropped.glb=PineA"` then `UnrealEditor-Cmd ... -script=Tools/import_prop.py`
   -> `/Game/Props/PineA` (Nanite on, no collision), and add the mesh to the `Art` table in `WRWorldDressing.cpp`.

Caves (procedural, no AI): `blender -b -P cave_gen.py -- C:\...\out\cave_a.fbx --seed 1 --preview C:\...\out\cave_a.png`
sculpts a noisy rock mound and carves the chamber, tunnel and a recessed entrance out of it (AWRCave's local layout,
open bottom). Import with `WR_IMPORTS="C:\...\cave_a.fbx=CaveA"` and `Tools/import_cave.py` (mesh collision, no Nanite).

Single-item tools: `concept.py`, `shape.py`, `texture.py`; preview any model with `blender -b -P render_preview.py -- model.glb out.png`.
