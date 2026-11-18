# Cook: fire-frost

Lane: **fire-frost**. Covers the two built biomes, Volcanic (Fire) and Frozen tundra (Frost), and their two paths. This replaces every GAP in FULL-GAME 3.2.1 and 3.2.7, the Fire and Frost parts of 3.3, and the Fire and Frost rows of 4.2. It also grades what's BUILT against the bar.
Written 2026-11-13. It covers research, design and plan; the build comes later, round by round, from section 4.

Every coordinate is in metres in today's `L_Island` frame: origin at the centre, +x east, +y south (the north pass is at y −1296). If the 12-wedge landmass moves a biome, its whole layout moves with it as one block, anchored on the crater (Fire) and the glacier bowl (Frost).

---

## 0. What I found in the repo (it changes the brief)

- **Every Fire and Frost tree node already has behaviour.** FULL-GAME 4.2 says 12 nodes "status to confirm". They are all coded as passives (`UWRAbilityRegistry::IsBuilt` in `WRAbilities.cpp:1072`; each one is hooked in `WRCombatLibrary.cpp`, `WRProjectile.cpp` or `WRStatusComponent.cpp`, with numbers in `tuning.json` `passives`).
- **Three stat nodes do nothing.** *Shockwave* (Fire), *Biting cold* and *Wide storm* (Frost) have no `nodeEffects` row and no code reads them. Players can spend 9 points for nothing.
- **Player bolts home** (`spells.lockHoming` 6000, `lockHomingPlayers` 2000). In PvP that breaks the never-home rule.
- **Creature spit flies at 1800 cm/s**, over the 1600 cm/s cap (`creatureAI.spit.speed`).
- **Bosses deal damage by shape tests at resolve time** (`AWRBoss::InShape`), not on visible contact. The full attack area is shown as a ground marker (`AWRGroundMarker`).
- **The music has 2 to 5 minute silent gaps** (`sounds.json` `music.gap` [120, 300]). It is 8 shared CC0 tracks with no biome pools.
- **Creature voices are 7 shared families with pitch shifts** (`sounds.json` `creatures`).
- **Scorchwing and Skaldrith use the same mesh** (`SK_MOUNTAIN_DRAGON`, Fab). The common flyer has the major boss's silhouette.
- **The island has 20 POIs on 9.9 km²**: 6 camps, 6 caves, 2 majors, 4 minis and 2 passes. The bar asks for about 115 per biome.
- **Lava kills slowly:** `hazards.lavaDps` is 8 plus burn buildup 12/s.
- **"Spiked boots" don't exist** as an item, although FULL-GAME lists them. Ice resistance today comes only from the frost path or frost-infused armour (`WRUIHud.cpp:803`).

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game | What it does | Source | Lesson for us |
|---|---|---|---|
| Breath of the Wild | The "triangle rule": hills hide what's behind them and pull the eye to a POI on the peak; a mix of landmark kinds (shrines, stables, camps), not one kind repeated | CEDEC 2017 notes https://gist.github.com/idbrii/e39fe96279aa1670319bfa521d907399 ; https://gmtk.substack.com/p/how-nintendo-solved-zeldas-open-world | Every POI sits where a rise or ridge reveals it. Mix about 15 POI kinds per biome, so 115 POIs never read as copies |
| Breath of the Wild / TotK | Heat and cold are gear checks (Flame Guard, Cold Resistance 2). Frost Talus and Igneo Talus are element field minis on their home ground | https://www.zeldadungeon.net/wiki/Flame_Guard ; https://zeldawiki.wiki/wiki/Frost_Talus | Keep Raven's rule: no meter, only gear checks. Give each biome a cheap consumable pass too (Cinder salve, Spiked boots), so a frost main can cross the lava river before having fire gear |
| Elden Ring | Consecrated Snowfield layers field bosses by time of day (Night's Cavalry at night). Erdtree DLC blends open field with dungeon density | https://eldenring.fandom.com/wiki/Consecrated_Snowfield ; https://pcgamer.com/games/rpg/with-shadow-of-the-erdtree-fromsoftware-wanted-to-build-a-map-thats-not-quite-open-field-and-not-quite-legacy-dungeon-but-instead-blends-the-two-together | Roaming minis keep a day route and a night lair. The Pale Hunter leaves his lair only in a whiteout. Hidden sub-biomes are dense, dungeon-like spaces under an open field |
| Dragon's Dogma 2 | Density over size. Monsters roam and hunt (the griffin leaves its nest). Up to three bosses can be on screen, and fights spill into whatever is nearby | https://techraptor.net/gaming/features/dragons-dogma-2-interview-tokyo-game-show-2023 ; https://gamingbolt.com/dragons-dogma-2-can-have-three-bosses-on-screen-simultaneously | Cinderhorn and Old Tusker walk real routes through POIs and fight whatever they meet. Packs aggro freely |
| Monster Hunter World / Wilds | Fixed environmental traps you learn (shoot crystals onto a monster). Oilwell Basin was built in vertical layers, with a geothermal food chain and monsters designed from the terrain | https://mein-mmo.de/en/monster-hunter-world-umgebung-nutzen,208843 ; https://www.pcgamer.com/games/rpg/monster-hunter-wilds-director-is-proud-of-his-horrible-oily-giant-octopus-son-that-belches-flame-and-writhes-in-the-muck-says-its-a-technical-challenge-hes-finally-getting-to-attempt | Each new mini's counter is a place: basalt pillars for Cinderhorn, thin lake ice for Old Tusker. Basaltbacks graze at sulfur vents, their taming food |
| Valheim | Mountains and Ashlands are gear and consumable checks (frost resistance mead, wolf cape; the boiling sea burns ordinary boats). Ashlands enemies are themed to the land | https://valheim.fandom.com/wiki/Frost_resistance_mead ; https://breezewiki.discard.no/valheim/wiki/Ashlands | The hazard is part of the land's identity. Each biome gets 3 to 4 small hazards per sub-zone on top of its main one |
| Enshrouded / Conan Exiles | Cold is zoned by area (Conan: forest, highland, ice, frozen lake), with gear that stacks | https://massivelyop.com/2017/09/21/cold-hot-burning-conan-exiles-revamps-the-temperature-system/ | Zone the danger by sub-zone (crevasses only on the ice shelves, thin ice only on lakes), never with a meter |
| Ghost of Tsushima | The Guiding Wind points the way without a HUD | https://geekculture.co/geek-interview-ghost-of-tsushima-nate-fox | Tundra wayfinding through the world: sastrugi point at the glacier, inuksuit mark Rime-folk roads, slag cairns mark smith roads |
| Horizon Frozen Wilds | The Cut: snow, frozen rivers, glaciers and geothermal vents in one region; a Scorcher designed for its land | https://blog.playstation.com/archive/2017/11/01/building-a-scorcher-for-the-frozen-wilds | Fire and ice meet at the passes: steam vents on the frost side of each pass, frost on the fire side |
| Skyrim / adaptive music | Separate wilderness, forest and tundra backdrops, day and night. Vertical layering, horizontal resequencing, stingers | https://en.wikipedia.org/wiki/The_Elder_Scrolls_V:_Skyrim_(soundtrack) ; https://blog.audiokinetic.com/how-to-use-interactive-music-to-score-gameplay/ ; https://www.gamedeveloper.com/audio/horizontal-resequencing-and-dynamic-transitions-for-game-music-composers-from-spyder-to-sackboy-gdc-2021- | Each biome gets day and night explore pieces, a combat layer in the same key and tempo, boss tracks in sections per phase, and stingers for events |
| Creature sound design | Jurassic Park's T-rex: baby elephant, tiger and alligator. Raptors: mating tortoises, geese, walrus. GoW's World Serpent: purrs, buffalo, beluga, throat singing. A single pitched animal still reads as that animal, so layer 3 or more | https://www.npr.org/2013/04/13/177153801/jurassic-bark-how-sound-design-changed-our-imaginations ; https://www.sciencealert.com/the-raptor-noises-in-jurassic-park-are-mating-tortoises ; https://www.gamesradar.com/this-behind-the-scenes-look-at-god-of-war-reveals-how-the-sounds-of-the-world-serpent-and-leviathan-axe-were-created ; https://violetrecording.com/how-to-design-a-creature-sound/ | Every species gets at least 3 fused animals plus one material layer (fire crackle, ice crack). No shared families |

### 1.2 Real world to build from

**Volcanic (NPS, USGS, swisseduc, AVO):**
- **Pāhoehoe and ʻaʻā.** Pāhoehoe is smooth, ropy and fast to cross; ʻaʻā is spiky clinker and slow. A pāhoehoe flow can turn to ʻaʻā as it cools. https://home.nps.gov/subjects/volcanoes/basaltic-lava-flows.htm
- **Lava tubes with skylights** make natural dungeons with drop-in points. https://www.nps.gov/articles/000/lava-flow-surface-features.htm
- **Hornitos** are spatter chimneys fed by the flow underneath. https://swisseduc.ch/stromboli/glossary/index-en.html
- **Kipukas** are islands of old green ground ringed by lava. https://avo.alaska.edu/explore/glossary
- **Tree molds** are holes left where lava chilled around a trunk. https://www.npshistory.com/publications/crmo/glossary/index.htm
- **Kawah Ijen:** sulfur burns blue, the acid lake is around pH 0.3, and 49 sulfur miners died in the 1976 eruption. https://en.wikipedia.org/wiki/Ijen
- **Pele's hair:** glass threads blown off lava fountains. https://www.bigislandvideonews.com/2023/03/25/volcano-watch-peles-hairs-beautiful-hazard-on-hawaiʻi/
- **Lava bombs and tephra.** https://en.wikipedia.org/wiki/Volcanic_bomb

**Tundra (NSIDC, swisseduc, NPS):**
- **Crevasses with snow bridges** are trapdoors. https://swisseduc.ch/glaciers/glossary/index-en.html
- **Moulins** roar before you see them (French for "mill"). https://nsidc.org/learn/cryosphere-glossary/m
- **Sastrugi** line up with the prevailing wind, so they work as a compass. **Katabatic wind** pours downhill off the ice. https://gdargaud.net/Antarctica/Glossary.html
- Also used: **pingos, diamond dust, sundogs, erratics, pressure ridges.** https://en.wikipedia.org/wiki/Pingo ; https://en.wikipedia.org/wiki/Sun_dog ; https://en.wikipedia.org/wiki/Pressure_ridge_(ice)
- **Musk oxen** form a defensive ring with the calves inside. https://www.nps.gov/gaar/learn/nature/muskox-circle-defense.htm
- **Wolves** run a herd to find the weak one and drive it into deep snow. https://www.livingwithwolves.org/how-wolves-hunt/
- **Arctic foxes** hear prey under the snow and dive-pounce. https://www.pbs.org/wnet/nature/arctic-fox-dive-bombs-lemmings-hidden-snow/14872
- **Homotherium** had wide paws for snow. https://www.smithsonianmag.com/smart-news/a-35000-year-old-saber-toothed-kitten-was-unearthed-in-siberiaand-it-still-had-its-whiskers-and-claws-180985488/

**Folklore used (used freely):**
- **Hephaestus' fire-breathing bronze bulls (Khalkotauroi)**, which Jason yoked to plough → Cinderhorn.
- **Hekla, "gate of hell"** → the Kiln Roads' mouth. https://earthsky.org/earth/mount-hekla-was-called-gateway-to-hell
- **Ymir, born where Niflheim's ice met Muspelheim's fire** → the old war told by the mirrored skeletons. https://www.encyclopedia.com/humanities/news-wires-white-papers-and-books/ymir
- **The Wendigo, cold and hunger made flesh** → the Pale Hunter's camp. https://www.britannica.com/topic/wendigo
- **Siberian "earth-bull" mammoth lore:** tusks found in the permafrost → Old Tusker and the tusk middens.
- **Trolls under bridges** → Grimjaw's ice arch.
- **Inuit inuksuit** (stone figures used for wayfinding) → Rime-folk road markers.
- **Sámi joik** → the wordless vocal in the tundra music.
- **Pele** → Pele's hair as an item name.

---

## 2. Grade: what's BUILT against the bar

| # | Built thing | Grade | Why | Raise (section) |
|---|---|---|---|---|
| G1 | POI density (20 on the island) | Below | Bar: a POI every 150 to 250 m means about 115 per biome | 3.1, 3.3, 3.4 |
| G2 | Sub-zones | Below | Height bands only; one ground look per side | 3.3, 3.4 |
| G3 | Environmental story | Below | None placed | 3.3.6, 3.4.6 |
| G4 | Music | Below | Shared pools with 120 to 300 s silences; no biome, night or boss sets; combat cuts the explore track | 3.6 |
| G5 | Ambient beds | Close | Lava, wind and day/night beds exist; no beds per sub-zone | 3.3.1, 3.4.1 |
| G6 | Creature voices | Below | 7 shared families with pitch shifts | 3.5 |
| G7 | Creature attacks | Below | 1 to 2 kinds each, one generic attack clip, procedural rear-back. Bar: 3 to 4 rows, real clip per attack, delays and punish windows | 3.3.8, 3.4.8 |
| G8 | Creature animation | Below | Clips are generated (`anim_creature.py`, listed as procedural in CREDITS). Bar: real animation clips | 4.3 |
| G9 | Creature look | Close / Below | Raven's 0.6.0 pass fixed sizes and shapes. Scorchwing is below: same mesh as Skaldrith. Every model needs Aaron's standing-at-scale look | 4.3 |
| G10 | Creature spit at 1800 cm/s | Below | Cap is 1600 | 4.1 data |
| G11 | Boss damage by shape | Below | Bar: damage only on visible contact | 3.3.9, 3.4.9, 4.2 |
| G12 | Boss full-area ground markers | Below | Bar: the telegraph is a small cue | 4.2 |
| G13 | Boss move counts | Below | Vulkarn P1 4 moves, Skaldrith P1 4, minis 2 to 3. Bar: majors 5 to 6 with a 3+ hit combo and a grab; minis 4 to 5 | 3.3.9, 3.4.9 |
| G14 | Boss lock points, run-back stones | Below | One lock point; no stone | 3.3.9, 3.4.9 |
| G15 | Minis per biome (2) | Below | 3 required | Cinderhorn, Old Tusker |
| G16 | Lava and slick ice | Close | Right gear checks, but lava is too forgiving for "players die" | 3.3.4 |
| G17 | Hidden sub-biomes, events, shrines, path masters | Below | None built | 3.3, 3.4 |
| G18 | Day/night creatures | Close | `activity` flags exist; no tables per sub-zone | 3.3.7, 3.4.7 |
| G19 | Tree nodes | Below (3) / Close (rest) | 3 dead stat nodes; the 12 passives work but show and sound like nothing when they fire | 3.7 |
| G20 | Homing player bolts in PvP | Below | Never-home rule | 3.7 |
| G21 | Hitstop 0.06 s (heavy only), parry 0.09 s | Below | Bar: 0.06 to 0.30 s scaled by weight. Combat lane owns this; noted only | — |
| G22 | Volcanic shrubs are green | Below | Raven left them because the ash coat hung the GPU. Fix with a different mesh, not a coat | 3.3.2 |
| G23 | Signature resources (sulfur, lichen) | Meets | | |
| G24 | Body changes | Below | None | 3.7 |

---

## 3. Design

Tags: **SPEC** keeps Raven's or the built design exactly as written. **GAP-COOKED** is this lane's design. **RAISE** is an addition that brings a built thing up to the bar without changing what Raven specified.

### 3.1 Shared rules for both biomes (GAP-COOKED)

**POI kit and density.**
- **Count.** Each biome gets 115 POIs: about 8 skyline landmarks, 5 shrines, 26 hand-made story sites and about 76 kit POIs placed by the dressing generator.
- **Spacing.** The generator places kit POIs with at least 150 m between any two POIs. It fills any point on a walkable route that is more than 125 m from a POI, so no stretch of travel goes past 250 m without one.
- **Siting.** Each POI sits on a rise or around a bend so it's revealed by the land, as Nintendo's triangle rule does.
- **Light.** Every POI has a light or a skyline shape that reads at night.

**Shrines.**
- **Count.** 5 per biome: one Great Shrine with the path master and 4 lesser shrines, one in each of the other sub-zones.
- **Claiming.**
  - A lesser shrine is guarded by a pack set from its sub-zone, 2 packs with one at midland tier.
  - Kill the guards, then hold Interact for 3 s to light the brazier. The shrine is then discovered for you and your clan.
- **Path XP.**
  - A lesser shrine pays 1,000 path XP to the biome's path if you own it, otherwise 500 to each path you own.
  - The Great Shrine pays 2,000 on first visit.
- **Fast travel.**
  - Travel between your discovered shrines from the map screen (`WRUIMap` already has waypoints).
  - You can't travel in combat (hit or hitting within 8 s). Tames within 30 m come with you.
  - Shrines are not safe zones.
- **Re-guarding.** A shrine re-guards every 4 game days. Re-cleansing pays 20%.
- **Separate from boss stones.** 3.3 run-back stones are their own markers: a basalt or ice post 40 to 60 m from each arena door, with no XP and no fast travel.

**Path masters (DD: a path master NPC per biome who teaches through quests).**
- **How they talk.** They stand at the Great Shrine and speak short lines when you come within 6 m, with no camera grab and no cutscene. Quests go to the Journal (`WRUIJournal`).
- **What quests pay.** Each quest pays path XP, plus a recipe or a unlocked secret.
- **Who they serve.** They teach anyone. Their last quest needs the biome's third mini.

**Roaming minis (DD: roaming minis wander the biome).**
- **Route.** A roaming mini walks a spline route from data at walk speed and rests at a lair. It sleeps at night if its species is diurnal.
- **Chase.** It chases 65 m from where it was engaged, not from a fixed home, then walks back to the nearest route point.
- **Fights on the way.** It attacks creatures that block its path, so fights spill over (as in DD2).
- **Respawn.** 4 hours, at the lair.

**Micro-hazards.**
- **Count.** 3 to 4 small hazards per biome, each in only one or two sub-zones.
- **Warning.** Each gives at least 0.9 s of sound plus a small visual cue.
- **No floors.** Each one can kill you, with no safety floors.
- **Gear checks.** Hazards stay gear checks with no meter.

**Biome events.**
- **Length and frequency.** One game day (60 real minutes), about once every 3 real days per biome. Server setting "Biome events": Off / Rare (one a week) / Normal (one in 3 days) / Frequent (daily).
- **Warning.** A world-visible sign starts 5 minutes before.
- **Payout.** Pays the signature resource at triple rate (3.3).
- **Safe zones.** Never touches spawn safe zones.

### 3.2 The old war (story spine for both biomes, GAP-COOKED)

The two biomes tell one story without a word, the way Ymir was born where fire met ice:
- **Smiths and hunters.** The Volcanic was worked by the **Smiths of the Mountain**, who served a forge giant. The tundra was home to the **Rime-folk**, hunters who followed the herds.
- **The war.** They fought across the passes.
- **The mirrored skeletons.**
  - In the lava fields lies **the Fallen Smith**: an earlier forge giant, its bones split by frost, with Rime-folk ice-spear heads still in them.
  - Under the glacier lies **the Burnt Wyrm**: an earlier wyrm, its bones charred black, with smith-forged iron spikes in its skull.
  - Each biome holds the corpse of the other's champion, killed by its opposite element. That's Raven's "bring frost to the fire boss" rule told by the land.
- **What's left.**
  - The Smiths' slag cairns still mark their roads, and the Rime-folk's inuksuit still point to the glacier.
  - Both peoples are gone, and only their camps, roads and graves remain.

---

### 3.3 Volcanic (Fire): west, 5.0 km²

SPEC kept as written: look, rivers, sulfur crystals, lava hazard and its passes, the 9 creatures and their taming, the Obsidian Warden, Ignessa, Vulkarn, Burning blood and the Fire tree (FULL-GAME 3.2.1; BO; CR; ST).

#### 3.3.1 Sub-zones (GAP-COOKED)

| # | Sub-zone | Where (x, y in m) | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|
| V1 | **Ash Plains** | Coastal band x −2000 to −1250, all y. Holds the 3 spawn camps | Grey ash dunes in wind ripples, pumice gravel, black sand at the shore; dead and charred trees | Ash grey #6E6A66, bone white #D8D2C4, rust #8A4B2A | Charred trees, pumice boulders, half-buried carts, slag cairns, plough trench | Wind over ash, distant crater rumble every 40 to 90 s, sea on the coast |
| V2 | **Lava Fields** | South: x −1250 to −250, y +250 to +2000. The lava river runs here | Pāhoehoe (smooth, ropy, full speed) next to ʻaʻā clinker (×0.85 run speed); live river; hornitos; tree molds; the **Ash Pine Kipuka**, an island of living old pines | Black #1A1817, ember #FF6A1A, kipuka green-grey #5C6B4E | Hornitos, tree molds, basalt columns, the Fallen Smith | Lava gurgle and crack, hornito spits, kipuka birds in daytime only |
| V3 | **Obsidian Ridges** | North-west: x −1600 to −1000, y −2000 to −350 | Black glass ridges and scree, sharp shadows, glassy reflections at sunset | Glass black #0D0D10, smoke violet #3B3445, copper green on outcrops #4E7D6A | Obsidian blades, glass shard fields, ore outcrops, Kilnmite mounds | Wind whistling through glass edges (a high harmonic), shard tinkle |
| V4 | **Vents and Geysers** (the Solfatara) | North-centre along the steaming water river: x −1000 to −150, y −1400 to −350 | Yellow sulfur crust, fumaroles, boiling pools, an acid lake; **blue sulfur fire at night** | Sulfur yellow #D9C230, steam white, acid teal #4FB8A8, blue fire #3A6CFF at night | Fumarole cones, the Bellows Stacks, the miners' camp, sulfur baskets | Hiss, gurgle, geyser roar every 40 s, rotten-egg drip |
| V5 | **Crater Highlands and Tube Mouths** | Ring 250 to 700 m round the crater at (−786, 0), up to the ridge | Steep basalt, lava-tube skylights glowing from below, the crater rim | Basalt black, magma orange, ash-sky brown | Skylights, broken smith stairs, the forge-glow rim | Low crater roar, skylight drafts, Magmaw calls |

**V1 ground fix (G22):**
- Swap `ember_shrub` in V1 to V3 for a dead, ash-grey, leafless shrub mesh: a new resource-look variant, no coat material.
- Keep green shrubs only inside the Kipuka and along the water river, where green is real.

#### 3.3.2 Skyline landmarks (GAP-COOKED)

8 landmarks, about one every 800 m, each readable from 1 km.

| Landmark | At | What it is | Reads at night as |
|---|---|---|---|
| Vulkarn's Crater (SPEC site) | −786, 0 | Smoking crater, forge glow inside | Orange glow under the plume |
| **The Bellows Stacks** | −620, −760 | Three 40 m hornito chimneys that "breathe" fire in turn, 6 s apart | Pulsing fire columns |
| **The Fallen Smith** | −900, +1050 | A 60 m forge-giant skeleton half-sunk in ʻaʻā. Its hammer haft spans the lava river as a walkable bridge | Lava glow through the ribs |
| **Glasswall** | −1180, −960 | A 70 m obsidian cliff, mirror-black, that flares at sunset | Moonlight reflected in a black wall |
| **The Blue Fire** | −430, −1160 | Sulfur pits that burn blue at night, with the miners' camp below | Blue flames, the only blue in the biome |
| **Ash Pine Kipuka** | −1300, +650 | 300 m island of tall living pines inside the lava field | Glowmoth lights in the canopy |
| **The Slag Light** | −1880, +120 | A ruined beacon tower on a sea stack, smith-built | Its brazier, lit for good once the nearest shrine is cleansed |
| **Wisp Spire** | −1036, +784 (Ignessa's site) | A spire of fused rock with a wisp halo | A column of rising wisps |

#### 3.3.3 POIs: 115 (GAP-COOKED)

Counts by sub-zone: V1 32, V2 28, V3 22, V4 15, V5 18.

**Already there and counted (10):** 3 camps, 3 caves, 2 minis, Vulkarn, and the north and south pass approaches.

**Hand-made story and set-piece sites (26):**

| # | Site | Sub-zone, approx. at | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| 1 | Plough Trench (Cinderhorn's lair) | V1, −1500, +200 | A 400 m straight furrow in the ash, a smashed giant iron plough, 6 basalt columns, and a burnt yoke-maker's hut | Cinderhorn rests here; iron scrap ×6 |
| 2 | The Miners' Grave | V4, −470, −1110 | 30 sulfur baskets beside bodies under ash, mid-stride toward the shore: they died fleeing an eruption | Sulfur ×20, crude pick |
| 3 | Miners' Camp | V4, −380, −1240 | Tents, a cookfire, a tally board of baskets | Bedroll spot, waterskin, seeds (fire peppers) |
| 4 | Acid Lake | V4, −230, −980 | Teal lake ringed with dead birds; a rusted cart rolled in | Rare gem in the shallows (the acid eats armour, see 3.3.4) |
| 5 | Bellows Stacks base | V4, −620, −760 | Three hornitos with stone vents at the base | Quest: light them (3.3.10). Opens Secret V-S4 |
| 6 | Fallen Smith (inside the ribs) | V2, −900, +1050 | Frost-split bones, Rime-folk ice-spear heads (blue-black stone), an iron-bound giant's ring | Frost essence ×3 (the old war); boss-stone style lore item "Rime spearhead" |
| 7 | Kipuka Grove | V2, −1300, +650 | Living pines; Basaltbacks shelter there; a smith's shrine in the roots | Lesser shrine "Kipuka" |
| 8 | Tree-Mold Field | V2, −1100, +500 | 20 trunk-shaped holes in old lava; one holds a miner's bones and pack | Hide armour, ruby |
| 9 | Hornito Row | V2, −600, +900 | Seven spatter cones in a line along the river; cones spit embers in turn | Pele's hair ×4 |
| 10 | The Hammer Bridge | V2, −880, +1110 | The giant's hammer haft across the lava river | Crossing for anyone; Magmaw ambush spot |
| 11 | Charred Caravan | V1, −1650, −700 | Smith wagons burnt. Guards' weapons point outward, toward the ridge: something came from the pass | Copper sword, bronze ingots ×2 |
| 12 | The Slag Light | V1, −1880, +120 | Spiral stair up a sea stack; a dead brazier at the top | Lesser shrine view; sea view of the whole coast |
| 13 | Slag-Cairn Road | V1 to V5 | 14 cairns of fused slag, 120 to 200 m apart, from camp 3 to the crater stair. Each holds a cold ember cup that lights when you pass at night | Wayfinding (counts as 14 kit POIs) |
| 14 | Glasswall Mirror Room | V3, −1190, −980 | A chamber cut into the glass; from one angle, reflections show a door the eye misses | Secret V-S3 |
| 15 | Warden's Quarry | V3, −1066, −716 (Obsidian Warden, SPEC) | Half-cut obsidian blocks, a smith's half-carved second golem, chisels | Warden's arena; obsidian ×10 |
| 16 | Kilnmite Hive Hill | V3, −1350, −1300 | Mound of chewed ore with 4 egg chambers | Kilnmite eggs (taming); copper ore ×8 |
| 17 | Shard Field | V3, −1450, −500 | 200 m of obsidian blades in the ground. Walking in costs durability on sandals and bare feet (cosmetic sound); flat paths through | Obsidian ×12 |
| 18 | Great Shrine of the Forge | V1, −1460, −260 (near camp 3) | Open-air smithy in a ruined forge hall; the path master works here | Great Shrine; Sigrun |
| 19 | Crater Stair | V5, −1050, −120 | Smith-cut steps up the crater wall, broken in 3 places (jumps); smith statues with their heads struck off | The way to Vulkarn |
| 20 | Skylight of Embers | V5, −520, +260 | A 12 m hole into a lava tube, glowing from below | Kiln Roads entry 1 |
| 21 | Skylight of Bones | V5, −980, +310 | Skylight ringed with Magmaw-gnawed bones | Kiln Roads entry 2 |
| 22 | Vulkarn's Run-back Stone | V5, 50 m outside the forge cavern door | Basalt post with an anvil-shaped cap | Boss stone (3.3) |
| 23 | Ridge Watch | V5, −180, −600 | Smith watchtower facing the ridge, its arrow slits facing the pass | Bronze arrows, map reveal of V5 |
| 24 | Lava-Fall Ledge | V2, −700, +1400 | Lava river pouring off a 25 m cliff; a geyser beside it launches you to a ledge behind the fall | Secret V-S1 |
| 25 | Steaming Ford | V4, −880, −520 | The only shallow, drinkable ford on the steaming river; Ashstriders water here at dawn | Water; easy Ashstrider taming |
| 26 | Ignessa's Wisp Spire | V2, −1036, +784 (SPEC site) | Wisp halo, scorched rings on the ground | Ignessa's arena |

**Kit POIs (the generator places about 76, with variants):**

| Kit piece | Sub-zones | Variants | Count about |
|---|---|---|---|
| Copper/tin outcrop | V3, V5 | 4 | 10 |
| Sulfur vent cluster (glows blue at night) | V4, V2 | 3 | 8 |
| Kilnmite mound | V1, V3 | 3 | 6 |
| Pumice boulder field with Pumicehog burrows | V1 | 3 | 6 |
| Hornito | V2, V4 | 3 | 6 |
| Geyser (launches, see 3.3.4) | V4, V2 | 2 | 5 |
| Basalt column ring | V1, V2 | 3 | 5 |
| Smith grave marker (an iron hammer head on a stake; a sword in it at random) | V1, V2, V3 | 3 | 6 |
| Glowmoth roost (dead tree with cocoons; lights at night) | V1, V2 | 2 | 5 |
| Kill set-piece (spawn hint: a pack on a fresh Ashstrider kill, using the built hunting AI) | V1, V2 | n/a | 5 |
| Giant's tool (a discarded tong, rasp or chisel 6 to 10 m long) | V2, V5 | 4 | 4 |
| Ember-cup cairn (off the road) | all | 2 | 4 |
| Tree-mold cache | V2 | 2 | 3 |
| Sulfur miner's lean-to | V4, V1 | 2 | 3 |

#### 3.3.4 Hazards (main: SPEC; micro: GAP-COOKED)

| Hazard | Where | Tell (≥ 0.9 s) | Effect | Passes with |
|---|---|---|---|---|
| **Lava** (SPEC) | River, pools, crater | Glow, heat shimmer | **RAISE:** `lavaDps` 8 → 20 and burn 12 → 20/s, so an unprotected player dies in about 4 s at 100 HP | Burning blood, fire-infused armour, volcanic mount (SPEC). **New:** Cinder salve, a brew: 45 s lava immunity, 3 min cooldown |
| **Fumarole burst** | V4, V2 | 1.0 s rising hiss and a small steam puff at the vent | 4 m column: 35 damage, 600 cm knock-up, no burn | Dodge out; Burning blood doesn't help (it's steam) |
| **Geyser** | V4, V2 | 1.0 s gurgle and wet bubbling | Launches whoever stands on it 12 m up, then normal fall. 10 damage. A traversal tool used to reach ledges | Ride it on purpose |
| **Acid lake** | V4 Acid Lake | Teal water, fizzing sound | Wading: 6 damage/s plus 2 durability/s on armour and boots | Avoid. Basaltback mount wades it (shell) |
| **Thin crust** | V2 pāhoehoe patches, marked by orange crack lines | Crack sound and glow on your first step; breaks 1.2 s later | Drops you 40 cm into live lava | As lava |
| **Lava-tube drip** | Kiln Roads | Glowing drop forms for 1.0 s | 1 m splash: 25 damage plus burn 30 | As lava |

#### 3.3.5 Hidden sub-biome: the Kiln Roads (RAVEN-PLANNED "lava tubes under the volcano"; layout GAP-COOKED)

**Size:** about 1.4 km of tubes, 6 to 18 m wide, 20 to 60 m under V2 and V5.

**Ways in:**
1. Skylight of Embers (drop 9 m into a deep ash pile, no fall damage).
2. Skylight of Bones (drop 7 m).
3. "Hekla's Mouth", a tube mouth behind the Lava-Fall Ledge (needs the geyser).
4. A back door into Vulkarn's forge cavern, opened from inside only, so it becomes a shortcut, as in Elden Ring.

**Getting back up:** craft a Rope ladder (workbench: fiber ×20, wood ×6). It's a freeform piece that hangs from a skylight lip and is allowed outside claims, then decays after 2 game days.

**Rooms (in order from the Embers skylight):**

| Room | What's there |
|---|---|
| The Ember Gallery | Lava stalactites (drip hazard). Live pāhoehoe stream on the floor with stepping crust. 3 Kilnmite |
| Smiths' Hall | A carved forge with cold anvils, giant tongs on the wall, and 40 smith skeletons sitting in rows facing the anvil. Iron ore ×3 nodes. Ruby. Lore item "Smith's tally" |
| Pele's Loom | A chamber where draughts spin Pele's hair into drifting threads. Harvest Pele's hair. 2 Scorchwing roosting (they wake at light) |
| The Flooded Tube | Live lava at the floor; crossable only on crust islands that sink 2 s after you land (timing route). **Closed during an eruption** (fully flooded) |
| The Sealed Vault | Opened only during an eruption: the eruption's quake breaks its seal for that game day. Holds iron ×6, a gem ×2, upgrade shards ×2 and the lore item "Giant's first hammer-head" |
| The Back Door | A stone door into Vulkarn's cavern. It opens one-way from this side |

**Light:** full dark. Bring torches or a Glowmoth tame (its built utility), or an Aurora fox to sense the Sealed Vault. Ore here is iron (CF: deeper is better).

**Creatures:** Kilnmite 6, Scorchwing 4, Magmaw 2 (in the Flooded Tube), plus a night-table Cinderfang pack at the Bones skylight.

**Music:** the "Kiln Roads" cue (3.6).

#### 3.3.6 Secrets (GAP-COOKED)

| Id | Secret | How found |
|---|---|---|
| V-S1 | Hekla's Mouth behind the lava-fall: a ledge cache (bronze armour, topaz) and a tube mouth | Ride the geyser beside the fall |
| V-S2 | Miner's tree-mold cache in the Kipuka | An Aurora fox (brought from the tundra) sniffs it, or a dug-out mark in the ash on close look |
| V-S3 | Glasswall Mirror Room door | Stand where the reflections line up (an ember-cup on the floor marks the spot) |
| V-S4 | Inside the middle Bellows Stack: a short tube with an onyx gem and the lore item "Bellows rune" | Light all three Stacks with any fire (Fire bolt, a torch, a resin pot), then drop in while it's cool for 60 s |

#### 3.3.7 Creatures: day and night tables by sub-zone (GAP-COOKED; species and taming SPEC)

Weights per region, used by `spawn_regions.json` with the new `night` table.

| Sub-zone | Day | Night |
|---|---|---|
| V1 Ash Plains | Pumicehog 16, Ashstrider 12, Kilnmite 6, Cinderfang 4 | Cinderfang 12, Glowmoth 12, Pumicehog 6 |
| V2 Lava Fields | Emberhorn 10, Basaltback 8 (Kipuka), Scorchwing 6, Cinderfang 4 | Cinderfang 10, Glowmoth 6, Magmaw 3 (riverbanks) |
| V3 Obsidian Ridges | Kilnmite 10, Scorchwing 6, Emberhorn 4 | Cinderfang 8, Kilnmite 6 |
| V4 Vents | Basaltback 6 (they lick sulfur crust, their taming food), Scorchwing 6, Ashstrider 4 | Glowmoth 10 (circle the blue fire), Cinderfang 6 |
| V5 Crater Highlands | Magmaw 6, Scorchwing 4 | Magmaw 6, Cinderfang 6 |

**Living-world beats:**
- Cinderfangs hunt Ashstriders at the Steaming Ford at dawn.
- Scorchwings dive on Pumicehogs.
- Basaltbacks form a line facing any predator, as musk oxen do.

#### 3.3.8 Creatures: attack rows (RAISE; species SPEC)

Every attack follows CAL 1.13:
- Damage lands only in the clip's active window, from a socket trace (jaw, claw, horn, shell edge).
- The tell is a small cue: a sound plus a pose. Every tell reads at 15 m.
- Projectiles give at least 0.9 s of warning, fly at 1600 cm/s or less, never home, and are never fired inside 4 m.
- Packs attack freely.

Timings are in seconds; damage is × the creature's bite.

| Creature | Attack | Wind-up | Delay hold | Active | Punish | Damage | Flags and tell |
|---|---|---|---|---|---|---|---|
| **Magmaw** (highland) | Snap bite | 0.45 | 0 | 0.12 | 0.7 | 1.0 | Parryable. Jaw opens, wet hiss |
| | Bite, bite, tail whip combo | 0.45 / 0.38 / 0.6 | 0.3 on the whip | 0.12 each | 0.9 | 1.0 / 0.8 / 1.3 | Whip is **low** (jump it). Body coils on the whip |
| | Lava burst (from a pool or the river) | 1.2 | 0 | 0.2 | 1.0 | 1.5 + burn 40 | **Unblockable** (crimson). Bubbles and an orange bulge under the crust plus a rising boil sound. Only near lava |
| | Magma spit (existing `spit`) | 0.75 | 0 | — | 0.8 | 0.8 + burn 25 | Glob 1400 cm/s with smoke trail, never homes, leaves a 2 m lava splash for 4 s. Never inside 4 m |
| | Reads action | — | — | — | — | — | Drink or cast within 8 m → a lunge inside 0.4 s |
| **Cinderfang** (pack 3 to 4) | Snap bite | 0.45 | 0 | 0.1 | 0.8 | 1.0 | Parryable |
| | Leap (existing `lunge`) | 0.6 | 0 | 0.15 | 0.9 | 1.4 | Haunches drop, ember spray off the back |
| | Ember shake | 0.8 | 0 | 0.3 | 0.8 | 0.4 + burn 30 | 3 m cone of sparks; blockable |
| | Pack behaviour | — | — | — | — | — | One hound circles behind while the others press from the front (wolves drive prey). First hit makes the pack howl, which pulls in packs within 60 m |
| **Scorchwing** (flyer) | Dive rake | 0.9 | 0.2 | 0.2 | 1.0 | 1.3 | Wings fold, shriek. Lands after the dive and shakes off (the punish) |
| | Ember spray (hovering) | 0.9 | 0 | — | 0.7 | 5 × 0.3 + burn 20 | 5 embers at 1200 cm/s in a 20° fan, never home |
| | Grounded snap | 0.5 | 0 | 0.1 | 0.8 | 0.9 | Only while grounded (6 s after 3 wing hits, SPEC "bring it down") |
| **Basaltback** (neutral) | Shell slam | 1.0 | 0 | 0.2 | 1.2 | 1.5 | Rears up, falls forward. 3.2 m circle in front |
| | Withdraw | 0.3 | — | — | — | — | Pulls into its shell for 3 s. Front hits are blocked; it bursts out with a 2 m shove |
| **Emberhorn** (neutral) | Charge (existing) | 0.9 | 0.2 | 1.2 | 1.0 | 1.6 | Paws the ground twice, horn flares |
| | Gore toss | 0.55 | 0 | 0.15 | 0.8 | 1.2 | Tosses 3 m |
| | Rear stomp | 0.8 | 0 | 0.15 | 0.9 | 1.3 | 2.5 m circle; low |
| **Kilnmite** | Mandible pinch | 0.45 | 0 | 0.1 | 0.8 | 1.0 | Parryable |
| | Hot spit | 0.75 | 0 | — | 0.8 | 0.6 + burn 15 | 1000 cm/s, range 8 m |
| | Nest swarm (SPEC) | — | — | — | — | — | 6 to 10 out when the nest is hit |
| **Ashstrider, Glowmoth, Pumicehog** | Cornered kick or flutter | 0.6 | 0 | 0.1 | 0.8 | 0.7 | Only when cornered; otherwise flee |

#### 3.3.9 Mini-bosses and major

**Obsidian Warden (SPEC fight and counter; RAISE adds 2 moves and lock points)**
- **Keeps (SPEC):** ground pound, charge, fist; shell break; chilled shell cracks 2.5× faster.
- **Adds:**
  - *Shard burst*: once the shell is half broken. 0.9 s wind-up (shell glows along its cracks, glass creak), 4 m ring, 35 damage, unblockable.
  - *Grab and slam*: 0.9 s wind-up, 3 m line, 50 damage. Dodge only.
- **Lock points:** head, core (the core opens when the shell is gone).
- **Damage:** fist and grab on the hand-socket trace only.

**Ignessa, Queen of Wisps (SPEC fight and counter; RAISE)**
- **Keeps (SPEC):** flies out of reach, sends exploding wisps; destroy them and she lands.
- **Wisps no longer chase.** Each wisp picks a point, glows and whines for 0.9 s, then dashes in a straight line at 1400 cm/s and explodes on contact or at the line's end (25 damage, 3 m). It can retarget 1.2 s later. That keeps "never home".
- **Adds:**
  - *Ember rain* (airborne): 1.0 s wind-up. 5 embers fall in a line toward you, each with a small ground glow 0.9 s before landing.
  - *Flame lash* (landed): 0.6 s wind-up, 4 m cone, parryable.
- **Lock points:** body, crown.

**Cinderhorn, the Yoked Bull: roaming (GAP-COOKED; the 3rd Fire mini)**

*Folklore:* Hephaestus' fire-breathing bronze bulls, the Khalkotauroi, which Jason yoked to plough. The forge giant yoked this one to plough the ash, and it broke free.

| Part | Detail |
|---|---|
| Body | An Emberhorn bull grown to 4.2 m at the shoulder and 7 m long. Bison hump, a single forward horn 2.2 m long, white-hot at the tip. Ash-black hide split by glowing ember seams. A broken iron yoke on the neck drags 6 m of chain |
| Silhouette | Hump, one horn, chain trailing. Readable in the dark by the horn glow and the chain sparks |
| Palette | Charcoal #1C1A19, rust #7A3B1E, seam ember #FF6A1A, horn tip near-white #FFE3B0 |
| Voice | Bull bellow, elk bugle (the scream), blast-furnace roar, and the chain-drag loop audible at 40 m: you hear him before you see him |
| Route | 2.4 km loop through V1 and V2: Plough Trench (lair), the Fallen Smith, the Kipuka edge, Hornito Row, the Charred Caravan road, then back. Walks 3 m/s (about 13 min a loop) and rests 3 min at the trench. Diurnal: sleeps at the trench at night. A sleeping bull can be opened with a sneak crit (backstab multiplier) |
| Aggro | Sight 45 m, hearing 25 m. Chases 65 m from where he aggroed. Charges through player-built camps (damages pieces as a raid does) but never enters spawn safe zones |
| Numbers | 3,000 solo HP (+80% per extra player or tame, SPEC rule). Stance 90; 2 parries to break. Weak to frost (1.5×) |
| Lock points | Horn (head), chest |

| Attack | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Horn hook | 0.7 | 0 | 0.15 | 0.8 | 45 | Parryable. 3.5 m, 70° |
| Hook, hook, toss | 0.7 / 0.5 / 0.65 | 0.25 on the toss | 0.15 each | 1.0 | 40 / 40 / 55 | Toss throws you 5 m; the toss is not parryable |
| **Rampage charge** | 1.1 | 0 | up to 2.8 | 1.2 if it misses | 80 | **Unblockable** (crimson horn flash). Paws twice and snorts sparks; the chain rattles. Tracks only for the first half of the wind-up, then runs straight up to 40 m at 14 m/s. Leaves a burning trail 2 m wide for 8 s (12 dps, burn 20/s). Skids 6 m and turns if it misses |
| Stomp quake | 1.0 | 0 | 0.2 | 1.0 | 50 | Rears up; 6 m ring, **low** (jump i-frames clear it) |
| Bellows snort | 0.8 | 0 | 0.6 | 0.8 | 20 + burn 60 | 6 m cone, 40°; not melee |

**Counter mechanics:**
1. **Bait the charge into something solid.** That means basalt columns (6 at the trench, rings along the route), a stone wall or a big trunk. The horn sticks: a 5 s stun, ×1.5 damage taken, ×2 stance damage. It's the long punish.
2. **Frost on the horn.** Hits on the horn lock point that build 150 frost freeze it for 20 s: no burning trail, and charge damage −40%.

**Drops:**
- Trophy "Cinder horn".
- Upgrade shards ×3 (BO rule).
- Ember essence ×4.
- "Yoke chain" ×1: a crafting extra, +10% stance damage on heavy weapons.

**Music:** the Volcanic mini cue when engaged.

**Vulkarn, the Forge Giant (SPEC; RAISE to the major bar)**
- **Keeps (SPEC):**
  - Phase 1: hammer combo ×3, overhead slam with lava cracks, tongs grab (parry staggers him), anvil slam flood.
  - Phase 2: white-hot armour, bellows breath, slag throw, more floods, melting anvils.
  - Hidden phase 3: quenched rage, two hammers, every slam floods.
  - Weak to frost. Drops core, materials, Giant's ember.
- **Phase 1 adds:**
  - *Tong sweep*: 0.75 s wind-up, low, 6 m arc; jump it.
  - *Drag upswing*: the hammer drags along the floor sparking, then swings up. 0.9 s wind-up plus 0.5 s delay hold.
  - That makes 6 moves, with a 3-hit combo and a grab.
- **Transition at 50%:** a forced anvil slam flood is the unblockable transition attack (CAL 1.14). It never happens during a stance break.
- **Phase 2 adds:**
  - *White-hot overhead*: delay hold 0.6 s, with a final glint 0.2 s before contact.
  - *Four-hit combo*: hammer, hammer, hammer, then the bellows breath.
  - Slag throw targets the **furthest tame first** (the tame-punishing move).
- **Lock points:** chest, hammer arm.
- **Run-back stone:** 50 m outside the cavern door.
- **Damage:** hammer head and tongs socket traces. Floods and breath hurt where the lava or fire is drawn, never past it.

#### 3.3.10 Shrines and path master (RAVEN-PLANNED exist; who and where GAP-COOKED)

| Shrine | At | Guard |
|---|---|---|
| Great Shrine of the Forge | −1460, −260 | none (path master) |
| Kipuka Shrine | −1300, +650 | 2 Cinderfang packs |
| Glasswall Shrine | −1180, −900 | Kilnmite swarm plus 2 Scorchwings |
| Blue Fire Shrine | −430, −1100 | Basaltback herd gone mad (aggressive while the shrine is dark) |
| Crater Rim Shrine | −1000, −250 | Magmaw pair |

**Path master: Sigrun Ashhand.**

*Look:*
- **Body and age:** a woman of about 32, tall and strong-shouldered.
- **Skin and hair:** warm brown skin streaked with soot; copper-dark hair in a thick braid.
- **Clothes:** a leather apron over a sleeveless linen shirt.
- **Fire marks:** bare forearms with the Fire glowing veins at 75 points, a living preview of the body change.
- **Eyes:** a faint ember ring in the iris.

*Voice:* low and dry.

*Where:* the Great Shrine's forge.

*Quests:*

| # | Quest | Task | Pays |
|---|---|---|---|
| 1 | Feed the fire | Bring 10 sulfur crystals | 800 Fire XP; recipe Cinder salve |
| 2 | Quench | Craft a fire-infused blade at her forge, then quench it in the steaming river | 1,000 XP; infusion shown in play |
| 3 | Ash and bone | Kill 5 Cinderfangs while they're burning | 1,200 XP |
| 4 | The Bellows | Light the three Bellows Stacks | 1,500 XP; opens V-S4 |
| 5 | Glass heart | Show her the Obsidian heart trophy | 2,000 XP; recipe obsidian arrow |
| 6 | The yoke | Kill Cinderhorn | 3,000 XP; 2 upgrade shards |

#### 3.3.11 Event: the Eruption (RAVEN-PLANNED name; behaviour GAP-COOKED)

| Time | What happens |
|---|---|
| −5 min | Ground tremors (camera trauma 0.05 every 30 s for players in the Volcanic). A plume rises from the crater, visible from the whole island. The event music layer fades in |
| 0 to 10 min, **Plume** | Ash fall: visibility 80 m in V2 and V5, ash drifts on the ground |
| 10 to 50 min, **Fire** | **Lava bombs**: every player outdoors within 1.2 km of the crater gets a bomb aimed 3 to 12 m from them every 8 to 14 s. Each is a visible glowing rock with a smoke trail on a ballistic arc. The launch boom comes 1.2 s before impact, with a small ground glow for the last 0.9 s. 3.5 m blast, 70 damage plus burn 50, unblockable; it never homes. Roofs block bombs (60 damage to the piece). Never within 50 m of a spawn camp. The lava river rises 0.5 m. One of 3 preset spill channels opens and cuts a road for the event. Magmaw counts double; Scorchwing aggro range ×1.5 |
| 50 to 60 min, **Settling** | Bombs stop, heavy ash. Fresh crust grows sulfur crystal clusters |
| After | Bomb cores stay 10 min: each gives sulfur ×4 to 6, iron ore ×1 to 2, and a 20% chance of obsidian. The Kiln Roads' Sealed Vault is open; the Flooded Tube is closed. Sulfur pays at triple rate for the event (3.3) |

---

### 3.4 Frozen tundra (Frost): east, 5.0 km²

SPEC kept as written: look, lichen, the slick-ice hazard and its passes, the 9 creatures (Floeback deferred to R6), Grimjaw, the Pale Hunter, Skaldrith, Permafrost and the Frost tree.

#### 3.4.1 Sub-zones (GAP-COOKED)

| # | Sub-zone | Where (x, y in m) | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|
| T1 | **Snow Flats** | Coastal band x 1250 to 2000, all y. Holds the 3 camps | Wind-packed snow with **sastrugi pointing at the glacier** (a compass); polygon ground where the snow is thin; erratic boulders | Snow #EEF2F5, shadow blue #9DB2C8, lichen ochre #B8913B | Inuksuit, sledge wrecks, tusk middens, erratics | Steady wind, snow hiss, distant drudge calls |
| T2 | **Pine Hollows** | South: x 250 to 1250, y +350 to +2000 (the built valley fog) | Deep snow under pines, still air, fog in the hollows | Pine green-black #1F2B24, fog grey, amber lamp light | Hunter blinds, fallen pines, the Pale Hunter's camp | Snow falling from branches, creaking trunks, owls at night |
| T3 | **Frozen Lakes** | North: x 400 to 1400, y −2000 to −600, around the Great Frozen Lake (900, −1250) | Glassy lake ice with **pressure ridges**, dark thin-ice patches, icy rivers | Ice teal #9FD4E0, black thin ice #1B2A33, white ridges | Ice-fishing camps, the ice-arch bridge, reed stubble | Ice booming and pinging (real lake ice), wind |
| T4 | **Ice Shelves and Crevasse Fields** | Mid: x 300 to 1000, y −600 to +350, the glacier tongue | Seracs, crevasses with snow bridges, roaring moulins, blue ice walls | Glacier blue #5FA8D3, deep crevasse navy, dirty-ice grey | Seracs, crevasses, moulins, the frozen whaler | Moulin roar (you hear it first), ice groans, meltwater |
| T5 | **Glacier Plateau and Ice-Cave Mouths** | Ring 250 to 700 m round Skaldrith's bowl at (804, 0), up to the ridge | Wind-polished ice, **katabatic wind** pouring downhill, diamond dust glitter | White-blue, sundog halos, near-black rock | Ice pillars, wyrm-claw scars, cave mouths | Katabatic howl, ice-crystal tinkle |

#### 3.4.2 Skyline landmarks (GAP-COOKED)

| Landmark | At | What it is | Reads at night as |
|---|---|---|---|
| Skaldrith's Bowl (SPEC site) | 804, 0 | Glacier bowl with ice pillars | Aurora over the bowl |
| **The Tusk Gate** | 1460, −220 | A Rime-folk arch of two 12 m mammoth-scale tusks | Lamps hung in the arch |
| **The Ice Arch** | 1064, −766 (Grimjaw's site) | A natural ice bridge over the river where the troll lives (the troll-under-the-bridge tale) | Blue glow through the ice |
| **The Frozen Whaler** | 520, −420 | A Rime-folk longboat locked in the ice shelf, its prow 15 m up a serac | Lantern left in the crow's nest |
| **The Great Inuksuk** | 1150, +120 | A 9 m stone figure pointing its arm at the glacier | Fire bowl at its feet |
| **Weeping Seracs** | 640, +250 | A 50 m wall of blue seracs that drips and calves | Blue glow |
| **Fox Circle Pingo** | 1700, +900 | A 30 m ice-cored mound where Aurora foxes gather | Fox glow on the slope |
| **Pale Pines** | 1034, +834 (Pale Hunter's site) | A stand of dead white pines, bark stripped as if gnawed | Nothing; the only dark spot in T2 (its threat is the dark) |

#### 3.4.3 POIs: 115 (GAP-COOKED)

Counts by sub-zone: T1 32, T2 28, T3 22, T4 18, T5 15. The 10 built ones (camps, caves, bosses, pass approaches) count.

**Hand-made story and set-piece sites (26):**

| # | Site | Sub-zone, at | What the player finds | Pays |
|---|---|---|---|---|
| 1 | Tusk Gate camp (Great Shrine) | T1, 1460, −220 | Ring of hide tents round the arch; drying racks; the path master | Great Shrine; Áilu |
| 2 | Tusk Midden | T1, 1600, −900 | Tusks stacked by the dozen, half-frozen in the bank | Ancient ivory ×6 |
| 3 | Sledge Wreck Line | T1, 1700, +300 | Three sledges abandoned in a line toward the coast; harnesses cut, not torn | Hide, rope (fiber ×10), a bronze knife |
| 4 | Inuksuk Road | T1 to T5 | 12 inuksuit, 150 to 200 m apart, from camp 1 to the glacier. Each arm points to the next. Lamp bowls light when you pass at night | Wayfinding (counts as 12 kit POIs) |
| 5 | Old Tusker's Wallow | T3, 1080, −1380 (lair) | A trampled lakeshore, broken spear shafts, tusk gouges in the ice | Old Tusker's lair |
| 6 | Great Frozen Lake | T3, 900, −1250 | 600 m of lake. Pressure ridges and dark thin ice in a ring around its centre | Lesser shrine on its island; counter-place for Old Tusker |
| 7 | Ice-Fishing Camp | T3, 760, −1500 | Holes cut in the ice, frozen fish on lines, a dead fire | Raw fish (food), a fishing line item for when fishing lands |
| 8 | Ice Arch (Grimjaw, SPEC) | T3, 1064, −766 | Troll den under the bridge: picked bones, a nest of hides, Rime-folk spears snapped | Grimjaw's arena |
| 9 | Drowned Sledge | T3, 1250, −1100 | Sledge and dogs frozen under clear ice, seen from above | Story only; points to the thin ice |
| 10 | The Pale Hunter's Camp | T2, 1000, +900 | One tent. A hunter's boots, upright, stepping out into the snow; past them the prints turn to clawed ones. Gnawed bones in the fire ring (the Wendigo) | Stalker lore item "Hunter's last arrow"; Pale Hunter's arena near |
| 11 | Hunter's Blind | T2, 700, +1300 | A hide in the pines over a game trail | Bow and tranq arrows ×10 |
| 12 | Resin Grove | T2, 1300, +1500 | Old pines bleeding amber resin | Pine resin ×10 |
| 13 | Fallen Giant Pine | T2, 600, +700 | A 40 m trunk across the hollow, the only bridge over a ravine | Shortcut |
| 14 | Frostantler Wintering | T2, 450, +1100 | A herd ground with scraped snow and antler sheds | Easy Frostantler approach |
| 15 | The Frozen Whaler | T4, 520, −420 | Longboat in the ice; the hull is reached through a crevasse | Secret T-S1 |
| 16 | Moulin of Voices | T4, 700, −250 | A roaring meltwater shaft | Blue Below entry 1 |
| 17 | Crevasse Maze | T4, 450, 0 | 200 m of crevasses with snow bridges, 3 safe lines marked by old Rime-folk poles | Rimeshell burrows; ore |
| 18 | Weeping Seracs | T4, 640, +250 | Blue serac wall that calves | Ice shard hazard; clear ice ×10 |
| 19 | Glacimaw Boneyard | T4, 380, +480 | Ring of ice-polished bones round a churned snow patch (it surfaces here) | Glacimaw meets; frost essence |
| 20 | Glacier Rim Shrine | T5, 1050, +420 | A lone shrine on the rim facing the bowl | Lesser shrine |
| 21 | Wyrm Claw Scars | T5, 650, −300 | Claw gouges 4 m long down a cliff | Story: where Skaldrith lands |
| 22 | Skaldrith's Run-back Stone | T5, 50 m from the bowl entry | Ice post capped with a wyrm tooth | Boss stone |
| 23 | Ice-Cave Mouth (cave 3, SPEC) | T5, 686, 396 | Built cave, dressed as the Blue Below entry 2 | Entry |
| 24 | Fox Circle Pingo | T1, 1700, +900 | Ice-cored mound; foxes gather on aurora nights | Secret T-S4 |
| 25 | Steam Vents at the South Pass | T1/T2, 150, +1250 | Fire's heat leaking through: steaming ground at the pass, the only bare earth in the tundra | Pass flavour; the borderland |
| 26 | Watch Cairn | T5, 300, −700 | Rime-folk watch post facing the ridge, spear racks facing the pass | Map reveal of T5; bronze spear |

**Kit POIs (about 76):**

| Kit piece | Sub-zones | Count about |
|---|---|---|
| Lichen rock field | T1, T3 | 10 |
| Abandoned tent ring (Rime-folk) | T1, T2 | 8 |
| Erratic boulder (with ore or a carving) | T1, T4 | 6 |
| Snowpuff nesting ground | T1 | 5 |
| Rimeshell burrow field | T4, T3 | 6 |
| Crevasse with a snow bridge | T4 | 6 |
| Moulin | T4, T5 | 3 |
| Pingo | T1, T3 | 4 |
| Frozen carcass (butcherable, from the built carcass system) | all | 6 |
| Ice-fishing hole | T3 | 4 |
| Ore outcrop in ice (copper, tin, iron deeper) | T4, T5 | 8 |
| Resin pine | T2 | 4 |
| Kill set-piece (Gloomhowls on a Frostantler at night) | T2, T1 | 4 |
| Ice-locked tree with a cache | T2 | 2 |

#### 3.4.4 Hazards (main: SPEC; micro: GAP-COOKED)

| Hazard | Where | Tell (≥ 0.9 s) | Effect | Passes with |
|---|---|---|---|---|
| **Slick ice** (SPEC) | Lakes, rivers, shelves | Glassy look | Built friction | Frost path, frost-infused armour, tundra mount (SPEC). **New: Spiked boots** (armorer: iron ingot ×2, hide ×3): grip on ice; −5% dodge distance |
| **Snow bridge** | T4 crevasses | Creak and a falling snow trickle on your first step; collapses 0.9 s later | Fall 8 to 25 m: shallow crevasses drop you into the Blue Below (a soft snow pile), deep ones kill | Walk the marked pole lines; a Rimeshell tame sniffs bridges (its ore sense extended) |
| **Thin ice** | T3 dark patches and pressure ridges | Three crack stages, each with a louder crack and a white line spreading | Breaks under 300 kg or more (mounts, Old Tusker, 3 or more players close together) or under a heavy landing. Icy water: frost buildup 25/s and drowning rules | Read the colour (dark = thin); walk spread out |
| **Serac fall** | T4 Weeping Seracs, T5 bowl edges | Deep crack plus falling snow dust for 1.2 s | 4 m impact: 60 damage; leaves clear ice ×3 | Stay out from under them; loud fights nearby make them fall twice as often |
| **Katabatic gust** | T5 | Rising howl for 1.0 s, snow streaming downhill | 700 cm/s push downhill for 0.6 s, every 10 to 18 s | Crouch halves it; spiked boots halve it |

#### 3.4.5 Hidden sub-biome: the Blue Below (GAP-COOKED; ice caves under the glacier, CAL)

**Size:** about 1.2 km of meltwater caves, 20 to 70 m under T4 and T5.

**Ways in:**
1. Moulin of Voices (drop 22 m into a deep meltwater pool, no damage).
2. Ice-Cave Mouth (cave 3).
3. Any shallow crevasse fall in the Crevasse Maze.
4. A frozen waterfall at the Weeping Seracs (2 hits of fire burn it open; it refreezes after 1 game day).

**Rooms:**

| Room | What's there |
|---|---|
| Meltwater Hall | Blue light through the ice, a waist-deep river (icy water rules), Rimeshells |
| The Burnt Wyrm | An earlier wyrm's skeleton frozen into a wall, its bones charred black, smith-forged iron spikes in the skull (the mirror of the Fallen Smith). Fire essence ×3; lore item "Smith's spike" |
| Rime-folk Ice Tomb | 12 hunters sealed upright in clear ice, spears in hand, facing the glacier | Bronze spear; ancient ivory ×4 |
| Glacimaw Galleries | Tunnels bored through the ice. 3 Glacimaw surface from the walls (their built burst) |
| The Deep Vein | Iron ore ×6 and a gem in blue ice, under a serac that falls when you mine (serac fall) |

**Light:** dim blue through the ice; the deep rooms are dark. Bring torches, a resin torch or a Glowmoth (cross-biome tame).

**Music:** the "Blue Below" cue.

#### 3.4.6 Secrets (GAP-COOKED)

| Id | Secret | How found |
|---|---|---|
| T-S1 | The Frozen Whaler's hull: a Rime-folk chieftain's chest (bronze armour, onyx) | Drop into the crevasse under the prow |
| T-S2 | Moulin drop to the Blue Below | Jump in (the roar is the clue) |
| T-S3 | Behind the frozen waterfall | Burn it open |
| T-S4 | Fox Circle Pingo's ice core: an ice chamber with sapphire-blue ice, topaz and lichen ×20 | On an aurora night (after a whiteout) the foxes dig at one spot; or heavy hits on the cracked side |

#### 3.4.7 Creatures: day and night tables (GAP-COOKED)

| Sub-zone | Day | Night |
|---|---|---|
| T1 Snow Flats | Snowpuff 16, Woolly drudge 6, Aurora fox 4 | Gloomhowl 12, Aurora fox 12 |
| T2 Pine Hollows | Frostantler 10, Snowpuff 6, Rimeshell 3 | Gloomhowl 10, Aurora fox 6 |
| T3 Frozen Lakes | Woolly drudge 8, Frostantler 6, Rimeshell 4 (they break the ice) | Gloomhowl 8, Glacimaw 2 |
| T4 Ice Shelves | Rimeshell 6, Glacimaw 5, Rimeclaw 1 | Gloomhowl 6, Glacimaw 5 |
| T5 Glacier Plateau | Rimeclaw 5, Glacimaw 3 | Rimeclaw 6 |

**Living-world beats:**
- **Woolly drudge ring:** when a herd is threatened by 3 or more attackers, the drudges form a ring facing out (the musk-ox defence). Hits from the front are blocked by their heads.
- Gloomhowls drive Frostantlers into deep snow (the wolf tactic).
- Aurora foxes dive-pounce on prey under the snow, as an idle.

#### 3.4.8 Creatures: attack rows (RAISE; species SPEC)

| Creature | Attack | Wind-up | Delay hold | Active | Punish | Damage | Flags and tell |
|---|---|---|---|---|---|---|---|
| **Rimeclaw** (highland) | Claw, claw, saber stab | 0.42 / 0.36 / 0.55 | 0.5 on the stab | 0.1 each | 0.8 | 0.9 / 0.9 / 1.6 | Stab is parryable. Head drops and lips peel back from the sabres |
| | Pounce (existing `lunge`) | 0.6 | 0 | 0.15 | 0.9 | 1.4 | Wide snow-paws spread |
| | Spine bristle | 0.4 | — | 3 s | — | — | At half stance its crystal spines rise; front melee hits give the attacker 10 frost buildup. Go for the flanks |
| | Reads action | — | — | — | — | — | Drink or cast within 8 m → pounce inside 0.4 s |
| **Glacimaw** | Emerge strike (built burst) | 1.2 | 0 | 0.2 | 1.0 | 1.5 | A snow bulge runs at you with a crunching sound for the whole wind-up. Dodge sideways |
| | Body slam roll | 1.0 | 0.2 | 0.4 | 1.2 | 1.4 | Rears up, rolls its length across; 8 m line |
| | Lunge bite (built) | 0.53 (`strikeAt`) | 0 | 0.15 | 0.9 | 1.2 | Parryable |
| **Gloomhowl** (pack, by sound) | Bite | 0.45 | 0 | 0.1 | 0.8 | 1.0 | Parryable |
| | Leap | 0.6 | 0 | 0.15 | 0.9 | 1.3 | |
| | Howl call | 1.0 | — | — | — | — | Pulls packs within 80 m. Hunts by sound: crouching halves its hearing range (the sneaking skill matters, SPEC) |
| **Woolly drudge** (neutral) | Head sweep | 0.9 | 0 | 0.2 | 1.0 | 1.3 | 4 m arc |
| | Rear double stomp | 1.2 | 0 | 0.2 | 1.2 | 1.6 | 3 m circle, two hits 0.3 s apart |
| **Frostantler** (neutral) | Antler charge | 0.9 | 0.2 | 1.0 | 1.0 | 1.5 | Head lowered, antlers chime |
| | Fore-hoof stab | 0.5 | 0 | 0.1 | 0.8 | 1.0 | Parryable |
| **Rimeshell** | Shell ram | 0.7 | 0 | 0.15 | 0.8 | 1.0 | |
| | Curl roll | 1.0 | 0 | 1.2 | 1.0 | 1.3 | Curls like a pangolin, rolls 10 m straight |
| **Snowpuff, Aurora fox** | Peck or nip when cornered | 0.55 | 0 | 0.1 | 0.8 | 0.6 | Otherwise flee |

#### 3.4.9 Mini-bosses and major

**Grimjaw (SPEC fight and counter; RAISE)**
- **Keeps (SPEC):** club swings, thrown ice boulders, regeneration unless burning or bleeding.
- **Adds:**
  - *Delayed overhead* as the club combo's third hit: 0.4 s hold, final glint 0.2 s before contact.
  - *Grab and gnaw*: 1.0 s wind-up. He bites and heals 3% per bite, 3 bites. Dodge or a heavy hit to his face breaks it.
  - The boulder becomes a real thrown boulder (1200 cm/s, arcing, never homing) with a small ground shadow.
- **Lock points:** head, club arm.
- **Arena:** under the Ice Arch.

**The Pale Hunter (SPEC fight and counter; RAISE)**
- **Keeps (SPEC):** invisible in blizzards, ambushes; footprints in the snow; fire or lightning hits reveal it.
- **Adds:**
  - *Fade and re-pounce*: it backsteps into the snow and pounces again 1.5 to 3 s later from a new side. The sound cue is its breath, 0.9 s before.
  - *Throat bite grab*: 0.8 s wind-up, 60 damage.
- **Voice:** it mimics the player's own hurt grunt when hidden (the Wendigo's mimicry). It's a lure, never shown on screen.
- **During a whiteout event** it leaves its camp and roams the Pine Hollows.

**Old Tusker, the Earth-Bull: roaming (GAP-COOKED; the 3rd Frost mini)**

*Folklore:* Siberian "earth-bull" mammoth tales, whose tusks are found in the permafrost.

| Part | Detail |
|---|---|
| Body | A Woolly drudge bull grown to 5 m at the shoulder. Six legs. Twin sweeping tusks 4 m long crusted in ice like stalactites. Matted fur hung with icicles that clatter. Three broken Rime-folk spear shafts in the hump |
| Silhouette | Hump, two sweeping tusks, six legs. The tusks catch light at night |
| Palette | Dun brown #6B5641, rime white, tusk ice blue #A9D8EE, old-blood rust at the spear wounds |
| Voice | Elephant trumpet, musk ox roar, walrus bellow, with icicle clatter as a body layer |
| Route | 2.2 km loop through T1 and T3: Wallow (lair), the Great Frozen Lake shore, the Tusk Midden, the snow flats past the Tusk Gate (never into the camp), then back. Led herd: 3 to 5 Woolly drudges follow. Diurnal |
| Numbers | 3,000 solo HP (+80% per extra player or tame). Stance 90; 2 parries. Weak to fire (1.5×). The herd joins the fight (neutral herds defend each other, SPEC) |
| Lock points | Left tusk, right tusk, head |

| Attack | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Tusk sweep | 0.8 | 0 | 0.25 | 0.9 | 50 | 150° arc, 5 m. Blockable, not parryable |
| Gore, gore, toss | 0.65 / 0.5 | 0.3 on the toss | 0.15 | 1.0 | 40 / 55 | Toss 5 m |
| **Six-leg stampede** | 1.2 | 0 | up to 2.5 | 1.2 if it misses | 85 | **Unblockable.** He trumpets and scrapes the ice. Tracks for the first half of the wind-up, then runs 35 m straight. Tramples build pieces |
| Rear and crush | 1.1 | 0 | 0.2 | 1.2 | 60 | 5 m ring. Throws 4 ice chunks that arc out (visible, 0.9 s or more in the air, no homing) |
| Trumpet call | 1.0 | — | — | — | — | Herd within 120 m comes in frenzied for 10 s |

**Counter mechanics:**
1. **Break the tusks.** Each tusk has 600 part HP; heavy and blunt hits do double. One broken: sweep reach −40%, and it drops Tusker ivory. Both broken: a 6 s stagger.
2. **Thin ice.** Lure him onto the Great Frozen Lake's dark ring. A stampede or crush there breaks through: he's stuck in the water for 8 s, takes ×1.5 damage and can't gain frost buildup. Burning his hide (the fire status) makes him run for the nearest water. If that's the lake, he breaks through by himself. Frost-only players can burn him with a resin fire pot.
3. **The herd ring.** At 30% HP his herd rings him, facing out. Break the ring by killing one drudge or scattering them with fire.

**Drops:**
- Trophy "Tusker ivory".
- Upgrade shards ×3.
- Frost essence ×4.
- Ancient ivory ×4.

**Skaldrith, the Frost Wyrm (SPEC; RAISE to the major bar)**
- **Keeps (SPEC):**
  - Phase 1: breath sweep, claws, pillar-breaking tail sweep, lunging bite.
  - Phase 2: flight and dives, slick-ice air breath, ice spikes.
  - Hidden phase 3: whiteout, freezing breath.
  - Weak to fire; wing hits in flight ground him. Drops core, materials, Wyrm scale.
- **Phase 1 adds:**
  - *Wing buffet, claw, bite*: 0.6 / 0.5 / 0.7 s, with a 0.3 s delay on the bite.
  - *Jaw grab*: 0.9 s wind-up. He picks you up, shakes and throws: 80 damage. Dodge only.
  - That makes 6 moves, with a 3-hit combo and a grab.
- **Transition at 50%:** a take-off gust is the unblockable transition attack (4 m knockback ring, 1.2 s wind-up), as in CAL.
- **Phase 2 adds:**
  - A delayed dive with a 0.5 s hover hold.
  - *Tail-spike combo*: tail sweep into a spike burst along its line.
  - Air breath targets the **furthest tame first**.
- **Lock points:** head, chest, left wing, right wing (CAL 1.14).
- **Run-back stone.**
- **Mesh:** a new mesh of his own (G9). Scorchwing keeps its role and gets its own bat-winged mesh too (4.3).

#### 3.4.10 Shrines and path master

| Shrine | At | Guard |
|---|---|---|
| Great Shrine of the Rime | Tusk Gate, 1460, −220 | none (path master) |
| Lake Island Shrine | 900, −1250 (on the lake's island; reached over the ice) | Gloomhowl packs ×2 at night, drudges by day |
| Pine Shrine | 700, +1250 | Gloomhowl pack plus Frostantler bull |
| Crevasse Shrine | 450, +60 | Rimeshell nest plus Glacimaw |
| Glacier Rim Shrine | 1050, +420 | Rimeclaw pair |

**Path master: Áilu the Tracker.**

*Look:*
- **Body and age:** a man of about 30, lean and broad-shouldered.
- **Face and hair:** wind-browned skin, dark hair tied back, a close beard rimed white.
- **Frost marks:** frost-pale lips and fingertips and misting breath, the Frost body change at 75.
- **Clothes:** a reindeer-hide tunic with a Sámi-style woven band in red, blue and yellow, open at the throat.

*Voice:* quiet, few words.

*Quests:*

| # | Quest | Task | Pays |
|---|---|---|---|
| 1 | Listen | Sneak within 5 m of a sleeping Gloomhowl pack and back out | 800 Frost XP; Sneaking XP |
| 2 | Read the ice | Cross the Great Frozen Lake to its island without breaking through | 1,000 XP; recipe Spiked boots |
| 3 | The patient hand | Tame a Woolly drudge with lichen | 1,200 XP |
| 4 | Slow the saber | Kill a Rimeclaw while it's slowed (Permafrost) | 1,500 XP |
| 5 | Under the bridge | Show him Troll marrow | 2,000 XP; recipe Resin fire pot |
| 6 | The Earth-Bull | Kill Old Tusker | 3,000 XP; 2 upgrade shards |

#### 3.4.11 Event: the Whiteout (GAP in FULL-GAME; behaviour GAP-COOKED)

| Time | What happens |
|---|---|
| −5 min | Sundogs ring the sun (by day), or the aurora goes out (by night). The wind rises. A white wall rolls off the glacier, visible from the whole island. The event music layer fades in |
| 0 to 60 min | Visibility 25 m outside camps. Gusts of 600 cm/s for 0.5 s every 10 to 20 s, each with a 1.0 s rising howl. Deep drifts slow you ×0.85 on T1. Gloomhowls hunt by day, packs +1. The Pale Hunter roams T2. Inuksuk lamps and shrine braziers stay visible at 80 m (a lifeline). Footprints show sharp in fresh snow (Tracking XP ×2) |
| After, the aurora night | The next night the aurora is full. Aurora foxes gather at the Fox Circle Pingo and dig (T-S4). Lichen blooms on wind-scoured rocks: lichen pays triple. Frost-felled creatures lie frozen and can be butchered |

**No temperature meter:** the danger is sight, wind, hunters and ice, not cold damage (DD rule).

---

### 3.5 Creature voices: all Fire and Frost creatures and bosses (GAP-COOKED, replaces the shared families)

**Layers per species:**
- The four 3.3 layers per attack: tell, strike, hit, pain.
- Plus idle, aggro and death.
- Each is built from at least 3 real animal calls and one material layer.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Magmaw | Komodo hiss, alligator bellow (infrasound growl), camel groan (pain) | Boiling oil sizzle | Wet roar as it bursts from lava |
| Cinderfang | African wild dog hoo-calls, spotted hyena whoop, Tasmanian devil screech (attack) | Coal crackle | Pack "hoo" chorus before the first rush |
| Scorchwing | Fruit-bat squabble, red-tailed hawk scream, bat echolocation clicks pitched down | Blowtorch roar on the dive | Rising scream on the wing fold |
| Basaltback | Tortoise mating groan, elephant seal rumble, hippo wheeze | Rock grind | Shell-scrape when it withdraws |
| Emberhorn | Bison grunt, elk bugle, horse snort | Bellows whoosh | Two hoof scrapes before a charge |
| Kilnmite | Stag beetle stridulation, hissing cockroach, cicada | Kettle whistle | Rising chitter when the nest is touched |
| Ashstrider | Emu drum, ostrich boom, horse whinny low | Ash puff steps | Drumming alarm that spooks herds |
| Glowmoth | Hawk-moth wing hum, bat squeak pitched up | Glass chime | Soft chime bloom when it lights up |
| Pumicehog | Pig grunt, rock hyrax squeak, peccary bark | Pumice clack | Squeal hop |
| Rimeclaw | Tiger chuff, leopard saw call, cougar scream | Ice crack | Saw-call stalking you in the dark |
| Glacimaw | Walrus bellow, bearded seal trill, sea lion roar slowed | Glacier groan | Trill rising under the snow before it bursts |
| Gloomhowl | Wolf howl (mournful), howler monkey, coyote yips | Sniffing clicks (eyeless) | Overlapping howls that tell you the pack's size |
| Woolly drudge | Musk ox snort-roar, elephant rumble, yak grunt | Icicle clatter | Low ring call when the herd rings up |
| Frostantler | Elk bugle, reindeer grunt, red deer roar | Antler ice chime | Bugle echoing across the hollows at dusk |
| Rimeshell | Crab clicks, armadillo grunt, pangolin hiss | Ice scrape | Clicks speeding up before the roll |
| Snowpuff | Ptarmigan croak, chicken cluck, quail | Feather fluff | Croak alarm |
| Aurora fox | Arctic fox scream and yips, red fox gekker | Faint glass shimmer | Night scream (unsettling, harmless) |
| Vulkarn | Tuvan kargyraa throat singing (bass), bull walrus, forge-bellows breath | Anvil ring as his laugh | Bellows breath before every slam |
| Skaldrith | Cassowary boom (sub-bass), elephant trumpet, lion roar | Glacier calving crack | Sub-bass boom you feel before you hear the roar |
| Obsidian Warden | Cassowary boom for breath, gorilla chest-beat pitched down | Glass grind and shatter | Glass creak as the shell cracks |
| Ignessa | Breathy female vocal (wordless), barn owl screech | Fire roar | Whispered laugh as wisps launch |
| Grimjaw | Gorilla roar, walrus, brown bear grunt | Ice crunch while chewing | Wet gnaw on the grab |
| Pale Hunter | Cougar scream (sounds like a woman), raven croak, bone clicks | Snow crunch | Mimics the player's own hurt grunt |
| Cinderhorn | Bull bellow, elk bugle scream, blast-furnace roar | Chain drag and spark | Chain-drag loop at 40 m |
| Old Tusker | Elephant trumpet, musk ox roar, walrus bellow | Icicle clatter | Trumpet before the stampede |

**Mixing rules:**
- Tells are mixed 3 dB over the bed.
- Every tell over 0.5 s rises in pitch (CAL "sound before sight").
- Creature calls duck music by at most 4 dB.

### 3.6 Music: Volcanic and Tundra (GAP-COOKED)

**Rules:**
- **Always audible.** Remove the engine's silent gaps; any silence is written into the piece.
- **Combat layer.** It's in the same key and tempo as the explore piece and comes in on the next bar, so it adds to the music instead of cutting it.
- **Ducking:** at most 4 dB.
- **Loudness:** −18 LUFS, calm −23, boss −15, true peak −1 dBTP (3.3).

**Volcanic: 66 BPM, D minor.** Taiko, low brass, anvil on the beat, a male throat-singing drone, frame drum.

| Cue | Length | Use | Feel |
|---|---|---|---|
| VOL_Explore_A / B / C | 3 to 4 min each | Day, anywhere in V1 to V5 | A: slow brass over taiko. B: anvil ostinato with drone. C: sparse frame drum and low strings (for V4) |
| VOL_Night_A / B | 3 min | Night | Drone and distant taiko, brass muted, embers in the texture |
| VOL_Combat (stems for A, B, C) | loops | Over explore within 50 m of an aggro creature | Taiko doubles, brass stabs |
| VOL_Mini | 3 min loop | Obsidian Warden, Ignessa, Cinderhorn | Shared theme; each mini gets a 4-bar intro motif (glass bells for the Warden, choir for Ignessa, chain percussion for Cinderhorn) |
| VOL_Boss_Vulkarn P1 / P2 / P3 | sections | Phase changes resequence on the bar | P1 anvil and brass march; P2 adds choir and doubles tempo feel; P3 adds a second anvil line (two hammers) |
| VOL_KilnRoads | 4 min | The lava tubes | Low drone, dripping percussion, a distant forge rhythm |
| VOL_Eruption | layer | Event | Rumbling sub-bass and war drums over whatever plays |
| VOL_Camp | 2 min | Spawn camps and your claimed Hearth | Warm low strings, the forge rhythm at rest |

**Tundra: 58 BPM, A minor.** Sparse piano, low strings, a wordless joik-style female vocal, tagelharpa (bowed lyre), bowed glass.

| Cue | Length | Use | Feel |
|---|---|---|---|
| TUN_Explore_A / B / C | 3 to 4 min | Day | A: piano and low strings with written 6 to 12 s rests. B: tagelharpa and vocal. C: bowed glass shimmer for T4 and T5 |
| TUN_Night_A / B | 3 min | Night | Vocal alone over a string pad; aurora shimmer |
| TUN_Combat (stems) | loops | Combat | Low string ostinato and frame drum |
| TUN_Mini | 3 min | Grimjaw, Pale Hunter, Old Tusker | Grimjaw: low brass grunts. Pale Hunter: breathing vocal, nearly silent. Old Tusker: horn calls |
| TUN_Boss_Skaldrith P1 / P2 / P3 | sections | | P1 string march; P2 choir rises with the flight; P3 whiteout: full choir over sub-bass, almost no melody |
| TUN_BlueBelow | 4 min | Ice caves | Glass harmonica, slow drips, a far vocal |
| TUN_Whiteout | layer | Event | Wind-noise pad and a pulsing low drum |
| TUN_Camp | 2 min | Camps and Hearth | Piano lullaby |

**Where the music comes from:**
- **First pass, licensed.** Pick real composed pieces per cue from CC0 and CC-BY libraries: OpenGameArt (today's source), Free Music Archive, Pixabay Music, incompetech. Match each cue's tempo, key and instruments. Where a source has no stems, its combat layer is a second piece in the same key, cross-faded on the bar.
- **Later, commissioned.** Commission the boss tracks once there's revenue.
- **No AI generation.**
- **Credits.** Credit every piece in `Docs/CREDITS.md` (Raven's rule).

### 3.7 Paths: Fire and Frost (4.2 rows)

**Trees (SPEC):** every node name, cost, gate and effect stays as in ST, `fire.json` and `frost.json`. Every node already has behaviour (section 0).

**RAISE 1: the three dead stat nodes** get their effects. Raven's node text already says what they do; these are the numbers.

| Node | Text (SPEC) | Effect per rank (GAP-COOKED) | Applies to |
|---|---|---|---|
| Fire: Shockwave | Bigger blast radius | +12% radius | Explosive bolt blast, Overheat, Meteor, Molten cleave lava, Giant's ember shockwave |
| Frost: Biting cold | Stronger slows | +10% slow strength (slow capped at −60% move speed) | Slowed status from any frost source of yours |
| Frost: Wide storm | Larger area effects | +10% radius or length | Frost nova, Blizzard, Ice wall length (pillar count rounds up) |

**RAISE 2: proc feedback.** Every passive shows and sounds like something when it fires. All of it is client-side FX from the server's hit events.

| Node | What you see | What you hear |
|---|---|---|
| Spreading flames | An ember streak (0.15 s) jumps from the burning target to the next | Crackle zip |
| Fuel the fire | Small orange motes drift from burning enemies into your Heat bar | Soft whoosh |
| Wildfire | The dying body swells with glow for 0.4 s, then a 3.5 m flame burst | Deep whump |
| Explosive bolt | Impact burst with debris; hitstop on hit | Crack and boom |
| Overheat | Your veins flare white for 0.3 s (a tell for nearby players), then the ring blast | Rising roar, boom |
| Searing parry | The parry spark becomes a flame tongue that wraps the attacker's limb | Hiss on the parry clang |
| Brittle | A glass-crack decal on the frozen target on a crit | High "tink" |
| Cold return | Blue motes flow into your Chill bar | Ice chime |
| Chain shatter | Crack lines race over the ground to each frozen enemy within 4.5 m; they shatter 0.08 s apart | Cascading shatter |
| Whiteout | Enemy swings inside your storm visibly whiff through a snow puff | Whiff |
| Cold riposte | The riposted target is crusted in ice | Freeze crackle |
| Frozen moment | A 4 m ice burst on a perfect dodge, plus 0.12 s camera hitstop on your screen only (no world slow-down in multiplayer) | Glass chime, then a boom |

**RAISE 3: homing.**
- Set `spells.lockHomingPlayers` to 0: a player's bolt never curves at another player.
- Keep `lockLead` (aim ahead on lock).
- Creature-target homing stays (it's the player's own spell). FULL-GAME's never-home rule protects the player being shot at.

**Body changes: Fire glowing veins (RAVEN-PLANNED, DD) and Frost (GAP).** Detail GAP-COOKED. These are built on the MetaHuman hero (R0).

**Strength:**
- Strength is the points in the path ÷ 100.
- On the main path it shows at full strength; as a subclass, ×0.6. Both show if you own both.
- Changes ramp smoothly; the thresholds below are where each part first appears.

**Fire: glowing veins.**

| At | What shows |
|---|---|
| 10 | Faint ember lines in the veins of the wrists and backs of the hands |
| 30 | Forearms |
| 50 | Upper arms, neck and collarbones |
| 75 | Chest centre line, temples; hair tips take a copper singe tint |
| 100 | Ember ring in the iris; heat shimmer around the hands at full Heat |

- **Glow:** brightness is 0.2 + 0.8 × Heat ÷ max Heat. It pulses at the heartbeat: 70 bpm calm, 110 in combat.
- **Colour:** ember orange #FF6A1A. Never crimson or gold (colour law).
- **Look:** veins are fine and luminous, never bulging. The skin takes a slight warm flush.

**Frost.**

| At | What shows |
|---|---|
| 10 | Breath mists on every exhale, everywhere, even in the volcano |
| 30 | Fingertips and nails go pale blue-white; a fine frost-fern pattern (real frost-flower shapes) from fingertips to wrists that glitters |
| 50 | The frost-fern reaches the forearms; lips cool to a pale rose-silver |
| 75 | Lashes and brows tipped with rime; cheekbone shimmer |
| 100 | Pale ice-blue iris ring #BFE6FF; frost sheen on the hair at full Chill |

- **Chill drives the effect:** the Chill bar sets glitter strength and breath density.
- **Look:** cool and glassy, never corpse-grey.

**Fire and Frost together:** at the Thaw moment (25/25 combo) a wisp of steam rises from the skin.

---

### 3.8 Fire and Frost invasions (3.3 rule; this pair's contents GAP-COOKED)

**Rules (3.3, unchanged):** the setting, the 5-minute warning, the 1.5 km target radius and the shrine hold all stay as written.

**Frost invades Fire (through either pass):**
- 8 to 14 creatures: Gloomhowl packs (at night) or Woolly drudges with Frostantlers (by day), plus 1 to 2 Rimeclaws.
- Old Tusker and his herd lead it. He leaves his route for the invasion.
- Each frost invader leaves a 6 m patch of slick ice where it stands for 4 s or more.

**Fire invades Frost:**
- Cinderfang packs, Scorchwings and 1 Magmaw.
- Cinderhorn leads it.
- Fire invaders melt the snow they pass into steaming slush that lasts 2 game hours.

**Payout:** fire and frost essence, plus the 3.3 path XP rule.

---

## 4. Plan: building it in Raven's repo

Follow Raven's patterns:
- Data first, in `Data/*.json`, with placeholder numbers marked as first drafts.
- Abilities are `UWRGameplayAbility` classes registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority throughout.
- Nothing edits `Data/paths/*.json` node text.

### 4.1 Data rows

| File | Change |
|---|---|
| `Data/tuning.json` `nodeEffects` | Add `fire.shockwave {"blastRadius": 0.12}`, `frost.biting_cold {"slowStrength": 0.10}`, `frost.wide_storm {"areaRadius": 0.10}` |
| `Data/tuning.json` `spells` | `lockHomingPlayers: 0` |
| `Data/tuning.json` `hazards` | `lavaDps: 20`, `lavaBurnPerSecond: 20`; new blocks `vent {interval:[30,50], tell:1.0, damage:35, launch:1200}`, `geyser {interval:40, tell:1.0, launch:1200, damage:10}`, `acid {dps:6, wearPerSecond:2}`, `crust {breakAfter:1.2}`, `drip {tell:1.0, damage:25, burn:30}`, `snowBridge {collapseAfter:0.9}`, `thinIce {massLimit:300, stages:3, frostPerSecond:25}`, `serac {tell:1.2, damage:60, radius:400}`, `katabatic {push:700, seconds:0.6, interval:[10,18], tell:1.0}` |
| `Data/tuning.json` `creatureAI` | Every attack in 3.3.8 and 3.4.8 as a row in the CAL 1.13 schema: `windup, trackUntil 0.5, delayHold, active, punish, damageMult, poise, flags[parryable, low, unblockable, grab], projectile {speed ≤ 1600, minRange 400}, socket, clip`. `spit.speed` 1800 → 1400. New kinds: `snap, combo_bite_whip, lava_burst, ember_shake, dive_rake, ember_spray, shell_slam, withdraw, gore_toss, rear_stomp, pinch, hot_spit, claw_stab_combo, spine_bristle, emerge, body_roll, howl_call, head_sweep, double_stomp, antler_charge, hoof_stab, shell_ram, curl_roll, cornered` |
| `Data/tuning.json` `bosses` | `cinderhorn {chargeSpeed:1400, chargeMax:4000, trailSeconds:8, trailDps:12, stuckSeconds:5, stuckDamageTaken:1.5, hornFreezeBuildup:150, hornFreezeSeconds:20}`, `old_tusker {tuskPartHealth:600, tuskBreakStagger:6, iceTrapSeconds:8, iceTrapDamageTaken:1.5, herdRingAt:0.3, trumpetRadius:12000}`; per-attack rows for all 8 bosses as in 3.3.9 and 3.4.9; `runbackStone` positions; `lockPoints` per boss |
| `Data/tuning.json` new `events` | `eruption {warnSeconds:300, plume:[0,600], fire:[600,3000], bombInterval:[8,14], bombOffset:[300,1200], bombRadius:350, bombDamage:70, bombBurn:50, bombTravel:1.2, safeCampRadius:5000, riverRise:50, coreSeconds:600}`, `whiteout {warnSeconds:300, visibility:2500, gust:600, gustSeconds:0.5, gustInterval:[10,20], driftSlow:0.85}`, `frequency` by setting |
| `Data/tuning.json` new `shrines` | XP numbers, re-guard days, fast-travel rules (3.1) |
| `Data/tuning.json` new `bodyChange` | Thresholds, colours, pulse rates, subclass factor 0.6 (3.7) |
| `Data/creatures.json` | New rows `cinderhorn`, `old_tusker` (role `mini-boss`, `roaming`, `route` id, `herd` for Tusker); `attacks` lists per 3.3.8 and 3.4.8; `anims` gets one clip per attack; new mesh paths for Scorchwing and Skaldrith |
| `Data/spawn_regions.json` | Split each side into the 5 sub-zones with `area` boxes (already supported); add `night` weight tables (3.3.7, 3.4.7) |
| `Data/items.json` | `obsidian`, `pele_hair`, `ancient_ivory`, `pine_resin`, `clear_ice`, `raw_fish`, `fire_pepper_seed`, `snow_turnip_seed`, `yoke_chain`, `trophy_cinder_horn`, `trophy_tusker_ivory`, `cinder_salve`, `spiked_boots`, `resin_fire_pot`, `rime_flask`, `resin_torch`, `obsidian_arrow`, lore items (Rime spearhead, Smith's tally, Giant's first hammer-head, Bellows rune, Hunter's last arrow, Smith's spike) |
| `Data/recipes.json` | Cinder salve (alchemy: sulfur ×3, Pele's hair ×2, ember essence ×1), Spiked boots (armorer: iron ingot ×2, hide ×3), Resin fire pot (workbench: pine resin ×3, fiber ×2), Rime flask (alchemy: frost essence ×1, clear ice ×2), Resin torch, Obsidian arrow (workbench: obsidian ×1, wood ×1, feather ×1), Rope ladder (workbench: fiber ×20, wood ×6) |
| `Data/pieces.json` | `rope_ladder` (freeform, allowed outside claims, decays in 2 game days) |
| New `Data/world_pois.json` | Hand POIs (3.3.3 and 3.4.3 tables with x, y, kind, prefab, light), landmarks, shrines, run-back stones, roaming routes (spline points), sub-zone polygons, kit rules (kind, sub-zones, count, min spacing 150, max gap 250) |
| New `Data/quests.json` | Sigrun's and Áilu's 6 quests each: id, giver, task type (bring, kill-with-status, light, craft, cross, sneak, trophy), targets, rewards |
| `Data/Audio/sounds.json` | `creatures.<id>` → own sets `tell/strike/hit/pain/idle/aggro/death`; `music.gap` → [0,0], `firstGap` → [0,0]; pools keyed `explore.volcanic.day/night`, `explore.tundra.day/night`, `combat.<biome>`, `mini.<biome>`, `boss.vulkarn`, `boss.skaldrith`, `sub.kiln_roads`, `sub.blue_below`, `event.eruption`, `event.whiteout`, `camp.<biome>`; `mix.musicDuckMaxDb: 4`; ambient beds per sub-zone |
| `Data/server_presets.json` | `biomeEvents` setting (Off, Rare, Normal, Frequent; default Normal) |

### 4.2 C++ (Raven's patterns)

| Where | Add or change |
|---|---|
| `WRAbilities.cpp` and the spell resolve path | Multiply spell radii by `1 + Stat("blastRadius")` (Fire) and `1 + Stat("areaRadius")` (Frost). Ice wall pillar count = ceil(pillars × that) |
| `WRStatusComponent.cpp` | Slowed strength × `1 + Stat("slowStrength")`, capped at 0.6 |
| `WRProjectile.cpp` | Homing off for player targets (data). Lava bomb as a ballistic `AWRProjectile` mode: arc, no homing, landing glow spawned 0.9 s before impact |
| `WRCreature.cpp` | Attack-row runner (shared with combat lane C9): load rows from `creatureAI`; play the attack's montage; `AN_WRWindup` / `ANS_WRActive` notifies; socket sphere sweeps during the active window; hits via `ApplyHit`; `delayHold` plays a final glint and sound 0.2 s before contact. Pack flank behaviour (one member circles behind), howl call, Woolly drudge ring formation, Basaltback line |
| `WRBoss.cpp` / `.h` | `BuildAttacks` rows for `cinderhorn` and `old_tusker`; new move rows for the 6 built bosses. **Damage on visible contact:** keep `InShape` only for drawn AoEs (floods, breath volume, trails); melee goes to socket sweeps in notify windows. Shrink `AWRGroundMarker` to small cues: a 1 m glow at a slam's centre, landing glows for thrown things, nothing for melee. Roaming: a route component (spline from `world_pois.json`) moves `Home` along the route; chase radius from the engage point; fight creatures in the way. Cinderhorn: charge sweep detects actors tagged `Solid` (basalt columns, walls, big trunks) → stuck stun; horn part frost buildup. Old Tusker: `PartHealth` per tusk lock point; thin-ice breakthrough via `FWRBiomeHazards::KindAt == "thin_ice"`; burn → flee to the nearest water; herd trumpet and ring. Lock points: list per boss on sockets, exposed to `WRPlayerController` lock-on switching. Tame-punish target pick (furthest tame) for slag throw and air breath |
| `WRWorldDressing.cpp` | `FWRBiomeHazards::KindAt` gains `vent, geyser, acid, crust, drip, snow_bridge, thin_ice, serac, katabatic` cells from `island_dressing.json` hazards. New actors in Raven's style: `AWRVent` (timer, tell, knock-up or launch), `AWRCrust` and `AWRSnowBridge` (step → crack → break), `AWRThinIce` (crack stages by mass on top), `AWRSeracFall`, and the katabatic volume. Kit and hand POI spawning from `world_pois.json` (Packed Level Actor prefabs) |
| New `WRShrine.cpp/.h` | `AWRShrine`, modelled on `AWRAltar`: guard spawns, a 3 s hold to light, the discovered set saved per clan in the world save, path XP payout, re-guard timer. Fast travel request from `WRUIMap` (server checks combat state, moves tames within 30 m) |
| New `WRPathMaster.cpp/.h` | `AWRPathMaster`: a character on the MetaHuman rig, idle work loop at the forge or racks, proximity lines (text plus voice), quest offer and turn-in into the Journal. Quest logic in a `UWRQuestComponent` on the player state reading `quests.json` |
| New `WRBiomeEvents.cpp/.h` | `UWRBiomeEventSubsystem` (world subsystem, server): scheduler from the setting; runs the Eruption and Whiteout timelines from `events`; drives `WRSky` (plume, ash fall, whiteout fog and wind), spawns bombs and cores, opens and closes the Kiln Roads rooms, switches spawn modifiers; replicates the state to clients for music and HUD hints |
| `WRSky.cpp` | Event fog, ash and whiteout states; sundogs and aurora on/off; sub-zone grade blends |
| `WRAudioDirector.cpp` | Biome and sub-zone pools; continuous play (no gap); combat layer bar-synced on top of explore; boss sections by phase; event layers; duck cap 4 dB |
| `WRSound.cpp` | Per-creature voice sets; tell events rise in pitch |
| `WRWildlife.cpp` | Night tables; sub-zone regions |
| New `WRBodyChange.cpp/.h` | `UWRBodyChangeComponent` on the hero: reads path points and the Heat/Chill bars, sets MetaHuman material parameters (vein mask reveal, glow, frost-fern reveal, sparkle, lip tint, iris ring) and the breath-mist Niagara. Replicated through the points already replicated. Waits on the MetaHuman hero (R0) |
| `WRFx` and `Data/fx.json` | The 12 proc recipes (3.7) and event FX (bomb trail, ash fall, crust cracks, whiteout snow) |

### 4.3 Assets and the art route

**Route:**
1. Concept, picked by Aaron.
2. Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. **Real clips** retargeted onto the rig with UE's IK Retargeter from Fab animal packs (bison or bull, elephant or mammoth, big cat, wolf, bat or dragon, tortoise, beetle) or Truebones animal mocap. The procedural `anim_creature.py` clips stay only as fallback blends for idle breathing.
6. Every creature is shown standing at scale beside the hero for Aaron before any behaviour is wired (3.3 rule).

| Asset | Route |
|---|---|
| Cinderhorn | New concept. Base from the Emberhorn concept language; horn, hump, yoke and chain sculpted in Blender. The chain is a separate mesh on a physics chain (AnimDynamics). Ember-seam emissive mask. Clips from a bison/bull pack: walk, trot, charge, gore, rear, stomp, horn-stuck struggle, sleep, death |
| Old Tusker | New concept. Built up from the Woolly drudge (6 legs); new tusks with ice crust; icicle cards; spear shafts. Clips from an elephant/mammoth pack retargeted to 6 legs (the mid pair follows the front with a phase offset): walk, stampede, trumpet, rear-crush, tusk sweep, flounder in water, death |
| Scorchwing | New bat-winged mesh (lean, membranous, ember-veined) to replace the shared MountainDragon. Clips from a bat or wyvern pack |
| Skaldrith | Own mesh: an ice wyrm with crystal spines, a longer neck than Scorchwing, wing ice-frills. Lock-point sockets on head, chest and both wings |
| 18 creatures | Attack clips per row (3 to 4 each) from Fab packs by body type; one clip per attack, matched to the row's wind-up and active frames |
| Path masters | MetaHuman presets (Sigrun, Áilu) with outfits from the survivor-outfit work (R0) |
| Landmarks | Fallen Smith and Burnt Wyrm skeletons (Blender, from bone kits on Fab), Bellows Stacks, Glasswall (terrain plus glass material), Tusk Gate, Great Inuksuk, Frozen Whaler (longboat from a Fab ship kit, iced), Slag Light |
| Kit prefabs | About 15 kinds per biome, 2 to 4 variants each, as Packed Level Actors; ash-grey shrub mesh for G22 |
| Hidden sub-biomes | Lava-tube and ice-cave room kits extend `Tools/art/cave_gen.py`; lava drip and meltwater FX |
| Materials | Pāhoehoe and ʻaʻā ground layers; sulfur crust; acid water; thin ice (dark) vs thick ice; sastrugi normal detail; snow-bridge crust |

### 4.4 Sound and music

**Voices:**
- **Sources.** Build from CC0 and CC-BY sources: Freesound CC0/CC-BY, the Sonniss GDC bundles (royalty-free, commercial use), Kenney. Use the Tools/audio pipeline Raven already has.
- **Process.** Per species, layer the 3.5 recipe in a DAW session, export each layer, and loudness-match.
- **Count.** About 8 files per creature (tell ×2, strike ×2, hit, pain ×2, death, plus idle and aggro loops). Bosses get 12+.

**Music:** pick the 3.6 cue sheet from libraries first (about 20 cues per biome counting stems), then update `sounds.json` pools and CREDITS.

**Sub-zone ambient beds:** 10 beds, one per sub-zone, from the same libraries (3.3.1, 3.4.1).

### 4.5 Map dressing

- `Tools/terrain/dressing.mjs` reads `world_pois.json`.
  - It places hand POIs at their coordinates (snapped to ground).
  - Then it fills kit POIs per sub-zone by count with Poisson spacing of at least 150 m.
  - Then it walks the walk map's routes (`island_walk.json`) and adds a kit POI wherever a route point is more than 125 m from any POI, so gaps never exceed 250 m.
  - It writes the result to `island_dressing.json` `pois`.
- Sub-zone ground layers are painted from the polygons in `world_pois.json` by the existing auto-material.
- Hazard cells for the new kinds are written from the same polygons (crevasse fields, thin-ice rings, vents).
- **Map files:** every placed actor is runtime-spawned from data (Raven's dressing pattern), so `L_Island` itself changes as little as possible. Say before touching it; maps can't be merged.

### 4.6 Build order inside this scope

Each step is one cook into Aaron's build. Then Aaron tests.

| # | Step | Why here |
|---|---|---|
| 0 | **Quick raises:** 3 dead stat nodes, PvP homing 0, spit ≤ 1600, lava 20 dps, music gaps 0 and the 4 dB duck cap | Small, data-heavy, fixes rule breaks now |
| 1 | **Proof of one: Cinderhorn end to end.** Concept, Aaron's pick, mesh, real clips, voice, attack rows with socket hits, route and lair (Plough Trench plus the basalt columns), counters, drops, music cue | Proves the roaming-mini frame, visible-contact damage, real clips, a fused voice and a set-piece place in one item |
| 2 | Old Tusker (same frame), plus thin ice and the Great Frozen Lake | Reuses step 1; adds the first micro-hazard |
| 3 | Biome music sets and the continuous-music director | Big feel gain, independent of art |
| 4 | Unique voices for all 18 creatures and 6 built bosses | Independent; one session per biome |
| 5 | Sub-zones and POIs: V1 Ash Plains first (proof of the density generator plus hand sites), then V2 to V5, then T1 to T5 | Biggest world raise; prove on one sub-zone |
| 6 | Micro-hazards (vents, geysers, acid, crust, snow bridges, seracs, katabatic) | After the sub-zones they live in |
| 7 | Shrines, fast travel, run-back stones, path masters and quests | After the POIs and the hero rig |
| 8 | Hidden sub-biomes: the Kiln Roads, then the Blue Below; secrets | After hazards (drips, bridges) |
| 9 | Events: Eruption, then Whiteout; the Pale Hunter's whiteout roam; the aurora night | Needs sky, hazards, the Kiln Roads vault |
| 10 | Creature attack rows for all 18 and the 6 built bosses' new moves and lock points (with combat lane C9 and C10) | Shares the frame from step 1 |
| 11 | Body changes (after the MetaHuman hero lands in R0) | Depends on the hero |
| 12 | Fire/Frost invasions | Needs both roaming minis and the invasion frame (3.3) |

**Gates:** compile, cook and package only. Aaron tests each step.
