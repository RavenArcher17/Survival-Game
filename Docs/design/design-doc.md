# Untitled Survival Game — Design Doc

Oct 21, 2026 · @RavenArcher17

## Overview

A multiplayer open world survival game with Elden Ring style combat, where every biome is its own power path and no biome is the "starter" or "endgame" zone. Players tame creatures, farm, build bases and fight bosses on persistent servers with open-world PvP.

Status: early brainstorming. Nothing here is final. Solo developer, leaning Unreal Engine.

**Core pillars**

- **Biomes are paths, not a ladder.** Volcano, graveyard, tundra and others each grant a different power path. None is best or worst.
- **Main path + subclass.** Players pick a main path and a subclass from another biome, which pulls them across the whole map.
- **Soulslike combat.** Stamina, dodges, parries and bosses with patterns you learn.
- **Survival sandbox.** Ark-style taming, farming, machinery and base building.
- **Mid-to-late game first.** Most of the fun should live in the mid and endgame, without tedious grind.

**Server model:** Ark/Rust style servers of 10–100 players, persistent (no wipes), handcrafted map. Death penalty is a server setting, from gentle respawn to full loot. Zombies are deliberately out.

## Biomes & paths

Twelve candidate biomes, each with its own power path, creatures, signature resource, look and music. The list is a menu; a first version would ship only one or two.

| Biome | Path | What the path does |
| --- | --- | --- |
| Volcanic | Fire | Burn damage, lava-forged weapons |
| Graveyard / catacombs | Necromancy | Raise dead creatures as minions and tames |
| Frozen tundra | Frost | Slows, shatter combos, ice armor |
| Ocean & reefs | Tide | Water magic, underwater breathing, harpoons |
| Ancient forest | Nature | Vines, healing, plant summons |
| Desert | Sun & sand | Blinding light, sandstorms, mirage decoys |
| Storm peaks | Lightning | Chain damage, burst speed |
| Fungal caverns | Spore | Poison clouds, rot, regrowth |
| Crystal caves | Arcane | Shields, reflected projectiles |
| Swamp | Blood & curse | Lifesteal, hexes |
| Sky islands | Wind | Gliding, knockback, aerial combat |
| Corrupted wasteland | Void | Big power that costs max HP |

&#91;embedded content: biome wheel · 12 paths, 6 opposite pairs\]

Opposites sit straight across the wheel and invade each other; neighbours are related themes, so combos between them feel natural.

**Every biome has**

- A signature resource found nowhere else
- Different spawns by day and night, and seasons that change conditions (frozen lakes in winter)
- Hidden sub-biomes, like lava tubes under the volcano
- Shrines that grant path XP when cleansed or claimed
- A path master NPC who teaches the path through quests
- Biome-specific events, such as eruptions or a blood moon over the graveyard

**Hazards are gear checks.** No temperature meter. Certain areas need the right path or gear to survive, which makes paths useful outside combat.

## Classes, builds & leveling

A character has a main path plus one subclass path from a different biome. A cap on total path points means no one masters everything on one character.

- **Combo abilities** for path pairings, such as fire + frost = steam burst.
- **Secret hybrid paths** unlock when two paths reach mastery.
- **Weapons are separate from paths.** Any path can use any weapon type, and the path infuses it, so a fire greatsword plays differently from a necro greatsword.
- **Respec** is possible but costly, through a rare item or a long quest.
- **Your body changes** with your path, like glowing veins for fire or pale skin for necromancy.
- **Gear:** talismans and rings, biome armor set bonuses, and equip load that affects dodge speed.

**Leveling is use-based.** Skills level by doing them (mining levels by mining), plus an overall character level from XP. No Souls-style stat points, and no swappable loadouts.

### How paths work

1. **Pick a main path at character creation.** It decides your spawn biome: pick fire, start in the volcano. The main path grants a passive the subclass doesn't get.
2. **Level paths by using them.** Path XP earns points for that path only. Tree nodes give stats, abilities and other bonuses, which replaces Souls-style stat points.
3. **Unlock a subclass at a boss altar.** Each biome's altar sits in its boss arena, so you beat that boss to unlock its path.
4. **Spend up to 150 points.** No single path can pass 100, so you can max your main and put 50 into a subclass, or split more evenly.
5. **Swap main or subclass at their altars.** Progress is saved per path. A new path starts from zero, but an old one keeps its points when you return. Either slot can be swapped, so players can make their strongest path their main. Bosses respawn, so you may have to beat one again to reach its altar.
6. **Combos come two ways.** Owning two paths unlocks combo abilities, and mixing elements in combat triggers reactions, like fire then frost causing a steam burst.

