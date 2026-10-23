# M5: Survival, crafting, building

**Goal:** the sandbox loop: gather → craft at the Hearth's stations → build a base → protect it.

**Gate:** two players gather copper, craft copper gear and minor healing potions, build a wood base
with a land claim totem, Hearth, door and code lock, and survive a night. The base and the Hearth are
still there after a server restart.

Depends on: M1 (GAS damage/potions), M3 (resources and caves). **Building pieces are still being designed:**
section 4 is the framework only, and its piece list waits for Raven.

## 1. Inventory & items
| System | Detail |
|---|---|
| Item definitions | `UWRItemDefinition` data assets from `Data/items/*.json`: id, category, weight, stack size, durability, station/tier, element |
| Item instances | Struct with def ref, quantity, durability, quality, stat rolls, sockets. Replicated as a **FastArraySerializer** (only changed slots replicate). |
| Containers | Player inventory (carry weight → over-encumbered slows and blocks dodge), hotbar, potion belt, storage boxes, tame saddlebags |
| Transfers | Server RPC with source/target/slot validation; distance checks for world containers |
| Drops | Death drop behaviour comes from the death-penalty server setting (M7) |

## 2. Survival
| System | Detail |
|---|---|
| Hunger & thirst | "Light" pressure: already in M1 as periodic GEs. Food restores hunger only; buffs come only from alchemy. |
| Healing | Crafted potions only, limited by belt slots and a cooldown |
| Durability | Weapons/armor/tools lose durability per use; repair at the matching station for materials |
| Bedrolls | Placeable respawn point (one active per player) |
| Darkness | Caves are dark (Glowmoth light, torches) |

## 3. Crafting (from the Crafting tab)
| System | Detail |
|---|---|
| Hearth | One per claim, linked to the land claim totem. Holds boss cores; its **tech level** sets every station's tier in that claim. |
| Tech levels | Level N needs cores from N's count of *different* major bosses (0/1/2/4/6). First Playable has two major bosses, so the cap is **level 3**. |
| Stations | Forge, armorer, alchemy table, workbench; all use the Hearth's tier |
| Recipes | One shared tree: base metal sets tier (copper/bronze/iron in First Playable), a path material sets the element, extras add bonuses. `Data/recipes/*.json`. |
| Quality | Crude/Common/Fine/Masterwork → 0/1/2/3 sockets; odds from crafting skill + Hearth level + material rarity; stats roll within quality |
| Extras | Gems (caves), runes (crafted/found), boss parts (major bosses). Removing one destroys it. |
| Upgrades | +1…+10 with mini-boss shards; the Hearth level caps it (+2/+4/+6 at levels 1/2/3) |
| Queues | Craft queue on the station runs server-side while the player is away (timestamps, not ticking) |
| Raids | A broken Hearth stops crafting until it's repaired; its cores are safe |

## 4. Building (from the Building tab)
| System | Detail |
|---|---|
| Placement | Client ghost preview → server validates (claim, overlap, support, cost) → spawns piece |
| Snapping | Piece sockets (foundation edges, wall tops, floor edges, doorframes) with rotation steps; data-driven per piece. Freeform placement for utility/decor (see review #28). |
| Materials | Wood at Hearth level 1, stone at level 2 (metal L4 and biome L5 are Full release). Higher tier = more HP, no elemental resists. |
| Span rule | A piece is supported if it's within N pieces of a foundation or pillar: **wood 4, stone 6** (metal/biome 8). Support recomputes on placement/destruction; unsupported pieces collapse. |
| Core pieces | Foundations, walls, half walls, floors, roofs, stairs, pillars, doors, windows |
| Defense | Palisade walls, gates, guard towers, reinforced doors with code locks. No spikes or traps. |
| Utility | Storage chests, beds, bedrolls, the four crafting stations, the Hearth, torches and braziers, animal pens, crop plots (see review #27) |
| Decor | Furniture, rugs, banners, boss trophies (comfort buff: review #30) |
| Doors & code locks | Lock state server-only; code entry by RPC with rate limiting |
| Land claim totem | Only your clan builds inside. **30 m radius**, +10 m per totem upgrade up to **60 m**. Unlimited claims per player/clan; each has its own upkeep and can hold one Hearth that levels separately. |
| Upkeep | Building materials stocked in the totem, scaled by piece count in the claim × the server's upkeep multiplier. Out of upkeep → claim drops → base slowly decays. |
| Damage | Pieces have HP; tools/explosives later; fire path burns wood (path raiding is Full release) |
| Replication & persistence | Piece actors use net dormancy (wake on damage/interaction); saved in SQLite as rows {claim, piece def, transform, HP, tier}. Large bases → evaluate ISM batching per claim. |

## 5. Life skills (First Playable subset)
Mining, Woodcutting, Smithing, Armorcraft, Alchemy, Building, Taming (and Tracking/Sneaking if cheap).
1–100 by doing, small per-level boost, a choice of 2 perks at 25/50/75/100. `Data/lifeskills.json` from
the Life skills tab; perks as GEs/tags. XP curve from Raven's starting numbers.

## Numbers (from the Starting numbers tab)
- Hunger full → empty in 60 min, thirst in 40 min; at empty, stamina regen halves, then health slowly drains.
- Life skill level n costs 50 + 5n XP (~30,000 to 100, ~50 h per skill).
- Taming at skill 1: easy 1 min, medium 5 min, hard 15 min; up to 50% faster at skill 100.
- Crops: common 20 min, rare 2 h real time.

## Still missing (for tuning, not blocking)
Piece HP and costs, item weights, durability, per-piece upkeep cost. These get placeholders in `Data/tuning.json`.
