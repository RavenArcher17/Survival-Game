# The round island: layout for sign-off (phase 3, step 1)

Plan phase 3 ("One big world") starts by laying the island out to scale for Raven's OK, before any ground is built.
This is that layout. The numbers come from `Tools/terrain/round_island.mjs`, which writes
`Tools/terrain/round_island_layout.json` (every number the build scripts will use: coast, land outlines, borders,
roads, camps, places) and `round_island_map.svg` beside this file. To change the layout, change the script's inputs and
run it again: `node Tools/terrain/round_island.mjs`.

![The round island to scale](round_island_map.svg)

## What it follows

- **Raven's answers (11-18, the plan doc):** a round island with Hearthmoot in the middle; nine surface lands sized like
  Aaron's lands; 10 to 12 km across, every land scaled up by the same amount (about double the area); open, blended
  borders; easy at the coast, harder inland, guarded roads to the city, bosses off the roads; the sky islands over the
  north, the Crystal caves under the north-east, the Fungal caverns under the south-west; the Ocean all round; Wind,
  Spore and Arcane players wake inside their layer; each land's half of its old pass becomes its road's works (fort,
  champion, mouth shrine); the Old Island stays its own map.
- **Raven 11-19:** the first layout (straight slices meeting at the city) looked too much like a pie. From four sketches
  Raven picked: borders that wander, a rugged coast, roads that join, and Hearthmoot in **a green meadow with a lake on
  the Ocean side**, so no land narrows to a point at the city.
- **Aaron's land sizes** (`Docs/design/cooks/world-map.md` 2.2), times two.

## The lands

Clockwise from the north-west. Bearings are degrees clockwise from north (where each land meets the coast); the bands
are equal depths between the meadow's edge and the coast.

| Land | Path | Aaron's km² | Now km² | Coast | Coast length | Coast / midlands / inland km² | Shares a main road with |
|---|---|---|---|---|---|---|---|
| Storm peaks | Lightning | 5.5 | 11.0 | 279–323° | 4.2 km | 5.1 / 3.3 / 2.5 | Desert |
| Desert | Sun | 6.5 | 13.0 | 323–16° | 5.1 km | 6.0 / 4.4 / 2.6 | Storm peaks |
| Ancient forest | Nature | 7.0 | 14.0 | 16–73° | 5.5 km | 6.8 / 4.7 / 2.5 | Frozen tundra |
| Frozen tundra | Frost | 5.0 | 10.0 | 73–113° | 3.8 km | 4.4 / 3.3 / 2.2 | Ancient forest |
| Swamp | Blood | 4.0 | 8.0 | 113–146° | 3.3 km | 3.8 / 2.5 / 1.8 | Ocean & reefs |
| Ocean & reefs (shore) | Tide | 3.0 | 6.0 | 146–174° | 2.6 km | 3.0 / 1.9 / 1.1 | Swamp |
| Graveyard | Necromancy | 2.5 | 5.0 | 174–196° | 2.0 km | 2.2 / 1.8 / 1.1 | Wasteland, Volcanic |
| Corrupted wasteland | Void | 6.0 | 12.0 | 196–248° | 4.9 km | 5.8 / 4.1 / 2.2 | Graveyard, Volcanic |
| Volcanic | Fire | 5.0 | 10.0 | 248–279° | 3.1 km | 4.2 / 3.7 / 2.1 | Graveyard, Wasteland |

- **Size:** 89 km² of land plus Hearthmoot's meadow (6.6 km²): **10.9 km across**, the coast 5.0 to 6.2 km from the
  centre. The volcano cape is the widest point (12.4 km across).
- **Borders wander:** each border bends along ridges, rivers and valleys instead of running straight to the city; the
  build blends the two lands over about 200 m either side (trees thinning into sand, snow patches into marsh). Every
  land keeps exactly its size.
- **A rugged coast:** the Volcanic juts out as a volcano cape; the Ocean shore curves into a reef bay with islets across
  its mouth; a cliff cape with sea stacks below the Storm peaks; an icy headland on the Tundra; the Swamp's muddy delta;
  the Desert's long dune coast; a forest inlet. Nine islets in all.
