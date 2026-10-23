# Design review list

Inconsistencies and open questions found while reading the design docs, and Raven's decisions.
Reviewed with Raven on **2026-10-22**. Decisions here override the design docs where they disagree
(the docs haven't been edited to match yet).

## Decided (First Playable)

| # | Question | Decision |
|---|---|---|
| 1 | How many abilities equipped at once? | **8 quick slots, 4 per path**, plus light/heavy attack. Reassign anytime outside combat; no saved presets. |
| 2 | Fire overheat | **No self-damage.** Full Heat just stops building. The Overheat node's explosion is the only payoff. (Overrides the main doc's "overheating burns you".) |
| 5 | Do combos cost resources? | **Some combos are active abilities that spend both bars**; others are free passives. Fire+Frost's **Thaw and Balance stay passive**. Which other pairs are active is decided when those paths are designed. |
| 6 | Status effects | **The full list is official**; the First Playable builds Burn, Frostbite, Slowed, Frozen, Shatter, Stagger. |
| 7 | Heat and Chill | **Keep for now**; may be renamed later (they're data, so renaming is cheap). |
| 8 | Node order inside a branch | **By points spent in the branch** (gates 0/5/10/15/20, capstone last); trunk's 10 first. |
| 9 | Respec | **Rare item / long quest only** (not in First Playable). Altar swaps keep ranks. |
| 10 | Boss count | **2 major + 4 mini** (build plan + Bosses tab). The main doc's "one boss" is outdated. |
| 3, 11 | Crafting tiers | **Crafting tab wins**: Hearth reaches level 3 (iron) with the two major bosses. |
| 12 | Signature resources | **Sulfur crystals** (volcanic), **lichen** (tundra). |
| 4, 34 | Tame limit | **Per player**, active cap grows with the Taming skill (e.g. 2 at start, +1 at Bonded, +1 at Beast lord); stored tames have a higher cap. Numbers to tune. |
| 14 | Floeback | **Deferred to Full release** with the other swimming mounts; tundra launches with 8 creatures. |
| 15 | Map layout | Refine toward a realistic landmass first; **Raven signs off before terrain is built** (CLAUDE.md). |
| 16 | Taming methods | **Only the listed method(s)** per creature. |
| 27 | Farming | **Basic farming is in**: crop plots (plant → grow → harvest) and animal pens (feed → produce). Irrigation, greenhouses, bees, recipe discovery wait. |
| 28 | Freeform placement | **Utility and decor only**; structure pieces snap. |
| 29 | Structural support | **Span rule is in** (wood 4, stone 6). |
| 30 | Comfort buff | **Decor now, buff in Full release.** |
| 31 | Creature raids | **Setting only**; the feature comes in Full release. |
| 32 | Bounties and infamy | **Setting only**; the feature comes in Full release. |
| 33 | Stone building gate | **Stone is available at Hearth level 1** alongside wood. (Overrides the Building tab and the Crafting tab's workbench row.) |
| 35 | Health/stamina growth | **Cap so level 50 = exactly 250 health / 150 stamina.** |
| 36 | Character level | **From path points only: level = total path points spent ÷ 3** (150 points → 50). Raven doesn't recall it doing anything else. Health/stamina growth follows it. |
| 37 | Highland creature HP | **Keep 200, tune in playtests.** |
| 38 | Scaffold placeholder numbers | No decision needed; M1 switches the code to the Starting numbers tab. |
| 21 | Base traps | Ruled out (Building tab). Dungeon traps: see #21 below. |

### Doc edits these decisions imply (Raven's docs, not edited yet)
- Main doc: overheating doesn't burn you (#2); "one boss" → 2 major + 4 mini (#10); stations tier 1–5 → Hearth levels (#11).
- Building + Crafting tabs: stone at Hearth level 1 (#33).
- Starting numbers: character level = path points ÷ 3 (#36).
- Creatures tab: Floeback moved to Full release (#14).
- Build plan: basic farming, span rule, utility/decor freeform moved into First Playable (#27–#29).

## Open: later biomes / Full release (review when those are designed)

| # | Issue | Where |
|---|---|---|
| 13 | "Rest" is used by Necro (Deathless) and Void (Corruption) but never defined; survival only has sleep/Rested. | Skill trees vs Main doc |
| 17 | Healing: "crafted potions only", but Tide, Nature, Blood, Necro and Spore trees have heal spells and lifesteal. | Main doc vs Skill trees |
| 18 | Secret hybrid paths unlock "when two paths reach mastery", but the 150 cap means two paths can't both be 100 at once. Does mastery count saved progress across swaps? Same question for the god path. | Main doc vs Skill trees |
| 19 | "Neighbours are related themes, so combos between them feel natural", yet the flagship combo (fire + frost) is between opposites. The wheel order isn't written down. | Main doc vs Combos |
| 20 | Portals: "portal timers" and "procedural dungeon layouts" are ruled out, but portals have real-day break timers and shuffled rooms. Unclear what happens to a cleared portal. | Main doc |
| 21 | Traps in dungeons, given traps are ruled out for bases. | Main doc |
| 22 | Raids are for 6–10 players while "party limits" are ruled out: tuned for, or capped at? | Main doc |
| 23 | "Open questions: none right now" in the main doc is stale. | Main doc |
| 24 | Skill trees tab ends with a broken mention ("… are in the  tab"), which should point to Combos. | Skill trees |
| 25 | Naming overlaps: Necro "Curse of weakness" = Blood "Hex of weakness"; Frost's Blizzard capstone shares its branch name; Blood "Lifesteal" vs "Bloodthirst"; Nature "Regrowth" vs Spore "Regrowth spores". | Skill trees |
| 26 | Game title is still "Untitled" (repo uses the working title Wildreach). | Main doc |
