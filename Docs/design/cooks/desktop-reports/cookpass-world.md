# cookpass-world: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-world.out).

LANDED cookpass-world 987cbcd 7013d11 45affb9

The Fire and Frost cook pass is pushed to master: the island's two sides now have their places, sub-zones and small hazards. It compiles clean, including a full unity build. **Nothing was played:** these gates are compile-only, so you test it. The cook doc is `showcase-notes/notes/Wildreach/cooks/cookpass-world.md` (plan in dcc28e3, final proofs in 04676d5).

**What's in the game now:**
- **Places:** `Data/island_places.json`, made by `node Tools/terrain/island_places.mjs`, holds:
  - 10 sub-zones (V1-V5, T1-T5);
  - 15 landmarks and 26 story sites;
  - the slag-cairn and inuksuk roads (24 waymarks);
  - 16 sastrugi fields;
  - 168 small kit places, spaced so no walkable spot is more than 125 m from a place.

  Every machine builds them at runtime once the ground under each one has loaded. `L_Island` itself is untouched.
- **Hazards** (all damage through `ApplyHit`):
  - Run on a shared clock (nothing to replicate): fumaroles, geysers (one lifts you onto the Glasswall ledge), the acid pool, serac falls and the glacier gusts.
  - Replicated: thin ice and snow bridges.
- **Quick raises:** lava now does 20 damage plus 20 burn a second (was 8/12). The volcanic scrub is dead charred brush, using the pack's charred-tree meshes instead of the coat that hung the GPU. The crater now smokes all the time and glows at night.

**Pushed bug, fixed:** my first commit (987cbcd) didn't compile as a unity build: one of my helper names clashed with WRLandSites'. 7013d11 fixes it; the full unity build was verified before pushing that commit and again after the last change.

**Requirements (all done; file:line proof for each is in the doc's section 0):**

| # | Requirement | Where |
|---|---|---|
| R1–R4 | Research, real-world reference, grades, plan | Doc sections 1.1, 1.2, 2, 3–4 |
| R5 | Build at full detail | `WRIslandPlacesLook.cpp`, `WRIslandPlacesLook2.cpp`, `WRIslandPlacesKit.cpp` |
| R6–R7 | Spot-check, proof for every claim | Doc section 6 (6 problems found and fixed), section 2 cites file:line |
| R8–R9 | Design with intention, smooth motion | Hammer bridge spans the measured river; Plough Trench on Cinderhorn's loop; Bellows Stacks breathe on the clock |
| R10 | POI density | About 145 places a side |
| R11 | Sub-zones | `WRIslandPlaces.cpp:95`, `WRAudioDirector.cpp:272`, `WRUIFlow.cpp:1452` |
| R12–R13 | Landmarks and story sites | The look files above |
| R14 | Kit places | The tool's kit section |
| R15 | Night lights | `Lamp`, `NightFx`, `PassCup` and `Glowmoth` in `WRIslandPlaces.cpp` |
| R16 | Micro-hazards | `WRIslandHazards.cpp`, tuning `hazards.ff` |
| R17 | Lava 20/20 | `Data/tuning.json` |
| R18 | Dead scrub | `WRWorldDressing.cpp:511` |
| R19 | Green only in the Kipuka | `WRIslandPlacesLook.cpp:471` |
| R20 | Crater plume | `WRIslandPlacesLook.cpp:158` |
| R21 | Sea | Graded as meeting the bar; no change |
| R22 | Wayfinding | Waymark roads and sastrugi (`WRIslandPlacesLook2.cpp:744`) |
| R23 | Places fit the roaming minis and stay clear of arenas | The tool's clearance list |
| R24 | Data-driven | The places file, `tuning.json`, 21 fx rows, 16 sound events and 11 beds |
| R25 | Server authority | `ApplyHit`; `AWRIsleState` replicates thin ice and bridges |
| R26 | Landmarks on the map, with a discovery banner | `WRIslandPlaces.cpp:575`, `WRUIFlow.cpp:1429` |
| R27 | Unity-build safe | 7013d11 |
| R28 | Landed in pieces | 3 commits, rebased, CHANGELOG and HANDOFF updated |
| R29 | Boots | Builds are guarded; no map or save change |
| R30 | Leftovers from other lanes | Doc section 3.9, plus 4 LATER-COOKS lines |
| R31 | Decisions for tomorrow | Doc section 7 |

**Not finished:**
- All 16 new hazard sounds and the 11 sub-zone beds are stand-ins built from existing recordings (marked `_standin`).
- Rivers, the steaming ford, the lava-fall ledge and real crevasse cuts need a terrain rebuild. The Crevasse Maze is built as lanes between ice blocks instead.
- Thin ice drops you into drawn icy water: it builds frost, but you can't swim in it.

**Decisions I made tonight** (these didn't go to DESKTOP-SAYS.md, which the coordinator writes):
- Several design-doc positions sat in the sea or on cliffs, so I moved those places to the nearest real ground. The alternative is reshaping the terrain to the doc.
- Crevasses are serac lanes. The alternative is cutting landscape holes.
- Thin ice opens onto drawn water. The alternative is a real water body under the lakes.

**New binary files: none** (only code and JSON, plus existing pack meshes).
EXIT 0
