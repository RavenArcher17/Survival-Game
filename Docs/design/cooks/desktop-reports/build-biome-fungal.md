# build-biome-fungal: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/build-biome-fungal.out).

LANDED build-biome-fungal 70812d1 0862a90 ec2b023 5f4ee38 7b01131 a24929d e7611b9

The Fungal caverns are on the map and live. The work landed as seven commits on `master`. Each one was compiled clean with the editor target before it was pushed. Two rebases were needed when the Ocean lane landed underneath me; one had a conflict in `WRStatusComponent.cpp`, which I resolved and recompiled before pushing. I ran no tests, as the brief said, and nothing has been played yet.

**What's in each part**
1. **The land and caverns:** the plateau, three cavern levels, landmarks, POIs, descents, camps and the Heart Grotto city.
2. **The caverns' things:** nodes (deepcap grows only on the dead), caches, lore and secrets, items and recipes, five shrines, Ilmari and his six quests, the Sporefall row, the invasion and the weather.
3. **Hazards and the Sporefall:** spore clouds (wind pushes them, fire flashes them, frost settles them), lure lines, springcaps, cap-falls, foul air, the Breath vents, the doors and the Gale flask.
4. **Creatures:** ten creatures plus the bosses' Sporelings, each on a real rig with its attack rows, a code-built look, its special moves, dens and living-world beats.
5. **Bosses:** the Sporemother, the Rot King, the Weaver and Mycelorn, each with its arena and counter-mechanic, plus run-back stones, summoning stones and the Spore altar.
6. **The Spore path:** the Spores bar, planted Spores you can see, Detonate (free with Spore cloud), 22 nodes, combos, the Sporestalker's path magic, the body look and 47 glyphs.
7. **Sound:** events, creature voices, ambient beds and music cues by zone, the Sporefall music layer, and the Fall Field's one planned silence.

**Requirement ledger:** every row R1–R60 has its proof in section 5 of `build-biome-fungal.md` (showcase-notes `c9a55c2`).
- R1–R7 (process), R56–R58 (gates and bookkeeping), R60 (binaries): done.
- R8–R21 (land and places): done in part 1.
- R22–R24, R28, R30, R31, R50, R51 (things, quests, weather, invasion): done in parts 2–3.
- R25–R27, R29, R59 (hazards, Sporefall, Sky cross-lane): done in part 3.
- R32 (portal slots): the `fungal` row in `portals.json`, which was already there.
- R33–R37, R55 (creatures and the combat rules): done in part 4; the Sporestalker's path magic landed in part 6.
- R38–R41 (bosses): done in part 5.
- R44–R46 (Spore path, combos, body): done in part 6.
- R47–R49 (armour set, building set, items): the Sporeweave set and Mycelium den set were already in the data; the items are from part 2.
- R52–R54 (data-driven, server authority, nothing unfinished shown as finished): done throughout.
- **R42 and R43 (voices and music) are only partly done.** The sound map and the Fall Field silence work, but every voice and music track is a stand-in made from recordings already in the repo, and each row is marked `_standin`.

**Not finished** (none of it is presented as finished in the game):
- The fused creature voices and the cook's composed music.
- The Weaver and the Sporestalker walk floors rather than walls and ceilings.
- Mycelorn is built entirely in code, and its pool doesn't visibly drain in phase 2.
- Smaller gaps:
  - A Draught plug doesn't take double from blunt weapons.
  - Mycelorn takes full poison and rot buildup instead of 30%.
  - An ally can't cut a cocooned player free (fire still frees them), and can't free a tame Mycelorn has pinned to the wall.
  - The Sporestalker has no wall-kick pounce.
  - The Spore body has no shoulder motes at 75 points and no cyan breath at 100.
  - Queen's jelly, King's slurry and Star-thread have no effects yet.
  - Mycelorn's Stipe Maul belongs to the boss-weapons cook.

All of these are logged in `LATER-COOKS.md`, and the `LANDED.md` line is appended.

**New binaries:**
- `M_FungalGlow.uasset`: 17.5 KB (LFS).
- `M_FungalVeil.uasset`: 12.2 KB (LFS).
- `Data/lands/fungal_ground.bin`: 0.97 MB (plain git).
- 47 glyph PNGs of about 1 KB each: 37 KB uploaded to LFS.

There are no new textures and no new audio files.
EXIT 0
