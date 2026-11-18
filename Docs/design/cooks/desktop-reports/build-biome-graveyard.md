# build-biome-graveyard: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/build-biome-graveyard.out).

You've hit your 5-hour limit · resets 9:50am (America/New_York)
EXIT 1
LANDED build-biome-graveyard f15b0b1 349acfd 5d3fc1e fdce5e7 f754b90 cc79e40 7f0f3f7

The Graveyard is on master in seven pieces. This session finished pieces 5 to 7. Every piece compiled with no new warnings. Nothing was played or tested here, by the lane rules. All of the Graveyard's sounds and music are borrowed recordings for now (existing ones, pitched).

**What this session added**
- **Piece 5, the bosses (f754b90):**
  - The Bell-Ringer: the bell, its wheel and the rope run as one turning machine, and cutting the rope silences the bell.
  - The Crypt Mother: eight egg sacs you can burn, and fire burns the young off her back.
  - The Ossuary Lord: it re-forms from its bone pile unless roots, two lime pots or its three keystones stop it. Its chandelier falls on it once, under gravity.
  - The Gravedigger: he walks a night round with his hounds. He can bury you or shut you in his cage, and you can smash his lantern.
  - Nhal: twelve chapel tombs, sealed by Nature hits or heartwood stakes. At half health The Opening is forced, then each open tomb raises dead. Her hidden third phase is a procession of 40 shades.
  - Players hold E to dig a buried friend out, pull a tame up, or drive a stake. Lime pots now work as thrown weapons.
  - Pieces 1–4 used 40+ effect and sound names that had no rows, so those effects and sounds never played. All of them now have rows.
- **Piece 6, the Necromancy path (cc79e40):**
  - All 22 nodes and the Souls resource, with a row of flames on the HUD.
  - Deathless: once per Rest, a killing blow leaves you at 1 HP.
  - Raising: a fresh body stands up and fights for you. Raised dead never count as tames, never save and drop nothing.
  - The nine abilities, every passive, and 7 of the 10 combos.
- **Piece 7 (7f0f3f7):**
  - The Blood Moon: a red-moon warning, then processions of Risen that walk toward lights at night. They collapse into bone piles at dawn.
  - The Graveyard→Forest invasion, led by the Gravedigger.
  - Ambient sounds and music for the Graveyard.

**Requirement ledger** (the full proof table is in `cooks/build-biome-graveyard.md` §5, pushed as 7fb3028)

| Rows | Status | Where |
|---|---|---|
| R1–R4, R7 | done | this lane doc, sections 1–5 |
| R5, R6 | done | `WRGraveBossLook.cpp` (bell and chandelier machines), `WRGraveScenery.cpp:878` (crane), Gravedigger round in `TickDigger` |
| R8–R16, R61 | done | f15b0b1 |
| R17–R19, R25–R26, R41–R44, R55 | done | f15b0b1 / 349acfd; the Digger's lantern now drops (f754b90) |
| R9, R20–R23, R56 | done | fdce5e7 |
| R27, R33, R34, R50 | done | f754b90 (`WRGraveBosses.cpp`, `WRGraveBossLook.cpp`, `WRGraveArenas.cpp`) |
| R28–R32, R57 | done | 5d3fc1e |
| R35–R37 | rows done, sounds borrowed | f754b90 / cc79e40 / 7f0f3f7 |
| R38, R40, R59 | done | cc79e40 (`WRNecroPath.*`, `WRAbilitiesNecro.*`, `necromancy.json`) |
| R39 | partly | 7 of 10 combos built |
| R24 | partly | 7f0f3f7 (`WRGraveEvents.*`) |
| R45 | partly | 7f0f3f7 |
| R46 | hidden on purpose | `portals.json` (no Graveyard portal kit exists yet) |
| R47–R49, R51–R54, R58 | done | data, tuning and compile in every commit; HANDOFF; LANDED |
| R60 | **not done** | bone altar ritual |

**Not finished** (each also logged in LATER-COOKS):
- The bone altar ritual for permanent undead tames.
- Three combos: Jolt of life, Blood bond, and Rot and bloom. They show in the combo list but say "not built yet".
- Necromancy node icons: the path screen shows the nodes without their pictures.
- Graveyard recordings and its own music score.
- The Blood Moon processions attacking lit bases.
- The Mourning's triple grave-seam yield.
- The invasion warning signs and the forest invasion's sealed band.
- Raised creatures from outside the Graveyard keep their own hide colour; only the green threads and eyes show.

**New binary files:** none in pieces 5–7. Earlier, piece 1 added `Data/lands/graveyard_ground.bin` (1.87 MB, plain git, not LFS).

The notes-repo commit 7fb3028 is missing the Co-Authored-By line. I left it rather than rewrite history.
EXIT 0
