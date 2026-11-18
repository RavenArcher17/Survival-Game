# Build cook: build-biome-ocean (the Ocean & reefs into Aaron's build)

Lane: build-biome-ocean, 2026-11-15. Design source (SPEC for this lane, not re-decided): `cooks/biome-ocean.md` (read in
full), plus `world-map.md` (B2 placement, Saltgate, camps), `combat.md` (attack-row schema, boss template, soak),
`survival-crafting.md` (breath / crush numbers, Reefmail, diving helm, sea kelp / sea grapes, Coral keep, Deepbreath,
the Lazarus Fish), LATER-COOKS lines 51-60. Built on what landed: the void lane's land framework (`WRLand`,
`Data/lands/`), the Sky lane's world / scenery / bodies / bosses pattern (`WRSky*`), the taming lane's swim mount and
Floeback, the combat lanes' soak status and attack rows, the world-systems lane's shrines / masters / quests / events,
the portals lane's slots.

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, the LATER-COOKS lines naming this biome, and every section of
biome-ocean.md. "Plan" points into this doc; "Proof" is filled at the end (section 5) with file:line or a commit.

| # | Requirement (source) | Plan |
|---|---|---|
| R1 | Full cook: research, plan, build, spot-check (brief) | 1, 2, 3, 5 |
| R2 | Research names real games, places, systems and why they work, plus real-world reference (brief) | 1 |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases (brief) | 2 |
| R4 | Every build step listed (brief) | 3 |
| R5 | Design with intention: every object's job, what it connects to, how it meets ground / water / player / light / sound (Aaron 11-14) | 2.2-2.6 |
| R6 | Machines engineered, moving things move continuously (Aaron 11-14): the sluice gates, the Tide Clock float, the Pound's flood, the drowned bell, the sun mirrors on pivots, the Low Gate's sand, the bore, the harpoon line | 2.4, 2.10, 2.12 |
| R7 | Requirement ledger, every row proven (Aaron 11-14) | 0, 5 |
| R8 | The land on the map: B2 between the Sky's Chain Cliffs and the Swamp's estuary; strand, shelf, terraces; sea floor bands to the Blue Wall (brief; OCEAN 2.1, 3.5) | 2.1 |
| R9 | Five sub-zones O1-O5: ground, palette, props, bed; 60-120 m blends (Swamp mangroves, Sky cliffs, Saltgate inlet) (OCEAN 2.2) | 2.1 |
| R10 | Read rules: water colour by depth, underwater visibility by zone, night lights (Noctiluca surf, Glowrays, photophores, beacon, chime paint) (OCEAN 2.1) | 2.1, 2.3 |
| R11 | Eight skyline landmarks (Pound, Colossus, Beacon Stack, Spouting Horn, Dyke Head, Reef Lighthouse, Bell Tower, Long Dyke) (OCEAN 2.3) | 2.2 |
| R12 | POIs every ~150-250 m: 18 land + 9 sea hand sites, 33 + 18 kit POIs, the 125 m route rule (brief; OCEAN 2.4) | 2.2 |
| R13 | Decoration rule: sea-grass dunes, driftwood, tamarisk, kelp forests, coral heads; full, never cluttered (brief; OCEAN 2.2, 3.5) | 2.2 |
| R14 | Three camps (KEPT coordinates) with safe zones and PlayerStarts above the Tsunami Line (world-map; OCEAN 2.10) | 2.1 |
| R15 | Underwater play: dive / rise, camera-steered 3D swim, 300 / 450 cm/s, kick-dodge, surface dodge, wading x0.85, currents (OCEAN 2.6) | 2.7 |
| R16 | Breath 30 s (+0.6 / Swimming level), refill 3 s, drown 10/s, crush below 25 m 5/s; breath arc, depth readout, heartbeat, pressure hum, ear pop (OCEAN 2.6, 2.8) | 2.7 |
| R17 | Underwater combat rules: thrust classes full speed, others x0.8, no heavy charges, guard +20%, no bows, crossbow 50%, bolts x0.6 (Tide x1.0), fire x0.3, lightning spreads 4 m, camera arm 250, underwater Wildbrands only (OCEAN 2.6) | 2.7 |
| R18 | Air pockets refill breath; the Drowned Hall exempt from the underground rescue; no claims there (OCEAN 2.5) | 2.7, 2.11 |
| R19 | Five sea caves: Blowhole Grotto, Saltkin Mine, Pearl Cave, Siren's Hollow, Lazarus Deep (OCEAN 2.7) | 2.5 |
| R20 | Resources: tidecoral (underwater, 3 hits, hardens in air, regrows 3 days), pearls, kelp, sea grapes, salt, sea-glass, driftwood, oysters, urchins, ores; creature drops (OCEAN 2.7) | 2.5 |
| R21 | Crops sea kelp and sea grapes wild on the reef flats, placed and obtainable (brief; OCEAN 2.7, SC) | 2.5 |
| R22 | The tide: highs 00/12, lows 06/18, +-1.2 m, local to B2 with an edge blend; shelf pools, grotto, dyke wall, bell tower top; the Tide Clock float (OCEAN 2.8) | 2.6 |
| R23 | Hazards: deep water tells; rip channels (250 cm/s, sideways escape); blowholes and surge channels and the Horn launch; man o' war fleets (OCEAN 2.8) | 2.8 |
| R24 | Weather: Clear 40 / Overcast 15 / Rain 15 / Sea fog 15 / Squall 15 and their effects; open-sea mast strike (OCEAN 2.9) | 2.9 |
| R25 | Biome event: the King Tide (warning, the Low, stranded and pool-bound creatures, the spawning night, the Return bore stopping at the Tsunami Line, pays, never in camps) (brief; OCEAN 2.10) | 2.10 |
| R26 | Five shrines with guards (2 at sea), the Great Shrine's Mirror Pool (brief; OCEAN 2.11) | 2.11 |
| R27 | Path master Hae-won (look, place, day loop, lines) and her 6 quests (OCEAN 2.11) | 2.11 |
| R28 | Eleven creatures: looks, anatomy, attacks with visible contact, numbers, drops, taming methods, Reefjaw path magic, Wavecharger egg steal, utilities (brief; OCEAN 2.12.1-2.12.2) | 2.12 |
| R29 | Real models where Fab / our packs allow, real animation, behaviour (brief) | 2.12 |
| R30 | Day / night tables per sub-zone, open-sea packs, living-world beats (OCEAN 2.12.3) | 2.12 |
| R31 | Old Barnacle: hull with its own health, fire and blunt, naked window, spare shells, denial (brief; OCEAN 2.13.1) | 2.13 |
| R32 | The Siren Eel: song turns tames (furthest first), Lure, interrupt, beeswax plugs, stalactites (OCEAN 2.13.2) | 2.13 |
| R33 | The Reef Tyrant: roaming route, rest at the Mooring Stones, harpoon tether, beaching, line limits (OCEAN 2.13.3) | 2.13 |
| R34 | Thalassor: look, the Pound, the sun counter (mirrors, sunstone, flare, Sun path), beaching, phase 1, Bubble net, the Sluice Breaks, phase 2, hidden phase 3, drops (brief; OCEAN 2.14) | 2.14 |
| R35 | Arenas for the boss and each mini (brief) | 2.13, 2.14 |
| R36 | Creature and boss voices, underwater low-pass (OCEAN 2.15) | 2.15 |
| R37 | Music: Eb major 76 BPM cue sheet, tide-driven mix, underwater low-pass, the planned silence (brief; OCEAN 2.16) | 2.15 |
| R38 | The Tide path: Flow, Gills, 22 nodes, 10 abilities, harpoon line, passives, proc feedback, combos (OCEAN 2.17) | 2.16 |
| R39 | Body change: Tide thresholds 10/30/50/75/100 (OCEAN 2.18) | 2.16 |
| R40 | Reefmail armour set placed and obtainable; diving helm; coast items; pearl ring; trophies; Hae-won's knife (brief; OCEAN 2.19) | 2.17 |
| R41 | Building set: the Coral keep placed and obtainable (brief) | 2.17 |
| R42 | Story only through the land; nine lore items (OCEAN 2.20) | 2.2, 2.11 |
| R43 | Six secrets O-S1..O-S6 (OCEAN 2.20) | 2.11 |
| R44 | Invasions Sun <-> Tide through Saltgate (Ocean side live; the Desert side waits for the Desert) (OCEAN 2.21) | 2.18 |
| R45 | Portal slots in the Ocean (brief) | 2.19 |
| R46 | Data-driven: numbers in Data/*.json and tuning; abilities as UWRGameplayAbility + ClassForNode; all damage through ApplyHit; sounds / fx as table rows (brief) | 2 throughout |
| R47 | Every path: devices (mouse + pad prompts, F1 rows for diving), states, edge cases, multiplayer (server authority, replication) (brief) | 2.7, 2.20 |
| R48 | No placeholders presented as finished; unfinished stays hidden (brief) | 4 |
| R49 | Gates: compile only; no tests or check scripts; one UE job at a time (brief) | 3 |
| R50 | Git: pull, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, no tags or releases, LFS sizes reported; LANDED line (brief) | 3 |
| R51 | LATER-COOKS 51-60: the underwater frame shared; the tide as a regional sea offset; Squall reconciled; Floeback tundra-only; Old Barnacle leads the invasion; sunstone / beeswax in items before the bosses; Tidebreaker blueprint; Reefmail look merge; AWRHarpoonLine generic | 2.7, 2.6, 2.9, 2.12, 2.18, 2.17 |
| R52 | Swimming mounts: Wavecharger (1,100 cm/s underwater, 40 m, rider's own breath), Driftback hauler (600, platform); Glowray light; Pearldiver finds (OCEAN 2.12) | 2.12 |
| R53 | The Lazarus Fish in Lazarus Deep's black pool; ocean fish on every coast (SC KEPT) | 2.5 |
| R54 | The Drowned Hall: 8 rooms, 4 ways in, light, music (OCEAN 2.5) | 2.11 |

---

## 1. Research (build-side: how the best games make this work, and what we copy)

The design cook (biome-ocean.md 1) already researched the content (MH Tri's underwater lesson, Uth Duna's veil,
Subnautica's sound-first leviathans, Horizon's breath gate, Ark's depth-for-value, Valheim's sea danger, TotK's geysers,
Ys, tsunami stones, orca wave-washing, bubble nets, mantis shrimp, morays, funayurei, sirens, rips, maelstroms, coral
spawning, the haenyeo, sperm whales). This section is the build research: how those games make the systems feel right
in the hand, and the real numbers we check ours against.

| Game / source | The exact thing | Why it works | What we build from it |
|---|---|---|---|
| **Subnautica** (Unknown Worlds) and **Subnautica 2** controls ([gamepressure keybinds](https://www.gamepressure.com/subnautica/controls-keybinds/z3a97b), [SCUF S2 settings](https://www.scufgaming.com/gaming/games/general/subnautica-2-best-controller-settings/), [Epic S2 tips](https://store.epicgames.com/news/subnautica-2-guide-tips-survive-ocean-depths)) | Underwater you swim where the camera looks; Space rises, C (pad: a shoulder) descends; oxygen is a meter of seconds, 45 base, tanks raise it; surfacing, a hatch or a vehicle refill it at once | Steering with the look direction makes 3D movement one-stick simple; separate up / down keys let you hold depth while looking around; refills are instant so the loop is "dive, surface, dive" not "wait" | Underwater forward follows the camera's full rotation; Jump rises, Dive (Ctrl / pad B hold) sinks; with neither, you hold your depth (neutral buoyancy) so you can look around; breath refills in 3 s at the surface or in an air pocket |
| **ABZU** ([Giant Squid: Fluid Motion in ABZU](https://giantsquidstudios.com/Fluid-Motion-in-ABZU)) | The diver's heading is levelled out near the surface and near-vertical headings are eased so the body never flips | Free 3D swimming disorients unless the game quietly keeps "up" stable | The body's pitch follows the swim velocity but is clamped to +-70 degrees and eased (the hero's mesh pitches, the capsule never rolls); at the surface the stroke stays flat |
| **Horizon Forbidden West** diving ([Push Square](https://www.pushsquare.com/guides/horizon-forbidden-west-how-to-breathe-underwater-for-longer), [GamesRadar: Diving Mask](https://gamesradar.com/horizon-forbidden-west-breathe-underwater-diving-mask)) | A breath timer shown only underwater; a quick refill at the surface; one story item removes the limit and opens the sea | The limit is the early gate and the item is the reward that changes how the whole region plays | Breath arc only underwater; the diving helm, Diver's draught, Reefmail 4, the Wavecharger and Gills open the deep in steps (KEPT design) |
| **Sea of Thieves** (Rare) | Sharks show a dorsal fin cutting the surface and a music sting before they bite; swimmers are slow and vulnerable | The tell is on the surface where a swimmer looks | Every Reefjaw approach at the surface shows its fin and V-wake; underwater it trails a bubble line; the bite has a 0.7 s gill-flare tell |
| **Assassin's Creed IV** diving bell sections | Sharks patrol on readable loops; hiding in seaweed breaks their sight; air barrels refill | Readable loops let a weak swimmer plan; refills are placed with intent | Tidecoral nodes and air pockets are placed along the dive lines with intent (the Kelp Cathedral anchor stone at 15 m, the Pearl Cave's pocket, the Drowned Hall's dome) |
| **Monster Hunter Wilds** Uth Duna | The veil (its element) is armour you strip with the opposite | One rule the player can see | Thalassor's Sun-dried parts grey and crack (visible), x1.5 damage, x2 stance |
| **Elden Ring** / **Dark Souls** shallow-water bosses | Wading slows you and every step splashes | The floor reads | Wading 60-140 cm: run x0.85, splash per step; the Pound's flat is a wading floor in phase 1 |
| **Unreal Engine CharacterMovement** swimming (engine source `PhysSwimming`, `ImmersionDepth`, `Buoyancy`) | Swimming keeps 3D input acceleration (only walking strips Z); net buoyancy = Buoyancy x immersion, applied as gravity x (1 - net) | Already predicted and replicated | Dive / rise ride the engine's replicated crouch and jump flags (no new saved-move bits: all four custom bits are taken by sprint and the action slows); underwater the buoyancy goes neutral inside `PhysSwimming` so both machines compute the same depth |
| **Tidal bores**, real ([Britannica](https://www.britannica.com/science/tidal-bore), [Physics World: the silver dragon](https://physicsworld.com/a/chasing-the-silver-dragon-the-physics-of-tidal-bores/), [Coastal Wiki](https://www.coastalwiki.org/wiki/Tidal_bore_dynamics)) | The Qiantang bore runs 10-24 km/h (2.8-6.7 m/s), up to 9 m; the Severn's is 1-2 m | A wall of water you can outrun only if you started early | The King Tide's bore: 3 m at 600 cm/s (inside the real range), warned 10 real minutes ahead by the white line and the roar |
| **Semidiurnal tides**, real | Two highs and two lows a lunar day (12 h 25 min); slack water at the turn | A clock the player can learn | Highs 00:00 / 12:00, lows 06:00 / 18:00 game time; the water level is a cosine of game hours, so slack water is the flat of the curve |
| **Tsunami stones**, Aneyoshi ([Smithsonian](https://www.smithsonianmag.com/smart-news/century-old-warnings-against-tsunamis-dot-japans-coastline-180956448/)) | "Do not build below this point" | The land teaches the rule | The nine stones sit exactly on the bore's stop contour (the 6 m contour) |

**Lessons we build to:**
1. One input model underwater: look where you swim, Jump up, Dive down, neither holds depth. No new buttons beyond the
   Dive hold, which is the Dodge button held (tap still dodges: the kick-dodge).
2. Breath and depth read only when they matter (underwater), and the refill is fast.
3. Every underwater threat shows on the surface (fin, wake) or in the water (bubble line, column) before it lands.
4. The tide is a visible clock: the Tide Clock's float, the wet band on the sand, the shelf pools.
5. The sea takes back: every place, creature and event follows the tide.

---

## 2. Plan (every gap solved before building)

### 2.1 The land on the map (R8, R9, R10, R14)

**Where, in this world.** The live world is compressed from world-map's: the Sky's Hollow Cape ends at its Chain Cliffs
(south coast y ~ -7,955, cliffs 240-300 m), the Storm peaks' Crown face rises out of the sea at y ~ -5,680 (x -200..+900)
and the Swamp's frame starts at y -5,592 (sea along its north edge). The Ocean fills the strait between them, world-map's
B2 east of the Spine. **Coordinate rule (decided):** x as in biome-ocean.md; y squeezed from the cook's -8,000..-5,400
into -7,955..-5,700: `yW = -7955 + (yD + 8000) x 0.8673`. Every cook coordinate goes through one function `Y()` in
`Tools/terrain/ocean.mjs`, so the cook's tables still read straight into the script.

**Ground as a heightfield land** (`Data/lands/ocean.json` + `ocean_ground.bin`, written by `node Tools/terrain/ocean.mjs`,
built by the existing `WRLand` heightfield path on every machine): x0 -350, y0 -7,960, step 5 m, 951 samples. Samples are
hidden (255) where another land owns the ground: wherever the Sky has ground (read from `sky_ground.bin`), inside the
Storm frame wherever the Storm's ground stands at or above ours (read from `storm_height.r16`), south of y -5,640. Seams:
at the Sky's cliff foot our shelf sits 0.5-3 m under the sea; at the Crown's north face our terraces end on the face's
contour; the strand's south end dips into a 6 m estuary channel toward the Swamp's sea floor (-14 m). West: the **Spine**
ridge (x -150..+300, crest 380-520 m), dropping west into the sea at x ~ -330 (where the Desert's ground goes later);
**Saltgate** cuts it as a 60 m saddle (floor 58-64 m, 90 m wide) from the east mouth west to the far shore.

| Band | x (m) | Height | Surface |
|---|---|---|---|
| Spine face | -150..+350 | 520 -> 60 | granite / limestone scree |
| O3 Salt Terraces | +350..+1,300 | 60 -> 8 in 6 benches (40-120 m flats, 2-4 m risers), pans on the lower three | terrace earth #B98A52, salt crust #F2EFE6 on pans, tamarisk speckle |
| The Pound | (+1,000, -7,262) | 80 m pool floor -8 m, flat -0.6 m; the wall built in code | lagoon mud |
| O2 Blowhole Shelf | +1,100..coast, north | 0.2-3 m wave-cut platform, surge channels 2-4 m deep, pools, stacks offshore | limestone gold #C9A46A, wet black below +1.2 m, pool green |
| O1 White Strand | +1,300..coast, south | dunes 3-9 m, beach 0-3 m, bars offshore with rip gaps | sand #EDE8DC, wet band below +1.2 m, sea-grass on dunes |
| O4 Kelp shelf | coast (~+1,850)..+2,500 | 0 -> -25 m, sand channels, barrens | sand #D8CDB0, barren purple |
| O5 Reef crest | +2,500..+2,800 | rises to -0.5 m | reef cream / coral orange |
| O5 Reef slope | +2,800..+3,500 | -1 -> -35 m | coral mottle |
| The Blue Wall | +3,500..+3,700 | -35 -> -70 m near-vertical | dark limestone |
| Open floor | > +3,700 | -70 -> -90 m | grey ooze |

**Water colour reads the depth** through the bottom showing through (real water's read): the floor's vertex colours are
painted by depth band (white sand under 3 m reads pale turquoise, olive-teal 3-25 m, coral mottle on the reef, near-black
ooze past the Wall reads ink). Underwater visibility per band (40 / 25 / 12 m; x0.5 in Sea fog) through an exponential
height fog override on the local camera while the head is under (`UWROceanWorld::TickLocal`).

**Zones** (grid letters, blend 60 m): `s` O1, `h` O2, `t` O3, `k` O4, `r` O5, `p` the Spine. Blends: Swamp border (mud
and oyster grey below y -5,750, mangrove scenery at Mangrove Mouth), Sky border (cliff foot), Gate Inlet salt marsh.
**Camps (R14):** three PlayerStarts tagged `ocean`, on dunes / shelf top at 6-9 m above the Tsunami Line, safe zones
35 m, starter stations at camp 2.

### 2.2 Landmarks, sites, kit POIs, decoration (R11-R13, R42, R5)

Everything above the ground is `Data/ocean.json` (from `ocean.mjs`, parsed by `FWROceanData`), built in code by
`AWROceanScenery` (the forest `FBuild` kit, the Sky's pattern) on every machine, collision where you can stand or bump.

| Piece | Built as | Job / connects to / meets |
|---|---|---|
| **The Pound** | ring wall 240 m, 30 m tall: coursed coral-limestone blocks on a battered plinth; 4 m walkway with parapet; 3 stair towers N / W / S; 4 bronze sluice gates on the east arc on hinge drums (3 shut, the NE one burst outward); 4 rim bollards with broken chains over bronze saddles on the parapet into the water; 8 stilt-tower stumps and 4 bollards inside | The boss arena. The burst gate feeds the **Sluiceway**, a cut channel (8 m wide, floor -6 m) east to the sea under the broken Sluiceway Bridge. The plinth is buried 1 m in the terrace bench; chains never pass through stone. |
| **The Drowned Colossus** | Ahrumet's twin in code: 60 m sandstone figure face-down in the Gate Inlet, back eaten into coral and barnacle hollows, a whaling chain thrice round the neck to a bronze stake, one arm reaching west to the pass | Story beat 9; chest buried 2 m so the water line cuts its shoulders. |
| **The Beacon Stack** | 70 m stack of eroded limestone drums 120 m off the shelf; a half-fallen causeway of block piers; a keeper's stair on corbels round the stack; the driftwood beacon (6 m cone) under a shell-chime cage on an iron hoop | E with 4 wood: burns all night for your clan (saved), lights O2 300 m, map vista. |
| **The Spouting Horn** | a 3 m vent into the grotto below, a Saltkin arch round it, worn footholds | The Horn hazard and launch; it blows only rising / high water because the grotto's surge drives it. |
| **The Dyke Head** | 25 m square tower at the dyke root, gate stair, seaward door with the empty 2 m iron key-hook, the iron-bound vault door under it | O-S2's vault; the dyke's wall top leaves its seaward door. |
| **The Long Dyke** | 11 tower stumps (8 m square) 200 m apart, a 3 m wall top at -1.0 m (mean): dry at low tide, 2.2 m under at high; phosphor lamp niches | Low-water walk to the reef; stump 7 holds the Hall's one-way Sluice Door. |
| **The Reef Lighthouse** | round tower 6 m across on the crest, broken at 20 m, leaning 8 degrees seaward, stair inside, bottomless bronze ladles on cords at every sill | Lighthouse Shrine in the lamp room. |
| **The Bell Tower** | top 6 m of a 4 m square tower in the crest's lee, louvres, phosphor finial; its stairwell 18 m down into the Drowned Hall | Way in 1; the shutter window opens at midnight high tide (O-S1). |
| **The Tide Clock** | 6 m notched pillar in a shelf pool, King Tide notch deep-cut; a cedar float in a carved slot riding the live level | Shows the tide; Great Shrine beside it. |
| 18 land + 9 sea sites | the cook's 2.4 tables, each built as described | pay what the cook says (caches, nodes, lore, spots) |
| Kit POIs 33 + 18 | the cook's 15 kit pieces with variants, Poisson >= 150 m per sub-zone, then the 125 m route rule along the coast road, the dyke top and the three dive lines | shell-chime beacons every 120-200 m on the coast road, leaning 4 degrees seaward |

**Decoration rule (R13)** in `ocean.mjs`: O1 sea-grass tufts on dunes (none below the wet band), driftwood along the
drift line (+1.2-1.8 m, where real driftwood lands), shell middens near sites; O2 barnacle / weed bands between -0.5 and
+1.2 m on rock, stacks offshore; O3 tamarisk on benches (never on pans or roads), dry-stone walls along bench edges; O4
giant kelp (holdfasts on rock 3-25 m, stipes to the surface, a 2 m canopy mat), urchin barrens; O5 coral heads (orange,
pink, cream; branching and brain) on crest and slope, giant clams on the flats. Kit pieces never within 150 m of each
other; decoration thins within 12 m of a site.

### 2.3 Night lights (R10)
Blue Noctiluca glow on the breaking surf lines (a glow strip along the strand's and crest's surf in `M_Glow`, pulsing with
the wave sets) and a blue stroke trail on swimmers at night (fx `ocean.noctiluca_stroke`); Glowray rings; Thalassor's
photophores; the Beacon Stack's fire; chime-beacon phosphor (night blocks); dyke lamp niches; the Bell Tower finial.

### 2.4 Moving machines (R6)
Tide Clock float (height = the live level, slides in its slot); sluice gates (each leaf swings 70 degrees outward on its
hinge drum over 0.8 s, ease-out, at the 50% transition; replicated, reset with the fight); the Pound's water (eases +4 m
over 10 s on server time, an `AWRSwampFlood`-style volume and a moving plane); sun mirrors (dish on a yoke-and-trunnion
pivot following the operator's aim at 90 degrees/s, replicated); the drowned bell (a pendulum, period 2.2 s, when rung);
the bore (a lathed curl advancing at 600 cm/s, stopping on the Tsunami Line); the harpoon line (catenary every frame;
chain for Leviathan's hook); the Low Gate's sand bank (slumps over 6 s when the King Tide's Low begins).

### 2.5 Caves, resources, crops (R19-R21, R53)
Caves built like the forest's (rock shells in code round air from `ocean.json caves`), underground ways registered so
the rescue leaves them alone: Blowhole Grotto (floods at high tide), Saltkin Mine (dry, rails, tide-tablet), Pearl Cave
(6 m entrance, air dome), Siren's Hollow, Lazarus Deep (52 m in the Blue Wall; the Lazarus Fish spot in its back pool).
Nodes from `ocean.json resources` on the server: **tidecoral** (gathered only while under the surface; 3 hits, 2 each;
the living orange whitens over 10 s when the tide leaves it in air; regrows 3 game days), clams (pearl 2%, Pearldiver
x5), oysters, sea-glass, driftwood, salt pans, urchins, kelp, copper / tin / iron in caves and wrecks. **Crops**: wild
sea kelp and sea grapes on the reef flat and kelp shelf (pickable, giving the crop and its seeds).

### 2.6 The tide (R22)
Level(t) = 120 cm x cos(2 pi hours / 12) (highs 00 / 12, lows 06 / 18) + the King Tide's drop (-600 cm eased over the
5 min warning) - computed on every machine from the replicated clock and `AWROceanState`'s King Tide timeline.
- **Box**: x -300..+4,700, y -7,955..-5,700, a 300 m ramp outward north, south and east (weight 1 -> 0).
- **Swimming water** (`AWRTideWater`): water-volume actors tiling the box and ramp (a core + ramp strips), each from
  -100 m to mean + level x weight; over each an air volume (not water, same priority 2) up to +2.5 m so the still sea
  (priority 0) never makes you swim above a low tide. Moved every 2 s.
- **Drawn sea**: `AWRSea::SetRegionOffset` re-places the vertices of tiles touching the box (Z + offset x weight) and
  updates them every 2 s; the rest of the sea is untouched.
- Everything that reads the water uses `UWRWaterSubsystem::SurfaceAt(Point)` (tide-aware).

### 2.7 Underwater play (R15-R18, R47, R51)
- **Input**: new rebindable **Dive** (Left Ctrl; pad: hold Dodge). Hold to sink, Jump to rise, neither holds depth;
  underwater Forward follows the camera's pitch too. Dive rides the engine's replicated crouch flag, Rise its jump
  flag: predicted, the server agrees.
- **Physics** (`PhysSwimming` override): fully under, buoyancy neutral unless rising; speed 300, fast 450 at 6 stamina/s;
  Gills x1.25, swim fins +25%, Tyrant fin +15%.
- **Kick-dodge**: Dodge while under = 400 cm 3D burst over 0.3 s, i-frames 0.2 s, 14 stamina; at the surface a 250 cm
  surface roll. Perfect-dodge rules as on land.
- **Wading** (60-140 cm at the feet): run x0.85, no sprint over 110 cm, a splash per step.
- **Breath / depth** (`UWRDiveComponent`, server ticks, owner-replicated): under = head below the surface (tide-aware;
  other water volumes' tops). Max 30 s + 0.6 s x Swimming level (x1.5 Diver perk; x2 helm / Reefmail 4 / Deepbreath; 90
  s Diver's draught, the largest wins; +5 s pearl ring). Refill full in 3 s at the surface or in an air pocket. At 0: 10
  a second via `ApplyHit` (environment, unblockable), no floor. Crush below 25 m (40 m helm / Reefmail 4 / Pressure, 80
  m Gills, none Abyss diver): 5 a second. Creatures exempt. Gills: no breath limit.
- **Read**: a thin sea-green arc around the crosshair underwater only; pulses at 25% with a heartbeat and mouth
  bubbles; edges darken per tick at 0; depth in metres below 10 m, coral orange past your crush limit; pressure hum past
  20 m, ear-pop at 25 m.
- **Underwater combat** (`WRUnderwater::*` read by melee, ranged and spells): thrust classes full speed, others x0.8
  (Gills none); no charged heavies for greatsword / great hammer / axe; guard stamina x1.2; bows refuse; crossbows
  range x0.5; bolts x0.6 (Tide x1.0); fire x0.3, no burn; lightning spreads 400 cm in the same water (caster too unless
  Grounded); camera arm 250 underwater; only `underwater` Wildbrands.
- **F1 page and prompts**: Dive / Rise / Fast swim / Kick-dodge rows, keyboard and pad.

### 2.8 Hazards (R23)
- **Rips** (`AWRRip`, 5 on O1 at bar gaps): 8 x 120 m seaward lane, 250 cm/s falling tide, 170 rising; straight against
  it costs x2 swim stamina and makes no headway; out by its side frees you (quest 2). Tells: a dark flat gap (the surf
  strip breaks there), foam flecks drifting out (fx), a hiss. Mounts ignore it.
- **Blowholes** (`AWRBlowhole`, fields + the Horn): live mid-rising to high tide, every 9-16 s: 1.0 s tell (boom, hiss,
  spray puffs, a 1 cm shake), then a 6 m column (40 m Horn): 40 + launch 6 m (18 m Horn); jumped within 0.3 s before:
  ride it unhurt (the Horn's ride reaches O-S3's ledge). Surge channels: a wave at 600 cm/s, 30 + knockdown + 6 m drag.
- **Man o' war** (`AWRManOWar`, 2 fleets of 6-20 on O5 drifting with the wind; a Squall strands one on O1): sails bob,
  tentacle shimmer 8 m down, a glassy tick within 6 m; a touch 25 + poison 50 (Reefmail 2 / swim mounts x0.5); fire pops
  sails.

### 2.9 Weather (R24)
`weather.json` ocean row: Clear 40 / Overcast 15 / Rain 15 / Sea fog 15 / Squall 15 (Squall = rain + gale + swells:
surface swim x1.5 stamina, man o' war inshore, Reefjaws x2 at sea). The open-sea mast strike rule is written against
any `mast` piece over open sea (boats aren't in the game yet; live when they land).

### 2.10 The King Tide (R25)
`world.json events.kingTide` run by `UWROceanWorld` (server), `AWROceanState` replicating phase + start: warning 5 min
(the sea draws back, every chime beacon rings, flocks lift, the drowned bell, toasts); the Low 50 min (-6 m: crest,
dyke, Sunken Market dry, Low Gate open, stranded Kelpgrazers / Driftbacks with x3 offer-taming, 4-6 pool-bound Reefjaws
x1.3 damage taken, Old Barnacle roams, nodes x3); the spawning night if 20:00-06:00 (glow clouds, all tidecoral regrows,
Glowray rings x2 taming); the Return (T-10 min white line and roar; T-3 min the 3 m bore at 600 cm/s from the reef to the
Tsunami Line: 80 + carried 20 m + soaked below it, pieces 200 once; camps above it).

### 2.11 Shrines, Hae-won, the Drowned Hall, secrets (R26, R27, R43, R54)
Five shrine rows (`land: ocean`, guards, the looks per cook); Hae-won a `masters` row on the MetaHuman rig with her stilt
house over the Mirror Pool and her day loop (dawn: the Kelp Cathedral anchor stone; day: mending at the pool; night:
porch lamp); six quests in `quests.haewon` (depth touch, rip escape sideways, underwater harpoon kills, tidecoral +
helm, Tsunami Line / stones, the Tyrant tethered then killed). The Drowned Hall's rooms as scenery round its air
(the Dome's pocket refills breath; Bell Chamber, King's Tower, Archive dry), its four ways, Glowray light, lore in place,
the Sluice key spot (3 s). Secrets O-S1..S6 as spots with their conditions.

### 2.12 The creatures (R28-R30, R52)
No sea packs exist in `Content/` (checked), so the eleven are **built in code** the Sky's way (`UWROceanBeast`, a
procedural body re-posed every frame from how the real animal moves; vertex colours, `M_Glow` lights): shark (Reefjaw;
the Tyrant x2.3): torpedo, the goblin-shark jaw as its own part shooting 120 / 300 cm on the bite, tiger bars, a scythe
tail beating side to side; eel (Tidecoil 3.5 m; Siren Eel 18 m): a trail-following ribbon with a pharyngeal jaw, banded
organs lighting on Shock pulse, the Siren's ray crest; crab (Spinecrab; Old Barnacle x2.5 with the hull): spined dome,
8 stilt legs stepping in alternating tetrapods with planted feet, crusher and club; bird (Gullraider: skua body, hooked
bill, inflating pouch; Tidefowl: puffin); seahorse (Wavecharger: upright head, ring plates, fronds, curling tail);
turtle (Driftback: ridged dome, long fore flippers, barnacle garden); ray (Glowray: manta wave-stroke, horns, barb,
photophores); otter (Pearldiver: floats on its back with its stone); sea cow (Kelpgrazer: bark-hide barrel, forked
tail); fish (Ripplefin: a school of bodies with boids). Swimmers steer in 3D inside their depth band and never leave the
water (except the surf lunge, the Driftback haul-out, crabs and birds on land). Rows of 2.12.2 in `attacks.json` with
`FWROceanAI` hooks for slingshot, thrash-drag (victim pulled 6 m down, breath held), breach strike (bubble column 1.0 s
before), surf lunge (stranded 1.5 s), hole strike / second jaw, shock pulse (same water), club snap (two hits 0.06 s
apart, a cavitation pop), shell guard, sand burst, snatch (a food stack to the Gullery, dropped on death within 60 s),
mob call, herd click, light flare. Reefjaw path magic, Wavecharger egg steal (Kelp Cathedral, night), offer-food tames;
Wavecharger mount, Driftback hauler / platform, Glowray light + hostile bubble lines, Pearldiver finds. Populations per
sub-zone from 2.12.3 (`tuning oceanLife`), near players, out of sight; the living-world beats.

### 2.13 The minis (R31-R33, R35)
`FWROceanBoss` hooks: Old Barnacle with the hull part (1,200, blunt x2, a plank per 300, fire 3 s -> naked), spare
shells in the cove (bell 800 / carapace 400, occupancy), Hull fortress; the Siren Eel in Siren's Hollow (5 rock islands,
ring ledge; song windup 1.2 s, 150 crest damage interrupts; tames turn 8 s, furthest first; the Lure pulls 250 cm/s, -25%
per dodge away; beeswax plugs; stalactites with 1 s shadows); the Reef Tyrant on its 3.2 km loop with the 2 game-hour
rest at the Mooring Stones, harpoon tethers (E at a stone within 3 m, 10 m leash, 20 s / 600 pull, two tethers in < 2 m
water = beached 8 s).

### 2.14 Thalassor and the Pound (R34, R35)
As the cook: body in code (14 m sperm-whale head on a 38 m serpent trunk, 9 m fluke, harpoon spines, collar chains,
sucker scars, 14 photophores a side); three `AWRSunMirror` pivots on the wall (E to take, camera to aim; a traced gold
beam in daylight or from a loaded sunstone; 2 s on a part = Sun-dried 20 s, x1.5 damage, x2 stance; flare = 1 s; Sun
path hits count); the creak before every move, silence before grabs; phase 1 (head ram, jaw snap x3, swallow lunge,
fluke slam, spout volley lobs with rings, wave wash with the draw-back); beaching 5 s; Bubble net from 75%; the Sluice
Breaks at 50% (+4 m in 10 s); phase 2 (breach destroys towers, undertow, coil-snap-lash, breaths); hidden phase 3 (the
maelstrom, broken by 3 s of beam); drops; run-back stone 50 m out from the north stair.

### 2.15 Sound and music (R36, R37)
Rows in `sounds.json`: voices per creature and boss; an underwater low-pass on the listener while the head is under;
beds `land.ocean`, `zone.O1..O5`, underwater bed, pressure, ear-pop, heartbeat; music pools `explore.ocean`,
`night.ocean`, `combat.ocean`, `mini.ocean`, `boss.thalassor.p1-3`, `sub.drowned_hall`, `layer.king_tide`, `camp.ocean`.
First pass from the repo's recordings re-pitched (the Sky lane's rule), marked `_standin` for the audio cookpass.

### 2.16 The Tide path and the body (R38, R39)
`Data/paths/tide.json` (the cook's 22 nodes), **Flow** an owner-replicated attribute on the Mana pattern
(`WRTide::TickOwner`: gains from speed / sprint / swim / dodges / perfect dodges / hits while moving; drain after 1.5 s
still), the HUD bar, Gills, ten abilities (`WRAbilitiesTide.*`) with `AWRHarpoonLine` (reel / Anchor / Tow /
Leviathan's hook / tie-off), passives via `nodeEffects`, proc fx rows, combos with Fire, Lightning, Nature, Frost,
Blood, Necromancy (Sun's when the Desert exists), the body (the existing `player_art.json paths.tide` row plus gills
opening underwater and Flow brightening the sheen).

### 2.17 Gear, items, the Coral keep (R40, R41)
Rows per cook 2.19 (many exist hidden by `biome: ocean`); new: the coast materials, essence, core, Leviathan pearl,
sluice key, pearl circlet, Hae-won's knife, Tidebreaker blueprint, the coast items and two pieces (sun mirror, driftwood
beacon), the diving helm, nine lore items. Reefmail and the Coral keep exist (crafting / building lanes) and become
obtainable with the land (their inputs drop here).

### 2.18 Invasions (R44)
`world.json invasions.pairs` gains `desert <-> ocean` (Old Barnacle leads the Ocean side: Spinecrab ranks, Gullraider
flocks, the wet-sand mark), live when the Desert is.

### 2.19 Portal slots (R45)
`ocean.json portalSlots` places the two slots; they open only with an Ocean fold kit. If the kit isn't built in this
lane `portals.json biomes.ocean.land` stays false (hidden, not a placeholder) and LANDED says so.

### 2.20 Multiplayer (R47)
Server authority for every hit, breath, crush, event, boss state; every machine builds land, scenery and water from data
and the replicated clock; replicated: `AWROceanState` (King Tide, beacon per clan, sluices, bell), `UWRDiveComponent`
(owner), bodies' flags, the harpoon line, mirrors' aim.

---

## 3. Build steps (each piece compiled, committed, pushed)

1. **Ocean (1) water and land**: dive component, CMC dive, kick-dodge, wading, underwater rules, HUD arc / depth, Dive
   key, F1 rows; `WRTide`, `AWRTideWater`, `AWRSea::SetRegionOffset`, `SurfaceAt`; `ocean.mjs` -> land + `ocean.json`;
   `FWROceanData`; `UWROceanWorld` + `AWROceanScenery` (landmarks, sites, kit, decoration, caves, the Hall's rooms),
   nodes / caches / spots, crops; biome rows (world.json, weather, names).
2. **Ocean (2) the Tide path**: tide.json, Flow, abilities, harpoon line, passives, combos, HUD, body.
3. **Ocean (3) the creatures**: bodies, AI, rows, populations, beats, taming, mounts; `ocean` joins `liveBiomes`.
4. **Ocean (4) the world**: hazards, weather, the King Tide, shrines, Hae-won + quests, the Hall's ways and secrets,
   items / recipes / gear, invasions.
5. **Ocean (5) the bosses**: three minis, Thalassor, the Pound's machines, mirrors.
6. **Ocean (6) sound and music** rows, the underwater filter; CHANGELOG / HANDOFF; LANDED.

---

## 5. Proof (filled at the end; commits on master, files in C:\Dev\Wildreach)

Commits: 3b908b7 Ocean (1) · 447e86e Ocean (2) · 2c7ed4a Ocean (3) · abbddff + 21da0d1 Ocean (4) · 7c24bb9 Ocean (5) ·
83e7c04 Ocean (6). Every piece compiled (editor target) before its push; no tests run (the brief's gate).

| # | Status | Where |
|---|---|---|
| R1-R4 | Done | this doc 0-3 |
| R5 | Done | WROceanScenery.cpp / WROceanSites.cpp (each piece's job in its comment), ocean.mjs |
| R6 | Done but two | sluice leaves (AWROceanScenery::Leaves), Tide Clock float, the drowned bell's swing, the Pound flood (UWROceanArenas::StepFlood), the sun mirrors on pivots (AWRSunMirror), the harpoon line (AWRHarpoonLine::DrawLine). Not done: the Low Gate's sand drifting off (the gate is a spot that opens in the Low), the bore drawn as a moving wall of water (it is effect bursts along its front plus the real water rising) |
| R7 | Done | this section |
| R8-R13 | Done | Tools/terrain/ocean.mjs, Data/lands/ocean.json, Data/ocean.json, WROceanScenery/Sites/Nodes.cpp (Ocean (1)) |
| R14 | Done | lands/ocean.json camps (3, safe 35 m, above the line) |
| R15-R17 | Done | WRUnderwater.*, WRCharacterMovementComponent.cpp, WRCharacter.cpp, WRAbilities.cpp, WRRanged.cpp, WRCombatLibrary.cpp; rips' current in UpdateCharacterStateBeforeMovement |
| R18 | Done | air pockets UWROceanHazards::Setup; no building AWRClaimTotem::CanBuild (WRBuilding.cpp); land underground ways |
| R19-R21 | Done | ocean.json caves/resources, UWROceanWorld::SpawnServerActors |
| R22 | Done | WRTide.*, WRSea.cpp SetRegionOffset |
| R23 | Done but one | WROceanHazards.cpp (rips, blowholes + Horn ride to O-S3, surges, man o' war). Not done: fire popping a sail (LATER-COOKS) |
| R24 | Done | Data/weather.json ocean; squall x2 Reefjaws (oceanLife.squallSeaMult); the mast strike waits for boats |
| R25 | Done | world.json events.king_tide, UWROceanHazards::StepKingTide / StartLow / BoreHit; TameSpeedMult |
| R26 | Done | world.json shrines ocean_* (5) |
| R27 | Done | world.json masters haewon, quests.haewon (6); quest event fix in WRWorldSystems.cpp |
| R28 | Done | WROceanCreatures.*, WROceanBeast.cpp, creatures.json / attacks.json rows |
| R29 | Code-built | no sea pack in Content/: all eleven bodies built in code and posed every frame (WROceanBeast) |
| R30 | Done | tuning oceanLife.tables, UWROceanLife |
| R31-R34 | Done | WROceanBosses.*, WROceanBeast.cpp (leviathan, hull, crest, chain) |
| R35 | Done | UWROceanArenas::Place (cove spares, the Hollow, the route, the Pound's towers / mirrors / altar / run-back) |
| R36-R37 | Stand-ins | sounds.json rows `_standin` (voices, events, beds, pools, layer.king_tide); low-pass in WRAudioDirector / WRSound. The real cue sheet is the audio cook's |
| R38 | Done | Data/paths/tide.json, WRTidePath.*, WRAbilitiesTide.* |
| R39 | Done | WRTidePath LookSig / ApplyLook, player_art.json paths.tide |
| R40 | Done | items / recipes (Ocean (4)); Reefmail rows from the crafting lane, obtainable now (tidecoral, spinecrab shell) |
| R41 | Done | building_sets.json coral_keep (building lane), coralstone_block recipe; the furnishings blueprint drops from Thalassor (gear.json set decor) |
| R42 | Done | lore items (8 in items.json) and the spots' lore; the ninth is the tablet in the Archive (spot) |
| R43 | Done | AWROceanSpot kind secret (WROceanWorld.cpp) |
| R44 | Done (dormant) | world.json invasions.pairs ocean -> desert |
| R45 | Hidden | portals.json ocean land:false (no Ocean fold kit); slots in ocean.json |
| R46-R47 | Done | data rows and tuning throughout; server authority; replicated state (AWROceanState, UWROceanBody, AWRSunMirror, AWROceanBossProp, AWRHarpoonLine) |
| R48 | Done | Tidebreaker blueprint withheld (no weapon yet); portal slots hidden |
| R49-R50 | Done | compile gate only; commits by name; CHANGELOG + HANDOFF each piece; LANDED line |
| R51 | Done | LATER-COOKS lines (this lane) |
| R52 | Done | Wavecharger dive (AWRCreature::TickMount, diveDepth), Driftback hauler, Glowray trails, Pearldiver finds |
| R53 | Done | fish.json lazarus water, AWRBobber::WaterHere |
| R54 | Done | Hall rooms (Ocean (1)), air, no claims, sub.drowned_hall cue |
