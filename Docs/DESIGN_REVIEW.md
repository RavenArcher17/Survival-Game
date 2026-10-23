# Design review list

Inconsistencies and open questions found while reading the design docs (2026-10-22).
To go through with Raven **after the design docs are finished**. Nothing here blocks the start of M1;
where the build needs an answer first, the plan uses a proposed default (marked) that's easy to change.

Columns: **Where** = which doc tabs disagree · **Needed by** = first milestone that depends on it.

## Affects the First Playable

| # | Issue | Where | Needed by | Proposed default in the plan |
|---|---|---|---|---|
| 1 | How many abilities can be equipped/used at once? "No swappable loadouts", but each tree has many spells; main doc says "a few path spells". | Main doc vs Skill trees | M1 | 8 quick slots (4 per path) + light/heavy attack, freely reassigned out of combat |
| 2 | Fire overheat: main doc says "overheating burns you"; the Overheat node makes full Heat an explosion; Burning blood makes main-path fire immune to own burns. What happens for a fire *subclass*? | Main doc vs Skill trees | M1 | Full Heat self-burns non-fire-main players; Overheat node replaces that with an explosion |
| 3 | Crafting tiers: build plan says "2 station tiers"; Crafting tab says two bosses reach Hearth level 3 (iron). | Build plan vs Crafting | M5 | Hearth level 3 |
| 4 | Tame limit: per player or per tribe? | Main doc | M4 | Per player, active cap from the Taming skill |
| 5 | Combos: main doc says combos "spend from both" resource bars; Skill trees says combos are automatic and cost no points. Do combos cost resources? | Main doc vs Skill trees, Combos | M1 | Passive combos (Thaw, Balance) cost nothing; active combo abilities would spend both bars |
| 6 | Status effects: main doc lists bleed, burn, frostbite, poison, rot. Trees and combos also use freeze/shatter, slow, soak, shock, blind, root, curse, stagger. Need one master list. | Main doc vs Skill trees, Combos | M1 | Fire/Frost set: Burn, Frostbite, Slowed, Frozen, Shatter, Staggered |
| 7 | Path resources: main doc calls them "suggestions, not decisions yet", but trees use them throughout. Are Heat and Chill final? | Main doc vs Skill trees | M1 | Heat and Chill treated as final |
| 8 | Skill-tree node prerequisites aren't specified (order inside a branch). | Skill trees | M2 | Nodes unlock by points spent in that branch; trunk (10) before branches |
| 9 | Respec vs swapping: main doc says respec is costly (rare item or long quest), but swapping paths at altars only means re-beating a boss. Is re-spending points *inside* a tree a respec? | Main doc vs Skill trees | M2 | Altar swap keeps ranks; no in-tree respec in the First Playable |
| 10 | First playable boss count: main doc's next steps say "one boss"; build plan and Bosses tab say 1 major + 2 minis per biome (2 major total). A subclass needs the other biome's boss altar, so two majors are required. | Main doc vs Build plan, Bosses | M6 | 2 major + 4 mini (build plan) |
| 11 | Hearth / boss-gated tech levels aren't in the main doc, which says stations go "tier 1 to 5" with no boss gating. | Main doc vs Crafting | M5 | Crafting tab (Hearth) wins |
| 12 | Signature resource per biome isn't named for volcanic/tundra. Creatures tab implies sulfur crystals and lichen. | Main doc, Creatures | M3 | Sulfur crystals (volcanic), lichen (tundra) |
| 13 | "Rest" is used by Necro (Deathless) and Void (Corruption) but never defined; survival only has sleep/Rested. | Skill trees vs Main doc | Later (not FP paths) | — |
| 14 | Floeback is the First Playable's only swimming mount, but swimming mounts are Full release in the build plan. | Creatures vs Build plan | M4 | Defer Floeback |
| 15 | Map tab: the layout needs more planning into a realistic landmass (coastlines, rivers, mountain range) before building. | Map | M3 | Plan includes a terrain-layout step first |
| 16 | Neutral creature tames: are they all tamable by every method listed, or only the one in the table? | Creatures | M4 | Only the listed method(s) |

## Later biomes / Full release

| # | Issue | Where |
|---|---|---|
| 17 | Healing: "crafted potions only", but Tide, Nature, Blood, Necro and Spore trees have heal spells and lifesteal. | Main doc vs Skill trees |
| 18 | Secret hybrid paths unlock "when two paths reach mastery", but the 150 cap means two paths can't both be 100 at once. Does mastery count saved progress across swaps? The god path ("mastering several paths") has the same question. | Main doc vs Skill trees |
| 19 | Combo themes: "neighbours are related themes, so combos between them feel natural", yet the flagship combo (fire + frost) is between opposites. The biome wheel order isn't written down anywhere. | Main doc vs Combos |
| 20 | Portals: "portal timers" and "procedural dungeon layouts" are on the ruled-out list, but portals have real-day break timers (1/3/6/10/18) and shuffled rooms. Also unclear what happens to a cleared portal. | Main doc (internal) |
| 21 | Traps are ruled out (for bases?) but used in dungeons. | Main doc (internal) |
| 22 | Raids are for 6–10 players, while "party limits" are ruled out. Tuned for, or capped at? | Main doc (internal) |
| 23 | "Open questions: none right now" while several TBDs remain (tame limit, mutations "a maybe", resources as suggestions, game title). | Main doc |
| 24 | Skill trees tab ends "All 66 path pairs … are in the  tab": the tab name is blank (broken mention to Combos). | Skill trees |
| 25 | Naming overlaps: Necro "Curse of weakness" = Blood "Hex of weakness"; Frost's Blizzard capstone shares its branch name; Blood "Lifesteal" node vs "Bloodthirst" passive; Nature "Regrowth" vs Spore "Regrowth spores". | Skill trees |
| 26 | Game title is still "Untitled" (repo uses the working title Wildreach). | Main doc |

## Still being designed (not inconsistencies)
- Building pieces
- Server settings
- Starting numbers
