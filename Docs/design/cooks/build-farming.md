# Wildreach build cook: FARMING AND FOOD (FULL-GAME 6.2)

Lane build-farming, 2026-11-14. Scope: biome crops, irrigation, greenhouses, animal husbandry, beekeeping, biome
fishing with Fishing XP, cooking stations and recipe discovery by experimenting, spoilage and preservation (salting,
smoking, ice boxes). The design to build is `cooks/survival-crafting.md` 2.7 to 2.11 and 2.23 (perks), 2.24 (POI
dressing), 2.25 (sound). This doc adds research, the requirement ledger, the gaps those sections left and the
build plan. Built to compile only (Aaron and testers test, F8).

---

## 0. Requirement ledger

Split from the brief, Aaron's quotes in it and FULL-GAME 6.2 / survival-crafting 2.7-2.11. Proof column filled at the end
(section 5).

| # | Requirement | Source |
|---|---|---|
| R1 | Biome crops: 24 crops (common + rare per biome), each grows in its home biome only | FG 6.2; SC 2.7 |
| R2 | Crop plot holds 9 plants of one crop, planting takes 9 seeds | SC 2.7 |
| R3 | Four visible growth stages (sprout, leaf, flower, ripe), each its own modelled look; ripe reads at 30 m | SC 2.7 |
| R4 | Common 20 min, rare 2 h (tuning `crops`) | FG 6.2 BUILT |
| R5 | Special grounds: Ember grape in an ash pit, Bog rice in water ≤50 cm, kelp on a kelp line in 2-6 m sea, Oyster caps on a log bed, Stormrye windy (>300 m or Storm), dark crops only without daylight, Ashen grain/Mandrake withered ground | SC 2.7 |
| R6 | Wrong ground shows "Wrong ground" and nothing grows | SC 2.7 |
| R7 | Irrigation (Hearth 3): watered when within 8 m of water, a well or an irrigation channel fed from water; ×0.67 time (Water wise ×0.5); rain counts while it rains; no watering can | SC 2.7 |
| R8 | Fertiliser: compost or bone meal, one per plot per harvest, +25% yield, +3 XP | SC 2.7, 2.23 |
| R9 | Pollinators: a beehive within 20 m ×0.85 time | SC 2.7, 2.8 |
| R10 | Harvest all 9 at once with E; yields common 9-12 / rare 3-5; seeds back common 9-11 / rare 1-3; XP plant 10, harvest 20, first harvest of each crop 50 | SC 2.7, 2.23 |
| R11 | Wild seeds: every live biome's crops come from its wild plants | SC 2.7 |
| R12 | Greenhouse (Hearth 4): glass walls and roof; a plot under a glass roof and ≥80% walled grows any biome's crop at full speed | SC 2.7, CF |
| R13 | Dark shed: dark crops grow under any full roof with no windows / underground | SC 2.7 |
| R14 | Signature resources never farmable | SC 2.7 |
| R15 | Farming perks live: Green fingers +25%, Seed keeper +50%, Grafting (off-biome at 50%), Rare seeds 3%, Water wise ×0.5, Companion planting ×0.9, Golden crop 2% (+50% hunger in a dish), Endless field | SC 2.23; lifeskills waitsFor |
| R16 | Animal pens: 4 animals (Large pens +2), a feed trough holding 50 feed, one feed per production (Content herd 50% none), no feed no produce, hungry animals don't die | SC 2.8 |
| R17 | 17 farm animals with their products and feeds (per-species intervals) | SC 2.8 |
| R18 | Products show before collection; E collects all of one pen | SC 2.8 |
| R19 | Pen breeding: 2+ of one kind with feed → one young every 60 min up to capacity, grows in 30 min; +10 XP per birth | SC 2.8, 2.23 |
| R20 | Husbandry perks live: Gentle hand +25%, Large pens, Bee whisperer +50%, Shearer +50% hide/wool, Selective breeding +10%, Content herd, Prize stock 5%, Royal jelly 5% | SC 2.23 |
| R21 | Beekeeping: beehive (workbench L2: wood 20, Hiveback wax 4) houses one tamed Hiveback; honey 15 min, wax 30 min; crops in 20 m ×0.85; no feed while flowers/crops in 40 m; honey never spoils; +5 XP per collection | SC 2.8 |
| R22 | Biome fishing: each of the 12 biomes has its fish (common, rare, legendary) in data | SC 2.9 |
| R23 | Fishing XP: common 20, rare 60, legendary 500 first / 100 after | SC 2.23 |
| R24 | Legendaries: Legend catch + the biome's bait, once per real day per player; the first catch gives a mountable trophy (comfort trophy group) | SC 2.9 |
| R25 | Fish trap (workbench L2: wood 10, fibre 10) in water: one common fish of that biome every 20 min, holds 6, +2 XP a fish | SC 2.9 |
| R26 | Biome baits (Bait maker) for every biome | SC 2.9 |
| R27 | Cooking stations: campfire (roast, 15 s, light 10 m), cooking pot (discovery, 4 slots, 20 s), drying rack (15 min outdoors), oven (45 s), smokehouse (10 at a time, 10 min, burns 5 wood), salting barrel (1 salt each, 5 min), ice box | SC 2.10 |
| R28 | Meals move off the workbench to the campfire (same item) | SC 2.10 |
| R29 | Discovery: any 1-4 ingredients, food groups, patterns; a new match is a card "New recipe", +100 XP, joins the list for one-click cooking; no match = Grey mush | SC 2.10 |
| R30 | 13 base dishes + 12 biome dishes with hunger / thirst / sated / keeps | SC 2.10 |
| R31 | Sated: hunger doesn't drain for that long; never a stat buff; +1% per Cooking level | SC 2.10 |
| R32 | Recipe hints found in the world (scraps), not from NPCs | SC 2.10 |
| R33 | Cooking perks live: Recipe sense (group counts), Bulk cook, Preserver ×0.7, Shared meal, Camp chef, Hearty, Master chef, Banquet | SC 2.10, 2.23 |
| R34 | Freshness 100→0 per stack, a thin bar on its icon; timestamp-based, no ticking; ≤25% gives half; 0 = Spoiled food | SC 2.11 |
| R35 | Keep times per food (raw meat 40 min ... dishes per table; salted 24 h, smoked 48 h, dried 72 h; grain, honey, dates, raisins never) | SC 2.11 |
| R36 | Where kept: bag/chest ×1, chest in the tundra ×0.5, ice box ×0.2 burning 1 ice block / 4 h (none needed in the tundra) | SC 2.11 |
| R37 | Salt: salt pan (hand: stone 10) on a sea beach, 1 salt / 10 min in daylight | SC 2.11 |
| R38 | Ice blocks from tundra ice | SC 2.11 |
| R39 | Compost bin (workbench L1: wood 12): 4 spoiled food → 2 compost in 20 min, worms for bait | SC 2.11 |
| R40 | Mandrake screams on harvest (30 m aggro pulse, 2 s stagger) unless wax earplugs | SC 2.7 |
| R41 | Sounds: animal voices, cooking sizzle / bubble / oven door / discovery chime, legendary sting | SC 2.25 |
| R42 | Every screen mouse AND pad; F1 controls page lists new keys | brief THE BAR |
| R43 | Multiplayer: server authority, replication of plots, pens, stations, freshness; saves | brief |
| R44 | Data-driven: numbers in Data/*.json and tuning | brief |
| R45 | Design with intention: why each object is there, what it connects to, how it meets the ground/water/player | Aaron 11-14 |
| R46 | Nothing unfinished shown: things for biomes not on the island stay hidden until their land is live | brief |
| R47 | Icons for every new item | THE BAR (no "_unknown") |
| R48 | Fishing life skill XP source | brief SCOPE |

---

## 1. Research

### 1.1 Games

| Game, place / system | What works | We take |
|---|---|---|
| **Stardew Valley** crops: every crop has 4-6 visible growth sprites; sprinklers water adjacent tiles every morning; the Greenhouse grows any season's crop all year ([wiki Greenhouse](https://stardewvalleywiki.com/greenhouse), [Carl's guide](https://carlsguides.com/stardewvalley/farming.php)) | The field tells you its state at a glance; automation (sprinklers) removes the chore; the greenhouse is the late-game "grow anything" reward | Four modelled stages; water by placement (well, channel); greenhouse = any biome's crop |
| **Valheim** cultivator: barley and flax only grow on the Plains; planting elsewhere fails ([allthings.how](https://allthings.how/valheim-1-0-how-to-plant-crops-with-the-cultivator/), [xgamingserver](https://xgamingserver.com/blog/?p=21445)) | Biome-locked crops make travel and trade matter; failure is told plainly | Home biome only; plot prompt says "Wrong ground: grows in the <biome>" |
| **Don't Starve Together** spoilage: freshness bar on every food icon, ice box halves spoilage, salt box quarters it, stale food gives less, rot becomes Rot used as fertiliser ([wiki Ice Box](https://dontstarve.fandom.com/wiki/Ice_Box), [Klei forum](https://kleiforums.com/forums/topic/120446-what-is-the-difference-between-icebox-and-saltbox/)) | Freshness is visible without a menu; the spoiled item loops back into farming | Thin bar under the icon (green→amber→red), half values at ≤25%, Spoiled food → compost → fertiliser and chum |
| **Breath of the Wild** cooking: throw up to 5 ingredients in a pot, the result is found by trying; failures are "Dubious food"; recipes appear on posters/books in the world | Experimenting is the fun; the world hints, never a quest giver | 4-slot pot, food-group patterns, Grey mush on no match, recipe scraps (found by fishing treasure and in the world) give hints |
| **Enshrouded** farming: wells and pumps water farmland; fertiliser from bone meal ([TechRaptor](https://techraptor.net/gaming/guides/enshrouded-farming-guide)) | Infrastructure, not chores | Well and irrigation channels; compost/bone meal +25% |
| **Once Human / Ark** fridges and preserving bins slow spoilage but need fuel/power | Cold storage has an ongoing cost | Ice box burns an ice block every 4 h, free in the tundra |
| **Monster Hunter Wilds** whoppers: a named big fish per area, their own fight | Legends worth hunting, shown off at home | Legendary per biome, once a day, a trophy you mount |
| **Ark** pens and **Palworld** ranch: animals produce into a box, feed troughs feed everyone in range | Feeding is one action for the whole pen | One trough per pen (50 feed), production eats from it |

### 1.2 Real world
- La Geria, Lanzarote: vines in pits dug through volcanic ash (picón), each with a crescent dry-stone wall (zoco) on the windward side. → The **Ash pit**: a 3.6 m round pit sunk 40 cm into the ground, ash floor, a 1 m high crescent of stacked basalt on its north-west side.
- Shadufs and stone-lined acequias (Moorish irrigation channels) carry water by gravity in open, narrow troughs. → The **irrigation channel**: a 4 m stone-lined trough with flat coping stones, water in it only when it's fed.
- Hand-dug well: a round stone curb ~1 m across, a timber windlass on two posts, a rope and an oak bucket. → The **well**.
- Smokehouses: a timber shed with a fire pit inside and racks above; salting: a cask with a lid and dry salt layered with meat; drying racks: poles with cross bars hung with strips in the open.
- Skep and log hives: a straw skep under a small roof on a stand. → The **beehive** (straw-coiled dome on a plank stand under a shingle hood).
- Victorian glasshouses: timber glazing bars every ~50 cm, a low brick/stone knee wall, a ridge vent. → **Glass wall / roof**: stone knee wall 60 cm, wooden mullions every 50 cm, panes in between.
- Salt pans: shallow clay-lined beds on the upper beach, sea water evaporating in the sun, a rake to pull the crust.
- Ice houses: blocks cut from frozen lakes and packed in sawdust/straw. → The **ice box**: a stone-walled chest lined with straw, iron bands, a heavy lid.

### 1.3 Lessons
1. State shown on the object: crop stage, watered (dark wet soil + water in the channel), products on the pen's shelf, smoke from the smokehouse, a lit campfire.
2. No chores: water by placement, one feed trough, spoilage on timestamps.
3. Every loop closes: spoiled food → compost → fertiliser; fish scraps → bait; tallow, eggs, milk → dishes.
4. Biomes matter: crops and fish belong to their land; things for unbuilt lands stay out of sight until the land exists.

---

## 2. Design (gaps solved; the rest is survival-crafting 2.7-2.11 as written)

### 2.1 Live biomes (R46)
`tuning world.liveBiomes` = `["volcanic","tundra"]` (what `WRBiome::At` can answer today). Crops, plot types, animals,
fish, baits, recipes and trophies tied to another biome are in the data and hidden from the build bar, the cook list,
the crafting tab and drops until their biome is listed. Adding a biome's land = add its id; nothing else.

### 2.2 Plots (R1-R14)
- **Crop plot** (`crop_plot`, soil): the existing 4 × 4 m timber-framed bed. The 9 plants stand on a 3 × 3 grid, 120 cm apart, each with a ±15 cm jitter and its own turn from a hash of the plot's position (same on every machine). Soil darkens when watered (cloth colour from dry 0.07/0.045/0.025 to wet 0.035/0.022/0.012).
- **Ash pit** (`ash_pit`, volcanic only, hand-free: workbench L1: stone 20): La Geria pit. Ember grape only grows here; Fire pepper also does. Can only be placed where `WRBiome::At` is volcanic.
- **Paddy** (`paddy`, Bog rice, needs 10-50 cm of water where placed), **Kelp line** (`kelp_line`, sea 2-6 m), **Log bed** (`log_bed`, Oyster cap): built in code and data, hidden until swamp / ocean / fungal are live.
- Crop rows in `Data/farming.json crops`: id, name, biome, rarity (common/rare), plots [soil/ashpit/paddy/kelp/log], needs (`light` default, `dark`, `windy`, `withered`), seed item, produce item and counts, form and colours for its look, `golden`.
- **Growth** is a 0..1 value the farm subsystem moves every 2 s: `rate = 1 / (base × 60) × server.cropMultiplier × (1 + planter's cropSpeed)` × modifiers. Modifiers, each shown in the prompt: watered ×1/0.67 (Water wise ×1/0.5), a hive within 20 m ×1/0.85, Companion planting (planter's perk, a plot of a different crop within 8 m) ×1/0.9. It stops (and the prompt says why) for: wrong biome (not in a greenhouse, no Grafting), wrong plot type, `light` crops with no daylight (under an opaque built roof and not a greenhouse, or underground), `dark` crops with daylight, `windy` crops below 300 m outside the storm peaks, Ashfall over an outdoor plot (the survival lane's rule, moved here).
- **Daylight** = no built roof within 15 m above, or the roof above is glass. **Greenhouse** = a glass roof piece within 6 m above the plot centre and ≥ 80% of 16 horizontal rays (at 1.2 m) hitting a built wall within 8 m. **Dark shed** = an opaque built roof above and no window/glass piece within 8 m (or underground).
- **Grafting**: off-biome grows at full speed, harvest × 0.5.
- **Stages**: sprout < 0.25, leaf < 0.6, flower < 0.9, ripe ≥ 0.9 (harvestable at 1.0; between 0.9 and 1.0 the fruit colours in). Replicated as a byte (0-255) so a change is 1 byte per 2 s per plot.
- **Planting**: E on an empty plot plants 9 of the seed that grows here that you hold most of (last crop of that plot first); "Use" on a seed in the bag next to an empty plot plants that seed. Not enough: "Need 9 <seed> (you have N)".
- **Fertilise**: E on a growing, unfertilised plot with compost or bone meal in the bag uses one (+3 XP), a brown mulch ring shows under the plants.
- **Harvest**: E at ripe 1.0. Yields from data × (1 + harvestExtra) × server.gatherMultiplier × 1.25 fertilised × 0.5 grafted. Seeds back from data × (1 + seedExtra). Rare seeds: 3% a seed of the biome's rare crop. Golden crop: 2% one `golden_crop` (a Veg/Fruit that makes its dish worth +50%). Endless field: replants from the harvested seeds (needs 9 back). First harvest of each crop +50 XP once (saved). Mandrake: scream (R40).
- **Wild seeds**: shrubs of a live biome (ember / snow shrubs) give that biome's seeds by `farming.json wildSeeds` (volcanic: Fire pepper seed 30%, Ember grape seed 6%; tundra: Snow turnip seed 30%, Roseroot seed 6%); the berry bush keeps giving berry seeds (`seeds`, renamed "Berry seeds"; the berry crop row keeps old plots working).

### 2.3 Water (R7)
- **Well** (`well`, Hearth 2 claim or none... spec workbench L2 → `hearth: 2`; stone 30, wood 10, rope→fibre 10): always a source (groundwater). Stone curb ring of 10 blocks, two posts, windlass drum, crank, rope, bucket; water disc inside (lake material) 70 cm below the rim.
- **Irrigation channel** (`irrigation_channel`, `hearth: 3`, stone 12): 4 m straight trough, stone base, two side walls, coping, water strip inside when fed. Fed if its end (either end, ±200 cm along its axis) is within 250 cm of: natural water (sea depth > 0), a well, or another fed channel's end. The subsystem floods from sources through channels every 5 s (breadth-first, ≤ 200 channels per claim).
- A plot is watered when its centre is within 8 m of natural water, a well or a fed channel, or its biome's weather has `watersCrops` (rain rows) and the plot is outdoors.

### 2.4 Greenhouse pieces (R12)
`glass_wall` (wall shape) and `glass_roof` (roof shape), material wood (timber glazing: wood span and upkeep), `hearth: 4`
(Hearth 4 is the steel round: the build bar shows them with "Needs Hearth 4" and refuses until then). Look: stone knee
wall 60 cm, posts, mullions every 50 cm, panes of glass (M_Ice, the clear icy material) between. `glass: true` in
pieces.json makes them count as daylight and as windows for the dark-shed rule.

### 2.5 Animals (R16-R20)
- `farming.json animals`: species id (the creature id), products [{item, every (s), shear?}], feed [items], prize item.
  Pumicehog: animal fat 600 s, feeds any crop / berries / spoiled food; Snowpuff: egg 600 s, feathers 1200 s, feeds snow
  turnip / lichen / seeds. The other 15 rows wait for their creatures (hidden: no creature def = no row used).
- **Pen**: capacity 4 (+2 Large pens, owner's perk). Trough inside (a hollowed log on two trestles at the pen's east
  side) shows feed level as a filled block. E with feed in the bag tops it to 50 from what the animals eat, else
  collects products. Products wait on a **shelf by the gate**: eggs in a straw nest box, wool/feathers as bundles,
  fat in a crock (one modelled thing per kind present).
- Each animal makes its own products on its own clock; each production eats 1 feed (Content herd 50% free). No feed:
  nothing, the prompt says "Hungry: fill the trough"; animals never starve.
- Breeding: ≥2 adults of a species tamed by the pen owner, feed ≥ 5, under capacity: a young every 60 min (eats 5 feed),
  spawned tamed to the owner at 0.5 scale growing to 1 over 30 min; +10 Husbandry XP. Young from Selective breeding make
  +10% products (stored on the creature as `bPrize`... saved field `ProduceBonus`).
- Perks: Gentle hand +25% eggs/milk/wool; Shearer +50% hide/wool/feathers (rows with `shear`); Prize stock 5% the row's
  prize item; Content herd 50% feed-free.

### 2.6 Beekeeping (R21)
Beehive (`beehive`, `hearth: 2`, wood 20, hiveback_wax 4): a coiled straw skep on a plank stand under a shingle hood.
A tamed Hiveback told to stay within 3 m moves in (hidden in the hive; its tame stays counted). Honey every 900 s, wax
every 1800 s, needs a crop plot or wild flower within 40 m else nothing. Bee whisperer +50%, Royal jelly 5% per
collection. Crops within 20 m ×0.85 time. Hidden until a `hiveback` creature def exists (forest), so it's never a dead
piece on the island.

### 2.7 Fishing (R22-R26, R48)
- `fish.json`: keeps the built volcanic/tundra lists; adds the spec's 30 rows for the 10 other biomes (common, rare,
  legendary each) and `xp` per rarity (common 20, uncommon 35, rare 60, legendary 500 first / 100 after), replacing the
  flat 15.
- Legendaries: need Legend catch (built) and the biome's bait in the bag (lure for the biome), and once per real day per
  player (UTC date saved). First ever catch of that legend: `trophy_<fish>` item; the `fish_trophy_<fish>` decor piece
  (wood 8 + the trophy) mounts it on a board: a modelled fish (lofted body, fins, tail in its colours) on an oak plaque
  with two iron pins; comfort trophy group 1.
- Biome baits for all 12 (Bait maker): ember / rime lures built; 10 more rows with their biome's input, hidden until live.
- Fish trap (`fish_trap`, `hearth: 2`, wood 10, fibre 10): must be placed with ≥ 50 cm of water under its spot; a willow
  funnel basket on a stake. Every 20 min one common fish of the biome and water depth (`FWRFishRegistry::Pick` with no
  rare boost, no legendaries) into its 6 slots; E collects, +2 Fishing XP each.

### 2.8 Cooking (R27-R33)
Stations are `AWRStation` types (their queue, tray and delivery reused):

| Piece | Station | Hearth | Cost | Look |
|---|---|---|---|---|
| Campfire | `campfire` | 1 (by hand) | stone 5, wood 4 | ring of 9 stones, 5 crossed logs, coals, a spit on two forked sticks; 10 m warm light, flicker (counts as fire comfort + light) |
| Cooking pot | `cooking_pot` | 1 | copper ingot 4, stone 6 | its own fire ring, an iron tripod of three legs meeting over it, chain, a round copper pot with a lid |
| Oven | `oven` | 2 | stone 30, copper ingot 4 | a stone dome on a square plinth, an arched mouth, a chimney, a wood store underneath |
| Smokehouse | `smokehouse` | 2 | wood 30, stone 10 | a 2 × 2 m plank shed, gable roof, stone fire pit inside, racks, smoke from the roof vent while it works |
| Salting barrel | `salting_barrel` | 2 | wood 12, copper ingot 2 | an oak cask with iron hoops on a low stand, a lid with a stone weight |
| Drying rack | `drying_rack` | 1 | wood 6, fibre 4 | two A-frames and three cross poles hung with strips; must be outdoors |
| Compost bin | `compost_bin` | 1 | wood 12 | a slatted box, open top, dark compost inside |
| Ice box | (chest) `ice_box` | 3 | stone 20, iron ingot 4, hide 4 | stone chest, straw lining, iron bands, heavy lid |
| Salt pan | `salt_pan` | 1 (by hand) | stone 10 | a 2 × 3 m shallow stone-rimmed bed, white crust, a wooden rake |

- The campfire's recipes: roast meat (`cooked_meat`, moved off the workbench), roast fish (every fish → `grilled_fish`, moved), roast vegetable (each veg crop → `roast_vegetable`), cut bait unchanged.
- **The Cook screen** (E at a cooking pot or oven; at a campfire with Camp chef): left, four ingredient slots; under them the bag's cookable things as a grid (click / A adds one, click a slot / X removes); right, "Known" dishes (click / A fills the slots from the bag and cooks one) and "Hinted" (from scraps: pattern shown with "?" where unknown; Recipe sense shows the group count for every undiscovered dish); bottom: Cook (Enter / pad Y), the station's work in progress with a bar, Take (the tray). A new discovery slides a parchment card in ("New recipe: Fire pepper stew") with the page chime.
- **Matching**: each dish row in `Data/cooking.json` has a station (pot / oven) and a pattern of 1-4 entries, each a group (`Meat`) or an item id (`fire_pepper`) or a list (`Dairy|Water`). The ingredients must fill the pattern one-to-one (all permutations of ≤ 4). Most specific (most item entries) wins, so biome dishes beat their base. Golden crop counts as Veg or Fruit and makes the dish's stack worth +50% (stack `Roll` 1.5).
- Items' groups in `cooking.json groups` (item → group). Water = a waterskin (it comes back empty... waterskins are consumed as one use).
- Discovery: first match → +100 XP and saved in the player's known list; then +15 per cook (built key). No match → Grey mush (still takes the ingredients).
- **Sated**: `use.sated` minutes on dishes. Eating sets Sated until max(current, now + minutes × (1 + 0.01 × Cooking level)); while sated hunger doesn't drain (`TickSurvival`). HUD line "Sated 12 min" with the glyph under Rested. Banquet: the feast made by a Banquet cook is `banquet_portion` ×12 (sated 45 min) instead of `hunters_feast` ×6 (30 min).
- **Recipe scraps** (`recipe_scrap`): from the Treasure hook table and the salvage of old kitchens later; Use it → a random undiscovered dish of a live biome becomes Hinted (pattern shown).

### 2.9 Spoilage (R34-R39)
- `items.json` food rows get `keeps` (minutes, 0 = never). Stacks of such items carry `Fresh` (0..1 at `FreshAt`) and `FreshRate` (fraction a second). Freshness now = Fresh − (now − FreshAt) × FreshRate. No per-stack ticking: a 10 s server sweep re-stamps a stack only when its container's rate changed and turns stacks at 0 into Spoiled food.
- Merging two stacks averages freshness by count (DST rule); stacks of different worth (`Roll`) don't merge.
- Rates: bag ×1 (Preserver ×0.7 for that player); chest ×1, ×0.5 in the tundra; ice box ×0.2 while it has an ice block (one burns every 4 h from its own slots; none needed in the tundra, where it's ×0.2 free).
- Eating at ≤ 25%: half hunger, thirst and sated. Icon: a 3 px bar under every food icon, green > 50%, amber > 25%, red below; tooltip "Fresh 64% · keeps 2 h · spoils in 51 min".
- Salt pan: on an upper beach (centre within 6 m of sea depth > 0 and less than 2 m above sea level), daylight only: 1 salt / 10 min into its 10 slots; E collects.
- Ice blocks: a swing at an ice rock (tundra) gives an ice block 35% of blows besides the stone.
- Compost bin: station recipes `compost` (4 spoiled food → 2 compost + 1 worm-rich... gives `cut_bait` 2 as worms) 20 min; `compost_fish` (4 fish scraps).
- Preservation recipes: salting barrel `salted_meat` (raw meat 1, salt 1, 300 s), `salted_fish` (any common fish + salt); smokehouse `smoked_meat` ×10 (raw meat 10, wood 5, 600 s), `smoked_fish`; drying rack `jerky` (raw meat 2 → 1, 900 s), `dried_fish`, `dried_fruit` (berries 4), `raisins` (ember grape 4). Fish inputs per fish id (rows generated per common fish from fish.json at load: "any fish of rarity common/uncommon").

### 2.10 Sound and effects (R41)
New sound rows (existing OGGs reused where they fit; a missing file plays nothing): `cook.sizzle` (campfire roast, pot), `cook.bubble`, `cook.oven`, `cook.discover` (page + chime: reuse `ui.page`), `farm.plant`, `farm.harvest`, `farm.water`, `farm.mandrake`, `animal.collect`, `fish.legend` (6 s sting slot). FX: `smoke.chimney` on the smokehouse and oven while working (reuse the campfire smoke recipe).

### 2.11 Intent (R45)
- Plots go where water is: the prompt and build card say "Watered: near water, a well or a channel", so farms sit by the shore, a well, or at the end of a channel run from the sea. Channels are laid end to end from a source; their water only appears when fed, so a broken run reads at a glance.
- The ash pit belongs to the volcanic slopes (placement refuses elsewhere); its wind wall faces north-west, where the island's wind comes from.
- The kitchen chain sits beside the Hearth: campfire / pot for daily meals, smokehouse and salting barrel turning the hunt into food that keeps, ice box in the store room, compost bin by the plots it feeds.
- Salt pans only work on a beach in daylight; ice comes from the tundra's ice rocks: each coast and biome feeds the kitchen.
- Pens get a trough and a shelf by the gate so feeding and collecting happen where the farmer stands.

---

## 3. Plan (build steps, in order)

1. **Data**: `Data/farming.json` (crops 24 + berry, plots, animals 17, hive, wildSeeds, saltPan, fishTrap), `Data/cooking.json` (groups, dishes 26), items.json (seeds 24, produce 24, golden crop, compost, bone meal, salt, ice block, spoiled food, honey, wax, royal jelly, earplugs, feed-less animal products, flour, preserved foods, dishes, baits, trophies, recipe scrap; `keeps` on all food), recipes.json (campfire, smokehouse, salting, drying, compost, oven/mill basics, baits, earplugs; workbench meals removed), pieces.json (campfire, cooking pot, oven, smokehouse, salting barrel, drying rack, compost bin, ice box, salt pan, well, irrigation channel, glass wall, glass roof, ash pit, paddy, kelp line, log bed, beehive, fish trap, 12 fish trophies; `hearth`, `biome`, `plot`, `glass`, `cold`, `hidden` fields), fish.json (10 biomes × 3, xp), tuning (`crops`, `farm`, `food`, `world.liveBiomes`, perks), lifeskills.json (remove `waitsFor` on built perks, set values), sounds.json rows.
2. **C++ `WRFarm.h/.cpp`**: `FWRFarmDefs` (registry), `WRBiome::IsLive`, `UWRFarmSubsystem` (server tick 2 s: plots, water network, pens, hives, salt pans, fish traps, ice boxes, freshness sweep), `FWRFarm` (plot interact, harvest, pen interact, use-seed), `UWRFoodComponent` on the PlayerState (known dishes, hinted, crops harvested, legend days, sated until; saved; replicated to owner).
3. **Spoilage** in `WRInventory`: stack fields, `FWRSpoil` helpers, merge in `FWRInventoryRules::Add` / moves / chest transfers, freshness in use, save/load.
4. **Pieces**: `AWRBuildPiece` gets farm state (CropId, Growth byte, flags, fertilised, feed, watered) replicated + saved; `Interact` routes to `FWRFarm`; hearth level and biome/plot checks in placement; looks in `WRBuildLook` for every new piece; plants + trophies + pen shelf + well/channel water via `WRFarmLook.cpp` (FWRToolMesh builders).
5. **Cooking**: `cooking_pot`/`oven` dishes registered as recipes (hidden from the Crafting tab), "cook" server command (match, take, queue), discovery, `EWRMenu::Cook` + `WRUICook.cpp` screen (mouse + pad), sated in `TickSurvival`, eat with freshness and worth.
6. **Fishing**: XP by rarity, legend gate + daily + trophy, fish trap, baits.
7. **UI glue**: crafting tab station list + names, build bar cards (hearth lock, biome), inventory freshness bar + tooltip, HUD sated line, prompts.
8. **Icons**: `Tools/art/make_farm_icons.py` (PIL, the fish icons' style) for every new item.
9. CHANGELOG, HANDOFF, compile, spot-check, commit, push, LANDED.

## 4. Edge cases
- Old saves: a planted plot with no CropId is the berry crop; old `cropSecondsLeft` converts to growth.
- A plot whose crop's biome isn't live can't be planted (seeds don't exist there anyway).
- Destroyed pen/hive/trap/ice box: storage drops as a loot bag like chests (Storage path already does for chests: extend to every kind with Storage).
- Client prompts read replicated state only; every action is a server command.
- Two players on one plot: the server's first E wins; the second sees the new state.
- Freshness on clients: computed from `GameState->GetServerWorldTimeSeconds()`.
- Server restart: freshness saved as the fraction now and re-stamped at load.

## 5. Proof (filled after the build)

All in commit "Farming and food" on master (Wildreach). Paths are `Source/Wildreach/` unless written.

| # | Done | Where |
|---|---|---|
| R1 | Yes (4 live, 20 in data hidden till their land) | `Data/farming.json` crops; `WRFarm.cpp:504` GrowthRate (WrongBiome) |
| R2 | Yes | `WRFarm.cpp:587` Plant (farm.seedsPerPlot 9); `WRFarmLook.cpp:387` Refresh (9 plants) |
| R3 | Yes (glossy ripe fruit; no wind sway) | `WRFarmLook.cpp:77` Plant (stages, fruit colouring in) |
| R4 | Yes | `WRFarm.cpp:504` (crops.commonMinutes / rareMinutes) |
| R5 | Yes (ash pit live; paddy, kelp, log, windy, dark, withered in code) | `WRFarm.cpp:504`, `WRFarm.cpp:457` PlaceCheck; `WRBuildLook.cpp:776` ash pit |
| R6 | Yes | `WRFarm.cpp` Describe ("wrong ground: grows in ...") |
| R7 | Yes | `WRFarm.cpp:1256` TickWater, `:1307` IsWateredAt; rain `Data/weather.json watersCrops`; well `WRBuildLook.cpp:717` |
| R8 | Yes (compost; bone meal waits for the grinder) | `WRFarm.cpp:752` Interact (Fertilise), Harvest x1.25 |
| R9 | Yes (waits for Hivebacks) | `WRFarm.cpp:504` HasHiveNear |
| R10 | Yes | `WRFarm.cpp:626` Harvest (yields, seeds, first-harvest 50 XP) |
| R11 | Yes | `WRWorldDressing.cpp:891` → `WRFarm.cpp:944` WildSeeds; `farming.json wildSeeds` |
| R12 | Yes (Hearth 4 lock) | `WRFarm.cpp:504` (glass roof + 80% walls); `Data/pieces.json` glass_wall / glass_roof; `WRBuildLook.cpp:756` |
| R13 | Yes | `WRFarm.cpp:504` bDarkShed |
| R14 | Yes | signature resources have no crop rows (`farming.json`) |
| R15 | Yes | `Data/lifeskills.json` farming perks; `WRFarm.cpp:587/626/504` |
| R16 | Yes (hungry shown on the prompt only) | `WRFarm.cpp:1459` TickPen (capacity, trough, Content herd) |
| R17 | Yes (2 live, 15 in data) | `Data/farming.json` animals |
| R18 | Yes | `WRFarmLook.cpp:466` shelf / nest box / trough; `WRBuildLook.cpp:1011` |
| R19 | Yes (young not saved mid-growth) | `WRFarm.cpp:1459` breeding |
| R20 | Yes (bee perks hidden till Hivebacks) | `Data/lifeskills.json`; `WRFarm.cpp:1459` |
| R21 | Yes, hidden till the Hiveback exists | `WRFarm.cpp:1365` hive; `WRBuildLook.cpp` hive blocks; pieces.json beehive needs hiveback |
| R22 | Yes | `Data/fish.json` (30 rows + lures) |
| R23 | Yes | `WRFishing.cpp:133` XpFor, `:416` |
| R24 | Yes | `WRFishing.cpp` ChooseFish (lure + daily), `:408` trophy; `WRFarm.cpp:1067`; 12 fish_trophy pieces |
| R25 | Yes | `WRFarm.cpp:1365` trap; pieces.json fish_trap |
| R26 | Yes (10 hidden till their land) | items/recipes lures; `fish.json lures` |
| R27 | Yes | `Data/pieces.json` kitchen; `WRBuildLook.cpp:540` FarmBlocks; recipes; drying rack open air `WRCrafting.cpp:487` |
| R28 | Yes | `Data/recipes.json` cooked_meat at the campfire; grills removed (fishRecipes) |
| R29 | Yes | `WRFarm.cpp:995` Cook, `:363` Match; `WRUICook.cpp` card |
| R30 | Yes | `Data/cooking.json` 22 dishes + items |
| R31 | Yes | `WRInventory.cpp:966`; `WRStatusComponent.cpp:796`; `WRUIHud.cpp:1080` |
| R32 | Yes (scraps from the Treasure hook; POI dressing waits for the map cooks) | `WRFarm.cpp:900` UseItem; fish.json treasure |
| R33 | Yes | lifeskills.json cooking perks; `WRFarm.cpp:1021` Banquet; `WRUICook.cpp` Recipe sense |
| R34 | Yes | `WRInventory.cpp:319` Sweep, `:348` ValueMult; `WRUIInventory.cpp:259` bar, `:1578` card |
| R35 | Yes | items.json keeps |
| R36 | Yes | `WRFarm.cpp` ContainerMult, ice box fuel in TickProducers |
| R37 | Yes | `WRFarm.cpp:1365` salt pan; PlaceCheck beach |
| R38 | Yes | `WRWorldDressing.cpp:896` |
| R39 | Yes | recipes compost / worms |
| R40 | Yes | `WRFarm.cpp:679` |
| R41 | Yes (reused clips) | `Data/Audio/sounds.json` cook.*, farm.*, animal.collect, fish.legend |
| R42 | Yes | `WRUICook.cpp:529` (SWRSlot / SWRButton, Enter / Backspace) |
| R43 | Yes | replicated farm state on AWRBuildPiece; server commands cook / cookknown; saves |
| R44 | Yes | farming.json, cooking.json, tuning farm / food |
| R45 | Yes | 2.11 above; channel / well / trough / shelf placement in code |
| R46 | Yes | `WRFarm.cpp:438` IsPieceLive; `WRUIBuild.cpp:109`; recipes `biome` |
| R47 | Yes | `Tools/art/make_farm_icons.py`, 148 icons, `Data/UI/Glyphs/sated.png` |
| R48 | Yes | `WRFishing.cpp:416` |
