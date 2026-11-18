# Wildreach: full-release build plan

For Aaron. Written 2026-11-10 against v0.7.1 (commit 372557e).

**Aaron's decisions this plan is built on:**
1. "Final game" means all 12 biomes at full release: every biome and power path, portals, machinery, farming, endgame. Polish Fire and Frost first, then one new biome per round.
2. Development builds while cooking. Shipping only for the final release.
3. Aaron claims the six Fab packs on his own account.
4. Wildreach is in Aaron's hands.
5. (11-10 form) We work in Raven's repo as an approved author: cook the plan exactly as Raven wrote it, then publish to his repo. After the cook, the project goes back to Raven to choose the next step.
6. (11-10 form) No world wipes for now; Raven decides if one ever happens. Who gets builds when is Raven's call.
7. (11-10 form) No First Playable stop: keep building while friends play, judged against the quality bar of shipped games.
8. (11-10, later) Regions may vary in size; this supersedes the form's "all the same size". Every biome keeps 1 major boss and 3 to 5 minis (Raven's build plan); land area and creature count fit the biome. Sizes in Round 0 step 0.4. Combat copies Elden Ring: `COMBAT-AND-LESSONS.md`.
9. (11-10 form) The ten open design items and anything not in Raven's plan go to Raven (email / next planning doc for his approval).

This replaces the "no new cook" recommendation in `FINAL-PROPOSAL.md`.

Where this says **Proposed**, the docs don't settle the point and it's a suggestion. "Not stated in the docs" means exactly that. All names, perks and numbers stay first drafts in data files, as the repo's rules ask.

**The biggest gap:** the docs fully design only Fire and Frost. For the other 10 biomes they give a one-line idea, a resource, a full skill tree and combos. Creature lists, bosses, signature resources, hazards and map areas are **not stated in the docs**, so every round opens with a design step.

---

## 1. Definition of done

The build plan's own finish line for this tier: **every biome has its boss, path and portal, and players have a long-term goal after maxing two paths.**

The table covers every line of the build plan's Full release tier. "R" is the round where it's first built. Items already in the game are marked done.

| System | Item (build plan, Full release) | Round |
|---|---|---|
| **Biomes** | The other 10 biomes in opposite pairs | R1 to R10 |
| | Day/night spawns; hidden sub-biomes; shrines; path masters; biome events | R1 (added to Fire and Frost in the same round) |
| **Paths** | All 12 paths and resources | R1 to R10 |
| | Owned-pair combo abilities (all 66 pairs; Fire + Frost is done) | R1 to R10 |
| | Main path passives (root passives) | Each path's round |
| | Body changes per path; respec rules finished | R1 |
| **Combat** | Ranged combat | Done (built early) |
| | Mounted combat; smart enemy groups | R2 |
| | All status effects | Spread across rounds, complete by R8 |
| | Elemental weaknesses tuned across 12 paths | Final round |
| **Bosses** | 1 major and 3 to 5 mini-bosses per biome | R1 to R10; Fire and Frost topped up in R1 |
| | Multi-phase fights; arena mechanics; group scaling | Done, repeated per boss |
| | Roaming mini-bosses; bosses that punish tames | R1 |
| | Summoning altars | R5 |
| **Taming** | Other taming methods | Done (four methods built) |
| | Necro raising, quick and ritual | R4 |
| | Creature armor and saddles; pack mounts; creature levels and skill trees; revive item | R2 |
| | Swimming mounts (and Floeback) | R6 |
| | Flying mounts | R7 |
| | Breeding with mutations | R8 |
| | Ore-sensing creatures | Done in part (Kilnmite, Rimeshell); checked in Round 0 |
| **Survival** | Weather that matters | R1 |
| | Corruption level; darkness in caves; rested buff | R4 |
| **Farming** | Varied crop times; biome crops; irrigation; greenhouses; husbandry; beekeeping; cooking and recipe discovery; spoilage; alchemy and brewing; farming skill tree | R3 |
| | Fishing | R6 |
| **Crafting** | Stations to tier 5 | R2 (level 4), R4 (level 5) |
| | Quality tiers, +1 to +10 upgrades, queues that run while away | Done |
| | Gem sockets | Done |
| | Rune sockets | R9 |
| | Salvage | R2 |
| | Blueprints | R5 |
| | Boats | R6 |
| | Siege weapons | R9 |
| **Machinery** | Water wheels, windmills, power, conveyors, sorting | R3 |
| | Drills; medium production chains | R4 |
| | Biome-themed machines | R5, then each later round |
| **Building** | Freeform structural pieces; metal tier; structural integrity | R2 |
| | Comfort buff; powered turrets | R4 |
| | Biome material tiers; themed building sets | R5, then each later round |
| | Random creature raids; offline protection setting | Done |
| **PvP** | Clan ranks and alliances; anti-zerg limits; explosives and raid tools; path-based raiding; tame raiding; bounties and infamy; contested bosses | R9 |
| | Arena PvP setting | R9, but see section 6, item 7 |
| **Portals** | 5 ranks; handmade rooms shuffled with modifiers; puzzles and traps; end bosses; keys; dungeon breaks and escaped bosses; legendary announcements | R5, rooms added each later round |
| **World** | Fill-in map; shrine fast travel; lore in items and ruins | R1, lore added each round |
| | Player messages | R2 |
| | Opposite-biome invasions | R2 (Fire and Frost, Lightning and Blood), each later pair when complete |
| | NPC villages and factions; traveling merchants | R3 |
| | World events | R5 |
| **Endgame** | Artifact modifiers with 3 slots | R5 |
| | Boss soul forging | R7 |
| | Raids for 6 to 10 players; mythic gear from every biome; prestige; leaderboards; achievements, titles, cosmetics; boss trophies as monuments | R10 |

