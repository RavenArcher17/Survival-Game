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
   Worms and moths use `rig_special.py -- in.glb out.fbx --body worm|moth [--head ..] [--hover 4] [--stretch 1.4]`: a
   10-bone spine that slithers and rears to strike, or a flat moth (concept seen from above) with hinged wings that beat
   up and down together and hovers. `--stretch` makes a worm longer but stretches everything on it too (it smeared
   Glacimaw's spikes into low fins); for a longer worm, rebuild its body instead, as Glacimaw's `gl_build.py`
   (WildreachArt\out\glacimaw_v2) does: the image model's head kept, a new ringed body grown from the neck's edge with
   crystal spikes and a pointed tail, painted into the right half of a texture twice as wide. Loose pieces automatic
   weights skip take their nearest skin's weights. A worm lies flat and slithers like a snake (waves running back down
   the body, head steady); `gl_build.py --flat` lays the body straight on the ground (the rig's clips make the curves).
   `--head-info <npz>` (gl_build writes `<out>_head.npz`: the neck, and the lower jaw's weights, hinge and chin in the
   model's own units) adds a rigid head bone and a jaw bone, so its bite lifts the front half, gapes, strikes and
   snaps shut. Its clips' paces belong in creatures.json (`pace`): the script prints the wavelength.
   Better clips onto that same skeleton (no new mesh, so LFS only grows by the clips):
   `blender -b -P anim_creature.py -- out\<id>.fbx out\<id>_clips.fbx --id <id> [--build light|heavy] [--attack bite|ram]
   [--hind-bend back|forward] [--preview <dir>] [--debug Walk:1,5,9]`. Feet stay planted while they bear weight (IK),
   the body rises, rolls and sways with its legs, the head holds steady, the tail trails on springs, and attacks wind up,
   strike in about a tenth of a second and recover. Gaits come from the creature's size and speeds in `creatures.json`
   (walk / trot or amble / gallop by Froude number), strides from the legs' reach, so the clips play near rate 1 in game.
   `--gait hop`: a hopper (Pumicehog) bounds at every speed instead, front paws landing together, then the hind feet.
   `--lift 0.6` lowers the paws' clearance in the swing (Rimeclaw: at full lift its forearms swung up level).
   `--motion barghest:<dir>` retargets the QuadrapedCreatures Barghest clips instead (exported to FBX from Unreal; the
   pack has no trot, so at our wander speeds its walk runs fast). Import with `Tools/import_anims.py`
   (`WR_ANIMS="...\<id>_clips.fbx=/Game/Creatures/<Name>/<id>_Skeleton=<id>V2_"`). Check the rig first: auto-weights on
   a bad leg layout (three-quarter concepts; Cinderfang's rig took its tail tip for a foot) stretch the mesh whatever
   the animation.
   Front legs stand as straight columns under the shoulders and fold at the wrist, low on the leg (`rig_quadruped.py`
   puts the front knee there; a fold halfway down the forearm reads as an extra joint); hind legs stand angled at the
   hock and the back stays near level. A long-legged model can still fold above its real wrist: `--front-wrist 0.11`
   puts the front knee at the top of the paw. But a long forearm on a short pastern then folds the paw right back up
   in every swing (the leg can only shorten there): `--front-elbow` puts the joint at the elbow under the chest, apex
   back, and `anim_creature.py` (it spots an elbow by its height) swings the forearm and paw as one straight piece,
   the paw flat again to touch down (Cinderfang). Look at each side alone to see the legs and tail clearly
   (`halves.py` in WildreachArt\out\cinderfang_v3 renders the y > 0 and y < 0 halves).
   Fixing a model's shape while rigging (all on the mesh, before the bones go in):
   `--mirror-leg x,y` replaces the leg standing at x,y with a mirror copy of the one opposite (Cinderfang's front right
   was what was left of a sixth leg, rooted behind the shoulder); `--straighten-front` stands crooked front legs up as
   columns (it moves the feet, so check the hips it then traces); `--legs-longer 1.2` / `--legs-thicker 1.15` (the body
   rides up, so a deep chest stops meeting the front legs: Woolly drudge; raise its `halfHeight` by the same share to keep
   its size); `--head-forward 0.09` brings a head jammed against the body out on a longer neck (Basaltback);
   `--widen 1.3` for a body too narrow side to side (or `meshWiden` in `creatures.json`, no re-rig: Rimeshell).
   To re-rig a model whose legs were fixed in an earlier rig, export that rig's mesh back to a GLB first
   (`fbx_to_glb.py -- out\<id>.fbx out\<id>_4leg.glb`): an old `--drop-leg` spot stops matching once the foot finder
   changes.
   Detail the image models don't have: `blender -b -P bake_detail.py -- out\<id>.fbx out\<id>_detail --kind
   scales|feathers|fur [--size 0.03] [--depth 1.5] [--aurora]` bakes a normal map and a groove-shaded colour texture on the
   model's own UVs (Magmaw's scales, Snowpuff's feathers, the Aurora fox's fur; `--aurora` turns its hard green patches
   back to pale fur under a soft teal-to-violet aurora). `Tools/art/creature_glow.py` imports them as
   `<Name>_DetailColor / _DetailNormal` and builds the creature's material from them (`MI_<Name>Glow`, or
   `MI_<Name>Skin` with no glow).
   Hair the image models make as a lump of the body (a mane, a ruff): `blender -b -P hair_locks.py -- out\<id>.fbx
   <mask.png> out\<id>_hair.fbx --face x,y,z [--count 1800] [--length 0.13] [--width 0.02] [--droop 0.7]
   [--glb <preview.glb>]` grows locks over the rig where a mask on the model's UVs is white (a fix script writes it:
   Rimeclaw's `rc_fix.py` saves its mane), combed out from the face and back, drooping at the tips; each takes the
   colour and skin weights under its root, so make the clips from its output. Rimeclaw: 2200 locks, `--width 0.016
   --length 0.14 --droop 1.1 --lift 0.08`, about 60k triangles on top of its 39k (wider, scattered locks came out like
   ice shards). A tail: `--along "x,z x,z ..."` (its middle line in the rig's frame, root first: x from the box's
   middle, z from the feet, in heights) combs each lock the way the line runs nearest it. Aurora fox: 3200 locks,
   `--width 0.011 --length 0.09 --lift 0.12 --droop 0.35` (mask from its `af_tail.py`; 0.016 wide came out leafy).
4. Import: `WR_IMPORTS="C:\...\out\a.fbx=A;C:\...\out\b.fbx=B"` then
   `UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script=Tools/import_creature.py`.
   The folder name lower-cased must equal the FBX name (`aurora_fox.fbx=Aurora_fox`). To replace a creature, move its
   `Content/Creatures/<Name>` folder out of the project first (the importer skips existing folders: a re-import would keep
   the old texture and animations).
5. Wire: `node Tools/art/wire_creatures.mjs id=Folder[:meshYaw] ...` fills `mesh` / `anims` in `Data/creatures.json`.
6. Check in game: `powershell -File Tools\creatureshots.ps1 id1 id2 ...` (two `WRArtCam` angles per creature, burrowers kept visible).

Pack creatures (Fab, git-ignored): `node Tools/art/wire_pack_creatures.mjs` points Gloomhowl (Barghest), Scorchwing
and Skaldrith (Mountain Dragon) at the pack meshes and animations, with tinted materials from
`Tools/art/creature_tints.py` (`M_CreatureTint` + one instance each; `TwoTone` > 0 puts `TintLight` on the
texture's lighter parts, from luminance `LightFrom` up: Skaldrith's frost white over glacier blue). Flyers can add
`fly` / `hover` animations.
`"nightGlow": N` in `creatures.json` raises the materials' `GlowStrength` to N as the daylight goes (Gloomhowl's eyes:
its instance keeps `GlowStrength` 0, so it is dark by day).
`"legs": [[upper, lower, foot], ...]` (4 or 6, front first, left then right; each bone the parent of the next) gives a
pack skeleton the generated rigs' feet on slopes: the body tilts to the ground and each foot reaches its own (Skaldrith:
`UpperArm / Forearm / Hand` in front, `Calf / HorseLink / Foot` behind, so the hock does the reaching).

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
   in through the page itself: `blender -b -P pack_for_page.py -- mixamo\x.fbx out\x_page.b64 2400 <height>` (the same height given to
   prep_mixamo: the rig comes back at the packed height and the full mesh is laid over it as is), then decode it in
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
are not touched, so Git LFS only grows by the clips. The ranged stance's strafes, steps back and turns (0.4.0) came the
same way from `anims\ranged_move`: copies of the Longbow pack's `standing walk / run / aim walk back|left|right` and
`standing turn 90 left|right`, renamed `bow walk back.fbx` etc. (-> `BowWalkBack`), built with `--out mixamo\ranged_move_m\...`
and `ranged_move_f` (the female's rest pose from `f\sword_shield\sheath sword 1.fbx`), ~170 KB per clip in LFS.
The Sword And Shield pack already held the melee stance's strafes (`Strafe2` left / `Strafe` right walking, `Strafe3`
left / `Strafe4` right running), walking / running back (`Walk2`, `Run2`) and turns (`Turn2` left, `Turn` right).
`player_art.json` wires them by direction; `UWRAnimInstance` measures each clip's pace and which way its feet go, and
warns if a clip is filed under the wrong direction. Check with `Tools\animshots.ps1` (frame sequences from two angles).

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
