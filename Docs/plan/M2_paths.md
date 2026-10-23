# M2: Paths & progression

**Goal:** the Fire and Frost trees work end to end: earn XP by fighting, spend points, unlock
the subclass at a boss altar, swap paths and keep progress.

**Gate:** a fresh character picks Fire, reaches 30 Fire points, unlocks Frost at the tundra altar,
reaches 25/25 and gets Thaw automatically, swaps Frost out and back in with points intact.
All of it persists across a server restart.

Depends on: M1 (GAS), M3 boss altar placeholders (can be a debug actor until then).

## Rules (from the Skill trees tab)
- Tree = **10-point trunk + three 30-point branches** = 100 points max per tree.
- Stat nodes cost 1 point per rank; abilities cost several; capstones cost 10 and end a branch.
- **150 points total across two paths, no path above 100.**
- Root passive is free and active **only on the main path**.
- Points come from path XP, earned by using that path's abilities.
- Two owned paths unlock automatic combos at **25/25** and **50/50**; combos cost no points.
- Main path picked at creation and sets spawn biome. Subclass unlocked at the other biome's boss altar.
- Either slot can be swapped at altars; progress saved per path; a new path starts at 0.

## Data
`Data/paths/fire.json`, `Data/paths/frost.json` (schema in `Docs/plan/DATA_SCHEMAS.md`), imported to
`UWRPathDefinition` Primary Data Assets by `Tools/import_data.py`. All 12 paths get stub files later; only
Fire and Frost are filled now. Combos: `Data/combos.json` (all 66 pairs, from the Combos tab), only
Fire+Frost wired to abilities.

## Systems
| Class | Responsibility |
|---|---|
| `UWRPathDefinition` (Primary Data Asset) | Tree layout: nodes, ranks, costs, prerequisite, granted GE/GA |
| `UWRPathProgressComponent` (on PlayerState) | Replicated (owner-only): `MainPath`, `SubPath`, map PathId → {XP, PointsEarned, NodeRanks}. All edits are server RPCs validated against the caps. |
| XP award | Hook in `UWRGameplayAbility::CommitAbility`: abilities tagged `Path.X` grant XP to path X, scaled by target difficulty (no XP vs training dummies on live servers). |
| XP curve | `Data/tuning.json` `pathXpPerPoint` curve. Placeholder: points 1–100 take 100 × 1.04^n XP. **Needs Raven's starting numbers.** |
| Node granting | Owning ranks grants the node's GE (stat) or GA (ability). Swapping a path out removes its grants and keeps the saved ranks. |
| Prerequisites (proposal) | Branch nodes unlock in order by points spent in that branch (e.g. 0/5/10/15/20, capstone at 20). Trunk must be complete (10) before any branch. |
| Combo service | Watches both paths' spent points; grants/removes the combo GA at 25/25 and 50/50. |
| Character level | Sum-of-activity level (path XP + life-skill XP). Proposal: gates nothing in First Playable except a display and matchmaking hint. |
| Altar actor | `AWRBossAltar` in each boss arena: interact → UI to unlock/swap subclass. Unlock condition: that biome's major boss killed by you (or with your damage ≥ 10%) at least once. |
| Character creation | Name, body, main path (Fire/Frost) → spawn at one of that biome's three spawn points. |

## UI (UMG)
- Skill tree screen: trunk + 3 branches, rank pips, cost, live points `used / 150`, per-path `x / 100`.
- Resource bars: one per owned path (Heat red-orange, Chill pale blue).
- Ability bar: 8 quick slots + light/heavy.

## Persistence
Path progress row per character in SQLite (see M7). Written on change (debounced 5 s) and on logout.

## Tests
- Unit: cap enforcement (101st point, 151st point rejected), swap keeps ranks, combo grant/revoke thresholds.
- Network: a client spamming forged `ServerSpendPoint` RPCs can't exceed caps.