Also required to call it done (from the First Playable tier and the design doc):
- [ ] First Playable evening run (friends play a full evening). Not a stop: building continues while they play.
- [ ] Dedicated server for 10 to 100 players. Today the host's PC is the server, and nothing is tested past a few players.
- [ ] Full test suite passes.
- [ ] A Shipping build, tested, with an installer and auto-update pointed at the right place.

### Dream list: as Raven's plan has it

Aaron (11-10): what's in Raven's plan gets cooked as written; anything we made up goes in the next planning doc for Raven to approve. Raven's build plan puts these nine in its Dream tier (after Full release), so they stay there unless Raven moves them.

1. Fully new hybrid creatures from cross-biome breeding
2. Secret hybrid paths
3. God path
4. Legendary weapons with unique movesets
5. Optional secret super-bosses
6. Tamable mini-bosses
7. Bases built on large tames
8. Seasons that change biomes
9. Endless tower portal with leaderboard

Knock-on: some design text points at these. The Beast lord perk's "easier mini-boss taming" has nothing to act on without item 6, and the design doc's biome list mentions seasons. Those lines would be hidden or reworded in data.

---

## 2. How every round works

### The proposed biome order

The docs give two rules that shape this: "biomes ship in opposite pairs", and "systems that multiply content wait". One biome per round fits both if each pair takes two rounds in a row.

| Round | Biome | Path | Pair | Why here |
|---|---|---|---|---|
| R1 | Storm peaks | Lightning | with R2 | Mountains suit the existing terrain tools. No new way of moving. Safest round to prove the 12-biome map and the shared biome kit |
| R2 | Swamp | Blood & curse | with R1 | Simple bar, adds bleed. Reaching 4 major bosses unlocks Hearth level 4 |
| R3 | Ancient forest | Nature | with R4 | First needs farming, irrigation, greenhouses and summoned plants |
| R4 | Graveyard / catacombs | Necromancy | with R3 | Reuses creature bodies for the raised dead. Brings corruption, darkness and rest. Reaching 6 major bosses unlocks Hearth level 5 |
| R5 | Desert | Sun & sand | with R6 | Light on new tech, leaving room for portals, by then there are six biomes to feed them |
| R6 | Ocean & reefs | Tide | with R5 | Needs the biggest water work: underwater play, boats, fishing, swimming mounts |
| R7 | Sky islands | Wind | with R8 | Needs gliding, flying mounts and land that isn't ground-level |
| R8 | Fungal caverns | Spore | with R7 | Underground biome. Breeding lands here, when there are many creatures to breed |
| R9 | Crystal caves | Arcane | with R10 | Reuses R8's underground work. Runes, shields. PvP pass |
| R10 | Corrupted wasteland | Void | with R9 | Reuses R4's corruption. Natural last stop, carrying the endgame |
| Final | No new biome | | | Balance across all 12, dedicated server proof, Shipping |

### The biome wheel (found while reading)

DESIGN_REVIEW #19 says the wheel order isn't written down, but `combos.md` gives every pair's position, and it fits together exactly:

**Fire, Lightning, Sun & sand, Nature, Wind, Arcane, Frost, Blood & curse, Tide, Necromancy, Spore, Void, and back to Fire.**

Each path sits straight across from its documented opposite. The map plan uses this.

### The steps in every biome round

Each round section below only lists what's specific to it.

| Step | What happens | Job size |
|---|---|---|
| 1. Design | Write the biome's roster, bosses, signature resource, hazard, caves, sub-biome, shrine, path master and event. Creature count and area from the size table (Round 0 step 0.4); the role mix leans to the biome's strengths, as Raven's creature doc allows. Every creature tameable by listed methods. Aaron approves before art starts | Lane |
| 2. Data | Fill the data files. Generate the path file with `Tools/data/extract_trees.mjs` | Lane |
| 3. Code | Each new ability is a `UWRGameplayAbility` class plus a line in `UWRAbilityRegistry::ClassForNode`. All damage through `UWRCombatLibrary::ApplyHit`. Stat nodes go through `nodeEffects` in tuning | Lane, then a heavy build |
| 4. Art | Creatures, bosses, props, trees, materials. Concepts first, picked by Aaron | Heavy (uses the GPU) |
| 5. Map | Dressing, caves and spawn regions by script. Landscape only if unavoidable | Lane, then heavy if the editor is needed |
| 6. Sound and effects | Rows in `sounds.json` and `fx.json` | Lane |
| 7. Tests | New bot test plus the existing ones below, then `Tools\alltests.ps1` | Heavy |
| 8. Cook | Development package and installer, into a new folder. Aaron plays it | Heavy |

**Standing rules for each biome, from the docs:**
- The major boss is weak to the opposite element, has 2 phases, and has a hidden third phase with artifacts. Its arena holds that biome's altar.
- Major boss drops: boss core, path materials, a chance at a boss part. Mini-bosses drop upgrade shards and a trophy.
- Every biome has caves and a signature resource found nowhere else, and its hazard is a gear check, never a temperature meter.
- Creatures resist their own biome's hazard.

