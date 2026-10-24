# Creature art pipeline (all local, free)

Runs on the RTX 4070 Ti. Python env + models live in `C:\Dev\WildreachArt` (not in git: ~35 GB).
Env: `HF_HOME=C:\Dev\WildreachArt\hf`, `HF_HUB_OFFLINE=1`, `HY3DGEN_MODELS=C:\Dev\WildreachArt\models`, CUDA 12.6 `bin` on PATH.

Batch flow (what the four-legged set used):

1. Concepts: `python concept_batch.py batch_quadrupeds.json [--only id1,id2] [--seed 31]` -> `out\<id>_0..3.png` + `out\<id>_sheet.png`
   (SDXL base 1.0, commercial use OK). Keep prompts short: CLIP drops anything past 77 tokens. Pick standing, clean side views;
   crouched or front-on poses turn out squashed.
2. Shape + texture: `python build3d_batch.py id:index ...` -> `out\<id>_shape.glb`, `out\<id>_textured.glb`
   (Hunyuan3D-2 + Paint turbo, Tencent Hunyuan 3D 2.0 Community License: not licensed in the EU, UK or South Korea;
   commercial use allowed below 1M monthly users). Needs transformers 4.49 / diffusers 0.33.1 / huggingface_hub 0.30.2.
3. Rig + animations: `blender -b -P rig_quadruped.py -- out\<id>_textured.glb out\<id>.fbx --head +x --preview out\<id>_walk.png`
   (finds the 4 feet, aligns the body, builds the skeleton, auto-weights, bakes Idle/Walk/Run/Attack/Hit/Death).
   Right-facing concepts come out head +X. Three-quarter or left-facing ones land on any axis: check `_walk_0.png`
   (side view, head should be on the right) and re-run with `--head -x|+y|-y` as needed.
4. Import: `WR_IMPORTS="C:\...\out\a.fbx=A;C:\...\out\b.fbx=B"` then
   `UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script=Tools/import_creature.py`.
   The folder name lower-cased must equal the FBX name (`aurora_fox.fbx=Aurora_fox`). To replace a creature, move its
   `Content/Creatures/<Name>` folder out of the project first (the importer skips existing folders: a re-import would keep
   the old texture and animations).
5. Wire: `node Tools/art/wire_creatures.mjs id=Folder[:meshYaw] ...` fills `mesh` / `anims` in `Data/creatures.json`.
6. Check in game: `powershell -File Tools\creatureshots.ps1 id1 id2 ...` (two `WRArtCam` angles per creature, burrowers kept visible).

Single-item tools: `concept.py`, `shape.py`, `texture.py`; preview any model with `blender -b -P render_preview.py -- model.glb out.png`.
