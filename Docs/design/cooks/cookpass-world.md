# Cook pass: the Fire and Frost world (cookpass-world)

Lane **cookpass-world**, desktop, 2026-11-15. Scope: the built Fire and Frost world on the Old Island (`L_Island`):
terrain, dressing, POIs, sub-zones, lava, ice, sea and sky. Design source: `cooks/fire-frost.md` sections 3.1, 3.3.1-3.3.6
and 3.4.1-3.4.6 (the world parts; creatures, music, voices, paths and bosses belong to the creature, audio and combat
cook passes). Systems built on top of the world by `build-world-systems` (shrines, villages, quests, events, hidden
tubes, invasions) are kept as they are, and the new places are laid out around them.

Coordinates are metres in `L_Island`'s frame (origin at the centre, +x east, +y south), as in fire-frost.md.

---

## 0. Requirement ledger

The brief, every Aaron quote in it, and the fire-frost.md world rows, split into single requirements. "Done where" is
filled in at the end.

| # | Requirement | Source | Done where |
|---|---|---|---|
| R1 | Research how the best games do open-world places, density, sub-zones, hazards and skylines (named games, specific places) | brief, EVERY LANE 1 | this doc 1.1 (12 games, named places and systems) |
| R2 | Real-world reference (volcanology, glaciology, masonry, Rime-folk/smith material culture) | EVERY LANE 1 | this doc 1.2 (16 references) |
| R3 | Grade every built piece of the Fire/Frost world against THE BAR | brief | this doc 2 (B1-B13, each with file:line) |
| R4 | Plan every detail before building: layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases; list the build steps | EVERY LANE 2 | this doc 3, 4 (written and pushed before any code: showcase-notes dcc28e3) |
| R5 | Build the plan at full detail, with no placeholders or bare primitives shown as finished | EVERY LANE 3, BAR | commits 987cbcd, 7013d11, 45affb9; builders WRIslandPlacesLook.cpp:158-700, WRIslandPlacesLook2.cpp:153-820, WRIslandPlacesKit.cpp |
| R6 | Spot-check against the plan and the bar by reading the code, and fix gaps before pushing | EVERY LANE 4 | this doc 6 (found and fixed: maze reach and gaps, Ridge Watch door, Slag Light stair, Slag Light start state, two unity clashes) |
| R7 | Nothing called "already done" without file:line proof | EVERY LANE 5 | grades cite file:line (this doc 2); ledger rows cite file:line |
| R8 | Every object answers why it's here, what it connects to, and how it meets ground, water, light, players and creatures | DESIGN WITH INTENTION | this doc 3.3 / 3.4 "why / meets" columns; e.g. Fallen Smith haft spans the measured river banks (Tools/terrain/island_places.mjs bridge), Plough Trench on Cinderhorn's loop (creatures.json roam), places kept clear of arenas/shrines/camps (island_places.mjs keepOut) |
| R9 | Machines look engineered; moving things move smoothly and continuously, never snapping | DESIGN WITH INTENTION | Bellows breathing eased on the server clock (WRIslandPlaces.cpp TickLooks, Breath case); lamps ease at dusk (TickLooks NightLight); hazards on the clock (WRIslandHazards.cpp:157) |
| R10 | POI density toward the bar of one POI every 150-250 m (fire-frost 3.1: about 115 per biome) | G1 | Data/island_places.json: 41 places + 24 waymarks + 16 sastrugi fields + 168 kit + 22 hazard spots; with the built 20 POIs, 10 shrines and 2 villages: about 145 a side; every walkable cell within 125 m of a place (island_places.mjs kit loop) |
| R11 | Sub-zones: 5 per biome (V1-V5, T1-T5) with their own ground, props and ambient bed | fire-frost 3.3.1 / 3.4.1, G2 | zones in Data/island_places.json; WRIsle::ZoneAt WRIslandPlaces.cpp:95; beds WRAudioDirector.cpp:272 + sounds.json ambient.beds ffzone.*; name banner WRUIFlow.cpp:1452 |
| R12 | Skyline landmarks, readable from 1 km, each with a night read | fire-frost 3.3.2 / 3.4.2 | WRIslandPlacesLook.cpp:158 (plume), :165 (Bellows), :210 (Fallen Smith), :327 (Glasswall), :368 (Blue Fire), :410 (Slag Light), :471 (Kipuka), :520 (Wisp Spire); WRIslandPlacesLook2.cpp:153 (Tusk Gate), :194 (Great Inuksuk), :225 (Frozen Whaler), :285 (Weeping Seracs), :315 (Fox Pingo), :344 (Pale Pines), :357 (Ice Arch) |
| R13 | Hand-made story sites (environmental story, the old war between smiths and Rime-folk) | fire-frost 3.2, 3.3.3, 3.4.3, G3 | 26 story sites: WRIslandPlacesLook.cpp:555 on (Plough Trench, Miners' Grave and Camp, Charred Caravan, Tree Molds, Crater Stair, Warden's Quarry, Shard Field, Ridge Watch, Kilnmite Hill, Acid Pool, Steam Ground); WRIslandPlacesLook2.cpp (Wallow, Midden, Sledges, Fishing Camp, Drowned Sledge, Hunter's Camp, Blind, Giant Pine, Boneyard, Claw Scars, Watch Cairn, Moulin, Crevasse Maze) |
| R14 | Kit POIs placed by a seeded generator: at least 150 m apart, and route gaps over 250 m filled | fire-frost 3.1, 4.5 | Tools/terrain/island_places.mjs (kit section: 150 m apart, coverage 125 m from the walk map); builders WRIslandPlacesKit.cpp |
| R15 | Every POI has a light or skyline shape that reads at night | fire-frost 3.1 | lamps / fires / glowmoths / cups per place (FWRIsleSite::Lamp, NightFx, PassCup, Glowmoth in WRIslandPlaces.cpp); skyline shapes for the rest (this doc 3.6 table) |
| R16 | Micro-hazards, 3-4 per biome, each in one or two sub-zones, with at least 0.9 s of sound plus a visual tell, able to kill, and no meter | fire-frost 3.1, 3.3.4, 3.4.4 | WRIslandHazards.cpp: fumarole / geyser / serac (clocked, tells in StepClient :339), acid, thin ice (:235), snow bridge (:284), katabatic (:317); tuning hazards.ff |
| R17 | Lava kills: `lavaDps` 8 → 20, burn 12 → 20 per second | fire-frost 3.3.4 RAISE, G16 | Data/tuning.json hazards.lavaDps 20, lavaBurnPerSecond 20 (read at WRWorldDressing.cpp Tick) |
| R18 | Volcanic shrubs not green (G22), fixed with a different mesh, not the coat that hung the GPU | G22 | WRWorldDressing.cpp:511 (ember_shrub = char-tree meshes at shrub size, no coat) |
| R19 | Green kept only where it's real (the Kipuka) | fire-frost 3.3.1 | Kipuka: living pines + GV shrubs + moss + glowmoths, WRIslandPlacesLook.cpp:471 |
| R20 | Sky: the crater reads from the whole island (smoke plume, forge glow at night) | fire-frost 3.3.2 | crater.plume fx (Data/fx.json) + rim glow, WRIslandPlacesLook.cpp:158 |
| R21 | Sea graded and raised where below the bar | brief | graded Meets (this doc 2 B11, WRSea.cpp); the Slag Light gives the west coast its sea landmark |
| R22 | Wayfinding through the world, not the HUD: slag-cairn road, inuksuk road, sastrugi pointing at the glacier | fire-frost 1.1 (Ghost of Tsushima) | slag-cairn road (WRIslandPlacesLook.cpp:98, cups lit as you pass), inuksuk road (WRIslandPlacesLook2.cpp:57, each arm pointing at the next: yaw from island_places.mjs waymarks), Great Inuksuk pointing at the bowl. 16 sastrugi fields of snow ridges lying along the wind off the glacier (WRIslandPlacesLook2.cpp:744 `Sastrugi`, Data/island_places.json `ground`) |
| R23 | Places meet the systems around them: Cinderhorn's loop runs through the Plough Trench, Old Tusker's through his Wallow, shrines and arenas left clear | DESIGN WITH INTENTION | Plough Trench on Cinderhorn's loop, Old Tusker's Wallow on his loop (island_places.mjs), arenas/shrines kept clear (keepOut) |
| R24 | Data-driven: places, kit rules, hazards and zones in `Data/*.json`, numbers in `tuning.json`, sounds and fx as table rows | BAR | Data/island_places.json, tuning places + hazards.ff, fx.json 21 rows, sounds.json 16 events + 11 beds |
| R25 | Multiplayer: hazards server-authoritative, damage through `ApplyHit`, the same picture on every machine | BAR | server-only blows through UWRCombatLibrary::ApplyHit (WRIslandHazards.cpp HzBlow :83, acid, thin ice); clocked hazards identical on every machine; AWRIsleState replicates thin ice / bridges (WRIslandPlaces.cpp:178) |
| R26 | Map and compass: landmarks on the fill-in map once seen | BAR (no dead states) | landmarks as FWRPoi kind landmark (WRIslandPlaces.cpp:575) on the fill-in map; banner line WRUIFlow.cpp:1429 |
| R27 | Unity build: unique prefixes for file-scope helpers | lane note | namespaces WRIsleV / WRIsleT / WRIsleK / WRIsleHz / WRIslePlacesImpl, Isle*/Hz* helpers, dispatch inside each namespace (commit 7013d11; verified by a clean-tree unity compile, blob Module.Wildreach.15.cpp) |
| R28 | Land in pieces (commit and push each part); `git pull --rebase`; CHANGELOG + HANDOFF in the same commit | lane note | commits 987cbcd, 7013d11, 45affb9, each pulled with --rebase and pushed; CHANGELOG + HANDOFF in 987cbcd, CHANGELOG in 45affb9 |
| R29 | Master always boots to the menu and into play | NEW SITE RULE | places are guarded builds (a missing file or mesh skips with a log line, WRIslandPlaces.cpp Load/BuildOne); no map or save change; compiled clean |
| R30 | Later-cooks and leftovers that touch this scope raised (LATER-COOKS: machinery "no rivers, no steam vents"; building "tundra hot springs"; combat "basalt pillars along Cinderhorn's roam, run-back stones"; world-systems "steaming ford") | lane note | this doc 3.9; logged to LATER-COOKS (geothermal turbines on fumaroles, the Steam Ground hot spring, rivers / ford / lava-fall terrain rebuild) |
| R31 | Questions decided with a recommendation; nothing waits on Raven | TONIGHT | this doc 7 (three decisions with alternatives) |

---

## 1. Research

### 1.1 Games: what they do, and what we take

| Game, place or system | What it does and why it works | What we take, concretely |
|---|---|---|
| **Breath of the Wild**, the "triangle rule" (CEDEC 2017) | Hills and ridges are triangles that hide what's behind them. Each rise reveals a new point of interest, so the player keeps choosing. Landmarks come in kinds (towers, stables, shrines), with no one kind repeated in a line | Every landmark stands on a rise or round a bend of the ridge. A kit POI is never in sight of another of its own kind (150 m spacing plus a kind deck that deals kinds in turn) |
| **BotW** Hateno / Eldin | Death Mountain's smoke plume and glow read from anywhere on the map, so the volcano is the compass. Eldin's ground features (bubbling pools, rising embers, sulfur crust) tell you it's hot before any meter | A permanent crater plume with a night glow. Sulfur crust, steaming vents and ember drift say "Volcanic" with no HUD |
| **Elden Ring**, Mountaintops / Consecrated Snowfield | Field ruins and minor erdtrees mark the land at every scale. Ruins tell a war through what's left: dead soldiers facing one way, broken carriages. Grace points glow at night | Story sites say who died, which way they faced and what they carried: the caravan guards' weapons face the pass, the miners fall facing the shore |
| **Elden Ring**, Caelid | One colour law (red sky, rot) makes the whole region read as dangerous. Its smaller landmarks (Sellia, Redmane Castle) silhouette against the sky | Each sub-zone has its own palette and one hero shape (Bellows Stacks, Glasswall, Tusk Gate, Great Inuksuk) |
| **Ghost of Tsushima** | No HUD map trail: the Guiding Wind, birds and smoke columns guide you | The slag-cairn road and the inuksuk road guide you. Each inuksuk's arm points at the next one; the cairns' ember cups light as you pass at night |
| **Red Dead Redemption 2** | Points of interest are small stories (a hanged man, a cabin with a diary) seen from the trail. The world is dense with the kind of detail you see in passing | Kit POIs are 10-30 m mini-stories (a smith's grave with a hammer head, a Rime-folk tent ring with a cold hearth) placed along the routes players actually walk (`island_walk.json`) |
| **Monster Hunter World**, Elder's Recess / Hoarfrost Reach | Environmental hazards are fixed and learnable: steam vents launch, ice drops from cliffs, thin ice breaks under monsters. Each has a clear tell | Fumaroles and geysers on a fixed clock with a 1 s hiss before. Serac fall with 1.2 s of cracking and snow dust first. Thin ice cracks in three audible stages |
| **Valheim**, Mountains / Ashlands | Gear-check biomes with no meter in Mountains (frost resistance). Ashlands' landmarks (charred fortresses, lava rivers) carry its lore | Hazards stay gear and skill checks. Lava is a lethal river, and ice-resist (frost path or infused armour) cuts the katabatic push |
| **Horizon Zero Dawn: The Frozen Wilds**, the Cut | Geothermal vents in a snow region make the borderland interesting: steam at the edge of ice | Steaming ground on the tundra side of the south pass, where Fire's heat leaks through the ridge (the borderland story) |
| **The Long Dark** | Wind is a character: gusts push you and the sound warns first. Landmarks (radio tower, dam) are the only way to navigate in fog | The katabatic gust: a 1 s rising howl, then a push that crouching halves. Landmarks are lit, so they read in a whiteout |
| **Death Stranding** | Terrain is the puzzle: crevasses, rivers and steep scree are read before they're crossed | The Crevasse Maze: lanes between seracs, snow bridges that hold for 0.9 s, poles marking the safe line |
| **Skyrim**, the Throat of the World / Nordic barrows | Place stories are told by placement (a skeleton with a journal, a bear in a cave with bones). Pine forests at night are dark and quiet | The Pale Pines carry no light at all: they're the one dark spot in T2, and that's its threat |
| **Dragon's Dogma 2** | Density beats size: something every 100-200 m. Roads carry the danger | The 150 m spacing, and gap fill along walk routes |

### 1.2 The real world

| Reference | Fact | Use |
|---|---|---|
| **Hornitos** (USGS, swisseduc Stromboli glossary) | Rootless spatter cones built from clots thrown up through a skylight in a tube or flow roof; 1-10 m, sometimes 20 m chimneys. Their walls are welded spatter, lumpy and layered | The Bellows Stacks are three 18-22 m hornito chimneys of stacked spatter rings, each ring offset and lumpy. Hornito Row: 7 small cones (2-4 m) along the lava edge |
| **Pāhoehoe / ʻaʻā** (NPS) | Ropy, smooth, glassy-black skin vs. clinker rubble; a flow's edge cools to a rubble levee | The lava river's banks get rubble levees (basalt clinker) at the hornito row |
| **Tree molds** (NPS Craters of the Moon) | A trunk-shaped hole, 0.3-1 m across, with bark-pattern walls left by chilled lava | Tree-Mold Field: 18 vertical moulds (dark rings with a raised chilled rim), one holding a miner's bones and pack |
| **Kawah Ijen** | Sulfur at 600 °C burns blue (visible only at night). Miners carry 70-90 kg baskets on shoulder poles. A ceramic pipe condenses the sulfur, which cools yellow-orange | The Blue Fire: vent pits with blue flame at night only, yellow crust round them, the miners' bamboo-style shoulder poles and baskets, and a condensing pipe of stacked clay sections dripping orange |
| **Acid crater lakes** (Ijen, pH ~0.3) | Milky turquoise water, a yellow sulfur rim, dead vegetation | The acid pool: milky teal (#4FB8A8) in a hollow, with a yellow crust rim, dead birds and a rusted cart |
| **Basalt columns** (Giant's Causeway, Svartifoss) | Hexagonal columns 0.4-0.5 m across, cracked into colonnades | Basalt column rings (kit) built with the kit's `Hex` |
| **Obsidian** | Conchoidal fracture: curved shell-like faces with razor edges, black with a glassy sheen | Glasswall: tall faceted black glass slabs leaning outward with curved faces. Shard Field blades set point-up |
| **Fumaroles / geysers** (Yellowstone, Strokkur) | Strokkur erupts every 6-10 minutes: a turquoise dome bulges, then bursts 15-20 m high. A fumarole hisses steadily and huffs | The geyser: 1.0 s of gurgle and bubbling, then the burst lifts you 12 m. The fumarole: a steady wisp, then a 1 s rising hiss, then the column |
| **Seracs** (glacier glossary) | Ice towers between crossing crevasses that topple without warning, house-sized, blue at the break | The Weeping Seracs: a 50 m wall of 6-14 m blue ice towers. Fallen blocks lie at the foot (the history of earlier falls) |
| **Snow bridges** | A drift arching over a crevasse; strongest in the cold morning; roping up and probing poles find them | The Crevasse Maze's bridges hold 0.9 s after the first creak. The Rime-folk's poles (a bundle of willow staffs with red rags) mark the safe line |
| **Moulins** | A vertical meltwater shaft into the glacier, heard long before seen | The Moulin of Voices: a round blue shaft mouth with a low roar loop audible at 60 m (the Blue Below route starts here) |
| **Katabatic wind** | Dense cold air pours downhill off an ice cap in gusts, strongest at the edge of the plateau | The gust zone is T5's ring round the glacier bowl. It pushes downhill (the ground's own slope, found by traces) |
| **Pingos** | Ice-cored mounds 3-70 m tall; the top cracks open (a star-shaped crater) as the core grows | The Fox Circle Pingo: a 10 m snow mound with a star crack on top and an exposed ice flank |
| **Inuksuit** (Inuit) | Stacked stone figures. "In the capacity of a human" marks routes, caches and good fishing. Some have an arm pointing the way | The inuksuk road: each one stands at a bend and its arm (the long top lintel) points at the next one |
| **Sámi lavvu / árran** | Cone tent of poles round a central hearth ring of stones. Hides over poles; a smoke flap at the top | Rime-folk tent rings (kit): 8-10 stone hearth ring, pole stubs in a 4 m circle, a collapsed hide |
| **Viking ships under ice** (the Oseberg ship's clinker build) | Overlapping planks (strakes) fastened with rivets, a keel with a raised prow and stern | The Frozen Whaler: a clinker hull of 7 strakes a side, a raised prow, a mast stub with the crow's nest, locked in a serac |
| **Mammoth ivory finds** (Siberia) | Tusks curve in a spiral, up to 4 m, yellow-brown, cracked in rings | The Tusk Gate and the Tusk Midden: tusks built from ringed segments in a spiral sweep, yellowed ivory (#D8C9A3) |
| **Masonry** | Dry-stone cairns: wide base, through-stones, each course stepped in; slag is glassy, bubbly, rust-brown and black | Slag cairns: a 4-course stack with a glassy-black slag cap and an iron ember cup |

---

## 2. Grade: what's built against the bar

| # | Built thing | Where (proof) | Grade | Why | Raise |
|---|---|---|---|---|---|
| B1 | Island terrain (4 km, ridge, two sides, passes) | `L_Island`, `Art/Terrain/island_height.r16`, `Tools/terrain/build_island.ps1` | Meets | Readable two-sided island; the ridge is the triangle that hides each side | None (map files untouched) |
| B2 | Dressing: trees, rocks, shrubs, cliffs | `WRWorldDressing.cpp:493-534` | Close | Dense and gatherable. Volcanic shrubs are green (`:507` comment: the coat hung the GPU) | §3.7 G22 swap |
| B3 | POIs | `Data/island_dressing.json` `pois` (20 on the F/F frame); shrines, villages, stones from world-systems | Below | 20 + 10 shrines + 2 villages on ~10 km²: one per ~300 m. The bar is 150-250 m | §3.3-3.6 |
| B4 | Sub-zones | `Data/spawn_regions.json` (low/mid/high bands only) | Below | No named sub-zones, no ground or bed per sub-zone | §3.2 |
| B5 | Environmental story | none on L_Island before world-systems | Below | The old war isn't told anywhere | §3.3, §3.4 |
| B6 | Lava | `WRWorldDressing.cpp:927` `BuildLavaFlow`, `M_Lava`, `tuning hazards.lavaDps 8` | Close | Looks right; too forgiving | §3.7 R17 |
| B7 | Ice sheet + slick ice | `WRWorldDressing.cpp:958` `BuildIceSheet`, `hazards.iceFriction` | Meets | Gear check works | Thin ice in §3.5 adds the second ice rule |
| B8 | Micro-hazards | none (`grep fumarole/geyser/serac/katabatic` → none in Source) | Below | Each biome has one hazard | §3.5 |
| B9 | Sky: day/night, moon, biome air, valley mist, exposure | `WRSky.cpp:325-790` | Meets | Warm ash west, cold east, real moon | Crater plume and forge glow (§3.8) |
| B10 | Crater | Vulkarn's cavern; no smoke except during the Eruption (`WRWorldSystems.cpp:1724`) | Below | A volcano with no plume reads as a hill | §3.8 |
| B11 | Sea | `WRSea.cpp`, `tuning sea` (WaterMaterials ocean, shore squares, night dim) | Meets | Shore foam, depth colour, night dim | Grade only: no change |
| B12 | Caves | `WRWorldDressing.cpp:1429` `AWRCave` | Meets | Sculpted shells with contents | None |
| B13 | Map markers for places | `WRUIMap.cpp:349` (`FWRPoi::All`, fill-in reveal) | Meets | Reveal works; landmarks can join it | Add landmarks (§3.6) |

---

## 3. Design

### 3.1 Rules for every place (GAP-COOKED)

- **Built at runtime from data** (the dressing pattern): `Data/island_places.json` lists every place: kind, x, y,
  facing, and where needed a target to face or a span. Every machine builds the same look locally. The server also
  builds the collision and runs the hazards. `L_Island` itself isn't touched.
- **Ground:** every part is laid on the ground found by tracing down onto the landscape. A place is built once the
  ground under its centre has streamed in, so a client far from it builds it when it arrives. Nothing floats: posts
  are sunk 30-60 cm, slabs follow the slope and stack stones sit on the one below.
- **Clear of systems:** a place keeps 30 m from a boss arena's centre (mini) or 40 m (major), 15 m from a shrine or
  a summoning stone, and stays out of lava cells (the builder checks `FWRBiomeHazards::KindAt` and nudges outward).
- **Collision:** boxes where you walk on or are stopped (bridge decks, walls, big bones, tower floors). Small things
  are look only, so they never snag the camera or a charge.
- **Night:** every place has either a light (a warm brazier, an ember cup, a lamp, the blue fire) or a skyline shape
  big enough to read against the moonlit sky. Lights are on from dusk (daylight < 0.25) to dawn, faded over 4 s; the
  sky's clock drives them on every machine.
- **Culling:** landmarks draw to 5 km (the whole island). Story sites draw to 600 m, kit POIs to 400 m.

### 3.2 Sub-zones (fire-frost 3.3.1 / 3.4.1, adjusted to the real island)

The island's heights put the coast at about x ±1,600 and the ridge at x ≈ 0 (peaks of 800 m). The lava river runs
west-east at y ≈ +850 to +950 between x −1,500 and −700. The ice sheet covers the north-east lakes (x 400-1,500,
y −1,100 to −500), the glacier bowl (around 804, 0) and the southern hollows. The sub-zones follow that ground.

| Id | Name | Shape (metres) | Ground cue we add | Ambient bed (sounds.json `ambient`) |
|---|---|---|---|---|
| V1 | The Ash Plains | x < −1,250 (coast band) | dead ash scrub (§3.7), slag cairns, charred caravan | `ff.V1`: wind over ash + far crater rumble |
| V2 | The Lava Fields | x −1,250 to −500, y > +450 | the river, hornitos, Fallen Smith, tree molds | `ff.V2`: lava gurgle and crack |
| V3 | The Obsidian Ridges | x −1,250 to −800, y < −350 | Glasswall, shard field, glass blades | `ff.V3`: wind whistle on glass edges |
| V4 | The Solfatara | x −800 to −150, y < −350 | sulfur crust, fumaroles, geysers, the Blue Fire, the acid pool | `ff.V4`: hiss and bubbling |
| V5 | The Crater Highlands | within 600 m of the crater (−786, 0) not in V2-V4 | crater stair, plume, skylights | `ff.V5`: low crater roar |
| T1 | The Snow Flats | x > +1,250 | inuksuk road, sledges, tent rings | `ff.T1`: steady wind + snow hiss |
| T2 | The Pine Hollows | x +250 to +1,250, y > +450 | Pale Pines, hunter's blind, giant pine | `ff.T2`: creaking trunks, owls at night |
| T3 | The Frozen Lakes | x +250 to +1,250, y < −550 | thin ice, ice-fishing camp, drowned sledge | `ff.T3`: lake-ice booming and pinging |
| T4 | The Ice Shelves | x +250 to +700, y −550 to +450 | Weeping Seracs, Crevasse Maze, Frozen Whaler, Moulin | `ff.T4`: ice groans + meltwater |
| T5 | The Glacier Plateau | within 450 m of the bowl (804, 0) | katabatic gusts, claw scars, Great Inuksuk | `ff.T5`: katabatic howl |

**Entering a sub-zone:** its name fades in under the compass for 3 s in the place-name style (Cinzel caps, the
biome's colour), once each time you cross into it, not more than once every 60 s per zone. It's client-side from
the zone test, so it costs no network. The zone shapes are rectangles and rings in data (`zones`), tested in the
order listed (rings first, as they overlap rectangles).

### 3.3 Volcanic places

#### 3.3.1 Landmarks (read from 1 km)

| Landmark | At | What it is (composition) | Why here / meets | Night read |
|---|---|---|---|---|
| **The Bellows Stacks** | −620, −760 (V4) | Three hornito chimneys 22, 18 and 20 m tall in a 14 m triangle. Each is 16-22 lumpy spatter rings (basalt, rough, offset 10-25 cm and tilting 2-4°), narrowing from 6 m at the base to 2.2 m at the throat, with a slag skirt at the foot. Their throats glow. A smith-built stone vent arch at each base (3 courses of dressed basalt, a keystone) shows the smiths once worked them as furnace bellows | The Solfatara's hero shape, seen from the north pass and Emberwick. The arches face the miners' road | Each throat **breathes** in turn (6 s apart, a 2.4 s cycle): ember spray and a flame tongue rise 6-9 m with a warm light swelling from 0 to 9,000 and easing back. Smooth, clock-driven, the same on every machine |
| **The Fallen Smith** | −900, +1,050 (V2), the hammer haft spanning the river northward | A forge-giant skeleton 46 m long, half-sunk in ʻaʻā. Skull (5 m) split by a frost crack with blue-black Rime spearheads in it; spine of 18 vertebrae; 9 rib pairs arching 6-8 m (the inner ribs broken); one arm reaching north with the **hammer**: a 4 m iron head sunk in the far bank and a 26 m haft (1.3 m thick, iron-banded every 3 m) across the lava as a walkable bridge | The old war's first chapter: the forge giant killed by frost. The bridge is the only dry crossing of the river west of the crater, so Cinderhorn's loop and the slag-cairn road both use it | Lava glow under the ribs (two point lights at the river's edge, orange, flickering), hot iron bands on the haft |
| **Glasswall** | −1,180, −960 (V3) | A 46 m long obsidian cliff, 18-26 m tall: 15 black-glass slabs, each 3-5 m wide, leaning out 4-9°, with curved conchoidal faces (two offset slabs per face) and razor rims. A ledge 9 m up holds the Mirror Room niche (a 3 m arch cut into the glass) | V3's hero; the Glasswall shrine is 60 m south. The geyser at its foot lifts you to the ledge (§3.5) | The glass catches the moon (low roughness), and a faint ember cup glows in the niche |
| **The Blue Fire** | −430, −1,160 (V4) | 5 vent pits (1.5-2.5 m) on a yellow sulfur crust 30 m across; a condensing pipe (8 clay sections, 0.5 m) runs from the biggest vent down to a collecting pan where orange sulfur drips and cools to yellow | The miners' reason for being here (Ijen) | **Blue flames** at night only (blue fire fx + blue point lights #3A6CFF), the only blue in the Volcanic |
| **The Slag Light** | the west headland, about −1,550, +120 (V1) | A ruined smith beacon tower 16 m tall: 4 tapering drums of dressed basalt, a spiral stair of 22 cantilevered steps round the outside, a collapsed top course, an iron fire-basket on the top | The sea's landmark: seen from the coast road and the sea | Dark until the **Kipuka Shrine** is kindled; then lit for good (a big brazier, warm light 20,000 over 30 m). The tower reads whether the land around it is held |
| **Ash Pine Kipuka** | −1,280, +620 (V2 edge) | 32 living pines (the island's pine meshes) and 40 green shrubs in a 110 m ring of old ground at the lava field's edge, with glowmoth cocoons in the lower branches | Real green, kept only where it's real (R19). The Kipuka Shrine (−1,150, 650) is at its east edge | 12 glowmoth lights (soft green-white, 400 lux, 6 m) pulsing slowly in the canopy |
| **Wisp Spire** | 40 m north of Ignessa's site, −1,036, +744 | A 24 m spire of fused rock: 9 tapering basalt drums twisted 7° each, wrapped by 3 glassy ribs that spiral up, with a split crown | Ignessa's arena backdrop, kept outside her 26 m arena | A halo of rising wisps at the crown (`wisp.halo` fx) with a pale gold-white light |
| **Vulkarn's Crater** (plume) | −786, 0 | Not a mesh: a permanent smoke column from the crater (`crater.plume` fx, 60 s life sprites rising 300 m and drifting with the wind) | The island's compass | An orange underglow (light 40,000, 80 m) at the rim at night |

#### 3.3.2 Story sites

| Site | At | What's there (the story without words) | Why here / meets |
|---|---|---|---|
| **The Plough Trench** | along Cinderhorn's loop, from −1,490, +440 to −1,430, +660 | A 230 m furrow: two ash berms 1.2 m high either side of a shallow ploughed strip (boxes laid along the line, following the ground); a smashed giant iron plough (a 7 m share, a beam snapped in two) at the north end; a burnt yoke-maker's hut (4 charred posts, a fallen roof, a rack of half-shaped yokes) at the south end | Cinderhorn's lair and loop (creatures.json `roam`): the bull ploughs his own trench, and the basalt pillars he crashes into stand along it |
| **The Miners' Grave** | −470, −1,110 | 12 sulfur baskets on shoulder poles lying in a line toward the shore (south-west); ash mounds the shape of bodies beside each; one pole still upright in the ash | Kawah Ijen 1976: they died fleeing |
| **The Miners' Camp** | −380, −1,240 | Two lean-to tents (poles, canvas), a cookfire ring with a pot on a tripod, a tally board (a plank with 30 notches burned in), stacked empty baskets | The life before the grave; its fire is lit at night (someone still comes here) |
| **The Charred Caravan** | −1,450, −700 | Three burnt smith wagons (beds, wheels with spokes, one tipped). The guards' spears and swords lie pointing **east toward the ridge** | Something came over the pass: the tundra's first raid in the old war |
| **The Tree-Mold Field** | −1,100, +500 | 18 tree molds (dark holes with chilled lava rims), one with a miner's bones and pack | The lava came through a living forest: the Kipuka is what's left of it |
| **Hornito Row** | 7 cones along the lava's north bank, from −1,350 to −850, about +820 | 2-4 m spatter cones, each puffing embers in turn | Shows the flow is alive under its crust |
| **The Crater Stair** | from −1,050, −120 up toward the crater | 36 smith-cut basalt steps laid up the slope in 3 flights, broken in two places (missing steps); two headless smith statues flank the foot | The way to Vulkarn; the smiths' road |
| **The Warden's Quarry** | round the Obsidian Warden (−1,066, −716), 32-40 m out | Half-cut obsidian blocks with chisel grooves; a half-carved second golem lying on its back; a stone sledge on log rollers | Where the Warden was made |
| **The Shard Field** | −1,350, −1,100 | 60 obsidian blades set point-up in the ground over 80 m, with two clear paths through | V3's danger read; cosmetic |
| **The Slag-Cairn Road** | 13 cairns from Volcanic camp 2 (−1,016, +1,294) by the Hammer Bridge, the Kipuka, the Tree-Mold Field, the Crater Stair foot and Emberwick to the Charred Caravan | 4-course dry-stone cairns 1.6-2.0 m with a slag cap and an iron ember cup | Wayfinding: each cairn is in sight of the next. The cup lights when you pass within 30 m at night and burns 10 minutes for you |
| **The Ridge Watch** | the ridge's west flank at about −380, −620 | A square smith watchtower (4 m, 9 m tall, two floors, a ladder) with arrow slits facing the north pass | The smiths watched the pass |
| **The Kilnmite Hill** | −1,350, −1,300 | A 6 m mound of chewed ore with 4 egg-chamber mouths ringed with crusted copper | The Kilnmites' home ground |

### 3.4 Tundra places

#### 3.4.1 Landmarks

| Landmark | At | What it is | Why here / meets | Night read |
|---|---|---|---|---|
| **The Tusk Gate** | on Rimehold's west road, 70 m west of the village centre | Two pairs of 12 m mammoth tusks (ringed ivory segments sweeping in a spiral) set in stone sockets, crossing overhead at 9 m and lashed with hide; four lamps hang from the crossing | The Rime-folk's front door, facing the glacier | 4 hanging lamps (warm, 2,500 each) |
| **The Great Inuksuk** | 1,150, +120 | A 9 m stone figure: 2 leg stacks, a hip lintel, a body stack, an arm lintel **pointing at the glacier bowl** (804, 0), a head stone | The Rime-folk road's centre: from it the glacier is straight ahead | A fire bowl at its feet (lit at night) |
| **The Frozen Whaler** | 520, −420 | A clinker longboat (19 m, 7 strakes a side, a raised prow) locked into a 14 m serac, its prow 11 m up, its stern buried; a mast stub with a crow's nest | Rime-folk sailed here when the ice was sea; the Blue Below route starts beside it | A lantern left in the crow's nest |
| **The Weeping Seracs** | 640, +250 | A 50 m wall of 11 blue ice towers 6-14 m tall, leaning; fallen blocks scattered at the foot; meltwater drips | T4's hero. The serac fall hazard (§3.5) lives here | Faint blue glow in the cracks |
| **The Fox Circle Pingo** | 1,450, +900 | A 10 m ice-cored mound (stacked snow terraces), a star-shaped crack in the top showing blue ice, one flank broken open to clear ice | Aurora foxes gather here (the whiteout's aurora night) | Blue ice core light |
| **The Pale Pines** | round the Pale Hunter's site (1,034, +834), 34-60 m out | 18 dead white pines (bone-white trunks, stripped, broken stubs of branches, a few gnaw-scarred) | The one dark place in T2 | **None.** Its threat is the dark |
| **The Ice Arch** | spanning Grimjaw's den (1,064, −766) | A natural ice bridge: two ice buttresses 36 m apart outside the arena, a 6 m thick arch of ice blocks peaking at 13 m | The troll under the bridge; the arena stays open beneath | Blue glow inside the ice |

#### 3.4.2 Story sites

| Site | At | What's there | Why here / meets |
|---|---|---|---|
| **Old Tusker's Wallow** | the middle of his loop, about 1,060, +300 | A 25 m trampled ring (churned snow mounds), 5 broken spear shafts standing at angles, tusk gouges (long dark grooves), a heap of chewed lichen-rock | His lair; he walks through it on every loop |
| **The Tusk Midden** | 1,450, −900 | 14 tusks stacked in a frozen bank, the bank's face exposed | Where the Rime-folk took the ivory for the gate |
| **The Sledge Wreck Line** | 1,450, +300 → coast | 3 sledges in a line (runners, slats, cut harness straps lying flat), fur bundles | They ran for the sea; the straps were cut, not torn |
| **The Inuksuk Road** | 12 inuksuit from Tundra camp 3 (1,112, +1,294) by Rimehold and the Great Inuksuk to the lakes and Tundra camp 2 (1,140, −1,296) | 1.5-2 m stacked figures, each arm pointing at the next | Wayfinding; lamp bowls light when you pass at night |
| **The Ice-Fishing Camp** | 760, −1,000 on the lake ice | 3 round holes in the ice (dark water discs with rims), a fish drying rack, a windbreak of hides, a cold fire | Life on the lakes |
| **The Drowned Sledge** | 1,250, −850 on the lake ice | A sledge and 4 dog shapes seen under a clear ice window | Points at the thin ice: read the colour |
| **The Pale Hunter's Camp** | 1,000, +900 (outside the arena) | One tent, a hunter's boots standing upright at the flap facing out; prints leading out turn to claw prints after 6 steps; gnawed bones in the fire ring | The Wendigo story |
| **The Hunter's Blind** | 700, +1,300 | A hide blind of poles and branches over a game trail, a bow rack | T2's hunting |
| **The Fallen Giant Pine** | 600, +700 | A 34 m trunk lying across the hollow with its root plate standing 6 m tall; walkable | A natural bridge |
| **The Glacimaw Boneyard** | 380, +480 | A ring of 20 ice-polished bones round churned snow | Where it surfaces |
| **The Wyrm Claw Scars** | 650, −300 | 4 gouges 6 m long down a rock face (dark grooves with ice in them) | Skaldrith lands here |
| **The Watch Cairn** | 330, −700 | A Rime-folk watch post (a stone ring wall 1.2 m high, a spear rack facing west) | They watched the north pass too |
| **The Moulin of Voices** | 560, −330 | A 4 m round shaft mouth of blue ice (a ring of ice blocks with a dark centre), meltwater channels running in | The Blue Below's route starts here; roar loop at 60 m |
| **The Crevasse Maze** | centred 450, 0 | 5 lanes between rows of serac blocks (3-5 m tall) over 120 m; snow bridges across the gaps on top; 3 lines of Rime-folk poles (willow staffs with red rags) mark the safe line along the tops | A puzzle on the way to the Crevasse Shrine (450, 60) |
| **The Steam Ground** | 150, +1,250 (south pass, tundra side) | Bare dark earth in the snow, 6 fumarole vents breathing steam | Where Fire leaks under the ridge: the borderland |

### 3.5 Micro-hazards (fire-frost 3.3.4 / 3.4.4)

All server-authoritative. Every timed hazard runs on the **server's world clock** (`GetServerWorldTimeSeconds`), each
with its own phase from a hash of its position. Every machine draws the tell and the burst at the same moment without
any replication, and only the server deals damage (`UWRCombatLibrary::ApplyHit`, environment source). Hazards that
react to players (snow bridges, thin ice) are replicated actors with a state byte and the server time of the change.

| Hazard | Where | Tell (≥ 0.9 s) | Effect | Counter | Tuning (`hazards.ff.*`) |
|---|---|---|---|---|---|
| **Fumarole** | V4 vent fields, the Bellows Stacks' feet, Steam Ground, every second sulfur-vent kit POI | A steady wisp, then 1.0 s rising hiss (sound) + a swelling steam puff | A 4 m wide column for 1.2 s: 35 damage, a 600 cm knock-up, no burn | Walk out during the hiss | `fumarole {interval:[30,50], tell:1.0, radius:200, damage:35, launch:600, seconds:1.2}` |
| **Geyser** | 4 in the Solfatara; 1 at Glasswall's foot (rides to the Mirror Room ledge) | 1.0 s gurgle + water bubbling at the mouth | 1,200 cm/s up (to about 12 m), 10 damage | Ride it on purpose | `geyser {interval:40, tell:1.0, radius:150, launch:1200, damage:10, seconds:1.6}` |
| **Acid pool** | V4, 1 pool in the hollow south of the Blue Fire | Teal water + fizz loop | Wading: 6 damage/s and 2 durability/s off worn gear | Don't wade | `acid {dps:6, wear:2, tick:0.5}` |
| **Thin ice** | T3: 7 dark patches on the frozen lakes (one under the Drowned Sledge's line) | Crack 1 at the first step, crack 2 at 0.6 s, crack 3 at 1.2 s: louder each time, with white crack lines spreading | It breaks when the weight on it reaches 300 (player 100, a mount 300, a creature by its size) for 1.2 s, or at once on a heavy landing (fall over 4 m). Inside the hole: icy water, with 25/s frost buildup and the slowed status. It refreezes after 90 s | Read the colour; spread out | `thinIce {weight:300, player:100, mount:300, stages:[0,0.6,1.2], frost:25, refreeze:90}` |
| **Snow bridge** | the Crevasse Maze (9 bridges) | A creak + falling snow trickle at the first step | Collapses 0.9 s after first weight: fall 4-5 m into the lane below. Reforms after 30 s | Walk the pole line (the poles mark the solid tops) | `snowBridge {collapse:0.9, reform:30}` |
| **Serac fall** | the Weeping Seracs (6 drop points at the foot) | 1.2 s: deep crack + falling snow dust from the top | A block lands: 4 m radius, 60 damage, unblockable | Stay out from under them | `serac {interval:[35,70], tell:1.2, radius:400, damage:60}` |
| **Katabatic gust** | T5: 250-450 m round the bowl | 1.0 s rising howl + snow streaming downhill along the ground | 700 cm/s downhill push for 0.6 s, every 10-18 s | Crouch halves it; ice resistance halves it | `katabatic {push:700, seconds:0.6, interval:[10,18], tell:1.0, inner:25000, outer:45000}` |

**Edge cases:**
- A hazard is never inside a spawn safe zone, a shrine's 15 m, or a boss arena.
- Downed players are skipped.
- A geyser's launch and the gust run on the owning client too (the same clock), so the push doesn't rubber-band.
- Tames and mounts take fumarole and serac damage (they're characters).
- Thin ice counts creatures standing on it, and a mounted player counts as the mount's weight.

### 3.6 Kit POIs and the generator

`Data/island_places.json` `kit` rules: per biome, the kinds and their weights. The generator (C++, at build, the same
seed on every machine) does this:
1. Takes all hand places, camps, caves, shrines, villages and arenas as taken points.
2. Walks the walk-map routes (`island_walk.json`) and puts a kit POI wherever a route point is more than 125 m from
   any taken point, so no stretch of road goes 250 m without a place.
3. Fills each biome with Poisson darts (150 m apart) up to its count.
4. Rejects sea (ground under 3 m), lava cells, steep ground (slope over 0.45), and the ridge crest band (|x| under 250).

| Kind | Biome | What it is | Night |
|---|---|---|---|
| `sulfur_vent` | V4, V2 | 3-4 vent mouths on yellow crust; every other one has a fumarole | blue flicker at night |
| `hornito` | V2, V4 | A 3-5 m spatter cone puffing embers | throat glow |
| `smith_grave` | V1-V3 | A slag mound with an iron hammer head on a stake (a sword in 1 of 3) | none (skyline stake) |
| `giant_tool` | V2, V5 | A giant's tongs, rasp or chisel 6-9 m long, half sunk | none (silhouette) |
| `glowmoth_roost` | V1, V2 | A dead tree with 6-9 cocoons | soft green-white glow |
| `basalt_ring` | V1, V2 | 7-11 hexagonal columns in a ring, 1.5-4 m | none (silhouette) |
| `miner_leanto` | V1, V4 | A lean-to, a basket, a tool rack, a fire ring | a small fire |
| `tent_ring` | T1, T2 | A Rime-folk hearth ring, pole stubs, a collapsed hide, a drying rack | none until passed (cold hearth) |
| `erratic` | T1, T4 | A house-sized boulder with a carved spiral on its face | none (silhouette) |
| `snowpuff_nest` | T1 | Grass-lined scrapes in a ring of stones with feathers | none |
| `cache_pole` | T1-T3 | A tall pole with a bundle hung out of reach of foxes, and a stone platform | a lamp hung at the top |
| `frozen_carcass` | all tundra | Ribs and a hide frozen into the snow | none |
| `fishing_hole` | T3 (on ice only) | A dark hole with a rim and a line on a stick | none |
| `small_pingo` | T1, T3 | A 4-6 m snow mound with a cracked top | none |

Landmarks (not kit) join `FWRPoi::All` as kind `landmark`, so they appear on the fill-in map once revealed, like the
camps. Kit POIs don't, so they don't crowd the compass.

### 3.7 Lava, ice and dressing raises

- **R17:** `hazards.lavaDps` 8 → 20, `hazards.lavaBurnPerSecond` 12 → 20. An unprotected 100 HP player dies in about
  4 s. Lava resistance is unchanged (Burning blood, fire-infused armour, a mount).
- **R18 / G22:** `ember_shrub` draws the char-tree meshes at shrub size (a dead, leafless charred scrub 1.6-2.4 m)
  in place of the green GV shrubs. No coat material, so nothing new touches the Nanite cull that hung the GPU. It's
  still a fibre harvest. The Kipuka's green comes from its own shrubs and pines (R19).

### 3.8 Sky and sea

- **Crater plume:** a permanent `crater.plume` fx at the crater: slow, tall grey sprites, 300 m column, lean with
  `weather` wind. At night, an orange light at the rim reflects off the plume's underside. During the Eruption,
  world-systems' `eruption.plume` adds to it.
- **Sea:** meets the bar (B11). No change; the Slag Light gives the west coast its sea landmark.

### 3.9 Leftovers raised from LATER-COOKS (R30)

| Leftover | Answer here |
|---|---|
| machinery: "no rivers or streams, no steam vents" | Steam vents are built (fumaroles and the Steam Ground) and listed in `FWRMachines`? **No:** machines read lava only. Hooking geothermal turbines to fumaroles is a machinery-lane change; logged to LATER-COOKS. Rivers need a terrain rebuild (not this pass) |
| building: "tundra hot springs for the hot spring bath" | The Steam Ground (150, +1,250) is the tundra's warm ground; a hot-spring pool there would need the bath's placement rule changed (building lane). Logged |
| combat: "basalt pillars along Cinderhorn's roam; run-back stones" | Pillars are built (`WRBoss.cpp:244`); the Plough Trench now gives them a place and a story. Run-back stones exist (`WRBoss.cpp:977`) |
| world-systems: "steaming ford, Hekla's Mouth, lava-fall ledge" | Need a river and a waterfall: a terrain rebuild. Logged |

---

## 4. Plan: building it

### 4.1 Data

| File | Change |
|---|---|
| `Data/island_places.json` (new) | `zones` (10), `landmarks` (15), `sites` (27), `roads` (slag cairns, inuksuit), `hazards` (fumarole, geyser, acid, thin ice, snow bridge, serac, katabatic positions), `kit` (kinds, weights, counts, spacing), `beds` |
| `Data/tuning.json` | `hazards.lavaDps` 20, `hazards.lavaBurnPerSecond` 20; new `hazards.ff` block (§3.5); `places` block (cull distances, night threshold, cup minutes, clearances) |
| `Data/fx.json` | Rows: `ff.bellows`, `ff.fumarole.tell`, `ff.fumarole.burst`, `ff.geyser.tell`, `ff.geyser.burst`, `ff.blue_fire`, `ff.serac.dust`, `ff.serac.impact`, `ff.gust`, `ff.ice_crack`, `ff.ice_break`, `ff.snow_trickle`, `ff.wisp_halo`, `ff.glowmoth`, `crater.plume`, `ff.acid` |
| `Data/Audio/sounds.json` | Rows for each tell and burst plus the 10 `ff.<zone>` beds, made from the existing recordings and marked `_standin` where a new recording is wanted |

### 4.2 C++

| File | What |
|---|---|
| `WRIslandPlaces.h/.cpp` (new) | `UWRIslandPlacesSubsystem` (world subsystem: loads the file, holds zones, `ZoneAt`, `FWRPoi` landmarks); `AWRIslandPlaces` (one per machine, not replicated): pending sites built when their ground streams in (trace), with clock-driven landmark motion (Bellows breathing, night lights, cairn cups) |
| `WRIslandPlacesLook.cpp` (new) | The builders: each landmark, story site and kit kind composed from the look kit (`FKit`) plus pack meshes (rocks, pines, char trees) |
| `WRIslandHazards.h/.cpp` (new) | `AWRFFHazardField` (server ticks the clocked hazards, applies hits; clients draw tells); `AWRThinIce`, `AWRSnowBridge` (replicated state actors) |
| `WRWorldDressing.cpp` | `ember_shrub` art → char-tree meshes at shrub size |
| `WRAudioDirector.cpp` | `ff.<zone>` beds weighted by `WRIslandPlaces::ZoneAt` |
| `WRUIHud.cpp` | The sub-zone name fade under the compass |

### 4.3 Build steps (each a commit and push)

1. Doc (this file), committed and pushed.
2. Quick raises: lava 20/20, the G22 shrub swap, the crater plume. Data + small code.
3. Places frame: the subsystem, the builder actor, zones, the zone name on the HUD, beds, `FWRPoi` landmarks. Then
   the volcanic landmarks and sites.
4. Tundra landmarks and sites.
5. Kit POIs and the generator.
6. Micro-hazards.
7. Spot-check, CHANGELOG/HANDOFF, ledger proof, LANDED.

**Boot safety (R29):** every builder runs inside the places actor's tick, guarded (a missing file or pack mesh means
the place is skipped with a log line, never a crash). Nothing changes the map or the save format.

---

## 5. Build log

| Step | What landed | Commit |
|---|---|---|
| 1 | This doc: ledger, research, grades, plan | showcase-notes dcc28e3 |
| 2-6 | Places tool + data, `UWRIslandPlaces`, 15 landmarks, 26 story sites, 2 waymark roads, 13 kit kinds, sub-zones (beds + banner), landmark map entries, micro-hazards, lava 20/20, dead scrub, crater plume, fx/sound/tuning rows, CHANGELOG + HANDOFF | 987cbcd |
| 6b | Unity-build safety, Slag Light start state | 7013d11 |
| 7 | Sastrugi fields (ledger R22 had no proof, so it was built), CHANGELOG line for the unity fix | 45affb9 |

Numbers: 41 hand places, 24 waymarks, 16 sastrugi fields, 168 kit POIs, 8 thin-ice patches, 10 + 6 + 3 + 4 fumaroles (fields, Steam Ground,
Bellows, kit vents), 5 geysers, 4 serac drop points, 9 snow bridges, 1 acid pool. No new binary files (everything is
code, JSON and existing pack meshes).

## 6. Spot-check (reading the code against the plan and the bar)

| Found | Fix |
|---|---|
| Crevasse Maze: its tops were out of reach (4-5 m walls) and its 1.2 m gaps could just be jumped, so the bridges meant nothing | Gaps along the way now 4.4 m (too far to jump), lanes 1.2 m between columns, snow ramps onto the first row, the Rime-folk's pole line has ice bridges that hold (WRIslandPlacesLook2.cpp:659) |
| Ridge Watch: the door's look (a 4 m gap) and its collision (offset box) didn't match | 1.6 m door with jambs and a wall over it, collision the same |
| Slag Light: 75 cm steps (no walking up) ending against the parapet | 40 steps of 41 cm, arriving through the gap in the fallen top course |
| Slag Light: the brazier eased toward a stale default for 2 s before its first shrine check | starts dark (period/phase/timer zeroed) |
| Unity build: a file-scope `using WRIsleLook::Lin` clashed with WRLandSites' `Lin` | scoped to each namespace; dispatch inside the namespaces; generic helper names prefixed (Isle*, Hz*); verified by compiling the clean tree |
| Hazard references taken from a growing array | each `S.Hazard(...)` reference is used before the next hazard is added (checked every builder) |
| Late-built places on a client missing a bridge / ice state the server already sent | `BuildOne` applies `AWRIsleState`'s rows to the new hazards |

Against the bar: data-driven (places, kit, zones, hazards, numbers, fx, sounds); all blows through `ApplyHit`; server
authority with the people-reacting hazards replicated; nothing unfinished shown (every listed place is built). The stand-in
sound rows are marked `_standin` and named in the CHANGELOG.

**Not finished (honest):**
- Rivers, the steaming ford, the lava-fall ledge and Hekla's Mouth (terrain rebuild).
- The crevasses are serac lanes, not cuts in the ground.
- Thin ice opens onto drawn icy water: no swimming in it.
- Every new sound row and bed reuses existing recordings (`_standin`), and composed sub-zone beds are wanted.
- Nothing was played: compile only, per the lane's gates.

## 7. Decisions made tonight (questions for tomorrow)

- **Coordinates moved to the real ground.** Several fire-frost.md positions sit in the sea or on 60° slopes on the
  built island (the Slag Light at −1,880 is 250 m out to sea; the Acid Lake at −230, −980 is on the ridge face; the
  Ridge Watch at −180, −600 is a cliff). Each moved to the nearest ground that fits its story. Alternative: rebuild the
  terrain to fit the doc.
- **Crevasses as serac lanes.** The island is a heightfield and can't be cut at runtime, so the Crevasse Maze is
  built as lanes between raised serac rows. Snow bridges span the gaps on top, and falling drops you into a lane.
  Alternative: landscape holes in a terrain rebuild.
- **Thin ice opens onto "icy water" drawn in the hole.** There's no water body under the lake ice, so the hole is a
  dark water disc. Standing in it gives frost buildup and slowed, but there's no swimming. Alternative: a water body
  under the lakes (terrain + water lane).