**Data files touched every round:** `Data/paths/<id>.json` (new), `combos.json` (new at R1), `creatures.json`, `items.json`, `recipes.json`, `spawn_regions.json`, `island_dressing.json`, `island_walk.json`, `tuning.json` (`spells`, `passives`, `nodeEffects`), `lifeskills.json` where a skill is touched, `preload.json`, `player_art.json` (body change), `fx.json`, `Data/Audio/sounds.json`, `Data/UI` (icons, portraits).

**Tests extended every round:** `test.ps1` (rules, tree shape, caps), `spelltest.ps1`, `aitest.ps1`, `tametest.ps1`, `bosstest.ps1`, `bosschasetest.ps1`, `wildlifetest.ps1`, `roamtest.ps1`, `terrain\hazardtest.ps1`, `terrain\walktest.ps1`, `crafttest.ps1`, `uishots.ps1`, `worldshots.ps1`, `audiotest.ps1`, and the `alltests.ps1` table. Proposed new names are given per round.

---

## 3. Work split rule

This desktop runs **one heavy job at a time, with at most two background lanes.**

- **Heavy** means anything that loads the CPU or GPU hard: a game or editor build, a cook, the full test suite, an editor map job, an art batch, a benchmark.
- **Lanes** are light: writing design, data and scripts, writing C++ to be compiled later.
- Art batches count as heavy. `PERFORMANCE.md` shows other GPU work wrecks the numbers, and the known GPU hang shows up under heavy load.
- The full suite takes about 90 minutes today and will grow every round.
- Only one job touches maps or `.uasset` files at a time, since they can't be merged.
- Two lanes must not edit the same files. New files in one build can break another's (unity build trap in HANDOFF).
- If a round doesn't fit, split it at a natural seam (R3a, R3b). Don't run two heavy jobs to catch up.
- No dates are promised anywhere in this plan. A round ends when Aaron has played its cook.

---

## 4. Round 0: polish Fire and Frost, and get ready

No new biome. Ends with the First Playable gate.

### 0.1 Set up this desktop