- **Why this order:** the sky is thickest over Storm, Desert and Forest, so they take the north; the Crystal caves lie
  under Forest and Tundra, the Fungal caverns under Graveyard and Wasteland; the Ocean keeps its shore in the south, by
  the meadow's lake. Opposites face each other across the city: Sun and Tide, Nature and Necromancy, Fire and Frost,
  Lightning and Blood (and the Wasteland faces the Crystal caves, Void and Arcane).

## Roads, camps and each land's places

- **Three wake-up camps** on each land's coast; the middle one starts the land's road.
- **Each land's own road** runs in down the middle of its land (3.2 to 5.2 km), past its **mouth shrine** where it
  leaves the coast band and its **champion** where it enters the inland band, then turns to a **crossroads** it shares
  with its neighbours, about 700 m out from the meadow.
- **Four main roads** run from the crossroads across the meadow to the city's four gates, each with its **fort** at the
  meadow's edge: Storm peaks + Desert (north-west), Forest + Tundra (east), Swamp + Ocean shore (south-east, passing
  east of the lake), Graveyard + Wasteland + Volcanic (south-west).
- **Each major boss** sits deep inland and off the roads: the spot in its land's inland band farthest from every road,
  its land's borders and the other bosses (370 to 750 m from the nearest road).

## Hearthmoot and its meadow

