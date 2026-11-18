# Build cook: build-biome-crystal (the Crystal caves into Aaron's build)

Lane: build-biome-crystal, desktop, 2026-11-15. Design source: `cooks/biome-crystal.md` (read in full: 1,056 lines), plus
`world-map.md` (where it sits on the Spine: the east half of belt B6), `combat.md` (attack-row schema 2.18.1, boss template
2.17), `survival-crafting.md` (Wardcloth, Geode sanctum, candle rhubarb / glowmelon, Chimecrab dust, crystal lamp, ward
charm, runes), `fire-frost.md` (shrine / master / event frame) and the Void lane's build cook (`build-biome-void.md`), whose
runtime-land pattern this lane extends. The design is SPEC for this lane; this doc is the build-side research, requirement
ledger and plan. No building before this plan was written.

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, and every section of biome-crystal.md. "Plan" points into this doc. "Proof" is
filled at the end with file:line or a commit; a row with no proof is not done.

| # | Requirement (source) | Plan | Proof |
|---|---|---|---|
| R1 | Full cook: research, plan, build, spot-check; no skipping (brief, Aaron 11-13 night) | 1, 2, 3, 5 | (end) |
| R2 | Research names real games, places, systems and why they work; real-world reference (brief) | 1 | section 1 |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases before building (brief) | 2 | section 2 |
| R4 | Every build step listed (brief) | 3 | section 3 |
| R5 | Design with intention: why each thing is here, what it connects to, how it meets what it touches (Aaron 11-14) | 2.3-2.6 | (end) |
| R6 | Machines engineered, parts mesh, nothing passes through anything; moving things move smoothly on a mechanism, never snap (Aaron 11-14) | 2.9, 2.10 | (end) |
| R7 | No missed requirements; ledger rows proven (Aaron 11-14) | 0, 5 | this table |
| R8 | The land on the map: terrain (brief; crystal 2.1) | 2.1, 2.2 | |
| R9 | Sub-zones C1-C5 with their own ground, palette, props, creature mix, bed (crystal 2.2) | 2.3 | |
| R10 | POIs every ~150-250 m (brief; crystal 2.4: 23 surface + 53 caves) | 2.5 | |
| R11 | Decoration rule (brief; kit rules 2.4) | 2.5 | |
| R12 | Landmarks L1-L5: Prism Crown, Hollowed Giantess, Split Geode, Hall of Beams, the Organ (crystal 2.3) | 2.5 | |
| R13 | Descents D1-D4 + the Severance mouth; camps K1-K3; surface sites S1-S6; cave sites H1-H8; kit POIs; ore galleries O1-O4 (crystal 2.4, 2.7) | 2.4, 2.5 | |
| R14 | Hidden sub-biome: the Silent Vault (door that opens to silence, silence rule, six rooms V1-V6 and their pay) (crystal 2.5) | 2.8 | |
| R15 | Sound makes light: crystals glow from noise, noise radii, keystone chimes light a hall 20 game min and reveal its map (crystal 2.1) | 2.6 | |
| R16 | Signature resource resonant crystal with the "pick on the ring" rule, 48 nodes + 2 outcrops, 20 m noise per strike (crystal 2.6) | 2.7 | |
| R17 | Other resources: crystal dust, selenite, doubling spar, amethyst, saltpetre, ochre, wax, creature parts; attuning chalk (crystal 2.7) | 2.7 | |
| R18 | Ward fields: tell (Chladni dust, hexafoil glow, rising hum), 25 dmg + 600 cm throw / 1.5 s, attunement passes, keystones drop 90 s, pattern A/B (crystal 2.8) | 2.9 | |
| R19 | Full dark between halls (crystal 2.8) | 2.4, 2.6 | |
| R20 | Micro-hazards: shard chandeliers, scalding pools, thin geode floors (crystal 2.8) | 2.9 | |
| R21 | Weather: surface Clear 40 / Overcast 20 / Rain 30 / Fog 10; drip song in rain; none underground (crystal 2.9) | 2.11 | |
| R22 | Biome event: the Resonance (warning, peals, ward shift, regrowth, singing geodes, danger, pays) (crystal 2.10) | 2.12 | |
| R23 | Shrines: 5 (Great Hexafoil, Cliff Bell, Geode, Beam, Severance) with guards (crystal 2.11) | 2.13 | |
| R24 | Path master Saga Venn and her 6 quests (crystal 2.11) | 2.13 | |
| R25 | 9 creatures: real models where packs allow, real animation, behaviour, attacks with visible contact (brief; crystal 2.12) | 2.14 | |
| R26 | Creature rules: attuned to wards, mark buildup on bites, hearing / vibration hunters, reflection, no attacker limit, tells at 15 m (crystal 2.12) | 2.14 | |
| R27 | Day / night tables per sub-zone (crystal 2.12.3) | 2.14 | |
| R28 | Taming methods and utilities (Lumenhorn mount + light + bellow, Druseback hauler, Gemsniffer sense, Alicanto ore lead, Chimecrab farm, Prismback path magic) (crystal 2.12.1, 2.12.4) | 2.14 | |
| R29 | 3 mini bosses with arenas: Mirrored Twins (Spar Lens gimbal + magazine, reflection rule), Echo Warden (loudest noise, chimes), Geode Tyrant (roaming route, chandeliers) (brief; crystal 2.13) | 2.15 | |
| R30 | Major boss Lyrathis with the Prism Hall (8 engineered turntables, beams, dome broken by the room, Sundering Flare, copies, phase 2, hidden phase 3), drops, summoning altar, two run-back stones (brief; crystal 2.14, 2.21.8) | 2.16 | |
| R31 | Creature voices (fused real calls, 4 layers) (crystal 2.15) | 2.17 | |
| R32 | Music: real, always audible, ducks <= 4 dB, the Vault the only planned silence (crystal 2.16) | 2.17 | |
| R33 | The Arcane path: Mana, tree, abilities, passives, stat nodes (crystal 2.17) | 2.18 | |
| R34 | Arcane combos (crystal 2.17.5) | 2.18 | |
| R35 | Arcane body change (crystal 2.18) | 2.18 | |
| R36 | Armour set Wardcloth placed and obtainable (brief; crystal 2.19.1) | 2.19 | |
| R37 | Building set Geode sanctum placed and obtainable (brief) | 2.19 | |
| R38 | Weapons, talismans, consumables, found Wardcarver runes (crystal 2.19.2, 2.19.3) | 2.19 | |
| R39 | Crops: candle rhubarb, glowmelon (brief; survival-crafting) | 2.7 | |
| R40 | Portal slots (brief) | 2.13 | |
| R41 | Story told only through the environment; 7 lore items (crystal 2.20) | 2.5 | |
| R42 | PvP pass R9 (clan ranks, alliances, anti-zerg, explosives, siege, path / tame raiding, bounties and infamy, contested bosses) (crystal 2.21) | 4 | |
| R43 | Invasions Arcane <-> Void through the Severance (crystal 2.22) | 2.12 | |
| R44 | Data-driven like Fire and Frost: numbers in Data/*.json and tuning, abilities as UWRGameplayAbility + ClassForNode, damage through ApplyHit, screens via WR_REGISTER_SCREEN mouse + pad, sounds / fx as rows (brief) | all | |
| R45 | Server authority and replication on every new system (brief) | all | |
| R46 | No placeholders shown as finished; unfinished stays hidden (brief) | 4 | |
| R47 | Combat bar: projectiles never home, >= 0.9 s warning; crimson only on unblockables; small cues; no attacker limit; players die, no helpers (combat KEPT) | 2.14-2.16 | |
| R48 | Compile gate clean of new warnings; no tests (brief) | 3 | |
| R49 | CHANGELOG Unreleased + HANDOFF in the same commit; LANDED line; LATER-COOKS lines (brief) | 3 | |
| R50 | Found pre-existing defect: the Void's nine creature rows are tripled in creatures.json (only the first is read) | 3 | |

---

## 1. Research (build-side; the design research is biome-crystal.md section 1)

How real games build cave biomes that read in the dark and stay fair, and how they put a cave network into a running world.
What each gives this build:

| Game / system | What it does, why it works | What this build takes |
|---|---|---|
| **Valheim, the Mistlands' infested mines and Ashlands' fortresses** (Iron Gate's "dungeon generator": rooms + connectors from a kit, sealed by the location's bounds) | Every underground place is a set of rooms joined by connector pieces; the generator only places a room where its doorway meets another's. It never has to solve arbitrary caves, so it never makes an impossible cave | **The caves are halls joined by tubes** (`crystal.json caves`): each hall a shell (geode, beam hall, lava-tube junction, hewn chamber, prism dome, vault room), each tube a lava-tube sweep whose ends sit inside two halls' walls. The builder cuts each hall's wall where a tube enters and trims the tube where it is inside the hall, with 1-2 m of overlap so there is never a crack |
| **Deep Rock Galactic** (procedural caves as signed-distance shapes; light only from what you carry or throw) | Caves are unions of spheres and tubes; total dark makes your flares the drama. A cave reads by its silhouette in your light | Halls are noise-pushed ellipsoids and tubes noise-pushed sweeps (never a clean pipe), and between halls it is full dark (SPEC): only carried light, tames and crystals woken by noise |
| **Minecraft amethyst geodes** (smooth basalt shell, calcite, then amethyst; budding blocks grow clusters that chime when walked on) | The real geode cross-section, made into a readable recipe; the sound is part of the block | The geode shell's vertex colours run basalt black -> cream/rust agate bands -> quartz white -> violet amethyst, the real cross-section; crystal points are real quartz habit (a six-sided prism with a six-faced pyramid tip), and every strike rings |
| **Tears of the Kingdom, the Depths' Lightroots** | Light a root and the dark round it is mapped and lit for good: a checkpoint you earn | **Keystone chimes**: strike a hall's chime and the hall holds lit for 20 game minutes and its map cells fill in |
| **Dark Souls, the Crystal Cave's invisible walkways** | The world, not the UI, shows the hidden thing (falling crystal dust lands on the path) | Ward fields show themselves by **Chladni dust**: pale dust lines rise on the floor along the field 0.9 s before it can throw you; the hexafoil over the door brightens; a hum rises |
| **Hollow Knight, Crystal Peak** (crystal hunters' lasers swept along a fixed line; crushers on rails) | Every hazard is a readable line or a readable machine | Lyrathis's beams sweep fixed arcs from fixed prisms; turntables are real mechanisms you can see turning |
| **Elden Ring, Raya Lucaria and Rennala** | The shield phase is a room puzzle | Lyrathis's dome breaks to her own beam turned by a prism |
| **Monster Hunter World, environment traps (the Ancient Forest's rock pillars, Rotten Vale's hanging bones)** | A fixed thing on the route the monster passes; the player sets it off | Shard chandeliers on the Geode Tyrant's route: a loud noise under one drops it |
| **Valheim / No Man's Sky seeded placement** (Void lane KEPT) | Props from a seed over a pure-function ground: same on every machine, nothing stored | Kit POIs in the caves from a seeded pass per hall and per tube (the 150 m rule along the route); dressing per hall from its seed |
| **Splinter Cell / Metal Gear noise meters; Thief's floor-surface noise** | Every action has a radius, and the guards hear by it; quiet is a skill | One **noise model** (`WRCrystal::Emit`, radii in tuning `crystal.noise`): walk 4 m, run 8, sprint 15, roll 8, land 6, hit on a creature 12, hit on crystal 20, cast 10, bellow 40, chime 25, powder 80, crouch 0. Light, Echobats, the Echo Warden, chandeliers and the Vault door all listen to it |

Real-world reference used (from biome-crystal.md 1.2): Ametista do Sul geodes in flood basalt (the plateau and the halls'
colour bands), Naica's selenite beams (the Hall of Beams), Lechuguilla's Chandelier Ballroom (chandeliers), basalt lava tubes
with ledges (the Echo Tunnels), Chladni figures (ward tells), hexafoils (every Wardcarver mark), Aegishjalmur (Lyrathis's
stave), Reznikoff's resonance dots (ochre marks), Iceland spar (the Twins' lens), scaly-foot snail plates (Prismback),
harvestmen and glass frogs (Spindleglass), oilbirds (Echobats), the Alicanto and the carbunclo, giant isopods (Druseback),
Franklin's glass armonica and Part's tintinnabuli (music).

---

## 2. Plan (every gap solved before building)

### 2.0 Re-check against master (2026-11-15, after the Forest and Graveyard lanes landed)

What changed on master since this plan was first written, and the decisions that follow. Where 2.0 and the older text
below disagree, 2.0 wins.

1. **The land is a heightfield land, not a function land.** The Forest and the Graveyard set the newer pattern: a node
   generator (`Tools/terrain/graveyard.mjs`) writes the land file, a 4 m sample file (`*_ground.bin`, uint16 heights +
   a surface byte, plain git) and a places contract (`Data/graveyard.json`) that one C++ loader reads (`FWRGraveData`).
   Surfaces per sample, a zone-letter grid, hidden samples for holes, `underground.ways` so the game mode never rescues
   a body from the caves, `priority` and `cuts` for overlaps. **Decision:** `Tools/terrain/crystal.mjs` writes
   `Data/lands/crystal.json` + `crystal_ground.bin` + `Data/crystal.json`; `FWRCrystalData` (`WRCrystalTypes.*`) reads
   it. The function-land items of 2.2 (holes, flatten, gorge, the multi-hole chunk change) are done in the generator
   instead; `WRLand.cpp` needs no change for the ground.
2. **Which frame.** The Forest and the Graveyard sit at real map metres (game cm = map m x 100); the Wasteland still sits
   shifted 850 m east and 3,700 m south (origin (0, 900000), mapCentre (-850, +5,300)) and its Spine crest
   ((-900, +4,200) -> (+1,700, +5,650)) does not agree with the Forest's Spine (x -800 at y +4,500). The Crystal caves
   touch two neighbours: the Forest (north, B5) and the Wasteland (west, through the Severance). Only one of the two can
   be met while the Wasteland is shifted. **Decision: the Wasteland's frame** (as first planned): the Severance, the
   Hollowed Giantess, the Arcane <-> Void invasions and the void-side lore are the biome's spine, and the B6 belt then
   moves to real metres as one piece (two origin lines) when the map's seams are reconciled. The Forest border's
   blend (crystal-veined boulders under moss, 2.2) can't exist yet: the plateau's north edge is a sea cliff for now.
   LATER-COOKS line: reconcile the Wasteland's Spine with the Forest's and move B6 to real metres together.
3. **Getting there.** The Wasteland is reached by the Grey Crossing (a longboat from the island, `crossing` in its land
   file). The Crystal caves get the same: **the Chime Crossing**, the island's south-east shore to K2's cove (generic in
   `WRLandThings.cpp`: every live land with a `crossing` gets its boats). And on foot from the Wasteland through the
   Severance. Arcane-path newcomers start at the three camps (`camps`, generic in `AWRLand::SpawnCamps`).
4. **Scenery kit.** Sites are built on the forest's `AWRForestScenery` + `FBuild` (blocks, code meshes, rocks, lamps,
   loops), as the Graveyard does, with a crystal kit header `WRCrystalLook.h` (palette, hex column, crystal point
   cluster, hexafoil, chime arch, bronze fittings).
5. **The glow.** No material in the repo lights by position from runtime noise, so a new one:
   `Tools/terrain/build_crystal_materials.py` (commandlet, the `build_void_materials.py` pattern) builds
   `/Game/Terrain/M_CrystalGlow`: vertex colour base, a glassy rough 0.12, a fresnel rim, and emissive = colour x
   (Rest + Lit + the sum of four rings, each a position, radius and strength). Every hall's crystals are one mesh with one
   MID; `AWRCrystalState` multicasts noises, each machine pushes them into the nearest hall MIDs (four ring slots, oldest
   replaced), so a struck crystal lights a sphere round it that fades over 6 s, exactly the rule. Shells use M_HeldTool
   (vertex colour), so the agate bands and lava glaze are in the vertex colours.
6. **The other lanes' frames now reused:** `WRLandThings` crossings, `AWRWorldCache` caches, a spot actor of our own
   (`AWRCrystalSpot`, the `AWRGraveSpot` pattern: keepsakes, the Singing School bars, the vista, the Vault petals),
   `AWRResourceNode` with its look rows, shrines / masters / quests rows in `world.json`, the boss hook chain
   (`FWRGraveBoss` pattern -> `FWRCrystalBoss`), the creature hook chain (`FWRGraveAI` -> `FWRCrystalAI`),
   `UWRGraveBody` -> `UWRCrystalBody`, the path pattern (`WRNecroPath` / `WRAbilitiesNecro`).
7. **Landing order** (each compiled, committed and pushed): (1) the land, caves, sites and the glow; (2) items, recipes,
   gathering, world rows, weather, the crossing; (3) noise and light, keystone chimes, hazards, the Vault, the Resonance;
   (4) the nine creatures; (5) the four bosses; (6) the Arcane path; (7) sound and music rows. Whatever isn't landed by
   the end of the run stays hidden and is listed.

### 2.1 Where the land goes (the gap: the B6 belt exists only as the Void's runtime land)

The Void lane built the Wasteland as a runtime land (`AWRLand`, `Data/lands/wasteland.json`) 9 km south of the island, in
the world-map's metres with `mapCentre` (-850, +5,300) on `origin` (0, 900000, 0). **Decision:** the Crystal caves are a
second runtime land, `Data/lands/crystal.json`, **on the same frame** (same origin and mapCentre). Their map metres are then
the design's own numbers, and the two lands meet at the Spine exactly as world-map.md draws them: the Void west of the Spine
crest, the Crystal plateau at its east foot, the Severance cutting between. When the 12-wedge map exists, both move together.

- The Crystal land's coast runs from the Spine foot (x +1,690, overlapping the Void's east cliff) to the sea cliffs at
  x +2,450, y +4,560..+6,440. Where the two grounds overlap, the higher shows: the Void's Spine cliff stands over the west
  edge of the Crystal plateau (the Spine), the plateau hides the Void's sea floor.
- **The Severance opens** (R10, the Void, is built): the Void's gorge is extended east to x +1,900 at its floor (20 m) and
  the Crystal land carries the gorge's east mouth from x +1,650 to +1,880 at the same floor, so one gorge runs through both
  grounds from the Hollow Fort to the Hollowed Giantess.
- `WRBiome::At` answers `crystal` inside the Crystal land (WRLand::BiomeAt: lands are checked before the island), and
  `WRBiome::IsUnderground` answers true inside any cave hall or tube (`WRCrystal::InCave`).
- `world.liveBiomes` gains `crystal`: its crops, fish, pieces (crystal lamp, Resonance pylon, monument), the Geode sanctum
  set, Wardcloth, the runes and talismans come out of hiding (they were data-ready, `liveWith: crystal`).

### 2.2 The plateau (terrain function)

Height in map metres (`crystal.json`):
1. **Base from the Spine foot**: the crest polyline runs along the foot (x +1,690) from y +4,450 to +6,550. h = 150 at the
   foot, 128 at 120 m, 112 at 300 m, 96 at 500 m, 84 at 700 m, 80 at 800 m (a plateau tilting gently to the sea, +80..+150).
2. **Coast**: cliffs, not beaches: `coastBeach` 6 m, `coastSea` 55 m, sea floor -14 m: 80-100 m black cliffs into the sea.
   The coves under the camps are `flatten` terraces at +4 m reached by cliff stairs (camp builder).
3. **Noise**: ±3 m broad, ±0.8 m fine (basalt pavement is flat, broken by steps).
4. **Features**: four descent `hole`s (D1 North Dome r 22, D2 Bell Throat r 9, D3 Sinking Geode r 26, D4 South Steam Vent
   r 11), ten collapsed geode domes (`hole` r 15-36 over a closed geode cup each: they glow violet at night), eleven Organ
   vents (`hole` r 3), the Severance gorge's east mouth, `flatten` yards for the three camps, the five surface shrines /
   sites, the Prism Crown's ring.
5. **Ground colours (palette law)**: C1 basalt black #1C1C20 with salt-grass #7E8C5A patches by noise and heather browns on
   the higher ground; slope rock (cliffs) #2A2A30; the gorge walls grey-bleached #8E8C92 (the Void's touch at the Severance).
   Chunks multi-hole aware (the builder now cuts every hole in a chunk, not only one).
6. **Hexagonal columns**: the plateau reads as columnar basalt from a seeded scatter of hex column tops (look-kit `Hex`,
   0.5-1.4 m across, standing 0.1-0.6 m proud of the ground in clusters of 7-30) and, along every cliff edge and dome rim,
   tall broken columns 2-12 m (the Giant's Causeway read), dark #1C1C20 with salt-white weathering rims.

### 2.3 The caves (the cavern frame; KEPT shapes, built at runtime)

`crystal.json caves`: **halls** `{id, kind, zone, at [x,y], floor m, size [rx, ry], height, yaw, keystone, light}` and
**tubes** `{id, kind, zone, points [[x,y,z]...], r, floorDrop}`. `AWRCrystalCaves` (one per machine, local, like the
ground) builds every hall and tube as procedural mesh with collision (inward faces), cuts the openings, and dresses them.

Shells:
- **geode** (C2, the cups under collapsed domes, the Deep Geode, the Hall of the Door): an ellipsoid pushed by noise (±8%),
  its floor flattened at `floor` (quartz sand, worn flat), walls coloured as the real cross-section: basalt outside (only at
  openings), then **agate bands** (cream #E6D8C0 / rust #A85A3A, 0.3-0.8 m bands by height) round the lower 25%, then quartz
  white #EDEAF4 and **amethyst violet #6B3FA0** above. **Crystal points** grow inward from walls and ceiling (density by
  hall size; 0.5-2 m long, 0.15-0.6 m thick, clusters of 3-9 round a common root, pointing along the wall's inward normal
  ±25°), never from the floor's walking band (they would trip collision) except at its edge.
- **beams** (C3): the Hall of Beams shell (180 x 90 x 50 m), its floor of scalding pools between mineral rims, **selenite
  beams** (long six-sided prisms 4-14 m, 0.4-1.4 m thick, white #F2F0E8 with grey growth bands) crossing at angles from wall
  to wall; the three great beams bridge the whole width 20 m above the pools with collision (walkable), the rest are scenery
  with collision only where you can reach them.
- **tube** (C4): a lava tube swept along a Catmull-Rom spline, round in section (6-15 m across) with a **flat floor** at
  0.55 r below the axis and a **bench ledge** each side 1.2 m up (old lava levels), lava-glaze black #121216 with a glassy
  sheen (vertex alpha gloss 0.6), guano grey drifts on the floor under roosts.
- **chamber** (C5): hewn square rooms from the look kit (dressed basalt #2A2A30 walls in courses, a ward-violet hexafoil
  over every doorway, bronze #8C6A3A fittings), floors polished black basalt with crystal dust lilac-white #D8D0FF. Built
  as kit blocks with box collision (straight walls, not a pushed shell: they were cut by hand).
- **prism** (the Prism Hall, W5): a geode dome 90 m across, 50 m high, amethyst-lined, polished basalt floor.
- **vault** (the Silent Vault): six hewn rooms in a line on a descending axis, lead-plated door frames, nothing glows.

Openings: a hall's wall triangles inside any tube (r x 0.92) or shaft that enters it are removed; a tube's triangles more
than 1.5 m inside a hall's ellipsoid are removed, so each junction overlaps and never cracks. Tube ends are set at the
hall's floor level so floors meet. **Descent shafts** run from the surface hole down to a hall's roof (the roof is cut there)
with their way down built by the descent builder (2.4).

Light and dark (full dark SPEC): no hall has ambient light. Each hall has 1-3 point lights at its crystal clusters whose
intensity follows the hall's **ring level** (2.6); tubes have none. `WRCrystal::InCave` makes `IsUnderground` true (dark
place rules: held lights matter, corruption rules, no building: no underground bases, DD).

### 2.4 The network (every hall, tube and route; map metres, floors in m above sea)

Routes are designed so every hall is reachable on foot from the surface by at least one descent, the boss halls have two
ways in, and the Tyrant's loop is a real loop.

| Hall | Kind / zone | At, floor | Size | Why here (job) |
|---|---|---|---|---|
| G1 North Dome hall | geode C2 | (2150, 4750), +40 | r 28 x 24, h 26 | D1's landing; first sight of a geode hall; its keystone teaches "sound makes light" |
| G2 the Singing School (H1) | geode C2 | (2205, 4860), +30 | r 22, h 18 | Benches, the 21-bar chime wall, the order puzzle |
| G3 Geode Shrine hall | geode C2 | (2100, 5000), +20 | r 30, h 24 | Geode Shrine on its agate ledge; a thin floor over cup G3b |
| G3b cup under G3 | geode C2 | (2112, 5004), +6 | r 12, h 10 | Where the thin floor drops you; tube back up to G6 |
| G4 Bell Throat hall | geode C2 | (2300, 5150), +20 | r 24, h 20 | D2's landing (the bronze bell); route east-south |
| G5 the Broken Nest (H2) | geode C2 | (1955, 5205), -10 | r 20, h 16 | The opened geode, the husks, the egg |
| G6 the Agate Rinds (O2) | geode C2 | (2055, 5255), -20 | r 32 x 26, h 22 | Ore gallery band 2; the Tyrant's turn-back point |
| G7 Upper geode | geode C2 | (1985, 4925), +35 | r 18, h 16 | Resonant crystal hall (the richest lining) |
| J1 West junction | tube C4 | (1700, 5520), -15 | junction | Organ west; the way down to the Ward Chambers |
| J2 Centre junction | tube C4 | (2000, 5560), -25 | junction | The throat from C2 and D3 meet the Organ |
| J3 East junction | tube C4 | (2300, 5605), -35 | junction | The Organ's east end; ways to C3 |
| H6 the Guano Mines | geode C4 | (2050, 5850), -30 | r 18, h 16 | Saltpetre works under the big roost |
| O3 the Silver Tube | tube C4 | (2250, 5800), -50 | tube | Rich iron, silver, amethyst; the Alicanto's hall |
| B6 Sinking Geode | geode C3 | (2000, 5600), +40 | r 20, h 18 | D3's landing; the collapsing dome (thin floor on the ramp) |
| B2 the Steam Baths (H3) | geode C3 | (2200, 5700), -10 | r 22, h 16 | The bathhouse round hot pools |
| B1 the Hall of Beams (L4) | beams C3 | (2050, 5950), -60 | 90 x 45, h 52 | The great hall; the Beam Shrine on the central beam |
| B4 the Tyrant's Wallow | geode C3 | (2150, 6055), -40 | r 26, h 18 | The Tyrant's lair: a hot pool |
| B3 the Chandelier Gallery (H4) | geode C3 | (1950, 6150), -30 | 40 x 12, h 16 | Six chandeliers on the Tyrant's route; the fallen one |
| B5 South Steam hall | geode C3 | (2100, 6200), 0 | r 22, h 18 | D4's landing |
| W1 the Severance Gate (H8) | chamber C5 | (1835, 5420), +18 | 14 x 10, h 8 | The great hexafoil door in the gorge's east wall; the guard room |
| W1s the Severance stair | shaft C5 | (1850, 5390), +18 -> -112 | r 6 | The Wardcarvers' spiral stair down to the Prism Hall's Severance Door |
| W5 the Prism Hall | prism C5 | (1700, 5300), -120 | r 45, h 50 | Lyrathis |
| W4 the Archive of Wards | chamber C5 | (1620, 5200), -110 | 40 x 40, h 18 | The Mirrored Twins |
| W3 the Lens Grinder's Room (H7) | chamber C5 | (1700, 5100), -80 | 12 x 9, h 6 | The lens on the lathe |
| W2 the Great Hexafoil | chamber C5 | (1780, 5650), -90 | 16 x 16, h 9 | The Great Shrine and Saga Venn |
| W6 the Hall of the Door | geode C5 | (1640, 5850), -150 | r 25, h 20 | The Echo Warden and the Vault Door |
| W7 the Deep Geode (O4) | geode C5 | (1500, 6050), -160 | r 26, h 22 | Star iron, band 4 |
| V1-V6 the Silent Vault | vault | (1600, 5895) -> (1600, 6110), -165 -> -180 | 6 rooms | The planned silence |

Tubes (each a lava tube or a hewn passage, `kind` tube / passage): D1 is a stair in the dome rind; G1-G2, G2-G7, G7-G3,
G3-G4, G3b-G6, G3-G6, G6-G5, G6-J2 (the 40 m throat, the C2 -> C4 blend), **the Organ** J1-J2-J3 and its west end to
(1450, 5500), B6-J2, J3-B2, B2-O3, O3-B1, H6-J2 branch, B1-B4, B1-B3, B4-B5, B3-(Organ east by the Tyrant's way)-J3,
J1-W2 (the descending way, via (1650, 5600, -50)), W2-W4 (passage), W4-W3, W4-W5 (the West Door), W1s-W5 (the Severance
Door), W2-W6 (passage down), W6-W7, W6-V1 (the Vault Door, a lead-plated round door). Every tube is at least 6 m across
(Lumenhorns duck under 6 m, so the narrow ones are passages, not tubes).

### 2.5 Places (POIs, landmarks, sites, decoration)

All composed in code from the look kit at full scale, each with its night light (world-map night rule), every one placed
for a reason (biome-crystal 2.3-2.4, 2.20). `WRCrystalSites.cpp`.

- **Camps K1-K3** (cliff mouths over coves): a geode broken open by the sea at the cliff (amethyst half-shell 8-12 m), a
  newcomer wreck in the cove below (hull timbers on a crystal reef), a fire ring, bedrolls, a rope-and-plank stair down the
  cliff to the cove, a chime-stone arch at the top (the waymark), a bounty post is NOT placed (PvP pass not built).
- **L1 the Prism Crown**: eight violet spires 40-60 m (stacked crystal prisms, each tilted 2-8 degrees off vertical,
  smaller points at their feet) in a ring r 30 m round (1850, 5300), a bronze collar with a hexafoil at each base; at night
  a faint violet column straight up from each spire whose prism below faces its shaft (2.16: driven by the boss's turntable
  states), all eight blazing in Lyrathis's phase 3 and in the Resonance.
- **L2 the Hollowed Giantess** (1820, 5380): a 30 m kneeling crystal woman facing west into the gorge, arms spread, her back
  whole (lilac glow), her front a black hollow full of grey bleached crystal; a ward cage of bronze rods round her (16 rods,
  4 rings). The Severance Shrine's brazier stands inside the cage.
- **L3 the Split Geode** (2430, 5750): an 80 m half-geode in the east cliff, its violet interior facing the sea, surf at its
  foot, Chimecrab ledges.
- **L4 the Hall of Beams** and **L5 the Organ**: the cave builders above; the Organ's eleven vents are holes in the plateau
  with a chime-stone collar each (wind sounds the chord).
- **Descents**: D1 dome-lip stairs cut into the rind (a stair ramp spiralling the hole's wall); D2 the Bell Throat (a 2 m
  bronze bell on a basalt beam across the throat, a rope-and-plank spiral stair hung on bronze pins down the wall: **a walkable
  stair, not a climb**, since the build has no free climbing); D3 a ramp of fallen rind blocks; D4 stone steps spiralling the
  steaming vent's wall. Each has its Wardcarver chime-stone arch at the lip (KEPT waymark).
- **Surface sites S1-S6** (the Watch-Cairn Line, the Ringing Wreck, the Dome of the Dropped Hammer, the Wax Fields, the
  Alicanto Runs, the Chime Garden) and **cave sites H1-H8**, **the Vault rooms V1-V6**: as biome-crystal 2.4 / 2.5, each
  with its keepsake / cache (E to take, server-owned, once per player) or its resource nodes.
- **Kit POIs**: surface (2 resonant outcrops, a Druseback ledge, a Lumenhorn pool, the vista stack), caves by a seeded pass:
  each hall gets its listed kit (outcrop, nest geode, roost, chime junction cache behind a ward, mineral basin, Chimecrab
  ledge, wallow, cave lake), each tube a chime stone every 150 m and a kit POI wherever the route is > 125 m from a POI.
  Every kit POI has a crystal that glows when you arrive (your steps light it).
- **Decoration**: surface hex columns, salt-grass and heather tufts (GV shrubs re-tinted), SimpleRocks basalt boulders, sea
  campion; caves the crystal points, agate ledges, dust drifts, guano mounds, ochre dot panels (red hand-dots at resonant
  points), Wardcarver bronze ladders on walls (decor), cave moss for Lumenhorns.
- **Lore items** (7): Watch tally, Child's tuning fork, Count of the quiet, Order of the door, Two plates, Wax seal, The
  miner's last page (`category: lore`, item descriptions only).

### 2.6 Sound makes light (the rule)

- **Noise** (`WRCrystal::Emit(World, At, Radius, Instigator, Kind)`, server): from hits (ApplyHit: 12 m on a creature,
  20 m on crystal), casts (10 m, ability activation), chimes (25 m), bellows (40 m), powder charges (80 m), thrown stones
  (15 m where they land), roll (8), jump landing (6) and footsteps (server samples every player and creature in the land
  every 0.25 s: walk 4 / run 8 / sprint 15 / crouch 0). Listeners: Echobats, the Echo Warden, Spindleglass (footsteps,
  rolls, landings only), chandeliers, the Vault door, crystal light.
- **Light** (every machine): discrete noises (not footsteps) are multicast from `AWRCrystalState` (unreliable, pos + radius);
  footsteps are computed locally on each client from the characters it sees (same rule, no traffic). Each hall's `MID`
  (material `M_CrystalGeode`) carries three ring slots (position, strength, radius) and a hall `Rest` / `Lit`; crystals glow
  `rest 0.15 + strength x (1 - d / r)` falling over `fade 6 s`; the hall's point lights follow the strongest ring. Rain on the
  surface raises C2's rest to 0.6 (drip song).
- **Keystone chimes** (`AWRKeystoneChime`, one per hall, at its far end under a cluster): E (or any hit) strikes it: a 25 m
  noise, the hall's `LitUntil` = now + 20 game minutes (replicated), the stinger, and the hall's map reveal (map fog).

### 2.7 Resources, crops, gathering

- **Resonant crystal** nodes (`AWRResourceNode`, look `resonant`: a violet cluster with a pale blue phantom inside, 1-3 m):
  48 in C2 / C5 linings, 2 surface outcrops; respawn 2 game days. **Pick on the ring**: each strike rings 1.2 s (a shimmer on
  the node); a strike while it rings yields +1. Every strike is a 20 m noise. The Resonance regrows every node at once.
- **Other nodes**: crystal dust drifts (hand), selenite stubs at C3 beam ends (pick), doubling spar (6 in C5, rare), amethyst
  gem clusters (O3), saltpetre (H6, Echobat roosts), ochre (C4 floors, hand), wax (S4 vats, V1 bowls). Ores by gallery O1-O4
  (copper / tin / stone / clay; iron / coal / gems / saltpetre; rich iron / silver / amethyst; star iron).
- **Crops**: candle rhubarb grows wild at S4 the Wax Fields; glowmelon in the Steam Baths' warm dark (both `needs: dark`,
  data KEPT); seeds from gathering them.
- Items new: selenite, selenite pane, doubling spar, gem amethyst, ochre, wax, prism plate, glass leg, clear membrane, venom
  gland, echobat membrane, mite crystal, crystal antler, drusy plate, glow feather, arcane essence, core_lyrathis, prism
  heart, the trophies' talismans (exist), wardcarver compass, carver's chisel, venn's ring, ward fork, silver bait, attuning
  chalk, lumen lantern, prismglass spear (+ blueprint), glassleg bow, drusy shield, prism plate shield, the four found runes,
  the seven lore items; recipes per biome-crystal 2.7 / 2.19.2.

### 2.8 The Silent Vault

`AWRVaultDoor` in W6: a round lead-plated door 6 m across on a bronze hinge-ring; a hexafoil before it. Server: a player on
the hexafoil and **no noise heard within 20 m for 10 s** -> its six rim petals dim one each 1.7 s (replicated count); any
noise relights all six; the sixth dims -> the door rolls aside on its rail over 3 s (eased, server clock) and stays open
60 s; locked while the Resonance is on. Inside (`WRCrystal::InVault`): spells fail (a violet dust puff,
`UWRGameplayAbility` spell check) except passives; path resources don't regen; the audio director plays the planned silence
(no music, no beds) with the heartbeat only under 30% HP. Six rooms with their pay (V3 ward forks x3, V6 the Carver's Chisel
and the Rune of Silence, once per player).

### 2.9 Hazards and their machines

- **Ward fields** (`AWRWardField`, replicated: `DownUntil`, pattern): a sheet across a doorway (size from data), invisible
  but a faint violet shimmer at its edges; **tell** on every machine when an unattuned character is within 3 m: the floor's
  Chladni dust lines rise (pale strips in a nodal pattern along the plane, fading in over 0.9 s), the hexafoil over the door
  brightens, the hum rises 49 -> 196 Hz (sound row). **Effect** (server): crossing the plane unattuned: 25 arcane through
  `ApplyHit` (unblockable, no stance) and a 600 cm throw back along the way you came; 1.5 s before it throws you again;
  kills. **Attuned**: the Arcane path (main or sub), the ward charm (`passWards`), Wardcloth 4-piece, Venn's ring; attuning
  chalk = half damage, no throw. Crystal creatures pass; void creatures take x2. **Keystone** (`AWRWardKeystone`: a lead
  plate with a hexafoil, set in sight of its field): any projectile or thrown thing that strikes it drops the field 90 s
  (descending chime, the dust lies still). Patterns A / B swap in the Resonance. 34 fields from data.
- **Shard chandeliers** (`AWRChandelier`, replicated: state, shiver start, fall start): hung from a bronze chain to an iron
  ring leaded into the ceiling; a noise of 15 m or more within its reach -> it tinkles 1.0 s (every crystal shivers on the
  clock), then falls straight down (gravity curve on the server clock, never a snap): 300 cm, 70 damage, impact 5 to players
  and creatures through `ApplyHit`; the Geode Tyrant takes 400 and a 6 s stun. A shard pile stays; it re-hangs after 2 game
  days (the chain draws it up over 20 s).
- **Scalding pools** (`AWRScaldPool`): teal water #4FA8A0 with mineral-gold rims #C8A048 and rising steam; wading: burn 15 / s
  buildup + 4 / s through `ApplyHit` (fire), immersed 12 / s; Burning blood (Fire root) and fire-resist gear pass / halve.
- **Thin geode floors** (`AWRThinFloor`, replicated broken): a crust disc with hairline cracks lit violet from below; hollow
  footstep boom within 6 m; a roll, a heavy, a jump landing or a heavy creature breaks a 4 m hole (collision off on every
  machine), dropping you 8-14 m into the cup below; it reforms after 2 game days.

### 2.10 Motion law

Everything that moves is a pure function of the server clock or eases to a replicated target: turntables (45 degrees over
0.8 s, eased, pawl clack at the end), the Spar Lens gimbal (a quarter turn over 1.0 s every 12 s), the lens magazine chain
(lowering a spare over 20 s), chandeliers (shiver, fall, re-hang), the Vault door (3 s roll on its rail), thin floors, the
Prism Crown columns.

### 2.11 Weather

`weather.json crystal` (row exists, underground true): clear is the remainder (0.4), overcast 0.2, **rain 0.3** (drip song:
`dripSong` in tuning: C2's crystal rest glow 0.6, Echobat hearing x0.7 in C2), **fog 0.1** (sea fog over the plateau; the
chime arches ring). Underground: no weather (the row's `underground` already keeps the caves out).

### 2.12 The Resonance; invasions

World event row `resonance` (biome crystal, one game day, about every 3 real days). `AWRCrystalState` drives it: warning
(5 min: a 40 Hz swell, the Crown blazes, chime stones ring by themselves), **peals** every 240 s from the Prism Hall outward
at 300 m/s (every crystal at full, Echobats deafened 30 s, chandeliers in its path fall, nest geodes crack and release
Geodemite swarms), the **ward shift** (pattern B), the **regrowth** (every resonant node at the first peal; six singing
geodes split open: resonant crystal x12 + amethyst; 10% each a found rune), the Vault locked; resonant crystal x3 for its
length. Invasions Arcane <-> Void: rows in `world.json invasions.pairs` with both mouths at the Severance (the system exists:
`WRWorldSystems`), the Geode Tyrant leading the Crystal side.

### 2.13 Shrines, master, quests, portals

`world.json`: biome `crystal` (path arcane, signature resonant_crystal, essence arcane_essence, land crystal, waymark
chime_stone, rock / accent / glow colours); 5 shrines on the land with their guards (the Great Hexafoil has none); Saga Venn
at the Great Shrine (`masters`) with her six quests in the existing kinds (event: `keystone_struck`, `ring_mined`,
`ward_dropped`, `quiet_road`; kill: `mirrored_twins`; event `vault_chisel`). Portals: `portals.json crystal` stays
`land: false` (no Crystal portal kit exists; Storm peaks, Swamp and Wasteland are the same): hidden, a later cook.

### 2.14 Creatures (9)

No sculpted crystal creature exists and ComfyUI is off-limits, so each borrows the nearest real rig by body plan with its
real clips, the cook's palette as a tint over the rig texture (`M_CreatureTint`, the Void / Swamp precedent) and silhouette
parts built in code on bone frames (`UWRCrystalBody`). Mapping:

| Creature | Rig (why) | Parts in code |
|---|---|---|
| Prismback | Magmaw (a low long heavy lizard body, the Murkjaw precedent) x1.3 | 4 rows of tilted mirror-plates along the spine (crystal face, horn edge), they turn toward a bow draw / cast |
| Spindleglass | Kilnmite core (its small limbs read as pedipalps) hung 3 m up, x2.2 | 8 selenite-banded long legs, procedural with planted feet (each foot steps when > 1.6 m from its rest point, 0.25 s arc), the clear body with a beating heart, clouding pink-grey when it hunts |
| Echobat | MountainDragon at bat size (membrane wings; Scorchwing precedent) | huge pale ears, a leaf nose, translucent teeth |
| Geodemite | Kilnmite x0.55 (anvil mite precedent) | a clear violet abdomen (honeypot) |
| Lumenhorn | Frostantler (tall antlered herd animal) x1.15 | a 3.2 m palmate crystal antler spread on the head bone, lit lilac-white |
| Druseback | Rimeshell (a shelled ground body) x2.4 widened | seven split-geode back segments with violet crystal at each seam |
| Gemsniffer | Pumicehog x1.6 (the Bogsnout precedent) | a long tubular snout, tall ears, a glowing brow gem |
| Alicanto | Ashstrider (a long-legged biped bird, the Mirestrider precedent) x0.9 | wide glowing wing fans held open, gold eye |
| Chimecrab | Kilnmite x1.1 | a geode shell (basalt outside, violet druse in the open face) |

Attack rows: biome-crystal 2.12.2 to `attacks.json` (the runner's motions; `mark` buildup on bites). Behaviours in
`FWRCrystalAI`: Prismback plate turn (reflects projectiles from its plated flank: the projectile flies back along its line
x0.8, never turning) and Glare (a white-out flash on the victim's screen and lock-on dropped, only with a light > 1 nit
within 15 m); Spindleglass vibration hunting, leg spear from above, Lift grab, shed leg (a twitching ringing leg on the
floor that pulls hearing hunters and your lock), leg parts (3 broken: collapse, 5: crawls); Echobat hearing-only hunting with
click trains (rate by state), the Shriek ripple (a moving ring of air at 900 cm/s), dive bite, deafened in the Resonance;
Geodemite latch (mark 10 / s, max 4, a roll or a hit shakes one off), burst on death; Lumenhorn bellow (a 40 m noise that
calls Echobats), rider light; Druseback roll (damage x0.3 rolled) and floor breaking; Gemsniffer sensing (points and
scratches toward the nearest node / cache within 20 m); Alicanto dim-and-run when it sees you, ore lead when tamed;
Chimecrab ring when hit. Dens per zone with day / night tables (`crystal.json spawns`).

### 2.15 Mini bosses (3,000 HP solo; stance 90; 2 parries; run-back stone)

- **The Mirrored Twins** (W4 the Archive of Wards: 40 x 40 m, 18 m high, a 2 cm water film on black basalt, four pillars,
  the **Spar Lens** 1.5 m in a bronze gimbal arm under a light vent, the **lens magazine** on the north wall: 3 spares on a
  chain over a pulley to a counterweight in a wall slot). Obsidian Warden rig in crystal plate (faceless hexafoil helm, a
  crystal longsword and kite shield built in code). **Real and double**: one real knight (takes damage, parries count) and a
  second actor, the double (hard light: hits as hard, weapons pass through it, no reflection in the water: only the real
  one shows a mirrored copy under the floor). Every 12 s the gimbal turns a quarter (1.0 s, chain rattle, flare): they swap.
  The double mirrors the real one's attack across you. 3 projectile hits on the lens during its flare shatter it: one knight
  for 20 s while the magazine lowers a spare (visible); after 3 spares one knight for good. Moves: Twin cross, Shield bash,
  Mirror lunge, Ward guard (reflects), Lens flare.
- **The Echo Warden** (W6 the Hall of the Door: a 50 m round geode hall, the Vault Door on the far wall, 4 chime stones on
  bronze posts at the compass points, throwable stones). Frostantler rig x2.6 in bronze, a great tuning-fork head, two
  swivelling ear-horns, humming plates. It goes to the **loudest noise** within 30 m; a struck chime sends it to hammer the
  chime 4 s with its back to you (stance x1.5 from behind); each chime breaks after 3 hammerings. Moves: Fork slam (at the
  noise point), Tine sweep, Resonance blast (a jumpable ring after its hits land), Listening stance (2 s; a noise then =
  Snap lunge).
- **The Geode Tyrant** (roaming): Prismback rig x3 (12 m) with 6 plate rows (stance 60, HP 300 each; reflect projectiles;
  strike cracks them), route Wallow -> Chandelier Gallery -> the Organ's east end -> the Agate Rinds -> back (1.5 game days,
  rests 2 game hours in the Wallow); chandeliers drop on it (400, 6 s stun); a charge into a selenite beam stuns it 3 s.
  Moves: Plate charge, Bite, Tail club, Glare, Shard shrug (8 lobbed shards whose landing spots glint the whole flight).

### 2.16 Lyrathis, the Prism Sovereign (12,000 HP solo; stance 160; 3 parries)

Ignessa rig x7.5 (a slender female form; wings folded and plated as her selenite mantle), crystal tint, basalt core glimpsed,
a brow gem, a crown of eight spires, selenite needle hair that chimes, a turning heart crystal, wrist blades (parts). The
**Prism Hall** (W5): the dome, the dais and its Arcane altar (the path unlocks there, SPEC), four broken pillars, **eight
prisms on bronze turntables** (`AWRPrismTurntable`: a 4 m bronze disc on a ring of rollers in a carved race; a ring gear on its
rim meshing a pinion on a vertical shaft up to a lever arm with a hexafoil hand-plate and a counterweight; strike the plate
-> the disc turns exactly 45 degrees clockwise over 0.8 s, eased, the pawl drops home with a clack; an engraved arrow shows
its facing; 0.8 s cooldown; replicated target yaw, clients ease), eight light shafts up to the Crown, two doors with a
run-back stone outside each. Moves: Shardblade x3, Prism beam (from the brow gem through the chosen prism, a harmless guide
thread first, splitting into 3 fanned beams sweeping 40 degrees; damage on contact; the centre beam at the furthest tame),
Shard rain (real shadows 1.0 s), Sovereign's grasp (grab, crimson), Prism ward (the dome at 85% and 65%: broken by a split
beam turned onto it, 400 / s, dome HP 1,200; unbroken it drops at 25 s), Sovereign's pulse, the Sundering Flare (transition;
cover behind prisms / pillars / dais), phase 2 refracted copies (on each prism's line 15 m beyond the ring if it meets open
floor; they repeat her attacks 0.5 s later; turn a prism so its line meets a wall to dissolve its copy), Held facet, Glint
lance (stays 20 s as a small prism), Prism waltz, Late Shardblade, Double prism beam, hidden phase 3 behind artifacts (all
prisms fire round her; three prisms turned to face her within 10 s break the permanent dome). Drops: core_lyrathis, arcane
essence x5, a chance of the Prism heart, the Prismglass spear blueprint, the Geode sanctum decor blueprint (exists), trophy
head (monument exists). Summoning: resonant crystal x20 + prism plate x4 at the basin.

### 2.17 Sound and music

`sounds.json`: voice sets for the 9 creatures and 4 bosses (pitched from the nearest existing recordings and the crystal /
bell families until the fused recordings exist: reported), world rows (crystal ring, keystone stinger, ward hum / throw /
drop, chandelier tinkle / crash / rehang, scald hiss, thin-floor boom / break, turntable lever / rollers / pawl, lens chain /
flare, Vault door, peal, the 40 Hz swell), beds per sub-zone. Music pools `explore.crystal.*`, `combat.crystal`, minis, boss
sections: the composed cues don't exist, so rows carry `waitsFor: music` and the director keeps the nearest calm pool (the
Void precedent); the **Silent Vault silence** works now.

### 2.18 The Arcane path (biome-crystal 2.17, 2.18)

`Data/paths/arcane.json` (the tree exactly as 2.17), Mana (a resource bar: capacity 100 + 15 a rank, regen 6 / s always,
x1.5 with Deep well), shields (`UWRShieldComponent`: an HP layer after guard in `ApplyHit`; max wins), Reflect (shields
bounce reflectable projectiles back), abilities: Arcane missile, Runic strike, Barrier (+ ally mode), Arcane blast, Rune
trap (+ Rune link), Inscribe, Aegis, Starfall, Rune circle; passives Echo, Mana burn, Reflect, Ward allies, Absorb; stat
nodes via `nodeEffects`. Body change in `player_art.json paths.arcane`. Combos with the paths that exist (Fire, Frost,
Lightning, Blood, Void).

### 2.19 Sets and gear

Wardcloth (gear.json set, items and recipes exist; inputs KEPT) and the Geode sanctum (building_sets.json, exists) become
live with `crystal` in `liveBiomes`; geode block from resonant crystal (recipe exists). New gear: Prismglass spear, Glassleg
bow, Drusy shield, Prism plate shield, Lumen lantern, the talismans (exist), Wardcarver compass, Carver's Chisel, Venn's ring,
ward fork, silver bait, attuning chalk; found runes Silence / Prism / Warding / Doubling as `category: rune` gems in the
existing socket system where their stat exists.

---

## 3. Build steps (in order; each compiled before the next; a commit after each group)

1. `Data/lands/crystal.json` (frame, coast, height, features, zones, caves, sites, nodes, fields, chandeliers, pools, floors,
   spawns, keystones) + multi-hole chunks + the Severance gorge joined + `WRBiome` answers + `liveBiomes`.
2. `Tools/terrain/build_crystal_materials.py` -> `M_CrystalGeode` (vertex-colour crystal with phantom, fresnel, ring slots).
3. `WRCrystalCaves.*`: hall / tube / shaft shells with openings and collision, crystal points, selenite beams, chambers,
   per-hall MIDs and lights; `WRCrystal::InCave` / `IsUnderground`.
4. `WRCrystalSites.cpp`: camps, landmarks, descents, surface and cave sites, the Vault rooms, kit POIs, decoration.
5. `WRCrystal.*`: `AWRCrystalState` (noise, light pings, keystone states, the Resonance), `AWRKeystoneChime`, noise model.
6. Hazards: `AWRWardField` + `AWRWardKeystone`, `AWRChandelier`, `AWRScaldPool`, `AWRThinFloor`, `AWRVaultDoor` + silence.
7. Resources: nodes from data, ring rule, crops, keepsakes; items / recipes rows.
8. Creatures: rows, attack rows, `UWRCrystalBody` looks, `FWRCrystalAI` behaviours, dens.
9. Bosses: the Twins (+ lens gimbal, magazine, double), the Echo Warden (+ chimes), the Geode Tyrant (route), Lyrathis (+
   turntables, beams, dome, copies, P3), arenas, run-back stones, summoning.
10. world.json rows (biome, shrines, master, quests, event, invasions), weather, sounds, music rows.
11. The Arcane path (resource, shields, abilities, passives, HUD bar, combos, body change).
12. Spot-check (5), compile gate, CHANGELOG / HANDOFF, commit, push, LANDED, LATER-COOKS.

## 4. Hidden until finished (no placeholders)

Anything not finished in this lane stays hidden: data rows carry `live: false` / `waitsFor`, a path file isn't added until
its abilities work, bosses not built don't spawn. **The PvP pass (biome-crystal 2.21: clan ranks, alliances, anti-zerg,
siege engines, infamy and bounties)** is a separate shared-systems round (R9 in FULL-GAME 11), not the Crystal caves'
content this lane's scope line lists (land, creatures, bosses, resources, crops, weather, shrines, portal slots, sets,
events, sound, music); it goes to LATER-COOKS as its own cook (the raid tools part already exists in `WRRaiding.cpp`).

## 5. Spot-check and proof (2026-11-15, commits 548b0b5 8ad8b88 a794e5c 6728f3a b50fd94 1f38c88 b4681fa)

Spot-checked by reading the code against this plan and THE BAR. Done = built, compiled, pushed (not played: Aaron tests).

| # | Status | Where |
|---|---|---|
| R1-R4 | done | sections 1-3 of this doc |
| R5 | done for what was built | every site/hazard/creature comment says why it is there and what it meets (WRCrystalScenery*.cpp, WRCrystalHazards.cpp, WRCrystalCreatures.cpp) |
| R6 | done for what was built | chandeliers fall on a gravity curve and re-hang over 20 s, the Vault door rolls 3 s on its rail (WRCrystalHazards.cpp MassZ, Slide); the turntables / lens gimbal are boss machines: NOT built |
| R7 | this table | |
| R8-R13 | done | Data/lands/crystal.json + crystal_ground.bin, Data/crystal.json (Tools/terrain/crystal.mjs), WRCrystalCaves.cpp, WRCrystalScenery*.cpp (548b0b5) |
| R14 | done | AWRSilentDoor, WRCrystal::SpellFails, the audio director's silence, V3 / V6 finds (6728f3a) |
| R15 | done | WRCrystal::Emit, UWRCrystalWorld::Light / StepLight, keystone chimes AWRCrystalSpot (a794e5c); map reveal NOT built (the game has no map fog) |
| R16 | done | pick on the ring, WRCrystalThings.cpp SpawnServerActors; Resonance regrowth RegrowNow |
| R17 | done | items.json rows, nodes from crystal.json resources; attuning chalk use (WRCrystal::UseItem) |
| R18 | done | AWRWardField / AWRWardKeystone (6728f3a) |
| R19 | done | sky light fades in caves (WRSky), no hall ambient, hall lights follow noise |
| R20 | done | AWRChandelier, AWRThinFloor, UWRCrystalWorld::StepPools |
| R21 | done | weather.json crystal fog row, drip song (rainRest), plenty after rain (SignatureScale) |
| R22 | done | world.json events.resonance, StepResonance / Peal (6728f3a) |
| R23 | done | world.json five crystal shrines (two on cave floors: the `floor` field) |
| R24 | done | Saga Venn and all six quests (quest 5 back in Part 2, 0a1d18b); her XP into the Arcane path (731fa59) |
| R25-R28 | done | creatures.json / attacks.json rows, WRCrystalCreatures.* (1f38c88); fused-call voices are stand-ins; Prismback path magic and Lumenhorn ordered bellow NOT built |
| R29-R30 | done (Part 2) | WRCrystalBosses.*, WRCrystalBossLook.cpp, WRCrystalLyrathis.cpp (0a1d18b); see P2.12 |
| R31 | stand-in | creature voice rows pitched from the nearest families (sounds.json creatures) |
| R32 | partly | the Vault's silence works; composed music pools NOT built |
| R33-R35 | done (Part 2) | Data/paths/arcane.json, WRArcanePath.*, WRAbilitiesArcane.* (731fa59); three combos shown only |
| R36-R37 | done | Wardcloth and the Geode sanctum come live with `crystal` in liveBiomes |
| R38 | partly | items + recipes rows; Prismglass Spear's light-line heavy and its blueprint drop wait for Lyrathis; the two shields wait for shield items |
| R39 | done | wild candle rhubarb and glowmelon nodes with seeds |
| R40 | kept hidden | portals.json crystal land false (no portal kit) |
| R41 | done | seven lore keepsakes at their sites |
| R42 | separate cook | the PvP pass (LATER-COOKS) |
| R43 | done (Part 2) | world.json invasions.pairs crystal <-> wasteland (0a1d18b) |
| R44-R45 | done for what was built | numbers in tuning crystalWorld / crystalLife / hazards.crystal / crystalEvents, damage through ApplyHit, server authority with replicated state |
| R46 | done | unbuilt things hidden or parked (quest 5, spear blueprint, shields) |
| R47 | done for creatures built | no homing; tells >= 0.9 s on the dangerous rows; crimson only on the Lift grab |
| R48 | done | editor target compiled clean after every piece |
| R49 | done | CHANGELOG Unreleased + HANDOFF in each commit; LANDED / LATER-COOKS lines |
| R50 | done | the Void's tripled creature rows deduped (1f38c88) |

---

# PART 2 (2026-11-15, second run of the lane): the four bosses, the Arcane path, and the rows that waited on them

Part 1 landed 548b0b5..b4681fa. This part builds the rows marked NOT BUILT / partly above. Research and plan first (no
building before this section was written); the ledger below splits each open row into single requirements.

## P2.0 Requirement ledger (Part 2)

| # | Requirement (source) | Plan | Proof |
|---|---|---|---|
| P1 | Full cook again for Part 2: research, plan, build, spot-check (brief) | P2.1-P2.11 | (end) |
| P2 | Mini boss shared rules: 3,000 HP solo +80% a player / tame, stance 90, 2 parries, windups 0.65-0.9, punish 0.8, projectiles >= 0.9 s never home, nothing inside 4 m, lock points head + chest, run-back stone 40-60 m out, first-kill drop flies to the killer, upgrade shards + trophy (crystal 2.13) | P2.3 | |
| P3 | Mirrored Twins: arena W4 (40 x 40 x 18 m, water film, four pillars), Spar Lens on a bronze gimbal arm under a light vent, lens magazine (3 spares on a chain over a pulley to a counterweight in a wall slot) (2.13.1) | P2.4 | |
| P4 | Twins: one real knight + a hard-light double; weapons pass through the double; only the real one has a reflection in the water (2.13.1) | P2.4 | |
| P5 | Twins: every 12 s the gimbal turns a quarter over 1.0 s (chain rattle, high chime, flare) and the two swap; the real one takes no damage during the flare (2.13.1) | P2.4 | |
| P6 | Twins: the double mirrors the real one's attack across your position at the same instant (2.13.1) | P2.4 | |
| P7 | Twins: 3 projectile / spell hits on the lens during its flare shatter it: one knight for 20 s while the magazine lowers a spare (visible, clanking 20 s); after 3 spares, one knight for good (2.13.1) | P2.4 | |
| P8 | Twins moves: Twin cross, Shield bash, Mirror lunge (converging), Ward guard (reflects 3 s), Lens flare; parries count only on the real one (parrying the double fails and leaves you open) (2.13.1) | P2.4 | |
| P9 | Twins trophy Twin shard (first hit after a dodge lands twice, the second at 40%); drops upgrade shards, doubling spar x2 (2.13.1) | P2.4 | |
| P10 | Echo Warden arena W6 (50 m round, the Vault Door on the far wall, 4 chime stones on bronze posts at the compass points, throwable stones) (2.13.2) | P2.5 | |
| P11 | Echo Warden look: blind bronze construct 5 m on four long legs, tuning-fork head (tines 2.5 m), two swivelling ear-horns, humming back plates (2.13.2) | P2.5 | |
| P12 | Echo Warden hunts the loudest noise within 30 m; crouch silent; a thrown stone (15 m) pulls it; a struck chime (25 m) sends it to hammer the chime 4 s back to you (stance x1.5 from behind); a chime breaks after 3 hammerings (2.13.2) | P2.5 | |
| P13 | Echo Warden moves: Fork slam at the noise point, Tine sweep, Resonance blast (ring after its hits land, jumpable), Listening stance (2 s; a noise = Snap lunge), chime hammering (2.13.2) | P2.5 | |
| P14 | Echo Warden trophy Warden's tuning fork (your footsteps and rolls make half the noise); drops upgrade shards, a Ward fork (2.13.2) | P2.5 | |
| P15 | Geode Tyrant look: Prismback 12 m long, 4.5 m at the shoulder, split-geode plates to 3 m, scarred hide, one clouded eye (2.13.3) | P2.6 | |
| P16 | Geode Tyrant roams Wallow -> Chandelier Gallery -> the Organ's east end -> the Agate Rinds -> back, one loop per 1.5 game days, rests 2 game hours in the Wallow (2.13.3) | P2.6 | |
| P17 | Tyrant counters: plates reflect every spell and projectile; melee and Void pass; strike cracks rows (6 rows, stance 60, HP 300; broken = no reflection there); chandeliers 400 + 6 s stun; a charge into a selenite beam: beam shatters, 3 s stun (2.13.3) | P2.6 | |
| P18 | Tyrant moves: Plate charge, Bite, Tail club, Glare, Shard shrug (8 lobbed shards, glinting landing spots, never home) (2.13.3) | P2.6 | |
| P19 | Tyrant trophy Tyrant plate (Barrier / Aegis +20%; non-Arcane block stamina -10%); drops upgrade shards, prism plate x6 (2.13.3) | P2.6 | |
| P20 | Lyrathis: 9 m crystal giantess (violet glass over a basalt core, brow gem, crown of eight spires, chiming selenite needle hair, turning heart crystal, wrist blades); steps plant and grind (dust); 12,000 HP solo, stance 160, 3 parries; weak to Void x1.5; lock points brow gem, heart, both wrist blades (parts, stance 50: a broken blade is gone, Shardblade uses the other arm) (2.14) | P2.7 | |
| P21 | Prism Hall: geode dome 90 x 50 m, polished floor whose dust leaps into Chladni lines when she stamps, the dais and its Arcane altar, four broken pillars (2.14.1) | P2.7 | |
| P22 | Eight prisms on bronze turntables 22 m round the dais: disc on rollers in a race, ring gear and pinion on a vertical shaft up to a lever with a hexafoil hand-plate and a counterweight, pawl and ratchet (clockwise only); strike the plate: exactly 45 degrees over 0.8 s eased, pawl clack, engraved arrow, 0.8 s cooldown, replicated target; nothing passes through anything (2.14.1, Aaron 11-14) | P2.7 | |
| P23 | Eight light shafts to the Prism Crown; a prism facing its shaft throws part of her beam up it (its spire beams on the surface); all eight blaze in phase 3 (2.14.1, L1) | P2.7 | |
| P24 | Two doors (West, Severance) with a run-back stone outside each (2.14.1, 2.21.8) | P2.7 | |
| P25 | Summoning altar: resonant crystal x20 + prism plate x4 at the basin while she's on her timer wakes her early (RAVEN-PLANNED) | P2.7 | |
| P26 | Lyrathis phase 1: Shardblade x3; Prism beam (guide thread, chosen prism, 3 fanned beams sweeping 40 degrees, centre at the furthest tame, cover); Shard rain (real shadows 1.0 s); Sovereign's grasp (crimson grab, 150 to the wrist frees you); Prism ward at 85% / 65% (a split beam on it 400 / s, HP 1,200, mana burn x2, void x3, a rift eats it; broken: 3 s stagger; unbroken drops at 25 s); Sovereign's pulse while the dome is up (2.14.2) | P2.7 | |
| P27 | Transition: the Sundering Flare (line-of-sight burst, cover behind prisms / pillars / dais, 50% damage taken in its windup) (2.14.2) | P2.7 | |
| P28 | Phase 2 copies on prism lines meeting open floor 15 m past the ring; repeat her attacks 0.5 s later; take no damage; turn the prism so its line meets a wall or a prism: the copy dissolves (20 s before it re-forms); she never stands on a prism line (2.14.2) | P2.7 | |
| P29 | Phase 2 moves: Held facet, Glint lance (stays 20 s as a small prism: beams split through it, a copy can form on its line, 3 hits shatter it), Prism waltz, Late Shardblade, Double prism beam (2.14.2) | P2.7 | |
| P30 | Hidden phase 3: all prisms fire as a beam turns 30 degrees a second; permanent dome breaks only when three prisms face her within 10 s (8 s stagger); the Crown blazes; damage x1.4 (2.14.2) | P2.7 | |
| P31 | Rhythm: phase 1 gaps 0.6-1.4 s, one ranged in flight; phase 2 0.3-0.9 s, two in flight; never the same move 3 times (2.14.2) | P2.7 | |
| P32 | Drops: core_lyrathis, arcane essence x5, chance of the Prism heart, Prismglass spear blueprint, Geode sanctum decor blueprint, trophy head; first-kill drop flies (2.14.2) | P2.7 | |
| P33 | Boss voice rows: tell, strike, hit, pain for the four (2.15) | P2.8 | |
| P34 | Music: boss cues composed (Twins canon, Warden sparse, Tyrant brass, Lyrathis I / II / III); stand-in rule if no composed asset (brief) | P2.8 | |
| P35 | Arcane path Mana (100 + 15 a rank, regen 6 / s always, Deep well x1.5 main only), violet bar under HP (2.17) | P2.9 | |
| P36 | Arcane abilities: Arcane missile, Runic strike, Barrier (+ ally mode), Arcane blast (chargeable), Rune trap (max 3, visible, not in another clan's claim), Inscribe, Aegis (blocks projectiles; a catapult stone breaks it), Starfall (stars glint 1.2 s before), Rune circle (2.17) | P2.9 | |
| P37 | Arcane passives / stats: Mana capacity, Mark chance, Hardened ward, Reflect, Ward allies, Absorb, Focus, Echo, Quick cast, Mana burn, Glyph power, Rune link, Lasting runes, Deep well (2.17) | P2.9 | |
| P38 | Shields: a new HP layer after guard, the bigger wins, faceted violet film, blocks buildup while it holds; reflectable projectiles bounce off Reflect shields (2.17) | P2.9 | |
| P39 | Arcane combos with the built paths (Fire, Lightning, Wind, Frost, Blood, Nature, Necromancy) (2.17.5); Sun / Tide / Spore pairs wait for their paths | P2.9 | |
| P40 | Arcane body change: rosettes, stages 0.3 / 0.6 / 1.0 (player_art row), 2 nits 2 s on each cast, fingertip trace and a chime per cast at 1.0 (2.18) | P2.9 | |
| P41 | Saga Venn's quest 5 "Two of one" restored; her quest XP paid into the Arcane path (brief R24) | P2.10 | |
| P42 | Prismglass spear special (a charged heavy throws two light-lines 15 degrees each side; light hits build Mark) and its blueprint from Lyrathis (brief R38) | P2.10 | |
| P43 | Arcane <-> Void invasions through the Severance: the Geode Tyrant leads the Crystal side, the Echo the Void side (brief R43; crystal 2.22, void 2.22) | P2.10 | |
| P44 | Prismback path magic: hold Interact 4 s with 30 Mana on a knocked-out or < 30% Prismback (10+ Arcane points): a full narcotic dose (2.12.1) | P2.10 | |
| P45 | Lumenhorn bellow on order: 40 m noise, crystals flare, calls Echobats (2.12.1) | P2.10 | |
| P46 | Numbers in data, every blow through ApplyHit, server authority, replicated state (brief) | all | |
| P47 | Machines move smoothly on believable mechanisms (gimbal, magazine chain, turntables, chimes, plates), never snap (Aaron 11-14) | P2.4-P2.7 | |
| P48 | Found defect: passages with no middle points (W4-W5 West Door, W4-W3, the Vault's rooms V1-V6) were generated with a NaN point: no passage. Fixed in crystal.mjs `tube()` | P2.2 | |
| P49 | CHANGELOG / HANDOFF / proof table / LANDED / LATER-COOKS; land in pieces (brief) | P2.11 | |

## P2.1 Research (this part)

| Game / reference | What it does and why it works | What this part takes |
|---|---|---|
| **Ocarina of Time: the Water Temple's Dark Link** (a misty room of still shallow water) and **Phantom Ganon** (the real rider among painted copies) | The fake is told apart by one physical rule you can read, not a UI marker | Only the real knight casts a reflection in the 2 cm water film; the double has none, and blades pass through it in a ripple of light |
| **Dark Souls: Ornstein and Smough; Elden Ring: the Godskin Duo** | A pair that converges from two sides teaches spacing: step back out of both, never between | The double's blow is the real one's mirrored across you at the same instant; Mirror lunge converges |
| **Ocarina of Time: the Spirit Temple's sun mirrors, Twinrova's reflected beams; Twilight Princess: the Arbiter's Grounds turning platforms; The Talos Principle: connectors** | A beam's path is a puzzle changed by an object the player turns; the room is the weapon | Lyrathis's beam goes through the nearest prism and splits; strike a prism's lever and the split beams move: onto her dome, onto a wall to dissolve a copy, three at her in phase 3 |
| **Lighthouse Fresnel pedestals** (Chance Brothers: the lens on a roller chariot in a cast race, turned at a ring gear); **railway turntables** (deck on rollers on a ring rail, driven by a rim pinion); **ratchet and pawl** | A heavy optic turns smoothly on rollers in a race, driven at its rim; the pawl makes it one-way and indexes each step | Each turntable: 4 m bronze disc on 16 rollers in a carved race, ring gear on its rim meshing a pinion on a vertical shaft, the shaft up 1.5 m to a lever with a hexafoil hand-plate and a counterweight, a ratchet wheel and a pawl that drops home every 45 degrees |
| **Minecraft's Warden; The Last of Us' Clickers** | A blind hunter that goes to sound; a thrown object pulls it; silence is the counter; a readout (sculk pulses, clicks) shows what it hears | The Warden goes to the loudest noise within 30 m; thrown stones and struck chimes pull it; crouch is silent; its ear-horns swivel to every sound |
| **Monster Hunter World: environment traps and Rathalos's territory loop; Breath of the Wild: Stone Talus ore on its back** | A roaming monster on a known route past traps you set off; armour broken part by part | The Tyrant walks a real tube route under the Chandelier Gallery; six geode rows reflect until struck apart |
| **Horizon Zero Dawn: the Thunderjaw** | Breaking a part visibly changes what works | A broken row falls off as two geode halves and that flank stops reflecting |
| **Elden Ring: Rennala; Destiny: Atheon** | A shield phase the arena solves; copies that echo the boss | The dome broken only by her own split beam turned onto it; phase 2 copies on prism lines repeating her 0.5 s later |
| **Path of Exile: Energy Shield, Spell Echo; Destiny: Ward of Dawn; WoW: Spell Reflection; Guild Wars 2: Guardian symbols** | A shield is one HP layer before health; a dome stops projectiles, not melee; a reflect returns the bolt the way it came; an echo recast is the same spell a beat later; ground symbols are visible areas | Shields one layer (max wins); Aegis stops projectiles only (a catapult stone breaks it); Reflect returns along the line at x0.8; Echo recasts 0.6 s later free; runes are visible hexafoils linked by violet threads |

Real world: Iceland spar's double refraction (the double is literally the lens's second image); a ship's compass gimbal
(two rings pivoted at right angles: the lens arm); Chladni figures (her stamps); tuning forks (two tines, one pitch; a
mallet strikes them); the amethyst geode section (her dome); calcite rhombs (the prisms: a rhombohedron on a hex foot).

## P2.2 Decisions on the gaps

1. **Files.** `WRCrystalBosses.h/.cpp` (`FWRCrystalBoss`, friend of `AWRBoss` beside the Graveyard's; `UWRCrystalArenas`
   placing them) and `WRCrystalBossLook.cpp` (machines' and bodies' looks). Machines are `AWRCrystalBossProp`
   (replicated Kind / State / Index / times): `lens`, `magazine`, `chime`, `turntable`, `lance`. Every motion is a pure
   function of the server clock and a replicated start and target (Part 1's motion law).
2. **The double** is a second actor (`AWRCreature` id `mirrored_twin_double`, never spawned by dens), driven only by the
   real knight's hooks: it keeps mirrored across the real one's target, plays the same swing (`PlaySwing` replicates),
   takes no damage, overlaps pawns (hard light: walk through it), and its blows are applied by the real knight's strike
   hook from the double's own place (`ApplyHit`, never parryable: a parry fails and opens you). At the flare the two
   trade places, each sliding along the lens light over 1.0 s, eased. The reflection: the real knight alone draws a
   mirrored copy of its pose under the water (a poseable mesh copying the rig, flipped in Z, dimmed).
3. **The Warden's hearing** uses `UWRCrystalWorld::OnNoise` with the noise model's radii (crouch 0). Its goal is a
   point; the boss machine's nominal target is the survivor nearest that point, and the hook moves `AttackPoint` to the
   noise point. Thrown stones already make landing noise.
4. **The Tyrant's route** is the tubes' own floor points (the Wallow B4 -> `B1-B4` -> B1 -> `B1-B3` -> the Chandelier
   Gallery B3 -> back to B1 -> `H6-B1` -> H6 -> `J2-H6` -> J2 -> the Organ east to J3 and back -> `G6-J2` -> the Agate
   Rinds G6 -> the same way home), walked at a roam pace with feeding stops so a loop takes 1.5 game days; 2 game hours
   lying in the Wallow's hot pool. Its home follows it, so a reset sends it on.
5. **Plates and reflection**: six rows on the Tyrant (replicated broken bits); a projectile from a side whose row is
   whole reflects as a real projectile flying back along its line (`FWRCrystalBoss::Reflect`, shared with the Twins'
   Ward guard and the Arcane path's Reflect).
6. **The optics** are computed the same on every machine from replicated numbers (each turntable's clicks and click
   time, the chosen prism and the beam's start). Beam: brow gem -> chosen prism (the guide thread), then three rays at
   the prism's facing -30 / 0 / +30, each sweeping +-20 over the active time, traced against the world up to 4,000 cm.
   A ray on the standing dome deals it 400 / s. Copies stand where the line from her through a prism meets open floor
   15 m past the ring (traced outward: a wall or a prism first, no copy).
7. **Turntable facing**: prism i faces i x 45 + 22.5 + 45 x clicks (its light shaft at clicks % 8 == 0); the server
   counts clicks (0.8 s cooldown) and the drawn yaw eases over 0.8 s from the replicated click time.
8. **The Arcane path** follows Necromancy's frame: `Data/paths/arcane.json`, `WRArcanePath.h/.cpp` (Mana, shields,
   Reflect, passives, combos, look), `WRAbilitiesArcane.h/.cpp` (nine abilities). Mana / MaxMana / Shield on
   `UWRAttributeSet`; the shield film rides a replicated loose tag. Runes and Aegis are actors.
9. **Quest XP into the Arcane path**: Saga's rows pay the master's path (check the quest reward code; add `path` if it
   pays the main path).
10. **Composed music**: none exists in the repo for the Crystal caves; boss pool rows carry `waitsFor: music` and the
    director keeps the nearest pool (the Void precedent). Reported.
11. **Hidden until finished**: anything not landed stays `live: false` / unspawned.

## P2.3 Shared boss frame

- `creatures.json`: `mirrored_twins` (Obsidian Warden rig, crystal tint, 2.6 m), `mirrored_twin_double` (same rig),
  `echo_warden` (Frostantler rig, 5 m), `geode_tyrant` (Magmaw rig, 12 m x 4.5 m), `lyrathis` (Ignessa rig, 9 m, wings
  hidden as on Nhal). Loot, firstKill, lockPoints, parts per 2.13-2.14; trophies are talisman items.
- `attacks.json` rows (W / H / A / P from the cook, tells on bones, crimson only on the grasp and the Flare).
- `tuning.json bosses.<id>` (health, arena / chase radius, every counter number).
- `UWRCrystalArenas` (server, after the caves are built): each boss's spawner, its machines, run-back stones
  (crystal.json `runback`), Lyrathis's altar (path arcane), her summoning stone.

## P2.4 The Mirrored Twins (W4, floor -110)

- Added to the arena: the water film (a dark glaze sheet 2 cm over the floor), the light vent in the ceiling (a hex
  shaft; a pale shaft of light down onto the lens), the **gimbal**: a bronze yoke on a vertical pivot under the vent rim,
  an outer ring on a horizontal pivot inside it, the lens on the second pivot; the yoke turns 90 degrees every 12 s over
  1.0 s (ease in-out), its drive chain over a sprocket on the vent rim. The **magazine** on the north wall: a bronze
  frame with three cradles stacked, a chain from the top cradle over a ceiling pulley down to a counterweight in a slot
  cut in the wall (it rises as a spare lowers); a lowered spare travels down and along a ceiling track to the yoke over
  20 s, eased.
- Rules: swap every 12 s (flare 1.0 s: the real one invulnerable, the lens exposed: 3 projectile / spell hits shatter
  it); shattered: the double fades, a spare comes over 20 s; after 3 spares, none.
- Rows: `twin_cross` (cone 400 x 35 half-angle, W 0.70, A 0.20, P 0.9, 35, impact 3, parryable), `shield_bash` (line 300,
  0.65 / 0.15 / 0.8, 30, impact 4), `mirror_lunge` (line 900, 0.80 / 0.25 / 1.1, 45, impact 4, parryable), `ward_guard`
  (self, 0.60, active 3.0: reflects projectiles).
- Trophy `twin_shard` (stat `dodgeEcho` 0.4).

## P2.5 The Echo Warden (W6, floor -150)

- Body on the Frostantler rig (antlers hidden): bronze plates, a tuning-fork head (a stem, a U-bend, two 2.5 m tines),
  two flared ear-horns on the shoulders yawing to the last noise (eased 0.3 s), four humming back plates, bronze
  greaves lengthening the legs.
- The four chimes become `chime` machines (the hall dressing no longer draws them): a 2 m crystal bell on a bronze post
  and arm; any blow or projectile strikes it (25 m noise, it swings as a damped pendulum from StruckAt); 3 hammerings
  by the Warden break it (cracked after 2; broken: the bell lies shattered at the post's foot).
- Behaviour: a heard list from OnNoise within 30 m; the loudest of the last 1.5 s is its goal. Engaged: a chime goal ->
  walk there and `chime_hammer` (4 s, back to the room: stance x1.5 from behind); else Fork slam at the point, Tine
  sweep when someone is in its cone. `listening` (2.0 s, no damage): a noise in it forces `snap_lunge` at it. After any
  hit lands: `resonance_blast` (low ring 600 at 1,200 cm/s, 30 + mark 40).
- Trophy `wardens_fork` (stat `quietSteps` 0.5: the owner's steps and rolls make half the noise).

## P2.6 The Geode Tyrant (roaming)

- Body: the Prismback kit x3 with six rows of split geodes (basalt rind, violet druse), a clouded left eye, scars; a
  broken row falls as two geode halves left on the floor.
- Plates: a projectile from a whole row's side reflects; a melee strike on a row damages it (stance 60, HP 300, strike
  x1.4, slash x0.7, pierce x0.8, mana burn x2); melee and Void pass to the body.
- Chandeliers exist (400 + 6 s stun). A Plate charge stopped by a selenite beam (the beam halls' scenery boxes) cracks the
  beam (fx, sound) and stuns it 3 s.
- Rows: `plate_charge` (line 2,000 x 220, W 1.00, 1,300 cm/s, 60, impact 5, wallStun 2), `bite` (cone 500, 0.65 / 0.20 /
  0.8, 45, impact 4, parryable), `tail_club` (self 600, behindOnly, 0.90 / 0.30 / 1.0, 50, impact 5, low), `glare` (cone
  1,500, 0.90, whiteout 1.0 s, only with a light within 15 m), `shard_shrug` (1.00: 8 lobbed shards at 900 cm/s into an
  800 cm ring, glint where each lands, 25 each).

## P2.7 Lyrathis (W5, floor -120)

- Body on the Ignessa rig (wings hidden): violet glass two-tone tint over a basalt core, the brow gem (brightens in a beam
  windup), a crown of eight spires, about 40 selenite needles from the head to the shoulders (they shiver and chime when
  she moves fast), the heart crystal turning in her chest (flares in the Pulse and the Flare), wrist blades (gone when
  that part breaks), a mantle of selenite plates on the shoulders.
- Turntables per P2.1; the lever stands outboard of the prism, the counterweight on its far side; the prism (a 6 m
  calcite rhomb on a hex foot, clear violet-white with a phantom) rides the disc's centre boss.
- Beams drawn on every machine as glowing hex rods (the guide thread thin and pale; the fans violet with a white core);
  damage on the server along the same rays: 35 a contact every 0.5 s, mark 30, blockable.
- Phase 1: `shardblade_1..3`, `prism_beam`, `shard_rain`, `sovereigns_grasp`, `prism_ward` (at 85% and 65%),
  `sovereigns_pulse` (dome up only). Transition `sundering_flare` at 50%. Phase 2: copies, `held_facet`, `glint_lance`,
  `prism_waltz_1..3`, `late_shardblade_1..3`, `double_prism_beam`. Phase 3 (artifacts): `all_prisms`.
- Drops: creatures.json loot; `bp_prismglass_spear` as a gear.json blueprint row (boss lyrathis); the Prism heart via
  `BossPart`; the head via deeds.
- The altar: `AWRBossAltar` PathId arcane at the dais's west edge. Summoning: `world.json summoning.effigies.lyrathis`
  (the effigy's recipe: resonant crystal x20 + prism plate x4), its stone by the west run-back.

## P2.8 Sound and music rows

`sounds.json`: tells / strikes / hits / pains for the four from the nearest existing recordings (bell, glass and
creature families), the machines (gimbal turn, magazine chain, lens shatter, chime struck / hammered / cracked, turntable
lever / rollers / pawl, beam hum, dome rise / break, flare swell, copy form / dissolve). Music rows `waitsFor: music`.

## P2.9 The Arcane path

- `Data/paths/arcane.json`: trunk arcane_missile 3, mana_capacity x3, mark_chance x2, runic_strike 2; Ward: barrier 5,
  hardened_ward x5, reflect 4, ward_allies 3, absorb x3, aegis 10; Sorcery: arcane_blast 5, focus x5, echo 4,
  quick_cast x3, mana_burn 3, starfall 10; Rune: rune_trap 5, inscribe 5, glyph_power x4, rune_link 3, lasting_runes x3,
  rune_circle 10; root passive Deep well.
- Mana as an attribute: 100 + 15 a rank, 6 / s (x1.5 Deep well, main path); each ability costs `spells.<key>.manaCost`;
  a refused press flashes the bar.
- Shields in `WRArcane::Incoming` (after guard, before HP): Absorb refunds Mana; buildup blocked while it holds; Reflect
  returns reflectable projectiles (never lobs, beams, siege).
- Abilities with the cook's numbers (2.17). Passives as 2.17. Combos for the built paths (Ball lightning and
  Phylactery shown only until built). HUD Mana bar; the shield as a film on the HP bar. Look: the row exists; a chime
  and a fingertip trace per cast at strength 1.0.

## P2.10 The rows that waited

- Quest 5 back in Saga's list; her quests pay Arcane XP.
- Prismglass spear: a charged heavy with the spear throws two light-lines 15 degrees either side (reach x1.6, 40% of
  the blow, mark 25 each).
- Invasions pairs crystal <-> wasteland (leaders geode_tyrant / echo), mouths at the Severance's two ends.
- Prismback path magic; the Lumenhorn's bellow on order.

## P2.11 Build steps (each compiled; commit + push after each piece)

1. crystal.mjs passage fix (data regenerated).
2. The Arcane path (attributes, WRArcanePath, WRAbilitiesArcane, arcane.json, tuning, combos, HUD, look, quest XP, spear
   lines, Prismback path magic).
3. The boss frame, the Twins, the Echo Warden, the Geode Tyrant (rows, machines, trophies, quest 5).
4. Lyrathis, the turntables, beams, dome, copies, phase 3, altar, summoning, drops; invasions; the Lumenhorn bellow.
5. Spot-check, proof table, CHANGELOG / HANDOFF, LANDED, LATER-COOKS.

## P2.12 Spot-check and proof (Part 2: commits 731fa59, 0a1d18b, 6b3c339; merges cfad5fc, 5cf1a59)

Spot-checked by reading the code against this plan and THE BAR; fixed on the way: a never-turned prism / lens read 45 /
90 degrees off home (FacingNow), the light into a prism and the light up a shaft could hurt people (harmless now), phase
3's turning beam ran over heads (now at chest height), the double and the copies would race two body looks, the three
trophy talismans carried old stand-in stats, quest XP for a path you don't walk was silently lost. Done = built,
compiled, pushed (not played: Aaron tests).

| # | Status | Where |
|---|---|---|
| P1 | done | P2.0-P2.11 above (research P2.1, decisions P2.2, plan P2.3-P2.10, steps P2.11) |
| P2 | done | creatures.json rows (stance 90, health 3000 in tuning `bosses.<id>`), attacks.json rows (windups 0.6-1.0, punish 0.8-1.2), WRCrystalBosses.cpp:1183 PlaceRunback (Twins, Warden; the roaming Tyrant has none, as the island's roamers), first-kill trophies (`firstKill`), upgrade shards in loot |
| P3 | done | WRCrystalBosses.cpp:1317 Place (lens under the vent, magazine on the north wall); WRCrystalBossLook.cpp:286 (gimbal: collar, sprocket, chain, yoke, ring, lens), :387 (magazine: frame, cradles, pulley, track, wall slot, counterweight) |
| P4 | done | double: WRCrystalBosses.cpp:450 TickTwins (spawned unpossessed, overlaps pawns), WRCrystalCreatures.cpp Incoming (hard light takes nothing); reflection: WRCrystalBossLook.cpp:745 + `M_MirrorWater` (Tools/terrain/build_crystal_materials.py) |
| P5 | done | WRCrystalBosses.cpp:534 StartSwap (lens TurnAt / Clicks), TickTwins slide over the flare, Incoming returns 0 while it flares |
| P6 | done | WRCrystalBosses.cpp:575 DoubleStrike (same move, same instant, its own side; lines converge) |
| P7 | done | AWRCrystalBossProp::TakeHit WRCrystalBosses.cpp:1230 (3 shots in the flare), :553 ShatterLens, magazine descent WRCrystalBossLook.cpp TickLook (20 s, counterweight rising) |
| P8 | done | attacks.json mirrored_twins rows; Ward guard reflect in Incoming; the double's blows `bParryable` false |
| P9 | done | items.json talisman_twin_shard `dodgeEcho` (WRArcanePath.cpp AfterHit); loot doubling_spar x2 |
| P10 | done | W6 dressing (stone heaps) + four `chime` machines WRCrystalBosses.cpp:1317, WRCrystalBossLook.cpp:448 |
| P11 | done | WRCrystalBossLook.cpp:768 (fork, swivelling horns, humming plates) |
| P12 | done | WRCrystalBosses.cpp:335 TickIdle (wakes to noise only), :629 TickWarden (loudest noise, chime goal), :745 HammerChime (4 s, stance open), chime cracked / broken after 3 |
| P13 | done | AttackStarted (Fork slam at the noise point), listening / snap lunge (TickWarden), resonance_blast (HitLanded -> forced; TickStrike ring, jump clears) |
| P14 | done | talisman_tuning_fork `quietSteps` (WRCrystalThings.cpp Emit); loot ward_fork |
| P15 | done | WRCrystalBossLook.cpp:792 (six split-geode rows, clouded eye), creatures.json geode_tyrant (Magmaw rig at 4.5 m) |
| P16 | partly | WRCrystalBosses.cpp:758 TyrantRoute (the tubes' floors), TickIdle feeding stops and a 2 game hour rest; the loop's length follows its walk + feedSeconds, not tuned to exactly 1.5 game days |
| P17 | done | Incoming (rows reflect shots from their side, strikes crack rows, Mana burn cracks, the Void passes), chandeliers (Part 1), TickStrike plate_charge into a selenite beam: shatter + 3 s stun; a broken row's halves fall as fx (not left lying) |
| P18 | done | attacks.json geode_tyrant rows; BeginStrike glare (light within 15 m), shard_shrug Lob (glint the whole flight) |
| P19 | done | talisman_tyrant_plate `shieldBonus` / `blockStaminaNonArcane`; loot prism_plate x6 |
| P20 | done | creatures.json lyrathis (12,000, stance 160, parts blade_left / blade_right), WRCrystalLyrathis.cpp:563 body, LyrathisIncoming (Void x1.5); 3 parries: the boss template's |
| P21 | done | Part 1 hall (dais, altar block, pillars); Place: the altar (AWRBossAltar path arcane, hidden under the carved basin) |
| P22 | done | WRCrystalLyrathis.cpp:193 (race, 16 rollers, 48-tooth ring gear, 12-tooth pinion, shaft, lever, hand-plate, counterweight, ratchet, pawl), :284 (eased 45 degrees, pinion half turn, lever swing, pawl), TakeHit (0.8 s cooldown) |
| P23 | done | AWRPrismBeam::Rays WRCrystalLyrathis.cpp:397 (a prism facing its shaft throws light up it past its spire; phase 3: all eight) |
| P24 | done | crystal.json runback (west, severance) -> PlaceRunback |
| P25 | done | world.json summoning effigies lyrathis, recipes.json effigy_lyrathis (resonant crystal x20, prism plate x4), items.json |
| P26 | done | attacks.json lyrathis rows; LyrathisStarted (beam through the nearest prism, tame bend), Strike (rain shadows, ward), LyrathisLanded (grasp), :774 RaiseDome, :795 BreakDome, DomeBurn, LyrathisIncoming |
| P27 | done | sundering_flare row (transition, damageTaken 0.5, unblockable) + LyrathisStrike line of sight |
| P28 | done | :842 StepCopies (prism's facing line, open floor, 20 s re-form), :906 Echo (0.5 s later) |
| P29 | done | rows held_facet, glint_lance (LyrathisStrike: lance stands 20 s as a prism, 3 hits break it), prism_waltz_1..3, late_shardblade_1..3, double_prism_beam |
| P30 | done | :988 TickLyrathis phase 3 (dais, permanent dome, three prisms faced within 10 s, 8 s stagger), all_prisms row, Rays Kind 1 |
| P31 | done | rows' cooldowns / weights; the boss machine's never-three-times and projectile cap |
| P32 | done | creatures.json loot (core, essence x5, blueprints, head), firstKill part_prism_heart, BossPart chance, gear.json bp_prismglass_spear |
| P33 | stand-in | sounds.json crystal.boss.* rows and boss voice families, marked `_standin` |
| P34 | stand-in | music pools boss.mirrored_twins / echo_warden / geode_tyrant / lyrathis(.p1-p3) from composed tracks, marked `_standin` / `_waitsFor` (no composed Crystal cues exist) |
| P35 | done | WRAttributeSet Mana / MaxMana, WRArcanePath.cpp TickOwner, WRUIHud.cpp Mana row |
| P36 | done | WRAbilitiesArcane.cpp (nine abilities, AWRArcaneField rune / dome / circle) |
| P37 | done | arcane.json; WRArcanePath.cpp (DamageScale, Incoming, ScaleBuildups, AfterHit, WindupScale); IsBuilt passives |
| P38 | done | Shield / MaxShield attributes, WRArcane::GiveShield / Incoming / Reflect, UWRStatusComponent buildup block, UWRArcaneWorld films |
| P39 | partly | combos.json: Mana flame, Meteor storm, Quick mind, Mana wings, Frost missile, Frozen rune, Blood magic, Blood star, Mana sap, Faerie ring, Bound dead built; Ball lightning, Rune volley, Phylactery shown only; Sun / Tide / Spore pairs wait for those paths |
| P40 | done | player_art.json paths.arcane (existing), WRArcane::OnCast (chime + fingertip trace at full strength), ApplyLook |
| P41 | done | world.json quests.saga_venn two_of_one; WRWorldSystems.cpp quest pay (WPayPathXP), boss kills credit every fighter |
| P42 | done | WRAbilities.cpp charged heavy -> WRArcane::PrismLines; bp_prismglass_spear in Lyrathis's loot and gear.json (the spear's Soul art waits: souls.json lyrathis live false) |
| P43 | done | world.json invasions.pairs crystal <-> wasteland (leaders geode_tyrant / echo, own pays); WRWorldSystems.cpp per-pair pays |
| P44 | done | WRCrystalCreatures.cpp FWRCrystalAI::PathTamePrompt / TryPathTame / StopPathTame + TickWild hold; interact / release / HUD prompt hooks |
| P45 | done | FWRCrystalAI::Bellow; whistle wheel "Call" (WRPlayerUI.cpp, WRPlayerMenus.cpp) |
| P46 | done | numbers in tuning arcane / spells / bosses.<id>; every blow ApplyHit; server state replicated (props, beams, bodies) |
| P47 | done | eased gimbal, magazine chain, chime pendulum, turntable disc / pinion / lever / pawl, slides for the swap, walking double and copies |
| P48 | done | Tools/terrain/crystal.mjs tube() (731fa59) |
| P49 | done | CHANGELOG / HANDOFF in each commit; LANDED; LATER-COOKS |

Main ledger rows updated by Part 2: R24 done (quest 5 back, XP into the Arcane path); R25-R28 done (path magic and the
ordered bellow built); R29-R30 done (P2-P32 above, two partly rows noted); R31 stand-in; R32 stand-in (the Vault's
silence works; the bosses' pools are marked stand-ins); R33-R35 done (P35-P40; three combos shown only); R38 done (the
spear's light-lines and blueprint; its Soul art and the two shields wait); R43 done.
