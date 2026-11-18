# build-biome-void: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/build-biome-void.out).

You've hit your monthly spend limit · raise it at https://claude.ai/settings/usage?from=cc_cli_limit_message · your weekly limit resets Nov 20, 11am (America/New_York)
EXIT 1
EXIT 1
EXIT 127
LANDED build-biome-void b6135cc f770c82 7a32dd0 0753b81 fc4b301 7485955

The Corrupted wasteland is finished on master. This run added six commits on top of the earlier three (9b5ad94, c5c9513, 0295547), all pushed. After every commit the editor build passed with no errors or warnings. Nothing was played or tested, as the brief said.

The parts of the land added in this run:
- **Gathering and items:** the land's resources and their recipes, lore keepsakes, and throwing stones and anchor salt bags.
- **World:** the five shrines, and Maren Vey with her six quests.
- **Creatures:** the nine creatures, plus Riftborn variants and day/night dens.
- **The Void path:** its bar, cost rules, 12 abilities and passives, four combos and the body change.
- **Bosses:** the four Gauntlet bosses, the Echo, and Vorhollow's three phases on a moving island, plus the Null Gate.
- **Hazards and event:** litter fire, hollow crust, and the Rift Storm.
- **The Unmade Place:** its stair and the Descent lift.

The lane doc in showcase-notes now has the proof column filled in, and I've appended to LANDED.md and LATER-COOKS.md on the share.

**Requirement ledger**

| # | Done | Where |
|---|---|---|
| R1–R4 research, plan, build steps | done | `cooks/build-biome-void.md` sections 1–3b |
| R5 design with intention | done | each builder comments why a piece is there (WRLandSites, WRVoidUnmade, WRVoidBosses) |
| R6 machines move smoothly | done | Null Gate, Descent lift and Vorhollow's island pieces all move from replicated clocks |
| R7 no missed requirements | done | proof column in the lane doc |
| R8–R13 land, zones, places, landmarks, camps, caves, stories | done | WRLand / WRLandSites (9b5ad94) plus pockets under the crust (0753b81); secrets X1–X4 not built |
| R14 Unmade Place | done | `WRVoidUnmade.cpp` (fc4b301) |
| R15 voidglass | done, part | rift rings, Glass Sea, U5 garden, storm rings; floating keels you knock down and mine are not built |
| R16 other resources | done | `WRVoidWorld.cpp`, items.json, recipes.json (b6135cc) |
| R17 rift drain, Unmade death | done | WRVoid; the drain is ×1.5 inside the pocket; falling past the void floor kills |
| R18 micro-hazards | done | rift pull and rubble in WRVoid; litter fire and crust in `WRVoidHazards.cpp` |
| R19 weather | done | c5c9513 |
| R20 Rift Storm | done | `WRVoidHazards.cpp`, world.json `events.riftstorm` |
| R21 shrines | done | world.json; placed on the land (`WRLand::PlaceOn`) |
| R22 Maren Vey and quests | done | world.json; quest events fired from the world, boss and Unmade code |
| R23–R25 creatures, Riftborn, day/night dens | done | `WRVoidCreatures.*`, creatures.json, attacks.json, wasteland.json `spawns` |
| R26 taming uses | part | Voidstrider ridden blink, Driftmote rift sensing, Withergoat farm and Hollowfang calming are in; the Riftback pocket is not |
| R27 mini-bosses | done | four stations plus the Echo, `WRVoidBosses.cpp` |
| R28 Vorhollow | done | phases 1 and 2, the transition, and the hidden phase 3 (Gullet) |
| R29 creature voices | stopgap | existing families, pitched |
| R30 music | stopgap | land pools using borrowed tracks; the Dead calm silence works |
| R31, R33 Void path and body change | done | f770c82 |
| R32 combos | part | 4 of 11 pairs; the other 7 partner paths don't exist yet |
| R34–R36 gear, building set, crops | done | the Riftback shield is not built (shields aren't items yet) |
| R37 portal slot | done | `portals.json biomes.void` (hidden) |
| R38 lore told by places | done | keepsakes and lore items |
| R39 Arcane ↔ Void invasions | not built | the Crystal caves land doesn't exist |
| R40–R45 data-driven, replication, hidden placeholders, combat rules, compile gate, notes | done | every commit |

**Not finished** (each line is in LATER-COOKS):
- The Riftback's pocket into the clan Rift chest network, and the Riftback shield.
- Real creature calls and composed music; voices and music above are stopgaps.
- Graveyard Riftborn, Arcane invasions and the Arcane weakness. Their lands don't exist yet.
- Inside the Null Warden's field, spells fizzle at no cost. The design says half the cost is refunded.
- Floating keels to mine, the Sea Throat seam, secrets X1–X4, and the cosmetic weapon-art scrolls.
- The Driftmote only senses rifts; it doesn't yet find caches, crust or storm seeds.
- Creatures don't flee litter fire.
- An Unmade death doesn't yet leave your body at U1.

**New binary files (Git LFS):** 45 PNG path glyphs for the Paths menu, about 132 KB in total. No new textures or uassets this run.
EXIT 0