- Walls **600 m across** on a low hill in the middle of **the meadow**: green, open, guarded ground about 2.9 km
  across with an uneven edge. It is the Commons: no wild creatures, fully safe, nothing owned inside (Raven's answers).
  The four main roads cross it to the gates.
- **The lake** (0.44 km²) sits in the meadow on the Ocean side, south of the walls. Its stream runs 4 km out through the
  Ocean shore into the reef bay.
- **Twelve quarters**, one per path, 30° each, in the ring's order so each land's quarter lies towards its land where
  it can: Lightning, Wind, Sun, Nature, Arcane, Frost, Blood, Tide, Necromancy, Spore, Void, Fire. Wind's, Arcane's and
  Spore's quarters sit between the lands their layers lie over or under. Each quarter opens when its land's boss falls.

## The layers

| Layer | Where | Size | Ways in |
|---|---|---|---|
| Sky islands | Over Storm, Desert and Forest (303–43°), 1.9 to 3.9 km from the centre, 500 to 1,300 m up; a few over other lands | 4 km² of islands in ~10 km² of air | Updrafts, gliding, flying mounts |
| Crystal caves | Under Forest and Tundra (23–108°), 1.75 to about 3.6 km from the centre (inland band into the midlands) | 8 km² | The Severance (a rift-cut gorge on the Forest–Tundra border), crystal rifts |
| Fungal caverns | Under Graveyard and Wasteland, reaching under the edges of the Ocean shore and the Volcanic (168–247°), 1.75 to about 4.4 km from the centre | 12 km² | Cave mouths, sinkholes; the Flue rises from its floor to the surface, a sky island anchored over it |

- **Bigger caverns (Raven 11-19):** both are twice the size they were (Aaron's 2 and 3 km², x4), with uneven outlines and
  rounded ends instead of straight-edged slices, kept at least 400 m in from the coast.

- **Starts inside the layers (Raven Q18):** Wind on a low starter sky island over the north coast (ropes, ledges and
  updrafts down); Spore in a shallow cavern under the south-west coast; Arcane in a shallow crystal hall under the
  north-east coast. No markings out of the two caverns.
- No sea or rain inside caverns; a dim glow that pulses with the day above.

## The sea

Calm shallows 300 m out from every coast (by the camps), reefs out to 1.2 km, then deep water; Thalassor's waters past
the reef bay south of the Ocean's shore; the level is 16 km square. Open sea counts as Ocean & reefs.

## After the OK

The build goes land by land, each checked against its land's design and its landmark checklist from the same camera
spots, with pictures for Raven at each land. Proposed first: Hearthmoot with its meadow, lake and main roads, then
Volcanic and Frozen tundra (their content and creatures exist on the Old Island, so the method is proven on known
ground), then the rest round the ring.

## Slices 2 and 3: the Volcanic and the Frozen tundra (from 11-19)

Step 1 (this): both lands laid out and their ground built. Step 2: their content and creatures brought over, then the
alive-and-explorable pass, as slice 1.

- **The Old Island's halves at true proportions:** `Tools/terrain/fire_frost.mjs` maps every Old Island point onto its
  wedge. It keeps its distance in from its own coast (x 1.1) and its place north or south of the half's middle (x 0.75,
  as arc round the wedge's middle bearing). The old ground fills the outer ~2 km of each wedge; the new inner ground
  past the old ridge is a sub-zone of its own, **V6 the Cinder Terraces** and **T6 the Thaw**. Zone edges are looked up
  through a two-scale warp (`warpXY`) and the inner line wanders at km scale (`beyondAt`), so no edge runs straight.
  It writes `fire_frost_layout.json` (every place, site, cave, shrine, village and boss at its round-island spot, the
  lava river, the Smith's crossing) and the review map `Docs/design/fire_frost_map.svg`. Hand moves (`NUDGE`): Ignessa
  and the Pale Hunter off the roads, the Fallen Smith at the crossing.
- **Roads:** `round_island.mjs` `ROAD_VIA` routes the Volcanic's road round the volcano and the Tundra's down the
  land's middle past Rimehold; a land's mouth shrine and champion stand at least 80 m inside its own land.
- **The ground** (`round_ground.mjs` `landV` / `landT`, run order unchanged: `round_island.mjs`, `fire_frost.mjs`,
  `round_ground.mjs`, `hearthmoot.mjs`):
  - V1 ash dunes and black sand beaches; V2 lava flow sheets with steep fronts and tumuli; V3 sharp obsidian ridges;
    V4 a sunken solfatara basin with fumarole mounds, sulfur and rust in patches on pale steaming ground; V5 the
    volcano (a crater with a steep inner wall, a flat floor for Vulkarn and a walkable breach toward the crater stair;
    radial gullies down its flanks); V6 crisp terraces stepping down to the meadow; ten cinder cones on the flanks, the
    lava fields and the terraces.
  - T1 snow flats with frost polygons; T2 rolling pine hollows with kettle holes; T3 the Great Frozen Lake (a flat
    basin with pressure ridges and drifts on its ice); T4 raised ice shelves ending in cliffs or ramps, crossed by
    crevasse fields; T5 Skaldrith's cirque (headwall on the inland side, open to the coast) on its glacier plateau;
    T6 the Thaw's moss hummocks, braided gravel channels and frozen ponds; eight pingos (the Fox Pingo the biggest).
  - Where they stand (for step 2's content): `Saved/RoundIsland/fire_frost_features.json` (cones, pingos, the lava's
    head, the bowl, the lake, the volcano), rewritten each run.
- **Sub-biome sizes (11-20):** each land's old ground is stretched in (`SIDE.depth`: Volcanic 1.75, Tundra 1.5) so the
  five old sub-biomes fill about 70%; `ZONE_FIX` widens the Obsidian Ridges and the Glacier Plateau; the new ground
  splits at `SIDE.split` m past the old ridge (wandering) into V7 the Ashfall Woods / T7 the Mammoth Steppe (outer) and
  V6 the Cinder Terraces / T6 the Thaw (inner). Zone letters m and n.
- **Dressing:** `node Tools/terrain/fire_frost_dressing.mjs` (after `round_ground.mjs`) writes
  `Data/wildreach_dressing.json` from the finished ground: each sub-zone's kinds per hectare (`PLAN`), clumped, kept
  off roads, lava, ice, steep ground, villages, camps and every place; its hazard cells (lava burns, ice slips;
  `drawCells: false`, as WRRoundIsland draws the lava and the ground the ice). Run order now: `round_island.mjs`,
  `fire_frost.mjs`, `round_ground.mjs`, `fire_frost_dressing.mjs`, `hearthmoot.mjs`.
- **Landform sizes (11-20, read from the air):** cinder cones 54-100 m with craters, maars 33-45 m deep (`MAARS`),
  obsidian ridges 90 m, solfatara basin 32 m; ice shelves 58 m with crevasses 18 m deep, the ice cap +85 m, the cirque
  95 m deep (breached east), nunataks 36-44 m, pingos 26-45 m. Positions in `Saved/RoundIsland/fire_frost_features.json`
  (cones, maars, pingos). A relief check: contours and hill shade from island_ground.bin, by eye.
- **Creatures (11-20):** `fire_frost_dressing.mjs` also writes `Data/wildreach_life.json`: `dens` [x, y, day kind,
  night kind, sub-zone] (map metres; per sub-zone kinds and counts in its `LIFE` table, from spawn_regions.json's
  Volcanic and Tundra kinds) and `roam` (the roaming minis' loops, moved whole into their home sub-biome).
  `AWRRoundIsland::PlaceLife` (server) spawns an `AWRCreatureSpawner` per pack (Shift 1 day / 2 night) and gives
  `FWRCreatureRegistry::SetMapRoam` the loops; on a map with lands of its own a roaming mini without a loop there
  stays in. `AWRCreatureSpawner::GroundAt` uses the land's height there.
- **Places, villages, shrines, camps (11-20):** `fire_frost_dressing.mjs` also writes `Data/wildreach_places.json`
  (the Old Island's places file format at round island spots; its sub-zones carry no shapes: `WRIsle::ZoneAt` answers
  the land's zone id on a map with lands of its own), `Data/wildreach_world.json` (villages and shrines with
  `"map": "L_Wildreach"`; `UWRWorldSystems` reads `<map>_world.json` after world.json, a row with `map` is live only
  there, a row with `land` stands on the land's height) and the dressing's `pois` (camps, bosses, minis: the map and
  the world systems' start). `UWRIslandPlaces` loads `<map>_places.json` on any map that has one and finds the ground
  from the land's height. island.json `camps.groups`: each built land's three camps, tagged with its biome.
- **Caves, tubes, bosses, nodes (11-20):** `fire_frost_dressing.mjs` part 3 fits the caves (`Tools/terrain/cave_hollows.json`,
  as the Old Island's dressing does) near their mapped spots into the dressing's `caves`, and writes `wildreach_life.json`
  `bosses` (`[id, at, altar?, altarAt?]`: `PlaceLife` spawns each as a spawner of 1, tag `round_boss`, and the altar
  with its path) and `nodes` (`[x, y, item, yaw]` → `AWRResourceNode`), and `wildreach_world.json` `hidden` (the tubes
  on `fire_frost.mjs` `HIDDEN_ROUTE`) and `biomes` (each land's centre for its event). `round_ground.mjs` raises a
  ridge over each tube (`TUBE`: nothing within 7.5 m of the line, so the corridor's floor is the ground as it was, full
  height by 9.5 m over `AWRHiddenTube`'s deeper walls on a map with lands, falling to its foot by ~34 m; open past
  both ends and beside each room) and the places kit's small pingos as mounds (after the roads' passes).
- **Cave networks (11-20, "Build them"):** `Tools/terrain/cave_layout.mjs` lays each cave from its mouth with a seed:
  `layoutCave` (entrance, entry chamber, slope or ledge + ramp to the hall, crawl to a pocket, squeeze to the deep
  chamber; a room: entrance, room, crawl, pocket; chamber floors no higher than the ground over them allows and no
  steeper than GRADE between), `coverNeeds` (round_ground.mjs `raiseCave` lifts the ground over thin rock),
  `addSkylight` (a shaft over a chamber where the fall is 14 m at most), `dressCave` (formations
  `[kind, x, y, z, a, b, yaw]`, lights `[x, y, z, r, g, b, intensity, radius m]`, spots `{ ore[], star, hazard, den,
  cache }`). island.json `caves` carry `shapes` (tube `p` [x, y, floor, halfWidth, height]; hall `c`, `r`, `yaw`,
  `h`; shaft `c`, `r`, `z`), `feats`, `lights`, `spots` and a `name`. WRLandCaves builds the formations (the look
  kit's spikes, rods and slabs, the rock set's rocks; capsules make the pillars solid) and the lights. The Tundra's
  walls are vertex-coloured ice (M_HeldTool). AWRCave `CaveId` puts its ores, hazard and den at the spots, and
  `UWRWorldSystems::PlaceCaveCaches` puts a chest in each pocket. Survivors are underground by the cave's air itself
  (`WRLand::InLandCave`).
  - Walkable by construction: passage floors are graded by the length walked (`graded`), level for FLAT m where
    they meet a chamber, and a slope has a level landing where its crawl leaves. The capsule climbs 0.45 m steps
    and ~44° slopes; keep slope runs at GRADE (0.36) or less, noise included. Only the ledge's drop is one-way.
  - The rock's look (WRLandCaves ComputeCube): skin less than LipDepth (0.9 m) under the ground's surface is the lip,
    drawn as the land (its material, its vertex data; a cut sample borrows its nearest drawn neighbour's ground). It
    is cut exactly along that depth, so the change is a smooth line; the lip's UVs are made per triangle (from above,
    or from the side on steep faces). The Tundra's ice (M_HeldTool) has UVs that put its grain in level strata, and
    its colours fade to the ground's toward the lip. The weather's fog layer fades out underground (WRSky).
  - Checking: probe the air with the game's noise (LcAir's hash noise ported to JS) along every passage before
    building. Then walk a bot through with `WRWalkTo x y 120` + `waitwalk`. A 4 m arrival radius cuts the corners
    of narrow passages and gets the bot "stuck" on walls a player would never touch.
- **Underground caves (11-20):** a land file's `caves` (`FWRLandCave`; `Tools/terrain/cave_shape.mjs` is the same
  formula) each a flat-floored passage along `pts` [x, y, floor] going down from its `mouth` into a `hall` (a
  flat-floored ellipsoid, `hallR` along and across `dir`), joined smoothly. `round_ground.mjs` sites them (near the
  mapped spot, an easy way up to the mouth, scored by the knoll it needs plus distance), raises a knoll where the hall
  or passage lack cover (hall 2.2 m, passage 1.8 m), and last cuts the land's ground (surface 255) wherever the cave's
  air comes within 1.2 m of it. `underground.ways` mark them rightly underground (no rescue). `AWRLandCaves`
  (spawned by `AWRRoundIsland` on every machine) meshes the caves' air unioned with the air over the ground by
  surface nets (1 m cells, 20 m cubes, workers). The ground's part of that skin is dropped where the land draws its
  own; round the hole it is drawn with the land's material and vertex data on the land's own triangles. The rest is
  the biome's rock (M_Rock basalt; ice). `AWRCave` `bUnderground` (dressing `"underground": true`) stands on the hall's
  floor with no shell. A tube's rooms are caves `<tube>_r<room>` whose passage starts in the corridor:
  `AWRHiddenTube` puts a lintel over the doorway and lights the hall, and `PlaceTubes` puts the room's things there.
- **Weather on the round island:** the weather's fog is the second fog layer, its floor on the ground under the camera
  (eased), falloff `sky.roundWeatherFalloff` (low: the air overhead as thick as round you), density the look's x
  `sky.roundWeatherDensity` or `sky.roundWeather.<kind>` "r,g,b,density" where the round island has its own look (the
  eruption). Thick weather there tints the sky atmosphere toward the weather's colour and stops the sky adding to
  the fog (`sky.weatherSkyFullAt`, `weatherSkyTint`, `weatherSkyLevel`, `weatherSkyHorizon`). The eruption's column
  (fx `eruption.column`) is drawn by every machine from the event's state (`AWRWorldState::Tick`).
- **The lava river:** island.json `lava` (width, points with the lava's level, ending at the shore); its channel and
  levees are cut into the ground, and `WRRoundIsland::BuildLava` draws it with `M_Lava` (molten in the middle, crust at
  the rims) and warm lights along its banks (`roundIsland.lavaGlow`, `lavaLight`, `lavaLightEvery`, `lavaLightRadius`).
- **Steep ground per zone:** a zone's `slopeRock` colours its own cliffs (basalt and obsidian black in the Volcanic,
  blue ice on the glacier) instead of the island's grey.
- **Exposure:** `sky.volcanicDayMinEV100` (by day the eye may not open far on black rock) and `sky.tundraNightMinEV100`
  (by night moonlit snow stays night), eased by the biome under the camera (WRSky::UpdateExposure).
- **Each zone carries its own biome** (island.json zones[].biome): `zoneBiomes` is a JSON object and the game matches
  its keys without case, so letters that differ only in case (c / C, d / D) can't share it.
- **Ground quads split along their flatter diagonal** (WRLand chunk build): no staircase on steep faces.
- **Dark ground stays dark:** `M_IslandGround` (`build_forest_ground.py --island`) takes nearly all of a dark ground's
  colour from the land, so pale textures can't lift basalt, obsidian or black sand to grey.

## How it is built (from slice 1, 11-19)

- **Its own map:** `L_Wildreach` (sky, sun, fog and clouds only; everything else is built by code at runtime). The Old
  Island keeps `L_Island`. A world remembers its map (`info.json` "map"); the New world form chooses it.
- **The ground:** `node Tools/terrain/round_ground.mjs` turns this layout into one heightfield land,
  `Data/lands/island.json` + `island_ground.bin` (4 m samples over ±6.6 km, zlib), with a zone letter per land (its
  biome via `zoneBiomes`), the meadow (M), the city's hilltop (C) and the sea floor (~, D). Its far detail levels and
  collision-near-bodies keep 95 km² of runtime ground cheap. Each land's slice replaces that land's first-pass shape in
  the same script, so borders stay crack-free (one ground).
- **The sea:** `Data/sea_mask_L_Wildreach.json` from the same script; the sea and the water you swim in pick the mask
  named after their map.
- **The city plan:** `Tools/terrain/hearthmoot_plan.mjs` holds the streets (avenues, ring, alleys, lanes), the beach,
  jetty, reeds, footbridge, farms, fields, windmills and meadow trees. Both scripts import it: the ground cobbles the
  streets and stripes the fields, the city script puts houses on plots facing their street. Run order:
  `round_island.mjs` (only when the layout changes), `round_ground.mjs`, `hearthmoot.mjs`.
- **Ground cover and the safe meadow:** `WRRoundIsland.cpp` grows low grass near the camera on the meadow and yards
  (`roundIsland.cover*`) and puts one safe zone over the meadow (`roundIsland.safeRadius`, 1.3 km).
- **Playtest:** `Tools/roundtest.ps1` (server + a rendered walker + a second player; walks, swim, frame times).
- **Lights:** `fires` (hearth, braziers, lantern lights, torches) and `nightBlocks` (lit windows, lantern glass) in
  Data/hearthmoot.json; WRRoundIsland's `StepLights` shows them with the clock.
- **Only the round island's own things on its map:** `WRLand::OwnLandMap()` is true on a map with a land of its own;
  `WRBiome::IsLive` is then false for every Old Island biome (nothing of theirs is placed there), while
  `WRBiome::IsContentLive` (recipes, crops, dishes, factions) is unchanged. Each land's slice brings its content here.
- **People:** hearthmoot.json "people" (walks + walkers, sellers, standers) spawned by WRRoundIsland on the server
  as AWRWorldNpc townsfolk / seller / guard; a seller trades from its TradeTable (world.json trade). Plain faces, net
  cull 90 m, pose and tick throttled by distance (StepCrowdCost); `wr.HidePeople 1` hides them for frame checks.
- **Rooms you walk into:** `hallInside` and the inn in hearthmoot.mjs (`wall` with openings, `openRoof`, `table`,
  `lampHang`); their floors in "floors" keep the grass out; fires kind 4 / 5 light them.
- **Sound:** sounds.json ambient beds on the island's zones (zone.yards / zone.city / zone.meadow, .day / .night).
- **Each slice is finished before the next** (Raven, 11-19): audit it from many angles, day and night, fix everything
  in it, make it alive (people or creatures that belong there) and explorable (key places you walk into), and list
  only what truly belongs to a later slice.
- **Hearthmoot and the forts:** `node Tools/terrain/hearthmoot.mjs` -> `Data/hearthmoot.json`, built on every machine
  by `WRRoundIsland.cpp` from the building look's blocks (no replication); its solids are hidden boxes.
- **Pictures:** `Tools/terrain/spotshots.ps1 -Map L_Wildreach` (camera spots in game cm: map metres x 100).