## Combat & bosses

Combat is Soulslike: you learn attack patterns, wait for openings and manage stamina. Bosses are the main source of progression.

**Combat basics**

- Stamina for attacks, dodges and blocks
- Dodge roll with invincibility frames, parry and riposte
- Lock-on targeting, as an option
- Ranged weapons and mounted combat
- Status effects: bleed, burn, frostbite, poison, rot
- Elemental weaknesses matter, so you bring frost to the fire boss
- Enemy groups that flank, surround and retreat
- Aggressive, neutral and passive mobs

**Bosses**

- Each biome has one major boss and 3–5 mini-bosses
- Multi-phase fights, poise and stagger bars, and arenas with mechanics like rising lava
- Roaming mini-bosses that wander the biome
- Optional secret super-bosses
- Bosses scale with group size and respawn on a server timer
- Summoning altars let players fight a boss on demand by offering items
- Bosses drop path-specific materials used in crafting

### How combat works

- **Abilities:** weapon arts from your infused weapon, plus a few path spells, in 8 quick slots (4 per path) you can reassign outside combat.
- **Resources:** each path has its own resource bar. With two paths you have two bars, and combo abilities spend from both.
- **Healing:** crafted potions only. Limited by belt slots and a short cooldown between drinks.
- **Mob difficulty:** easy near each biome's spawn, brutal deeper in.
- **Boss loot:** everyone who deals at least 10% of the boss's health gets their own loot, including rival clans.
- **Dying in a boss fight:** respawn and run back while others keep fighting.
- **PvP inside boss arenas:** a server setting.

| Path | Resource | How it works |
| --- | --- | --- |
| Fire | Heat | Builds as you attack; high Heat boosts damage; full Heat stops building but never hurts you |
| Necromancy | Souls | Collected from kills; spent to raise and empower the dead |
| Frost | Chill | Fills slowly; spending it all at once triggers bigger shatters |
| Tide | Flow | Builds while moving and dodging; drains when standing still |
| Nature | Growth | Regenerates fast near plants, slow in barren areas |
| Sun & sand | Radiance | Charges in daylight, weak at night |
| Lightning | Charge | Fills fast, empties fast; best in quick bursts |
| Spore | Spores | Planted on enemies, then detonated |
| Arcane | Mana | Steady classic pool |
| Blood & curse | Blood | Spends your own HP; lifesteal earns it back |
| Wind | Momentum | Builds while airborne or sprinting |
| Void | Corruption | Huge power; each use lowers max HP until you rest |

The resources are suggestions, not decisions yet.

## Creatures & taming

Taming is Ark-style, and tames are for fighting, riding and raiding. Base guarding is handled by tames too, so there are no NPC guards.

- Knockout-and-feed taming, plus other methods: stealing eggs, offering food, path magic
- Necromancers raise dead creatures instead of taming living ones
- Breeding with stat inheritance; mutations are a maybe
- Creature levels and skill trees
- Flying, swimming, pack and hauling mounts
- Creatures with biome resistances, like a fire lizard that walks on lava
- Creatures that sense ore or secrets
- Craftable saddles and creature armor
- Tamable mini-bosses, very hard to get
- Tames can die permanently, but a rare item can revive them
- Tame limit per player, growing with the Taming skill

**Living world:** neutral herds defend each other, and predators hunt prey whether or not players are nearby.

### How taming works

- **Taming time varies by creature:** small creatures tame fast, big ones slowly.
- **Elements:** tames keep their own biome element; your path doesn't change them.
- **Necromancy:** quick temporary raises in combat, paid with Souls, plus permanent undead tames through a ritual. Combat raises don't count against the tame limit.
- **Active tames:** the number you can have out at once grows with a taming skill.
- **Boss fights:** any tame can join. Each tame raises boss health like an extra player, and bosses have attacks that punish or scatter tames.
- **Breeding:** Ark-style eggs or pregnancy and raising babies. Cross-biome pairs can make hybrid creatures.

