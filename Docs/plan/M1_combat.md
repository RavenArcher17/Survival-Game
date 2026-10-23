# M1: Combat core

**Goal:** one fight feels good online. Design rule from the build plan: "If one boss fight feels good,
12 biomes are worth building; if it doesn't, more biomes won't fix it."

**Gate:** on a dedicated server with 2 clients at ~100 ms simulated latency
(`Net PktLag=100`), a player can (a) duel the other player and (b) fight a 3-Cinderfang pack,
and both feel responsive: no rubber-banding on dodge, hits land where they look like they land.

Depends on: M0. Blocked by: nothing (starting numbers use placeholders in `Data/tuning.json`).

## 1. GAS foundation
| Task | Detail |
|---|---|
| Enable `GameplayAbilities` plugin, add `GameplayAbilities`, `GameplayTags`, `GameplayTasks` modules | Build.cs + .uproject |
| `AWRPlayerState` owns `UWRAbilitySystemComponent` | ASC on PlayerState so path progress survives pawn death/respawn. Replication mode **Mixed**. |
| Creatures/bosses own their ASC on the pawn | Replication mode **Minimal**. |
| `UWRAttributeSet_Vitals` | Health, MaxHealth, Stamina, MaxStamina, Poise, MaxPoise, Hunger, Thirst |
| `UWRAttributeSet_Paths` | Heat, MaxHeat, Chill, MaxChill (one pair per path; others added with their biomes) |
| `UWRAttributeSet_Combat` | meta attributes: IncomingDamage, IncomingPoiseDamage; elemental resist per element |
| Damage pipeline | `UWRDamageExecution` (GameplayEffectExecutionCalculation): base × weapon × element vs resist × status modifiers (e.g. Permafrost vs Slowed) → Health and Poise. Every source (weapons, spells, hazards, starvation) goes through it. |
| Retire `UWRSurvivalComponent` | Hunger/thirst decay becomes an infinite periodic GameplayEffect; starvation damage is a GE through the damage pipeline. |
| Native tags | `WRGameplayTags.h`: `Path.Fire`, `Path.Frost`, `Element.*`, `Status.Burn/Frostbite/Frozen/Slowed/Staggered`, `State.Dodging/Parrying/Invulnerable`, `Ability.*` |

## 2. Character & animation
| Task | Detail |
|---|---|
| Skeletal mesh | Import Manny/Quinn from Epic's free **Game Animation Sample** (GASP) into `Content/Characters/Survivor` |
| Locomotion | Use GASP's anim blueprint as a base (C++ `UWRAnimInstance` exposes speed, direction, lock-on, combat stance) |
| Attack anims | Mixamo (free) one-handed sword set as a start: 3-hit light chain, 2-hit heavy, riposte, hit reacts, stagger, death |
| FP view | Separate first-person arms mesh (owner-only) + full body for others. Same montages drive both. |
| Equip load | `EquipLoad / MaxEquipLoad` → Light (<30%) / Medium (<70%) / Heavy roll; affects roll distance, i-frame count, stamina cost |

## 3. Melee & defense (server-authoritative, client-predicted)
| Mechanic | Implementation |
|---|---|
| Light / heavy attack | GameplayAbility playing a montage with `AbilityTask_PlayMontageAndWait`; combo chaining via anim notify windows |
| Hit detection | `UWRWeaponTraceNotifyState`: sweeps weapon sockets each frame of the active window. **Server** confirms hits and applies damage. Clients run the same trace locally for hit sparks/sound only. |
| Lag compensation | Server rewinds target capsules to the attacker's timestamp (small ring buffer of transforms, ~250 ms) before confirming hits. Needed for PvP. |
| Dodge roll | Predicted GA + root motion source; `State.Invulnerable` tag during i-frame window (data per equip load). |
| Parry / riposte | Short parry window tag; an incoming melee hit during it → attacker gets `Status.Staggered` (riposte-able), defender takes 0. Ripostes are a context ability on staggered targets. |
| Stamina | Every action has a stamina cost GE; regen delay after spending; can't start an action at 0. Sprint moves to GAS too (the CMC flag stays for prediction). |
| Poise & stagger | Poise damage per hit; at 0 → Staggered montage + poise reset. Bosses have big poise pools. |
| Lock-on | Local-only targeting component: nearest valid target in a cone; camera and strafe orientation follow it. Optional; free-aim always works. |

