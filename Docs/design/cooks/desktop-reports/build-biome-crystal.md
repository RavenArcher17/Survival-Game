# build-biome-crystal: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/build-biome-crystal.out).

You've hit your 5-hour limit · resets 4:50am (America/New_York)
EXIT 1
EXIT 127
You've hit your 5-hour limit · resets 2:50pm (America/New_York)
EXIT 1
The Crystal caves are live on master, built from four of the plan's seven pieces. The four bosses, the Arcane path and the composed music are not built. Every piece compiled clean (editor target) and was pushed. Nothing was played or tested; that's yours.

**LANDED build-biome-crystal** 548b0b5 8ad8b88 a794e5c 6728f3a b50fd94 1f38c88 b4681fa (plan-doc proof: showcase-notes 61fc8f1)

**What landed:**
1. **The land** (finished the earlier run's uncommitted work): the basalt plateau and the cave network (halls and lava tubes you walk and collide with), the landmarks, descents, camps, sites, kit places and the Chime Crossing.
2. **Sound makes light:** blows, casts, rolls, landings, footsteps, arrows and strikes on crystal light the crystals around them on every machine, and the server hears them for the creatures and hazards. Keystone chimes light a hall for 20 game minutes.
   - Gathering includes resonant crystal "picked on the ring" (strike again while it rings for one more) and the wild crops.
   - Also: the seven lore keepsakes, the found runes and compass, 37 items, 7 recipes, five shrines (two on cave floors), Saga Venn and her quests.
3. **Hazards:** ward fields with their tell, throw and keystone plates, falling shard chandeliers, thin floors, scalding pools, the Silent Vault door and its silence, and the Resonance event.
4. **The nine creatures** with their attack rows, crystal body parts, behaviours and dens by sub-zone. This is when `crystal` went into the live biomes list.

**Things to know:**
- **Two of Saga's quests can be finished but don't fully pay yet.** Her fifth quest (beat the Mirrored Twins) is parked in `world.json` under `quests._saga_venn_later` so her chain doesn't dead-end; it can go back once the Twins exist. Her quest XP goes nowhere until the Arcane path exists.
- **All the new sounds and creature voices are stand-ins** pitched from existing recordings, marked `_standin`.
- **Creatures borrow existing rigs**, retinted, with the crystal parts built in code.
- **Two wording changes from the design:** "On the ring" asks for 8 on-ring strikes rather than 20 crystal with 8 on the ring. A Prismback's reflected shot hits the shooter directly with a flash instead of flying back as a projectile.
- **Another lane had to patch WRFarm** because my helper names clashed in a unity build. I renamed all my helpers so it can't happen again (b50fd94).
- **I removed 18 duplicate Void creature rows** in `creatures.json` (each was there three times).

**Requirements (R1–R50; full table with file references is section 5 of the plan doc):**

| Rows | Status | Where |
|---|---|---|
| R1–R4 | done | plan doc sections 1–3 |
| R5 | done for what was built | comments in the WRCrystal scenery, hazard and creature files |
| R6 | done for what was built | the chandelier fall and the Vault door's 3 s roll are driven by the server clock; the boss machines (turntables, lens gimbal) aren't built |
| R7 | done | the ledger itself |
| R8–R13 | done | land, caves, places (548b0b5) |
| R14 | done | Silent Vault (6728f3a) |
| R15 | done | sound makes light (a794e5c); the keystone map reveal isn't possible because the game has no map fog |
| R16–R17 | done | resonant crystal and the other resources (a794e5c) |
| R18 | done | ward fields (6728f3a) |
| R19 | done | full dark in the caves (WRSky, the hall lights) |
| R20 | done | chandeliers, thin floors, scalding pools (6728f3a) |
| R21 | done | weather (a794e5c) |
| R22 | done | the Resonance (6728f3a) |
| R23 | done | shrines (a794e5c) |
| R24 | partly | quests 1–4 and 6 are live; quest 5 parked |
| R25–R28 | done | creatures (1f38c88); Prismback path magic and the Lumenhorn's on-command bellow not built; voices are stand-ins |
| R29–R30 | **not built** | the three mini bosses and Lyrathis, with their arenas |
| R31 | stand-in | creature voice rows |
| R32 | partly | the Vault's silence works; composed music not built |
| R33–R35 | **not built** | the Arcane path, its combos, its body change |
| R36–R37, R39 | done | Wardcloth and the Geode sanctum went live with the biome; wild crops |
| R38 | partly | items and recipes are in; the spear's special attack and blueprint drop need Lyrathis; the two shields need shield items, which don't exist yet |
| R40 | still hidden | portals: no Crystal portal kit exists |
| R41 | done | lore keepsakes |
| R42 | separate cook | the PvP pass |
| R43 | **not built** | Arcane ↔ Void invasions (they need the Geode Tyrant) |
| R44–R50 | done | data-driven numbers, server authority, nothing unfinished shown, clean compile, CHANGELOG/HANDOFF/LANDED/LATER-COOKS, the creature-row dedupe |

Seven LATER-COOKS lines cover the unbuilt items, plus reconciling the Wasteland/Forest map frames and the PvP pass.

**New binary files:** `Content/Terrain/M_CrystalGlow.uasset` is 38 KB (Git LFS). `Data/lands/crystal_ground.bin` is 723 KB (plain git).
EXIT 0
I've built the rest of the Crystal caves: the Arcane path, the four bosses with their arena machines, and the rows that were waiting on them. Everything is pushed to master and the editor build compiles with no new warnings. Nothing has been played or tested, per your rules.

LANDED build-biome-crystal 731fa59 0a1d18b 6b3c339 (merges cfad5fc, 5cf1a59; proof table in showcase-notes 5236746)

**What's in it**
- **Arcane path:**
  - Mana: a violet bar under health that refills steadily, faster with the root passive Deep well.
  - Shields: a new shared protection layer that absorbs damage before health. While one holds, no status effect builds up. With the Reflect node, shots bounce back as real bolts that never home.
  - Nine abilities, the passives, seven combos with the paths that exist, node icons, the cast chime and fingertip trace at full strength.
  - Two new materials: one for the glow of shields, runes and beams, and the Twins' water reflection (`M_MirrorWater`, built with the bosses).
- **Mirrored Twins:**
  - The Spar Lens hangs in a bronze gimbal and turns a quarter every 12 s; the knight and its double trade places along its light.
  - The double takes no damage, can't be parried, and makes every move at the same moment from your other side.
  - Only the real knight has a reflection in the water.
  - Three shots on the flaring lens shatter it, and the wall magazine lowers a spare down its chain over 20 s.
- **Echo Warden:** blind; it goes to the loudest noise. Four chime stones are now real machines: it hammers a struck chime with its back to you, and three hammerings break it.
- **Geode Tyrant:** walks a real route along the cave tubes. Each plate row throws shots back until strikes break it. Charging into a selenite beam shatters the beam and stuns it.
- **Lyrathis:** eight prisms on geared bronze turntables; hitting a lever turns a prism exactly 45° over 0.8 s.
  - Her beam splits through the prism nearest her, and the same rays are traced on every machine.
  - Her dome breaks when you turn a prism so her own beam hits it.
  - Phase 2 copies stand on the prisms' lines; phase 3 (artifacts) needs three prisms turned to face her.
  - Also built: the altar, two run-back stones, summoning, and her drops.
- **Rows that waited:**
  - Saga Venn's quest 5 is back, and her quest XP now goes into the Arcane path.
  - A boss kill counts for everyone who did their share, not only the killing blow.
  - The Prismglass spear's charged heavy throws the two light-lines, and its blueprint drops from Lyrathis.
  - Arcane ↔ Void invasions run through the Severance.
  - The Prismback can be tamed with path magic, and the Lumenhorn bellows on order from a new Call wedge on the whistle wheel.
  - The three trophy talismans now do what the design says.
- **Fixed a Part 1 bug:** three sets of cave passages had no opening at all: the Prism Hall's West Door, the Lens Grinder's door and the Silent Vault's rooms. I fixed the generator and regenerated the cave data.

**Requirement ledger (P1–P49)**

| # | Status | Where |
|---|---|---|
| P1 | done | research, plan and build steps written first (cook doc P2.0–P2.11) |
| P2 | done | boss rows in `creatures.json`, `attacks.json` and tuning; run-back stones; trophies and loot |
| P3–P9 (Twins) | done | `WRCrystalBosses.cpp` (Place, TickTwins, StartSwap, ShatterLens, DoubleStrike), `WRCrystalBossLook.cpp` (gimbal, magazine, reflection), `M_MirrorWater` |
| P10–P14 (Warden) | done | `WRCrystalBosses.cpp` (TickIdle, TickWarden, HammerChime), the chime machine, the tuning-fork talisman |
| P15, P17–P19 (Tyrant) | done | `WRCrystalBosses.cpp` (Incoming, TickStrike), body look, talisman; broken plates shatter in an effect but leave no halves on the floor |
| P16 (Tyrant's route) | partly | `TyrantRoute` / TickIdle; the loop isn't tuned to exactly 1.5 game days |
| P20–P32 (Lyrathis) | done | `WRCrystalLyrathis.cpp` (turntables, beam, dome, copies, phase 3, grasp, rain, Flare, lance), placement, effigy, drops |
| P33 voices, P34 music | stand-in | marked rows in `sounds.json`; no composed Crystal music exists yet |
| P35–P38, P40 (Arcane) | done | `arcane.json`, `WRArcanePath.*`, `WRAbilitiesArcane.*`, attributes, HUD |
| P39 (combos) | partly | Ball lightning, Rune volley and Phylactery are shown but not built; the Sun, Tide and Spore pairs wait for those paths |
| P41 | done | quest 5 in `world.json`; quest pay change in `WRWorldSystems.cpp` |
| P42 | done | spear light-lines and blueprint; the spear's Soul art is not built |
| P43 | done | invasion pairs in `world.json` |
| P44 | done | Prismback path magic in `WRCrystalCreatures.cpp` |
| P45 | done | Lumenhorn bellow + whistle Call wedge |
| P46–P49 | done | data-driven numbers, all damage through `ApplyHit`, replicated state; the passage fix; CHANGELOG/HANDOFF in each commit; LANDED and LATER-COOKS lines written |

The main ledger rows are now done: R24, R25–R28, R29–R30, R33–R35, R38 and R43. R31 and R32 remain stand-ins (the Vault's planned silence does work).

**Not finished**
- **Stand-in audio:** no composed Crystal music or fused creature voices exist, so stand-ins from existing tracks play, marked `_standin` in the data.
- **Missing pieces:**
  - The Prismglass spear's Soul art.
  - The two crystal shields: the game has no shield items yet.
  - The Riftborn Prismbacks in the Void's invasion roster.
- **Known gaps in what's built:**
  - Lyrathis's copies repeat only her melee and slam moves; her beams, rain, grasp and lance are hers alone.
  - The Warden's hammering opens its stance from every side, not only from behind.
- **Worth checking first when you play:** the Twins' reflection trick, and the beam traces in the Prism Hall.
- These are in LATER-COOKS.

**New binary files:** `M_ArcaneWard.uasset` 23.8 KB, `M_MirrorWater.uasset` 26.3 KB, and 45 glyph PNGs at 48.5 KB, about 99 KB in total.
EXIT 0
