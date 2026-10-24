# Creature art pipeline (all local, free)

Runs on the RTX 4070 Ti. Python env + models live in `C:\Dev\WildreachArt` (not in git: ~35 GB).

1. Concept image: `python concept.py "<description>" out\name.png` (SDXL base 1.0, commercial use OK).
2. Image -> 3D shape: `python shape.py out\name_0.png out\name_shape.glb --steps 50 --octree 384`
   (Hunyuan3D-2, Tencent Hunyuan 3D 2.0 Community License: not licensed in the EU, UK or South Korea;
   commercial use allowed below 1M monthly users). Needs transformers 4.49 / diffusers 0.33.1 / huggingface_hub 0.30.2.
3. Texture: Hunyuan3D-Paint needs the CUDA 12.6 toolkit to build its rasterizer (one admin approval). Not done yet.
4. Rig + animations: `blender -b -P rig_quadruped.py -- out\name_shape.glb out\name.fbx --head +x --preview out\name_walk.png`
   (finds the 4 feet, aligns the body, builds the skeleton, auto-weights, bakes Idle/Walk/Run/Attack/Hit/Death).
5. Preview any model: `blender -b -P render_preview.py -- model.glb out.png`
6. Import into the game: set WR_FBX / WR_NAME and run `UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script=Tools/import_creature.py`,
   then add `mesh` / `anims` (and optional `meshYaw`, `meshScale`) to the creature in `Data/creatures.json`.
