# Wildreach cook: SURVIVAL, CRAFTING, BUILDING, TAMING, LIFE SKILLS

Lane: survival-crafting. Written 2026-11-13. Scope: FULL-GAME section 6 (6.1 survival, 6.2 farming and food, 6.3 crafting and stations, 6.4 building, 6.5 taming, 6.6 life skills), every item tied to the 12 biomes' resources in FULL-GAME 3.2.

How to read the tags:
- **SPEC**: BUILT or RAVEN-PLANNED. Kept exactly as Raven wrote it; the source is cited. Where Raven named a system but gave no numbers, the numbers are added and marked **SPEC + numbers**. The behaviour stays his.
- **GAP-COOKED**: was a GAP in FULL-GAME. Fully designed here.
- Times are real time unless "game" is written. All numbers are first drafts in `Data/*.json` (Raven's CLAUDE.md rule).
- Raven's hard rules this cook never breaks: **food fills hunger only, buffs come from potions and brews** (DD Survival); **no temperature meter, no food buffs, no injuries, no disease, no natural disasters** (DD Ruled out); **no spikes or traps in bases, no underground bases, no NPC guards** (BL, DD); **each creature tames only by its listed method** (DR #16); **tames keep their biome element** (DD).
- Tie-ins from the other lanes, used as they wrote them: equip load and armour poise (`cooks/combat.md` 2.6), crafting pulls from chests within 20 m in the claim, the whistle wheel and the build bar on pad (`cooks/ui-controls.md` 2.10, 2.11, 2.13).

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game | What it does | Lesson for Wildreach |
|---|---|---|
| **Valheim** | Three food slots set max HP and stamina; Rested comes from a fire, shelter and comfort furniture, and its length grows with comfort level; structural stability shown by colour (blue grounded to red failing), wood shortest reach, iron poles longest; each biome has its own fishing bait, fish come at quality 1 to 5 [PC Gamer food guide; GameSpot Hearth & Home preview; xgamingserver building guide; games.gg fishing guide] | Comfort lengthens Rested (copied). Stability colours in build mode (copied onto Raven's span rule). Bait per biome (copied). **Food slots are not copied**: Raven rules out food buffs, so food stays hunger-only |
| **Breath of the Wild / Tears** | Up to five ingredients in a pot; recipes found by trying; posters in stables hint at recipes; only one effect active at a time; wood or ore makes "rock-hard food" [RPG Site cooking guide; Zelda Dungeon recipes; Pocket Tactics] | Cooking pot with free ingredient slots and food-group patterns; hints found in ruins (environment, not NPCs); a failed mix gives "Grey mush". One brew active at a time (BotW's one-effect rule) |
| **Palworld** | Every creature has work suitabilities with levels; passives (Artisan +50%, Work Slave +30%) raise base work speed; passives inherit through breeding [palworld.wiki.gg Work Suitability; Mobalytics base guide; allthings.how passives] | Tame roles already do the work (Raven). Taken: a farm and utility skill tree per role, so a tame gets better at its job by doing it |
| **Ark** | Each stat inherited separately, about 55% from the stronger parent; mutations about 1 in 40 per roll add stat points and a colour change, capped per lineage; imprinting while the baby grows; per-species saddles [guildorder Ark breeding guide; xgamingserver ASA breeding] | Raven chose Ark-style breeding. Copied: 55/45 per stat, 3 mutation rolls of 2.5%, +2 stat points and a visible colour change, 20-per-side cap, imprint care prompts. Saddles per body rig, not per species (fewer assets) |
| **Rust** | Twig, wood, stone, sheet metal, armoured; HP 250/500/1000/2000; stone wall about 2 explosive charges; the tool cupboard burns each tier's own material as upkeep; no upkeep = decay in hours [xgamingserver Rust building and decay guides; zap-hosting base calculator] | Raid cost is a material economy: our explosive needs Volcanic sulfur. Two charges per stone wall, about nine per metal wall. Upkeep per tier in its own material (Raven already does this) |
| **V Rising** | The Castle Heart burns blood essence; empty = decay, walls breachable by anyone; tile limit rises with heart level; siege golems break walls [VRising Wiki Castle Heart; supercraft castle guide] | Confirms Raven's Hearth + totem upkeep model. Siege as a crafted, slow, visible tool (ram, catapult), not instant |
| **Conan Exiles** | The Purge sends NPC waves at bases; tier-3 building is the defence floor; thralls guard [xgamingserver purge guide; guildorder purge readiness] | Raven's creature raids are our Purge (built). Thralls are out (no NPC guards); tames guard instead (Raven's own rule) |
| **Enshrouded** | Farm soil made from dirt and bonemeal; seedbeds make seeds; watering can, wells and pumps; a rescued farmer unlocks it all [TechRaptor farming guide; Prima Games] | Bone meal and compost as fertiliser; wells and irrigation channels. No watering chore: a plot is watered or it isn't (DD "no tedious grind") |
| **Monster Hunter Wilds** | Fishing is a light side activity with "whoppers" that need their own catch minigame; players fish for fun and trophies [PC Gamer whopper guide; RPG Site MH Wilds fishing guide] | One legendary fish per biome, named, at one place, with its own three-phase fight and a mountable trophy |
| **Once Human** | Fridges slow spoilage and need power [game8 Once Human; Steam forum] | Ice box (ice-fed) and the Frost biome's Cold store (powered). Spoilage runs on timestamps, never ticking |

### 1.2 Real-world folklore, biology and culture used

| Source | Used for |
|---|---|
| Canary Islands farming: vines planted in pits dug through volcanic ash at La Geria (Lanzarote); chillies thrive on volcanic soil | Volcanic crops: Ember grape grows only in ash pits; Fire pepper |
| Yorkshire "forced rhubarb", grown in dark sheds by candlelight | Crystal caves crop: Candle rhubarb grows only in darkness |
| Ergot, the fungus on rye that caused medieval "St Anthony's fire" hallucinations | Graveyard Black rye: some harvests carry ergot, a narcotic for tranq darts |
| Mandrake: the root screams when pulled; diggers plugged their ears (and in the old tales tied a dog to it) | Void rare crop: harvesting it screams; ear wax plugs (Hiveback wax) block it |
| Iðunn's apples, the Norse apples of youth | Forest rare crop: Iðunn apple, ingredient of elixirs and the tame revive |
| Asphodel, the flower of the Greek underworld meadows | Graveyard rare crop for necro brews and the undead-tame ritual |
| Roseroot (Rhodiola rosea), arctic herb used against fatigue | Tundra rare crop, stamina and frostbite brews |
| Gentian root, alpine bitters | Storm peaks rare crop, shock resist and stamina brews |
| Pair Dadeni, the Welsh cauldron of rebirth (Mabinogion, Branwen) | The tame revive: the Dadeni draught |
| The Domovoi, the Slavic hearth spirit fed with bread and milk by the stove, carried in embers to a new home | The Hearth: a first cooked meal left on a new Hearth gives its claim +2 comfort (a household custom, no spirit actor) |
| Saltpetre scraped from bat guano in caves and nitre crusts in dry canyons; sulfur and charcoal | Black powder: sulfur (Volcanic only) + saltpetre (bat roosts, desert canyons) + charcoal |
| Coelacanth, the "Lazarus fish" thought extinct | Ocean legendary fish |
| Alligator gar, snakeheads, sturgeon, arctic char, cavefish (Astyanax) | Real fish behind each biome's catch list |

### 1.3 Sources
- PC Gamer, Valheim food guide: https://www.pcgamer.com/best-valheim-food-recipes-combo-list
- GameSpot, Valheim Hearth & Home food preview: https://gamespot.com/articles/heres-a-sneak-peek-at-valheims-new-food-system-with-hearth-home-update/1100-6494951/
- Valheim building and stability: https://xgamingserver.com/blog/valheim-base-building-guide/ ; https://mobalytics.gg/gamebase/guides/valheim-general-building-guide
- Valheim fishing: https://games.gg/valheim/guides/valheim-fishing-guide/ ; https://www.exitlag.com/blog/valheim-fishing/
- BotW cooking: https://www.rpgsite.net/feature/5363-the-legend-of-zelda-breath-of-the-wild-guide-cooking-recipes-and-bonuses-explained ; https://www.zeldadungeon.net/wiki/Breath_of_the_Wild_Recipes ; https://www.pockettactics.com/botw/recipe
- Palworld work suitability: https://palworld.wiki.gg/wiki/Work_Suitability ; https://mobalytics.gg/gamebase/guides/palworld-base-pal-guide ; https://allthings.how/palworld-best-passives-to-put-on-your-base-pals/
- Ark breeding: https://guildorder.com/games/ark/wiki/breeding-guide ; https://xgamingserver.com/blog/ark-survival-ascended-breeding-imprinting-guide
- Rust building, raid cost and decay: https://xgamingserver.com/blog/rust-base-building-guide/ ; https://xgamingserver.com/blog/rust-decay-and-upkeep-guide/ ; https://zap-hosting.com/en/tools/rust/base-calculator
- V Rising castle heart and decay: https://vrising.fandom.com/wiki/Castle_Heart ; https://supercraft.host/article/v-rising-castle-building-guide-2026/
- Conan Exiles purge: https://xgamingserver.com/blog/conan-exiles-the-purge-guide/ ; https://guildorder.com/games/conan_exiles/guides/purge-readiness
- Enshrouded farming: https://techraptor.net/gaming/guides/enshrouded-farming-guide ; https://primagames.com/tips/everything-you-need-to-know-about-farming-in-enshrouded
- MH Wilds fishing: https://www.pcgamer.com/games/action/monster-hunter-wilds-whopper-location ; https://www.rpgsite.net/guide/16989-monster-hunter-wilds-fishing-guide-how-to-catch-rare-fish-which-is-the-best-place-to-fish-bait-and-more
- Once Human fridge: https://game8.co/games/Once-Human/archives/463422
- House spirits (Domovoi) and hearth goddesses: https://en.wikipedia.org/wiki/House_spirit ; https://www.atlasobscura.com/articles/how-to-appease-household-spirits-across-the-world

Game guide numbers vary by patch (the searches flagged it); ours are designed to Raven's starting numbers and our bar, not copied.

### 1.4 Lessons we take (short)
1. **Light pressure, real choices.** Hunger and thirst stay Raven's light 60/40 minutes. Good food is worth making because it fills more, keeps you **sated** (hunger paused) longer, weighs less and keeps longer. It never gives stat buffs.
2. **Every biome feeds the others.** Each biome grows two crops, raises its own animals, has its own fish and gives one key input to shared recipes (sulfur for black powder, salt from coasts, sunstone lamps, ice for ice boxes). Trade and travel follow.
3. **No chores.** Plots are watered by placement, not by a can. Spoilage runs on timestamps. Machines run while you're near; queues finish while you're away (built).
4. **Show it.** Crops grow in 4 visible stages, animals produce on their bodies (wool grows, eggs appear in the nest), fish fight on the line, machines turn. Stability colours show where a wall will fall.
5. **Raids cost something.** Explosives need Volcanic sulfur and cave saltpetre; metal walls cost about nine charges. Builders get a real reason to climb tiers.
6. **Tames grow by doing their job** (Palworld's lesson), and breeding is Ark's proven loop.

---

## 2. Design

### 2.0 Index

| # | Area | Status |
|---|---|---|
| 2.1 | Survival: hunger, thirst, carry, durability, beds | SPEC (BUILT) |
| 2.2 | Rest, Rested and comfort | SPEC + numbers; "Rest" GAP-COOKED |
| 2.3 | Darkness and light | SPEC + numbers |
| 2.4 | Weather that matters, per biome | SPEC + numbers |
| 2.5 | Corruption | SPEC + numbers |
| 2.6 | Swimming, breath and death | SPEC (BUILT) + GAP-COOKED (no floors) |
| 2.7 | Crops: 24 biome crops, stages, irrigation, fertiliser, greenhouses, dark sheds | SPEC + numbers; crop list GAP-COOKED |
| 2.8 | Animal husbandry and beekeeping | SPEC + numbers |
| 2.9 | Fishing | SPEC + numbers |
| 2.10 | Cooking, discovery and the sated rule | SPEC + numbers |
| 2.11 | Spoilage and preservation | SPEC + numbers |
| 2.12 | Alchemy: potions, brews, resist potions, tonics, elixirs | SPEC + numbers |
| 2.13 | Crafting: tiers, metals, ores by depth, salvage, blueprints, runes | SPEC + numbers |
| 2.14 | Gear slots, the 12 biome armour sets, hazard gear, rings, talismans | SPEC + numbers; sets GAP-COOKED |
| 2.15 | Machinery, power and the 12 biome machines | SPEC + numbers |
| 2.16 | Siege weapons, explosives and boats | SPEC + numbers |
| 2.17 | Building: tiers, freeform structure, integrity, biome sets, new pieces | SPEC + numbers |
| 2.18 | Raids: creature raids, player raid costs, path raiding, turrets, monuments | SPEC + numbers |
| 2.19 | Taming: methods, limit, feeding, levels, role skill trees | SPEC + numbers; trees GAP-COOKED |
| 2.20 | Saddles, barding, flying, swimming and pack mounts | SPEC + numbers |
| 2.21 | Breeding, eggs, babies, imprint, mutations | SPEC + numbers |
| 2.22 | Necro raising, revive, Dream items | SPEC + numbers |
| 2.23 | Life skills: XP sources, per-level stats, perk values, hidden perks | SPEC + numbers; hidden rule GAP-COOKED |
| 2.24 | Survival POIs and dressing per biome | GAP-COOKED |
| 2.25 | Sound and music | GAP-COOKED |

### 2.1 Survival basics (SPEC, BUILT)

| Item | Numbers | Tag | Source |
|---|---|---|---|
| Hunger and thirst | Full to empty in 60 / 40 min; empty: stamina regen ×0.5, then after 120 s health drains 0.5/s | SPEC (BUILT) | `tuning.json` `survival`; SN |
| Food fills hunger only; buffs from potions and brews | Rule | SPEC | DD Survival |
| Drinking | E at water (built gulps); waterskin 40 thirst (built); sea water is too salty (built `seaDrinkable 0`) | SPEC (BUILT) | `tuning.json` `water` |
| Carry weight | Bag 80 kg; over it: speed ×0.7, no dodge | SPEC (BUILT) | `tuning.json` `inventory` |
| Equip load | Max 60 kg + armour and node bonuses, separate from the bag | SPEC (combat.md 2.6) | `cooks/combat.md` |
| Durability | Tools, weapons, armour; repair at the matching station for 30% of the inputs | SPEC (BUILT) | `WRCrafting.h` `RepairCostFraction` |
| Beds and bedrolls | Respawn points; a bed rests 60 s between wakes; a bedroll is used up when you wake on it | SPEC (BUILT) | `tuning.json` `beds` |

### 2.2 Rest, Rested and comfort

**Rest (GAP-COOKED, answers DR #13 with the proposed default).** "Rest" means finishing a sleep in a bed or bedroll you own or your clan owns.
- Interact with the bed: the hero lies down (real clip, `sleep_enter` 1.2 s), the screen dims to 30% over 2 s, a 10 s sleep runs. Any damage, hunger or thirst at 0, or moving cancels it with no Rest.
- On a finished sleep: **Rest** fires once. It restores all max HP lost to Void (ST Void), re-arms Deathless (ST Necromancy), halves world corruption (2.5), and grants Rested.
- Sleeping in a bedroll doesn't use it up; only waking there after death does (BUILT rule unchanged).
- In multiplayer time never skips. Sleeping is a 10 s commitment in the open world, so you sleep behind walls.

**Rested (SPEC + numbers; DD Survival "Rested buff from sleeping"; R4).**

| | Value |
|---|---|
| Effect | Stamina regen +20%; hunger and thirst drain ×0.8; life skill XP +10% |
| Length | Bedroll 10 min; bed 20 min; +1 min per comfort level (max 20), so 40 min at most |
| Refresh | Sleeping again restarts it at the new length |
| HUD | A small bed icon under the vitals with minutes left; pulses at 1 min |

**Comfort (SPEC + numbers; BL "decor raises the comfort buff", DR #30 buff in Full release; R4).** Comfort is counted at the bed when you lie down: every decor piece within 12 m that is **under a roof**. Only the highest piece in each group counts, so variety beats stacking (Valheim's rule). Comfort 0 to 20.

| Group | Pieces and comfort |
|---|---|
| Seat | Chair 1, bench 1, carved throne 2 |
| Table | Table 1, long table 2 |
| Bed | Bed 1, fur bed (Woolly drudge or Puffram fleece) 2 |
| Floor | Rug 1, biome rug 2 |
| Wall | Banner 1, tapestry 2 |
| Fire | Brazier 1, campfire 1, fireplace 2, the claim's Hearth within 12 m 2 |
| Light | Candles or lantern 1, crystal lamp or sunstone lamp 2 |
| Green | Potted plant 1, flowering pot (any rare crop in a pot) 2 |
| Bath | Wooden tub 2; **hot spring bath** 3 (placed on a Volcanic vent or Tundra hot spring only; real geothermal baths) |
| Music | Hand drum 1, lyre 1 (only one counts) |
| Mirror | Mirror 1 (`cooks/ui-controls.md`) |
| Trophies | 1 per different trophy mounted, up to 3 |
| Monument | A boss monument 3 (2.18) |
| Biome set decor | Any piece from a biome building set 2 (2.17) |
| Hearth offering | A cooked meal placed on a new Hearth in its first game day: +2 to every bed in that claim for good (the Domovoi custom of feeding the hearth) |

Groups add up past 20; the cap is 20. The build bar shows a comfort number on every decor card. Standing in a claim, the HUD shows "Comfort 14" beside the bed icon.

### 2.3 Darkness and light (SPEC + numbers; DD "Darkness is dangerous, so caves need light"; R4)

- **Lit / unlit:** a player is **unlit** when no light source reaches them and the sky light is below the night threshold (all cave interiors, the catacombs, the fungal caverns, and open ground at night in the Graveyard and the Void wedge, which have no moon).
- **Unlit players are hunted:** dark-hunters (Gloomhowl, Echobat, Gravehound, Wraithmoth, Cryptstalker, Hollowfang, Riftmaw, Rotmaw, Webspinner) get **double aggro range** on them (the 3.3 Graveyard rule, extended to every dark-hunter). Nothing else changes; you can fight in the dark if you choose, and you can die there.

| Light | Radius | Burn | Where | Notes |
|---|---|---|---|---|
| Torch | 6 m | 20 min | Built piece and held item | Held in the off hand: no shield; goes out underwater; rain halves its burn time |
| Lantern (workbench L2: bronze 2, glass 1, fiber 2) | 9 m | 90 min per oil (animal fat or fish oil) | Belt hook | Both hands free; refills from the bag |
| Glowmoth, Wickbeetle, Lanterntoad, Glowgrub, Glowray, Lumenhorn tames | 12 m aura | None | Following tame | Utility tames (BUILT for Glowmoth) |
| Brazier / campfire | 10 m | 2 h per 10 wood | Placed | Stops corruption building within 10 m |
| Sunstone lamp (workbench L3: sunstone 2, bronze 2) | 10 m | 1 h of light per 1 h of daylight, holds 8 h | Placed outdoors | Desert tie; no fuel |
| Crystal lamp (workbench L4: resonant crystal 2, steel plate 1) | 10 m | Permanent | Placed | Crystal caves tie |
| Glowmelon lantern (hand: glowmelon 1, fiber 1) | 5 m | 30 min, then eat it | Held | Crystal crop tie |

### 2.4 Weather that matters (SPEC + numbers; DD Survival "storms, blizzards, heat waves"; R1)

Each biome rolls its weather every 30 min. Weather never adds a temperature meter (DD Ruled out); every effect is a gear check or a visibility change with a clear counter. Signs come 60 s ahead: a wall of cloud, ash or sand on the horizon and a rising ambient bed. Biome **events** (FULL-GAME 3.3) are separate and bigger.

| Biome | Weather (chance per roll, length) | Effect | Counter |
|---|---|---|---|
| Volcanic | **Ashfall** (30%, 10 to 20 min) | Vision 60 m; thirst ×1.25 outdoors; outdoor crop plots pause | A helmet with a face cover; Forgeplate; a roof over plots |
| Storm peaks | **Thunderstorm** (40%, 10 to 25 min) | The 3.3 strike rule (above 600 m, a strike every 20 to 40 s on the highest player in a 60 m cell: 1.0 s ground glow, 60 damage, 0.8 s stun); gusts ×2; windmills ×2 output | Grounded root, grounding charm, Stormweave 4, a tamed Rodcrest within 30 m of the base |
| Desert | **Heat wave** (35% of day rolls, 20 min) | Thirst ×1.5 in direct sun (not in shade or under a roof) | Shade, Dunewrap 2, sand veil, extra water |
| Ancient forest | **Rain** (40%, 15 min) | Every outdoor plot counts as irrigated while it rains; burns last half as long; tracks fade 2× faster | None needed; a gift to farmers |
| Sky islands | **Gale front** (30%, 10 min) | 3.3 gusts every 4 to 8 s instead of 8 to 15; flying mounts spend +50% stamina flying into the wind | Anchor boots, Skysilk 4, fly with the wind |
| Crystal caves | None underground; the small surface takes the nearest neighbour's roll | | |
| Frozen tundra | **Blizzard** (35%, 15 min) | Vision 40 m; off-path deep snow run ×0.85; stamina regen ×0.8 in the open | Rimeguard 2, frost gear (BUILT), a roof within 6 m |
| Swamp | **Fog** (50%, 20 min) | Vision 30 m; Murkjaw and Bogstalker ambush from 2× further | Lanterntoad or lantern (fog doesn't cut light radius) |
| Ocean & reefs | **Squall** (30%, 15 min) | 2 m waves: boats roll, surface swimming ×1.5 stamina; lightning strikes the tallest mast on open water every 40 s (1.0 s warning, 60 damage) | Dive under; Tidecoral 2; a grounding charm on the mast (2.16) |
| Graveyard | **Corpse fog** (40% of night rolls, 20 min) | Every light radius ×0.7 | Bigger lights; Gravecloth 4 |
| Fungal caverns | **Spore bloom** (every 20 min in the halls, 3 min) | Hazard spore clouds spread 50% wider | Filter mask, Sporeweave 4, Immune system |
| Corrupted wasteland | **Grey wind** (30%, 15 min) | Corruption builds ×2; footsteps muffled 30% (creature tells always play at full) | Gravecloth 4 or Voidglass 2; stay near a brazier |

Weather is drawn and heard in the biome's own look (rain sheets, ash flakes, sand walls, snow drift). Music never stops for weather; it ducks no more than 4 dB (3.3).

### 2.5 Corruption (SPEC + numbers; DD Survival; R4)

A world meter, 0 to 100, shown as a thin ring around the hunger and thirst arcs only above 0. The HUD calls it **Corruption**; the Void path's resource bar is labelled **Void** so the two never read alike.

| | Rule |
|---|---|
| Where it builds | Graveyard and Corrupted wasteland only (DD names these two) |
| Build rate | +1 per 6 s at night, underground or inside a rift zone; +1 per 15 s by day on the surface |
| Stops building | Within 10 m of a brazier, campfire or Hearth; a Wickbeetle or lantern halves the build rate |
| Falls | −1 per 3 s outside those biomes; halved by Rest; −60 from a Cleansing draught (2.12) |
| 25 | Healing received −10% |
| 50 | Stamina regen −15%; dark-hunters treat you as unlit |
| 75 | Max stamina −20; whispers join the mix (a planned sound layer) |
| 100 | **Marked:** every creature of the biome has double aggro range on you; your path resources fill 25% slower |
| Matching path | A Necromancer in the Graveyard or a Void walker in the Wasteland takes no penalties; path damage +3 / +6 / +10 / +15% at 25 / 50 / 75 / 100 (DD "minor buffs if the biome matches your path") |
| Death | Corruption carries through death (no reset helper) |

### 2.6 Swimming, breath and death

| Item | Numbers | Tag |
|---|---|---|
| Swimming and diving | Built swim volumes, strokes, drinking | SPEC (BUILT, `WRWater.cpp`) |
| Breath | 30 s base (3.3), +0.6 s per Swimming level (90 s at 100), Diver ×1.5; out of breath: 10 damage a second | SPEC (LS) + numbers |
| Crushing depth | Below 25 m: 5 damage a second (3.3); Pressure perk to 40 m; Tidecoral 4 to 40 m; Abyss diver no limit | SPEC + numbers |
| Swim stamina | 6 a second swimming fast, 0 floating; Strong stroke −30%; Seaborn 0 | SPEC + numbers |
| **No floors** | Hunger, thirst, drowning, depth, corruption, weather strikes, darkness and every status can kill. Nothing holds HP at 1 (CAL T10, the 1-HP rule) | GAP-COOKED |

### 2.7 Crops (SPEC + numbers; crop list GAP-COOKED; R3)

**SPEC:** crop plots, plant → grow → harvest, common 20 min, rare 2 h (BUILT, `tuning.json` `crops`); biome crops, irrigation (Hearth 3), greenhouses for off-biome crops (Hearth 4) (DD Farming; CF); Farming perks (LS).

**How a plot works (numbers GAP-COOKED):**
- The built `crop_plot` (4 × 4 m) holds **9 plants of one crop**. Planting takes 9 seeds of that crop (a quick plant-all from the bag).
- **Four visible stages**: sprout (0 to 25%), leaf (25 to 60%), flower (60 to 90%), ripe (90 to 100%). Each stage is its own mesh. Ripe plants sway and catch light so a field reads ripe at 30 m.
- **Home biome only:** a crop grows in its own biome. Anywhere else the plot shows "Wrong ground" and nothing grows, unless it's inside a greenhouse (Hearth 4) or the planter has the Grafting perk (grows at 50% yield).
- **Irrigation (Hearth 3):** a plot is watered when its centre is within 8 m of water: a river, a lake, a **well** (workbench L2) or an **irrigation channel** (L3) fed from water or a water wheel sluice. Watered plots grow in ×0.67 the time (×0.5 with Water wise). Forest rain counts while it rains. No watering can, no chore.
- **Fertiliser:** compost (2.11) or bone meal (grinder, 2.15: 4 bones → 2 bone meal). One fertiliser per plot per harvest: +25% yield.
- **Pollinators:** a Hiveback hive or a tamed Glimmerfinch within 20 m: growth time ×0.85 each, both together ×0.72.
- **Harvest:** E on the ripe plot harvests all 9 plants (a real clip, 1.4 s). Yield: common 9 to 12, rare 3 to 5; seeds back: common 9 to 11 (always enough to replant), rare 1 to 3 (Seed keeper +50%). XP as built (`plant` 10, `harvest` 20).
- **Wild seeds:** every crop also grows wild in its biome as a resource node (in old farm ruins, meadows, ash pits, pond edges). Hitting it gives the crop and 2 to 4 seeds. Trufflers, Bogsnouts and Scarab diggers dig up seeds of their biome.
- **Greenhouse (Hearth 4):** glass walls and roof on any foundation; a plot inside a greenhouse that's fully roofed and at least 80% walled grows any biome's crop at full speed. Greenhouse lights (4 power, 2.15) let it grow at night and underground.
- **Dark crops** (Crystal and Fungal) grow only where there's no daylight: underground, in caves, or in a **dark shed** (any fully roofed room with no windows). This is the real Yorkshire forced-rhubarb shed.

**The 24 crops (GAP-COOKED; replaces the first-draft list).** Signature resources (sulfur, lichen, stormglass, sunstone, heartwood, levistone, resonant crystal, bloodmoss, tidecoral, soulstone, deepcap, voidglass) are **never** farmable: they stay "found nowhere else" (DD).

| Biome | Common crop (20 min) | Rare crop (2 h) | Grows | Uses |
|---|---|---|---|---|
| Volcanic | **Fire pepper**: squat red-black chillies on a waxy bush | **Ember grape**: dusky vines with ember-red fruit | Grape only in an **ash pit** plot (a plot sunk into ash ground; La Geria) | Peppers: every Volcanic dish, burn-resist potion. Grapes: raisins (never spoil), Ember wine (brew) |
| Storm peaks | **Stormrye**: tall grey-gold rye that bends flat in the wind and stands back up | **Gentian**: deep blue alpine flower with a bitter root | Rye needs a windy plot (above 300 m or any Storm plot) | Flour, bread, kibble. Gentian root: shock-resist potion, Swiftfoot brew |
| Desert | **Prickly pear**: paddle cactus with magenta fruit | **Date palm**: a young palm on its own plot; 9 dates a harvest | Both on sand or oasis ground | Pears: food, thirst (+10), Dromad taming food (CR). Dates: sweet, never spoil, honey cake, tagine |
| Ancient forest | **Meadow oats** | **Iðunn apple**: a small orchard tree with gold-red apples | Anywhere in the forest | Oats: porridge, kibble, horse feed. Apples: elixirs, the Dadeni draught (revive), pie |
| Sky islands | **Windflax**: blue-flowered flax | **Cloudberry**: amber berries on low leaves | Cloud gardens and any island plot | Flax: fiber ×2, linen (loom), flaxseed oil (lantern oil). Cloudberries: jam (keeps 3 days), blind-resist potion |
| Crystal caves | **Candle rhubarb**: pale pink stalks grown in darkness | **Glowmelon**: a melon that glows soft blue-green | Dark only | Rhubarb: food, pie, red dye. Melon: food (+20 thirst), Glowmelon lantern, Nightsight brew |
| Frozen tundra | **Snow turnip**: purple-white turnip that sweetens under frost | **Roseroot**: fleshy arctic herb with a rose-scented root | Tundra ground | Turnips: stews, Woolly drudge and Snowpuff feed. Roseroot: frostbite-resist, stamina tonic |
| Swamp | **Bog rice**: grows standing in water | **Bloodroot**: white flower whose root bleeds red sap | Rice only in a plot placed in water up to 50 cm deep | Rice: food, sake-style "Mire wine" brew. Bloodroot: bleed-resist potion, red dye, hex ink |
| Ocean & reefs | **Sea kelp** | **Sea grapes**: green bead-like seaweed | On a **kelp line** (workbench L2) in sea water 2 to 6 m deep, or a sea pen | Kelp: food, Driftback and Kelpgrazer feed, kelp ash (glass flux, iodine for potions). Sea grapes: food (+15 thirst), Deepbreath brew |
| Graveyard | **Black rye**: dark-eared rye that grows in grave soil | **Asphodel**: tall pale lily of the underworld meadows | Graveyard ground | Rye: bread; 1 harvest in 10 carries **ergot** (2 narcotic). Asphodel: decay-resist, Cleansing draught, the undead ritual |
| Fungal caverns | **Oyster cap**: on a **log bed** (workbench L1) | **Lion's mane**: a white shaggy mushroom | Dark only | Caps: food, soup. Lion's mane: rot-resist, Focus elixir |
| Corrupted wasteland | **Ashen grain**: grey grain that only takes in withered soil | **Mandrake**: forked root with a crown of dark leaves | Withered ground only (or a greenhouse) | Grain: flour. Mandrake: elixirs, narcotic ×3. **Harvesting a mandrake screams**: a 30 m aggro pulse and a 2 s stagger on the harvester unless they wear **wax earplugs** (hand: Hiveback wax 1) |

Berries (BUILT, wild shrubs) stay as they are.

### 2.8 Animal husbandry and beekeeping (SPEC + numbers; R3)

**SPEC:** animal pens, feed → produce (Pumicehog, Snowpuff BUILT, every 600 s); husbandry and beekeeping (DD Farming); Husbandry perks (LS).

**Pens (numbers GAP-COOKED):**
- The built `animal_pen` holds **4 animals** (Large pens +2). A **feed trough** inside holds 50 feed; each production eats 1 feed (Content herd: 50% chance it doesn't).
- No feed, no produce. Farm animals in pens don't starve to death; they stop producing and look thin (ribs show, head down).
- **Breeding in pens:** two or more of one kind with feed make one young every 60 min up to capacity. Young grow to adult in 30 min. (Tame breeding with stats is 2.21; farm stock is simple.)
- **Sea pen** (workbench L3: wood 40, rope 10) sits in sea water for Kelpgrazer, Ripplefin and sea grapes.
- Products show on the body before collection: wool grows shaggy, eggs appear in the nest, slime pools under the snail. E collects all of one pen.

| Animal (biome) | Product (every) | Feed | Notes |
|---|---|---|---|
| Pumicehog (Volcanic) | Animal fat (10 min) | Any crop, berries, spoiled food | BUILT species; fat = lantern oil, fire pots, pies |
| Snowpuff (Tundra) | Egg (10 min), feathers (20 min) | Snow turnip, lichen, seeds | BUILT species |
| Puffram (Storm) | Wool (20 min) | Stormrye, any grain | Fur bed, Stormweave |
| Sandhen (Desert) | Egg (10 min) | Prickly pear, seeds | |
| Fernsheep (Forest) | Wool (20 min), milk (10 min) | Oats, greens | |
| Dappledoe (Forest) | Shed antler (30 min): bone and glue | Oats, apples | Meat and hide only when culled |
| Puffwing (Sky) | Glider feather (20 min) | Windflax seed | Glider, Skysilk |
| Chimecrab (Crystal) | Crystal dust (15 min) | Any meat or fish | Gemsniffer taming food (CR); glass flux |
| Reedfowl (Swamp) | Egg (10 min) | Bog rice, insects | |
| Slimeshell (Swamp) | Slime (15 min) | Greens | Alchemy base for every brew from L2 |
| Kelpgrazer (Ocean) | Milk (10 min) | Kelp | Sea pen |
| Tidefowl (Ocean) | Egg (10 min) | Fish scraps | |
| Ripplefin (Ocean) | Raw fish (15 min) | Bait | Sea pen |
| Mournhare (Graveyard) | Fur (20 min) | Any crop | Breeds ×2 as fast (hares) |
| Capling (Fungal) | Capling flesh (15 min; regrows on it) | Compost | |
| Puffball (Fungal) | Spore sac (20 min) | Compost | Alchemy, filter mask |
| Withergoat (Void) | Milk (10 min), hide (30 min) | Ashen grain | |

**Beekeeping (SPEC + numbers; DD; R3):** a **beehive** (workbench L2: wood 20, Hiveback wax 4) houses one tamed **Hiveback** (Forest, steal an egg, CR). It makes honey every 15 min and wax every 30 min (Bee whisperer +50%; Royal jelly perk: 5% of collections give royal jelly, an elixir ingredient). Crops within 20 m grow ×0.85 time. Hivebacks forage: no feed needed while a crop or wild flower is within 40 m. Honey never spoils.

### 2.9 Fishing (SPEC + numbers; DD "biome-specific fishing", LS Fishing; R6)

**Rods and line:**

| Rod | Station | Cast | Line class | Notes |
|---|---|---|---|---|
| Crude rod | Hand: wood 3, fiber 6 | 12 m | 1 | Shore only |
| Bronze-reel rod | Workbench L2: wood 4, bronze 2, fiber 10 | 20 m | 2 | |
| Iron rod | Workbench L3: wood 4, iron 3, linen 4 | 25 m | 3 | Deep water spots (with Deep line) |
| Steel rod | Workbench L4: steel plate 2, heartwood 2, linen 6 | 30 m | 4 | Legendaries need line 4 |

A fish above your line class breaks the line on its first run.

**Bait:** worms (dug from soil nodes and compost bins), insects (Lanterntoad food, swamp reeds), fish scraps (butchered fish), chum (spoiled food). **Biome baits** (Bait maker perk; workbench: 10 worms + 1 biome item, such as sulfur grit, lichen, cactus pulp, heartwood grub): the biome's rare fish bites 2× as often.

**The catch (one verb: reel):**
1. **Cast:** hold attack to charge (0.3 to 1.2 s sets distance), release to cast. A real cast clip, the lure arcs, the bobber lands with a plop.
2. **Wait:** a bite comes in 6 to 20 s (−0.5% per Fishing level). 0 to 2 nibbles first (the bobber twitches, a soft tick); the real bite dunks the bobber under with a deep "thunk" and a rumble.
3. **Strike:** press attack within 0.6 s of the real bite. Late or on a nibble: the fish is gone and the bait with it.
4. **Fight:** hold attack to reel. The fish alternates **rest** (reel freely, it comes closer) and **run** (2 to 4 s). Every run is told 0.4 s ahead by a splash and a rising line squeal. Reeling during a run raises line tension 35% a second; release to let the line run. Tension at 100% snaps the line (bait and the hook lost). Reeling costs 3 stamina a second. Dodge or guard cancels (the fish is lost).
5. **Land:** the fish is lifted out with a real clip and shown in the hand for 1.5 s (size and weight on screen).

| Fish tier | Fight | Runs | XP |
|---|---|---|---|
| Common | 8 to 15 s | 2 to 3 | 20 |
| Rare | 20 to 30 s | 4 to 6, longer | 60 |
| Legendary | 60 to 90 s, 3 phases (each phase shorter rests, faster runs, and one leap with a 0.5 s tell where you must release) | 10+ | 500, first catch only; then 100 |

**Fish by biome (real fish behind each):**

| Biome | Water | Common | Rare | Legendary and where it lives |
|---|---|---|---|---|
| Volcanic | Steaming river, hot crater lake | Vent tilapia | Ember char | **Old Cinderjaw**, a scarred hot-water carp, in the crater lake under the lava-light |
| Storm peaks | Snowmelt tarns, cliff pools | Ridge trout | Thunder grayling | **The Lightning Pike**, in the Hollow Spire's flooded floor |
| Desert | Oases, wells | Oasis catfish | Sun koi | **The Sandfather**, an ancient catfish in the deepest oasis well |
| Ancient forest | Streams, falls | Brook trout | Silver salmon | **Grandfather Sturgeon**, in the pool under the great falls |
| Sky islands | Waterfall pools | Cloud minnow | Skyfin (flying fish that leap from falls) | **The Aethercarp**, in the floating shrine's pool |
| Crystal caves | Underground lakes | Blind cavefish | Glass shrimp | **The Prism Eel**, in the Silent Vault's lake |
| Frozen tundra | Frozen lakes (cut a hole with a pick: ice fishing) | Arctic char | Ice cod | **The Pale Pike**, under the glacier lake |
| Swamp | Murk pools | Bog eel | Snakehead | **The Old Gar**, a 3 m alligator gar in the Sunken Coven pool |
| Ocean & reefs | Shore, reef, open sea (from a boat) | Mackerel, red snapper | Bluefin (deep line) | **The Lazarus Fish**, a coelacanth in the deepest sea cave |
| Graveyard | Catacomb cisterns | Ghost carp | Lamprey | **The Drowned Bell**, a catfish with a bronze bell lodged in its jaw |
| Fungal caverns | Flooded grottos | Glowfin | Cave crayfish | **The Mycelial Koi**, in the Heart Grotto |
| Corrupted wasteland | Rift pools | Hollow perch | Null eel | **The Fish That Isn't**, a shape seen only by the light it bends, in the Unmade Place's pool; lands as voidglass and a trophy |

- Fish weight rolls 0.5× to 2× the species' base; size shows on the catch. Fish: raw fish (common 2, rare 4), fish scraps, and for rares a fish oil.
- **Legendaries** bite only with the **Legend catch** perk (LS), line 4 and the biome's bait, once per real day per player. The first catch gives a **mountable trophy** (decor, comfort trophy group).
- **Fish trap** (workbench L2: wood 10, fiber 10) placed in water: one common fish of that biome every 20 min, holds 6. XP 2 per fish collected.
- **Spear fisher** (perk, R6): spearfish underwater with a thrown or held spear; a hit on a fish in range lands it at once (common and rare only).
- **Treasure hook** (perk): 5% of catches are an item instead (a sunken coin purse, a ring of the biome's metal, a recipe note).
- **Monster line** (perk, R6): with a steel rod, a hooked swimming creature (Wavecharger, Floeback, Glowray) that's reeled in through a legendary-length fight is knocked out, ready to tame by its listed method (it doesn't change the method, DR #16).

### 2.10 Cooking and recipe discovery (SPEC + numbers; DD Farming "cooking stations, and recipes you discover by experimenting"; R3)

**The sated rule (GAP-COOKED, inside Raven's "food fills hunger only"):** every dish restores hunger (and some thirst). Better dishes also add **Sated**: for that many minutes hunger doesn't drain. Sated is hunger itself, not a stat buff, so no meal ever raises damage, health or stamina. Cooking's every-level boost ("food keeps hunger away longer", LS) is +1% sated time per level.

**Stations:**

| Station | Build | Makes |
|---|---|---|
| Campfire | Hand: stone 5, wood 4 (Survival category) | Roasts one ingredient: meat, fish, vegetables (15 s); light 10 m |
| Cooking pot | Workbench L1: copper ingot 4, stone 6; sits over a campfire or fireplace | **Discovery cooking**, 4 ingredient slots, 20 s |
| Drying rack | Workbench L1: wood 6, fiber 4 | Jerky, dried fruit, dried fish (15 min, outdoors) |
| Oven | Workbench L2: stone 30, copper 4 | Bread, pies, cakes (45 s) |
| Smokehouse | Workbench L2: wood 30, stone 10 | Smoked meat and fish: 10 at a time, 10 min, burns 5 wood |
| Salting barrel | Workbench L2: wood 12, copper 2 | Salted meat and fish: 1 salt each, 5 min |
| Ice box | Workbench L3: stone 20, iron 4, hide 4 | Storage, 20 slots, spoilage ×0.2 (2.11) |

Meals no longer cook at the workbench: the built `cooked_meat` recipe moves to the campfire (same item).

**Discovery:** drop any 1 to 4 ingredients in the pot and cook. Each ingredient has a **food group**: Meat, Fish, Grain (grains and flour), Veg, Fruit, Herb (peppers, gentian, roseroot, herbs), Dairy (milk, eggs), Sweet (honey, dates, cloudberry jam), Mushroom, Water (a waterskin's water). The pot matches the **pattern** below. A new match is a discovery: a parchment card slides in ("New recipe: Fire pepper stew"), +100 Cooking XP, and the dish joins the cooking list for one-click cooking from then on. No match: **Grey mush** (8 hunger). Recipe hints are found in the world, not given by NPCs: recipe scraps in old kitchens, carved on a ruined oven, a dried-out pot with its ingredients still beside it. **Recipe sense** (perk) shows how many groups a pattern needs.

| Dish (pattern) | Hunger | Thirst | Sated | Keeps |
|---|---|---|---|---|
| Roast meat (1 Meat on a campfire) | 35 | 0 | 0 | 2 h (BUILT item `cooked_meat`) |
| Roast fish (1 Fish) | 30 | 0 | 0 | 90 min |
| Roast vegetable (1 Veg) | 15 | 0 | 0 | 3 h |
| Grey mush (no match) | 8 | 0 | 0 | 1 h |
| Porridge (Grain + Dairy or Water) | 35 | 10 | 5 min | 2 h |
| Stew (Meat + Veg + Veg + Water) | 60 | 15 | 15 min | 3 h |
| Fish soup (Fish + Veg + Water) | 45 | 35 | 10 min | 3 h |
| Mushroom broth (Mushroom + Mushroom + Water) | 35 | 30 | 8 min | 3 h |
| Omelette (Dairy egg + Dairy egg + Veg or Herb) | 40 | 0 | 10 min | 2 h |
| Skewer (Meat or Fish + Veg + Herb) | 45 | 0 | 10 min | 2 h |
| Bread (oven: Grain + Grain + Water) | 30 | 0 | 5 min | 6 h |
| Pie (oven: Grain + Fruit + Dairy or animal fat) | 55 | 0 | 20 min | 8 h |
| Honey cake (oven: Grain + Sweet + Dairy) | 60 | 0 | 25 min | 8 h |
| Hunter's feast (pot: Meat + Meat + Veg + Herb, then on a table) | 100 per portion, 6 portions | 20 | 30 min | 2 h | 

**Biome dishes (12):** the pattern plus that biome's crop; each is a discovery of its own and fills more.

| Biome | Dish | Pattern | Hunger / thirst / sated |
|---|---|---|---|
| Volcanic | Fire pepper stew | Stew with Fire pepper as one Veg | 70 / 15 / 20 min |
| Storm peaks | Stormrye flatbread with gentian | Oven: Stormrye flour + Water + Gentian | 45 / 0 / 15 min |
| Desert | Date and pear tagine | Pot: Meat + Date + Prickly pear + Water | 70 / 20 / 20 min |
| Ancient forest | Iðunn apple pie | Oven: Oats + Iðunn apple + Dairy | 70 / 0 / 30 min |
| Sky islands | Cloudberry porridge | Pot: Windflax seed or any Grain + Milk + Cloudberry | 50 / 15 / 20 min |
| Crystal caves | Glowmelon and rhubarb compote | Pot: Glowmelon + Candle rhubarb + Sweet | 45 / 30 / 15 min |
| Frozen tundra | Snow turnip and char chowder | Pot: Arctic char + Snow turnip + Snow turnip + Milk | 65 / 35 / 20 min |
| Swamp | Bog rice and eel | Pot: Bog eel + Bog rice + Herb + Water | 65 / 25 / 20 min |
| Ocean & reefs | Kelp-wrapped snapper | Pot: Red snapper + Sea kelp + Sea grapes | 60 / 30 / 20 min |
| Graveyard | Mourner's bread | Oven: Black rye flour + Mournhare fat + Water | 50 / 0 / 20 min |
| Fungal caverns | Oyster cap and crayfish broth | Pot: Oyster cap + Cave crayfish + Mushroom + Water | 55 / 35 / 20 min |
| Corrupted wasteland | Grey loaf with goat cheese | Oven: Ashen grain flour + Withergoat milk + Withergoat milk | 55 / 0 / 20 min |

Cooking perks with numbers: Bulk cook (cook 4 at once), Preserver (spoilage ×0.7), Shared meal (eating gives each party member within 8 m 50% of the dish), Camp chef (any recipe at a campfire), Hearty (hunger drain ×0.8), Master chef (every dish also restores thirst equal to half its hunger), Banquet (a Hunter's feast made by you holds off hunger (sated) for 45 min for every clan member who takes a portion, 12 portions).

### 2.11 Spoilage and preservation (SPEC + numbers; DD Farming; R3)

Every food has **freshness** 100% to 0, shown as a thin bar on its icon. Spoilage is stored as a timestamp per stack (no ticking). At 25% and below a dish gives half its values; at 0 it becomes **Spoiled food**.

| Item | Keeps |
|---|---|
| Raw meat 40 min · raw fish 30 min · milk 1 h · eggs 3 h · fresh crops 4 h · fruit 3 h | |
| Cooked dishes: as in the table above (90 min to 8 h) | |
| **Salted** meat or fish (1 salt) | 24 h |
| **Smoked** meat or fish | 48 h |
| **Jerky, dried fruit, dried fish** | 72 h |
| Grain, flour, dates, raisins, honey, jam (3 days), seeds | Never (jam 72 h) |

| Where it's kept | Spoilage rate |
|---|---|
| Bag, chest | ×1 |
| Any chest placed in the Frozen tundra | ×0.5 (cold ground; not a temperature meter, a place rule) |
| Ice box | ×0.2; burns 1 **ice block** per 4 h. Ice blocks: mined from tundra ice, frozen lakes and glacier faces, or dug by a Rimeshell. In the tundra an ice box needs no ice |
| Cold store (Frost biome machine, 2.15) | ×0.05 for every container in the room |

**Salt:** a **salt pan** (hand: stone 10) on any sea beach makes 1 salt per 10 min in daylight; rock salt nodes on desert hardpan and in Storm peaks caves (halite).
**Spoiled food** goes into a **compost bin** (workbench L1: wood 12): 4 spoiled food or plant waste → 2 compost in 20 min (worms appear in it for bait). Pigs and Bogsnouts eat spoiled food; it is also fishing chum.

### 2.12 Alchemy: potions, brews and elixirs (SPEC + numbers; CF tech levels; R3 and R4)

**SPEC:** potions BUILT (minor healing 45, healing 90, antidote cleanse, cooldown 6 s; belt slots); per-level alchemy rows from CF: L2 first brews, L3 status-resist potions and resource tonics, L4 greater healing and buff brews, L5 elixirs with rare effects. Alchemy perks (LS). Drinking is the committed 0.9 s clip from combat (CAL 1.10).

**Rules (GAP-COOKED):** potions are instant (belt). **One brew active** at a time; a new brew replaces it (BotW's one-effect rule). **One elixir active** in its own slot. Mixologist lets one brew hold two brew effects. Brews and elixirs show as icons beside the Rested icon with minutes left. Every brew from L2 up uses **slime** (Slimeshell) or **royal jelly** as its base, so the Swamp and the Forest feed every alchemist.

| Level | Item | Inputs | Effect | Lasts |
|---|---|---|---|---|
| 2 | **Stonehide brew** | Slime 1, Snow turnip 2, stone dust 4 | +10 poise | 10 min |
| 2 | **Packmule brew** | Slime 1, Meadow oats 3, Pumicehog fat 1 | +25 kg bag capacity | 15 min |
| 2 | **Nightsight brew** | Slime 1, Glowmelon 1, fish oil 1 | See as dusk in total dark out to 20 m (you still count as unlit) | 10 min |
| 2 | **Ember wine** | Ember grape 4, honey 1 | Burn buildup ×0.75 | 10 min |
| 2 | **Mire wine** | Bog rice 4, honey 1 | Bleed buildup ×0.75 | 10 min |
| 3 | **Resist potions** (one per status): Burn (Fire pepper + sulfur), Frostbite (Roseroot + lichen), Bleed (Bloodroot + bloodmoss), Poison (Puffball spore sac + deepcap), Rot (Lion's mane + slime), Shock (Gentian + stormglass), Soak (Sea grapes + tidecoral), Blind (Cloudberry + sunstone), Decay (Asphodel + soulstone), Mark (Candle rhubarb + resonant crystal), Curse (Ashen grain + voidglass) | Slime 1 + the two | That status buildup ×0.5 | 10 min (a brew; takes the brew slot) |
| 3 | **Path tonic** (one recipe; the path material set when brewing chooses the path, like infusing a weapon) | Slime 1, any 1 path material, honey 1 | That path's resource fills 25% faster | 10 min |
| 3 | **Cleansing draught** | Asphodel 2, sunstone 1, slime 1 | Corruption −60, and no corruption builds for 10 min | Instant + 10 min |
| 3 | **Ward charm** (hazard item, 2.14) | Resonant crystal 3, copper 2 | | |
| 3 | **Filter mask** (hazard item, 2.14) | Deepcap 2, fiber 6, charcoal 2 | | |
| 4 | **Greater healing potion** | Royal jelly 1 or slime 2, Iðunn apple 1, healing potion 1 | Heal 150 | Instant |
| 4 | **Ironbark brew** | Slime 1, heartwood resin 2, Roseroot 1 | +20 poise, stance damage taken −10% | 15 min |
| 4 | **Swiftfoot brew** | Slime 1, Gentian 2, Puffwing feather 2 | Run and sprint +8%, roll stamina −15% | 15 min |
| 4 | **Deepbreath brew** | Slime 1, Sea grapes 3, kelp 2 | Breath ×2, swim stamina −30% | 20 min |
| 4 | **Beastfriend brew** | Royal jelly 1, honey 2, Cloudberry 2 | Taming 25% faster; your tames +10% damage within 15 m | 20 min |
| 4 | **Wrath brew** | Slime 1, Fire pepper 4, Bloodroot 1 | +10% damage dealt, +15% damage taken | 10 min |
| 5 | **Iðunn's elixir** | Royal jelly 1, Iðunn apple 3, starsteel dust 1 | Out of combat (8 s after your last hit), heal 1% max HP a second | 30 min |
| 5 | **Elixir of the Second Wind** | Royal jelly 1, Mandrake 1, Roseroot 2 | The first time stamina hits 0, refill it to 50% (once per 60 s) | 30 min |
| 5 | **Focus elixir** | Royal jelly 1, Lion's mane 2, resonant crystal 1 | Both path resources start full after any rest of 5 s out of combat | 30 min |
| 5 | **Mandrake draught** | Mandrake 2, honey 1, slime 1 | Creatures with less than 25% HP that you hit fall unconscious (ready to tame by knockout, if that's their method) | 30 min |
| 5 | **Dadeni draught** (tame revive, 2.22) | Bond token 1, core shard 1, Iðunn apple 3, bloodmoss 2 | Revives one dead tame | |
| 5 | **Philosopher's elixirs** (perk only): Elixir of Stillness (no corruption gain at all) and Elixir of the Deep (no breath limit) | Royal jelly 2, the biome's legendary fish 1, + Asphodel 2 / Sea grapes 4 | As named | 30 min |

### 2.13 Crafting: tiers, metals, ores, salvage, blueprints, runes

**SPEC (all kept):** one shared tree (base metal = tier, path material = element set forever, extras add bonuses); four stations levelled by the claim's Hearth; tech levels by distinct major boss cores 0 / 1 / 2 / 4 / 6; quality crude to masterwork with 0 to 3 sockets and stat rolls; gems and boss parts in sockets; upgrades +1 to +10 with mini-boss shards capped at Hearth level × 2; queues run while away; a broken Hearth stops crafting, cores stay safe (CF; `WRCrafting.cpp`). Weapon damage multipliers copper / bronze / iron / steel / starsteel 1.0 / 1.3 / 1.7 / 2.2 / 2.8 (SN).

**Ores by depth (SPEC rule "deeper caves hold better metal", numbers GAP-COOKED).** Every biome has 3 to 6 caves (3.3). Each cave has depth bands; the deepest band of a highland cave reaches band 4.

| Band | Where | Ores and minerals |
|---|---|---|
| 1 | Cave mouths, surface outcrops | Copper, tin, stone, clay |
| 2 | Midland cave halls | Iron, coal seams, gems (ruby, topaz, onyx: BUILT), bat roosts (saltpetre from guano) |
| 3 | Deep midland and highland caves | Rich iron, silver (rings, lanterns), gem clusters |
| 4 | The deepest highland chambers, and meteor craters from the world event (DD World) | **Star iron** |

| Material | Made at | Recipe | Level |
|---|---|---|---|
| Copper, bronze, iron ingots | Forge | BUILT | 1, 2, 3 |
| Charcoal | Charcoal kiln (workbench L2: stone 20, clay 10) | 10 wood → 4 charcoal, 2 min | 2 |
| **Steel ingot** | Forge | 2 iron ingot + 2 charcoal | 4 |
| Steel plate | Forge | 1 steel ingot → 2 plates | 4 |
| **Starsteel ingot** (CF: "forged with boss cores") | Forge | 2 steel ingot + 1 star iron + 1 **core shard** | 5 |
| Core shard | Forge (salvage) | A spare major boss core → 10 core shards. A core already fed to a Hearth can't be broken (cores stay safe) | 5 |
| Glass pane | Forge | 4 sand + 1 kelp ash or charcoal → 2 panes. Sand: any beach, desert dunes | 2 |
| Linen | Loom (2.15) or workbench | 4 fiber or 2 Windflax → 1 linen | 2 |

**Salvage (SPEC: "returns some materials, socketed extras destroyed"; numbers GAP-COOKED; R2).** At the matching station: 40% of the item's metal and main inputs (rounded down, at least 1), +10% per quality step above crude, 25% of the upgrade shards spent (rounded down). The path material and every socketed extra are destroyed (the card warns in crimson before you confirm).

**Blueprints (SPEC; numbers GAP-COOKED; R5).** A blueprint is an item. Reading it teaches the recipe to that character forever (journal "Known blueprints"); it can then be crafted at any station of the blueprint's tier. Drops: a major boss's first kill per player 100%, repeat kills 20%; portal bosses by rank (rank 1 10% up to rank 5 60%). Each major boss has 2 blueprints: its **boss weapon** (a unique moveset weapon, combat lane) and its biome's **set decor pack** (2.17). Legendary blueprints (CF L5) come only from rank 4 and 5 portals.

**Runes (SPEC: "crafted or found"; numbers GAP-COOKED; R9).** A rune sits in a socket like a gem. Crafted at the alchemy table (L3+) from **resonant crystal** (Crystal caves) + one biome material, or found in portals. One of each rune per item.

| Rune | Inputs | Effect |
|---|---|---|
| Rune of Thirst | Resonant crystal 2, bloodmoss 2 | Lifesteal 3% of damage dealt |
| Rune of Haste | Resonant crystal 2, stormglass 2 | Status buildup from this item +15% |
| Rune of the Anvil | Resonant crystal 2, sulfur 2 | Stance damage +10% |
| Rune of the Tide | Resonant crystal 2, tidecoral 2 | Stamina +3 per hit landed |
| Rune of Thorns | Resonant crystal 2, heartwood 2 | Melee attackers take 10% of the damage back |
| Rune of the Grave | Resonant crystal 2, soulstone 2 | Kills give +5 path resource |
| Rune of Clear Sight | Resonant crystal 2, sunstone 2 | Critical hits +10% |
| Rune of Hollowing | Resonant crystal 2, voidglass 2 | Ignore 10% of the target's armour |

### 2.14 Gear slots, biome armour sets, hazard gear, rings and talismans

**Slots (GAP-COOKED, needed for Raven's set bonuses; R2).** The built `Armor` slot becomes four: **Head, Chest, Hands, Legs**. Plus **Talisman** ×1 and **Ring** ×2 (DD Gear "talismans and rings"). Artifacts keep their own 3 slots (DD). Saves migrate: the old single armour item becomes the Chest piece, and its stats split across a matching four-piece kit made on load (hide, copper, bronze, iron: 15% head, 40% chest, 15% hands, 30% legs). Poise per piece follows the combat lane's totals (hide 8 to steel 40, biome sets 30 to 50, `cooks/combat.md` 2.6).

**The 12 biome armour sets (GAP-COOKED names and bonuses; RAVEN-PLANNED set bonuses at Hearth 4, CF; R2 for the first, then each biome's round).** Steel tier (×2.2 armour). Four pieces. Bonuses at 2 and 4 pieces (Set crafter perk: at 1 and 3). The 4-piece bonus always passes that biome's hazard (3.3), so the set is the endgame answer to its own land. Every set is fitted and tailored to the MetaHuman male and female bodies, cut to show a strong, attractive build where the culture and climate would (bare arms in the heat, open collars, fitted waists), never sanitised, never shapeless.

| Set | Biome | Load | Poise | Inputs (per full set) | 2 pieces | 4 pieces (+ hazard pass) | Look and palette |
|---|---|---|---|---|---|---|---|
| **Forgeplate** | Volcanic | Heavy | 50 | Steel plate 16, sulfur 8, obsidian 8, Magmaw hide 4 | Burn buildup ×0.5 | Walk lava unhurt; Heat fills 15% faster | Blackened plate with basalt inlays, seams that glow ember-orange when you fight, an open-throated gorget, scorched leather skirt. Black, iron grey, ember orange |
| **Stormweave** | Storm peaks | Light | 30 | Steel plate 6, stormglass 8, Puffram wool 10, copper 6 | Shock buildup ×0.5 | Never targeted by storm strikes; sprint stamina −25% | Quilted oilcloth coat over a fitted vest, copper wire stitched in lightning lines, fur collar, wind goggles. Slate, storm blue, copper |
| **Dunewrap** | Desert | Light | 30 | Steel plate 6, sunstone 8, linen 12, Dromad leather 6 | Blind buildup ×0.5; heat waves don't raise thirst | Sandstorm slow halved and vision 60 m (sand veil); Radiance fills at night at half rate | Layered linen wraps and a cropped vest, bare sun-bronzed shoulders and midriff, sunstone-studded belt, a veil that drapes from a gold circlet. Sand, white, sun gold |
| **Heartwood** | Ancient forest | Medium | 40 | Steel plate 10, heartwood 10, Fernsheep wool 6, resin 4 | Bleed buildup ×0.5 | Pass thornbrakes; standing still 2 s heals 1 HP a second | Lacquered heartwood plates with amber cores over green-dyed leathers, leaf-cut pauldrons, braided cords. Moss green, amber, bark brown |
| **Skysilk** | Sky islands | Light | 30 | Steel plate 6, levistone 8, Puffwing feathers 12, linen 8 | Fall damage −50% | Gusts never push you (anchor); glide 6 s from any jump (fall 300 cm/s, forward 900) | Pale fitted silks that ripple in any wind, feathered half-cape, levistone clasps that float a finger off the cloth. White, sky blue, silver |
| **Wardcloth** | Crystal caves | Light | 30 | Steel plate 6, resonant crystal 8, linen 10, Chimecrab dust 4 | Mark buildup ×0.5 | Walk through ward fields (ward charm); spell cost −10% | Deep violet long coat open over a fitted bodice or doublet, crystal studs that hum when you cast, rune-stitched gloves. Violet, midnight blue, crystal white |
| **Rimeguard** | Frozen tundra | Medium | 40 | Steel plate 10, lichen 8, Rimeclaw spines 4, Woolly drudge fleece 6 | Frostbite buildup ×0.5; no blizzard stamina penalty | Full grip on slick ice; Chill fills 15% faster | White fur mantle over scaled steel, Rimeclaw spines on the shoulders, frost-blue lacing. White, ice blue, steel |
| **Mirehide** | Swamp | Medium | 40 | Steel plate 10, bloodmoss 8, Murkjaw hide 4, Slimeshell slime 4 | Bleed buildup ×0.5 | Wade bleedwater unhurt (sealed waders); lifesteal +2% | Glossy dark croc-hide, hanging moss fringes, red-lacquered bone buckles. Black-green, blood red, bone |
| **Tidecoral** | Ocean & reefs | Medium | 40 | Steel plate 10, tidecoral 10, Spinecrab shell 4, kelp 6 | Soak buildup ×0.5 | Breath ×2; no crushing damage to 40 m (diving helm) | Coral plate grown over steel, pearl inlay, sea-glass beads, a scale-mail skirt. Coral pink, sea teal, pearl |
| **Gravecloth** | Graveyard | Light | 30 | Steel plate 6, soulstone 8, Mournhare fur 8, linen 8 | Decay buildup ×0.5 | Corruption builds at half rate; dark-hunters never get double aggro on you | A high-collared black coat, fitted under-layers, bone buttons, silver thread, a hood that shadows the eyes. Black, ash grey, silver |
| **Sporeweave** | Fungal caverns | Medium | 40 | Steel plate 10, deepcap 8, Webspinner silk 6, Puffball sacs 4 | Poison and rot buildup ×0.5 | Walk spore clouds unhurt (filter mask) | Living mycelium cloth over leather that glows faintly at the seams, cap-shaped pauldrons, a half-mask. Umber, bioluminescent teal, cream |
| **Voidglass** | Corrupted wasteland | Heavy | 50 | Steel plate 16, voidglass 10, Withergoat hide 6, star iron 2 | Curse buildup ×0.5 | Rift max-HP drain halved (nullward); max HP lost to void abilities −25% | Black glass plates that swallow light, edges that bend the air, grey leather under. Black, grey, a violet edge-glow |

**Hazard gear (GAP-COOKED recipes for the 3.3 "passes with" items).** Hazard gear takes the slot it's worn in, so it costs a slot of real armour or the talisman. That trade is the gear check.

| Item | Slot | Station, level | Inputs | Passes |
|---|---|---|---|---|
| Spiked boots | Legs | BUILT | BUILT | Slick ice |
| Grounding charm | Talisman | Armorer 2 | Stormglass 2, copper 3 | Storm strikes (3.3) |
| Sand veil | Head | Armorer 2 | Linen 4, sunstone 1 | Sandstorm (3.3) |
| Briarproof leathers | Chest | Armorer 2 | Hide 8, heartwood resin 2 | Thornbrakes |
| Sealed waders | Legs | Armorer 2 | Bloodmoss-cured hide 6 (hide + bloodmoss at the tanning rack) | Bleedwater |
| Filter mask | Head | Alchemy 3 | Deepcap 2, fiber 6, charcoal 2 | Spore clouds |
| Ward charm | Talisman | Alchemy 3 | Resonant crystal 3, copper 2 | Ward fields |
| Diving helm | Head | Armorer 3 | Tidecoral 4, copper 6, glass 1 | Crushing depth to 40 m, breath ×2 |
| Anchor boots | Legs | Armorer 3 | Iron 6, hide 4, levistone 1 | Gusts |
| Glider | Back (no slot; one at a time) | Workbench 3 | Levistone 4, Puffwing feathers 10, linen 6, bronze 4 | Falls (glide while held) |
| Nullward | Talisman | Armorer 4 | Voidglass 4, steel plate 2 | Rift drain halved |

**Rings (SPEC "rings", DD Gear; numbers GAP-COOKED; R2).** Forge, a band of any metal + one gem. Effect 60% of the gem's socket value; metal tier adds 10% per tier above copper. Two ring slots; two of the same ring don't stack.

**Talismans from trophies (SPEC "talismans", DD Gear; GAP-COOKED use of the trophies).** Every mini-boss trophy can be mounted (BUILT trophy mount, comfort) **or** made into its talisman at the armorer (L2+; trophy + 2 of the biome's signature resource; the trophy is used up). One talisman slot.

| Trophy (mini) | Talisman effect |
|---|---|
| Obsidian heart (Obsidian Warden) | +10 poise while guarding |
| Wisp crown (Ignessa) | Spells that hit ignite 5% more burn |
| Cinder horn (Cinderhorn) | Sprint attacks +15% stance damage |
| Bellwether horn (The Bellwether) | Knockback taken −30% |
| Kite crown (Galvara) | Ranged hits from above +10% |
| Coil heart (Coilback) | Leg i-frames on jumps +0.06 s |
| Leviathan tooth (Ssarrok) | Sand and dunes don't slow you |
| Mirage antler (Mirage Matriarch) | A perfect dodge leaves a 1 s decoy that draws one attack |
| Empress stinger (Glass Empress) | Strike damage +8% vs armoured targets |
| Gnarl heart (Old Gnarl) | Woodcutting yield +15% |
| Queen's sting (Bramblequeen) | Poison buildup on your hits +10% |
| Matron claw (Grey Matron) | Backstab damage +10% |
| Warden antler (The Hornwarden) | Your mount +10% speed |
| Harrier plume (Kyrra) | Glide time +3 s |
| Sentinel eye (Bridge Sentinel) | Fall damage −30% |
| Gulper's pearl (Old Gulper) | Breath +15 s |
| Twin shard (Mirrored Twins) | Reflect 10% of projectile damage taken |
| Warden's tuning fork (Echo Warden) | Your footsteps are 30% quieter |
| Tyrant plate (Geode Tyrant) | Spell damage taken −10% |
| Troll marrow (Grimjaw) | Out of combat heal 0.5 HP a second |
| Stalker fang (The Pale Hunter) | Blizzards don't cut your vision |
| Tusker ivory (Old Tusker) | Hauler tames +15% carry |
| Maw tooth (Mother Maw) | Your hits on swimming targets +15% |
| Hex mask (Hex-Warden) | Curse buildup ×0.7 |
| Bloat sac (The Bloat) | Bleed buildup ×0.7 |
| Barnacle claw (Old Barnacle) | Guard stamina cost −10% |
| Siren scale (Siren Eel) | Your tames can't be turned or scattered |
| Tyrant fin (Reef Tyrant) | Swim speed +15% |
| Bell clapper (Bell-Ringer) | Corruption builds 20% slower |
| Mother's fang (Crypt Mother) | Damage vs swarms (Geodemite, Mycomite, Gnawer, young) +15% |
| Ossuary crown (Ossuary Lord) | Necro raises last 20% longer |
| Gravedigger's spade (The Gravedigger) | Mining and digging speed +15% |
| Mother's cap (Sporemother) | Spore and rot buildup ×0.7 |
| Rot crown (Rot King) | Hits on rotting targets +10% |
| Weaver silk (The Weaver) | Webs and snares hold you 50% shorter |
| Unravelled thread (The Unravelled) | Max HP loss from your own void abilities −15% |
| Rift heart (Mother of Rifts) | Rift zones drain 25% slower |
| Starving maw (The Starving One) | Hunger drains 20% slower |
| Null seal (The Null Warden) | Spell buildup on you (shock, mark, curse) ×0.8 |
| Echo stone (The Echo) | The first hit after a dodge +10% |

### 2.15 Machinery and power (SPEC + numbers; DD Machinery, CF tech levels; R3, R4, R5)

**SPEC:** water wheels (L2), windmills, power, conveyors, sorting, irrigation (L3), drills, turrets, greenhouses (L4), biome machines and production chains (L5); power from water, wind, geothermal lava and soul energy; power runs stations (faster crafting), drills, conveyors, turrets, lights, greenhouses; automation is medium: simple chains, no factory lines (DD).

**Power (numbers GAP-COOKED).** Power is a number (P) on a **network**: sources and machines joined by **drive shafts** (wood, 4 m a piece; Long lines perk 6 m) or touching each other. A network's load can't exceed its supply; if it does, every machine on it runs at supply ÷ load speed. Machines run while a player is within 250 m (creatures and machines only live near players); queues already in a station finish by timestamp while you're away (BUILT queue rule).

| Source | Level | Inputs | Output | Where |
|---|---|---|---|---|
| Water wheel | Workbench 2 | Wood 40, bronze 6 | 10 P | On a river or stream with current |
| Windmill | Workbench 3 | Wood 50, linen 10, iron 6 | 6 P (storm ×2, Storm peaks and Sky islands always ×1.5) | Open ground, nothing taller within 10 m |
| Geothermal turbine | Workbench 4 | Steel plate 12, stone 30 | 25 P | Within 6 m of lava or a vent |
| Soul engine (Graveyard biome machine, DD) | Workbench 5 | Steel plate 10, soulstone 12, bone 20 | 40 P for 1 h per soulstone burned | Anywhere |

| Machine | Level | Load | Does |
|---|---|---|---|
| Powered station (any of the four, + oven, smokehouse) | 3 | 5 P | Crafts ×1.5 faster |
| Sawmill | 3 | 6 P | 10 wood → 15 planks (planks replace wood in building recipes 1:1) |
| Grinder | 3 | 4 P | Grain → flour; bones → bone meal; stone → stone dust |
| Loom | 3 | 3 P | Fiber or flax → linen ×1.5 |
| Smelter | 3 | 8 P | Ore → ingot by itself (forge recipes for ingots only), 6 s each |
| Irrigation pump | 3 | 3 P | Feeds a channel 40 m from any water |
| Conveyor (2 m piece) | 3 | 1 P per 4 pieces | Moves one stack every 2 s from one container or machine to the next (items ride visibly on the belt) |
| Sorting chest | 3 | 2 P | Takes from a conveyor; sends each item to the chest set for it within 20 m |
| Drill | 4 | 8 P | Placed on an ore outcrop or vein node: 1 ore every 60 s into its hopper (40 slots); the node never runs out while drilled, but drills 4× slower than mining by hand at level 1, so players still mine |
| Greenhouse lights | 4 | 4 P | Greenhouse plots grow in the dark |
| Lamps (wired) | 3 | 1 P per 4 | Light without fuel |
| Turrets | 4 / 5 | 6 P | 2.18 |

**Production chains (medium).** A chain is any line of machines joined by conveyors: drill → conveyor → smelter → conveyor → sorting chest is the reference chain. Chief engineer (perk) lets a chain run 6 machines long instead of 4 (counted along the belt).

**The 12 biome machines (L5; RAVEN-PLANNED "biome-themed machines, like a lava forge or a necro soul engine"; each needs that biome's signature resource and must be built in that biome unless stated):**

| Biome | Machine | Does |
|---|---|---|
| Volcanic | **Lava forge** | A forge set into a lava channel: smelts and forges ×2 speed with no fuel; steel without charcoal |
| Storm peaks | **Storm coil** | A copper-crowned mast: 60 P while a thunderstorm is over it, 0 otherwise; charges a stormglass battery (holds 2 h at 20 P) |
| Desert | **Sun mirror array** | 20 P by day, 0 at night; sunstone battery holds 4 h at 10 P |
| Ancient forest | **Grove press** | Turns plant waste and spoiled food into fertiliser ×2 and has a 10% chance per batch to give a rare seed of any biome |
| Sky islands | **Updraft lift** | A cargo and rider platform between two linked points up to 120 m apart (island to island or ground to island), 10 s a trip |
| Crystal caves | **Resonance pylon** | Shares power with any network within 40 m, no shafts (builds anywhere) |
| Frozen tundra | **Cold store** | Every container in its room spoils ×0.05 (builds anywhere, needs 6 P outside the tundra) |
| Swamp | **Blood still** | Makes slime from bloodmoss and bog water by itself (1 per 5 min), the brew base for every alchemist |
| Ocean & reefs | **Tide mill** | 20 P, rising to 30 at high tide; must sit in a tidal channel |
| Graveyard | **Soul engine** | Above (DD) |
| Fungal caverns | **Mycelium vat** | Grows Oyster caps and Lion's mane by itself in the dark (1 a minute), fed compost |
| Corrupted wasteland | **Rift chest** | 60 slots shared between every Rift chest your clan owns, on any claim (builds anywhere) |

### 2.16 Siege weapons, explosives and boats (SPEC + numbers; CF L4; DD Multiplayer; R6 boats, R9 siege)

**Explosives (SPEC "explosives and raid tools"; numbers GAP-COOKED).** Black powder is the real recipe: **sulfur** (Volcanic only) + **saltpetre** (bat guano in band-2 cave roosts, nitre crust in desert canyons) + **charcoal**. So raiding runs on the Volcanic trade.

| Item | Station | Inputs | Damage |
|---|---|---|---|
| Fire pot | Alchemy 2 | Sulfur 1, animal fat 2, clay 1 | Thrown (arc shown, never homes): 150 to wood + 30 a second for 10 s; 20 to stone; 0 to metal and biome; creatures and players 40 + burn 60 |
| Black powder | Alchemy 3 | Sulfur 2, saltpetre 2, charcoal 2 | Ingredient |
| Powder charge | Workbench 3 | Black powder 4, clay 2, fiber 2 | Placed on a piece: 5 s fuse with a hiss and sparks, 4 m blast: wood 800, stone 600, metal 300, biome 250, Hearth 300; players and creatures inside 4 m take 120 (no friendly-fire exceptions outside clans) |
| Mining charge | Workbench 3 | Black powder 2, clay 1 | Clears a cave rockfall or breaks an ore vein for ×3 ore (no damage to pieces) |

**Siege (L4 forge, SPEC; numbers GAP-COOKED).**

| Siege | Inputs | Use | Damage |
|---|---|---|---|
| Battering ram | Steel plate 6, heartwood 10 | Pushed by 2 players or 1 hauler tame; doors and gates only | 250 a hit, every 3 s |
| Catapult | Wood 60, steel plate 6, rope 10 | 2 m boulders, 15 s reload, 20 to 60 m range; the arc shows to everyone for the full flight | 500 a stone to any piece; 80 to anyone it lands on |
| Ballista | Steel plate 8, wood 30 | Bolts at a tame or player, 2 s draw (visible), straight flight, never homes | 220 to creatures, 120 to players |

**Boats (SPEC L4 "boats", R6).** Every boat is a building on water: it holds a storage chest and up to 2 build pieces (a bed, a station).

| Boat | Inputs | Crew / riders | Speed (sail / oars) | Storage | HP |
|---|---|---|---|---|---|
| Skiff | Planks 40, linen 10, steel plate 2 | 2 | 1100 / 600 cm/s | 12 slots | 1,200 |
| Longboat | Planks 120, linen 30, steel plate 8, tidecoral 6 | 6 | 1300 / 700 | 40 slots | 4,000 |

Sailing speed follows the wind relative to the sail (a wind arrow on the HUD at sea). Tidecoral fittings (workbench 4) add 25% HP and stop barnacle drag. A grounding charm fixed to the mast (one item) stops squall strikes on that boat. A boat sinks at 0 HP; its chest floats up as a bag.

### 2.17 Building (SPEC + numbers; BL; DR #28, #29, #30, #33; R2, R4, R5)

**SPEC (kept):** snap for structure, freeform for utility and decor (DR #28); wood and stone at Hearth 1 (DR #33); span wood 4, stone 6, metal and biome 8 (BL); 45 BUILT pieces; claim totem 30 m +10 m per upgrade to 60 m, unlimited claims, one Hearth each; upkeep in the totem by piece count; out of upkeep the claim drops and decays 10% an hour; doors with code locks; creature raids; offline protection; no spikes, traps, underground bases or NPC guards (BL, DD); grid 4 m, storey 3 m (`pieces.json`).

**Material tiers (numbers GAP-COOKED for metal and biome).**

| Tier | Hearth | Wall HP | Foundation HP | Span | Wall cost | Upkeep per piece per hour (BUILT units) | Notes |
|---|---|---|---|---|---|---|---|
| Wood | 1 | 400 | 600 | 4 | Wood 12 | Wood 2 | BUILT |
| Stone | 1 | 1,000 | 1,500 | 6 | Stone 20 | Stone 2 | BUILT |
| Metal | 4 | 2,500 | 3,500 | 8 | Steel plate 8, stone 10 | Steel plate 0.1 | Brushed dark steel panels with rivet lines on a stone footing |
| Biome | 5 | 3,000 | 4,200 | 8 | Biome block 20 | Biome block 0.1 | The biome's themed look; same HP for every biome (BL "high, with a themed look") |

Every metal and biome piece mirrors the built shape list (foundation, wall, half wall, doorway, door, window, floor, stairs, roof, gable, upper wall, pillar), so the build bar gains a Metal page and one page per biome set. **Biome block:** workbench L5, stone 4 + 1 signature resource + 1 steel plate → 4 blocks.

**Freeform structural pieces and structural integrity (RAVEN-PLANNED, BP Full release; R2).**
- New freeform pieces in each tier: **beam** (2 m and 4 m), **post** (3 m), **brace** (45° diagonal). They place at any angle and snap only at their ends to any piece's surface.
- Integrity keeps Raven's span rule (support = pieces from the ground), and freeform pieces carry it: a beam or post adds 1 span per 2 m of length; a brace touching a wall and a pillar gives the wall +1 span.
- **Stability colours in build mode** (Valheim): every piece under the hammer tints by remaining span: blue (on the ground), green, yellow, orange, red (one step from failing). The ghost of a piece you're placing shows the colour it would be.
- **Collapse:** an unsupported piece breaks into 4 to 8 rubble chunks with dust and a stone-or-timber crack sound, chunks fall for 3 s and deal 50 damage to anyone they land on, then vanish. A collapse checks the pieces it held (BUILT `RecomputeSupport`).

**Biome building sets (RAVEN-PLANNED, BL; Hearth 5; R5 then each biome's round).** Each set = the 12 structure shapes in biome blocks + 6 decor pieces (comfort group "biome set", 2) + 1 biome rug. Each set's decor blueprint drops from that biome's major boss (2.13).

| Biome | Set | Structure look | Its 6 decor pieces |
|---|---|---|---|
| Volcanic | **Basalt hall** | Hexagonal basalt columns, obsidian lintels, ember-glass windows | Obsidian throne, forge-hearth fireplace, lava-glass lamp, anvil table, ash-wood bench, hot spring bath |
| Storm peaks | **Crag hold** | Slate and granite with copper flashing that goes green, storm shutters | Copper lightning-rod lamp, fur-draped bench, slate table, wind chime, horn rack, storm drum |
| Desert | **Sun court** | Pale sandstone, carved arches, shaded colonnades, sunstone inlay | Tiled fountain, cushioned divan, brass lantern, low table, rug loom, sun-dial |
| Ancient forest | **Heartwood lodge** | Carved heartwood frames, living-moss roofs, amber windows | Root throne, carved long table, hanging herb rack, lantern tree, harp, flower trough |
| Sky islands | **Cloud spire** | White stone, blue tile, open arches, levistone keystones that hover | Floating lamp, wind harp, feather bed, sky map table, open-air bath, kite rack |
| Crystal caves | **Geode sanctum** | Violet crystal set in dark stone, faceted windows, humming pillars | Crystal lamp, rune-carved desk, crystal throne, chime tree, star globe, reading alcove |
| Frozen tundra | **Rime lodge** | Pine timber over stone, carved prows, packed-snow roofs, frost-glass | Fur bed, antler chandelier, carved bench, ice-clear window seat, wolf-pelt rug, hot spring bath |
| Swamp | **Stilt house** | Black mangrove stilts, reed thatch, red-lacquered bone details | Witch's cauldron hearth, bone chimes, moss bed, lantern-toad lamp, carved stool, reliquary shelf |
| Ocean & reefs | **Coral keep** | Coral-grown stone, shell tile, round windows, sea-glass | Shell basin, net hammock, pearl lamp, ship's wheel table, aquarium, coral throne |
| Graveyard | **Mourning chapel** | Grey stone, pointed arches, iron fences, stained glass in bruise colours | Candle rack, carved pew, lectern, ossuary shelf, bell, velvet chaise |
| Fungal caverns | **Mycelium den** | Shelf-fungus floors, cap roofs, glowing gills | Glowcap lamp, spore-silk hammock, stump table, puffball seats, moss bath, lantern grub jar |
| Corrupted wasteland | **Hollow bastion** | Black voidglass and grey stone, edges that bend the air | Null throne, floating rubble table, rift lamp, black mirror, broken crown stand, silence bell |

**New pieces in this cook (all GAP-COOKED numbers; utility and decor place freely, DR #28).**

| Piece | Level | Cost | HP | Kind |
|---|---|---|---|---|
| Campfire | Hand | Stone 5, wood 4 | 100 | station (cooking) |
| Lean-to | Hand | Wood 12, fiber 10 | 200 | structure (a roof for Rested and weather) |
| Salt pan | Hand | Stone 10 | 150 | producer |
| Compost bin | 1 | Wood 12 | 200 | producer |
| Drying rack | 1 | Wood 6, fiber 4 | 120 | station |
| Cooking pot | 1 | Copper ingot 4, stone 6 | 300 | station |
| Log bed (mushrooms) | 1 | Wood 10 | 150 | crop (dark) |
| Feed trough | 1 | Wood 10 | 150 | container (pens, tames) |
| Well | 2 | Stone 30, wood 10, copper 2 | 800 | water source (drink, irrigation) |
| Oven | 2 | Stone 30, copper 4 | 900 | station |
| Smokehouse | 2 | Wood 30, stone 10 | 600 | station |
| Salting barrel | 2 | Wood 12, copper 2 | 200 | station |
| Charcoal kiln | 2 | Stone 20, clay 10 | 800 | station |
| Tanning rack | 2 | Wood 10, hide 2 | 200 | station (cured hides) |
| Beehive | 2 | Wood 20, Hiveback wax 4 | 300 | producer (Hiveback home) |
| Fish trap | 2 | Wood 10, fiber 10 | 150 | producer (in water) |
| Kelp line | 2 | Rope 6, wood 4 | 100 | crop (in sea) |
| Water wheel | 2 | Wood 40, bronze 6 | 1,000 | power |
| Irrigation channel (4 m) | 3 | Stone 6 | 600 | water carrier |
| Ice box | 3 | Stone 20, iron 4, hide 4 | 700 | container |
| Sea pen | 3 | Wood 40, rope 10 | 800 | pen (in sea) |
| Incubator | 3 | Stone 20, iron 4, linen 4 | 600 | breeding (2.21) |
| Stable | 3 | Wood 60, stone 20 | 1,500 | stores tames (2.19) |
| Windmill, sawmill, grinder, loom, smelter, pump, conveyor, sorting chest, drive shaft, lamps | 3 | 2.15 | 400 to 1,200 | machines |
| Bone altar (Necromancy only) | 3 | Stone 30, bone 30, soulstone 4 | 1,200 | ritual (2.22) |
| Greenhouse wall, roof, door | 4 | Glass pane 4, steel plate 1 each | 500 | structure (glass) |
| Geothermal turbine, drill, greenhouse lights | 4 | 2.15 | 1,500 | machines |
| Bolt turret | 4 | Steel plate 8, crossbow 1, bronze 4 | 1,500 | defence (2.18) |
| Element turret | 5 | Steel plate 10, the path material 4, biome block 6 | 2,000 | defence (2.18) |
| Biome machines | 5 | 2.15 | 2,500 | machines |
| Boss monument | Any (after the kill) | Stone 40 + the major boss's trophy head (new drop, 100% first kill) | 3,000 | decor (2.18) |
| Decor added by 2.2 | 1 to 5 | Chair 4 wood … throne 20 heartwood; tapestry linen 8 dye 2; hot spring bath stone 30 (vent only); wooden tub wood 20; hand drum hide 2 wood 4; lyre wood 4 gut 4; potted plant clay 4 + any seed | 50 to 600 | decor |

Rope (new item): workbench L1, 6 fiber → 1 rope. Clay: dug from riverbanks and swamp pools (a new gather node).

**Rules kept:** no piece places inside caves or under terrain (no underground bases, DD); nothing damages a player for touching it (no traps, BL).

### 2.18 Raids, defence, turrets and monuments

**Creature raids (BUILT, `WRRaids.cpp`):** Off / Rare (72 h) / Normal (24 h) / Frequent (12 h) game time, 60 s warning, raiders from the claim's region, points scale with Hearth level, raids only when someone of the claim is within 250 m. **SPEC.** Per-biome raider lists for the 10 new biomes (GAP-COOKED, `spawn_regions.json` `raiders`): each biome's aggressive fighters of the claim's band plus, from Hearth 3, one of its roaming minis in 1 raid in 5.

**Player raid costs (GAP-COOKED, R9; the explosive numbers above).** What one wall costs to breach:

| Wall | Powder charges | Fire pots | Catapult stones | Ram hits (doors only) |
|---|---|---|---|---|
| Wood (400) | 1 | 3 | 1 | Wood door (350): 2 |
| Stone (1,000) | 2 | can't | 2 | Reinforced door (1,400): 6 |
| Metal (2,500) | 9 | can't | 5 | Metal door (2,800): 12 |
| Biome (3,000) | 12 | can't | 6 | Biome door (3,200): 13 |

Weapons and tools on pieces: wood takes 10% of a hit, stone 2%, metal and biome 0%. The offline protection setting reduces all of it (BUILT, 0 to 100%).

**Path raiding (RAVEN-PLANNED "fire burns wooden walls, necromancers raise the dead inside"; numbers GAP-COOKED; R9).** Path damage on pieces: Fire ×2 vs wood (spec); Lightning ×1.5 vs metal (it conducts); Spore rot ×1.5 vs wood over time; Void ×1.25 vs everything; every other path ×1 (weapon rules above). **Necromancy:** a raise can target any corpse or bone pile within 12 m even through walls, at 3× its Soul cost (spec "raise the dead inside").

**Tames in raids (RAVEN-PLANNED, DD; R9):** a tame ordered to attack a piece bites it at its bite damage × 0.5 (the built raider `pieceDamageMult` rule). Haulers push rams.

**Defence (SPEC: towers, palisades, gates, reinforced doors BUILT; powered turrets RAVEN-PLANNED L4).**

| Turret | Power | Ammo | Range | Behaviour |
|---|---|---|---|---|
| Bolt turret | 6 P | Crossbow bolts from its 50-slot box | 30 m | Targets hostile creatures; on PvP servers, any non-clan player in the claim. A 0.9 s red aim line before every shot (our ≥0.9 s warning rule), bolts fly straight at 4,000 cm/s and never home. 45 damage, one shot every 1.5 s. All damage through `ApplyHit` |
| Element turret | 8 P | A path material per 50 shots | 20 m | Fire: a 6 m cone, 30 damage + burn 20 buildup. Frost: a slowing bolt. The 12 paths' versions use their status (shock, soak, blind, decay, mark, curse, bleed, poison...). Same 0.9 s tell |

Turrets are buildings, not NPC guards: they never move, they need power and ammo, and tames do the real guarding (DD).

**Boss monuments (RAVEN-PLANNED "boss trophies as base monuments", DD Endgame; R10).** A major boss drops its **trophy head** on each player's first kill. On a stone plinth it becomes a monument: comfort 3, a bronze plaque with the killers' names and the kill time (from the leaderboard), and the claim's map icon shows the boss's sigil to every player.

### 2.19 Taming (SPEC + numbers; R2, then per round)

**SPEC (all BUILT):** knockout and feed (torpor, tranq arrows and darts, narcotic, kibble), offer food, steal an egg, path magic; each creature only by its listed method (DR #16); taming time easy 1 / medium 5 / hard 15 min, up to 50% faster at Taming 100; orders by wheel; ground mounts; haulers; utility tames (cave light, ore digging, secret sensing); tame limit 2 at start, +1 Bonded, +1 Beast lord, stored tames a higher cap; tames fight and count +80% boss HP each; permadeath (`tuning.json` `taming`; M4; SN). **Whistle wheel:** SPEC as cooked in `cooks/ui-controls.md` 2.13 (hold E or X at nothing: orders to every tame within 30 m). Tames keep their biome element (DD).

**Stored tames (numbers GAP-COOKED):** a **stable** (workbench L3) holds tames out of the world: 10 per player per stable, 3 stables per claim. Stored tames don't eat or age. Calling one out takes 5 s at the stable.

**Feeding (GAP-COOKED).** An active tame has hunger 0 to 100 that empties in 3 h (small), 4 h (medium), 6 h (large). It eats its listed food (`creatures.json` `food`) from its own inventory or a trough within 20 m. Empty: no stamina regen, then −1% max HP a minute. A starving tame can die. Tames in an unloaded part of the world don't tick (BUILT wildlife rule), so a tame left at home while you're away never starves.

**Creature levels (RAVEN-PLANNED "creature levels"; numbers GAP-COOKED; R2).**
- **Wild level** 1 to 30 by band: lowland 1 to 10, midland 11 to 20, highland 21 to 30. Each wild level is one random stat point (Ark style). It changes the creature's stats by at most ±10% around its tier numbers (3.3), so combat tuning holds.
- After taming, **bond levels** 1 to 20 come from the tame's own use. Each bond level gives +2% to every stat and 1 **skill point** (20 in all).
- Taming effectiveness (BUILT) still adds its built bonus.

| Tame XP from | Amount |
|---|---|
| Fighting | 1 per 10 damage dealt; a kill it helped with gives its full kill XP |
| Riding | 1 per 50 m ridden |
| Hauling | 1 per 100 m travelled carrying at least 25% of its capacity |
| Utility work | 3 per ore dug, secret found or nest sensed; 1 per 5 min lighting a dark place |
| Farm work | 2 per product collected |
| Bond level n costs | 100 + 40n tame XP (about 10,400 to reach 20, roughly 15 to 20 hours of use) |

**Role skill trees (GAP-COOKED; FULL-GAME's "one 20-point tree per role").** Each tame gets its role's tree (`creatures.json` `role`; a "fighter, scout" uses Fighter). Four nodes in a line (2 + 3 + 3 + 4 = 12 points), then **one of two capstones** (8 points). Flying and swimming mounts use the Mount tree in their medium.

| Tree | 2 | 3 | 3 | 4 | Capstone A (8) | Capstone B (8) |
|---|---|---|---|---|---|---|
| Fighter | Hardened: +15% HP | Bite force: +15% damage | Pack tactics: +10% damage per other ally hitting the same target (max 3) | Guardian: within 4 m of its owner, takes 30% of the damage meant for them | Berserk: below 30% HP, +40% damage | Stalwart: stance damage taken −40%, can't be knocked down |
| Mount | Sure-footed: stamina −15% | Stride: +8% speed | Charge: sprinting into an enemy knocks it back for 40 damage (an 0.4 s lowered-head tell) | Steady seat: the rider isn't knocked off by hits under 80 stance damage | Warhorse: mounted attacks +20% | Courser: +15% speed, +50% stamina regen |
| Hauler | Broad back: +25% carry | Packing: +8 slots | Steady: no slowdown when loaded | Forager: picks up drops within 8 m | Caravan: +50% carry | Bulwark: +200 HP, allies within 6 m take 10% less damage |
| Utility | Keen sense: sensing range +50% | Tireless: ability cooldown −25% | Bright: light radius +50% or digging yield +25% (by species) | Lucky find: +15% gems and rare finds | Seeker: what it senses is marked on the map for your clan for 5 min | Lodestar: gathering speed +15% for players within 10 m |
| Farm | Content: eats 25% less | Bountiful: +20% product | Fertile: breeds ×1.5 as fast | Calm herd: pen-mates +10% product | Prize stock: 5% of products are rare (golden egg, fine fleece, prize milk) | Prolific: 25% twins |

### 2.20 Saddles, barding and mounts (SPEC + numbers; DD "craftable saddles and creature armor", "flying, swimming, pack and hauling mounts"; R2, R6, R7)

**Saddles (workbench; L2 in CF).** One saddle per body rig, not per species (the 5 built rigs from M4 plus a swimmer and a flyer rig), fitted per species by socket offsets. Riding without a saddle stays as built, but bareback the rider can't attack and the mount takes +25% damage.

| Saddle | Level | Inputs | Mount damage reduction | Extra |
|---|---|---|---|---|
| Hide saddle | 2 | Hide 10, fiber 10, wood 4 | 10% | Mounted attacks unlocked |
| Bronze-fitted saddle | 2 | Hide 8, bronze 4, linen 4 | 20% | |
| Iron saddle | 3 | Hide 8, iron 6, linen 6 | 30% | |
| Steel saddle | 4 | Leather 8, steel plate 4, linen 6 | 40% | |
| Pack saddle (haulers) | 2 | Hide 10, wood 10, rope 4 | 10% | +10 slots |

Saddle master (perk): saddles +25% reduction value. Quality rolls apply like gear.

**Barding (armorer L3 iron 20%, L4 steel 30%; L5 biome barding = steel 30% + that biome's 4-piece hazard pass for the mount**, so Forgeplate barding carries any mount across lava, Tidecoral barding lets it dive to 40 m). Barding weighs the mount: −5% speed. Creature armorer perk: +30% barding value.

**Mount numbers (GAP-COOKED):**

| Kind | Creatures (as Raven and FULL-GAME list them) | Numbers |
|---|---|---|
| Ground mounts | Ashstrider (fastest), Emberhorn, Frostantler (BUILT); Cragstrider (climbs 60° rock), Shimmerbuck, Elderelk, Lumenhorn, Mirestrider, Palestrider (never tires: no sprint stamina), Tunnel toad (leaps 12 m gaps), Voidstrider (5 m blink every 8 s) | Ground mount sprint 1,200 to 1,500 cm/s; stamina 200 |
| Flying mount | **Windmane** (Sky islands, steal an egg, hard): the first flying mount | Flight 1,600 cm/s; stamina 300; climbing 15/s, level 4/s, gliding 0; landed regen 30/s; out of stamina it glides down. Ceiling 400 m above the ground. Forced landing within 150 m of any boss arena (bosses are fought on foot or on ground mounts). Storm strikes target flyers first |
| Floating hauler | **Cloudback** (Sky, offer levistone moss) | Floats at 2 m, crosses gaps to 60 m, 900 cm/s, carries 250 kg |
| Swimming mounts | **Floeback** (tundra shore, offer fish; DR #14 Full release), **Wavecharger** (ocean, steal an egg) | Floeback: surface only, 900 cm/s, can haul out onto ice. Wavecharger: 1,100 cm/s underwater, dives to 40 m; the rider uses their own breath (Gills, diving helm, Deepbreath) |
| Swimming hauler | **Driftback** (ocean, offer kelp) | Surface, 600 cm/s, 300 kg |
| Haulers (pack) | Basaltback, Woolly drudge (BUILT); Galeback, Dromad, Burlbeast, Mudhulk, Tombback, Shroomback (its back garden grows 1 Oyster cap per 10 min), Riftback (its pack is a pocket of space: it weighs nothing to the beast) | Carry: Basaltback 300, Woolly drudge 280, Dromad 260 (no sand slow), Burlbeast 300, Mudhulk 260 (no water slow), Galeback 240 (no gust push), Tombback 240, Shroomback 220, Riftback 200 |

**Mounted combat** is the combat lane's (Elden Ring Torrent rules, `cooks/combat.md`). This cook only sets the saddle gate.

### 2.21 Breeding (SPEC + numbers; DD "Ark-style eggs or pregnancy and raising babies", "mutations"; R8)

| Step | Rule |
|---|---|
| Sex | Every tame is male or female (50/50 at taming or hatching), shown on its card |
| Mating | Two tames of one species, opposite sexes, both with mating on, within 10 m and fed: a mating bar fills over 2 min (they stay close; real courtship idle clips). Then the female can't mate again for 4 h (Fertile: ×1.5 faster) |
| Eggs or birth | **Eggs** for reptiles, birds, insects, fish and fungal creatures; **pregnancy** for mammals (`creatures.json` `birth: egg | live`). Pregnancy: easy 30 min, medium 2 h, hard 6 h, then a live birth at the mother |
| Incubation | An **incubator** (workbench L3) feeds the egg its element's fuel: fire and sun eggs burn charcoal or sulfur, frost and wind eggs need an ice block per hour, tide and blood eggs need a water basin, every other element burns wood. Time as pregnancy. A fuel-less incubator pauses (eggs don't die). Eggs from **steal an egg** taming use the same incubator (built egg items) |
| Baby | Grows from 10% to full size: easy 2 h, medium 8 h, hard 24 h (the server's maturation setting scales it). A baby eats from a trough and stays near it; it can die like any tame |
| Imprint | Every 1/8 of growth the baby asks for care with a bubble icon: **feed** a named food, **walk** it 50 m, or **cuddle** (E). Each answered prompt +12.5% imprint. Missing one only loses that 12.5%. Imprint gives +20% to all stats when the imprinter rides or fights beside it, and the baby is bonded to that player (it counts against their limit) |
| Stats | Each of the 6 stats (health, stamina, damage, carry, speed, torpor) comes from one parent: 55% the higher, 45% the lower |
| Mutations | 3 rolls per birth, 2.5% each (Breeder perk ×1.5, Rare finder makes wild mutants appear 2× as often). A mutation adds +2 points to a random stat and changes one colour region to a new pattern (a real visible change: banding, a glow line, a pale or dark morph). Each parent's line counts its mutations; a parent with 20 or more on its side can't pass new ones from that side |
| Hybrids | Cross-biome hybrids stay in the Dream tier (BP); two species never mate today |

### 2.22 Necro raising, revive, and Dream items

**Necro raising (RAVEN-PLANNED, DD; R4).** Quick combat raises with Souls belong to the Necromancy tree (path lane) and don't count against the limit (DD). **Permanent undead tames by ritual (numbers GAP-COOKED):** a Necromancer (main or subclass) brings a creature's **remains** (its body within 3 min of death, or a **remains** item a butchered carcass leaves) to a **bone altar**: Souls 10 / 25 / 60 by the creature's taming difficulty, soulstone 3, Asphodel 2, a 60 s channel (a real clip; the body knits with bone and grave-light). Any creature can be raised whatever its taming method (CR rule). The undead tame keeps its role and tree, takes the necrotic element instead of its own (it was a living thing; now it's the dead), eats no food but needs 1 soulstone dust per real day, can't breed, can't be revived, and counts against the tame limit. Look: the same body with exposed bone, dull eyes lit pale green, torn hide; never a zombie (DD rule: bone, shade and carrion).

**Revive (RAVEN-PLANNED "a rare item can revive them", M4; numbers GAP-COOKED; R2).** A dying tame drops its **bond token** (its collar, named). The **Dadeni draught** (alchemy L5, 2.12) poured on the token at a Hearth within 72 h of the death brings the tame back at full bond level beside the Hearth. Once per tame. **Soulbond** (perk) makes one chosen tame unable to die for good (it falls unconscious at 0 for 60 s instead; still not an HP floor on anyone else).

**Dream tier (kept off, BP):** tamable mini-bosses, bases on large tames, cross-biome hybrids. Beast lord's "easier mini-boss taming" half stays hidden (2.23).

### 2.23 Life skills (SPEC + numbers; LS; hidden-perk rule GAP-COOKED)

**SPEC (kept):** 15 skills, 1 to 100 by doing, a small boost per level, a choice of two perks at 25 / 50 / 75 / 100, no cap, no decay; level n costs 50 + 5n XP (about 50 h each); every perk name in LS; `Data/lifeskills.json`, `WRLifeSkills.cpp`; built XP keys `mine 4, chop 4, plant 10, harvest 20, collect 5, craftGear 25, craft 10, cook 15, upgrade 15, place 8, feed 5, tame 100, kill 10, sneak 1`.

**XP sources (new ones GAP-COOKED).** All scale by the server's XP multiplier (BUILT) and Rested (+10%).

| Skill | Built sources | New sources (this cook) | Round |
|---|---|---|---|
| Mining | Ore and stone | Drill hoppers emptied: 1 per 10 ore; mining charges: 10 | R4 |
| Woodcutting | Trees | Heartwood from fallen giants: 8 a hit | R3 |
| **Fishing** | None today | Catch common 20, rare 60, legendary 500 (first) / 100; fish trap 2 per fish; spear catch as the fish | R6 |
| Tracking | Kills | Following tracks 1 per 20 m; marking a creature 5 | R1 |
| Smithing | Weapons and upgrades | Salvage 5; steel and starsteel ingots 5 each | R2 |
| Armorcraft | Armour | Saddles and barding 25 (as gear); set pieces 40 | R2 |
| Alchemy | Potions | Brews 20, elixirs 60, resist potions 15 | R3 |
| Cooking | Food recipes | First discovery of a dish 100; each pot dish 15 (built key); smoking, salting, drying a batch 10 | R3 |
| Farming | Plant, harvest | Fertilising 3; first harvest of each crop 50 | R3 |
| Husbandry | Collecting | Beehive collections 5; pen births 10 | R3 |
| Building | Placing, repairing | Freeform structural pieces 8; metal and biome pieces 12 | R2 |
| **Engineering** | None today | Placing a machine 30; each machine you own that runs: 1 per 5 min of running (up to 10 machines counted); repairing a machine 10; crafting machine parts 15 | R3 |
| Taming | Taming | A tame's bond level-up 20; a birth or hatch 40; each imprint prompt answered 10; an undead ritual 60 | R2 / R4 / R8 |
| **Swimming** | None today | 1 per 10 m swum, 2 per 10 m underwater, 5 per dive below 10 m. Only while moving faster than 200 cm/s; at most 600 XP an hour (no idling in a current) | **R0** (swimming exists now; FRP) |
| Sneaking | Sneaking | Unseen backstab 10 | R0 |

**Every-level boosts that had no stat (numbers GAP-COOKED):**

| Skill | Every level | At 100 |
|---|---|---|
| Fishing | Bite wait −0.5% | −50% |
| Engineering | Machine speed +0.3% | +30% |
| Swimming | Swim speed +0.3%, breath +0.6 s | +30%, +60 s |
| Cooking | Sated time +1% | +100% |
| Husbandry | Production interval −0.3% | −30% |
| Farming | Growth time −0.3% | −30% (BUILT stat `cropSpeed` already) |
| Taming | Taming time −0.5% | −50% (SN) |

**Perk values for this scope (GAP-COOKED; names and effects are Raven's).**

| Skill | Perk | Value |
|---|---|---|
| Fishing | Patient angler / Net caster | Rare fish ×1.5 often / 15% chance of two fish |
| Fishing | Deep line / Bait maker | Iron rods catch the deep-water list (bluefin, ice cod, etc.) / biome baits at the workbench |
| Fishing | Treasure hook / Spear fisher | 5% of catches are treasure / spearfishing underwater |
| Fishing | Legend catch / Monster line | Legendaries can bite / hook swimming creatures to tame (2.9) |
| Cooking | Recipe sense / Bulk cook | Group-count hints / 4 at once |
| Cooking | Preserver / Shared meal | Spoilage ×0.7 / party within 8 m gets 50% |
| Cooking | Camp chef / Hearty | Every recipe at a campfire / hunger ×0.8 |
| Cooking | Master chef / Banquet | Thirst = half the hunger / clan feast, sated 45 min (2.10) |
| Farming | Green fingers / Seed keeper | Yield +25% / seeds +50% |
| Farming | Grafting / Rare seeds | Off-biome at 50% yield / 3% of harvests give a rare seed of that biome |
| Farming | Water wise / Companion planting | Irrigated ×0.5 time / plots within 8 m of a different crop grow ×0.9 time |
| Farming | Golden crop / Endless field | 2% golden crop (sells high to merchants, a rare cooking ingredient: +50% hunger in any dish) / harvested plots replant themselves from the harvest's seeds |
| Husbandry | Gentle hand / Large pens | +25% milk, eggs, wool / +2 per pen |
| Husbandry | Bee whisperer / Shearer | Honey and wax +50% / hide and wool +50% |
| Husbandry | Selective breeding / Content herd | Pen young +10% product / 50% chance a cycle eats no feed |
| Husbandry | Prize stock / Royal jelly | 5% rare products / 5% royal jelly from hives |
| Building | Thrifty / Quick repair | Pieces −15% cost / repairs ×2 speed |
| Building | Load bearing / Fortifier | Span +1 every tier / walls +15% HP |
| Building | Upkeep saver / Decorator | Upkeep −20% / comfort +3 (cap 23) |
| Building | Master builder / Siege-proof | Biome set pieces −25% cost / walls take −20% explosive damage |
| Engineering | Efficient power / Tinkerer | Machines use 20% less power / machines −20% cost |
| Engineering | Long lines / Turret tech | Shafts and conveyors 6 m / turrets +20% damage |
| Engineering | Overclock / Self-repair | A machine set to overclock runs ×1.5 at ×2 power / machines lose no durability |
| Engineering | Chief engineer / Siege engineer | Chains 6 long / siege +25% damage |
| Taming | Calm presence / Strong knockout | Wild creatures notice you 25% later / torpor +25% |
| Taming | Bonded / Saddle master | +1 active tame (BUILT) / saddles +25% |
| Taming | Breeder / Pack leader | Mutation odds ×1.5 / tames +10% damage within 15 m of you |
| Taming | Beast lord / Soulbond | +1 active tame (the mini-boss half hidden: Dream) / one tame can't die for good |
| Swimming | Diver / Strong stroke | Breath ×1.5 / swim stamina −30% |
| Swimming | Pressure / Water fighter | Safe to 40 m / underwater attacks lose no speed and damage |
| Swimming | Current rider / Salvager | Currents ×1.5 speed / sunken loot caches show within 30 m |
| Swimming | Seaborn / Abyss diver | No swim stamina / no depth limit |

**Hidden until their system ships (GAP-COOKED rule, FRP 0.3; R0 build).** A perk whose system isn't in the build shows as a locked card with "Comes with fishing" (or its system's name) and can't be picked; the other perk at that level can. Each `lifeskills.json` perk gets `requires: "<system>"`; `tuning.json` `systems` lists which are on.

| Perk(s) | Needs | Unhides in |
|---|---|---|
| Every Fishing perk; Treasure hook, Spear fisher, Legend catch, Monster line | `fishing` | R6 |
| Every Engineering perk (Turret tech needs `turrets`, Siege engineer `siege`) | `machinery` | R3 / R4 / R9 |
| Bee whisperer, Royal jelly | `beekeeping` | R3 |
| Grafting, Water wise, Companion planting, Rare seeds, Golden crop, Endless field | `farming2` | R3 |
| Recipe sense, Preserver, Camp chef, Banquet, Master chef | `cooking2` | R3 |
| Breeder, Rare finder, Selective breeding | `breeding` | R8 |
| Decorator | `comfort` | R4 |
| Saddle master, Creature armorer | `saddles` | R2 |
| Salvager, Abyss diver, Pressure, Water fighter | `underwater` | R6 |
| Beast lord's mini-boss half | Dream | never in Full release (text shows only "+1 active tame") |

### 2.24 Survival POIs and dressing per biome (GAP-COOKED; feeds the 150 to 250 m POI rule)

Each biome gets 3 survival set-pieces that teach its systems through the place alone (no NPCs explaining, no staged scenes, DD World). They count toward the 3.3 POI density.

| Biome | Set-piece 1 | Set-piece 2 | Set-piece 3 |
|---|---|---|---|
| Volcanic | **Ash-pit vineyard**: a ruined terrace of La Geria-style pits, wild Ember grapes still growing, a cracked wine jar | **The vent bathhouse**: a collapsed stone bath on a hot vent, a skeleton still reclining in it, bath tiles to scavenge | **Sulfur miners' camp**: a powder store blown open from inside, burn marks outward, saltpetre sacks, a mining charge recipe scratched on a crate |
| Storm peaks | **Ruined storm mill**: a windmill with its sails torn, a still-standing copper rod, a melted lantern | **The rye terraces**: walled high fields, Stormrye flattened by gusts and standing back up | **Goatherd's cave**: Puffram pen, cold fire, gentian drying on strings |
| Desert | **Caravan oasis**: date palms, a stone well, a broken salt-caravan with its guards' weapons pointing outward | **Nitre canyon**: white crusts on red walls, a scraper left in a crust, bat roost above | **Sunken greenhouse**: a glass dome buried to its shoulders, dead off-biome plants inside, one living |
| Ancient forest | **Abandoned farmstead**: oat field gone wild, a beehive tree, an orchard with one Iðunn apple tree, the farmer's tools hung neatly | **The mill race**: a water wheel ruin on a stream, gears to salvage | **Truffle glade**: rooted-up ground, a pig skull, a basket of rotten truffles |
| Sky islands | **Cloud garden**: terraced flax and cloudberry beds on a floating rock, a gardener's glider snapped in a tree | **Feather rookery**: Puffwing nests on a cliff, a collapsed plucking hut | **Lift station**: a broken updraft lift platform and its twin on the next island |
| Crystal caves | **Forcing shed**: a dark stone shed in a cave mouth, candle rhubarb grown pale and tall, candles burned to stubs | **Glass lake**: an underground lake, a cavefish trap still baited | **Rune smithy**: a crystal anvil, a half-carved rune on it |
| Frozen tundra | **Ice-fishing camp**: holes in a frozen lake, a sled, frozen char on a line | **The ice house**: blocks cut from a glacier face, a sawn block still in the cut | **Hot spring hollow**: steam, a turnip patch kept alive by the warm ground |
| Swamp | **Stilt village**: rice paddies gone to reeds, a still dripping slime, a hex-marked door | **The leech pond**: a bleedwater pool with a pair of sealed waders hung on a post | **Clay pit**: wet clay banks, a potter's wheel, fired pots |
| Ocean & reefs | **Salt works**: salt pans on a beach, a sea-pen of rotted posts, kelp lines in the shallows | **Fishing wreck**: a beached longboat, its catch dried to bone, a coelacanth sketch nailed to the mast | **Pearl beds**: a diver's helm on the sand at 8 m, a basket of shells |
| Graveyard | **Ergot field**: a black rye field with a crooked scarecrow and a burned cottage | **Ossuary kitchen**: a crypt-keeper's hearth with mourner's bread gone to stone | **Asphodel meadow**: pale lilies over a grave field, a bone altar half-built |
| Fungal caverns | **Mushroom farm**: log beds in rows, a Capling pen, a spore-mask on a hook | **Flooded grotto weir**: a fish weir with glowfin in it | **The vat room**: a cracked mycelium vat, its pipes still pulsing |
| Corrupted wasteland | **Grey farm**: an ashen grain field, a goat pen, a family's chairs facing a rift | **Mandrake ring**: a circle of mandrakes, a dog's leash tied to one of them, no dog | **The last greenhouse**: a sealed glasshouse with living green inside, the only colour for a kilometre |

Wild crop nodes, salt flats, clay banks, bat roosts (saltpetre), ice faces (ice blocks), sand piles and fishing ripples (circling birds, rising fish) are dressed in every biome's lists (`island_dressing.json` layers), so every system's raw material has a place you go for it.

### 2.25 Sound and music (GAP-COOKED)

| Moment | Sound (4 layers where it's an action: tell, action, result, tail) |
|---|---|
| Crops | Planting soil pat; growth stage changes are silent; harvest: leaves rustle, stalks snap, a basket thump |
| Animals | Every farm animal has its own voice fused from real ones (Pumicehog = piglet + rock clatter; Puffram = sheep + wind; Sandhen = hen + desert lark; Chimecrab = crab clicks + glass chimes; Mournhare = hare thump + soft owl; Withergoat = goat + reversed bleat) |
| Fishing | Cast whoosh + reel tick; bobber plop; nibble tick; bite "thunk" + rumble; run tell: splash + rising line squeal 0.4 s ahead; snap: a crack and a slack whip; landing: a wet slap and the reel's last clicks |
| Cooking | Campfire sizzle, pot bubble, oven door, the discovery card's page-turn chime |
| Machines | Water wheel creak and splash, windmill sail sweep, turbine roar, conveyor clatter, drill knock; each loops seamlessly and falls off over 30 m |
| Building | Wood knock and saw, stone grind, metal clang; collapse crack and rubble; stability colours have no sound |
| Raids | Powder fuse hiss (5 s, rising), a low thump and a high crack; catapult creak and whoosh (the flight is heard the whole way); ram boom |
| Tames | Mating calls (fused animal courtship calls per species), egg crack, baby squeak versions of the adult voice, the imprint prompt (a soft two-note call) |
| Weather | Ash patter, rain on leaves and roofs (different on wood, stone and metal), wind howl, sand hiss, fog drip |

**Music (GAP-COOKED, real composed, always audible):** each biome's composer brief (3.3) adds a **home variation**: when you are inside your own claim at night with comfort 10 or more, the biome's explore theme plays in its calmest arrangement (fewer instruments, warmer), at the same −23 LUFS calm level. A legendary catch plays a 6 s composed sting. No new silences.

---

## 3. Plan

How to build this in Raven's repo (`C:\Dev\Wildreach`), following his patterns: everything in `Data/*.json` (no number in code), registries loaded from JSON (`FWRItemRegistry`, `FWRRecipeRegistry`, `FWRBuildRegistry`, `FWRCreatureRegistry`, `FWRLifeSkillRegistry`), server-authoritative actions, timestamps instead of ticking for anything that runs while nobody watches, all damage through `UWRCombatLibrary::ApplyHit`, sounds and effects as named events (`WRSound::PlayAt`, `WRFx::SpawnAt`) with rows in `Data/Audio/sounds.json` and `Data/fx.json`, screens registered with `WR_REGISTER_SCREEN` / `WR_REGISTER_JOURNAL_TAB`, every new ability a `UWRGameplayAbility` with a line in `UWRAbilityRegistry::ClassForNode`. Saves stay JSON per world (BUILT `WRWorlds.cpp`); every new saved field is added to the existing `ToJson` / `FromJson` pairs with a default so old worlds load.

### 3.1 Data rows

| File | New or changed | Rows and fields |
|---|---|---|
| `Data/items.json` | Changed | **Crops** (24 + seeds 24): `{id, name, category: "crop"|"seed", foodGroup, biome, weight, stack, spoilMinutes, use{hunger, thirst}}`. **Dishes** (14 generic + 12 biome + grey mush): `category: "food"`, `use{hunger, thirst, satedMinutes}`, `spoilMinutes`. **Preserved** (salted, smoked, jerky, dried). **Fish** (36 + 12 legendary): `category: "fish"`, `fish{tier, baseKg, biome}`. **Materials**: salt, clay, rope, sand, glass_pane, charcoal, steel_ingot, steel_plate, star_iron, starsteel_ingot, core_shard, saltpetre, black_powder, linen, planks, compost, bone_meal, spoiled_food, ice_block, animal_fat, fish_oil, slime, royal_jelly, honey, wax, feathers, wool, milk, crystal_dust, spore_sac, bond_token, remains, biome blocks (12), the 12 signature resources not yet in (stormglass … voidglass). **Potions/brews/elixirs** (2.12): `use{heal | brew{id, minutes, effects[]} | elixir{...} | cleanse}`. **Gear**: 48 biome set pieces `{slot: head|chest|hands|legs, set, load, poise, armor}`; hazard gear (10); rings (gem × metal built from a rule, not 30 rows: `ringRule{effectShare 0.6, perTier 0.1}`); 40 talismans `{slot: talisman, fromTrophy, effect{stat, value}}`; runes (8) `category: "rune"`; blueprints `category: "blueprint", teaches`; tools: 4 rods, lantern, glider, saddles (5), barding (iron, steel, 12 biome); siege ammo; fire pot, powder charge, mining charge; wax earplugs |
| `Data/recipes.json` | Changed | Rows for every item above with `station` (`hand`, `campfire`, `cooking_pot`, `oven`, `smokehouse`, `drying_rack`, `salting_barrel`, `forge`, `armorer`, `alchemy_table`, `workbench`, `charcoal_kiln`, `tanning_rack`, `loom`, `sawmill`, `grinder`, `smelter`), `level`, `inputs`, `seconds`. Move `cooked_meat` from `workbench` to `campfire`. Path tonic uses the built `pathMaterial` rule |
| `Data/dishes.json` | New | `groups{itemId: group}`; `patterns[{id, station, needs{group: count}, optional{group: count}, requiresItem?, output, priority}]` (biome dishes have `requiresItem` and a higher priority); `fallback: "grey_mush"` |
| `Data/crops.json` | New | `[{id, seed, biome, tier: common|rare, minutes, stages[4]{mesh}, yield[min,max], seedsBack[min,max], needs: "ashPit"|"water"|"dark"|"windy"|"withered"|null, wildNode, onHarvest{scream?: {radius 3000, stagger 2}}, ergot?{chance 0.1, item narcotic, count 2}}]` |
| `Data/fish.json` | New | `rods[{id, cast, lineClass}]`; `baits[{id, biome?, rareBoost}]`; `fish[{id, biome, water: river|lake|sea|cave|ice|rift, tier, lineClass, runs[min,max], fightSeconds[min,max], baseKg, drops{}}]`; `legendary[{id, biome, poiTag, phases[3]{restSeconds, runSeconds, leap}}]`; `fishing{biteMin 6, biteMax 20, strikeWindow 0.6, tensionPerSecond 0.35, runTell 0.4, staminaPerSecond 3, legendaryPerDay 1}` |
| `Data/weather.json` | New | `[{biome, kinds[{id, chance, minutes[min,max], dayOnly?, nightOnly?, effects{vision, thirstMult, staminaRegenMult, irrigates, burnDurationMult, gustMult, lightRadiusMult, corruptionMult, strikes?}}]}]`; `rollMinutes 30`, `warnSeconds 60` |
| `Data/pieces.json` | Changed | `span` adds `metal: 8, biome: 8`; `upkeepPerPiecePerHour` adds `metal`, `biome_<id>`; metal and 12 biome piece families (same shape list); freeform `beam2/beam4/post/brace` (`kind: "structural_free"`); every new piece in 2.17 with `kind` (`station`, `producer`, `crop`, `pen`, `seapen`, `container`, `power`, `machine`, `turret`, `incubator`, `stable`, `altar`, `glass`); per piece `comfort{group, value}`, `power{supply|load}`, `needs: "lava"|"river"|"sea"|"vent"|"biome:<id>"`, `damageTaken{weapon, fire, explosive, siege}` multipliers per material in a top-level `materialDamage` table |
| `Data/sets.json` | New | `[{id, biome, pieces[4], at2{effects}, at4{effects, hazardPass}}]` |
| `Data/machines.json` | New | `[{piece, kind: source|machine, supply|load, recipeStation?, rateSeconds, inputs, outputs, biomeMachine?}]`, `power{shaftLength 400, longLines 600, chainLength 4, chiefChain 6, activeRange 25000}` |
| `Data/husbandry.json` | New (replaces the single `husbandry.produceSeconds`) | `[{creature, products[{item, seconds}], feed[], pen: land|sea, breedMinutes 60, growMinutes 30, rareProduct}]`, `pen{capacity 4, troughSlots 50}`, `hive{honeySeconds 900, waxSeconds 1800, pollinateRadius 2000, cropMult 0.85}` |
| `Data/tame_trees.json` | New | `[{role, nodes[4]{id, cost, effect}, capstones[2]{id, cost 8, effect}}]` |
| `Data/creatures.json` | Changed | Per creature: `sex: true`, `birth: egg|live`, `hungerHours`, `carryKg`, `saddleRig`, `flight{speed, stamina, climb, level}` / `swim{speed, depth}`, `incubation: fire|frost|water|wood`, `remains: true`. New biomes' creatures arrive with their rounds |
| `Data/lifeskills.json` | Changed | `everyLevel.stat/perLevel` for fishing, engineering, swimming, cooking, husbandry; perk `stat`/`value` per 2.23; `requires` per perk |
| `Data/tuning.json` | Changed | New blocks: `rest{sleepSeconds 10, dimSeconds 2}`, `rested{bedrollMinutes 10, bedMinutes 20, perComfort 1, staminaRegen 1.2, drain 0.8, skillXp 1.1}`, `comfort{radius 1200, cap 20}`, `darkness{nightThreshold, darkHunterAggroMult 2}`, `lights{...}`, `corruption{buildNight 6, buildDay 15, fall 3, steps[25,50,75,100], effects...}`, `spoilage{bagMult 1, frostBiomeMult 0.5, iceBoxMult 0.2, coldStoreMult 0.05, iceBlockHours 4, halfValueAt 0.25}`, `cooking{satedPerLevel 0.01}`, `crops` adds `rareMinutes 120 (BUILT), plants 9, irrigationMult 0.67, waterWise 0.5, irrigationRadius 800, fertiliserYield 1.25, offBiomeGraftYield 0.5`, `tameLevels{wildBands, bondMax 20, xpBase 100, xpPerLevel 40, statPerBond 0.02, sources{...}}`, `tameHunger{...}`, `breeding{mateSeconds 120, cooldownHours 4, pregnancy{easy 30, medium 120, hard 360}, growth{easy 120, medium 480, hard 1440}, imprintSteps 8, imprintBonus 0.2, inherit 0.55, mutationRolls 3, mutationChance 0.025, mutationPoints 2, mutationCap 20}`, `raidDamage{...}` (2.18), `turrets{...}`, `systems{fishing 0, machinery 0, beekeeping 0, farming2 0, cooking2 0, breeding 0, comfort 0, saddles 0, underwater 0, turrets 0, siege 0}` (each round flips its system on), `lifeSkills.xp` adds `fishCommon 20, fishRare 60, fishLegend 500, trap 2, swim10m 1, dive10m 2, deepDive 5, swimHourCap 600, machinePlace 30, machineRun5m 1, discover 100, tameBond 20, birth 40, imprint 10, ritual 60` |
| `Data/spawn_regions.json` | Changed | `raiders` lists for each new biome (2.18) |
| `Data/island_dressing.json` (via `Tools/terrain/dressing.mjs`) | Changed | Layers: wild crop nodes, clay banks, salt beaches, sand piles, bat roosts (saltpetre), ice faces, nitre crusts, rock salt, fishing spots, the 36 survival set-pieces (2.24) |
| `Data/Audio/sounds.json`, `Data/fx.json` | Changed | Rows in 3.4 |

### 3.2 C++ (classes and functions to add or change)

| Where | Change |
|---|---|
| `UWRAttributeSet` | New attributes `Corruption`, `MaxHealthLost` (Void, path lane may own), `SatedUntil` is a GE, not an attribute. Hunger drain (existing periodic GE) checks the tag `Survival.Sated` and skips while it's present |
| `WRBeds.cpp` | `AWRBed::BeginSleep / FinishSleep`: the 10 s channel, cancel rules, then `FWRRest::Apply(PlayerState)`: Rested GE (length from `FWRComfort::At`), corruption ×0.5, `OnRest` broadcast (Void and Deathless listen) |
| New `WRComfort.h/.cpp` | `FWRComfort::At(UWorld*, FVector Bed)`: overlap 12 m, keep pieces under a roof (a line up hits a roof piece), highest per group, cap |
| New `WRLight.h/.cpp` | `UWRLightSubsystem` (world subsystem): lights register and unregister (pieces, held torches, lanterns, light tames); `IsLit(FVector)`; `AWRCreatureAI` perception reads it for dark-hunter aggro ×2 |
| New `WRWeather.h/.cpp` | `UWRWeatherSubsystem` (server rolls per biome every 30 min; replicated state in `AWRGameState`); effects applied by tags on players in that biome (`Weather.Ashfall` etc.); `UWRSkySubsystem` and `UWRAudioDirector` read it for visuals and beds; `UWRRaids` and crops read `irrigates` |
| `UWRStatusComponent` or new `UWRCorruptionComponent` | Corruption build and fall by biome, light and tags; step effects as GEs; path match from `UWRPathComponent` |
| `FWRItemStack` (`WRInventory.h`) | Add `SpoilsAt` (server seconds) and recompute on container move with the container's multiplier (`UWRInventoryComponent::MoveStack`, `AWRBuildPiece` chests read their `spoilMult` and biome); `Freshness()` derived on read; spoiled conversion when read past 0. Stacks of different freshness don't merge (average them if within 10%) |
| `EWREquipSlot` | Add `Head, Chest, Hands, Legs, Talisman, Ring1, Ring2`; keep `Armor` as an alias of `Chest` for old saves; `UWRInventoryComponent::RecomputeGear()` sums armour and poise, counts set pieces (`FWRSetRegistry` from `sets.json`), applies set GEs |
| `WRCrafting.cpp` | Station types added as data (`AWRStation` already takes a type); `FWRCrafting::Cook(Player, Inputs[])` for the pot: pattern match from `FWRDishRegistry`, discovery stored in the player JSON (`KnownDishes`), XP; `FWRCrafting::Salvage(Player, Slot, Station)`; blueprints: `KnownBlueprints` on the player, recipe rows with `blueprint: true` show only when known; `AWRHearth` ignores cores used for shards |
| `WRBuilding.cpp` | Crop logic: `AWRBuildPiece` crop state gains `CropId`, `PlantedAt`, `ReadyAt`, `bFertilised`; stage = elapsed ÷ total, mesh swapped on rep; `FWRCropRules::GrowSeconds(crop, irrigated, pollinated, skill, biome, weather)`; `IsIrrigated` (water volume, well, channel within 800 cm); `BiomeAt(FVector)` from the spawn-region map; dark check (no sky visibility); pens read `husbandry.json` by species; hive; fish trap; salt pan; compost; producers share one `FWRProducer` timestamp struct |
| `WRBuilding.cpp` (structure) | `SpanFor` reads metal and biome; freeform structural kind carries support (`SupportCost` by length); `ApplyIntegrityTint(bool)` for build mode colours; collapse spawns rubble (pooled Niagara/`WRFx` chunks + `WRSound`), 50 damage via `ApplyHit` with a `Collapse` source |
| `WRBuilding.cpp` (raid) | `AWRBuildPiece::TakeBuildDamage` multiplies by `materialDamage[material][damageType]`; offline protection stays as built |
| New `WRPower.h/.cpp` | `UWRPowerSubsystem` (server): networks built from touching power pieces and shafts (flood fill on place and destroy), supply and load per net, `SpeedFor(Machine)`; machines advance in 5 s steps only while a player is within 250 m, else they freeze (queues keep their timestamps) |
| New `WRMachine.h/.cpp` | `UWRMachineComponent` on a build piece: input/output buffers, `Step()`, conveyor hand-off (`TryPushTo(next)`), sorting rules; drills on resource nodes (`AWRResourceNode` gets `bDrilled`); biome machines as data rows with one special hook each (`OnStorm`, `OnDaylight`, `ShareRadius`, `ColdRoom`, `RiftShared`) |
| New `WRTurret.h/.cpp` | `AWRTurret` (piece kind turret): target selection (hostile creatures; non-clan players on PvP), 0.9 s aim tell (`WRFx` line + `WRSound`), fires `AWRProjectile` (BUILT) with homing off; damage through `ApplyHit` |
| New `WRExplosive.h/.cpp` | `AWRCharge` (placed, 5 s fuse, radial `ApplyHit` to actors and `TakeBuildDamage` to pieces); fire pot is a thrown `AWRProjectile` row |
| New `WRSiege.h/.cpp`, `WRBoat.h/.cpp` | Ram (pushed vehicle, hauler attach), catapult and ballista (crewed, arc preview to all), boats (`APawn` with buoyancy on the sea's water volume, wind from `UWRWeatherSubsystem`, a chest and two piece sockets) |
| New `WRFishing.h/.cpp` | `UWRGA_Fish` (`UWRGameplayAbility`, granted with any rod: cast charge, bite wait, strike, reel); `AWRBobber`; server picks the fish (`FWRFishRegistry`: biome, water kind, bait, rod, perks) and its run schedule, sends it to the owner; client draws tension; input is the attack verb only; `UWRUIFishing` HUD strip (tension bar, fish shape) |
| `AWRCreature` | Tame state: `Sex`, `WildLevel`, `StatPoints[6]`, `BondLevel`, `BondXP`, `SkillNodes`, `Hunger`, `Imprint`, `Mutations{mother, father}`, `bUndead`, `Saddle`, `Barding`; `AwardTameXP(Source, Amount)`; skill effects as GEs from `FWRTameTreeRegistry`; mating and pregnancy timestamps; flight and swim movement modes on its own CMC (BUILT mount CMC extended) |
| New `WRBreeding.h/.cpp` | `FWRBreedingRules::Child(Mother, Father, Rand)`: inheritance 55/45, mutation rolls, colour regions; `AWRIncubator` piece; baby scale and growth by timestamp; imprint prompts (`UWRUITaming` bubble) |
| New piece `AWRBoneAltar` and item use | Ritual flow (path check for Necromancy, 60 s channel, consume, spawn undead variant with a material swap); Dadeni draught: item use on a bond token at a Hearth |
| `UWRLifeSkillsComponent::Award` calls | `WRFishing` (catch), `UWRWaterSubsystem` swimmer step (distance, depth, speed gate, hourly cap), `UWRPowerSubsystem` (machine run time), crafting (discovery, salvage, brews), building (structural, metal), creature (bond, birth, imprint, ritual) |
| `WRUISkills.cpp` | Locked perk card for `requires` systems that are off |
| New screens (`WR_REGISTER_SCREEN`) | `WRUICooking` (pot slots + known dishes list; mouse and pad), `WRUIPower` (network panel on any machine), `WRUIBreeding` (pair card, stats with arrows for higher or lower), `WRUITameTree` (journal tab on a tame), `WRUIStable`, `WRUIBoat`. Every screen: mouse and pad, no dead states, F1 page (ui-controls rules) |

### 3.3 Assets and art route

Route for anything not in a Fab pack: **concept (Aaron picks) → Hunyuan3D 2.1 base → Blender finish (retopology, UVs, hand-fixed details, PBR) → `Tools/art` rig and import scripts**. Fab first where a real pack fits (no engine primitives, no unfinished AI meshes).

| Group | Count | Route |
|---|---|---|
| Crops: 24 × 4 growth stages + wild nodes | 96 + 24 | Real-crop Megascans/Fab foliage where the plant exists (pepper, grape vine, rye, flax, oats, apple tree, kale/turnip, rice, kelp, mushrooms, rhubarb, cactus, date palm); invented ones (glowmelon, ashen grain, mandrake, asphodel variants) by Hunyuan → Blender, emissive masks for glow crops |
| Food and ingredients: dishes, fish, preserved food, materials | ~150 icons, ~60 held meshes | Fab food scans for real dishes (bread, pies, stew bowls, fish); icons rendered from the meshes by the existing icon tool |
| Fish: 36 + 12 legendaries | 48 | Fab fish pack for real species with a simple tail rig; legendaries are each a unique Hunyuan → Blender sculpt (Old Cinderjaw scarred, the Drowned Bell with its bell), mountable trophy versions |
| Stations and producers | ~30 | Fab medieval kitchen, farm and blacksmith packs (pot, oven, smokehouse, barrel, well, beehive, drying rack, kiln, loom); `WRBuildLook` detail over the collision blocks as built |
| Machines + 12 biome machines | 12 + 12 | Fab medieval mill and steampunk packs for wheel, windmill, gears, conveyor; biome machines concept → Hunyuan → Blender |
| Building: metal family + 12 biome sets (12 shapes + 6 decor + rug each) | ~230 | Fab modular kits matched per biome (basalt, nordic timber, desert sandstone, gothic chapel, coral/shell), kitbashed in Blender to the 4 m grid and the built shape list |
| Biome armour sets: 12 × 4 pieces × 2 bodies | 96 | Concept (Aaron picks) → Hunyuan base → Blender: retopology, fit and skin-transfer to the MetaHuman male and female bodies, cloth sim weights where it drapes (Dunewrap veil, Gravecloth coat) |
| Hazard gear, rings, talismans (as worn small meshes) | ~50 | Blender from Fab bits; talismans reuse each trophy mesh scaled to a pendant |
| Saddles (7 rigs) and barding (2 metal + 12 biome) | ~100 fitted | Blender per rig, fitted per species by socket offsets |
| Siege, boats | 5 | Fab medieval siege pack; boats from a Fab viking ship pack refitted |
| Tames: undead variants, mutation colour patterns, babies | Material swaps + scaled meshes | Undead = an overlay material (exposed bone mask, eye glow) per species; mutations = colour-region masks on the existing creature materials; babies = the adult mesh at 10 to 100% scale with head 1.3× at birth easing to 1.0 |
| Rubble chunks, compost, clay, salt, ice blocks | Small | Fab |

### 3.4 Sound and music cues

All rows in `Data/Audio/sounds.json` (keys like `fish.cast`, `fish.bite`, `fish.runTell`, `fish.snap`, `cook.discover`, `crop.harvest`, `machine.waterwheel`, `build.collapse`, `raid.fuse`, `raid.blast`, `catapult.flight`, `tame.mate.<species>`, `tame.baby.<species>`, `weather.<biome>.<kind>`). Sources: licensed real recordings (field recordings of mills, kitchens, fishing reels, animals) fused per creature as 2.25 says; nothing generated. Music: one **home variation** per biome added to each biome's composer brief (12 cues), one legendary-catch sting; `UWRAudioDirector` picks the home variation when inside your claim at night with comfort ≥ 10.

### 3.5 Map dressing

`Tools/terrain/dressing.mjs` gains layers for wild crops, clay, salt, sand, bat roosts, nitre, ice faces, rock salt and fishing spots, per biome, and places the 36 survival set-pieces (2.24) as kits (the same camp-kit mechanism as `Data/camp_kits.json` from the ui-controls cook). Fire and Frost get theirs first on the current island.

### 3.6 Build order inside this scope (proof-of-one first)

Follows FULL-GAME section 11 rounds. Each step: build, compile, cook, package; Aaron tests.

| Order | Round | Build | Proof-of-one inside it |
|---|---|---|---|
| **1** | **R0** | **Swimming XP; hidden perks (`requires` + locked card); Rest defined (10 s sleep, `OnRest`); "no floors" audit of deprivation and drowning damage** | **Proof-of-one for the whole lane:** a player swims 200 m, gains Swimming XP, sees the Fishing perks locked "Comes with fishing", sleeps 10 s in a bedroll and gets the Rest event. Small, touches the life-skill, UI and bed code every later step uses |
| 2 | R1 | Weather (`UWRWeatherSubsystem`, all 12 rows as data; Fire and Frost first: Ashfall and Blizzard), Tracking XP | Ashfall over the Volcanic wedge with its counter (a face-cover helmet) |
| 3 | R2 | Gear slots (4 + talisman + 2 rings) and save migration; steel; salvage; metal building; freeform structure and integrity colours; creature levels, bond XP, the 5 role trees; saddles and barding; revive (Dadeni needs L5 to craft: the item and flow land here, the recipe opens at L5); rings and talismans from the Fire and Frost trophies; Forgeplate and Rimeguard sets (Fire, Frost) and Mirehide (the round's Swamp) | Forgeplate full set crossing lava; a Cinderfang reaching bond level 2 and buying Hardened |
| 4 | R3 | Crops (all 24 as data; Fire, Frost, Forest live), irrigation, wells, compost, husbandry table, beekeeping, cooking stations, discovery, sated, spoilage, preservation, salt, brews L2 to L4, machinery part 1 (water wheel, windmill, shafts, powered stations, sawmill, grinder, loom, smelter, conveyor, sorting), Engineering XP | **Fire pepper** from wild seed → plot (4 stages, irrigated by the steaming river) → pot → **Fire pepper stew** discovered → eaten (sated 20 min) → leftovers spoil to compost. Then Heartwood set and the Forest dishes |
| 5 | R4 | Rested and comfort, darkness and lights, corruption, Cleansing draught, Hearth 5 and starsteel, drills, turrets, soul engine, necro ritual, elixirs | A bed in a room with comfort 12 giving 32 min Rested |
| 6 | R5 | Blueprints, biome machines (first: Lava forge, Cold store, Sun mirror array), biome building sets (Basalt hall, Rime lodge, Sun court), Dunewrap | Basalt hall walls built from biome blocks |
| 7 | R6 | Fishing (all), fish traps, boats, swimming mounts and Floeback, Tidecoral, Deepbreath, underwater perks | A vent tilapia on a crude rod, then Old Cinderjaw on a steel rod |
| 8 | R7 | Windmane and flight rules, Cloudback, glider, Skysilk, Updraft lift | Windmane flight with the boss-arena forced landing |
| 9 | R8 | Breeding, incubator, babies, imprint, mutations, Sporeweave, Mycelium vat | Two Pumicehogs → hogling with inherited stats → one mutation seen |
| 10 | R9 | Explosives, siege, path and tame raiding, runes, Wardcloth, Resonance pylon | A stone wall falling to 2 powder charges |
| 11 | R10 | Monuments, Voidglass, Rift chest | A Vulkarn monument with its plaque |
| Each biome round | R1 to R10 | That biome's crops, animals, fish, set, set decor, biome machine, weather row, survival set-pieces, raider list | |