## Survival, farming & food

Survival pressure is real but light: food keeps you alive, and buffs come from potions and brews instead of meals.

**Survival**

- Hunger and thirst
- Rested buff from sleeping, plus bedrolls and respawn points
- Hunger is light: eat rarely, but going without has moderate penalties
- Durability on tools, weapons and armor, repaired at a station
- Darkness is dangerous, so caves need light
- Weather that matters: storms, blizzards, heat waves
- A corruption level that builds in dark biomes like the graveyard or void. It gives minor debuffs, or minor buffs if the biome matches your path
- Carry weight limit

**Farming & food**

- Crops with growth stages, where common crops grow fast and rare ones slowly, biome-specific crops, irrigation, and greenhouses for off-biome crops
- Animal husbandry, beekeeping, and biome-specific fishing
- Cooking stations, and recipes you discover by experimenting
- Food spoilage and preservation: salting, smoking, ice boxes
- Alchemy, potions and brewing, where the buffs come from
- A farming skill tree for bigger yields and rare seeds

## Crafting, machinery & building

Everyone shares one crafting tree. Path materials from bosses decide what an item becomes, so one sword recipe can make a fire sword or a frost sword.

**Crafting**

- Stations level up together through the claim's Hearth, from tech level 1 to 5 (see the Crafting tab)
- Blueprints from bosses and dungeons
- Quality tiers from crude to masterwork, with random stat rolls
- Weapon upgrades from +1 to +10, plus gem and rune sockets
- Salvage gear for materials, and crafting queues that run while you're away
- Siege weapons and boats

**Machinery**

- Water wheels and windmills, conveyors and auto-sorting storage, automated drills
- A power system: water, wind, geothermal lava, soul energy
- Biome-themed machines, like a lava forge or a necro soul engine

**Building**

- Both snap-based and freeform building
- Material tiers (wood, stone, metal, biome materials) with structural integrity
- Biome-themed building sets and decorations that give a comfort buff
- Land claim totems, base upkeep, doors and code locks
- Powered turrets and creature raids on bases
- Bases built on large tames

### How crafting and machinery work

- **Element at crafting:** adding a path material to a recipe sets the item's element. It can't be changed later.
- **Quality:** crafting skill, station tier and material rarity all raise the odds of a better tier.
- **Power runs:** crafting stations (faster crafting), drills and conveyors, turrets and lights, and greenhouses.
- **Automation:** medium. Machines can feed each other in simple production chains, without full factory lines.

## Multiplayer, PvP & raiding

PvP is open everywhere except safe zones, and each server sets its own death penalty. A PvE-only server option exists for players who don't want PvP.

| Death setting | What you lose |
| --- | --- |
| Gentle | Little or nothing; respawn |
| Souls-like | XP or currency, recoverable at your death spot |
| Drop inventory | Carried items; your base is safe |
| Full loot | Everything carried, and others can loot it |

- Safe zones at spawn and trading hubs
- Offline raid protection; bases can be raided at any time when owners are online
- Clans or tribes with ranks, and alliances between clans
- Anti-zerg limits on clan size
- Explosives and raid tools, plus path-based raiding: fire burns wooden walls, necromancers raise the dead inside enemy bases
- Tames used in raids
- Bounties on player killers, and infamy that marks aggressive players
- Contested bosses that rival clans can fight over

### How bases and raiding work

- **Land claim totem:** only your clan can build within its radius. Upgrade the totem for a bigger radius.
- **Upkeep:** paid in building materials stocked in the totem. When it runs out, the claim drops and the base slowly decays until restocked or gone.
- **Offline protection:** bases take reduced damage while all owners are offline. How much is a server setting.
- **Raid tools:** explosives, siege weapons, path abilities and tames can all break in.
- **Building tiers:** higher tiers have more health, with no elemental resistances.
- **Creature raids:** local biome creatures attack bases at random.

## Dungeon portals

Small portals spawn at random in each biome and lead to short dungeon crawls. Dungeons are built from handmade rooms shuffled into new layouts, with random modifiers on top.