## 4. Path abilities (Fire + Frost trunks only)
Each listed ability is one GameplayAbility class + data row. Costs/cooldowns from `Data/tuning.json`.

| Ability | Type | Behaviour |
|---|---|---|
| Fire bolt | Spell | Predicted projectile (server-spawned, client-predicted visual); applies Burn buildup; generates Heat |
| Flame slash | Weapon art | Melee montage with fire infusion; Burn buildup |
| Frost shard | Spell | Projectile; Frostbite buildup; generates Chill |
| Ice slash | Weapon art | Melee; Frostbite buildup |

Resources: Heat and Chill fill from using that path's abilities and drain slowly out of combat.

## 5. Status effects
| Status | Rule (first draft) |
|---|---|
| Burn | Buildup meter → on full: damage over time; spreads nothing by default (Spreading flames node later) |
| Frostbite | Buildup → on full: Slowed + bonus damage taken |
| Frozen | Frostbite while already Slowed → Frozen (can't act, short) |
| Shatter | A heavy hit on a Frozen target → burst damage, ends Frozen |

Buildup = attribute per status with decay; implemented as GE stacks so it replicates for free.

## 6. Fire + Frost combos
| Combo | Unlock | Implementation |
|---|---|---|
| **Thaw** | 25/25 | Hitting a `Status.Frozen` target with Fire damage → bonus burst, consumes Frozen. Passive GA granted when both paths ≥ 25 points (M1 grants it with a debug command). |
| **Balance** | 50/50 | Spending Heat grants Chill and vice versa (GE triggered on resource spend). |

## 7. Test content
- `L_CombatArena`: small WP-free arena map, training dummies (infinite HP, DPS/poise readout), a respawning Cinderfang pack.
- Cinderfang (greybox six-legged hound): StateTree with Idle → Chase → Attack (bite, lunge) → Flank (pack members pick different angles). Reused as the template for every aggressive creature in M4.
- Debug commands (`UWRCheatManager`): `WR.GrantPath Fire 50`, `WR.Status Frozen`, `WR.God`, `WR.NetStats`.

## 7b. Starting numbers (from the Starting numbers tab)
- Health 100 (+3/character level, max 250), Stamina 100 (+1/level, max 150).
- Stamina regen 30/s, starting 0.8 s after the last spend. Costs: light 10, heavy 20, dodge 15, block 10–25 by hit, sprint 5/s.
- Respawn 10 s.
- Copper sword: light 12, heavy 22. Metal ×1.0/1.3/1.7/2.2/2.8 (copper→starsteel), quality ×0.9/1.0/1.1/1.2, +5% per upgrade level.
- Enemies: lowland 70 HP / 12 dmg, midland 120 / 20, highland 200 / 32. Mini-boss 3,000 HP solo. Major boss 12,000 HP solo, +80% per extra player or tame.
- Scaffold code still has older placeholders; M1 swaps them for these (review #38).

## 8. Verification
- Automated: Functional tests in `Source/WildreachTests` (damage pipeline math, parry window, Thaw only on Frozen) run headless with `-ExecCmds="Automation RunTests Wildreach"`.
- Network: headless server + 2 nullrhi bots running scripted input (bot controller that attacks/dodges on a timer) for 10 min with `PktLag=100 PktLoss=2`; log corrections per minute from `LogNetPlayerMovement`.
- Manual: Raven plays `play.ps1 local -Clients 2`.

## Decided with Raven (2026-10-22, see DESIGN_REVIEW.md)
- **Equipped abilities (#1):** 8 quick slots (4 per path) plus light/heavy attack, reassignable anytime out of combat; no saved presets.
- **Overheat (#2):** no self-damage. Full Heat just stops building; the Overheat node's explosion is the only payoff.
- **Combos (#5):** Thaw and Balance are free passives. The combo system still supports *active* combos that spend both bars (`kind: "active"` in `combos.json`) for other pairs later.
- **Statuses (#6):** full list is official; build Burn, Frostbite, Slowed, Frozen, Shatter, Stagger now.
