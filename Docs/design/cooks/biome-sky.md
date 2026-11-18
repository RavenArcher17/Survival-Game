# Cook: biome-sky

Lane: **biome-sky**. Covers the Sky islands biome and the Wind path (round R7): FULL-GAME 3.2.5, the Sky parts of 3.3, and the Wind row of 4.2. Every GAP there is replaced with a build-ready design. Written 2026-11-13. This lane does research, design and plan; the build comes later, round by round, from section 3.

**Frame.** Coordinates are metres in the continent frame of `cooks/world-map.md` (origin at the old island centre, +x east, +y south). The Sky biome is the east half of belt B1, the Hollow Cape: 0.8 km² of **Windward Cliffs** ground on the cape's east coast (x +1,000 to +1,800, y −9,500 to −8,000), plus **2.0 km² of islands in about 5 km² of air** (CAL, SPEC) between the Flue at (0, −8,800) and the north-east sea out to x +3,600, y −10,300. Islands float from 500 m to 1,300 m. Altitude replaces distance for bands (world-map SPEC): lowland under 750 m, midland 750 to 1,050 m, highland above 1,050 m.

**What this cook keeps from other cooks (SPEC for this lane, not re-decided):**

| From | Kept exactly |
|---|---|
| `world-map.md` | Belt B1 (Wind above, Spore below), the Flue at (0, −8,800) (cavern floor −40 m, rim 700 m, updraft +12 m/s to 1,300 m, 250 m wide), camps at (+1,600, −8,300), (+1,500, −8,900), (+1,000, −9,300) with a **Kitewright lift** each (40 s crank to the first island at 520 m, 4 people or 1 hauler), Aeolith's arena (a ring of drifting platforms over the Flue, 1,250 m), the Undercloud (cloud bank 500 to 600 m at (+2,600, −9,600)), 18 cliff POIs + 46 island POIs on ≈40 islands, island rules (≥ 40 m across, ≤ 220 m apart, every island ≥ 1 POI, islands > 150 m ≥ 3), claims allowed on islands ≥ 120 m, the Kitewrights (lung ta wind-poles; their champion's winged skeleton at the Flue's floor), the Deepfarmers' mushroom titan husk on the highest island, glider table (crude / levistone), updraft kinds (ridge lift +8 m/s, island columns +10 m/s, the Flue +12 m/s), weather row (Clear 45, Overcast 10, Rain 10, Cloud fog 15, Gale 20), the hazard row (lethal falls, gusts 1,200 cm/s for 0.8 s, 1.5 s whistle), Featherdown draught (fall damage ×0.5), the Wind–Spore invasion route through the Flue, night light (stars close overhead, moonlit cloud tops), the Chain Cliffs border landmark |
| `combat.md` | Knockback hit property (2 cm per poise point, wall slam +20 stance), Wildbrand rows Wind cut, Whirlwind, Plunge, Wind arrow (2.12), Wind spell numbers (2.13: Gust, Air dash, Vortex, Tornado, Sky lord, Storm volley), creature row schema and archetype bands (2.18), boss template (2.17.1, 2.17.1b, 2.17.5), fall damage (none under 6 m, lethal over 18 m; Featherfall exempt) |
| `hero.md` | Wind trim pale teal #9FD6CF; Skysilk look hook (levistone shards orbiting the shoulders, long silk scarf); Wind body change row (2.6) |
| `survival-crafting.md` | Skysilk set stats and recipe; anchor boots; glider (back slot); Windflax and Cloudberry crops; Puffwing glider feather; cloud minnow, Skyfin, the Aethercarp in the floating shrine's pool; Cloudberry porridge; windmill ×1.5 on Sky islands; Updraft lift machine; Cloud spire building set; Windmane flight rules and Cloudback carry; the three Sky survival sites (cloud garden, feather rookery, lift station); Gale front weather effect |
| `fire-frost.md` | Shrine rules (5 per biome, claiming, XP, fast travel), path master rules, roaming-mini rules, micro-hazard rules, biome-event frame (one game day, 5 min sign, triple signature pay), POI density and night rules |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game | What it does | Source | Lesson for the Sky islands |
|---|---|---|---|
| **Tears of the Kingdom** (sky islands) | Sky, surface and depths are one system joined by updrafts and freefall. Early islands looked like "specks of trash" until they were made bigger and readable | https://zeldawiki.wiki/wiki/Updraft ; https://gamedeveloper.com/production/zelda-devs-originally-made-sky-islands-look-like-specks-of-trash (world-map) | Few, big islands with bold silhouettes and visible updraft columns. The Flue joins sky and caverns: Wind above, Spore below |
| **TotK: Colgera** (Wind Temple boss) | Fought entirely in freefall and gliding; weak points on head, body and tail open when it fires; phase 2 adds tornadoes | https://zelda.fandom.com/wiki/Colgera ; https://zeldatearsofthekingdom.wiki.fextralife.com/Colgera | Aeolith's hidden phase 3: you ride its back and break crest vents. Its airborne moves open its weak points |
| **Elden Ring: Crumbling Farum Azula** | A ruined city drifting in a permanent storm, fragments hanging in the air, dragons roosting on them; a lord who waits at the storm's heart | https://eldenring.wiki.gg/wiki/Crumbling_Farum_Azula ; https://eldenring.wiki.gg/wiki/Placidusax | The Kitewrights' ruins are white stone fragments in moving air. Aeolith waits at the Flue's heart, guarding something long gone |
| **Elden Ring: Lansseax, Agheel** | Four-winged dragon; lightning spear shown by head and wings raised plus a roar; leaves to the sky at half HP (phase change) | https://eldenring.wiki.fextralife.com/Ancient+Dragon+Lansseax+(Altus+Plateau) ; https://upcomer.com/how-to-defeat-flying-dragon-agheel-in-elden-ring/ | Big tells on the whole body (wings up, throat swelling). A phase change by taking to the air |
| **Monster Hunter: Kushala Daora** | Wind pressure is the fight's main mechanic: it trips hunters; a wind barrier deflects projectiles; toppling or breaking its head removes it; a 3-piece armour set nullifies wind pressure | https://monsterhunterwiki.org/wiki/Kushala_Daora_(MHGen) ; https://monsterhunterworld.wiki.fextralife.com/Wind+Pressure+Nullified | Gusts are the biome's gear check (anchor boots, Skysilk 4). Aeolith's gale breath is a push, not a nuke; staggering it breaks its gale crown |
| **Dragon's Dogma 2: Griffin** | Grab on and ride it; it shakes you off at height, which kills; burn its wings to stop it fleeing | https://dragonsdogma2.wiki.fextralife.com/Griffin ; https://www.gamerevolution.com/guides/972098-dragons-dogma-2-griffin-escape-stop-griffin-following-chasing-griffon-follow-chase | Riding a boss is a real risk: hold on through barrel rolls or fall. Wing damage grounds flyers (Kyrra) |
| **Horizon: Sunwing, Stormbird** | One flying mount, earned late; the strongest flyers are fought, not ridden; flying was "the most difficult feature" | https://gamesradar.com/how-to-fly-horizon-forbidden-west-sunwing-override-wings-of-the-ten | Windmane is the first and only flying mount here, earned by stealing an egg. Flight is the R7 headline, built carefully, after gliding |
| **Enshrouded** | Hook for up, glider for across; ~130 updrafts on the map; a patch made updrafts need built-up momentum and players hated it | https://pindrop.gg/enshrouded/guides/glider-grappling-hook ; https://enshrouded.featureupvote.com/suggestions/707933/comment/778071 | Updrafts always lift on entry, never need a run-up. Kitewright lifts are "up", gliders are "across" |
| **Palworld, Ark** | Glider drains the player's stamina; flying mounts have their own stamina bars (Palworld top flyers 300 to 350) | https://palworld.wiki.gg/wiki/Gliders ; https://www.gameserverkings.com/knowledge-base/palworld/palworld-best-flying-mounts/ | Kept from survival-crafting: Windmane stamina 300, its own bar |
| **Ghost of Tsushima** | The wind itself guides you; no minimap | https://www.gamerevolution.com/guides/653064-how-to-use-the-guiding-wind-in-ghost-of-tsushima-ps4 | The wind is readable everywhere: the Great Kite always flies downwind, lung ta cloth shows gusts, Mistlarks trace updrafts |

### 1.2 Real world to build from

| Fact | Source | Use |
|---|---|---|
| **Ehecatl-Quetzalcoatl**, the feathered serpent as wind god. His temples are round so the wind can flow round them; doorways are carved as serpent jaws | https://www.worldhistory.org/Ehecatl/ ; https://m.phys.org/news/2016-11-circular-temple-god-uncovered-mexico.html | Aeolith is a feathered wind serpent. Kitewright temples are round; the arena door is a serpent jaw |
| **Sumpango giant kites (barriletes)**: flown on the Day of the Dead so the dead know their family by the kite's colours and speak through the string; burnt after (UNESCO 2024) | https://ich.unesco.org/en/RL/technique-of-making-the-giant-kites-of-santiago-sacatequez-and-sumpango-guatemala-01991 ; https://www.latinpost.com/articles/24924/20141102/guatemala-all-saints-day-dia-de-todos-santos-kite-festival-kite-festival-in-guatemala.htm | The Kitewrights flew a kite for each of their dead. The Kite Field and the Undercloud's kite graveyard |
| **Lung ta** prayer flags on wind poles (world-map SPEC) | https://en.wikipedia.org/wiki/Prayer_flag | Waymarks; their cloth snapping is the gust tell |
| **Tower of the Winds**: an octagon with the eight wind gods carved round its top and a bronze Triton weathervane | https://en.wikipedia.org/wiki/Tower_of_the_Winds | The Wind Rose at the Great Shrine: eight carved winds; the Great Kite as the world's weathervane |
| **Fūjin's bag of winds** (and Aeolus' oxhide bag for Odysseus) | https://en.wikipedia.org/wiki/F%C5%ABjin | Aeolith's throat sac swells like a wind-bag before its gale breath (the tell) |
| **Ziz / Roc / Simurgh**: a sky bird whose wings blot out the sun | https://en.wikipedia.org/wiki/Roc_(mythology) | The silhouette scale of Aeolith's wings over the arena |
| **Angel Falls**: water falls so far it turns to mist before it lands | https://geology.com/records/highest-waterfall.shtml | Island waterfalls dissolve into mist 150 to 300 m below (a Niagara fade, never a hard end) |
| **Fog drip / fog nets**: trees and nets comb water out of passing cloud | https://en.wikipedia.org/wiki/Fog_drip | Island water comes from cloud combed by wind-sculpted trees: why floating rock has rivers |
| **Ballooning spiders**: spiders fly on silk threads; "gossamer" falls in sheets | https://en.wikipedia.org/wiki/Ballooning_(spider) | **Skysilk** the material is gathered from gossamer drifts snagged on Cloud Garden trees |
| **Portuguese man o' war**: a gas float that sails at 45° to the wind, left- and right-handed forms drift opposite ways; tentacles trail below and sting even when dead | https://www.britannica.com/animal/Portuguese-man-of-war ; https://usa.oceana.org/blog/portuguese-man-war-beautiful-and-dangerous/ | The **Sailbell**, a new drifting creature: blooms that drift across bridges, readable by their handedness |
| **Mobula rays** leap from the sea in huge groups and belly-flop | https://scubadiverlife.com/mobula-rays-jump/ | The Skyray's belly-flop slam and its group "breaching" at dusk |
| **Frigatebird**: stays aloft for weeks, harasses other seabirds in mid-air until they drop their catch; red inflatable throat sac | https://www.birdguides.com/articles/ornithology/how-frigatebirds-soar-for-weeks-on-end/ ; https://whyevolutionistrue.com/2016/07/06/how-frigatebirds-stay-aloft-for-weeks-and-weeks/ | Galeharriers rob Puffwings and players' carried meat in mid-air; Aeolith's red throat sac |
| **Peregrine stoop**: wings tucked into a teardrop, 39 to 51 m/s, steering by cupping the hand-wings at the pull-out | https://doi.org/10.1038/s42003-018-0029-3 ; https://audubon.org/news/research-reveals-exactly-why-peregrine-falcons-are-so-deadly | The Galeharrier and Kyrra stoop: the fold is the tell, the line is fixed after the fold |
| **Microraptor**: four feathered wings in a biplane layout; hind wings raise turn rate 33 to 50% | https://pmc.ncbi.nlm.nih.gov/articles/PMC1780066 ; https://www.nationalgeographic.com/adventure/article/121105-microraptor-dinosaurs-flying-wings-paleontology-science | Windmane's four wings are believable anatomy: fore-wings from the shoulders, hind-wings from the thighs, used to turn hard |
| **Shrike larders**: shrikes impale prey on thorns | https://en.wikipedia.org/wiki/Shrike | The **Gallowshrike**, a new pack hunter; its larder trees are a story and a warning |
| **Cassowary kick**: a 12 cm dagger claw on the inner toe | https://en.wikipedia.org/wiki/Cassowary | The Gallowshrike's kick |
| **Owls' silent flight**, **nightjars' and frogmouths' huge gapes** | https://en.wikipedia.org/wiki/Owl#Adaptations_for_hunting ; https://en.wikipedia.org/wiki/Nightjar | The **Hushwing**, a new night hunter: silent wings, a mouth that opens wider than its head |
| **Lenticular and Kelvin-Helmholtz clouds**: standing caps over peaks in wave flow; breaking-wave billows in shear | https://scied.ucar.edu/image/kelvin-helmholtz-clouds ; https://skybrary.aero/index.php/Kelvin-Helmholtz_Waves | The Turning Wind's world sign: a lenticular cap over the Flue and wave billows across the sky |
| **Aeolian harp**: wind-driven strings that sing harmonics via vortex shedding | https://www.britannica.com/art/Aeolian-harp | Real recorded aeolian harps as the music's drone and as wind harps on Kitewright ruins |
| **Siku panpipes**: two players hocket (alternate notes), each half needed for the tune | https://clas.osu.edu/andean-music-teacher-guide | Kitewright music: the melody split between two flutes, panned left and right (two wings) |

### 1.3 Lessons we take (short)

1. **Height is the danger and the joy.** Falls kill; gusts tell 1.5 s ahead; updrafts always lift on entry.
2. **Wind you can see.** Cloth, grass, gossamer, birds and the Great Kite show it everywhere.
3. **Fight the wind, not a nuke.** Gusts and gale breath push; the edge does the killing; anchor gear and bracing answer them.
4. **Flyers are fair.** Every dive has a fold tell, a fixed line, and a grounded window after.
5. **Brighter magic, real monsters.** Bright white stone and blue sky (DD tone, SPEC), with predators that are terrifying up close.
6. **One story, told by kites and stone.** Kite-flyers who spoke to their dead and bound a serpent to hold back the spores.

---

## 2. Design

Tags: **SPEC** keeps Raven's design, or a settled cook's design, exactly. **GAP-COOKED** is this lane's design. **RAISE** adds to a SPEC item without changing it.

### 2.1 The biome at a glance

| Item | Design | Tag |
|---|---|---|
| Path theme | Gliding, knockback, aerial combat; brighter magic in tone (DD) | SPEC |
| Look | Floating islands of pale limestone and granite, their undersides bearded with pale blue **levistone keels** that glint like mica and glow faint cyan where they lift. Waterfalls pour off island lips and dissolve into mist far below. White Kitewright ruins: round houses, round temples, broken sky-bridges on chains. Wind-sculpted cedars and junipers leaning all one way, grass combed silver by the wind, lung ta cloth faded on every pole. Above: a deep, clear sky; below: a sea of cloud with the ocean showing through gaps | GAP-COOKED (FULL-GAME draft kept, made concrete) |
| Biome palette | Sky blue #8EC3E6, cloud white #F4F6F8, Kitewright limestone #E9E4D8, levistone #A9C4D9 with lift-glow #CFE9FF, wind-combed grass #9DB59A, cedar bark #6B5442, lung ta cloth (faded) red #B5523B, yellow #D8B24A, green #6E9A5A, blue #4F7FAF. Path trim pale teal #9FD6CF (hero, SPEC) | GAP-COOKED |
| Signature resource | **Levistone**: stone that floats (2.6) | SPEC name / GAP-COOKED detail |
| Main hazard | Falls and gales (2.5) | SPEC numbers + RAISE (visual gust tells) |
| How you get up | Updraft columns from the Windward Cliffs (ridge lift), Kitewright lifts, gliding, then the Windmane | SPEC (world-map) |
| Hidden sub-biome | The Undercloud (2.7) | SPEC site / GAP-COOKED contents |
| Event | The Turning Wind (2.15) | GAP-COOKED |
| Shrines and path master | 5 shrines; path master **Yara Kesh**, the one-winged glider-maker (2.13) | RAVEN-PLANNED exist / GAP-COOKED who and where |
| Music | 100 BPM, F Lydian (2.12) | SPEC tempo and key / GAP-COOKED cue sheet |
| Night | Stars close overhead, moonlit cloud tops (SPEC); levistone keels glow faint cyan under every island so islands read as floating shapes at night; Sailbells glow violet; lit shrine braziers | SPEC + GAP-COOKED |
| Tone | Bright, wide, high and quiet up close; the wind the loudest thing. Fear comes from the drop and from what hunts you at the edge | GAP-COOKED |

