# Wildreach: the full game, imagined

Written 2026-11-13 for the full cook. This is the finished game as a player meets it, from the main menu to endgame, built only from Raven's design docs (copies of 2026-11-08 in `C:\Dev\Wildreach\Docs\design\`), his plans (`Docs\plan\`, `TECH_PLAN.md`, `DESIGN_REVIEW.md`, `HANDOFF.md`, `terrain\LAYOUT_PROPOSAL.md`), what is in the repo at v0.7.1 (commit 372557e), and Aaron's 11-10 and 11-13 decisions. Raven owns the game; his docs are the source of truth.

## How to read the tags

Every item carries one tag:

| Tag | Meaning |
|---|---|
| BUILT | In the repo now. The cite is a file in `C:\Dev\Wildreach`. |
| RAVEN-PLANNED | In Raven's docs but not built. The cite is the doc and section. |
| GAP | Raven's docs say nothing. Filled with the best experience for Wildreach, decided and built. Where the fill came from our plans (Aaron's 11-10 decisions), the cite says so. |

**Source key**

| Short | File |
|---|---|
| DD | `Docs/design/design-doc.md` (Raven, 10-21, edited 10-22) |
| BP | `Docs/design/build-plan.md` (Raven, 10-22; "Where it stands" 11-08) |
| ST / CB | `Docs/design/skill-trees.md` / `combos.md` |
| CR / BO | `Docs/design/creatures.md` / `bosses.md` |
| CF / BL | `Docs/design/crafting.md` / `building.md` |
| LS / SS / SN / MP | `life-skills.md` / `server-settings.md` / `starting-numbers.md` / `map.md` |
| M1 to M7 | `Docs/plan/M1_combat.md` ... `M7_server.md` |
| DR | `Docs/DESIGN_REVIEW.md` (Raven's decisions of 10-22 win over the doc tabs) |
| LP | `Docs/terrain/LAYOUT_PROPOSAL.md` (approved by Raven 10-23) |
| FRP / CAL | our `FULL-RELEASE-PLAN.md` / `COMBAT-AND-LESSONS.md` (Aaron's 11-10 decisions) |

**How every GAP is filled (Aaron 11-13):** where Raven gives a spec, it is written exactly to spec. Every GAP is the best experience we can build, drawn from proven games (Elden Ring, Monster Hunter, Valheim, Breath of the Wild, Palworld, Enshrouded, V Rising, Conan Exiles, Rust) and our hard lessons: the full hit stack, real weapons and claws that hit only on visible contact, projectiles with at least 0.9 s warning that never home, no attacker limit, players die with no safety floors, terrifying monsters with readable silhouettes, real animation clips, unique creature voices, real composed music that is always audible, a POI every 150 to 250 m, story through the environment only, fast fun movement, full-body first person, mouse and pad on every screen, attractive real-world characters. The shared numbers every GAP builds to are in section 3.3. GAP labels stay so Raven can review each one.

**Precedence used everywhere:** DR beats the doc tabs (DR's own rule). Raven's newer doc beats his older one. Aaron's 11-10 decisions win on scope: all 12 biomes, Elden-Ring-style combat, weapon skills named Wildbrands, no attacker limit, regions vary in size (about 49 km² in total). Aaron's 11-13 decision wins on the hero: a MetaHuman male and female; the Mixamo survivor is scrapped. Nothing here overrides a choice Raven already made. All names and numbers stay first drafts in `Data/*.json`, as Raven's CLAUDE.md asks.

---

## 1. The fantasy and the core loop

**The fantasy.** You wash into a wild, persistent world split into twelve elemental lands. Each land teaches one power. You pick a home power, then cross the map to beat a rival land's guardian and take its power too. Your body shows what you've become. You tame its beasts, build a fort, and fight Elden-Ring-hard bosses with friends, while other clans can come for your base.

| Pillar | Tag | Source |
|---|---|---|
| Biomes are paths, not a ladder. No starter zone, no endgame zone | RAVEN-PLANNED | DD Overview, pillars |
| Main path plus a subclass from another biome pulls you across the map | BUILT (Fire/Frost) | `Source/Wildreach/WRPathComponent.cpp`, `WRAltar.cpp` |
| Soulslike combat: stamina, dodges, parries, learnable bosses | BUILT (first version) | `WRCombatLibrary.cpp`, `WRAbilities.cpp`, `WRBoss.cpp` |
| Combat copies Elden Ring in detail | GAP (Aaron 11-10, CAL Part 1; Raven's DD says "Elden Ring style combat") | CAL |
| Survival sandbox: Ark-style taming, farming, machinery, base building | BUILT in part (taming, basic farming, building); machinery RAVEN-PLANNED | `WRCreature.cpp`, `WRBuilding.cpp`; DD Crafting, machinery |
| Mid-to-late game first, no tedious grind | RAVEN-PLANNED | DD pillars |
| Persistent servers of 10 to 100, no wipes, handcrafted map | RAVEN-PLANNED (host PC is the server today) | DD Overview; M7 |
| Zombies are out | RAVEN-PLANNED (rule) | DD Overview, Ruled out |

**The core loop, minute to minute:** explore, gather, fight, tame. **Hour to hour:** craft at your Hearth's stations, build, push deeper into a biome, beat a mini-boss for upgrade shards. **Week to week:** beat a major boss, feed its core to your Hearth for the next tech level, unlock a subclass at its altar, reach the 25/25 and 50/50 combos, run portals, then artifact-hard bosses and raids.

| Loop step | Tag | Source |
|---|---|---|
| Gather from trees, rocks, shrubs, ore nodes; carcasses butchered hit by hit | BUILT | `WRWorldDressing.cpp`, `WRResourceNode.cpp`, `WRCarcass.cpp` |
| Craft at four stations whose tier the Hearth sets | BUILT | `WRCrafting.cpp`, `Data/recipes.json` |
| Beat major bosses for cores, which raise the Hearth's tech level | BUILT (levels 1 to 3) | `WRBoss.cpp`, `WRCrafting.cpp`; CF Tech levels |
| Unlock the subclass at the other biome's boss altar | BUILT | `WRAltar.cpp` |
| Combos at 25/25 and 50/50 | BUILT (Fire+Frost) | `WRCombatLibrary.cpp` |
| Long-term goal after two maxed paths: portals, artifacts, raids, mythic gear, prestige | RAVEN-PLANNED | BP Full release gate; DD Endgame |

---

## 2. The first hour

**What it feels like.** You type a name, pick a male or female body, and pick a main path. That choice sets where you wake: pick Fire and you open your eyes on black ash at one of the volcano's three coastal camps, a safe zone. Bare hands pull fiber from shrubs and stones from the ground. Ten minutes in you've knapped a crude hatchet and pick, felled a tree, and built a workbench and a lean-to. Pumicehogs hop past; a Cinderfang pack watches from the ridge line. You eat berries, fill a waterskin at the steaming river, and plant your claim totem. As the sun goes down the ash glows from below and the Cinderfangs come in. You learn to dodge late, parry the lunge, and drink a potion only when the pack backs off. By the end of the hour you've tamed an Ashstrider with food, ridden it toward the north pass, and seen Vulkarn's crater smoking above the midlands: the first thing you'll need friends and frost for.

| Beat | Tag | Source |
|---|---|---|
| Main menu: name, body (male/female), main path (Fire or Frost), Host or Join by IP, worlds list | BUILT | `WRMainMenu.cpp`, `WRUIWorlds.cpp` |
| Body is the new MetaHuman male or female, shaped in a short creator (face, body, hair, skin) | GAP (Aaron 11-13; M2 says character creation has "Name, body, main path") | M2 Systems; Aaron 11-13 |
| Path picker shows all 12 paths once their biomes exist | GAP (FRP R1) | FRP R1 |
| Main path sets your spawn biome; one of three spawn points on its outer coast | BUILT | `WRGameMode.cpp`; DD How paths work 1; MP |
| Spawn camps are safe zones | BUILT | `WRSocial.cpp`; MP Outer lowlands |
| Gather bare-handed, crude tools by hand, stone tools at the workbench | BUILT | `Data/recipes.json` (station `hand`), `WRHeldTool.cpp` |
| Hunger and thirst, light pressure (empty in 60 / 40 min) | BUILT | `WRAttributeSet.cpp`, `Data/tuning.json` `survival`; SN |
| Day and night; creatures sleep or hunt by activity | BUILT | `WRSky.cpp`, `WRCreature.cpp` |
| Easy creatures near the coast, harder inland | BUILT | `Data/spawn_regions.json` |
| Claim totem, Hearth, bedroll as respawn | BUILT | `WRBuilding.cpp`, `WRBeds.cpp` |
| Death: a map of where to wake, 10 s wait | BUILT | `WRFlowDeath.cpp` |
| Hints and "moments" (discoveries, level-ups, boss cards) | BUILT | `WRUIFlow.cpp` |
| Lore told only through items, ruins and places, never cutscenes | RAVEN-PLANNED | DD World, lore & events |
| A first-run note in the world at each camp (a Souls-style message from "the last one here") pointing at the bedroll and the workbench | GAP | DD "player messages" exists as a system; this use is ours |

---

## 3. The world

### 3.1 The whole map

| Item | Tag | Source |
|---|---|---|
| Today: a 4 × 4 km island, about 9.9 km² of land, Volcanic west and Frozen tundra east, split by an unclimbable ridge with exactly two passes | BUILT | `Content/Maps/L_Island`, `Data/island_walk.json`; LP |
| Zones per biome: outer lowlands (spawns, easy), midlands (minis, caves, ore), highlands (major boss, hardest creatures) | BUILT | `Data/spawn_regions.json`; MP |
| Passes are the contested borderland: both biomes' creatures fight there, invasions come through, PvP hotspot | BUILT in part (passes exist; invasions not) | MP; LP |
| The map must grow to fit all biomes | RAVEN-PLANNED | LP decision 6, Next steps 4; M3 |
| Full map: one landmass, 12 wedges round a central massif, in wheel order so opposites sit straight across; Fire stays west, Frost east | GAP (FRP 0.4) | FRP 0.4 |
| Wheel order: Fire, Lightning, Sun & sand, Nature, Wind, Arcane, Frost, Blood & curse, Tide, Necromancy, Spore, Void | GAP (read from CB's pair table; DR #19 says it isn't written down; emailed to Raven 11-10) | CB; DR #19 |
| Sizes vary by biome, about 49 km² of surface plus caverns, sea and air | GAP (Aaron 11-10, CAL Part 3) | CAL Part 3 |
| **The central massif keeps Raven's pass idea:** it is the hub, cut by six passes, each running straight across between an opposite pair. Each pass is that pair's borderland, invasion route and PvP hotspot, as the north and south passes are today | GAP | builds on MP "North and south passes" |
| Unbuilt wedges stay closed (their passes sealed by rockfall) until their round | GAP (FRP 0.4) | FRP 0.4 |
| Every biome has a signature resource found nowhere else | RAVEN-PLANNED | DD Every biome has |
| Day and night spawn tables per biome | RAVEN-PLANNED | DD Every biome has; BP Full release Biomes |
| Hidden sub-biomes (example: lava tubes under the volcano) | RAVEN-PLANNED | DD Every biome has |
| Shrines that give path XP when cleansed or claimed; fast travel between discovered shrines | RAVEN-PLANNED | DD Every biome has; DD World |
| A path master NPC per biome who teaches the path through quests | RAVEN-PLANNED | DD Every biome has |
| Biome events (eruptions, a blood moon over the graveyard) | RAVEN-PLANNED | DD Every biome has |
| Hazards are gear checks; no temperature meter | BUILT (lava, slick ice) | `WRWorldDressing.cpp`; DD Every biome has |
| Every biome has caves; deeper caves hold better metal; no "metal biome" | BUILT (six caves on the island) | `Data/island_dressing.json`; CF Rules |
| Ore by depth: copper, tin, iron today; steel and starsteel later | BUILT (to iron) / RAVEN-PLANNED (steel, starsteel) | `Data/items.json`; CF Tech levels |
| Mob difficulty: easy near spawn, brutal deeper in | BUILT | `Data/spawn_regions.json`; DD How combat works |
| Living world: herds defend each other, predators hunt prey with nobody near | BUILT | `WRWildlife.cpp`, `WRCreature.cpp`; DD Creatures |
| Weather that matters: storms, blizzards, heat waves | RAVEN-PLANNED | DD Survival; BP Full release Survival |
| Seasons that change biomes (frozen lakes in winter) | RAVEN-PLANNED (Dream tier) | BP Dream list |
| NPC villages and factions per biome; traveling merchants | RAVEN-PLANNED | DD World |
| A fill-in map you complete by exploring | RAVEN-PLANNED (the map screen and waypoints are built) | DD World; `WRUIMap.cpp` |
| Player messages left in the world | RAVEN-PLANNED | DD World |
| World events (meteor showers dropping rare ore) | RAVEN-PLANNED | DD World |
| Opposite-biome invasions: fire/frost, lightning/blood, sun/tide, nature/necromancy, wind/spore, arcane/void | RAVEN-PLANNED | DD World |
| Tone per biome: dark fantasy in the graveyard, brighter magic in the sky islands | RAVEN-PLANNED | DD World |
| Density rule: a landmark or encounter every 150 to 250 m; 3 to 5 sub-zones per biome | GAP (Aaron 11-10, CAL Part 3) | CAL Part 3 |
| Every biome has 1 major boss and 3 to 5 mini-bosses; roaming minis wander the biome | RAVEN-PLANNED | DD Bosses; BP Full release |

**Shared rules every biome below follows (from Raven):** creatures resist their own biome's hazard (CR); every creature is tameable by its listed method only (DR #16); neutral herds defend each other (CR); the major boss is weak to its opposite element, has 2 phases plus a hidden third with artifacts, and its arena holds the biome's altar (BO, DD How paths work); the major drops a boss core, path materials and a chance at a boss part; minis drop upgrade shards and a trophy (BO).

**Invasions:** how they play, with numbers, is in section 3.3.

### 3.2 The twelve biomes, in map order

Each biome lists: look, sub-zones, signature resource, hazard, hidden sub-biome, event, creatures, minis, major boss, and music. Sizes come from CAL Part 3.

---

#### 3.2.1 Volcanic (Fire). West. 5.0 km², built.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Look | Black basalt, ash plains, glowing lava rivers and crater; dead and charred trees | BUILT | `Data/island_dressing.json`, `M_Lava`, `WRSky.cpp` (ash air) |
| Sub-zones | Ash plains, lava fields, obsidian ridges, vents and geysers, lava-tube mouths | GAP (CAL Part 3) | CAL |
| Rivers | One water river (steaming, drinkable) north, one lava river south | BUILT | LP decision 5 |
| Signature resource | Sulfur crystals (also Basaltback taming food) | BUILT | `Data/items.json`; DR #12 |
| Hazard | Lava: damage unless Burning blood, fire gear or a volcanic mount | BUILT | `WRWorldDressing.cpp`; M3 Hazards |
| Hidden sub-biome | Lava tubes under the volcano | RAVEN-PLANNED | DD Every biome has |
| Event | Eruptions: ash fall, lava bombs, new sulfur and ore spilling out | RAVEN-PLANNED (event named) / GAP (what it does) | DD Every biome has |
| Shrine, path master | Shrine of the Forge in the ash plains; path master: an old smith who teaches Fire by forge quests | RAVEN-PLANNED (exist) / GAP (who, where) | DD Every biome has |
| Ambient sound | Lava, wind, day and night beds | BUILT | `Data/Audio/sounds.json` |
| Music mood | Low drums and brass, slow and heavy; the forge rhythm under it | GAP | DD says each biome has its own music |

**Creatures (9):**

| Creature | Behavior / role | What it is | Taming | Tag |
|---|---|---|---|---|
| Magmaw | Aggressive fighter | Lizard that swims through lava and bursts out of pools | Knockout and feed, or fire path magic. Hard | BUILT (`Data/creatures.json`) |
| Cinderfang | Aggressive fighter | Six-legged ember hounds in packs | Knockout and feed. Medium | BUILT |
| Scorchwing | Aggressive fighter, scout | Bat-winged fire flyer that dives | Knockout and feed after bringing it down. Medium | BUILT |
| Basaltback | Neutral hauler | Huge rock-shelled tortoise beast | Offer sulfur crystals. Medium | BUILT |
| Emberhorn | Neutral mount | Charging beast with a glowing horn | Knockout and feed. Medium | BUILT |
| Kilnmite | Neutral utility | Burrowing beetle that mines ore and senses veins; nest swarms | Steal an egg. Easy | BUILT |
| Ashstrider | Passive mount | Tall long-legged grazer, fastest ground mount | Offer food. Easy | BUILT |
| Glowmoth | Passive utility | Giant moth that lights caves | Offer nectar. Easy | BUILT |
| Pumicehog | Passive farm animal | Small hopping critter for meat and fat | Offer food. Easy | BUILT |

**Mini-bosses (3):**

| Mini-boss | Fight | Counter | Trophy | Tag |
|---|---|---|---|---|
| The Obsidian Warden (golem) | Armored brute, ground pounds, charges | Break the shell to the core; chilling the shell cracks it faster | Obsidian heart | BUILT (`WRBoss.cpp`) |
| Ignessa, Queen of Wisps | Flies out of reach, sends exploding wisps | Destroy her wisps; she lands to make more | Wisp crown | BUILT |
| Cinderhorn, the Rampaging Bull (roaming) | An Emberhorn bull the size of a house that roams the ash plains and charges through camps; each charge leaves a burning trail | Bait the charge into basalt pillars or walls: a wall hit stuns him for a long punish; frost on his horn stops the trail | Cinder horn | GAP (top-up to 3 minis: RAVEN-PLANNED, BP Full release; this mini is ours) |

**Major boss: Vulkarn, the Forge Giant** (crater forge cavern). Molten giant blacksmith. Phase 1: three-hit hammer combo, overhead slam leaving lava cracks, tongs grab (parry staggers him), anvil slam floods the floor (stand on the raised anvils). Phase 2 under 50%: white-hot armor, bellows breath cone, thrown slag, floods more often, anvils melt. Hidden phase 3: quenched rage, dual hammers, every slam floods. Weak to frost. Drops: boss core, fire path materials, Giant's ember (staggering sends a lava shockwave). **BUILT** (`WRBoss.cpp`, `Data/creatures.json`; BO).

---

#### 3.2.2 Storm peaks (Lightning). North-west. 5.5 km².

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Chain damage, burst speed | RAVEN-PLANNED | DD Biomes & paths |
| Look | Jagged granite peaks over slate cliffs, thin wind-bent pines, iron-dark storm clouds that never clear the summits; glassy scars where lightning hit | GAP | |
| Sub-zones | Cliff paths, wind passes, lightning-struck crags, rope bridges, cliff shrines | GAP (CAL Part 3) | CAL |
| Signature resource | Stormglass: fulgurite tubes fused where lightning strikes the ground, only after a storm | GAP | |
| Hazard | Strike zones: in a storm, anything metal on the highest ground draws strikes (damage plus stun). Grounded (root passive) or a grounding charm (armorer) passes. Gusts on cliff paths knock you off unless braced | GAP (Grounded's "immune to lightning stuns, including from enemy storms" is RAVEN-PLANNED, ST Lightning) | |
| Hidden sub-biome | The Hollow Spire: a peak hollowed by strikes, its insides lined with stormglass and copper veins | GAP | |
| Event | The Great Storm: a server-wide-visible storm that rolls over the peaks; strikes everywhere, stormglass appears where they land | GAP | |
| Shrine, path master | Shrine on a lightning-struck crag; path master: a scarred climber who teaches Lightning by summit quests | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Taut strings and timpani, sudden stops, thunder used as percussion | GAP | |

**Creatures (8):** all GAP, taming methods from Raven's four.

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Voltfang | Aggressive fighter (pack) | Lean cliff wolves whose raised fur crackles; they shock on the bite and flank on ledges | Knockout and feed. Medium |
| Thunderhorn | Aggressive fighter | Ram-like beast whose head-butt lands with a thunderclap; knocks you off ledges | Knockout and feed, or lightning path magic. Hard |
| Stormkite | Aggressive fighter, scout | Long-tailed raptor that rides storm fronts and dives with a charged strike | Knockout and feed (bring it down). Medium |
| Cragstrider | Neutral mount | Six-hoofed goat that climbs near-vertical rock; the peaks' route-finder | Knockout and feed. Medium |
| Galeback | Neutral hauler | Shaggy, low-built yak-beast that stands into the wind | Offer stormglass. Medium |
| Coilburrow | Neutral utility | Burrowing mole whose whiskers feel metal through rock; senses ore | Steal an egg. Easy |
| Rodcrest | Passive utility | Lizard with a copper-bright crest that draws lightning to itself; a tamed one at base grounds strikes near it | Offer food. Easy |
| Puffram | Passive farm animal | Small cloud-woolled sheep; wool and meat | Offer food. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| The Bellwether (roaming) | A giant Thunderhorn that roams the ridgelines; charges along narrow paths and slams to knock players off | Sidestep a charge into rock: it stuns on impact. Bleed cuts its charge regen | Bellwether horn |
| Galvara, the Kite Queen | Rides inside a storm cloud, raining strikes and sending Stormkites | Shoot the cloud's core with arrows or spells to thin it; when it breaks she drops to the crag | Kite crown |
| Coilback | A huge burrower that electrifies the ground in rings before surfacing | Jump the ground arcs (leg i-frames); frost slows its tunnelling so its surfacing spot shows | Coil heart |

**Major boss: Kharvox, the Storm Roc** (GAP). A vast bird whose feathers are charged; it nests on the highest summit, ringed by old copper lightning rods. Phase 1: wing sweeps, talon dives, chain lightning between the rods. Phase 2: the storm thickens, safe ground shrinks to the rods' shadow, it hovers and calls strikes. Hidden phase 3: the eye of the storm, constant strikes, it never lands except to grab. **Weak to Blood & curse** (opposite, RAVEN-PLANNED rule): bleed stops it rebuilding charge, and a curse grounds it. Boss part: Stormheart (every fifth hit chains to a second enemy).

---

#### 3.2.3 Desert (Sun & sand). North-north-west. 6.5 km².

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Blinding light, sandstorms, mirage decoys | RAVEN-PLANNED | DD Biomes & paths |
| Look | Gold dunes against cracked white hardpan, red rock spires, half-buried temples, a permanent wall of sandstorm on one edge | GAP | |
| Sub-zones | Oases, ruins, dunes (soft sand) vs hardpan, canyons, wrecks (caravans and sunken-ship hulls), rock spires, the sandstorm belt | GAP (Aaron 11-10, CAL Part 3: "desert especially") | CAL |
| Signature resource | Sunstone: amber crystal that stores daylight and glows at night | GAP | |
| Hazard | Sandstorms (blind and slow) and soft dunes (slow, quicksand pockets). Sandwalker (root) or sand veil and wraps (armorer) pass. No heat meter | GAP (Sandwalker is RAVEN-PLANNED, ST Sun & sand) | |
| Hidden sub-biome | Buried ruins: a sunken temple city under the dunes | GAP (CAL "buried ruins") | |
| Event | The Long Noon: the sun holds still for a game day; Radiance never fades, sunstones bloom, and the Colossus walks | GAP | |
| Shrine, path master | Shrine in a ruined sun-temple; path master: a veiled caravan guide | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Hand drums, oud-like plucked strings, long held drones; wide and empty | GAP | |

**Creatures (7), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Dunejaw | Aggressive fighter | Sand-swimming shark-lizard; a fin in the sand, then a lunge | Knockout and feed, or sun path magic. Hard |
| Glasstail | Aggressive fighter | Scorpion with a sun-glass stinger that blinds on a hit | Knockout and feed. Medium |
| Carrion kite | Aggressive scout | Vulture-like flyer that circles the wounded and calls others | Knockout and feed (bring it down). Medium |
| Dromad | Neutral hauler | Long-legged, hump-backed pack beast; carries the most in sand | Offer cactus fruit. Medium |
| Shimmerbuck | Neutral mount | Fast antelope that leaves a mirage of itself when spooked | Knockout and feed. Medium |
| Scarab digger | Passive utility | Giant scarab that digs up buried ruin loot; senses hidden rooms | Steal an egg. Easy |
| Sandhen | Passive farm animal | Ground bird for eggs and meat | Offer food. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| Ssarrok, the Dune Leviathan (roaming sand-beast) | A Dunejaw the length of a ship that roams the dunes and swallows whole | Lure it onto hardpan: it can't dive there and flounders. Tide soaks dry it out | Leviathan tooth |
| The Mirage Matriarch | A Shimmerbuck doe that splits into four mirages | Only the real one casts a shadow; blind breaks all mirages | Mirage antler |
| The Glass Empress | Giant Glasstail in a canyon, armored in fused glass | Shatter the glass with heavy or blunt hits; frost then fire cracks it | Empress stinger |

**Major boss: Ahrumet, the Sun Colossus** (GAP). A stone god-statue in a sundial plaza that wakes when the sun crosses its gnomon. Phase 1: sweeping beam from its eye, stomps, sand waves. Phase 2: it tears the gnomon free as a club; standing in the gnomon's moving shadow blocks the beam. Hidden phase 3: noon lock, the beam never stops, the shadow is the only safe ground. **Weak to Tide** (opposite): soaked stone cracks. Boss part: Colossus eye (critical hits blind the target).

---

#### 3.2.4 Ancient forest (Nature). North. 7.0 km², the richest biome.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Vines, healing, plant summons | RAVEN-PLANNED | DD Biomes & paths |
| Look | Old-growth giants with canopy walkways, moss on everything, shafts of green light, meadows and farmland at the edges | GAP | |
| Sub-zones | Old-growth groves, meadows and farmland, villages, streams and falls, canopy paths | GAP (CAL Part 3) | CAL |
| Signature resource | Heartwood: amber-cored wood from fallen giants, the forest's building and bow material | GAP | |
| Hazard | Thornbrakes: walls of bramble that cut and bleed anyone pushing through. Nature path or briarproof leathers pass; fire clears them for a while | GAP | |
| Hidden sub-biome | The Hollow Root: a cave world inside the roots of the oldest tree | GAP | |
| Event | The Bloom: every crop and wild plant in the forest grows double for a day; rare seeds drop | GAP | |
| Villages | The main NPC village and traveling merchants start here | RAVEN-PLANNED (villages exist) / GAP (here first, FRP R3) | DD World |
| Shrine, path master | Shrine in a grove circle; path master: a herbalist hermit | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Woodwinds and harp, warm and slow; birdsong woven in | GAP | |

**Creatures (12), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Mossmaul | Aggressive fighter | Moss-backed bear the size of a cart | Knockout and feed, or nature path magic. Hard |
| Thornlynx | Aggressive fighter | Ambush cat that waits in canopy and drops | Knockout and feed. Medium |
| Rootcoil | Aggressive fighter | Constrictor snake as thick as a tree | Knockout and feed. Hard |
| Bramblewasp | Aggressive fighter | Fist-sized wasps from paper nests; swarms when the nest is hit | Steal an egg (larva). Medium |
| Elderelk | Neutral mount | Elk whose antlers carry living leaves; fast through trees | Knockout and feed. Medium |
| Burlbeast | Neutral hauler | Slow, huge ox with bark-plated back | Offer heartwood shavings. Medium |
| Truffler | Neutral utility | Boar that roots out buried seeds, truffles and secrets | Offer food. Easy |
| Hiveback | Neutral utility | Giant bumblebee; tamed ones make honey and wax (beekeeping) | Steal an egg. Medium |
| Fernsheep | Passive farm animal | Wool and milk | Offer food. Easy |
| Dappledoe | Passive farm animal | Deer for meat and hide | Offer food. Easy |
| Glimmerfinch | Passive utility | Tiny bird that pollinates; crops near it grow faster | Offer seeds. Easy |
| Treeglider | Passive utility | Flying squirrel that scouts the canopy and finds nests | Offer food. Easy |

**Mini-bosses (4), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| Old Gnarl | An awakened tree; slams roots, throws boulders, regrows bark | Axes strip its bark; fire on stripped wood burns it out | Gnarl heart |
| The Bramblequeen | Wasp queen in a hive the size of a house | Break the combs first; without them she can't call swarms | Queen's sting |
| Grey Matron | A Thornlynx that only attacks from the canopy | Cut her walkways; on the ground she's slow | Matron claw |
| The Hornwarden (roaming) | A giant Elderelk that guards the herds and roams the meadows | Never attack his herd: if a herd member is hurt he enrages. Root snares hold him | Warden antler |

**Major boss: Sylvaroth, the World-Root** (GAP). A walking tree-titan in a clearing ringed by four heart-trees that heal it. Phase 1: root eruptions, branch sweeps, seed bombs. Phase 2: it pulls itself out of the ground and walks; vines snare. Hidden phase 3: every heart-tree wakes and fights. **Weak to Necromancy** (opposite): necrotic damage stops its roots healing. Boss part: Verdant core (healing potions also heal your tames nearby).

---

#### 3.2.5 Sky islands (Wind). North-north-east. 2.0 km² of islands in about 5 km² of air.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Gliding, knockback, aerial combat; brighter magic in tone | RAVEN-PLANNED | DD Biomes & paths; DD World |
| Look | Floating rock islands trailing waterfalls into cloud, white stone ruins, wind-sculpted trees, bright sky | GAP | |
| Sub-zones | Windward cliffs, cloud gardens, ruined sky-bridges, floating shrines | GAP (CAL Part 3) | CAL |
| How you get up | Updraft columns from the Wind wedge's cliffs, gliding, then flying mounts | GAP | |
| Signature resource | Levistone: stone that floats; makes gliders and lighter armor | GAP | |
| Hazard | Falls and gales: gusts push you off edges. Featherfall (root) or a glider and anchor boots pass | GAP (Featherfall RAVEN-PLANNED, ST Wind) | |
| Hidden sub-biome | The Undercloud: islands hidden inside a permanent cloud bank | GAP | |
| Event | The Turning Wind: the islands drift and new bridges of cloud form for a day | GAP | |
| Shrine, path master | Floating shrine; path master: a one-winged glider-maker | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Flutes, choir and open fifths; light and soaring | GAP | |

**Creatures (6), mostly flyers, all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Galeharrier | Aggressive fighter | Hawk-like raptor that knocks you off ledges | Knockout and feed (bring it down). Medium |
| Skyray | Aggressive fighter | Drifting manta that sucks prey off edges with a gust | Knockout and feed, or wind path magic. Hard |
| Windmane | Neutral flying mount | Four-winged sky-stag; the first flying mount | Steal an egg. Hard |
| Cloudback | Neutral hauler | Small floating whale that carries cargo between islands | Offer levistone moss. Medium |
| Mistlark | Passive utility | Bird whose flight shows updrafts and wind currents | Offer seeds. Easy |
| Puffwing | Passive farm animal | Ground bird with glider-grade feathers | Offer food. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| Kyrra, the Tempest Harrier (roaming) | A giant Galeharrier that hunts the bridges | Fight her on wide islands; arrows to the wings ground her | Harrier plume |
| The Bridge Sentinel | A stone guardian that walks the ruined sky-bridge and knocks sections loose | Lure its slam onto weak sections: it falls through to a lower island | Sentinel eye |
| Old Gulper | A Cloudback grown huge and mean that inhales players | Feed it a lit explosive or a fire spell while it inhales | Gulper's pearl |

**Major boss: Aeolith, the Sky Serpent** (GAP). A winged serpent coiled round a ring of drifting platforms. Phase 1: tail sweeps, gale breath, dives across platforms. Phase 2: platforms drift apart, it pulls players into the air. Hidden phase 3: it flies only, and you fight on its back. **Weak to Spore** (opposite): spores weigh its wings and bring it down. Boss part: Gale plume (air dodges push nearby enemies back).

---

#### 3.2.6 Crystal caves (Arcane). North-east. 1.0 km² of surface, about 2 km² of caves.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Shields, reflected projectiles | RAVEN-PLANNED | DD Biomes & paths |
| Look | Cliff mouths opening into geode halls of violet and blue crystal that hum and light the dark | GAP | |
| Sub-zones | Geode halls, ward chambers, crystal forests, echo tunnels | GAP (CAL Part 3) | CAL |
| Signature resource | Resonant crystal: rings when struck; runes and wards are made from it | GAP | |
| Hazard | Ward fields: old arcane barriers that burn and throw back anyone unattuned. Arcane path or a ward charm passes. Also full dark outside the crystal light | GAP | |
| Hidden sub-biome | The Silent Vault: a chamber where sound and spells die | GAP | |
| Event | The Resonance: the caves ring; every crystal regrows and ward fields shift | GAP | |
| Shrine, path master | Shrine inside a ward chamber; path master: a blind rune-carver | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Glass harmonica, bells and celesta, echoing and precise | GAP | |

**Creatures (6), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Prismback | Aggressive fighter | Crystal-plated lizard whose plates reflect projectiles | Knockout and feed, or arcane path magic. Hard |
| Echobat | Aggressive fighter | Pale flyer that hunts by sound (sneaking matters) | Knockout and feed (bring it down). Medium |
| Geodemite | Aggressive fighter (swarm) | Crystal mites that boil out of geodes | Steal an egg. Easy |
| Lumenhorn | Neutral mount | Cave stag with light-giving crystal antlers | Knockout and feed. Medium |
| Gemsniffer | Neutral utility | Long-snouted burrower that senses gems | Offer crystal dust. Easy |
| Chimecrab | Passive farm animal | Crab that sheds crystal dust and meat | Offer food. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| The Mirrored Twins | Two crystal knights that copy each other | Only the one casting a reflection in the floor takes damage | Twin shard |
| The Echo Warden | A construct that attacks wherever sound is | Sneak; throw stones to pull it | Warden's tuning fork |
| The Geode Tyrant (roaming) | A Prismback the size of a house that roams the halls | Its plates reflect spells; melee and void damage pass | Tyrant plate |

**Major boss: Lyrathis, the Prism Sovereign** (GAP). A crystal giantess in a hall of prisms that split her beam. Phase 1: prism beams, shard rain, a dome shield. Phase 2: she splits into light copies. Hidden phase 3: every prism fires at once. **Weak to Void** (opposite): void rifts eat her shield. Boss part: Prism heart (spells crit when cast at full resource).

---

#### 3.2.7 Frozen tundra (Frost). East. 5.0 km², built.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Look | Snow flats, glacier plateau, glassy ice rivers and lakes, pines | BUILT | `Data/island_dressing.json`, `M_Ice` |
| Sub-zones | Snow flats, ice shelves, frozen lakes, pine hollows, ice-cave mouths | GAP (CAL Part 3) | CAL |
| Signature resource | Lichen (Woolly drudge food) | BUILT | `Data/items.json`; DR #12 |
| Hazard | Slick ice: no grip unless spiked boots, frost gear or a tundra mount | BUILT | `WRWorldDressing.cpp`; M3 |
| Hidden sub-biome | Ice caves under the glacier | GAP (CAL) | |
| Event | Whiteout: a blizzard covers the tundra; the Pale Hunter and Gloomhowls own it | GAP | |
| Shrine, path master | Shrine on the glacier rim; path master: a hunter who teaches Frost through hunts | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Sparse piano and low strings, cold air, long silences | GAP | |

**Creatures (9):**

| Creature | Behavior / role | What it is | Taming | Tag |
|---|---|---|---|---|
| Rimeclaw | Aggressive fighter | Four-eyed saber-tooth with crystal spines | Knockout and feed, or frost path magic. Hard | BUILT |
| Glacimaw | Aggressive fighter, utility | Ice worm that bursts from snow, tunnels ice | Knockout and feed. Hard | BUILT |
| Gloomhowl | Aggressive fighter | Pale eyeless pack hunters that track by sound | Knockout and feed. Medium | BUILT |
| Woolly drudge | Neutral hauler | Massive shaggy six-legged beast | Offer lichen. Medium | BUILT |
| Frostantler | Neutral mount | Elk with ice-crystal antlers, fast on ice | Knockout and feed. Medium | BUILT |
| Rimeshell | Neutral utility | Shelled creature that breaks ice, digs up ore | Steal an egg. Medium | BUILT |
| Snowpuff | Passive farm animal | Fluffy flightless bird: eggs, meat, feathers | Offer food. Easy | BUILT |
| Aurora fox | Passive utility | Glowing fox that senses hidden areas | Offer food. Easy | BUILT |
| Floeback | Passive swimming mount | Seal-like swimmer that carries a rider through icy water | Offer fish. Easy | RAVEN-PLANNED (CR; deferred, DR #14) |

**Mini-bosses (3):**

| Mini-boss | Fight | Counter | Trophy | Tag |
|---|---|---|---|---|
| Grimjaw (frost troll) | Club swings, thrown ice boulders | Regenerates unless burning or bleeding | Troll marrow | BUILT |
| The Pale Hunter | Invisible in blizzards, ambushes | Footprints in snow; fire or lightning hits reveal it | Stalker fang | BUILT |
| Old Tusker (roaming) | A Woolly drudge bull gone feral that roams the snow flats, tramples camps and calls the herd | Break off his ice-caked tusks with heavy hits; burning hide makes him flee to water | Tusker ivory | GAP (top-up required: RAVEN-PLANNED, BP; this mini is ours) |

**Major boss: Skaldrith, the Frost Wyrm** (glacier bowl). Breath sweep, claws, pillar-breaking tail sweep, lunging bite; phase 2 flies and dives, air breath turns the ground slick, ice spikes; hidden phase 3 whiteout, freezing breath. Weak to fire; hitting his wings in flight grounds him. Drops: core, frost materials, Wyrm scale (blocking can freeze the attacker). **BUILT** (`WRBoss.cpp`; BO).

---

#### 3.2.8 Swamp (Blood & curse). East-south-east. 4.0 km².

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Lifesteal, hexes | RAVEN-PLANNED | DD Biomes & paths |
| Look | Black water under mangroves and hanging moss, red-tinged pools, rotting boardwalks, bone mounds, low fog | GAP | |
| Sub-zones | Murk pools, mangrove thickets, rotting boardwalks, bone mounds, reed flats | GAP (CAL Part 3) | CAL |
| Signature resource | Bloodmoss: red moss that clots wounds; the base of blood brews and hexes | GAP | |
| Hazard | Bleedwater: leech-thick pools that bleed anyone wading. Sealed waders (armorer) or a swamp mount pass | GAP | |
| Hidden sub-biome | The Sunken Coven: a drowned village under the deepest pool | GAP | |
| Event | The Red Tide: pools turn blood-red, leeches swarm, hexers walk at night | GAP | |
| Shrine, path master | Shrine on a bone mound; path master: a swamp witch | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Low cello, bowed metal, slow heartbeat percussion | GAP | |

**Creatures (10), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Murkjaw | Aggressive fighter | Ambush croc under the murk | Knockout and feed, or blood path magic. Hard |
| Gorgeleech | Aggressive fighter | Leech the size of a dog that latches and drains | Knockout and feed. Medium |
| Hexcrow | Aggressive fighter, scout | Crow that curses with a cry and calls its flock | Knockout and feed (bring it down). Medium |
| Bogstalker | Aggressive fighter | Frog-beast whose tongue pulls you into the water | Knockout and feed. Medium |
| Mirestrider | Neutral mount | Long-legged wader, fast in water and mud | Knockout and feed. Medium |
| Mudhulk | Neutral hauler | Hippo-like beast that wades anywhere | Offer bloodmoss. Medium |
| Bogsnout | Neutral utility | Pig that roots out bloodmoss and buried things | Offer food. Easy |
| Lanterntoad | Passive utility | Toad with a glowing throat sac; light in the swamp's dark | Offer insects. Easy |
| Reedfowl | Passive farm animal | Water bird for eggs and meat | Offer food. Easy |
| Slimeshell | Passive farm animal | Giant snail whose slime is an alchemy base | Steal an egg. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| Mother Maw | A giant Murkjaw guarding a nest | Fight her on the boardwalks; she can't dive there | Maw tooth |
| The Hex-Warden | A crow-masked hexer who curses and hides behind totems | Break her totems; each break strips a curse | Hex mask |
| The Bloat (roaming) | A swollen leech that drifts through the pools and bursts into a bleed cloud | Burn or freeze it before it bursts | Bloat sac |

**Major boss: Vharra, Queen of the Mire** (GAP). A leech-drake that rises from a flooded sinkhole. Phase 1: drain bite, tail lash, blood spray. Phase 2: she floods the arena and swims. Hidden phase 3: the water turns to blood and heals her. **Weak to Lightning** (opposite): shock the water to stun her. Boss part: Mire heart (kills on bleeding enemies heal you).

---

#### 3.2.9 Ocean & reefs (Tide). South-south-east. 3.0 km² of land, about 6 km² of sea.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Water magic, underwater breathing, harpoons | RAVEN-PLANNED | DD Biomes & paths |
| Look | White beaches, turquoise shallows, kelp forests and coral reefs, wrecks, sea caves | GAP | |
| Sub-zones | Beaches, tide pools, kelp forests, reefs, wrecks, sea caves | GAP (CAL Part 3) | CAL |
| Signature resource | Tidecoral: living coral that hardens in air; armor and boat fittings | GAP | |
| Hazard | Deep water: drowning and crushing depth. Gills (root), diving gear or a swimming mount pass | GAP (Gills is RAVEN-PLANNED, ST Tide) | |
| Hidden sub-biome | The Drowned Hall: an air-filled ruin under the reef | GAP | |
| Event | The King Tide: the sea pulls far out, uncovering wrecks and caves for a day | GAP | |
| Shrine, path master | Shrine in a tide pool; path master: an old harpooner | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Rolling strings, low choir, water sounds; tidal swells | GAP | |

**Creatures (12), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Reefjaw | Aggressive fighter | Shark that patrols the reef edge | Knockout and feed, or tide path magic. Hard |
| Tidecoil | Aggressive fighter | Eel that shocks and drags | Knockout and feed. Medium |
| Spinecrab | Aggressive fighter | Giant beach crab | Knockout and feed. Medium |
| Gullraider | Aggressive scout | Sea bird that steals from bags and calls flocks | Knockout and feed (bring it down). Easy |
| Wavecharger | Neutral swimming mount | Seahorse-like steed, fast underwater | Steal an egg. Medium |
| Driftback | Neutral swimming hauler | Big sea turtle that carries cargo | Offer kelp. Medium |
| Glowray | Neutral utility | Ray that lights the deep | Offer fish. Easy |
| Pearldiver | Neutral utility | Otter that finds sunken loot and pearls | Offer fish. Easy |
| Kelpgrazer | Passive farm animal | Manatee-like grazer for milk and meat | Offer kelp. Easy |
| Tidefowl | Passive farm animal | Shore bird for eggs | Offer food. Easy |
| Ripplefin | Passive farm animal | Fish kept in sea pens | Offer bait. Easy |
| Floeback (from the tundra) | Passive swimming mount | Lives along the tundra's icy shore and the cold edge of the sea | Offer fish. Easy (RAVEN-PLANNED, CR) |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| Old Barnacle | A hermit crab using a wreck as its shell | Blunt hits break the hull; fire drives it out | Barnacle claw |
| The Siren Eel | An eel whose song turns tames on their owners | Send tames away or fight deaf (sneaking); punishes tames, as Raven asks | Siren scale |
| The Reef Tyrant (roaming) | A Reefjaw grown huge that roams the reef | Harpoon and anchor it in the shallows | Tyrant fin |

**Major boss: Thalassor, the Tide Leviathan** (GAP). A whale-serpent in a tidal lagoon whose tide rises and falls with each phase. Phase 1: waves, tail slams, swallowing lunges. Phase 2: high tide; you fight swimming. Hidden phase 3: maelstrom. **Weak to Sun & sand** (opposite): light dries and cracks its skin; at low tide it's beached. Boss part: Leviathan pearl (dodging through water restores stamina).

---

#### 3.2.10 Graveyard / catacombs (Necromancy). South. 2.5 km² surface, about 2 km² of catacombs.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Raise dead creatures as minions and tames | RAVEN-PLANNED | DD Biomes & paths |
| Tone | Dark fantasy | RAVEN-PLANNED | DD World |
| Look | Grave fields under a bruise-coloured sky, leaning mausoleums, a ruined chapel, bone pits, fog | GAP | |
| Sub-zones | Grave fields, mausoleum rows, ruined chapel, bone pits, crypt entrances | GAP (CAL Part 3) | CAL |
| Signature resource | Soulstone: stone that holds a trace of the dead; powers soul engines | GAP (soul energy as power is RAVEN-PLANNED, DD Machinery) | |
| Hazard | Darkness and corruption: corruption builds here and gives minor debuffs (minor buffs to necromancers) | RAVEN-PLANNED | DD Survival |
| Hidden sub-biome | The catacombs, a deep maze of crypts | GAP (CAL) | |
| Event | Blood moon over the graveyard | RAVEN-PLANNED (named) / GAP (what it does: the dead rise at night as raiders, soulstone drops double) | DD Every biome has |
| No zombies | Undead here are bone, shade and carrion creatures, never zombies | RAVEN-PLANNED (rule) | DD Ruled out |
| Shrine, path master | Shrine in the ruined chapel; path master: a gravedigger | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Organ, slow choir, tolling bells | GAP | |

**Creatures (8), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Gravehound | Aggressive fighter (pack) | Skeletal hounds that rise from grave fields at night | Knockout and feed, or necro path magic. Medium |
| Cryptstalker | Aggressive fighter | Long-legged crypt spider | Knockout and feed. Hard |
| Wraithmoth | Aggressive fighter | Shade moth that drains warmth and stamina | Knockout and feed. Medium |
| Palestrider | Neutral mount | Gaunt pale horse that never tires | Knockout and feed. Medium |
| Tombback | Neutral hauler | Beetle that carries a slab on its back | Offer soulstone dust. Medium |
| Gloamcrow | Neutral utility | Crow that senses graves and secrets | Offer food. Easy |
| Mournhare | Passive farm animal | Grey hare for meat and fur | Offer food. Easy |
| Wickbeetle | Passive utility | Beetle with a candle-wax back; light and wax | Steal an egg. Easy |

**Mini-bosses (4), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| The Bell-Ringer | Rings a bell that raises the dead around him | Break the bell rope; without it he's a weak old man | Bell clapper |
| The Crypt Mother | Giant Cryptstalker in the catacombs, egg sacs on the walls | Burn the egg sacs or she keeps calling young | Mother's fang |
| The Ossuary Lord | A bone construct that reforms after it falls | Nature roots bind the bones; or scatter them before it reforms | Ossuary crown |
| The Gravedigger (roaming) | A huge figure with a shovel who buries players in place | Dodge the shovel arc; allies dig you out | Gravedigger's spade |

**Major boss: Nhal, the Last Mourner** (GAP). A veiled, towering mourner in the ruined chapel who raises the dead each phase. Phase 1: scythe arcs, grave hands, a wail. Phase 2: every grave in the chapel opens. Hidden phase 3: she becomes the shade of what she mourns. **Weak to Nature** (opposite): roots seal the graves and stop her raises. Boss part: Mourner's lantern (each kill gives a short stacking damage buff).

---

#### 3.2.11 Fungal caverns (Spore). South-south-west. 1.5 km² surface, about 3 km² of caverns.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Poison clouds, rot, regrowth | RAVEN-PLANNED | DD Biomes & paths |
| Look | Sinkholes open into caverns of giant glowing mushrooms, mycelium webs, drifting spores, flooded grottos | GAP | |
| Sub-zones | Spore gardens, glow-moss halls, mycelium webs, flooded grottos | GAP (CAL Part 3) | CAL |
| Signature resource | Deepcap: a rare glowing mushroom; alchemy and spore gear | GAP | |
| Hazard | Spore clouds (poison and rot). Immune system (root) or a filter mask passes | GAP (Immune system is RAVEN-PLANNED, ST Spore) | |
| Hidden sub-biome | The Heart Grotto: the mycelium's centre, lit like a city | GAP | |
| Event | The Sporefall: clouds drift up to the surface and seed fungus on the nearest wedges for a day | GAP | |
| Shrine, path master | Shrine in a spore garden; path master: a mushroom farmer | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Wet, organic textures, low drones, slow pulses | GAP | |

**Creatures (10), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Sporestalker | Aggressive fighter | Fungus-furred predator that releases spores when hit | Knockout and feed, or spore path magic. Hard |
| Rotmaw | Aggressive fighter | Ambusher that waits as a mushroom mound | Knockout and feed. Hard |
| Mycomite | Aggressive fighter (swarm) | Swarming fungal ticks | Steal an egg. Easy |
| Webspinner | Aggressive fighter | Spider that weaves mycelium webs | Knockout and feed. Medium |
| Tunnel toad | Neutral mount | Big toad that leaps chasms underground | Knockout and feed. Medium |
| Shroomback | Neutral hauler | Slow beast growing a mushroom garden on its back | Offer deepcap. Medium |
| Truffle mole | Neutral utility | Senses ore and hidden passages | Steal an egg. Easy |
| Glowgrub | Passive utility | Grub that lights the cavern | Offer food. Easy |
| Capling | Passive farm animal | Hopping mushroom critter; edible, regrows | Offer food. Easy |
| Puffball | Passive farm animal | Floating spore sac kept for alchemy | Offer food. Easy |

**Mini-bosses (3), all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| The Sporemother | Spawns sporelings from egg sacs | Burn the sacs | Mother's cap |
| The Rot King | Rot aura, regenerates in his own rot | Frost stops the rot spreading; wind blows it off | Rot crown |
| The Weaver (roaming) | A Webspinner that webs whole halls and moves between them | Wind or fire clears webs; she's slow off them | Weaver silk |

**Major boss: Mycelorn, the Hivemind** (GAP). The cavern's fungal heart, a towering mushroom-thing with a hundred tendrils. Phase 1: tendril slams, spore clouds, sporeling waves. Phase 2: it moves through the mycelium and surfaces anywhere. Hidden phase 3: the whole cavern attacks. **Weak to Wind** (opposite): wind blows its clouds back on it. Boss part: Hive spore (kills burst into a poisoning cloud).

---

#### 3.2.12 Corrupted wasteland (Void). South-west. 6.0 km², the endgame wedge.

| Item | Detail | Tag | Source |
|---|---|---|---|
| Path theme | Big power that costs max HP | RAVEN-PLANNED | DD Biomes & paths |
| Corruption builds here, as in the graveyard | RAVEN-PLANNED | DD Survival |
| Look | Grey dead land, withered forest, floating rubble, black rifts in the air, light that bends | GAP | |
| Sub-zones | Rifts, withered forest, void-scarred ruins, the mini-boss gauntlet road | GAP (CAL Part 3) | CAL |
| Signature resource | Voidglass: black glass from rift edges | GAP | |
| Hazard | Rift zones drain max HP until you rest. Empty vessel (root) or nullward gear passes | GAP (Empty vessel is RAVEN-PLANNED, ST Void) | |
| Hidden sub-biome | The Unmade Place: a pocket inside a rift | GAP | |
| Event | The Rift Storm: rifts open across the wedge and spit void creatures at the nearest bases | GAP | |
| Shrine, path master | Shrine at a rift edge; path master: someone who came back from a rift | RAVEN-PLANNED (exist) / GAP (who) | DD Every biome has |
| Music mood | Reversed sounds, detuned strings, sub-bass; silence used as threat | GAP | |

**Creatures (8), all GAP:**

| Creature | Behavior / role | What it is | Taming |
|---|---|---|---|
| Hollowfang | Aggressive fighter (pack) | Hounds that blink a few metres to flank | Knockout and feed, or void path magic. Hard |
| Riftmaw | Aggressive fighter | Mouth-first horror that bursts out of rifts | Knockout and feed. Hard |
| Nullwing | Aggressive fighter | Flyer that leaves void trails | Knockout and feed (bring it down). Medium |
| Gnawer | Aggressive fighter (swarm) | Small grey eaters in swarms | Steal an egg. Easy |
| Voidstrider | Neutral mount | Gaunt mount that blinks short distances | Knockout and feed. Hard |
| Riftback | Neutral hauler | Beast whose back holds a small pocket of space | Offer voidglass. Medium |
| Driftmote | Passive utility | Floating mote that senses rifts and secrets | Offer food. Easy |
| Withergoat | Passive farm animal | Grey goat; milk and hide | Offer food. Easy |

**Mini-bosses (5), the gauntlet road, all GAP:**

| Mini-boss | Fight | Counter | Trophy |
|---|---|---|---|
| The Unravelled | A figure that comes apart and reforms | Hit the core between reforms | Unravelled thread |
| Mother of Rifts | Opens rifts that spit creatures | Arcane wards close rifts | Rift heart |
| The Starving One | Eats your buffs and grows | Fight with no buffs, or bait it with food | Starving maw |
| The Null Warden | Cancels every spell near it | Melee only | Null seal |
| The Echo (roaming) | Copies the last path ability used on it | Vary your attacks | Echo stone |

**Major boss: Vorhollow, the Devouring Dark** (GAP). A shape of absence on a collapsing island in the void. Phase 1: pulls, consumes, rift beams. Phase 2: the island breaks into pieces. Hidden phase 3: you fight inside it. **Weak to Arcane** (opposite): arcane wards keep the ground solid and break its shield. Boss part: Void heart (kills restore max HP lost to void).

### 3.3 Build-ready numbers for every GAP in the world

These tables turn the biome GAPs above into data rows. Raven's starting numbers are used wherever they exist (SN); everything else is GAP and goes in `Data/*.json` as a first draft.

**Creature tiers (all 12 biomes).** Health and damage are Raven's (SN Enemies); stance, wind-ups and punish windows are CAL 1.4 and 1.13.

| Tier | Where | HP | Hit | Stance | Wind-up | Punish after combo | Pack size | Tag |
|---|---|---|---|---|---|---|---|---|
| Lowland | Outer band, near spawns | 70 | 12 | 20 | 0.45 to 0.65 s, no delay holds | 0.8 s | 1 to 3 | RAVEN-PLANNED (HP, hit: SN) |
| Midland | Middle band, caves | 120 | 20 | 35 | 0.40 to 0.80 s, delay hold up to 0.3 s | 0.7 s | 2 to 4 | RAVEN-PLANNED (HP, hit: SN) |
| Highland | Against the massif | 200 | 32 | 55 | 0.34 to 0.90 s, delay hold up to 0.6 s, reads drinks and casts | 0.6 s | 1 to 5 | RAVEN-PLANNED (HP, hit: SN) |
| Stance, wind-up, punish, delay and pack columns | All tiers | | | | | | | GAP (CAL 1.4, 1.13) |

Every new creature in section 3.2 sits in one tier: aggressive fighters with "Hard" taming are highland, "Medium" midland, passives lowland. Every attack shows the real jaw, claw, horn or projectile; damage lands only on the frame of visible contact. Packs attack freely, all at once if they can reach you. GAP.

**Boss numbers (all 10 new majors and all new minis).** HP and scaling are Raven's (SN); the rest GAP (CAL 1.4, 1.14).

| | Major boss | Mini-boss | Tag |
|---|---|---|---|
| Solo HP | 12,000 | 3,000 | RAVEN-PLANNED (SN) |
| Per extra player or tame | +80% HP | +80% HP | RAVEN-PLANNED (SN) |
| Fight length, group of 3 | 10 to 15 min | 3 to 4 min solo | RAVEN-PLANNED (SN) |
| Stance | 140 to 180 (the heaviest bodies 180) | 90 | GAP |
| Parries to break | 3 | 2 | GAP |
| Phase 2 at | 50%, entered by a telegraphed unblockable transition attack | none | RAVEN-PLANNED (50%: BO) / GAP (transition attack) |
| Hidden phase 3 at | 25% with artifacts, damage ×1.4 | none | BUILT (`WRBoss.cpp` `phase3At`, `phase3DamageScale`) |
| Moves | Phase 1: 5 to 6 attacks including one 3+ hit combo and one grab. Phase 2: 2 new delayed attacks, 1 new combo, 1 arena change | 4 to 5 attacks and one counter mechanic | GAP |
| Wind-ups | Specials 1.0 s or more; slam punish 1.2 s | 0.65 to 0.9 s; punish 0.8 s | GAP |
| Projectiles | 0.9 s wind-up plus travel minimum, max 1,600 cm/s, never home after launch, nothing fired inside 4 m | same | GAP |
| Tame-punishing move | One per major: targets the furthest tame first | Siren Eel only | RAVEN-PLANNED (exists, DD) / GAP (which move) |
| Chase radius | 90 m | 65 m | BUILT (`tuning.json` `bosses`) |
| Respawn | Server setting, 4 h default | same | BUILT |
| Lock points | Head, chest, and every limb or wing that has its own stance | Head, chest | GAP |
| Run-back | A shrine-stone respawn point 40 to 60 m from every arena door | same | GAP |
| First-kill drop | Named, visible, flies to the killer within 6 s | same | GAP |

**Hazard numbers (one gear check per biome).** Built hazards keep their tuning. New ones, all GAP:

| Biome | Hazard | Effect | Passes with |
|---|---|---|---|
| Volcanic | Lava | Built (`tuning.json` `lava`, `hazards`) | Burning blood, fire gear, volcanic mount (BUILT) |
| Frozen tundra | Slick ice | Built (`hazards`) | Spiked boots, frost gear, tundra mount (BUILT) |
| Storm peaks | Strike zones in storms | Above 600 m in a storm, a strike every 20 to 40 s on the highest player in a 60 m cell: 1.0 s ground-glow warning, then 60 damage and a 0.8 s stun. Gusts: 900 cm/s push for 0.6 s every 8 to 15 s on marked cliff paths | Grounded (root); grounding charm (armorer, stormglass + copper); a tamed Rodcrest within 30 m of your base stops strikes there |
| Desert | Sandstorm belt and soft dunes | In the belt: vision 25 m, run speed ×0.7, Radiance doesn't fill. Quicksand pockets: sink over 4 s, dodge out | Sandwalker (root); sand veil (armorer) halves the slow and keeps vision 60 m |
| Ancient forest | Thornbrakes | Pushing through: 6 bleed buildup a second, run speed ×0.5 | Nature path; briarproof leathers; a torch burns a 4 m gap for 5 game hours |
| Sky islands | Falls and gales | Fall damage as normal (lethal off an island); gusts 1,200 cm/s for 0.8 s, 1.5 s whistle warning | Featherfall (root); glider (workbench, levistone + skysilk); anchor boots stop gusts |
| Crystal caves | Ward fields | Walking in: 25 damage and a 600 cm throw, every 1.5 s; plus full dark beyond crystal light | Arcane path; ward charm (alchemy, resonant crystal) |
| Swamp | Bleedwater | Wading: 10 bleed buildup a second | Sealed waders (armorer, bloodmoss-cured hide); swamp mount |
| Ocean & reefs | Deep water | Breath 30 s (swimming skill raises it); below 25 m depth, 5 damage a second | Gills (root); diving helm (armorer, tidecoral); swimming mount |
| Graveyard | Darkness and corruption | Raven's corruption level (DD); outside light, Gravehounds and Wraithmoths hunt at double aggro range | Light (torch, lantern, Wickbeetle); necromancer path turns corruption into a buff (RAVEN-PLANNED) |
| Fungal caverns | Spore clouds | Inside: 12 poison buildup a second, rot after 10 s | Immune system (root); filter mask (alchemy, deepcap) |
| Corrupted wasteland | Rift zones | Max HP −2 per second inside, restored by resting | Empty vessel (root); nullward (armorer, voidglass) halves it |

**World density (all 10 new biomes, GAP; Aaron 11-10 rule).**

| Element | Rule |
|---|---|
| Landmark | One major landmark (a silhouette on the skyline from 1 km: a broken spire, a giant's skeleton, a crashed sky-bridge) every ~800 m |
| POI | Something to walk to every 150 to 250 m on any route: a camp, wreck, ruin, nest, cave mouth, shrine, ore outcrop, a creature set-piece (a predator on a kill) |
| Story | Told only by the place: a burned caravan with its guards' weapons pointing outward, a path master's empty first hut, bones laid out round a dead fire. No cutscenes, no staged scenes (DD World) |
| Sub-zones | 3 to 5 per biome (the lists above), each with its own ground material, palette, prop set, creature mix and ambient bed |
| Caves | 3 to 6 per biome, ore tier rising with depth (CF) |
| Read in the dark | Every POI has a light or silhouette that reads at night |

**Music (all biomes, GAP).** Real composed pieces, never generated loops. Each biome has an explore theme, a night variation, a combat layer and a boss track; music is always audible and ducks no more than 4 dB under combat. Mix at −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP.

| Biome | Tempo | Instruments and feel |
|---|---|---|
| Volcanic | 66 BPM, D minor | Taiko and low brass, anvil strikes on the beat |
| Storm peaks | 92 BPM, E Phrygian | Driving low strings, timpani, thunder as percussion, sudden rests |
| Desert | 84 BPM, D Hijaz | Frame drums, oud-like plucks, ney-like flute over a drone |
| Ancient forest | 72 BPM, G major | Woodwinds, harp, warm strings, birdsong in the mix |
| Sky islands | 100 BPM, F Lydian | Flutes, choir, open fifths, light percussion |
| Crystal caves | 60 BPM, B minor | Glass harmonica, celesta, bells, long reverb |
| Frozen tundra | 58 BPM, A minor | Sparse piano, low strings, wind, long silences between phrases (silences are planned, never the default) |
| Swamp | 64 BPM, C minor | Cello, bowed metal, heartbeat kick |
| Ocean & reefs | 76 BPM, Eb major | Rolling strings, low choir, swells timed like tides |
| Graveyard | 54 BPM, F minor | Organ, slow choir, tolling bell |
| Fungal caverns | 70 BPM, Bb Locrian | Wet organic textures, low drones, slow pulses |
| Corrupted wasteland | 80 BPM, atonal | Reversed and detuned strings, sub-bass, silence used as a threat |

**Creature voices (all new creatures, GAP).** Every species gets its own voice, made by fusing real animal calls (for example Voltfang = wolf howl + electrical crackle; Murkjaw = crocodile hiss + bullfrog; Skyray = whale call pitched up). Four layers per attack: tell (rising cue before every wind-up over 0.5 s), strike, hit, pain.

**Monster look (all new creatures and bosses, GAP).** Silhouette readable at a glance in the dark; terrifying for predators and bosses. Shown standing, still and at scale beside the hero before any behaviour is written. No engine primitives, no unfinished AI models: concepts picked by Aaron, then finished in Blender. Real animation clips per species; feet planted, no gliding, gait speed matched to ground speed.

**Invasions (GAP, concrete).** Server setting "invasion frequency" (SS lists it for full release): Off, Rare (one per pair per real week), Normal (two), Frequent (four). An invasion is announced to the defending biome's players 5 min ahead; 8 to 14 creatures of the attacking biome plus one of its minis come through the pair's pass and march on the nearest claim or shrine within 1.5 km. They hold the shrine for 30 min if not stopped (fast travel to it shuts). Beating it pays both biomes' path materials and 2,000 path XP to everyone who dealt 10% (Raven's loot rule reused). Raven names the pairs and the setting.

**Biome events (GAP, concrete).** Each event runs one game day, starts with a world-visible sign (eruption plume, storm wall, red moon), happens about once every 3 real days per biome, and always pays in its biome's signature resource at triple rate.

---

## 4. Paths and skill trees

### 4.1 Rules (all from Raven)

| Rule | Tag | Source |
|---|---|---|
| Main path picked at creation sets spawn biome; main path's root passive works only as main | BUILT | `WRPathComponent.cpp`; ST Rules |
| Level paths by using them; path XP earns points for that path | BUILT | `WRAbilities.cpp`, `WRPathComponent.cpp` |
| Tree = 10-point trunk + three 30-point branches = 100; capstones 10 points | BUILT (Fire, Frost data) | `Data/paths/fire.json`, `frost.json` |
| Node order by points spent in the branch (gates 0/5/10/15/20); trunk first | BUILT | DR #8; `WRPathTypes.cpp` |
| 150 points total, no path over 100 (fixed on every server) | BUILT | `WRPathComponent.cpp`; SS Community servers |
| Subclass unlocked at another biome's boss altar; swap main or subclass at altars; progress saved per path | BUILT | `WRAltar.cpp` |
| Point n costs 100 + 10n path XP (about 80 h per path at 1×) | BUILT | `Data/tuning.json` `paths`; SN Progression |
| Character level = path points ÷ 3; +3 HP and +1 stamina per level, 250 / 150 at level 50 | BUILT | DR #35, #36 |
| Two automatic combos per pair at 25/25 and 50/50, cost no points | BUILT (Fire+Frost) / RAVEN-PLANNED (64 others) | CB |
| Combos that change an ability need that ability unlocked | RAVEN-PLANNED | CB |
| Respec: rare item or long quest only | RAVEN-PLANNED | DR #9 |
| Body changes with your path (glowing veins for fire, pale skin for necromancy) | RAVEN-PLANNED (two named) | DD Classes |
| Each path has its own resource bar; two paths = two bars | BUILT (Heat, Chill) | `WRAttributeSet.cpp` |
| 8 quick slots (4 per path) + light/heavy, reassignable outside combat | BUILT (as two ability wheels on Q/Z, or direct keys) | DR #1; `WRUIWheel.cpp` |
| Weapon arts are called **Wildbrands** in the game | GAP (Aaron 11-10, CAL 1.8; Raven's nodes say "weapon art") | CAL 1.8 |
| Secret hybrid paths; god path | RAVEN-PLANNED (Dream tier) | BP Dream list |
| Prestige: reset a path for permanent perks | RAVEN-PLANNED | DD Endgame |

### 4.2 The twelve trees

Every node name, cost and effect is in ST; this table is the at-a-glance version. "Built nodes" means the node has a working ability class in `UWRAbilityRegistry::ClassForNode` (`WRAbilities.cpp`); stat nodes work through `nodeEffects` in `Data/tuning.json`.

| Path | Resource (how it works) | Root passive | Branches (capstone) | Status | Body change |
|---|---|---|---|---|---|
| Fire | Heat: builds as you attack; full Heat stops building, never hurts (DR #2) | Burning blood: immune to own burns and lava | Inferno (Wildfire), Eruption (Meteor), Forge (Molten cleave) | BUILT: tree data, 7 ability nodes (Fire bolt, Flame slash, Fire wall, Lava pool, Molten edge, Molten cleave, Meteor) | RAVEN-PLANNED: glowing veins (DD) |
| Lightning | Charge: fills fast, empties fast | Grounded: immune to lightning stuns | Storm (Tempest), Surge (Lightning step), Conductor (Thunder god) | RAVEN-PLANNED (ST Lightning) | GAP: faint branching scars under the skin that flicker in combat; hair lifts |
| Sun & sand | Radiance: charges in daylight, weak at night | Sandwalker: no penalty in sand or sandstorms | Radiant (Sunburst), Dune (Desert's wrath), Solar blade (Noon strike) | RAVEN-PLANNED (ST Sun & sand) | GAP: sun-darkened skin with gold flecks, eyes that catch light |
| Nature | Growth: fast near plants, slow in barren land | Rooted: standing still slowly heals | Grove (World tree), Thorn (Strangling vines), Bloom (Ancient guardian) | RAVEN-PLANNED (ST Nature) | GAP: bark-like patches at the joints, leaf-green tint in the eyes |
| Wind | Momentum: builds while airborne or sprinting | Featherfall: no fall damage | Skyborne (Sky lord), Cyclone (Tornado), Windblade (Storm volley) | RAVEN-PLANNED (ST Wind) | GAP: pale wind-marked tattoos that move, hair always stirring |
| Arcane | Mana: steady pool | Deep well: faster Mana regen | Ward (Aegis), Sorcery (Starfall), Rune (Rune circle) | RAVEN-PLANNED (ST Arcane) | GAP: rune lines on the forearms that glow when casting |
| Frost | Chill: fills slowly; spend it all for bigger shatters | Permafrost: slowed enemies take extra damage | Shatter (Chain shatter), Blizzard (Blizzard), Ice blade (Frozen moment) | BUILT: tree data, 7 ability nodes (Frost shard, Ice slash, Frost nova, Ice lance, Flurry, Ice wall, Blizzard) | GAP: frost-pale lips and fingertips, breath that mists |
| Blood & curse | Blood: spends your HP; lifesteal earns it back | Bloodthirst: lifesteal rises as HP drops | Hemomancy (Crimson tide), Hex (Ruin), Vampiric (Sanguine lord) | RAVEN-PLANNED (ST Blood & curse) | GAP: dark red veins at the neck, sharper canines |
| Tide | Flow: builds moving and dodging, drains standing still | Gills: breathe and fight underwater | Undertow (Maelstrom), Harpooner (Leviathan's hook), Tidecaller (High tide) | RAVEN-PLANNED (ST Tide) | GAP: faint scale sheen on the neck and gill lines that open underwater |
| Necromancy | Souls: from kills, spent to raise and empower | Deathless: once per rest, survive a killing blow at 1 HP | Horde (Army of the dead), Reaper (Final harvest), Lich (Lich form) | RAVEN-PLANNED (ST Necromancy) | RAVEN-PLANNED: pale skin (DD) |
| Spore | Spores: planted on enemies, then detonated | Immune system: immune to poison and rot | Rot (Pandemic), Colony (Fungal behemoth), Symbiote (Living colony) | RAVEN-PLANNED (ST Spore) | GAP: faint glowing fungal freckles, mossy hair tips |
| Void | Corruption: huge power; each use lowers max HP until you rest | Empty vessel: corruption slower, void costs less max HP | Abyss (Black hole), Hollow (Nowhere), Devour (Maw of the void) | RAVEN-PLANNED (ST Void) | GAP: eyes go black at the centre, skin greys at high corruption |

**Unbuilt Fire and Frost nodes with behaviour** (Spreading flames, Fuel the fire, Wildfire, Explosive bolt, Overheat, Searing parry; Brittle, Cold return, Chain shatter, Whiteout, Cold riposte, Frozen moment): the changelog of 10-24 says every node works, CLAUDE.md says some show "[not built yet]". Check in game. Tag: RAVEN-PLANNED (ST), status to confirm.

**Combos:** Fire+Frost (Thaw at 25/25, Balance at 50/50) are BUILT (`WRCombatLibrary.cpp`). The other 64 pairs are RAVEN-PLANNED with every name and effect written (CB). `Data/combos.json` (DATA_SCHEMAS) is RAVEN-PLANNED and doesn't exist yet.

**Mixing elements in combat triggers reactions** (fire then frost = steam burst) separately from owned-pair combos. RAVEN-PLANNED (DD How paths work 6). Only Thaw exists; a general reaction table across the 12 elements is a GAP: reactions fire when two different players' elements meet on one target (so groups get combos without owning both paths), using the owned-pair combo's 25/25 effect at reduced strength.

---

## 5. Combat

Raven's docs say "Elden Ring style combat" and design the frame; Aaron's 11-10 decision copies Elden Ring in detail (CAL). Where Raven's starting numbers and CAL differ, CAL's numbers are the cook target and Raven's stay in history (both are data in `tuning.json`).

### 5.1 The fight

| Item | Tag | Source |
|---|---|---|
| Stamina for attacks, dodges, blocks | BUILT | `WRAttributeSet.cpp`; SN Player |
| Light and heavy attacks, 3-hit light chain | BUILT | `WRAbilities.cpp` |
| Dodge roll with i-frames | BUILT | `WRAbilities.cpp`, `WRCharacterMovementComponent.cpp` |
| Block, with parry in the first moment; riposte on a staggered target | BUILT | `WRCombatLibrary.cpp` |
| Lock-on, optional | BUILT | `WRPlayerController.cpp` |
| Poise and stagger | BUILT | `WRAttributeSet.cpp` |
| Ranged weapons (bow, crossbow, blowgun, throwing spear, bola) with aim over the shoulder | BUILT (built early from full release) | `WRRanged.cpp`, `Data/items.json` |
| Mounted combat | RAVEN-PLANNED | DD Combat basics; BP Full release |
| Equip load affects dodge (light / medium / heavy roll) | RAVEN-PLANNED | DD Gear; M1 section 2 |
| Healing is crafted potions only, belt slots, short cooldown | BUILT | `WRInventory.cpp`; DD How combat works |
| Weapons separate from paths; the path infuses the weapon | BUILT (element set at crafting) | `WRCrafting.cpp`; DD Classes |
| Elemental weaknesses matter (bring frost to the fire boss) | BUILT (1.5× weakness) | `Data/tuning.json` `bosses.weaknessMultiplier` |
| Status effects: burn, frostbite, slowed, frozen, shatter, stagger | BUILT | `WRStatusComponent.cpp`; DR #6 |
| All statuses: bleed, poison, rot, plus each path's (shock, soak, blind, decay, mark, curse) | RAVEN-PLANNED (bleed, poison, rot named in DD; DR #6 "full list is official") / GAP (shock, soak, blind, decay, mark, curse as buildups: CAL 1.12) | DD Combat basics; CAL 1.12 |
| Enemy groups that flank, surround and retreat | RAVEN-PLANNED | DD Combat basics |
| **No attacker limit:** packs aggro and attack freely; pulling one at a time is the player's skill | GAP (Aaron 11-10, CAL 1.13) | CAL 1.13 |
| Aggressive, neutral and passive mobs | BUILT | `Data/creatures.json` |
| Creature special moves (lunge, charge, spit, stomp) with wind-ups | BUILT | `WRCreature.cpp`, `tuning.json` `creatureAI`, `telegraph` |

### 5.2 Elden Ring detail (the cook target)

All GAP (Aaron 11-10; CAL section in brackets). Raven's own frame items they build on are tagged in 5.1.

| Item | Tag |
|---|---|
| Full attack set: charged heavy, running, rolling, backstep, jump and crouch attacks, guard counter (CAL 1.1) | GAP |
| Animation commitment, 0.33 s input buffer, notify-driven windows (CAL 1.1, 1.16) | GAP |
| Guard boost per item, chip damage, guard break, regen paused while guarding, stamina spent only on blocked hits (CAL 1.2) | GAP |
| Parry as a tap, riposte 3.45×, backstab 2.3×, paired crits by motion warping (CAL 1.3) | GAP |
| Creatures and bosses get Stance (break = crit); players keep Poise; group stance scaling (CAL 1.4) | GAP |
| Hyperarmor windows; poise from armour (CAL 1.5) | GAP |
| Stamina regen 45/s after 0.5 s; roll 12; "last swing" grace to −20 (CAL 1.6) | GAP |
| Load tiers set roll distance, i-frames and recovery; backstep; jump leg i-frames (CAL 1.7) | GAP (tiers themselves are RAVEN-PLANNED, M1) |
| Wildbrands: path weapon arts fired through the held weapon, with class fit, charge and follow-ups (CAL 1.8) | GAP |
| Spells cast while walking for light ones, rooted for big ones; chargeable (CAL 1.9) | GAP |
| Potion drink is a 0.9 s committed animation that AI punishes; belt of 4 (CAL 1.10) | GAP |
| Weapon classes with their own movesets: sword (built), axe, pick-hammer, spear, greatsword (DD names a "fire greatsword"), later dagger, great hammer, scythe (CAL 1.11) | GAP (greatsword is RAVEN-PLANNED by name, DD Classes) |
| Status buildup thresholds rise after each proc; fire ends frostbite (CAL 1.12) | GAP |
| Every creature attack is a data row: wind-up, tracking, delay hold, combo next, punish window, flags (CAL 1.13) | GAP |
| Tells only start on screen, sound before sight, colour law (gold = yours/parry, crimson = unblockable) (CAL 1.13) | GAP |
| Boss phase change is itself an attack; phase 2 changes timing; several lock points on big bosses (CAL 1.14) | GAP |
| Camera: spring arm that ignores small props, giant framing, flick lock-switching, never inverted by default (CAL 1.15) | GAP |
| PvP block: ×0.6 player damage, narrower backstab, shorter dodge-skill i-frames (CAL 1.18) | GAP |
| Hit feel stack: reaction, flash, sparks, camera trauma, rumble, 4 sound layers, hitstop 0.06 to 0.30 s (CAL L1) | GAP |
| One verb per button pad layout; F1 controls; F8 in-game bug report (CAL L4, L10) | GAP |
| Not copied: spirit summons (taming is the ally system), stat points, FP, sleep, dodge on release (CAL 1.19) | GAP (rule) |

### 5.3 Bosses (rules for all)

| Item | Tag | Source |
|---|---|---|
| Major boss: 2 phases + hidden phase 3 with artifact modifiers | BUILT (phase 3 behind a flag until artifacts exist) | `WRBoss.cpp`; BO Rules |
| Health scales with group size; each tame counts as a player (+80% each) | BUILT | `WRBoss.cpp`; SN Enemies |
| 10% damage gets your own loot, rival clans included (fixed on every server) | BUILT | `WRBoss.cpp`; SS |
| Dying: respawn and run back while others fight | BUILT | `WRBoss.cpp` |
| Bosses chase up to 90 m (minis 65 m) and walk home | BUILT | `WRBoss.cpp`, `tuning.json` `bosses` |
| Bosses respawn on a server timer (1 to 24 h; 4 h default) | BUILT | `Data/server_presets.json` |
| Roaming mini-bosses | RAVEN-PLANNED | DD Bosses |
| Summoning altars: offer items to fight a boss on demand | RAVEN-PLANNED | DD Bosses |
| Bosses with attacks that punish or scatter tames | RAVEN-PLANNED | DD How taming works |
| Optional secret super-bosses | RAVEN-PLANNED (Dream) | BP Dream list |
| PvP inside boss arenas (server setting) | BUILT | `WRSocial.cpp`; SS |
| A respawn point beside each arena (Elden Ring's stake) | GAP (CAL 1.14) | CAL |
| The first kill's drop is always visible, named, and flies to the killer | GAP (CAL L14) | CAL |
| Boss kill-time leaderboards | RAVEN-PLANNED | DD Endgame |

---

## 6. Survival, crafting, building, taming, life skills

### 6.1 Survival

| Item | Tag | Source |
|---|---|---|
| Hunger and thirst, light (empty: stamina regen halves, then health drains) | BUILT | `WRAttributeSet.cpp`; SN |
| Food fills hunger only; buffs come from potions and brews | BUILT (rule) | DD Survival |
| Carry weight; over-encumbered slows and blocks dodge | BUILT | `WRInventory.cpp` |
| Durability on tools, weapons, armor; repair at the matching station | BUILT | `WRCrafting.cpp` |
| Bedrolls and beds as respawn points | BUILT | `WRBeds.cpp` |
| Rested buff from sleeping | RAVEN-PLANNED | DD Survival |
| "Rest" means sleeping in a bed or bedroll (needed by Deathless and Void) | GAP (DR #13 open; emailed to Raven 11-10) | DR #13 |
| Darkness is dangerous; caves need light | RAVEN-PLANNED (caves and Glowmoth light exist) | DD Survival |
| Weather that matters | RAVEN-PLANNED | DD Survival |
| Corruption in dark biomes: minor debuffs, minor buffs if the biome matches your path | RAVEN-PLANNED | DD Survival |
| Swimming and diving | BUILT (swim volumes) | `WRWater.cpp` |
| Players die from every hazard and status; no floors | GAP (CAL T10) | CAL |
| Ruled out: temperature, food buffs, injuries, disease, natural disasters | RAVEN-PLANNED (rule) | DD Ruled out |

### 6.2 Farming and food

| Item | Tag | Source |
|---|---|---|
| Crop plots (plant, grow, harvest; common 20 min, rare 2 h) | BUILT | `WRBuilding.cpp`, `tuning.json` `crops` |
| Animal pens (feed, produce; Pumicehog, Snowpuff) | BUILT | `WRBuilding.cpp`, `tuning.json` `husbandry` |
| Biome crops, irrigation, greenhouses for off-biome crops | RAVEN-PLANNED | DD Farming; BP Full release |
| Animal husbandry, beekeeping, biome fishing | RAVEN-PLANNED | DD Farming |
| Cooking stations and recipe discovery by experimenting | RAVEN-PLANNED (meals cook at the workbench today) | DD Farming |
| Spoilage and preservation: salting, smoking, ice boxes | RAVEN-PLANNED | DD Farming |
| Alchemy, potions and brewing (where buffs come from) | BUILT (potions, antidote) / RAVEN-PLANNED (brews, elixirs) | `Data/recipes.json`; CF Tech levels |
| Biome crop list: fire peppers (volcanic), snow turnips (tundra), stormwheat, date palms, heartwood seedlings, cloudberries, glowmelons, bloodroot, sea kelp, grave lilies, deepcaps, ashen grain | GAP | |

### 6.3 Crafting

| Item | Tag | Source |
|---|---|---|
| One shared tree: base metal = tier, path material = element (set forever), extras add bonuses | BUILT | `WRCrafting.cpp`; CF Rules |
| Four stations (forge, armorer, alchemy table, workbench) levelled together by the claim's Hearth | BUILT | `WRCrafting.cpp`, `WRUIHearth.cpp` |
| Hearth tech levels by different major bosses' cores: 1 (0), 2 (1), 3 (2), 4 (4), 5 (6) | BUILT (1 to 3) / RAVEN-PLANNED (4, 5) | CF Tech levels |
| Metals copper, bronze, iron | BUILT | `Data/items.json` |
| Steel (level 4) and starsteel (level 5, forged with boss cores) | RAVEN-PLANNED | CF Tech levels |
| Quality crude to masterwork, 0 to 3 sockets, random stat rolls | BUILT | `WRCrafting.cpp` |
| Gems (ruby, topaz, onyx) and boss parts in sockets | BUILT | `Data/items.json` |
| Runes (crafted or found) | RAVEN-PLANNED | CF Extras |
| Upgrades +1 to +10 with mini-boss shards, capped by Hearth level | BUILT (to +6) | `WRCrafting.cpp` |
| Crafting queues that run while you're away | BUILT | `WRCrafting.cpp` (0.7.0) |
| A broken Hearth stops crafting; cores stay safe | BUILT | `WRCrafting.cpp` |
| Salvage | RAVEN-PLANNED | CF Other crafting |
| Blueprints from bosses and portals | RAVEN-PLANNED | CF Other crafting |
| Siege weapons, boats | RAVEN-PLANNED | CF Tech levels 4 |
| Biome armor set bonuses | RAVEN-PLANNED | CF Tech levels 4; DD Gear |
| Talismans and rings | RAVEN-PLANNED | DD Gear |
| A named armor set per biome made from its signature resource (Forgeplate, Rimeguard, Stormweave, Dunewrap, Heartwood, Skysilk, Wardcloth, Mirehide, Tidecoral, Gravecloth, Sporeweave, Voidglass) | GAP | |
| Machinery: water wheels, windmills, power (water, wind, lava, soul energy), conveyors, sorting, drills, medium production chains, biome machines (lava forge, soul engine) | RAVEN-PLANNED | DD Machinery; CF Tech levels |

### 6.4 Building

| Item | Tag | Source |
|---|---|---|
| Snap building for structure, freeform for utility and decor | BUILT | `WRBuilding.cpp`, `WRPlayerBuild.cpp`; DR #28 |
| Wood and stone at Hearth level 1; span support wood 4, stone 6 | BUILT | `Data/pieces.json`; DR #29, #33 |
| 45 pieces: core, defense (palisade, gate, guard tower, reinforced door), utility, decor | BUILT | `Data/pieces.json` |
| Land claim totem: only your clan builds inside; 30 m, +10 m per upgrade to 60 m; unlimited claims | BUILT | `WRBuilding.cpp`, `WRUIClaim.cpp` |
| Upkeep in materials in the totem; out of upkeep the claim drops and the base decays | BUILT | `WRBuilding.cpp` |
| Doors with code-lock keypads | BUILT | `WRUIDoor.cpp` |
| Creature raids on bases (Off / Rare / Normal / Frequent) | BUILT | `WRRaids.cpp` |
| Offline protection setting | BUILT | `WRBuilding.cpp` |
| Metal tier (Hearth 4, span 8) and biome materials (Hearth 5, themed look) | RAVEN-PLANNED | BL Materials |
| Freeform structural pieces; structural integrity | RAVEN-PLANNED | BP Full release Building |
| Biome-themed building sets; comfort buff from decor | RAVEN-PLANNED | BL; DR #30 |
| Powered turrets | RAVEN-PLANNED | DD Building |
| Bases built on large tames | RAVEN-PLANNED (Dream) | BP Dream list |
| Boss trophies as base monuments | BUILT (trophy mount) / RAVEN-PLANNED (monuments) | `Data/pieces.json`; DD Endgame |
| No spikes or traps in bases; no underground bases; no NPC guards | RAVEN-PLANNED (rule) | BL; DD Ruled out |

### 6.5 Taming

| Item | Tag | Source |
|---|---|---|
| Knockout and feed (torpor, tranq arrows and darts, narcotic, kibble), offer food, steal an egg, path magic | BUILT | `WRCreature.cpp`, `WRUITaming.cpp` |
| Each creature tames only by its listed methods | BUILT | DR #16 |
| Taming time by size: easy 1 min, medium 5, hard 15; skill up to 50% faster | BUILT | SN |
| Orders (follow, stay, guard, attack, stance) by an order wheel | BUILT | `WRUIWheel.cpp` |
| Ground mounts | BUILT | `WRCreature.cpp` |
| Haulers carry cargo; utility tames (cave light, ore digging, secret sensing) | BUILT | `WRCreature.cpp` |
| Tame limit per player, growing with the Taming skill | BUILT | DR #4 |
| Tames fight with you; count as a player for boss HP | BUILT | `WRBoss.cpp` |
| Tames keep their own biome element | RAVEN-PLANNED (rule) | DD How taming works |
| Tame whistle has a screen (orders all tames near you) | GAP (console only today, HANDOFF) | HANDOFF |
| Necro raising: quick combat raises (Souls, not counted against the limit) and permanent undead tames by ritual | RAVEN-PLANNED | DD How taming works |
| Flying, swimming and pack mounts | RAVEN-PLANNED | DD Creatures |
| Creature levels and skill trees | RAVEN-PLANNED | DD Creatures |
| Breeding with stat inheritance, eggs or pregnancy, raising babies; mutations | RAVEN-PLANNED | DD How taming works; BP Full release |
| Cross-biome hybrid creatures | RAVEN-PLANNED (Dream; early hybrids reuse a parent's body) | BP Dream list |
| Craftable saddles and creature armor | RAVEN-PLANNED | DD Creatures |
| Permadeath, with a rare revive item | BUILT (permadeath) / RAVEN-PLANNED (revive) | M4 Tames |
| Tamable mini-bosses | RAVEN-PLANNED (Dream) | BP Dream list |
| Creature skill tree shape: one 20-point tree per role (fighter, mount, hauler, utility, farm), points from the tame's own use | GAP | |

### 6.6 Life skills

Fifteen skills, 1 to 100 by doing, a small boost per level and a choice of two perks at 25, 50, 75 and 100; no total cap, no decay; level n costs 50 + 5n XP (about 50 h each). Every perk name is in LS.

| Skill | Group | Status | Tag | Source |
|---|---|---|---|---|
| Mining | Gathering | XP and perks live | BUILT | `WRLifeSkills.cpp`, `Data/lifeskills.json` |
| Woodcutting | Gathering | Live | BUILT | same |
| Fishing | Gathering | Data only, no XP source (no fishing yet) | RAVEN-PLANNED | LS; CLAUDE.md batch 5 |
| Tracking | Gathering | XP from kills | BUILT | `WRCreature.cpp` |
| Smithing | Making | Live | BUILT | `WRCrafting.cpp` |
| Armorcraft | Making | Live | BUILT | same |
| Alchemy | Making | Live | BUILT | same |
| Cooking | Making | XP from food recipes | BUILT | same |
| Farming | Base and farm | XP from planting, harvest | BUILT | `WRBuilding.cpp` |
| Husbandry | Base and farm | XP from collecting | BUILT | same |
| Building | Base and farm | Live | BUILT | `WRBuilding.cpp` |
| Engineering | Base and farm | Data only (no machines yet) | RAVEN-PLANNED | LS |
| Taming | Creatures | Live | BUILT | `WRCreature.cpp` |
| Swimming | Exploration | Data only, no XP source though swimming exists | RAVEN-PLANNED | LS; CLAUDE.md |
| Sneaking | Exploration | XP from sneaking | BUILT | `WRCreature.cpp` |

Perks with nothing behind them yet (Monster line, Spear fisher, Rare finder, Breeder, Beast lord's mini-boss taming, every engineering perk) are hidden until their system ships. GAP (FRP 0.3).

---

## 7. Multiplayer, clans, PvP and server settings

| Item | Tag | Source |
|---|---|---|
| Host-and-play listen server; join by IP; worlds saved per folder on the host | BUILT | `WRWorlds.cpp`, `WRMainMenu.cpp` |
| Installer and auto-update from GitHub releases | BUILT | `Tools/installer/` |
| Dedicated server for 10 to 100 players | RAVEN-PLANNED (on hold, Raven's call) | BP First playable; M7 |
| Persistence: SQLite on the server, hourly backups | RAVEN-PLANNED (JSON files per world today) | M7 Persistence |
| Steam/EOS login | RAVEN-PLANNED | M7 Identity |
| Open PvP everywhere but safe zones; PvE-only option | BUILT | `WRSocial.cpp` |
| Death penalty: gentle, Souls-like (bag at your death spot), drop inventory, full loot | BUILT | `WRSocial.cpp` |
| Safe zones at spawn and trading hubs | BUILT (spawn) / RAVEN-PLANNED (trading hubs) | DD Multiplayer |
| Clans: found, invite, remove, leave; shared bases, beds, no friendly fire | BUILT | `WRUIClan.cpp`, `WRSocial.cpp` |
| Clan ranks and alliances; anti-zerg clan size limits | BUILT (size limit setting) / RAVEN-PLANNED (ranks, alliances) | SS; DD Multiplayer |
| Offline raid protection (0 to 100% less damage) | BUILT | `WRBuilding.cpp` |
| Explosives, raid tools, siege weapons | RAVEN-PLANNED | DD Multiplayer |
| Path-based raiding (fire burns wood walls, necromancers raise the dead inside) | RAVEN-PLANNED | DD Multiplayer |
| Tames used in raids | RAVEN-PLANNED | DD Multiplayer |
| Bounties on player killers and infamy marks | BUILT (setting only) / RAVEN-PLANNED (feature) | DR #32 |
| Contested bosses rival clans fight over | BUILT in effect (10% rule includes rivals) | DD Multiplayer |
| Raiders can damage the Hearth | BUILT | `WRCrafting.cpp` |
| Text chat (global, local, clan) | BUILT | `WRPlayerController.cpp` |
| Three locked official presets (PvP standard, PvE, Casual co-op) and custom community settings | BUILT | `Data/server_presets.json` |
| Internal Playtest preset (10× XP, taming) | BUILT | `Data/server_presets.json`; M7 |
| Full-release settings: portal break timers, artifact limits, invasion frequency, raid group sizes | RAVEN-PLANNED | SS Full release settings |
| Fixed on every server: 10% loot rule, 150/100 cap, artifact weird rules only on the wearer | BUILT (first two) / RAVEN-PLANNED (artifacts) | SS |
| Ruled out: raid windows, territory control, PvP arenas, scheduled sieges | RAVEN-PLANNED (rule) | DD Ruled out |

---

## 8. Progression and endgame

### 8.1 The arc

| Stage | What you're doing | Tag | Source |
|---|---|---|---|
| Hours 0 to 5 | Home biome lowlands: tools, first base, first tame, copper gear, Hearth level 1 | BUILT | |
| Hours 5 to 20 | Midlands: caves and ore, first mini-bosses, upgrade shards, bronze | BUILT | |
| Hours 20 to 40 | First major boss with a group; Hearth level 2; cross the pass; unlock the subclass | BUILT (Fire/Frost) | |
| Mid game | Two paths growing toward 25/25 and 50/50 combos; second and third majors; iron; mounts, breeding, farming, machinery | BUILT in part / RAVEN-PLANNED | |
| Late game | Four and six different majors for Hearth 4 (steel) and 5 (starsteel); biome armor sets; portals by rank | RAVEN-PLANNED | CF; DD Dungeon portals |
| Endgame | Artifact modifiers and hidden boss phases, raids for 6 to 10, mythic gear from every biome, boss soul forging, prestige | RAVEN-PLANNED | DD Endgame |
| Character level 50 = 150 path points | BUILT | DR #36 |

### 8.2 Dungeon portals

| Item | Tag | Source |
|---|---|---|
| Random portals in every biome, themed, rank shown by colour, 5 ranks deeper in a biome | RAVEN-PLANNED | DD Dungeon portals |
| Handmade rooms shuffled into new layouts, random modifiers (no healing, double damage) | RAVEN-PLANNED | DD Dungeon portals |
| Puzzles and traps; a boss at the end of every portal | RAVEN-PLANNED (traps in dungeons open, DR #21) | DD; DR #21 |
| Breaks after 1, 3, 6, 10, 18 real days; spills monsters; closes when its boss escapes; escaped boss blocks that rank in that biome until killed | RAVEN-PLANNED | DD How portals work |
| Craftable keys spawn a chosen rank; key portals belong to the opener | RAVEN-PLANNED | DD |
| Legendary portals announced server-wide | RAVEN-PLANNED | DD |
| Death inside uses server rules; bodies appear at the entrance if it closes | RAVEN-PLANNED | DD |
| No dungeon currency, no forced party size | RAVEN-PLANNED (rule) | DD |
| Endless tower portal with a leaderboard | RAVEN-PLANNED (Dream) | BP Dream list |
| A cleared portal closes and drops a rank-coloured shard used in keys | GAP (DR #20 says it's unclear) | DR #20 |

### 8.3 Artifacts and the rest of the endgame

| Item | Tag | Source |
|---|---|---|
| Three artifact slots; artifacts drop from bosses and portals | RAVEN-PLANNED | DD How artifacts work |
| Penalties (more damage taken, weird rules) for rewards (loot, rarity, path XP, exclusive drops); party uses the highest multiplier; weird rules only on the wearer; PvE only | RAVEN-PLANNED | DD How artifacts work |
| Hidden third boss phases need artifacts | BUILT (phases) / RAVEN-PLANNED (artifacts) | `WRBoss.cpp`; BO |
| Raids for 6 to 10 players | RAVEN-PLANNED | DD Endgame |
| Raid content: one raid per opposite pair, held in the central massif where the pair's pass meets, against a fused boss of both elements | GAP (DD rules out cross-biome endgame bosses; these are raid bosses in a raid instance, so Raven must rule) | DD Ruled out |
| Mythic gear needing materials from every biome | RAVEN-PLANNED | DD Endgame |
| Legendary weapons with unique movesets | RAVEN-PLANNED (Dream) | BP Dream list |
| Boss soul forging: boss drops into unique items | RAVEN-PLANNED | DD Endgame |
| Prestige: reset a path for permanent perks | RAVEN-PLANNED | DD Endgame |
| God path after mastering several paths; secret hybrid paths | RAVEN-PLANNED (Dream) | BP Dream list |
| Leaderboards, achievements, titles, cosmetics, boss trophies as monuments | RAVEN-PLANNED | DD Endgame |
| Ruled out: server-wide boss events, a world boss, cross-biome endgame bosses, world-wide escalation, weekly challenges, first-kill announcements | RAVEN-PLANNED (rule) | DD Ruled out |

---

## 9. The main character

| Item | Tag | Source |
|---|---|---|
| Today: a Mixamo-rigged AI "survivor", male or female, with an AI sword and shield | BUILT (to be replaced) | `Data/player_art.json`, `Content/Characters/Survivor` |
| Raven's plan: Manny/Quinn with Epic's Game Animation Sample locomotion, Mixamo attacks | RAVEN-PLANNED (superseded) | M1 section 2; TECH_PLAN art strategy |
| **New hero: MetaHuman male and female, GASP motion matching, the Mixamo survivor scrapped** | GAP (Aaron 11-13; keeps Raven's GASP choice from M1) | Aaron 11-13 |
| Attractive, real-world bodies; hair on every head; body sliders feed the creator | GAP (Aaron 11-13 plan) | |
| Survivor outfit (leather, fur, wraps) tailored per body; biome armor sets on top | GAP (outfit) / RAVEN-PLANNED (biome armor) | CF; DD Gear |
| Body changes with path: Fire glowing veins, Necromancy pale skin, the other ten as in section 4.2 | RAVEN-PLANNED (two) / GAP (ten) | DD Classes |
| Changes show at both paths, the main path strongest | GAP | |
| First and third person; full body in first person, camera on the head bone | BUILT (FP and TP, FP hides body) / GAP (full body FP, FRP 0.3) | `WRCameraBoom.cpp`; FRP |
| Held tools and weapons as real models with grip sockets | GAP (FRP 0.3; tools are engine shapes today) | `WRHeldTool.cpp` |
| Weapon classes each with their own moveset on the new rig | GAP (CAL 1.11) | |
| Swim, climb ladders, ride mounts, glide (Wind) | BUILT (swim, ride) / RAVEN-PLANNED (glide via Wind) | `WRCharacter.cpp`; ST Wind |
| Name is free text; the hero has no voice lines and no story of their own (Souls-style lore) | GAP (fits DD's "no cutscenes") | |
| Movement: walk 450, run 600, sprint 750 cm/s (5 stamina a second, Raven's SN), instant direction change on the ground, 0.15 s to full speed; jump 1.2 m; mantle ledges up to 1.8 m with a 0.5 s clip; slopes up to 50° walkable; no fall damage under 6 m, lethal over 18 m (except Featherfall) | GAP (speeds today: `tuning.json` `player`) | |
| Every screen works by mouse and by pad; prompts show only for the last device used; F1 opens the controls page for that device; F8 files a bug report with a screenshot | GAP (CAL L4, L10) | |
| Look: real-world attractive faces and athletic builds from explicit MetaHuman presets, skin and hair that hold up in close first-person and in the creator's close-up | GAP (Aaron 11-13) | |

---

## 10. Conflicts and which source wins

| # | Conflict | Sources | Winner |
|---|---|---|---|
| 1 | **Biome size.** Our 11-10 email told Raven "every new biome is the same size: 9 creatures, 1 major, 3 minis". Later that day Aaron said regions vary (6 to 12 creatures, 3 to 5 minis, about 49 km²) | RAVEN-EMAIL-1110 vs FRP 0.4, CAL Part 3 | Aaron's later call: vary. **Raven still has the wrong number in his inbox; send a one-line correction** |
| 2 | **Fire and Frost adjacency.** Raven's island puts them side by side across one ridge with two passes; the wheel puts opposites straight across, so a 12-wedge map separates them | MP, LP vs CB wheel, FRP 0.4 | Aaron's scope (12 biomes). Fix in this doc: a central massif whose passes link opposites directly, keeping Raven's pass idea. Raven signs off the layout (M3 rule) |
| 3 | **Map rebuild vs no wipe.** A new landmass ends existing worlds; Aaron said no wipe for now, Raven decides | FRP 0.4 vs FRP question 3 | Open for Raven. Proposed: grow the island outward and keep today's Fire/Frost ground in place so old worlds survive |
| 4 | **Scope.** Raven's CLAUDE.md and HANDOFF say Fire and Frost only unless Raven asks | CLAUDE.md vs Aaron 11-10 | Aaron (all 12). Raven's release rules stand |
| 5 | **The hero.** Raven planned Manny/Quinn + GASP, shipped a Mixamo survivor; Aaron chose MetaHuman + GASP | M1, `player_art.json` vs Aaron 11-13 | Aaron. Raven asks to be told before anything big; this is big |
| 6 | **"Weapon arts" vs Wildbrands** | ST node text vs CAL 1.8 | Aaron: Wildbrands in the game; CAL itself flags the rename for Raven |
| 7 | **Combat numbers.** Stamina regen 30/s after 0.8 s, block 10 to 25 flat, one Poise for all | SN, M1 vs CAL 1.2, 1.4, 1.6 | Aaron (Elden Ring): 45/s after 0.5 s, guard boost, stance for creatures. All stay data |
| 8 | **Healing.** "Crafted potions only" vs heal spells in five trees | DD vs ST (DR #17) | Open, emailed. Proposed: heal spells stay; potions are everyone's base |
| 9 | **Combos passive or active.** "Automatic, cost no points" vs "combo abilities" and DR #5's active combos | ST, CB vs BP, DR #5 | Open, emailed. Proposed: passive unless a round's design says otherwise |
| 10 | **"Rest" undefined** (Deathless, Void corruption) | ST vs DD (DR #13) | Open, emailed. Proposed: sleep in a bed or bedroll |
| 11 | **Portal timers ruled out vs real-day break timers** | DD Ruled out vs DD How portals work (DR #20) | Open. Proposed: keep break timers; "ruled out" means timed runs |
| 12 | **Traps** ruled out for bases, wanted in dungeons | BL vs DD Portals (DR #21) | Open. Proposed: dungeons yes, bases no |
| 13 | **Raids for 6 to 10 vs no party limits** | DD (DR #22) | Open. Proposed: tuned for, not capped at |
| 14 | **"Arena PvP setting"** in BP vs "PvP arenas" ruled out | BP vs DD | Open. Proposed: it means the built "PvP inside boss arenas" setting |
| 15 | **Mini-bosses: 2 per biome built** vs 3 to 5 | BP First playable vs DD, BP Full release | Raven's newer full-release line: 3 to 5. Fire and Frost each get a third |
| 16 | **Creature raids** "setting only, full release" (DR #31) but shipped in 0.7.0 on a plan Raven approved | DR #31 vs `WRRaids.cpp` | Built wins (Raven approved). DR #31 should be marked changed |
| 17 | **Persistence:** SQLite planned, JSON files built | TECH_PLAN, M7 vs `WRWorlds.cpp` | Built works today; a 100-player dedicated server is when SQLite matters. Raven decides |
| 18 | **Dedicated server** on hold (Raven) vs DD's 10 to 100 player servers and FRP's "prove it in R5" | HANDOFF vs DD, FRP | Raven's on-hold stands until he lifts it; the design requirement stays |
| 19 | **Seasons:** DD lists them under every biome; BP puts them in Dream | DD vs BP | BP (newer, sorted tiers): Dream |
| 20 | **Overheat** "burns you" (old DD) vs no self-damage | DD vs DR #2 | DR #2 (already edited into DD) |
| 21 | **Stone at Hearth 1** vs Building tab's original gate | BL vs DR #33 | DR #33 (built) |
| 22 | **Cross-biome endgame bosses ruled out**, while our raid fill uses fused bosses | DD Ruled out vs this doc 8.3 | Raven's rule wins unless he says raids are different; the fill is flagged |
| 23 | **Beast lord "easier mini-boss taming"** and **Rare finder (mutations)** point at Dream or maybe items | LS vs BP Dream, DD "mutations are a maybe" | BP: mutations are full release, tamable minis Dream; hide Beast lord's half until then |
| 24 | **CLAUDE.md "[not built yet]" nodes** vs changelog "every node works" | CLAUDE.md vs CHANGELOG 10-24 | Check in game; the registry builds 14 ability nodes |
| 25 | **Raven's test suites must stay green** vs Aaron's 11-12 no-checks rule | Raven CLAUDE.md vs Aaron | Aaron for our cook (gates compile, cook, package); not a design matter |

---

## 11. Build order

Maps to `FULL-RELEASE-PLAN.md` rounds. One focus at a time, each one a Development cook for Aaron. Nothing here is released to Raven's players without his OK.

| Round | Build | Biome | Shared systems first needed |
|---|---|---|---|
| R0 | The new MetaHuman hero; combat C0 to C12 on Fire and Frost (F8 report, pad layout, feel, stamina, guard, stance, Wildbrands, creature attack rows, bosses); polish list; Fire/Frost third minis (Cinderhorn, Old Tusker); the 12-wedge map plan | Fire, Frost | Tame whistle screen; hidden dead perks |
| R1 | Build the landmass once; biome kit on every biome (day/night spawns, sub-biomes, shrines and fast travel, path masters, events, weather, fill-in map); roaming minis; tame-punishing moves; respec; body changes | Storm peaks (Lightning) | `combos.json`, shock |
| R2 | Hearth 4 and steel, metal building, structural integrity, salvage, mounted combat, smart groups, saddles and armor, creature levels, revive item, player messages, first invasions | Swamp (Blood & curse) | bleed, curse |
| R3 | Full farming and food, machinery part 1, villages and merchants, summon framework | Ancient forest (Nature) | |
| R4 | Necro raising, corruption, darkness, rest and rested buff, comfort buff, Hearth 5 and starsteel, drills, turrets, soul energy | Graveyard (Necromancy) | decay |
| R5 | Portals (5 ranks, rooms for six biomes), artifacts (unlocks hidden phase 3), blueprints, summoning altars, world events, biome machines and building sets; prove the dedicated server if Raven lifts the hold | Desert (Sun & sand) | blind |
| R6 | Underwater play, swimming mounts and Floeback, boats, fishing | Ocean & reefs (Tide) | soak |
| R7 | Flying mounts, aerial combat, gliding, boss soul forging | Sky islands (Wind) | knockback |
| R8 | Big caverns, breeding with mutations | Fungal caverns (Spore) | poison, rot |
| R9 | Rune sockets; PvP pass (ranks, alliances, explosives, siege, path and tame raiding, bounties and infamy) | Crystal caves (Arcane) | mark |
| R10 | Raids, mythic gear, prestige, leaderboards, achievements, titles, cosmetics, monuments; every pass opens | Corrupted wasteland (Void) | |
| Final | Weakness balance across 12, the full-release gate, dedicated server and load, PSO cache, Shipping build, signing, title (DR #26) | none | |
