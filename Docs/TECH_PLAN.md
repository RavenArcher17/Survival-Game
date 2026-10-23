# Wildreach: First Playable technical plan

Source of truth for *what* to build: Raven's design docs (still being written):
- Build plan: https://claude.ai/artifact/RexKaP5rCQmTdw3vmUfzbq
- Design doc: https://claude.ai/artifact/VQd91XUksbVNsMTZx8EP7k

This file is *how* to build it. As of 2026-10-22 every First Playable topic is designed
(including the Building, Server settings and Starting numbers tabs). Open inconsistencies are in
DESIGN_REVIEW.md. Re-read the docs before each milestone.

**First Playable gate:** a group of friends plays a full evening on the server and wants to come back.

---

## Architecture decisions

| Area | Decision | Why |
|---|---|---|
| Abilities, resources, status effects | **Gameplay Ability System (GAS)**, built-in, free | Replicated + predicted abilities, attributes (Heat, Chill, Health, Stamina...), tag-driven statuses (Burn, Frostbite, Frozen) and combos are exactly what GAS is for. Avoids hand-rolling netcode for 12 paths × ~20 nodes. |
| Survival stats | Move `UWRSurvivalComponent` into a GAS `AttributeSet` in M1 | One damage/heal pipeline for combat, hunger, hazards. |
| Game data | **Text data in the repo** (`Data/*.json` or CSV) imported into DataTables / Primary Data Assets by a Python editor script | Skill trees, combos, creatures, recipes, bosses come straight from the docs; Claude can author and diff them without the editor UI. |
| Identity | `GameplayTags` for paths, elements, statuses, creature roles, stations | Data-driven checks ("is Frozen", "path.Fire") without hard-coded enums. |
| Creature AI | `StateTree` + AI Perception, server-only | Built-in, data-driven, scales to 18 creatures from ~5 behavior archetypes. |
| Persistence (no wipes) | Server-side **SQLite** (engine's `SQLiteCore`) for players, tames, bases, claims | Survives crashes better than SaveGame blobs; easy to inspect. |
| Networking scale (10–100) | Push-model replication, net relevancy/dormancy for buildings and tames; evaluate Iris before M7 | Buildings and tames are the actor-count problem, not players. |
| World | 4×4 km World Partition, Landscape from a Gaea/heightmap, foliage + rocks via PCG | Two biomes on one map; handcrafted layout, procedural dressing. |
| Combat hit detection | Weapon traces during animation windows, run by server; client plays predicted effects | Server-authoritative PvP without trusting clients. |

### $0 art strategy (biggest risk)
The First Playable needs a humanoid player, 18 creatures, 2 big bosses and 4 mini-bosses.
- Player: UE5 Manny/Quinn + Epic's free **Game Animation Sample** (locomotion, dodge) + Mixamo for attacks.
- Creatures: reuse a few rigs across many creatures (quadruped, six-leg, flyer, worm, small critter), tinted and scaled, with Fab free packs where licensing allows. Build gameplay on greyboxes first; swap art late.
- Bosses: greybox until combat feels right (design rule: "combat before content").

---

## Milestones

Each one ends with a playable check on a dedicated server with 2+ clients.

### M0 Foundations (done 2026-10-22, commit c5d790a)
Project, FP/TP character, predicted sprint, survival stats, respawn, WP test map, `play.ps1`.
Remaining: headless bot clients for load tests; SQLite save skeleton.

### M1 Combat core (the "does it feel good" milestone)
- GAS setup: ability system on PlayerState, attribute sets (Health, Stamina, Poise, Heat, Chill, Hunger, Thirst).
- Real character mesh + animations; equip load affects dodge.
- Stamina, dodge roll with i-frames, parry/riposte, optional lock-on, light/heavy attacks.
- Status effects: Burn, Frostbite → Frozen, Shatter. Stagger/poise.
- Fire trunk (Fire bolt, Flame slash) + Frost trunk (Frost shard, Ice slash); Heat/Chill bars.
- Fire+Frost combos: **Thaw** (25/25) and **Balance** (50/50).
- Target: training dummies + one Cinderfang pack in a test arena.
- **Gate:** a 1v3 fight vs Cinderfangs and a 1v1 duel both feel good.

### M2 Paths & progression
- Path XP from ability use; points per path; **150 total / 100 per path** cap.
- Full Fire and Frost trees as data (trunk 10 + 3 branches × 30, capstones 10).
- Main path picked at creation sets spawn biome; root passive only on main path.
- Subclass unlock at the other biome's boss altar; swapping keeps saved per-path progress.
- Character level; skill-tree UI (UMG).

### M3 World
- 4×4 km map with Volcanic and Frozen tundra regions, difficulty rising with depth.
- Signature resource per biome; hazard gear checks (lava, slick ice), not a temperature meter.
- Caves and mines with depth-scaled ore (copper → iron for First Playable).
- Spawn safe zone(s), boss arenas (Vulkarn's forge cavern, Skaldrith's mountain bowl).

### M4 Creatures & taming
- Behavior archetypes: aggressive hunter, pack hunter, flyer, neutral herd (defend each other), passive grazer.
- All 18 First Playable creatures as data on those archetypes (Floeback may slip to Full release, as the doc notes).
- Taming: knockout-and-feed, offer food, steal egg, path magic. Tame follow/stay/attack; ground mounts; tame limit.
- Tames count as an extra player for boss HP scaling.

### M5 Survival, crafting, building
- Inventory with carry weight, hotbar, potion belt + cooldown (healing = crafted potions).
- Hearth tied to land claim totem; tech levels 1–3 reachable (two major bosses); four stations
  (forge, armorer, alchemy table, workbench); copper/bronze/iron recipes; quality + sockets.
- Durability + repair at the matching station; bedrolls as respawn points.
- Snap building (wood, stone), doors, code locks, upkeep in materials, Hearth damage by raiders.
- Life skills that touch First Playable systems (mining, woodcutting, smithing, armorcraft, alchemy, building, taming).

### M6 Bosses
- Vulkarn (2 phases + hidden phase 3), Skaldrith (2 phases + hidden phase 3).
- Obsidian Warden, Ignessa, Grimjaw, Pale Hunter.
- Group-size HP scaling, 10% damage loot rule, boss cores / path materials / boss parts, mini-boss upgrade shards, respawn timers, rejoin after death.

### M7 Server & ship-to-friends
- Open PvP + spawn safe zone, PvE-only toggle, death penalty server setting, small clans.
- Server config file (ini) for all server settings.
- Source-built engine → packaged Windows/Linux dedicated server.
- $0 hosting: own PC first; then a free-tier cloud VM if it fits.
- Load test with bot clients toward 100 players.

---

## Detailed milestone plans
| Milestone | File | Review items it uses (all decided 2026-10-22) |
|---|---|---|
| M1 Combat core | [plan/M1_combat.md](plan/M1_combat.md) | #1, #2, #5, #6, #7 |
| M2 Paths & progression | [plan/M2_paths.md](plan/M2_paths.md) | #8, #9, #35, #36 |
| M3 World | [plan/M3_world.md](plan/M3_world.md) | #12, #15 |
| M4 Creatures & taming | [plan/M4_creatures.md](plan/M4_creatures.md) | #4/#34, #14, #16 |
| M5 Survival, crafting, building | [plan/M5_survival_crafting_building.md](plan/M5_survival_crafting_building.md) | #3, #11, #27–#30, #33 |
| M6 Bosses | [plan/M6_bosses.md](plan/M6_bosses.md) | #10 |
| M7 Server & ship | [plan/M7_server.md](plan/M7_server.md) | #31, #32 |
| Data formats | [plan/DATA_SCHEMAS.md](plan/DATA_SCHEMAS.md) | — |

Suggested order: M1 → M2, with M3 terrain work in parallel; then M4 → M5 → M6 → M7.
Persistence and the bot load-test harness (both in M7) should start during M2 so every later milestone is tested with them.

## Open questions & inconsistencies
[DESIGN_REVIEW.md](DESIGN_REVIEW.md): every First Playable item was decided with Raven on 2026-10-22, and those
decisions override the design docs where they disagree. Items for later biomes stay open there.
