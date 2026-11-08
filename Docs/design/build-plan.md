# Survival Game Build Plan

Oct 22, 2026 · @RavenArcher17

Every idea from the design doc, sorted into three tiers so a solo developer knows what to build first.

## At a glance

&#91;embedded content: build roadmap · 3 tiers, 2 gates\]

Each tier starts only once the gate before it is passed; the diamonds are those gates.

## First playable

Two opposite biomes, fire and frost, on one small server. The goal is a session your friends can actually play: spawn, fight, tame, build, and beat a boss together.

Fire and frost are the clearest opposites on the wheel, and they show the steam burst combo straight away.

| System | What's in |
| --- | --- |
| Biomes | Volcanic and Frozen tundra, small handcrafted map, signature resource each, one gear-check hazard each |
| Paths | Fire and frost paths; main picked at creation sets your spawn; subclass unlocked at the other biome's boss altar; 150-point cap, 100 per path; swapping with saved progress |
| Leveling | Path XP from using abilities; a few stat and ability nodes per tree; character level |
| Combat | Stamina, dodge roll with i-frames, parry, optional lock-on; weapon arts plus 2–3 spells per path; Heat and Chill resources; burn and frostbite; steam burst combo |
| Bosses | 1 major boss and 2 mini-bosses per biome; boss drops path materials; 10% damage loot rule; rejoin after death |
| Enemies | Aggressive, neutral and passive mobs; difficulty rising deeper into each biome |
| Taming | Knockout-and-feed taming, a few ground mounts, tames that fight with you; tame limit |
| Survival | Hunger and thirst, crafted healing potions, durability and repair station, carry weight, bedrolls, basic farming (crop plots and animal pens) |
| Crafting | One shared tree; path material sets the element; Hearth tech levels 1–3 (copper, bronze, iron); basic weapons, armor and potions |
| Building | Snap building, wood and stone tiers, land claim totem, doors and code locks, upkeep in materials, freeform placement for utility and decor, span-based support (wood 4, stone 6) |
| Multiplayer | Dedicated server for 10–100 players, open PvP, safe zone at spawn, death penalty as a server setting, PvE-only option, small clans |

**Gate to pass:** a group of friends plays a full evening on the server and wants to come back.

### Where it stands

Checked against the game on Nov 8, 2026, after v0.7.0. Every system above is built except the dedicated server, and the gate hasn't been tried yet.

| System | Status | Notes |
| --- | --- | --- |
| Biomes | Done | Volcanic and Frozen tundra on one island; lava and ice gear checks; six caves |
| Paths | Done | Fire and frost; subclass at the other biome's boss altar; point caps; swapping keeps progress |
| Leveling | Done | Path XP from abilities, tree nodes, character level |
| Combat | Done | Stamina, dodge, parry, lock-on, spells, Heat and Chill, burn, frostbite, steam burst |
| Bosses | Done | Six bosses (1 major and 2 mini per biome), 10% loot rule; since v0.7.0 they chase you up to 90 m and walk home |
| Enemies | Done | Aggressive, neutral and passive; spawn regions get harder inland |
| Taming | Done | Knockout-and-feed, ground mounts, tames fight with you, tame limit |
| Survival | Done | Hunger, thirst, potions, durability and repair, carry weight, beds, crop plots, animal pens |
| Crafting | Done | One shared tree, path material sets the element, Hearth tech levels 1–3 |
| Building | Done | Snap and freeform placement, wood and stone, claim totem, upkeep, span support. Code locks only worked from the console until a door keypad was added on 8 Nov (built and tested, released in 0.7.1) |
| Multiplayer | Partly | Open PvP, safe zone, death penalty and PvE settings, clans. The host's PC is the server: no packaged dedicated server yet (on hold), and nothing tested past a few players |

**Gate:** not tried yet. It needs a full evening on the server with a group of friends.

**Built early from the full release:** ranged weapons, crafting quality and upgrades, gem sockets, crafting queues that run while you're away, creature raids, and the offline protection setting.

## Full release