| Item | Today | Needed |
|---|---|---|
| Six Fab packs | None in `Content\` | Aaron adds them from his account into the exact folder names in the README (about 4.8 GB), then moves the four Barghest mane and tail files out |
| Art from Git LFS | Not downloaded | One pull, about 755 MB. Depends on question 1 |
| Unreal | 5.8.3 at `D:\UnrealEngine\UE_5.8`; about 60 scripts expect the default folder | A folder link at the default path, so no script changes. 5.8.4 is what the project was built with |
| Visual Studio | Build Tools only, no game workload found | Install the README's list |
| Disk | About 38 GB free on C:, 86 on D:, 129 on T: | README asks about 100 GB. Move the project and build output off C: |
| Art pipeline | Not on this PC (section 6, item 1) | Rebuild, see question 2 |

Then `Tools\check_setup.ps1` until all PASS, build, and `Tools\alltests.ps1 -SkipBuild -Only unit,survival`. Nothing opens or saves an asset before all six packs are in.

### 0.2 Grading tools

From `QUALITY-PLAN.md`: sign off the bars, build the inventory, build the zoo map (`L_Zoo`) and the note key. Aaron signs the bars now.

### 0.3 Polish list

| Area | Work | Source |
|---|---|---|
| Movement | Feel pass on any terrain. Walk and sprint speeds, walkable angle and step height, with a person testing slopes. Bar's "instant respawn" reworded to match the 0.6.0 wake map | Quality row 1 |
| Camera and body | Full-body first person, camera on the head bone | Quality row 2 |
| Animation | Fresh look at foot slide on the new clips and the pack creatures. Decide if generated clips meet the bar | Quality row 3 |
| Creatures | Grade the whole roster against the bar | Quality row 4 |
| Surfaces | Shrubs: grey them some other way than the snow coat, which hangs the GPU. Check trees and rocks at distance. Check the ridge's sawtooth shadow | Quality row 5, changelog |
| Items | Held tools as real models, not engine shapes. Build detail against collision | Quality row 6 |
| Sound | Unique creature voices in place of shared classes. Set a gentle default volume | Quality row 7 |
| Unfinished skills and perks | Swimming gets its XP source now, since swimming exists. Fishing waits for R6, Engineering for R3. Perks with nothing behind them are hidden until their system arrives. Bounties stay off the screen until R9 | Changelog 10-23, Quality row 8 |
| "[not built yet]" labels | Check in game. CLAUDE.md says some nodes show it; the changelog says every node works | Quality row 8 |
| Tame whistle | Give it a screen. It has none, and Shipping removes the console | HANDOFF |
| Menus | Mouse sensitivity and invert. Controller support | Quality row 9 |
| Tuning | One pass on combat, XP, hunger, wildlife and raids from play. Numbers stay in `tuning.json` | CLAUDE.md |
| GPU hang | Find the cause of the `NodeAndClusterCull` hang. It's "not looked into yet" and will get worse with bigger worlds | PERFORMANCE |
| Weaker PC | Measure on something below the 4070 Ti. Medium settings target | Quality row 10 |
| Melee lag compensation | The changelog listed none on 10-22 and no doc says it was added. Check, build if missing | Changelog, M1 |
| Bot load test | M7 planned `Tools/bots.ps1`. It doesn't exist. Build it and run 20 bots | M7 |
| Stale notes | README's "placeholder art", CLAUDE.md's old status notes, DESIGN_REVIEW #31 (raids shipped), the M7 Shipping line | Found in the read-through |

### 0.4 The 12-biome map plan (plan only, no terrain built)

`map.md` covers only Fire and Frost and says even that needed planning. The layout document says Full release needs "a larger map to fit the other 10 biomes". This step produces the layout for Aaron's approval.

**Proposed shape:** one landmass with 12 wedges around a central high ridge, in wheel order, so opposites really do sit straight across. Fire stays west and Frost east, as they are today. Clockwise from north: Nature, Wind, Arcane, Frost, Blood, Tide, Necromancy, Spore, Void, Fire, Lightning, Sun. Sky islands float above the Wind wedge, the Tide wedge opens onto the sea, and the two cavern biomes have surface entrances with most of their play underground.

**Size (Aaron 11-10: regions may vary).** Fire and Frost use about 9.9 km² together. Each wedge's angle follows its biome's area; opposite pairs still sit straight across. Proposed, from `COMBAT-AND-LESSONS.md` Part 3:

| Biome | Surface km² | Extra volume | Creatures | Major | Minis |
|---|---|---|---|---|---|
| Volcanic (Fire) | 5.0 (built) | Lava tubes | 9 | 1 | 3 (top up 1) |
| Frozen tundra (Frost) | 5.0 (built) | Ice caves | 9 (Floeback at R6) | 1 | 3 (top up 1) |
| Storm peaks (Lightning) | 5.5 | Vertical cliffs | 8 | 1 | 3 (1 roaming) |
| Swamp (Blood & curse) | 4.0 | Shallow water | 10 | 1 | 3 |
| Ancient forest (Nature) | 7.0 | Canopy | 12 | 1 | 4 |
| Graveyard (Necromancy) | 2.5 | Catacombs ~2 km² | 8 | 1 | 4 |
| Desert (Sun & sand) | 6.5 | Buried ruins | 7 | 1 | 3 (1 roaming) |
| Ocean & reefs (Tide) | 3.0 land | Sea and reef ~6 km² | 12 | 1 | 3 |
| Sky islands (Wind) | 2.0 islands | Air ~5 km² | 6 | 1 | 3 |
| Fungal caverns (Spore) | 1.5 | Caverns ~3 km² | 10 | 1 | 3 |
| Crystal caves (Arcane) | 1.0 | Caves ~2 km² | 6 | 1 | 3 |
| Corrupted wasteland (Void) | 6.0 | Rifts | 8 | 1 | 5 |
| **Total** | **49.0** | | **105** | **12** | **40** |

About 49 km² of surface land (underground and air extra). **Decoration and variety rule (Aaron 11-10):** every biome keeps a landmark or encounter every ~150-250 m of travel, 3-5 distinct sub-zones, and set dressing so no stretch reads empty; the desert especially (oases, ruins, dunes vs hardpan, canyons, wrecks). Sub-zone lists are in `COMBAT-AND-LESSONS.md` Part 3. Art: 87 new creatures plus 46 new bosses (10 majors, 34 minis, 2 Fire/Frost top-ups), about 133 models.

**What must change in the tools:** `layout_proposal.mjs` takes a size, but `dressing.mjs`, `spawns.mjs`, `sea_mask.mjs`, `valleys.mjs` and `walkmap.mjs` have 4033 fixed in them, and two have a fixed ridge line.

**The plan must answer:**
- Land resolution. Each rebuild at 4 km costs about 80 MB of LFS; at this size, about 7 times that (multiplication again). So the landmass should be built **once**.
- Whether a rebuild can rewrite only one wedge's pieces: not stated in the docs. Test it on the current island.
- How unbuilt wedges stay shut (Proposed: closed passes until their round).
- Server loading. Today a server loads every cell and joined players load the whole island (x24). That won't scale. Server streaming (planned in M3, off today) and HLODs become required, which settles the HLOD question: **needed for full release.** Prove both here on a test map, heavy job.
- Old worlds. Worlds store positions, so a new landmass ends them (question 3).

### 0.5 Exit

Development cook. Aaron plays, and friends play a full evening. R1 starts without waiting on it (decided 11-10).

---

## 5. The biome rounds

### R1: Storm peaks (Lightning), plus the map and the biome kit

Size warning: the largest round. Likely splits as R1a map, R1b kit, R1c biome.

- **Map:** build the approved 12-wedge landmass, once. Move Fire and Frost onto it by regenerating dressing, spawns, camps and arenas from layout data. Build the Storm peaks wedge. One heavy editor job, nothing else touching maps.
- **Biome kit (added to Fire and Frost in the same round):** day/night spawn tables; a hidden sub-biome per biome (design's example: lava tubes under the volcano); shrines that give path XP when cleansed or claimed, plus fast travel; path master NPCs who teach through quests; biome events (example: eruptions); fill-in map; weather system (storms first). Top up Fire and Frost to at least 3 mini-bosses each (build plan asks 3 to 5, they have 2). Roaming mini-bosses. Bosses that punish or scatter tames. Respec item and long quest. Body changes per path (design: glowing veins for fire).
- **Biome content:** storm-swept mountains, weather that matters. Signature resource and hazard not stated in the docs: decided in the design step.
- **Path:** Charge (fills fast, empties fast). Root: Grounded. New abilities: Spark, Static strike, Chain lightning, Thunderclap, Bolt dash, Charged weapon, Static field. Passives with behaviour: Conductive, Afterimage, Rapid fire, Discharge. Capstones: Tempest, Lightning step, Thunder god. New status: shock. Path picker on the main menu goes past two choices. Conductive needs soak, which arrives at R6, so it waits or gets a stand-in.
- **Combos:** 2 new pairs (Fire + Lightning: Plasma, Meltdown. Lightning + Frost: Brittle shock, Blizzard storm).
- **Creatures and bosses:** not stated in the docs. Size: 5.5 km², 8 creatures, 1 major, 3 minis (1 roaming). Major is weak to Blood & curse, the opposite, which isn't in until R2, so tune it then.
- **Art:** about 12 models (8 creatures, 4 bosses), mountain trees and rocks, ground and rock materials for the wedge. One biome's worth of landscape material layering to prove on this round.
- **Data (beyond the standing list):** `Data/combos.json` (schema in `DATA_SCHEMAS.md`; doesn't exist today), `Data/hearth.json` if tech levels move out of code, new `Data/shrines.json`, `Data/quests.json`, `Data/weather.json` (proposed names).
- **Tests:** proposed `Tools\weathertest.ps1`, `shrinetest.ps1`, `lightningtest.ps1`. `walktest` over the new landmass. `worldstest` for joined players on the new map.
- **Depends on:** Round 0, the map plan, the art pipeline.

### R2: Swamp (Blood & curse)

- **Biome content:** swamp and wet ground. Details not stated in the docs. Design note: a blood moon is the doc's example event, but it names the graveyard, so R4.
- **Path:** Blood (spends your own HP; lifesteal earns it back). Root: Bloodthirst. Abilities: Blood bolt, Crimson slash, Blood spear, Blood boil, Transfusion, Hex of weakness, Doom, Leech strike. Passives: Misfortune, Contagion, Blood frenzy, Feast, Clotting. Capstones: Crimson tide, Ruin, Sanguine lord. New statuses: bleed, curse. The healing rule needs settling first (section 6, item 7).
- **Combos:** 3 new pairs, including the opposite pair Lightning + Blood.
- **Creatures and bosses:** 4.0 km², 10 creatures, 1 major (weak to Lightning), 3 minis.
- **Shared systems first needed here:** Hearth level 4 reachable (4 major bosses), so steel recipes and metal building. Freeform structural pieces and structural integrity. Salvage. Mounted combat. Smart enemy groups that flank, surround and retreat. Creature armor, saddles, pack mounts, creature levels and skill trees, revive item. Player messages. **First invasions** between Fire and Frost, then Lightning and Blood.
- **Art:** about 14 models (10 creatures, 4 bosses), swamp plants, water surface, steel gear, metal building pieces, saddles and armor on existing creatures.
- **Map:** Swamp wedge dressing, shallow water (extends `sea_mask` and the water system).
- **Data:** add `pieces.json` (metal), `server_presets.json` (invasion frequency).
- **Tests:** proposed `invasiontest.ps1`, `mountedtest.ps1`. Extend `housetest`, `buildtest`, `crafttest`, `tametest`.
- **Depends on:** R1's map, kit, weather.

### R3: Ancient forest (Nature)

Size warning: farming and machinery both start here. Likely R3a farming, R3b machinery.

- **Biome content:** ancient forest. Details not stated in the docs.
- **Path:** Growth (fast near plants, slow in barren areas). Root: Rooted. Abilities: Thorn shot, Briar strike, Regrowth, Sanctuary, Vine lash, Root snare, Sprout turret. Passives: Purify, Thorned skin, Pollen, Overgrowth. Capstones: World tree, Strangling vines, Ancient guardian. Builds the **summon framework** used again by R4 and R8. Bleed already in from R2.
- **Combos:** 4 new pairs.
- **Creatures and bosses:** 7.0 km², 12 creatures, 1 major (weak to Necromancy, so tune at R4), 4 minis.
- **Shared systems first needed here:**
  - **Farming:** crops with varied growth times, biome crops (all built biomes), irrigation, greenhouses (Hearth 4), animal husbandry, beekeeping, cooking and recipe discovery, spoilage and preservation, alchemy and brewing, farming tree. Life skills Farming, Husbandry and Cooking go live.
  - **Machinery, part 1:** water wheels, windmills, power (water, wind, lava), conveyors, sorting. Engineering skill goes live.
  - **World:** NPC villages and factions, traveling merchants.
- **Art:** about 17 models (12 creatures, 5 bosses), forest trees and plants, crop stages, machines, NPC bodies, villages.
- **Map:** forest wedge; village sites.
- **Data:** new `Data/crops.json`, `machines.json`, `npcs.json` (proposed names); `lifeskills.json`.
- **Tests:** proposed `farmtest.ps1`, `machinetest.ps1`, `summontest.ps1`, `npctest.ps1`.
- **Depends on:** R2 for Hearth level 4; R1 for NPC basics.

### R4: Graveyard / catacombs (Necromancy)

- **Biome content:** dark-fantasy graveyard and catacombs; the doc's blood moon event.
- **Path:** Souls (from kills, spent to raise and empower). Root: Deathless, which needs "rest" defined (#13). Abilities: Raise dead, Grave strike, Second wave, Corpse explosion, Scythe sweep, Shadow bolt, Curse of weakness, Life drain. Passives: Shared pain, Soul siphon, Reaping stride. Capstones: Army of the dead, Final harvest, Lich form. New status: decay/wither.
- **Raising the dead:** quick combat raises paid in Souls (not counted against the tame limit), permanent undead tames by ritual. Any creature can be raised.
- **Combos:** 5 new pairs, including opposite pair Nature + Necromancy.
- **Creatures and bosses:** 2.5 km² surface plus about 2 km² of catacombs, 8 creatures, 1 major (weak to Nature), 4 minis.
- **Shared systems first needed here:** corruption level (minor debuffs, or buffs if the biome matches your path); darkness in caves, on every biome; rested buff; comfort buff; **Hearth level 5** (6 major bosses) and starsteel; machinery part 2 (drills, medium production chains, powered turrets, soul energy).
- **Art:** about 13 models (8 creatures, 5 bosses), undead look over existing bodies, tombs and catacomb pieces, drills and turrets.
- **Map:** graveyard wedge with catacomb interiors.
- **Data:** `tuning.json` (corruption, darkness, rested), `machines.json`.
- **Tests:** proposed `raisetest.ps1`, `corruptiontest.ps1`, `darknesstest.ps1`.
- **Depends on:** R3's summon framework and power.

### R5: Desert (Sun & sand), plus portals

- **Biome content:** desert, sandstorms. Details not stated in the docs.
- **Path:** Radiance (charges in daylight, weak at night). Root: Sandwalker. Abilities: Sunbeam, Sun strike, Solar flare, Beacon, Sandstorm, Burrow, Quicksand, Blazing arc. Passives: Dawn, Gleam, Zenith. Capstones: Sunburst, Desert's wrath, Noon strike. New status: blind. Beacon hurts undead and void creatures (undead from R4, void at R10).
- **Combos:** 6 new pairs.
- **Creatures and bosses:** 6.5 km², 7 creatures, 1 major (weak to Tide, so tune at R6), 3 minis (1 roaming).
- **Shared systems first needed here:**
  - **Portals:** 5 ranks shown by colour, higher ranks deeper in a biome. Handmade rooms shuffled, with random modifiers, puzzles and traps, and an end boss. Keys craft a chosen rank. Ignored portals break on the doc's real-day timers (1, 3, 6, 10, 18), spill monsters, and close when the boss escapes, which blocks that rank in that biome until it's killed. Legendary portals announced to the server. Rooms built for all six existing biomes this round.
  - **Artifact modifiers** with 3 slots. Unlocks the bosses' hidden third phases, which are already built.
  - Blueprints, summoning altars, world events, biome-themed machines and building sets (first set, back-fill earlier biomes).
- **Art:** about 11 models (7 creatures, 4 bosses), ruins, portal rooms per biome (the largest art list so far), artifact icons.
- **Map:** desert wedge; portal spawn points across all built wedges.
- **Data:** new `Data/portals.json`, `artifacts.json`; `server_presets.json` (portal break timers, artifact limits).
- **Tests:** proposed `portaltest.ps1`, `artifacttest.ps1`. `persisttest` for portals across restarts.
- **Depends on:** R4 (six biomes, Hearth 5).
- **Dedicated server:** the best point to prove it before the world grows further (question 7).

### R6: Ocean & reefs (Tide)

- **Biome content:** ocean and reefs, the Tide wedge opening onto the sea.
- **Path:** Flow (builds moving and dodging, drains standing still). Root: Gills. Abilities: Water bolt, Wave cut, Riptide pull, Whirlpool, Harpoon throw, Healing wave, Cleanse, Tidal shield. Passives: Drowning, Anchor, Tow. Capstones: Maelstrom, Leviathan's hook, High tide. New status: soak (R1's Conductive turns on here).
- **Combos:** 7 new pairs, including opposite pair Sun + Tide.
- **Creatures and bosses:** 3.0 km² land plus about 6 km² of sea, 12 creatures including swimmers, 1 major (weak to Sun), 3 minis.
- **Shared systems first needed here:** fighting and breathing underwater; swimming mounts, including deferred Floeback; boats; fishing (the skill goes live, biome fish). Today water is a swim volume and creatures stop at belly depth, so water AI is new.
- **Art:** about 16 models (12 creatures, 4 bosses), reef props, sea floor, boats, fish.
- **Map:** sea floor depth and reefs. Check how the landscape and `sea_mask.json` handle depth. Heavy job if the landscape changes.
- **Data:** `items.json` (fish), `pieces.json` (boats, if placed).
- **Tests:** extend `watertest.ps1`; proposed `boattest.ps1`, `fishtest.ps1`, `swimmounttest.ps1`.
- **Depends on:** R3 (fishing ties to cooking), R5 pair completes for invasion.

### R7: Sky islands (Wind)

- **Biome content:** floating islands, brighter magic in tone (design doc). How they're built isn't stated in the docs: landscape can't float, so they'd be meshes. Prove on one island first.
- **Path:** Momentum (builds airborne or sprinting). Root: Featherfall. Abilities: Gust, Wind cut, Glide, Air dash, Plunge, Whirlwind, Vortex, Wind arrow. Passives: Eye of the storm, Lingering gale, Tailwind shot, Piercing gale. Capstones: Sky lord, Tornado, Storm volley.
- **Combos:** 8 new pairs.
- **Creatures and bosses:** 2.0 km² of islands in about 5 km² of air, 6 creatures, mostly flyers, 1 major (weak to Spore, tune at R8), 3 minis.
- **Shared systems first needed here:** flying mounts (Scorchwing is the doc's existing flyer); aerial combat; boss soul forging.
- **Art:** about 10 models (6 creatures, 4 bosses), island meshes, sky set dressing.
- **Map:** islands placed over the Wind wedge; travel up by flying or glide. Walk map doesn't cover flight, so wildlife needs a rule.
- **Data:** `tuning.json` (flight).
- **Tests:** proposed `flighttest.ps1`, `glidetest.ps1`. Extend `walktest` with the route up.
- **Depends on:** R6's non-ground creature AI, R1's Wind-side wedge.

### R8: Fungal caverns (Spore)

- **Biome content:** large underground caverns, surface entrances. Today's caves are single meshes, so big connected caverns are new.
- **Path:** Spores (planted on enemies, then detonated). Root: Immune system. Abilities: Spore cloud, Infect strike, Blight, Sporeling, Spore armor. Passives: Corrode, Burst, Host, Hive mind, Toxic blood, Adrenal. Capstones: Pandemic, Fungal behemoth, Living colony. New statuses: poison, rot. **All status effects now complete.**
- **Combos:** 9 new pairs, including opposite pair Wind + Spore.
- **Creatures and bosses:** 1.5 km² surface plus about 3 km² of caverns, 10 creatures, 1 major (weak to Wind), 3 minis.
- **Shared systems first needed here:** breeding with stat inheritance and mutations (eggs or pregnancy, raising babies). Hybrids stay out (Dream).
- **Art:** about 14 models (10 creatures, 4 bosses), fungal props, cavern meshes (Blender, as `cave_gen.py` does today).
- **Map:** cavern meshes under the Spore wedge; walk map and spawn regions for underground.
- **Data:** `creatures.json` (breeding fields), `tuning.json`.
- **Tests:** extend `terrain\cavetest.ps1`; proposed `breedtest.ps1`.
- **Depends on:** R4 (darkness), R3 (husbandry).

### R9: Crystal caves (Arcane), plus the PvP pass

- **Biome content:** crystal caves, reusing R8's cavern work.
- **Path:** Mana (steady pool). Root: Deep well. Abilities: Arcane missile, Runic strike, Barrier, Arcane blast, Rune trap, Inscribe. Passives: Reflect, Ward allies, Echo, Mana burn, Rune link. Capstones: Aegis, Starfall, Rune circle. New status: mark.
- **Combos:** 10 new pairs.
- **Creatures and bosses:** 1.0 km² surface plus about 2 km² of caves, 6 creatures, 1 major (weak to Void, tune at R10), 3 minis.
- **Shared systems first needed here:** rune sockets. PvP: clan ranks and alliances, anti-zerg limits, explosives and raid tools, siege weapons, path-based raiding, tame raiding, bounties and infamy, contested bosses.
- **Art:** about 10 models (6 creatures, 4 bosses), crystal props, siege weapons, explosives.
- **Map:** cavern meshes, Arcane wedge surface.
- **Data:** `items.json` (runes), `recipes.json` (siege), `server_presets.json`.
- **Tests:** extend `socialtest.ps1`, `buildtest.ps1`, `clantest.ps1`; proposed `siegetest.ps1`.
- **Depends on:** R8's caverns, R4's Hearth level for siege.

### R10: Corrupted wasteland (Void), plus the endgame

- **Biome content:** corrupted wasteland, using R4's corruption.
- **Path:** Corruption (huge power; each use lowers max HP until you rest). Root: Empty vessel. Abilities: Void bolt, Null strike, Void rift, Collapse, Blink, Shadow step, Consume, Sacrifice. Passives: Event horizon, Phase, Feed. Capstones: Black hole, Nowhere, Maw of the void.
- **Combos:** 11 new pairs, including opposite pair Arcane + Void. **All 66 pairs complete.**
- **Creatures and bosses:** 6.0 km², 8 creatures, 1 major (weak to Arcane), 5 minis.
- **Shared systems first needed here:** raids for 6 to 10 players, mythic gear needing materials from every biome (only possible now), prestige, leaderboards, achievements, titles and cosmetics, boss trophies as monuments.
- **Art:** about 14 models (8 creatures, 6 bosses), wasteland set, mythic gear, raid arenas.
- **Map:** last wedge opens; every pass opens.
- **Data:** `items.json` (mythic), `server_presets.json` (raid group sizes), new `Data/achievements.json` (proposed).
- **Tests:** proposed `raidbosstest.ps1`, `prestigetest.ps1`.
- **Depends on:** every earlier round.

### Final round: no new biome

1. Tune elemental weaknesses across 12 paths and balance every pair.
2. Play the build plan's Full release gate.
3. Dedicated server on Windows, load test toward 100 players with bots, long soak (M7).
4. Bundled shader cache, recorded from play (`PERFORMANCE.md` steps).
5. **Shipping build**, the first ever. Known work: the allowed-action list in `WRPlayerController.cpp` needs every new player action, or it'll silently fail; `package.ps1` writes a console section into the README either way; Shipping blocks or removes the commands the bot tests rely on, so run the full suite on Development first and play Shipping by hand.
6. Installer, code signing (unsigned today), auto-update pointed at the right place.
7. Game title (DESIGN_REVIEW #26: still "Untitled").

---

## 6. Risks

1. **The art pipeline isn't here.** It lives in `C:\Dev\WildreachArt` on another PC: about 35 GB, not in git, and that folder isn't on this desktop. The scripts in `Tools/art` are in git, but the per-creature fix scripts sit in `WildreachArt\out\` and don't. Expect about 133 new models at the Round 0 step 0.4 sizes. Rebuilding it needs disk this PC's C: drive doesn't have.
2. **Hunyuan3D licence.** The 3D models are made with Hunyuan3D 2.0, whose licence doesn't cover the EU, UK or South Korea (and allows commercial use below 1M monthly users). Every release note says so. A full public release is limited that way for as long as the art comes from it. Aaron's own licence notes of 10-16 list TripoSG and Hi3DGen as MIT alternatives; re-check before use. See question 2.
3. **LFS on Raven's account.** About 755 MB in the current tree, and every clone and pull counts. The docs put a creature at about 11 MB, a Mixamo character at 30 to 40 MB, an island rebuild at about 80 MB. The new art alone is well over 1 GB, plus the bigger map. The allowance itself is not stated in the docs. See question 1.
4. **The map can't be merged and must hold 12 biomes.** `L_Island` and its files can't be merged, the landscape only rebuilds in the full editor, and the terrain tools assume today's size. Getting the layout wrong after several biomes are built is the most expensive mistake available. Hence one plan in Round 0 and one build in R1.
5. **Fab pack licences.** README: their licence doesn't allow sharing them publicly, so they must never reach git, least of all a public repo. Each new pack needs its licence checked and a row in `CREDITS.md`. A "complete project" pack can overwrite `Config\` (HANDOFF trap).
6. **Dedicated server needs a source-built engine.** About 200 GB. No drive here has that free (C: 38, D: 86, T: 129). The built copy is on Raven's PC only. The design calls for 10 to 100 players and nothing is tested past a few.
7. **Design questions now become live.** The open items in `DESIGN_REVIEW.md` were parked for this stage, and none has a default listed. The list below gives a recommended answer. All are one decision for Aaron: question 8.

   | # | Issue | Round | Recommended |
   |---|---|---|---|
   | 13 | "Rest" undefined | R4 | Rest means sleeping in a bed or bedroll |
   | 17 | "Potions only" healing against heal spells | R2 | Keep heal spells; potions-only applies to anyone without a healing path |
   | 18 | Hybrid mastery rule | Dream | Leave parked |
   | 19 | Wheel order | R1 | Use the order `combos.md` implies |
   | 20 | Portal timers "ruled out" but real-day breaks exist | R5 | Keep break timers; the ruled-out line means timed runs |
   | 21 | Traps in dungeons | R5 | Yes in dungeons, still none in bases |
   | 22 | Raids 6 to 10 with no party limits | R10 | Tuned for that size, not capped |
   | 23, 24, 25 | Stale line, broken mention, name overlaps | Any | Fix in data as each path arrives |
   | New | Build plan lists "arena PvP setting", design doc rules out PvP arenas | R9 | Means the existing "PvP inside boss arenas" setting. Already built |
   | New | Combos are "automatic, cost no points", yet build plan says "combo abilities" and #5 says some spend both bars | R1 | All passive unless a round's design says otherwise |

8. **The design docs are frozen copies.** The repo's copies date from 2026-11-08, and the live versions open only from Raven's account. From here the repo copies are the baseline and any edits are Aaron's.
9. **Size.** 10 biomes, 130 models, 120 or so new abilities, and every system in the table is a very long build for one desktop. The full suite and the map's memory use will keep growing. Quality could thin out late. Round 0's bars exist to stop that.
10. **The GPU hang.** Unsolved, happens under load, and bigger worlds mean more load.

---

## 7. Questions for Aaron

**1. Where does the project live, and whose rules apply?**
The repo is Raven's and public. Its `CLAUDE.md` says never publish a release without Raven's OK, ask Raven before anything big, and don't build the other 10 biomes unless Raven asks. Any build session reading it will stop at those lines. Installed copies auto-update from that repo (`WildreachUpdater.cs`, `WRMainMenu.cpp`), so a release there reaches everyone who has the game. LFS growth lands on Raven's account.
**Decided 11-10:** keep working in Raven's repo. We're an approved author: cook exactly as Raven wrote it, then publish to his repo. Raven's `CLAUDE.md` rules stand.

**2. Which art pipeline makes the new art, and what happens to the existing models?**
Recommended: build new art with a pipeline that has no territory limit, set up off C:. Decide at the Final round whether to remake existing Hunyuan-made models or accept the limit for release.

**3. Is one wipe of existing worlds acceptable when the 12-biome landmass arrives?**
**Decided 11-10:** no wipe for now. Raven decides.

**4. Should players get a build every round, or only when a pair is complete?**
**Decided 11-10:** Raven's call. After our cook, it goes back to him for the next step.

**5. Must the First Playable gate pass before R1 starts?**
**Decided 11-10:** no, keep building while they play. Judge pass/fail against the quality of shipped games.

**6. Are all nine Dream list items out?**
**Decided 11-10:** cook what's in Raven's plan as written (these nine sit in its Dream tier); anything we invented goes to the next planning doc for Raven's approval.

**7. When should the dedicated server get built, given the 200 GB it needs?**
Recommended: prove it during R5, before the world grows further, rather than leaving it to the end. The 4 TB drive arriving 11-10 has room for it, though a hard drive will make that engine build slow.

**8. Accept the recommended answers in the design-question table (section 6, item 7)?**
**Decided 11-10:** these go to Raven by email for his answer.

**9. How big is each new biome?**
**Decided 11-10 (later the same day, superseding "all the same size"):** regions may vary in size. Each keeps 1 major and 3 to 5 minis; area and creature count per biome are in Round 0 step 0.4.