### 2.2 Sub-zones (GAP-COOKED; names SPEC from CAL Part 3)

CAL's four names are kept; the Undercloud is the hidden fifth. Each sub-zone follows world-map 2.4 (own ground, palette, props, day and night table, ambient bed, a signature silhouette, at most one micro-hazard).

| # | Sub-zone | Where (x, y), altitude | Islands, area | Band | Ground and feel | Palette | Props (unique kits) | Micro-hazard | Ambient bed | Silhouette |
|---|---|---|---|---|---|---|---|---|---|---|
| W1 | **Windward Cliffs** | Ground: the cape's east coast x +1,000 to +1,800, y −9,500 to −8,000, cliff tops 150 to 320 m over the sea. Plus the **Lift Steps**, 9 islands at 520 to 750 m right above it | Ground 0.8 km²; islands 0.45 km² | Lowland | Sea cliffs with ridge lift, sheep-cropped turf, thrift and sea campion, shard fields of fallen levistone at the cliff foot, cage-lift towers and chains. The Lift Steps: low, green, gentle islands with Kitewright farm terraces | Turf #8FA77A, cliff granite #9A9690, levistone #A9C4D9, rust iron chain #7A4A2E | Lift towers and cages, shard fields, chain anchors, puffin-like rookery burrows, wind-poles | **Lee rotors** (2.5) | Surf boom below, constant wind, kittiwake-like calls, chain creak, Puffwing honks | The Chain Cliffs (L6) |
| W2 | **Cloud Gardens** | Over the sea, x +1,900 to +3,300, y −9,100 to −8,100; 600 to 900 m | 11 islands, 0.55 km² | Lowland to midland | Terraced garden islands gone wild: Windflax in blue drifts, Cloudberry mats, cedars hung with gossamer sheets that flash in the sun, waterfall pools, a rain-lagoon island | Flax blue #6F8FC9, cloudberry amber #E0A040, gossamer silver #E8EEF2, moss gold #B9A55A | Garden terraces, silk-houses with spindles, fog-net frames, waterfall pools, kite racks | **Sinking stones** (2.5) | Wind in flax, waterfalls, Mistlark trills, wind harps on ruins, the hum of Sailbell blooms | The Weeping Isle (L2) |
| W3 | **Ruined Sky-bridges** | The long chain from x +3,100, y −9,500 arcing north and west to x +500, y −10,100; 750 to 1,050 m | 10 islands, 0.45 km² | Midland | Narrow, rocky islands linked by broken Kitewright bridges: stone decks on chains, plank spans, toll towers, stubs over nothing. Windmane nesting ledges on the island flanks | Pale granite #B7B3AA, chain iron #5C4A3C, plank grey #8E8679, kite-cloth faded colours | Chain-hung bridge decks, toll towers, winch drums, fallen spans, Windmane ledges | **Downbursts** (2.5) | Chains singing, plank creak, gusts through towers, Galeharrier screams | The Fallen Span (L3) |
| W4 | **Floating Shrines** | Round the Flue, x −300 to +900, y −9,500 to −8,300; 1,050 to 1,300 m | 8 islands + Aeolith's ring, 0.35 km² | Highland | High, cold, bright: round white temples with serpent-jaw doors, a wind-wheel's broken vanes across three islands, mycelium crusting the undersides nearest the Flue, the Flue's roar below | Temple white #F0EDE6, jade feather tile #2F8F7A, spore grey #8A8A80 and spore glow cyan #39E6C2 (near the Flue only) | Round temples, wind roses, wind-wheel vanes, spore-rotted gardens, serpent statues | None (the Flue and the boss) | The Flue's warm roar from below, thin high wind, temple bells, Aeolith's distant call | The Serpent's Ring (L1) and the Paper Titan (L4) |
| W5 | **The Undercloud** (hidden) | Inside the cloud bank at (+2,600, −9,600), radius 500 m; 500 to 600 m | 6 islands, 0.2 km² | Midland (a hidden-biome exception: its altitude says lowland, its creatures are midland and highland) | Islands lost in permanent cloud: vision 25 m, soft grey light, dripping trees, the Kitewrights' kite graveyard | Fog grey #BFC5CA, wet stone #6E7378, kite colours muted | Tethered death-kites by the thousand, bone tubes, round tomb-houses | Fog (vision 25 m) | Drip, muffled wind, kite cloth fluttering everywhere unseen, Hushwing churr | (Hidden; read from outside as a cloud bank that never moves) |

**Blends:** 60 to 120 m between sub-zones (world-map rule): W1's green islands get rockier and higher into W3; W2's gardens thin to bare rock toward W3. **Borders:** W1 ground meets the Ocean's Blowhole Shelf at y −8,000 (the Chain Cliffs, world-map SPEC); W4 meets the Fungal biome only through the Flue, where spore crust creeps up island undersides.

**Island list (38 + ring + 6 Undercloud).** Each island is a hand-placed mesh (2.6 note on building); sizes are longest axis. Every island ≥ 40 m, gaps ≤ 220 m horizontal, and a glide line from a higher island or an updraft column reaches every island.

