# Cook: biome-lightning

Lane: **biome-lightning**. Covers the Storm peaks biome and the Lightning path (round R1): FULL-GAME 3.2.2, the Storm peaks parts of 3.3, and the Lightning row of 4.2. It replaces every GAP there with a build-ready design. Written 2026-11-13. This lane does research, design and plan; the build comes later, round by round, from section 3.

**Frame.** Coordinates are metres in the continent frame of `cooks/world-map.md` (origin at the old island centre, +x east, +y south). Storm peaks is the west half of belt B3 "The Crown": coast at about x −2,600, the Spine and the Crown to the east, y −5,400 (Desert rib spur) to −2,400 (the north-west neck to the Volcanic). Area 5.5 km² (CAL, SPEC). Height runs from sea level to the Spire at 1,650 m.

**What this cook keeps from other cooks (SPEC for this lane, not re-decided):**

| From | Kept exactly |
|---|---|
| `world-map.md` | Belt, neighbours, Stormgut pass, the Crown, the Spire (Kharvox's nest at (−200, −3,300), 1,600 m), the Ring and Twelve Pillars, camps at (−2,250, −2,700), (−2,300, −3,700), (−2,150, −4,700), the Rodwardens as the old people (copper rod waymarks every 150 m), the roc and leech-drake champions in Stormgut, 127 surface POIs + 12 in the Hollow Spire, weather (Thunderstorm 30%, Gale above 300 m), the strike rule, the Grounding salve, `AWRLightningStrike` |
| `combat.md` | Shock status (2.11), Static strike and Charged weapon Wildbrand rows (2.12), Lightning spell numbers (2.13), creature row schema and archetype bands (2.18), boss template (2.17.1, 2.17.5) |
| `hero.md` | Lightning body change (2.6), Stormweave look (2.5), Lightning trim #B9A7FF |
| `survival-crafting.md` | Stormweave stats and recipe, grounding charm recipe, Stormrye and Gentian crops, Puffram wool, shock-resist potion, Storm coil machine, Crag hold building set, Lightning Pike fish, the three Storm survival sites, mini trophy talismans |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game | What it does | Source | Lesson for Storm peaks |
|---|---|---|---|
| Breath of the Wild | In a thunderstorm, equipped metal sparks first, then a strike follows. Metal in the world draws strikes. Take metal off, wear rubber or the Thunder Helm | https://zelda.fandom.com/wiki/Thunderstorm ; https://zeldawiki.wiki/wiki/Lightning | The strike hazard is a gear check with a **long, readable build-up on your own body**: your weapon sparks and your hair lifts before the ground glow. Heavy metal armour raises your pick weight; Stormweave (cloth and glass) lowers it |
| Horizon Zero Dawn | Guerrilla nearly shipped random lightning strikes but cut them because "it just felt unfair" | https://www.gamesradar.com/horizon-zero-dawns-weather-system-nearly-had-random-lightning-strikes-but-it-just-felt-unfair | Strikes are never random-from-nowhere: only above 600 m, only in a visible storm, always 3 s of body tell plus 1 s of ground tell, always dodgeable. Deaths come from greed, not luck |
| Monster Hunter Wilds | Windward Plains cycle Fallow, Inclemency (Sandtide thunderstorm with real ground strikes), Plenty. Rey Dau, the apex, comes with the storm and powers up when struck | https://monsterhunterwiki.org/wiki/Seasons_(MHWilds) ; https://www.thebackdash.com/gaming/monster-hunter-wilds-weather-system-explained ; https://powerpyx.com/monster-hunter-wilds-rey-dau-large-monster | The biome's apex things ride the storm: the **Raiju** arrives inside strikes, Galvara builds her own cloud, Kharvox charges faster in storms. Stormglass appears only after a storm (Plenty) |
| Monster Hunter (Zinogre) | A thunder wolf that visibly charges itself (bugs cluster, fur lights up) before its charged moves; breaking parts weakens the charge | https://monsterhunterwiki.org/wiki/Zinogre_(MHRise) | Charge is **shown on the body**: Voltfang fur rises and glows, Kharvox's feather tips light from wingtip inward. Bleed and broken wings drain the boss's charge (the opposite-element counter made visible) |
| Horizon (Stormbird) | Flying boss: dive with a shock blast, a lightning gun, a gust; shoot off parts to disable moves; bring it down with ropes | https://horizonzerodawn.wiki.fextralife.com/Stormbird | Kharvox's wings are breakable lock points that ground it. Galvara's cloud cores are shootable parts that remove her strike rain |
| Elden Ring (Lansseax) | Red lightning raised in her hand is the tell; the arena has its own gold lightning separate from hers | https://eldenring.wiki.fextralife.com/Ancient+Dragon+Lansseax+(Altus+Plateau) | Keep the colour law: the arena's weather strikes glow storm white; the boss's unblockable moves glow crimson. Never mix them |
| Dragon's Dogma 2 / fire-frost cook | Monsters roam and fights spill into what's nearby | fire-frost 1.1 | The Bellwether walks real ridge routes and fights what it meets; the Raiju hunts anything in a storm |
| Valheim, BotW | Mountains are gear checks plus consumables | fire-frost 1.1 | Grounding salve (early pass), grounding charm (mid), Stormweave (end) |

### 1.2 Real world to build from

| Fact | Source | Use |
|---|---|---|
| **Hair standing up, skin tingling, metal buzzing** come just before a strike; the US weather service calls it nature's final warning | https://unofficialnetworks.com/2024/08/23/if-your-hair-stands-on-end-while-hiking-lightning-is-imminent ; https://www.mountbakerexperience.com/how-to-stay-alive-when-lightning-strikes/ | The strike pre-tell: your hair lifts (WPO), your weapon tip glows with St Elmo's fire, a rising buzz |
| **Fulgurites**: lightning melts sand into hollow glass tubes; on summits it leaves glassy crusts on rock | https://geology.utah.gov/fulgurites-when-lightning-strikes-sand-magic-is-formed ; https://www.nature.com/articles/s41598-021-01559-x | Stormglass: tubes in sandy ground, glass crusts on crag tops; the Glass Scar landmark |
| **Lichtenberg figures** (fern-shaped burns on skin and ground), **St Elmo's fire** (corona glow on points), **ball lightning**, **stepped leaders** (faint branching channel that crawls down before the bright return stroke), **step potential** (current spreading through wet ground) | https://en.wikipedia.org/wiki/Lichtenberg_figure ; https://en.wikipedia.org/wiki/St._Elmo%27s_fire ; https://en.wikipedia.org/wiki/Ball_lightning ; https://en.wikipedia.org/wiki/Lightning#Stepped_leaders | Body-change scars; rod-tip glow tells; the ball-lightning micro-hazard; Kharvox's eye bolt shows a stepped leader before the stroke; strikes on wet ground push a jumpable ground-current ring |
| **Lightning rods** protect a cone beneath them | https://en.wikipedia.org/wiki/Lightning_rod | Rodwarden rods, the Rodcrest's tamed effect, and Kharvox's phase 2 "rod shadow" safe ground |
| **Banner cloud**: a cloud that streams off a lone peak's lee side in wind | https://en.wikipedia.org/wiki/Banner_cloud | Galvara's Eyrie has a permanent banner cloud: a landmark you read from 1 km |
| **Lammergeier** (bearded vulture): red eye ring, dyes its chest rust in iron-rich mud, drops bones from height to smash them on rock ("ossuaries") | https://en.wikipedia.org/wiki/Bearded_vulture | Kharvox's face, its rust-stained chest, its Rust Pools, its bone-drop grab, the ossuary slab in its nest |
| **Sky burial** (Tibetan jhator): the dead offered to vultures on high stones | https://en.wikipedia.org/wiki/Sky_burial | The Rodwardens fed their dead to the roc on Feeding Stones |
| **Thunderbird**: wingbeats make thunder, its eyes flash lightning | https://en.wikipedia.org/wiki/Thunderbird_(mythology) | Kharvox's wing-clap transition and eye bolts |
| **Raijū**: Raijin's thunder beast, a cat, weasel or wolf of lightning that falls with the bolt and leaps wildly in storms | https://en.wikipedia.org/wiki/Raij%C5%AB | The 9th creature, the Raiju |
| **Perun and Veles**: the Slavic thunder god (oak, the mountain top) strikes the serpent of water and the underworld (willow, roots), who hides below; the fight repeats every year | https://en.wikipedia.org/wiki/Perun ; https://en.wikipedia.org/wiki/Veles_(god) | The Storm–Swamp war across Stormgut: Rodwardens (Perun) vs the Mire Coven (Veles). Burnt willows and leech-drake bones on the Storm side, a bled roc on the Swamp side |
| **Thunder marks** (six-petal rosettes carved on roof beams against lightning) and **thunderstones** (old axe-heads kept in houses as fallen bolts) | https://en.wikipedia.org/wiki/Thunder_marks ; https://en.wikipedia.org/wiki/Thunderstone_(folklore) | Rodwarden lintels carry thunder marks; every ruined house has a thunderstone in a wall niche (a lore item) |
| **Bidental**: Romans fenced and consecrated any spot struck by lightning and buried the struck person where they fell | https://en.wikipedia.org/wiki/Bidental | Kit POI: small fenced stone rings with a buried thunderstone, everywhere strikes happen |
| **Meteora, Paro Taktsang, the Hanging Temple of Hengshan**: monasteries on cliff faces | https://en.wikipedia.org/wiki/Meteora ; https://en.wikipedia.org/wiki/Paro_Taktsang ; https://en.wikipedia.org/wiki/Hanging_Temple | The Cliff Shrines sub-zone: the Hanging Monastery |
| **Star-nosed mole** (22 fleshy feelers), **pangolin** scales, **bighorn** horn clashes heard far off, **mountain goat** climbing and alarm snort, **yak** standing into wind, **bellwether** (the belled lead ram) | https://en.wikipedia.org/wiki/Star-nosed_mole ; https://en.wikipedia.org/wiki/Bighorn_sheep ; https://en.wikipedia.org/wiki/Mountain_goat ; https://en.wikipedia.org/wiki/Bellwether | Coilburrow and Coilback; Thunderhorn and the Bellwether; Cragstrider; Galeback |
| **Fujara** (Slovak overtone flute), **cimbalom** (hammered dulcimer), Bulgarian-style open-throat women's choir | https://en.wikipedia.org/wiki/Fujara ; https://en.wikipedia.org/wiki/Cimbalom | The Storm peaks instrument set on top of FULL-GAME's strings, timpani and thunder |

### 1.3 Lessons we take (short)

1. **Fair storms.** The strike is a gear check with a long body tell, never random. It lives above 600 m and in the Great Storm.
2. **Charge shown on bodies.** Every electric creature shows its charge on its fur, feathers or crest before it uses it.
3. **The place is the counter.** Rock walls stop the Bellwether, shootable cores break Galvara's cloud, frost shows Coilback's tunnel, rods shelter you from Kharvox.
4. **Vertical land.** Cliff paths, rope bridges and a hanging monastery. Falls kill; gusts have tells.
5. **One story, told by stones.** Rod-keepers who fed their dead to a thunder-bird and warred with a swamp coven.

---

## 2. Design

Tags: **SPEC** keeps Raven's design, or a settled cook's design, exactly. **GAP-COOKED** is this lane's design. **RAISE** adds to a SPEC item without changing it.

### 2.1 The biome at a glance

| Item | Design | Tag |
|---|---|---|
| Path theme | Chain damage, burst speed (DD) | SPEC |
| Look | Jagged grey granite peaks over dark slate cliffs. Thin, wind-bent mountain pines and krummholz. Iron-dark storm clouds that never clear the Crown's summits. Glassy fulgurite scars where lightning hit. Old green-copper rods everywhere. Tarns of snowmelt. Wind-flattened grass on the coast | GAP-COOKED (FULL-GAME draft kept and made concrete) |
| Biome palette | Slate #3E4650, granite #8A8D91, storm violet-white #B9A7FF (strikes, the path colour), verdigris #4E9A84 (rods), lichen ochre #B79A4A, rye gold #C9B26A (lowland only) | GAP-COOKED |
| Signature resource | **Stormglass**: fulgurite. Tubes in sandy ground and glass crusts on crag tops, made only where lightning hits; most of it appears after a storm (2.6) | GAP-COOKED (FULL-GAME draft kept) |
| Main hazard | Strike zones in thunderstorms (2.5) | SPEC numbers (FULL-GAME 3.3, world-map) + RAISE (body pre-tell) |
| Hidden sub-biome | The Hollow Spire (2.7) | GAP-COOKED (site SPEC from world-map) |
| Event | The Great Storm (2.15) | GAP-COOKED |
| Shrines and path master | 5 shrines; path master **Vesna Groza** at Strikerock (2.13) | RAVEN-PLANNED exist / GAP-COOKED who and where |
| Music | 92 BPM, E Phrygian (2.12) | SPEC tempo and key (FULL-GAME 3.3) / GAP-COOKED cue sheet |
| Night light | Lightning flicker in the clouds over the Crown; St Elmo's fire on rod tips in storms; violet glow on fresh stormglass; the Hanging Monastery's braziers once lit | SPEC rule (world-map 2.8) / GAP-COOKED details |
| Tone | Exposed, loud, high and beautiful. Bright days with long views; storms that make the land scary | GAP-COOKED |

### 2.2 Sub-zones (GAP-COOKED; names SPEC from CAL Part 3)

CAL's five names are kept. Each sub-zone follows world-map 2.4 (own ground, palette, props, day and night table, ambient bed, a signature silhouette, at most one micro-hazard).

| # | Sub-zone | Where (x, y) | Height | Area | Band | Ground and feel | Palette | Props (unique kits) | Micro-hazard | Ambient bed | Silhouette |
|---|---|---|---|---|---|---|---|---|---|---|---|
| S1 | **The Wind Passes** | Coast band x −2,600 to −1,900, y −5,300 to −2,450, plus three west–east valleys cutting inland (Northgap y −4,700, Midgap y −3,700, Southgap y −2,800) | 0 to 300 m | 1.6 km² | Lowland | Wind-flattened grass and thrift on the sea cliffs, grey shingle coves, rye terraces in the valleys, snowmelt tarns, a few wind-bent pines leaning east | Grass grey-green #7C8A6A, rye gold #C9B26A, slate #3E4650 | Wind-bent pines, rye-terrace walls, sheep folds, ruined storm mills, wind-pole cairns | None (the gentle zone) | Constant wind with gust swells, grass hiss, sea below, Puffram bleats, distant thunder over the Crown | The Bell Rod on its headland (2.3) |
| S2 | **The Cliff Paths** | x −1,900 to −1,200, y −4,000 to −2,450 | 250 to 650 m | 1.4 km² | Midland | Dark slate cliffs in steps, paths cut into the faces, ledges and goat trails, waterfalls off the Crown's foot, krummholz and scree | Slate #3E4650, wet black #1F242A, verdigris #4E9A84 | Cut paths with copper chains bolted in the rock, ledge shrines, goat ladders, waterfall pools, Rodwarden waystations | **Cliff-path gusts** (2.5) | Wind through rock gaps (a hollow whistle), waterfalls, Cragstrider hooves on stone, eagle calls | The Thunderstone Arch (2.3) |
| S3 | **The Rope Bridges** | x −1,900 to −500, y −5,350 to −4,000; the gorge country north toward Stormgut and the Desert rib | 200 to 750 m | 1.2 km² | Midland, highland to the east | Three slot gorges (Thundergorge, Kitecut, Saltslot) running east to Stormgut, spanned by Rodwarden rope bridges; pinnacles and needles between them | Granite #8A8D91, rope tan #9C8460, storm blue #4A5A78 | Rope bridges (3 lengths), bridge towers, winch huts, rock needles, kite nests | **Rotten planks** (2.5) | Gorge echo, rope creak, roaring water far below, Stormkite whistles | Galvara's Eyrie and its banner cloud (2.3) |
| S4 | **The Struck Crags** | The Crown's west and south-west flanks, x −1,500 to −700, y −4,000 to −2,500 (minus S5) | 600 to 1,150 m | 0.8 km² | Highland | Bare granite crags glazed with fulgurite, the Rodfield (hundreds of old rods), boulder fields, black lichen, ice in shadow | Granite #8A8D91, glass sheen #C9D4E8, violet-white #B9A7FF in storms | Fulgurite crusts, rod clusters, bidental rings, struck and split boulders, burnt krummholz | **Ball lightning** (in storms, 2.5) | Rod hum and wind-song through the rods, crackle, close thunder | The Glass Scar (2.3) |
| S5 | **The Cliff Shrines** | The Crown's west face, x −1,000 to −600, y −3,600 to −3,000 | 650 to 900 m (up to the Ring) | 0.5 km² | Highland | The Rodwardens' hanging monastery: terraces, cells and stairs built into the cliff, linked by ladders, galleries and chain walks, ending on the Ring | Whitewash #D9D5CC, timber brown #5A4636, copper #B87333 and verdigris | Cliff cells, prayer galleries, chain walks, feeding stones, copper torc hooks | None (the strike hazard is live here) | Wind on timber, bells (cracked), wind-chimes of copper, choir-like wind tones at the galleries | The Hanging Monastery (2.3) |

**Blends:** 60 to 120 m between sub-zones (world-map rule). **Borders:** the north-west neck to the Volcanic carries ash on rod-roads (world-map); the Desert rib spur at y −5,400 has two river gaps with sand creeping onto granite, and the first fulgurite tubes appear in that sand (stormglass's richest low ground).

### 2.3 Skyline landmarks (GAP-COOKED)

10 landmarks (world-map: one per ~0.6 km²), each readable from 1 km.

| # | Landmark | At (x, y), height | What it is | Reads at night as |
|---|---|---|---|---|
| L1 | **The Spire** (SPEC site) | (−200, −3,300), 1,650 m | The black granite horn of the Crown. A ring of twelve old rods on its summit; Kharvox's nest | Violet flicker of strikes on the rod ring; Kharvox's silhouette circling at dusk |
| L2 | **The Bell Rod** | (−2,500, −3,950), sea headland at 90 m | A 60 m Rodwarden copper mast on the headland north of camp 2, green with age, bronze bell-cages at three heights. It takes a strike in every storm | A strike on its tip every storm; its bells ring after |
| L3 | **Strikerock** | (−1,880, −3,350), 280 m | A lone granite tor split down the middle by one huge old strike, glass down both faces. The Great Shrine and the path master sit in the split | The shrine brazier in the split |
| L4 | **The Thunderstone Arch** | (−1,550, −2,900), 420 m | A natural rock arch over the cliff path. From its keystone hangs an 8 m polished stone axe-head on copper chains: the Rodwardens' great thunderstone | Copper chains catching lightning flicker |
| L5 | **Galvara's Eyrie** | (−1,250, −4,600), needle top at 760 m | A 140 m granite needle between two gorges, with a permanent banner cloud streaming off its east side | The banner cloud lit from inside by small flashes |
| L6 | **The Bridge of Nine Spans** | (−1,500 to −1,120, −4,350), 420 m | The longest Rodwarden rope bridge, 380 m across the Thundergorge on 8 rock pillars, each with a winch hut | Lanterns in the winch huts once their shrine is lit |
| L7 | **The Copperfall** | (−1,350, −3,950), 500 m | A 90 m waterfall stained bright green-blue by copper, falling into a scree bowl of verdigris pools: Coilback's ground | Pools glowing faint blue (copper-salt shimmer, emissive 0.3) |
| L8 | **The Glass Scar** | (−1,000, −3,050 to −1,000, −2,650), 900 to 1,050 m | A 400 m groove of fused glass down a granite slab, where the Great Storm always strikes first. The violet ray of the Ring's starburst (world-map 2.12) points straight at it | A seam of violet glow for 2 game hours after any storm |
| L9 | **The Hanging Monastery** | (−800, −3,300), 650 to 900 m | Whitewashed cells and timber galleries stacked up the Crown's west face, chain walks between them, the top terrace opening onto the Ring | Its braziers, lit for good once the Hanging Shrine is cleansed |
| L10 | **The Storm Mill** | (−2,150, −3,150), 140 m | Survival-crafting's ruined storm mill made a landmark: a stone windmill tower on the Midgap's shoulder, sails torn, its copper rod still standing and melted at the tip | A rod-tip spark every storm |

### 2.4 POIs: 127 surface + 12 Hollow Spire (GAP-COOKED; count SPEC from world-map)

**Counts by sub-zone:** S1 38, S2 32, S3 26, S4 18, S5 13. Mix (world-map): 9 landmarks (L1 is the Crown's, counted there), 5 shrines, 29 hand-made sites, 84 kit POIs.

**Already counted elsewhere:** 3 spawn camps, the Stormgut west mouth and its pass fort, the north-west neck road.

**Hand-made story and set-piece sites (29):**

| # | Site | Sub-zone, at | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| 1 | Castaway wrecks (SPEC rule) | S1, beside each camp | The ship that brought each camp, hull of the shared newcomer make | Starter wood, rope |
| 2 | Bell Rod base | S1, (−2,480, −3,950) | A Rodwarden keeper's hut, a ladder up the mast's first 20 m, a sea-chest of burnt bell clappers | Copper ×8, lore item "Keeper's tally" (a stick notched once per strike, thousands of notches) |
| 3 | Ruined storm mill (survival-crafting site, SPEC) | S1, (−2,150, −3,150) | Sails torn, copper rod still standing, a melted lantern | Recipe scrap: windmill |
| 4 | The rye terraces (survival-crafting site, SPEC) | S1, Midgap, (−2,050, −3,700) | Walled high fields, Stormrye flattened by gusts and standing back up | Stormrye seeds ×6 |
| 5 | Goatherd's cave (survival-crafting site, SPEC) | S2, (−1,700, −3,500) | Puffram pen, cold fire, gentian drying on strings | Gentian seeds, bedroll spot |
| 6 | The Feeding Stones | S5, (−760, −3,200), 700 m | Six flat slabs on a terrace, iron hooks, flensing knives, bone splinters and small copper torcs in the cracks. The slabs face the Spire | Lore item "Torc of the given" |
| 7 | The Torc Road | S5 to the Ring | Copper torcs hung on iron pins every 30 m up the last stair, each one smaller (adults, then children) | Wayfinding to the Ring |
| 8 | The Melted Cell | S5, (−720, −3,380), 860 m | The top cell of the monastery. A torc melted into the floor at the centre of a Lichtenberg burn that spreads across the whole floor and up the walls. The roof is open to the sky | Lore item "Struck one's torc" (melted) |
| 9 | Monastery kitchens | S5, (−860, −3,250), 680 m | Long tables, bowls still set, an oven, a chain winch to haul food up the cliff | Salt, pine nuts, bedroll spot |
| 10 | Thunderstone Arch | S2, (−1,550, −2,900) | The hanging axe-head; under it a ring of small thunderstones offered in a hollow | Thunderstone ×3 (a crafting extra, 2.17) |
| 11 | Strikerock Great Shrine | S2, (−1,880, −3,350) | The split tor with glass faces; a climbers' camp of ropes and pitons; the path master's lean-to | Great Shrine; Vesna |
| 12 | The Copperfall | S2/S3, (−1,350, −3,950) | Green fall, verdigris pools, scree with copper float, old Rodwarden mine props half-buried | Coilback's arena; copper ×12 |
| 13 | Bridge of Nine Spans | S3, (−1,500, −4,350) | Nine spans; winch huts with crank wheels; one span hangs broken (2.8 secret) | Crossing; a vista |
| 14 | Kitecut Nests | S3, (−1,050, −4,850) | Stormkite nests of sticks and copper wire on the gorge walls, eggshells, dropped fish | Stormkite eggs are not a taming route (knockout only, SPEC); feathers ×6 |
| 15 | Galvara's Eyrie | S3, (−1,250, −4,600) | The needle; a cut stair spiralling up its outside; the top flat with kite bones and a cloud always streaming east | Galvara's arena |
| 16 | The Stormgut Fort (pass kit SPEC) | S3, west mouth, (−600, −4,900) | The Rodwarden fort facing east, rods on every tower, Coven bone hooks embedded in its east wall, a willow stake shrine smashed | Lesser shrine "Stormgut Mouth"; vista into the pass |
| 17 | The Burnt Drake (pass kit SPEC: "leech-drake on the Storm side, burnt black") | S3, (−420, −4,950), in the gut's west end | A 30 m leech-drake skeleton burnt black, Lichtenberg scorch fanning from it across the rock; Rodwarden rods driven through its ribs into the stone | Lore item "Rod through a rib" |
| 18 | The Halite Cave | S1, sea cave at (−2,580, −4,250) | Pink rock salt walls, tide pools, a smugglers' ledge | Rock salt nodes (survival-crafting), a cave pack |
| 19 | The Copper Adit | S4, (−1,180, −3,700), 780 m | A Rodwarden mine: timbered adit, ore carts, a long copper vein; deeper, iron | Cave (ore by depth); back route into the Hollow Spire (2.7) |
| 20 | Thundergorge Grotto | S3, (−1,600, −4,450), gorge floor | A wet grotto behind a fall at the gorge's bottom; tin and copper | Cave |
| 21 | The Rodfield | S4, (−1,150, −3,150) | About 200 old rods, 3 to 12 m tall, in rough rows: the Rodwardens' "field of the sky". They hum in wind. In storms they glow at the tips and take most strikes | In a storm: the safest place above 600 m; fresh stormglass at rod feet after |
| 22 | The Glass Scar | S4, (−1,000, −2,850) | The 400 m glass groove; Raiju lairs (scorched hollows) along it | Stormglass ×15 crust; the Glass Scar Shrine |
| 23 | The Rust Pools | S4, (−560, −3,520), 1,050 m, under the Spire | Iron-red mud pools on a ledge. Giant feathers stuck in the mud, deep talon prints: Kharvox bathes here (the lammergeier's habit) | Iron ore ×6 (red mud), Kharvox feathers ×2 |
| 24 | The Last Stair | S4/S5 to the summit | The Rodwarden stair from the Ring to the summit nest; carved thunder marks on every 10th step; the run-back stone 50 m below the nest | The way to Kharvox |
| 25 | Struck Climbers | S2, (−1,400, −3,250), 600 m | Three roped climbers' skeletons on a ledge at exactly 600 m, metal axes still in hand, burn ferns on the rock round them | Iron axe, rope, a grounding charm (worn, half durability) |
| 26 | The Thunder Rams' Ring | S2, (−1,650, −3,000) | A flat meadow on a cliff shelf where Thunderhorns clash at dusk; horn tips and broken horn on the ground | Horn ×3; a set-piece at dusk |
| 27 | The Bellwether's Fold | S2, (−1,800, −2,650) | A ruined stone sheep-fold, gate smashed outward, a bronze bell-clapper as long as an arm in the grass | The Bellwether's lair and night rest |
| 28 | The Twelve Stairs, Storm stair (world-map SPEC) | S4, from (−1,450, −3,300) | The Rodwarden stair up the Crown, broken at 700 m: a viewpoint | Vista; the stair's end is a fall |
| 29 | The Neck Road (world-map SPEC) | S1, (−600, −2,300) | Rods and slag cairns side by side, ash on the road | Wayfinding to the Volcanic |

**Kit POIs (the generator places about 84, with variants):**

| Kit piece | Sub-zones | Variants | Count about |
|---|---|---|---|
| Rod waymark (on the rod-road, every 150 m; SPEC from world-map) | all | 3 | 16 |
| Bidental ring (fenced stone ring where a strike killed someone; a thunderstone under the centre stone) | S1, S2, S4 | 3 | 8 |
| Fulgurite field (sandy patch with glass tubes; refills after storms) | S1 dunes by the Desert rib, S4 | 3 | 7 |
| Copper or tin outcrop | S2, S3, S4 | 4 | 9 |
| Rodcrest basking rock (a high rock where Rodcrests raise their crests) | S1, S2, S4 | 2 | 5 |
| Puffram fold (dry-stone wall, a lambing shed) | S1 | 3 | 6 |
| Tarn (snowmelt pool, drinkable, ridge trout) | S1, S2 | 3 | 6 |
| Rodwarden waystation (a one-room stone hut with a thunder-marked lintel and a thunderstone niche) | S1, S2, S3 | 3 | 6 |
| Short rope bridge (20 to 60 m) | S3 | 3 | 5 |
| Stormkite roost (dead pine with nests) | S2, S3 | 2 | 4 |
| Coilburrow mound field (egg chambers) | S1, S2 | 2 | 4 |
| Kill set-piece (a Voltfang pack on a Puffram or Cragstrider kill) | S1, S2 | n/a | 4 |
| Struck tree (pine split and charred; resin and charcoal) | S1, S2 | 2 | 4 |

Every kit POI sits on a rise or round a bend (triangle rule) and has a light or skyline shape at night (SPEC rules).

### 2.5 Hazards (main: SPEC numbers + RAISE; micro: GAP-COOKED)

| Hazard | Where | Tell | Effect | Passes with |
|---|---|---|---|---|
| **Strike zones** (SPEC numbers) | Above 600 m in a Thunderstorm: S4, S5, upper S2 and S3, the Crown, Stormgut | **RAISE, pre-tell 3.0 s:** the picked player's hair lifts (hero WPO hair lift, also visible to others), metal weapon and armour edges grow St Elmo's fire (small violet-white corona), a rising buzz in the mix. **Then the SPEC tell 1.0 s:** a 2 m ground glow and crackle at the spot. The strike lands on the **spot**, not the player (never homes) | SPEC: a strike every 20 to 40 s on the highest player in each 60 m cell above 600 m; 60 damage and a 0.8 s stun. **RAISE, ground current:** on wet ground (Rain or Thunderstorm), a 4 m ring of current spreads from the strike at 1,200 cm/s for 0.3 s: 15 damage, no stun, `low` (a jump clears it). Flyers above 600 m are picked first (SPEC). **Pick weight:** each heavy-load armour piece or metal shield ×1.25 to your chance of being picked within the cell (BotW metal rule); a Stormweave piece ×0.85 | Grounded (root, main path): no stun, strike damage ×0.5, and the picker skips you if anyone else in the cell can be picked. Grounding charm (talisman): same as Grounded. Stormweave 4 pieces: never picked (SPEC from survival-crafting). A tamed Rodcrest within 30 m of a base: no strikes in that radius. **Grounding salve** (early pass, SPEC from world-map): 10 min, no stun, damage ×0.5. **Rodfield and rod shadow:** within 3 m of a standing rod base, the strike goes to the rod |
| **Cliff-path gusts** (SPEC numbers from FULL-GAME 3.3) | Marked cliff paths in S2, and the Last Stair | 1.0 s rising whistle, plus grass, dust or snow streaking across the path in the gust direction | 900 cm/s push for 0.6 s every 8 to 15 s, toward the drop. Falls kill (no floors) | **Brace:** hold Block while not attacking: push ×0.3. Crouching: push ×0.5. Galeback mount: no push (SPEC). Stormweave doesn't help; Skysilk 4 does (Wind lane) |
| **Rotten planks** | S3 rope bridges (about 1 plank in 12 on long bridges; never two in a row) | A plank darker than the rest; on your first step it creaks loudly (1.0 s) | Breaks 1.0 s after the creak. Fall into the gorge (lethal on most bridges) | Step over it (it reads before you reach it); jump. Repairs: any player can place a "plank" (wood ×2) on a broken slot from the bag; it stays for 2 game days |
| **Rockfall** | S2 cliff paths under overhangs, more in rain | Pebbles trickle and click down the face for 1.2 s, dust puffs | 3 m circle: 45 damage, knockdown | Dodge forward or back |
| **Ball lightning** | S4 in Thunderstorms; S4 and S5 in the Great Storm | A violet-white sphere (0.5 m) forms at a rod tip or crag with a hum, 1.5 s before it moves | Drifts in a straight line at 200 cm/s for up to 20 s at 1 to 2 m height; never turns toward anyone. Touching it, hitting it or its end: pop for 40 damage and shock 60 in 3 m | Walk round it; pop it on purpose with an arrow or spell from range (no damage to you beyond 3 m) |

**No floors.** Every hazard can kill. **No helpers after a death.**

**Why strikes are fair:** 4.0 s of total warning (3.0 s body, 1.0 s ground), on a spot, only in a visible storm, only above a height the HUD compass shows (the altitude line on the compass turns violet-white above 600 m during a Thunderstorm).

### 2.6 Weather and resources

**Weather (SPEC from world-map 2.6):** Clear 25, Overcast 20, Rain 15, Fog 10, **Thunderstorm 30**. Gale above 300 m in storms. Fronts from the Swamp cross the Spine an hour later (50%). Thunder follows the flash at 1 s per 340 m. **Plenty** after a storm: stormglass yield ×1.5 (SPEC).

**What a Thunderstorm changes in Storm peaks (GAP-COOKED, on top of the SPEC state):**
- **Strike zones** go live (2.5).
- **Raiju** appear (2.9), only in storms and the Great Storm.
- **Wild Rodcrests** climb basking rocks and raise their crests; strikes within 30 m go to them (they glow for a minute after). A Rodcrest rock is safe ground.
- **Voltfangs** are charged for the whole storm (their charged attacks, 2.9, are ready at once).
- **Stormkites** fly the storm front at 300 to 600 m and dive from it.
- **Puffram wool** stands out with static; shearing in a storm gives "charged wool" (Stormweave extra, 2.17).

**Stormglass rules (GAP-COOKED):**

| Source | When | Yield |
|---|---|---|
| A strike that lands on bare ground (hazard strike, Kharvox's strikes, the Great Storm, Tempest) | 35% of strikes on sand or granite cells leave a node | A fulgurite node: 3 stormglass (pickaxe, 2 hits), lasts until 2 game hours after the storm ends |
| Fulgurite fields (kit POIs) and the Glass Scar | Refill fully at the end of each Thunderstorm | 4 to 6 nodes per field |
| Rod feet in the Rodfield | After each storm | 1 node per 6 rods |
| The Great Storm | Event | All the above ×3 (3.3 SPEC) |
| Clear weather | | None appear; old nodes stay until mined |

**Other resources:**

| Resource | Where | Notes |
|---|---|---|
| Copper (rich), tin | Outcrops S2 to S4, the Copperfall, Thundergorge Grotto | The Rodwardens' metal. Copper ×1.3 yield in this biome |
| Iron | Deep Copper Adit, the Rust Pools, the Hollow Spire | Ore by depth (CF SPEC) |
| Rock salt | Halite Cave | SPEC (survival-crafting) |
| Mountain pine, krummholz, struck pine (charcoal, resin) | S1, S2 | Wood and resin |
| Stormrye, Gentian (wild) | S1 terraces, S2 meadows | SPEC crops (survival-crafting) |
| **Storm essence** (path material) | Drops from Voltfang, Stormkite, Thunderhorn, Raiju (×1 each), minis (×4), Kharvox (×8) | The Lightning element at crafting, like `ember_essence` (CF SPEC rule) |
| Thunderstone | Bidental rings, the Arch, waystation niches | Crafting extra (2.17) |
| Puffram wool, Thunderhorn horn, Voltfang pelt, Stormkite feathers, Raiju pelt | Creatures | Stormweave and trophies |
| Ridge trout, Thunder grayling, the Lightning Pike | Tarns, gorge pools, the Hollow Spire | SPEC (survival-crafting) |

### 2.7 Hidden sub-biome: the Hollow Spire (GAP-COOKED; site SPEC from world-map)

A peak hollowed by ages of strikes. Its inside is a vertical cave: a 70 m wide shaft from the Ring (900 m) up to 1,500 m, with chambers off it, lined with stormglass and copper veins. A flooded floor at the bottom holds the Lightning Pike.

**Ways in:**
1. **The Warden Door** on the Ring at (−600, −3,450): a Rodwarden bronze door half open.
2. **The Strike Chimney** from the summit's west shoulder at 1,480 m: a glass-lined hole where strikes go down. Drop 9 m onto a slope of glass sand. A strike comes down it every 60 s in storms (1.0 s glow at the bottom): the chimney is a timing route.
3. **The Copper Adit** from S4 (2.4 #19): its deepest drift breaks into the Spire's floor.
4. **The Nest Door** (shortcut): a stair that opens one way from inside into the Last Stair, 30 m below the run-back stone. Once opened it stays open for your clan.

**Getting back up the shaft:** Rodwarden chain walks spiral up the walls (some broken: jumps); rope ladders (fire-frost SPEC recipe) can be hung at any lip.

| # | Room | Height | What's there | POI |
|---|---|---|---|---|
| H1 | **The Drowned Floor** | 900 m | A black lake at the shaft's foot; strikes down the chimney flash the whole shaft. The Lightning Pike (SPEC fish) | Fishing; iron nodes on the shore |
| H2 | **The Ringing Hall** | 950 m | A chamber of stormglass pipes that sound when the wind blows across the Warden Door: a chord changes with the wind | Stormglass ×10, lore item "Pipe-keeper's tuning rod" |
| H3 | **Copper Galleries** | 1,000 m | Rodwarden mine galleries; rich copper and iron; 4 Coilburrows and their eggs | Ore; Coilburrow eggs (the best egg spot) |
| H4 | **The Torc Vault** | 1,080 m | Thousands of copper torcs on chains from the ceiling, chiming; one empty hook per torc on the Torc Road | Copper ×20; lore item "The count of the given" |
| H5 | **The Lichtenberg Wall** | 1,150 m | A 40 m wall burned with one giant fern of a strike. Inside the burn, carved figures of the twelve peoples climbing; the figure at the top is struck out | Lore item, a vista down the shaft |
| H6 | **Raiju Den** | 1,200 m | Scorched hollows in glass where Raiju sleep curled between storms (folklore: they sleep in hollows) | 3 Raiju asleep by day (sneak to pass or crit them) |
| H7 | **The Rod-Maker's Forge** | 1,250 m | A forge where the rods were drawn: copper billets, drawing plates, a cold furnace, half-made rods | Recipe "Rodwarden spear" (2.17); copper ingots ×6 |
| H8 | **The Sealed Choir** | 1,300 m | A round room of stone benches facing a central rod. Sealed by a bronze grille that opens only during the Great Storm (the strike melts the lock) | Upgrade shards ×2, sapphire-blue gem (topaz in data), lore item "Choir's last hymn" |
| H9 | **The Glass Sand Slope** | 1,480 m | Under the Strike Chimney; glass sand; fresh stormglass after every storm | Stormglass ×8 each storm |
| H10 | **The Nest Door** | 1,550 m | The one-way door to the Last Stair | Shortcut |
| H11 | **The Feather Shaft** | 1,350 m | A side shaft lined with moulted Kharvox feathers and pellets of bone and copper wire | Kharvox feathers ×4, copper wire |
| H12 | **The Keeper's Cell** | 1,400 m | A last cell with a bed of rope, a thunderstone pillow, and a skeleton facing the Nest Door with a copper rod held across its knees | Lore item "Keeper's rod" |

**Light:** dark except during strikes, when the whole shaft flashes violet-white. Bring light. **Creatures:** Coilburrow 4, Raiju 3 (asleep by day), a Voltfang pack on H1's shore at night, Stormkites roost in H11. **Music:** STO_HollowSpire (2.12).

**Lore items (text, Souls style, nothing more):**
- *Pipe-keeper's tuning rod:* "Tuned to the east wind. When the east wind failed, the pipes sang the other chord."
- *The count of the given:* "One torc for each one given to the sky. Count the hooks; the last are empty."
- *Choir's last hymn:* "Twelve went up. Eleven came down. We kept the stair, and the sky kept the rest."
- *Keeper's rod:* "Held across the knees so that the bird would see it and remember what we were."

### 2.8 Secrets (GAP-COOKED)

| Id | Secret | How found |
|---|---|---|
| ST-S1 | **The Broken Span:** the Bridge of Nine Spans' broken fifth span hangs down the gorge wall; climb its rope down to a ledge cave with a bronze chest (Rodwarden spear blueprint copy, onyx) | Look over the broken span's edge: the rope is still tied |
| ST-S2 | **The Bell Rod's crown:** the mast's ladder is broken at 20 m; a Cragstrider can climb the bell-cages to the top cage, which holds the lore item "First bell" and a grounding charm of masterwork quality | Ride a tamed Cragstrider up the mast (it climbs 60°) |
| ST-S3 | **Behind the Copperfall:** a cut passage behind the falls into a Rodwarden smelter | A Coilburrow tame senses the copper lode behind the water, or the green stain on the rock is wet in a straight line |
| ST-S4 | **The Choir** (H8) | Be in the Hollow Spire during the Great Storm |
| ST-S5 | **The Thunderstone's hollow:** strike the Arch's hanging thunderstone with any lightning ability: it rings, and a niche opens in the arch's foot (Wildbrand recipe scroll "Rodwarden thrust": a cosmetic Static strike clip variant, plus storm essence ×3) | Lightning hit on the stone |

### 2.9 Creatures (9): roster, look, rows, drops (GAP-COOKED; 8 species and taming SPEC-as-drafted in FULL-GAME, the 9th is new)

FULL-GAME lists 8; the brief asks for about 9. The 9th is the **Raiju**, a storm-only highland hunter from Japanese folklore. Tiers follow FULL-GAME 3.3: Hard aggressive = highland (HP 200, hit 32, stance 55), Medium = midland (120, 20, 35), passives = lowland (70, 12, 20). Every creature resists strikes and shock (CR: "creatures resist their own biome's hazard").

**Roster and look**

| Creature | Tier, role, taming (SPEC) | Size | Silhouette (reads in the dark) | Look and palette | Where (day / night) |
|---|---|---|---|---|---|
| **Voltfang** | Midland, aggressive pack (3 to 5). Knockout and feed. Medium | 1.0 m at the shoulder, 2.0 m long | Long-legged, deep-chested wolf with a ridge of fur that stands up from nape to tail like a crest; ears long and swept back | Slate-grey coat, white-tipped guard hairs that glow violet-white when charged (emissive mask, 0 to 2 nits), black lips, pale eyes. Lean, ribs show when it runs | S1, S2 by day in 3s; S1 to S4 by night in 4 to 5s |
| **Thunderhorn** | Highland, aggressive. Knockout and feed, or lightning path magic. Hard | 2.0 m at the shoulder, 450 kg | Massive ram with horns curling a full turn and a half, the curl wider than its body; heavy neck ruff | Granite-grey wool over blue-black skin, horns ridged amber with copper-green tips (they lick copper outcrops), yellow eyes with goat pupils | S2, S4 by day; asleep in rings by night (sneak-tame window) |
| **Stormkite** | Midland, aggressive flyer and scout. Knockout and feed (bring it down). Medium | 4.0 m wingspan, tail 1.6 m | Long narrow wings, deeply forked tail like a swallow-tailed kite, a short hooked beak | White underside, slate back, black wing edges; the tail fork's inner feathers glow violet-white before a dive | S3, S2 by day; roost in nests by night |
| **Raiju** (new) | Highland, aggressive storm hunter. Knockout and feed. Hard | 2.4 m long plus a 1.4 m forked tail, 0.9 m at the shoulder | Low, long, arched like a weasel crossed with a lynx; its fur stands out in a spiky halo; the tail splits into two near the tip (forked lightning); bare, tight skin on the face showing the skull's shape | Charcoal fur with white-blue Lichtenberg ferns glowing through the skin (3 nits in a storm), no visible pupils (white eyes), black claws 8 cm long. Terrifying up close: it moves in sudden bursts and holds still between | Thunderstorms and the Great Storm only: S2 to S4 and the Hollow Spire. Asleep in H6 and the Glass Scar hollows otherwise |
| **Cragstrider** | Midland, neutral mount. Knockout and feed. Medium | 1.6 m at the shoulder | Six-legged mountain goat: white shaggy coat, short black dagger horns, a beard; the middle legs short and set high, used for climbing | White wool, black hooves and horns, pink-grey nose | S2, S4 cliffs by day; sleep on ledges at night |
| **Galeback** | Midland, neutral hauler. Offer stormglass. Medium | 2.0 m at the shoulder, 3.2 m long | Low and wide, head carried below the shoulders, a skirt of long hair to the ground, a big hump; it always faces into the wind | Brown-black hair with grey frosting, wide flat horns swept sideways, a copper ring of verdigris stain round the muzzle from licking stormglass | S1, S2 herds of 4 to 8 |
| **Coilburrow** | Midland, neutral utility. Steal an egg. Easy | 1.0 m long | Hump-backed burrower with a pink star of 22 feelers on the snout and spade claws | Velvety black fur, pink star nose that sparks blue when it senses metal | S1, S2 mound fields; H3 |
| **Rodcrest** | Lowland, passive utility. Offer food (berries, insects). Easy | 1.2 m long | Monitor-lizard body with a tall sail crest of copper-bright scales that it raises | Grey scales, a bright copper crest that tarnishes green with age (old ones are green), gold eyes | S1, S2, S4 basking rocks |
| **Puffram** | Lowland, passive farm animal. Offer food (Stormrye). Easy | 0.8 m at the shoulder | Small sheep with a huge round cloud of white wool, little curled horns | White wool that stands out with static in storms, black face and legs | S1 folds and pastures |

**Living-world beats (GAP-COOKED):**
- Voltfangs hunt Puffram flocks at dusk; Galebacks form a ring facing Voltfangs, as musk oxen and yaks do.
- Thunderhorns clash at the Rams' Ring at dusk; the crack carries 1 km (real bighorn behaviour). A fighting pair ignores you unless you come within 10 m.
- Stormkites snatch Rodcrests off basking rocks and drop them (they're tough); you see a Rodcrest fall.
- In a storm, a Raiju arrives in a strike and hunts the nearest living thing, player or creature. Kill set-pieces after storms: a Galeback dead in a scorch fern.
- Cragstriders sneeze-whistle an alarm (real mountain goat call) that puts every creature within 40 m on alert.

**Attack rows (GAP-COOKED, combat.md 2.18 schema and bands).** Timings in seconds. Damage is × the tier hit. Damage only on visible contact through socket traces; projectiles ≤ 1,400 cm/s, never home, never fired inside 4 m. Every tell over 0.5 s has a rising sound cue. Packs attack freely.

| Creature | Attack | Wind-up | Delay hold | Active | Punish | Dmg × | Buildup | Flags and tell |
|---|---|---|---|---|---|---|---|---|
| **Voltfang** | Snap bite | 0.45 | 0 | 0.12 | 0.8 | 1.0 | Shock 15 | Parryable. Lips peel, short growl |
| | Bite, bite | 0.45 / 0.40 | 0 | 0.12 each | 0.8 | 1.0 / 0.9 | Shock 15 each | Parryable |
| | Flank lunge | 0.60 | 0.2 | 0.15 | 0.9 | 1.4 | Shock 25 | Haunches drop, crest rises; knockback 400 cm (on ledges this kills). Comes from 90° or 150° off your facing (flank role) |
| | Static shake (charged only) | 0.80 | 0 | 0.30 | 0.8 | 0.5 | Shock 45 | 3 m burst round it, blockable; fur glows white from nape to tail over the wind-up |
| | Charge-up | 1.5 | — | — | — | — | — | Out of a storm, a pack member that hasn't attacked for 6 s rolls in grit and shakes: its fur glows; the next shake is ready. In a storm they're always charged |
| | Pack crackle-howl | — | — | — | — | — | — | First hit on any member: a howl with a crackle that pulls packs within 60 m |
| **Thunderhorn** | Head-butt ("thunderclap") | 0.70 | 0.3 | 0.15 | 0.7 | 1.4 | Shock 20 | Parryable. Rears its head back; on contact a loud clap and a white flash at the horns. Launch (combat.md impact 5) |
| | Horn hook, hook | 0.55 / 0.45 | 0 | 0.15 each | 0.7 | 1.0 / 1.1 | — | Parryable |
| | Ridge charge | 1.00 | 0 | up to 2.2 | 1.0 if it misses | 1.6 | Shock 30 | **Unblockable** (crimson glint at the horns). Two hoof scrapes, head lowered. Tracks for the first half of the wind-up only, then straight at 1,000 cm/s up to 22 m. Wall or rock hit: stuck 2.5 s (combat.md herd charger) |
| | Rear stomp | 0.80 | 0 | 0.15 | 0.8 | 1.2 | — | 2.5 m ring, `low` |
| | Reads action | — | — | — | — | — | — | Drink or cast within 10 m: a charge within 0.4 s |
| **Stormkite** | Dive strike | 0.80 | 0.2 | 0.20 | 1.5 grounded | 1.3 | Shock 30 | Wings fold, a rising whistle-scream; the tail-fork glows. Straight line from the fold; lands after and shakes off for 1.5 s (the punish) |
| | Spark darts (hovering) | 0.90 | 0 | — | 0.7 | 3 × 0.35 | Shock 20 each | 3 darts in a 15° fan at 1,200 cm/s, a white trail, never home, not inside 4 m |
| | Grounded snap | 0.50 | 0 | 0.10 | 0.8 | 0.9 | — | Only while grounded: 6 s after 3 wing hits (SPEC "bring it down") |
| **Raiju** | Strike arrival | 1.0 ground glow (the hazard's tell) | 0 | 0.15 | 0.6 | 1.0 | Shock 40 | It rides a strike down: a strike lands 6 to 12 m from a target with the normal 1.0 s glow, and the Raiju is crouched in the scorch. Then it acts at once |
| | Pounce | 0.50 | 0.4 | 0.20 | 0.8 | 1.3 | Shock 30 | Flattens, tail forks flare. Covers 8 m |
| | Claw flurry (3) | 0.34 / 0.30 / 0.50 | 0.4 on the third | 0.12 each | 0.6 | 0.8 / 0.8 / 1.2 | Shock 15 each | Parryable (first two); third is a two-claw rake with a final glint 0.2 s before |
| | Rebound leap | 0.70 | 0 | 0.20 | 0.8 | 1.2 | Shock 25 | Springs off a rock face, rod or tree to come at you from above (folklore: it leaps wildly in storms). Only with a surface within 6 m |
| | Discharge curl | 0.90 | 0 | 0.20 | 1.2 | 1.8 | Shock 80 | **Unblockable** (crimson flare in the eyes). Curls into a ball, fur halo blazing, then bursts 4 m round itself |
| | Reads action | — | — | — | — | — | — | Drink within 8 m: pounce in 0.4 s |
| | Storm return | — | — | — | — | — | — | After 60 s in combat, or when the storm ends, it leaps into the next strike near it and is gone (no drops, no XP). Kill it before then |
| **Cragstrider** (neutral) | Horn jab | 0.55 | 0 | 0.12 | 0.8 | 1.0 | — | Parryable |
| | Six-leg kick | 0.70 | 0 | 0.15 | 0.8 | 1.2 | — | `behindOnly`; knockback 500 |
| **Galeback** (neutral) | Head shove | 0.80 | 0 | 0.20 | 0.9 | 1.2 | — | Knockback 600 |
| | Trample | 1.00 | 0 | 0.40 | 1.0 | 1.5 | — | Hyperarmor; 2 m wide path |
| | Herd ring | — | — | — | — | — | — | The herd forms a ring, horns out, young inside |
| **Coilburrow** (neutral) | Snout jab | 0.50 | 0 | 0.10 | 0.8 | 1.0 | Shock 10 | Parryable |
| | Burrow burst | 0.80 | 0 | 0.20 | 0.9 | 1.2 | — | Ground bulge and soil hop under you |
| | Nest swarm | — | — | — | — | — | — | 4 to 8 out when an egg chamber is touched |
| **Rodcrest, Puffram** | Cornered crest flash or butt | 0.60 | 0 | 0.10 | 0.8 | 0.7 | Shock 10 (Rodcrest) | Only when cornered; otherwise flee |

**Drops (GAP-COOKED; carcasses butchered hit by hit, BUILT pattern):**

| Creature | Loot |
|---|---|
| Voltfang | Raw meat ×2, Voltfang pelt ×2, bone ×1, storm essence ×1 (40%) |
| Thunderhorn | Raw meat ×5, hide ×3, Thunderhorn horn ×2, storm essence ×1 |
| Stormkite | Raw meat ×1, Stormkite feathers ×6, storm essence ×1 (50%) |
| Raiju | Raiju pelt ×1, storm claws ×2, storm essence ×2 |
| Cragstrider | Raw meat ×4, hide ×3, wool ×2 |
| Galeback | Raw meat ×6, hide ×4, Galeback hair ×4 |
| Coilburrow | Raw meat ×1, hide ×1 |
| Rodcrest | Raw meat ×1, copper scale ×2 |
| Puffram | Raw meat ×2, wool ×3 (shear alive: wool ×2 every 20 min, SPEC husbandry) |

**Tame uses (GAP-COOKED, within Raven's roles):**

| Tame | Use |
|---|---|
| Cragstrider | Mount, sprint 1,300 cm/s; climbs slopes up to 60° (SPEC from survival-crafting); jump 3 m |
| Galeback | Hauler 240 (SPEC); no gust push for rider or itself (SPEC); it walks the cliff paths safely |
| Coilburrow | Senses copper, tin and iron within 40 m: marks them on the owner's compass; digs 2 ore from any ore node per 10 min while following |
| Rodcrest | At a base: strikes don't land within 30 m (SPEC). Following you: strikes in your cell go to it, not you (it's unharmed); one per player counts |
| Voltfang | Pack fighter; its bites carry shock 15 |
| Thunderhorn | Fighter and slow mount; its charge launches enemies |
| Stormkite | Flying scout (not rideable): sent ahead, it circles a point and marks creatures within 50 m for 20 s |
| Raiju | Fighter; out of storms it is a normal hunter; in storms it gains +20% damage. Stays with its owner (no storm return once tamed) |

### 2.10 Mini-bosses (GAP-COOKED; names, fights and counters from FULL-GAME kept and made concrete)

Shared numbers (FULL-GAME 3.3 and combat.md 2.17, SPEC): 3,000 HP solo, +80% per extra player or tame, stance 90, 2 parries to break, chase 65 m, respawn 4 h, run-back stone 40 to 60 m from the arena, first-kill named drop flies to the killer. Weak to Blood & curse (×1.5, the opposite). Drops: trophy, upgrade shards ×3 (BO), storm essence ×4.

#### The Bellwether (roaming)

*Folklore and real root:* the bellwether, the belled lead ram a flock follows. The Rodwardens belled the greatest Thunderhorn to lead their flocks across the cliffs. It still wears the cracked bronze bell.

| Part | Detail |
|---|---|
| Body | A Thunderhorn grown to 3.4 m at the shoulder. Horns curled three full turns, each curl as thick as a man's waist, chipped from a hundred fights. A bronze bell 1 m tall hangs from a copper collar, cracked down one side |
| Silhouette | The huge triple-curl horns and the swinging bell. In the dark: the bell's tone and the white flash at each head-butt |
| Palette | Granite grey #8A8D91 wool, horn amber #A8823E with green copper tips, bell bronze #8C6A3A |
| Voice | Bighorn rut grunt, bison bellow, walrus roar (low end); **material:** the cracked bell clanking with every step, audible at 50 m |
| Route | A 2.6 km loop of ridgelines and cliff paths: the Bellwether's Fold (lair), the Thunder Rams' Ring, the cliff path under the Arch, the Struck Climbers' ledge, the Storm Mill shoulder, back to the fold. 3 m/s walk, rests 3 min at the fold. Diurnal; sleeps at the fold at night (sneak crit opener) |
| Herd | Walks with 2 to 4 Thunderhorns. They join any fight within 30 m |
| Lock points | Head (horns), bell, chest |

| Attack | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Thunderclap butt | 0.70 | 0.3 | 0.15 | 0.8 | 50 + shock 30 | Parryable. Launch |
| Hook, hook, clap | 0.60 / 0.50 / 0.70 | 0.4 on the clap | 0.15 each | 1.0 | 35 / 35 / 55 | The clap is not parryable |
| **Ridge charge** | 1.10 | 0 | up to 3.0 | 1.2 if it misses | 75 | **Unblockable**. Two hoof scrapes, the bell swings back. Tracks for the first half of the wind-up, then a straight line at 1,200 cm/s up to 35 m. Knocks you 900 cm sideways (off a ledge if there is one) |
| Stamp quake | 1.00 | 0 | 0.20 | 1.0 | 45 | 6 m ring travelling at 1,200 cm/s, `low` (jump it) |
| Bell toll | 0.90 | 0 | 0.40 | 0.8 | 0 | Shakes its head: a 10 m sound ring drains 40 stamina from everyone hit (block doesn't stop it). Once per fight below 60% HP it calls the herd: 2 more Thunderhorns arrive within 20 s |

**Counter mechanics:**
1. **Sidestep a charge into rock** (a cliff face, a boulder, a ruin wall): horns stuck, **5 s stun**, ×1.5 damage and ×2 stance damage taken.
2. **Bleed cuts its charge regen** (SPEC from FULL-GAME): the charge has a 10 s cooldown; while bleeding it can't charge, and each bleed proc adds 8 s to the cooldown.
3. **Break the bell** (part, 400 HP): no more tolls or herd calls, and a 3 s stagger.

**Trophy:** "Bellwether horn" (talisman: knockback taken −30%, SPEC survival-crafting). **Extra:** "Cracked bell bronze" ×1 (crafting extra: +15% stamina damage on blocked hits).

#### Galvara, the Kite Queen

*Real root:* the banner cloud that streams off a lone peak. Galvara fans and churns it; she lives in it.

| Part | Detail |
|---|---|
| Arena | **Galvara's Eyrie** (L5): the needle top, 70 m by 45 m, at 760 m, sheer drops on all sides (no railings, falls kill). Above 600 m: the strike hazard is live in storms on top of her own strikes. The banner cloud streams east from 20 to 40 m above the top |
| Body | A Stormkite matriarch: 14 m wingspan, a whip tail 7 m long ending in a scissor fork, long secretary-bird legs with heavy talons, a crest of quills. Violet-white light runs along her flight feathers |
| Her cloud | Three **thunderhead cores**, glowing orbs 1.5 m across, orbit 8 m under her inside the cloud at 25 to 30 m height. They are shootable lock points |
| Silhouette | Fork tail and long legs against the cloud; the three orbs glowing in it |
| Palette | White #EDEFF2, slate #3E4650, black wing edges, violet-white #B9A7FF |
| Voice | Swallow-tailed kite whistle, harpy eagle scream pitched up, gull chorus; **material:** crackling static in her cloud |
| Lock points | Body, core 1, core 2, core 3 (in the air); head, body (grounded) |

**Air phase (start):**

| Attack | Wind-up | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|
| Rain of strikes | 1.0 s glow per strike | 0.1 each | — | 45 + shock 40 each | One strike per living core, on the spots of up to 3 players (or the same player 0.4 s apart). Glows on the ground, never homes. Every 6 s |
| Kite call | 1.0 (a shriek) | — | — | — | 2 Stormkites dive from the cloud. Cooldown 25 s; at most 4 alive |
| Tail lash pass | 0.90 | 0.5 | 1.0 | 50 + shock 30 | She drops out of the cloud and sweeps along a straight line across the top at 3 m height, tail sparking. The line shows as a swirl in the cloud 0.9 s before. Jump or roll |
| Gust beat | 1.00 | 0.6 | 0.8 | 0 | A wingbeat cone, 40°, 25 m: push 700 cm toward the nearest edge. Brace (hold Block) halves it |

**Counter:** shoot the cores. Each core has 400 HP (arrows and spells ×1.5 against cores; melee can't reach). Each broken core removes one strike from the rain. **When the last core breaks,** the cloud tears, and she drops to the crag: **6 s stun**, ×1.5 damage taken.

**Ground phase:**

| Attack | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Talon stomp, stomp, rake | 0.55 / 0.45 / 0.70 | 0.3 on the rake | 0.12 each | 0.9 | 35 / 35 / 50 | First two parryable |
| Tail whip | 0.80 | 0 | 0.30 | 0.8 | 45 | 360°, 6 m, `low` |
| Wing-claw slash | 0.60 | 0 | 0.15 | 0.8 | 40 | Parryable |
| Retake the sky | 1.2 | — | — | — | — | If she takes less than 150 stance damage in any 20 s on the ground, she leaps back up and reforms her cloud with 1 core (500 HP) |

**Trophy:** "Kite crown" (talisman: ranged hits from above +10%, SPEC). **Extra:** "Kite tailfeathers" ×6 (fletching: arrows fly 10% faster).

#### Coilback

*Real root:* the star-nosed mole's 22 feelers, the pangolin's scales.

| Part | Detail |
|---|---|
| Arena | **The Copperfall** bowl (L7): 55 m across, scree and verdigris pools, copper veins showing in the rock. It hunts here |
| Body | A Coilburrow grown to 9 m long and 2.2 m high. Its back is armoured with overlapping plates crusted with copper ore. Its snout is a star of 22 pink, fleshy feelers, each as long as an arm, that crackle with sparks when they sense metal. Spade claws 60 cm long. Tiny eyes |
| Silhouette | The plated hump and the writhing star nose |
| Palette | Black fur, copper #B87333 and verdigris #4E9A84 plates, flesh pink #D88A8A star |
| Voice | Mole-cricket churr pitched down, pangolin hiss, badger snarl; **material:** grinding scree and a rising electric buzz before each ring |
| Senses metal | It targets the player wearing the most metal (heavy, then medium load) first |
| Lock points | Star nose, head, back |

| Attack | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Ring pulse** (from underground) | 1.00 | 0 | 0.3 per ring | — | 30 + shock 40 per ring | SPEC "electrifies the ground in rings": copper veins light up in rings round its hidden spot with a buzz, then 1 to 3 rings of current spread at 900 cm/s, 0.5 m tall, `low`. Jump them (leg i-frames, SPEC counter) |
| **Surface burst** | 0.90 | 0 | 0.20 | 1.4 | 70 | **Unblockable**. Ground bulges and scree hops in a 6 m circle under its target, then it erupts |
| Star lash | 0.50 | 0 | 0.15 | 0.7 | 35 + shock 20 | Parryable. The feelers whip in a 3 m cone |
| Claw sweep | 0.80 | 0.3 | 0.20 | 0.9 | 50 | 180°, 5 m |
| Scree spray | 0.90 | 0 | — | 0.8 | 5 × 12 | Digs and flings 5 stones in a 30° fan at 1,200 cm/s, never home, not inside 4 m |
| Tunnel | 0.6 dive | — | 3 to 6 s under | — | — | A moving soil ridge shows its path |

**Counter mechanics:**
1. **Jump the rings** (SPEC).
2. **Frost slows its tunnelling** (SPEC): a frost proc makes it tunnel at ×0.4 for 12 s and leaves a frost-rime line on the surface along its path, so its surfacing spot shows.
3. **Break the star nose** (part, 300 HP): it can't sense metal for 20 s and surfaces at random spots, slower (burst wind-up 1.2 s).
4. **Take your metal off:** unequipping metal pieces moves its target pick to someone else (the real BotW lesson, here as a group tactic).

**Trophy:** "Coil heart" (talisman: leg i-frames on jumps +0.06 s, SPEC). **Extra:** copper ore ×20, "Coil plate" ×2 (crafting extra: +10% lightning resistance on armour).

### 2.11 Major boss: Kharvox, the Storm Roc (GAP-COOKED; FULL-GAME draft kept and made concrete)

**Who:** the thunderbird of the Rodwardens, part god, part monster. They fed their dead to it on the Feeding Stones; it took their rods for its nest.

**Arena: the Summit Nest** at (−200, −3,300), 1,600 m (SPEC site).
- A granite bowl 60 m across (arenaRadius 30 m, SPEC major), its floor glazed with fulgurite.
- The nest: a 14 m ring of whole pine trunks and hundreds of stolen copper rods twisted in like twigs, on the north side. It is solid cover (blocks eye bolts).
- **Twelve old Rodwarden rods** stand round the rim at 26 m from the centre, each 8 m tall.
- The **ossuary slab** on the east: a flat rock covered in smashed bone, copper torcs among the bones (the lammergeier habit).
- The Rust Pools are on a ledge 60 m below (2.4 #23).
- **Always a storm:** the summit is under its own cloud during the fight, whatever the weather.
- **Run-back stone** 50 m down the Last Stair. **Flyers can't enter** (SPEC arena roof rule). The altar (DD SPEC: "its arena holds the biome's altar") stands beside the ossuary slab: a bronze rod-altar.

**Body.**
- 22 m wingspan, 9 m beak to tail, about 6 m tall standing.
- **Face:** a lammergeier's: a black mask through the eyes, a beard of black bristles under the beak, and a red ring round each pale eye.
- **Chest and neck:** rust-orange, stained in the Rust Pools.
- **Feathers:** slate-black primaries with copper-bright tips that light violet-white from wingtip inward as it charges. A long wedge tail.
- **Legs:** feathered, with talons the size of a man.

**Silhouette:** the vast wedge tail and long narrow wings; the bearded head. At night, the charge glowing along its wing edges.
**Palette:** slate #2E333A, rust #9A4A24, bone #D8D2C4, copper #B87333, charge violet-white #B9A7FF; crimson glints only on unblockable moves.
**Voice:** harpy eagle shriek pitched down an octave, lammergeier whistle, lion roar sub-layer, condor hiss. **Material:** each wingbeat is a thunder roll (thunderbird); the charge is a rising arc whine.

**Numbers (SPEC rules):** 12,000 HP solo, +80% per player or tame. Stance 160. 3 parries to break. Chase 90 m. Phase 2 at 50%, hidden phase 3 at 25% with artifacts (damage ×1.4). Weak to Blood & curse (×1.5). Group of 3: 10 to 15 min.
**Lock points:** head, chest, left wing, right wing. Each wing has its own part stance (300): breaking one grounds Kharvox for 6 s and drops "Storm plume" ×1 (once per wing per fight).

**Kharvox's Charge (GAP-COOKED; the Zinogre lesson).** It has a charge bar of 100, shown only on its body: feather tips light from the wingtips inward (25% per quarter).
- **Gains:** +6 per second while airborne, +10 per strike it calls, +3 per hit it lands.
- **At 100:** its next eye bolt forks to 2 more targets within 8 m, and its next dive's shockwave ring is doubled (two rings 0.4 s apart). Then the charge drops to 0.

**The opposite-element counter (SPEC from FULL-GAME: "bleed stops it rebuilding charge, and a curse grounds it").**
- **Bleed proc:** −30 charge, and no charge gain for 10 s. The feathers dim to grey: you see it work.
- **Curse proc:** Kharvox is grounded for 8 s. It crashes down, wings dragging; head lock point at ground height; damage taken ×1.25.

**Phase 1 (6 moves; one 3-hit combo, one grab):**

| Move | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| Wing buffet | 0.75 | 0 | 0.25 | 0.9 | 50, knockback 600 | Parryable. A wing raised and swept across the front, 8 m, 140°. Wing-edge socket trace |
| **Talon combo** (3 hits) | 0.60 / 0.45 / 0.70 | 0.4 on the beak | 0.15 each | 1.2 | 40 / 40 / 60 | Left talon, right talon, beak hammer down. First two parryable |
| **Talon dive** | 1.2 climb, then 1.0 from the fold | 0 | 0.3 | 1.6 (it shakes off) | 90 talons, 35 ring | Climbs 40 m, then folds its wings: its shadow grows on the floor. Aims at the fold; then a straight line, no tracking. Talons are **unblockable** (crimson glint at the fold). Impact: a dust-and-spark ring at 1,200 cm/s, 6 m, `low` |
| **Bone drop** (grab; the tame-punish move) | 1.00 | 0 | 0.20 | 1.4 | 30 grab + the fall | **Unblockable**, dodge only. A low swoop; on contact it carries the target up 15 m and drops them on the ossuary slab (fall damage by the built rule; 15 m hurts badly, it doesn't kill a full-HP target alone). Allies can free them: 150 stance on the head during the climb. **Targets the furthest tame first** (SPEC rule): a tame is carried 25 m and dropped |
| **Rod chain** | 1.20 | 0 | 0.40 | 1.0 | 45 + shock 50 | Screams and rakes the air: 3 random pairs of rim rods light at the tips (St Elmo's fire) with a buzz. Then arcs jump straight across between each pair, 0.4 s, visible as lines across the floor. Step off the lines, or roll through them (i-frames). The "chain lightning between the rods" (SPEC) |
| **Eye bolt** | 1.00 | 0 | 0.10 | 0.8 | 55 + shock 60 | Head rears, eyes flare (thunderbird). At 0.5 s the aim locks on the spot where the target was. A faint branching **stepped leader** crawls along the line for the last 0.5 s, then the bright stroke. Range 30 m, never inside 4 m, never homes. Blocked by the nest and rods |

Pattern rules (SPEC combat.md 2.17.1): bands close / mid / far; never the same move 3 times running; one ranged area in flight at a time in phase 1.

**Transition at 50%: the Thunderclap** (unblockable, crimson). It beats up to 10 m and claps its wings together over the arena (thunderbird).
- **Wind-up:** 1.4 s, the wings drawn far back.
- **Hit:** a 12 m ring at 1,200 cm/s, 60 damage, launch. It takes 50% damage during the wind-up (SPEC).
- **Arena change:** the clap knocks down 4 of the 12 rim rods (their stumps stay); the storm thickens to black; rain lashes.

**Phase 2 (2 new delayed attacks, 1 new combo, 1 arena change, 1 delayed variant):**

| Move | Wind-up | Delay hold | Active | Punish | Damage | Notes |
|---|---|---|---|---|---|---|
| **Called strikes** (arena change) | 3.0 body tell + 1.0 ground glow, as the hazard | — | 0.1 | — | 50 + shock 50 | Every 4 s a strike falls on one player's spot, in turn. **Rod shadow:** a player within 3 m of a standing rod's base is never picked; the rod takes the strike. Each rod melts and falls after 6 strikes. The safe ground shrinks as the fight goes on |
| **Storm pillar** (new delayed 1) | 1.6 | up to 0.6 | 0.5 | 1.3 | 80 + shock 60 | Hovers 12 m up, feathers glowing inward; a final glint 0.2 s before contact, then a 5 m column of lightning straight down under it. **Unblockable** |
| **Delayed talon swipe** (new delayed 2) | 0.70 | up to 0.6 | 0.15 | 0.9 | 60 | Raises one foot high and holds. Final glint 0.2 s before. Parryable |
| **Sky combo** (new combo) | dive 1.0, then 0.45 / 0.75 | 0.3 on the buffet | as their rows | 1.6 | 90 / 40 / 50 | Talon dive, landing rake, wing buffet, then takes off |
| **Double eye bolt** (delayed variant) | 1.00 + 0.6 | — | 0.10 each | 1.0 | 55 each | A second flare after the first stroke: the second bolt re-aims once, 0.5 s before it fires, at where you dodged to; a second stepped leader shows it |
| Bone drop, Talon combo, Wing buffet, Rod chain | as phase 1 | | | | | Idle gaps 0.3 to 0.9 s (SPEC). Rod chain uses only standing rods |

**Hidden phase 3 at 25% (artifacts only; SPEC flag): the Eye of the Storm.**
- **Arena:** every remaining rod melts in one burst. Called strikes now fall every 2 s.
- **The eye:** a 10 m circle of calm moves round the arena at 100 cm/s. A hole in the cloud lets sunlight in on it. No called strikes land inside the eye.
- **Kharvox flies only.** It never lands except to grab (Bone drop). It uses dives, eye bolts and storm pillars from the air.
- **Damage ×1.4** (SPEC).
- **To hit it:** ranged attacks, curse procs (grounded 8 s, as in phases 1 and 2), wing breaks, and parrying its landing rakes after a grab attempt.

**Drops:**
- Boss core "core_kharvox" (Hearth tech, SPEC).
- Storm essence ×8 (path material).
- **Boss part: Stormheart** (SPEC from FULL-GAME: every fifth hit chains to a second enemy within 6 m for 40% of the hit). Chance by `bosses.partChance` (0.25, BUILT).
- First kill (named, flies to the killer): **"Crown Plume of Kharvox"**, a trophy-mount item and talisman (Wildbrand charge time −15%).
- Kharvox feathers ×6 (Stormweave extra, 2.17).

**Altar:** the Lightning subclass unlocks here (BUILT `WRAltar.cpp` pattern).

**Music:** STO_Boss_Kharvox P1 / P2 / P3 (2.12).

### 2.12 Voices and music (GAP-COOKED; music tempo and key SPEC from FULL-GAME 3.3)

**Creature voices.** Fire-frost 3.5 rules (SPEC): four layers per attack (tell, strike, hit, pain) plus idle, aggro and death; at least 3 real animal calls and one material layer per species; tells mixed 3 dB over the bed and rising in pitch.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Voltfang | Grey wolf howl, dhole whistle-call, wolverine growl (attack) | Electrical crackle (FULL-GAME example, SPEC) | A howl that breaks into crackling static at its peak |
| Thunderhorn | Bighorn rut grunt, musk ox roar, red stag roar | Horn-clash crack (a real bighorn clash, recorded far off) | The clap of the head-butt, heard 1 km off |
| Stormkite | Swallow-tailed kite whistle, peregrine falcon cack, osprey cry | Wind tearing across a sail | The rising whistle-scream as it folds to dive |
| Raiju | Fisher cat scream, bobcat yowl, wolverine snarl | Arc welder buzz and a sizzling crack | A scream that arrives with the thunderclap of its strike |
| Cragstrider | Mountain goat sneeze-whistle (alarm), ibex bleat, chamois whistle | Hooves on slate | The alarm whistle that freezes the area |
| Galeback | Yak grunt, bison huff, camel groan | Hair thrashing in wind | Low grunt chorus as the herd rings up |
| Coilburrow | Mole cricket churr, badger chitter, hedgehog huff | Soil crumble plus a faint spark tick | Spark ticks speeding up near ore |
| Rodcrest | Monitor lizard hiss, chuckwalla puff, gecko chirp | Copper sheet flex | A metallic "tunk" as it raises its crest |
| Puffram | Sheep bleat (SPEC example "sheep + wind"), lamb bleat, goat bleat | Wool static crackle | Static crackle in storms |
| The Bellwether | Bighorn rut grunt, bison bellow, walrus roar | The cracked bell's clank with every step | The bell heard at 50 m before you see him |
| Galvara | Swallow-tailed kite whistle, harpy eagle scream pitched up, gull chorus | Crackling static inside her cloud | A long whistle as the cloud tears |
| Coilback | Mole cricket churr pitched down, pangolin hiss, badger snarl | Grinding scree and a rising buzz before each ring | The buzz under your feet |
| Kharvox | Harpy eagle shriek an octave down, lammergeier whistle, lion roar (sub layer), condor hiss | A thunder roll on each wingbeat; an arc whine as it charges | The Thunderclap: a scream, then a real thunderclap |

**Music: Storm peaks, 92 BPM, E Phrygian (SPEC).** Driving low strings, timpani, thunder used as percussion, sudden rests (SPEC). Added on top: the fujara (Slovak overtone flute) for the high open land, cimbalom tremolos for flickering lightning figures, an open-throated women's choir for the summit, and frame drum. The rests are written into the pieces (planned), never engine gaps. Rules are SPEC from fire-frost 3.6: always audible, combat layer bar-synced in the same key and tempo, duck at most 4 dB, −18 LUFS (calm −23, boss −15), true peak −1 dBTP.

| Cue | Length | Use | Feel |
|---|---|---|---|
| STO_Explore_A / B / C | 3 to 4 min each | Day. A: S1, S2. B: S3. C: S4, S5 | A: driving cellos and basses under a fujara melody, sudden 1-bar rests. B: a cimbalom ostinato with timpani rolls, echoey for the gorges. C: choir drones over low strings, thunder hits on the downbeats |
| STO_Night_A / B | 3 min | Night | Fujara alone over a string pad; distant thunder; cimbalom shimmer |
| STO_Storm (stem) | layer | During a Thunderstorm, on top of the explore piece (world-map storm-stem SPEC) | Timpani and low brass swells, thunder samples placed on the beat |
| STO_Combat (stems for A, B, C) | loops | Combat | Strings in driving 8ths, timpani doubles, stabs |
| STO_Mini | 3 min loop | Bellwether, Galvara, Coilback | A shared theme; each mini gets a 4-bar intro motif: tuned bell for the Bellwether, choir whistle for Galvara, low cimbalom rumble for Coilback |
| STO_Boss_Kharvox P1 / P2 / P3 | sections | Phase changes resequence on the bar | P1: string and timpani march with choir. P2: everything doubles, real thunder in the percussion, the choir climbs. P3 (the eye): one held choir chord inside the eye, the full storm outside it, crossfading as the eye moves past you |
| STO_HollowSpire | 4 min | The Hollow Spire | Low drone, the Ringing Hall's pipe chord as a recorded texture, slow cimbalom |
| STO_Monastery | 3 min | S5 and the Ring approach | Choir and fujara, slow; the most beautiful cue in the biome |
| STO_GreatStorm | layer | The event | War timpani and choir over whatever plays |
| STO_Camp | 2 min | Camps and your Hearth in Storm peaks | Fujara and soft cello, at rest |
| PASS_Stormgut (duet; world-map SPEC: one per pass) | 3 min | Stormgut | Storm's timpani and fujara against the Swamp's cello and bowed metal. In E Phrygian over a C pedal, which nods to the Swamp's C minor |

**Sources (SPEC from fire-frost 3.6):** licensed real compositions first (CC0 / CC-BY: OpenGameArt, Free Music Archive, Pixabay Music, incompetech), matched to tempo, key and instruments. Boss tracks are commissioned once there is revenue. No AI generation. Every piece is credited in `Docs/CREDITS.md`.

**Ambient beds:** five, one per sub-zone (2.2), plus the Hollow Spire and the Rodfield's wind-song (wind through rods: a real aeolian-harp recording).

### 2.13 Shrines and path master (RAVEN-PLANNED exist; who and where GAP-COOKED; shrine rules SPEC from fire-frost 3.1)

| Shrine | At | Guard |
|---|---|---|
| **Great Shrine of Strikerock** | (−1,880, −3,350), S2 | None (path master) |
| Windgap Shrine | (−2,300, −4,300), S1, in the Northgap | 2 Voltfang packs |
| Stormgut Mouth Shrine (the pass's west lesser shrine, world-map SPEC) | (−650, −4,880), S3 | A Thunderhorn pair and a Stormkite |
| Glass Scar Shrine | (−1,020, −2,700), S4, 960 m | 2 Thunderhorns; in storms, a Raiju too |
| Hanging Shrine | (−800, −3,420), S5, 880 m | A Voltfang pack on the galleries |

Each pays and works as fire-frost 3.1 (SPEC): lesser 1,000 Lightning XP (or 500 to each owned path), Great 2,000 on first visit, fast travel between discovered shrines, re-guard every 4 game days. **Biome look:** the shrine braziers here are copper rod-tips; lighting one sends a small spark up the rod.

**Path master: Vesna Groza** ("spring storm" in Slavic).
- **Look:** a woman of about 30 with a climber's build: long, strong arms and shoulders, a lean waist, powerful legs. Wind-burned olive skin. Black hair cut short on the struck side and long on the other, worn in a loose braid. A sleeveless quilted climbing vest open at the throat, rope coiled across her body, bare arms.
- **Her scar:** a Lichtenberg fern spreads from her left shoulder down the arm and up the side of her neck to the jaw. She was struck on the Crown and lived. It is the Lightning body change at full strength, shown on a real person. Her eyes have the violet-white spark ring.
- **Voice:** quick and dry, with a short laugh.
- **Where:** by day, at her climbers' camp in Strikerock's split, sharpening pitons. At night, asleep in the lean-to (still talkable). No cutscene, no camera grab (SPEC).

**Quests: summit quests (in the Journal):**

| # | Quest | Task | Pays |
|---|---|---|---|
| 1 | Glass | Bring 6 stormglass (you must be out after a storm) | 800 Lightning XP; recipe Grounding salve |
| 2 | The rod-road | Walk the rod-road from camp 2 to the Thunderstone Arch, touching the 8 rods on the way (each sparks) | 1,000 XP; the rod-road's waymarks light at night for you |
| 3 | Hair on end | Stand above 600 m in a Thunderstorm and live through 3 strike pre-tells without being struck | 1,200 XP; recipe grounding charm |
| 4 | Nine spans | Cross the Bridge of Nine Spans and light the lanterns in its 8 winch huts | 1,500 XP; she mentions the rope on the broken span (ST-S1 hint) |
| 5 | Banner | Show her the Kite crown | 2,000 XP; recipe stormglass arrows |
| 6 | The bell | Kill the Bellwether | 3,000 XP; 2 upgrade shards |
| 7 | The Crown | Reach the Ring by the Hanging Monastery and light the Hanging Shrine | 3,000 XP; she moves her camp to the Ring for good, as a second place to find her |

### 2.14 The story the land tells (GAP-COOKED; environment only, DD SPEC)

**The Rodwardens** (world-map SPEC name): keepers of rods, a Perun-like people of the high stone.

| Beat | Told by |
|---|---|
| **They feared and worshipped the strike** | Rods everywhere; thunder marks carved on every lintel; a thunderstone in a niche in every house; bidental rings where people died |
| **They fed the sky** | The Feeding Stones face the Spire, with torcs in the cracks. The Torc Road climbs the last stair, its torcs getting smaller. The Torc Vault's chains end in empty hooks. They gave their dead to Kharvox, then more than their dead |
| **The struck were holy** | The Melted Cell at the monastery's top, open to the sky, a torc melted into a Lichtenberg burn. Vesna's own scar: a living person who was struck and came down |
| **They warred with the Mire Coven** (Perun and Veles) | The Stormgut fort faces east, with Coven bone hooks in its walls and a smashed willow-stake shrine at its foot. The Burnt Drake lies in the gut with rods through its ribs. Across the pass (Swamp lane), the roc champion lies bled out with bone hooks in its wings (world-map SPEC) |
| **Kharvox took the rods** | Hundreds of copper rods twisted into the summit nest; pellets of bone and copper wire in the Feather Shaft |
| **The last keeper** | The Keeper's Cell: a skeleton facing the Nest Door with a rod across its knees |
| **The climb of the twelve** (shared Crown story) | The Lichtenberg Wall's carved climbers with the top figure struck out; the Choir's hymn ("Twelve went up. Eleven came down."); the Ring's violet ray pointing at the Glass Scar. Nobody explains it (world-map SPEC) |
| **The newcomers** | Camps at the wrecks; fresh rope on Vesna's pitons next to Rodwarden chains gone green |

### 2.15 Event: the Great Storm (GAP-COOKED; framework SPEC from fire-frost 3.1 and world-map 2.7)

One game day, about once every 3 real days (server setting, SPEC). The Ring's violet-white pillar burns as the world-visible sign (world-map SPEC). It never touches spawn safe zones or claims.

| Time | What happens |
|---|---|
| −5 min | A wall of black cloud rises over the Crown, visible from the whole map. The Ring's Lightning pillar burns violet-white. Every rod in the biome hums and its tip glows. Wind rises to 20 m/s. STO_GreatStorm fades in |
| 0 to 10 min, **The Front** | Thunderstorm over the whole biome with heavy rain (Wet everywhere, SPEC Rain effects). Gale over the whole biome, not just above 300 m |
| 10 to 50 min, **The Storm** | **Strikes everywhere:** below 600 m, one per 60 m cell's highest player every 30 to 50 s; above 600 m, every 10 to 20 s. Same tells (3 s body + 1 s ground) and same counters. Strikes never land within a claim or within 50 m of a spawn camp. **Raiju** arrive on 1 in 6 strikes near players (at most 2 per player at once). Ball lightning across S4 and S5. Stormkites hunt in flocks of 4. The Hollow Spire's Choir opens (H8). Storm coil machines run at ×3 (survival-crafting SPEC) |
| 50 to 60 min, **The Clearing** | Strikes and rain stop. Sun through broken cloud, steam off the rock |
| After | Stormglass nodes everywhere strikes fell (×3, SPEC payout); fulgurite fields full; the Glass Scar glows for 2 game hours. Puffram wool is charged wool for 1 game hour |

### 2.16 Invasions: Lightning and Blood through Stormgut (3.3 rule SPEC; Storm side GAP-COOKED)

**Storm invades the Swamp (through Stormgut):**
- 8 to 14 creatures: Voltfang packs at night, or Thunderhorns with Stormkites by day, plus 1 to 2 Raiju if a storm is on.
- **The Bellwether leads it** with his herd. He leaves his route for the invasion.
- **The invaders bring the storm.** A Thunderstorm state follows the invasion for its length. Its strikes keep the above-600 m rule, so in practice they fall on the Crown's east cliffs and in Stormgut.
- **Payout:** storm essence plus the Swamp's material (3.3 SPEC rule).

**Swamp invades Storm peaks:** its creature list belongs to the Swamp cook (R2). On Storm's side, the invaders cross Stormgut and march on the Stormgut Mouth Shrine or the nearest claim within 1.5 km. Strikes treat invaders like any creature, and they don't resist strikes (they aren't from this biome), so defenders who fight them high up in a storm get help from the sky.

### 2.17 Gear from this biome (SPEC from the other cooks, plus GAP-COOKED items)

| Item | What | Tag |
|---|---|---|
| **Stormweave** (biome set, Hearth 4) | Light, poise 30. Steel plate 6, stormglass 8, Puffram wool 10, copper 6. 2 pieces: shock buildup ×0.5. 4 pieces: never targeted by storm strikes; sprint stamina −25% | SPEC (survival-crafting) |
| Stormweave look | Stormglass rods in woven grey wool; fulgurite spikes rising from the shoulders like a crown of glass; a long tattered half-cape; slate with violet-white glints; forearms and face open | SPEC (hero.md owns looks). survival-crafting describes a quilted oilcloth coat with copper wire and goggles. **Merge:** hero.md's shapes, with survival-crafting's copper wire stitched in lightning lines through the wool and goggles worn up on the brow |
| Stormweave extras | Charged wool (sheared in a storm) or Kharvox feathers as optional inputs: +1 roll on the quality chance | GAP-COOKED |
| Grounding charm | Talisman, Armorer 2: stormglass 2, copper 3. Passes strikes | SPEC (survival-crafting) |
| **Grounding salve** | Early pass, 10 min: no stun, strike damage ×0.5 (SPEC world-map). **Recipe:** alchemy table, Hearth 1: Rodcrest copper scale ×2, pine resin ×2, rock salt ×1 | SPEC effect / GAP-COOKED recipe |
| Shock-resist potion | Gentian + stormglass + slime: shock buildup ×0.5, 10 min | SPEC (survival-crafting) |
| **Stormglass arrows** | Workbench 2: stormglass ×1, wood ×1, Stormkite feather ×1 → 4 arrows. +25 shock per hit; pierces one target | GAP-COOKED |
| **Rodwarden spear** | Forge, Hearth 2: copper ingot ×6, wood ×4, thunderstone ×1. A spear (combat.md class) whose element is Lightning without storm essence. Its charged heavy can be thrown 15 m; pick it up to get it back | GAP-COOKED |
| Storm essence | Path material: sets Lightning as an item's element at crafting (CF SPEC rule) | GAP-COOKED item, SPEC rule |
| Thunderstone | Crafting extra: +10% shock buildup on a weapon | GAP-COOKED |
| Trophy talismans | Bellwether horn, Kite crown, Coil heart (SPEC survival-crafting); Crown Plume of Kharvox (Wildbrand charge time −15%) | SPEC / GAP-COOKED |
| Boss part | Stormheart | SPEC (FULL-GAME) |
| Building set | **Crag hold** (slate and granite, copper flashing, storm shutters) | SPEC (survival-crafting) |
| Biome machine | **Storm coil** | SPEC (survival-crafting) |
| Crops, fish, dish | Stormrye, Gentian; ridge trout, Thunder grayling, the Lightning Pike; Stormrye flatbread | SPEC (survival-crafting) |

### 2.18 The Lightning path (FULL-GAME 4.2 row)

#### 2.18.1 What stays SPEC

From ST (Raven):
- Root passive **Grounded**: immune to lightning stuns, including from enemy storms.
- Resource **Charge**: fills fast, empties fast, best in quick bursts (DD).
- Every node name, cost and text below, including the capstones Tempest, Lightning step and Thunder god.

From the other cooks:
- Ability numbers from `combat.md` 2.12 and 2.13.
- The Shock status from `combat.md` 2.11.
- The body change from `hero.md` 2.6.

#### 2.18.2 Charge (resource numbers GAP-COOKED)

Charge is a **builder** (combat.md SPEC mode): your lightning hits and spells add to it, and it drains fast once you stop.

| Key | Value | Why |
|---|---|---|
| `chargeMax` | 100 | Same scale as Heat and Chill |
| `chargePerHit` | 10 per hit with a Lightning-element weapon or ability | Fills fast (Heat is 8, Chill 4) |
| `chargeDecayDelay` | 1.5 s | Empties fast (Heat waits 4 s, Chill 8 s) |
| `chargeDecayPerSecond` | 25 | A full bar is gone in 4 s without fighting |
| `chargeDamageBonusAtHigh` | +20% lightning damage while Charge ≥ 60 | Rewards keeping the burst going |
| `chargeMoveBonusAtHigh` | +5% move speed while Charge ≥ 60 | "Burst speed" (DD theme) |
| `chargeCapacityPerRank` | +15 | Same as Heat and Chill capacity |
| Bar colour | Violet-white #B9A7FF; it crackles at 60 and above | Path trim colour (hero.md) |

#### 2.18.3 The tree: every node, with gates and numbers

Gates follow `fire.json`'s rule: a node's gate is the points spent in its branch on the nodes before it. Costs and texts are SPEC (ST); kinds, gates and numbers are GAP-COOKED. Ability numbers marked (combat) are SPEC from `combat.md`.

**Root passive: Grounded** (main path only).
- No stun or stagger from any lightning source: shock's stagger, storm strikes, Kharvox's and Galvara's strikes, other players' Thunderclap.
- Storm-strike damage ×0.5, and the strike picker skips you while anyone else in the cell can be picked.
- As a subclass it shows, detached and inactive (SPEC rule).

| Branch | Node | Kind | Cost | Gate | Effect (numbers) |
|---|---|---|---|---|---|
| Trunk | **Spark** | ability (spell) | 3 | — | Bolt: 16 damage, shock 35, 3,200 cm/s, cooldown 1.0 s, +8 Charge (combat). The starter |
| Trunk | Charge capacity | stat, 3 ranks | 3 | — | +15 max Charge per rank |
| Trunk | Shock chance | stat, 2 ranks | 2 | — | Each weapon hit has a 5% chance per rank to add a free 25 shock buildup |
| Trunk | **Static strike** | ability (Wildbrand) | 2 | — | All melee; +15 Charge; cooldown 2.5 s; 0.24 / 0.16 / 0.38; ×1.2 damage; shock 45; ×1.5 stance. A thrust that arcs to 1 more target within 4 m at 40% (combat) |
| Storm | **Chain lightning** | ability (spell) | 5 | 0 | Chain: 20 damage, shock 30 per jump, 2 jumps of up to 8 m each, cooldown 6 s, +12 Charge (combat) |
| Storm | Arc | stat, 5 ranks | 5 | 5 | +1 jump per rank for Chain lightning and for Static strike's arc (combat) |
| Storm | Conductive | passive | 3 | 10 | Soaked enemies take +25% lightning damage (combat soak row) |
| Storm | **Thunderclap** | ability (spell) | 4 | 13 | Nova: 18 damage, shock 60, stance ×2, radius 450 cm, cooldown 10 s (combat). Rooted cast, 0.35 s wind-up |
| Storm | Overcharge | stat, 3 ranks | 3 | 17 | +8% lightning damage per rank while Charge ≥ 95 |
| Storm | **Tempest** (capstone) | ability (spell) | 10 | 20 | A storm of radius 900 cm round you for 10 s: a strike every 0.8 s on a random enemy inside (30 damage, shock 40), cooldown 45 s (combat). Each strike shows the 1.0 s ground glow to enemies and other players, like every strike |
| Surge | **Bolt dash** | ability | 5 | 0 | Dash 600 cm through enemies: 12 damage and shock 40 to each one passed; i-frames during the dash; cooldown 6 s; spends 20 Charge (combat) |
| Surge | Swift | stat, 5 ranks | 5 | 5 | +3% move speed per rank (all sources capped at +25%) |
| Surge | Afterimage | passive | 3 | 10 | A dodge leaves a spark where it started; 0.6 s later it bursts: 2 m, 10 damage, shock 30. At most one per second |
| Surge | Rapid fire | passive | 4 | 13 | While Charge ≥ 70: light and heavy attack animations play ×1.12 faster, and spell cooldowns run ×0.85 |
| Surge | Recharge | stat, 3 ranks | 3 | 17 | +10% Charge gained per rank |
| Surge | **Lightning step** (capstone) | ability | 10 | 20 | Bolt dash up to 3 times within 2.0 s (combat); the second and third cost no Charge; cooldown 12 s for the set |
| Conductor | **Charged weapon** | ability (Wildbrand) | 5 | 0 | All melee; spends all Charge (minimum 30); cooldown 8 s; 0.40 / 0.24 / 0.55; damage ×(1.0 + 1.5 × Charge%); shock 30 + 50 × Charge%; ×3 stance; hyperarmor. Hold to store more (+20 Charge a second from your bar) (combat) |
| Conductor | Capacitor | stat, 5 ranks | 5 | 5 | +6% Wildbrand damage per rank |
| Conductor | **Static field** | ability (spell) | 4 | 10 | Pool of radius 400 cm, 6 damage a second, shock 15 a second, 8 s, cooldown 14 s (combat) |
| Conductor | Discharge | passive | 3 | 14 | Blocking a hit releases a shock: 3 m, 8 damage, shock 30; cooldown 2 s. A parry doubles it |
| Conductor | Field strength | stat, 3 ranks | 3 | 17 | Static field +10% radius and +1.5 s per rank |
| Conductor | **Thunder god** (capstone) | ability | 10 | 20 | Self buff for 12 s: every weapon hit calls down a bolt on the target (20 damage, shock 20); cooldown 60 s (combat) |

Totals: trunk 10, each branch 30, tree 100 (SPEC shape).

**Proc feedback (GAP-COOKED, fire-frost RAISE 2 pattern):**
- Every Lightning hit plays a 0.08 s violet-white flash on the target and a crack layered over the weapon sound.
- Chain jumps draw a ribbon arc (0.12 s) from target to target with a zip.
- Conductive shows steam crackling off soaked targets.
- Afterimage leaves a translucent body-shaped spark.
- Discharge turns the block spark violet.
- Thunder god's bolts each show a 0.15 s ground glow under the target before they land, so other players can read them.

**Homing:** Spark follows the fire-frost RAISE 3 rule (`lockHomingPlayers` 0): a player's bolt never curves at another player.

#### 2.18.4 Combos with the built paths (CB names SPEC; numbers GAP-COOKED; buildable in R1)

These two pairs can be built in R1 because Fire and Frost exist. Each needs points in both paths at the SPEC thresholds (25/25 and 50/50) and lives in the new `Data/combos.json` (a RAVEN-PLANNED file in DATA_SCHEMAS).

| Pair | At | Name (SPEC) | Effect (SPEC text) | Numbers (GAP-COOKED) |
|---|---|---|---|---|
| Fire + Lightning | 25/25 | **Plasma** | Fire on shocked enemies arcs as burning lightning | Any fire hit on a shocked target arcs to 1 enemy within 5 m: 50% of the hit as fire, burn 30 |
| Fire + Lightning | 50/50 | **Meltdown** | Overheat explosions chain lightning to every enemy they hit | Each enemy the Overheat blast hits takes a chain: 20 damage, shock 40, jumping once more within 5 m. Needs Overheat (CB rule) |
| Frost + Lightning | 25/25 | **Brittle shock** | Shocks on frozen enemies shatter them | A shock proc on a Frozen target triggers Shatter (SPEC: 30 burst + stance ×2). Bosses get Rimed, not Frozen (combat SPEC), so this works on creatures only |
| Frost + Lightning | 50/50 | **Blizzard storm** | Lightning strikes inside your blizzard | While your Blizzard runs, a strike every 1.5 s on a random enemy inside it: 25 damage, shock 40, with the ground glow. Needs Blizzard (CB rule) |

The other eight Lightning pairs (Sun, Nature, Wind, Arcane, Blood, Tide, Necromancy, Spore) keep their SPEC names from CB and get numbers in their partner's round.

#### 2.18.5 Body change (SPEC from hero.md 2.6, restated)

- **Skin (mask B):** branching **Lichtenberg scars**, pale, that flicker white at 6 Hz for 0.3 s on each cast.
- **Eyes:** a violet-white spark ring.
- **Hair:** the tips lift +1.5 cm (WPO) and flicker in combat.
- **Sound:** a static crackle on cast.
- **Strength steps:** scars on the hands (0.3), arms and chest (0.6), full scars with flicker (1.0). Main path at full strength, subclass ×0.6; combat pulse ×1.6 for 1.5 s (SPEC).
- **Link to the hazard (GAP-COOKED):** the strike pre-tell's hair lift uses the same WPO channel at full strength on anyone picked, so a Lightning player reads it at once.

---

## 3. Plan: building it in Raven's repo

Raven's patterns (SPEC):
- Data first, every number in `Data/*.json` as a first draft.
- Abilities are `UWRGameplayAbility` classes registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority throughout.
- Generated data comes from `Tools/terrain/*.mjs`: edit the script, not the json.

This lane reuses frameworks the other cooks plan and does not rebuild them:
- **fire-frost:** `AWRShrine`, `AWRPathMaster`, `UWRQuestComponent`, `UWRBiomeEventSubsystem`, `world_pois.json`, `quests.json`, roaming-mini routes, `UWRBodyChangeComponent`, the attack-row runner.
- **world-map:** `world_layout.json`, `world_walk.json`, `UWRWeatherSubsystem`, `AWRLightningStrike`, `weather.json`.
- **combat:** shock status, Wildbrand and spell shapes, boss parts and lock points.
- **survival-crafting:** item, recipe, set and machine frames.

### 3.1 Data rows

| File | Change |
|---|---|
| **New** `Data/paths/lightning.json` | Same shape as `fire.json`: `id "lightning"`, `name "Lightning"`, `biome "storm_peaks"`, `starter "spark"`, `resource {id "charge", name "Charge"}`, `rootPassive {id "grounded", name "Grounded", text}` (ST text), trunk 4 nodes, branches `storm`, `surge`, `conductor` with the 2.18.3 ids (`spark, charge_capacity, shock_chance, static_strike, chain_lightning, arc, conductive, thunderclap, overcharge, tempest, bolt_dash, swift, afterimage, rapid_fire, recharge, lightning_step, charged_weapon, capacitor, static_field, discharge, field_strength, thunder_god`), `kind`, `cost` or `ranks`, ST `text`, and `gate` |
| `Data/tuning.json` `resources` | `chargeMax 100, chargePerHit 10, chargeDecayPerSecond 25, chargeDecayDelay 1.5, chargeDamageBonusAtHigh 0.2, chargeMoveBonusAtHigh 0.05, chargeHighAt 60, chargeCapacityPerRank 15` |
| `tuning.json` `spells` | Blocks `spark`, `staticStrike`, `chainLightning`, `thunderclap`, `boltDash`, `staticField`, `chargedWeapon`, `tempest`, `lightningStep`, `thunderGod` with the 2.18.3 numbers (fields mirror `fireBolt`, `flameSlash`, `frostNova`: damage or multiplier, shockBuildup, speed, radius, seconds, cooldown, charge) |
| `tuning.json` `nodeEffects` | `lightning.charge_capacity {maxCharge: 15}`, `lightning.shock_chance {shockChance: 0.05}`, `lightning.arc {extraJumps: 1}`, `lightning.overcharge {fullChargeDamage: 0.08}`, `lightning.swift {moveSpeed: 0.03}`, `lightning.recharge {chargeGain: 0.10}`, `lightning.capacitor {weaponArtDamage: 0.06}`, `lightning.field_strength {fieldRadius: 0.10, fieldSeconds: 1.5}` |
| `tuning.json` `passives` | `conductiveBonus 0.25`, `afterimageDelay 0.6, afterimageRadius 200, afterimageDamage 10, afterimageShock 30, afterimageCooldown 1.0`, `rapidFireAt 70, rapidFireAnimRate 1.12, rapidFireCooldownMult 0.85`, `dischargeRadius 300, dischargeDamage 8, dischargeShock 30, dischargeCooldown 2, dischargeParryMult 2`, `groundedStrikeMult 0.5`, `moveSpeedCap 0.25` |
| `tuning.json` `status` | Shock (combat.md 2.11 SPEC): `shockThreshold`, `shockStagger 0.4`, `shockStaminaLoss 20`, `shockChainShare 0.25, shockChainTargets 2, shockChainRadius 400` |
| `tuning.json` `hazards` | `strike {preTell 3.0, tell 1.0, damage 60, stun 0.8, interval [20,40], cell 6000, minAltitude 60000, heavyPickMult 1.25, weavePickMult 0.85, rodShadow 300, groundCurrent {radius 400, speed 1200, damage 15}, nodeChance 0.35}`, `gust {push 900, seconds 0.6, interval [8,15], tell 1.0, braceMult 0.3, crouchMult 0.5}`, `plank {tell 1.0, breakAfter 1.0, repairDays 2}`, `rockfall {tell 1.2, radius 300, damage 45}`, `ballLightning {form 1.5, speed 200, life 20, radius 300, damage 40, shock 60}` |
| `tuning.json` `creatureAI` | Every 2.9 row in the combat.md schema. New kinds: `static_shake, flank_lunge, charge_up, thunderclap_butt, ridge_charge, spark_darts, strike_arrival, claw_flurry, rebound_leap, discharge_curl, storm_return, six_leg_kick, head_shove, trample, snout_jab, crest_flash` |
| `tuning.json` `bosses` | `bellwether {chargeSpeed 1200, chargeMax 3500, stuckSeconds 5, stuckDamageTaken 1.5, chargeCooldown 10, bleedChargeAdd 8, bellPartHealth 400, tollStamina 40, tollRadius 1000, herdCallAt 0.6}`, `galvara {cores 3, coreHealth 400, coreRangedMult 1.5, strikeEvery 6, kiteCallCooldown 25, maxKites 4, dropStun 6, retakeStance 150, retakeWindow 20, reformCoreHealth 500}`, `coilback {ringSpeed 900, ringDamage 30, ringShock 40, burstRadius 600, frostTunnelMult 0.4, frostTunnelSeconds 12, nosePartHealth 300, noseBlindSeconds 20, metalTargeting true}`, `kharvox {stance 160, wingPartStance 300, wingGroundSeconds 6, chargeMax 100, chargeAir 6, chargePerStrike 10, chargePerHit 3, bleedChargeLoss 30, bleedChargeLock 10, curseGroundSeconds 8, curseDamageTaken 1.25, rods 12, rodRadius 2600, rodStrikesToMelt 6, rodsLostAtTransition 4, calledStrikeEvery 4, p3CalledStrikeEvery 2, eyeRadius 1000, eyeSpeed 100, boneDropHeight 1500, boneDropTameHeight 2500, boneDropFreeStance 150}`, plus per-attack rows (2.10, 2.11), `runbackStone` positions, `lockPoints` |
| `tuning.json` `events` | `greatStorm {warnSeconds 300, front [0,600], storm [600,3000], clearing [3000,3600], lowInterval [30,50], highInterval [10,20], raijuChance 0.1667, raijuMaxPerPlayer 2, claimSafe true, campSafe 5000, payout 3}` |
| `Data/creatures.json` | 9 new rows (`voltfang, thunderhorn, stormkite, raiju, cragstrider, galeback, coilburrow, rodcrest, puffram`): `biome "storm_peaks"`, tier, behavior, archetype, role, `element "lightning"`, `resists ["strike", "shock"]`, taming and food from 2.9, `attacks` lists, `activity` (Raiju `"storm"`, a new value), `loot` (2.9), mesh, materials and `anims` (one clip per attack). Mini rows `bellwether, galvara, coilback` (role `mini-boss`; Bellwether `roaming`, `route`, `herd`). Major `kharvox`. Tame-use fields: `senseOre` (Coilburrow), `strikeGround 3000` (Rodcrest), `climbSlope 60` (Cragstrider), `noGust` (Galeback), `scoutMark` (Stormkite) |
| `Data/spawn_regions.json` | `biome "storm_peaks"` regions per sub-zone S1 to S5 (polygons from `world_pois.json`), day and night tables (2.9 "Where"), plus a `weather: "thunderstorm"` table for Raiju |
| `Data/items.json` | `stormglass, storm_essence, thunderstone, voltfang_pelt, thunderhorn_horn, stormkite_feather, raiju_pelt, storm_claw, galeback_hair, copper_scale, charged_wool, kharvox_feather, storm_plume, cracked_bell_bronze, kite_tailfeathers, coil_plate, core_kharvox, part_stormheart, trophy_bellwether_horn, trophy_kite_crown, trophy_coil_heart, trophy_crown_plume, grounding_salve, stormglass_arrow, rodwarden_spear`, and the lore items of 2.7 and 2.4 (Keeper's tally, Torc of the given, Struck one's torc, Rod through a rib, Pipe-keeper's tuning rod, The count of the given, Choir's last hymn, Keeper's rod, First bell) |
| `Data/recipes.json` | Grounding salve, stormglass arrows, Rodwarden spear (2.17); plank repair (hand: wood ×2) |
| `Data/combos.json` (new, DATA_SCHEMAS) | `fire+lightning` (plasma 25, meltdown 50) and `frost+lightning` (brittle_shock 25, blizzard_storm 50) with 2.18.4 numbers |
| `Data/world_pois.json` | Storm peaks: sub-zone polygons S1 to S5, landmarks L2 to L10, the 29 hand sites, kit rules (2.4 table: kind, sub-zones, count, variants, spacing 150, max gap 250), shrines, run-back stones, the Bellwether's route spline, the Hollow Spire rooms and doors, rod-road waymarks |
| `Data/quests.json` | Vesna's 7 quests (2.13): task types bring, walk-touch, survive-pretell, light, trophy, kill, reach |
| `Data/weather.json` | `storm_peaks` biome row SPEC from world-map; add `strike` effect hooks (pre-tell, ground current) and the Raiju `weather` spawn tag |
| `Data/Audio/sounds.json` | `creatures.<id>` sets for 9 creatures and 4 bosses (2.12); pools `explore.storm_peaks.day/night`, `combat.storm_peaks`, `mini.storm_peaks`, `boss.kharvox` (3 sections), `sub.hollow_spire`, `sub.monastery`, `event.great_storm`, `camp.storm_peaks`, `layer.weather.storm.storm_peaks`, `music.pass.stormgut`; beds per sub-zone; `strike.pretell` (rising buzz), `strike.crack`, `strike.groundcurrent`, `rod.hum`, `bell.crack` |
| `Data/fx.json` | Strike pre-tell (St Elmo's corona on weapon and armour sockets; hair WPO via a material param), ground glow, ribbon bolt with stepped-leader pre-stroke, ground-current ring, ball lightning, fulgurite node sparkle, rod-tip glow, chain arc ribbon, Lichtenberg flash decal, Galvara's banner cloud with core orbs, Coilback's ring pulse along copper veins |
| `Data/server_presets.json` | Nothing new (the biome-events, invasion and weather settings are SPEC from other lanes) |

### 3.2 C++ (Raven's patterns)

| Where | Add or change |
|---|---|
| `WRAttributeSet.h/.cpp` | `Charge`, `MaxCharge` attributes with OnRep, mirroring `Heat`/`MaxHeat` |
| `WRPathComponent.cpp` | Charge gain on lightning hits, decay after the delay, the bonuses at ≥ 60, capacity from `nodeEffects`. Grounded as the Lightning root passive (main only) |
| `WRAbilities.h/.cpp` | New `UWRGameplayAbility` classes: `UWRGA_Spark` (bolt, from the FireBolt pattern), `UWRGA_StaticStrike` and `UWRGA_ChargedWeapon` (Wildbrands, through the held weapon's trace notifies; ChargedWeapon reads hold time), `UWRGA_ChainLightning` (chain shape: nearest unhit enemy within 800 cm, jumps from data + `extraJumps`), `UWRGA_Thunderclap` (nova), `UWRGA_BoltDash` (root-motion dash, capsule sweep along the path, i-frame tag), `UWRGA_LightningStep` (BoltDash ×3 window), `UWRGA_StaticField` (pool), `UWRGA_Tempest` (storm shape; spawns `AWRLightningStrike` in strike-only mode on enemies), `UWRGA_ThunderGod` (self buff tag; the weapon-hit hook spawns a small strike). Register all of them in `ClassForNode` under `"lightning"` |
| `WRCombatLibrary.cpp` | Passives through `HasNode`: Conductive (`Status.Soaked` × 1.25), Discharge (on block or parry), Afterimage (on dodge start), Rapid fire (anim rate and cooldown scale from the Charge level). Shock-chance roll on weapon hits. Stormheart part: every 5th hit chains. Combo hooks for Plasma, Meltdown, Brittle shock, Blizzard storm, read from `combos.json` (a new `FWRComboRegistry`) |
| `WRStatusComponent.cpp` | The Shock status (combat.md SPEC): buildup, stagger, chain on proc, the Grounded immunity to the stagger, the creature hyperarmor rule |
| `WRWeather.cpp` (world-map's `AWRLightningStrike`) | Add the **3.0 s pre-tell phase** (multicast: picked pawn's hair WPO param, weapon and armour corona FX, buzz). Pick weights from load and Stormweave pieces; skip Grounded, charm and Stormweave-4 wearers; rod shadow (nearest actor tagged `Rod` within 300 cm takes it); Rodcrest exclusion radius; ground current on Wet cells (an expanding ring sweep, `low`); fulgurite node spawn on sand or granite cells (35%) |
| **New** `WRStormHazards.cpp/.h` | `AWRGustZone` (spline-marked cliff path volume: whistle tell, push toward the drop side, brace and crouch multipliers), `AWRBridgePlank` (creak, break, and a placeable repair slot), `AWRRockfall` (overhang volume, trickle tell), `AWRBallLightning` (forms at rods or crags in storm states, moves in a straight line, pops on overlap, hit or end of life) |
| `WRCreature.cpp` | Rows through the shared attack-row runner (fire-frost and combat C9). New: Voltfang charge-up and the always-charged storm flag; flank lunge knockback; the Raiju `strike_arrival` spawn (spawned by the strike actor) and `storm_return` despawn; Rebound leap (trace for a surface within 600 cm); Thunderhorn and Bellwether stuck-on-rock stun; Stormkite grounded state (SPEC "bring it down" exists for Scorchwing; reuse); Rodcrest strike attraction; Coilburrow ore sensing (owner compass marks); Galeback herd ring (reuse Woolly drudge ring) |
| `WRWildlife.cpp` | Weather-tagged spawns (`activity: "storm"`); living-world beats (Rams' Ring dusk clash, Rodcrest basking in storms, Stormkite snatch-and-drop) |
| `WRBoss.cpp/.h` | Rows for `bellwether` (roaming route, herd, bell part), `galvara` (air phase on the built flying code from Skaldrith P2; three core sub-actors as lock points with HP; the ground phase; retake-the-sky timer), `coilback` (underground state: hidden, moving ridge FX, surfacing; ring pulses along copper-vein splines from `world_pois`; metal-weight targeting; frost tunnel slow; nose part), `kharvox` (charge bar driving a feather emissive param; wing parts; bleed and curse hooks; rim rods as `AWRArenaRod` actors with strike counts and melt; called strikes through `AWRLightningStrike`; the moving eye in P3 as a volume that blocks strike picks; Bone drop as a grab that carries the victim on a socket and drops at a set height; furthest-tame target pick). All melee on socket traces (combat C10 rule); full-area markers only for things really on the ground |
| `WRBiomeEvents.cpp` | The Great Storm timeline (2.15): forces the Storm peaks weather state, runs the low and high strike schedulers, Raiju arrivals, ball lightning, the Hollow Spire Choir grille, the ×3 node payout, Storm coil ×3 |
| `WRAltar.cpp` | Lightning altar row beside the ossuary slab |
| `WRUIHud.cpp` | The Charge bar (violet-white, crackles at ≥ 60); the compass altitude line turning violet-white above 600 m in a Thunderstorm |
| `WRUIMainMenu` / path picker | Lightning appears in the path picker (FULL-GAME 2 "all 12 paths once their biomes exist"); spawn at the three Storm camps |
| `WRBodyChange.cpp` (hero lane) | Lightning row: Lichtenberg mask B, flicker on cast, iris ring, hair WPO; the strike pre-tell drives the same hair param |

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
| Voltfang | Wolf base; crest fur cards with an emissive tip mask. Clips: wolf pack (walk, trot, run, bite, lunge, shake, howl, death) |
| Thunderhorn | Bighorn/argali base; horn sculpt. Clips: ram or goat pack (walk, charge, butt, rear, stuck, sleep) |
| Stormkite | Raptor base with a forked tail. Clips: bird of prey pack (soar, dive, hover, land, grounded flap) |
| Raiju | New concept (weasel-lynx, fur halo, forked tail, skull-tight face); emissive Lichtenberg mask. Clips: big cat plus mustelid (crouch, pounce, flurry, wall leap, curl). Fur halo through groom cards with a static-lift WPO |
| Cragstrider | Mountain goat base, middle legs added. Clips: goat pack, mid legs phase-offset (fire-frost Tusker method) |
| Galeback | Yak base, hair skirt as groom cards. Clips: yak or bison pack |
| Coilburrow, Coilback | Mole base; star nose as 22 bones with AnimDynamics; Coilback plates sculpted in Blender. Clips: mole or badger digging set, scaled up for Coilback |
| Rodcrest | Monitor lizard base; crest as a separate mesh with raise morph. Clips: lizard pack |
| Puffram | Sheep base, wool as a soft groom. Clips: sheep pack |
| The Bellwether | From the Thunderhorn, scaled, triple-curl horns, bell and collar (bell on a physics bone) |
| Galvara | From the Stormkite, with long legs and a whip tail; banner cloud as Niagara volumetric cards with three emissive core orbs (separate actors) |
| Kharvox | New concept: lammergeier plus harpy eagle plus thunderbird; feathers as cards with an emissive charge mask driven wingtip-inward. Clips: large bird of prey or wyvern pack (soar, hover, dive, land, rake, buffet, clap, grab-carry, crash-ground) |
| Vesna Groza | MetaHuman preset; Lichtenberg scar from the body-change mask at 1.0; climbing outfit from the R0 outfit work |
| Landmarks | Bell Rod and rods (Blender kit, verdigris material), the hanging thunderstone, the Hanging Monastery (modular cliff-cell kit plus the face terrain), rope bridges (spline-built planks and ropes, a plank actor per slot), the Burnt Drake (fire-frost skeleton kit, charred), the nest (pine trunks plus rods), the Glass Scar (terrain plus a glass material with emissive after storms) |
| Kit prefabs | 13 kinds (2.4) as Packed Level Actors, 2 to 4 variants each |
| Materials | Slate and granite ground layers, fulgurite glass, verdigris copper, lichen, wet rock in rain, glass sand |
| Hollow Spire | Vertical shaft kit extending `Tools/art/cave_gen.py`; glass pipes; chain walks |

### 3.4 Sound and music

- **Voices:** 13 sets (9 creatures, 4 bosses) built from the 2.12 recipes from CC0 / CC-BY sources (Freesound, the Sonniss GDC bundles, Kenney) through Raven's `Tools/audio` pipeline. About 8 files per creature, 12 or more per boss (SPEC counts from fire-frost 4.4).
- **Hazard sounds:** strike pre-tell buzz (rising, 3 s), crack and close thunder, ground-current sizzle, gust whistle, plank creak and snap, rockfall trickle, ball-lightning hum, rod hum.
- **Music:** pick the 2.12 cue sheet from libraries (about 22 cues counting stems), update `sounds.json` pools and `Docs/CREDITS.md`.
- **Beds:** 5 sub-zone beds plus the Hollow Spire and the Rodfield wind-song.

### 3.5 Map dressing

- The B3 west terrain is generated by world-map's W8 step (SPEC): heights, the Crown's west half, the gorges, the Stormgut west mouth. This lane adds the sub-zone polygons and ground layers to `world_pois.json`.
- `Tools/terrain/dressing.mjs` places the 29 hand sites and 10 landmarks at their coordinates, fills the 84 kit POIs by sub-zone with Poisson spacing of at least 150 m, then walks every route (roads, cliff paths, bridges) and adds a kit POI wherever a point is more than 125 m from any POI (SPEC rule, fire-frost 4.5).
- The rod-road waymarks go every 150 m along the coast road and the shrine road (world-map SPEC).
- **Hazard cells:** gust paths, rope bridges, overhangs, the strike altitude (from the height map), and sand and granite cells for fulgurite nodes.
- The Hollow Spire is a streamed sub-level under the Spire with its four doors.
- Everything is runtime-spawned from data (Raven's dressing pattern). Say before touching a map; maps can't be merged.

### 3.6 Build order inside this scope

Each step is one cook into Aaron's build; Aaron tests each. Gates: compile, cook, package.

| # | Step | Why here |
|---|---|---|
| 0 | **Data and shock:** `lightning.json`, Charge attribute, the Shock status, `combos.json` frame | Everything below reads these |
| 1 | **Proof of one: the Voltfang end to end.** Concept, Aaron's pick, mesh, real wolf clips, fused voice, attack rows with socket hits and shock, pack flanking and charge-up, drops, spawned on a test patch by camp 2 | The first new-biome creature through the whole pipeline (art, clips, voice, rows, a new status) on a pack, the hardest fight shape |
| 2 | **The Lightning path, trunk first:** Spark, Static strike, Charge bar, Grounded, the path picker and Storm camps; then the three branches; then proc feedback | Players can pick Lightning and play it on the Old Island while the biome grows |
| 3 | **The land:** B3 west terrain (world-map W8), S1 Wind Passes with camps and its POIs first (proof of density), then S2, S3, S4, S5 | Biggest piece; proven on one sub-zone |
| 4 | **Thunderstorms and strikes:** the strike with pre-tell, pick weights, rods, Rodcrest, ground current, fulgurite nodes; Grounding salve, grounding charm | The biome's identity; needs the land's heights |
| 5 | **Micro-hazards:** gusts, planks, rockfall, ball lightning | After the sub-zones they live in |
| 6 | **The other 8 creatures** (Raiju with the storm), voices, day, night and storm tables, living-world beats | Reuses step 1's frame |
| 7 | **Minis:** the Bellwether (roaming frame from fire-frost), Coilback, Galvara | After creatures and hazards (Galvara's eyrie is above 600 m) |
| 8 | **Shrines, run-back stones, Vesna and her quests** | After the POIs and the hero rig |
| 9 | **Kharvox and the altar**, the Summit Nest, the Last Stair | Needs strikes, rods, flight, parts |
| 10 | **The Hollow Spire and secrets** | After Kharvox (the Nest Door shortcut) |
| 11 | **Music set and beds** (can start any time; independent of art) | Big feel gain |
| 12 | **The Great Storm event** | Needs strikes, Raiju, the Choir room |
| 13 | **Gear:** Stormglass arrows, Rodwarden spear, Stormweave (when Hearth 4 lands, survival-crafting R2) | Needs items and Hearth tiers |
| 14 | **Combos** Plasma, Meltdown, Brittle shock, Blizzard storm | Needs the full Lightning tree |
| 15 | **Invasion contents** (Storm side) | Needs the invasion frame and the Swamp (R2) |
| 16 | **Body change** (after the MetaHuman hero lands in R0) | Depends on the hero |
