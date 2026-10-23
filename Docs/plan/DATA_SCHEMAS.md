# Data schemas

All game content lives as JSON under `Data/` and is imported into Unreal data assets by
`Tools/import_data.py` (editor Python, runnable headless). JSON is the source of truth; the generated
`.uasset`s are never hand-edited. Ids are `snake_case` and stable forever (save files reference them).

Numbers marked `null` in the files are **waiting on Raven's starting numbers**; the importer fills
them from `Data/tuning.json` defaults and logs a warning so they're easy to find.

## Path: `Data/paths/<id>.json`
```json
{
  "id": "fire",
  "name": "Fire",
  "biome": "volcanic",
  "resource": { "id": "heat", "name": "Heat", "max": null, "decayPerSecond": null },
  "rootPassive": { "id": "burning_blood", "name": "Burning blood",
                   "text": "Immune to your own burns and to lava damage.", "grants": ["GE_BurningBlood"] },
  "trunk": [
    { "id": "fire_bolt", "name": "Fire bolt", "kind": "ability", "cost": 3, "grants": ["GA_FireBolt"],
      "text": "Basic spell" },
    { "id": "heat_capacity", "name": "Heat capacity", "kind": "stat", "ranks": 3, "grants": ["GE_HeatCapacity"],
      "perRank": { "MaxHeat": null } }
  ],
  "branches": [
    { "id": "inferno", "name": "Inferno",
      "nodes": [
        { "id": "spreading_flames", "kind": "ability", "cost": 3, "gate": 0, "grants": ["GA_SpreadingFlames"] },
        { "id": "wildfire", "kind": "capstone", "cost": 10, "gate": 20, "grants": ["GA_Wildfire"] }
      ] }
  ]
}
```
- `kind`: `stat` (1 point per rank, `ranks` > 1), `ability` (`cost` points), `capstone` (10, ends branch).
- `gate`: points already spent in this branch needed to buy the node (proposed prerequisite rule).
- Importer validates: trunk sums to 10, each branch to 30, capstone last.

## Combo: `Data/combos.json`
```json
[{ "paths": ["fire", "frost"], "wheel": "opposites",
   "at25": { "id": "thaw", "name": "Thaw", "text": "Burning a frozen enemy deals a big burst of damage", "grants": ["GA_Combo_Thaw"] },
   "at50": { "id": "balance", "name": "Balance", "text": "Heat and Chill fill each other as you use them", "grants": ["GA_Combo_Balance"] },
   "requiresAbility": null }]
```
All 66 pairs are imported for display; `grants` stays empty until a pair is implemented.
`requiresAbility` covers combos that modify a specific ability ("only works once you have unlocked that ability").

## Creature: `Data/creatures/<id>.json`
```json
{ "id": "cinderfang", "name": "Cinderfang", "biome": "volcanic",
  "behavior": "aggressive", "archetype": "pack_hunter", "roles": ["fighter"],
  "element": "fire", "resists": ["lava"],
  "taming": { "methods": ["knockout_feed"], "difficulty": "medium", "food": ["raw_meat"], "seconds": null },
  "stats": { "health": null, "damage": null, "speed": null, "torpor": null },
  "pack": { "min": 3, "max": 5 },
  "mesh": "greybox_sixleg", "notes": "Six-legged ember hounds that hunt in packs" }
```

## Boss: `Data/bosses/<id>.json`
```json
{ "id": "vulkarn", "name": "Vulkarn, the Forge Giant", "tier": "major", "biome": "volcanic",
  "weakness": "frost", "baseHealth": null, "scalingPerParticipant": null,
  "phases": [ { "below": 1.0, "attacks": ["hammer_combo", "overhead_slam", "tongs_grab", "anvil_slam"] },
              { "below": 0.5, "attacks": ["bellows_breath", "slag_throw", "anvil_slam"] },
              { "hidden": true, "requires": "artifact_modifiers", "attacks": ["dual_hammer_slam"] } ],
  "drops": { "core": "core_vulkarn", "pathMaterials": "fire", "part": "giants_ember" },
  "respawnMinutes": null }
```

## Item: `Data/items/<category>.json`
```json
{ "id": "copper_sword", "name": "Copper sword", "category": "weapon", "weight": null, "stack": 1,
  "durability": null, "tier": 1, "station": "forge", "sockets": "by_quality",
  "weapon": { "class": "sword_1h", "damage": null, "poiseDamage": null, "staminaCost": null } }
```

## Recipe: `Data/recipes/<station>.json`
```json
{ "id": "copper_sword", "station": "forge", "hearthLevel": 1,
  "inputs": [ { "item": "copper_ingot", "count": null }, { "item": "wood", "count": null } ],
  "optionalElement": "path_material", "output": { "item": "copper_sword", "count": 1 }, "seconds": null }
```

## Life skill: `Data/lifeskills.json`
```json
[{ "id": "mining", "group": "gathering", "levelsBy": "Mining ore and stone",
   "perLevel": { "text": "Faster mining", "stat": "MiningSpeed", "amount": null },
   "perks": { "25": [ { "id": "prospector", "text": "See nearby ore veins through rock" },
                      { "id": "strong_back", "text": "Ore weighs less" } ] } }]
```

## Hearth: `Data/hearth.json`
```json
[{ "level": 1, "distinctMajorBosses": 0, "baseMetal": "copper", "upgradeCap": 2 },
 { "level": 2, "distinctMajorBosses": 1, "baseMetal": "bronze", "upgradeCap": 4 },
 { "level": 3, "distinctMajorBosses": 2, "baseMetal": "iron",   "upgradeCap": 6 },
 { "level": 4, "distinctMajorBosses": 4, "baseMetal": "steel",  "upgradeCap": 8 },
 { "level": 5, "distinctMajorBosses": 6, "baseMetal": "starsteel", "upgradeCap": 10 }]
```

## Tuning: `Data/tuning.json`
Every global number in one place (XP curves, stamina costs, i-frame windows, regen rates, status
buildup thresholds). This is where Raven's starting numbers go.
