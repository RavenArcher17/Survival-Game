# wild-answers-1116: the better forest, clan map pins, places placed with intent

Lane build-wild-answers-1116 (laptop, 2026-11-16). Source: Aaron's answers on the `wildreach-open-questions` page
(`wildreach-night/answers-wildreach-open-questions.json`, 13:31-13:38Z), cards from ship-7's list.

## 1. REQUIREMENT LEDGER

| # | Requirement (one each) | Source | Done (proof) |
|---|---|---|---|
| R1 | Rebuild the Ancient forest's look with the Electric Dreams pack (find it, use it, not just its lessons) | q-electric-dreams "Yes do the better forest", brief 1 | 1ff3978: `WRWorldDressing.cpp:711` (scans per kind), `WRForestArt.cpp`, `Data/forest_art.json`; kit copied to Content (1.6 GB, git-ignored) |
| R2 | Check what wild-answers-1115 did for the forest and finish it properly (its ground stays; the pack was missing there) | brief 1 | 415bc08 ground kept; 1ff3978 adds the pack it lacked (laptop copy) |
| R3 | Use the Trails of Appalachia forest lessons (its Electric Dreams notes, its look bible) | brief 1 | ED notes 1-3 (embed `sink`, layers, clustering, no grass, contact moss) in `forest_dressing.mjs:486` section 8; Trails look bible (litter main cover, bare trails) |
| R4 | "An incredible forest": canopy trees, understorey, ground layer, deadwood, rocks, all real scanned art, no primitives | brief 1 | hornbeam canopy + leaf colour per species, saplings, ferns, boulders, ~80k ground pieces (`Data/forest_ground.json`) |
| R5 | Every forest thing keeps its job: wood/fiber/stone harvesting, trunk collision, the server agrees with clients | THE BAR | `WRWorldDressing.cpp:711` harvest parts (NArt, v), colliders from data on server + client |
| R6 | Works without the pack (Raven's PC, a server): falls back to the tree kit, never engine shapes | repo rule | `WRWorldDressing.cpp` kit fallback (bAnyScan false), ground layer skipped; `check_setup.ps1` NOTE |
| R7 | Players can share map pins with their clan | q-map-pins | 1700357: `WRClanPins.cpp:126` SetPlayerPins, `WRPlayerController.cpp:1398` ServerMapPins |
| R8 | A "See clan pins" option on the map | q-map-pins | `WRUIMap.cpp:1572` legend row "Clan pins" |
| R9 | Default: you see only your own pins | q-map-pins | `WRClanPins::ShowClan` default false (GameUserSettings [Wildreach.Map] ShowClanPins) |
| R10 | Shared pins reach clanmates online or not (offline members' pins still show), server-authoritative | THE BAR, multiplayer | world doc `map_pins` (offline mates), `WRSocial.cpp:110` SendAll on join/clan change |
| R11 | Mouse AND pad for the option and for a clanmate's pin | THE BAR | legend is an SButton (pad A via the menu cursor), clan pin card on right-click / pad X (`WRPadInput.cpp:623` MenuSecond) |
| R12 | Keep the moved places moved (no going back to the sea/cliff spots) | q-world-moved KEEP | d086062: no place back on its sea/cliff spot (fallback = last settled spot, none used) |
| R13 | Every place cookpass-world moved to "the nearest ground" gets a real placement redesign with clear intention | q-world-moved | d086062: 16 places re-sited by landform rules (`island_places.mjs:149` site(), rules from :164); Slag Light footing `WRIslandPlacesLook.cpp:416` |
| R14 | Each placement says why it is there, what it connects to, how it meets ground/water/route (DESIGN WITH INTENTION) | preamble | section 4.3 |
| R15 | Do NOT build q-server-32 (100 players) or q-stronghold-garrison: Raven decides | brief 4 | not built (server size and garrison untouched) |
| R16 | Full cook: research with named games, plan before building | brief | this doc sections 2-4, written before building |
| R17 | Data-driven numbers (tuning / data files) | THE BAR | `Data/forest_art.json`, `Data/forest_ground.json`, `ui.pins.sendSeconds` in tuning |
| R18 | Compile clean; no tests run | GATES | Build.bat WildreachEditor: Succeeded after each part, no warnings from new code |
| R19 | CHANGELOG Unreleased + HANDOFF in the same commits; push to master; no release | GIT | CHANGELOG Unreleased + HANDOFF in 1700357, d086062, 1ff3978 (+ the HANDOFF path fix) |
| R20 | LANDED line (LANDED.md + LAPTOP-SAYS.md); binary sizes reported; later-cook / questions lines | brief | LANDED.md + LAPTOP-SAYS.md lines; LATER-COOKS + QUESTIONS lines |

## 2. Research

### 2.1 What the 1115 lane did and what's left (R2)
- 415bc08: `M_ForestGround` (Trails' CC0 litter / soil / moss scans + the MW pack's grass, stones, rock), height-blended,
  per-vertex layer weights from the zone rows, contact skirts at boulders and big trunks. Kept as it is.
- It could not use Electric Dreams: the pack was not on the desktop. **It is on this laptop**:
  `OneDrive\Documents\Unreal Projects\ElectricDreamsEnv` (UE 5.8 project, `Content\Megascans` 57 3D assets, 40 plants,
  15 decals, 12 surfaces). The trees, ferns, boulders and stumps the forest draws are still the tree kit's code meshes.

### 2.2 How the best forests are built
- **Electric Dreams (Epic, UE 5.x)** (Trails' notes, `ELECTRIC-DREAMS-NOTES.md`): one hero tree species (European
  hornbeam, Megascans, Nanite, SimpleWind), eight size variants mixed so no two neighbours match; layers in order (hero
  trees, trees, rocks, clutter), each subtracted from the one above; clutter clustered round features (`ClusterSearch
  Distance`, `NonClusterDensity`) with sparse singles between; ground props projected onto the slope and sunk
  10-25 % of their height; no landscape grass, the ground covered by litter, dead leaves and moss meshes.
- **Red Dead Redemption 2 (Roanoke Ridge, Big Valley)**: under every tree, a layer of fallen twigs and leaves; ferns in
  drifts on north slopes and along water; saplings in light gaps, never under closed canopy; deadwood everywhere.
- **Kingdom Come: Deliverance 2 (the Bohemian beech-oak woods)**: a real European mixed woodland: oak, beech and
  **hornbeam** (Carpinus betulus is the classic understorey-to-mid-canopy tree of oak forests in Central Europe:
  the *Querco-Carpinetum*); elder at the edges and in clearings; the woods read real because the species mix is
  ecological, not random.
- **The Witcher 3 (Velen, Skellige woods)** and **Ghost of Tsushima**: colour variation per tree (a few hue steps of
  the same species) so a canopy never reads tiled; Tsushima's per-instance tint.
- **Real woodland** (UK ancient woodland / Białowieża): canopy oak, beech, lime, ash; mid-storey hornbeam, hazel, holly,
  elder; field layer of ferns (lady fern, male fern, beech fern), dog's mercury, wood anemone; ground layer of moss and
  leaf litter that's deepest in hollows and at trunk bases, dead twigs under every crown (self-pruning).
- **Trails of Appalachia look bible (08-art-direction)**: litter is the main ground cover, darker and compacted on
  trails; "full and alive, never cluttered"; open-tight-open rhythm.

### 2.3 Map pins shared with a clan (R7-R11)
- **Rust**: your map markers are visible to your team in your colour; the team sees all of them, nobody else does.
- **Sea of Thieves**: the crew shares one map table; marks are for the crew.
- **Valheim**: pins are your own; sharing is an explicit act (the cartography table), and shared pins come in
  distinguishable (crossed/greyed) so they never mix with yours.
- **ARK: Survival Ascended**: map markers can be your own or your tribe's; the map filters tribe markers.
- **Palworld / Enshrouded**: guild members' markers appear on the map with their name on hover.
- **Take**: every pin you make is offered to your clan (Rust/ARK), but on your map clanmates' pins stay hidden until you
  tick **Clan pins** on the legend (default off, kept between runs: Aaron's rule). Clanmates' pins draw in their own
  look (a sage ring, the owner's initial on the badge) so they never mix with yours (Valheim's lesson); hover/zoom shows
  "Kara · Den"; right-click (or the pad's map action) on one opens a read-only card: Waypoint here, **Copy to my
  pins**. They appear on your compass only while the option is on. Offline clanmates' pins still show (the server keeps
  everyone's pins in the world's store). Leaving the clan takes theirs off your map at once.

### 2.4 Placing places with intent (R12-R14)
- **Breath of the Wild / Tears of the Kingdom**: every landmark sits where the terrain explains it: towers on the
  highest spur with a view of the region they reveal, stables at road junctions, Koroks on tips and hollows. The
  "triangle rule": a landmark is placed on a terrain feature that shapes the approach.
- **Ghost of Tsushima**: lighthouses on headlands you sail past, shrines on cliff tops at the end of a climb, hot springs in
  hollows: the place type and the landform agree.
- **Skyrim**: watchtowers on the knoll commanding the road; Nordic barrows dug into a hillside; wrecks on the shoreline.
- **Real world**: lighthouses stand on the outermost headland or a sea stack (Fastnet, Hook Head), visible from the
  open sea; acid crater lakes fill closed basins (Kawah Ijen's turquoise lake in the crater floor); watchtowers sit on
  the crest with line of sight to the pass they watch; ice-fishing camps stand on lake ice a short walk out from the
  shore; pingos rise from flat drained-lake lowland; scree and obsidian blades collect at the foot of steep faces;
  middens are banked against a slope; a fallen tree is a bridge only where it spans a ravine between two banks.

## 3. Decisions (made tonight, written to QUESTIONS-FOR-TOMORROW)
- **The pack is copied in, a lean forest kit only** (C: has 8.5 GB free; the whole pack is ~35 GB). Copied with paths
  unchanged under `Content/Megascans`, `Content/MSPresets`, `Content/Custom`, `Content/PhysicalMaterials` (git-ignored like
  the Fab packs; `Tools/art/electric_dreams_forest.ps1` copies the same list on any PC from the project folder or
  a fresh Fab download). Textures capped at 2K in the copy (the perf pass's 8 GB-card rule).
- **Which forest trees become hornbeam**: oak, beech, lime and ash (the woodland canopy + mid-storey) become the
  pack's eight hornbeam forest trees, each "species" a set of variants and its own leaf colour variation, so the
  canopy reads as one real woodland with species tone steps (KCD2's oak-hornbeam wood, Tsushima's tint). The landmark
  giants, the alders that lean over the streams, holly (evergreen, a story plant) and brambles stay the kit's: they are
  designed shapes the pack doesn't have.
- **Hazel becomes elder** (Elderberry scans; same job: the shrub layer at edges and gaps).
- **Clan pins: every pin is shared**; no per-pin private switch (the card didn't ask). Alternative in the questions file.

## 4. PLAN

### 4.1 The forest with Electric Dreams (R1-R6)
Art (UnrealEditor-Cmd, never the GUI):
1. `Tools/art/electric_dreams_forest.json`: the list (meshes + their dependency closure, crawled from the uasset name
   tables), and per kind what draws it.
2. `Tools/art/electric_dreams_forest.ps1 -From <ElectricDreamsEnv>`: copies the list into `Content/` (same paths).
3. `Tools/art/electric_dreams_forest.py` (commandlet): caps every copied texture at 2048, then measures every mesh it
   will draw (height from bounds; trunk centre and radius from the LOD0 trunk section's vertices 0.4-1.4 m up; a rock's
   capsule along its long axis from the bounds) and writes `Data/forest_art.json`.
Code (`WRWorldDressing.cpp`):
4. Read `Data/forest_art.json`: per forest kind, meshes `{path, height, colliders[]}` plus a leaf material override.
5. A kind with an art row draws its instances with the pack meshes (instance i -> mesh i % N; scale = the instance's
   scale x the kit's reference height / the mesh's height, so every tree keeps the height the generator gave it), and
   its collision from the row's colliders (the server reads the same file: no meshes needed, both sides agree).
   Harvest parts register with Variants = N, so felling hides the right instance in every part.
6. If no mesh of a row loads (no pack), that kind draws from the tree kit as before (collision still the row's: the
   collision never depends on what a PC has installed).
7. New ground layer kinds from the generator, looks only (no collision, no harvest, short cull, no shadow):
   `forest_leaves` (DeadLeaves drifts), `forest_moss` (CustomMoss mats), `forest_cover` (GroundCover: wood anemone and
   low herbs), `forest_twigs` (WoodenSticksAndTwigs / DryBranches). Missing pack: not drawn.
Generator (`Tools/terrain/forest_dressing.mjs`):
8. Litter under every broadleaf crown (2-4 twig/leaf pieces inside the drip line, more under giants), moss mats at the
   foot of every boulder, stump, giant and fallen log (the contact the 1115 skirts started), dead-leaf drifts in hollows
   (lower than the 12 m ring around them), ground cover in light gaps and along the meadow edges, clustered (ED's
   cluster rule), never on roads, trails, rivers, arenas or walkways (the same keep-outs the trees use), all
   deterministic; then regenerate `island_dressing.json`.
9. Cull distances + density in `tuning.json` (`forestArt.*`), so they can be tuned without code.

### 4.2 Clan map pins (R7-R11)
1. `FWRSharedPin` (owner, x, y, glyph, label) USTRUCT.
2. Client: after any pin change, and once when the world's pins load, `ServerMapPins(pins)` (clamped to `ui.pins.max`,
   labels to the label max, sent at most once a second).
3. Server `FWRClanPins` (WRClanPins.cpp): each player's pins in the world doc `map_pins`; on a change, each online
   clanmate gets `ClientClanPins(their clan's pins but their own)`; on join and on any clan change
   (`FWRClans::RefreshPlayerStates`) everyone gets theirs again.
4. Map legend: a **Clan pins** row (sage ring glyph, "· your clan's marks" / "· join a clan to share"), off by default,
   kept in `GameUserSettings.ini` ([Wildreach.Map] ShowClanPins). Mouse click and pad A like every legend row.
5. Clanmates' pins: sage-ringed badge with the glyph and the owner's initial; label "Name · label" at zoom / hover;
   right-click opens a read-only card (owner, mark, Waypoint here, Copy to my pins; B/Esc closes).
6. The compass shows them (sage ring) only while Clan pins is on.

### 4.3 Places with intent (R12-R14)
`Tools/terrain/island_places.mjs`: every place that was "settled" onto the nearest flat ground gets a `site` rule: the
landform its story needs, searched for within a radius of the fire-frost.md spot, scored, and written with the reason.
The builder draws on what's chosen. Per place (fire-frost.md spot -> what it needs):

| Place | Its job (why it's there) | Landform rule | What it meets |
|---|---|---|---|
| Slag Light (−1880, +120) | The smiths' beacon for ships on the west coast; a sea view of the whole coast; lit by the Kipuka shrine | The west headland's tip: land with the most sea round it within 150 m, the waterline within 30 m, high (cliff top), facing the open sea | Built at the cliff edge, its stair facing inland where you come up; a causeway is not needed (on the tip) |
| Acid Pool (−230, −980) | Acid run-off from the fumarole fields collects; dead birds, a rolled cart | The lowest closed hollow in the Solfatara near the fumaroles: ground ≥2 m higher all round at 30 m, floor flat | The cart lies on the slope above the pool, rolled down from the rim toward it |
| Ridge Watch (−180, −600) | Smith tower watching the north pass; reveals V5 | Volcanic-side crest (higher than all ground within 50 m) with an unbroken sight line to the pass mouth (−13, −1296) and to the crater, 250-800 m from the pass | Faces the pass |
| Watch Cairn (300, −700) | The Rime-folk's mirror post on the tundra side; reveals T5 | The same rule on the tundra side (x > 150) | Faces the pass |
| Charred Caravan (−1650, −700) | Smith wagons burnt on the road; guards face the pass | On the Slag-Cairn road where it passes nearest the spot | Wagons along the road, guards facing the pass |
| Plough Trench (−1500, +200) | A 400 m furrow Cinderhorn ploughed across the ash | The longest straight, flat, dry run of ash plain near the spot (≤ 0.12 slope all along, no sea or lava), 300-420 m | from/to are the run's ends |
| Shard Field (−1450, −500) | Obsidian blades shed from the ridges | The apron at the foot of the steepest obsidian face near the spot: gentle ground with a steep face 40-120 m uphill | Spread downhill of the face |
| Fox Circle Pingo (1700, +900) | An ice-cored mound on flat permafrost where foxes watch the aurora | Flattest open lowland of the Snow Flats near the spot, ≥ 40 m from the sea, no pines | Open sky |
| Tusk Midden (1600, −900) | Tusks banked half-frozen into a bank | A bank: a 0.3-0.7 slope rising behind flat ground, near the spot | Faces out from the bank |
| Sledge Wreck Line (1700, +300) | Three sledges abandoned in a line toward the coast | Flat snow 60-160 m inland from the coast near the spot | The line points at the nearest coast |
| Ice-Fishing Camp (760, −1500) | Holes in the lake ice a short walk from shore | Lake ice 15-45 m from the shore, flat | Faces the shore it walked from |
| Drowned Sledge (1250, −1100) | Sledge frozen under clear ice; points to the thin ice | Lake ice 30-120 m out, nearest the doc spot | Faces the nearest thin-ice patch |
| Moulin of Voices (700, −250) | A meltwater shaft on the glacier | The lowest point of a gentle channel on the ice shelf near the spot | Mist, the shaft |
| Glacimaw Boneyard (380, +480) | Where it surfaces: churned snow in a bone ring | The flattest open 20 m disc on the shelf near the spot | Ring on level snow |
| Fallen Giant Pine (600, +700) | The only bridge over a ravine | Two banks 30-36 m apart with ground ≥ 5 m below their midpoint: the trunk spans it | Yaw along the span |
| Miners' Grave (−470, −1110) | Bodies mid-stride fleeing toward the shore | On the line from the Blue Fire toward the nearest shore, 60-140 m from the pits | Bodies face the shore |
| Miners' Camp (−380, −1240) | "With the miners' camp below" the Blue Fire | Lower ground within 80-160 m of the Blue Fire, flat for tents | Faces the pits |
| Kilnmite Hill, Steam Ground x2, Tree-Mold Field, Glasswall, Blue Fire, Crater Stair, Bellows, Frozen Whaler, Weeping Seracs, Great Inuksuk, Claw Scars, Crevasse Maze, Hunter's Blind | moved < 40 m by the settle, already on the landform their row names (checked: z, slope); keep, with the reason recorded |
| Spec-anchored: Crater plume, Fallen Smith (measured river span), Kipuka, Wisp Spire, Tusk Gate, Pale Pines, Ice Arch, Warden's Quarry, Hunter's Camp, Tusker's Wallow | Pinned by the SPEC's boss sites / villages: unchanged |

Edge cases: a rule that finds nothing keeps today's spot and says so in the tool's output (never the sea); every rule
still respects the keep-outs (camps, villages, shrines, arenas, caves) and lava; results are deterministic.
Builders: the Giant Pine gets its span length from the row (`span`, metres) and lays its trunk bank to bank.

### 4.4 Build steps (landing order)
A. Clan pins (code). B. Places with intent (generator + Giant Pine span). C. The forest: copy, commandlet, art layer,
generator, regenerate. Each lands as its own commit with CHANGELOG + HANDOFF.

## 5. Proof and what's not finished

Commits on Wildreach master: 1700357 (clan pins), d086062 (places with intent), 1ff3978 (the forest), and the docs fix.
Compile gate only (Build.bat WildreachEditor Succeeded after each); nothing played or cooked here (Aaron tests).

What each place rule chose (metres moved from fire-frost.md): Slag Light 303 (west headland tip), Fox Pingo 19, Miners'
Grave 143, Miners' Camp 71, Charred Caravan 210 (on the road), Shard Field 81, Ridge Watch 679 (sight line to the pass),
Acid Pool 509 (a pan on the Solfatara floor), Tusk Midden 62, Sledge Line 262, Ice-Fishing Camp 392, Drowned Sledge 171,
Giant Pine 166 (a real ravine), Glacimaw Boneyard 240, Watch Cairn 504, Moulin 31. Every rule found a fit.

Not finished / honest:
- The release PC (desktop) doesn't have the Electric Dreams kit yet: until `Tools\art\electric_dreams_forest.ps1` and its
  commandlet run there, a package built there ships the tree-kit forest. Told the desktop (LAPTOP-SAYS).
- Giants, landmark trees, leaning alders, holly, hazel and bramble stay the tree kit's (no scan of those species at
  those sizes in the pack): a later cook can bring scanned species for them (LATER-COOKS).
- The scans' materials were not compiled for a cook here (6 GB free on C:); the masters are legacy (non-Substrate) and the
  project has virtual textures on, so they should compile: the first cook will say.
- Clan pins: every pin is shared (no per-pin private switch): QUESTIONS-FOR-TOMORROW.
