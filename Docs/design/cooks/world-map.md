# Cook: world-map (FULL-GAME 3.1 and 3.3)

Lane: world-map. Written 2026-11-13. Scope: the whole 12-biome map layout, passes and borderlands, travel (foot, mounts, boats, gliding, flying, fast travel), density rules, POI types, sub-zone rules, world events, weather, day and night, the hazards table, how biomes connect, and the story the land tells.

Coordinates: metres in today's `L_Island` frame. Origin is the island centre, +x east, +y south. This matches `cooks/fire-frost.md`. The north pass is at (0, −1296) and the south pass at (+82, +1294). **The origin never moves.** Every claim, bed, tame and chest in today's saves keeps its world position.

Tags: **SPEC** = Raven's docs or the built game, kept as written. **GAP-COOKED** = this lane's design, replacing a FULL-GAME GAP first draft.

---

## 1. Research

### 1.1 What the best games do

| Game / source | What they do | Lesson we take |
|---|---|---|
| Elden Ring: the Erdtree ([PC Gamer](https://pcgamer.com/games/rpg/with-shadow-of-the-erdtree-fromsoftware-wanted-to-build-a-map-thats-not-quite-open-field-and-not-quite-legacy-dungeon-but-instead-blends-the-two-together), [Game Developer](https://www.gamedeveloper.com/game-platforms/how-elden-ring-succeeds-by-ignoring-20-years-of-open-world-design)) | One giant landmark is visible from everywhere. It works as the compass and as the lore. Legacy dungeons are visible from far off. The DLC blends dense field and dungeon | **One world mountain, the Crown**, visible from every biome. Each major boss lair is its biome's far-visible "super landmark" |
| Breath of the Wild: the triangle rule ([CEDEC notes](https://gist.github.com/idbrii/e39fe96279aa1670319bfa521d907399), [GMTK](https://gmtk.substack.com/p/how-nintendo-solved-zeldas-open-world)) | Hills hide what is behind them, so every rise reveals a new target. Landmark types are mixed | POIs sit where a rise or a bend reveals them. We use a shared taxonomy of 20 POI types, about 15 per biome |
| Tears of the Kingdom: three layers ([Destructoid](https://destructoid.com/?p=380677), [Zelda Wiki: Updraft](https://zeldawiki.wiki/wiki/Updraft), [Game Developer](https://gamedeveloper.com/production/zelda-devs-originally-made-sky-islands-look-like-specks-of-trash)) | Sky, surface and depths work as one connected system. Updrafts and freefall are the glue. Sky islands looked like "specks of trash" until they were made bigger and more readable | Wind (sky) sits straight above Spore (deep), joined by one vertical pass, **the Flue**. Islands are few and big, at least 40 m across, with readable silhouettes |
| Southern Alps, New Zealand ([Te Ara](https://teara.govt.nz/en/1966/southern-alps), [Arthur's Pass](https://www.Wikipedia.com/wiki/Arthur%27s_Pass_(mountain_pass)), [Wikipedia](https://www.wikipedia.org/wiki/Main_Divide)) | A long mountain spine. Only three road passes cross it (Arthur's 920 m, Lewis, Haast 550 m). The windward side is wet and the lee side is semi-desert | **Raven's ridge becomes a continent-long Spine.** Each opposite pair sits straight across it with its own pass. Prevailing wind from the east sea makes the east side wet (sea, swamp, forest, snow) and the west side a dry rain shadow (desert, ash, dry lightning, wasteland) |
| Monster Hunter Wilds weather ([Icy Veins](https://www.icy-veins.com/monster-hunter-wilds/news/how-to-master-monster-hunter-wilds-dynamic-weather/), [MH wiki](https://monsterhunterwiki.org/wiki/Seasons_(MHWilds))) | Fallow, Inclemency and Plenty cycle per region. Each region has its own Inclemency with its own apex monster | Weather is per biome and every biome gets its own signature bad weather. Rough weather is followed by a "plenty" window that pays out |
| Valheim weather and sailing ([weather wiki](https://gamecytus.netlify.app/host-https-valheim.fandom.com/wiki/Weather), [Steam](https://steamcommunity.com/app/892970/discussions/0/4635986167856179737), [sailing](https://mobalytics.gg/gamebase/guides/valheim-sailing), [hostedgg](https://hostedgg.com/wiki/valheim/sailing-boats)) | Weather is biome-based, the same for every player, and deterministic (the community can forecast it). Bosses force weather. The wind shifts every few minutes. You can't sail straight upwind, so you tack or row | Weather comes from the world seed: the server and clients compute the same sky with nothing to sync. Sailing uses a wind-angle speed curve and you row when becalmed. The wind is one global vector shown on the HUD compass while you sail |
| Dragon's Dogma 2 travel ([PC Gamer](https://pcgamer.com/dragons-dogma-2s-director-didnt-want-his-game-to-rely-on-fast-travel-travel-is-boring-thats-not-true-its-only-an-issue-because-your-game-is-boring), [GamingBolt](https://gamingbolt.com/dragons-dogma-2s-developers-explain-restricted-fast-travel-options)) | "Travel is only boring if your game is boring." The oxcart can be ambushed and a griffin can wreck it. Travel events come from systems colliding, not scripts | Fast travel works only between shrines you have walked to. Merchant caravans walk real roads and can be attacked. Migrations cross passes with predators following. None of it is scripted |
| Rust monuments and boats ([Facepunch wiki](https://wiki.facepunch.com/rust/map~diff:547398), [BisectHosting](https://www.bisecthosting.com/es-es/blog/rust-monuments-loot-resources-locations-items)) | Monuments are high-risk loot sites with gear checks (radiation needs a hazmat suit). Boats bypass land routes | Hazard zones are gear checks. Boats can round the Spine's ends, but only through tidal races that are crossable at slack water, so the passes stay the main route |
| Axis mundi folklore ([Encyclopedia.com](https://www.encyclopedia.com/environment/encyclopedias-almanacs-transcripts-and-maps/axis-mundi), [New World Encyclopedia](https://www.newworldencyclopedia.org/entry/Axis_Mundi.)) | Meru, Hara Berezaiti and Himinbjörg are each the highest point and centre of the world, where heaven meets earth | The Crown is the Reach's world mountain. All twelve old peoples climbed toward it |
| Norse creation, Ymir ([Wikipedia](https://en.wikipedia.org/wiki/Ymir)) | The world is made from a giant's body: mountains from bones, sea from blood. Fire-frost already uses "Ymir was born where fire met ice" | The continent is a fallen giant lying face-up. The Spine's crest rocks are vertebrae and the spur ridges are ribs. No one ever says it; you see it from the Crown or on the map |
| Fire-frost cook (`cooks/fire-frost.md` 1, 3.1) | 115 POIs per 5 km², 150 m minimum spacing, no 250 m gap, shrine and event frameworks | **SPEC for this lane.** We scale it to every biome and reuse its classes |

**Real-world facts used for the land**

| Fact | Source | Used for |
|---|---|---|
| Þríhnúkagígur in Iceland is an open, walkable magma chamber 120 m deep | [Wikipedia](https://en.wikipedia.org/wiki/%C3%9Er%C3%ADhn%C3%BAkag%C3%ADgur) | The Flue |
| Corryvreckan and the Pentland Firth are tidal races with standing waves, passable at slack water | [Wikipedia](https://en.wikipedia.org/wiki/Gulf_of_Corryvreckan) | Races at the Spine's two ends |
| Dry lightning is common in rain-shadow deserts and mountains | [Wikipedia](https://en.wikipedia.org/wiki/Dry_thunderstorm) | Storm peaks on the dry west |
| Ridge lift: wind blowing onto sea cliffs rises (gliders soar coastal cliffs) | [Wikipedia](https://en.wikipedia.org/wiki/Ridge_lift) | Updrafts on the Windward cliffs |
| Bioluminescent fungi glow on a circadian clock (Oliveira et al. 2015) | [Current Biology](https://doi.org/10.1016/j.cub.2015.02.021) | Cavern light pulses with the surface day |
| Lung ta (prayer flags on wind poles) | [Wikipedia](https://en.wikipedia.org/wiki/Prayer_flag) | Sky people's waymarks |
| Trail trees (saplings bent to mark paths) | [Wikipedia](https://en.wikipedia.org/wiki/Trail_tree) | Forest people's waymarks |
| Ringing rocks (stones that ring like bells) | [Wikipedia](https://en.wikipedia.org/wiki/Ringing_Rocks_Park) | Crystal people's waymarks |
| Tōrō stone lanterns | [Wikipedia](https://en.wikipedia.org/wiki/T%C5%8Dr%C5%8D) | Graveyard people's waymarks |

### 1.2 Lessons, short

1. **One landmark rules the sky.** The Crown is visible from all 12 biomes.
2. **Extend Raven's idea; don't replace it.** He built a central ridge with two passes and opposites on either side. The full map is that idea six times along one Spine.
3. **Opposites meet at their pass. Neighbours meet along open borders with mixed creatures and props.**
4. **The land does the talking.** Roads, markers, wrecks, war lines and skeletons tell the story, with no cutscenes.
5. **Weather is per biome, deterministic and readable before it lands.** It changes how fights go, so it matters.
6. **Movement is fun and earned.** Mounts and roads come early, then a crude glider, boats, the levistone glider and flying mounts.
7. **No wipe.** Every walkable metre of today's island is frozen. New land only fills sea and the unclimbable ridge interior.

---

## 2. Design

### 2.1 Why this replaces the 12-wedge wheel (GAP-COOKED)

FULL-GAME 3.1's GAP drew 12 pie wedges round a central massif, with six passes running "straight across" through it. Laid over today's island, that draft breaks three things:

| Problem with the wheel | Why it fails | The Spine layout |
|---|---|---|
| Fire and Frost fill a 4 × 4 km square, not 37° wedges | Fire's land would spill into the Lightning and Void wedges. Keeping it would mean redrawing built ground | The existing island is one belt, untouched |
| Six passes straight across a wheel all cross at the centre | One 12-way crossroads. Fire–Frost's built passes sit 1.3 km off-centre and don't fit | Each pair gets its own pass across the Spine, as Fire–Frost already has |
| Raven's code places wildlife by side of the ridge (`X < 0`) and by distance from it (`island_walk.json` bands) | Wedges need new spawn logic | Same idea everywhere: biome by mask, band by distance from the Spine |
| The coastal spawn camps (MP) must keep the sea | Wedges wrap land around the island | Fire's west coast and Frost's east coast stay open sea |

The Spine keeps all of Raven's rules: every biome has an outer coast with three spawn camps, lowlands by the coast, midlands, highlands against the Spine, the major boss against the Spine, opposites across a pass, and invasions through the pass. It also keeps the "central massif" idea. The **Crown** is the great massif in the middle of the Spine, the tallest point in the world, seen from everywhere.

### 2.2 The whole map (GAP-COOKED)

**The Reach** is one long island continent, about 16 km north to south and 4.5 km wide, ringed by sea. The **Spine** runs down its full length. The Spine swells into the **Crown** (1,650 m) just north of today's island. Six **belts** of land sit along the Spine, each holding one opposite pair: west of the Spine, east of it, or (for Wind and Spore) above and below it.

**Map at a glance (north at top, not to scale):**

```
                 ~ ~ sky islands ~ ~ (Wind, 500-1,300 m up)
   y -9,500  ┌──────────────────────────────┐
             │ Fungal surface │FLUE│ Windward│  B1 HOLLOW CAPE   (Spore below / Wind above)
             │ (caverns below)│    │ cliffs  │                    the Gullet race off the tip
   y -8,000  ├────────────────┼────┼─────────┤
             │                │ S  │  Ocean  │~~~~ reef sea ~~~~
             │     Desert     │ALT-│  land   │  B2 NORTH COAST   (Sun / Tide)  pass: SALTGATE
             │                │GATE│         │~~~~~~~~~~~~~~~~~~~
   y -5,400  ├────────────────┼────┼─────────┤
             │                │STORM│        │
             │  Storm peaks   │ GUT │ Swamp  │  B3 THE CROWN     (Lightning / Blood)
             │  ( THE CROWN, Spire 1,650 m ) │                    pass: STORMGUT
   y -2,400  ├──Ash Sound──┤neck│neck├──Rime Sound──┤
   y -2,016  ├────────────────┼────┼─────────┤
             │                │ N pass       │
             │   Volcanic     │RIDGE│ Frozen  │  B4 OLD ISLAND    (Fire / Frost)  BUILT
             │   (Fire)       │ S pass tundra│                    north + south passes
   y +2,016  ├──Bone Sound─┤neck│neck├──Pine Sound──┤
   y +2,350  ├────────┼────┼─────────────────┤
             │Graveyard│ROOT│ Ancient forest │  B5 ROOTLANDS     (Necromancy / Nature)
             │         │GRAVE│                │                    pass: ROOTGRAVE
   y +4,600  ├─────────────────────┼────┼────┤
             │  Corrupted wasteland │SEVER│Crys│  B6 BROKEN END    (Void / Arcane)
             │        (Void)        │ANCE │tal │                    pass: THE SEVERANCE
   y +6,400  └─────────────────────┴────┴────┘  the Broken Race off the tip
```

**Belts, north to south.** All areas are surface; the extra volume is listed separately. Sizes come from CAL Part 3 (SPEC for sizes).

| Belt | y range (m) | West of the Spine | East of the Spine | Pass across | Opens in round |
|---|---|---|---|---|---|
| B1 Hollow Cape | −9,500 to −8,000 | Fungal caverns surface, 1.5 km² (caverns ~3 km² below B1 and north B2) | Windward cliffs ground, 0.8 km², plus 2.0 km² of sky islands in ~5 km² of air above B1 and the north-east sea | **The Flue** (vertical) | R7 / R8 |
| B2 North Coast | −8,000 to −5,400 | Desert, 6.5 km² | Ocean & reefs land, 3.0 km², plus ~6 km² of reef sea to the east | **Saltgate** | R5 / R6 |
| B3 The Crown | −5,400 to −2,400 | Storm peaks, 5.5 km² (includes the Crown's west half) | Swamp, 4.0 km² | **Stormgut** | R1 / R2 |
| B4 Old Island | −2,016 to +2,016 | Volcanic, 5.0 km² (BUILT) | Frozen tundra, 5.0 km² (BUILT) | **North pass, South pass** (BUILT) | Built |
| B5 Rootlands | +2,350 to +4,600 | Graveyard, 2.5 km² (catacombs ~2 km² below) | Ancient forest, 7.0 km² | **Rootgrave** | R3 / R4 |
| B6 Broken End | +4,600 to +6,400 | Corrupted wasteland, 6.0 km² | Crystal caves surface, 1.0 km² (caves ~2 km² below) | **The Severance** | R9 / R10 |

**The round order fits the map.** Each round opens the belt touching land that is already built: B3 and B5 touch the Old Island, B2 touches B3, B1 touches B2, and B6 touches B5. Nobody walks through unbuilt land.

**Totals.** About 49.8 km² of biome surface (CAL's 49.0 plus Wind's 0.8 km² cliff ground). About 5.4 km² of Spine and Crown cliffs (unwalkable, outside the biome totals). About 7 km² of caverns, catacombs and caves. About 5 km² of air and 6 km² of reef sea. Map frame: x −3,600 to +4,200, y −10,600 to +7,400.

**Where each biome sits.** This replaces the compass words in the FULL-GAME 3.2 headers.

| Biome | Where | Neighbours (open borders) | Opposite (via pass) |
|---|---|---|---|
| Fungal caverns | Far north-west, mostly underground | Desert (south), Sky (above) | Sky, via the Flue |
| Sky islands | Far north-east and in the air over the cape | Ocean (south), Fungal (below) | Fungal, via the Flue |
| Desert | North-west | Fungal (north), Storm peaks (south) | Ocean, via Saltgate |
| Ocean & reefs | North-east | Sky (north), Swamp (south) | Desert, via Saltgate |
| Storm peaks | West of centre, on the Crown | Desert (north), Volcanic (south, neck) | Swamp, via Stormgut |
| Swamp | East of centre, under the Crown's east face | Ocean (north), Frozen tundra (south, neck) | Storm peaks, via Stormgut |
| Volcanic | West, middle (built) | Storm peaks (north), Graveyard (south) | Frost, via the north and south passes |
| Frozen tundra | East, middle (built) | Swamp (north), Forest (south) | Fire, via the north and south passes |
| Graveyard | South-west | Volcanic (north), Void (south) | Forest, via Rootgrave |
| Ancient forest | South-east | Frozen tundra (north), Crystal (south) | Graveyard, via Rootgrave |
| Corrupted wasteland | Far south-west, the southern tip | Graveyard (north) | Crystal, via the Severance |
| Crystal caves | Far south-east, mostly underground | Forest (north) | Void, via the Severance |

**Climate logic (one rule, real geography).** The prevailing wind blows from the **east sea**. East-side biomes are wet: sea, swamp, snow, forest, crystal-cave drip. The west side is in the Spine's rain shadow: desert, ash, dry lightning, graveyard dust, wasteland. Storm fronts arrive from the east and cross the Spine an hour later (2.6).

### 2.3 The Spine, the Crown and the passes (GAP-COOKED)

**The Spine** is a hard wall like today's ridge: slopes over 35°, unclimbable (LP decision 3, SPEC). Only the passes, the Crown's Storm-side cliff paths and the necks cross or skirt it.

**Spine crest polyline** (x, y, crest height in m). The generator draws the ridge through these points.

| Point | x | y | Height |
|---|---|---|---|
| North headland (sea stacks "the Teeth") | 0 | −9,700 | 120 |
| The Flue rim | 0 | −8,800 | 700 |
| Saltgate saddle | +350 | −6,700 | **60** |
| North spine | +300 | −5,700 | 900 |
| Stormgut saddle | +200 | −4,900 | **420** |
| Crown, the Spire summit | −200 | −3,300 | **1,650** |
| Crown south shoulder | 0 | −2,100 | 600 |
| Today's ridge (BUILT, untouched where walkable) | 0 | −1,900 to +1,900 | 884 peak; passes 110 |
| Rootlands bend | −600 | +2,600 | 750 |
| Rootgrave saddle | −1,150 | +3,300 | **190** |
| South spine | −900 | +4,200 | 600 |
| Severance rift floor | +1,250 | +5,400 | **20** (a gorge cut to near sea level) |
| South tip ("the Broken End") | +1,350 | +6,500 | 120, then sea stacks |

**The Crown** is a massif 2.6 km across, centred at (−200, −3,300). It is the axis mundi of the Reach.

| Part | Height | What it is | Walkable from |
|---|---|---|---|
| The Spire | 1,650 m | A black granite horn, the tallest point in the world. Copper lightning rods of the old Rodwardens ring its top. **Kharvox's nest** (FULL-GAME: "nests on the highest summit") | Storm peaks cliff paths only |
| The Ring | 900 m | A wide terrace round the Crown's south and west sides holding the **Twelve Pillars** (2.12). Far-visible from B2 to B5 | Storm peaks cliff paths; one rope-bridge route from the Stormgut side |
| The Twelve Stairs | 0 to 700 m | Twelve ruined stairways climbing the Crown from every direction, each in one old people's stone. Every one breaks off at the same height, 700 m. Four are walkable to the break (from Storm peaks, Swamp, Volcanic, Frost) as viewpoints | Their own side |
| East face | 0 to 1,400 m | Sheer cliffs with five waterfalls dropping into the Swamp. Unclimbable | None |
| South shoulder | 300 to 600 m | Joins today's ridge north of the north pass. Its skirts carry the two north necks | The necks |

**The passes.** Each pass is its pair's borderland, invasion route and PvP hotspot (MP, SPEC rule). Built Fire–Frost passes are SPEC; the rest are GAP-COOKED.

| Pass | Pair | Centre (x, y) | Saddle | Length × width | Look | Pair blend |
|---|---|---|---|---|---|---|
| **The Flue** | Wind / Spore | (0, −8,800) | Vertical: cavern floor −40 m to rim 700 m, updraft to 1,300 m | 250 m wide shaft, 740 m deep | Þríhnúkagígur-style open volcanic throat. Walls ringed with fungus shelves low down and wind-scoured, bird-nested rock high up. A permanent warm updraft carries glowing spores out of the top like a slow chimney | Fungus crusts the underside of the nearest sky islands. Mistlarks and Puffwings nest on the upper walls. Spore clouds rise; gliders drop |
| **Saltgate** | Sun / Tide | (+350, −6,700) | 60 m | 1,400 × 300 m | A white salt flat in a low gap, with a stranded fleet of half-buried ships pointing west, the sea's old shore | Dunes spill east over the reef beach. Kelp wrack and shells lie on the western dunes. Dunejaws hunt the wet sand at the tide line |
| **Stormgut** | Lightning / Blood | (+200, −4,900) | 420 m | 1,600 × 120 m | A deep slot gorge between the Crown and the north Spine. Rain from the swamp side pours down it as waterfalls. Lightning-glassed walls | Struck, half-dead mangroves on the west mouth. Copper rods sunk in bog on the east mouth. Thunderstorms sit in the gut |
| North pass | Fire / Frost | (0, −1,296) | 110 m | BUILT | BUILT | Steam vents on the frost side, frost on the fire side (fire-frost cook) |
| South pass | Fire / Frost | (+82, +1,294) | 110 m | BUILT | BUILT | As above |
| **Rootgrave** | Necromancy / Nature | (−1,150, +3,300) | 190 m | 1,200 × 200 m | A green pass where giant roots grip an old cemetery, splitting tombs open | Roots and flowers on the graveyard side; leaning headstones and lanterns deep in the forest side; Gravehounds and Thornlynx fight here at dusk |
| **The Severance** | Void / Arcane | (+1,250, +5,400) | 20 m | 1,100 × 80 m | A rift-cut gorge that splits the Spine's last stretch. Crystal veins on its walls are bleached grey where the void touches them | Crystals grow out of grey dead ground. Floating rubble drifts into the crystal mouths. Ward fields flicker at the gorge's east end |

**Every pass gets the same kit** (GAP-COOKED, from the fire-frost war pattern):

- **Two pass forts**, one at each mouth, each built by its side's old people and facing the other side. Arrow slits and weapon racks point across.
- **The mirrored champions.** Each pass holds two skeletons of the old champions, each killed by its opposite element and lying on the wrong side. Fire-frost's Fallen Smith and Burnt Wyrm are this rule for the Fire–Frost pair.
- **A lesser shrine at each mouth.** These are two of each biome's 5 shrines (fire-frost 3.1). That gives fast travel to a pass only after you have walked it.
- **Mixed creatures.** Each biome's midland table at 50% weight inside the pass, and the two sides fight (SPEC, MP).
- **No claims** inside the pass corridor (2.11).
- **A pass music duet** (2.13).

**Necks and sounds (the joins to today's island, GAP-COOKED).**

- Today's north and south coasts stay coast. Four sea **sounds**, 350 to 450 m wide, separate the Old Island from B3 and B5: the **Ash Sound** (north-west), **Rime Sound** (north-east), **Bone Sound** (south-west) and **Pine Sound** (south-east).
- Four land **necks**, 450 m wide, join them on the Spine's skirts:
  - North-west neck: x −700 to −250, y −1,950 to −2,400.
  - North-east neck: x +250 to +700, y −1,950 to −2,400.
  - South-west neck: x −700 to −250, y +1,950 to +2,350.
  - South-east neck: x +250 to +700, y +1,950 to +2,350.
- Every built spawn camp keeps open water. The generator never raises sea within 500 m of a camp. The nearest camp to any neck is about 720 m away.
- Each neck has an old **neck road** with markers from both neighbours' peoples (2.12).

**Neighbour borders inside a side (GAP-COOKED).** Where two belts meet on the same side, a low **rib spur** comes off the Spine (200 to 350 m high) with two gaps cut by rivers. There is a 150 to 250 m blend strip with both ground sets, both prop kits at half density and both biomes' lowland and midland creatures at half weight. On the B1/B2 and B5/B6 borders, the rib is replaced by:

- **Fungal–Desert:** a karst field of sinkholes in the sand. Dunes pour into cavern skylights.
- **Graveyard–Void:** a grey gradient where graves give way to withered forest and corruption builds (both biomes have corruption, DD SPEC).

### 2.4 Inside a biome: bands, sub-zones, spawns (rules, GAP-COOKED)

**Bands (SPEC idea, generalised).** Outer lowlands are by the coast (spawns, easy creatures). Midlands hold minis, caves and ore. Highlands sit against the Spine (major boss, hardest creatures). Each band is measured as distance from the Spine polyline, exactly as `island_walk.json` measures distance from the ridge today:

| Band | Distance from the Spine crest | Surface biomes | Vertical biomes |
|---|---|---|---|
| Lowland | 900 to 1,900 m | Coast and outer plains | Sky: islands under 750 m altitude. Fungal and Crystal: entrance halls 0 to 60 m deep |
| Midland | 450 to 1,050 m (overlap blends) | Middle ground, most caves | Sky: 750 to 1,050 m. Underground: 60 to 180 m deep |
| Highland | 140 to 620 m | Against the Spine, the major's arena | Sky: over 1,050 m. Underground: deepest halls |

Where a biome is narrower than 1.9 km (Graveyard, Crystal, Fungal, Wind's ground), the bands squeeze proportionally. Vertical biomes swap distance for altitude or depth.

**Sub-zone rules.** CAL's sub-zone lists are SPEC; these are the rules every biome cook builds to.

1. **3 to 5 sub-zones** per biome, each at least 0.6 km² on the surface or 0.3 km² underground or in the air.
2. **Own identity:** 2 to 3 ground layers, a 3-colour palette (hex), at least 3 prop kits unique to it, a day and a night creature table, an ambient bed, and one signature silhouette visible from its border.
3. **One micro-hazard at most** per sub-zone. Each biome has 3 to 4 in total (fire-frost 3.1 SPEC).
4. **One lesser shrine** per non-great sub-zone (fire-frost 3.1 SPEC).
5. **Blends** between sub-zones are 60 to 120 m wide. There are no hard texture lines.
6. **Bands and sub-zones cross.** A sub-zone may span bands, and difficulty follows the band, not the sub-zone.
7. **Placement:**
   - Sub-zones that need the sea (beaches, wrecks, tide pools, reed flats) sit on the outer coast.
   - Sub-zones that need height (crags, obsidian ridges, canopy) sit against the Spine.
   - Each biome's hidden sub-biome sits under or inside its highland.

**Spawn camps (SPEC: three per biome on the outer coast, safe zones).** Positions for the new biomes are GAP-COOKED. Each camp is within a 4 to 6 minute walk of its biome's pass or neck (MP's 5-minute rule), with the middle camp moved toward the pass (LP decision 2).

| Biome | Camp 1 | Camp 2 | Camp 3 |
|---|---|---|---|
| Storm peaks | (−2,250, −2,700) | (−2,300, −3,700) | (−2,150, −4,700) |
| Swamp | (+2,250, −2,750) | (+2,250, −3,800) | (+2,150, −4,800) |
| Desert | (−2,550, −5,900) | (−2,550, −6,700) | (−2,350, −7,600) |
| Ocean & reefs | (+1,650, −5,800) | (+1,700, −6,650) | (+1,550, −7,500) |
| Fungal (on sinkhole rims; the caverns are below) | (−1,800, −8,300) | (−1,650, −8,900) | (−1,100, −9,300) |
| Sky (cliff-top camps, each with a Kitewright lift, 2.5) | (+1,600, −8,300) | (+1,500, −8,900) | (+1,000, −9,300) |
| Graveyard | (−2,450, +2,750) | (−2,450, +3,500) | (−2,350, +4,300) |
| Ancient forest | (+2,300, +2,800) | (+2,350, +3,600) | (+2,250, +4,400) |
| Corrupted wasteland | (−2,550, +5,000) | (−2,400, +5,800) | (−1,600, +6,300) |
| Crystal (cliff mouths over the sea) | (+2,250, +4,900) | (+2,200, +5,500) | (+2,000, +6,000) |

**Major boss arenas and hidden sub-biomes on the map.** Positions are GAP-COOKED. The fights belong to the biome lanes.

| Biome | Major arena | Hidden sub-biome (under or inside the highland) |
|---|---|---|
| Storm peaks | Kharvox: the Spire's summit nest, (−200, −3,300), 1,600 m | The Hollow Spire: inside the Spire, entered from the Ring |
| Swamp | Vharra: a flooded sinkhole under the Crown's east face, (+900, −3,600) | The Sunken Coven, under the deepest pool, (+1,300, −3,100) |
| Desert | Ahrumet: the sundial plaza, (−600, −6,600), 600 m from Saltgate | Buried ruins under the dunes, (−1,500, −7,000) |
| Ocean & reefs | Thalassor: a tidal lagoon, (+1,000, −7,300) | The Drowned Hall, under the reef, (+2,600, −6,500) |
| Fungal | Mycelorn: the Heart chamber at the cavern bottom, (−600, −8,600), −220 m | The Heart Grotto ring around it |
| Sky | Aeolith: a ring of drifting platforms over the Flue, (0, −8,800), 1,250 m | The Undercloud: a cloud bank at 500 to 600 m, (+2,600, −9,600) |
| Graveyard | Nhal: the ruined chapel, (−1,500, +3,800) | The catacombs under the grave fields |
| Ancient forest | Sylvaroth: the clearing of four heart-trees, (−500, +3,600) | The Hollow Root, under the oldest tree, (−300, +3,000) |
| Corrupted wasteland | Vorhollow: a collapsing island over the biggest rift, (+300, +5,600) | The Unmade Place, inside that rift |
| Crystal | Lyrathis: the hall of prisms, (+1,700, +5,300), −120 m | The Silent Vault, (+1,600, +5,900), −180 m |
| Volcanic, Frost | BUILT (crater, glacier bowl) | Lava tubes, ice caves (fire-frost cook) |

### 2.5 Travel (GAP-COOKED unless tagged)

**Speeds and times.** The numbers come from `tuning.json` and `creatures.json` (SPEC where built).

| Mode | Speed | Tag |
|---|---|---|
| Walk / sprint | 4.5 / 7.5 m/s | BUILT |
| Ashstrider / Frostantler / Emberhorn | 10 / 8.5 / 8 m/s | BUILT |
| Haulers (Basaltback, Woolly drudge) | 3.8 / 4.0 m/s | BUILT |
| Mount on an old road | +15% | GAP-COOKED |
| Swimming | Built `water.swimSpeed` | BUILT |
| Raft / Skiff / Longboat | 3.5 rowed; skiff up to 9 and longboat up to 12 under sail | GAP-COOKED |
| Crude / levistone glider | 8 / 11 m/s forward | GAP-COOKED |
| Flying mount (Windmane, R7) | 14 m/s | GAP-COOKED |

| Trip | On foot | Fast mount | Notes |
|---|---|---|---|
| Spawn camp → own pass or neck | 4 to 6 min | 2 to 3 min | MP rule |
| Across one pass, mouth to mouth | 3 to 6 min | 2 to 3 min | |
| One belt to the next along a coast | 6 to 8 min | 4 to 5 min | |
| Hollow Cape to Broken End along the west coast (~16.5 km) | ~45 min | ~28 min | The world feels big |
| Skiff from the B3 east coast to the B2 reef with a tailwind (~3 km) | | 6 min | |
| Shrine to shrine | 10 s load | | Only discovered shrines |

**Old roads (GAP-COOKED).** Each old people built roads, and these are the routes worth walking:

- **Network.** One **coast road** per side runs the length of the map, linking all three camps of each biome. One **pass road** runs through each pass. Every biome also has a **shrine road** linking its 5 shrines.
- **Look.** Packed earth or stone, 4 to 6 m wide, worn and broken in places, with each people's waymark every 120 to 200 m (2.12). The fire-frost slag-cairn and inuksuk roads are the first two.
- **Rules.**
  - Mounts get +15% speed on road cells.
  - Wildlife spawns no closer than 40 m to a road.
  - Building pieces cannot be placed on road cells. Claims may sit beside a road.
- **The merchant caravans walk the coast roads** (2.7).

**Gliding (GAP-COOKED; Raven's Wind theme "gliding" is SPEC).**

| | Crude glider | Levistone glider |
|---|---|---|
| Where | Workbench, Hearth 2: wood ×12, fiber ×20, hide ×6, copper ingot ×2 | Workbench, Hearth 3: levistone ×6, skysilk ×4, iron ingot ×2, crude glider ×1 (R7) |
| Opens | Jump while falling, 2 m or more above ground | Same |
| Forward / sink | 8 m/s / 2.6 m/s (ratio ~3:1) | 11 m/s / 2.0 m/s (ratio ~5.5:1) |
| Stamina | 5 per second; none inside an updraft | 3 per second; none in an updraft |
| Featherfall (Wind root, SPEC) | Halves stamina cost; no fall damage when the glider closes under 20 m | Same |
| Combat | Bows and thrown items only. Any hit taken closes the glider | Same |
| Wind | Drift = global wind × 0.15 | Drift = wind × 0.1 |
| Durability | 300 s of glide | 900 s |

**Updrafts** are visible as rising dust, ash, leaves, spores or circling birds, and their sound rises with height:

| Updraft kind | Where | Lift | Top |
|---|---|---|---|
| Lava thermals | Over lava fields and vent clusters (Volcanic) | +4 m/s | 120 m over the ground |
| Ridge lift | Over the Windward cliffs, and the Crown's south and west faces when the wind is from the east | +8 m/s | 400 m over the cliff top |
| Pass draughts | In each pass when the wind lies along it | +3 m/s | 80 m |
| The Flue | Always on | +12 m/s | 1,300 m altitude |
| Sky island columns | Between islands, 3 to 5 per big island | +10 m/s | 150 m over the island |

**Getting up to the sky without a glider (GAP-COOKED).** The three Sky camps each have a **Kitewright lift**: an old counterweight cage on chains up the Windward cliff to the first island at 520 m. Hold Interact to crank. It takes 40 s and carries 4 people or 1 hauler. A Wind-path newcomer reaches the islands on day one. Levistone shards (the glider's material) lie at the cliff foot, shed by the islands above.

**Boats (RAVEN-PLANNED: "siege weapons and boats", BP Crafting; numbers GAP-COOKED).**

| Boat | Recipe | Crew / seats | Storage | Speed | HP | Notes |
|---|---|---|---|---|---|---|
| Raft | Workbench, Hearth 1: wood ×24, fiber ×30 | 2 | 4 slots | 3.5 m/s rowed | 400 | Capsizes in waves over 1.5 m (open sea in a squall, tidal races) |
| Skiff | Workbench, Hearth 2: wood ×60, hide ×10, copper ingot ×6, fiber ×40 | 3 | 12 | Sail 0 to 9 m/s, row 3.5 | 1,200 | Crosses races only at slack water |
| Longboat | Workbench, Hearth 3: wood ×140, iron ingot ×16, tidecoral ×12, hide ×20 (R6) | 6, plus 1 mount or hauler kneeling on deck | 30 | Sail 0 to 12 m/s, row 4 | 4,000 | Crosses races any time at half speed |

- **Placing.** A boat is placed from the bag onto water within 10 m of the shore. It is allowed outside claims, and the owner's clan can use it.
- **Saving.** Boats persist in the world save like pieces.
- **Sinking.** At 0 HP a boat sinks. Its storage floats up as a sack that lasts 10 real minutes.
- **Sailing.**
  - The sail is up or down, and you steer with move left and right. Speed is the boat's sail top speed × the wind factor below.
  - With the sail down you row at the row speed.
  - The HUD shows a wind arrow on the compass while you are aboard.

| Wind angle off the bow | Sail factor |
|---|---|
| Tailwind, 150° to 180° | 1.0 |
| Broad reach, 110° to 150° | 0.95 |
| Beam reach, 70° to 110° | 0.85 |
| Close-hauled, 35° to 70° | 0.45 |
| Into the wind, under 35° | 0 (row) |

**The sea (GAP-COOKED).**

| Water | Where | Rules |
|---|---|---|
| Shallows | Within 150 m of shore | Safe to swim. Built swimming applies |
| Sounds | The four sounds | Swimmable. **Rime Sound and the tundra coast are icy:** 8 frostbite build-up per second while swimming (status, not a meter). The Frost path, frost gear or a Floeback passes |
| Open sea | Over 600 m from shore | Deep water (hazard table). Swells 1 to 3 m. One Reefjaw pack per 1 km² on the east sea (Tide lane species) |
| Tidal races | **The Gullet** off the Hollow Cape tip, **the Broken Race** off the Broken End tip | 5 m/s cross-current, standing waves. Slack water 05:30 to 06:30 and 17:30 to 18:30 game time (2.5 real minutes each, twice a game day). Raft: capsizes outside slack. Skiff: slack only. Longboat: any time at half speed |

So boats can go round the Spine's ends, but the passes stay the main route.

**Mounts on the map (GAP-COOKED rules; species SPEC per biome).**

- **Hazards.** Every ground mount carries its biome's hazard resistance (SPEC, CR). Taking a mount into a foreign hazard hurts it like a player.
- **Swimming mounts** (Floeback, Wavecharger; R6) carry a rider through sounds, the open sea and races at slack.
- **Flying mounts (R7, Windmane first).**
  - 14 m/s, ceiling 1,800 m.
  - Flight stamina lasts 90 s of powered flight. Gliding down costs nothing, and stamina refills on the ground.
  - In a Storm peaks thunderstorm, strikes target flyers above 600 m first.
  - A hit on a wing makes the mount glide down.
  - Flyers can't enter boss arenas: an arena roof volume dismounts the rider at the door.
- **Fast travel (SPEC, fire-frost 3.1).** Discovered shrines only, no travel in combat, tames within 30 m come along. **Map rule:** you can't travel to a shrine held by an invasion, and you can't travel into a sealed belt.

### 2.6 Weather (RAVEN-PLANNED: "weather that matters: storms, blizzards, heat waves"; design GAP-COOKED)

**How it works**

- **Per biome.** Each biome rolls its own weather in 3 game-hour blocks (7.5 real minutes).
- **Deterministic.** States come from the world seed, biome id and block number, so the server and every client compute the same sky with nothing to sync (Valheim).
- **Changes are visible early.** A change builds over 1 game hour (2.5 real minutes): cloud fronts roll in from the east, fog creeps up the valleys, ash darkens the crater.
- **Wind.** There is one global wind vector. It blows from the east (±60°) and turns by up to 40° every 2 game hours. Speed is 2 to 14 m/s, or 15 to 25 m/s in a storm state. The wind drives sails, glider drift, sand, spores, smoke, foliage and cloth.
- **Fronts cross the Spine (rain shadow).** When an east-side biome rolls Rain or a storm state, its opposite across the Spine rolls its storm state 1 game hour later with a 50% chance.
- **Underground.** No weather below 30 m depth. The surface weather is heard muffled through cave mouths.
- **No meter, no floors.** Weather changes fights and gear checks. It never adds a temperature bar (DD rules out temperature).
- **Plenty after rough weather** (MH Wilds). The block after a storm state gives +50% gather yield of the biome's signature resource and a dew or wet-ground look.

**Shared weather states.** "Exposed" means no roof within 30 m straight up.

| State | Sight | Effects |
|---|---|---|
| Clear | Full | None |
| Overcast | Full | Light −30%. Radiance (Sun path) charges ×0.6 |
| Rain | 400 m | **Wet** on exposed bodies: fire damage dealt and taken ×0.8, Burn build-up ×0.7, Frostbite build-up ×1.15, shock damage taken ×1.2. Crops count as watered. Stealth noise radius ×0.7. Torches burn ×1.5 faster. Wet lasts 60 s after you get under cover |
| Fog | 60 to 80 m | Creature sight aggro range ×0.7. Stealth detection ×0.6. Sounds carry normally (you hear them before you see them) |
| Snow | 200 m | Footprints show for 120 s (yours, creatures', the Pale Hunter's) |
| Gale | Full | Gust pushes of 600 cm/s for 0.5 s every 10 to 20 s on exposed ground above 300 m altitude. Each gust has a 1.0 s whistle tell. Gliders drift at wind × 0.3 |
| Heat haze | Mirage shimmer beyond 150 m | Thirst drains ×2 in the sun. Shade, a sand veil or the Sun path cancel it |

**Weather per biome.** Weights are percentages per block; the **bold** state is each biome's signature weather. Biome events (2.8) are separate and bigger.

| Biome | Clear | Overcast | Rain/Snow | Fog | Signature state (weight) and its effect |
|---|---|---|---|---|---|
| Volcanic | 40 | 20 | 10 rain | 0 | **Ashfall** (30): sight 150 m, grey light, Glowmoths fly by day, ember shrubs smoulder. Heat haze 10% inside Clear |
| Frozen tundra | 35 | 20 | 25 snow | 10 freezing fog | **Blizzard** (10): sight 60 m, run ×0.9 in deep snow, Gloomhowl aggro range ×1.3. Lighter than the Whiteout event |
| Storm peaks | 25 | 20 | 15 rain | 10 | **Thunderstorm** (30): Strike zones are live (hazard table), plus Gale above 300 m |
| Desert | 50 | 5 | 0 | 0 | **Heat wave** (30): Heat haze plus Radiance ×1.2. **Sandstorm** (15): the belt's effect over the whole desert (vision 25 m, run ×0.7), moving with the wind |
| Ancient forest | 35 | 20 | 30 rain | 15 morning mist | Signature is **Rain** itself: crops +25% growth for the block, Truffler finds double |
| Sky islands | 45 | 10 | 10 rain | 15 cloud fog (islands under 800 m) | **Gale** (20): gust hazard everywhere; Featherfall or anchor boots pass |
| Crystal caves (surface) | 40 | 20 | 30 rain | 10 | Underground: none |
| Swamp | 15 | 20 | 25 rain | 25 | **Miasma** (15): fog with a red cast, sight 40 m. Murkjaws hunt in water within 2 m of boardwalks |
| Ocean & reefs | 40 | 15 | 15 rain | 15 sea fog | **Squall** (15): rain plus wind 20 m/s, swells 2 to 3 m (rafts capsize), lightning flashes with no strikes. Wrecks wash up after (2.7) |
| Graveyard | 15 | 35 | 15 drizzle | 35 ground mist | Signature is the **ground mist**: Gravehounds rise out of it at night |
| Fungal (surface) | 30 | 20 | 20 rain | 0 | **Spore drift** (30): spore clouds (Spore hazard) spread 200 m round every sinkhole downwind |
| Corrupted wasteland | 20 | 40 grey | 0 | 10 dust | **Dead calm** (30): no wind, no ambient sound, footsteps doubled in the mix. Hollowfangs can hear you at ×1.5 range. Silence used as threat, matching the music rule |
| The Crown (above 600 m) | Follows Storm peaks | | | | Ridge lift live on its east and south faces in an east wind |

**Weather sound and light**

- Rain sound is layered by surface (leaf, stone, water, canvas, tent).
- Thunder follows the flash at 1 s per 340 m of real distance.
- Wind rises with altitude. The surf bed follows distance to the coast.
- Every state has a skybox, cloud cover and fog preset in WRSky.

### 2.7 World events (RAVEN-PLANNED where named; design GAP-COOKED)

Raven rules out server-wide boss events, a server-ending world boss, world-wide escalation, first-kill announcements and natural disasters (DD Ruled out). None of these events touch bases, and none is a boss event. Server setting **World events: Off / Rare / Normal / Frequent** (default Normal; frequencies below are Normal, ×0.5 for Rare, ×2 for Frequent).

| Event | Tag | When | Warning (world-visible) | What happens | Pays |
|---|---|---|---|---|---|
| **Starfall** (meteor shower) | RAVEN-PLANNED (DD "meteor showers that drop rare ore") | Night, once every 2 real days | From dusk: streaks across the whole sky for 5 min | 3 to 5 meteors land in random midland or highland spots of built biomes, never within 400 m of a claim, camp, shrine or arena. Each leaves a 20 m smoking crater with a glowing **star-iron** core for 1 game day. After 2 game hours the glow draws one or two of the nearest biome's midland packs | Per crater: star-iron ×6 to 10 (starsteel ore once Hearth 5 exists, R4; before that iron ×10 and a gem) |
| **The Migration** | GAP-COOKED | Once every 2 real days per pass | Herd calls and dust on the move 5 min before | 8 to 14 neutral haulers or mounts of one biome walk through a pass or neck into the neighbour or opposite and back over 1 game day. A predator pack of either biome trails 300 m behind and hunts stragglers (living world, SPEC) | Migrants tame ×2 faster by offer-food methods. Kills of the trailing pack pay double hide and meat |
| **Wreck tide** | GAP-COOKED | After every Ocean Squall, and 1 in 4 rain blocks elsewhere on the east coast | The squall itself | 1 or 2 wrecks wash up on a random outer coast at least 300 m from camps: a broken skiff or a castaway longboat with a chest, rope, iron nails and a lore item. Spinecrabs or Gullraiders claim it after 1 game hour | Wood, iron ×4 to 8, hide, a recipe scrap or lore item |
| **The Dimming** (eclipse) | GAP-COOKED | Once every 7 real days, at noon | The sun's edge bites in over 5 min. Birds go silent | 3 game hours (7.5 real min) of dusk light across the whole map. Night spawn tables run. Radiance stops charging. Corruption builds at half rate in the open | Path XP ×1.5 for kills during it. Night-only plants can be gathered by day |
| **Merchant caravans** | RAVEN-PLANNED ("traveling merchants", DD) | Always, from R3 (villages) | Bells and hooves on the road | One caravan per side walks the coast road village to village at 3 m/s: 2 merchants and 2 pack beasts, stopping 1 game hour at each village and pass mouth. Creatures may attack it on the way. If it is killed, it drops 10% of its stock and respawns at its home village the next game day | Trade (villages cook) |
| **Invasions** | RAVEN-PLANNED (DD; numbers in FULL-GAME 3.3, kept) | Server setting (3.3) | 5 min warning to the defending biome | Through the pair's pass (Fire–Frost: either pass). **Wind–Spore:** Spore invaders climb the Flue walls and spill onto the nearest islands over mycelium bridges that grow for the event; Wind invaders dive down the Flue into the caverns | 3.3 SPEC |
| **Biome events** (12) | RAVEN-PLANNED (DD) / framework from the fire-frost cook | One game day, about once every 3 real days per biome (fire-frost SPEC) | Each biome's world-visible sign, **plus its pillar on the Crown's Ring burning in its colour** (2.12) | Per biome cook | Signature resource ×3 (3.3 SPEC) |

**Never at once.** At most one world event and two biome events run at the same time on a server. Invasions don't count toward that limit.

### 2.8 Day and night

| Item | Detail | Tag |
|---|---|---|
| Clock | 60 real minutes per game day. Dawn 06:00, dusk 20:00, 1 h twilight either side. Server owners can change day length (SS) | BUILT (`tuning.json` `clock`) |
| Day and night spawn tables | Per biome and sub-zone. At night 30 to 50% of a sub-zone's species change, nocturnal hunters get aggro range ×1.25, and diurnal herds sleep in rings (a sneak-tame window) | RAVEN-PLANNED (tables) / rules GAP-COOKED |
| Moon | An 8-game-day cycle: new, crescent, half, gibbous, full and back. Night light follows the phase: moonLux 0.15 at new moon up to 0.5 at full (0.5 is the built value). The moon rises in the east over the sea | GAP-COOKED |
| Readable nights | Every biome has its own night light so silhouettes read: lava glow (Fire), aurora (Frost), sulfur blue fire, lightning flicker (Storm), sunstone glow (Desert), glowworm and foxfire banks (Forest), bioluminescent surf (Ocean), lanterntoads (Swamp), tōrō lanterns (Graveyard), crystal hum light (Crystal), spore glow (Fungal), stars close overhead and moonlit cloud tops (Sky), rift edge glow (Void) | GAP-COOKED |
| Underground | Cavern bioluminescence follows the surface clock: brightest at surface midnight, dimmest at noon (the real fungal circadian glow). Underground spawns use the night table from 22:00 to 04:00 | GAP-COOKED |
| Night POI rule | Every POI has a light or a skyline shape that reads at night | SPEC (fire-frost 3.1) |
| Seasons | Dream tier, not in this cook | RAVEN-PLANNED (BP Dream) |

### 2.9 Density and POI types (GAP-COOKED; numbers scale the fire-frost SPEC)

**Density rules, every biome**

| Rule | Number |
|---|---|
| Surface POIs | 23 per km² (fire-frost's 115 per 5 km²), with at least 150 m between any two |
| Max gap | No point on any walkable route, road, glide line between islands, cavern route or boat lane along the coast is more than 125 m from a POI. So no stretch of 250 m is empty |
| Underground | One POI per 150 m of main route |
| Sky | Every island has at least 1 POI and islands over 150 m across have at least 3. Islands are at least 40 m across and at most 220 m apart horizontally, so a crude glider can make each hop |
| Sea | 6 POIs per km² of open sea and reef: wrecks, sea stacks, reef heads, buoys, drowned ruins |
| Skyline landmarks | One per ~0.6 km², each a silhouette readable from 1 km |
| Super landmark | Each major boss lair reads from 3 km or more (2.12). The Crown reads from everywhere |
| Siting | Every POI sits on a rise or round a bend, so the land reveals it (triangle rule) |
| Variety | About 15 of the 20 POI types per biome, each kit type with 2 to 4 variants |
| Mix per biome | ~7% skyline landmarks, ~4% shrines, ~23% hand-made story sites, ~66% kit POIs (the fire-frost ratio) |

**POI counts.**

| Biome | Surface | Underground / air / sea | Total |
|---|---|---|---|
| Volcanic | 115 | (lava tubes, fire-frost) | 115 (SPEC) |
| Frozen tundra | 115 | (ice caves, fire-frost) | 115 (SPEC) |
| Storm peaks | 127 | Hollow Spire 12 | 139 |
| Swamp | 92 | Sunken Coven 10 | 102 |
| Desert | 150 | Buried ruins 20 | 170 |
| Ocean & reefs | 69 | Sea 36, Drowned Hall 8 | 113 |
| Fungal caverns | 35 | Caverns 80 (12 km of route) | 115 |
| Sky islands | 18 (cliff ground) | Islands 46 (≈40 islands) | 64 |
| Graveyard | 58 | Catacombs 60 | 118 |
| Ancient forest | 161 | Hollow Root 14 | 175 |
| Corrupted wasteland | 138 | Unmade Place 8 | 146 |
| Crystal caves | 23 | Caves 53 (8 km of route) | 76 |
| The Crown (shared) | Ring, Stairs, Pillars, pass forts | | 30 |
| **Total** | | | **≈1,480** |

**The 20 POI types** (the shared kit every biome picks about 15 from):

| # | Type | What it is | Pays or does |
|---|---|---|---|
| 1 | Skyline landmark | A silhouette that reads from 1 km: a skeleton, a broken tower, a giant tree, a sky-bridge stub | Orientation. Often holds a type-5 site |
| 2 | Spawn camp | Coastal safe zone (SPEC) | Respawn, starter gather |
| 3 | Shrine (great or lesser) | SPEC (fire-frost 3.1) | Path XP, fast travel |
| 4 | Cave mouth | 3 to 6 per biome (SPEC), ore by depth | Ore, darkness, a cave pack |
| 5 | Story site (hand-made) | A scene that tells one event with no words: a burnt caravan with guards' weapons facing out | A named item, lore item, recipe scrap |
| 6 | Ruin | The old people's buildings: a house row, kiln, watchtower, temple | Containers, a vista, a pack |
| 7 | Wreck | Caravan, ship, sky-bridge span, crashed cage lift | Salvage: wood, iron, rope |
| 8 | Nest or den | A creature home with eggs or young | Steal-egg taming, an aggressive guard |
| 9 | Kill set-piece | A live predator on prey (living world sim) | A fight, or a free carcass if you wait |
| 10 | Resource outcrop | Signature resource cluster or rich ore vein | Gathering |
| 11 | Vista | A high point that reveals 400 m of the fill-in map | Map reveal |
| 12 | Hazard teacher | A small spot that shows the biome's hazard before you meet it big: one lava tongue, one ward field | Learning the tell |
| 13 | Cache | A hidden stash behind an environmental clue (scratch marks, a cairn pointing at a crack) | Gear, gems, shards |
| 14 | Mini lair / roaming rest | Where a mini sleeps or rests on its route | The fight |
| 15 | Water and forage | Spring, oasis, berry grove, fish run | Food and water |
| 16 | Waymarker road | The people's markers along a road; counts as kit POIs (fire-frost SPEC) | Wayfinding; lights at night when you pass |
| 17 | Message stone | A note from "the last one here" (`AWRMessageStone`, ui-controls lane); later, player messages | Hints, warnings |
| 18 | Boss approach | The run-back stone and arena door (combat lane SPEC) | Retry point |
| 19 | Pass fort | One at each pass mouth (2.3) | Pass war story, a vista, a pack |
| 20 | Village or merchant stop | Newcomer villages (R3 on) | Trade, quests |

**Village sites (placement only; contents need their own cook).** One newcomer village per biome, on its lowland coast next to its middle camp, about 250 m from it. The village is a safe zone like the camps. The largest village is in the Ancient forest at (+2,100, +3,450) (FULL-GAME: "the main NPC village starts here").

### 2.10 Hazards table, whole map

Raven's rules (SPEC) apply throughout:

- Hazards are gear checks with no temperature meter.
- Creatures resist their own biome's hazard.
- No safety floors.

Every hazard gives at least 0.9 s of tell with sound and a small visual cue. The **early pass** column is GAP-COOKED: a cheap consumable so any path can cross before having the gear, as the fire-frost cook did with Cinder salve (BotW elixirs). Biome cooks set its final recipe.

| Biome | Main hazard | Numbers | Gear and path passes | Early pass (consumable, 10 real min unless noted) | Where on the map |
|---|---|---|---|---|---|
| Volcanic | Lava | BUILT (`hazards`; fire-frost raises to 20 dps) | Burning blood, fire gear, volcanic mount (BUILT) | Cinder salve (fire-frost SPEC) | Lava river south, crater, Flooded Tube |
| Frozen tundra | Slick ice | BUILT | Spiked boots, frost gear, tundra mount | Rime flask (fire-frost SPEC) | Ice rivers, lakes, glacier |
| Storm peaks | Strike zones in thunderstorms | Above 600 m: a strike every 20 to 40 s on the highest player in each 60 m cell, with a 1.0 s ground-glow and crackle tell, then 60 damage and a 0.8 s stun (FULL-GAME 3.3, kept). Flyers above 600 m are picked first | Grounded (root); grounding charm; a tamed Rodcrest within 30 m of a base | **Grounding salve:** no stun, strike damage ×0.5 | The Crown above 600 m, cliff paths, Stormgut |
| Desert | Sandstorm belt and soft dunes | Belt: vision 25 m, run ×0.7, no Radiance fill. Quicksand: sink over 4 s, dodge out | Sandwalker (root); sand veil | **Dune oil:** quicksand frees you on the first dodge, sandstorm slow ×0.85 | The belt on the west coast edge, dune seas |
| Ancient forest | Thornbrakes | 6 bleed build-up per second, run ×0.5 | Nature path; briarproof leathers; a torch burns a 4 m gap for 5 game hours | **Briar balm:** bleed build-up ×0.3 | Old-growth groves, round heart-trees |
| Sky islands | Falls and gales | Lethal falls off an island; gusts of 1,200 cm/s for 0.8 s with a 1.5 s whistle tell | Featherfall (root); glider; anchor boots | **Featherdown draught:** fall damage ×0.5 | Island edges, sky-bridges |
| Crystal caves | Ward fields, full dark | 25 damage and a 600 cm throw every 1.5 s | Arcane path; ward charm | **Attuning chalk (5 min):** no throw, damage ×0.5 | Ward chambers, the Severance east end |
| Swamp | Bleedwater | 10 bleed build-up per second while wading | Sealed waders; swamp mount | **Leech tar:** build-up ×0.3 | Murk pools, red pools |
| Ocean & reefs | Deep water | Breath 30 s; below 25 m depth, 5 damage per second | Gills (root); diving helm; swimming mount | **Diver's draught:** breath 90 s | Reefs, the Drowned Hall, open sea |
| Graveyard | Darkness and corruption | Raven's corruption (DD). Outside light, Gravehounds and Wraithmoths hunt at double aggro range | Light; the Necromancy path turns corruption into a buff (SPEC) | **Lantern oil:** light radius ×1.5, corruption gain ×0.5 | All of it, worst in the catacombs |
| Fungal caverns | Spore clouds | 12 poison build-up per second; rot after 10 s inside | Immune system (root); filter mask | **Spore wad (5 min):** build-up ×0.3 | Spore gardens, sinkholes, Spore drift weather |
| Corrupted wasteland | Rift zones | Max HP −2 per second inside, restored by resting | Empty vessel (root); nullward halves it | **Anchor salt (5 min):** drain ×0.5 | Rifts, the Unmade Place, the Severance west end |

**Map-level hazards (GAP-COOKED).**

| Hazard | Where | Effect | Passes with |
|---|---|---|---|
| Open sea | Over 600 m from shore | Swells; capsizes rafts in squalls; Reefjaw packs on the east sea | A skiff or longboat; swimming mounts |
| Tidal races | The Gullet, the Broken Race | 5 m/s current, standing waves (2.5) | Slack water; a longboat |
| Icy water | Rime Sound, the tundra coast | 8 frostbite build-up per second while swimming | Frost path, frost gear, Floeback |
| Falls | The Crown, sky islands, Stormgut, sea cliffs | Built fall damage, lethal from cliffs | Glider, Featherfall |
| Darkness | Every cave and underground biome | DD SPEC: caves need light | Torch, lantern, light tames |
| Weather hazards | 2.6 states | Gales, strikes, sandstorms, miasma | Per state |

**The pass rule.** In a pass, both sides' hazards are live: steam vents on the frost side and ice on the fire side (SPEC from fire-frost), thunderstorms and bleedwater in Stormgut, wards and rifts in the Severance. **No helpers are added after a death.**

### 2.11 Claims and the open map (GAP-COOKED)

So nobody on a persistent server can wall off the world:

| Zone | No claim totem within | No building pieces on |
|---|---|---|
| Pass corridor | 150 m of the pass centre line | The corridor |
| Neck | 100 m of the neck road | The road |
| Roads | Claims allowed beside them | Road cells |
| Shrines | 200 m | |
| Boss arenas and run-back stones | 250 m | |
| Spawn camps and villages | Their safe zone plus 150 m (SPEC safe zones) | |
| The Flue rim, the Ring, the Kitewright lifts | 200 m | |
| Underground (caverns, catacombs, crystal caves, lava tubes, ice caves) | Everywhere: Raven rules out underground bases (DD) | Everywhere except rope ladders (fire-frost SPEC) |
| Sky islands | Allowed on islands 120 m or more across | |
| Boats | Not a claim; clan-owned | |

### 2.12 The story the land tells (GAP-COOKED, environment only)

No staged scenes, no cutscene actors, no narrator (DD World, SPEC). It is all ruins, skeletons, roads and placement.

**1. The body.** The Reach is a fallen giant lying face-up (the Ymir motif fire-frost already uses).

- **The Spine's crest.** It is lined with vertebra-shaped rock towers every 300 to 500 m.
- **The rib spurs.** They curve like ribs.
- **The bone strata.** Sea cliffs and road cuts show bands of giant-scale bone in the rock.
- **The shape.** From the Spire, or on the full map screen, the coastline reads as a body:
  - The Hollow Cape is the head, and the Flue is its mouth, still breathing up warm air.
  - The Crown sits over the heart.
  - The Broken End splits like legs at the Severance.
- **Nobody says it.**

**2. The Twelve Peoples.** Each biome had an old people, now gone. Fire-frost's two are SPEC; the other ten names are GAP-COOKED, and biome cooks may rename them.

| Biome | People | Waymark on their roads (real-world root) | Champion's skeleton at their pass |
|---|---|---|---|
| Volcanic | Smiths of the Mountain (SPEC) | Slag cairns (SPEC) | The Fallen Smith (SPEC) |
| Frozen tundra | Rime-folk (SPEC) | Inuksuit (SPEC) | The Burnt Wyrm (SPEC) |
| Storm peaks | Rodwardens | Copper lightning rods, green with age, every 150 m | A giant roc on the Swamp side of Stormgut, bled out, with Coven bone hooks in its wings |
| Swamp | the Mire Coven | Bone fetish posts with crow skulls | A leech-drake on the Storm side of Stormgut, burnt black by lightning |
| Desert | the Sunwatch | Gnomon stelae whose shadow points down the road at noon | A sea-serpent skeleton on the Ocean side of Saltgate, sun-bleached to chalk |
| Ocean & reefs | the Saltkin | Driftwood beacons with shell chimes | A stone colossus on the Desert side of Saltgate, eaten through by coral and barnacles |
| Sky islands | the Kitewrights | Lung ta wind-poles with faded cloth | Their cage lifts. Their champion fell down the Flue: a winged skeleton wrapped in mycelium at the cavern floor |
| Fungal caverns | the Deepfarmers | Glow-cap rings | A mushroom titan's husk on the highest sky island, dried by wind to paper |
| Graveyard | the Lantern Mourners | Tōrō stone lanterns | A treant on the Forest side of Rootgrave, its roots rotted black |
| Ancient forest | the Grovekin | Trail trees bent at knee height | A bone giant on the Graveyard side of Rootgrave, grown through by roots |
| Crystal caves | the Wardcarvers | Ringing chime stones that ring when struck, every 150 m | A void-shape's ash cast on the Void side of the Severance, warded in a crystal cage |
| Corrupted wasteland | the Hollow Choir | Cairns balanced point-down | A crystal giantess on the Crystal side of the Severance, hollowed out from the inside |

**3. The Crown and the Twelve Stairs.**

- **The climb.** All twelve peoples climbed toward the Crown. Every stair is built in its people's stone, and every one breaks off at the same height, 700 m.
- **The Ring.** Above, at 900 m, stand **twelve pillars**, one per people, each in its stone with its waymark carved on top. The Ring's floor is scorched in a twelve-coloured starburst pointing outward, so something went out from here.
- **The beacons.** During a biome event, that people's pillar burns in its colour: a 300 m light column seen across the map. The Crown becomes the world's event beacon, with no UI.
- **Nobody explains it.** Lore items in the Hollow Spire (Storm lane) can hint; the land never says who broke what.

**4. The pass wars.** Each pair fought across its pass. The forts face each other, the champions lie dead on the wrong side, and the weapons in the bones are the opposite's (2.3).

**5. The newcomers.**

- **The wrecks.** Every spawn camp sits by the wreck of the ship that brought its castaways. Hulls of the same make sit on all coasts, so everyone came the same way.
- **The camps.** The camps are newer than the ruins: fresh rope, new canvas.
- **The villages** (R3 on) are newcomer towns built from the old peoples' stone.

**6. Borderlands mix props.** Every neighbour border (2.3) carries both sides' kits at half density, and the props tell the contact:

- Volcanic–Storm: ash on rod-roads.
- Frost–Swamp: frozen bog with stopped bubbles.
- Volcanic–Graveyard: ash-graves of smiths with slag headstones.
- Frost–Forest: pine hollows grading into old growth (taiga to temperate).
- Desert–Fungal: sinkholes in sand.
- Ocean–Swamp: a brackish estuary with mangroves.
- Ocean–Sky: sea cliffs with cage-lift chains.
- Forest–Crystal: crystal-veined boulders under moss.
- Graveyard–Void: graves that end mid-row where the ground goes grey.

### 2.13 Music and sound on the map (biome music SPEC in FULL-GAME 3.3; map rules GAP-COOKED)

| Rule | Detail |
|---|---|
| Always audible | Music never stops except at planned silences. It ducks no more than 4 dB under combat (SPEC bar) |
| Biome borders | A bar-synced crossfade over 120 m of travel. Both pieces share tempo maps at the blend: the outgoing piece ends its phrase, then the incoming one enters |
| Pass duets | Each pass has its own piece: both biomes' instrument families in one key, a "standoff" piece. Fire–Frost: taiko and brass against sparse piano, D minor, 62 BPM. One duet per pass, 7 in all (the Flue's is vertical: drones low, flutes high, crossfading with altitude) |
| The Crown | **The Crown chorale:** a slow 50 BPM piece using one instrument family from each of the twelve biomes, entering one by one as you climb past each pair of stairs. Plays above 500 m on the Crown and on the Ring |
| The sea | A sea theme of rolling strings in Eb (the Ocean key) plays over open sea beyond 600 m from shore |
| Weather | A storm stem (low percussion and brass swells) layers on during storm states in the biome's key. Fog removes the high register |
| World events | Each world event has a stinger and a layer: Starfall (celesta and choir), Migration (low drums walking), the Dimming (all music detunes 30 cents and drops an octave), caravan (hand bells) |
| Planned silences | The Flue's floor (only the updraft roar), the Silent Vault (Crystal), the Void's Dead calm (silence as threat). No other silences |
| Beds | Wind rises with altitude; surf bed by distance to coast; weather beds by surface; thunder delayed 1 s per 340 m |

---

## 3. Plan (how to build it in Raven's repo)

Raven's rules (SPEC) apply throughout:

- Data first: every number in `Data/*.json` as a first draft.
- New behaviour follows Raven's patterns: `UWRGameplayAbility` plus `ClassForNode` for abilities, and all damage through `UWRCombatLibrary::ApplyHit`.
- Generated data comes from `Tools/terrain/*.mjs` (edit the script, not the json).
- Reuse the fire-frost frameworks: `AWRShrine`, `UWRBiomeEventSubsystem`, `world_pois.json`, `quests.json`. Reuse the ui-controls frameworks: `UWRMapReveal`, `AWRMessageStone`.

### 3.1 Data

| File | Change |
|---|---|
| **New** `Data/world_layout.json` (generated by `Tools/terrain/continent_layout.mjs`) | `frame`; `belts` (id, yRange, west, east, pass, openRound, open: bool); `biomes` (id, polygon, areaKm2, bandMode surface/altitude/depth, camps[3], villageSite, majorArena, hiddenSubBiome); `spine` (points x, y, crest); `passes` (id, pair, polyline, width, saddle, forts[2], shrines[2]); `necks` (polygon, road); `sounds` (polygon, icy: bool); `crown` (spire, ring polygon, stairs[12], pillars[12] with biome and colour); `flue` (centre, radius, floor, rim, lift); `roads` (id, kind coast/pass/shrine, people, polyline); `updrafts` (kind, centre, radius, lift, top, windGate); `races` (id, polygon, current, slackHours); `claimExclusions` (kind, shape, radius); `sealed` (rockfalls[], veilVolumes[], mistwalls[]) per belt |
| `Data/island_walk.json` → **new** `Data/world_walk.json` (from `walkmap.mjs`, extended) | The same 32 m grid over the whole frame. New rows layer `biome` (one letter per biome), per-cell `spineDist` (m), road cells `r`, underground zone cells `u`. `bands` keep Raven's numbers, measured from the Spine. `island_walk.json` stays for the Old Island until W3 has switched every reader |
| `Data/spawn_regions.json` | `side` → `biome` (the old `volcanic`/`tundra` values map 1:1). `band` measured by `spineDist`. Add `depth` and `altitude` ranges for vertical biomes. Fire-frost adds `night` tables (SPEC) |
| **New** `Data/weather.json` | `states` (id, sight, lightMult, tags[], effects {fireDealt, fireTaken, burnBuild, frostBuild, shockTaken, stealthNoise, aggroSight, thirstMult, gust {...}}); `biomes` (id → weights, signature, blockHours 3, buildHours 1); `wind` (prevailingDeg 90 meaning from the east, swingDeg 60, turnDeg 40, turnHours 2, speed [2, 14], stormSpeed [15, 25]); `fronts` (eastBiome → westBiome, delayHours 1, chance 0.5); `plenty` (signatureYield 1.5) |
| `Data/tuning.json` | New blocks: `glide` (2.5 table), `updraft` (by kind), `boats` (3 rows plus `sailCurve` and `capsizeWave`), `roads` (`mountSpeedMult` 1.15, `spawnGap` 4000), `moon` (`cycleDays` 8, `lux` [0.15, 0.5]), `races` (current 500, slack hours), `sea` (`openSeaFrom` 60000, `icyBuild` 8), `worldEvents` (each row in 2.7), `claims` (2.11 radii). Change `world.streamingRangeScale` from 24 to a value the performance cook sets (24 covers the whole old island; the continent is 18 km long). Change `clock`: none (SPEC) |
| `Data/items.json` / `recipes.json` | `crude_glider`, `levistone_glider` (R7), `raft`, `skiff`, `longboat` (R6), `star_iron` (Starfall). The early-pass consumables are named in 2.10; each biome cook adds its own row |
| `Data/pieces.json` | `raft`, `skiff`, `longboat` as `vehicle: true` placeables (water, within 1,000 cm of shore, allowed outside claims) |
| `Data/server_presets.json` | `worldEvents` (Off / Rare / Normal / Frequent, default Normal); `weather` (Off / On, default On); `invasionFrequency` (3.3) |
| `Data/world_pois.json` (fire-frost's file) | Add the map-level POIs: 12 pass forts, 12 champion skeletons, the Ring and 12 pillars, 12 stairs, 4 necks, 3 Kitewright lifts, wreck-tide coast spawn lines, the Starfall exclusion list, village sites |
| `Data/sea_mask.json` | Regenerated by `sea_mask.mjs` for the whole frame (sounds, open sea, races) |
| `Data/Audio/sounds.json` | `weather.<state>` beds by surface; `wind.altitude`; `surf`; `thunder` (delay by distance); `music.pass.<id>` ×7; `music.crown`; `music.sea`; `stinger.world.<event>`; `layer.weather.storm.<biome>`; `boat` (hull, luff, oars, creak); `glider` (cloth flap by speed) |
| `Data/fx.json` | Rain, snow, ash, sand, sea fog, miasma, spore drift, dust (Niagara, camera-attached, density from the state); lightning bolt (ribbon) plus ground glow; meteor streak, impact and crater glow; updraft visual per kind; boat wake and spray; pillar light column; Veil fog |

### 3.2 C++ (Raven's patterns)

| Where | Add or change |
|---|---|
| **New** `WRWorldLayout.h/.cpp` | `UWRWorldLayoutSubsystem` (game instance subsystem). Loads `world_layout.json` and `world_walk.json`. Helpers: `BiomeAt(FVector)`, `SpineDistance(FVector)`, `BandAt`, `IsRoad`, `IsUnderground`, `IsSealed(belt)`, `NearestPass`, `ClaimBlocked(FVector, Reason&)`. Every system below reads these, so nothing else checks `X < 0` again |
| `WRWildlife.cpp` | Side from `BiomeAt`, band from `SpineDistance`, depth and altitude bands for vertical biomes. **Proof that nothing broke:** the Old Island regions are equal to today's by construction (same masks, same distances) |
| `WRSky.cpp` | Biome air (haze, grade, fog) per biome from `BiomeAt` with a 600 m blend, replacing the `biomeCentre` and `biomeWidth` X blend; per-biome presets in `tuning.sky.biomes`. Weather presets layered on top (cloud cover, fog density, light multiplier). Moon phase from game day. Pillar light columns when the biome-event subsystem says so |
| **New** `WRWeather.h/.cpp` | `UWRWeatherSubsystem` (world subsystem on server and clients). `StateAt(Biome, GameTime)` is a pure function of world seed, biome and block (no replication). Wind vector the same way. Exposure trace (30 m up, cached 1 s per pawn). Applies loose gameplay tags `Weather.Wet`, `.Fog`, `.Gale`, `.HeatHaze` via the ability system. Drives camera-attached Niagara and audio beds. Server spawns `AWRLightningStrike` in Storm peaks thunderstorms and spawns gusts |
| `WRCombatLibrary.cpp` | `ApplyHit` reads the target's and source's weather tags and applies the `weather.json` multipliers (fire, shock, burn and frost build-up). Damage goes through `ApplyHit` only (SPEC) |
| **New** `AWRLightningStrike` (in `WRWeather.cpp`) | 1.0 s ground-glow tell (an `AWRGroundMarker` small cue, 1 m, as the combat lane rule asks), then a bolt FX and `ApplyHit` (60, stun 0.8). Cell picker: the highest player in each 60 m cell above 600 m; flyers first. Respects Grounded, the grounding charm and Rodcrest ranges |
| `WRCharacterMovementComponent.cpp` | `CUSTOM_Glide` movement mode (`PhysGlide`): forward speed, sink, wind drift, updraft lift, stamina drain via the attribute set, closes on hit or landing. Road speed multiplier for mounts (`UWRWorldLayoutSubsystem::IsRoad`). Gust impulses from weather |
| **New** `WRUpdraft.h/.cpp` | `AWRUpdraftVolume`: cylinder, lift, top, optional wind gate (ridge lift only when the wind's angle to the cliff normal is under 60°). Spawned from `world_layout.updrafts` at load, plus volcanic vent thermals from `island_dressing.json` vents |
| **New** `WRBoat.h/.cpp` | `AWRBoat` (replicated `APawn`; server moves it, clients smooth it). Hull sits on `WRSea` level plus a bob from the swell. Sail state, steering and the `sailCurve` speed. Rowing. Seats (attach sockets), driver input. Storage via `UWRInventory`. HP and sinking to a sack. Capsize test against wave height in races and squalls. Saved in the world save through the same path as pieces. Deck mount socket on the longboat |
| **New** `AWRCurrentVolume` (in `WRWater.cpp`) | Pushes swimmers and boats (tidal races, the Veil's mistwall). Strength follows the slack schedule |
| `WRWater.cpp` (`UWRWaterSubsystem`) | Icy-water zones (sounds flagged `icy`) apply frostbite build-up through the status component. Open-sea flag beyond `openSeaFrom` |
| `WRBuilding.cpp` | The totem placement check calls `ClaimBlocked`, which covers 2.11 and the underground ban. Piece placement refuses road and pass cells. Return a reason code for the build UI (ui-controls lane reason chips) |
| `WRGameMode.cpp` | **Fix:** `undergroundMargin` puts anyone under the landscape back on top, which would break caverns, catacombs and tubes. Skip it when `IsUnderground(pos)`. Spawn points per biome from `world_layout.camps`, skipping sealed belts in the path picker |
| `WRUIMap.cpp` / `UWRMapReveal` (ui-controls lane) | The map texture becomes the whole continent. Reveal cells go from 4 m to 8 m (2,250 × 975 mask) and the saved RLE scales. Sealed belts are drawn under painted cloud. Vistas reveal 400 m. Wind arrow on the compass while aboard a boat or gliding |
| `UWRBiomeEventSubsystem` (fire-frost lane) | Gets a `scope` (`biome` or `world`) and runs the 2.7 world events from `tuning.worldEvents`: Starfall (meteor projectile with 0.9 s or more of landing glow, crater actor, star-iron node), Migration (spawns a herd with a pass route and a trailing pack through `WRWildlife`), Wreck tide (coast spline spawn), the Dimming (forces the night table and the sky preset), Caravan (R3: a route walker like the roaming-mini route component). Concurrency limit from 2.7. Tells `WRSky` to light the event's Ring pillar |
| `WRRaids.cpp` | Invasions (R2 on) path along `passes[].polyline`; the Flue path for Wind and Spore |
| `WRWorldDressing.cpp` / `UWRDressingSubsystem` | Read a list of dressing files from `world_layout` (one per belt: `belt_dressing_<id>.json`) instead of the single `island_dressing.json`. Belts with `open: false` don't spawn dressing, spawns or nodes. Spawn rockfall blockers and Veil fog volumes for sealed belts |
| **New** `AWRLift` (Kitewright lift) | A hold-to-crank platform on a spline, 40 s, carries 4 players or 1 hauler. Replicated. Modelled on the existing door and interact pattern |

### 3.3 Terrain and map tools

| Step | Tool | Output |
|---|---|---|
| 1 | **New** `Tools/terrain/continent_layout.mjs` (extends `layout_proposal.mjs`'s parameters and seed) | Reads the Spine points, belts and biome boxes in this cook; writes `world_layout.json`, a preview `continent_layout.png`, and land, biome, band, road and river masks at 4 m |
| 2 | `gen_heightmap` path (as M3) per belt | One 1 m heightmap per new belt (the land bounding box of each belt). **The Old Island's heightmap is never regenerated.** The step reads the existing `L_Island` edge rows and matches the new belts to them at the necks |
| 3 | Neck stitch | Only Old Island cells that are under sea level and outside every saved claim (the tool reads the world saves) are raised into the four necks. It refuses to raise anything within 500 m of a camp |
| 4 | `import_island.py` → `import_belt.py` | One new Landscape actor per belt at 1 m quads, aligned to the same world grid. The existing `L_Island` landscape actor is untouched |
| 5 | `dressing.mjs`, `walkmap.mjs`, `spawns.mjs`, `valleys.mjs`, `sea_mask.mjs` | Run per belt (dressing) or for the whole frame (walk, sea). The POI generator (fire-frost) places kit POIs with the 150 m minimum and fills any route point more than 125 m from a POI |
| 6 | HLOD | The Spine and the Crown are always-loaded HLOD, so their silhouettes read from every belt (M3 asks this for the ridge) |
| 7 | Sealing | Rockfalls at necks and passes into unbuilt belts. A Veil fog volume over each unbuilt belt (dense sea fog). A mistwall current at sea. When a round ships, its belt's `open` flag flips to true: rockfalls go and the Veil burns off over one game hour, visible from the open belts |

### 3.4 Assets and the art route

**Route (SPEC from the cooks):** concept picked by Aaron, then a Hunyuan3D 2.1 base, a Blender finish (sculpt, retopo, real textures), then the `Tools/art` scripts. Fab packs where a pack fits. Licence rule: use it unless it says we can't.

| Asset | Route |
|---|---|
| The Crown and the Spine | Terrain plus Quixel/Fab cliff and rock-face kits on slopes over 35°; granite for the Spire. Vertebra crest towers: Blender sculpts from a bone kit, scaled to 40 to 80 m, 3 variants |
| The Twelve Stairs and Pillars | One modular stair kit and one pillar, re-skinned in each people's stone (basalt, ice-dressed granite, slate, black bog oak, sandstone, coral limestone, white marble, glowcap-spotted tuff, grave granite, mossed heartwood, crystal, void-grey glass). Each pillar has its waymark carved on top |
| Pass forts (12) | Fab modular ruin kits (medieval stone, desert, Nordic), re-dressed per people. Each fort is a Packed Level Actor |
| Champion skeletons (10 new) | Concept, then Blender from Fab bone kits, at giant scale, with the opposite element's weapons embedded |
| Waymarks (10 new kits) | Small props: copper rods, bone posts, gnomon stelae, driftwood beacons, wind-poles, glow-cap rings, tōrō lanterns, trail trees, chime stones, inverted cairns. Each gets a lamp or glow that lights at night when you pass |
| Boats | A Fab Viking or longboat kit cut into a raft, skiff and longboat; sails as cloth sim; oars animated |
| Gliders | Concept, then Blender: a crude bone-and-hide wing; a levistone wing with floating stones in the spars. Hero glide clips from a paraglider or flying anim pack retargeted to the MetaHuman rig (hero lane) |
| Kitewright lifts | A Fab industrial or medieval crane kit: a cage, chains and a counterweight wheel |
| Wrecks (wreck tide and camp castaway hulls) | The same ship kit, broken and barnacled |
| Weather FX | Niagara on top of WRSky (no paid weather pack; WRSky owns the sky): rain sheets and splashes, snow, ash, sand, spores, fog cards, ribbon lightning, meteor |

**Sound** (real recordings: Sonniss GDC bundles and Freesound; the licence rule applies):

- Rain on each surface, real thunder, surf, gale and whistle.
- Sand hiss, boat hull creak and slap, sail luff, oars, cloth flap.
- Crank and chain for the lifts.
- Meteor roar and impact, migration herd calls.

**Music (composed):**

- 7 pass duets, the Crown chorale and the sea theme.
- Storm stems per biome and 5 world-event stingers.

These pieces sit alongside the biome music in FULL-GAME 3.3.

### 3.5 Build order inside this scope

Each step is one focus, shipped into Aaron's build when done. Compile, cook and package are the only gates.

| Step | What | Ships with | Visible to Aaron |
|---|---|---|---|
| **W1 (proof of one)** | **Rain on the Old Island.** `UWRWeatherSubsystem` with the deterministic roll, the global wind, Clear/Overcast/Rain for Fire and Frost, the `Weather.Wet` tag read in `ApplyHit`, rain FX, rain beds by surface, the sky preset | Now (R0) | Rain rolls in from the east, crosses the ridge an hour later, and wet Cinderfangs take more shock and less fire |
| W2 | The rest of the Fire and Frost weather (Ashfall, Heat haze, Snow, Blizzard, Freezing fog, Gale) plus the moon cycle and the "plenty after rough weather" yield | R0 | Each side has its own skies and nights |
| W3 | `continent_layout.mjs`, `world_layout.json`, `world_walk.json`, `UWRWorldLayoutSubsystem`; switch `WRWildlife`, `WRSky` and spawns to biome and Spine distance; fix `undergroundMargin` | R0 | Nothing should change on the island; regression-free |
| W4 | Crude glider plus updrafts on the Old Island: lava thermals, ridge lift on the ridge's east face, pass draughts | R0 | Fun movement now |
| W5 | Old roads (fire-frost's slag and inuksuk roads as road cells, +15% mount speed), claim exclusions (2.11) | R0 | |
| W6 | Raft and skiff on the Old Island's sea; sail curve; icy water on the tundra coast | R0 | Boats round the island |
| W7 | World events: Starfall, Migration (Fire↔Frost through both passes), Wreck tide | R0 / R1 | |
| W8 | **R1: the landmass.** All belt heightmaps; B3 imported and opened (Storm peaks, then Swamp in R2); necks, sounds, the Crown, the Ring and pillars, the Stairs, Stormgut, pass forts; Veil sealing of B1, B2, B5 and B6; map texture and reveal resize; streaming settings; Storm and Swamp weather; lightning strikes | R1 / R2 | The world grows north; the Crown towers over everything |
| W9 | B5 Rootlands: Rootgrave, merchant caravans (with villages) | R3 / R4 | |
| W10 | B2 North Coast: Saltgate, longboat, tidal races, the Dimming, open-sea rules | R5 / R6 | |
| W11 | B1 Hollow Cape: the Flue, Kitewright lifts, the levistone glider, sky islands placement (a tool that places islands with ≤ 220 m hops and ≥ 1 POI), flying-mount map rules | R7 / R8 | |
| W12 | B6 Broken End: the Severance, the Broken Race; every pass open | R9 / R10 | |

**Proof-of-one item: W1, rain on the Old Island.** It touches every layer of this scope at the smallest size: data (`weather.json`), a deterministic server and client system, a combat effect through `ApplyHit`, sky, FX, sound and wind. Aaron feels it in today's build before any new land exists.

### 3.6 Lines for later cooks

Appended to `C:\Users\acars\wildreach-night\LATER-COOKS.md` (see the lines tagged "world-map lane").
