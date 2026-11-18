# Wildreach: combat bible and lessons from our games

Written 2026-11-10 (laptop, paper only) against Raven's v0.7.1 (commit 372557e). For the desktop's Wildreach cook.

**Aaron 11-10:** "Regions can vary in size. The combat should copy Elden Ring. Use all of the research and lessons from all of our games."
**Aaron 11-10, via the lead:** (1) Elden Ring's swappable weapon skills get a Wildreach name: **Wildbrands** (below). Never call our system "Ashes of War"; Elden Ring is only the reference. (2) Elden Ring's spirit summons are **not copied**, not even as a stand-in. Taming stays exactly as Raven designed it.

**How to read this:**
- **ER** means Elden Ring. Numbers are 30 fps logic frames unless they're given in seconds. **[EST]** means a community estimate, not datamined.
- **Today** is what Wildreach has now, from `Data/tuning.json`, `CLAUDE.md` and the M1/M4/M6 plans.
- **Set** is the value to cook. Every number goes in `tuning.json` (`WRTune`), as Raven's rules ask.
- **Raven's rules stand** (Aaron 11-10 form). All damage goes through `UWRCombatLibrary::ApplyHit` on the server. New abilities are a `UWRGameplayAbility` plus a `UWRAbilityRegistry::ClassForNode` entry. Clients send intent, never results. Tests go green before a push, as his CLAUDE.md says.
- **[RAVEN]** marks something his docs don't contain. It goes in the next planning doc for his approval, not straight into the cook (Aaron 11-10: "if you made them up, add them to the next planning doc").
- Sources are listed at the end (S1 to S16).

---

## Part 1. Elden Ring combat, mapped onto Wildreach

### 1.0 What's settled before anything is copied
Raven's design already says "Elden Ring style combat", so most of this copies straight in. Five of his decisions change how it maps:

