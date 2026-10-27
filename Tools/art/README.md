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

Humanoids (player survivors, humanoid bosses), rigged and animated on mixamo.com (Raven's Adobe login):

1. Concepts in a T-pose: `python concept_batch.py batch_humanoids_t.json --count 8`. Mixamo fails ("Unknown error while
   generating motion") when the arms touch the body, so arms-down concepts (`batch_humanoids.json`) won't rig.
   Build3d as usual, then `blender -b -P prep_mixamo.py -- out\x_textured.glb mixamo\x.fbx 1.8` (welds the texture-seam
   pieces into one closed mesh, feet at the origin, real height) and `blender -b -P check_arms.py -- mixamo\x.fbx`
   (slices at chest height should show 3 shapes: arms clear of the body).
2. Mixamo's page blocks fetching local files, so either Raven picks the file in the upload box, or a decimated copy goes
   in through the page itself: `blender -b -P pack_for_page.py -- mixamo\x.fbx out\x_page.b64 2400`, then decode it in
   the page into an OBJ `File` and set it on the upload input. That only works if the upload dialog was opened with a real
   click, and `window.scrollTo(0, 0)` first (the page scrolls behind the dialog and throws the marker drags off).
   Keep every marker on the mesh (elbows especially). Turn "Use Symmetry" off when the body sits off the centre line
   (a jacket flap or a longer arm shifts it). A character wider than it is tall (a hulking boss) gets cropped by the
   marker view: add a tiny triangle at 1.45x its height above the feet when building the OBJ, so the camera frames
   the whole body (the triangle is in the throwaway rig copy only).
3. Download the rigged character (Original Pose) and the animations ("Without Skin" / packs: "No Character", In Place for
   locomotion). A pack downloaded with "Original Pose" instead brings the rigged character too, as `<name>.fbx` in the
   zip next to the clips. Then `blender -b -P survivor_build.py -- rigged.fbx mixamo\x.fbx anims\pack anims\singles --out
   mixamo\game\x.fbx --preview out\x_rig.png`: copies the rig's weights onto the full-detail mesh, moves the rest pose to
   the animations' T-pose, loads every animation (named after its file, hips' horizontal travel removed), exports one
   FBX. Check with `blender -b -P check_actions.py -- mixamo\game\x.fbx out\x_act.png Idle Run Slash`.
4. `WR_DEST_ROOT=/Game/Characters WR_IMPORTS="...\x.fbx=Survivor"` + `Tools/import_creature.py`, then point
   `Data/player_art.json` at it (Unreal drops the `mixamorig:` bone prefix: the hand is `RightHand`).
   Check with `powershell -File Tools\playershots.ps1` (`WRArtCam me`; `-Body f` for the female body). More bodies go
   under `"bodies"` in `player_art.json` (the main menu's Male / Female buttons send `?Body=f` on the join URL).

Humanoid bosses (Vulkarn, Grimjaw, the Obsidian Warden) go through the same Mixamo steps with `batch_bosses_t.json`
(the concepts came out in an A-pose, which rigs as long as the arms stand clear) and Mixamo's **Creature Pack**
downloaded with "Original Pose" (idle, breathing idle, walking, run, swiping, punch, jump attack, roaring, dying,
flexing). Build with `survivor_build.py ... --cut-stretch 4`: image-to-3D brutes often have a hand fused to a thigh, and
those faces stretch into long planks in every clip; the cut drops faces whose edges stretch past 4x their length in the
model's own pose during any clip, plus the scraps left floating. Import into `/Game/Creatures/<Name>` and wire
`creatures.json` like the other creatures (`meshYaw` -90 for anything built from a Mixamo rig). "Unknown error while
generating motion" usually clears by nudging the elbow markers up to the bend and the chin down to the jaw line.

Winged figures (Ignessa): `blender -b -P rig_special.py -- out\ignessa_tb_textured.glb out\ignessa.fbx --body fae` gives
a body, a head and a hinged wing per side behind the shoulders (wings found as whatever spreads out to the sides above
the waist), with floating, swaying and wing-fanning clips; `meshYaw` 0 and `fly` / `hover` mapped to Run / Idle.

Adding clips to the survivors later (the ranged batch: bow, rifle and throw clips): download them on Mixamo's stock
**Y Bot** (same Mixamo skeleton, ~1.8 m, no upload needed) as "Without Skin" / packs "No Character", then build
animation-only FBXs for each survivor with that survivor's own first clip as the rest pose:
`blender -b -P survivor_build.py -- rigged.fbx mixamo\survivor_m.fbx anims\ranged --out mixamo\ranged_m\survivor_m.fbx
--rest-from "anims\sword_shield\sheath sword 1.fbx"` (finger tracks the rig lacks are stripped; otherwise the FBX
exporter drops the whole clip). Copy it to `survivor_m_clips.fbx` (a file named like the mesh re-imports the mesh)
and run `Tools/import_anims.py` with `WR_ANIMS="...\survivor_m_clips.fbx=/Game/Characters/Survivor/survivor_m_Skeleton=survivor_mArmature_"`:
Interchange with an animations-only pipeline onto the existing skeleton, clips renamed to the survivor's prefix. The meshes
are not touched, so Git LFS only grows by the clips.

Ranged gear: `batch_ranged.json` concepts; `normalize_gear.py -- bow ... 120` (limbs along +Y, the arrow's flight
along +X, riser at the origin), `-- pole ... --front wide|narrow --grip 0.35` (crossbow, blowgun, spear, arrow: tip along
+Y, grip a fraction from the back), `-- ball ... 45` (bola). Blender's +Y comes into Unreal as -Y, so `items.json` says
`"meshAlong": "-Y"`. `ranged.socket / along / face / rot / offset` place each weapon in the hand.

Held gear (the player's sword and shield): concepts from `batch_gear.json`, build3d, then
`blender -b -P normalize_gear.py -- sword out\sword_a_textured.glb out\sword_game.glb 95` (grip at the origin, blade along
its longest axis, 95 cm) or `-- shield ... 70` (face along +Z, 70 cm across, stray struts off the back cut away), and
`Tools/import_prop.py`. `player_art.json` `weaponMesh` / `shieldMesh` / `shieldSocket` attach them: the game works out the
blade and face directions from the mesh bounds and the skeleton's T-pose.

Caves (procedural, no AI): `blender -b -P cave_gen.py -- C:\...\out\cave_a.fbx --seed 1 --preview C:\...\out\cave_a.png`
sculpts a noisy rock mound and carves the chamber, tunnel and a recessed entrance out of it (AWRCave's local layout,
open bottom). Import with `WR_IMPORTS="C:\...\cave_a.fbx=CaveA"` and `Tools/import_cave.py` (mesh collision, no Nanite).

Base props (the stations, the Hearth's fire ring, the claim totem, the storage chest; polish pass 2026-10): concepts from
`batch_stations.json` / `batch_stations2.json` (a three-quarter studio shot of one object), build3d, then
`blender -b -P normalize_prop.py -- out\x_textured.glb out\x_game.glb --faces 14000 --tex 1024 [--yaw 90] [--crop 0.05]`
(decimates, shrinks the texture to 1k, centres it with its bottom at zero: ~1.5 MB in Git LFS instead of ~5 MB) and
`Tools/import_prop.py`. A concept with several objects can be cropped to one first (the totem was). `pieces.json` `mesh`
/ `meshYaw` / `meshFit` show it: the game scales it to fit inside meshFit (cm) and keeps the plain box as the collision
(`FWRBuildLook::AttachProp`); turn it with meshYaw so its working side faces -X (the side toward whoever placed it).

Single-item tools: `concept.py`, `shape.py`, `texture.py`; preview any model with `blender -b -P render_preview.py -- model.glb out.png`.