| Group | Islands (size m, altitude m) |
|---|---|
| W1 Lift Steps | **First Isle** 140 (520; all three lifts arrive here), Still Pool Isle 120 (560; Great Shrine), Terrace Isle 260 (600), Larder Isle 90 (620), Ewe Isle 70 (640), Gap Rock 50 (660), Chainfoot 110 (690), Tumble 60 (720), Highstep 150 (745) |
| W2 Cloud Gardens | **The Weeping Isle** 420 (880; biggest island, 3 caves), Flax Isle 200 (640), Silk Isle 180 (700), Rain Lagoon (Gulper's Bight) 240 (760), Kitefield 300 (820), Berry Isle 90 (660), Nets 70 (690), Twin Pools ×2 60 and 55 (730), Moss Stair 110 (790), Drift Rock 45 (850) |
| W3 Sky-bridges | Tollgate 160 (780), the Span North 90 (810), the Span South 85 (805) (the Fallen Span hangs between), Winch Isle 70 (840), Eyrie Rock 120 (900; Kyrra's rest), Mane Ledges 200 (960; Windmane nests), Long Deck 60 (930), Needle 40 (990), Last Toll 130 (1,020), Brink 75 (1,040) |
| W4 Floating Shrines | **Lid Gate** 110 (1,230; the Serpent Door, run-back stone and altar), **Titan's Crown** 230 (1,300; the highest island, the Paper Titan), Wheel Isle 140 (1,150), Vane Rocks ×2 70 and 60 (1,120), Rose Temple 120 (1,180), Spore Scar 150 (1,080), Bell Rock 50 (1,260) |
| Aeolith's ring | 8 platforms 14 to 18 m round the Flue axis at 1,250 m (2.11) |
| W5 Undercloud | Kite Barrow 160, Tube Isle 90, Bonebell 70, Lantern Shelf 80, the Drowned Garden 120, the Last Kite 60 (all 500 to 600 m) |

### 2.3 Skyline landmarks (GAP-COOKED)

Six landmarks (world-map: one per ~0.6 km² of biome), each readable from 1 km. L1 is the super landmark (reads from 3 km).

| # | Landmark | At (x, y), altitude | What it is | Reads at night as |
|---|---|---|---|---|
| L1 | **The Serpent's Ring** | (0, −8,800), 1,250 m | Aeolith's arena: eight white platforms circling the Flue's updraft, a 60 m feathered serpent coiling through them. A lenticular cap of cloud sits over it in calm weather. Reads from the Crown and from every B1 and B2 coast | Spore-glow rising up the Flue's column in cyan; the platforms' lit braziers |
| L2 | **The Weeping Isle** | (+2,500, −8,600), 880 m | The biggest island: three waterfalls pour off its lip and turn to mist 250 m below; its keel is a 60 m beard of levistone | The keel's cyan glow and the moonlit mist trails |
| L3 | **The Fallen Span** | (+2,350, −9,950), 805 m | A 160 m stone bridge deck that broke in the middle; one half hangs straight down on its chains from Span North, swinging slowly; its lower end is 120 m under the island | Lanterns on the hanging deck once the W3 shrine is lit |
| L4 | **The Paper Titan** (SPEC, world-map) | Titan's Crown (+600, −9,300), 1,300 m | The Deepfarmers' mushroom titan, 40 m tall, dried by wind to grey paper, kneeling on the highest island with Kitewright spears through it | Its gill-ribs catch the moon; faint spore-glow at its roots |
| L5 | **The Great Kite** | Tethered to Titan's Crown, flies 150 to 300 m downwind | A 60 m Kitewright kite of faded red, yellow, green and blue cloth on a chain-and-silk line, still flying. It always streams downwind: **the world's weathervane**, readable from anywhere in B1 and B2 | Its line lanterns, a string of 12 small lights curving into the dark |
| L6 | **The Chain Cliffs** (SPEC, world-map) | (+1,400 to +1,800, −8,000), cliffs 80 to 320 m | Three Kitewright lift towers on the sea cliffs, their chains rising from the sea-cliff foot to the First Isle | Brazier on each tower top; the cages' lamps when moving |

### 2.4 POIs: 18 cliff + 46 island + 8 Undercloud (GAP-COOKED; counts SPEC from world-map)

**Mix (world-map):** 6 landmarks, 5 shrines, 16 hand-made story sites, 37 kit POIs over the 64 surface and island POIs; the Undercloud's 8 are hand-made. **Already counted elsewhere:** the 3 camps, the Ring (Crown), the Flue rim.

**Hand-made story and set-piece sites (16):**

| # | Site | Sub-zone, at | What you see (story through the place only) | Pays |
|---|---|---|---|---|
| H1 | **The Shard Fall** | W1 ground, (+1,400, −8,650) | A small island fell here. Its levistone keel lies shattered at the cliff foot in a field of blue shards; the island's round house sits upside down on the beach with its table and bowls still nailed to the floor (now the ceiling) | Levistone ×10 to 14 (lowland's richest), a Kitewright bowl (lore) |
| H2 | **The Lift Wreck** | W1 ground, (+1,250, −9,050) | A cage lift snapped its chain and fell 300 m. Inside, two skeletons hold each other. A bone tube in one's hand holds a rolled kite of yellow cloth | Iron ×8, chain ×2, "Kite of the given" (lore item) |
| H3 | **The Larder Tree** | W1 Larder Isle | A wind-sculpted cedar hung with impaled Puffwings, a Galeharrier, and a newcomer's backpack on the highest thorn. A Gallowshrike pack lives here | The backpack: a copper glider part and rations |
| H4 | **The Feather Rookery** (SPEC, survival-crafting) | W1 Ewe Isle | Puffwing nests in turf burrows; a plucking hut collapsed under a fallen wind-pole | Puffwing eggs, glider feathers |
| H5 | **The Lift Station** (SPEC, survival-crafting) | W1 Chainfoot ↔ W1 Highstep | A broken updraft-lift platform and its twin on the next island; a newcomer can repair it (the Updraft lift machine's blueprint lies in its winch house) | Updraft lift blueprint |
| H6 | **The Cloud Garden** (SPEC, survival-crafting) | W2 Flax Isle | Terraced Windflax and Cloudberry beds run wild; a gardener's glider snapped in a cedar, its pilot's hat on a branch below | Windflax and Cloudberry seeds |
| H7 | **The Silk-House** | W2 Silk Isle | A round house with spindles and looms, its walls hung with gossamer sheets gathered from the trees; outside, drift frames still catch silk. One loom holds a half-woven Skysilk scarf | Skysilk ×4, recipe scrap "Skysilk weave" |
| H8 | **The Kite Field** | W2 Kitefield | Hundreds of rotted kite frames on stakes up a hill, each with a bone tube at its foot (the Kitewrights' death-kites). At the top a single new kite, crude, sewn from a newcomer's shirt, still flying: someone tried to call someone | 1,000 Wind path XP on first visit (a vista), "Bone tube" lore ×3 |
| H9 | **Gulper's Bight** | W2 Rain Lagoon | A rain-filled lagoon island where Old Gulper floats. Its shore is a white ring of Puffwing bones and one capsized Kitewright skiff (they had boats on the lagoons) | Mini lair (2.10) |
| H10 | **The Toll House** | W3 Tollgate | A toll tower and gate where the old bridge began; beside the gate, offering bowls overflowing with copper rings. The Bridge Sentinel walks out from here (2.10) | Copper ×12 from the bowls (taking them wakes the Sentinel at once) |
| H11 | **The Harrier's Eyrie** | W3 Eyrie Rock | A nest 15 m across built of bridge planks, chain and kite cloth; Windmane skulls round its rim. Kyrra's rest stop | Harrier feathers, a Windmane egg (sometimes, 25%) |
| H12 | **The Glider Graves** | W3 Brink | A ledge under the Weeping Isle's view where broken gliders are lined up and staked like graves, each with a name-stone carved by a different hand. The last stake is empty | Message stone "it's further than it looks" |
| H13 | **The Wind Wheel** | W4 Wheel Isle and Vane Rocks | A Kitewright wind-wheel, 70 m across, that once stood over the Flue and blew the spores back down. Its hub sits on Wheel Isle; three of its vanes lie across the two Vane Rocks like fallen sails; its drive chain trails toward the Ring | Bronze ×10, "Wheelwright's plan" (lore) |
| H14 | **The Spore Scar** | W4 Spore Scar | The nearest island to the Flue, its gardens rotted grey, its underside crusted with Deepfarmer mycelium (world-map SPEC). Gardeners lie face-down at their beds, wearing cloth masks | Deepcap spores (a Spore ingredient), a "Gardener's mask" (filter mask recipe early, needs R8 items to craft) |
| H15 | **The Paper Titan** | W4 Titan's Crown | L4 up close: Kitewright spears through its body, a broken kite line round its neck, the Great Kite's anchor drum at its feet | The Great Kite's anchor drum is a vista (reveals all of W4) |
| H16 | **The Serpent Door** | W4 Lid Gate | A round white temple whose door is a serpent's open jaw. Inside: carvings of eight winds round the wall (Tower of the Winds), and a serpent coiled round a wheel. The arena's run-back stone and the Wind altar | Run-back stone; altar |

**Kit POIs (37), placed by the dressing generator inside the sub-zones (≥ 150 m apart, max gap 125 m on every route and glide line):**

| Kind (world-map type) | Count | Sub-zones | Variants | Pays |
|---|---|---|---|---|
| Kitewright round house (ruin, 6) | 6 | W1, W2, W3 | Roofed, roofless, toppled half over an edge, two-storey | Container: linen, rope, bronze; 15% a lore item |
| Wind-pole cairn (waymarker, 16) | 7 | All, on bridges and island lips | 1, 3 or 5 poles; cloth colours vary | Lights at night when you pass; gust tell |
| Levistone keel outcrop (resource, 10) | 6 | Island undersides W1 to W4 | Small beard, big keel, spore-crusted | Levistone (mined from a hanging ledge or from a glider/ Windmane hover) |
| Gossamer drift (resource, 10) | 3 | W2 | Cedar sheets, fog-net frame | Skysilk ×2 to 4 |
| Galeharrier nest (nest, 8) | 3 | W1 high islands, W3 | Ledge, tower top | Eggs (none: knockout-tame only), feathers, a guard pair |
| Windmane ledge (nest, 8) | 2 | W3 Mane Ledges | Single, double | Egg (steal-tame, SPEC), adults defend |
| Waterfall pool (water and forage, 15) | 4 | W1, W2 | Plunge pool, lip pool | Water, cloud minnows (SPEC fish) |
| Kill set-piece (9) | 2 | W1, W2 | Galeharrier on a Puffwing; Gallowshrikes at a larder | A fight or a free carcass if you wait |
| Vista (11): a high lung ta stone | 2 | W2 Moss Stair, W3 Last Toll | — | Reveals 400 m |
| Hazard teacher (12) | 2 | W1 (a single gust lane with three wind-poles and a short drop onto turf, fall 5 m), W2 (one sinking stone over a 4 m drop) | — | Learning the tell |
| **Total** | **37** | | | |

**Undercloud sites (8, hand-made):** see 2.7.

### 2.5 Hazards (main: SPEC numbers + RAISE; micro: GAP-COOKED)

**Main: falls and gales (SPEC numbers, world-map).** Fall damage by the built rule: none under 6 m, lethal over 18 m (combat SPEC). Gusts: 1,200 cm/s for 0.8 s, with a 1.5 s whistle tell, on marked **gust lanes** (island lips, bridges, exposed ridges) every 8 to 15 s in fair weather (Gale state: everywhere above 300 m, every 4 to 8 s, survival-crafting Gale front SPEC).

| Part | Design | Tag |
|---|---|---|
| The tell (RAISE: visible, not just a sound) | 1.5 s before: a rising whistle from the upwind side; a wave of flattened grass and torn cloud racing toward you along the lane; wind-pole cloth snaps taut; Mistlarks in range scatter downwind; gossamer ripples. The camera gets 1 cm of shake at 1.0 s | GAP-COOKED |
| Direction | Always along the world wind vector (world-map), ±20°. On a lane it pushes toward the lane's marked drop side | GAP-COOKED |
| Answers | **Crouch-brace** (hold crouch): push ×0.5. **Anchor boots** (SPEC): no push. **Skysilk 4** (SPEC): no push. **Featherfall** (root, SPEC): no fall damage. **Glider** (SPEC): open as you go over. **Featherdown draught** (SPEC early pass): fall damage ×0.5 | SPEC + GAP-COOKED (brace) |
| Gusts in combat | Gusts push creatures too, except fliers and Sky-born ground creatures (they resist their biome's hazard, CR SPEC). A gust that pushes an enemy off an edge credits you if you hit it within 6 s | GAP-COOKED |
| No floors | No safety nets, no invisible walls at lips, no ledge-grab assist (DD and Aaron SPEC) | SPEC |

**Micro-hazards (fire-frost rule: 3 to 4, each in one or two sub-zones, ≥ 0.9 s tell, each can kill).**

| Hazard | Where | Tell | Effect | Answers |
|---|---|---|---|---|
| **Lee rotors** | W1: the lee (downwind) side of the Lift Steps and of the cliff-top lips | Torn, tumbling cloud wisps and spinning gossamer on the lee; a low "whump-whump" flutter 1.0 s before a rotor hits a glider | Gliders entering a rotor: sideways shove 600 cm/s for 0.6 s and sink +3 m/s for 2 s; on foot at a lee lip: a down-gust 400 cm/s toward the edge for 0.5 s | Glide on the windward side (the lesson); anchor boots stop the on-foot part |
| **Sinking stones** | W2 stepping-stone paths between garden islands (5 to 9 m flat stones floating in lines) | Tired levistone: the stone's keel glow flickers and dims, dust trickles off its edge and it groans; 1.0 s after you land on it | It dips 1 m, then sinks at 4 m/s for 6 s, then rises back over 20 s. Anyone on it rides it down into cloud (fall lethal off it once it's 18 m below the next stone) | Keep moving: never stop on a flickering stone; Wind path air dash; glider |
| **Downbursts** | W3 glide lines between bridge islands | Grey rain streaks (virga) hanging under a dark cloud puff; a hiss of falling air 1.2 s before you enter; the streaks are visible from 300 m | A 30 m column: gliders sink +6 m/s inside; on bridges, a 0.8 s push straight down: crouch-brace or stagger 0.6 s | Glide round the streaks; brace on bridges |

**The fog (W5):** vision 25 m inside the Undercloud; no other effect. Lights carry 40 m.

### 2.6 Weather and resources

**Weather (SPEC table, world-map):** Clear 45, Overcast 10, Rain 10, Cloud fog 15 (islands under 800 m), Gale 20.

| State | Look and sound in the Sky | Play |
|---|---|---|
| Clear | Deep blue, cloud sea below, sun glitter on levistone; aeolian harps sing faintly in ruins | Normal; Galeharriers hunt |
| Overcast | Flat white light, the cloud sea rises 100 m | Skyrays drift lower, into W2 |
| Rain | Rain from above, waterfalls double, gossamer hangs heavy | Gossamer drifts pay ×2; footing normal (no slip on turf) |
| Cloud fog | Islands under 800 m inside cloud, vision 40 m | Hushwings hunt by day in the fog; Mistlarks call to find each other (follow them to updrafts) |
| Gale (Gale front) | Torn cloud racing, the Great Kite at full stretch, every cloth snapping | Gust lanes everywhere above 300 m, every 4 to 8 s (SPEC); flying mounts +50% stamina into the wind (SPEC); Skyrays breach in groups at dusk |

**Resources.**

| Resource | Where | Gather | Use |
|---|---|---|---|
| **Levistone** (signature, never farmable, SPEC rule) | Shards at the W1 cliff foot (lowland, 1 to 3 per node, respawn 2 game days); **keels** on island undersides (midland, 4 to 6 per node) and **cores** inside the Weeping Isle's caves (highland grade, 8 per node, glowing). Keels are reached from hanging ledges, chain walks under islands, a hovering Windmane or a glider landing on a keel shelf | Pickaxe; a struck shard floats up 1 m before it can be picked (you must grab it within 3 s or it drifts away up into the sky, gone) | Gliders, Skysilk, anchor boots, light armour, Cloud spire keystones (SPEC) |
| **Skysilk** (the material) | Gossamer drifts on W2 cedars and fog nets (2.4) | Hold Interact with any tool: winds silk onto a spindle, 4 s | Glider (levistone glider SPEC), Skysilk set (as linen weave), Gale arrows' fletching |
| Windflax, Cloudberry (SPEC crops) | Wild on W2 terraces | Hand | Linen, oil, jam, porridge (SPEC) |
| Cloudpine wood | Wind-sculpted cedars and junipers on every island | Axe; light, strong wood | Bows (+5% draw speed), gliders, bridge repairs |
| Ore | Iron and tin in the Weeping Isle's 3 caves (the deeper the better: the third cave holds a steel-grade vein when Hearth 4 exists, CF SPEC rule); copper on W1 ground | Pickaxe | Normal |
| Water | Waterfall pools; rain | Drink, waterskin | |
| Fish (SPEC) | Cloud minnow (pools), Skyfin (leaping from falls), the Aethercarp (Still Pool) | Fishing | Food |
| Puffwing feathers and eggs (SPEC) | Rookery, tamed Puffwings | | Glider, Skysilk |

**Building on islands (rules):** islands are static meshes (landscape can't float, FRP R7 SPEC). Claims on islands ≥ 120 m (SPEC). Small islands (< 120 m) can drift during the Turning Wind (2.15), which is why they can't be claimed.

### 2.7 Hidden sub-biome: the Undercloud (GAP-COOKED; site SPEC from world-map)

**What it is.** A cloud bank that never moves, 1 km across, sitting at 500 to 600 m over the sea at (+2,600, −9,600). Inside: six islands where the Kitewrights sent their dead, each tethered with kites.

**Finding it.** From W2's Kitefield, Mistlarks fly into the bank at one spot all day: an updraft hole (a column +6 m/s) that keeps a 20 m gap open in the cloud's top. Glide in through the gap; or drop from the Kitefield's lip and steer by the sound of kite cloth. The "secret" is only that it's in plain sight and nobody tells you.

**Getting out.** Two updraft columns inside (+10 m/s, top 150 m over the bank) and the Last Kite's line, a climbable silk-and-chain line that runs from the Last Kite island up to the Kitefield (a 260 m climb, stamina-free on a line, 3 m/s).

**Rooms (6 islands, 8 sites):**

| # | Island, site | What you see | Pays |
|---|---|---|---|
| U1 | Kite Barrow: **the Kite Graveyard** | Thousands of death-kites on lines tied to stakes, flying unseen into the fog above, their cloth flapping all round you. A path of stakes through them | Lesser shrine (the Barrow Shrine) |
| U2 | Tube Isle: **the Bone Tubes** | Racks of bone tubes, each sealed with wax, each holding a rolled kite of a dead person's colours | Lore ×4; Skysilk ×6 |
| U3 | Bonebell: **the Bonebell** | A bell cast of bronze and bone in a round frame; strike it and every kite in the bank shakes (a sound you hear through the fog for 300 m) | Ringing it wakes the Hushwing roost on U4 (a fight, deliberate) |
| U4 | Lantern Shelf: **the Hushwing Roost** | A shelf under an overhang lined with sleeping Hushwings by day (4 to 6), pellets of Puffwing and Galeharrier bones under them | Hushwing feathers; Hushwing day-tame window (they sleep) |
| U5 | The Drowned Garden: **the Gardener's Grave** | A garden island tilted 30°, half its keel spent, its beds sliding into fog; a round house where a family's bones sit at a table set for five, with five kites on the wall and a sixth frame unfinished | "Unfinished kite" (lore), Cloudberry heirloom seed (grows ×1.5) |
| U6 | The Last Kite: **the First Kite** | One huge old kite, 20 m, tied to a stone anchor carved with a serpent: the Kitewrights' first, flown for whoever bound Aeolith. Its line rises through the cloud (the climb out) | 1,500 Wind path XP; "Serpent anchor rubbing" (opens the Aeolith quest line, 2.13 Q6) |
| U7 | Kite Barrow edge: **the Newcomer's Kite** | A fresh kite of a newcomer's shirt and sticks tied with fiber, beside a camp of three bedrolls, one with a body | A full newcomer pack: a crude glider, food, a waterskin |
| U8 | Tube Isle underside: **the Keel Vault** | A cache cut into the keel: reached only by a ledge under the island | Levistone cores ×6, a Kitewright glaive (2.17) |

Creatures: Hushwings (roost), Sailbell blooms drifting through the fog (their violet glow is the only colour), Gallowshrikes on Kite Barrow (larders on the kite stakes).

### 2.8 Secrets (GAP-COOKED)

| Secret | Clue in the world | Reward |
|---|---|---|
| The Keel Vault (U8) | Chain bolts on Tube Isle's lip lead over the edge | 2.7 |
| The Weeping Isle's third cave | The middle waterfall: walk behind it (a ledge 2 m wide) | Steel-grade ore later; levistone cores now; a Hushwing nest |
| The Wind Rose door | At the Great Shrine, the eight carved winds; stand at the one the Great Kite points away from at noon and the floor plate turns | A cache: bronze ×10, a Gale arrow recipe |
| The Bonebell's echo | Ringing the Bonebell during the Turning Wind makes the death-kites rise out of the cloud, one column over the Kitefield | A one-time "Kitewright's last kite" talisman: glider stamina cost −20% |
| The Wheel's chain | The wind-wheel's drive chain trails off Wheel Isle toward the Ring; it's walkable for 180 m as a chain walk | A shortcut onto the Lid Gate from Wheel Isle |
| The Gulper's pearl lagoon | Diving in Gulper's Bight (it's 12 m deep) | A sunken Kitewright skiff with a bronze chest |

### 2.9 Creatures (9): roster, look, rows, drops (GAP-COOKED; 6 species and taming SPEC-as-drafted in FULL-GAME, 3 new)

FULL-GAME lists 6; the brief asks for about 9. The three new ones fill gaps the six leave: a ground pack for the islands (**Gallowshrike**), a night hunter (**Hushwing**) and a drifting hazard-creature (**Sailbell**). Tiers follow FULL-GAME 3.3: Hard aggressive = highland (HP 200, hit 32, stance 55), Medium = midland (120, 20, 35), passives and Easy = lowland (70, 12, 20). Every Sky creature resists gusts and takes no fall damage (CR: creatures resist their own biome's hazard).

**Roster and look**

| Creature | Tier, role, taming | Size | Silhouette (reads in the dark) | Look and palette | Where (day / night) |
|---|---|---|---|---|---|
| **Galeharrier** | Midland, aggressive flyer (pairs). Knockout and feed (bring it down). Medium (SPEC) | 2.8 m wingspan, 9 kg | Broad harpy-eagle wings with deep "fingers", a short wide tail, a raised double crest that flares before a stoop, feet and talons too big for its body | Slate-grey back, white belly barred black, a black hood with a white face-ring, crest feathers tipped pale teal, yellow feet, black talons 7 cm | W1 high islands, W3 by day, circling; roost on towers by night |
| **Skyray** | Highland, aggressive drifter. Knockout and feed, or Wind path magic. Hard (SPEC) | 9 m span, 1.2 t | A manta's diamond with long horn-lobes that unroll in front of the mouth, a whip tail with a barb; from below, a white belly with a dark mouth slot | Charcoal back flecked with pale spots, white belly with black blotches unique to each one (real manta trait), a violet bioluminescent rim along the wing edge at night | W2, W3 below island lips, drifting 10 to 30 m off the edge; lower in overcast |
| **Windmane** | Highland, neutral flying mount (herds of 2 to 4). Steal an egg. Hard (SPEC) | 1.8 m at the shoulder, 6 m fore-wingspan | A lean stag with **four feathered wings** (biplane, Microraptor): long fore-wings from the shoulders, shorter hind-wings from the thighs; swept antlers like a dead tree; a long mane that streams | Pale dun coat, white belly, mane and wing coverts silver-white, flight feathers barred grey, antlers bone-pale with dark tips | W3 Mane Ledges; graze W2 and W1 islands at dawn |
| **Cloudback** | Midland, neutral hauler (pods of 2 to 3). Offer levistone moss. Medium (SPEC) | 7 m long, floats at 2 m | A small whale-shape with a barnacled levistone hump (it floats on it), stubby side fins, a fluke | Grey-blue skin, white throat grooves, the hump crusted with glowing levistone barnacles | W2 lagoons and garden islands; sleeps over lagoons |
| **Mistlark** | Lowland, passive utility (flocks of 6 to 20). Offer seeds. Easy (SPEC) | 30 cm | A lark with long pointed wings and a fanned tail; flocks swirl like smoke | Soft grey and white with a mist-blue throat | Everywhere, always in updrafts by day; roost in cedars at night |
| **Puffwing** | Lowland, passive farm animal (flocks 3 to 6). Offer food. Easy (SPEC) | 0.9 m tall | A heavy, round, flightless ground bird (takahē-like) with oversized soft wings it uses to parachute off edges when scared | Moss-green back, blue breast, a red bill and shield, white glider-grade under-wing feathers | W1 turf islands and ground, W2 gardens; burrows at night |
| **Gallowshrike** (new) | Midland, aggressive pack hunter (2 to 4). Knockout and feed. Medium | 2.2 m tall, 70 kg | Upright and long-legged; a shrike's head with a black bandit mask and a hooked beak with a tomial "tooth"; short stiff wings spread to hop-glide between islands; a dagger claw on each inner toe (cassowary) | Ash-grey body, white throat, black mask and wings, rust-red under the tail, black dagger claws 12 cm. Up close: bare grey skin round the eyes, the beak stained dark | W1 Larder Isle, W2 edges, W5 Kite Barrow; hunt at dusk and dawn; sleep in larder trees |
| **Hushwing** (new) | Highland, aggressive night hunter (single). Knockout and feed. Hard | 5 m wingspan | Owl-broad round wings with soft fringed edges; a flat frogmouth head that splits open into a gape wider than the head; huge forward eyes | Bark-brown and moon-grey mottling (vanishes against cloud at night), a pale facial disc that flashes white as it strikes, a pink-black mouth lining; amber eyeshine | Night: everywhere above 700 m. Day: the Undercloud roost (U4), the Weeping Isle's third cave. In Cloud fog it hunts by day |
| **Sailbell** (new) | Lowland to midland, aggressive drifter (blooms of 3 to 8). Steal an egg (a bud). Easy | 2 m float, tentacles hang 12 m | A translucent gas-bell with a raised crest-sail and a curtain of fine tentacles; it drifts at 45° to the wind, left-handed bells to one side and right-handed to the other (man o' war) | Clear-blue float with a violet crest, pale tentacles with violet beads that glow at night. Beautiful from far, horrible up close: the curtain holds half-digested Mistlarks | W2, W3, W5, drifting across bridges and gaps; any time |

**Flyer rule (FRP R7: "walk map doesn't cover flight, so wildlife needs a rule"; GAP-COOKED).** Flyers use a **sky graph**: nodes at every updraft column, island centre and lip (from `world_pois.json`), edges where a straight line is clear. Flyers path node to node, steer locally with sphere traces (avoid islands and each other) and leash to a home node (150 m; minis chase 65 m, bosses 90 m, SPEC). Ground creatures on islands use a per-island walk grid (`sky_walk.json`, generated from the island meshes' top surfaces). Ground creatures never path off an island; Gallowshrikes hop-glide only along marked short edges (≤ 40 m, level or downhill).

**Living-world beats (GAP-COOKED):**
- Galeharriers rob: a Galeharrier stoops on a Puffwing, or on a player holding raw meat, and takes the meat (frigatebird). Feathers burst.
- Gallowshrikes impale kills in larder trees; a fresh kill on a thorn means a pack within 60 m.
- Skyrays breach at dusk in Gale weather: two to five leap 20 m out of the cloud sea and belly-flop back (mobula).
- Sailbell blooms drift on the wind: left-handed bells drift right of the wind, right-handed left. One bloom tells you where the next will cross a bridge.
- Windmanes defend herd and eggs together; a Hushwing takes a Windmane foal at night if no adult is within 30 m.
- Mistlarks circle in every updraft by day: a flock spinning up is a free lift.

**Attack rows (GAP-COOKED, combat.md 2.18 schema and bands).** Seconds. Damage × the tier hit. Damage only on visible contact through socket traces (talons, beak, wing edge, tail, tentacles). Projectiles ≤ 1,400 cm/s, never home, never fired inside 4 m. Every tell over 0.5 s has a rising sound cue. Packs attack freely. Knockback is the combat SPEC hit property; near an edge it kills.

| Creature | Attack | Wind-up | Delay hold | Active | Punish | Dmg × | Flags and tell |
|---|---|---|---|---|---|---|---|
| **Galeharrier** | Stoop | 0.80 (climbs, crest flares, rising scream; the line locks at the fold) | 0.2 | 0.30 | 1.5 grounded | 1.4 | Parryable; knockback 700 cm. Grounded 1.5 s after (the bring-down window, Scorchwing SPEC) |
| | Talon rake | 0.45 | 0 | 0.15 | 0.7 | 1.0 | Parryable. From a hover |
| | Wing buffet | 0.50 | 0 | 0.20 | 0.7 | 0.6 | Push 500 cm; wings drawn back and clapped forward |
| | Meat theft | 0.80 (stoop) | 0 | 0.20 | — | 0.3 | Only on a player with raw meat in hand: takes it and flies 60 m |
| **Skyray** | **Undertow** | 1.20 (hovers off the lip, horn-lobes unroll, a rising whoosh; dust and grass pulled toward it) | 0 | 1.50 | 1.4 | 0 | Pulls 500 cm/s toward its mouth in a 15 m, 40° cone; off the lip = the fall. Brace ×0.5, anchor boots none; a hit to its mouth with ≥ 30 stance cancels it. Never inside 4 m |
| | Wing slap | 0.70 | 0.2 | 0.20 | 0.9 | 1.3 | Push 600 cm; the near wing rises high and falls |
| | Barb strike | 0.60 | 0.3 | 0.15 | 0.8 | 1.2 | Bleed 30 (R2 status). Passes over you, tail arcs down; glint on the barb 0.2 s before |
| | Belly-flop | 1.00 (rises 15 m; its shadow grows on the ground) | 0 | 0.30 | 1.6 | 1.6 | Unblockable (crimson). Impact ring 6 m at 1,200 cm/s, `low` |
| | readsAction | Undertow within 0.4 s of a drink on an island lip | | | | | |
| **Windmane** (defending) | Antler charge | 0.90 (hoof scrape, head low) | 0 | 1.0 run | 1.3 | 1.5 | Knockback 900 cm. Wall or tree hit stuns it 2.5 s |
| | Hind-wing kick | 0.40 | 0 | 0.15 | 0.7 | 1.0 | `behindOnly`; hind-wings snap open, talons out |
| | Takeoff gust | 0.60 | 0 | 0.30 | 1.0 | 0.4 | Push 600 cm r 6 m; airborne 4 s, then returns with a charge |
| **Cloudback** (provoked) | Fluke slam | 0.90 (fluke rises high) | 0 | 0.25 | 1.2 | 1.4 | `low`, r 4 m behind it |
| | Body bump | 0.70 | 0 | 0.30 | 1.0 | 0.8 | Push 600 cm |
| | Spout | 0.80 (blowhole whistle) | 0 | 0.30 | 1.2 | 0 | Launches anyone on or over its back 6 m up |
| **Mistlark** | — | | | | | | Flees, never attacks |
| **Puffwing** | Hiss peck (cornered) | 0.50 | 0 | 0.12 | 1.0 | 0.4 | Parryable; then parachutes off the nearest edge |
| **Gallowshrike** | Hook bite | 0.45 | 0 | 0.12 | 0.8 | 1.0 | Parryable. Neck snakes back, beak opens |
| | Dagger kick | 0.55 | 0.2 | 0.15 | 0.8 | 1.3 | Parryable; bleed 25. Hops, one leg high: the claw glints |
| | Hop-glide pounce | 0.70 (wings spread, crouch) | 0 | 0.25 | 0.9 | 1.4 | From ≥ 3 m above or across a gap; knockback 400 cm |
| | **Impale** (grab) | 0.90 | 0 | 0.20 | 1.2 | 1.6 | Unblockable, dodge only. Seizes you and slams you onto the nearest thorn tree or stake within 6 m: bleed 60, pinned 1.5 s (mash −0.15 s per press). No tree near: a 400 cm throw |
| | Rattle | 0.60 | 0 | — | 0.8 | — | Calls packmates within 50 m, once per fight |
| **Hushwing** | **Silent swoop** | 0.90 (the only sound: a rising churr; the facial disc flashes white 0.2 s before contact) | 0.3 | 0.20 | 1.4 grounded | 1.5 | Parryable. Starts only on screen, or within 8 m with the churr giving direction (CAL off-screen rule) |
| | Gape snap | 0.60 (the head splits open) | 0.3 | 0.15 | 0.8 | 1.3 | Parryable; on the ground after a swoop |
| | **Wing smother** (grab) | 1.00 (wings mantle up and wide like a cloak) | 0 | 0.25 | 1.4 | 0.6 + 8/s | Unblockable, dodge only. Wraps you up to 3 s (mash to free); allies break it with 60 stance |
| | readsAction | Swoop with no hold within 0.4 s of a drink or a raised torch | | | | | |
| **Sailbell** | Curtain (contact) | — | — | always | — | 0.5 per touch, max 1 per s | Slowed ×0.6 (SPEC) 3 s. The tentacles are visible; passing under a bloom is the risk |
| | Lash | 0.60 (bell contracts, beads pulse brighter, a wet crackle) | 0 | 0.25 | 0.9 | 1.0 | Sweep r 8 m below and round it; Slowed 3 s |
| | Burst (death by fire) | — | — | 0.20 | — | 0.5 | Push 300 cm r 3 m (gas pop). Any other death: it deflates and its curtain falls, still stinging for 60 s (man o' war fact) |

**Numbers and drops**

| Creature | HP / hit / stance | Speed (cm/s) | Drops per kill | Tame use |
|---|---|---|---|---|
| Galeharrier | 120 / 20 / 35 | Fly 1,600, stoop 3,600 | Feathers ×3, raw meat ×1, harrier talon (20%) | Scout: circles where you point and pings creatures within 80 m to the owner |
| Skyray | 200 / 32 / 55 | Drift 400, dash 1,200 | Ray leather ×3, raw meat ×4, ray barb (25%) | Sky raft: up to 2 riders stand on its back; 900 cm/s, ceiling 200 m above ground, no combat while carrying |
| Windmane | 200 / 32 / 55 | Ground 1,100; flight SPEC (survival-crafting) | Hide ×3, raw meat ×3, mane hair ×2 | The flying mount (SPEC rules) |
| Cloudback | 120 / 20 / 35 | 900 (SPEC) | Blubber ×4, levistone barnacles ×2 | Floating hauler, 250 kg, gaps to 60 m (SPEC) |
| Mistlark | 70 / — / 20 | Fly 1,200 | Feathers ×1 | Flies to and circles every updraft column within 300 m: your eyes, no HUD |
| Puffwing | 70 / 12 / 20 | 450 | Raw meat ×2, glider feathers ×2 | Feathers and eggs (SPEC) |
| Gallowshrike | 120 / 20 / 35 | 750, hop-glide 40 m | Raw meat ×2, shrike claw ×1, feathers ×2 | Guard: impales intruders on the base's thorn stakes; its larder keeps meat ×2 longer |
| Hushwing | 200 / 32 / 55 | Fly 1,400, silent | Hush feathers ×3, raw meat ×2, Hushwing eye (20%) | Night guard: hunts any hostile creature entering the claim at night |
| Sailbell | 70 / 12 / 20 | Drifts with the wind | Sail membrane ×2, sting oil ×1 | Tethered at base: stings anything hostile within 10 m (Slowed), a living fence |

### 2.10 Mini-bosses (GAP-COOKED; names, fights and counters from FULL-GAME kept and made concrete)

Template (combat SPEC): 3,000 HP solo, +80% per extra player or tame, stance 90, 2 parries to break, 4 to 5 attacks and one counter mechanic, chase 65 m, wind-ups 0.65 to 0.9 s, punish 0.8 s, respawn 4 h. Trophies are talismans (survival-crafting SPEC).

#### Kyrra, the Tempest Harrier (roaming)

- **Who:** a Galeharrier hen grown to 9 m wingspan, her crest white with age, one talon a stump bound in old Kitewright chain. She hunts the bridges.
- **Route:** a 2.4 km spline loop over W3, Tollgate to Brink and back over Eyrie Rock (her rest, H11). Every 6 min she lands on the widest island on her route for 60 s to scan. She attacks any creature in her way (DD2 rule, fire-frost SPEC).
- **Counter (SPEC "fight her on wide islands; arrows to the wings ground her"):** each wing has part stance 250; ranged hits do ×2 to it. Break one wing: she crashes onto the nearest island, **grounded 10 s** (head at ground height, damage taken ×1.25), then takes off again. Break both and she fights on foot to the end, slower.

| Move | Wind-up | Hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Tempest stoop | 0.90 (climbs 40 m, crest flares, folds) | 0.2 | 0.3 | 1.6 grounded | 70, knockback 900 | Parryable at the end; line fixed at the fold. Landing dust ring 4 m (`low`, 25) |
| Snatch and drop (grab) | 0.90 | 0 | 0.2 | 1.4 | 30 + the fall | Unblockable. Carries you 20 m up and drops you; over a gap you need a glider or Featherfall. Allies free you with 120 stance on her body during the 3 s climb |
| Talon combo (3 hits) | 0.65 / 0.45 / 0.75 | 0.3 on the third | 0.15 each | 1.0 | 35 / 35 / 50 | From a hover. First two parryable |
| Gale wing | 0.80 | 0 | 0.4 | 0.9 | 20, push 1,000 | Forward gust 12 m, 50°; the edge kills |
| Screech | 0.70 | 0 | — | 0.8 | — | Calls 2 Galeharriers within 200 m, once per fight |

- **Trophy: Harrier plume** (talisman: +15% damage to flying and airborne targets). **Drops:** Kyrra feathers ×6, upgrade shards ×2 (SPEC).

#### The Bridge Sentinel

- **Who:** a Kitewright guardian 6 m tall, carved of white granite, with a halo of eight stone wind-vanes behind its head that turn as it walks and sing like an aeolian harp (heard 80 m off). It pries up deck stones to throw.
- **Where:** the standing stone bridge beside the Fallen Span, Tollgate to Span North (180 m long, 8 m wide). Lair: the Toll House (H10).
- **Counter (SPEC "lure its slam onto weak sections: it falls through to a lower island"):** the deck has **5 weak sections**, each 6 m long, shown by dull, flickering levistone in the joints and moss in the cracks. When its Vane slam lands on one, the section gives way: the Sentinel falls 35 m onto **Winch Isle** below, takes 25% max HP and **kneels 10 s** (riposte window). Glide or jump down (in glide reach; without a glider the Winch Isle chain ladder takes 20 s). It then fights on Winch Isle, with no deck to throw.

| Move | Wind-up | Hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Vane slam** | 1.00 (fists up, the vanes stop and point down) | 0.3 | 0.3 | 1.4 | 70 + ring 30 | Ring 5 m at 1,200 cm/s (`low`). On a weak section: the fall |
| Sweep | 0.75 | 0 | 0.3 | 0.9 | 50, push 700 | 200° arm sweep; the edge kills |
| Deck throw | 0.90 (pries a 2 m deck stone) | 0 | — | 0.8 | 60 | Projectile 1,200 cm/s on a gravity arc, its shadow shows; never inside 4 m; leaves a hole in the deck for 2 game hours |
| Grab and hurl | 0.85 | 0 | 0.2 | 1.2 | 40 + the throw | Unblockable; hurls you 12 m sideways |
| Vane spin | 0.80 (vanes spin up to a whine) | 0 | 1.0 | 1.0 | 15 per 0.25 s | Gust ring 6 m pushes outward 600 cm/s |

- **Trophy: Sentinel eye** (talisman: poise +15 while standing still or bracing). **Drops:** worked granite ×10, bronze ×6, upgrade shards ×2.

#### Old Gulper

- **Who:** a Cloudback grown to 22 m, mean and half-blind, its hump a reef of levistone barnacles, scarred with old harpoon wounds. It floats over Gulper's Bight (H9) and inhales anything near.
- **Counter (SPEC "feed it a lit explosive or a fire spell while it inhales"):** during Inhale its mouth is a target. A fire-element hit, a lit fire bomb or a thrown lit torch into the mouth makes it **choke**: a ring of smoke, it deflates and sinks onto the lagoon island for 10 s with its mouth open (lock point, damage taken ×1.5). Two chokes inside 60 s crack its hump: no more barnacle drops.

| Move | Wind-up | Hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Inhale** | 1.20 (mouth opens, throat grooves stretch, a rising roar; spray drawn in) | 0 | 2.0 | 1.2 | 0, then swallow | Pulls 500 cm/s in a 20 m cone. Reach the mouth = swallowed: 4 s inside (12 per s), then spat 15 m in a random direction (often off the island). The fire-feed window |
| Fluke slam | 0.90 | 0.3 | 0.3 | 1.2 | 60 | `low`, ring 5 m |
| Body roll | 0.80 | 0 | 0.8 | 1.0 | 50, push 800 | Rolls on its axis |
| Barnacle shed | 0.90 (hump shudders) | 0 | — | 0.8 | 40 each | 5 barnacles fall; each shadow grows 0.9 s before it lands (r 2 m) |
| Belch | 0.70 | 0 | 0.3 | 0.9 | 20, push 1,200 | Cone 10 m, 45°; only after a swallow |

- **Trophy: Gulper's pearl** (talisman: glide stamina cost −20%). **Drops:** blubber ×10, levistone ×12, upgrade shards ×2.

### 2.11 Major boss: Aeolith, the Sky Serpent (GAP-COOKED; FULL-GAME draft kept and made concrete)

**Who:** the feathered wind serpent the Kitewrights bound to circle the Flue and blow the Deepfarmers' spores back into the deep. The Kitewrights are gone; it still circles: guardian and prisoner at once.

**Arena: the Serpent's Ring** at (0, −8,800), 1,250 m (SPEC site).
- **Eight white platforms**, round, 14 to 18 m across, each with a low wind-stone (1.5 m tall, 4 m wide) on its outer rim, circle the Flue's axis at 28 m radius with 4 m gaps in phase 1 (a running jump crosses).
- **The centre is empty:** the Flue's warm updraft roars up through it. Aeolith coils through the centre and round the platforms.
- **The Lid Gate** (H16) is 50 m south, joined to platform 1 by a 50 m chain walk; the arena door is the serpent-jaw door. **Run-back stone** at the Lid Gate (SPEC 40 to 60 m). The **Wind altar** stands in the Lid Gate temple (DD SPEC).
- **Flying mounts land** 150 m out (survival-crafting SPEC). Gliders work in the arena (the Flue updraft is live), but any hit closes them (SPEC).
- **Falling:** 1,290 m of air below, down to the Fungal caverns' floor. Drop 40 m under the platforms and you're out of the fight: a glider rides the Flue updraft back up (+12 m/s, about 15 s); Featherfall lands you alive in the caverns; anyone else dies. No nets (SPEC).
- **The spore plumes:** three vents in the Flue's throat take turns to **exhale** Deepfarmer spores up through the centre: every 40 s one plume (6 m wide, glowing cyan) rises for 6 s. Tell 1.0 s: the cyan glow brightens below and a deep huff rises. Plumes hurt nobody; they are the stage for the Spore counter.

**Body.**
- 60 m long, 2.2 m thick at the chest. **One pair of huge feathered wings**, 34 m span, set behind the head like a cobra's hood when spread.
- **Head:** a long serpent skull with a **bone wind-mask**: a hard, duck-billed snout-plate (Ehecatl's buccal mask) with two flared nostril vents; behind it a jaw that opens 120° on rows of backward-hooked needle teeth. Amber eyes with slit pupils and a nictitating membrane that slides across before the gale breath.
- **Throat sac:** a red, inflatable pouch (frigatebird; Fūjin's wind-bag) that swells to twice the head's size before every breath attack: the main tell, readable across the arena.
- **Crest and train:** quetzal plumes along the spine, longest at the neck (3 m) and the tail fan (6 m).
- **Silhouette:** the hood of wings round the head, the endless coil, the tail fan. At night the throat sac glows dull red as it fills.
- **Palette:** scales jade-teal #2F8F7A to an ivory belly #E9E4D8; feathers ivory with jade and gold tips #C9A04A; throat sac red #B23A2A; mask bone #D8D2C4; spore-sickness shows as spreading grey #8A8A80. Crimson glints only on unblockable moves.

**Numbers (SPEC rules):** 12,000 HP solo, +80% per player or tame. **Stance 170.** 3 parries to break. Chase 90 m. Phase 2 at 50%, hidden phase 3 at 25% with artifacts (damage ×1.4). **Weak to Spore** (×1.5, SPEC). Group of 3: 10 to 15 min.
**Lock points:** head, chest (throat sac), left wing, right wing, tail fan. Each wing has part stance 350; breaking one grounds it 8 s on the nearest platform (wing dragging, fighting from a coil).

**The Gale Crown (Kushala lesson).** At full stance it wears a ring of wind: player projectiles entering within 3 m of it are deflected (you see arrows swirl away). A stance break, wing break or spore-weighting drops the Crown for 20 s. Melee and non-projectile spells ignore it.

**The opposite-element counter (SPEC: "spores weigh its wings and bring it down").**
- **Spore-weighted:** Spore buildup (threshold 400, SPEC major) procs it: grey spreads over its feathers, it crashes onto the nearest platform, **grounded 8 s** (head at ground height, damage taken ×1.25, Crown off).
- **The plume rule (any path):** passing through a live plume adds 150 spore buildup (it screams and veers). Its Crosswind dive runs a straight line through the centre: stand so the line crosses the live plume and it flies through it. Two passes = weighted. Spore path players fill it faster (SPEC planted-then-detonated works on it).

**Phase 1 (6 moves; one 3-hit combo, one grab):**

| Move | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Tail sweep** | 0.80 (tail fan raised, rattling) | 0 | 0.35 | 1.0 | 55, push 600 | `low`, across one platform; jump it or roll through. Tail socket trace |
| **Gale breath** | 1.20 (throat sac swells red, eyes membrane over, a rising organ-pipe roar) | 0 | 1.50 | 1.4 | 25 + push 1,500 cm/s | A 25 m, 35° cone of wind from the mask; the edge kills. Answers: get behind a wind-stone (blocks it), brace (×0.5), anchor boots or Skysilk 4 (no push); roll i-frames stop the push only during the roll. After it the sac is empty: the chest is open for 1.4 s (×1.3 damage) |
| **Coil strike** (3 hits) | 0.65 / 0.50 / 0.90 | 0.4 on the third | 0.15 / 0.15 / 0.30 | 1.2 | 45 / 45 / 70 | Bite, bite, then a coiling body crush across the platform (third unblockable, crimson). First two parryable |
| **Crosswind dive** | 1.00 (rises 30 m above the ring, wings fold to a hood; its shadow lines up) | 0 | 0.6 | 1.6 (loops back) | 80 | Unblockable. A straight dive across the ring through the centre, 6 m wide; the line locks at the fold. The plume counter uses this line |
| **Feather volley** | 1.00 (wings spread, every feather lifts) | 0 | — | 0.9 | 30 each | 12 quills in a fan at 1,400 cm/s from ≥ 6 m; each quill's line shows as it lifts; never homes. Wind-stones block them |
| **Snatch** (grab; tame-punish) | 1.00 | 0 | 0.20 | 1.4 | 40 + the drop | Unblockable, dodge only. **Targets the furthest tame first** (SPEC): a tame is carried over the Flue and dropped (it dies unless it flies). A player is carried 15 m up and dropped on another platform. Allies free the held one with 150 stance on the head during the 2.5 s carry |

Pattern rules (SPEC combat.md 2.17.1): bands close / mid / far; never the same move 3 times running; one ranged move in flight at a time in phase 1.

**Transition at 50%: the Unbinding** (unblockable, crimson). Aeolith coils round the Flue's axis and spins up a column of wind; the chains between the platforms snap.
- **Wind-up:** 1.4 s (it coils tight; sac and feathers swell). It takes 50% damage during it (SPEC).
- **Hit:** a ring of wind from the centre, 30 m at 1,200 cm/s, 60 damage and push 900 cm. Brace, hide behind a wind-stone, or roll the front.
- **Arena change:** the platforms **drift apart** and orbit on their own: gaps open and close between 4 m and 14 m on a 20 s cycle while the ring slowly turns. **Updraft columns** (+10 m/s, 15 m tall) open in the gaps: jump in and you're thrown up and across (airborne 1.5 s; anyone can use them).

**Phase 2 (2 new delayed attacks, 1 new combo, 1 arena change, 1 delayed variant):**

| Move | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Updraft Inhale** (new delayed 1; "pulls players into the air", SPEC) | 1.6 (rears in the centre, wings beat inward, a deep rising roar; dust and spores spiral up) | up to 0.6 | 1.2 | 1.4 | 0, then 70 | Every player within 18 m is lifted 8 m and held 1.2 s (air dash, Plunge, bows and spells work). At the top a **tail lash** crosses the lift (70, crimson glint 0.2 s before). Brace, anchor boots or Skysilk 4 stop the lift. A Plunge from the top onto its head does ×1.5 |
| **Downburst** (new delayed 2) | 1.8 (hovers 15 m over one platform, wings fold, a falling hiss) | up to 0.6 | 0.4 | 1.3 | 90 | Unblockable. A 6 m column of air slams the platform; final glint 0.2 s before. Ring 8 m at 1,200 cm/s (`low`, 30) |
| **Spiral combo** (new combo) | dive 1.0, then 0.55 / 0.80 | 0.3 on the last | as rows | 1.6 | 80 / 45 / 60 | Crosswind dive, a body coil across the platform it lands on, a tail sweep; then it rises |
| **Held gale** (delayed variant) | 1.20 + up to 0.6 | 0.6 | 1.50 | 1.4 | 25 + push | The breath waits with the sac full and swings to the side with the most players |
| Snatch, Coil strike, Feather volley (two may be in flight), Tail sweep | as phase 1 | | | | | Idle gaps 0.3 to 0.9 s (SPEC). Snatch now drops players into gaps |

**Hidden phase 3 at 25% (artifacts only; SPEC flag): the Last Circle.** "It flies only, and you fight on its back" (SPEC).
- **It never lands.** It circles 20 to 40 m out, using Feather volley, Held gale from the air and Downburst.
- **Low passes:** every 20 s it passes low across the ring, its back 2 m above the platforms for 1.5 s; a rising wing-roar tells it 1.2 s ahead. **Press Interact within 4 m of its back to grab on.** Up to 4 players ride.
- **On its back** (a 40 m spine): run the crest. Three **crest vents** (behind the head, mid-back, tail root) are parts with 900 HP each, its only damage points in phase 3 besides ranged hits on its wings. Each vent broken: 8% max HP and a 2 s stagger.
- **Barrel roll:** every 12 s on its back it rolls, told by a 1.2 s whistle and its crest flattening. **Hold Guard to brace** (stamina 30 per second for the 2.0 s roll). No brace or no stamina: you're thrown. Over a platform you land (fall damage applies); over the Flue you need a glider or Featherfall.
- **All three vents broken:** it crashes onto the largest platform for 10 s (riposte window); if it lives, it rises and the vents regrow at 50% (once).
- **Damage ×1.4** (SPEC).

**Drops:**
- Boss core "core_aeolith" (Hearth tech, SPEC). Gale essence ×8 (path material).
- **Boss part: Gale plume** (SPEC: air dodges push nearby enemies back): an air dash or airborne dodge pushes enemies within 4 m back 400 cm. Chance `bosses.partChance` (0.25, BUILT).
- First kill (named, flies to the killer): **"The Wind Mask of Aeolith"**, trophy and talisman: Momentum gain +20%; glide stamina cost 0 in winds over 10 m/s.
- Aeolith plumes ×6 (Skysilk extra, Gale arrows).

**Altar:** the Wind subclass unlocks here (BUILT `WRAltar.cpp` pattern). **Music:** SKY_Boss_Aeolith P1 / P2 / P3 (2.12).

### 2.12 Voices and music (GAP-COOKED; music tempo and key SPEC from FULL-GAME 3.3)

**Creature voices.** Fire-frost 3.5 rules (SPEC): four layers per attack (tell, strike, hit, pain) plus idle, aggro and death; at least 3 real animal calls and one material layer per species; tells mixed 3 dB over the bed and rising in pitch.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Galeharrier | Harpy eagle scream, peregrine "cack", red-tailed hawk cry | Wind tearing over feathers in the stoop | The scream that rises as it folds |
| Skyray | Humpback whale call pitched up (FULL-GAME SPEC example), beluga chirps, fin whale pulse | A huge in-breath through a hollow (the Undertow whoosh) | The rising whoosh before the pull |
| Windmane | Red deer stag roar, wapiti bugle, mute swan wingbeat hum | Feathers thrumming (four wings) | The bugle that echoes between islands at dawn |
| Cloudback | Humpback grunt, walrus groan, elephant rumble | Blowhole spout | The spout's whistle |
| Mistlark | Skylark song, horned lark tinkle, swift scream (flock) | Wingbeats in a swirl | A flock's rising chorus in an updraft |
| Puffwing | Takahē honk, goose hiss, kākāpō boom | Soft wing-flap parachute | The boom at dusk from the rookery |
| Gallowshrike | Shrike harsh rattle, cassowary boom (sub), secretary bird croak | Claw tick on stone | The rattle: you hear the pack before you see it |
| Hushwing | Nightjar churr, frogmouth "oom" (low), barn owl screech (pain only) | None in flight (silent wings, by design); a wet jaw-open click | The churr, the only warning |
| Sailbell | Bowed glass harmonics, jellyfish pulse (hydrophone), cicada buzz pitched down | Wet tentacle crackle | The bloom's hum on the wind |
| Kyrra | Harpy eagle scream −5 semitones, peregrine cack, red-tailed hawk | Sail-tear in the stoop; old chain clink on her stump | The chain clink overhead |
| Bridge Sentinel | (construct) granite grind, rockfall pitched down | Its eight vanes as an aeolian harp chord | The harp chord heard 80 m off |
| Old Gulper | Humpback moan −1 octave, walrus bellow, elephant seal gurgle | A huge bellows breath | The inhale roar |
| Aeolith | Serpent hiss stretched long, golden eagle scream −1 octave, humpback long call, howler monkey roar (sub) | Organ-pipe wind through its nostril vents; the gale breath recorded through a real pipe | The Unbinding: a hiss rising into a whale call over the Flue's roar |

**Music: Sky islands, 100 BPM, F Lydian (SPEC).** Flutes, choir, open fifths, light percussion (SPEC). Added: **paired flutes in hocket** (two siku-style panpipes or two wooden flutes sharing one melody, panned left and right: the two wings), a recorded **aeolian harp** drone in F, harp ostinatos, a women's and boys' choir in open fifths, frame drum and finger cymbals (light). The raised fourth (B natural) is the Lydian lift: used on every phrase peak. Rules SPEC (fire-frost 3.6): always audible, combat layer bar-synced in the same key and tempo, duck ≤ 4 dB, −18 LUFS (calm −23, boss −15), true peak −1 dBTP. No silences except the Flue floor (world-map SPEC, Spore side).

| Cue | Length | Use | Feel |
|---|---|---|---|
| SKY_Explore_A / B / C | 3 to 4 min each | Day. A: W1, W2. B: W3. C: W4 | A: hocketed flutes over harp, open and sunny. B: the same melody thinner, with chains and wind harp, a minor colour in the middle section. C: choir in fifths over the harp drone, slow, high, the Flue's roar mixed under |
| SKY_Night_A / B | 3 min | Night | A single flute over aeolian harp and a soft choir pad; star-bright celesta |
| SKY_Undercloud | 4 min | W5 | Muted flutes, distant women's voices, kite-cloth flutter as rhythm; the most intimate piece |
| SKY_Gale (stem) | layer | Gale weather, on top of the explore piece | Fast harp tremolo and frame drum |
| SKY_Combat (stems for A, B, C) | loops | Combat | Frame drum in driving 8ths, flutes in quick hocket, choir stabs |
| SKY_Mini (×3 variations) | 2 min loops | Kyrra, Sentinel, Gulper | Kyrra: fast flutes and drum; Sentinel: low choir and the harp chord; Gulper: low brass and whale-like horn |
| SKY_Boss_Aeolith P1 / P2 / P3 | 3 sections | The boss | P1: full choir and flutes in F Lydian, heroic. P2: the key shifts to D Lydian, drums double, flutes split into wild hocket. P3: everything drops to choir, harp and a single high flute, with wind roar as part of the score; it builds back to full for the last 10% |
| SKY_Event_TurningWind | layer | The Turning Wind | A canon: the main theme played against itself backwards (the wind turning) |
| SKY_Camp | 2 min | Spawn camps | Solo flute and harp, warm |
| Stingers | 3 to 8 s | Shrine lit, Undercloud found, mini down, Aeolith down, first glide over 300 m | Choir swell to the Lydian fourth |

### 2.13 Shrines and path master (RAVEN-PLANNED exist; who and where GAP-COOKED; shrine rules SPEC from fire-frost 3.1)

| Shrine | Sub-zone, at | Guard |
|---|---|---|
| **Great Shrine of the Still Pool** | W1 Still Pool Isle (560 m), above camp 2's lift | None (path master). The Aethercarp pool (SPEC); the Wind Rose (eight carved winds round the floor, 2.8 secret) |
| Gardens Shrine | W2 Moss Stair | A Gallowshrike pack (3) and a Sailbell bloom drifting through |
| Toll Shrine | W3 Last Toll | 2 Galeharriers and a Windmane herd gone aggressive while the shrine is dark |
| Rose Shrine | W4 Rose Temple | A Skyray off the lip and a Gallowshrike pair |
| Barrow Shrine | W5 Kite Barrow (hidden) | 2 Hushwings (one roosting, one hunting) |

**Path master: Yara Kesh, the one-winged glider-maker.**

*Look:*
- **Body and age:** a woman of about 30, lean and strong like a climber: long legs, defined shoulders and arms, a narrow waist. Naturally beautiful: high cheekbones, a wide mouth, sun-browned skin with freckles across the nose.
- **Hair:** black, long, worn in a single heavy braid that the wind never leaves alone.
- **Her missing wing:** her left arm ends just below the elbow. She wears a brass-and-levistone hook-hand she uses to tension glider lines. Her own glider has only one wing left; it hangs over her workbench.
- **Clothes:** a cropped, fitted Skysilk wrap-top that leaves her midriff and arms bare, loose pale trousers tied at the ankle, a long teal scarf (the Skysilk scarf), bare feet on the stone.
- **Wind marks:** pale spiral tattoos drifting on both forearms and up her shoulders: the Wind body change at 75 points, a living preview. Her hair and scarf stir even indoors.
- **Eyes:** dark brown with the pale teal limbal ring (SPEC body change).

*Voice:* warm, quick, teasing; she laughs at falls but not at deaths.
*Where:* the Great Shrine's open workshop: glider frames on every wall, silk on spindles, the Aethercarp pool below her bench.
*How she talks:* short lines within 6 m, no camera grab, no cutscene; quests go to the Journal (SPEC).

| # | Quest | Task | Pays |
|---|---|---|---|
| 1 | Feathers and stone | Bring 6 levistone shards and 10 Puffwing glider feathers | 800 Wind XP; recipe **Featherdown draught** |
| 2 | First flight | Glide from the First Isle to Terrace Isle without touching ground (any glider) | 1,000 XP; crude glider repair kit recipe |
| 3 | Read the wind | Ride three different updraft columns to their tops (Mistlarks show them) | 1,200 XP; vista reveal of W1 and W2 |
| 4 | Silk | Bring 8 Skysilk from the gossamer drifts | 1,500 XP; recipe **levistone glider** (SPEC recipe, unlocked here) |
| 5 | The bridges | Light the Toll Shrine, then cross the whole Broken Way from Tollgate to Brink without a lift or a mount | 2,000 XP; recipe **Gale arrows** |
| 6 | The first kite | Bring her the Serpent anchor rubbing from the Undercloud (U6) | 2,500 XP; she shows you the Lid Gate on your map |
| 7 | Grounded | Show her a mini trophy from each of Kyrra, the Sentinel and Old Gulper (third-mini SPEC rule) | 3,000 XP; 2 upgrade shards; the Kitewright glaive recipe |

### 2.14 The story the land tells (GAP-COOKED; environment only, DD SPEC)

No staged scenes, no narrator. It is all kites, stone, bones and placement.

1. **The Kitewrights lived in the sky.** Round houses on every island, terraces, lifts down to the sea, bridges on chains. Their waymark is the lung ta wind-pole (world-map SPEC).
2. **They spoke to their dead with kites.** The Kite Field (H8), the bone tubes, the Undercloud's graveyard (W5), the family table set for five with five kites on the wall (U5). A crude new kite on the Kite Field: a newcomer tried it too.
3. **The spores came up the Flue.** The Spore Scar (H14): rotted gardens, gardeners face-down in cloth masks, mycelium crusting island undersides nearest the Flue.
4. **They fought the Deepfarmers.** The Paper Titan on Titan's Crown with Kitewright spears through it and a kite line round its neck (world-map SPEC). Their champion fell down the Flue (world-map SPEC: winged skeleton in mycelium on the cavern floor).
5. **They built the Wind Wheel to blow the spores down,** and it broke: its vanes lie across Vane Rocks, its chain trails toward the Ring (H13).
6. **Then they bound the serpent.** The Serpent Door's carvings show a serpent coiled round a wheel. The First Kite in the Undercloud (U6) is tied to an anchor carved with the serpent, flown for whoever bound it. Aeolith still circles the Flue where the wheel stood.
7. **And they left, or fell.** The Shard Fall (H1): an island that fell. The Drowned Garden (U5): an island sinking as its levistone tires. The Lift Wreck (H2): two who fell holding each other. Nobody says why the stone tires.
8. **The Twelve Peoples (SPEC):** the Kitewrights' stair on the Crown breaks at 700 m like every other; their pillar on the Ring carries a carved wind-pole.

**The Wind–Spore war in one image each side:** on the Sky side, the Paper Titan speared on the highest island; on the Spore side (world-map SPEC), the Kitewright champion wrapped in mycelium at the Flue's floor. Each lies dead on the other's ground, killed by its opposite (Raven's weakness rule, told by the land).

### 2.15 Event: the Turning Wind (GAP-COOKED; frame SPEC from fire-frost 3.1 and world-map)

One game day (60 real min), about once every 3 real days (server setting, SPEC), never touching spawn safe zones, paying levistone at triple rate (SPEC).

| Time | What happens |
|---|---|
| −5 min (world sign) | A huge lenticular cap forms over the Flue; Kelvin-Helmholtz wave billows roll across the whole sky; **every wind-pole's cloth swings to point the same way, against the normal wind**; the Great Kite turns slowly round its anchor; the Kitewright pillar on the Crown's Ring glows pale teal (world-map rule). Music: the event layer fades in |
| 0 to 60 min | **The wind reverses:** the world wind vector for B1 swings 180° (from the west). Updraft columns move to the islands' other sides; ridge lift on the Windward Cliffs stops; the gust lanes flip (the drop side on most lips changes: careful) |
| | **Small islands drift:** every island under 120 m (unclaimable by rule, SPEC) leaves its spot and drifts 40 to 120 m along a fixed spline, at 1 m/s, then holds. Anything standing on one rides it (based movement) |
| | **Cloud bridges:** six bridges of dense cloud (4 m wide, walkable, white, lit from inside) form along the event's splines, linking islands that are normally only a glide apart: First Isle → Highstep, Flax Isle → Weeping Isle, Kitefield → Undercloud top gap, Tollgate → Last Toll, Last Toll → Rose Temple, Wheel Isle → Lid Gate. A newcomer without a glider can walk to every sub-zone. Cloud bridges thin at the edges: a 1.0 s tell (the edge goes grey and frays) before any section fades at the end |
| | **Shards fall:** drifting islands brush each other and shed levistone: 20 shard falls across the biome, each a cluster of 6 to 10 shards that drift up if not grabbed in 3 s (2.6 rule) |
| | **Creatures:** Skyrays migrate along the cloud bridges' undersides in a line (a hunting window: they're spread out, and they ambush bridge walkers from below); Cloudback pods cross the sky; Galeharriers stoop on everything on the bridges; Sailbell blooms pile up on the reversed wind's lee |
| | **The Undercloud opens wide:** the top gap widens to 80 m; its kites stream up through it in a column you can see from the Kitefield |
| +60 min | The wind swings back over 2 min; islands drift home; bridges fade from the ends inward with the 1.0 s fray tell (stand on one at the end and you fall) |

Pays: levistone ×3 everywhere (SPEC), shard falls, and the Bonebell secret (2.8).

### 2.16 Invasions: Wind and Spore through the Flue (3.3 rule SPEC; Sky side GAP-COOKED)

Rules SPEC (FULL-GAME 3.3, world-map): server setting, 5 min warning, 8 to 14 creatures plus one mini, march on the nearest claim or shrine within 1.5 km, hold a shrine 30 min, pay both biomes' materials and 2,000 path XP to everyone who dealt 10%. **Route SPEC (world-map):** Spore invaders climb the Flue walls and spill onto the nearest islands over mycelium bridges grown for the event; Wind invaders dive down the Flue.

| | Wind invading the Fungal caverns | Spore invading the Sky islands |
|---|---|---|
| Sign (5 min) | The Flue's updraft stops dead and reverses: a howling downdraft; Mistlarks flee the shaft | The Flue exhales a continuous cyan column for 5 min; mycelium bridges start growing up from the rim toward W4 islands (visible, 1 m per second) |
| Who comes | 4 Galeharriers, 3 Gallowshrikes (hop-gliding down the shaft ledges), 2 Skyrays, a Sailbell bloom (4), and **Kyrra** | Fungal lane's roster (that lane names them) plus its mini; they arrive on the Spore Scar, Wheel Isle and the Lid Gate |
| Target | The nearest Fungal claim or shrine within 1.5 km of the Flue floor | The Rose Shrine or a W4 claim |
| Sky's defence edge | — | The Flue's wind: gusts on the mycelium bridges push invaders off; Wind path knockback kills them over the edge |

### 2.17 Gear from this biome (SPEC from the other cooks, plus GAP-COOKED items)

| Item | Slot / kind | Station | Inputs | Effect | Tag |
|---|---|---|---|---|---|
| **Skysilk set** (4 pieces) | Head, chest, hands, legs; Light; poise 30 | Armorer, Hearth 4 | Steel plate 6, levistone 8, Puffwing feathers 12, linen 8 (SPEC) | 2: fall damage −50%. 4: gusts never push you; glide 6 s from any jump (fall 300 cm/s, forward 900) (SPEC) | SPEC (survival-crafting) |
| Skysilk look | | | | Pale fitted silks that ripple in any wind, a feathered half-cape, levistone clasps that float a finger off the cloth (SPEC); levistone shards orbiting the shoulders, a long silk scarf (hero SPEC). Cut to the body: open collar and bare arms (hero "open windows": arms, collarbones, face), fitted at the waist. White, pale teal #9FD6CF, silver | SPEC (both cooks merged) |
| Anchor boots | Legs | Armorer 3 | Iron 6, hide 4, levistone 1 (SPEC) | Gusts don't push you (SPEC); also Skyray Undertow and Aeolith's lifts | SPEC + RAISE |
| Crude glider / levistone glider | Back | Workbench 2 / 3 | World-map SPEC recipes | World-map SPEC stats | SPEC |
| **Featherdown draught** (early pass) | Consumable, 10 real min | Alchemy 1 (or cooking pot) | Puffwing down ×3, Cloudberry ×2, water | Fall damage ×0.5 (SPEC effect) | SPEC effect / GAP-COOKED recipe |
| **Gale arrows** | Arrows ×10 | Workbench 2 | Cloudpine ×2, Galeharrier feathers ×2, levistone ×1 | Arrow damage ×1.0; knockback 400 cm (the knockback hit property, any path); 10% faster flight | GAP-COOKED |
| **Hush arrows** | Arrows ×10 | Workbench 2 | Cloudpine ×2, Hush feathers ×1, flint ×2 | Silent (no flight sound; creatures don't hear where it came from); sneak hits ×1.15 | GAP-COOKED |
| **Kitewright glaive** | Spear class (combat 2.4 spear moveset) | Workbench 3 (recipe from Yara Q7 or U8) | Cloudpine ×4, levistone ×3, bronze ×6, Skysilk ×2 | Bronze-tier glaive, 30% lighter (stamina −10% per swing); its running heavy is a leaping cut: +2 m reach airborne. Accepts Wind infusion (DD) | GAP-COOKED |
| **Sailbell lantern** | Off-hand light | Workbench 1 | Sail membrane ×1, sting oil ×1, linen ×1 | A violet hanging light, 12 m radius, never blows out in gusts (torches do in Gale) | GAP-COOKED |
| Trophy talismans | Talisman | — | Harrier plume, Sentinel eye, Gulper's pearl (2.10) | As listed | GAP-COOKED (SPEC frame) |
| Boss part, first kill | | | Gale plume (SPEC), Wind Mask of Aeolith (2.11) | | |

### 2.18 The Wind path (FULL-GAME 4.2 row)

#### 2.18.1 What stays SPEC

From ST (Raven):
- Root passive **Featherfall**: no fall damage (main path only).
- Resource **Momentum**: builds while airborne or sprinting (DD).
- Every node name, cost and text below, including the capstones Sky lord, Tornado and Storm volley.

From the other cooks: Wildbrand numbers (combat 2.12: Wind cut, Whirlwind, Plunge, Wind arrow); spell numbers (combat 2.13: Gust, Air dash, Vortex, Tornado, Sky lord, Storm volley); knockback (combat 2.11); Momentum is a **spender** (combat 2.12 SPEC); body change (hero 2.6).

#### 2.18.2 Momentum (resource numbers GAP-COOKED)

| Key | Value | Why |
|---|---|---|
| `momentumMax` | 100 | Same scale as Heat, Chill and Charge |
| `momentumAirborne` | +14 per second while airborne (jumping, falling, gliding, lifted) | "Builds while airborne" (DD) |
| `momentumSprint` | +8 per second while sprinting above 650 cm/s | "or sprinting" (DD) |
| `momentumAirHit` | +4 per hit landed while you're airborne | Rewards fighting in the air (the theme) |
| `momentumDecayDelay` | 2.5 s after the last gain | A pause to land and spend |
| `momentumDecayPerSecond` | 10 | A full bar lasts 10 s on the ground |
| `momentumHighAt` | 60 | |
| `momentumKnockbackAtHigh` | Knockback distance ×1.25 at ≥ 60 | The path's identity: the wind gets stronger |
| `momentumStaminaRegenAtHigh` | Stamina regen +15% at ≥ 60 | Keeps you moving |
| `momentumCapacityPerRank` | +15 | Same as the other paths |
| Bar colour | Pale teal #9FD6CF; streaks of air run along it at ≥ 60 | Path trim (hero SPEC) |
| No dead button | Any Wind ability with less Momentum than its cost still fires at half effect (half damage, half push) for its stamina cost; never a greyed slot in combat | GAP-COOKED (Aaron's "no dead states") |

#### 2.18.3 The tree: every node, with gates and numbers

Gates follow `fire.json`'s rule: a node's gate is the points spent in its branch on the nodes before it. Costs and texts are SPEC (ST); kinds, gates and numbers are GAP-COOKED. Ability numbers marked (combat) are SPEC from `combat.md`.

**Root passive: Featherfall** (main path only). No fall damage from any height (SPEC). It also halves glider stamina cost and removes fall damage when a glider closes under 20 m (world-map SPEC, glider table).

**Trunk (10 points)**

| Node | Cost | Kind | Gate | Numbers |
|---|---|---|---|---|
| Gust | 3 pts | ability (spell, starter) | trunk | (combat) Push 600 cm in a 40° cone, 6 dmg, stance ×1.5, cd 4. Cast 0.30 s walking. Costs 8 Momentum. Airborne: it also pushes **you** 300 cm the other way (a mid-air reposition) |
| Momentum capacity | 3 ranks | stat | trunk | +15 max Momentum per rank |
| Off balance | 2 ranks | stat | trunk | +10% stance damage per rank on all Wind hits and knockback wall-slams |
| Wind cut | 2 pts | ability (Wildbrand) | trunk | (combat) 15 Momentum, cd 2.5, 0.24 / 0.18 / 0.36, ×1.1 + wave ×0.8 (900 cm at 2,000 cm/s), knockback |

**Skyborne (30 points): Glide, Air dash, Plunge, Lift, Aerial mastery, Sky lord**

| Node | Cost | Kind | Gate | Numbers |
|---|---|---|---|---|
| Glide | 5 pts | ability (passive move) | 0 | (SPEC "glide from any height") Jump again while airborne at **any height** to glide with no glider item: forward 950 cm/s, sink 230 cm/s, costs 4 Momentum per second (then 3 stamina per second when empty); free in updrafts. If you carry a levistone glider, you use whichever stats are better. Any hit closes it (SPEC glider rule) |
| Air dash | 5 pts | ability (spell, dash) | 5 | (combat) Dash in the air, 400 cm in 0.18 s, i-frames for the travel, cd 3, 10 Momentum. One per airtime (resets on landing, or on entering an updraft) |
| Plunge | 4 pts | ability (Wildbrand) | 10 | (combat) 20 Momentum, cd 4, airborne only: 0.20 / until landing / 0.45; ×1.5 + landing ring ×0.8 (300 cm); fall height × 0.2 per m extra, max ×2; hyperarmor |
| Lift | 3 ranks | stat | 14 | Jump height +12% per rank (+36%); at rank 3, a second jump (a small air hop, 2 m) once per airtime |
| Aerial mastery | 3 ranks | stat | 17 | +6% damage per rank while you're airborne (weapons, bows, spells, Plunge) |
| **Sky lord** | 10 pts | capstone (transform) | 20 | (combat) Hover 8 s; all attacks from the air. Numbers: 60 Momentum, cd 90, transform cast 1.0 s rooted with hyperarmor. You rise to 6 m above ground (or stay at your height if airborne) and hover, moving at 600 cm/s in any direction. Light and heavy become air strikes (light: a 2-hit wind slash, ×1.0; heavy: a diving strike ×1.6 that returns you to hover height); dodge = a free air dash; Gust and Wind cut have no cost. Fall damage from Sky lord's end is always zero |

**Cyclone (30 points): Whirlwind, Vortex, Eye of the storm, Debris, Lingering gale, Tornado**

| Node | Cost | Kind | Gate | Numbers |
|---|---|---|---|---|
| Whirlwind | 5 pts | ability (Wildbrand) | 0 | (combat) Greatsword, scythe, axe, great hammer; 30 Momentum, cd 8, 0.30 / 1.2 (4 spins) / 0.50; ×0.7 per spin, 280 cm; hold to keep spinning 0.3 s per 8 Momentum; hyperarmor |
| Vortex | 5 pts | ability (spell, pull) | 5 | (combat) Pull 360°, radius 600 cm, pulls 400 cm, cd 14, 20 Momentum. Cast while spinning: the pull adds to the spin (the ST text "pull enemies into your spin") |
| Eye of the storm | 4 pts | passive | 10 | (combat SPEC behaviour) −20% damage taken while spinning (Whirlwind, Tornado's centre, Vortex cast) |
| Debris | 3 ranks | stat | 14 | Each Whirlwind spin throws 1 stone per rank outward at 1,400 cm/s in a fixed pattern (front, back, sides): 8 dmg, stance ×1, 900 cm range |
| Lingering gale | 3 pts | passive | 17 | Spins leave a gale on the ground behind you: radius 300 cm, 4 s, 5 dps, pushes enemies outward 100 cm/s; lowland creatures can't walk into it |
| **Tornado** | 10 pts | capstone (summon) | 20 | (combat) A tornado that moves where you aim at 400 cm/s, 10 dps, lifts lowland creatures, 10 s, cd 60, 50 Momentum. Steer: hold Aim and move the camera; release to let it go straight. Radius 250 cm; midland creatures pulled 150 cm/s toward its centre; bosses unaffected except for damage |

**Windblade (30 points): Wind arrow, Swift draw, Piercing gale, Crosswind, Tailwind shot, Storm volley**

| Node | Cost | Kind | Gate | Numbers |
|---|---|---|---|---|
| Wind arrow | 5 pts | ability (Wildbrand) | 0 | (combat) Bow, crossbow; 15 Momentum, cd 2.0, draw 0.35 / release / 0.30; arrow ×1.6; knockback; pierces 1 target |
| Swift draw | 5 ranks | stat | 5 | −6% draw and throw time per rank (−30% at 5) |
| Piercing gale | 4 pts | passive | 10 | All your arrows, bolts and thrown weapons pierce 1 more target (Wind arrow pierces 2) |
| Crosswind | 3 ranks | stat | 14 | +10% projectile speed and +5% projectile damage per rank |
| Tailwind shot | 3 pts | passive | 17 | Fully drawn shots and Wind arrows push you 250 cm backward over 0.15 s (airborne: 400 cm, and it doesn't use your air dash). Hold Sprint while releasing to shoot without the push |
| **Storm volley** | 10 pts | capstone (strike) | 20 | (combat) 30 arrows over a radius of 600 cm, 8 dmg each; cd 50; 60 Momentum; draw 1.0 s rooted with hyperarmor; arrows land over 1.5 s from 0.9 s after release (the rain of arrows is visible falling) |

Totals: trunk 10, branches 30 each, 100 per path (SPEC).

#### 2.18.4 Combos with the paths built before R7 (CB names SPEC; numbers GAP-COOKED)

R7 adds 8 pairs (FRP R7 SPEC): Wind with Fire, Frost, Lightning, Blood & curse, Nature, Necromancy, Sun & sand, Tide. Wind + Spore comes in R8; Wind + Arcane and Wind + Void in their rounds.

| Pair | 25/25 (CB) | Numbers | 50/50 (CB) | Numbers |
|---|---|---|---|---|
| Fire + Wind | Firestorm: gusts carry flames and spread burns | Gust and Vortex add burn 40; a burning target pushed by your Wind ability spreads burn 25 to enemies within 300 cm of where it stops | Ember rain: Storm volley rains fire | Every volley arrow adds burn 20 and leaves a 100 cm fire patch 3 s |
| Wind + Frost | Ice arrows: wind arrows freeze on hit | Wind arrow adds frost 60 | Avalanche plunge: Plunge attacks shatter frozen enemies | Plunge's landing ring Shatters any Frozen target in it (Shatter SPEC burst) and adds frost 50 to the rest |
| Lightning + Wind | Tailcharge: sprinting and gliding build Charge | +6 Charge per second while sprinting or gliding | Jet stream: Bolt dash works in mid-air and chains with air dashes | Bolt dash usable airborne; an air dash within 0.5 s after a Bolt dash costs nothing and resets Bolt dash once |
| Wind + Blood & curse | Bleeding arrows: wind arrows cause bleed | Wind arrow adds bleed 50 | Hex storm: Storm volley curses every target | Every target hit by ≥ 3 volley arrows gets curse 100 |
| Nature + Wind | Leaf glide: glide farther in forests | Inside the Forest biome or within 20 m of 5+ trees: glide sink −40% | Windborne seeds: plant turrets anywhere your arrows land | Every 4th arrow that lands plants a Sprout turret (half HP, 10 s), max 2 at once |
| Wind + Necromancy | Soul wind: Souls are pulled to you from farther away | Soul pickup radius ×2.5 | Reaper's cyclone: Scythe sweep becomes a spinning attack | Scythe sweep spins 360° twice (×0.8 each), with Whirlwind's hyperarmor |
| Sun & sand + Wind | Sand blast: gusts blind enemies they push | Gust and Vortex add blind 40 | Sunshot: wind arrows become beams of light | Wind arrow becomes an instant beam (hitscan, 2,500 cm), ×1.6, blind 50, pierces all in line |
| Wind + Tide | Sea spray: gusts soak enemies | Gust and Vortex add soak 40 | Hurricane eye: allies inside your tornado heal | Allies within the Tornado's radius heal 6 per second |

#### 2.18.5 Body change (SPEC from hero.md 2.6, restated)

| Stage | Mask (B, pale spiral tattoos that drift, UV pan 0.02/s) | Eyes | Hair and cloth |
|---|---|---|---|
| 25 points | Spirals on the forearms | Pale teal limbal ring (faint) | WPO stir always on (0.5 Hz sine, 1 cm) |
| 50 points | Arms and back | Ring stronger | Same, plus scarves and loose cloth lift 10° |
| 75 points | Full: arms, back, chest, neck | Ring clear at 3 m | Same |
| In combat at ≥ 60 Momentum (GAP-COOKED, RAISE) | The spirals' drift speeds ×4 and brighten 1 nit | — | Hair streams as if in a 10 m/s wind |

The open windows (arms, collarbones, face, SPEC) keep the marks visible.

### 2.19 Aerial combat (R7 shared system; GAP-COOKED within combat SPEC)

| Rule | Value |
|---|---|
| Airborne attacks (anyone) | Light while airborne = a jump slash (×1.0, stance ×1.2); heavy while airborne = a jump heavy (×1.3, stance ×2.5, lands you) (ER jump attacks, combat SPEC style). Bows draw at ×0.8 speed in the air |
| Gliding | Bows and thrown items only; any hit closes the glider (SPEC) |
| On the Windmane | Rider attacks per combat 2.19 mounted rules (SPEC): side swings, Bolt / Lance / Chain spells only, bows. **Talon pass** (Windmane's own attack, RT while flying): a low fly-by rake ×1.4, stance ×3, cd 4. Hits on a rider: 25% chance per hit over 40 damage to knock the rider off (the fall is real). Flying mounts land 150 m from boss arenas (SPEC) |
| Flyers vs riders | Galeharriers, Hushwings and Kyrra target riders and gliders first when they're airborne in their range: a stoop on a glider always closes it |
| Knockback off an edge | **Non-Sky creatures** (tames, invaders, creatures of other biomes) knocked off and falling over 18 m die, credited to whoever hit or pushed them in the last 6 s (loot rule SPEC; their loot drops where they land). **Sky ground creatures** (Gallowshrike, Puffwing) spread their wings and parachute away: out of the fight, no loot, back on their home island after 2 min (they resist their biome's hazard, CR SPEC). Flyers are pushed but don't fall. Minis and bosses recover onto the nearest platform or island after 3 s (never ledge-killed) |
| PvP | Edge kills by knockback count as kills (Raven's PvP rules, SPEC); safe zones unchanged |

---

## 3. Plan: building it in Raven's repo

Raven's patterns (SPEC):
- Data first, every number in `Data/*.json` as a first draft.
- Abilities are `UWRGameplayAbility` classes registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority throughout.
- Generated data comes from `Tools/terrain/*.mjs`: edit the script, not the json.

This lane reuses frameworks other cooks plan and doesn't rebuild them:
- **fire-frost:** `AWRShrine`, `AWRPathMaster`, `UWRQuestComponent`, `UWRBiomeEventSubsystem`, `world_pois.json`, `quests.json`, roaming-mini routes, `UWRBodyChangeComponent`, the attack-row runner.
- **world-map:** `world_layout.json` (the Flue, updrafts, camps, the Kitewright lifts), `UWRWeatherSubsystem` (wind vector, Gale), `CUSTOM_Glide` in the CMC, `AWRUpdraftVolume`.
- **biome-lightning:** `AWRGustZone` (cliff-path gust volume with whistle tell, brace and crouch multipliers): reused for the Sky's gust lanes with Sky numbers.
- **combat:** knockback, Wildbrand and spell shapes, boss parts and lock points, grab framework.
- **survival-crafting:** items, recipes, sets, the Windmane flight rules and mount code, Cloudback carry, Updraft lift machine.

### 3.1 Data rows

| File | Change |
|---|---|
| **New** `Data/paths/wind.json` | Same shape as `fire.json`: `id "wind"`, `name "Wind"`, `biome "sky_islands"`, `starter "gust"`, `resource {id "momentum", name "Momentum"}`, `rootPassive {id "featherfall", name "Featherfall", text}` (ST text), trunk `gust, momentum_capacity, off_balance, wind_cut`; branches `skyborne` (`glide, air_dash, plunge, lift, aerial_mastery, sky_lord`), `cyclone` (`whirlwind, vortex, eye_of_the_storm, debris, lingering_gale, tornado`), `windblade` (`wind_arrow, swift_draw, piercing_gale, crosswind, tailwind_shot, storm_volley`), each with `kind`, `cost` or `ranks`, ST `text` and `gate` (0, 5, 10, 14, 17, 20) |
| `Data/tuning.json` `resources` | `momentumMax 100, momentumAirborne 14, momentumSprint 8, momentumSprintSpeed 650, momentumAirHit 4, momentumDecayDelay 2.5, momentumDecayPerSecond 10, momentumHighAt 60, momentumKnockbackAtHigh 1.25, momentumStaminaRegenAtHigh 0.15, momentumCapacityPerRank 15, halfEffectWhenShort true` |
| `tuning.json` `spells` | Blocks `gust`, `windCut`, `glide`, `airDash`, `plunge`, `skyLord`, `whirlwind`, `vortex`, `tornado`, `windArrow`, `stormVolley` with the 2.18.3 numbers (fields mirror `fireBolt`, `flameSlash`, `frostNova`: damage or multiplier, push, radius, cone, speed, seconds, cooldown, momentum) |
| `tuning.json` `nodeEffects` | `wind.momentum_capacity {maxMomentum: 15}`, `wind.off_balance {stanceDamage: 0.10}`, `wind.lift {jumpHeight: 0.12, doubleJumpAtRank: 3, doubleJumpHeight: 200}`, `wind.aerial_mastery {airDamage: 0.06}`, `wind.debris {debrisPerSpin: 1, debrisDamage: 8, debrisSpeed: 1400, debrisRange: 900}`, `wind.swift_draw {drawTime: -0.06}`, `wind.crosswind {projectileSpeed: 0.10, projectileDamage: 0.05}` |
| `tuning.json` `passives` | `featherfallGliderStaminaMult 0.5`, `eyeOfTheStormDamageTaken 0.8`, `lingeringGale {radius 300, seconds 4, dps 5, push 100, blockTier "lowland"}`, `piercingGaleExtra 1`, `tailwindShot {push 250, airPush 400, seconds 0.15, suppressWithSprint true}` |
| `tuning.json` `glide` (world-map block) | Add `pathGlide {forward 950, sink 230, momentumPerSecond 4, staminaWhenEmpty 3}`; `skysilk4 {seconds 6, fall 300, forward 900}` |
| `tuning.json` `hazards` | `gustLane {push 1200, seconds 0.8, tell 1.5, interval [8,15], galeInterval [4,8], braceMult 0.5, creditSeconds 6}` (shared `AWRGustZone`), `leeRotor {glideShove 600, shoveSeconds 0.6, sinkAdd 300, sinkSeconds 2, footPush 400, footSeconds 0.5, tell 1.0}`, `sinkingStone {tell 1.0, dip 100, sinkSpeed 400, sinkSeconds 6, riseSeconds 20}`, `downburst {radius 1500, glideSinkAdd 600, tell 1.2, bridgePushSeconds 0.8, staggerIfUnbraced 0.6}`, `fallOut {creditSeconds 6}`, `undercloudSight 2500` |
| `tuning.json` `creatureAI` | Every 2.9 row in the combat.md schema. New kinds: `stoop, talon_rake, wing_buffet, meat_theft, undertow, wing_slap, barb_strike, belly_flop, antler_charge, hindwing_kick, takeoff_gust, fluke_slam, body_bump, spout, hiss_peck, hook_bite, dagger_kick, hop_glide_pounce, impale, rattle, silent_swoop, gape_snap, wing_smother, curtain, lash, gas_burst` |
| `tuning.json` `flyers` (new) | `skyGraphLeash 15000, steerTraceRadius 300, soarAltitudeBand [1000, 4000], groundedAfterStoop 1.5, offScreenTellRadius 800` |
| `tuning.json` `bosses` | `kyrra {wingPartStance 250, rangedWingMult 2, groundSeconds 10, groundDamageTaken 1.25, scanEvery 360, scanSeconds 60, callCount 2}`, `bridgeSentinel {weakSections 5, fallDamagePct 0.25, kneelSeconds 10, deckHoleSeconds 7200}`, `oldGulper {inhalePull 500, inhaleCone 40, swallowSeconds 4, swallowDps 12, spitDistance 1500, chokeSeconds 10, chokeDamageTaken 1.5, crackChokes 2, crackWindow 60}`, `aeolith {stance 170, wingPartStance 350, wingGroundSeconds 8, crownRadius 300, crownOffSeconds 20, sporeThreshold 400, plumeBuildup 150, plumeEvery 40, plumeSeconds 6, plumeTell 1.0, weightedSeconds 8, weightedDamageTaken 1.25, ringRadius 2800, p2GapRange [400, 1400], p2GapCycle 20, columnLift 1000, lowPassEvery 20, lowPassSeconds 1.5, ventHealth 900, ventDamagePct 0.08, rollEvery 12, rollTell 1.2, rollSeconds 2.0, braceStaminaPerSecond 30, crashSeconds 10, ventRegrowPct 0.5}` and each move's row (2.11) |
| `tuning.json` `events` | `turningWind {warnSeconds 300, windSwingDeg 180, swingSeconds 120, driftMaxSize 12000, driftSpeed 100, cloudBridges 6, bridgeWidth 400, frayTell 1.0, shardFalls 20, undercloudGapWide 8000, payout 3}` |
| `Data/creatures.json` | 9 new rows (`galeharrier, skyray, windmane, cloudback, mistlark, puffwing, gallowshrike, hushwing, sailbell`): `biome "sky_islands"`, tier, behavior, archetype (`flyer`, `drifter` new, `pack_hunter`, `herd_charger`, `swarm`), role, `element "wind"`, `resists ["gust", "fall"]`, taming and food (2.9), `attacks`, `activity` (Hushwing `"nocturnal"`, plus `"fog"` day-table tag), `locomotion` (new field: `ground`, `fly`, `drift`, `float`), `loot` (2.9), mesh, materials, `anims` (one clip per attack plus soar, glide, hover, land, grounded for flyers). Mini rows `kyrra` (`roaming`, `route`), `bridge_sentinel`, `old_gulper`. Major `aeolith`. Tame-use fields: `scoutMark` (Galeharrier), `skyRaft {riders 2, speed 900, ceiling 20000}` (Skyray), `showUpdrafts 30000` (Mistlark), `larderGuard` (Gallowshrike), `nightGuard` (Hushwing), `tetherSting 1000` (Sailbell) |
| `Data/spawn_regions.json` | `biome "sky_islands"`, `bandMode "altitude"` (world-map SPEC); regions per sub-zone W1 to W5 (polygons plus altitude ranges from `world_pois.json`), day, night and `weather: "fog"` tables (2.9 "Where") |
| `Data/items.json` | `levistone, levistone_core, skysilk, cloudpine, gale_essence, galeharrier_feather, harrier_talon, ray_leather, ray_barb, mane_hair, blubber, levistone_barnacle, shrike_claw, hush_feather, hushwing_eye, sail_membrane, sting_oil, kyrra_feather, worked_granite, aeolith_plume, core_aeolith, part_gale_plume, trophy_harrier_plume, trophy_sentinel_eye, trophy_gulpers_pearl, trophy_wind_mask, featherdown_draught, gale_arrow, hush_arrow, kitewright_glaive, sailbell_lantern`, lore items (Kite of the given, Bone tube, Kitewright bowl, Wheelwright's plan, Gardener's mask, Unfinished kite, Serpent anchor rubbing, Kitewright's last kite) |
| `Data/recipes.json` | Featherdown draught, Gale arrows, Hush arrows, Kitewright glaive, Sailbell lantern, crude glider repair kit |
| `Data/combos.json` (new, DATA_SCHEMAS; frame from biome-lightning) | The 8 Wind pairs with 2.18.4 numbers |
| `Data/world_pois.json` | Sky: sub-zone polygons with altitude ranges; **`skyIslands`** rows (id, name, mesh set, centre x y z, yaw, size, sub-zone, claimable bool, `drift` spline for islands under 120 m, `walkGrid` ref); landmarks L1 to L6; the 16 hand sites and 8 Undercloud sites; kit rules (2.4 table: kind, sub-zones, count, variants, spacing 150, max gap 125 m on routes and glide lines); shrines; run-back stone; Kyrra's route spline; the Sentinel's deck with 5 weak-section slots; gust lanes (spline + drop side); sinking-stone paths; downburst sites; lee-rotor volumes; Turning Wind cloud-bridge splines; **`skyGraph`** nodes and edges |
| **New** `Data/sky_walk.json` (generated by `Tools/terrain/sky_walk.mjs`) | Per-island walk grids from the island meshes' top surfaces (the R7 "walk map doesn't cover flight" answer for ground creatures) |
| `Data/quests.json` | Yara's 7 quests (2.13): task types bring, glide-without-touching (new: a flag cleared on ground contact), ride-updrafts (new), light-shrine-then-cross (new: route volumes), bring-lore, trophy |
| `Data/weather.json` | `sky_islands` row SPEC from world-map; add `gale.gustLaneInterval [4, 8]` and the Turning Wind override hook (`windOverrideDeg`) |
| `Data/Audio/sounds.json` | `creatures.<id>` sets for 9 creatures and 4 bosses (2.12); pools `explore.sky_islands.day/night`, `combat.sky_islands`, `mini.sky_islands` ×3, `boss.aeolith` (3 sections), `sub.undercloud`, `event.turning_wind`, `camp.sky_islands`, `layer.weather.gale.sky_islands`; beds per sub-zone; `gust.whistle` (rising, 1.5 s), `rotor.whump`, `stone.groan`, `downburst.hiss`, `plume.huff`, `aeolian.harp` (ruins, positional), `chain.sing`, `kite.flutter` (W5 bed) |
| `Data/fx.json` | Gust wave (grass flatten front + torn cloud ribbon along the lane), lung ta cloth snap (cloth sim trigger), levistone keel glow (emissive pulse; flicker for tired stones), waterfall-to-mist fade (Niagara), gossamer sheets (translucent cloth cards with sun glints), virga streaks, rotor wisps, Sailbell tentacle glow, Skyray Undertow inflow, stoop speed lines on feathers, Aeolith throat-sac fill (material param), Gale Crown swirl (deflect), spore plume column, Unbinding wind ring, cloud bridges (volumetric cards, fray edge), lenticular cap and K-H billows (sky cards for the event sign) |
| `Data/server_presets.json` | Nothing new (events, invasions and weather settings are SPEC from other lanes) |

### 3.2 C++ (Raven's patterns)

| Where | Add or change |
|---|---|
| `WRAttributeSet.h/.cpp` | `Momentum`, `MaxMomentum` with OnRep, mirroring `Heat`/`MaxHeat` |
| `WRPathComponent.cpp` | Momentum tick (server, 0.1 s): airborne from the CMC (`IsFalling()` or `CUSTOM_Glide` or lifted), sprint speed check, air-hit gains, decay after the delay, the ≥ 60 bonuses, capacity from `nodeEffects`. Featherfall as the Wind root passive (main only): sets the fall-damage exempt flag `combat.md` already reads, halves glider stamina. The half-effect rule for spenders short of cost |
| `WRAbilities.h/.cpp` | New `UWRGameplayAbility` classes, registered in `ClassForNode`: `UWRGA_Gust` (cone push via the knockback hit property; self-push when airborne), `UWRGA_WindCut` (Wildbrand + player wind-wave projectile), `UWRGA_Glide` (passive: grants the glide-from-any-height input to the CMC), `UWRGA_AirDash` (airborne root-motion dash with i-frame tag; one per airtime, reset on landing or updraft entry), `UWRGA_Plunge` (airborne Wildbrand; landing ring through ApplyHit; height scaling), `UWRGA_SkyLord` (transform: `CUSTOM_Hover` mode, swaps light and heavy montages to air strikes, free Gust and Wind cut via a cost tag), `UWRGA_Whirlwind` (held spin, hit every spin, Debris projectiles, Lingering gale volume spawn), `UWRGA_Vortex` (pull; adds to an active spin), `UWRGA_Tornado` (summon actor `AWRTornado`: steerable by aim, lift and pull by tier), `UWRGA_WindArrow` (Wildbrand on the bow; pierce and knockback), `UWRGA_StormVolley` (strike shape; spawns 30 arrow projectiles falling from a sky point over 1.5 s) |
| `WRCombatLibrary.cpp` | Knockback hit property (combat SPEC) with the Momentum ×1.25 at high; **edge-fall credit** (`FWRFallCredit`: last hitter or pusher within 6 s gets the kill and loot when a non-Sky creature falls > 18 m; Sky ground creatures parachute instead); passives through `HasNode`: Off balance, Aerial mastery (airborne damage), Eye of the storm (spinning tag), Piercing gale, Crosswind, Tailwind shot (self-push on full-draw release unless Sprint held). Gale plume part: airborne dodge pushes. Combo hooks for the 8 pairs from `combos.json` |
| `WRCharacterMovementComponent.cpp` | `CUSTOM_Glide` (world-map) gets the path-glide stats and Momentum drain; air jump from Lift rank 3; `CUSTOM_Hover` for Sky lord (600 cm/s in 3D, height cap 6 m over ground); brace (hold crouch) multiplies external pushes ×0.5; anchor boots and Skysilk 4 set push immunity; external impulses from gusts, rotors, downbursts, Undertow and Aeolith's lifts through one `AddWindImpulse(FVector, float Seconds, FName Source)` so every immunity is checked in one place; based movement on drifting islands (UE's moving-base support) |
| **New** `WRSkyIslands.h/.cpp` | `AWRSkyIsland` (Nanite static mesh set spawned from `world_pois.skyIslands`; claimable flag read by `WRBuilding`; keel glow material params); `UWRIslandDriftComponent` (server-driven spline move at 100 cm/s with replicated movement, only on islands under 120 m, only in the Turning Wind); `AWRSinkingStone` (weight trigger, 1.0 s tell multicast, sink and rise timeline); `AWRDownburst` (cylinder: glide sink add, bridge push down via `AddWindImpulse`, virga FX); `AWRLeeRotor` (volume gated by the wind vector: active only on the lee side); `AWRCloudBridge` (event-spawned walkable collision along a spline, fray-fade from the ends with the 1.0 s tell); `AWRLevistoneShard` (pickup that floats up and despawns after 3 s unless grabbed) |
| `WRStormHazards.cpp` (biome-lightning's `AWRGustZone`) | Data-driven numbers per biome (`hazards.gustLane`); the lane's drop side from the spline; visual front (grass flatten, cloud ribbon) multicast at tell start; Gale interval from `UWRWeatherSubsystem` |
| **New** `WRSkyNav.h/.cpp` | `UWRSkyNavSubsystem`: loads `skyGraph`, A* over nodes, `FindAirPath(From, To)`; local steering helper (sphere traces, separation); `UWRIslandWalk` reads `sky_walk.json` grids for ground creatures and exposes `IsWalkable`, `RandomPointOn(island)` to `WRWildlife` |
| `WRCreature.cpp` | Rows through the shared attack-row runner. New: flyer locomotion (soar, circle, hover, stoop on a locked line, grounded window) using `UWRSkyNavSubsystem`; Skyray Undertow (pull volume with brace and boot checks; a mouth part that cancels on 30 stance); Sailbell drift (velocity = wind × 0.25 rotated ±45° by handedness, curtain overlap damage with a per-target 1 s gate, death-curtain actor for 60 s); Gallowshrike impale (grab framework; nearest actor tagged `Thorn` within 600 cm; pin timer with mash), hop-glide edges from `skyGraph` short links, larder decoration on kill; Hushwing silent mode (no flight loop sound; churr tell; on-screen or 8 m start check), wing smother grab; Cloudback float and spout launch; Puffwing and Gallowshrike edge parachute (out of fight, return in 120 s); Galeharrier meat theft (reads the held item) |
| `WRWildlife.cpp` | Altitude band spawning; fog weather table; living-world beats (Skyray breaching at dusk in Gale, Mistlark updraft circling from `updrafts`, Windmane foal predation, Gallowshrike larders) |
| `WRBoss.cpp/.h` | Rows for **`kyrra`** (route component from fire-frost, air-scan landings, wing parts with the ranged ×2 rule and grounded state, snatch-and-drop grab carrying the target on a socket), **`bridge_sentinel`** (deck weak-section actors `AWRWeakSpan` that break when a Vane slam's impact point is inside them; the fall to Winch Isle as a scripted root-motion drop to a target point, then a kneel state; deck-stone throw that spawns a hole decal plus a collision cut for 2 h), **`old_gulper`** (inhale volume, swallow state: hide the player inside, damage tick, spit with a random yaw impulse; the mouth target that accepts a fire-element hit or a thrown lit item to trigger Choke; hump crack after 2 chokes), **`aeolith`** (spline flight round and through the ring; coil states; throat-sac material param driving the tell; Gale breath as a push cone through `AddWindImpulse`, blocked by actors tagged `WindStone`; Gale Crown projectile deflect; wing parts; spore buildup and the plume volumes `AWRSporePlume` on a 40 s rotation with the 1.0 s tell; Spore-weighted grounded state; the Unbinding transition; `AWRAeolithPlatform` ×8 with a drift timeline in phase 2 and updraft-column volumes in the gaps; Updraft Inhale lifting via `AddWindImpulse` and a hold; phase 3 low passes with an Interact grab-on prompt and **`UWRBossRideComponent`** (attach riders to the spine spline, crest vent parts with HP, barrel-roll brace check against stamina, throw-off impulse)) |
| `WRBiomeEvents.cpp` | The Turning Wind timeline (2.15): world sign (sky cards, pillar, pole cloth override), the wind override on `UWRWeatherSubsystem`, island drift start and return, cloud-bridge spawn and fade, shard falls, Undercloud gap widen, creature changes, ×3 levistone payout |
| `WRRaids.cpp` | The Flue route for Wind and Spore invasions (world-map SPEC): Wind invaders path down the shaft; Spore invaders' mycelium bridges as `AWRCloudBridge`-style spline bridges with a growth timeline |
| `WRAltar.cpp` | Wind altar row in the Lid Gate temple |
| `WRUIHud.cpp` | The Momentum bar (pale teal, air streaks at ≥ 60); wind arrow on the compass while gliding (world-map SPEC) |
| Path picker / main menu | Wind appears in the path picker (FULL-GAME 2); spawns at the three Sky camps |
| `WRBodyChange.cpp` (hero lane) | Wind row (2.18.5): spiral mask B, limbal ring, WPO stir; the ≥ 60 Momentum drift speed-up |
| Mount code (survival-crafting lane) | Windmane flight (SPEC numbers), Talon pass attack, rider knock-off chance, forced landing within 150 m of arenas |

### 3.3 Assets and the art route

**Route (SPEC from fire-frost 4.3):**
1. A concept, picked by Aaron.
2. A Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. Real clips retargeted from Fab animal packs or Truebones mocap (procedural clips only as idle fallbacks).
6. Each creature shown standing at scale beside the hero before any behaviour is wired.

| Asset | Route and clip source |
|---|---|
| Galeharrier | Harpy eagle / raptor base; crest feathers as cards with a flare morph. Clips: bird-of-prey pack (soar, flap, hover, stoop fold, strike, grounded flap, land, perch) |
| Kyrra | From the Galeharrier at 9 m span, white crest, chained stump (chain on physics bones) |
| Skyray | Manta base; horn-lobes as bones with an unroll curve; tail barb; emissive rim mask. Clips: ray swim cycles retimed for air (glide, bank, hover-flap, breach, belly-flop) |
| Windmane | Red deer base with four wing rigs added (fore at the scapula, hind at the femur); feather cards. Clips: deer pack on the ground; flight from a large-bird pack on the wings, mixed on the legs-tucked flight pose (the fire-frost extra-limb method) |
| Cloudback / Old Gulper | Whale base, scaled; levistone barnacle hump as a separate mesh with emissive; Gulper scarred, 22 m. Clips: whale swim retimed for floating; inhale and spout as additive poses on real clips |
| Mistlark | Lark base; flock as Niagara-driven instanced birds with a real flap cycle (boids from data) |
| Puffwing | Takahē / goose base, oversized wings. Clips: goose or chicken pack plus a parachute pose blend |
| Gallowshrike | New concept: shrike head on a cassowary-proportioned body, short stiff wings. Clips: ostrich/cassowary or raptor-dinosaur pack (walk, run, kick, peck, hop, hop-glide with wings out), bite from a heron strike |
| Hushwing | New concept: owl wings, frogmouth head with a split-jaw rig (the gape opens wider than the head: two jaw bones plus a cheek stretch morph). Clips: owl pack (silent glide, swoop, mantle, perch), the gape as a separate jaw track |
| Sailbell | New concept: man o' war float with a crest sail; tentacles as a strand groom on AnimDynamics chains with a per-strand glow mask; translucent material with subsurface |
| Bridge Sentinel | Concept (Kitewright granite guardian, eight-vane halo); Hunyuan base, Blender finish; the vanes as separate meshes spinning on a bone. Clips: golem/stone-giant pack, or a heavy humanoid set on the Sentinel rig |
| Aeolith | New concept (the 2.11 description): serpent body as a spline-driven rig of 60 segments for coils, with a wyvern-wing pair on the shoulders; feather cards; throat sac as a morph plus a material param; bone wind-mask a separate hard mesh. Clips: snake locomotion procedural on the spline (real snake reference for the coil timing), wing beats and attacks from a dragon or wyvern pack retargeted to the wing and head chains. The body spline is the only procedural motion; every attack is a keyed clip |
| Yara Kesh | MetaHuman preset; left-forearm prosthetic hook (Blender, brass and levistone); Wind marks at 75 from the body-change mask; Skysilk wrap outfit from the R0 outfit work |
| **Islands** | **Kitbash, not primitives:** 6 hero island meshes (the First Isle, Weeping Isle, Kitefield, Titan's Crown, Lid Gate, Tollgate) built in Blender from Megascans cliff, rock and boulder scans (Fab, free for UE), with a sculpted top surface and a levistone keel underside; plus 12 modular rock-and-keel pieces that make the other 32 islands by combination, rotation and scale (each unique in silhouette). Nanite on. Top surfaces get the biome's ground layers by world-aligned blend. Waterfalls as Niagara ribbons with the mist fade |
| Kitewright kit | Round house (4 variants), round temple, serpent-jaw door, toll tower, winch hut, lift tower and cage, chain-hung bridge deck (spline-built, deck stones as instances), wind-poles with cloth (cloth sim), kite frames and kites (cloth), bone tubes, wind-wheel hub and vanes, wind roses, wind harps |
| The Paper Titan | From the fungal lane's titan concept if it exists, dried: paper-grey material with torn gills; Kitewright spears |
| The Great Kite | 60 m cloth kite with a chain line spline; the kite's yaw follows the wind vector (it's the world's weathervane) |
| Materials | Limestone and granite tops, levistone (pale blue mica glint, emissive keel glow, tired flicker), wind-combed grass (foliage WPO tuned to the world wind), gossamer (translucent), cloud bridges (volumetric), spore crust |
| Kit prefabs | 10 kit kinds (2.4) as Packed Level Actors, 2 to 4 variants each |

### 3.4 Sound and music

- **Voices:** 13 sets (9 creatures, 4 bosses) built from the 2.12 recipes from CC0 / CC-BY sources (Freesound, the Sonniss GDC bundles, Kenney) through Raven's `Tools/audio` pipeline. About 8 files per creature, 12 or more per boss (SPEC counts from fire-frost 4.4). Hushwing has no flight loop by design.
- **Hazard sounds:** gust whistle (rising, 1.5 s), rotor whump, sinking-stone groan, downburst hiss, plume huff, chain singing, positional aeolian harps on ruins.
- **Music:** pick the 2.12 cue sheet from libraries (about 20 cues counting stems); the aeolian harp drone recorded or licensed as a real instrument; update `sounds.json` pools and `Docs/CREDITS.md`.
- **Beds:** 5 sub-zone beds (W1 surf and wind, W2 flax and falls, W3 chains, W4 the Flue's roar, W5 kite flutter and drip); wind rises with altitude (world-map SPEC).

### 3.5 Map dressing

- **Ground:** the B1 east terrain (Windward Cliffs) comes from world-map's terrain step for B1 (SPEC): heights, sea cliffs, the Flue's rim. This lane adds the W1 ground polygon and layers to `world_pois.json`.
- **Islands** are runtime-spawned from `world_pois.skyIslands` (Raven's dressing pattern: no hand edits to the map; say before touching a map, maps can't be merged). World Partition streaming treats each island as a spatially loaded actor.
- `Tools/terrain/dressing.mjs` places the 16 hand sites, 8 Undercloud sites and 6 landmarks at their coordinates, fills the 37 kit POIs by sub-zone with Poisson spacing ≥ 150 m, then walks every route **including glide lines between islands** and adds a kit POI wherever a point is more than 125 m from any POI (SPEC rule).
- **New** `Tools/terrain/sky_walk.mjs` bakes per-island walk grids (`sky_walk.json`) and the **sky graph** (nodes at island centres, lips and updraft columns; edges by clear line), and checks the island rules (≥ 40 m, ≤ 220 m gaps, every island reachable by glide from a higher island or a column). It reports failures; it doesn't test gameplay.
- Wind-pole waymarks every 150 m along bridges and island lips (world-map SPEC).
- **Hazard cells:** gust lanes (splines with drop side), sinking-stone paths, downburst sites, lee-rotor volumes, Turning Wind drift splines and cloud-bridge splines.
- The Undercloud is a cloud-bank volume (fog density, vision 25 m) over its 6 islands, streamed with them.

### 3.6 Build order inside this scope

Each step is one cook into Aaron's build; Aaron tests each. Gates: compile, cook, package.

| # | Step | Why here |
|---|---|---|
| 0 | **Data frame:** `wind.json`, the Momentum attribute, `combos.json` rows, `AddWindImpulse` in the CMC with all immunity checks, the edge-fall credit | Everything below reads these |
| 1 | **Proof of one: the First Isle with a Galeharrier.** One hero island mesh spawned from data over the Windward Cliffs, the camp 2 Kitewright lift up to it, its gust lane with the visible tell, and a Galeharrier pair end to end (concept, Aaron's pick, mesh, real raptor clips, fused voice, sky-graph flight, stoop with the fold tell and grounded window, drops) | Proves the riskiest new things together on one spot: a floating island from data, flyer AI off the walk map, and the creature pipeline (FRP R7: "prove on one island first") |
| 2 | **The Wind path, trunk first:** Gust, Wind cut, Momentum bar, Featherfall, the path picker and Sky camps; then Skyborne (Glide, Air dash, Plunge, Lift, Aerial mastery, Sky lord), Cyclone, Windblade | Players can pick Wind and play it on the Old Island while the biome grows |
| 3 | **Islands and land:** the W1 ground and the Lift Steps (proof of density), then W2, W3, W4; the kitbash set; waterfalls; the Great Kite | Biggest piece; proven on one sub-zone |
| 4 | **Gusts everywhere, then micro-hazards:** gust lanes per sub-zone and in Gale; lee rotors, sinking stones, downbursts; Featherdown draught, anchor boots | The biome's identity; needs the islands |
| 5 | **The other 8 creatures**, voices, day, night and fog tables, living-world beats; island walk grids for ground creatures | Reuses step 1's frame |
| 6 | **Minis:** Kyrra (roaming frame), the Bridge Sentinel (needs W3's deck), Old Gulper | After creatures and W2/W3 |
| 7 | **Shrines, run-back stone, Yara and her quests** | After the POIs and the hero rig |
| 8 | **Aeolith and the altar:** the ring, plumes, platforms, the ride component | Needs flight, parts, pushes, the Flue |
| 9 | **The Undercloud and secrets** | After W2 (the Kitefield entry) |
| 10 | **Music set and beds** (can start any time; independent of art) | Big feel gain |
| 11 | **The Turning Wind event** | Needs drift, cloud bridges and the Undercloud |
| 12 | **Gear:** Gale and Hush arrows, Kitewright glaive, Sailbell lantern, Skysilk (when Hearth 4 lands) | Needs items and Hearth tiers |
| 13 | **Aerial combat on the Windmane** (with the survival lane's mount flight) and the 8 combos | Needs the full Wind tree and the mount |
| 14 | **Invasion contents** (Sky side) | Needs the invasion frame and the Fungal caverns (R8) |
| 15 | **Body change** (after the MetaHuman hero lands in R0) | Depends on the hero |

**Spot-check list for the builders (read the code against these, no test runs):** every attack row has a tell over 0.5 s with a sound; no projectile homes or exceeds 1,400 cm/s; every push goes through `AddWindImpulse` so brace, anchor boots, Skysilk 4 and roll i-frames all apply; no invisible walls or nets at island lips; Hushwing never starts a swoop off-screen without the churr inside 8 m; Aeolith's damage comes from socket traces on the tail, jaw, body and quills, never from a floor decal; packs have no attack token.