| Wildreach rule (Raven's docs) | What it means for the ER copy |
|---|---|
| No Souls-style stat points, no swappable loadouts. Leveling is by use | No Str/Dex scaling letters and no respec-by-stats. Damage comes from metal × quality × upgrade, times path nodes |
| 150 path points, no path over 100; main path plus a subclass | Path trees play the role of ER's stats, spells and affinities |
| Each path has its own resource (Heat, Chill, Charge…), not one FP bar | ER's FP is replaced by the path resource. Combo abilities may spend both bars |
| Healing is crafted potions only, limited by belt slots, with a short cooldown | This is the ER flask: few uses, a commit animation, and the AI punishes it |
| Weapons are separate from paths; the path infuses the weapon | ER affinities become automatic: your path infuses whatever you hold |

### 1.1 Attack set
**ER (S1, S2):**
- **R1 light:** a 3 to 6 hit chain, depending on the weapon class.
- **R2 heavy:** can be held to charge. A full charge does about 3× R2 stance damage.
- **Contextual attacks:** running R1/R2, rolling or backstep R1, crouch R1, jump R1/R2. A jump R2 does 2× R2 stance damage.
- **Two-handing:** Strength ×1.5, stance +30% on lights and +10% on heavies.
- **Counter-hit:** pierce does +30% to a target that is mid-attack.
- **No cancels:** "you won't be able to dodge until you've finished that attack animation." Control returns only at the recovery cancel point.

**Today:**
- Light 12 / heavy 22 (copper). Light timing 0.25 / 0.18 / 0.30 s (wind-up / active / recovery); heavy 0.55 / 0.22 / 0.45.
- Mixamo 3-hit light chain and 2-hit heavy.
- Reach 220 cm, sweep 45.

**Set:**

| Attack | Input (pad / KB+M) | Stamina | Damage × | Stance dmg (sword) | Notes |
|---|---|---|---|---|---|
| Light chain 1-2-3 | RB / LMB | 10 each | 1.0, 1.0, 1.15 | 5, 5, 6 | Chain window from an anim notify, in the last 40% of recovery |
| Heavy | RT tap / RMB | 20 | 1.0 (the 22 base) | 10 | Hyperarmor from wind-up to active (1.4) |
| Charged heavy | RT hold 0.35 s or more / RMB hold | 30 | 1.6 | 30 | Charge can be held up to 1.0 s. Full charge at 0.8 s |
| Running light / heavy | RB or RT while sprinting | 12 / 24 | 1.1 / 1.25 | 6 / 14 | Step-in by motion warping |
| Rolling light | RB inside 0.3 s after a roll ends | 10 | 1.0 | 5 | Fast poke; the roll-punish tool |
| Backstep light | RB after a backstep | 10 | 0.9 | 4 | |
| Jump light / heavy | RB or RT in the air | 15 / 25 | 1.1 / 1.5 | 8 / 20 | Heavy is the stance tool (1.10) |
| Crouch light | RB while sneaking | 8 | 0.9 | 4 | Small hurtbox. **Stealth bonus:** a target that hasn't noticed you takes +20% stance and status buildup (ER), which ties to the Sneaking life skill |
| Guard counter | RT inside the window after a block (1.2) | 20 | 1.3 | 30 | |

- **Animation commitment is the rule.** After the active window, nothing cancels until the `CancelWindow` notify opens, and that includes dodge. Dodge is buffered (1.15), never instant.
- **Counter-hit:** +20% damage and +30% stance against a target that is mid-wind-up. Read it from the target's `State.Attacking.Windup` tag. Make it pierce-only once there are weapon classes with pierce.
- **Two-handing:** ER's ×1.5 Strength has no meaning without stats. Two-handing a one-handed weapon gives +15% damage, the ER stance bonuses, and guard from the weapon (no shield). The sword-and-shield default stays. **[RAVEN]**, since his docs don't mention two-handing.

### 1.2 Guard and guard counter
**ER (S3, S4):**
- Stamina lost on a block = the attacker's stamina damage × (1 − GuardBoost/100). Examples: dagger 15, medium shield about 60, greatshield 70-80.
- Stamina at 0 while blocking means a guard break: the defender is staggered and can be riposted.
- Holding guard pauses stamina regen. Stamina is spent only when a blow is actually blocked. Holding costs nothing.
- **Guard counter** is R2 straight after a blocked hit. It does about full-charge stance damage and has no hyperarmor.

**Today:**
- Block angle 70°.
- `blockDamageTaken` 0: a block takes no HP at all.
- Block costs 10-25 stamina.
- Guard break staggers for 1.2 s.

**Set:**
- **Guard Boost per item** in `items.json` (`guardBoost`).
  - Sword with no shield: 25.
  - Crude shield: 45. Wooden: 55. Metal: 65 + 2 per upgrade. Greatshield: 80.
  - Stamina lost on a block = hit stamina damage × (1 − GB/100), spent only when a blow is blocked. The flat 10-25 goes. Holding guard with nothing hitting you drains nothing.
- **Guarded damage negation per item and per element.**
  - Physical: 100% on shields, 60% on a guarding sword.
  - Element: 60-80%. Frost and Fire shields come from the biome tier.
  - The leftover HP damage gets through. That's ER's chip damage, which the 0% today lacks.
- **Guard break:** stamina reaches 0 on a block, causing a 1.2 s stagger. On creatures and PvP it's riposte-able (1.3).
- **Regen while guarding:** paused for as long as guard is held (Aaron 11-10). Stamina is not lost for holding, only for blocked hits (scaled by the blow and the weapon's guard boost). Today is full regen, which makes block-forever free; pausing regen plus per-block cost closes that (2.1 L9).
- **Guard counter window:** 0.6 s from the end of block-stun. Input is RT, the heavy button. Releasing guard is never the counter. A guard pressed early still blocks, so a failed defence is never worse than doing nothing (Waveraider law).
- **Visible guard:** while blocking, the shield shows a faint gold edge (Waveraider REQ-025). Gold means "block/parry" (2.1 L3).

### 1.3 Parry, riposte, backstab, critical hits
**ER (S3, S5, S6):**
- Parry frames (startup / active):

| Tool | Startup | Active |
|---|---|---|
| Small shield | 4 | 5 |
| Dagger | 4 | 4 |
| Medium shield | 4 | 2 |

- Parry recovery is about 13-17 frames.
- **Crit damage = AR × class multiplier × crit stat / 100.**
  - Straight sword: backstab 230%, riposte 345%.
  - Dagger: 294% / 420%. Greatsword: 220% / 275%.
- Bosses need 2 or 3 parries to break (Margit 2, Malenia 3).
- **Backstab:** a light attack from behind a human-sized target, inside about a 90° rear arc [EST].
- **Riposte triggers:** a parry, a guard break or a stance break. The crit is a long paired animation.

**Today:**
- `parryWindow` 0.2 s.
- `riposteMultiplier` 3.
- Parry staggers the attacker, and riposte is a context ability.
- No backstab found in the docs.

**Set:**

| Item | Value |
|---|---|
| Parry input | LB tap (guard button tap; block is LB hold). One verb per button: 2.1 L4 |
| Parry startup / active / recovery | 0.13 / 0.17 / 0.45 s for buckler and small shield. 0.13 / 0.10 / 0.50 s for medium shield and sword. Greatshields can't parry (they get Discharge-style block passives instead) |
| Parry on creatures | Only attacks marked `parryable` in data. Boss grabs (the Vulkarn tongs) are parryable, as Raven wrote |
| Parries to break | Normal creature 1. Mini-boss 2. Major boss 3. Each counts a parry hit until stance regen resets it |
| Riposte | 3.45× (straight sword). Paired animation; both actors locked, with i-frames for the attacker against third parties for 0.6 s **[EST, ER doesn't publish]** |
| Backstab | 2.3× from a 90° rear arc, on targets of human size or smaller and not attacking. Bosses only after a stance break |
| Crit by class | Use the ER table (S6) as class multipliers in `items.json` `crit.backstab` / `crit.riposte` |
| Missed parry | Plays the full recovery, so spam is punished. **Sekiro spam rule:** a second parry press within 0.5 s of the last shrinks the active window by 30% (Waveraider research 66) |
| Symmetric judgement | The parry is judged on the server at the moment the hit lands, rewound to the attacker's timestamp (1.17). There are no "perfect" tiers in Wildreach, so Waveraider's symmetric ±window law reduces to "the window is centred on what the player sees" |

- **Paired animations:** motion warping to a sync point (1.16), so attacker and victim never pop.
- **PvP crits** are validated on the server with the 250 ms rewind. ER's peer-to-peer laggy backstabs are what not to copy (S16).

### 1.4 Stance (enemy posture) and the critical
**ER (S7):**
- Every enemy has a hidden stance meter.
  - Normal enemies 15-65. Bosses 80-120. Radahn 200. Dragons 600.
- **Regen:** starts after stance/13 seconds without a hit, then refills at 13/s.
- **Break:** the enemy is stunned for several seconds and shows a weak point. A light attack there triggers the crit.
- Stance damage by class follows the S7 table, for example sword R1 5, jump R2 20, charged R2 30, guard counter 30.
- Bosses take 40% less stance damage when a summon is present. Co-op cuts it further: ×0.6, then ×0.35.

**Today:**
- The M1 plan gives one "Poise" attribute to players and creatures alike, with staggers at 0.
- Player poise 40, regen 10/s after 2 s.
- Weapon poise: light 10, heavy 25. `staggerSeconds` 1.5.

**Set: split it the way ER does.** Players get **Poise** (1.5). Creatures and bosses get **Stance**.

| Target | Stance | Break stun | Notes |
|---|---|---|---|
| Lowland creature | 20 | 2.5 s | |
| Midland | 35 | 2.5 s | |
| Highland | 55 | 3.0 s | |
| Mini-boss | 90 | 4.0 s | |
| Major boss | 140 (Skaldrith 160, Vulkarn 180) | 5.0 s | Skaldrith's wings keep their own stance (6 hits grounds him, as in Raven's tuning today) |

- **Regen:** after stance/13 s, at 13/s (ER exact).
- **Break:** the target plays its stance-break clip, and a gold weak-point mark shows on the crit bone. A light attack inside 1.0 m and 60° of the front triggers the crit: 3.0× riposte-class damage, a paired animation, and hitstop 0.20 s.
- **Group scaling (copied from ER):** boss stance damage taken is ×1.0 solo, ×0.6 with 2 participants, ×0.45 with 3, ×0.35 with 4 or more. Each tame counts as a participant, the same rule Raven uses for HP. Without this, a group stun-locks every boss.
- **The 1-HP rule (Waveraider's unkillable bug, 2.1 L5):** a blow that would take a creature to 0 kills it. Nothing in Wildreach holds HP at 1. If any system ever floors HP (a phase change, a capture), the same blow must also break stance so the finisher is offered. A creature must never be left alive with nothing that can finish it.
- **Phase transitions** never happen on a crit. ER lets Malenia's phase 2 start only from a non-crit hit. The crit always finishes; the transition plays after it.

### 1.5 Poise and hyperarmor (players and big creatures)
**ER (S8):**
- Shown poise / 10 = internal poise. Excess damage doesn't carry over.
- Resets 30 s after the last hit, and right after a stagger.
- Enemy hits typically do 50 or 100 poise damage.
- **Hyperarmor** is extra poise during part of a swing on great and colossal weapons.
- **Damage reduction during hyperarmor (PvP 1.10):** colossal 17.5%, great weapons 7.5%.
- Grabs and status procs ignore hyperarmor.

**Today:** player poise 40, regen 10/s after 2 s. No hyperarmor.

**Set:**

| Item | Value |
|---|---|
| Base poise | 0 (cloth) up to 50 (heaviest set), from armour pieces in `items.json` (`poise`). Path nodes add more, as Fire's Tempered (+5 per rank) already does |
| Poise damage taken | Light creature hit 20, heavy 50, boss 100, boss slam 150 or more. A player sword R1 does 30 against players |
| Reset | Full reset 30 s after the last poise hit, and at once after a stagger (ER exact). Today's 10/s regen goes |
| Hyperarmor | Notify state `ANS_Hyperarmor` from wind-up to the end of active. Adds poise: heavy ×1.5, charged heavy ×2, great weapons ×2.5. During it, damage taken drops 7.5% (great weapons 17.5%) |
| What breaks through | Grabs, Frozen, Shatter, a stance break and the 'heavy' boss slams marked `ignoreHyperarmor` |
| Creatures | Creatures with poise (Basaltback, Woolly drudge, bosses) get hyperarmor on their own attacks from the first tell frame to the last strike. The punish window is the recovery after it (Waveraider COMBAT.md) |

### 1.6 Stamina
**ER (S9):**
- Pool 80 to 170 by Endurance.
- Regen about 45/s [EST], after a delay of about 0.5 s [EST].
- Roll and backstep cost 12 at every load.
- Regen pauses while guarding and while sprinting. Stamina is spent while guarding only on a blocked hit.
- Attacks have per-weapon costs.

**Today:**
- 100, rising to 150 at level 50.
- Regen 30/s after 0.8 s.
- Costs: light 10, heavy 20, dodge 15, sprint 5/s.

**Set:**
- **Regen 45/s, delay 0.5 s.** Today's 30/s refills 30% of the bar a second; ER refills about 55% at level 1. This is the single biggest "feels like ER" number.
- **Roll 12, backstep 8, sprint 5/s.** No regen while sprinting.
- **Guarding pauses regen** (no drain for holding; stamina is spent only on a blocked hit). Hunger or thirst empty halves regen, as Raven's survival rule already says.
- **Attack costs by weapon class:**

| Class | Light | Heavy | Charged |
|---|---|---|---|
| Dagger | 7 | 14 | 21 |
| Sword (today's) | 10 | 20 | 30 |
| Axe and hatchet | 12 | 22 | 33 |
| Great weapons | 18 | 28 | 40 |

- Nothing starts at 0 stamina (Raven's rule, kept). Below the action's cost, the action still starts if stamina is above 0 and goes negative to −20. That's ER's "last swing" grace and kills the dead-button feel. **[EST, ER behaviour]**

### 1.7 Roll, backstep, jump and i-frames by equip load
**ER (S10):**
- **Load tiers:** Light under 30%, Medium 30-69.9%, Heavy 70-99.9%, Overloaded can't roll.
- **Roll i-frames:** 13 frames (0.43 s) light and medium, 12 (0.40 s) heavy. Fextralife says 13/12; wiki.gg says 14/13.
- **Roll recovery:** light and medium 8 frames (0.27 s), heavy 16 (0.53 s). The light roll travels further.
- **Backstep:** almost no i-frames.
- **Jump:** 25 i-frames (0.83 s) on legs and torso only, which is why jumping over sweeps works.

**Today:** distance 450 cm, duration 0.45 s, i-frames 0.05 to 0.35 s, no load tiers in data. The M1 plan names the tiers.

**Set:**

| Load (worn + held kg ÷ max) | Roll distance | I-frames | Recovery | Cost |
|---|---|---|---|---|
| Light < 30% | 500 cm | 0.03 to 0.46 s | 0.27 s | 12 |
| Medium < 70% | 420 cm | 0.03 to 0.46 s | 0.27 s | 12 |
| Heavy < 100% | 330 cm | 0.03 to 0.43 s | 0.53 s | 12 |
| Overloaded | No roll; a slow step | 0 | | |

- **Max equip load** is separate from the 80 kg bag. Worn and held items only: 60 kg base, plus Hauler and armour nodes. The bag's carry weight stays as Raven built it.
- **Backstep** (B with the stick at rest): 0.10 s of i-frames, 250 cm.
- **Jump:** a `State.JumpLegsInvuln` tag for 0.8 s on the leg and torso hit zones. Sweeps marked `low` miss a jumping player. Creature ground waves and tail sweeps get the `low` flag.
- **Dodge input:** dodge fires on **press**. ER fires on release, which adds 1-4 frames of lag; don't copy that. Sprint is LT hold, a separate button (one verb per button, 2.1 L4). ER's shared button is the part not worth copying.

### 1.8 Wildbrands (Wildreach's weapon skills)
**ER reference (S11):** a weapon carries one swappable skill (an "Ash of War", ER's name), used with L2 and paid in FP. The weapon class limits which skills fit.

**Wildreach already has the content.** Every path tree has weapon-art nodes: Flame slash, Molten edge, Molten cleave, Ice slash, Flurry, Static strike, Charged weapon, Grave strike, Scythe sweep, Wave cut, Harpoon throw, Briar strike, Vine lash, Sun strike, Blazing arc, and so on. Raven's "weapon arts from your infused weapon" and the 8 quick slots (4 per path) are decided.

**The Wildreach name: Wildbrand.** It suits the game's world, where a path's element is *branded* into the weapon by the biome. A weapon art you've learned and slotted is a Wildbrand, so "the Molten edge Wildbrand" or "your Frost Wildbrands". It uses the same nodes Raven already designed and adds no new item.
- **Use:** the quick-slot button (D-pad up, or keys 1-8; see 2.1 L4) fires the Wildbrand in the selected slot through the held weapon, paid in that path's resource (Heat, Chill, …) plus stamina 18 (tuning `stamina.weaponArt`, today's number).
- **Class fit:** each Wildbrand node gets a `fits` list of weapon classes in data (Scythe sweep fits great weapons and spears; Harpoon throw fits spears). A Wildbrand that doesn't fit the held weapon greys out in its slot.
- **Charge and follow-ups (ER):** Wildbrands may have a hold-to-charge (Molten cleave) or a follow-up press (Flurry's second flurry), from data.
- **Hyperarmor and i-frames per Wildbrand** come from data and notifies, the way ER gives skills their own frames.
- **Renaming the UI from "weapon art" to "Wildbrand" is [RAVEN].** His nodes say "weapon art". Until he agrees, the docs and data keep "weapon art", and this bible uses Wildbrand as our working name for the system.

### 1.9 Spells on path resources
**ER (S12):**
- Spells need a casting tool (staff or seal) and cost FP.
- Memory slots: 2 base, up to 10 or 12.
- Many spells can be charged. Light spells are cast walking; heavy spells root you.

**Today:**
- Spells cost the path resource plus 12 stamina, with a cooldown.
- Lock-on bolts lead and home (6000 cm/s² against creatures, 2000 against players).

**Set:**
- **No FP and no memory slots.** The 8 quick slots (4 per path) are Wildreach's memory, as decided. The path resource is the FP.
- **No casting tool:** your path is the focus, since Raven has paths infuse whatever you hold. Spells sit in the same quick slots as Wildbrands and fire from the quick-slot button (D-pad up; 1-8 on keyboard, see 2.1 L4).
- **Cast while walking** at ×0.5 speed for bolts and shards. Root the caster for walls, novas, Meteor and Blizzard. Per-spell `rootsCaster` in data.
- **Charge:** spells marked `chargeable` deal up to ×1.5 at 1.0 s of hold.
- **Keep the homing rule:** players get less (2000) so they can dodge. Waveraider's projectile rules apply too: no homing after launch for boss projectiles, wind-up plus travel of at least 0.9 s early in the game, and nothing fired inside 4 m (2.1 L3).

### 1.10 Healing (the flask)
ER's flask: a limited stock refilled at rest, a commit animation, and enemies that read the drink and punish it (S13 input reading).

**Raven's rule:** crafted potions only, belt slots, a short cooldown.

**Set:**
- Drinking is a 0.9 s committed animation that can't be cancelled, with walking at ×0.4.
- Healing lands at 0.5 s.
- Belt holds 4 slots.
- Shared cooldown 1.5 s.
- AI reads the drink (1.13).

### 1.11 Weapon classes, movesets, scaling, infusions, upgrades
**ER (S11):**
- About 30 classes, each with its own moveset, stance values and crit multipliers.
- 13 affinities.
- +25 with smithing stones or +10 with somber stones.

**Today:**
- Melee: copper, bronze and iron swords. Tools (hatchet, pick) can be used as weapons.
- Ranged: hunting bow, crossbow, throwing spear, bola, blowgun.
- Damage = base × metal (1.0 to 2.8) × quality (0.9 to 1.2) × (1 + 5% per upgrade, +1 to +10).

**Set:**
- **Movesets per class**, each one a data row: chain length, timings, reach, stance damage, stamina, crit multipliers and `fits` for Wildbrands. One shared `UWRMeleeAbility` reads it; no class-specific code.
- **Classes:**
  - **Built:** sword (exists), axe (from the hatchet), pick-hammer (from the pick), spear (from the throwing spear).
  - **Next:** greatsword. Raven's own design doc names "a fire greatsword", so it's his.
  - **[RAVEN]:** daggers, great hammers, scythe.
- **Scaling:** no stat letters (ruled out). Path nodes are the scaling: Heated blade, Capacitor and Sun-forged give more Wildbrand damage. Keep it that way.
- **Infusions:** automatic from the path. A fire-path player's melee carries Fire, with burn buildup scaled by Fire nodes. The subclass path's infusion applies when a subclass Wildbrand is active. This is ER's affinity system without the menu, as Raven wrote ("the path infuses it").
- **Upgrades:** +1 to +10 with upgrade shards from mini-bosses, as Raven built. ER's two-track stones aren't needed.

### 1.12 Status buildup: ER's six onto Wildreach's paths
**ER rules (S14):**
- A meter fills per hit and procs at the target's resistance.
- The resistance rises after each proc. Malenia's bleed threshold goes 421, 519, 809, 1266.
- The meter decays when the target isn't being hit.
- Main bosses take less from procs: bleed ×0.7.

**Today:** `status` buildup threshold 100, decay 8/s after 3 s. Burn 4/s for 6 s. Frostbite: +15% damage taken for 6 s. Slowed ×0.6. Frozen 2.5 s. Shatter 30.

**Set: the shared rules (all statuses):**
- Threshold by target: creatures 100, mini-bosses 250, major bosses 400.
- After each proc, the threshold ×1.3 (stacks, resets when combat ends).
- Decay 8/s after 3 s (keep).
- Stealth hits +20% buildup (1.1).
- Players resist through armour sets and path root passives (Burning blood is immune to burns), never through stat points.

| ER status | ER effect (S14) | Wildreach status | Path(s) | Round | Set |
|---|---|---|---|---|---|
| Frostbite | 10% HP + 30, then +20% damage taken and slower stamina regen for 30 s. Can't re-proc while active | **Frostbite** (+ Slowed, Frozen, Shatter) | Frost | Built | 8% max HP + 20 burst, +20% damage taken, stamina regen ×0.6 for 12 s. Can't re-proc while active. Fire damage ends it (ER) and triggers Thaw (Raven's combo) |
| (none: fire is a damage type in ER) | | **Burn** | Fire | Built | Keep the damage over time. Add ER's "fire ends frostbite" cross-rule above |
| Hemorrhage | 15% max HP + 100, bosses ×0.7 | **Bleed** | Blood & curse; also Nature (Barbs) and Tide (Barbed tip) | R2 | 12% max HP + 15 burst. Bosses ×0.7. Grimjaw's "regenerates unless Bleeding" goes live |
| Poison | Small damage over time, long (90 s) | **Poison** | Spore | R8 | 0.4% max HP + 2 per second for 30 s |
| Scarlet rot | Large damage over time, plus a heal block in Wildreach | **Rot** | Spore | R8 | 0.8% max HP + 4 per second for 20 s. Purify (Nature) cleanses it |
| Sleep | Enemies sleep until hit (up to 60 s). Players are stunned about 1 s and lose FP | (no Wildreach sleep) | | | Not copied. Raven's statuses don't include sleep. Blind does the "AI loses you" job |
| Madness | 15% HP + 100, plus FP loss and a stun | **Curse** | Blood & curse | R2 | Proc: the target loses 25% of its path resource and deals −15% damage for 15 s (Raven's "Cursed enemies deal less damage"). The resource loss is the madness part |
| Death blight | Instant death | **Doom** (an ability, not a buildup) | Blood & curse | R2 | Built as Raven's Doom node specifies. No instant kills on players or bosses (proposal; Raven decides) |
| (Lightning damage) | | **Shock** | Lightning | R1 | Proc: a 0.4 s stagger plus chain damage to targets within 4 m. +25% on Soaked targets (Conductive) |
| (none) | | **Soak** | Tide | R6 | Proc: Slowed ×0.8 for 10 s; Drowning drains stamina. Fire buildup −50% on Soaked targets |
| (none) | | **Blind** | Sun & sand | R5 | Proc: creature AI loses target memory for 3 s and attacks the spot it last saw. Players see their screen vignette dim (no full blackout) |
| (none) | | **Decay / wither** | Necromancy | R4 | Proc: max HP −10% until the fight ends, and healing taken ×0.5 |
| (none) | | **Mark** | Arcane | R9 | Proc: the target takes +15% from all sources for 10 s and shows through walls to the marker's party |
| (none) | | Knockback (a hit property, not a status) | Wind | R7 | Pushes by the hit's poise damage. Never a status meter |
| (none) | | Corruption (cost, not status) | Void | R10 | A self-cost, as Raven wrote |

- **Status procs break hyperarmor**, as in ER (1.5).
- **Status UI:** a small meter under the enemy's health bar, filling in the status colour. ER shows player meters only; showing enemy meters is Wildreach's choice **[RAVEN]**.

### 1.13 Enemy design: telegraphs, delays, combos, aggression, punish windows
**ER (S13, S15):**
- **Long, readable wind-ups.** Some are held at the top of the swing to beat roll spam (Margit's staff).
- **Combo strings** whose last hit is delayed or reaches further (Malenia).
- **AI reads the player's action start** (drink, cast, attack), not the button press. Zullie's datamine calls parts of this conjecture.
- **Punish windows** after a final combo hit, a slam's recovery, or the pause after an AoE.

**Today:**
- Creature wind-ups rear back and growl. Raven turned the glow off on 11-01: "the attack clips wind up on their own".
- Bite tracking turns at 180°/s for the first 50% of the wind-up, then commits, so a late step aside makes it miss. That's correct ER behaviour; keep it.
- Bosses have phases at 50%, with ground decals on big attacks.

**Set (every creature and boss attack is a data row):**

| Field | Rule |
|---|---|
| `windup` | At least 0.34 s for the fastest pokes. Big hits 0.65-0.9 s. Boss specials 1.0 s or more. Projectile wind-up plus travel at least 0.9 s early, 0.65 s late. Max speed 1000 cm/s early, 1600 late (Waveraider 66/67) |
| `trackUntil` | Fraction of the wind-up spent turning (keep today's 0.5). Never tracks during the active window |
| `delayHold` | 0 to 0.6 s held at the top of the swing. Only on midland creatures and up, and bosses. A delayed attack always shows its final cue (sound and flash) about 0.2 s before contact, so the delay reads as a delay, not a cheat (Waveraider: "the glint always comes on the beat before contact") |
| `comboNext` | Chains to another attack row. At most 4 hits for creatures, 6 for bosses. The last hit may be delayed or longer-reaching |
| `punish` | Recovery after the last hit, at least 0.6 s for creatures and 1.2 s after boss slams. Hyperarmor ends at the last strike, so the punish is real |
| `parryable`, `low`, `unblockable`, `grab`, `ignoreHyperarmor` | Flags |
| `readsAction` | The creature can answer a player action-start (drink, cast, charged heavy) with a quick attack inside 0.4 s. Highland creatures and bosses only; never lowland |

**Rules for the stage:**
- **Packs aggro and attack freely (Aaron 11-10).** There is no one-at-a-time rule and no attack token; no real game has one. Pulling a pack one at a time is the PLAYER's skill: aggro radius, line of sight and pull tools (thrown stone, bow, whistle). Creatures keep real punish windows after their combos. Smart groups flank and surround (R2).
- **A creature starts a wind-up only while it's on the player's screen** (or audible inside 8 m with a direction cue). Nothing hits from off-screen without a sound first. Seen-from-the-knees-down counts as off-screen.
- **Colour law, Wildreach version.** Raven turned the body glow off (11-01), and that stands. Colour lives on ground decals, weapon edges and the UI:
  - **Gold:** yours, and block/parry.
  - **Crimson:** unblockable, dodge it.
  - **Status colours** follow the path (fire orange, frost blue…).
  - Nothing decorative may use crimson or gold. If Raven ever wants the glow back, it follows the same law.
- **Sound before sight.** Every wind-up longer than 0.5 s has a rising sound cue; people react about 40 ms faster to sound.

### 1.14 Bosses: phases, arenas, group play, running back
**ER (S15):**
- Phases change at HP thresholds, with a transition animation that is itself an attack (Radahn's gravity pull, Malenia's Scarlet Aeonia).
- Phase 2 adds new moves and changes the rhythm. It doesn't only add damage.
- Stakes of Marika put the respawn next to the boss door.
- Co-op HP scaling is about ×1.6 with 1 summon and ×2.3 with 2.
- Boss stance damage taken drops with helpers.

**Today:** Raven's frame already copies most of this: phases at 50%, arenas with mechanics, HP +80% per extra player or tame, the 10% loot ledger, and dying players run back in.

**Set:**
- **Transition = attack.** The 50% change plays an unblockable, telegraphed move (Vulkarn's anvil flood; Skaldrith's take-off gust). It never happens during a stance break or crit (1.4).
- **Phase 2 changes timing, not just damage.** At least two new delayed attacks and one new combo. One phase-1 move gains a delayed variant.
- **Several lock points on big bosses:** Skaldrith gets head, chest and each wing; Vulkarn gets chest and hammer arm (1.15).
- **HP scaling:** keep Raven's +80% per participant. Add ER's stance scaling (1.4). Status resistance ×1.25 per extra participant.
- **Run-back:** keep Raven's rule. A respawn point beside each boss arena, ER's stake, is **[RAVEN]**.
- **Bosses that punish tames** (R1): one move per boss that targets the furthest tame first.

### 1.15 Lock-on and camera
**ER:**
- Lock-on goes to the nearest target to the screen centre.
- A right-stick flick switches left or right.
- Big bosses have several lock points.
- The camera pulls back and pitches up on giants.
- Lock breaks at distance or when line of sight is lost.

**Today:** range 2500 cm, cone 40°, keep at 3500, lost after 2.5 s unseen. Turn speed eases from 2.5 to 7 over 0.5 s.

**Set:**
- **Flick switching:** the right stick past 0.6 sends the lock to the next target in that direction. Mouse: a flick of 300 px/s, or the wheel.
- **Lock points** come from bone sockets in data (`lockPoints`), and the flick switches between points too.
- **Giant framing:** when the lock target's height is over 3× the player's, the boom pulls back +150 cm and pitches up 10°.
- **Tall enemies:** lock-on aims at the weapon's height, not the feet (Waveraider).
- **Walls:** one spring arm that collides only with walls and big rocks (second-largest extent 1.6 m or more). Small props are ignored on a dedicated camera channel.
  - It pulls in within 0.12 s and eases back out after a 0.15 s hold, over about 0.4 s.
  - It hides the player below 1.0 m from the camera.
  - The camera never turns on its own (Waveraider 57).
  - Today's `WRCameraBoom` pulls in at once and eases out: right. Add the collision filter and the hide rule.
- **Never inverted by default.** Invert is an option.

### 1.16 Input buffer, cancel windows, animation commitment
**ER:**
- An input "will queue and be performed at the next available chance."
- About 10 frames (0.33 s) before the cancel point [EST]. The last input wins.

**Set:**
- **Buffer:** 0.33 s for attacks, roll, Wildbrand and items. The last input wins.
- **Dodge pressed mid-swing** stays buffered until the `CancelWindow` notify, up to 0.6 s (Waveraider's DS3 rule), then fires.
- **Notify states drive every window.** Designers never hard-code timings. The names:

| Notify | Purpose |
|---|---|
| `ANS_ComboWindow` | Accepts the next chain input |
| `ANS_CancelWindow` | Dodge, guard and item allowed |
| `ANS_WeaponTrace` | The existing `UWRWeaponTraceNotifyState` |
| `ANS_IFrames` | Invulnerable window |
| `ANS_Hyperarmor` | Extra poise window (1.5) |
| `ANS_ParryActive` | Parry window |
| `ANS_Tell` | Starts the sound and decal cue on creatures |
| `ANS_Tracking` | Creature turning allowed |

- Each notify sets a GAS tag (`State.ComboWindow`, `State.IFrames`…), so server and client read the same thing.
- **Held buttons across mode changes.** On entering a menu, build mode, taming channel, mount or the death screen, every held input is dropped and the buffer clears. On leaving, buttons still held are ignored until released, and discrete presses are blocked for 0.14 s (Waveraider game.ts 770-830). Otherwise the press that closed the menu also swings.

### 1.17 UE 5.8 C++: how to build it
Raven's stack is already right: GAS with the ASC on the PlayerState (Mixed replication), creature ASCs Minimal, StateTree AI, montages via `AbilityTask_PlayMontageAndWait`, server-confirmed traces, and a 250 ms rewind planned. Build on it; no new framework.

| Need | How |
|---|---|
| Attack step-in, ripostes, backstabs, stance crits | **Motion Warping** plugin. `UMotionWarpingComponent` on player and creatures. Warp targets set by the ability before the montage: step-in toward the lock target, clamped to 0.45-1.3 m (Waveraider). Paired crits warp both actors to a sync point |
| Root motion | Attacks and dodges use **montage root motion** (in place of a root motion source for dodge, if that fits Raven's predicted dodge). The dodge's distance by load tier is a warp scale. Never play a clip with baked root travel in place (2.2 T3) |
| Windows | The notify states in 1.16, each adding and removing a loose GAS tag. Server and client both run them; the server's tags are the truth |
| Stance and poise | New attributes in `UWRAttributeSet_Combat`: `Stance`, `MaxStance`, `IncomingStanceDamage`. Players keep `Poise`. One `UWRDamageExecution` path computes HP, stance, poise and buildup from the same hit. Never a second damage path (2.3 B1) |
| Lag compensation | The 250 ms ring buffer (M1 plan). The FULL-RELEASE-PLAN Round 0 notes it may not be built for melee, so check it first. Parries and i-frames are judged against the rewound time. Crits validated on the server |
| Hitstop | Client-only, as `WRFx` does today. Set `CustomTimeDilation` to 0.02 on the two actors' meshes (the attacker creeps at about 2%, God of War) for the hitstop time, then resume. Never pause server time or the CMC. After hitstop the montage catches up to server time over 0.1 s (no snap) |
| Prediction | Light, heavy, roll and parry are locally predicted GAs. Hits and statuses are server-only. Rolls stay predicted (today's design) so there's no rubber-banding at 100 ms |
| AI | StateTree per archetype (Raven's). Add tasks: `Circle`, `ReadPlayerAction`, `DelayHold`, `ComboNext`. Pack members act independently; there is no attack token |
| Data | Attack rows in `tuning.json` today. If the per-class and per-attack tables outgrow it, move them to `Data/attacks.json`, with the schema in `DATA_SCHEMAS.md` |

### 1.18 PvP rules
**ER (S16):**
- PvP damage has its own per-weapon, per-skill and per-spell multipliers (patch 1.07).
- Hyperarmor damage tiers were added in 1.10.
- Some dodge-skill i-frames are shorter against players.
- Peer-to-peer netcode gives laggy backstabs.

**Raven:** PvP is open except in safe zones. Artifacts never change PvP damage. PvP inside boss arenas is a setting. The death penalty is per server (Souls-like by default).

**Set:**
- **`pvp` block in tuning:** a global ×0.6 player-to-player damage, plus per-class, per-Wildbrand and per-spell multipliers on top. Status buildup in PvP ×0.7.
- **Crits in PvP:** backstab angle 75° (narrower than PvE). The victim gets 0.3 s of i-frames after getting up.
- **Dodge-skill i-frames** (Bolt dash, Air dash, Blink) are ×0.7 against players.
- **Homing:** keep `lockHomingPlayers` 2000.
- **Rewind:** server rewind is the only judge. No client-reported hits.

### 1.19 Not copied from Elden Ring
- **Spirit summons** (Aaron 11-10). Taming is the ally system, as Raven designed it.
- Stat points and scaling letters (ruled out by Raven).
- FP and memory slots (path resources and quick slots instead).
- Sleep as a status.
- Dodge on release (1.7).
- Peer-to-peer hit authority.

---

## Part 2. Lessons from our games, as do/don't for Wildreach

### 2.1 Waveraider (research 57, 66, 67, 71, 73-76, 82-84, 87; docs/COMBAT.md)

**L1. Hit feel is a stack, not a number.** Waveraider's "hits don't feel impactful" was fixed by building the whole stack (73), and Aaron said "feels way better".
- **Do:** on every connected hit:
  - the victim always reacts (a flinch, or an armour shudder);
  - white flash 70 ms (light) to 260 ms (heavy);
  - sparks along the swing;
  - camera trauma 0.04 + 0.22w², and an FOV punch of 0.2 + 1.3w°;
  - rumble 35 + 85w ms (the strong motor stays off on lights);
  - four sound layers (transient, body, material, creature grunt);
  - music ducks 2-4 dB on the biggest hits.
- **Don't:** ever let a hit land silently or without a reaction.
- **Hitstop:**

| Hit | Hitstop | Today |
|---|---|---|
| Light | 0.06 s | 0 |
| Heavy | 0.10 s | 0.06 |
| Charged and guard counter | 0.14 s | |
| Parry | 0.09 s | 0.09 (keep) |
| Riposte and crit | 0.20 s | |
| Boss kill | 0.30 s | |

  Freeze at the blade's deepest point, not first contact. Shake the victim 3-6 cm at 30 Hz, not the camera. Nothing over 0.3 s.

**L2. Swing weight.** Waveraider's wind-up was warped to 16 ms and swings felt like nothing.
- **Do:** a minimum visible coil (0.14 s light, 0.20 s heavy). Follow-through at ×1.0. Settle 0.26-0.34 s.
- **Don't:** speed-scale wind-ups past ×2.2 for attack-speed nodes.

**L3. Telegraphs and the colour law** (COMBAT.md, 66, 82).
- Gold = yours and block/parry. Crimson = unblockable. Never use these colours on decoration.
- Length was never the problem; visibility was. Tells grow over the whole wind-up and read at 15 m.
- Projectiles: wind-up plus travel of at least 0.9 s early. No homing, nothing fired inside 4 m.
- **Wildreach version:** see 1.13. It respects Raven's glow-off call.

**L4. One verb per button** (71). Waveraider's RT was parry, block, charge, counter and finisher at once, and Aaron rejected the "fix it with words" plan. The Wildreach pad layout:

| Button | Action |
|---|---|
| RB | Light |
| RT | Heavy (hold = charge; also guard counter) |
| LB | Hold = block, tap = parry |
| LT | Sprint (hold) |
| A | Jump |
| B | Roll (stick at rest = backstep) |
| X | Use / interact |
| Y | Drink the selected belt item |
| D-pad up | Fire the selected quick slot (Wildbrand or spell) |
| D-pad left / right | Cycle the 8 quick slots |
| D-pad down | Cycle the belt |
| R3 | Lock on; flick the right stick to switch |

- **Why not ER's L2:** on this pad LT is sprint, and chords (LB+RB) break one-verb-per-button. Quick slots fire from **D-pad up**, cycled left/right, which is close to ER's own D-pad spell and item cycling. The keyboard keeps 1-8 for direct quick slots, as Raven built.
- A guard pressed early still blocks. A failed defence is never worse than doing nothing.

**L5. Unkillable enemies have causes, not bad luck** (67, 71, 75, 82).
- **Floor bug:** blows stopped at 1 HP while the finisher needed a stance break.
- **Targeting bug:** the check used the creature's origin height, so wall-slotted and half-sunk creatures couldn't be hit. Lock-on skipped the check.
- **Flyer bug:** flyers had gravity applied and sank into the floor.
- **Do:**
  - the 1-HP rule (1.4);
  - aim and lock-on share one target test against body height, not the origin;
  - flyers (Scorchwing, Ignessa, Skaldrith in the air) have no gravity while flying, and their hurtboxes stay reachable by lock-on with +50 cm reach;
  - every creature is checked in the combat lab for "can I hit it from the ground".

**L6. Prompt diet** (COMBAT.md, REQ-013/038).
- At most one prompt on screen.
- The riposte and crit prompt shows once per species, then never again (saved per player).
- No hit words (PERFECT, BLOCKED). Only state words: GUARD BROKEN, STANCE BROKEN.
- Damage numbers sit beside the creature, never over its tell. They can be turned off in options.

**L7. Camera at walls** (57). Twelve systems fighting over one camera was the bug. One spring arm with a filtered channel is the fix (1.15).

**L8. Input isolation** (game.ts 770-830). Taming channels, build mode, menus and the tame whistle drop held inputs, block presses for 140 ms on exit, and need a fresh press (1.16).

**L9. Block costs** (COMBAT.md). Waveraider's "block-forever is free" was fixed with stamina costs and a guard break. Wildreach has the same gap today (0% chip, full regen while guarding). Fixed in 1.2: regen pauses while guarding and a blocked hit costs stamina. Holding alone costs nothing.

**L10. Controller first, F1, F8** (REQ-069/073/074; 84).
- Prompts show only for the last-used device, and only when a press is needed. No stray icons.
- **F1** opens the controls page for the device in use.
- **F8 (Back on pad)** is the in-game bug report: a screenshot, a short text box, the build version, map position and last 60 s of log, with B to back out. This is how Aaron and his testers report (standing rule).
  - Wildreach doesn't have one. Where reports go (Raven's inbox, a GitHub issue, F35's portal) is **[RAVEN]**.
  - Build it in the first combat step so every later step is reported on.
- Keep one requirements ledger of every report. The latest call wins. Pull open reports at the start of every session: Waveraider missed 16.

**L11. The lab, not agent playtests** (74, 84 REQ-049/050/051).
- `L_CombatArena` exists. Make it the combat lab:
  - auto-spawn the chosen creature 6 m away 0.4 s after load, and respawn it 1.5 s after death;
  - D-pad cycles preset question cards;
  - one test card at text size 24 or more, the only text on screen;
  - every tunable on that card has a row.
- **Each lab round asks specific questions,** for example:
  1. Can you see every Cinderfang wind-up coming?
  2. Does the heavy feel heavy?
  3. Can you parry the Vulkarn tongs?
  4. Is the roll distance right at medium load?
  5. Is the stance break readable?
- Answers come back as F8 reports tagged "Lab". Aaron and testers play it; we never do.

**L12. Geometry audit** (`tools/levelcheck/geometry-audit.mjs`).
- **The checks:** floating props, walkable no-collider pieces, invisible walls, clipping through walls, gaps at an area's edge.
- **The self-check:** it first proves every check on planted bugs (exit 2 if one is missed). Allowed cases go in a list with a reason.
- **For Wildreach:** an editor commandlet doing the same over boss arenas and caves. Arenas matter most: a camera or body stuck in an anvil ruins the fight.

**L13. Music and mix** (20, 24, 58, 74).
- Loudness around −18 LUFS integrated (calm −23, boss −15 short-term), true peak −1 dBTP. The limiter acts less than 1% of the time.
- Master volume sits *before* the dynamics. A master after the limiter was the "painful at 10%" bug.
- Music ducks under combat sound effects instead of both hitting the limiter.
- Every species gets its own voice (FULL-RELEASE Round 0 already asks for unique creature voices).
- Music models must allow commercial use (ACE-Step MIT, not MusicGen or YuE).

**L14. Rewards from first kills** (83, the Miracles drop design).
- Each named boss's or elite's first fall drops something visible. Waveraider's is a gold light that flies to the player after 6 s or within 4 m, with a name card.
- It's usable at once if a slot is free, so the reward is played with straight away.
- **Wildreach version:** the boss part (25% chance today) and the altar's path unlock already follow this idea. Make the first kill's drop always visible and named, and fly it to the killer.
- Raven's personal-loot rule stays.

**L15. Never add helpers after a failure** (memory 11-07). No slow-downs or wider windows because someone died. Design the fight so it can be passed by learning, then tune from reports.

### 2.2 Trails of Appalachia (Docs, Saved/Handoff, memory)

**T1. Hitches are VRAM and PSO first, not "smoke".**
- **What happened:** another app holding VRAM made the GPU page over PCIe. There were 219-289 runtime PSO hitches because no PSO cache was bundled.
- **Do:**
  - record and bundle a fresh PSO cache every package (`PERFORMANCE.md` has the steps; Raven planned it for the Final round, do it every cook);
  - warm shaders at boot;
  - clamp the Nanite and VSM pools for 8 GB cards;
  - warn at launch when another process holds more than 1 GB of VRAM.

**T2. Runtime code never sync-loads.** A film-grain texture sync-loaded mid-play and hitched.
- **Do:** preload combat VFX, hit sounds and boss clips via `preload.json`, which Raven already has.
- **Don't:** load any montage or Niagara system on first hit.

**T3. Animation must be driven by real movement.** Deer "teleported back" every loop, humans glided, a 6.5 m/s run played a 4.7 m/s clip.
- **Do:** one choice per mover, either in-place loops with stride warping or real root motion. Starts and stops. State switches gated by hysteresis.
- **Don't:** play baked root travel in place, mix procedural lean on clips that already lean, or hand-roll locomotion. Use GASP, which Raven's M1 already picked.

**T4. Additive layers can build up.** Trails' hands and Waveraider's torso twist both built up over frames.
- **Do:** reset to the clean pose each frame before adding. Blend IK alphas, never flip 0↔1. Hysteresis on every state threshold.

**T5. Attach and grounding.** Held items floated because sockets didn't exist, pivots sat at the base, and items attached before the body streamed in.
- **Do:** one attach path. A `Grip` socket, then a grip offset from data, then a bounds fallback. Re-seat after the body loads. Absolute scale. Left-hand IK on two-handed weapons.
- This is FULL-RELEASE Round 0's "held tools as real models".

**T6. First-person body** (Aaron 10-29, Trails).
- Camera at the head bone. The body is visible from the belly down, plus the arms. Head and neck hidden. Real gait.
- This is Round 0's "full-body first person". Wildreach today has separate FP arms (M1).
- **For combat:** FP attacks read the same notifies as 3P. The trace comes from the 3P weapon on the server, never the FP arms.

**T7. Cooking traps.**
- A skeleton-less clip crashes the cook. Raven already hit this with the Barghest mane and tail.
- Stale Niagara blocks imports.
- Always recompile after a merge.
- Ship the exe with its matching PDB.
- Two `UnrealEditor-Cmd` processes deadlock: one editor slot per machine.
- Shipping strips the logs the checks read.

**T8. Disk and RAM.**
- Cooks need about 30 GB plus staging. Refuse a cook under 40 GB free.
- Cap UBT `MaxParallelActions` (6 on the laptop).
- One heavy job at a time, and kill stalled jobs at once.
- FULL-RELEASE section 3 already has the Wildreach version. Keep it.

**T9. Never spawn a pawn before its ground exists.** Trails' perf camera fell out of the world. Bosses, packs and respawning players wait for collision under them (World Partition streaming).

**T10. Players die, no floors.** Trails removed a 20% bee-sting floor, bleed floors and an immortal Downed state.
- **Do:** every Wildreach hazard and status can kill. Bleed, burn and starvation all go to zero.
- The only protections are the ones Raven designed: safe zones and offline raid protection.

**T11. The verb quality bar.** Each verb gets a sheet: logic, fun, clip, sound, feel, readability, and what it adds. No placeholder clip and no silent verb. Apply it to every attack row in 1.1.

### 2.3 CORELODE, Black Monolith, Tavern's Mettle
**B1. One combat engine.** Black Monolith's battle code and balance sim each re-coded crits, statuses and lifesteal, so they drifted.
- **Do:** everything through `ApplyHit` / `UWRDamageExecution` (Raven's rule already).
- Any balance or bot tool calls the same code, never a copy.

**B2. Difficulty bands** (Black Monolith).
- Every fight should be dangerous but winnable at level with the core kit (roughly a 40-75% win chance on first meetings).
- Stacked multipliers (area × base) made walls at 3× player HP.
- **Do:** compute player time-to-kill against creature time-to-kill for each band. Raven's starting numbers aim at 5-7 hits per mob, so check them after every multiplier.

**B3. Green tests mean nothing** (CORELODE).
- A fishing system passed 118/0 while unplayable.
- A mission was arithmetically unwinnable with every suite green.
- Five "built" systems had no call sites.
- **Do:** keep Raven's suites green (his rule), but treat them as wiring checks only. Fun and feel come from Aaron's and testers' F8 reports.
- Grep every new ability for a real call site and a reachable unlock.

**B4. Monsters must terrify to look at** (CORELODE).
- A detail budget above everything else, and a silhouette readable at a glance in the dark.
- Show the body standing still, at scale, before writing behaviour.
- **For Wildreach:** boss and highland creature art is graded first (QUALITY-PLAN row 4).

**B5. Never procedural primitives** (Black Monolith). "Disgusting trash." Bosses on greybox proxies are fine while fights are tuned (Raven's M6 plan), but nothing ships on engine shapes.

**B6. Counterplay from the build, not from numbers** (Black Monolith loadout).
- Wildreach has no loadouts by design. Counterplay comes from the subclass (the boss weak to the opposite element) and the 8 quick slots reassigned out of combat.
- Bosses are tuned so the intended counter shows. Don't flatten weaknesses with HP.

**B7. Server-authoritative, shared rules code** (Tavern's Mettle).
- The server replays the outcome. Shared rules live in one place.
- Every unlockable must have a place where it's earned. Wildreach's path nodes, boss parts and Wildbrands each need a reachable source.

### 2.4 Aaron's standing rules for this cook
- **No agent playtests.** Aaron and his testers test, and report through F8 (L10). We don't play the build, run bots as players, or loop screenshots. Raven's own automated suites still run before a push, because his repo rules stand (Aaron 11-10).
- **Ship finished steps.** Each combat step below is one cook into a new Development package for Aaron. One cook is one version. Who else gets builds is Raven's call.
- **Never claim done** unless it's in the cooked build.
- **Cook it again means:** research, then plan, then an improved build. Never a patch on a patch.
- **Build exactly what's written.** Anything in this bible marked [RAVEN] waits for his approval in the next planning doc.

---

## Part 3. Region sizes (Aaron 11-10: regions may vary)

This supersedes "all the same size, 9 creatures, 1 major, 3 minis".
- **Fixed by Raven's rules:** every biome has exactly **1 major boss** (its altar unlocks the path) and **3 to 5 mini-bosses** (build plan).
- **What varies:** land area and the creature count. Raven's creature doc says "Biomes don't need every role, so each one leans toward its own strengths."
- **Area** is the biome's wedge of the 12-wedge landmass. Underground and air volume are listed separately.

| Biome (path) | Surface km² | Extra volume | Creatures | Major | Minis | Why |
|---|---|---|---|---|---|---|
| Volcanic (Fire) | 5.0 (built) | Lava tubes (sub-biome) | 9 | 1 | 3 | Built. Top up 1 mini |
| Frozen tundra (Frost) | 5.0 (built) | Ice caves | 9 (Floeback at R6) | 1 | 3 | Built. Top up 1 mini |
| Storm peaks (Lightning) | 5.5 | High cliffs, vertical | 8 | 1 | 3 (1 roaming) | Steep, sparse. Fewer, tougher creatures; weather is the threat |
| Swamp (Blood & curse) | 4.0 | Shallow water | 10 | 1 | 3 | Dense and close. Ambushers in murky water |
| Ancient forest (Nature) | 7.0 | Canopy paths | 12 | 1 | 4 | The richest biome: farming, villages, husbandry need many species |
| Graveyard (Necromancy) | 2.5 | Catacombs about 2 km² | 8 | 1 | 4 | Small surface, deep dungeon. Raised dead add variety; crypts hold minis |
| Desert (Sun & sand) | 6.5 | Buried ruins | 7 | 1 | 3 (1 roaming sand-beast) | Wide and open, but dense with oases, ruins, canyons and wrecks. Long sight lines, few big hunters |
| Ocean & reefs (Tide) | 3.0 land | About 6 km² of sea and reef | 12 | 1 | 3 | Swimmers fill the sea; the most species by volume |
| Sky islands (Wind) | 2.0 (islands) | About 5 km² of air | 6 | 1 | 3 | Few, mostly flyers. The space is the challenge |
| Fungal caverns (Spore) | 1.5 | Caverns about 3 km² | 10 | 1 | 3 | Underground, crowded, swarming |
| Crystal caves (Arcane) | 1.0 | Caves about 2 km² | 6 | 1 | 3 | The smallest. Puzzles and wards over head counts |
| Corrupted wasteland (Void) | 6.0 | Rifts | 8 | 1 | 5 | Endgame. Corrupted variants, a mini-boss gauntlet before the last major |
| **Total** | **49.0** | | **105** | **12** | **40** | |

- **Map:** about 49 km² of surface. That replaces the earlier "about 59 km² at equal density"; underground and air are extra. The 12 wedges are no longer equal slices: each wedge's angle follows its area, while opposite pairs still sit straight across.
- **Decoration and variety rule (Aaron 11-10: enough decoration and variety to keep every region interesting).**
  - **Density floor:** a landmark, encounter or point of interest at least every ~150-250 m of travel on any route. No stretch of 250 m reads empty.
  - **Sub-zones:** each biome has 3-5 distinct sub-zones, each with its own ground, palette, props and creature mix.
  - **Set dressing:** every sub-zone gets scatter props, ground-cover variation, silhouettes on the skyline and small story details (camps, wrecks, bones, markers), so no view is bare terrain.
  - **Sub-zone lists:**
    - Volcanic: ash plains, lava fields, obsidian ridges, vents and geysers, lava-tube mouths.
    - Frozen tundra: snow flats, ice shelves, frozen lakes, pine hollows, ice-cave mouths.
    - Storm peaks: cliff paths, wind passes, lightning-struck crags, rope bridges, shrines.
    - Swamp: murk pools, mangrove thickets, rotting boardwalks, bone mounds, reed flats.
    - Ancient forest: old-growth groves, meadows and farmland, villages, streams and falls, canopy paths.
    - Graveyard: grave fields, mausoleum rows, ruined chapel, bone pits, crypt entrances.
    - Desert: oases, ruins, dunes (soft sand) vs hardpan (cracked flats), canyons, wrecks (caravans and sunken-ship hulls), rock spires, a sandstorm belt.
    - Ocean and reefs: beaches, tide pools, kelp forests, reefs, wrecks, sea caves.
    - Sky islands: windward cliffs, cloud gardens, ruined sky-bridges, floating shrines.
    - Fungal caverns: spore gardens, glow-moss halls, mycelium webs, flooded grottos.
    - Crystal caves: geode halls, ward chambers, crystal forests, echo tunnels.
    - Corrupted wasteland: rifts, withered forest, void-scarred ruins, mini-boss gauntlet road.
  - **Desert especially:** a smaller map (6.5 km²) with oases, ruins, dune and hardpan contrast, canyons and wrecks spaced inside that density floor, so long sight lines still always show something to walk toward.
- **Art:**
  - **New creatures:** 87 (R1 to R10).
  - **New bosses:** 10 majors plus 34 minis, and 2 top-up minis for Fire and Frost, so 46.
  - **Total:** about 133 models, about the same as the earlier estimate of about 130.
- **Roles per biome** still come from each round's design step (aggressive, neutral, passive mix chosen to the biome's strengths). Aaron approves before art.
- All names and numbers are first drafts in data, per Raven's rules.

---

## Part 4. Combat build order for the desktop cook

Starts with what exists: Fire and Frost, the sword and shield, the 18 creatures, and Vulkarn, Skaldrith and the 4 minis. Each step is one Development cook into a new folder for Aaron. Raven's suites run before each push. One heavy job at a time. Step C0 needs the Fab packs, the 4 TB drive and `check_setup` all PASS (FULL-RELEASE 0.1).

| Step | What | Sections |
|---|---|---|
| **C0** | **Read and check.** Read `WRCombatLibrary`, the melee GAs, `WRCreature`, `WRBoss` and `WRFx` against this bible. List what's built, what differs and what's missing (including the melee rewind). Paper and a compile, no cook | 1.17 |
| **C1** | **Reporting and controls first.** F8 bug report (destination [RAVEN]), F1 controls per device, the one-verb pad layout, held-input isolation, and the combat lab with question cards in `L_CombatArena` | L4, L8, L10, L11, 1.16 |
| **C2** | **Feel.** The full hit stack and hitstop table, swing weight, motion-warp step-in, the waist split for attacking while moving, the notify windows (combo, cancel, i-frames), the 0.33 s buffer | L1, L2, 1.1, 1.16, 1.17 |
| **C3** | **Stamina and roll.** Regen 45/s after 0.5 s, load tiers and i-frames, backstep, jump leg i-frames, the "last swing" grace | 1.6, 1.7 |
| **C4** | **Guard, parry, crits.** Guard Boost, chip damage, guard break, guard counter, parry tap/hold, riposte, backstab, paired crits with motion warping, server rewind validation | 1.2, 1.3 |
| **C5** | **Stance and poise.** The stance/poise split, stance break and crit, group stance scaling, hyperarmor, the 1-HP rule, the prompt diet | 1.4, 1.5, L5, L6 |
| **C6** | **The full attack set.** Charged heavy, running, rolling, jump and crouch attacks, the stealth bonus, counter-hits | 1.1 |
| **C7** | **Status.** Rising resistance and decay. Frostbite and Burn to the 1.12 numbers. Fire ends frostbite. Meters under health bars ([RAVEN] for the meter) | 1.12 |
| **C8** | **Wildbrands and spells.** Every built Fire and Frost weapon-art and spell node: fits, charge and follow-ups, cast-while-walking, the D-pad slots. Build the "[not built yet]" Fire and Frost nodes (Meteor, Ice wall, Blizzard, Flurry…) here | 1.8, 1.9 |
| **C9** | **Creatures.** Attack rows for all 18: wind-up and tracking, delay holds, combos, punish windows, on-screen-only tells, sound cues, `readsAction` on highland creatures, the colour law on decals | 1.13, L3 |
| **C10** | **Bosses.** Vulkarn and Skaldrith to the 1.14 bar: transition attacks, phase-2 timing changes, lock points, stance, and the 4 minis. Top up a third mini per biome (Round 0 / R1) | 1.14, 1.15 |
| **C11** | **Camera and healing.** Spring-arm filter, giant framing, flick switching; drink commit and AI reading | 1.15, 1.10 |
| **C12** | **PvP pass on what exists.** The `pvp` tuning block, crit angles, dodge-skill i-frames vs players | 1.18 |
| Later | **Each biome round** adds its path's status (1.12 column "Round"), its Wildbrands, and its creature and boss attack rows on this frame. No new combat code per biome unless its design step asks | |

C1 to C5 are the "one boss fight feels good" core, Raven's own M1 rule: "If one boss fight feels good, 12 biomes are worth building; if it doesn't, more biomes won't fix it."

---

## Sources
- S1 Fextralife, Attacks: https://eldenring.wiki.fextralife.com/Attacks
- S2 Bandai Namco, Elden Ring combat guide: https://www.bandainamcoent.com/news/elden-ring-introduction-part-4-combat-guide
- S3 Fextralife, Parry: https://eldenring.wiki.fextralife.com/Parry
- S4 wiki.gg, Guard Boost: https://eldenring.wiki.gg/wiki/Guard_Boost ; Fextralife, Guard Counter: https://eldenring.wiki.fextralife.com/Guard+Counter
- S5 wiki.gg, Critical Attack: https://eldenring.wiki.gg/wiki/Critical_Attack
- S6 Fextralife, Critical Damage: https://eldenring.wiki.fextralife.com/Critical+Damage
- S7 wiki.gg, Stance: https://eldenring.wiki.gg/wiki/Stance ; Fextralife, Stance: https://eldenring.wiki.fextralife.com/Stance
- S8 Fextralife, Poise: https://eldenring.wiki.fextralife.com/Poise ; wiki.gg, Hyperarmor: https://eldenring.wiki.gg/wiki/Hyperarmor
- S9 Fextralife, Stamina: https://eldenring.wiki.fextralife.com/Stamina ; wiki.gg, Stamina: https://eldenring.wiki.gg/wiki/Stamina
- S10 Fextralife, Equip Load: https://eldenring.wiki.fextralife.com/Equip+Load ; wiki.gg, Invincibility Frames: https://eldenring.wiki.gg/wiki/Invincibility_Frames
- S11 wiki.gg, Affinities: https://eldenring.wiki.gg/wiki/Affinities ; Fextralife, Upgrades: https://eldenring.wiki.fextralife.com/Upgrades
- S12 Fextralife, Memory Stone: https://eldenring.wiki.fextralife.com/Memory+Stone
- S13 PC Gamer on Zullie's AI input-reading analysis: https://www.pcgamer.com/elden-ring-dataminer-explains-why-enemies-smack-you-when-you-reach-for-a-potion
- S14 Fextralife: Hemorrhage https://eldenring.wiki.fextralife.com/Hemorrhage , Frostbite https://eldenring.wiki.fextralife.com/Frostbite , Sleep https://eldenring.wiki.fextralife.com/Sleep , Madness https://eldenring.wiki.fextralife.com/Madness ; wiki.gg Poison https://eldenring.wiki.gg/wiki/Poison
- S15 Fextralife: Margit https://eldenring.wiki.fextralife.com/Margit+the+Fell+Omen , Malenia https://eldenring.wiki.fextralife.com/Malenia+Blade+of+Miquella , Radahn https://eldenring.wiki.fextralife.com/Starscourge+Radahn , Stakes of Marika https://eldenring.wiki.fextralife.com/Stakes+of+Marika
- S16 wiki.gg, Multiplayer Correction: https://eldenring.wiki.gg/wiki/Multiplayer_Correction ; Shacknews, patch 1.07 PvP scaling: https://www.shacknews.com/article/132654/elden-ring-version-107-patch-notes-pvp-damage-scaling
- Our files:
  - Waveraider: `C:\Users\acars\waveraider\research\` 57, 66, 67, 71, 73, 74, 75, 82, 83, 84, 87, plus `docs\COMBAT.md`.
  - Trails: `C:\Users\acars\TrailsOfAppalachia\Docs` (MOVEFIX, ANIM-PLAN, ATTACH, HANDS-PLAN, LAB3-PLAN) and `Saved\Handoff`.
  - Memory files: trails-*, corelode-*, black-monolith-*, taverns-mettle-project.
  - Raven's repo: `Docs/plan/M1_combat.md`, `M4_creatures.md`, `M6_bosses.md`, `Docs/design/*`, `Data/tuning.json`, `CLAUDE.md`.