- Each portal is themed by its biome, and its rank shows as a color
- Modifiers change each run, such as no healing or double damage
- Puzzles and traps, not just combat
- A boss at the end of every portal
- Craftable keys spawn a specific portal on demand
- Legendary portals are announced to the whole server
- Dungeon breaks: ignored portals spill monsters into the world
- An endless tower portal with a leaderboard

Rewards are loot and materials; there is no separate dungeon currency. There is no forced party size limit. A portal only closes when its boss escapes.

### How portals work

Portals come in five ranks. Higher ranks spawn deeper in a biome, take longer to run, and take longer to break.

| Rank | Real days until it breaks |
| --- | --- |
| Common | 1 |
| Uncommon | 3 |
| Rare | 6 |
| Epic | 10 |
| Legendary | 18 |

- **Breaking:** an ignored portal escalates, spilling monsters first. When its boss escapes, the portal closes.
- **Escaped bosses:** no new portal of that rank can spawn in that biome until the escaped boss is killed.
- **Entering a breaking portal:** allowed until the boss escapes; clearing it stops the break.
- **Access:** key portals belong to whoever opened them; wild portals are first come.
- **Keys:** craft a key to spawn a portal of a chosen rank.
- **Death inside:** server death rules apply. If the portal closes, bodies appear where the entrance was.

## World, lore & events

The map is handcrafted and persistent, and lore is told Souls-style through items and ruins rather than cutscenes. Each biome has its own tone, from dark fantasy in the graveyard to brighter magic in the sky islands.

- Lore in item descriptions, and ruins with environmental storytelling
- NPC villages and factions per biome, plus traveling merchants
- Hidden areas and secret paths, and a map you fill in by exploring
- Fast travel through discovered shrines
- Player messages left in the world, like Souls notes
- World events, such as meteor showers that drop rare ore
- **Invasions between opposite biomes:** the pairs are fire and frost, lightning and blood, sun and tide, nature and necromancy, wind and spore, arcane and void.

## Endgame & progression

Endgame is sized for groups, not the whole server. Difficulty is personal: artifacts on your gear make you or your group take more damage in exchange for better rewards, so no player can make a biome harder for everyone else.

- **Artifact modifiers:** equipment modifiers that raise difficulty and rewards for the player or group wearing them
- Raids for 6–10 players
- Mythic gear that needs materials from every biome
- Legendary weapons with unique movesets
- Boss soul forging, turning boss drops into unique items
- Prestige: reset a path for permanent perks
- A god path unlocked after mastering several paths
- Boss kill time leaderboards, achievements, titles and cosmetics for hard content
- Boss trophies as endgame monuments for your base

### How artifacts work

- **Source:** dropped by bosses and found in dungeon portals.
- **Slots:** three dedicated artifact slots per character.
- **Group rule:** in a party, the damage and loot multipliers are shared, using the highest in the party. Weird rules apply only to the player wearing them.
- **Penalties:** take more damage, plus weird rules.
- **Rewards:** more loot, better rarity, more path XP, and exclusive drops at high artifact levels.
- **PvE only:** artifacts never change damage from other players.

**Example weird rules** (suggestions)

- No dodge roll
- Enemies you kill explode
- Your own path element hurts you
- Healing potions heal half
- Bosses use their final-phase attacks on you from the start
- Your light radius is halved

## Ruled out, open questions & next steps

**Ruled out for now**

- Zombies
- Souls-style stat points and swappable build loadouts
- Temperature system, food buffs or food-based stamina, injuries, disease and natural disasters
- Soil quality, fertilizer, pests and blight
- Separate crafting tree per path
- Traps, underground bases, NPC thralls, and bases decaying while offline (upkeep is enough)
- Raid windows, territory control points, PvP arenas and scheduled sieges
- Procedural dungeon layouts, portal timers, party limits, PvP portals and dungeon currency
- Procedural maps, seasonal wipes and treasure maps
- Server-wide boss events, a server-ending world boss and cross-biome endgame bosses
- World-wide biome escalation, replaced by personal artifact modifiers
- Lore ending for mastering paths, weekly challenges, mentor system, and first-kill announcements

**Open questions**

- None right now

**Next steps**

- [ ] Work through how each system works, one at a time
- [ ] Sort ideas into first playable, full release and dream list
- [ ] Build a first playable slice: two biomes (volcanic and frozen tundra), 2 major bosses and 4 mini-bosses, taming, building and a dedicated server
