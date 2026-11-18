# Cook: biome-desert (Desert / Sun & sand, plus dungeon portals, round R5)

Lane: biome-desert. Written 2026-11-13. Scope: FULL-GAME 3.2.3, the Desert rows of 3.3, the Sun & sand row of 4.2, and the dungeon portals that first open in R5 (FULL-GAME 8.2). It covers the whole Desert, the whole Sun path, and the portal system with its Desert rooms.

**Tags.** **SPEC** keeps Raven's design, or a built thing, exactly as written. **GAP-COOKED** is this lane's design and replaces the FULL-GAME first draft. **KEPT** means a number or name another 11-13 cook already set; it's restated here so the build has one page, and this lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** the Desert sits in belt B2 (North Coast), west of the Spine, at y −8,000 to −5,400. Neighbours: Fungal (north, a sinkhole karst border) and Storm peaks (south, a rib spur). Its opposite is Ocean & reefs, through **Saltgate** (+350, −6,700). Camps are at (−2,550, −5,900), (−2,550, −6,700) and (−2,350, −7,600). Ahrumet's arena is at (−600, −6,600), and the buried ruins at (−1,500, −7,000). There are 170 POIs (150 surface plus 20 buried). Weather weights: Clear 50, Overcast 5, Heat wave 30, Sandstorm 15. The people are the **Sunwatch**, and their waymark is gnomon stelae. The Dimming eclipse event is world-map's too.
- **combat.md:** the blind status, the Sun Wildbrands (Sun strike, Blazing arc, Noon strike) and the Sun spell numbers, Radiance as a *spender*, the attack-row schema, the boss template and visible-contact damage.
- **survival-crafting.md:** Dunewrap stats, the sand veil, prickly pear and date palm, Sandhen eggs, oasis fish, the date and pear tagine, rock salt and nitre, the sunstone lamp, the Sun mirror array, the Sun court building set, the three desert farm set-pieces, and the blueprint drop rates.
- **hero.md:** the Sun body change base row and Sun gold #E8B230.

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Monster Hunter Wilds, the Windward Plains and Balahara** ([Game8](https://game8.co/games/Monster-Hunter-Wilds/archives/501827), [Shacknews](https://shacknews.com/article/143627/monster-hunter-wilds-all-leviathans?amphtml=1), [Icy Veins](https://icy-veins.com/monster-hunter-wilds/windward-plains)) | One region cycles through fallow, storm (the Sandtide) and plenty. Balahara is a sand-swimming leviathan that ambushes from under the dunes and makes quicksand traps; a loud noise knocks it out of the sand for a stun | **The dunes hide the predator, and the ground is the tell.** Dunejaw and Ssarrok swim under soft sand and show a fin, a bulge and a hiss. Hard ground and wet sand are your counter (they can't dive there). A loud hit while one is diving stuns it. After a sandstorm comes plenty: sunstone yield is +50% (world-map KEPT) |
| **Breath of the Wild, the Gerudo Desert and the Molduga** ([Zelda Wiki](https://zeldawiki.wiki/wiki/Molduga), [Zelda Dungeon](https://www.zeldadungeon.net/wiki/Medicinal_Molduga), [Thonky](https://www.thonky.com/zelda-breath-of-the-wild/side-quest/medicinal-molduga)) | The Molduga senses vibration through sand and attacks from below. You stand on rock and bait it with noise, and a bomb in its mouth stops it. The desert is a gear and route check: you can only cross the sandstorm by following landmarks | **Vibration rules the sand.** Sprinting on soft sand draws Dunejaws; walking doesn't. You stand on rock and bait the leap. Ssarrok's swallow can be punished from inside. The **sandstorm belt** needs a guide: the Sunwatch lens, a tamed Scarab digger, or landmarks you can still see at 25 m |
| **Ark: Scorched Earth** ([Ark wiki](https://ark.gamepedia.com/File:ARK_SE_04.jpg), [Wikily](https://wikily.gg/en/ark-survival-ascended/maps/scorched-earth/)) | Heat drinks your water, sandstorms cut visibility and drain stamina, the Jerboa forecasts weather, and the Morellatops carries water and gives shade | **Utility tames solve the land.** The Wellfrog is a living water source and the Scarab digger is a navigator. The Gildback stores sunlight for the Sun path at night. **We don't copy** heat stroke or temperature: Raven rules them out (DD), so heat only raises thirst, as world-map KEPT says |
| **Elden Ring and Dark Souls** (CAL S-sources) | Big readable silhouettes, a boss that changes the arena, and lore in ruins only | Ahrumet tears out the gnomon and the plaza collapses into a pit. The Sunwatch story lives only in stelae, hulls and statues |
| **Ghost of Tsushima** (CAL) | The wind and birds guide you | Carrion kites circling in the sky mark a wounded beast, a kill, or *you* when you're hurt |
| **Dune Awakening, Conan Exiles** (general genre) | Open dunes as a hunted space; shade and water as the economy | Shade is a real thing in our desert: in a heat wave, roofs, rocks and palms stop the thirst drain. We use **no** thumpers, sandworm IP or shield tech. Our sand-swimmers come from the real sandfish skink and the Mongolian death-worm legend |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Colossi of Memnon ([Wikipedia](https://en.Wikipedia.com/wiki/Colossi_of_Memnon), [Amusing Planet](https://www.amusingplanet.com/2016/06/colossi-of-memnon-singing-statues.html), [Atlas Obscura](https://atlasobscura.com/places/colossi-of-memnon)) | A cracked statue "sang" at dawn, probably from warming and evaporating dew working through the cracks. Strabo heard something like a lyre or a snapping harp string. Roman repairs silenced it | **Ahrumet sings.** At every dawn his cracked chest gives one long, snapping lyre-tone that you can hear 2 km across the desert. It's his voice's base layer, and the tell for his eye beam is that same rising tone |
| Wadi Al-Hitan, the Valley of the Whales ([Wikipedia](https://en.wikipedia.org/wiki/Wadi_al_Hitan), [Live Science](https://www.livescience.com/planet-earth/whale-valley-the-whale-graveyard-in-the-sahara-desert-that-shows-they-once-had-feet-and-toes), [Nat Geo](https://www.nationalgeographic.com/travel/article/tour-egypt-valley-of-the-whales-wadi-al-hitan)) | Hundreds of ancient whale skeletons lie on the surface of the Egyptian Western Desert. It was a sea | **The desert was a sea.** The Whale Road landmark is a line of whale skeletons in the Erg. Ancient hulls sit in the dunes and shells lie in the sand. It ties to Saltgate's stranded fleet and to Tide being the opposite |
| Libyan desert glass in Tutankhamun's pectoral ([Archaeology](https://archaeology.org/issues/online/collection/trenches-egypt-tut-desert-glass/), [StarDate](https://legacy.stardate.org/radio/program/2019-08-12)) | A golden glass that an impact fused in the Sahara, carved into Tutankhamun's scarab | **Sunglass**, the Glasstail and Glass Empress material. The Hourhold's **Glass Scarab** lore item is a scarab of golden desert glass |
| Thorny devil ([Phys.org](https://phys.org/news/2016-11-thorny-devil-skin-gravity.html), [UWA](https://www.news.uwa.edu.au/archive/201611039172/research/flat-out-lizard-drinking-thorny-devils-remarkable-desert-survival/), [Live Science](https://www.livescience.com/animals/lizards/thorny-devil-the-spike-covered-lizard-that-sucks-water-from-sand-through-its-skin)) | Skin grooves wick water to the mouth, and it has a fake head knob as a decoy | **Gildback:** skin channels, and a false-head hump that takes ×0.5 damage while the real head ducks |
| Sekhmet and the red beer, the *Book of the Heavenly Cow* ([The Past](https://the-past.com/feature/the-book-of-the-heavenly-cow/), [TheCollector](https://www.thecollector.com/sekhmet-egyptian-goddess/)) | Ra's Eye, as a lioness, was slaughtering mankind. She was stopped by 7,000 jars of beer dyed red with ochre, which she drank as blood and slept | **Sekhra, the Lioness of the Eye** (rank 2 portal boss). Throw red ochre beer and she drinks, staggers and sleeps for 6 s |
| Scorpion fluorescence; Viking sunstone; dung beetles and the Milky Way; booming dunes; flash floods in slot canyons; lammergeier red-earth bathing; sandgrouse water carrying; the water-holding frog; Tuareg indigo veils; ushabti; Apep; fire-setting; Fata Morgana; Kolmanskop ([Scorpion](https://en.wikipedia.org/wiki/Scorpion), [Sunstone (medieval)](https://en.wikipedia.org/wiki/Sunstone_(medieval)), [Dung beetle](https://en.wikipedia.org/wiki/Dung_beetle), [Singing sand](https://en.wikipedia.org/wiki/Singing_sand), [Flash flood](https://en.wikipedia.org/wiki/Flash_flood), [Bearded vulture](https://en.wikipedia.org/wiki/Bearded_vulture), [Sandgrouse](https://en.wikipedia.org/wiki/Sandgrouse), [Water-holding frog](https://en.wikipedia.org/wiki/Water-holding_frog), [Tagelmust](https://en.wikipedia.org/wiki/Tagelmust), [Ushabti](https://en.wikipedia.org/wiki/Ushabti), [Apep](https://en.wikipedia.org/wiki/Apep), [Fire-setting](https://en.wikipedia.org/wiki/Fire-setting), [Fata Morgana](https://en.wikipedia.org/wiki/Fata_Morgana_(mirage)), [Kolmanskop](https://en.wikipedia.org/wiki/Kolmanskop)) | Real desert biology, myth and engineering | Glasstails glow cyan under the moon. The **Sunwatch lens** finds the sun through cloud and storm. **Scarab diggers** navigate by the stars. **Booming slipfaces.** **Flash floods** in the canyons. **Carrion kites** stained red. **Sandhens** soak their bellies. **Wellfrogs** give water. **Amastan's** indigo veil stains his skin blue. **Ushabti** servant constructs. **Apepa**, the serpent below the sun. Thermal shock cracks Ahrumet (hot stone plus cold water: fire-setting). **Mirages** of the buried city in the Long Noon. Sand-filled rooms in the Hourhold |

### 1.3 Lessons, short

1. **Ground is the language.** Soft sand (dark gold, rippled) means slow and swimmers. Hardpan (white, cracked) is fast and safe from swimmers. Wet sand (dark, glossy) is firm, but quicksand sits at its edges. Rock is safe. Each reads at a glance in day and night.
2. **Shade is a resource.** In a heat wave you plan your route by shade, and the Long Noon takes shade away. Ahrumet's fight is about shadow.
3. **Never make the empty desert empty.** Dunes are big, so POIs must poke up out of them (masts, ribs, stelae, pylons). Every POI tops a rise and reads at 1 km against the sky (world-map triangle rule).
4. **A storm wall is a door.** The Haboob Wall is visible from the whole desert. What's inside (the Sieves, the karst into Fungal, the best sunglass) pays for crossing it with the right gear.
5. **Never stack slow and blind on the same ground.** The belt slows and cuts vision. Blind comes only from creatures and spells, which you can see coming.
6. **No world boss.** FULL-GAME's draft "the Colossus walks" during the Long Noon breaks Raven's rule (DD rules out server-wide boss events and a world boss). It's replaced (2.10).
7. **Sun and night.** The Sun path is strong in daylight and needs a plan at night: sunstone lamps, a tamed Gildback, Dawn. That is Raven's "weak at night" made into a playstyle, not a dead zone.

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item | Status here |
|---|---|
| Path theme: blinding light, sandstorms, mirage decoys | SPEC (DD) |
| Look, sub-zones, signature resource, hazard, hidden sub-biome, event, shrine and path master, music | GAP-COOKED (2.1 to 2.11, 2.16). The shrine and path master existing is SPEC |
| Creatures (FULL-GAME had 7) | GAP-COOKED: 10. The 7 names are kept, and the Duskjackal, Gildback and Wellfrog are added so the roster fits Raven's mix (aggressive, neutral and passive) and the lowland gets a hunter |
| Mini-bosses (3) | Premises kept; fights GAP-COOKED (2.13) |
| Major boss Ahrumet | Premise kept (sundial plaza, eye beam, gnomon club, shadow, Tide weakness, Colossus eye); fight GAP-COOKED (2.14) |
| Desert rows of 3.3 (hazard, music, weather) | KEPT and restated |
| Sun & sand row of 4.2 | Nodes SPEC (ST); numbers KEPT from combat where set, the rest GAP-COOKED (2.17); body change GAP-COOKED on hero.md's base (2.18) |
| Gear | Dunewrap and sand veil KEPT; the rest GAP-COOKED (2.19) |
| Dungeon portals (8.2) | SPEC rules from DD; framework and the Desert rooms GAP-COOKED (2.22) |

### 2.1 Where it sits and how it reads (placement KEPT, look GAP-COOKED)

- **Footprint.** About x −3,000 (west coast) to −150 (the Spine's foot), and y −8,000 to −5,400. That's 6.5 km² of land after the coastline.
- **Bands.** These are KEPT (distance from the Spine crest). The lowland is the west coast and the outer dunes. The midland is the wadi, the Erg's core and the canyons. The highland is the White Pan and the Glass Canyon against the Spine.
- **First read from the camps.** Gold dunes roll east to a white line (the Pan), and behind it the Spine climbs into the Crown. Ahrumet's gnomon stands on the Pan as a black needle against the white. To the north, the brown Haboob Wall stands 300 m tall and never moves. To the south, red spires rise where the land climbs to Storm peaks.
- **Palette.** Dune gold #D9A85B, hardpan white #EDE6D6, canyon red #A8462E, oasis green #4E7A3A, sunstone amber #F2A23A, shadow blue #3B4A6B (shadows go cold blue at noon so they read as safe ground).
- **Light.** Hard noon sun with a short, crisp shadow, a strong sky dome and heat shimmer past 150 m in a heat wave (KEPT). Dawn and dusk are long and orange. Nights are clear and starry, with the moon very bright on white ground, the sunstone glowing amber (KEPT world-map readable nights) and Glasstails glowing cyan.

### 2.2 Sub-zones (GAP-COOKED; five, from FULL-GAME's list)

FULL-GAME's seven items fold into five sub-zones (world-map rule: 3 to 5). Oases and ruins become D3. Dunes become D2 and hardpan becomes D3's White Pan. Canyons and rock spires become D4. Wrecks become D1 (newcomer ships) and D2 (ancient hulls). The sandstorm belt is D5.

| Id | Sub-zone | Area, band, where | Ground (2-3 layers) | Palette | Prop kits (≥3, unique) | Signature silhouette from its border | Micro-hazard | Day / night creatures |
|---|---|---|---|---|---|---|---|---|
| D1 | **The Wreck Strand** | 1.2 km², lowland. The west coast from y −7,450 to −5,400, 300 to 700 m deep. Camps 1 and 2 | Salt crust (sabkha), wet shell sand, sea-grass dune | White, sea grey-green, rope brown | Newcomer wrecks (KEPT world-map rule: every camp has its ship), salt pans, driftwood shelters, sea-grass hummocks, shell middens | The **Tall Wreck**: a three-masted newcomer ship thrown 200 m inland on a dune | **Brine crust** | Sandhen, Wellfrog, Dromad herds, Duskjackal (night) |
| D2 | **The Erg** | 1.6 km², lowland to midland. The centre-north, x −2,300 to −1,000, y −7,450 to −6,200 | Soft dune sand (rippled), slipface sand, shell grit in the troughs | Gold, ochre, bone | Star and crescent dunes (terrain), ancient hull ribs, whale bones, half-buried caravans, ventifact rocks | **The Mother Dune**, 140 m, and the **Whale Road** | **Booming slipfaces** | Dunejaw, Shimmerbuck, Scarab digger, Carrion kite; Duskjackal and Glasstail at night |
| D3 | **Oasis Reach and the White Pan** | 1.7 km², midland to highland. A dry river (wadi) runs from the coast at y −6,000 east to the White Pan (x −1,100 to −150, y −7,450 to −6,000), the dry lake the river died in | Oasis clay and grass, wadi gravel, white hardpan (cracked polygons), salt | Palm green, white, turquoise water | Date palms and acacias, wells and qanat shafts, Sunwatch waystations, gnomon stelae, sun-temple pylons, salt cakes | **The Broken Sun Gate** (Great Shrine) and **Ahrumet's gnomon** | **Dust devils** (the Pan only) | Gildback, Dromad, Sandhen, Wellfrog, Carrion kite; Glasstail and Duskjackal at night |
| D4 | **The Red Canyons** | 1.0 km², midland to highland. x −2,300 to −150, y −6,000 to −5,400, against the Storm peaks rib spur and the Spine | Red sandstone, slot-canyon sand, varnished rock (desert varnish streaks), fulgurite glass near the Spine | Rust red, purple-black varnish, green-black glass | Hoodoos and spires, slot canyons, rock art and handprints, malachite (copper) seams, nitre crusts (KEPT), bat roosts | **The Needles**: a forest of 40 to 80 m red spires | **Flash floods** | Glasstail, Carrion kite (nests on spires), Gildback; Duskjackal at night |
| D5 | **The Haboob Wall** | 1.0 km², lowland to highland. The north edge, x −2,200 to −150, y −8,000 to −7,450, along the Fungal border | Blown sand, scoured pavement (desert pavement), the karst sinkhole rims | Brown, rust, dim grey | Sand-blasted ruins, buried Sunwatch towers (tops only), sinkhole rims into the Fungal caverns, storm-wrecked caravans, fulgurite fields | **The Wall itself** (300 m tall, seen from everywhere) | The belt is the hazard (no other) | Duskjackals (they hunt by scent in the storm, day and night), Dunejaw, Glasstail |

**Blends** are 60 to 120 m wide (KEPT), with no hard texture lines. The Fungal border inside D5 is a karst field of sinkholes in the sand (KEPT world-map). The Storm border in D4 is a red rib spur with two river gaps, the dry wadis that flood.

### 2.3 Skyline landmarks (GAP-COOKED; 11, about one per 0.6 km², each read from 1 km)

| # | Landmark | Where (x, y) | Silhouette | What it is |
|---|---|---|---|---|
| L1 | **The Haboob Wall** | The D5 line, y −7,450 | A 300 m brown wall of moving dust with a cauliflower top, lightning flicker inside at night | The sandstorm belt; never moves (2.8) |
| L2 | **Ahrumet's Gnomon** (super landmark, reads from 3 km) | (−600, −6,600) | A 30 m black basalt obelisk with a hole near its top, and beside it a seated 16 m colossus | The arena (2.14) |
| L3 | **The Broken Sun Gate** | (−1,700, −6,050) | Two 22 m sandstone pylons with a fallen lintel, a sun disc carved between them | The Great Shrine (2.11) |
| L4 | **The Mother Dune** | (−1,450, −7,250) | A 140 m star dune with three razor arms; it booms when walked (real) | Ssarrok's lair at its foot; one of the Hourhold entrances on its lee side |
| L5 | **The Whale Road** | (−1,850, −7,000) to (−1,250, −6,800) | A 600 m line of 20 to 40 m whale skeletons, ribs up, half in sand | The old sea (1.2) |
| L6 | **The Keelyard** | (−2,050, −6,450) | Twelve ancient sun-barque hulls (cedar, high curved prows) standing on end in the sand like a fence | The Sunwatch dragged the sun-boats here when the sea left |
| L7 | **The Tall Wreck** | (−2,700, −6,300) | A newcomer three-master on a dune crest 200 m inland, masts at 30° | Story site S1 |
| L8 | **The Fallen Hour** | (−900, −7,200) | A 12 m colossus head toppled on its side on the Pan, sand to the eyes, with a broken face (Ozymandias) | The colossus Ahrumet's twin; a Hourhold entrance under the jaw |
| L9 | **The Needles** | (−1,100, −5,650) | 30 to 40 hoodoo spires, the tallest 80 m, with kite nests and rope bridges | The Carrion kite roost and a canyon maze |
| L10 | **The Glass Canyon** | (−350, −5,600) | A canyon mouth that flashes green and black in the sun: its walls are fused glass | The Glass Empress's lair (2.13.3) |
| L11 | **The Sevenfold Wells** | (−1,250, −6,250) | A crown of 40 tall date palms round a stone qanat tower | The biggest oasis and the Mirage Matriarch's water garden (2.13.2) |

**Plus one border landmark (KEPT idea from world-map, fixed here):** the **Leviathan Ribs** at the Saltgate west mouth (−150, −6,700). It's a 90 m sea-serpent skeleton bleached chalk-white by the sun, with Sunwatch bronze spear-heads still in its vertebrae. See 2.20 for the champion-row fix.

### 2.4 POIs: 150 surface + 20 in the Hourhold = 170 (count KEPT from world-map; contents GAP-COOKED)

**Mix:** 11 landmarks (2.3), 6 shrines (2.11), 35 hand-made story sites (below), 98 kit POIs (below). That's 150. Spacing, the 125 m max gap and the triangle-rule siting are KEPT.

**Hand-made story sites (35).** Every one tells its story with placement only (DD SPEC; no actors, no staged scenes).

| Id | Sub-zone | Where (x, y) | Site | What the place says |
|---|---|---|---|---|
| S1 | D1 | (−2,700, −6,300) | **The Tall Wreck** (L7) | The castaways' ship. A rope ladder hangs from the rail, and the hold is full of empty water casks with their bungs out. A grave line of 6 driftwood crosses faces the sea. The cask labels match the camps |
| S2 | D1 | (−2,600, −7,100) | **Salt-cutters' camp** | Salt cakes are stacked to dry. One stack is dragged in a line toward a Dunejaw trough, ending in a snapped harness |
| S3 | D1 | (−2,800, −5,700) | **The Signal Fire** | A driftwood beacon pyre, never lit, with a Saltkin shell-chime tied on top (contact with the other side long ago) |
| S4 | D1 | (−2,400, −6,950) | **Brine-pit drowned** | A Dromad skeleton half out of a brine crust, with its pack still on. Salt has crystallised over the cargo of copper ingots (loot) |
| S5 | D1 | (−2,500, −5,550) | **The Caravan Oasis** (KEPT farm set-piece) | Date palms, a stone well, a broken salt-caravan, its guards' weapons pointing outward |
| S6 | D1 | (−2,850, −7,300) | **Sea-grass shrine of the newcomers** | A ring of shells and a carved driftwood sun round a cold fire. Someone tried to pray to what they found here |
| S7 | D2 | (−1,650, −7,150) | **The Ship in the Dune** | An ancient sea-going hull with oar ports. A Sunwatch sun-disc has been nailed over its old sea-god figurehead (the sea people's ship, taken by the sun people) |
| S8 | D2 | (−1,900, −6,700) | **Jackal tomb** | A tiny rock-cut tomb whose door has been dug out from inside the dune. Duskjackal pups are in the burial niche, and gilt dust is on the floor |
| S9 | D2 | (−1,300, −7,000) | **The Swallowed Caravan** | Eight Dromads and their loads in a perfect line, each sunk to the neck in a dune. The last one is gone, with a 6 m trough where Ssarrok pulled it under |
| S10 | D2 | (−2,150, −7,300) | **The Navigator's Stones** | Three ventifact rocks with sun lines cut into them. A Sunwatch lens holder is on top, empty (quest 2 lens recipe) |
| S11 | D2 | (−1,100, −6,550) | **Dune-buried stelae avenue** | Gnomon stelae march into a dune and come out the other side 200 m later. The dune walked over the road |
| S12 | D2 | (−2,250, −6,550) | **Kite feast** | A fresh Dunejaw carcass on a dune crest with 6 Carrion kites on it, a living set-piece that respawns with a different carcass |
| S13 | D2 | (−1,500, −6,350) | **The Lost Expedition** | Newcomer tents half under sand, a shovel stuck in a dug pit that exposed a carved lintel (Hourhold entrance H-E3), and notes in a lore item |
| S14 | D2 | (−1,750, −7,400) | **The Booming Slope** | A slipface with handprints in the crest's crust: someone slid down and kept sliding. The boom is loudest here |
| S15 | D3 | (−2,200, −6,050) | **Waystation of the First Hour** | A roofed Sunwatch rest house with stone benches facing east, and a water channel still trickling |
| S16 | D3 | (−1,950, −5,950) | **The Qanat Line** | A line of 9 shaft mouths every 30 m. Shaft 6 is open into a flooded tunnel (Cave C2) |
| S17 | D3 | (−1,400, −6,100) | **Gardener's Last Row** | A walled water garden whose channels are blocked by one fallen stone. Everything below the stone is dead and everything above it is green |
| S18 | D3 | (−900, −6,050) | **The Measuring Wall** | A 40 m wall of carved marks (sun angles over years). The last third is unfinished, with the chisel still in the groove |
| S19 | D3 | (−1,050, −6,800) | **The Pilgrims' Ring** | A circle of 30 kneeling stone figures facing Ahrumet. Every head is broken off and lies facing *away* |
| S20 | D3 | (−400, −7,000) | **Salt cathedral** | A salt dome eroded into arches on the Pan, cut with tool marks, with a natural lens of salt crystal in the roof that throws a sunbeam spot that moves |
| S21 | D3 | (−750, −6,350) | **The Avenue of Hours** | A 300 m processional road of 24 stelae to Ahrumet's plaza. The run-back stone is here (2.14) |
| S22 | D3 | (−1,600, −6,250) | **Sunken Greenhouse** (KEPT farm set-piece) | A glass dome buried to its shoulders, dead off-biome plants inside, one living |
| S23 | D3 | (−300, −6,300) | **Cartographer's Table** | A stone table with a map of the old coastline carved into it, showing the sea where the Pan is |
| S24 | D4 | (−1,800, −5,600) | **The Nitre Canyon** (KEPT farm set-piece) | White crusts on red walls, a scraper left in a crust, bat roost above |
| S25 | D4 | (−1,300, −5,500) | **Hands Wall** | Hundreds of ochre hand stencils. One newer set of small hands is in blue (indigo), at child height. It's Amastan's people's mark (quest 3) |
| S26 | D4 | (−700, −5,750) | **Copper Drift** | A Sunwatch copper mine with green malachite walls, wooden ladders and a cage of bones where a Glasstail nest took the miners |
| S27 | D4 | (−1,050, −5,450) | **Flood Line** | A slot canyon with a dead tree jammed 6 m up the walls and debris lodged at the same height. The canyon floods (2.8), and this shows how high |
| S28 | D4 | (−500, −5,450) | **The Kite Cliffs** | A cliff face of Carrion kite nests with bones dropped below in a fan (lammergeiers drop bones to break them) |
| S29 | D4 | (−1,600, −5,750) | **The Lookout** | A spire-top Sunwatch watchpost with a bronze mirror on a pivot that points at Saltgate (a signal mirror; it works, see secret X3) |
| S30 | D5 | (−1,900, −7,700) | **The Last Caravan** | A full caravan in the storm, sand-blasted to bare wood and metal, its beasts' bones roped in a circle round it. They stopped and waited out a storm that never ended |
| S31 | D5 | (−1,200, −7,850) | **The Sieves** | Sinkholes with dunes pouring into the Fungal caverns' skylights (KEPT border), glow from below at night |
| S32 | D5 | (−600, −7,700) | **Storm Tower** | A Sunwatch tower whose top 8 m stick out of the sand. Its windows are packed solid with sand on the windward side and open on the lee side, where the Sunwatch lens recipe lies |
| S33 | D5 | (−1,600, −7,600) | **Glass Field** | A field of fulgurite tubes where lightning in the Wall hits sand: sunglass harvest |
| S34 | D5 | (−400, −7,900) | **The Faceless Sentinels** | Two 8 m guardian statues sand-blasted smooth, with no faces left |
| S35 | D5 | (−2,100, −7,800) | **Storm Wreck** | A newcomer skiff dragged 1 km inland by storms, with its sail wrapped round a stela |

**Kit POIs (98), placed by the dressing generator (2 to 4 variants each):**

| Kit type | D1 | D2 | D3 | D4 | D5 | Total |
|---|---|---|---|---|---|---|
| Wreck fragment (newcomer or ancient) | 6 | 4 | 0 | 0 | 2 | 12 |
| Half-buried caravan | 2 | 4 | 3 | 1 | 2 | 12 |
| Gnomon stela (waymark, KEPT) on old roads | 1 | 3 | 5 | 2 | 1 | 12 |
| Ruin (wall, hut, cistern) | 1 | 3 | 5 | 2 | 3 | 14 |
| Ore or salt outcrop (sunstone, copper, rock salt, sunglass) | 2 | 2 | 4 | 4 | 2 | 14 |
| Creature set-piece (a nest, a kill, a burrow, a herd at water) | 3 | 4 | 3 | 3 | 1 | 14 |
| Small oasis or seep (a palm or two, a spring) | 2 | 1 | 4 | 1 | 0 | 8 |
| Cave mouth or slot entrance | 0 | 0 | 1 | 4 | 1 | 6 |
| Bones (whale, Dromad, giant) | 2 | 2 | 0 | 1 | 1 | 6 |
| **Total** | 19 | 23 | 25 | 18 | 13 | **98** |

**Night rule (KEPT):** every POI has a light or a skyline shape. In the desert that means sunstone glow on ore, oasis water catching the moon, lit braziers at shrines and newcomer lanterns at camps.

### 2.5 Hidden sub-biome: the Hourhold (FULL-GAME "buried ruins"; position KEPT; layout GAP-COOKED)

**What it is.** It's the Sunwatch's temple city, swallowed by the Erg. Its centre is (−1,500, −7,000), 20 to 70 m under the dunes. It's about 0.35 km² of streets, halls and vaults, and it's where the desert's deep ore sits.

**Three entrances, and the sand decides which are open.** Real dunes migrate and bury and expose ruins (Kolmanskop).

| Entrance | Where | Opens |
|---|---|---|
| H-E1, the Lee Door | The Mother Dune's lee side | After every Sandstorm weather block, for the next 3 game hours |
| H-E2, the Jaw | Under the Fallen Hour's jaw (L8) | Always open, but past a quicksand floor (sink over 4 s, KEPT) |
| H-E3, the Dig | S13, the Lost Expedition | Always, once you clear the dug pit with a shovel action (hold Interact 4 s) |

**Scarab diggers** sense the closed doors within 40 m. Their antennae fan out and they chirp.

**The 20 POIs:**

| # | Room | What's there |
|---|---|---|
| 1 | Sand-choked street | Dune sand pours through a broken roof into a street. You slide down it to get in |
| 2 | The Hall of Hours | A 60 m hall of 24 pillars, each with an hour carved on it. Light shafts from roof holes move across the floor with the real sun |
| 3 | Scribes' rooms | Lore items: 4 tablets |
| 4 | Granary | Sealed jars of grain turned to dust, and one jar of still-good dates (a never-spoil food, KEPT) |
| 5 | Cistern Below | A huge pillared cistern with dark water. Oasis catfish live here (the fishing spot for **the Sandfather**, KEPT, is in its deepest well) |
| 6 | Copper Deep | A copper and tin seam (midland ore) |
| 7 | Iron Deep | An iron seam 60 m down (the deepest ore, CF depth rule) |
| 8 | The Sun Barque Dock | A dry dock with a 30 m sun-barque on rollers, its oars laid out (a hand-made set-piece) |
| 9 | Ushabti Hall | 200 small servant figures in rows. 12 of them are animated Ushabti that wake (a fight; see the portal bestiary for the look) |
| 10 | The Weighing Room | A great bronze scale with a feather on one pan (the portal puzzle's origin) |
| 11 | Glasstail nest | A brood chamber, highland tier |
| 12 | Jackal kennels | Gilded collar hooks; Duskjackals den here |
| 13 | The Mirror Gallery | Bronze mirrors on pivots carry a light shaft round 4 corners to a sealed door. Turning them is a puzzle that opens room 14 |
| 14 | The Sun-Vault | A sealed treasury: a Glass Scarab lore item, sunstone ×20, a gem and a sunglass ring |
| 15 | The Drowned Stair | A stair down into sand that flows like water (quicksand). It leads to 16 |
| 16 | The Seventh Tilted Vault | Opens only in the Long Noon (2.10) |
| 17 | The Collapse | A 40 m void where the city fell into a sinkhole. Climb down by rope |
| 18 | Workshop of Faces | Unfinished colossus faces. Ahrumet's face is here, with an unfinished eye socket |
| 19 | The Lens Forge | Where they ground sunstone into lenses. The Colossus eye's empty mold (lore) |
| 20 | The Spring | A living spring at the bottom: the source of the dead river. A Great Shrine-style brazier gives a run-back point (not a shrine) |

### 2.6 Caves (CF SPEC: every biome has caves; deeper is better)

| Cave | Sub-zone, entry | Depth | Ore and finds | Creatures |
|---|---|---|---|---|
| C1 **Nitre Caves** | D4 (−1,800, −5,600), at S24 | 0 to 40 m | Copper (malachite), tin, nitre (saltpetre, KEPT), bat guano | Glasstail; bats as ambient |
| C2 **The Qanat** | D3 (−1,950, −5,950), from shaft 6 | 0 to 25 m, a flooded tunnel in parts (swim) | Copper, clay, oasis catfish | Wellfrogs; Glasstails at its dry end |
| C3 **Ssarrok's Hollow** | D2, at the Mother Dune's foot | 0 to 35 m, sand-floored | Sunglass, gold flakes (decor), Dunejaw teeth | Dunejaws (they swim in the sand floor); Ssarrok rests here |
| C4 **The Sieve Throats** | D5, at S31 | 0 to 60 m, joins the Fungal caverns (border) | Iron, sunglass, and one deepcap patch from below | Glasstails, plus Fungal creatures that wander up |
| The Hourhold | 2.5 | 20 to 70 m | Copper, tin, iron (deepest) | Highland |

### 2.7 Resources (GAP-COOKED; sunstone name KEPT)

| Resource | Where | Look | Use |
|---|---|---|---|
| **Sunstone** (signature, found nowhere else) | Sun-facing rock and the Pan's salt-cracks: D3 and D4, 60 nodes; blooms in the Long Noon (2.10) | Amber crystals with copper-glitter flecks inside (real aventurescent sunstone feldspar). They glow amber at night with the day's stored light: brightness = hours of sun that day ÷ 14 | Dunewrap, sand veil, sunstone lamp, Sun mirror battery (KEPT), blind resist potion and Clear-eye drops, the Sunwatch lens, Gildback taming food, Sun path materials |
| Sunglass | Glass Field (S33), the Glass Canyon, Glasstail drops, C3 and C4 | Gold-green translucent glass lumps (Libyan desert glass) | Sunglass arrowheads, the Empress's lens ring, glazing for Sun court windows |
| Rock salt | Pan and D1 (KEPT) | White cubes in crusts | Food preserving (KEPT) |
| Nitre | Canyons (KEPT) | White crust | Black powder (KEPT) |
| Sand | Dunes (KEPT) | — | Glass panes (KEPT) |
| Copper (malachite) | D4 seams, C1, Hourhold | Green-banded rock in red walls (Timna-style) | Copper and bronze |
| Acacia wood | D3 and D4: the only hardwood trees (plus palms) | Flat-topped thorn trees | Desert building, tool hafts |
| Palm fibre | Date palms | — | Rope |
| Clay and red ochre | Oasis banks, wadi cuts | Red and yellow earth | Pottery (water jars), ochre dye, red ochre beer (portal item) |
| Prickly pear, date palm (wild) | KEPT | KEPT | KEPT |
| Gold flakes | Wadi gravel, C3 | Glints in gravel | Decor only (gilding pieces in the Sun court set) |

### 2.8 Hazards

**The main hazard (KEPT, 3.3):** the sandstorm belt and soft dunes.

| Piece | Effect (KEPT numbers) | Detail (GAP-COOKED) | Passes with (KEPT) |
|---|---|---|---|
| **The belt (D5, permanent)** | Vision 25 m, run ×0.7, Radiance doesn't fill | Fog and particle sheet at 25 m. Sound: roaring wind, sand hiss, and music low-passed but audible (never muted, the music rule). The map's player arrow still works. Waypoints and the compass ribbon hide in the belt, because there's no sun to read. The Sunwatch lens (2.19) or a following tamed Scarab digger shows the bearing to your waypoint again | Sandwalker (root); sand veil (halves the slow and keeps vision 60 m); Dune oil (quicksand frees on the first dodge, sandstorm slow ×0.85, KEPT world-map) |
| **Sandstorm weather** (15% of blocks, KEPT) | The belt's effect over the whole desert, moving with the wind | It arrives as a **haboob**: a 300 m wall seen 3 km off, rolling west at 12 m/s off the Spine. It's 1 game hour (2.5 real min) from the first sight to arrival (KEPT world-map "changes are visible early") | Same |
| **Soft dunes (D2, D5, parts of D1)** | Slow | Run ×0.85, sprint stamina ×1.3 on soft sand. Hard ground is fast (white or rock). Mounts: the Dromad and Shimmerbuck ignore it | Sandwalker; the Dromad and Shimmerbuck |
| **Quicksand pockets** | Sink over 4 s, dodge out (KEPT) | They're real: they sit where water saturates sand, at oasis margins, wadi beds and at the foot of the Mother Dune. Tell: a dark, glossy, slightly wobbling skin, and a wet suck sound when you step on it (0.9 s before you start sinking). Each dodge lifts you 25%; four dodges get you out. At full sink you're held (no actions) and drown 5 s later: you die (no floors). An ally can pull you out (hold Interact 2 s) | Sandwalker (no sinking); Dune oil |

**Micro-hazards (GAP-COOKED; one per sub-zone, 4 in all; each with ≥ 0.9 s of sound plus a visual; each can kill):**

| Hazard | Where | Tell | Effect | Counter |
|---|---|---|---|---|
| **Brine crust** | D1 sabkha flats, 30 patches of 6 to 15 m | The crust has a faint wet-grey sheen and creaks under foot (1.0 s, getting louder) | It breaks: you drop 1.5 m into brine. Run ×0.4, no roll, 4 damage a second (salt in wounds) until you climb out (2 s climb at the edge) | Walk the white (dry) crust; rope planks (freeform, workbench: palm fibre 4, acacia 2) |
| **Booming slipfaces** | D2 dune lee faces steeper than 30° | The dune starts a deep 100 Hz hum (real booming sand) 1.2 s before it slides, and the crest grains start to flow | A sand avalanche carries you 12 m down the face. You're buried for 2 s (stamina −40, 20 damage). If a Dunejaw is under the face, it surfaces. Kill chance: a slide into quicksand at the foot | Walk the crest, not the face; Burrow (Sun path) through it |
| **Dust devils** | The White Pan (D3), 11:00 to 16:00, 2 to 4 at once | A visible spinning dust column 3 m wide and 20 m tall with a hiss you hear at 40 m, wandering at 4 m/s | It lifts you 1.2 s and throws you 6 m: 20 damage, blind 30. It also scatters loose items from the ground | See it, walk round it; ground items stay put inside a claim |
| **Flash floods** | D4 slot canyons and the two wadi gaps | Only after rain or a storm state upstream (the Storm peaks front crossing, KEPT world-map). Tell: a far roar 6 s ahead, a brown trickle at your feet 3 s ahead, then a 2 m wall of brown water | 50 damage, carried 15 m downstream with debris hits (10 each, 3 to 5 of them). Drowning in it is possible | Climb: every slot canyon has ledges and rope points at least every 40 m. The Flood Line site (S27) teaches the height |

**No heat meter.** The heat wave only raises thirst (KEPT). Shade stops it: any roof, rock overhang, palm canopy or the shadow of a large creature or landmark.

### 2.9 Weather (weights KEPT from world-map; detail reconciled)

| State | Weight (KEPT) | Detail |
|---|---|---|
| Clear | 50 | Hard sun, 30 km visibility, shimmer past 300 m at noon |
| Overcast | 5 | Radiance ×0.6 (KEPT). Rare: high dusty cloud |
| Heat wave | 30 | Heat haze (thirst ×2 in the sun, mirage shimmer past 150 m) plus Radiance ×1.2 (KEPT). **Mirage water:** fake lakes shimmer on the Pan and vanish within 150 m |
| Sandstorm | 15 | 2.8. The next block is +50% sunstone yield (KEPT plenty rule) and fresh-exposed ruins (H-E1 opens) |

**Conflict resolved:** survival-crafting's "Heat wave 35% / 20 min" yields to world-map's 30% weight in 3 game-hour blocks. survival-crafting's effect "Thirst ×1.5" yields to world-map's ×2. It's logged to LATER-COOKS.

### 2.10 Event: the Long Noon (FULL-GAME name kept; behaviour GAP-COOKED; frame KEPT from fire-frost 3.1)

**Sign (−5 min, world-visible):** the desert's sun stops climbing and goes white. A column of heat shimmer rises over the Pan, seen from the whole map. The **Sunwatch pillar on the Crown's Ring** burns gold (KEPT world-map beacon rule). At dawn-to-noon, every gnomon stela's shadow shrinks to nothing.

| Time | What happens |
|---|---|
| 0 to 60 min (one game day; the desert's sky holds at 12:00) | **No night falls in the desert.** Other biomes keep their clock. The desert's light holds at noon, and creatures keep their day tables |
| | **No shade from the sky.** Shadows fall straight down. Only roofs, overhangs, caves and canopy stop the heat haze, which runs all day (thirst ×2 in the sun) |
| | **Radiance never drains** and fills at ×1.5 anywhere in the desert, the belt included |
| | **Sunstones bloom.** Every sunstone node regrows with ×3 yield (the 3.3 triple rule). 20 extra bloom nodes rise on the Pan |
| | **Mirages show the buried city.** A Fata Morgana image of the Hourhold's towers, upside down, hangs over the Erg at (−1,500, −7,000). It's an image in the sky, not an actor, and it fades within 300 m. It's the land pointing at the hidden city |
| | **The seven tilted stelae.** Seven stelae lean a few degrees. At true noon they are the only ones that cast shadows, and each shadow points to a cache. The seventh points to the Hourhold's room 16, which opens only now |
| | **Creatures.** Glasstails and Dunejaws dig in (half spawns). Carrion kites ×2 and they circle any player under 50% HP within 1 min. Gildbacks bask on the Pan and are calm (tame window: offer sunstone, ×2 speed) |
| | **Ahrumet** doesn't sleep in the Long Noon, and his boss part chance is doubled for kills during it. His fight is unchanged (no world boss, no walking: lesson 6) |
| After | Night falls at once (a 2-minute dusk). Bloom nodes stay until gathered or for 1 game day |

**Pays:** sunstone ×3 (KEPT), the seven caches (sunstone, a gem, a blueprint chance 10%, gold flakes), and the Seventh Vault (2.5 room 16: a Sunwatch artifact with a 20% chance once artifacts exist, otherwise an Ascendant-tier sunstone cache).

### 2.11 Shrines and the path master

**Rules (KEPT, fire-frost 3.1):** guards, the 3 s brazier hold, path XP 1,000 / 500 / Great 2,000, fast travel between discovered shrines, re-guard every 4 game days, not safe zones.

**Six shrines.** That's one more than fire-frost's 5, because the desert has 5 sub-zones *and* a pass mouth (world-map rule).

| Shrine | At (x, y) | Guard |
|---|---|---|
| **Great Shrine of the Hours** (in the ruined sun-temple: SPEC "shrine in a ruined sun-temple") | The Broken Sun Gate, (−1,700, −6,050) | None (the path master) |
| Strand Shrine | (−2,650, −6,900), on a dune over the salt flats | 2 Duskjackal packs (it re-guards at night; by day the guard is a Dromad bull gone wild) |
| Erg Shrine | (−1,850, −6,850), on the Whale Road's biggest skull | 2 Dunejaws in the sand round it |
| Canyon Shrine | (−1,250, −5,600), on a spire reached by rope bridge | A Glasstail nest plus 2 Carrion kites |
| Wall Shrine | (−1,500, −7,650), inside the belt | 3 Duskjackals that hunt by scent in the storm |
| Saltgate Mouth Shrine (pass rule) | (−200, −6,600) | 2 Glasstails plus a Dunejaw at the tide-line sand |

**Path master: Amastan, the veiled guide** (FULL-GAME "a veiled caravan guide"; who GAP-COOKED; from real Tuareg dress, where men wear the indigo *tagelmust*).

*Look:*
- **Body and age:** a man of about 34, tall, lean and long-limbed, with a dancer's balance.
- **Face:** strong cheekbones, a trimmed beard line under the veil, and gold eyes (the Sun body change at 75, so a living preview).
- **Skin:** warm brown, and where the indigo veil and robe touch it, faintly blue (the real indigo stain).
- **Clothes:** an indigo tagelmust wrapped high with only the eyes and the bridge of the nose open, worn lowered to the chin when he speaks to you. A sleeveless white tunic open at the chest, a leather sword-belt with a straight sword (a *takouba*), silver-and-sunstone cross pendant, bare sun-gold forearms with the gold flecks of the path.
- **Voice:** quiet, warm, amused. He never shouts.

*Where:* by day in the Great Shrine's shade with a Dromad couched beside him, at night on the pylon's top reading the stars. Short lines within 6 m, no camera grab (KEPT).

*Quests:*

| # | Quest | Task | Pays |
|---|---|---|---|
| 1 | Water before gold | Fill 3 water jars at the Sevenfold Wells and bring them back before sunset | 800 Sun XP; recipe water jar (clay) and Oasis water jar |
| 2 | Read the sky | Craft a Sunwatch lens (recipe from S10's empty holder and S32's tower) and use it inside the belt to walk to the Wall Shrine | 1,000 XP; the lens |
| 3 | The small hands | Find the blue hands at the Hands Wall (S25) and bring him an ochre rubbing | 1,200 XP; lore; the Duskjackal taming tip (gilt meat) |
| 4 | The shadow's road | Stand on the Avenue of Hours at noon and follow the stelae shadows to the plaza's run-back stone | 1,500 XP; opens the Seventh Vault's lock for the next Long Noon |
| 5 | Glass in the sun | Show him the Empress stinger trophy | 2,000 XP; recipe Sunglass arrowheads |
| 6 | The swallowed road | Kill Ssarrok (the third, roaming mini) | 3,000 XP; 2 upgrade shards; recipe Dune oil |

### 2.12 Creatures (FULL-GAME's 7 names, roles and taming KEPT; 3 added; everything else GAP-COOKED)

#### 2.12.1 The ten at a glance

Tiers follow the 3.3 rule: aggressive Hard = highland, Medium = midland, Easy = lowland; neutral = midland stats; passive = lowland stats. Creature levels (R2) let highland tables field Glasstails and Duskjackals at highland stats.

| Creature | Role | Tier | Size | What it is (real roots) | Silhouette and look | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Duskjackal** (new) | Aggressive pack, nocturnal | Lowland, packs of 2 to 3 (4 in the belt) | 0.9 m at the shoulder | Golden jackal and fennec, with a tomb-guardian's stillness (Anubis, Wepwawet "opener of ways") | Very tall ears, long thin legs, a low bushy tail. A black coat with gilt dust on the muzzle and paws (it dens in tombs), gold eyeshine at night | Knockout and feed (gilt meat: meat + gold flakes, from quest 3). Easy | Hide, meat, jackal fang |
| **Glasstail** | Aggressive fighter | Midland | 2.2 m long, 0.9 m tall | Deathstalker scorpion; real UV fluorescence | Low, wide pincers held high, a segmented tail arched with a **gold-green sunglass bulb** at the stinger that glints in sun. Sand-coloured armour; at night under the moon it glows cyan-green | Knockout and feed. Medium | Chitin, sunglass shard, venom sac |
| **Carrion kite** | Aggressive scout (flyer) | Midland | 4 m wingspan | Lammergeier (bearded vulture), red-earth bathing, bone-dropping | Long diamond tail, narrow wings, a black beard of bristles, a chest stained rust-red, red eye rings. It circles high | Knockout and feed after you bring it down. Medium | Feathers (fletching), bone marrow |
| **Dunejaw** | Aggressive ambusher (sand swimmer) | Highland | 6 m long, 1.4 m tall surfaced | Sandfish skink (it swims in sand), a shark's jaw, the death-worm legend | A **sail-fin of grit-scales** cutting the sand. On the surface: a shovel snout, needle teeth in a wide grin, eyes on the top of the head, a flat body on short paddle limbs, a tapering tail | Knockout and feed, or Sun path magic. Hard | Dunejaw hide, teeth, meat |
| **Gildback** (new) | Neutral utility | Midland stats | 3 m long, 1.6 m tall | Thorny devil: skin water channels and the false head | Squat, armoured, covered in **amber spines** that store sunlight and glow at night, with a **false-head hump** behind the neck. A jerky, rocking walk (real) | Offer sunstone. Medium | Spines (sunstone shards ×2), hide |
| **Dromad** | Neutral hauler | Midland stats | 3 m at the hump | Dromedary with long legs; the real *dulla* (a pink throat sac males blow in rut) | A tall hump, a long arched neck, splayed padded feet, long lashes, nostrils that close in sand | Offer cactus fruit (prickly pear, KEPT). Medium | Dromad leather (Dunewrap), meat, wool |
| **Shimmerbuck** | Neutral mount | Midland stats | 1.5 m at the shoulder | Arabian oryx: in profile, one horn, the unicorn legend | Long straight spiral horns, a white body with black face and leg bands. It leaves a **mirage** of itself when spooked | Knockout and feed. Medium | Hide, horn (spear tips), meat |
| **Scarab digger** | Passive utility | Lowland stats | 1.2 m long | Dung beetle, Khepri rolling the sun, Milky Way navigation | A domed, metallic gold-green shell, rake-toothed front legs, fan antennae. It rolls a ball of sand bigger than itself | Steal an egg (from the brood balls in ruin burrows). Easy | Shell plates, dye |
| **Sandhen** | Passive farm | Lowland | 0.6 m | Sandgrouse (males soak their belly feathers to carry water to chicks) | A plump ground bird, sand-barred, with a black chest band | Offer food. Easy (KEPT) | Eggs (KEPT), meat, feathers |
| **Wellfrog** (new) | Passive utility | Lowland | 0.4 m, round | Water-holding frog: buried in clay, swollen with water | A round, pale, translucent water-filled body with gold eyes. Buried in clay pans until it rains | Offer food (beetle grubs). Easy | — (never killed for drops; squeezing it gives water and it lives) |

**Utility of tames (GAP-COOKED):**
- **Scarab digger:** digs up buried ruin loot at dig spots (shown as a sand mound, 1 per 200 m); senses closed Hourhold doors and hidden rooms within 40 m; in the belt it walks toward your waypoint, so it's a living compass.
- **Gildback:** at a base its spines light 10 m round it at night (like a lantern). A Sun path player within 6 m of it at night fills Radiance at 0.5/s.
- **Wellfrog:** in a pen with a water trough, it fills 1 waterskin every 10 min. In the wild, squeeze one (Interact) for one full drink (thirst +30) and it goes back into the clay; it has a 2-game-hour cooldown.
- **Duskjackal:** a fighter that tracks. It points at the nearest wounded creature within 150 m.
- **Carrion kite:** a scout. Ordered to "circle", it hovers over your lock or map target, and the kite icon shows on the map.

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, in s)

Damage is × the tier hit (lowland 12, midland 20, highland 32). Every attack shows the real jaw, claw, sting or projectile, and damage only lands on visible contact (KEPT).

**Duskjackal (pack hunter, lowland)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes and tell |
|---|---|---|---|---|---|
| Bite | snap | 0-250 | 0.50 / 0 / 0.15 / 0.8 | 1.0 | A short snarl; the ears pin back |
| Hamstring | snap, `low` | 0-200, flank only | 0.55 / 0 / 0.18 / 0.85 | 0.8 | Slowed ×0.8 for 2 s. It comes from 90° to 150° off your facing (KEPT flank roles) |
| Lunge | lunge | 300-600 | 0.60 / 0 / 0.25 / 0.9 | 1.3 | A crouch and a whine |
| Chorus | roar (call) | — | 1.2 rooted | — | The yip-howl calls packmates within 60 m. Hitting it during the call cancels it |

**Glasstail (ground predator, midland)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes and tell |
|---|---|---|---|---|---|
| Pincer snap, 2-hit | sweep | 0-250 | 0.45 / 0 / 0.15, then 0.40 / 0.15 / 0.15 / 0.7 | 0.9 each | A pincer clack |
| Pincer grab | grab, `unblockable` | 0-220 | 0.90 / 0 / 0.30 / 0.9 | — | A crimson glint on both pincers. On hit: held 1.5 s, then a sting for ×2.0 and blind 40. An ally hit on the tail frees you |
| Sting | stab, pierce | 0-300 | 0.70 / ≤ 0.3 / 0.12 / 0.7 | 1.4 | Blind 40. The sunglass bulb flashes white 0.2 s before contact (KEPT final flash) |
| Glare | burst (light) | Cone 600, 60° | 0.90 / 0 / 0.2 / 1.0 | 0 | **Daylight only.** It raises the bulb to the sun and flashes: blind 60, but only if your camera is facing it within 60°. Turn away or roll away. A rising glass-ring tone |
| Sand burst (night) | burst | 0-300 | 0.90 / 0 / 0.2 / 0.8 | 1.2 | A night ambush from under sand. Tell: a sand bulge and a dry rattle 0.9 s ahead |

**Carrion kite (flyer, midland)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes and tell |
|---|---|---|---|---|---|
| Talon dive | dive | From 15 to 40 m up | 0.80 / 0 / 0.25 / 1.5 grounded | 1.4 | A screech as it folds its wings. Grounded 1.5 s after (wings are lock points; KEPT flyer rule) |
| Bone drop | projectile (ballistic) | Target within 20 m below | 0.9 release + 1.0 fall | 1.2 | It carries a bone or rock. The rock's shadow grows on the ground for the last 0.9 s; it never homes (KEPT) |
| Wing buffet | sweep | 0-300, grounded | 0.55 / 0 / 0.20 / 0.8 | 0.8 | Knockback 200 cm |
| Circle and call | roar | — | 20 s circling over a target under 50% HP, then a 1.5 s scream | — | Predators within 120 m turn toward the target. Kill or ground it before the scream |

**Dunejaw (ambusher, highland)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes and tell |
|---|---|---|---|---|---|
| Fin stalk | (move) | — | — | — | It swims at 9 m/s under soft sand with only its fin showing and a sand hiss. Sprinting on soft sand within 40 m draws it; walking doesn't |
| Burst lunge | burst | 0-500 | 0.90 / 0 / 0.30 / 0.8 lying out | 1.6 | A sand bulge, a rising hiss and the fin dropping. It opens on a lunge from below. A hit during the burst's first 0.3 s staggers it |
| Bite | snap | 0-300 | 0.40 / 0 / 0.15 / 0.6 | 1.0 | Surfaced |
| Double bite | snap ×2 | 0-300 | 0.40 / 0 / 0.15, then 0.34 / ≤ 0.6 / 0.15 / 0.6 | 1.0 each | The second bite can hold (highland delay) |
| Tail swipe | sweep, `low` | 0-400 behind and sides | 0.55 / 0 / 0.20 / 0.7 | 1.1 | Jumpable |
| Sand spray | spit (cone) | 0-500 cone 50° | 0.60 / 0 / 0.30 / 0.8 | 0.3 | Blind 50; it kicks sand with its front paddles |
| Re-dive | (move) | — | 0.8 vulnerable | — | On soft sand only. **It can't dive on hardpan, rock or wet sand:** there it fights on the surface and takes stance damage ×1.3 |

**Gildback (neutral; it only fights back)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes |
|---|---|---|---|---|---|
| False-head duck | (defence) | — | — | — | When threatened, it ducks its real head and presents the hump: hump hits take ×0.5. The real head (lock point) comes up for every attack |
| Spine flare | burst (light) | Nova 400 | 0.80 / 0 / 0.20 / 1.0 | 0.4 | Blind 60. Its spines brighten white (a white glint), with a rising hum |
| Spine roll | charge | 300-800 | 0.90 / 0 / 1.0 rolling / 1.2 | 1.3 | It curls into a spiked ball and rolls 8 m |
| Headbutt | snap | 0-200 | 0.60 / 0 / 0.15 / 0.8 | 1.0 | — |

**Dromad (herd heavy; neutral; the herd defends; hyperarmor on every attack, KEPT archetype)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes |
|---|---|---|---|---|---|
| Spit | spit (projectile) | 300-900 | 0.70 + travel 1,000 cm/s | 0.2 | Blind 20; a gargle, then a spray of cud. Bulls blow the pink *dulla* sac with a deep gurgle before it (the tell) |
| Neck slam | sweep | 0-350 | 0.75 / 0 / 0.20 / 0.8 | 1.2 | — |
| Rear kick | stomp, behind only | 0-250 | 0.60 / 0 / 0.15 / 0.8 | 1.3 | `behindOnly` |
| Body crash | charge | 400-1200 | 0.90 / 0 / 0.6 / 1.2 | 1.4 | A hoof scrape and a bellow |

**Shimmerbuck (herd charger; neutral)**

| Attack | Motion | Range | W / H / A / P | Dmg × | Notes |
|---|---|---|---|---|---|
| Mirage | (escape) | — | 0.3 | — | When spooked: it leaves a mirage of itself (no shadow) that stands 3 s, then it bolts. Creatures and players both see the mirage |
| Horn thrust | snap, pierce | 0-350 | 0.60 / 0 / 0.20 / 0.8 | 1.2 | — |
| Horn sweep | sweep | 0-300 | 0.65 / 0 / 0.20 / 0.8 | 1.0 | — |
| Charge | charge | 500-1500 | 0.90 / 0 / 0.8 / 1.0 | 1.5 | A hoof scrape. A wall or rock stuns it 2.5 s (KEPT) |

**Scarab digger, Sandhen, Wellfrog (passive):** they flee. A Scarab digger whose egg you steal rolls its sand ball at you (W 0.8, ball at 600 cm/s along the ground, 8 damage, knockdown) and then runs.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table)

| Sub-zone | Day | Night |
|---|---|---|
| D1 Wreck Strand | Sandhen 30, Dromad 25, Wellfrog 15, Carrion kite 15, Scarab digger 15 | Duskjackal 45, Sandhen (roosting) 20, Wellfrog 20, Glasstail 15 |
| D2 Erg | Shimmerbuck 25, Dunejaw 20, Scarab digger 20, Carrion kite 20, Dromad 15 | Duskjackal 35, Glasstail 30, Dunejaw 25, Shimmerbuck (sleeping rings) 10 |
| D3 Oasis Reach and Pan | Gildback 25, Dromad 20, Sandhen 20, Wellfrog 10, Carrion kite 15, Shimmerbuck 10 | Glasstail 35, Duskjackal 30, Gildback (glowing) 20, Wellfrog 15 |
| D4 Red Canyons | Glasstail 35, Carrion kite 30, Gildback 20, Scarab digger 15 | Glasstail 45, Duskjackal 40, Carrion kite (roosting) 15 |
| D5 Haboob Wall | Duskjackal 40, Glasstail 30, Dunejaw 30 | Same (no day or night in the storm) |

**Living-world beats (SPEC "herds defend each other, predators hunt prey"; contents GAP-COOKED):**
- Dunejaws hunt Shimmerbucks at dune troughs (the fin, then the burst).
- Duskjackals harry Dromad stragglers at night.
- Kites circle every kill.
- Gildbacks bask at noon on the Pan.
- Sandhen flocks fly to the oases at dawn, soaked-belly males fly back.
- At dawn, Ahrumet's song makes every Dromad in 2 km turn its head east.

### 2.13 Mini-bosses (premises kept from FULL-GAME; fights GAP-COOKED)

**Numbers (KEPT):** 3,000 HP solo, +80% per extra player or tame. Stance 90, 2 parries. Chase 65 m. 4 to 5 attacks and one counter mechanic. Windups 0.65 to 0.9 s, punish 0.8 s. Projectiles ≥ 0.9 s warning and never home. Drops: upgrade shards plus a trophy (SPEC).

#### 2.13.1 Ssarrok, the Dune Leviathan (roaming; folklore: the Mongolian death worm, Jonah)

- **Look:** a Dunejaw grown to 22 m, the length of a ship. Old harpoon heads and a Sunwatch bronze collar are sunk in its hide. Its grit-fin is 4 m tall and torn. One eye is milky.
- **Route:** a spline through the Erg (D2) from the Mother Dune's foot (its lair, C3) round the Whale Road, and at dusk down to the D1 strand's wet sand to hunt. It rests at the lair by day in a heat wave.
- **Counter (KEPT idea, made real): firm ground.** It can't dive into hardpan, rock or **wet sand**. Lure it there (its burst lunge carries it 20 m onto whatever you're standing on) and it **beaches**: 6 s floundering, belly up (a lock point), stance damage ×2. You make wet sand with the Oasis water jar (a 4 m splash makes 6 m of wet sand for 30 s) or any Tide ability (from R6); it ends its dive in the strand's tide-line sand too.
- **Weak to:** Tide ×1.5 (the biome's opposite; standard for desert creatures).

| Attack | W / H / A / P | Damage | Notes and tell |
|---|---|---|---|
| Fin wake | 0.9 / 0 / along a 30 m line / 1.0 | 30 | It swims a straight line under you, and the sand wave throws you up. `low`: jump it. Fin and hiss |
| Breach | 1.0 / 0 / 0.4 / 1.2 | 60, launch | It bursts up under you. Tell: the sand dimples and pours down into a cone, with a deep boom 1.0 s ahead |
| Swallow | 1.1 / 0 / 0.4 / 1.0 | Gulp: 12 a second | A surfaced lunge-grab, crimson glint on the lips, `unblockable`. **Inside:** 4 s, mash to struggle (−0.3 s per press). Allies hit the throat (a lock point, 400 part HP) to free you. If not freed, it spits you out: 40 damage and blind 80 |
| Tail breach slam | 0.8 / 0 / 0.3 / 1.0 | 45 | A 6 m ring wave of sand rides out at 1,200 cm/s (KEPT shockwave rule); jumpable |
| Sand geyser roar | 0.9 / 0 / 0.5 / 0.8 | 10 | A 70° cone, 12 m: blind 60 |

- **Trophy:** Leviathan tooth. **Drops:** upgrade shards, Dunejaw hide ×6, and the **Tooth of Ssarrok** talisman recipe (2.19).

#### 2.13.2 The Mirage Matriarch (folklore: the unicorn from the oryx; the Fata Morgana)

- **Look:** a Shimmerbuck doe of 3 m at the shoulder with 2.5 m spiral horns. Her white coat has a heat shimmer always rising off it.
- **Where:** the Sevenfold Wells (L11), a walled water garden of channels, palms and pools, 70 m across.
- **When:** she's diurnal and wakes at dawn. At night she sleeps in the deepest pool and can't be fought (no sun, no mirages).
- **Counter (KEPT): only the real one casts a shadow.**
  - **Mirage split:** she makes 3 mirages (1.2 s shimmer windup), so 4 does fight at once. Each mirage deals 50% damage and pops on any hit. A popped mirage re-forms after 12 s.
  - **Blind breaks them all:** blind buildup on the real one breaks every mirage and stops new ones for 15 s. Blind comes from the Sun path, Sunglass arrows, a flash pot (alchemy: sunstone + nitre), or a Glasstail's glare if you pull one in.
  - **Sun height matters:** her shadow is long at morning and evening and short at noon, so the fight is hardest at noon.
  - Under overcast, mirages last half as long.

| Attack | W / H / A / P | Damage | Notes and tell |
|---|---|---|---|
| Horn combo, 3 hits | 0.5 → 0.45 → 0.6 (hold ≤ 0.3) / 0.2 each / 0.8 | 35, 35, 50 | Thrust, sweep, rising gore |
| Wide sweep | 0.7 / 0 / 0.25 / 0.8 | 40 | A 200° horn arc |
| Rear stomp | 0.8 / 0 / 0.2 / 0.9 | 45 | `low`, a 5 m ring wave |
| Crossfire charge | 0.9 / 0 / 1.0 / 1.0 | 60 | All four does charge at once from different angles. A hoof scrape from each |
| Mirage split | 1.2 | — | As above |

- **Trophy:** Mirage antler. **Drop:** the **Mirage charm** recipe (2.19).

#### 2.13.3 The Glass Empress (real: fulgurites, desert glass; fire-setting)

- **Look:** a Glasstail of 9 m in the Glass Canyon (L10). She's armoured in **six fused-glass plates** (both pincers, two back plates, the tail and the stinger bulb), green-black and mirror-bright. Lightning from the Storm peaks border fused the canyon sand to glass, and she grew it on.
- **Counter (KEPT): shatter the glass.**
  - **Plates:** each has 300 part HP. While a plate is on, hits there deal ×0.3.
  - **Strike hits** (blunt: pick-hammer, great hammer, fists, charged heavies) deal ×2 part damage.
  - **Thermal shock (KEPT "frost then fire"):** a frost hit then a fire hit within 4 s shatters a plate outright.
  - **Shards:** a shattered plate sprays shards 4 m (15 damage to everyone near, her included).
  - **The stinger bulb:** shattering it removes the sting's blind and stops the Prism beam.

| Attack | W / H / A / P | Damage | Notes and tell |
|---|---|---|---|
| Pincer combo, 3 hits | 0.65 → 0.5 → 0.7 (hold ≤ 0.3) / 0.15 each / 0.8 | 30, 30, 45 | Clack, clack, crush |
| Crushing grab | 0.9 / 0 / 0.3 / 1.0 | 25 a second for 2 s, then a sting ×1.5 | Crimson glint, `unblockable` |
| Sting | 0.8 / ≤ 0.5 / 0.12 / 0.8 | 55 | Blind 80; white flash 0.2 s before contact |
| Prism beam | 1.1 / 0 / 2.0 / 1.2 | 15 a tick (0.25 s) | Her back plates focus the sun into a 20 m beam that sweeps 40° at 50°/s. It's daylight only and its start direction locks at windup end (no tracking). Rock blocks it. A rising glass-ring chord |
| Burrow and emerge | 0.9 / 0 / 0.3 / 0.9 | 40 | Sand bulge plus rattle |

- **Trophy:** Empress stinger. **Drop:** the **Empress's lens** ring recipe (2.19).

### 2.14 Major boss: Ahrumet, the Sun Colossus (FULL-GAME premise kept: sundial plaza, wakes when the sun crosses the gnomon, eye beam, stomps, sand waves; phase 2 gnomon club and shadow; hidden phase 3 noon lock; weak to Tide; Colossus eye. Fight GAP-COOKED)

**Numbers (KEPT rules):** 12,000 solo HP, +80% per extra player or tame. **Stance 180** (the heaviest body). 3 parries to break. Phase 2 at 50% by a telegraphed unblockable transition. Hidden phase 3 at 25% with artifacts (damage ×1.4). Weak to Tide ×1.5. Chase 90 m. Group of 3: 10 to 15 min.

**Look.**
- **Shape:** a 16 m sandstone colossus of a seated king, rising to stand. Broad shoulders, a pleated kilt of stone, and arms banded with bronze.
- **The face:** worn smooth by 3,000 years of sand: no nose, no mouth. One great **sunstone eye** sits in the brow and burns amber, white when it fires.
- **The crown:** a 6 m sun-disc ring stands behind its head.
- **Cracks:** cracks run through the chest. When it moves, sand pours from them like blood, and heat shimmer rises off its sun-baked stone.
- **The legs:** buried to the knee while it sits. It tears them free when it wakes.
- **Read at 15 m:** the eye, the ring and the arms against white ground. At 3 km: the ring and gnomon on the skyline (super landmark).

**Arena: the Sundial Plaza (−600, −6,600).**
- **The dial:** a 140 m disc of white limestone on the Pan, sand drifting at the edges.
- **The gnomon:** a 30 m black basalt obelisk at the centre with a sighting hole near its top.
- **The stelae:** 12 hour-stelae (4 m tall, 2 m wide) stand on a 50 m ring.
- **The throne:** Ahrumet sits on the north edge, facing the gnomon.
- **The noon line:** a bronze meridian strip runs from the gnomon to the throne.
- **Run-back stone:** at the foot of the Avenue of Hours (S21), 50 m south-west of the plaza edge (KEPT 40 to 60 m).

**How it wakes ("when the sun crosses the gnomon", made to work at any hour).**
- Stepping onto the meridian strip makes the gnomon's hole throw a focused shaft of light (sun by day, moon by night) up the strip onto the throne.
- The eye lights, the chest sings its dawn note (Memnon), and Ahrumet tears its legs from the sand (4 s, no damage) and stands.
- No camera grab: the boss bar appears as it stands.

**Counter (Tide weakness, KEPT, made physical with fire-setting): sun-hot stone cracks under water.**
- **Sun-hot:** after each eye beam and each Noon Pulse, Ahrumet's stone glows sun-hot (an orange heat glow on the arms, knees and chest) for 6 s.
- **Cracking:** a **water hit** on a sun-hot part cracks it: +60 stance, and that part takes ×1.5 damage for the rest of the fight (one crack per part per phase; parts are the eye, chest, both wrists and both knees). Water hits are the Oasis water jar (thrown, ballistic, 1,200 cm/s, 250 cm splash), any Tide ability (R6 on), or frost (a cold quench) at half the stance.
- **Tide also gets the opposite rule's ×1.5 damage (SPEC).**
- This gives R5 players a counter before the Tide path exists (as other cooks do: the swamp's stormglass jar, the forest's blight jar).

**Lock points:** the eye (head), the chest (the sun-heart crack), the left and right wrist, and the knees (one point). A stance break makes it **kneel** with the eye at ground height for 4 s: the riposte point.

**Phase 1 (100% to 50%)**

| Move | W / H / A / P | Damage | What you see and do |
|---|---|---|---|
| **Eye beam sweep** | 1.2 / 0 / 1.5 / 1.2 | 40 a tick (0.25 s), blind 30 a tick | It kneels its head forward. The eye brightens over 1.2 s with the rising Memnon tone. A 40 m beam then sweeps 90° at 60°/s. **The gnomon and stelae block it** (a line trace); a stela takes 3 beams to break. Jumping doesn't help; cover or i-frames do. It isn't a projectile, and its sweep direction is fixed at windup end |
| **Stomp combo, 3 hits** | Left 0.9 → right 0.6 → double-fist slam (hold ≤ 0.4) 1.1 / 0.2 each / punish 1.2 | 45, 45, 70 | Each impact sends a real sand ring wave out at 1,200 cm/s (`low`, jumpable; damage rides the wave front, KEPT). The slam is the big punish window |
| **Palm sweep** | 0.8 / 0 / 0.35 / 1.0 | 50, knockback 600 cm | It drags its palm 180° across the ground in front. `low`: roll through or jump |
| **Grab** | 1.0 / 0 / 0.4 / 1.0 | Squeeze 15 a second for 2 s, then the eye at point blank: 60 | Crimson glint on the palm, `unblockable`. Mash to shorten the squeeze. Allies who hit the held wrist (lock point) for 300 stance make it drop you |
| **Sand wave** | 1.0 / 0 / ~2.5 travel / 1.0 | 45 | It drags an arm through the drift at the plaza edge, and a 3 m wall of sand 20 m wide rolls across the dial at 800 cm/s. Roll through it with i-frames, or put a stela between you and it |
| **Gaze of Judgement** (tame-punish, KEPT rule) | 1.5 / 0 / 0.5 / 1.0 | ×2 against tames | Its eye fixes the furthest tame first (a thin amber line on the tame), then fires a short beam down that line |

**Transition at 50%: the Uprooting of the Gnomon** (KEPT rule: unblockable, 1.4 s, crimson, half damage taken during it).
- It strides to the centre, grips the gnomon and tears it out of the dial.
- The landing is a 360° swing of the obelisk (60 damage, `unblockable`; jump doesn't help, so roll or stay over 14 m away).
- **Arena change:** the centre of the dial collapses into a 20 m sand funnel (quicksand: sink over 4 s), and the sundial's ring of stelae is the only cover left.

**Phase 2 (50% to 0, or to 25% with artifacts)**

| Move | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Club combo, 4 hits** (new combo) | Sweep 0.7 → backhand 0.5 (hold ≤ 0.3) → overhead (delayed: 1.0, hold ≤ 0.6) → thrust 0.6 / 0.25 each / 1.2 | 55, 50, 90, 60 | The gnomon as a 30 m club. The overhead leaves a 40 m crack line of sand bursts |
| **Noon Pulse** (new delayed attack) | It plants the gnomon upright for 8 s and raises its eye to the sky. 2.0 s to burst | 80, blind 100, to everyone **not in shadow** | The sky whitens and a rising hum builds. The **gnomon's shadow** (a 4 × 30 m strip pointing away from its eye) and the shadows of standing stelae are safe ground. Its stone goes sun-hot after |
| **Held beam** (delayed variant of the phase-1 beam) | 1.2, then a 0.6 hold with the eye blazing / 1.5 / 1.2 | As phase 1 | It holds, then sweeps, and **reverses once** at the end of the arc |
| **Glassfall** (new delayed attack) | 1.0 / 0 / 1.2 travel / 1.0 | 40 each | Its eye melts sand into 3 molten-glass globs, lobbed on ballistic arcs at 1,200 cm/s, each with a growing ground glow for the last 0.9 s. They land as 4 m patches of cooling glass (slick 10 s). They never home |
| Phase-1 stomps, grab and Gaze | Idle gaps 0.3 to 0.9 s (KEPT) | — | — |

**Hidden phase 3 at 25% (artifacts only, KEPT flag; damage ×1.4): the Noon Lock.**
- The arena's sky snaps to noon and holds (the Long Noon in miniature).
- **The beam never stops:** the eye fires a continuous beam that turns 360° round the plaza at 30°/s while Ahrumet fights with the club.
- **Noon Pulse every 20 s:** at noon the only shadow is straight down, **under Ahrumet's own body.** The safe ground is under the colossus, where its stomps land.
- The fight becomes a dance between its legs: the boss's stomps and the sky's pulse together.
- **FULL-GAME's draft "the shadow is the only safe ground" is kept, and made concrete.**

**Drops.**
- **SPEC:** boss core; Sun path materials (Radiant essence ×6 to 10, sunstone ×10); a chance at the boss part, the **Colossus eye** (KEPT: critical hits blind the target; blind 60 on a crit, cooldown 3 s).
- **KEPT survival-crafting rule:** 2 blueprints, the **Gnomon Maul** (a boss weapon: great hammer; its moveset goes to the boss-weapons cook) and the **Sun court decor pack**. Plus a trophy head for monuments.

**Voice.** A deep grinding of stone (recorded rock-on-rock drags, pitched down), sand pouring, and the Memnon note (a bowed and plucked low lyre-string snap through a stone-resonance convolution). The eye beam's tell is that note rising. At death the eye cracks with a glass shatter and the song stops mid-note.

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT: tell, strike, hit, pain)

| Creature | Fused from (real recordings) | Signature |
|---|---|---|
| Duskjackal | Golden jackal yip-howl chorus + hyena whoop + a dry bone rattle | The chorus at dusk, heard 500 m off. It means "pack hunting" |
| Glasstail | Scorpion stridulation (real) + crab clicks + a struck glass bell | The glass ring before the glare and the sting |
| Carrion kite | Bearded vulture whistle + raven croak + a pitched-down hawk scream | The circle scream |
| Dunejaw | Monitor lizard hiss + a pitched-down sea lion bark + sand pouring | The sand hiss rising before the burst |
| Gildback | Gecko chirp + tortoise grunt + a wind chime of spines | The hum when the spines flare |
| Dromad | Camel roar and gurgle (real *dulla* gurgle) + a cow moan | The gurgle before spit and charge |
| Shimmerbuck | Oryx snort + gazelle alarm whistle + a heat-shimmer swish | A whistle as it leaves a mirage |
| Scarab digger | Beetle stridulation + rolling sand | The antenna chirp when it finds something |
| Sandhen | Hen + desert lark (KEPT) | — |
| Wellfrog | Water-holding frog call (a low "mawp") + water sloshing | Its croak from underground after rain |
| Ssarrok | Elephant seal roar pitched down + whale moan + an avalanche rumble | The boom before the breach |
| Mirage Matriarch | Oryx bellow + a reversed whistle chorus (one voice per mirage, slightly detuned) | Four voices out of time; the real one is the one on the beat |
| Glass Empress | Scorpion stridulation pitched down + cracking ice and glass + a low cello | The beam's glass chord |
| Ahrumet | 2.14 | The Memnon note |

### 2.16 Music: the Desert (tempo, key and instruments SPEC 3.3: 84 BPM, D Hijaz, frame drums, oud-like plucks, ney-like flute over a drone; cues GAP-COOKED; rules KEPT)

**Rules (KEPT):** real composed music, always audible, ducks ≤ 4 dB, −18 LUFS (calm −23, boss −15), true peak −1 dBTP. Silences only where planned.

| Cue | Where and when | Writing |
|---|---|---|
| **Explore day: "Long Shadows"** | Daytime, D1 to D4 | A D drone (tanpura-style) under oud phrases in Hijaz (the augmented second, E♭ to F♯). Bendir (frame drum) in a slow 84 BPM *maqsum* feel. The ney enters on the second phrase. Wide and empty, with long rests between phrases (planned; the drone stays) |
| **Explore night: "Cold Stars"** | Night | The same theme on a solo ney and a qanun with a soft drone, no percussion except a finger-tapped daf every fourth bar. Slower phrasing (still 84 BPM) |
| **Combat layer** | Any fight | Adds darbuka and riq on top of the bendir, the oud in tremolo, low strings in fifths. Stems switch on the bar |
| **The Wall** | Inside D5 | The explore theme low-passed at 800 Hz, with storm wind. Still audible (never muted) |
| **Mini-boss** | All three | Driving 7/8 on darbuka and tabl; an oud riff in Hijaz; a male chant on open vowels |
| **Boss: "The Song of Ahrumet"** | 3 sections | **Phase 1:** low male choir and a deep drone on D, slow frame drums, the Memnon note as a recurring motif. **Phase 2:** tempo stays 84 but the drums double, brass and a big daf ensemble come in, and the choir climbs. **Phase 3:** everything up, with the choir holding a single blazing high note through the Noon Pulse |
| **Long Noon layer** | The event | A high shimmering string cluster and a slow ney melody over the explore theme, with no rest phrases: the sun doesn't move, and neither does the drone |
| **The Hourhold** | Underground | Drone and distant oud with long stone reverb, water drips as percussion |
| **Camp** | Spawn camps and the Great Shrine | Solo oud, warm, in D major's neighbour (the Hijaz home chord resolving), with Amastan humming at the Great Shrine at night |

### 2.17 The Sun & sand path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Radiance (DD SPEC: "charges in daylight, weak at night"; a *spender*, KEPT combat).**
- **Bar:** 100, +10 per rank of Radiance capacity (130 at 3 ranks).
- **Day fill:** +2.5 a second while exposed to the sun (world-map "exposed": no roof within 30 m straight up), ×0.5 in shade, plus +3 per landed melee hit. Weather (KEPT): overcast ×0.6, heat wave ×1.2, sandstorm belt 0, the Dimming 0, the Long Noon ×1.5 and no drain.
- **Night:** no passive fill, and the bar **drains 1.5 a second** down to 0. Dawn cuts the drain to 0.45 a second.
- **Night fill sources:** a lit sunstone lamp within 6 m (0.5 a second), a tamed Gildback within 6 m (0.5 a second), Dunewrap's 4-piece (half the day rate, KEPT), and kills (+5 each).
- **Underground:** counts as night unless lit by a sunstone lamp.
- **The bar** is Sun gold #E8B230. At night its edge dims with the drain.

**Root passive: Sandwalker (SPEC text: no movement penalty in sand or sandstorms; numbers GAP-COOKED).** Main path only. No soft-sand slow and no sprint stamina increase. No sandstorm slow and vision 60 m in storms (as the sand veil). You don't sink in quicksand. The belt still stops Radiance filling (KEPT).

**The tree.** Gates are points spent in the branch before the node, as `fire.json` does.

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Sunbeam | 3 pts | 0 | Basic light spell | Bolt 16 dmg, blind 30, 3,400 cm/s, cd 1.0, 8 Radiance; homes only on creatures (KEPT fire-frost RAISE 3) | KEPT combat |
| Trunk | Radiance capacity | 3 ranks | 3 | Larger Radiance bar | +10 max Radiance per rank | GAP-COOKED |
| Trunk | Blind chance | 2 ranks | 6 | Higher chance to blind enemies | +10% blind buildup on all your hits, and +5 blind on every weapon hit, per rank | GAP-COOKED |
| Trunk | Sun strike | 2 pts | 8 | Basic light weapon art | Wildbrand: 15 Radiance, cd 3.0, W/A/R 0.30/0.20/0.44, ×1.35, blind 40, stance ×2, HA | KEPT combat |
| Radiant | Solar flare | 5 pts | 0 | Spell: a flash that blinds everything nearby | Nova radius 600, 10 dmg, blind 80, cd 14, 30 Radiance | KEPT combat |
| Radiant | Brilliance | 5 ranks | 5 | More light damage | +6% damage on all Sun spells and Wildbrands per rank | GAP-COOKED |
| Radiant | Searing light | 3 ranks | 10 | Blinded enemies take more damage | Blinded targets take +6% damage from you per rank (+18% at 3) | GAP-COOKED |
| Radiant | Beacon | 4 pts | 13 | Spell: a light zone that hurts undead and void creatures | Pool radius 500, 10 dps to undead and void, 10 s, cd 20 (KEPT); 25 Radiance; it also lights the zone (a 10 m light) and counts as a sunstone lamp for your Radiance at night | KEPT combat (cost and light GAP-COOKED) |
| Radiant | Dawn | 3 pts | 17 | Radiance drains less at night | Night drain ×0.3 (1.5 → 0.45 a second) | GAP-COOKED |
| Radiant | Sunburst | 10 pts | 20 | Capstone: call down a column of sunlight | Strike radius 600, 70 dmg, blind 100, delay 1.0, cd 50 (KEPT); 70 Radiance; a gold column of light that the whole party sees 1.0 s ahead (your own ability) | KEPT combat (cost GAP-COOKED) |
| Dune | Sandstorm | 5 pts | 0 | Spell: a storm that blinds and slows | Storm radius 700, blind 10 a second, slow ×0.8, 8 s, cd 20 (KEPT); 30 Radiance. It's centred on you and moves with you | KEPT combat |
| Dune | Shifting sands | 3 ranks | 5 | Bigger storms | +15% radius per rank on Sandstorm and Desert's wrath | GAP-COOKED |
| Dune | Burrow | 5 pts | 8 | Dive under sand to dodge and move | Dash under sand only, 800 cm, cd 4 (KEPT); 10 Radiance; i-frames the whole way; works on any sand ground (desert, beaches, sand in other biomes); greys out with "needs sand" elsewhere | KEPT combat |
| Dune | Quicksand | 4 pts | 13 | Spell: a trap that sinks enemies | Pool radius 300, roots lowland and midland 2 s, slow ×0.5, cd 16 (KEPT); 20 Radiance; works on any ground (it turns it to sand for 6 s) | KEPT combat |
| Dune | Scour | 3 ranks | 17 | Storms deal damage over time | Your storms deal 4 dps per rank to enemies inside | GAP-COOKED |
| Dune | Desert's wrath | 10 pts | 20 | Capstone: a giant sand vortex | Storm radius 1,000, pulls 200 cm/s, 14 dps, 10 s, cd 60 (KEPT); 80 Radiance. Bosses aren't pulled | KEPT combat |
| Solar blade | Blazing arc | 5 pts | 0 | Wide sun-infused weapon art | Wildbrand: sword, greatsword, scythe, spear; 25 Radiance, cd 6, W/A/R 0.42/0.26/0.56, ×1.6, blind 50, stance ×2.5, HA; charge ×2.0 and range 500 cm; a 220° arc, 380 cm | KEPT combat |
| Solar blade | Sun-forged | 5 ranks | 5 | More weapon art damage | +6% Sun Wildbrand damage per rank | GAP-COOKED |
| Solar blade | Gleam | 3 pts | 10 | Parries blind the attacker | A parry applies blind 100 (bosses 60) | GAP-COOKED (behaviour SPEC; hook KEPT combat) |
| Solar blade | Zenith | 4 pts | 13 | Bonus damage at full Radiance | While Radiance is ≥ 90%, all your Sun damage is +15% and your weapon glows gold | GAP-COOKED |
| Solar blade | Heat shimmer | 3 ranks | 17 | Longer dodges | +12% roll distance and +0.03 s i-frames per rank | GAP-COOKED |
| Solar blade | Noon strike | 10 pts | 20 | Capstone: an overhead blow that calls down sunlight | Wildbrand: all melee; 60 Radiance, cd 30, W/A/R 0.80/0.30/0.80, ×2.2 + a column of 50, blind 80, stance ×6, HA; the column falls on the target point 0.9 s later | KEPT combat |

**Points check:** trunk 3+3+2+2 = 10; each branch 5+5+3+4+3+10 = 30 (SPEC 100).

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Any blind proc | A cold-white flash in the target's eyes; the target paws at its face | A high glass ring |
| Searing light | Gold sparks on hits against blinded targets | A sizzle |
| Gleam | The parry spark becomes a white starburst in the attacker's eyes | A bright "ting" on the clang |
| Zenith | Your weapon edge glows gold while you're ≥ 90% | A soft hum while held |
| Scour | Sand-grit hits sparkle on enemies inside your storm | Grit hiss |
| Burrow | A fin of sand cuts the ground where you travel, then a sand burst where you come up | Sand rush and a pop |
| Radiance at night | The bar's edge dims, and sunstone motes drift to you from a lamp or a Gildback | A faint chime |

**Combos with Sun (CB SPEC, names and effects as written; built in R5 for the paths that exist then: Fire, Frost, Lightning, Blood, Nature, Necromancy):**

| Pair | 25/25 (CB) | 50/50 (CB) | Numbers (GAP-COOKED) |
|---|---|---|---|
| Fire + Sun | Desert heat: Heat builds faster in sunlight | Supernova: full Heat and full Radiance release a blinding explosion | Heat gain ×1.25 while exposed by day. Supernova: radius 700, 120 dmg, blind 100, burn 80; spends both bars; cd 90 |
| Lightning + Sun | Thunderflash: shocks also blind | Solar storm: lightning strikes inside your sandstorms | Each shock proc adds blind 40. Inside your Sandstorm: a strike every 1.0 s on a random enemy (25 dmg, shock 30) |
| Sun + Nature | Desert bloom: your plants survive in sand and storms | Sun garden: healing zones also blind enemies | Plant summons ignore sand and storms. Healing zones apply blind 10 a second to enemies inside |
| Sun + Frost | Frozen sand: quicksand freezes enemies stuck in it | Sun and snow: shattering a blinded enemy causes a blinding flash | Quicksand-rooted targets get frost 30 a second. Shatter on a blinded target: a 500 radius flash, blind 80 |
| Sun + Blood | Dry blood: bleeds deal more damage in sunlight | Sacrifice to the sun: spend HP to fill Radiance instantly | Bleed burst +20% by day exposed. Sacrifice: a quick-slot action, spend 20 HP (into Spilled) for +40 Radiance, cd 20 |
| Sun + Necromancy | Desert graves: raise minions from the sand anywhere | Scarab swarm: Corpse explosion releases biting scarabs | Raise dead on sand needs no corpse (a sand-shell raised copy of the last thing killed nearby, 50% stats). Corpse explosion releases 6 scarabs (8 dmg bites, 4 s) |
| Sun + Tide, Wind, Arcane, Spore, Void | CB names as written | | Built in those paths' rounds |

### 2.18 Body change: Sun & sand (hero.md base KEPT: albedo −15% sun-darkened, gold flecks on the G mask, gold iris with specular ×1.4, sun-bleached hair tips; detail GAP-COOKED in fire-frost's frame)

**Frame (KEPT):** strength is points ÷ 100; main path full, subclass ×0.6; changes ramp smoothly; built on the MetaHuman hero (R0). **The look:** golden, warm, sunlit skin. It's a glow of health and heat, attractive and striking, never burnt or leathery. Colour Sun gold #E8B230.

| At | What shows |
|---|---|
| 10 | Skin warms a shade (the albedo shift starts) with a soft sheen on the cheekbones and shoulders, as if just out of the sun |
| 30 | Fine gold flecks on the cheekbones, collarbones and shoulders, like gold dust on the skin (metallic mask), catching every light |
| 50 | Hair tips sun-bleached a shade lighter; the flecks reach the upper arms and the upper back; a gold ring at the iris edge |
| 75 | Full sun-darkened skin (−15% albedo); the flecks trace the line of the spine and the hip bones; lashes tipped gold |
| 100 | The full gold iris that catches light (specular ×1.4); in direct sun the flecks glint as you move; at full Radiance a faint warm glow rises off the skin (heat shimmer at the hands, owner and 15 m only) |

- **At night:** the flecks hold a faint stored glow, brighter the more Radiance you have.
- **Sun with another path:** with Fire, ember veins run under the gold-flecked skin; with Frost, frost-fern over gold reads as rime on brass; with Blood, the garnet tracery stays garnet, never crimson (the colour law).

### 2.19 Gear and items from the desert

**Dunewrap set (KEPT stats, survival-crafting):** light, poise 30. Inputs per full set: steel plate 6, sunstone 8, linen 12, Dromad leather 6. 2 pieces: blind buildup ×0.5, and heat waves don't raise thirst. 4 pieces: sandstorm slow halved and vision 60 m (sand veil), and Radiance fills at night at half rate.

**Dunewrap look (merge proposal; the two KEPT looks differ, logged to LATER-COOKS):**
- hero.md has "a wide hooded wrap with a face veil (lowered), sunstone disc on the chest; bone, ochre, amber glow; arms to the shoulder, face, F: midriff; veil tails, sash".
- survival-crafting has "layered linen wraps and a cropped vest, bare sun-bronzed shoulders and midriff, sunstone-studded belt, a veil that drapes from a gold circlet".
- **Merged:**
  - **Head:** a gold circlet holding a hooded veil, worn lowered to frame the face.
  - **Chest:** layered linen wraps crossed over a fitted, cropped vest with a sunstone disc at the sternum. The shoulders and arms are bare; on the female body the midriff is too.
  - **Hands:** wrapped forearms to mid-forearm.
  - **Legs:** loose wrapped trousers over sandals with sunstone-studded straps.
  - **Waist:** a sunstone-studded belt with sash tails (cloth sim).
  - **Colour:** palette bone, sand and ochre with sun-gold trim; the sunstones glow amber at night.

**Hazard gear (KEPT):** sand veil (Head, Armorer 2, linen 4, sunstone 1).

**Desert items (GAP-COOKED):**

| Item | Station, inputs | Effect |
|---|---|---|
| **Sunwatch lens** | Workbench 2: sunstone 2, bronze 1, linen 1 (recipe from S10 and S32) | A held tool. It shows the sun's (or moon's) bearing through cloud and storm, restores the compass ribbon and your waypoint marker in the sandstorm belt while held, and adds a 0.5 s look-down action that shows the hour |
| **Water jar** | Hand: clay 3, after any campfire | Holds 3 drinks. Never spills on dodge |
| **Oasis water jar** | Workbench 1: clay 3, filled at any water | Thrown (ballistic, 1,200 cm/s, never homes). 250 cm splash: wets sand for 30 s (6 m) and makes a water hit (Ahrumet's crack counter, Ssarrok's beach) |
| **Dune oil** | Alchemy 2: Gildback spine 1, date 2, palm fibre 1 (quest 6 recipe) | 10 min: quicksand frees on the first dodge, sandstorm slow ×0.85 (KEPT effect) |
| **Clear-eye drops** | Alchemy 1: sunstone 1, prickly pear 1, water | Cures blind (KEPT cure name) |
| **Flash pot** | Alchemy 2: sunstone 1, nitre 1, clay 1 | Thrown, 0.6 s fuse, blind 80 in a 400 radius (you too, if you're facing it) |
| **Sunglass arrowheads** | Workbench 2: sunglass 1 → 5 (quest 5) | Arrows: blind 25, +10% pierce damage |
| **Red ochre beer** | Cooking: ashen grain or dates 2, red ochre 1, water | Portal tool (Sekhra, 2.22.4). Thrown: an animal-type portal boss drinks it. Drunk by you: stamina regen +10% and a 10% stagger chance on yourself for 60 s |
| **Tooth of Ssarrok** | Talisman: Armorer 3, Leviathan tooth, Dunejaw hide 2 | Burrow, dodge or roll on sand: the next melee hit within 2 s deals +25% |
| **Mirage charm** | Talisman: Alchemy 3, Mirage antler, sunstone 3 | A roll leaves a 2 s mirage of you (no shadow) that draws creature attacks (not bosses; PvP: it shows to players but deals nothing). Cooldown 8 s. **This is the path theme's "mirage decoys" (DD SPEC), as gear** |
| **Empress's lens** | Ring: Armorer 3, Empress stinger, sunglass 4, gold flakes 2 | Critical hits (ripostes, backstabs, stance-break hits) add blind 30 |
| **Sun mirror array, sunstone lamp, Sun court set, crops, fish, tagine** | KEPT survival-crafting | KEPT |

### 2.20 The story the desert tells, with no words (GAP-COOKED; fits world-map's Twelve Peoples and pass wars)

**The Sunwatch** measured the sun. Everything they built tells time: the stelae, the Measuring Wall, the Hall of Hours, the sundial plaza. Their colossi were their champions, and Ahrumet is the last one standing.

1. **This was a sea.**
   - The Whale Road, the ancient hulls with oar ports in the Erg, the shells in the dune troughs, the Cartographer's Table with its old coastline.
   - At Saltgate, the stranded fleet of half-buried ships points west.
   - The sea left. Nobody says why.
2. **The sun people took the sea people's land.**
   - The Ship in the Dune has a sun disc nailed over its sea-god figurehead.
   - The Keelyard is the sea people's hulls turned into the sun people's fence.
   - **The pass war at Saltgate (world-map rule: champions lie on the wrong side, killed by the opposite):** the Saltkin's champion, a 90 m sea-serpent, lies on the Desert side, bleached to chalk by the sun, with Sunwatch bronze spears in its spine. The Sunwatch's champion, a colossus twin of Ahrumet, lies on the Ocean side, eaten through by coral and barnacles. *This swaps world-map's two champion descriptions so each lies on the wrong side; logged to LATER-COOKS.*
3. **Then the water ran out.**
   - Gardener's Last Row: one fallen stone killed the garden below it.
   - The Measuring Wall stops two-thirds of the way with the chisel in the groove.
   - The qanat runs dry except one shaft.
   - The Hourhold's spring still flows at the bottom of the buried city: the river's source, sealed under the sand.
4. **They turned on their god.**
   - The Pilgrims' Ring has every head broken off and laid facing away from Ahrumet.
   - The Fallen Hour, Ahrumet's twin, was pulled down on purpose: ropes are still round its neck, and the sand under it holds tow-sledges.
   - Ahrumet still sings at dawn to nobody.
5. **The storm.**
   - The Last Caravan waited out a storm that never ended.
   - The Faceless Sentinels have been sand-blasted smooth.
   - The Haboob Wall has stood so long that the towers inside are buried to their tops.
6. **The newcomers.**
   - The Tall Wreck's empty casks, its 6 graves facing the sea, and the shell shrine with its driftwood sun.
   - The Lost Expedition dug into the Hourhold and didn't come back up.
7. **Amastan's people.**
   - The blue hands on the Hands Wall, at child height, are newer than the ochre ones.
   - Someone has been crossing this desert since the Sunwatch fell, and they know the way through the Wall.
   - Amastan never says so. His veil is the same blue.

**Lore items (8)** sit in the Hourhold's scribes' rooms (4 tablets), the Lost Expedition (a newcomer journal), the Sun-Vault (the Glass Scarab), the Lens Forge (an eye mold) and the Seventh Vault (a sun-disc of the Sunwatch's last king). Each is a short text, 40 words at most, with no exposition of the whole.

### 2.21 Invasions: Sun ↔ Tide through Saltgate (3.3 rule KEPT; contents GAP-COOKED; live from R6, when the Ocean opens)

- **Desert invades Ocean:** Duskjackal packs (at night) or Dromad stampedes driven by Glasstails (by day), plus Dunejaws. Ssarrok leads it, swimming the beach sand. Desert invaders dry the ground they cross: wet sand turns to dry sand for 2 game hours, and tide pools steam away.
- **Ocean invades Desert:** the Ocean lane's creatures and its roaming mini. Each Ocean invader leaves a 6 m patch of wet sand for 4 s, which stops Dunejaws diving there.
- **Payout:** Sun and Tide essence, plus the 3.3 path XP rule.

### 2.22 Dungeon portals (DD SPEC rules restated; framework and Desert rooms GAP-COOKED; first built in R5)

#### 2.22.1 Raven's rules (SPEC, DD "Dungeon portals" and "How portals work")

- Small portals spawn at random in each biome and lead to short dungeon crawls.
- Each portal is themed by its biome, and its rank shows as a colour.
- There are five ranks: Common, Uncommon, Rare, Epic, Legendary. Higher ranks spawn deeper, take longer to run and take longer to break.
- **Break timers:** 1, 3, 6, 10, 18 real days.
- Handmade rooms are shuffled into new layouts, with random modifiers (no healing, double damage).
- Puzzles and traps, not just combat. A boss at the end of every portal.
- **Breaking:** an ignored portal escalates and spills monsters first. When its boss escapes, the portal closes. An escaped boss blocks that rank in that biome until it's killed. Entering a breaking portal is allowed until the boss escapes, and clearing it stops the break.
- Key portals belong to the opener; wild portals are first come. Craftable keys spawn a chosen rank.
- Legendary portals are announced to the whole server.
- Death inside follows server death rules. If the portal closes, bodies appear where the entrance was.
- No dungeon currency, no forced party size. Loot and materials only.
- Ruled out: procedural layouts, portal timers, party limits, PvP portals, dungeon currency.
- Dream tier: the endless tower with a leaderboard (not in R5).

**Two open conflicts, resolved as FULL-GAME proposes:**
- **DR #20:** "portal timers" in the ruled-out list means *timed runs*; the real-day break timers stay.
- **DR #21:** traps are out for bases and in for dungeons.
- **Procedural layouts vs shuffled rooms:** both the layout graphs and the rooms are handmade. The shuffle picks a handmade graph and fills its slots with handmade rooms. No room, corridor or graph is generated.

#### 2.22.2 Ranks (GAP-COOKED numbers)

| Rank | Colour (never gold or crimson: the colour law) | Where it spawns | Rooms | Run length | Modifiers | Boss HP solo (+80% per extra player or tame, SPEC scaling) | Breaks after (SPEC) |
|---|---|---|---|---|---|---|---|
| 1 Common | Bone white #E8E2D0 | Lowland | 3 + boss | ~10 min | 0 | 1,500 | 1 day |
| 2 Uncommon | Green #4FB04A | Lowland or midland | 5 + boss | ~15 min | 1 | 2,500 | 3 days |
| 3 Rare | Blue #3A7BDB | Midland | 7 + boss | ~25 min | 1 to 2 | 4,000 | 6 days |
| 4 Epic | Violet #8E4FD8 | Highland | 9 + boss | ~35 min | 2 | 7,000 | 10 days |
| 5 Legendary | Flame orange #FF7A1A | Highland or the hidden sub-biome | 12 + boss | ~50 min | 3 | 12,000 (major-tier) | 18 days |

**Spawning (GAP-COOKED).**
- Each biome has wild portal slots: 1 per 1.5 km² of surface, rounded up (Desert: 5).
- Mix: 2 Common, 1 Uncommon, 1 Rare, and 1 high slot that rolls Epic 95% / Legendary 5%.
- An empty slot refills 2 to 6 real hours later at a random valid point for its rank: open ground ≥ 150 m from claims, camps, shrines and arenas, never in a pass.
- A biome's rank is blocked while its escaped boss lives (SPEC).
- **Server setting** "Portal break timers" (SS SPEC, full-release settings): ×0.5 / ×1 / ×2 / Never break.

**The look.**
- **The portal:** a 3 m tall vertical tear in the air, edged in the biome's material. In the Desert it's a ring of fused sunglass with sand pouring *up* through it.
- **Visible from afar:** a light pillar in the rank colour, 60 m tall (rank 5: 300 m), visible 500 m away (rank 5: across the biome).
- **Sound:** a hum pitched by rank, low for Common and deep and wide for Legendary.
- **Legendary portals:** announced in global chat with the biome name ("A Legendary portal has opened in the Desert"), and the biome's Crown pillar flickers in flame orange (SPEC "announced to the whole server").

**Breaking (GAP-COOKED escalation inside SPEC timers):**

| Share of timer gone | What happens at the portal |
|---|---|
| 0 to 50% | Stable. The light pillar is steady |
| 50% | **Cracking.** The pillar flickers and the edge cracks. Every 30 game min, one pack of the portal's creatures spills out within 150 m and roams the area |
| 80% | **Spilling.** Packs every 15 game min, plus one **warden** (an elite of the portal's creatures at mini-boss stats, 3,000 HP) that guards the portal mouth |
| 100% | **Break.** The portal boss walks out and the portal closes. The **escaped boss** roams within 400 m of the old portal at full portal-boss HP, never rests, and fights anything near it. It blocks new portals of that rank in that biome until it's killed (SPEC). It never targets claims; the creature-raid system stays the only base attacker |

Clearing a cracking or spilling portal stops the break (SPEC). The spilled creatures stay until killed.

**Entering and running (GAP-COOKED, inside SPEC rules).**
- Interact at the portal: everyone within 10 m of the interacting player enters together (a prompt with a 5 s accept). Others can join later until the boss room door seals (when the first player enters the boss room).
- **One run-back point:** a portal stone at the entry room. Death follows server rules (SPEC): you respawn at your bed or camp outside, or, if your death bag is in the portal and the portal is still open, you can re-enter and run back from the entry stone.
- **The modifiers** show on the portal's prompt *before* you enter (no surprises).
- **On clear:** the boss drops loot; the portal stays open 2 min for looting, then closes; everyone inside is moved to the entrance.
- **Shards (GAP: DR #20 says this is unclear):** a cleared portal drops a **rank shard** in its colour to every player who dealt 10% (Raven's loot rule reused), used for keys.

**Modifiers (Raven's two plus eight; each adds +25% loot quantity):**

| Modifier | Effect |
|---|---|
| No healing (SPEC example) | Potions and heals are disabled; lifesteal still works |
| Double damage (SPEC example) | Everyone, both ways, takes ×2 |
| Darkness | Light radius halved; enemies don't care |
| Iron hide | Enemies have hyperarmor on every attack |
| Frenzy | Enemies move and attack ×1.15 faster |
| Venomous | Enemy status buildup ×1.5 |
| Brittle gear | Durability loss ×3 |
| No tames | Tames wait at the entrance |
| Bloodied | You enter at 50% HP |
| Swarming | +50% enemies per room |

**Keys (SPEC "craftable keys spawn a chosen rank"; recipes GAP-COOKED; Alchemy table).**
- A rank-n key costs 3 shards of rank n−1, 1 shard of rank n, and the biome's signature resource ×(2 × n). The rank 1 key costs 3 rank 1 shards and 2 signature resource.
- Using a key on open ground in its biome opens a portal of that rank on the spot. It belongs to the opener, their party and clan (SPEC), and it never breaks.
- The biome is set by the signature resource: a key made with sunstone opens a Desert portal.

**Rewards per rank (SPEC: loot and materials; numbers GAP-COOKED).**
- Rooms hold chests: rank 1 has 1 chest; rank 5 has 4 chests plus a vault.
- **Boss loot:** biome materials and ore at rank-appropriate tiers (rank 1 copper up to rank 5 steel and starsteel inputs), gems, upgrade shards (rank 3+), rank shards.
- **Blueprints (KEPT survival-crafting):** rank 1 10% up to rank 5 60%; legendary blueprints only from ranks 4 and 5.
- **Artifacts (SPEC: they drop from portals):** a chance from rank 3 up (3 / 8 / 15%), once the artifact system exists (logged).

#### 2.22.3 The room kit (GAP-COOKED; one per biome)

**Rooms are handmade modules on a 4 m grid,** 12 to 40 m across, with door sockets at standard points (4 m wide, 6 m tall). The **layout graphs** are handmade too: per biome, 4 graphs per rank, each a main line plus 1 or 2 side branches with a treasure room. A run picks a graph and fills each slot from the room pool for that slot's type.

| Room type | Desert pool | What it is |
|---|---|---|
| Entry | 2 | The portal stone (run-back) and a short reveal |
| Combat hall | 10 | Fights in handmade spaces with cover and height |
| Trap corridor | 6 | Traps, with a creature or two to push you into them |
| Puzzle room | 4 | A puzzle that opens the next door; never needs a specific path |
| Rest alcove | 2 | No enemies, a water spring, a chest. Ranks 3+ |
| Treasure | 3 | Side-branch rewards |
| Boss arena | 5 (one per rank) | 2.22.4 |

**Trap rules (DR #21 dungeons yes).** Every trap has ≥ 0.9 s of sound plus a visual tell, can kill, and can be disarmed or used against enemies. Enemies are hurt by traps too.

**Desert theme: the Sunwatch tombs (the portal is a fold into the Hourhold's lost vaults).**

| Trap or puzzle | Tell | Effect | Answer |
|---|---|---|---|
| **Dart walls** (trap) | Pressure plates are a slightly raised, darker stone. The click comes 0.9 s before the volley | Darts from wall holes cross the corridor: 15 each, blind 20 (sunglass tips) | Step round the plates, roll the volley, or trigger it with a thrown stone |
| **The sand room** (trap) | The doors grind shut and sand begins to pour from ceiling slots with a roar | The room fills over 40 s; at full you're buried and die | Climb the pillars' handholds to the vent at the top, or break the 2 sand-slot stoppers (300 HP each) |
| **Rolling sun-stone** (trap) | A deep rumble 2 s ahead and dust falling at the far end | A 4 m stone ball rolls the corridor: 120 damage | Side niches every 12 m |
| **Scarab floor** (trap) | The tiles shift and click | A swarm of flesh scarabs (swarm archetype, 25 HP each, ×8) boils up | Burn them (fire), or run to the far door before 6 s |
| **Collapsing floor** (trap) | Cracks and trickling sand 1.0 s before | A floor section drops into quicksand 3 m down | Sprint or jump across; Sandwalker doesn't sink |
| **Mirror relay** (puzzle) | A sun-disc lock on the door; a shaft of light from the roof | Rotate 3 to 5 bronze mirrors (Interact turns 45°) to carry the beam to the lock | Any player |
| **The hour lock** (puzzle) | A gnomon on a dial with an hour carved over the door | Turn a roof lens so the gnomon's shadow falls on that hour | Any player |
| **The weighing** (puzzle; the Weighing of the Heart) | A bronze scale with a feather on one pan, and objects on shelves round the room | Place objects until the pans balance (each object shows its weight on inspect). Overweight opens a scarab floor | Any player |
| **The sand clock** (puzzle) | A great hourglass in the room centre, and three doors | The door that's open changes each time the glass is flipped (Interact); the right sequence of three flips is carved round the glass base | Any player |

**Desert portal creatures:** the biome's own (Glasstails, Duskjackals, Dunejaws in sand-floored halls), plus three portal-only ones, all real-myth roots:
- **Ushabti** (servant construct): a 2 m blue-faience and clay figure with a hoe and a flail; it wakes when you pass. Midland stats. Attacks: hoe chop (0.6 / 0.2 / 0.8), flail sweep (0.7, 2 hits), and a sand-shell crumble on death that blinds 30 in 2 m.
- **Flesh scarabs:** a swarm (KEPT archetype).
- **Tomb jackal:** a gilded Duskjackal elite, highland stats.

#### 2.22.4 Desert portal bosses (one per rank; 4 to 5 attacks and one mechanic, the mini template KEPT)

| Rank | Boss | Look (silhouette, materials) | Attacks (W / A / P, s) | Mechanic |
|---|---|---|---|---|
| 1 | **The Ushabti Overseer** | A 4 m faience-blue overseer with a gold-leaf face, a crook and a flail, and an army of tiny Ushabti marching round its feet | Flail combo ×3 (0.65 / 0.2 / 0.8); crook pull (0.8, pulls 400 cm); stomp (0.7, `low` ring); call workers (1.0: 4 Ushabti) | Every 20 s it raises the crook and its workers repair it (+3% HP each per second within 4 m). Kill the workers first |
| 2 | **Sekhra, the Lioness of the Eye** | A 5 m lioness with a mane of flame-gold light, a sun-disc between her ears, and blood on her muzzle | Claw combo ×3 (0.55 / 0.2 / 0.8); pounce (0.9, 12 m); roar cone (0.8, blind 50); sun-disc beam (1.1 W, 15 m line, locks at windup end) | **The red beer (myth):** throw red ochre beer within 6 m of her. She drinks (2 s), staggers, and sleeps 6 s with stance damage ×2. One jar per 30 s works. Without it she's still beatable |
| 3 | **The Scarab Mother** | A 6 m gold-green scarab rolling a 4 m ball of burning sun-glass (Khepri) | Ball roll (0.9 tell, rolls a lap of the arena); horn toss (0.7); stridulate (1.0, a sound cone that staggers 0.6 s); brood (scarab swarm ×8) | When her ball is away she's exposed: her ball hits the walls and cracks; three cracks break it and she flounders for 8 s |
| 4 | **The Weigher** | A 7 m jackal-headed giant (Anubis) in black-and-gold, holding a bronze balance | Staff combo ×4 (0.6 → 0.5 → hold 0.4 → 0.9 / 0.2 each / 1.0); scale slam (1.1, a 6 m crater); judging grab (1.0, crimson); the Devourer's call (Ammit: a crocodile-lion add) | **The scales:** two scale pans on the arena floor (6 m discs). Every 30 s he weighs: players on the heavier side (more players standing there) take a 60-damage judgement. Balance the pans to deny it; drop a heavy item to tip it |
| 5 | **Apepa, the Serpent Below the Sun** | A 40 m serpent of black scales whose coils come up through a sand-sea arena, with a stone sun barque stranded in the middle (Apep attacks the sun's boat every night) | Coil sweep (1.0, `low`); bite and drag (0.9, grab); venom rain (1.2 W, ballistic globs with 0.9 s ground glow); burrow and erupt (1.0 with the sand dimple tell); eclipse (1.4 W: darkness, light radius halved for 10 s) | **Defend the barque:** the barque has 5 sun-lanterns. Apepa's eclipse snuffs one each time. Re-light them (Interact 2 s under fire). While ≥ 3 are lit, she takes ×1.25 and can be stance-broken; at 0 lit, she heals 1% a second |

**Portal themes for the other built biomes (R5 delivers rooms for these too; the same framework).** Each biome gets the same pool sizes. These are first-pass themes for their own lanes to fill:

| Biome | Room kit | Signature trap | Signature puzzle | Rank 5 boss seed |
|---|---|---|---|---|
| Volcanic | Kiln Roads forge halls (fire-frost) | Lava floor pulses | Quench the blade (water to the right mould) | A forge-giant apprentice of Vulkarn |
| Frozen tundra | Blue Below ice halls | Falling icicles | Light the braziers to melt a path | A rime-witch of the Rime-folk |
| Storm peaks | Hollow Spire copper halls | Charged floor plates | Route the current through rods | A storm-ram spirit |
| Swamp | Sunken Coven rooms | Rising bleedwater | Feed the right tithe basins | A coven elder |
| Ancient forest | Hollow Root chambers | Snapping briar walls | Grow the bridge (water the right roots) | A rot-heart treant |
| Graveyard | Catacomb vaults | Swinging censers that put out light | Lay the bones in order | A lantern-bearer giant |

These seeds are for the biome lanes or a portals content cook to finish (logged).

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing from data, so `L_Island` is touched as little as possible.

**R5 also needs these shared pieces first:**
- the **blind** status (FULL-GAME 11 lists it for R5);
- the attack-row runner (combat C9) and visible-contact boss damage (combat C10);
- the world-map terrain for the Desert half of B2 and Saltgate;
- shrines, path masters and events from the fire-frost frame;
- the R2 grab framework (LATER-COOKS: swallow and gulp) for Ssarrok;
- blueprints (survival-crafting, R5).

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/sun.json`** | Same schema as `fire.json`: `id: "sun"`, `name: "Sun & sand"`, `biome: "desert"`, `starter: "sunbeam"`, `resource {id: "radiance", name: "Radiance"}`, `rootPassive {id: "sandwalker", name: "Sandwalker", text: SPEC}`. Trunk `sunbeam, radiance_capacity, blind_chance, sun_strike`. Branches `radiant` (`solar_flare 0, brilliance 5, searing_light 10, beacon 13, dawn 17, sunburst 20`), `dune` (`sandstorm 0, shifting_sands 5, burrow 8, quicksand 13, scour 17, deserts_wrath 20`), `solar_blade` (`blazing_arc 0, sun_forged 5, gleam 10, zenith 13, heat_shimmer 17, noon_strike 20`). `kind` and costs from 2.17; node text copied from ST word for word |
| `Data/tuning.json` `resources` | `radiance {max: 100, capacityPerRank: 10, dayFill: 2.5, shadeMult: 0.5, perHit: 3, nightDrain: 1.5, dawnMult: 0.3, lampFill: 0.5, gildbackFill: 0.5, killGain: 5, overcast: 0.6, heatWave: 1.2, longNoon: 1.5, exposedRoofCm: 3000}` |
| `Data/tuning.json` `nodeEffects` | `sun.radiance_capacity {maxRadiance: 10}`, `sun.blind_chance {blindBuildup: 0.10, weaponBlind: 5}`, `sun.brilliance {sunDamage: 0.06}`, `sun.searing_light {vsBlinded: 0.06}`, `sun.shifting_sands {stormRadius: 0.15}`, `sun.scour {stormDps: 4}`, `sun.sun_forged {sunBrandDamage: 0.06}`, `sun.heat_shimmer {rollDistance: 0.12, rollIFrames: 0.03}` |
| `Data/tuning.json` `passives` | `sandwalker {noSandSlow: true, noStormSlow: true, stormVision: 6000, noSink: true}`, `gleamBlind: 100, gleamBossBlind: 60`, `zenithAt: 0.9, zenithBonus: 0.15`, `dawnDrain: 0.3` |
| `Data/tuning.json` `spells` | Sun entries as combat 2.13 (KEPT), plus costs `beacon 25, sandstorm 30, burrow 10, quicksand 20, desertsWrath 80, sunburst 70`, `beacon.light 1000`, `quicksand.anyGround: true, sandSeconds: 6`, `burrow.needsSand: true` |
| `Data/tuning.json` `status` | The blind row from combat 2.11 (KEPT): AI memory loss 3 s (bosses 1.5), last-seen-spot attacks, backstab on blinded, the player vignette 40% and lock-off 3 s; colour cold white |
| `Data/tuning.json` `hazards` | `sandBelt {vision: 2500, run: 0.7, radiance: 0, veilVision: 6000, veilSlow: 0.85}`, `softSand {run: 0.85, sprintStamina: 1.3}`, `quicksand {sinkSeconds: 4, dodgeLift: 0.25, drownAfter: 5, pullHold: 2, tellSeconds: 0.9}`, `brineCrust {creak: 1.0, drop: 150, run: 0.4, dps: 4, climb: 2}`, `slipface {angle: 30, boom: 1.2, carry: 1200, bury: 2, stamina: 40, damage: 20}`, `dustDevil {hours: [11, 16], count: [2, 4], speed: 400, lift: 1.2, throw: 600, damage: 20, blind: 30}`, `flashFlood {roar: 6, trickle: 3, damage: 50, carry: 1500, debrisHits: [3, 5], debrisDamage: 10}` |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `fin_stalk, sand_burst, sand_spray, re_dive, glare, pincer_grab, bone_drop, circle_call, chorus, hamstring, spine_flare, spine_roll, false_head, mirage_escape, ball_roll, dulla_spit`. Flags: `sandDiver` (Dunejaw, Ssarrok: dive only on `soft_sand`), `daylightOnly` (glare, Prism beam) |
| `Data/tuning.json` `bosses` | `ahrumet {stance: 180, parries: 3, wakeOnMeridian: true, beamRange: 4000, beamSweep: 90, beamRate: 60, beamTick: 0.25, stelaBeamHits: 3, sunHotSeconds: 6, crackStance: 60, crackMult: 1.5, frostCrackScale: 0.5, kneelSeconds: 4, pulseDelay: 2.0, pulsePlantSeconds: 8, p3BeamRate: 30, p3PulseEvery: 20, pitRadius: 1000, longNoonPartMult: 2}`, `ssarrok {beachSeconds: 6, beachStance: 2.0, gulpSeconds: 4, gulpDps: 12, throatHealth: 400, spitDamage: 40, spitBlind: 80, wetSandSeconds: 30, route: [...]}`, `mirage_matriarch {mirages: 3, mirageDamage: 0.5, reform: 12, blindBreakSeconds: 15, overcastMirage: 0.5, diurnal: true}`, `glass_empress {plates: 6, plateHealth: 300, platedMult: 0.3, strikePartMult: 2.0, shockWindow: 4, shardDamage: 15, shardRadius: 400}`; per-attack rows from 2.13 and 2.14; run-back stone positions; lock points |
| `Data/tuning.json` `events` | `longNoon {warnSeconds: 300, holdHour: 12, radianceMult: 1.5, sunstoneYield: 3, bloomNodes: 20, tiltedStelae: 7, kiteMult: 2, digInMult: 0.5, gildbackTame: 2}` |
| **New `Data/portals.json`** | `ranks` (2.22.2: colour, band, rooms, modifiers count, boss HP, break days, pillar height, chest count, shard id); `slots {perKm2: 0.667, mix: [1,1,2,3,"4|5"], legendaryChance: 0.05, refillHours: [2, 6], minDistance: 15000}`; `escalation {crackAt: 0.5, spillAt: 0.8, crackPackMinutes: 30, spillPackMinutes: 15, warden: {...}, escapedRoam: 40000}`; `modifiers` (the 10 rows); `keys` (the recipe rule); `rewards` (chests, tiers, blueprint and artifact chances); `entry {partyRadius: 1000, acceptSeconds: 5, lootMinutes: 2}` |
| **New `Data/portal_rooms/desert.json`** | Rooms: id, type, level path, door sockets, bounds, spawn points, trap and puzzle actor lists. Layout graphs: 4 per rank, slot types, side branches. Bosses: 5 rows (`ushabti_overseer, sekhra, scarab_mother, the_weigher, apepa`) with attacks and mechanics. Creatures: `ushabti, flesh_scarab, tomb_jackal` |
| `Data/portal_rooms/<biome>.json` × 6 | Same schema, filled by each biome's portal content pass (2.22.4 seeds) |
| `Data/creatures.json` | 10 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `sun`, `resists ["sand_belt", "soft_sand", "quicksand"]` (Dunejaw, Dromad, Shimmerbuck also `heat`), taming, difficulty, food (Gildback: `sunstone`; Dromad: `prickly_pear`; Duskjackal: `gilt_meat`; Wellfrog: `beetle_grub`), attacks, activity (Duskjackal and Glasstail nocturnal), loot, mount and rideSpeed for Shimmerbuck and Dromad, utility (`dig`, `sense_hidden`, `navigate` for Scarab digger; `light`, `radiance_aura` for Gildback; `water` for Wellfrog; `track` for Duskjackal; `scout` for Carrion kite)); 3 portal creatures; 4 boss rows (`ahrumet` major, `ssarrok` with `roaming` and `route`, `mirage_matriarch`, `glass_empress`) and 5 portal bosses |
| `Data/spawn_regions.json` | Desert regions D1 to D5 with `area` polygons from 2.2 and `night` tables from 2.12.3 |
| `Data/items.json` | `sunstone, sunglass, rock_salt (KEPT), nitre (KEPT), acacia_log, palm_fibre, clay, red_ochre, gold_flakes, copper_ore (exists), dunejaw_hide, dunejaw_tooth, glasstail_chitin, venom_sac, kite_feather, gildback_spine, dromad_leather, oryx_horn, scarab_shell, beetle_grub, gilt_meat, radiant_essence, core_ahrumet, colossus_eye, trophy_leviathan_tooth, trophy_mirage_antler, trophy_empress_stinger, trophy_ahrumet_head, gnomon_maul_bp, sun_court_decor_bp`; tools and consumables `sunwatch_lens, water_jar, oasis_water_jar, dune_oil, clear_eye_drops, flash_pot, sunglass_arrow, red_ochre_beer`; talismans and rings `tooth_of_ssarrok, mirage_charm, empress_lens`; portal `shard_common … shard_legendary`, `portal_key_1 … 5`; 8 lore items |
| `Data/recipes.json` | 2.19 recipes; Dunewrap (KEPT inputs); sand veil (KEPT); portal keys (2.22.2 rule) |
| `Data/pieces.json` | `rope_plank` (freeform, allowed outside claims, decays in 4 game days) |
| `Data/world_pois.json` (fire-frost's file) | Desert sub-zone polygons, 11 landmarks + the Leviathan Ribs, 35 hand sites, 6 shrines, 4 caves, the Hourhold's rooms and three entrances with their open rules, kit rules (2.4), Ssarrok's route spline, the 7 tilted stelae and their cache points, Ahrumet's plaza layout (dial, gnomon, 12 stelae, throne, meridian strip), dig-spot scatter (1 per 200 m) |
| `Data/quests.json` | Amastan's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 10, 3 portal creatures, 4 bosses and 5 portal bosses; pools `explore.desert.day/night`, `combat.desert`, `sub.wall`, `mini.desert`, `boss.ahrumet` (3 sections), `sub.hourhold`, `event.long_noon`, `camp.desert`, `portal.<rank>` hums; ambient beds D1 to D5; `ahrumet.dawn_song` (a world sound at 06:00 heard within 2 km) |
| `Data/fx.json` | Radiance motes, Zenith edge glow, Burrow fin and burst, blind flash, Gleam starburst, Sunburst column; haboob wall (Niagara volume), sand sheets, dust devils, quicksand wobble skin, brine crust break, slipface flow, flash-flood wall; Dunejaw fin wake and burst; Glasstail glint and night fluorescence; Ahrumet eye charge, beam, sun-hot glow, crack, Noon Pulse whiteout and shadow-safe tint, glassfall glob; portal tear, rank pillars, cracking and spilling states |
| `Data/combos.json` (R1 creates it) | Fire+Sun, Lightning+Sun, Sun+Nature, Sun+Frost, Sun+Blood, Sun+Necromancy rows from CB with the 2.17 numbers |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `BlindBuildup`; `bWaterHit` (Oasis water jar, Tide, flagged frost) for crack checks; `bLight` for Searing light, Zenith and Brilliance |
| `WRStatusComponent.cpp/.h` | **Blind:** buildup meter, proc, threshold growth (KEPT). On creatures: clear the AI's target memory for 3 s (bosses 1.5 s), so the attack runner swings at the last-seen spot, and backstab is allowed from any side. On players: the vignette, and lock-on off for 3 s. Gleam hook on parry |
| `WRCombatLibrary.cpp` `ApplyHit` | Sun damage multipliers (Brilliance, Sun-forged, Zenith ≥ 90%, Searing light vs blinded, Dry blood by day); Blind chance buildup on weapon hits; Colossus eye crit blind; Empress's lens; **part cracks:** if the target part is flagged sun-hot and the hit is `bWaterHit`, add crack stance and set the part's damage multiplier |
| `WRAttributeSet` | Add `Radiance` and `MaxRadiance` (replicated), as Raven did Heat and Chill |
| `WRPathComponent.cpp` | Radiance tick (server, 0.25 s): day or night from `WRSky`, exposure (an upward trace of 30 m), weather multipliers, Long Noon, belt (no fill), lamp and Gildback proximity, Dawn. Spender check on Sun abilities. Sandwalker flags read by the CMC |
| `WRCharacterMovementComponent.cpp` | Ground-surface speed: soft sand ×0.85 (and sprint stamina ×1.3), belt ×0.7, quicksand sink state (a sink amount, dodge lift, held-and-drown at full), brine drop, slipface carry. All skipped by Sandwalker flags, veil and Dune oil. Heat shimmer roll distance and i-frames |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_Sunbeam` (UWRGA_Spell bolt), `UWRGA_SolarFlare` (AreaSpell nova), `UWRGA_Beacon` (AreaSpell pool that is also a light and a Radiance source), `UWRGA_Sunburst` (AreaSpell strike), `UWRGA_Sandstorm` and `UWRGA_DesertsWrath` (AreaSpell storm that follows the caster; wrath pulls), `UWRGA_Burrow` (dash: a sand-surface check, i-frames, hidden mesh with a fin FX), `UWRGA_Quicksand` (pool that roots by tier and turns ground to sand), `UWRGA_SunStrike`, `UWRGA_BlazingArc`, `UWRGA_NoonStrike` (UWRGA_Melee Wildbrands; Noon strike spawns a delayed column). `ClassForNode`: a new `sun` block mapping the 11 ability node ids. Passives (`sandwalker`, `gleam`, `zenith`, `dawn`, `scour`, `searing_light`) through `UWRPathComponent::HasNode` as Fire's are |
| `WRCreature.cpp` | Attack-row kinds from 3.1. **Sand diving:** a sand-swim state (capsule under the surface, fin actor on top, nav restricted to `soft_sand` surface areas); burst-from-below; surfaced when the ground isn't soft sand (stance ×1.3). **Vibration sense:** sprinting players on soft sand within 40 m count as a sound stimulus. **Glare:** a light cone check against the target camera's facing (60°). **Circle and call:** a flyer orbit state over targets under 50% HP, then an aggro broadcast at 120 m. **False head:** a damage multiplier zone. **Mirage escape:** spawn a mirage actor (no shadow casting: `bCastShadow` off) for 3 s. Tame utilities: `navigate` (follow toward the owner's waypoint), `sense_hidden` (query Hourhold doors and dig spots), `water` (trough fill tick), `radiance_aura` |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `ahrumet`, `ssarrok`, `mirage_matriarch`, `glass_empress`. **Ahrumet:** the wake trigger (a meridian volume), beam as a swept line trace that stops at blocking actors (gnomon, stela actors with 3 beam-hit health), sun-hot part flags with a 6 s timer and a material parameter, crack states per part, kneel on stance break with the eye socket lowered for riposte, the Uprooting transition (detach the gnomon actor, attach to the hand socket, spawn the funnel quicksand volume), Noon Pulse (shadow test per player: a trace from the eye-sun direction to the player, blocked = safe), held beam reverse, glassfall ballistic projectiles with ground glow, phase 3 continuous beam and noon sky override. **Ssarrok:** sand-swim, route component (fire-frost's), swallow with the R2 grab framework (gulp, throat lock point), beach state on non-sand ground. **Mirage Matriarch:** mirage actors (50% damage, 1-hit pop, reform timer), blind buildup break, diurnal sleep. **Glass Empress:** 6 part plates with health and the frost-then-fire shatter window, shard burst, Prism beam (daylight only) |
| `WRWorldDressing.cpp` | `FWRBiomeHazards::KindAt` gains `soft_sand, sand_belt, quicksand, brine_crust, slipface, flash_flood_channel` from hazard cells. New actors: `AWRDustDevil` (wander, lift, throw), `AWRFlashFlood` (a channel spline: roar, trickle, water wall moving down the spline; triggered by the weather front), `AWRSandBeltVolume` (the permanent belt), the brine and quicksand volumes. Kit and hand POI spawning from `world_pois.json`; dig spots; the tilted stelae |
| `WRSky.cpp` | The haboob state (wall front arriving with the wind), heat-haze shimmer and mirage water, the Long Noon sky hold for the desert's region only (a per-biome sun-angle override blended at the border), Ahrumet's local noon override in phase 3, the cold-blue noon shadow tint, starry nights |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The Long Noon timeline: warn, sky hold, Radiance rules, sunstone bloom nodes, the Fata Morgana sky card over the Hourhold, the seven tilted stelae shadows and caches, spawn changes, the Seventh Vault door, Ahrumet's part chance |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 6 desert shrines; Amastan (MetaHuman male, day and night idle spots, the couched Dromad); quests |
| **New `WRPortal.cpp/.h`** | `AWRPortal`: rank, biome, owner (key portals), timer start, state (stable, cracking, spilling, broken, cleared), the light pillar and hum, the escalation ticks (spill packs, warden), entry prompt showing the modifiers, the party entry. `UWRPortalSubsystem` (world subsystem, server): wild slots per biome, refills, rank blocks while an escaped boss lives, the legendary announcement (global chat line plus the Crown pillar flicker), key use. **Instances:** each run loads its graph's rooms as level instances (`ULevelStreamingDynamic::LoadLevelInstance`) at a reserved offset far below the map, one bay per open run (cap 8 concurrent, server setting), unloaded on close. Death bags inside persist until close, then move to the entrance (SPEC). Rank shards on clear with the 10% rule |
| **New `WRPortalRoom.cpp/.h`** | Trap actors: `AWRDartWall`, `AWRSandRoom`, `AWRRollingStone`, `AWRScarabFloor`, `AWRCollapseFloor` (each with a tell timeline, ApplyHit damage, disarm). Puzzle actors: `AWRMirrorRelay` (rotating mirrors with a beam trace), `AWRHourLock`, `AWRWeighingScale`, `AWRSandClock`. Room door controller (doors open on clear or solve) |
| `WRProjectile.cpp` | Thrown `oasis_water_jar` (splash: wet-sand decal and `bWaterHit` hits), `flash_pot`, `red_ochre_beer` (an `animal` boss drink reaction); bone drop and glassfall as ballistic, never-homing projectiles |
| `WRBodyChange.cpp` (fire-frost's) | Sun parameters: albedo shift, fleck mask reveal by zone (cheekbones, collarbones, shoulders, arms, spine, hips), hair-tip lightness, iris ring then full gold with specular ×1.4, a night fleck glow from Radiance, the full-Radiance warm shimmer; blend rules with Fire, Frost and Blood |
| `WRUIHud` / `WRPlayerUI` | The Radiance bar (Sun gold, dimming edge at night); the blind meter (status UI); the compass ribbon and waypoint hide in the belt, restored by the lens; portal prompt with rank, modifiers and break state; rank colour on map icons |
| `WRUIMap.cpp` | Portal icons in rank colour; escaped boss marker after a break; Carrion kite scout marker |
| `WRAudioDirector.cpp` | Desert pools and sub-zone beds, the Wall low-pass, Ahrumet's 3 sections, the Long Noon layer, the Hourhold bed, the dawn song world sound, portal hums |

### 3.3 Assets and the art route

**Route (KEPT):**
1. Concept, picked by Aaron.
2. Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. **Real clips** from Fab animal packs or Truebones mocap, retargeted with UE's IK Retargeter.
6. Every creature shown standing at scale beside the hero before any behaviour is wired.

No engine primitives, no unfinished AI meshes.

| Asset | Route and clip source |
|---|---|
| Dunejaw | Concept from sandfish skink + shark jaw + a sail of grit scales; Hunyuan base; Blender teeth, scale detail. Clips: a Fab monitor-lizard or crocodile pack (walk, run, bite, tail sweep, death) plus keyed sand-swim undulation and the burst; the fin is a separate mesh for the under-sand state |
| Glasstail | Concept deathstalker scorpion + a glass bulb; a Fab scorpion or arachnid pack (walk, pincer attacks, sting, death); the bulb glass material with a sun-catch specular; a night UV emissive mask |
| Carrion kite | A Fab vulture or eagle pack (soar, flap, dive, land, hop, peck); a rust-stained chest texture and beard cards |
| Duskjackal | A Fab wolf or fox pack (trot, gallop, bite, howl, sleep) with jackal proportions and fennec ears (bone scale); gilt-dust material on the muzzle and paws |
| Gildback | Concept thorny devil; a Fab lizard pack (jerky walk with the rocking gait as a play-rate pattern, bask, headbutt) plus keyed curl-and-roll; emissive spines driven by stored light |
| Dromad | A Fab camel pack if one fits, else a horse or llama rig with camel proportions; the *dulla* as a blend-shape sac |
| Shimmerbuck | A Fab antelope or deer pack (walk, run, gore, rear); oryx horns and face bands; the mirage is a copy with a shimmer material and no shadow |
| Scarab digger | Custom sculpt; a Fab beetle pack or keyed (walk, roll ball, dig, chirp); the ball as a separate physics-driven mesh |
| Sandhen, Wellfrog | A Fab bird pack (grouse or chicken); Wellfrog keyed (squat idle, hop, burrow, squeeze) with a translucent water body shader |
| Ssarrok | The Dunejaw rig ×3.7 with harpoon and collar props, a torn fin, a milky eye; a gulp blend shape |
| Mirage Matriarch | The Shimmerbuck rig ×2 with longer horns; a constant shimmer FX; three mirage copies |
| Glass Empress | The Glasstail rig ×4 with six plate meshes (break states with Chaos fracture or pre-broken pieces) |
| Ahrumet | New concept (Aaron picks). A seated-colossus sculpt in Blender (Hunyuan base for the body, hand-sculpted eroded face), the sun-disc ring, the sunstone eye (emissive), crack decals pouring sand (Niagara). Rig: a humanoid skeleton for UE Mannequin-retargeted giant clips (stomp, sweep, grab, kneel, rise), with keyed gnomon-club attacks. The gnomon as a separate actor mesh that attaches to the hand |
| Portal creatures and bosses | Ushabti (faience material over a humanoid rig, Mannequin clips); Sekhra (a Fab lion pack plus a flame-light mane FX); Scarab Mother (the Scarab digger rig ×5); the Weigher (MetaHuman-proportion giant with a jackal head sculpt, Mannequin clips, a bronze scale prop); Apepa (a spline-chain serpent rig like the swamp's Gorgeleech, 24 segments) |
| Amastan | A MetaHuman male preset (attractive, early 30s), the indigo tagelmust and robe (cloth sim on the veil tail), the takouba, the silver-and-sunstone cross |
| Landmarks | The Haboob Wall (a Niagara volume with mesh cards), the gnomon and plaza (Blender), the Broken Sun Gate pylons, the Fallen Hour head, whale skeletons (a Fab whale skeleton scaled, or Blender from a scan), sun-barque hulls (Blender), the Tall Wreck (the newcomer ship kit, re-dressed), the Needles (terrain stamps plus sculpted spires) |
| Kit | Fab desert packs for dunes, rocks, palms, acacia, ruins (licence rule: use unless explicitly barred); a Sunwatch kit in Blender (stelae, waystations, pylons, mirrors, scales, hourglass, tomb modules for the portal rooms) |
| Portal rooms (Desert) | 32 handmade rooms built from the Sunwatch tomb kit on the 4 m grid, each a Packed Level Actor / level instance |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles and Kenney, layered in a DAW per species. About 8 files per creature and 12+ per boss. Loudness matched.
- **Music:** the 2.16 cue sheet, chosen from libraries first (Middle-Eastern and North-African ensemble libraries with real oud, ney, qanun, bendir and darbuka players), credited. About 20 cues counting stems.
- **Beds:** 5 sub-zone beds, the Wall storm bed, the Hourhold bed, oasis water and palms, and the dawn song.

### 3.5 Map dressing

- `Tools/terrain/dressing.mjs` reads the desert entries in `world_pois.json`:
  - It places hand sites at their coordinates.
  - It fills kit POIs per sub-zone with Poisson spacing of 150 m or more.
  - It walks routes (the wadi road, the Avenue of Hours, the caravan routes, dune crests) and adds a kit POI wherever a point is more than 125 m from any POI.
- **Ground layers** are painted from sub-zone polygons by the existing auto-material: soft sand, hardpan, wet sand, salt crust, red rock, desert pavement. Surface kind is written to the hazard cells for the CMC and sand-divers.
- **Dunes:** star and crescent dune stamps aligned to the east wind (KEPT world wind), with the slipfaces on the west (lee) sides.
- **The landmass itself** comes from world-map's B2 terrain step (Desert half and Saltgate). This lane only dresses it.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R5 status: blind** in `WRStatusComponent` + `FWRHit` buildup + the blind meter and vignette | Everything below uses it |
| 1 | **Proof of one: the Dunejaw on a dune field, end to end.** Concept (Aaron picks), mesh, real clips, voice, attack rows with socket hits (fin stalk, burst lunge, bites, tail swipe, sand spray), sand-swimming on soft sand only, beaching on hardpan, the vibration sense, the soft-sand surface and a quicksand pocket with its tells, and one D2 POI (the Swallowed Caravan) dressed around it | Proves the sand-swim tech, visible-contact damage, real clips, a fused voice, the ground-surface system, the hazard and a dressed place in one item |
| 2 | **The Sun path:** `sun.json`, the Radiance attribute and tick (day, night, exposure, weather), the 11 ability classes in `ClassForNode`, the passives, Sandwalker, the proc FX, the 6 CB combos | The path is the round's headline; it needs only step 0 |
| 3 | Desert terrain dressing: sub-zones D1 to D5, ground surfaces, dunes, the 11 landmarks, the 35 hand sites, the kit fill, caves, resources (sunstone, sunglass, copper, acacia, clay, ochre) | The world the rest stands in |
| 4 | The other 9 creatures with clips, voices and rows; day and night tables; living-world beats; tame utilities | Reuses step 1's frame |
| 5 | Hazards (the belt, haboob weather, brine, slipfaces, dust devils, flash floods) and the Sunwatch lens | After the ground exists |
| 6 | The Mirage Matriarch (mirages and shadows), the Glass Empress (plates), then Ssarrok (roaming, swallow, beaching) | Minis in order of new tech |
| 7 | Ahrumet: plaza, the wake, phase 1 with the beam and cover, sun-hot cracking and the water jar, the Uprooting, phase 2 with the Noon Pulse shadow test, phase 3 behind the artifact flag; drops; music | The biggest single item; needs the creatures, blind and the Oasis water jar |
| 8 | Shrines, Amastan and his 6 quests, run-back stone, fast travel | After POIs and the hero rig |
| 9 | The Hourhold and its three entrances, the mirror puzzle and the Sun-Vault | After the hazards and the quests |
| 10 | Music set and sub-zone beds | Independent; can slot in anywhere after step 3 |
| 11 | The Long Noon event (sky hold, bloom, the tilted stelae, the Fata Morgana, the Seventh Vault) | Needs the Hourhold and the sky work |
| 12 | **Dungeon portals, proof of one:** one Common Desert portal with a fixed graph of 3 rooms (an entry, a dart-wall corridor, a combat hall) and the Ushabti Overseer; the portal actor, the light pillar, entry, the level-instance bay, clear and close, the rank shard | Proves the instance tech before the full system |
| 13 | Portals in full: all 5 ranks, slots and refills, escalation and break, escaped bosses, modifiers, keys, legendary announcements; the 32 Desert rooms, traps and puzzles, the 5 Desert portal bosses | On top of step 12 |
| 14 | Portal rooms for the other six built biomes (seeds in 2.22.4) | Each a content step; the framework is done |
| 15 | Gear: Dunewrap fit on the MetaHuman bodies, desert items, talismans and rings | After the R2 slot split (survival-crafting) |
| 16 | Body change for Sun | After the MetaHuman hero (R0) |
| 17 | Sun ↔ Tide invasions | In R6, once the Ocean and its roaming mini exist |

**Gates:** compile, cook and package only. Aaron tests each step.
