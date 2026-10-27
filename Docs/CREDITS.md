# Credits and licences

Third-party content in the game and where it came from. Fab packs live in `Content/<pack>` (git-ignored, added by
Raven from his Fab library); everything listed here as "in git" is committed.

## World (polish pass, 2026-10)

### Base props made with the local AI pipeline (in git)

The workbench, forge, armorer, alchemy table, Hearth (a stone fire ring), claim totem (a carved post) and storage
chest under `Content/Props/` (`Workbench`, `Forge`, `Armorer`, `AlchemyTable`, `Hearth`, `ClaimTotem`,
`StorageChest`) were made on this machine with the pipeline in `Tools/art/README.md`:

- Concept images: Stable Diffusion XL base 1.0 (Stability AI, CreativeML Open RAIL++-M licence: commercial use
  allowed). Prompts in `Tools/art/batch_stations.json` and `batch_stations2.json`.
- Shape and texture: **Hunyuan3D 2.0** (Tencent Hunyuan 3D 2.0 Community License). Commercial use is allowed below
  1 million monthly active users, and the licence **does not cover the European Union, the United Kingdom or South
  Korea**: these props, like the AI creatures, carry that territory limit.
- Cleanup: `Tools/art/normalize_prop.py` (Blender) decimates, shrinks the texture and centres each one.

### The sea (Fab pack, not in git)

The ocean material is `M_Ocean` from the **WaterMaterials** pack (`Content/WaterMaterials`, Raven's Fab library,
standard Fab licence for use in a game). The game uses it through its own instance `Content/Sea/MI_Ocean` (in git),
which is cooked with the game (`DirectoriesToAlwaysCook` `/Game/Sea`), so the packaged game carries the parts of
the pack it needs.

### Building pieces, sky and camera

Made from engine shapes and the project's own `M_BuildWood` / `M_BuildStone` materials (`Tools/make_build_materials.py`,
engine content only); the atmosphere and camera are code. Nothing downloaded.
