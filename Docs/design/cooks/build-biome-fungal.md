# Build cook: build-biome-fungal (the Fungal caverns and the Spore path into Aaron's build)

Lane: build-biome-fungal, desktop, 2026-11-15. Design source: `cooks/biome-fungal.md` (read in full: 983 lines), plus
`world-map.md` (B1 west, the Flue, the camps), `combat.md` (attack-row schema, boss template), `survival-crafting.md`
(Sporeweave, filter mask, oyster cap / lion's mane, Capling / Puffball yields, Mycelium vat / den), `biome-sky.md` (the
Flue, its plumes and Aeolith's spore counter, the Wind side of the invasion) and the three earlier land lanes' build cooks
(`build-biome-void.md`, `build-biome-graveyard.md`, `build-biome-crystal.md`), whose runtime-land, cave and boss frames
this lane reuses. The design is SPEC for this lane; this doc is the build-side research, the requirement ledger and the
plan. No building before this plan was written.

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, and every section of biome-fungal.md. "Plan" points into this doc. "Proof"
is filled at the end (section 5) with file:line or a commit; a row with no proof is not done.

| # | Requirement (source) | Plan |
|---|---|---|
| R1 | Full cook: research, plan, build, spot-check; never skip (brief; Aaron 11-13 night) | 1-5 |
| R2 | Research names real games, the specific places / systems and why they work, plus real-world reference (brief) | 1 |
| R3 | Plan designs layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases before building (brief) | 2 |
| R4 | Every build step listed (brief) | 3 |
| R5 | Design with intention: each object's job, what it connects to, how it meets what it touches (Aaron 11-14) | 2.3-2.7 |
| R6 | Machines look engineered; nothing passes through anything; moving things move continuously on a mechanism, never snap (Aaron 11-14) | 2.8 (bellows vents, Draught gates, springcaps, diving bell, weir) |
| R7 | No missed requirements; every row proven (Aaron 11-14) | 0, 5 |
| R8 | Land on the map: B1 west, the Hollow Cape's west half, surface +40..+140, sea cliffs W and N, Desert edge S, the Spine and the Flue E (fungal 2.1, world-map) | 2.1 |
| R9 | Caverns ~3 km² on three levels (Upper Halls, Middle Deep, the Deep) under the cape (fungal 2.1) | 2.2 |
| R10 | Five sub-zones F1-F5 each with its ground, palette, props, light colour and bed (fungal 2.2) | 2.3 |
| R11 | Blends between sub-zones (vertical fades, Sand Falls Desert border, Flue Sky border) (fungal 2.2) | 2.3 |
| R12 | Read rules: holes read from 300 m, one light colour per sub-zone, a far light in every hall, ceilings always visible, cavern glow on the surface clock (fungal 2.1) | 2.4 |
| R13 | Landmarks L1-L8 (Ancestor Cap + the Breath, Kneeling Titan, Ring of Caps, False Sky, Shoestring Bridge, Fall Field, Drowned Granary, Sand Falls) (fungal 2.3) | 2.5 |
| R14 | POIs every ~150-250 m: 115 (35 surface + 80 cavern), the 125 m max gap rule (brief; fungal 2.4) | 2.5 |
| R15 | Six descents D1-D6 (fungal 2.4) | 2.5 |
| R16 | Surface hand sites S1-S8 (fungal 2.4) | 2.5 |
| R17 | Cavern hand sites C1-C18 (fungal 2.4) | 2.5 |
| R18 | Kit POIs (glow-cap rings, host clusters, puffball fields, ore pockets, summited niches, mite nests, web pockets, glowgrub colonies, capling meadows, shelf deadwood, doline lookouts) and the decoration rule (brief; fungal 2.4, 3.5) | 2.5 |
| R19 | Camps C1-C3 on sinkhole rims with newcomer wrecks in the coves; no underground bases (fungal 2.1, DD) | 2.1, 2.5 |
| R20 | Hidden sub-biome: the Heart Grotto (ring city, 10 rooms, 4 ways in incl. the moat and the mole wall, summited rooftops, the Last House, the Seed Vault) (fungal 2.5) | 2.6 |
| R21 | Ore galleries: Upper Vein, Nitre Gallery, Deep Vein, Rimstone Pocket (fungal 2.6) | 2.5, 2.7 |
| R22 | Signature resource deepcap: only on the dead, knife doubles, 3-day respawn, mole senses (fungal 2.7) | 2.7 |
| R23 | Other resources: spore sac, glow-moss, amadou, rhizomorph cord, webspinner silk, mite chitin, saltpetre, stalker stalks, ores (fungal 2.7) | 2.7 |
| R24 | Crops: oyster cap and lion's mane (dark crops), wild in F2 and C4; Capling / Puffball yields (brief; survival-crafting KEPT) | 2.7 |
| R25 | Spore clouds: 12/s poison, rot after 10 s, 12 m sight, tell, no slow, passes, never over a ledge; three answers wind / fire flash / frost settle (fungal 2.8) | 2.8 |
| R26 | Breath vents at the Flue floor: three bellows-stones, one every 40 s for 6 s, 1.0 s tell, feeding Aeolith's counter (fungal 2.8; biome-sky KEPT) | 2.8 |
| R27 | Micro-hazards: lure lines, springcaps, cap-falls, foul air, each with its tell, effect, answer (fungal 2.8) | 2.8 |
| R28 | Weather: surface Clear 30 / Overcast 20 / Rain 20 / Spore drift 30 with effects; underground Spore bloom every 20 min for 3 min with a 30 s tell; none in the Heart Grotto (brief; fungal 2.9) | 2.9 |
| R29 | Biome event: the Sporefall (warning, seeded ground incl. Desert north 400 m and three sky islands, flush, the Flue at 30 s, bloomed beasts, the noon summit, Seed Vault, pays) (brief; fungal 2.10) | 2.10 |
| R30 | Shrines: five (Great Shrine of the Garden Doline, Ring, Starfall, Cable, Flue Floor) with guards (brief; fungal 2.11) | 2.11 |
| R31 | Path master Ilmari Kask and his 6 quests (fungal 2.11) | 2.11 |
| R32 | Portal slots (brief) | 2.11 |
| R33 | Ten creatures with real models where packs allow, real animation, behaviour, attacks with visible contact (brief: "9"; fungal 2.12 keeps 10) | 2.12 |
| R34 | Creature rules: cloud / foul-air immunity, herds defend, tiers, swarm archetype, dark-hunters, warm light draws Mycomites, cyan spores, no crimson but unblockables (fungal 2.12) | 2.12 |
| R35 | Attack rows for all ten + the Sporeling (fungal 2.12.2), physical resists and lock points | 2.12 |
| R36 | Day / night tables per sub-zone; living-world beats (fungal 2.12.3) | 2.12 |
| R37 | Taming methods and utilities: Tunnel toad mount (12 m leaps), Shroomback hauler (220, oyster caps, 3 m aura), Truffle mole (sense, dig, cap-fall warning), Glowgrub light, Sporestalker path magic (fungal 2.12.1) | 2.12 |
| R38 | Mini: the Sporemother in the Brood Doline (six egg sacs, burn the sacs, egg toss, brood press) (fungal 2.13.1) | 2.13 |
| R39 | Mini: the Rot King in the Steeping Vats (rot pools heal him; frost freezes, wind dries, water channels) (fungal 2.13.2) | 2.13 |
| R40 | Mini: the Weaver, roaming her 1.8 km loop, webbing halls, on-web x1.6 / off-web x0.6, cocoon, tear the web (fungal 2.13.3) | 2.13 |
| R41 | Major: Mycelorn in the Heart chamber: phase 1 rooted crown, the Wind counter (choke), Draught gates, Summit call, the Sinking Root, phase 2 vein travel with Physarum tell and fairy-ring eruptions, mound pods, hidden phase 3 the Whole Cavern; drops; altar; run-back stone (brief; fungal 2.14) | 2.14 |
| R42 | Creature voices (fused calls, 4 layers each) (fungal 2.15) | 2.15 |
| R43 | Music: 70 BPM Bb Locrian cue sheet, always audible, the Fall Field's one planned silence (brief; fungal 2.16) | 2.15 |
| R44 | The Spore path: Spores resource (bar, planted nodules, detonate), 22 nodes, root passive Immune system, costs, passives, PvP, proc feedback (fungal 2.17) | 2.16 |
| R45 | Spore combos for the paths that exist (fungal 2.17) | 2.16 |
| R46 | Spore body change (freckles by stage) (fungal 2.18) | 2.16 |
| R47 | Armour set Sporeweave placed and obtainable (brief; fungal 2.19) | 2.17 |
| R48 | Building set (Mycelium den) placed and obtainable (brief; survival-crafting KEPT) | 2.17 |
| R49 | Cavern items: filter mask, spore wad, antidote, resists, glow-cap lantern, glow paste, spore pot, Gale flask, amadou tinder, rhizomorph rope, chitin arrows, knocker idol, log bed; trophy talismans; Farmer's ring (fungal 2.19) | 2.17 |
| R50 | Story only through the environment; 9 lore items; 5 secrets F-S1..F-S5 (fungal 2.20) | 2.5, 2.6 |
| R51 | Invasions Wind <-> Spore through the Flue, both directions (fungal 2.21) | 2.10 |
| R52 | Data-driven like Fire and Frost: numbers in Data/*.json and tuning, abilities UWRGameplayAbility + ClassForNode, damage through ApplyHit, screens via WR_REGISTER_SCREEN mouse + pad, sounds / fx as rows (brief) | all |
| R53 | Server authority and replication on every new system (brief) | all |
| R54 | No placeholders shown as finished; unfinished stays hidden (brief) | 4 |
| R55 | Combat bar: projectiles never home, >= 0.9 s warning, crimson only on unblockables, small cues, no attacker limit, players die, no helpers (combat KEPT) | 2.12-2.14 |
| R56 | Compile gate clean of new warnings; no tests (brief) | 3 |
| R57 | CHANGELOG Unreleased + HANDOFF in the same commit; lane doc committed after the plan and at the end; LANDED line; LATER-COOKS lines (brief) | 3 |
| R58 | Unity build: unique prefixes for file-scope helpers (lane note) | 3 |
| R59 | Cross-lane: keep the Flue's plume behaviour (biome-sky); the spore pot item Aeolith reads; the Sky island Spore Scar as seeded ground (fungal 2.10, LATER-COOKS 65/70) | 2.8, 2.10 |
| R60 | Watch LFS / binary size; report new binaries (brief) | 3, 5 |

---

## 1. Research (build-side; the design research is biome-fungal.md section 1)

How the best games build a whole biome underground that lights itself, stays readable in the dark and keeps a toxic air
fair, and how they put living cave networks into a running world. Each row says what this build takes.

| Game / system | What it does, why it works | What this build takes |
|---|---|---|
| **Subnautica, the Jellyshroom Cave and the Lost River** (Unknown Worlds) | Whole cave biomes lit only by their own life: the jellyshroom caps pulse purple, the Lost River's green brine glows from the floor, the ghost tree is the one far light. Every cave has one bright thing to swim toward and a ceiling you can always see by its glow | **Self-lit halls with one far light** (fungal 2.1 read rules): the halls' glow-moss, caps and gills are the light (`M_FungalGlow`, emissive by vertex alpha); one real point light per hall stands at its far end (a skylight's shaft, the biggest glowing cap, a sand fall), and every ceiling carries glow so its height reads |
| **Minecraft, Lush Caves (glow berries, moss carpets, dripleaf)** | Vegetation is the cave's lighting and its platforming: glow berries hang as lanterns, big dripleaf tips you down after a beat | Glow is grown, never placed lamps: hanging glow-caps on rhizomorph threads, moss carpets on floors, **springcaps** that pop 0.6 s after you land (dripleaf's beat, inverted into a launch) |
| **Terraria, the Glowing Mushroom biome** | A biome whose every block glows blue; a friendly Truffle NPC lives there; giant mushrooms are trees you harvest | Giant caps are the "trees" of the halls (lathed caps 4-25 m in code, their gills glowing); the friendly master Ilmari lives in a cap-house |
| **Deep Rock Galactic (signed-distance caves)** | Caves as unions of soft shapes, so every junction closes by construction; total darkness makes carried light the drama | **The Crystal lane's cave frame, reused as a type**: halls (ellipsoids with flat floors), dolines (open-topped cylinders), chasms (deep halls with no walkable floor), tubes, shafts, unioned as one signed distance and meshed by surface nets (`WRFungalCaves`), so no crack can exist |
| **Ark: Aberration** (charge light, glowing mushroom forest, depth bands) | Light is a resource with types; the lower bands are the hard ones | Fire light vs cold light (fungal lesson 3): a torch flashes clouds and draws Mycomites from 2x; a glow-cap lantern / Glowgrub / glow paste is safe and burns nothing (`WRFungal::CarriesFire`) |
| **Valheim, the Mistlands** (Wisplight against the mist; the backlash against mist on vertical ground) | The fog must have an answer and never sit on a jump | Clouds are pockets 6-20 m, 12 m sight inside, never within 4 m of a ledge, jump or springcap (the generator checks), with three answers (wind, fire, frost) |
| **Monster Hunter World, the Rotten Vale** (effluvium; Vaal Hazak's effluvial body) | One gas drives a whole ecology and a boss feeds on it; a meter that you clear with a consumable | Spore buildup on the poison / rot meters (no new HUD meter: the status meters are the only readout, DD), the filter mask, the spore wad, Mycelorn feeding on its own clouds until wind chokes it with them |
| **Hollow Knight, the Fungal Wastes** (bounce shrooms) | A tool that is also terrain: once learned, the cave gets fast | Springcaps: hazard and tool, 9 m launches aimable 20 degrees, 20 s regrow |
| **Elden Ring, Siofra River / Nokron (the false night sky), Lake of Rot** | A ceiling of stars underground is the most praised view in the game; rot is % max HP | **The False Sky**: about 6,000 glowing beaded lure-thread cards on the Starfall Hall's ceiling (instanced in one code mesh) with a milky way along its crack; real hazard lines hang lower over the routes |
| **Grounded (spider webs)** | Webs are terrain you cut or burn; spiders come when you touch them | Lure lines and the Weaver's web sheets: cut (a light swing within 1.5 m), burn (fire 3-4 m), tear (wind 8 m); a touched line twangs 0.9 s before it grips, and the nearest Webspinner within 30 m comes |
| **Breath of the Wild / Tears of the Kingdom, the Depths' Lightroots** | Lighting a point reveals the map round it | Glow-cap ring waymarks (the Deepfarmers' kit) brighten in turn toward the nearest undiscovered POI when you stand in one at night (fungal 2.4) |
| **Dark Souls, Blighttown (the lesson) and Darkroot's mushroom men** | Poison + slow + vertical clutter is hated; slow mushroom men with big tells are loved | No slow on any status ground (lesson 5); the Rot King's pools rot, never slow; big, honest windups on every heavy |
| **Hades / Monster Hunter boss rooms with an environmental counter** (the Lernaean Bone Hydra's pillars, MH's slinger + environment) | A room hands everyone the same tool, so a boss is a puzzle anyone can solve | Mycelorn's Draught gates (any path breaks a plug and a gale blasts a lane), the Sporemother's sacs (any fire), the Rot King's vats (frost, wind, water), the Weaver's web (fire, wind, toad croak) |

Real-world reference used (from biome-fungal.md 1.2): *Ophiocordyceps* summit disease (the summited bodies, Mycelorn's
Summit call, the Sporefall's noon summit), *Armillaria* (one organism, rhizomorph cords, the Shoestring Bridge),
bioluminescent fungi on a circadian clock (the glow curve), *Arachnocampa* glowworms (lure lines, the False Sky),
Hang Son Doong's dolines and cloud (the Spore Gardens, the Cloud Room), Movile Cave's air bells (foul air: the blue,
shrinking flame), *Physarum* (phase 2's vein race), *Pilobolus* (springcaps, the stalk volley), fairy rings and their
toad guardians (the Ring of Caps, the eruption tell), Cornish knockers (knocker idols, cap-fall knocking, the Truffle
mole), the olm (pale eyeless cave creatures), Ötzi's amadou, spore-print colours (the Spore Archive), cave saltpetre,
karst clints and grykes (the surface pavement), cave breathing (the Breath Vents' 60 s out / 60 s in).

---

## 2. Plan (every gap solved before building)

### 2.0 What exists on master and the decisions that follow

1. **Data rows already in place from earlier lanes** (built with `liveWith` / biome `fungal` and waiting): items
   deepcap, puffball_sac, webspinner_silk, toad_gland, spore_pot, deepcap_spores, mycel_block, oyster_cap(+seeds),
   lions_mane(+seeds), fish glowfin / cave crayfish / mycelial koi, the Sporeweave set (+ ascendant) and its barding,
   the Mother's cap / Rot crown / Weaver silk trophies and talismans, bootlace_rhizomorph, foxfire_wood /
   foxfire_lantern, bp_mycelium_den_decor; pieces and the Mycelium den building set; farming rows; weather rows;
   achievements; the invasion pair sky_islands -> fungal (dormant). **Decision:** reuse every one of them under its
   existing id (the cook's `rhizomorph_cord` is the built `bootlace_rhizomorph`; its `spore_sac` is `puffball_sac`), add
   only what is missing, and switch the biome live (`world.liveBiomes` += fungal) in the commit that lands the land.
2. **Poison and rot statuses exist** (`UWRStatusComponent` EId Poison / Rot, tuning `status.poison` / `status.rot` with the
   cook's numbers). Spore clouds and every bite feed them through `FWRHit` buildups and `ApplyHit` or the status
   component's `AddBuildup`; nothing new in the status core except the planted-Spores stack (2.16).
3. **No diving exists** (WRWater is surface-only). The cook's moat dive and the two sumps need the shared underwater
   frame. **Decision:** build those three as **flooded crawls**: a low passage with chest-deep water and a 1.6 m air gap
   under the roof (you wade, you don't dive), lit by Glowfin. Same route, same place, no breath meter. LATER-COOKS line:
   turn them into real sumps when the ocean lane's dive lands.
4. **The frame.** The Sky lane's Hollow Cape (`Data/lands/sky.json`, priority 0) is at real metres (game cm = map m x 100)
   and covers x -1,150..+1,900; its west part (the downs at 240 m and the Flue's crown, a dome to 700 m at r 150 falling
   to 240 m at r 980) was left "for the Fungal lane to replace". **Decision:** the Fungal caverns are a heightfield land
   `Data/lands/fungal.json` at real metres, **priority 1**, 4 m samples from x -2,500, y -9,800, 651 samples
   (2,600 m square: x -2,500..+100, y -9,800..-7,200). It owns everything **west of the seam**: the circle r 520 m round the
   Flue's axis (0, -8,800) east of x -150 and the line x -150 north and south of it. Inside the seam the sky's dome stays
   (it buries the Flue's shaft wall, which stands to 700 m). In the last 110 m before the seam the fungal ground rises
   from the plateau to exactly the sky land's height on the seam ring (read from `sky_ground.bin` by the generator): the
   **Spine's foot**, a 60-75 degree wall of grey limestone with fungal shelves, where the design puts it ("x about -200,
   the Spine's foot below the Flue"). East of the seam the fungal samples are hidden.
5. **The Desert edge.** No Desert land exists yet. The fungal land carries the Desert's north strip (y -8,000..-7,300) as
   sand dunes with three sinkholes (the Desert's Sieves over the Sand Falls, L8) and a sea coast at the box edge for now.
   LATER-COOKS: the Desert lane replaces the strip (priority) and keeps the three Sieves at their points.
6. **The cave frame.** The Crystal lane's frame is crystal-specific code (`FWRCrystalData::Air`, `AWRCrystalCaves`). A
   shared refactor would put a landed biome at risk tonight. **Decision:** a fungal twin of the same design
   (`FWRFungalData::Air` + `AWRFungalCaves`, surface nets in 31 m cubes, a few cubes a frame, every machine, no network
   cost), with the fungal shape kinds (hall, doline, chasm, grotto, dome, tube, crawl, shaft) and fungal colouring. The
   mesher is copied, not shared, with `Fg` prefixes (unity build). LATER-COOKS: fold the two meshers into one
   `WRCaveFrame` once both are played.
7. **The Fall Field opening.** The Sky lane's Flue wall is one closed cylinder of procedural mesh with collision. The
   Grottos' river passage must come through it at the floor. **Decision:** `WRSkyParts::Flue` skips the wall quads in a
   window (yaw and height from `Data/fungal.json` `flueMouth`) while the Fungal caverns are live: a 2-quad-wide, 1-row-high
   gap (15 m x 18 m) where the passage's tube meets the shaft; the fungal caves' skin closes round the hole's edges.
8. **Getting there.** The cape is 6 km north of the island. **Decision:** the Spore Crossing (`crossing` in the land
   file, the generic longboat in `WRLandThings`): the island's north-west shore to C3's cove. Spore-path newcomers start
   at the three camps (`camps`, `AWRLand::SpawnCamps`). The Sky land's Wind walkers can walk in over the downs' seam.
9. **Glow.** A new material and a parameter collection (`Tools/terrain/build_fungal_materials.py`, commandlet, the
   Crystal script's pattern): `/Game/Terrain/MPC_Fungal` (GlowScale: the circadian curve; Pulse: the bloom's and the
   Sporefall's sync beat; Breath: the Heart's 35 bpm) and `/Game/Terrain/M_FungalGlow` (vertex colour base, rough 0.55,
   sheen 0.3, emissive = rgb x alpha x GlowColor x MPC.GlowScale x (1 + Pulse x beat)); `/Game/Terrain/M_FungalVeil`
   (translucent, two-sided, fresnel: the Puffball sac, the Rotmaw lure, Mycelorn's amber stipe, the Sporemother's
   abdomen, the cloud flash shell). Two ~20 KB uassets.
10. **The other lanes' frames reused:** `AWRForestScenery` + `FBuild` for every built place (blocks, code meshes, rocks,
    lamps, loops); `AWRResourceNode` + look rows; `AWRWorldCache` caches; `world.json` shrines / masters / quests /
    events / invasions / summoning; the boss hook chain (`FWRCrystalBoss` pattern -> `FWRFungalBoss`); the creature hook
    chain (`FWRCrystalAI` -> `FWRFungalAI`, `UWRCrystalBody` -> `UWRFungalBody`); the Sky lane's `procBody` for the
    bodies no rig has (the Puffball, Mycelorn); the path pattern (`WRArcanePath` / `WRAbilitiesArcane` ->
    `WRSporePath` / `WRAbilitiesSpore`).
11. **Landing order** (each compiled, committed and pushed): (1) the land, caves, scenery, glow; (2) items, recipes,
    nodes, world rows, weather, the crossing, shrines, Ilmari; (3) hazards (clouds and their three answers, lure lines,
    springcaps, cap-falls, foul air, Breath vents), the Spore bloom, the Sporefall, invasions; (4) the ten creatures and
    the Sporeling; (5) the four bosses; (6) the Spore path; (7) sound and music rows, spot-check. Whatever isn't landed by
    the end stays hidden and is listed.

### 2.1 The land (terrain; fungal 2.1, world-map KEPT)

Height in map metres (`Tools/terrain/fungal.mjs`):
1. **Base:** a limestone plateau tilting from +140 at the Spine's foot (x -500) to +60 at the west cliffs (x -2,250) and
   +55 at the north coast, with broad swells (+-12 m at 300 m) and **karst pavement** fine relief (+-0.6 m clints cut by
   grykes: a 6-9 m grid of 0.4 m-wide, 0.8 m-deep cracks rotated 18 degrees, the real joint pattern).
2. **Coast:** sea cliffs 40-80 m high on the west (x -2,300) and north (y -9,450), shingle aprons at their feet, coves at
   the three camps (flatten terraces at +3 m with cliff stairs built by the camp builder); the sea floor -16.
3. **The Spine's foot:** the seam rise of 2.0.4.
4. **Dolines:** the doline field of F2: **11 sinkholes** (round, 30-90 m across, walls 70-85 degrees, ferns on the rims):
   the Garden Doline (D1, r 34, over the Great Shrine's hall), the Brood Doline (D3, r 30), the Springcap Shaft (D4, r 9),
   the Cloud Room's mouth (r 26), five garden dolines over the gardens' halls, the Great Doline (r 130, round the
   Ancestor Cap) and the Long Stair's fissure (D2: a slot 4 m wide, 180 m long). Each is a hidden-sample hole whose wall
   is the cave skin rolling over the lip (2.2). **Breath Vents** (S8): a field of 30 fissures 0.5-1.2 m wide round the
   Great Doline's south rim, hidden samples over little shafts.
5. **Zones (letters):** `1` F1 the Sinkhole Rim; `s` the Spine's foot; `d` the Desert's north strip; `~` sea.
6. **Surfaces:** pavement (limestone grey #A7A498 with dark grykes), salt-grass #7E8C5A, sea thrift speck pink, moss
   rims #5E7A3A round every doline, sand #D8C08A in the Desert strip, shingle, cliff grey, path (bare feet worn into
   the turf: the Deepfarmer routes between descents), the Ring of Caps' clean turf, the Breath Vents' crust (pale
   spore crust #B8B8A8).
7. **The surface's own dressing:** juniper (the tree kit's conifer, wind-bent 10-25 degrees from the west), mossy
   boulders (SimpleRocks tinted limestone), salt-grass tufts (GV shrubs tinted), shelf-fungus deadwood with foxfire at
   night (the forest's log kit + lathed bracket caps), fern skirts on doline rims; Poisson 4-18 m by zone, none on paths,
   camps, sites or doline throats.

### 2.2 The caverns (fungal 2.2-2.6; the cave frame)

`Data/fungal.json caves`: **halls** `{id, kind, zone, x, y, floor, rx, ry, h, yaw, open, light...}`, **tubes**
`{id, kind, zone, pts [[x,y,floor]...], r | w,h}`, **shafts** `{id, kind, x, y, r, z0, z1}`. `AWRFungalCaves` builds the
rock skin; its colour by zone and height (vertex colour, M_HeldTool), its glow skin (moss carpets, ceiling glow, gill
bands) as a second mesh in M_FungalGlow over the same surface where the rules say (floors of F3 halls; ceilings of every
hall above 0.6 h; F5's rimstone rims).

Shape kinds:
- **hall**: an ellipsoid (rx, ry, h) with a flat floor at `floor` (the Glow-moss Halls 40-70 m high).
- **doline**: an open-topped hall: a vertical cylinder (r) from its floor up through the ground, its wall flaring 10% at
  the floor (undercut), the skin rolling over the lip; daylight falls in (the Spore Gardens).
- **chasm**: an elongated hall whose floor is 50 m below its walkways (the Webs): the walkways are rhizomorph cables
  (2.5) with collision; falling to the floor is survivable (a 50 m fall kills: the floor is a talus slope that you can
  walk out of along the chasm's low end into a tube).
- **grotto**: a hall with a lake (a water surface at `lake` m; white sand under teal water; WRWater volume so you swim).
- **dome**: the Heart chamber (r 60, h 70) and the Heart Grotto's ring (a torus-like hall built as 10 linked halls).
- **tube** (round, flat floor at 0.55 r), **crawl** (a low wide passage: 6 m x 2.6 m, used for the mole wall and the
  flooded crawls), **stair** (a hewn passage whose floor steps), **shaft** (a vertical cylinder: descents, vents).

The network (map metres; floors in metres above sea; every hall has a reason):

| Hall | Kind / zone | At, floor | Size rx x ry, h | Why here |
|---|---|---|---|---|
| G1 Garden Doline | doline F2 | (-1500, -8700), +40 | r 34 | D1's floor; the Great Shrine, Ilmari's terraces, the Offering Niches |
| G2 Garden Farm | doline F2 | (-1400, -8640), +38 | r 28 | C2 the farm set-piece, the Capling pen |
| G3 Brood Doline | doline F2 | (-1850, -9150), +40 | r 30 | the Sporemother's arena (D3 rope ladders) |
| G4 Springcap Grove | doline F2 | (-1150, -8950), +30 | r 26 | C3: five bounces to a ledge 40 m up; D4's foot |
| G5 the Cloud Room | hall F2 | (-1800, -8500), +30 | 60 x 45, 38 | C4: a real cloud under the ceiling over a cap jungle; its doline mouth |
| G6 Upper Vein | hall F2 | (-1950, -8700), +30 | 22 x 18, 14 | the newcomer's first copper and tin |
| G7 East garden | doline F2 | (-1250, -8800), +34 | r 24 | a garden doline on the route east (Capling meadow) |
| G8 North garden | doline F2 | (-1600, -9050), +36 | r 22 | a garden doline (puffball field) |
| H1 Starfall Hall (the False Sky) | hall F3 | (-1600, -8000), -60 | 200 x 125, 70 | L4; the Weaver's lair; the Starfall Shrine at its edge |
| H2 the Lure Gallery | hall F3 | (-1750, -8300), -20 | 70 x 14, 12 | C5: long and low, its ceiling thick with lines |
| H3 Knockers' Junction | hall F3 | (-1350, -8150), -40 | 16 x 16, 10 | C7: four tunnels, four idols, the fifth tunnel (F-S2) |
| H4 the Spore Archive | hall F3 | (-1900, -8050), -50 | 20 x 12, 9 | C8 |
| H5 the Nitre Gallery | hall F3 | (-1500, -7900), -70 | 40 x 18, 14 | C9 the Saltpetre Walls; O-Nitre |
| H6 the Sand Falls hall | hall F3 | (-1200, -7850), -20 | 70 x 50, 60 | L8: three sand falls from the Sieves, dunes within (C6); F-S4 |
| W1 the Shoestring chasm | chasm F4 | (-900, -8250), -140 | 70 x 30, 70 | L5: the 120 m cable over a 60 m drop; the Cable Shrine at its west anchor |
| W2 the Silk Larders | hall F4 | (-700, -8050), -130 | 30 x 22, 22 | C10 |
| W3 the Root Ceiling | chasm F4 | (-400, -8200), -110 | 40 x 25, 60 | C11: hanging rhizomorphs over the drop |
| W4 the Titan Nursery | hall F4 | (-950, -8000), -160 | 36 x 26, 30 | C12 |
| W5 the Sealed Gate | hall F4 | (-820, -8380), -180 | 14 x 12, 14 | C13: the round web-sealed door |
| W6 the Deep Vein | hall F4 | (-500, -7900), -180 | 30 x 18, 18 | the best iron; cap-fall gallery |
| W7 the mole wall | crawl F4 | (-300, -8500), -150 | 6 m long | way 3 into the Heart Grotto |
| R1 the Drowned Granary | grotto F5 | (-500, -9050), -30 | 200 x 60, 40 | L7: nine silos in the lake |
| R2 Grotto Weir | grotto F5 | (-800, -9150), -20 | 30 x 18, 16 | C14 |
| R3 the Air Bell | hall F5 | (-300, -8950), -50 | 14 x 12, 9 | C15 foul air, the diving bell |
| R4 the Vat Room | hall F5 | (-350, -9150), -50 | 16 x 14, 10 | C16 |
| R5 the Steeping Vats | hall F5 | (-250, -9200), -50 | 35 x 14, 16 | the Rot King's arena (eight vats) |
| R6 the Toad Leaps | chasm F5 | (-600, -8850), -10 | 60 x 18, 30 | C17: five 10-12 m gaps over the river |
| R7 the Rimstone Pocket | grotto F5 | (-200, -9230), -55 | 16 x 14, 10 | the ore gallery behind a flooded crawl |
| R8 the Flue mouth | tube F5 | (-400, -8900) -> (-128, -8805), -38 | r 7 | the river passage into the Fall Field |
| Heart Grotto ring | dome rooms F4/HG | ring r 120..380 round (-600, -8600), -180..-220 | 10 rooms | 2.6 |
| HC the Heart chamber | dome HG | (-600, -8600), -220 | r 60, 70 | Mycelorn |

Tubes (each with a reason): D1's spiral path cut in G1's wall (a ramp in the doline wall); G1-G2, G2-G7, G7-G4 (the
Gardens' floor route), G1-G5 (west), G5-G6, G1-G8-G3 (north), D2 the Long Stair (a stair from the surface fissure at
(-1250, -8450) down past G2's level to H3 Knockers' Junction: 180 m), H3-H2, H2-H4, H3-H1, H1-H5, H1-H6 (the Sand
Falls), H3-W1 (the descent into the Webs: -40 -> -140 in a 300 m spiral tube), W1-W2, W1-W4, W1-W5, W2-W3, W3-W6, W3-W7
(crawl), G4-R2 via D4's foot (the Springcap Shaft continues down: Gardens -> Grottos), R2-R1, R1-R6, R6-R3, R3-R4, R4-R5,
R1-R7 (flooded crawl), R6-R8 (the river to the Flue), R1-moat (flooded crawl into the Heart Grotto: way 2), D6 the Great
Doline rim path (a spiral ledge down the Great Doline's wall to W1 at -80, then a tube to -140).

Every hall reachable on foot from the surface by at least one descent; the Heart Grotto by four ways; Mycelorn's chamber
from the Heart Wall door (and back out by the Heart Gate to the Fall Field: a one-way door). Every tube at least 7 m across
(the Shroomback and the Tunnel toad pass); crawls are only on optional routes.

Roof cover check (the generator, as the Crystal's): no cave roof within 6 m of the ground except where a doline, a
descent or a vent opens it.

### 2.3 Sub-zones and blends (fungal 2.2)

`WRFungal::ZoneAt(pos)`: the surface by land letter (F1 / Spine / Desert), underground by the nearest shape's zone (F2-F5,
HG). Each zone has its palette (fungal 2.2, exact hexes), its light colour (Gardens daylight gold, Halls cyan under white
stars, Webs black and amber, Grottos teal, Heart Grotto warm amber), its props, its creature table (2.12), its ambient bed
and reverb (2.15). Blends: the Gardens' doline walls fade from moss and fern (where daylight lands) to cyan glow-moss
down the 60 m descent (colour by height in the skin); the Sand Falls bring dunes into F3 (Desert border); the Flue's
spore crust (the Sky lane's shaft) meets the Fall Field's mycelium.

### 2.4 Light and the clock (fungal 2.1 read rules; 3.0 light)

- `UWRFungalWorld` sets `MPC_Fungal.GlowScale` every second from the surface clock: 0.35 at noon, 1.0 at midnight,
  eased (the circadian curve, tuning `fungalWorld.glow`). Pulse: 0 normally; the Spore bloom's 30 s tell and the Sporefall
  set it (a 1 Hz / 35 bpm sync beat).
- **Far lights:** each hall's `light` point in data gets one point light (colour by zone, 8-30 m radius by hall size),
  lit always; dolines get a daylight shaft (a warm directional-ish spot from the opening) by day and moonlight by night.
- **Under the rock** the sky light fades (`WRSky`: as the Crystal's CrystalCave fade), `WRBiome::IsDark` true underground
  (so Glowgrubs, lanterns and the dark-hunter rule matter), `IsUnderground` true in every shape (no rescue, no building).
- **The surface at night:** every doline rim's glow (a cyan point light at each doline's mouth, 0 by day) and foxfire on
  deadwood (night blocks).

### 2.5 Places (POIs, landmarks, sites, kit, decoration; fungal 2.3-2.4, 2.20)

All modelled in code at full scale on `FBuild` (`WRFungalScenery*.cpp`, kit header `WRFungalLook.h`: palette, the lathed
cap of any species (parasol, ink cap, oyster shelf, honey cluster, deepcap, springcap) at any size with gills, the
rhizomorph cable, the summited body (a Deepfarmer figure built from limbs in 4 poses: clamped on a beam, on a rack, on a
cable tip, on a rooftop; a white stalk from the skull; jaw on the root), the knocker idol, the cap-house, the glow-cap
ring, log beds, drying racks, granary towers, clay jars, the bronze bell, sickles, standing stones). Every place has its
job (the cook's tables) and its night light.

- **Camps C1-C3** on sinkhole rims: a fire ring, bedrolls, a lean-to of driftwood and sailcloth, a newcomer wreck in the
  cove below (hull ribs on the shingle), a rope-and-plank cliff stair down to the cove, a glow-cap ring (the waymark) at
  the rim. Stations at C2.
- **L1 the Ancestor Cap and the Breath**: a 90 m honey-gold stem (lathed, 9 m thick at the base flaring into a buttress
  of root flanges into the Great Doline's floor at -80) and a 60 m cap whose gills glow cyan; fissures round its foot; the
  Breath: a column of fx (`fungal.breath`) rising 400 m (800 m and gold in the Sporefall). From the Sky islands it is the
  biome's read.
- **L2 the Kneeling Titan**: a 35 m Deepfarmer titan of stone-hard fungal matter (built from lathed limbs: a cap head
  split by a 12 m Kitewright spear, gill-ribs open like a birdcage, one arm sunk in a doline to the shoulder), foxfire in
  its cracks at night.
- **L3 the Ring of Caps**: 64 white caps 6-12 m in a 200 m circle (glowing gills at night), seven carved standing stones
  and an altar inside; Tunnel toads sit in the ring at night (a den there with the night table).
- **L4 the False Sky**: H1's ceiling: ~6,000 lure-thread cards (thin glowing beaded strands 2-12 m long) in one
  M_FungalGlow mesh, a milky way along a crack; the dark star patch (F-S3) with a shelf-fungus stair to it.
- **L5 the Shoestring Bridge**: one rhizomorph cable 3 m thick sagging 120 m across W1 (a walkable spline with box
  collision along its top), 24 hangers fanning to wall anchors, silk wrapped on it.
- **L6 the Fall Field**: the Sky lane's floor dressed: bone scatter (sky beasts, kites, people) furred white, the
  Kitewright champion (9 m winged skeleton in its kite rig, wrapped in cords, a bronze sickle in the ribs: climbable
  chest with box collision), the three Breath vents (2.8), the Flue Floor Shrine in a ring of kite spars.
- **L7 the Drowned Granary**: nine round silo towers 14-22 m tall of shelf fungus standing in R1's lake, rope bridges
  between their tops, Glowfin light in the water.
- **L8 the Sand Falls**: three 80 m falls of sand (fx ribbons from the Sieves' holes) and dunes inside H6.
- **Descents D1-D6** as 2.2 (each a POI with its glow-cap ring at the top).
- **Surface hand sites S1-S8** and **cavern hand sites C1-C18**: exactly the cook's tables (what is there, what it pays),
  each a scenery piece + its caches (`AWRWorldCache`, server-owned, once per player) + its resource nodes + its lore
  item. The summited are scenery (never creatures: no zombies, DD).
- **Kit POIs (53)**: the generator places them: 4 + 10 glow-cap rings on the Deepfarmer routes every 150 m, host
  clusters, puffball fields, ore pockets, summited niches, mite nests, web pockets, glowgrub colonies, capling meadows,
  shelf deadwood, doline lookouts, at the counts of fungal 2.4; along every cavern tube a kit POI wherever the route is
  more than 125 m from any POI (alcoves cut off the tube, the Crystal's rule).
- **Ore galleries** (2.6): their nodes in their halls at the cook's ores and counts.
- **Lore items (9)**: Terrace tally, Knocker idol (talisman), Spore print jar, Titan mould sherd, Diver's bell, Kite
  harness token, Champion's kite-token, Elder's bowl, Heart-seed husk (`category: lore`, short Souls-style text).
- **Secrets F-S1..F-S5**: the ring's door (midnight + a toad tame on the altar: the stones light in turn and the altar
  slides to a stair: a spot actor with a server-timed slide), the fifth tunnel (a mole tame knocks; pick 6 hits opens
  the wall), the dark star (a ledge cache), behind the Sand Falls (the noon light reveals a ledge path: a path hidden by
  the fall fx except 11:30-12:30), the Seed Vault (Sporefall only).

### 2.6 The Heart Grotto (fungal 2.5)

Ten rooms round the Heart chamber on a ring r 120..380 m: the Lantern Street (cap-houses with amber gill windows,
dozens of summited on roofs, 2 Sporestalkers on the rooftops), the Market of Hands (stalls, tools with deepcaps), the
Feeding Hall (mounded graves, deepcap x6), the Elders' Tower (a 40 m shelf tower, the ring of seven, the bronze bowl),
the Last House (barred from inside; the one who refused), the Koi Pool (the Mycelial Koi fishing spot), the Heart Wall
(the living membrane door to the chamber, pulsing at 35 bpm; the run-back stone 50 m out), the Seed Vault (opens only in
the Sporefall), the Mite Warren (nests), the Heart Gate (one-way door to the Fall Field). Lit like a city at dusk (warm
amber point lights at the street corners, <= 24 in view). Four ways in: the Sealed Gate (burn 3 s or any wind hit tears
the web sheet: a spot actor with server state), the flooded crawl from the Drowned Granary, the mole wall (a tame
Truffle mole knocks then digs 20 s: the crawl's rubble plug opens), and the Heart Gate from inside.

### 2.7 Resources, crops, gathering (fungal 2.7)

- **Deepcap** nodes (`AWRResourceNode`, look `deepcap`: a 30 cm translucent blue-white cap glowing cyan on a white
  stipe, on a body) only where something died: host clusters, the Offering Niches (C1), the Feeding Hall, the Fall Field,
  the Breath Vents' bones; 1 per node by hand, 2 with a knife; respawn 3 game days. The Sporefall adds surface deepcaps.
- Others: puffball sac (wild Puffballs; nodes at puffball fields), glow-moss (F3 floors: hand, 2), amadou (shelf
  deadwood: hand), bootlace rhizomorph (F4 cables, C11: axe 4 hits), webspinner silk (larders: hand), mite chitin
  (Mycomite butchering), saltpetre (C9, the Nitre Gallery: pick), stalker stalks (Sporestalker butchering), ores (copper
  / tin / iron / gems by gallery), oyster cap and lion's mane wild (F2, C4: hand, seeds).
- Crops: oyster cap and lion's mane already in farming.json as dark crops; the log bed piece; Capling flesh regrowth and
  the Puffball's 20-minute sac (creature farm rows).

### 2.8 Hazards and their machines (fungal 2.8)

**Spore clouds** (`AWRSporeCloud`, server-authoritative, replicated: centre, radius, owner kind (world / player /
boss), state (live, flashing, settling, gone), push offset (eased over 0.8 s, never a snap)). World clouds come from
data (`clouds` in fungal.json: puffball fields, host clusters, the Breath Vents' out-breath, every third ledge of D6) and
respawn after their gone time. Inside: 12 poison buildup a second (after a 0.9 s grace in which motes gather on you),
rot 12/s after 10 s continuous (a break is 3 s outside), both through the status component's buildup (Immune system,
filter mask, Sporeweave 4, spore wad x0.3, every fungal creature exempt). The look: fx rows `fungal.cloud` (slow-turning
cyan-green haze with motes, sized by radius, 12 m sight inside), `fungal.cloudEdge` (the hiss and glitter). **Push:** any
`FWRHit` with element wind (and the Gale flask, Draught gates, the Flue's downdraft) pushes a cloud along the hit's
direction 6 m (15 m for gusts flagged strong). **Flash:** fire (a fire hit, a burn proc on someone inside, a torch
swing, a burning creature) inside a cloud: a 0.9 s rising whoomp and glow (`fungal.flashTell`), then the flash (40 +
burn 40 through `ApplyHit` to everyone inside, element fire), the cloud gone 2 game hours. **Settle:** frost damage in it:
spore snow 3 s (`fungal.sporeSnow`), gone 1 game hour. A wind-pushed cloud remembers it was pushed for 2 s (Mycelorn's
choke reads it).

**The Breath vents** (`AWRBreathVent` x3 at the Fall Field, 40 m apart): three bellows-stones (a carved stone collar
1.5 m high round a fissure, a hinged bronze flap lid on a pivot pin that lifts on the exhale and drops on the inhale,
mycelium over the stone). One exhales every 40 s (30 in the Sporefall) for 6 s with a 1.0 s tell (the stone's glow
brightens, a deep huff); its plume is a tall cloud column 6 m wide up the shaft (counts as a spore cloud). The phase is
the same clock the Sky lane's `AWRSkyRing::PlumeNow` uses (server time, `plumeEvery`), so the plume you see leave the
floor is the one that reaches Aeolith's ring.

**Lure lines** (`AWRLureLines`, one actor per hall set; lines from data: top point, length, density): drawn as glowing
beaded threads (M_FungalGlow), hum in moving air. Touch (a body capsule or a swing inside 40 cm of a line at 1.5-2.5 m
above the floor; crouching passes under) -> twang + brighten 0.9 s -> stuck 2 s (no move / roll; attack and block OK) ->
reeled 3 m up over 1.5 s (eased) -> dropped. A light swing within 1.5 m cuts lines; fire burns 3 m; wind tears 8 m (each
removal replicated as a cut mask; lines regrow after 1 game hour). The nearest Webspinner within 30 m is told.

**Springcaps** (`AWRSpringcap`): a bulging glossy cap 2.4 m across on a short thick stipe. Landing on it starts a 0.6 s
creak (the cap compresses 30%); jumping in that window launches you 9 m up (launch velocity from tuning), aimable 20
degrees by your input; landing from over 6 m fires it anyway; any attack on it fires it (5 m sideways throw). The cap
springs back to shape over 0.4 s, regrows its charge in 20 s (shown deflated and dull until then).

**Cap-falls** (`AWRCapFall` ceilings in F4 from data): loud work within 15 m (mining, hits, explosions: the server hit
hook) sets a ceiling knocking (two knocks, three knocks: audio rows), grit trickles and a dust shaft shows the 3 m circle
for 1.0 s, then the block falls (a rot-softened slab, simulated as a kinematic fall over 0.45 s, then it lies broken):
70 + knockdown through `ApplyHit`; the ceiling regrows 2 game hours. A tame Truffle mole knocks back 10 s early (marks the
circle with dust); the Knocker idol talisman makes the knocking 1 s earlier.

**Foul air** volumes (data: R3, R7, the moat crawl): your torch flame shrinks and turns blue (the held torch's light
colour and scale), lanterns dim 30%, heavy breathing; stamina regen x0.5, no sprint; after 20 s 3 damage a second; no
fire can be lit (torches won't light, fire spells fizzle). Filter mask does not help.

**Draught gates, egg sacs, rot pools, web sheets, mound pods** belong to their bosses (2.13, 2.14).

### 2.9 Weather (fungal 2.9)

`weather.json` biome `fungal`: Clear 30, Overcast 20, Rain 20, **Spore drift** 30 (some rows exist: reuse). Spore drift
(surface only): every doline's mouth breathes a thin cloud (`AWRSporeCloud` with the surface flag: 20 m sight) that
drifts 200 m downwind on the world wind and fades; rain stops clouds forming outdoors (the surface world clouds are
suspended in Rain). Underground: the **Spore bloom**: every 20 min for 3 min, every world cloud underground grows 1.5x;
30 s tell: `MPC_Fungal.Pulse` beats 1 Hz and the cave breathing bed deepens (audio layer); Mycomites see warm light from
2x range during it. The Heart Grotto never blooms.

### 2.10 The Sporefall and the invasions (fungal 2.10, 2.21)

**The Sporefall** (`world.json events.sporefall`, `WRFungalEvents.cpp` on the world events frame): warning 5 min (the
Breath turns gold and climbs to 800 m; the glow beats in sync 35 bpm; every Puffball slips its tether and drifts up; the
Deepfarmer pillar on the Crown's Ring burns cyan (the generic event pillar row)). Then one game day: flush patches burst
up across the seeded ground (the fungal surface, the Desert strip's north 400 m, and the three sky islands Spore Scar,
Wheel Isle, the Lid Gate) one per 40 m grid cell near players, each a cluster of pale caps (scenery) with a 6 m cloud;
surface deepcaps on patches that have bones (x3 pay); the Flue's vents every 30 s; creatures that spend 10 s in a flush
cloud **bloom** (a replicated flag: a pale fruiting cap on the back, bites +poison 20, burst into a 4 m cloud on death;
fungal creatures exempt); at 12:00 every bloomed passive creature climbs to the highest point within 30 m and clamps
there (a scripted walk and a hold), dies at dusk, and at dawn a deepcap node stands on its summit; Sporestalkers hunt the
surface by day; Mycomite swarms x2; the Weaver webs the Long Stair; the Seed Vault opens; spore sacs double; Mycelorn's
clouds last 50% longer and its drop gains +1 spore essence.

**Invasions** (`world.json invasions.pairs`): sky_islands -> fungal exists (leader kyrra, mouths at the Flue). Add
fungal -> sky_islands (leader the Weaver, day: Mycomite x12 in two swarms, Sporestalker x2, Webspinner x2, Puffball x4;
night: Sporestalker x4, Webspinner x3, Mycomite x8; pays spore and gale essence). The Spore side's marks: each invader
that stands 4 s leaves a 3 m cloud (2 game hours); Wind invaders in the caverns push clouds within 30 m 6 m every 8 s.
The Flue's continuous cyan column and the mycelium bridges are fx and scenery rows on the sign.

### 2.11 Shrines, master, quests, portal slots (fungal 2.11)

- Five shrines in `world.json` (land fungal, `at` + `floor` for the cavern ones): the Great Shrine of the Garden Doline
  (great, no guard; a ring of 7 caps round a stone basin where a deepcap grows in water; the brazier a bowl of glow-moss
  oil burning cold cyan), the Ring Shrine (L3 altar; tunnel_toad x3), the Starfall Shrine (H1 edge; webspinner x2), the
  Cable Shrine (W1 west anchor; sporestalker x1, mycomite x6), the Flue Floor Shrine (L6; rotmaw x1, mycomite x4).
- **Ilmari Kask** (`masters`): path spore, at the Great Shrine's terraces by day, his cap-house by night (the swamp
  master's day/night places), sex m, the cook's look carried by the hero-look rows (vest, bare chest, rolled trousers,
  bare feet, a spore mask at his neck, cyan freckles), work "gather" on the log beds. Lines (short, warm, never
  explaining). His six quests: First flush (plant and harvest an oyster cap log bed: type `harvest`), What grows on the
  dead (bring deepcap x3), Cold light (tame a Glowgrub or craft a glow-cap lantern, then walk the Lure Gallery end to end
  without being reeled: an event quest), The lone cap (kill a Rotmaw before it bursts: event `rotmaw_lure_first`), Down
  the Long Stair (light the Starfall and Cable shrines), Cut the starfield (beat the Weaver: kill quest). Rewards as the
  cook (XP into the Spore path; recipes; the Farmer's ring).
- **Portal slots**: `portals.json` already lists fungal rows (check and make them live with the biome): a portal frame
  at the Great Shrine's terrace and at the Fall Field shrine.
- **Summoning**: effigies for the four bosses (`world.json summoning.effigies` + recipes), stones at the lairs / the
  Heart Wall's run-back stone.

### 2.12 Creatures (fungal 2.12)

Rigs (real skeletal meshes and real clips from the packs, the cook's anatomy carried by code-built look parts on the
rig's bones, `UWRFungalBody`, palette over the texture with M_CreatureTint):

| Creature | Rig (scale) | Look parts built in code |
|---|---|---|
| Sporestalker | Rimeclaw (1.25, long and low) | olm-pale head skin tint, crest of 14 cordyceps stalks along the spine (bone-white, cyan tips; they lift on aggro), orange gill ruff under the jaw (flares on Gill bite), hyphae grey on the umber pelt |
| Rotmaw | Rimeshell (2.6, widened 1.3) | a mound of ivory and tan caps over the shell (shiver on the tell), the lure: a translucent cyan deepcap on a tongue stalk in M_FungalVeil, rust gums |
| Mycomite | Kilnmite (0.5) | a pale cap on the back, cyan veins on the abdomen when fed |
| Webspinner | Kilnmite (1.9) | long banded legs (code legs over the rig, the Spindleglass's planted-foot legs reused as a pattern: 8 legs, 4 m span), black abdomen with glowing cyan dots, two silver eye-lenses |
| Tunnel toad | Magmaw (1.1) | pink-white skin tint, buried eyes, huge eardrums, a translucent throat sac that swells on Croak blast |
| Shroomback | Basaltback (1.0) | the domed shell's oyster-cap garden (fan caps that regrow: the yield), a spiked club tail, pangolin scales on the head |
| Truffle mole | Pumicehog (1.0) | velvet black-brown tint, a pink 22-tentacle star nose (wiggling), spade claws |
| Glowgrub | Rimeshell (0.45) | a soft segmented pale body hood and the glowing tail lantern (light 12 m, cold) |
| Capling | Snowpuff (1.0) | a rust cap with cream spots on its back (gone when harvested, regrows) |
| Puffball | procBody `puffball` | a 1.2 m pale sac in M_FungalVeil with 8 trailing cyan-tipped tendrils (sim'd by a spring chain), buoyant bob |
| Sporeling | Kilnmite (0.7) | a squat cap-headed fungal biter |

Behaviour (`FWRFungalAI`): cloud and foul-air immunity; herd defence; dark-hunters (Rotmaw, Webspinner, Sporestalker:
aggro x2 on unlit players); warm light draws Mycomites from 2x; **ceiling hunters** (Sporestalker, Webspinner, the
Weaver) cling to hall ceilings when idle (a "ceiling" state: the body hangs from a ceiling point found by a trace up,
drawn upside down, moving along it) and drop (`ceiling_drop` / `ceiling_pounce`: a visible fall to the target point, the
tell above); the **Rotmaw** hides as a mound (only its caps and the lure show) and opens with Mound burst, or Lure snap
when someone reaches for the lure (the lure uses the deepcap's E prompt: picking it is the grab); hitting the lure from
range forces it out stunned 2 s; **Mycomite latch** (up to 4 per player, each visible on a limb socket, drain, x0.9 move
each, off by roll 50%, fire, an ally's hit, its death; burst on death); **Web shot** (a net projectile: webbed 2 s,
mash to shorten); **Tunnel toad** (tongue pull, leap slam to a point up to 12 m with a `low` ring, croak blast snapping
lure lines); **Shroomback** (club tail, withdraw); **Truffle mole** (burrow pop; tame: senses ore / deepcap / hidden
walls in 15 m and knocks toward them, digs the mole wall, warns cap-falls); **Glowgrub** (flash and flee; tame: light);
**Capling** (puff and hop); **Puffball** (drifts toward warm air; bursts into a cloud when killed or burned). Attack rows
in `attacks.json` exactly as fungal 2.12.2; physical resists and lock points as the cook. Day / night tables per
sub-zone from `fungal.json dens` (the Crystal's StepDens pattern). Living-world beats: Rotmaws lure Caplings, Webspinners
reel Glowgrubs up, Mycomites go to carcasses and fire, Puffballs drift up by day and settle at night, Caplings ride
Shroombacks.

Taming and utilities: Tunnel toad mount (ride 1,250 cm/s, held jump charges a 12 m leap with a landing slam ring),
Shroomback hauler (carry 220, an oyster cap every 10 min, buildup x0.5 within 3 m), Truffle mole utility, Glowgrub light
(12 m, cold), Capling / Puffball farm yields; Sporestalker path magic (a Spore walker with 10+ points holds E 4 s with
10 Spores on a knocked-out or rotting Sporestalker under 30%: a full narcotic dose).

### 2.13 Mini bosses (fungal 2.13; 3,000 HP solo, stance 90, 2 parries, run-back stones)

- **The Sporemother** (Brood Doline, G3): Kilnmite rig x6 (6 m), translucent glowing abdomen with shifting egg sacs (inner
  meshes on a slow wobble), a crown of pale caps. Six wall egg sacs (`AWRFungalBossProp` kind `sac`: 150 HP, fire x3,
  any fire hit kills; hatch 3 Mycomites every 25 s; a burnt sac makes her shriek and stagger 2 s within 20 m; all six
  burnt: egg toss throws duds). Rows: hypostome jab, leg stamp x3 (third `low`), egg toss (2 sacs, ballistic, shadow the
  whole flight, hatch 1.5 s after landing), brood press (grab, crimson glint, 4 mites latch, ally hits 150 on the abdomen
  free you), spore vent (8 m ring cloud 6 s; fire on it flashes it on her: 200 + 3 s stagger).
- **The Rot King** (Steeping Vats, R5): Rimeshell rig x6 (9 m) with a crown of black rotting caps 3 m high dripping rust
  slurry (fx), white chipped beak, bronze hooks in his shell. Eight vats (four full of slurry); rot pools as server
  volumes with a decal-like flat mesh (heal him 1.5%/s, rot 15/s on players, no slow); frost freezes a pool 15 s (ice
  look, walkable, inert); a frostbite proc stops his heal 12 s; a wind hit dries his crown 10 s (no new pools); the clean
  channels at the edge (water) give no heal. Rows: beak snap x2 + crown slam (pool), rot spread (an arc of pool), rot
  belch (cone), engulf (grab), shell spin (spreads pools).
- **The Weaver** (roaming): Kilnmite rig x4 (9 m legs), constellation abdomen; route (Starfall Hall lair -> Sand Falls ->
  Lure Gallery -> Knockers' Junction -> Shoestring Bridge -> Silk Larders -> back) walked on hall ceilings and tube
  roofs at 2 m/s, 4 min rest at the lair; nocturnal (sleeps hanging by day: a sneak crit opens). Where she fights she
  webs the hall: a web sheet (an `AWRFungalBossProp` kind `web`: a flat translucent sheet between two wall points) every
  10 s up to 6, plus lure lines; on web (within 4 m of a sheet or a line field) x1.6 speed and Starfall drop available;
  off web x0.6. Fire burns 4 m, wind tears 8 m (Gale flask 12 m), a Tunnel toad's croak snaps lines in 8 m. Rows: fang
  lunge, leg sweep x3, web volley (3 nets), Starfall drop (from the ceiling to a point; single thread tell), cocoon (grab:
  wrapped 3 s, hauled 6 m up and dropped; an ally's 100 on the cocoon or fire frees).
- Trophies (Mother's cap, Rot crown, Weaver silk: built rows), upgrade shards x4, the cook's crafting extras (Queen's
  jelly, King's slurry, Star-thread) and loot.

### 2.14 Mycelorn, the Hivemind (fungal 2.14; 12,000 HP solo, stance 175, 3 parries)

- **Body** (`procBody` `mycelorn`, `UWRFungalBeast`: built in code, no rig has it): a 6 m translucent amber stipe (M_FungalVeil
  over an inner titan ribcage and the spiral of summited Deepfarmer bodies as inner meshes), a 36 m umber cap with a cyan
  gill underside (its own glow scalar: brightens white along the breathing side), a torn veil ring at 8 m, the pulsing
  Heart-seed (3 m amber sphere at the stem base, scale-beats at 35 bpm), and a curtain of tendrils: **12 heavy clubbed
  tendrils** as 8-segment chains driven by attack curves (lift, swing back, strike along a curve; the club's tip is the
  hit volume: damage only when the club passes through you), and **88 light cords** as spring chains that trail them.
  The body rears / sinks / bows by a 12-step stem bend.
- **The arena** (HC): a 120 m dome, the Heart pool (40 m, ankle-deep sap, no slow), six stone terraces 1.5 m high round the
  edge with summited elders at their backs, four **Draught gates** (N S E W: stone doors 6 m wide with a fungal plug,
  600 HP, x2 from fire and blunt; broken: a 12 m-wide gale lane for 10 s pushing clouds 15 m; the plug regrows over 45 s:
  a swelling mesh, never a snap), the Spore altar (south terrace), the run-back stone 50 m out in the Heart Grotto.
- **The Wind counter:** any of its own clouds whose last push was wind (within 2 s) overlapping the stipe or the cap's
  underside chokes it: gills go dark, 4 s stagger, stance taken x2, wind damage x1.5, the cloud spent.
- **Phase 1** (rooted, yaw only): Club slam, Lash x3 (low / roll / sidestep), Gill breath (cone that leaves a cloud),
  Spore mortar (3 pods, ballistic, shadows), Sporeling wave (8), Engulf (grab, crimson glint), Summit call (a stalk at
  the furthest tame: the tame walks to a terrace wall and clamps 8 s; 100 on the stalk frees it).
- **The Sinking Root** at 50% (1.4 s tell, unblockable ring wave, damage taken x0.5 during it): the pool drains, the floor
  veins brighten, 12 mound pods sprout.
- **Phase 2:** it travels under the mat along a vein graph (data: nodes and edges over the floor); the vein race
  (shortest path, Dijkstra, drawn as a light travelling along the vein meshes) then a fairy ring of white caps springs
  up 1.2 s before it erupts; surfaced 8-14 s; new moves Ring eruption, Root lash line (unblockable, a line of 8 tendrils
  along one vein), Erupt-spin-slam, delayed Gill breath; mound pods exhale 6 m clouds every 20 s near players (burnable,
  200); clouds pushed onto the ring as it rises choke it 5 s.
- **Hidden phase 3** (25%, artifacts): the Whole Cavern: every 8 s one of lure lines over a third of the floor, wall
  exhales, cap-falls at three players, terrace elders puff; stays surfaced at the centre, the Heart-seed exposed (x1.25);
  Lash x4; damage x1.4.
- Drops: core_mycelorn, spore_essence x5, the Hive spore boss part (chance), trophy (Heart-seed husk), blueprints Stipe
  Maul and the Deepfarmer decor pack. Altar: `AWRBossAltar` path spore.

### 2.15 Sound and music (fungal 2.15-2.16)

`sounds.json` rows: per creature tell / strike / hit / pain / idle / aggro / death from the nearest existing recordings
(the fused recipes are the target; until those files exist the rows point at the nearest recordings and are marked
`_standin`, as the Crystal lane did), the hazards (cloud hiss, flash whoomp, spore snow, line twang and hum, springcap
creak and pop, knocking, cap-fall, foul-air breath, vent huff), the machines (vent flap, Draught gate gale, plug regrow,
sac burst, web tear). Beds per sub-zone and the Heart Grotto; reverb by zone. Music pools `explore.fungal.*`,
`night.fungal.surface`, `combat.fungal`, `mini.fungal`, `boss.mycelorn` p1-p3, `sub.heart_grotto`, `sub.fall_field` with
the 20 s planned silence (`WRAudioDirector` fades the music on stepping onto the Fall Field), `event.sporefall`,
`camp.fungal`: from existing composed tracks, marked `_waitsFor: music` (no Fungal cues exist; no AI music, KEPT).

### 2.16 The Spore path (fungal 2.17-2.18)

- `Data/paths/spore.json` (the Arcane file's schema): trunk spore_cloud 3, spore_capacity x3, rot_chance x2,
  infect_strike 2; Rot: decay x5, corrode 4, blight 5, lingering x3, burst 3, pandemic 10; Colony: sporeling 5, swarm x5,
  host 4, hive_mind 3, thick_spores x3, fungal_behemoth 10; Symbiote: spore_armor 5, regrowth_spores x5, toxic_blood 3,
  adrenal 4, bloom_body x3, living_colony 10; root passive Immune system; gates 0/5/10/14/17/20.
- **Spores** as attributes (`Spores`, `MaxSpores` on `UWRAttributeSet`, the Mana pattern): 100 + 20 a rank, 5/s, x2 in
  any spore cloud, +3 per detonated Spore; costs from `spells.<key>.sporeCost`; a refused press greys and shakes the slot.
- **Planted Spores** (`UWRSporeHost`, a replicated component added to a target on its first plant): per planter a count
  (cap 10 + capacity ranks; 5 on players) and an expiry (20 s, refreshed); nodules drawn on the body (cyan beads on
  random bone sockets, one per Spore). **Detonate** (free with Spore cloud, its own quick-slot, 0.30 s cast while walking):
  every Spore of yours within 20 m bursts (8 + rot 10 each through ApplyHit, a chain 0.05 s apart).
- Abilities (`WRAbilitiesSpore`): `UWRGA_SporeCloud`, `UWRGA_Detonate`, `UWRGA_Blight`, `UWRGA_InfectStrike`,
  `UWRGA_Sporeling`, `UWRGA_FungalBehemoth`, `UWRGA_SporeArmor`, `UWRGA_LivingColony`, registered in `ClassForNode`.
  Spore cloud / Blight / Living colony's aura are real `AWRSporeCloud`s owned by the caster (wind pushes, fire flashes
  them; harmless to their owner and party).
- Passives through `WRSpore::` hooks in `ApplyHit` (Corrode, Burst, Pandemic tick, Host, Hive mind, Toxic blood, Adrenal,
  Regrowth, Bloom body, Rot chance, Decay, Lingering), Immune system (main path: poison and rot buildup ignored).
- Combos (`combos.json`) for the paths that exist: Wind + Spore (Spore drift / Plague wind), Fire (Combustion / Ash
  colony), Lightning (Static cloud / Neural spores), Nature (Mycorrhiza / Forest floor), Frost (Hardy colony / Ice
  bloom), Blood (Infected wounds / Blood colony), Necromancy (Fungal puppets / Endless colony), Arcane (Fungal mind /
  Glowcap colony), Void (Hungry spores / Entropy); Sun and Tide shown only until those paths exist.
- HUD: the Spores bar (the Mana row's pattern) and the planted count on the locked target.
- Body change: `player_art.json paths.spore` rows (freckles by stage, iris fleck, moss hair tips, shoulder motes, the
  shimmer at 100) and a cast pulse; the hero-look rows exist (93 references): wire the stage strengths.

### 2.17 Sets and gear (fungal 2.19)

Sporeweave (built rows: check its 2- and 4-piece stats do the cook's buildup x0.5 and cloud immunity, wire them in the
cloud and status code), the filter mask, spore wad, glow-cap lantern (a cold light: the light item kind with `cold`), glow
paste, spore pot (built: wire it to `AWRSporeCloud`), Gale flask (levistone from the Sky), amadou tinder, rhizomorph
rope, chitin arrows, the knocker idol, the Farmer's ring, the Deepfarmer ring, the trophies' talismans' stats (spore and
rot buildup x0.7; hits on rotting +10%; webs hold 50% shorter). The Mycelium den building set (built rows) unhidden with
the biome.

---

## 3. Build steps (in order; each compiled before the next; a commit + push after each group)

1. `Tools/terrain/fungal.mjs` (land, caves, places, kit, dens, clouds, lines, springcaps, cap-falls, vents) -> data.
2. `build_fungal_materials.py` (MPC_Fungal, M_FungalGlow, M_FungalVeil) run headless.
3. `WRFungalTypes` (data + Air), `WRFungalCaves`, `WRFungalWorld`, `WRFungalLook.h`, `WRFungalScenery*.cpp`; the Sky
   Flue's mouth; IsUnderground / IsDark / the sky fade; liveBiomes; the crossing and camps. Commit 1.
4. Items, recipes, nodes and their looks, world rows (shrines, Ilmari, quests, portals, summoning), weather. Commit 2.
5. `WRFungalHazards` (clouds and answers, lines, springcaps, cap-falls, foul air, vents, bloom), `WRFungalEvents`
   (Sporefall), invasions. Commit 3.
6. `WRFungalCreatures` (body + AI), creature and attack rows. Commit 4.
7. `WRFungalBosses` + `WRFungalBeast` (Mycelorn's body), rows. Commit 5.
8. `WRSporePath`, `WRAbilitiesSpore`, spore.json, tuning, combos, HUD, look. Commit 6.
9. Sounds / music rows, spot-check, proof, CHANGELOG / HANDOFF, LANDED, LATER-COOKS. Commit 7.

## 4. Hidden until finished (no placeholders)

Anything not built by the end keeps its row out of the game (data `live: false` or not placed) and is listed in section
5 and the LANDED line.

## 5. Spot-check and proof

Landed in seven commits on Survival-Game `master` (each compiled clean with the editor target before its push; no tests
run, per the brief): `70812d1` Fungal (1) the land, the caverns and their places; `0862a90` Fungal (2) the caverns'
things; `ec2b023` Fungal (3) the hazards and the Sporefall; `5f4ee38` Fungal (4) the ten creatures and the Sporelings;
`7b01131` Fungal (5) the bosses; `a24929d` Fungal (6) the Spore path; `e7611b9` Fungal (7) the caverns' sound.
Spot-check: each part re-read before its commit (the code paths a player hits first: the mound and its lure, the latch
cap, the ceiling drop, the choke window, the draught lane, the plant cap, the free Detonate slot, the summons kept out of
tames, saves and boss HP; the include order and the unity-blob names were fixed where the compiler or the adaptive
build showed them). Not played here (Aaron tests).

| Row | Done | Where |
|---|---|---|
| R1 | yes | research 1, plan 2, build 3, this section; seven parts, each compiled |
| R2 | yes | section 1 (research table: real games, places and systems, the real-world roots) |
| R3 | yes | section 2.0-2.17 (layout, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases) |
| R4 | yes | section 3 |
| R5 | yes | section 2.3-2.7; the scenery kit `WRFungalLook.h`, `WRFungalScenery.cpp`, `WRFungalSceneryCaves.cpp` (70812d1) |
| R6 | yes | the bellows vents `AWRBreathVent`, springcaps, the Draught gates' plugs (`AWRFungalBossProp` plug), the doors `AWRFungalSpot` (all eased on the server clock, WRFungalHazards.cpp, WRFungalSpots.cpp, WRFungalBosses.cpp) |
| R7 | yes | this table |
| R8 | yes | `Tools/terrain/fungal.mjs` -> `Data/lands/fungal.json` + `fungal_ground.bin`, the Sky land's cuts (70812d1) |
| R9 | yes | `Data/fungal.json` halls/tubes/shafts on three levels, `AWRFungalCaves` surface nets (70812d1) |
| R10 | yes | zones F1-F5 + HG in `fungal.mjs`, `WRFungalSceneryCaves.cpp` (halls by zone), beds by zone `UWRAudioDirector` (e7611b9) |
| R11 | yes | `fungal.mjs` blends (dolines, the Sand Falls, the Flue mouth R9, `WRSkyScenery2.cpp` skipped panel) (70812d1) |
| R12 | yes | `UWRFungalWorld::StepGlow` (surface clock, 0.35 noon / 1.0 midnight), far lights `StepLights`, `M_FungalGlow` (70812d1) |
| R13 | yes | L1-L8 in `fungal.mjs` sites, built in `WRFungalScenery*.cpp` (70812d1) |
| R14 | yes | 120 POIs (`fungal.mjs` -> `island_dressing.json` pois), the 125 m kit rule (70812d1) |
| R15 | yes | descents D1-D6 in `fungal.mjs` / `Data/fungal.json` descents (70812d1) |
| R16 | yes | S1-S8 (70812d1) |
| R17 | yes | C1-C18 (70812d1) |
| R18 | yes | kit + alcoves in `fungal.mjs`, `WRFungalSceneryCaves.cpp` CaveKit (70812d1) |
| R19 | yes | camps on the sinkhole rims with cove wrecks (`fungal.mjs` camps, land `camps`) (70812d1) |
| R20 | yes | the Heart Grotto ring city HC + HG1-HG10, RQ, the moat, the mole wall `AWRFungalSpot` mole_wall, the Seed Vault (70812d1, ec2b023) |
| R21 | yes | the galleries' nodes (`WRFungalThings.cpp`, `fungalWorld.nodes`) (0862a90) |
| R22 | yes | deepcap only on the dead, x2 with a blade, 3-day respawn (`WRFungalThings.cpp`); the mole senses it (`FWRFungalAI::TickTame`, 5f4ee38) |
| R23 | yes | `WRFungalThings.cpp`, `WRResourceLook.cpp` (`WRFungal::NodeLook`), items.json (0862a90) |
| R24 | yes | oyster cap / lion's mane farm rows and wild beds (0862a90); Capling / Puffball yields `farming.json`; the Shroomback's oyster cap (5f4ee38) |
| R25 | yes | `AWRSporeCloud` (WRFungalHazards.cpp: grace, poison, rot after 10 s, wind push, fire flash, frost settle) (ec2b023) |
| R26 | yes | `AWRBreathVent` on `WRSkyBosses.cpp`'s plume clock (`WRFungal::VentEvery`) (ec2b023) |
| R27 | yes | `AWRLureLines`, `AWRSpringcap`, `AWRCapFall`, foul air (`StepHazards`) (ec2b023) |
| R28 | yes | `Data/weather.json` fungal row; the bloom `WRFungal::BloomOn` / `BloomTell` (0862a90, ec2b023) |
| R29 | yes | `WRFungalEvents.cpp` StepSporefall / StepFlush, `world.json events.sporefall` (0862a90, ec2b023) |
| R30 | yes | five shrines in `world.json` with guards (0862a90) |
| R31 | yes | `world.json` master ilmari + `quests.ilmari` six quests; `StepIlmari`; the lone cap's credit `FWRFungalAI::Died` (0862a90, 5f4ee38) |
| R32 | yes | `Data/portals.json` fungal row (slots 1, 3, high: kept as it stood) |
| R33 | yes | ten rows + the Sporeling in `creatures.json` on real rigs with their clips; `WRFungalCreatures.*` looks and moves (5f4ee38) |
| R34 | yes | `WRFungal::SporeTaken` (fungal creatures 0), herds (archetype herd), tiers, swarm (25 HP), darkHunter rows, `FWRFungalAI::TickWild` warm light x2, cyan parts, crimson only on lure snap / brood press / engulf / cocoon (5f4ee38, 7b01131) |
| R35 | yes | `attacks.json` rows for all ten + the Sporeling; physResist and lockPoints in `creatures.json` (5f4ee38) |
| R36 | yes | `denTables` + dens in `Data/fungal.json`, `UWRFungalWorld::StepDens`; beats in `FWRFungalAI::TickWild` (5f4ee38) |
| R37 | yes | the toad's held-jump leap and landing slam (`UWRFungalBody::BeginPlay`, `TickBody`), Shroomback hauler + garden aura (`WRFungal::SporeTaken`), the mole (`TickTame`, `AWRFungalSpot`, `AWRCapFall::MoleWarn`), Glowgrub light (utility light), the Sporestalker's calm (`WRSporePath::PathTame*`) (5f4ee38, a24929d) |
| R38 | yes | `FWRFungalBoss` sporemother (sacs, egg toss, brood press, vent flash), `UWRFungalArenas::Place` (7b01131) |
| R39 | yes | rot_king pools (vats, slam, spread, spin), frost freeze, frostbite, wind dry (7b01131) |
| R40 | yes | weaver loop, day sleep, webbing, x1.6 / x0.6 (`FWRFungalBoss::MoveScale`), volley, starfall drop, cocoon, fire / wind tear (7b01131) |
| R41 | yes | mycelorn: phase 1 moves, choke, Draught gates, gills, Summit call, the Sinking Root, vein travel (`FbVeinPath`) and fairy rings, mound pods, the Whole Cavern, Heart-seed, Lash x4; drops + Hive spore (`WRFungal::CreatureDied`); runback, summoning stones, Spore altar (7b01131) |
| R42 | partly | voices are the nearest family pitched (`sounds.json` creatures, `_standin`); the fused 4-layer calls are the audio cook's (LATER-COOKS) (e7611b9) |
| R43 | partly | the cue map by zone, bosses' phases, the Sporefall layer and the Fall Field's planned silence are built (`UWRAudioDirector`); the tracks are stand-ins, the 70 BPM Bb Locrian sheet is the audio cook's (e7611b9) |
| R44 | yes | `Data/paths/spore.json` (22 nodes + Detonate), `WRSporePath.*`, `WRAbilitiesSpore.*`, Spores attribute and bars, `UWRSporeHost` nodules, PvP caps, proc fx / sounds (a24929d, e7611b9) |
| R45 | yes | `Data/combos.json` spore pairs (8 built, Arcane / Void shown only) and their tags in `WRSporePath.cpp` (a24929d) |
| R46 | yes | the existing freckle pattern (`player_art.json` paths.spore) + `WRSporePath::ApplyLook` (dark glow, 0.3 Hz wave, colony blaze) (a24929d) |
| R47 | yes | Sporeweave pieces in `items.json` / `recipes.json` (with its Ascendant tier) |
| R48 | yes | `building_sets.json` mycelium_den; Mycelorn's decor drop `bp_mycelium_den_decor` (7b01131) |
| R49 | yes | the items and recipes (0862a90); the Gale flask and spore pot models / clouds (ec2b023); trophies and talismans, Farmer's ring (`sporesRegen` read by `WRSporePath::TickOwner`) |
| R50 | yes | ten lore keepsakes, the five secrets and their pay (`WRFungalThings.cpp`, `AWRFungalSpot`) (0862a90, ec2b023) |
| R51 | yes | `world.json` invasions fungal -> sky_islands (led by the Weaver) and the Sky's back; `StepInvasionMarks` (0862a90, ec2b023) |
| R52 | yes | numbers in tuning (`fungalWorld`, `hazards.fungal`, `fungalEvents`, `fungalLife`, `bosses.*`, `spores`, `sporePath`, `spells.*`), abilities in ClassForNode, damage through `UWRCombatLibrary::ApplyHit`, fx / sounds as rows; no new screen was needed (prompts go through the existing E chain) |
| R53 | yes | server-side brains and timers; replicated state on `UWRFungalBody`, `UWRFungalBossBody`, `AWRFungalBossProp`, `UWRSporeHost`, the hazard actors |
| R54 | yes | nothing unfinished is shown as finished (stand-in sounds are marked `_standin`; Arcane / Void spore combos say "not built yet") |
| R55 | yes | lobs and nets never home; every opener >= 0.9 s with its tell; crimson only on grabs; no attacker cap added |
| R56 | yes | each part's editor build clean of new errors and warnings |
| R57 | yes | CHANGELOG Unreleased + HANDOFF in every commit; this doc; LANDED and LATER-COOKS lines |
| R58 | yes | Fk / Fb / Fl / Sp / Sa prefixes; the look file's colours renamed for the unity blobs |
| R59 | yes | the plume clock shared (`WRSkyBosses.cpp`), the spore pot cloud for Aeolith (ec2b023), the Spore Scar seeded in the Sporefall (ec2b023) |
| R60 | yes | binaries below |

**New binaries (this lane):** `Content/Terrain/M_FungalGlow.uasset` 17.5 KB, `M_FungalVeil.uasset` 12.2 KB (LFS);
`Data/lands/fungal_ground.bin` 0.97 MB (plain git); 47 path glyph PNGs (`Data/UI/Glyphs/path_*`, `spore_puff`) about
1 KB each, 37 KB uploaded to LFS. No textures; no audio files (the sounds reuse the recordings already in the repo).

**Not finished (shown nowhere as finished):** the fused creature voices and the cook's own music (stand-ins in their
place); the Weaver and the Sporestalker walk floors, not walls and ceilings between their perches; Mycelorn's body is
code-built and its phase-2 pool doesn't visibly drain; a Draught plug's blunt x2 (needs the blow's damage type at
`HitThing`); Mycelorn's poison / rot buildup x0.3; an ally cutting a cocoon (fire does) or freeing a summited tame; the
Sporestalker's wall-kick pounce; the Spore body's shoulder motes at 75 and cyan breath at 100; the crafting extras'
effects (Queen's jelly, King's slurry, Star-thread); the Stipe Maul (boss-weapons cook).