The rest of the design doc, built on top of the first playable. Biomes can arrive in opposite pairs, so each update adds a new rivalry.

| System | What's added |
| --- | --- |
| Biomes | The other 10 biomes in opposite pairs; day/night spawns; hidden sub-biomes; shrines; path masters; biome events |
| Paths | All 12 paths and resources; owned-pair combo abilities; main path passives; body changes per path; respec rules finished |
| Combat | Ranged and mounted combat; all status effects; smart enemy groups; elemental weaknesses tuned across 12 paths |
| Bosses | 1 major boss and 3–5 mini-bosses per biome; multi-phase fights; arena mechanics; roaming mini-bosses; summoning altars; group scaling; bosses that punish tames |
| Taming | Other taming methods; necro raising, both quick and ritual; flying, swimming and pack mounts; creature levels and skill trees; breeding with mutations; perma-death and revive item; creature armor and saddles; ore-sensing creatures |
| Survival | Corruption level in dark biomes; weather that matters; darkness in caves; rested buff |
| Farming | Crops with varied growth times; biome crops; irrigation; greenhouses; animal husbandry; beekeeping; fishing; cooking and recipe discovery; spoilage; alchemy and brewing; farming skill tree |
| Crafting | Upgradable stations to tier 5; quality tiers and stat rolls; +1 to +10 upgrades; gem and rune sockets; salvage; queues that run while away; blueprints; siege weapons; boats |
| Machinery | Water wheels, windmills, power system, conveyors, sorting, drills, medium production chains, biome-themed machines |
| Building | Freeform structural pieces; metal and biome material tiers; structural integrity; themed building sets; comfort buff; powered turrets; random creature raids; offline protection setting |
| PvP | Clan ranks and alliances; anti-zerg limits; explosives and raid tools; path-based raiding; tame raiding; bounties and infamy; contested bosses; arena PvP setting |
| Portals | Random portals in 5 ranks; handmade rooms shuffled with modifiers; puzzles and traps; end bosses; keys; dungeon breaks and escaped bosses; legendary announcements |
| World | NPC villages and factions; traveling merchants; lore in items and ruins; fill-in map; shrine fast travel; player messages; world events; opposite-biome invasions |
| Endgame | Artifact modifiers with 3 slots; raids for 6–10 players; mythic gear from every biome; boss soul forging; prestige; leaderboards; achievements, titles and cosmetics; boss trophies |

**Gate to pass:** every biome has its boss, path and portal, and players have a long-term goal after maxing two paths.

## Dream list

Ideas that stay in the game's future but cost a lot of art, animation or balancing time. Each one is worth building only once the game has players.

| Idea | Why it waits |
| --- | --- |
| Fully new hybrid creatures from cross-biome breeding | Each needs its own model, animations and abilities; early hybrids can reuse a parent's body |
| Secret hybrid paths | A third ability tree for every valid pair multiplies balancing work |
| God path | Needs every path finished and balanced first |
| Legendary weapons with unique movesets | Each moveset is a full animation set |
| Optional secret super-bosses | Hand-built fights on top of 12 biome bosses |
| Tamable mini-bosses | Mini-bosses need tame behavior, saddles and balance |
| Bases built on large tames | Moving bases break building, physics and raiding rules |
| Seasons that change biomes | Every biome needs seasonal versions |
| Endless tower portal with leaderboard | Needs a large room pool and anti-cheat for rankings |

## Why this order

- **Multiplayer comes first.** Networking is the hardest thing to add late, so the first playable runs on a real dedicated server from day one.
- **Combat comes before content.** If one boss fight feels good, 12 biomes are worth building; if it doesn't, more biomes won't fix it.
- **Two opposite biomes prove the core idea.** Fire and frost test paths, subclasses, combos and boss altars with the least art.
- **Systems that multiply content wait.** Portals, artifacts, raids and breeding are strongest once there are many biomes, bosses and creatures to feed them.
- **Biomes ship in opposite pairs.** Each update adds a rivalry and an invasion pair, which gives players a reason to return.
