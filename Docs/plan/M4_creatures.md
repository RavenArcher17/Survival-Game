# M4: Creatures & taming

**Goal:** both biomes are alive with their 9 creatures each, and players can tame, ride and fight beside them.

**Gate:** a player tames one creature of each method (knockout-and-feed, offer food, steal egg, path magic),
rides an Ashstrider or Frostantler through a pass, and fights a Cinderfang pack with a tamed Rimeclaw.

Depends on: M1 (combat, GAS on creatures), M3 (spawn regions; a test map works until then).

## Roster (from the Creatures tab)
**Volcanic:** Magmaw, Cinderfang, Scorchwing (aggressive) · Basaltback, Emberhorn, Kilnmite (neutral) · Ashstrider, Glowmoth, Pumicehog (passive)
**Tundra:** Rimeclaw, Glacimaw, Gloomhowl (aggressive) · Woolly drudge, Frostantler, Rimeshell (neutral) · Snowpuff, Aurora fox, Floeback (passive)

Floeback (swimming mount) may wait until Full release, as the Creatures tab suggests.

## Archetypes (one StateTree + C++ task set each; creatures are data on top)
| Archetype | Creatures | Key behaviours |
|---|---|---|
| Ground predator | Magmaw, Rimeclaw | ambush (Magmaw from lava pools), charge, bite/claw combos, retreat at low HP |
| Pack hunter | Cinderfang, Gloomhowl | pack leader + flankers; Gloomhowl hunts by sound (noise events, sneaking matters) |
| Burrower | Glacimaw, Kilnmite | bursts out of terrain, tunnels; Kilnmite mines ore and senses veins |
| Flyer | Scorchwing | dive attacks; has to be brought down before taming |
| Herd grazer | Basaltback, Emberhorn, Woolly drudge, Frostantler, Rimeshell, Ashstrider | wander/graze; neutral ones defend each other when one is attacked |
| Critter | Pumicehog, Snowpuff, Glowmoth, Aurora fox | flee; utility tames (cave light, secret sensing) |
| Swimmer | Floeback | water nav; optional for First Playable |

All creatures resist their home biome's hazard (`Resist.Lava` / `Resist.Ice` granted by the creature's GE).

## Taming
| Method | Flow |
|---|---|
| Knockout and feed | Torpor attribute (blunt weapons, tranq bolts and frost path add torpor); at max → Unconscious. Feed preferred food from inventory → taming progress over time (scales with creature size). Damage interrupts and resets progress. |
| Offer food | Passive/neutral: approach slowly (sneak), hold food, interact for a short channel. |
| Steal an egg | Nest actor guarded by adults (Kilnmite nests swarm); egg is an item → incubate at base → hatchling grows. |
| Path magic | Same-element path ability held on a creature of that element (Fire on Magmaw, Frost on Rimeclaw) fills taming meter; costs that path's resource. |

## Tames
| System | Detail |
|---|---|
| Ownership | Tame stores owner character ID (and clan ID when clans exist). Server-authoritative orders: follow, stay, guard area, attack target, passive/neutral/aggressive stance. |
| Tame limit | Proposal: **per player**, active (out in the world) cap from the Taming life skill. Total stored tames higher. Needs Raven's number. |
| Mounts | Ground mounts first (Ashstrider fastest, Emberhorn charge, Frostantler fast on ice). Rider attaches to saddle socket; mount uses its own CMC, rider's input drives it; mounted combat is Full release. |
| Haulers | Basaltback, Woolly drudge carry inventory (carry-weight buff). |
| Utility | Glowmoth light aura, Kilnmite/Rimeshell ore digging, Aurora fox secret ping. |
| Boss interaction | A tame in a boss arena counts as +1 player for boss HP scaling. |
| Death | Tame permadeath (revive item is Full release). |
| Persistence | Tames saved in SQLite: species, stats, level, owner, location, inventory. Despawned tames (stored at a base) don't tick. |

## Performance (100-player servers)
- AI ticks on server only; LOD by distance to nearest player: full StateTree < 150 m, simplified 150–500 m, dormant beyond.
- Net dormancy for idle tames; replicate movement at reduced rate when far from viewers.
- Spawn budget per region; creature counts tuned with a 100-bot load test (M7).

## Art
Five shared rigs (quadruped, six-legged, flyer, worm, small critter) cover 17 of 18 creatures. Greybox shapes first; Fab free creature packs or Blender kitbashes later.
