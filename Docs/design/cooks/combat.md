# Wildreach cook: COMBAT

Lane: combat. Written 2026-11-13. Scope: FULL-GAME section 5 (5.1 to 5.3) and COMBAT-AND-LESSONS (CAL) build order C0 to C12: weapons and movesets, stamina, roll, guard, parry, crits, poise and stance, status effects, Wildbrands, spells, lock-on and camera, potions, boss rules, creature attack rows, PvP.

How to read the tags:
- **SPEC**: BUILT or RAVEN-PLANNED. Kept exactly; the source is cited.
- **GAP-COOKED**: was a GAP in FULL-GAME. Fully designed here. Where CAL (Aaron's 11-10 decisions) already set a number, CAL's number stays unless this cook says why it changes.
- Times are seconds. "Windup" runs from the press (or the creature's first tell frame) to the first active frame. "Active" is the frame range that can deal damage. "Recovery" runs from the last active frame to the end of the clip. Frames at 30 fps = seconds × 30.
- Damage numbers are copper, common quality, +0. Every other tier multiplies as Raven built (SN Weapons).

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game | What it does | Lesson for Wildreach |
|---|---|---|
| **Elden Ring** | Stamina-gated attacks, roll i-frames by equip load, guard boost, parry into riposte, enemy stance break into a crit, hyperarmor on big weapons, contextual attacks (running, rolling, jump, crouch), weapon skills on one button, AI that reads the drink [S1-S16 in CAL; Bandai Namco combat guide S2] | The frame. Already mapped in CAL; this cook keeps every CAL number and fills the parts CAL left open (movesets per class, every Wildbrand, every spell shape, every status on players, creature rows, boss move lists) |
| **Sekiro** | Posture fills fastest from deflects; follow-up counters do posture-only damage; posture regenerates slower when vitality is low; break = deathblow [Fextralife Posture; samurai-gamers] | Parries are the best stance tool (a parry on a mini or major deals a parry count, not HP). Stance regen slows as HP drops: below 50% HP, stance regen ×0.5. A defensive player wins faster than a greedy one |
| **Monster Hunter Wilds** | Focus mode shows wounds; hitting a wound with a focus strike destroys it, staggers the monster, drops material. Weapon identity: the hammer is the KO weapon (head hits stun), the great sword is the charge weapon [Mobalytics focus/wounds; Fextralife MHW Great Sword] | Breakable parts on big creatures and bosses: a part has its own stance, breaking it staggers the body, changes a move and drops a material. Strike weapons on the head build an extra "daze" for KO (here: head lock point takes ×1.5 stance from strike). Weapon classes get a job, not just a speed |
| **Horizon Zero Dawn** | Tear damage strips armour and components; removed parts expose weak points and change machine behaviour [Horizon Wiki Damage; Shacknews weaknesses] | Same as above for armoured creatures (Obsidian Warden shell, Glass Empress, Old Barnacle): physical type matters. Strike breaks shells, pierce finds gaps, slash shreds hide |
| **Dragon's Dogma 2** | Climbing big monsters to reach weak points; grabbing legs trips them [Dragon's Dogma Wiki Climbing; primagames] | Not copied as climbing (Raven has no climb verb). Taken as: legs as lock points with their own stance on giants; breaking a leg's stance trips the body (knockdown = crit window on the head) |
| **Valheim** | Block armour vs. stagger bar; a timed block parries and doubles damage taken by the enemy for a short time; tower shields can't parry [highgroundgaming; gamever] | Greatshields can't parry (CAL agrees). Parried creatures take ×1.5 damage from everyone for 2.0 s on top of the riposte option: a group reward for one player's parry |
| **Enshrouded** | Parry stuns; merciless attacks during the stun; stagger bar built by weapon and shield power [GameSkinny; Sportskeeda] | Confirms the survival-crafting audience accepts a real parry. The parry tool (shield class) sets how fast you build stance by parrying |
| **Breath of the Wild / Tears of the Kingdom** | Perfect dodge and perfect guard open a slow-time flurry rush counter [Zelda Wiki Perfect Dodging, Flurry Rush] | Not copied as slow time (bad in multiplayer). Taken as Frost's Frozen moment capstone trigger and a clear rule for "perfect dodge": a hit absorbed by roll i-frames in the roll's first 0.20 s |
| **Ghost of Tsushima** | Four stances, each strong against one enemy type [Fextralife stances; primagames] | Weapon classes and physical types play this role: no stance-swap button (one verb per button), but each creature row has `physResist` so the right weapon visibly works |
| **V Rising** | Every ability is an aimed, telegraphed skillshot; counters and shields are readable answers [TechRaptor abilities; studioloot] | Enemy projectiles are aimed at where you were, never home after launch (our hard rule). Player spells keep Raven's lock-on homing (players are the ones who must hit) |
| **Conan Exiles** | Light/heavy chains per weapon type, up to 4 hits, mixed chains, heavy finishers carry statuses [Funcom forums; mein-mmo] | Mixed chains: a heavy pressed inside a light chain's combo window plays that class's "chain heavy" (a stronger heavy at the same cost). It makes the chain an input language without new buttons |
| **Elden Ring mounted (Torrent)** | No block or weapon skill on horseback; attacks to either side; unhorsed = stunned on the ground [primagames Torrent guide] | Mounted rules below: no guard, no parry, Wildbrands only if flagged, rider knockdown 1.2 s when unhorsed |
| **Dark Souls 3 greatswords** | Hyperarmor on 2H lights and heavies, running and rolling attacks [DS3 wiki] | Great weapons get hyperarmor on every attack except the first light of a chain |

### 1.2 Sources (new in this cook; CAL S1 to S16 still apply)
- Mobalytics, Focus Mode: https://mobalytics.gg/mhw/guides/focus-mode-explained ; Wounds: https://mobalytics.gg/mhw/guides/wounds-explained
- Fextralife Sekiro, Posture: https://sekiroshadowsdietwice.wiki.fextralife.com/Posture ; samurai-gamers, Posture System: https://samurai-gamers.com/sekiro-shadows-die-twice/posture-system/
- High Ground Gaming, Valheim shields: https://www.highgroundgaming.com/valheim-best-shields/ ; gamever, Valheim blocking: https://gamever.io/knowledge-base/blocking-parrying-in-valheim-shield-types-the-parry-window-and-stagger
- Primagames, Dragon's Dogma 2 climbing: https://primagames.com/gaming/how-to-cling-to-and-climb-enemies-in-dragons-dogma-2 ; Dragon's Dogma Wiki, Climbing: https://dragonsdogma.fandom.com/wiki/Climbing
- Zelda Wiki, Perfect Dodging: https://zeldawiki.wiki/wiki/Perfect_Dodging ; Flurry Rush: https://zeldawiki.wiki/wiki/Flurry_Rush
- Horizon Wiki, Damage: https://horizon.fandom.com/wiki/Damage ; Shacknews, machine weaknesses: https://shacknews.com/article/99122/horizon-zero-dawn-how-to-exploit-machine-weaknesses
- Fextralife, Ghost of Tsushima stances: https://ghostoftsushima.wiki.fextralife.com/stances
- GameSkinny / Sportskeeda, Enshrouded combat: https://www.gameskinny.com/?p=151785 , https://www.sportskeeda.com/esports/enshrouded-combat-mechanics-how-attack-and-defend
- TechRaptor, V Rising abilities: https://techraptor.net/gaming/guides/v-rising-abilities-guide
- mein-mmo, Conan Exiles combos: https://mein-mmo.de/en/conan-exiles-combos-und-karte,269247/
- Primagames, Elden Ring Torrent and mounted combat: https://primagames.com/gaming/elden-ring-torrent-mounted-combat
- Fextralife, Elden Ring Stance: https://eldenring.wiki.fextralife.com/Stance ; Attacks: https://eldenring.wiki.fextralife.com/Attacks
- Fextralife DS3, Claymore (greatsword hyperarmor): https://darksouls3.wiki.fextralife.com/Claymore
- Fextralife MHW, Great Sword: https://monsterhunterworld.wiki.fextralife.com/Great Sword

ER per-class frame data isn't published; class timings below are designed to our coil, buffer and hitstop rules and to ER's feel bands (dagger fastest, colossal slowest), not copied frame-for-frame.

### 1.3 Lessons we take (short)
1. Copy ER's frame exactly (CAL); add Sekiro's "parry is the best stance tool", MH/Horizon's breakable parts and physical types, Valheim's group reward for a parry.
2. Every attack, ours and theirs, is a data row with a real clip, a visible coil and a tell that reads at 15 m.
3. Damage only on visible contact; enemy projectiles never home; packs attack freely; players die.
4. One verb per button. A guard pressed early still blocks.
5. Weapon classes each get a job (speed, reach, stance, shell-breaking), so the second weapon you craft is a real choice.

---

## 2. Design

### 2.0 Scope map (every FULL-GAME 5.x row)

| FULL-GAME row | Tag | Where in this file |
|---|---|---|
| Stamina for attacks, dodges, blocks | SPEC (BUILT) → numbers GAP-COOKED (CAL 1.6) | 2.3 |
| Light/heavy, 3-hit light chain | SPEC (BUILT, sword) | 2.4 |
| Dodge roll with i-frames | SPEC (BUILT) → load tiers GAP-COOKED | 2.6 |
| Block, parry, riposte | SPEC (BUILT) → GB, chip, guard counter, parry tools GAP-COOKED | 2.4.4, 2.8, 2.9 |
| Lock-on, optional | SPEC (BUILT) → flick, lock points, framing GAP-COOKED | 2.15 |
| Poise and stagger | SPEC (BUILT) → poise/stance split GAP-COOKED | 2.5, 2.7, 2.10 |
| Ranged weapons with over-shoulder aim | SPEC (BUILT) | 2.4.5 |
| Mounted combat | SPEC (RAVEN-PLANNED) → details GAP-COOKED | 2.19 |
| Equip load affects dodge | SPEC (RAVEN-PLANNED, M1) → numbers GAP-COOKED | 2.6 |
| Healing is crafted potions, belt, short cooldown | SPEC (BUILT) → drink commit GAP-COOKED | 2.16 |
| Weapons separate from paths; path infuses | SPEC (BUILT) | 2.4.0 |
| Elemental weakness 1.5× | SPEC (BUILT) | 2.11 |
| Burn, frostbite, slowed, frozen, shatter, stagger | SPEC (BUILT) | 2.11 |
| Bleed, poison, rot + path statuses | SPEC (RAVEN-PLANNED names) → all numbers GAP-COOKED | 2.11 |
| Groups that flank, surround, retreat | SPEC (RAVEN-PLANNED, R2) → behaviour GAP-COOKED | 2.18.4 |
| No attacker limit | GAP-COOKED (Aaron rule, kept) | 2.18.4 |
| Aggressive / neutral / passive | SPEC (BUILT) | 2.18 |
| Creature special moves with wind-ups | SPEC (BUILT) → full rows GAP-COOKED | 2.18 |
| 5.2 every row (full attack set, buffer, guard, parry, stance, hyperarmor, stamina, load, Wildbrands, spells, drink, classes, status rules, attack rows, tells, boss phases, camera, PvP, hit feel, pad layout, not copied) | GAP-COOKED | 2.1 to 2.22 |
| 5.3 boss rules | SPEC (BUILT / RAVEN-PLANNED) + stake and first-kill drop GAP-COOKED | 2.17 |

---

### 2.1 Controls (GAP-COOKED; base CAL L4)

One verb per button. Holds on the D-pad are ER's own pattern (hold to reset a slot), so they count as a separate verb on a separate press length, never a chord.

| Pad | KB + mouse | Action |
|---|---|---|
| RB | LMB | Light attack (contextual: running, rolling, backstep, jump, crouch, chain) |
| RT | RMB | Heavy. Hold ≥ 0.35 s = charge. Inside the guard-counter window = guard counter |
| LB | Mouse 4 / Left Alt | Press = parry attempt + guard up; hold = guard (2.8). With a ranged weapon out: hold = aim |
| LT | Left Shift | Sprint (hold) |
| A | Space | Jump |
| B | Left Ctrl | Roll in stick direction; stick at rest = backstep |
| X | E | Use / interact / mount. Hold 0.5 s on a mount = dismount |
| Y | R | Drink the selected belt item |
| D-pad up | 1-8 direct, or F | Fire the selected quick slot (Wildbrand or spell). Hold = charge where the slot allows |
| D-pad left / right | Mouse wheel (with Alt) / Q, Z wheels (BUILT) | Cycle the 8 quick slots |
| D-pad down | V | Tap = cycle belt. Hold 0.5 s = toggle two-handing |
| D-pad left hold 0.5 s | G (BUILT) | Swap melee ↔ ranged |
| L3 | C | Crouch / sneak toggle |
| R3 | Middle mouse / Tab | Lock on / off; right-stick flick or mouse flick switches |
| View (Back) | F8 | Bug report |
| Menu (Start) | Esc | Pause menu |
| — | F1 | Controls page for the device in use (pad: Menu → Controls) |

Rules: prompts show only for the last device used; every combat input is rebindable in Raven's key-binding screen (`WRKeyBindings`); invert-Y off by default.

---

### 2.2 Movement in a fight (GAP-COOKED)

| Item | Value |
|---|---|
| Walk / run / sprint | 450 / 600 / 750 cm/s (SPEC walk and sprint, `tuning.player`; run from FULL-GAME 9) |
| Locked-on strafe | 0.85× run; backpedal 0.75×. Sprinting while locked turns the body to run (ER) and keeps the camera lock |
| Turn while attacking | Free turn during the first 60% of windup (max 540°/s), then committed. Motion-warp step-in toward the lock target or the nearest enemy in a 60° cone within 3 m, clamped 0.45 to 1.3 m (CAL 1.17) |
| Moving while attacking | Lights: legs keep walking at 0.35× during windup and recovery (waist split, CAL C2). Heavies and great weapons: rooted |
| Jump | 1.2 m, costs 8 stamina, leg invuln (2.6) |
| Fall damage | Under 6 m none, lethal over 18 m (FULL-GAME 9). Featherfall exempt (SPEC, ST Wind) |

---

### 2.3 Stamina (GAP-COOKED; base CAL 1.6)

| Item | Value | Note |
|---|---|---|
| Max | 100 → 150 at level 50 | SPEC (SN, DR #35) |
| Regen | 45/s after 0.5 s | CAL (replaces SN 30/s after 0.8 s; SN kept in tuning history) |
| Paused while | Guard held, sprinting, any attack/Wildbrand/spell from press until its CancelWindow opens, drinking | ER: no regen mid-swing |
| Hunger or thirst empty | Regen ×0.5 | SPEC (SN) |
| Roll / backstep / jump / sprint | 12 / 8 / 8 / 5 per s | CAL (roll at every load) |
| Last-swing grace | An action starts if stamina > 0; it may take stamina to −20. While negative, no new action starts and regen's 0.5 s delay restarts from the moment it hits bottom | CAL |
| Guard | Holding costs 0; each blocked hit costs `staminaDamage × (1 − GB/100)` | CAL 1.2 |
| Attack costs | Per class (2.4 table) | CAL extended to every class |
| Stamina bar | Flashes gold-free white when an action is refused for stamina; shakes 3 px | No dead button: the refusal is shown |

---

### 2.4 Weapons and movesets

#### 2.4.0 Rules that hold for every class
- **SPEC:** damage = base × metal (1.0/1.3/1.7/2.2/2.8) × quality (0.9/1.0/1.1/1.2) × (1 + 5% per upgrade); weapons are separate from paths; the path material set at crafting fixes the element; a broken weapon hits at half (BUILT, `WRCombatLibrary::WeaponDamage`).
- **Infusion (GAP-COOKED, CAL 1.11):** an infused weapon's hits carry its element and status buildup: light 8, heavy 14, charged 22, Wildbrand by its row. Path nodes (Ignite chance, Frostbite) add as Raven built.
- **Physical type (GAP-COOKED):** every hit is slash, strike or pierce. Creatures carry `physResist` (default 1.0 each). Shelled and armoured bodies: strike 1.2, slash 0.8, pierce 1.0. Hide and fur: slash 1.15, strike 0.9. Swarms and slimes: slash 1.0, strike 1.1, pierce 0.7. This is how "blunt hits break the hull" (Old Barnacle, Glass Empress, Obsidian Warden) works.
- **Counter-hit (CAL):** a hit that lands on a target in `State.Attacking.Windup` does +15% damage and +30% stance; pierce does +30% damage.
- **Stealth (CAL):** a target that hasn't noticed you takes +20% stance and status buildup. A crouch light on an unaware target of human size or smaller from behind = backstab.
- **Chain heavy (GAP-COOKED, Conan lesson):** RT pressed inside a light's combo window plays the class's chain heavy: heavy damage ×1.1 and stance ×1.2 at the heavy's cost, from the current chain position.
- **Two-handing (GAP-COOKED, CAL):** D-pad down hold. +15% damage, stance +30% on lights and +10% on heavies, the shield goes on the back, guard uses the weapon's GB. Great weapons are always two-handed.
- **Hyperarmor (CAL 1.5):** listed per attack (HA). Adds poise ×1.5 heavy, ×2 charged, ×2.5 great weapons; damage taken −7.5% (great weapons −17.5%).
- **Windows:** ComboWindow = the last 40% of recovery. CancelWindow (dodge, guard, item) opens at 50% of recovery on lights, 65% on heavies and charged, 75% on great weapons. Buffer 0.33 s, last input wins (CAL 1.16).
- **Minimum coil:** windup ≥ 0.14 s on lights, ≥ 0.20 s on heavies, never speed-scaled past ×2.2 by nodes (CAL L2).

#### 2.4.1 Class table (GAP-COOKED; sword numbers SPEC)

| Class | Hands | Weight kg | Reach cm | Light / heavy dmg | Types (light / heavy) | Chain | Stamina L / H / charged | Stance L | Poise dmg vs players (L) | Stamina dmg on guard (L) | Crit backstab / riposte | Job |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Fists | — | 0 | 120 | 5 / 9 | strike / strike | 4 | 6 / 12 / 18 | 2 | 12 | 10 | 2.50 / 3.50 | Bare-hand start; never useless |
| Dagger | 1H | 1.5 | 170 | 8 / 15 | slash / pierce | 5 | 7 / 14 / 21 | 3 | 18 | 18 | 2.94 / 4.20 | Fastest; crit and bleed builder |
| Sword | 1H | 3.0 | 220 | 12 / 22 (SPEC) | slash / pierce | 3 (SPEC) | 10 / 20 / 30 | 5 | 30 | 30 | 2.30 / 3.45 | All-rounder (BUILT) |
| Axe (hatchet line) | 1H | 3.5 | 210 | 13 / 25 | slash / slash | 3 | 12 / 22 / 33 | 6 | 34 | 34 | 2.30 / 3.30 | Hide-shredder; best vs fur |
| Pick-hammer (pick line) | 1H | 4.0 | 200 | 12 / 24 | strike / pierce | 3 | 12 / 22 / 33 | 7 | 38 | 36 | 2.20 / 3.00 | Shell-breaker; head daze |
| Spear | 1H (2H optional) | 3.5 | 300 | 11 / 20 | pierce / pierce | 3 | 10 / 20 / 30 | 5 | 30 | 30 | 2.30 / 3.20 | Reach; pokes while guarding; mounted charge |
| Greatsword | 2H | 9.0 | 270 | 18 / 32 | slash / strike | 3 | 18 / 28 / 40 | 9 | 55 | 50 | 2.20 / 2.75 | Wide arcs; trades with hyperarmor |
| Great hammer | 2H | 12.0 | 250 | 20 / 36 | strike / strike | 2 | 20 / 30 / 42 | 12 | 70 | 62 | 2.20 / 2.50 | Stance and shell king |
| Scythe | 2H | 7.0 | 290 | 15 / 27 | slash / slash | 3 | 16 / 26 / 38 | 7 | 45 | 44 | 2.30 / 3.20 | Crowd sweeps; bleed and Necro fits |

Stance ratios from the class's light stance (CAL sword: L 5, H 10, charged 30): heavy ×2, charged ×6, running light ×1.2, running heavy ×2.8, rolling and backstep light ×1.0 and ×0.8, jump light ×1.6, jump heavy ×4, crouch light ×0.8, guard counter ×6. Poise damage vs players: heavy ×1.6, charged ×2.2, jump heavy ×2.0, guard counter ×1.8. Stamina damage on guard: heavy ×1.5, charged ×2.0.

Tools: the hatchet and pick use the axe and pick-hammer movesets with their own damage (7 / 9 light, heavy ×1.6, SPEC `items.json`). Crafting: daggers, greatswords, great hammers, scythes and spears join the forge at the tier their metal allows (copper at Hearth 1). The craft rows belong to the crafting cook; the weapon rows are here.

#### 2.4.2 Attack timings per class (windup / active / recovery, s; HA = hyperarmor)

Every row shares: ComboWindow last 40% of recovery; CancelWindow as 2.4.0. Charged heavies: hold from 0.35 s, full at 0.80 s, auto-release at 1.0 s; damage scales linearly from 1.0× at 0.35 s to the charged value at 0.80 s; the "windup" listed is the time from release to active.

**Sword (BUILT light 1 and heavy keep Raven's tuning)**

| Attack | Windup | Active | Recovery | Dmg × | HA |
|---|---|---|---|---|---|
| Light 1 (SPEC) | 0.25 | 0.18 | 0.30 | 1.0 | — |
| Light 2 | 0.22 | 0.16 | 0.30 | 1.0 | — |
| Light 3 | 0.30 | 0.20 | 0.42 | 1.15 | — |
| Heavy (SPEC) | 0.55 | 0.22 | 0.45 | 1.0 | windup→active |
| Chain heavy | 0.50 | 0.22 | 0.48 | 1.1 | windup→active |
| Charged heavy | 0.14 after release | 0.24 | 0.55 | 1.6 | hold→active |
| Running light / heavy | 0.20 / 0.45 | 0.18 / 0.24 | 0.40 / 0.55 | 1.1 / 1.25 | — / yes |
| Rolling light | 0.18 | 0.16 | 0.35 | 1.0 | — |
| Backstep light | 0.20 | 0.16 | 0.35 | 0.9 | — |
| Jump light / heavy | 0.20 / 0.30 (air) | until landing + 0.10 | 0.35 / 0.50 (on landing) | 1.1 / 1.5 | — / yes |
| Crouch light | 0.16 | 0.14 | 0.30 | 0.9 | — |
| Guard counter | 0.30 | 0.20 | 0.50 | 1.3 | — (CAL: no HA) |

**Dagger** — Lights 1-5: 0.16/0.12/0.22, 0.15/0.12/0.22, 0.15/0.12/0.22, 0.16/0.12/0.24, 0.20/0.14/0.34 (×1.15). Heavy 0.40/0.16/0.38 (pierce stab). Chain heavy 0.36/0.16/0.40. Charged 0.14/0.18/0.45. Running L/H 0.16/0.14/0.30, 0.35/0.18/0.45. Rolling 0.15/0.12/0.28. Backstep 0.16/0.12/0.28. Jump L/H 0.18/0.26. Crouch 0.14/0.12/0.24. Guard counter 0.26/0.16/0.40. No HA anywhere.

**Axe** — Lights 1-3: 0.30/0.18/0.34, 0.28/0.18/0.34, 0.36/0.20/0.46 (×1.15, overhead). Heavy 0.62/0.24/0.50 HA. Chain heavy 0.56/0.24/0.52 HA. Charged 0.16/0.26/0.60 HA. Running 0.24/0.18/0.42, 0.50/0.24/0.58 HA. Rolling 0.20/0.16/0.38. Backstep 0.22/0.16/0.38. Jump 0.22 / 0.32 HA. Crouch 0.18/0.14/0.32. Guard counter 0.32/0.20/0.52.

**Pick-hammer** — Lights 1-3: 0.32/0.18/0.36, 0.30/0.18/0.36, 0.38/0.20/0.48 (×1.15). Heavy 0.66/0.24/0.52 HA (pick end: pierce, +25% vs shells' gaps). Chain heavy 0.60/0.24/0.54 HA. Charged 0.18/0.26/0.62 HA. Running 0.26/0.18/0.44, 0.52/0.24/0.60 HA. Rolling 0.22/0.16/0.40. Backstep 0.24/0.16/0.40. Jump 0.24 / 0.34 HA. Crouch 0.20/0.14/0.34. Guard counter 0.34/0.20/0.54. Strike hits on a `head` lock point build ×1.5 stance (MH hammer lesson).

**Spear** — Lights 1-3 (thrusts): 0.24/0.16/0.30, 0.22/0.16/0.30, 0.28/0.18/0.40 (×1.15, longer lunge, reach +40). Heavy 0.55/0.24/0.48 (hop thrust, HA). Chain heavy 0.50/0.24/0.50 HA. Charged 0.14/0.26/0.55 HA. Running 0.20/0.18/0.40, 0.45/0.24/0.55 HA. Rolling 0.18/0.16/0.34. Backstep 0.20/0.16/0.34. Jump 0.20 / 0.30 HA. Crouch 0.16/0.14/0.28. **Guard poke:** RB while guarding with a shield: 0.28/0.14/0.36, ×0.85, guard stays up through it (ER spear+shield); stamina 12. Guard counter 0.30/0.20/0.50.

**Greatsword** — Lights 1-3: 0.42/0.22/0.52, 0.40/0.22/0.52 HA, 0.48/0.24/0.62 HA (×1.15). Heavy 0.80/0.26/0.70 HA. Chain heavy 0.74/0.26/0.72 HA. Charged 0.20/0.28/0.80 HA. Running 0.34/0.22/0.58 HA, 0.70/0.26/0.75 HA. Rolling 0.30/0.20/0.52 HA. Backstep 0.32/0.20/0.52. Jump 0.30 / 0.40 HA. Crouch 0.28/0.18/0.46. Guard counter 0.42/0.24/0.66.

**Great hammer** — Lights 1-2: 0.50/0.24/0.60, 0.56/0.26/0.70 HA (×1.15). Heavy 0.95/0.28/0.80 HA (ground slam; 120 cm shockwave ring hits low). Chain heavy 0.88/0.28/0.82 HA. Charged 0.22/0.30/0.90 HA. Running 0.40/0.24/0.66 HA, 0.80/0.28/0.85 HA. Rolling 0.36/0.22/0.58 HA. Backstep 0.38/0.22/0.58. Jump 0.34 / 0.44 HA. Crouch 0.32/0.20/0.52. Guard counter 0.48/0.26/0.72.

**Scythe** — Lights 1-3: 0.38/0.22/0.46 (200° sweep), 0.36/0.22/0.46, 0.44/0.24/0.56 HA (×1.15, pull-back hook: reach 290). Heavy 0.75/0.26/0.65 HA (full spin 360°). Chain heavy 0.70/0.26/0.68 HA. Charged 0.20/0.28/0.75 HA. Running 0.30/0.22/0.52, 0.65/0.26/0.70 HA. Rolling 0.28/0.20/0.48. Backstep 0.30/0.20/0.48. Jump 0.28 / 0.38 HA. Crouch 0.26/0.18/0.42. Guard counter 0.40/0.24/0.60.

**Fists** — Lights 1-4: 0.15/0.10/0.20 ×3, 0.18/0.12/0.30 (×1.15). Heavy 0.40/0.14/0.36. Chain heavy 0.36/0.14/0.38. Charged 0.14/0.16/0.42. Running 0.16/0.12/0.30, 0.34/0.16/0.42. Rolling and backstep 0.15/0.10/0.26. Jump 0.18 / 0.26. Crouch 0.14/0.10/0.22. Guard counter 0.26/0.14/0.38.

#### 2.4.3 Trace shapes (GAP-COOKED)
Hits come from the weapon socket trace (`UWRWeaponTraceNotifyState`, BUILT) along the clip's real blade path. Trace radius per class: dagger 18 cm, sword 24, axe 26 (head only), pick-hammer 26 (head only), spear 16 (tip 60 cm only), greatsword 30, great hammer 40 (head only), scythe 22 (blade 90 cm only), fists 14. No hit outside the blade path. No hit before the first active frame or after the last.

#### 2.4.4 Shields and off-hands (GAP-COOKED; GB numbers CAL)

| Off-hand | Weight | Guard Boost | Physical negation | Element negation | Parry startup / active / recovery | Notes |
|---|---|---|---|---|---|---|
| None (1H weapon guards) | — | Sword/axe/pick/spear 25, dagger 15, fists 10 | 60% | 30% | 0.13 / 0.10 / 0.50 (dagger 0.13 / 0.13 / 0.45) | Parries with the weapon |
| Crude shield (hide over sticks) | 2.0 | 45 | 85% | 40% | 0.13 / 0.17 / 0.45 | Small shield |
| Buckler (metal tiers) | 1.5 | 35 + 1/upgrade | 75% | 45% | 0.13 / 0.17 / 0.45 | Best parry |
| Wooden shield | 3.5 | 55 | 100% | 55% | 0.13 / 0.10 / 0.50 | Medium |
| Metal shield (copper→starsteel) | 5.0 | 65 + 2/upgrade | 100% | 60% (biome set element 80%) | 0.13 / 0.10 / 0.50 | Medium |
| Greatshield | 12.0 | 80 | 100% | 70% | cannot parry | Guard poke allowed with spears; block stun ×0.7 |
| Torch | 0.8 | 15 | 40% | 20% | 0.13 / 0.10 / 0.50 | Light radius 12 m; a guard-counter or parry riposte with torch adds 20 burn buildup; light lets darkness biomes be fought (Graveyard, caves) |

Biome shields made from a signature resource (crafting cook) take that biome's element negation to 80%.

#### 2.4.5 Ranged (SPEC, BUILT)
Bow, crossbow, blowgun, throwing spear, bola keep Raven's `items.json` and `tuning.ranged` numbers. Additions (GAP-COOKED): arrows and bolts carry pierce; each arrow does stance = damage × 0.3 (crossbow ×0.5); a ranged hit on a flyer's wing lock point counts toward "bring it down" (Scorchwing, Skaldrith wingHitsToGround SPEC); with a ranged weapon out, LB hold aims (no guard), RB fires.

---

### 2.5 Hit reactions on players (GAP-COOKED)

Every attack row carries `impact` 1 to 5. If the hit breaks the player's poise (poise damage ≥ current poise), the reaction plays; if not, the body shudders (additive 0.12 s) and the action goes on.

| Impact | Reaction | Lock time | Notes |
|---|---|---|---|
| 1 | Flinch | 0.25 s | Lights from small creatures, player daggers |
| 2 | Stagger | 0.55 s | Most bites, player swords |
| 3 | Heavy stagger | 0.85 s | Heavies, charges, great weapons |
| 4 | Knockdown | Fall 0.6 s, down up to 1.2 s | Press B after 0.5 s down = wake-roll with 0.40 s i-frames; else auto get-up at 1.2 s with 0.35 s i-frames on rising |
| 5 | Launch | Airborne by the hit, then knockdown | Boss slams, Thunderhorn butt, charges at full speed |

Grabs ignore poise and hyperarmor. Being hit in a reaction resets nothing; there is no stagger-lock protection (ER), but every pack creature has a real punish window after its combo (2.18), so lock-chains end.

---

### 2.6 Roll, backstep, jump (GAP-COOKED; base CAL 1.7)

| Load (worn + held kg ÷ max equip load) | Roll distance | I-frames | Recovery | Cost |
|---|---|---|---|---|
| Light < 30% | 500 cm | 0.03 to 0.46 s | 0.27 s | 12 |
| Medium < 70% | 420 cm | 0.03 to 0.46 s | 0.27 s | 12 |
| Heavy < 100% | 330 cm | 0.03 to 0.43 s | 0.53 s | 12 |
| Overloaded | Slow step 120 cm, 0 i-frames, 0.6 s | — | — | 12 |

- **Max equip load** 60 kg + armour and node bonuses; separate from the 80 kg bag (SPEC bag, over-encumbered blocks dodge, BUILT).
- Roll total length = i-frame end + recovery (light 0.73 s). Rolling attack window: 0.30 s after the roll ends. Light step (Frost node) +5% roll speed per rank (SPEC `nodeEffects`).
- **Backstep:** stick at rest + B. 250 cm, 0.10 s i-frames (0.03 to 0.13), recovery 0.22 s, cost 8. Backstep attack window 0.30 s.
- **Jump:** `State.JumpLegsInvuln` for 0.8 s from takeoff on leg and lower-torso hit zones; attacks flagged `low` miss.
- **Perfect dodge (for Frozen moment, Afterimage, Heat shimmer, Fade):** an enemy hit is absorbed by roll i-frames in the roll's first 0.20 s. Event `Event.PerfectDodge` fires on the server; nodes listen to it.
- **Dodge fires on press** (CAL). Roll direction: 8-way relative to camera; locked-on rolls are relative to the lock target.
- Roll into ledge: rolls stop at ledges over 2 m drop unless the stick holds toward it for the whole roll (no accidental death rolls; you can still roll off deliberately).

---

### 2.7 Poise and hyperarmor on players (GAP-COOKED; base CAL 1.5)
- Poise 0 (no armour) to 50 (heaviest iron set) from armour pieces (`items.json` `poise`): hide 8, copper 18, bronze 24, iron 32, steel 40, starsteel 46, biome sets 30 to 50. Onyx gem +10 (SPEC, BUILT). Tempered +5 per rank (SPEC).
- Poise damage accumulates; full reset 30 s after the last poise hit or at once after a stagger (ER).
- Hyperarmor multiplies current poise during the HA window (2.4.0).
- What breaks through anything: grabs, Frozen, Shatter, a status proc (CAL), attacks flagged `ignoreHyperarmor`.
- Creature poise damage to players by attack size: small bite 20, big bite 35, charge 50, boss light 60, boss heavy 100, boss slam 150 (CAL).

---

### 2.8 Guard and guard counter (GAP-COOKED; base CAL 1.2)

| Item | Value |
|---|---|
| Input | LB press raises guard at once (block works from frame 0) and opens the parry window of the off-hand (2.4.4) |
| Block angle | 70° half-angle in front (SPEC `defense.blockAngleDegrees`) |
| Stamina per blocked hit | Attack `staminaDamage` × (1 − GB/100). Creature default `staminaDamage` = hit damage × 1.2; boss slams × 1.6; player weapons by class (2.4.1) |
| Chip HP | Damage × (1 − negation) for the hit's type |
| Block stun | 0.20 s (impact ≤ 2), 0.35 s (impact ≥ 3); greatshield ×0.7 |
| Guard break | Stamina would drop below 0: guard breaks, 1.2 s stagger (SPEC `guardBreakStaggerSeconds`). Creatures get their `onGuardBreak` follow-up; players can riposte a guard-broken player within 1.0 s (PvP) |
| Regen | Paused while guard is held (CAL) |
| Guard counter | RT inside 0.6 s from the end of block stun. Row per class (2.4.2). Never on guard release (CAL) |
| Unblockable | Attacks flagged `unblockable` (crimson glint on the weapon) pass the guard fully and break it |
| Visible guard | Faint gold edge on the guarding item (CAL) |

**Parry and tap/hold (GAP-COOKED).** Pressing LB always starts the parry window, then guard holds as long as LB is held. Releasing LB before 0.30 s = a "tap": the parry recovery (2.4.4) plays and you can't guard, roll or attack until it ends. Holding past 0.30 s = plain guard with no recovery penalty. So a tap is a committed parry attempt; a hold is a safe block whose press could also parry. Spam rule: a press within 0.5 s of the last press shrinks the active window by 30% (CAL).

---

### 2.9 Parry, riposte, backstab, crits (GAP-COOKED; base CAL 1.3)

| Item | Value |
|---|---|
| What can be parried | Creature and boss rows flagged `parryable`; every player melee attack except great hammers' and charged heavies; no projectiles (Arcane Reflect and Aegis do projectiles) |
| On a parry | Attacker plays its parried clip (1.6 s for creatures and players, open to riposte); hitstop 0.09 s (SPEC); gold flash 0.18 s (SPEC `fx.hud.parryFlashSeconds`) |
| Parries to break | Normal creature 1, mini-boss 2, major boss 3 (a non-breaking parry interrupts the combo and deals 25% of max stance) |
| Group reward (Valheim) | A parried creature takes ×1.5 damage from everyone for 2.0 s |
| Riposte | Paired animation: 1.6 s total, damage at 0.9 s. Attacker i-frames from start to the damage frame; 0.45 s exit is open. Multiplier by class (2.4.1, sword 3.45) |
| Backstab | 2.3× (sword); 90° rear arc, within 1.2 m, target radius ≤ 70 cm and halfHeight ≤ 110 cm, not in an attack windup. Big creatures and bosses: only on a stance break. Paired 1.4 s, damage at 0.75 s, attacker i-frames to the damage frame |
| Stance-break crit | Light attack within 1.0 m of the gold weak-point mark (1.5 m on targets with radius > 150 cm), from any side the mark faces. Damage = riposte multiplier × weapon light hit. Hitstop 0.20 s |
| Knockdown crit | A creature tripped by a leg-part break or a stance break while knocked down: same as stance crit, mark on the head |
| Rule | Phase transitions never start during a crit (CAL). A blow that would take a creature to 0 kills it; nothing floors HP (CAL 1-HP rule) |
| Motion warping | Both actors warp to a sync point 1.1 m apart over 0.12 s (CAL 1.17) |
| Server judge | Parries, i-frames and crits judged at the attacker's rewound time (≤ 250 ms ring buffer, M1) |

Node hooks (SPEC behaviour from ST): Searing parry (parry adds 100 burn buildup, `passives.searingParryBuildup`), Cold riposte (riposte freezes), Gleam (parry blinds), Discharge (blocking releases a shock), Brittle (frozen take more crits, `passives.brittleCrit`).

---

### 2.10 Stance on creatures and bosses (GAP-COOKED; base CAL 1.4)

| Target | Stance | Break stun | Regen |
|---|---|---|---|
| Lowland | 20 | 2.5 s | After stance/13 s, then 13/s (CAL) |
| Midland | 35 | 2.5 s | same |
| Highland | 55 | 3.0 s | same |
| Mini-boss | 90 | 4.0 s | same |
| Major boss | 140 (Skaldrith 160, Vulkarn 180) | 5.0 s | same |

- **Sekiro rule (new):** below 50% HP, stance regen ×0.5.
- **Group scaling (CAL):** boss stance damage taken ×1.0 solo, ×0.6 with 2 participants, ×0.45 with 3, ×0.35 with 4+; tames count.
- **Breakable parts (new, MH/Horizon):** a creature row may list `parts`. Each part has its own stance (and optionally HP). Breaking it plays a part-stagger (1.2 s), deals 30% of the body's max stance, drops the listed material once per fight, and may change moves (`onBreak`). Parts used now: Skaldrith wings (SPEC 6 hits grounds him, kept as the part rule), Vulkarn hammer arm, Obsidian Warden shell (SPEC shell rule), Cinderhorn horn, Old Tusker tusks, giant legs (trip = knockdown crit).
- **Head daze:** strike hits on a `head` lock point do ×1.5 stance.
- **Stance hidden** (ER): no bar. The break shows by the stance-break clip, a crack sound and the gold weak-point mark.

---

### 2.11 Status effects (GAP-COOKED; base CAL 1.12; Raven's names SPEC)

**Shared rules.** Meter fills per hit; procs at the threshold; threshold ×1.3 after each proc (resets 30 s after combat ends); decay 8/s after 3 s (SPEC `status`). Thresholds: lowland 100, midland 120, highland 150, mini 250, major 400; ×1.25 per extra boss participant (CAL). Players: 100 + armour `resist` (each biome set +50 to its own status) + root passive immunities (SPEC). Stealth +20% buildup. PvP buildup ×0.7. A proc breaks hyperarmor (CAL). Every status can kill (no floors).

| Status | Path / source | On creatures | On players | Cure | Round | Tag |
|---|---|---|---|---|---|---|
| Burn | Fire | 4/s for 6 s (SPEC) + Spreading flames etc. (SPEC nodes) | Same | Water (wade or swim), Ember salve | Built | SPEC |
| Frostbite | Frost | Burst 8% max HP + 20 (bosses 4% + 20); +20% damage taken; Slowed ×0.6 (SPEC slow); 12 s; no re-proc while active | Same + stamina regen ×0.6 | Fire damage ends it (and triggers Thaw if Frozen); Warming draught | Built | Effect GAP-COOKED on Raven's chain |
| Frozen | Frost | Buildup that fills while Frostbitten procs Frozen 2.5 s (SPEC): can't act; ends Frostbite. Mini and major bosses can't be Frozen: they get Rimed instead (stance damage taken ×1.5 for 4 s) | 2.5 s, mash any button −0.15 s per press (min 1.2 s) | Fire hit breaks it (Thaw burst if combo owned) | Built | SPEC + GAP-COOKED (Rimed) |
| Shatter | Frost | A heavy, charged or Wildbrand hit on a Frozen target: 30 burst (SPEC) + stance ×2; ends Frozen | Same | — | Built | SPEC |
| Bleed | Blood; Nature Barbs; Tide Barbed tip; daggers, scythes, axes by gem or node | 12% max HP + 15 burst; bosses ×0.7 | Same | Bloodmoss poultice (swamp) | R2 | CAL |
| Poison | Spore | 0.4% max HP + 2 per s for 30 s | Same | Antidote (SPEC item) | R8 | CAL |
| Rot | Spore | 0.8% max HP + 4 per s for 20 s; healing taken ×0.5 | Same | Antidote, Purify (SPEC node) | R8 | CAL |
| Shock | Lightning | 0.4 s stagger (ignores HA on creatures; on bosses in HA: +15 stance instead) + chain 25% of the hit to 2 targets within 4 m | 0.4 s stagger, −20 stamina; Grounded immune (SPEC) | Grounding tonic | R1 | CAL |
| Soak | Tide | Slowed ×0.8 for 10 s; fire buildup −50%; shock buildup +50%; Conductive +25% lightning damage (SPEC node) | Same; Drowning node drains 6 stamina/s (SPEC node behaviour) | Stand by fire 3 s | R6 | CAL |
| Blind | Sun & sand | AI loses target memory 3 s (bosses 1.5 s), attacks the last-seen spot; any backstab works on a blinded target | Screen vignette to 40% edge dim, lock-on off for 3 s | Clear-eye drops | R5 | CAL |
| Decay | Necromancy | Max HP −10% until fight ends; healing ×0.5 for 20 s | Max HP −10% until rest; healing ×0.5 for 20 s | Rest (bed or bedroll) | R4 | CAL |
| Mark | Arcane | +15% damage from all sources for 10 s; visible through walls to the marker's party | Same (PvP only via Mark nodes) | — | R9 | CAL |
| Curse | Blood & curse | −25% of its path resource (creatures: −15% damage dealt) for 15 s | −25% path resource, −15% damage dealt for 15 s | Hex-ward charm (swamp) | R2 | CAL |
| Slowed | Frost, Tide | ×0.6 (SPEC) | Same | — | Built | SPEC |
| Stagger | Any | Plays stagger clip (SPEC `staggerSeconds` 1.5 for full stagger) | 2.5 impact table | — | Built | SPEC |
| Knockback | Wind (hit property) | Pushes 2 cm per poise damage point; slams a wall = +20 stance | Same | — | R7 | CAL |
| Corruption | Void (self cost) | — | Max HP cost per use until rest (SPEC rule) | Rest | R10 | SPEC |

**Reactions table (elements meeting on one target, GAP-COOKED, FULL-GAME 4.2's GAP).** Fire on Frozen = Thaw (SPEC combo, 40 dmg `status.thawDamage`) for owners; for non-owners of the pair, the reaction fires at 50% strength. Lightning on Soaked = +25% damage and a free shock proc chance 30%. Fire on Soaked: buildup −50%. Wind on spore clouds: pushes the cloud 6 m in the gust direction. Frost on Soaked: frost buildup +50%. Any element reaction needs the two elements from two different hits within 4 s.

**Status UI (GAP-COOKED).** Enemy: a thin meter under the health bar per active buildup, in the status colour (burn orange, frost pale blue, bleed dark red #7A0A14 (not crimson), poison green, rot rust, shock violet-white, soak teal, blind cold white, decay grey-green, mark arcane violet, curse black-purple). Player: the same meters above the stamina bar.

---

### 2.12 Wildbrands (GAP-COOKED; nodes and effects SPEC from ST)

**Rules.**
- A Wildbrand is a weapon-art node slotted in a quick slot, fired through the held weapon from D-pad up (1-8 on keyboard).
- **Resource by path (SPEC behaviour, ST/DD; mode GAP-COOKED):** Heat, Chill and Charge are *builders*: Wildbrands **add** to them (Raven's tuning `heat`/`chill` values are gains) and are gated by cooldown. Radiance, Growth, Momentum, Mana, Flow, Souls and Spores are *spenders*: Wildbrands **cost** them. Blood costs HP; Void costs max HP until rest.
- Stamina 18 (SPEC `stamina.weaponArt`) unless the row says otherwise.
- **Fits:** a Wildbrand that doesn't fit the held class greys out in its slot with the reason ("needs a spear").
- Charge: hold D-pad up (max 1.0 s). Follow-up: press again inside the listed window.
- Windup ≥ 0.20 s (heavy coil rule). Every Wildbrand has hyperarmor or i-frames from data.
- Subclass Wildbrands use the subclass element while they play (CAL 1.11).

| Wildbrand | Path | Fits | Resource | Cooldown | Windup / active / recovery | Dmg × | Buildup | Stance × light | Defence | Charge / follow-up | Effect | Tag |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Flame slash | Fire | All melee | +12 Heat | 3.0 | 0.32 / 0.22 / 0.45 | 1.4 | Burn 45 | ×2 | HA | — | Flaming wide slash, 300 cm | Numbers SPEC; frames GAP-COOKED |
| Molten edge | Fire | Sword, axe, dagger, greatsword, scythe | +8 Heat | 2.0 | 0.24 / 0.18 / 0.36 | 1.1 | Burn 55 | ×1.5 | — | Follow-up within 0.5 s: a second edge ×1.1 | Fast molten double cut | SPEC numbers |
| Molten cleave | Fire | Axe, greatsword, great hammer, pick-hammer | +20 Heat | 12 | 0.60 / 0.28 / 0.70 | 2.0 | Burn 80 | ×6 | HA | Charge to 1.0 s: ×2.6, lava radius ×1.3 | Overhead cleave, lava pool 220 cm, 6 s, 10 dps (SPEC) | SPEC numbers |
| Ice slash | Frost | All melee | +8 Chill | 3.0 | 0.30 / 0.22 / 0.44 | 1.3 | Frost 50 | ×2 | HA | — | Rime arc | SPEC |
| Flurry | Frost | Dagger, sword, spear, fists | +6 Chill | 1.5 | 0.20 / 0.40 (4 cuts) / 0.36 | 0.9 per cut | Frost 35 total | ×0.5 per cut | — | Follow-up within 0.4 s: second flurry ×0.9 | Fast frost cuts | SPEC numbers |
| Static strike | Lightning | All melee | +15 Charge | 2.5 | 0.24 / 0.16 / 0.38 | 1.2 | Shock 45 | ×1.5 | — | — | Thrust that arcs to 1 target within 4 m at 40% | GAP-COOKED |
| Charged weapon | Lightning | All melee | Spends all Charge (min 30) | 8 | 0.40 / 0.24 / 0.55 | 1.0 + 1.5 × Charge% | Shock 30 + 50 × Charge% | ×3 | HA | Charge hold stores more (+20 Charge/s from your bar) | Releases stored Charge in one overhead blow | GAP-COOKED |
| Sun strike | Sun & sand | All melee | 15 Radiance | 3.0 | 0.30 / 0.20 / 0.44 | 1.35 | Blind 40 | ×2 | HA | — | Gleaming slash | GAP-COOKED |
| Blazing arc | Sun & sand | Sword, greatsword, scythe, spear | 25 Radiance | 6 | 0.42 / 0.26 / 0.56 | 1.6 | Blind 50 | ×2.5 | HA | Charge: ×2.0, arc range 500 cm | Wide 220° sun arc, range 380 cm | GAP-COOKED |
| Noon strike | Sun & sand | All melee | 60 Radiance | 30 | 0.80 / 0.30 / 0.80 | 2.2 + column 50 | Blind 80 | ×6 | HA | — | Overhead blow; a sun column falls on the target point 0.9 s later (your own ability: a gold sun-shaft glow on the spot, 300 cm) | GAP-COOKED |
| Briar strike | Nature | All melee | 15 Growth | 3.0 | 0.28 / 0.20 / 0.42 | 1.3 | Bleed 30, root 0.6 s on lowland | ×1.5 | — | — | Thorned cut | GAP-COOKED |
| Vine lash | Nature | All melee, fists | 20 Growth | 5 | 0.30 / 0.20 / 0.45 | 1.2 | Bleed 25 | ×1 | — | Follow-up within 0.6 s: pull the target 300 cm toward you (not bosses) | Whip from the weapon, reach 700 cm | GAP-COOKED |
| Wind cut | Wind | All melee | 15 Momentum | 2.5 | 0.24 / 0.18 / 0.36 | 1.1 + wave 0.8 | — | ×1.5 | — | — | Slash sends a wind wave 900 cm at 2000 cm/s (player projectile) with knockback | GAP-COOKED |
| Whirlwind | Wind | Greatsword, scythe, axe, great hammer | 30 Momentum | 8 | 0.30 / 1.2 (4 spins) / 0.50 | 0.7 per spin | — | ×1 per spin | HA; Eye of the storm node −20% damage (SPEC behaviour) | Hold to keep spinning 0.3 s per 8 Momentum | Spin hits all around, 280 cm | GAP-COOKED |
| Plunge | Wind | All melee | 20 Momentum | 4 | Airborne only: 0.20 / until landing / 0.45 | 1.5 + landing ring 0.8 | — | ×4 | HA | Fall height × 0.2 per m extra damage, max ×2 | Falling attack, landing ring 300 cm | GAP-COOKED |
| Wind arrow | Wind | Bow, crossbow | 15 Momentum | 2.0 | Draw 0.35 / release / 0.30 | Arrow ×1.6 | — | ×1.5 | — | — | Arrow that knocks back and pierces 1 target | GAP-COOKED |
| Runic strike | Arcane | All melee | 15 Mana | 2.5 | 0.28 / 0.20 / 0.42 | 1.3 | Mark 40 | ×1.5 | — | — | Arcane cut | GAP-COOKED |
| Inscribe | Arcane | All melee | 30 Mana | 30 | 0.6 rooted buff | — | — | — | — | — | Rune on the weapon: +8 arcane per hit for 30 s | GAP-COOKED |
| Crimson slash | Blood | All melee | 6 HP | 2.5 | 0.28 / 0.20 / 0.42 | 1.4 | Bleed 45 | ×1.5 | — | — | Blood-edged slash | GAP-COOKED |
| Leech strike | Blood | Dagger, sword, scythe, fists | 4 HP | 4 | 0.26 / 0.18 / 0.40 | 1.2 | Bleed 30 | ×1.5 | — | — | Heals 50% of damage dealt | GAP-COOKED |
| Wave cut | Tide | All melee | 15 Flow | 2.5 | 0.28 / 0.20 / 0.42 | 1.25 | Soak 45 | ×1.5 | — | — | Water-edged arc | GAP-COOKED |
| Harpoon throw | Tide | Spear, throwing spear | 25 Flow | 8 | 0.40 throw / flight 2700 cm/s / 0.40 | 1.5 | Soak 30 (+Barbed tip bleed SPEC) | ×2 | — | Follow-up within 1.5 s: reel. Lowland and midland pulled to you; larger: you're pulled to them (Tow); Anchor pins large 3 s; Leviathan's hook staggers bosses (SPEC) | Spear on a line, range 1800 cm | GAP-COOKED (behaviour SPEC) |
| Grave strike | Necromancy | All melee | 1 Soul | 2.5 | 0.30 / 0.20 / 0.44 | 1.3 | Decay 40 | ×1.5 | — | — | Necrotic cut | GAP-COOKED |
| Scythe sweep | Necromancy | Scythe, greatsword, spear | 2 Souls | 6 | 0.40 / 0.30 / 0.55 | 1.5 | Decay 50 | ×2 | HA | — | 300° sweep, 320 cm | GAP-COOKED |
| Infect strike | Spore | All melee | 10 Spores | 2.5 | 0.28 / 0.20 / 0.42 | 1.2 | Rot 30; plants 3 Spores | ×1.5 | — | — | Spore-tipped cut | GAP-COOKED |
| Null strike | Void | All melee | 3 max HP | 3 | 0.30 / 0.20 / 0.44 | 1.5 | — (ignores 30% armour; Dark matter SPEC) | ×1.5 | — | — | Void cut | GAP-COOKED |
| Consume | Void | All melee, fists | 0 (restores max HP) | 15 | 0.35 / paired 1.4 s | Execute | — | — | i-frames during | — | Finisher on a target below 25% HP (Insatiable raises to 35%): kills it, restores 10% max HP lost to void; bosses: only during a stance break, deals riposte × 1.2 instead | GAP-COOKED (behaviour SPEC) |

Spells that move the body (Bolt dash, Blink, Shadow step, Burrow, Air dash) are in 2.13.

---

### 2.13 Spells (GAP-COOKED; nodes SPEC from ST)

**Rules (CAL 1.9).** Path is the focus (no staff); spells sit in the quick slots; light shapes cast walking at ×0.5 speed, heavy shapes root; `chargeable` spells deal up to ×1.5 at 1.0 s hold. Stamina 12 per cast (SPEC `stamina.spell`). Player bolts keep Raven's lock-on lead and homing (SPEC `spells.lockHoming` 6000 / players 2000). Enemy spells never home (hard rule). Built spells keep their tuning blocks exactly (SPEC).

**Shapes (one code path each; `EWRAreaShape` already holds 7 of them, BUILT).**

| Shape | Cast windup | Root | Recovery | Default numbers | Built? |
|---|---|---|---|---|---|
| Bolt | 0.30 | walk ×0.5 | 0.35 | 2600-3000 cm/s, range 4000 | BUILT (`UWRGA_Spell`) |
| Lance | 0.45 | walk ×0.5 | 0.40 | Line 2000 cm, pierces | BUILT |
| Nova | 0.55 | rooted | 0.45 | Radius 500 | BUILT |
| Wall | 0.60 | rooted | 0.45 | 800 wide at 600 out | BUILT (Fire wall, Ice wall) |
| Pool | 0.50 | walk ×0.5 | 0.40 | Radius 300 at range 1500 | BUILT |
| Strike (delayed area) | 0.90 | rooted | 0.50 | Delay 1.2 s then radius 450 | BUILT (Meteor) |
| Storm (around you, lasting) | 0.80 | rooted | 0.50 | Radius 650, 8 s | BUILT (Blizzard) |
| Chain | 0.35 | walk ×0.5 | 0.35 | Hits target then jumps 3 times within 600 cm, ×0.8 per jump | new |
| Self buff | 0.40 | walk ×0.5 | 0.20 | — | new |
| Ally target (heal, shield) | 0.50 | walk ×0.5 | 0.30 | Range 1500, needs line of sight | new |
| Summon / plant | 0.80 | rooted | 0.40 | Placed within 800 | new |
| Pull | 0.50 | rooted | 0.45 | Cone 30°, 1200 cm, pulls 500 cm (not bosses unless the row says) | new |
| Dash / blink | 0.08 | — | 0.25 | Travel 600 cm in 0.18 s, i-frames whole travel | new |
| Transform (capstone forms) | 1.00 | rooted, HA | 0.30 | Lasts per node | new |

**Every spell node, mapped (numbers are first drafts in tuning `spells.<id>`).** Damage is at copper-tier parity (a spell hit ≈ a heavy hit).

| Path | Spell → shape, key numbers |
|---|---|
| Fire (BUILT, SPEC numbers) | Fire bolt Bolt; Fire wall Wall; Lava pool Pool; Meteor Strike; Overheat (passive burst, SPEC) |
| Frost (BUILT, SPEC numbers) | Frost shard Bolt; Frost nova Nova; Ice lance Lance; Ice wall Wall; Blizzard Storm |
| Lightning | Spark Bolt 16 dmg, shock 35, 3200 cm/s, cd 1.0, +8 Charge · Chain lightning Chain 20, shock 30/jump, cd 6, +12 Charge (Arc: +1 jump/rank) · Thunderclap Nova 18, shock 60, stance ×2, radius 450, cd 10 · Bolt dash Dash 12 to everything passed, shock 40, cd 6, 20 Charge spent · Static field Pool radius 400, 6 dps, shock 15/s, 8 s, cd 14 · Tempest Storm radius 900, a strike every 0.8 s on a random enemy (30 dmg, shock 40), 10 s, cd 45 · Lightning step = Bolt dash ×3 within 2.0 s · Thunder god Self buff: every weapon hit drops a bolt (20 dmg) for 12 s, cd 60 |
| Sun & sand | Sunbeam Bolt 16, blind 30, 3400 cm/s, cd 1.0, 8 Radiance · Solar flare Nova radius 600, 10 dmg, blind 80, cd 14, 30 Radiance · Beacon Pool radius 500, 10 dps to undead and void, 10 s, cd 20 · Sandstorm Storm radius 700, blind 10/s, slow ×0.8, 8 s, cd 20 · Burrow Dash under sand only, 800 cm, cd 4 · Quicksand Pool radius 300, roots lowland/midland 2 s, slow ×0.5, cd 16 · Desert's wrath Storm radius 1000, pulls 200 cm/s, 14 dps, 10 s, cd 60 · Sunburst Strike radius 600, 70 dmg, blind 100, delay 1.0, cd 50 |
| Nature | Thorn shot Bolt 15, bleed 20, cd 1.0, 6 Growth · Regrowth Ally target heal 30 over 10 s, cd 8 · Sanctuary Pool radius 450 heals 4/s for 10 s, cd 30 · Root snare Pool radius 250, roots 2.5 s (bosses: slow ×0.7 3 s), cd 14 · Sprout turret Summon, 3 thorns/s × 6 dmg, 60 HP, 20 s, cd 24 · World tree Summon: heals 6/s and shields 20% in 600 cm, 15 s, cd 90 · Strangling vines Self-aimed Pool on a large target: holds 4 s, 12 dps, cd 60 · Ancient guardian Summon 400 HP walker, 25 dmg slams, 30 s, cd 120 |
| Wind | Gust Pull (reversed: push 600 cm, cone 40°), 6 dmg, stance ×1.5, cd 4 · Glide passive (from Skyborne, SPEC) · Air dash Dash in the air, 400 cm, cd 3 · Vortex Pull 360°, radius 600, pulls 400 cm, cd 14 · Tornado Summon: moves where you aim at 400 cm/s, 10 dps, lifts lowland, 10 s, cd 60 · Sky lord Transform: hover 8 s, all attacks from the air · Storm volley Strike: 30 arrows over radius 600, 8 dmg each, cd 50 |
| Arcane | Arcane missile Bolt 14, mark 25, cd 0.8, 8 Mana · Barrier Self buff shield 30 HP for 8 s, cd 12 · Arcane blast Nova at aim point (range 1200) radius 300, 32 dmg, stance ×2, chargeable, cd 8 · Rune trap Summon a rune 300 radius, 40 dmg on enter, 30 s, cd 10 · Aegis Storm-shaped dome radius 500 blocks all projectiles 8 s, cd 60 · Starfall Strike 12 stars over radius 700, 18 dmg each, cd 50 · Rune circle Pool radius 600, allies +15% dmg, enemies mark 10/s, 15 s, cd 60 |
| Blood & curse | Blood bolt Bolt 22, bleed 30, costs 5 HP, cd 1.0 · Blood spear Lance 34, bleed 50, 10 HP, cd 6 · Blood boil Nova radius 600: every bleeding enemy takes 20% of its bleed burst again, cd 12 · Transfusion Ally target: give 20 HP, cd 10 · Hex of weakness Bolt (no damage), curse 80, cd 10 · Doom Bolt: mark that deals 60 after 4 s, cd 20 (no instant kills; CAL) · Crimson tide Wall that travels 1500 cm at 900 cm/s, 45 dmg, bleed 60, 15 HP, cd 60 · Ruin passive capstone (SPEC) · Sanguine lord Self buff 10 s, cd 90 |
| Tide | Water bolt Bolt 15, soak 35, cd 1.0, 6 Flow · Riptide pull Pull, cd 10 · Whirlpool Pool radius 350 holds lowland/midland, 6 dps, 6 s, cd 18 · Healing wave Wall-shaped ally heal 35, travels 1200 cm, cd 12 · Cleanse Ally target removes statuses, cd 15 · Tidal shield Ally target 35 HP shield 8 s, cd 15 · Maelstrom Storm radius 1000, pull 300 cm/s, 12 dps, cd 60 · High tide Wall heal 60 allies, knockback enemies, cd 90 |
| Necromancy | Raise dead Summon from a corpse within 1000: a raised copy at 60% stats for 30 s, 2 Souls, cd 6 · Corpse explosion Nova on your minion, 40 dmg, radius 400 · Shadow bolt Bolt 30, decay 40, 1800 cm/s, cd 3, 2 Souls · Life drain Chain-shaped single beam 2.0 s channel, 12/s heals 50%, cd 12 · Army of the dead Summon every corpse in 1200, 20 s, cd 120 · Lich form Transform 15 s, cd 120 · Final harvest passive (executes < 20%, SPEC) |
| Spore | Spore cloud Pool radius 300, plants 1 Spore/s, poison 10/s, 6 s, cd 6 · Blight Pool radius 400 that spreads 50 cm/s to 700, rot 15/s, cd 18 · Sporeling Summon 60 HP biter, 25 s, cd 10 · Spore armor Self buff −15% damage taken 20 s, cd 30 · Fungal behemoth Summon 500 HP, 40 s, cd 120 · Living colony Transform 15 s, cd 120 · Detonate (the Spores mechanic, SPEC "planted then detonated"): granted free with Spore cloud as its own quick-slot entry, 0.30 s cast walking, every Spore you've planted within 2000 cm bursts for 8 dmg + rot 10 each |
| Void | Void bolt Bolt 24, costs 2 max HP, cd 1.2 · Void rift Pool radius 300, 14 dps, 6 s, 4 max HP, cd 12 · Collapse Pull 360°, radius 500, 30 dmg on pull end, cd 16 · Blink Dash 700 cm, cd 3, 1 max HP · Shadow step Dash behind the lock target, cd 8 · Phase Self buff: i-frames 0.6 s, cd 10 · Nowhere Self buff: untargetable 3 s, next hit crits (riposte multiplier), cd 60 · Black hole Strike radius 600, pulls 600 cm/s 4 s then 80 dmg, cd 60 · Maw of the void Cone 90° 800 cm, 90 dmg, 15 max HP, cd 90 · Sacrifice Self buff: spend 10 max HP, +30% dmg 10 s |

Summons and raises are Raven's path abilities and stay; they are not ER spirit summons (CAL 1.19 rule is about spirit ashes only).

---

### 2.14 Hit feel (GAP-COOKED; base CAL L1, L2)

On every connected hit, both sides of the fight:

| Layer | Light | Heavy | Charged, guard counter | Parry | Riposte, crit | Boss kill |
|---|---|---|---|---|---|---|
| Hitstop (both actors' meshes `CustomTimeDilation` 0.02) | 0.06 | 0.10 | 0.14 | 0.09 (SPEC) | 0.20 | 0.30 |
| White flash on the victim | 70 ms | 140 ms | 200 ms | gold rim 180 ms | 260 ms | 260 ms |
| Music duck | none | 2 dB | 3 dB | 2 dB | 4 dB | 4 dB (never more) |

- Camera trauma 0.04 + 0.22w² and FOV punch 0.2 + 1.3w degrees, with weight w: light 0.3, heavy 0.6, charged 0.8, crit and boss kill 1.0. Rumble 35 + 85w ms, strong motor off on lights. Victim mesh shake 3 cm (light) to 6 cm (crit) at 30 Hz, never the camera (CAL L1).
- Hitstop freezes at the blade's deepest point, not first contact; after it the montage catches up to server time over 0.1 s (CAL 1.17).
- **Sparks and blood by surface** (`physMaterial` on the creature row): flesh (blood mist, dark red), scale (blood + scale chips), chitin (ichor + shell shards), stone/obsidian (rock chips + orange sparks), ice (shards + frost puff), metal (white-yellow sparks), wood/bark (splinters), fungal (spore puff), crystal (glass ring + prism glints), ember (cinders), void (inward ink smear). Infused and path hits add the element burst at `fx.impact.elementScale` 0.45 (SPEC).
- **Four sound layers per hit (CAL):** transient (weapon class × surface), body (weapon weight thud), material (surface), creature voice (pain call from the species' voice set). Whooshes on every swing sized by class.
- **Swing weight:** coil minimums 0.14 / 0.20 s; follow-through at ×1.0; settle 0.26 to 0.34 s (CAL L2).
- Damage numbers: off by default; beside the creature's bar when on (CAL L6).

### 2.15 Lock-on and camera (GAP-COOKED; base CAL 1.15; numbers SPEC from `tuning.camera`)

| Item | Value |
|---|---|
| Acquire | Nearest visible target to screen centre within 2500 cm and 40° (SPEC); lock points count as candidates |
| Keep / lose | Keep to 3500 cm; lost after 2.5 s unseen (SPEC) |
| Turn | 2.5 → 7 over 0.5 s (SPEC) |
| Switch | Right stick past 0.6 → next target or lock point in that direction, 0.25 s re-arm. Mouse flick ≥ 300 px/s, or Alt + wheel |
| Lock points | Creature row `lockPoints` (bone sockets). Head and chest on highland creatures and up; bosses add every part with its own stance. Aim at the point's height, not the feet |
| Giant framing | Target height > 3× the player's: boom +150 cm and pitch +10° over 0.4 s |
| Spring arm | One arm on a camera-block channel (walls, cliffs, rocks with second-largest extent ≥ 160 cm); props ignore it. Pull in within 0.12 s, hold 0.15 s, ease out over 0.4 s. Hide the player mesh under 100 cm |
| Never | Turns on its own (except lock-on); inverted by default |
| First person | Full body, camera on the head bone (FULL-GAME 9). Same montages and notifies; the server traces the 3P weapon only (CAL T6) |
| Off-screen rule | A creature starts a windup only while on the player's screen or audible within 8 m with a directional cue (CAL 1.13) |

### 2.16 Healing (GAP-COOKED; potions SPEC)

| Item | Value | Tag |
|---|---|---|
| Potions | Minor healing 45, healing 90, antidote cleanses (SPEC `items.json`). New: greater healing 140 (Hearth 3), superior 200 (Hearth 4) | SPEC + GAP-COOKED |
| Status cures | Ember salve (burn), Warming draught (frostbite), Bloodmoss poultice (bleed), Grounding tonic (shock), Clear-eye drops (blind), Hex-ward charm (curse), antidote (poison, rot). Each drinks like a potion; recipes go to the alchemy cook | GAP-COOKED |
| Drink | 0.9 s committed, can't cancel; heal lands at 0.5 s; walk ×0.4 | CAL |
| Belt | 4 slots, each one item type drawn from the bag; Y drinks, D-pad down cycles | CAL |
| Cooldown | 1.5 s shared (replaces the per-item 6 s placeholder in `items.json`) | CAL wins (FULL-GAME 5 rule) |
| AI reading | Highland creatures and bosses with `readsAction` answer a drink start with their fastest attack within 0.4 s if in range | CAL |
| Heal spells | Stay (FULL-GAME conflict 8, proposed); interruptible by staggers like any cast | GAP-COOKED |
| No floors | A drink interrupted by a stagger is lost; healing never undoes a landed blow | Aaron rule |

### 2.17 Bosses

#### 2.17.1 Rules for all

| Rule | Value | Tag |
|---|---|---|
| Phases | 2 + hidden 3 with artifacts (phase 3 at 25%, damage ×1.4) | SPEC (BUILT) |
| HP | Major 12,000, mini 3,000 solo; +80% per extra player or tame | SPEC |
| Loot | 10% damage = own loot, rivals included | SPEC |
| Chase / reset | 90 m / 65 m; reset after 8 s empty; respawn 4 h default | SPEC |
| Stance / parries | Majors 140 to 180, 3 parries; minis 90, 2 parries | GAP (3.3) kept |
| Transition | At 50%, at the next moment not inside a crit or stance break: an unblockable telegraphed attack (crimson glint, 2.17.1b), windup 1.4 s; the boss takes 50% damage during it; phase 2 starts as it lands | GAP-COOKED |
| Phase 2 | ≥ 2 new delayed attacks, 1 new combo, 1 arena change, one phase-1 move gets a delayed variant; idle gaps drop from 0.6-1.4 s to 0.3-0.9 s | GAP-COOKED |
| Attack choice | Bands close < 4 m, mid 4-12 m, far > 12 m; weights per band; never the same move 3 times running; the punish window after a combo always plays; at most one projectile or ranged area in flight per boss in phase 1, two in phase 2 | GAP-COOKED |
| Tells | Windups > 0.5 s have a rising sound cue; tells read at 15 m; delayed attacks flash and sound 0.2 s before contact; cues are small glints on the attacking part (2.17.1b): white for blockable, crimson for unblockable | CAL |
| Projectiles | Windup + travel ≥ 0.9 s; max 1000 cm/s in Fire/Frost, 1600 cm/s in late biomes; never home; nothing fired inside 4 m | GAP (3.3) kept |
| Tame-punishing move | One per major, targets the furthest tame first | SPEC rule / move GAP-COOKED |
| Lock points | Head, chest, every part with its own stance | GAP (3.3) kept |
| Run-back | Shrine-stone respawn 40 to 60 m from every arena door, picked on the death map | GAP (3.3) kept |
| First kill | Named drop, visible, flies to the killer within 6 s or at 4 m | GAP (3.3) kept |
| Status | Resistance ×1.25 per extra participant; minis and majors can't be Frozen (Rimed instead, 2.11) | CAL + 2.11 |
| Health bar | Bottom centre: name, bar, phase pip; no stance bar | GAP-COOKED |

#### 2.17.1b Visible contact and small cues (GAP-COOKED; replaces today's full-area boss decals)

Today every boss attack draws a full-area `AWRGroundMarker` decal and deals damage to anyone `InShape` (BUILT, `WRBoss.cpp`). Aaron's bar is damage only on visible contact, with a small telegraph cue. The rule from C10 on, for every boss and creature:
- **Damage comes from traces on the visible body part:** hammer head, claws, jaw, tail, horn, wing edge (bone-socket sweeps, the same `UWRWeaponTraceNotifyState` the hero uses), or from the real projectile or splash. The SPEC shape (cone, line, circle, range, size) stays as the AI's "in range to pick this" test and as the hard limit on how far the trace may reach. It no longer deals damage by itself.
- **The cue is small:** a glint on the weapon or a flare in the eyes at the tell's start, then a second glint 0.2 s before contact on delayed attacks. Crimson glint = unblockable, gold-free white glint = blockable. Never a full-area floor paint.
- **Ground marks only for things that are really on the ground:** lava cracks after a slam, the flood itself (rising lava you can see), slick ice, Grimjaw's boulder shadow growing where it will land, Slag throw's falling glob shadow. These are the world showing what will happen, not a UI overlay.
- **Shockwaves are visible:** slam rings are a real dust/ember wave that travels outward at 1200 cm/s from the impact, and damage rides that wave's front (jumpable, `low`).

#### 2.17.2 Vulkarn, the Forge Giant (moves SPEC from BO and `WRBoss.cpp`; timing fields GAP-COOKED)

Stance 180, 3 parries. Lock points: head, chest, hammer arm (part, stance 60: broken → he drops the hammer 4 s and fights with the tongs; drops Forge slag once). Weak to frost (SPEC). Voice: bull walrus roar under a bellows wheeze; struck anvil on his footfalls.

| Move | Phase | Shape (SPEC) | Windup | Hold | Active | Punish | Damage | Impact | Flags |
|---|---|---|---|---|---|---|---|---|---|
| Hammer combo ×3 | 1 | Cone 480, 55° | 0.60 each, gap 0.55 | — | 0.20 | 1.0 | 40 each | 3 | parryable |
| Overhead slam | 1 | Target circle 900, r 280 | 1.20 | — | 0.25 | 1.4 | 70 + lava cracks 6 s, 8 dps | 5 | blockable |
| Tongs grab | 1 | Line 750, w 120 | 0.90 | — | 0.30 | 1.2 on a miss | 45 + throw | grab | parryable (SPEC): parry staggers 2.5 s |
| Anvil slam | 1 | Self 2500, r 450 | 1.60 | — | 0.30 | 1.6 | 50 + flood 4 s × 30 dps (SPEC) | 5 | unblockable; stand on anvils |
| Forge roar (transition) | 1→2 | Self, r 900 | 1.40 | — | 0.40 | 1.0 | 45, push 900 | 5 | unblockable; then flood 22 s (SPEC) |
| Bellows breath | 2 | Cone 1000, 35° | 1.00 | — | 1.2 sweep | 1.2 | 35 + burn 60 | 2 | blockable |
| Slag throw | 2 | Target circle 1800, r 220 | 1.10 + flight 1000 cm/s | — | 0.20 | 0.8 | 45 + burn 40 | 3 | lands where you stood at windup start; **tame-punish: furthest tame first** |
| Late hammer (delayed variant) | 2 | Cone 480 | 0.60, 0.55, 0.90 | 0.5 on hit 3 | 0.20 | 1.2 | 40 / 40 / 55 | 3 | parryable |
| Quench feint | 2 | Target circle 700, r 300 | 0.70 | 0.6 | 0.25 | 1.4 | 75 | 5 | blockable |
| Bellows into slam (combo) | 2 | Cone → circle | 1.00 → 0.80 | — | — | 1.6 | 35 + 70 | 2 / 5 | — |
| Dual hammer | 3 | Cone 520 ×4 | 0.55 each | 0.4 on hit 4 | 0.20 | 1.2 | 45 each; every slam floods (SPEC) | 4 | parryable hits 1-3 |

#### 2.17.3 Skaldrith, the Frost Wyrm (moves SPEC; timing GAP-COOKED)

Stance 160, 3 parries. Lock points: head, chest, left wing, right wing (6 wing hits in flight grounds him 8 s, SPEC). Weak to fire. Voice: saltwater crocodile bellow + calving-glacier crack + low whale moan.

| Move | Phase | Shape (SPEC) | Windup | Hold | Active | Punish | Damage | Impact | Flags |
|---|---|---|---|---|---|---|---|---|---|
| Frost breath sweep | 1 | Cone 1200, 28° | 1.00 | — | 1.6 sweep | 1.4 | 40 + frost 60 | 2 | blockable; pillars block it (SPEC) |
| Claw ×2 | 1 | Cone 420, 60° | 0.55, 0.50 | — | 0.20 | 0.9 | 38 each | 3 | parryable; 40% → lunge bite |
| Tail sweep | 1 | Self 650 | 0.90 | — | 0.35 | 1.0 | 45 | 4 | `low` (jump it); breaks pillars (SPEC) |
| Lunging bite | 1 | Line 900, w 160 | 0.80 | — | 0.30 | 1.2 | 60 | 4 | parryable |
| Pin (grab) | 1 | Line 600, w 140 | 1.00 | — | 0.25 | 1.3 on a miss | 30 + bite 50 | grab | unblockable; dodge only |
| Take-off gust (transition) | 1→2 | Self, r 900 | 1.40 | — | 0.40 | 0.8 | 20, push 900 | 4 | unblockable; then air 14 s / landed 10 s cycle (SPEC) |
| Dive | 2 | Target circle 3000, r 350 | 1.20 | — | 0.30 | 1.6 on landing | 70 | 5 | unblockable |
| Air breath | 2 | Target circle 3000, r 450 | 1.00 | 0.5 (new) | 1.0 | — | 30 + frost 50 + slick ice 10 s | 2 | blockable |
| Ice spikes | 2 | Target circle, r 200 | 1.00 | — | 0.25 | — | 50 | 5 | crimson; **tame-punish: first volley at the furthest tame** |
| Late tail (delayed variant) | 2 | Self 650 | 0.90 | 0.4 | 0.35 | 1.0 | 50 | 4 | `low` |
| Claw-claw-lunge (combo) | 2 | Cone → line | 0.50, 0.45, 0.70 | — | — | 1.4 | 38 / 38 / 60 | 3 / 3 / 4 | parryable |
| Freezing breath | 3 | Cone 1200 | 1.00 | — | 1.6 | 1.4 | 30 + freezes on contact (SPEC) | — | unblockable |

#### 2.17.4 Fire and Frost mini-bosses

Built minis keep their SPEC moves and numbers (some windups sit under 3.3's 0.65 band; BUILT wins). This cook adds stance 90, 2 parries, flags, punish and lock points.

| Mini | Moves (SPEC shape, windup, damage) | Added | Counter (SPEC) |
|---|---|---|---|
| Obsidian Warden | Ground pound self 450, 1.0, 45 · Charge line 1000, 1.0, 50 · Fist cone 380, 0.7, 35 | Fist parryable; charge impact 5, punish 1.4 (2.5 if it hits a wall); pound `low`, punish 1.2. Shell: damage taken 0.15 until 40% broken (SPEC); strike ×1.2 on shell | Chill the shell (×2.5, SPEC) |
| Ignessa | Fire bolt target circle 2200, r 180, 0.9, 25 · Swipe cone 400, 0.6, 30 · Wisps 4 × 25 (SPEC) | Bolt lands where you stood; airborne damage taken 0.25 (SPEC); landed 8 s is the punish | Kill the wisps; she lands |
| Grimjaw | Club ×2 cone 450, 0.8, 50 · Boulder target circle 1600, r 260, 1.3, 55 · Stomp self 400, 0.9, 35 | Clubs parryable; club 2 delay hold 0.3; boulder flight 1000 cm/s; stomp `low` | Regen 1.5%/s unless burning or bleeding (SPEC) |
| Pale Hunter | Pounce line 800, 0.7, 45 · Claw cone 380, 0.5, 32 | A snow-crunch sound always plays 0.6 s before a pounce from invisibility; claw parryable | Footprints; fire or lightning reveals 6 s (SPEC) |
| **Cinderhorn** (GAP-COOKED, roaming, ash plains) | Charge line 2500, w 180, windup 1.0 (hoof scrape + snort), runs 1400 cm/s up to 1.8 s, 60, impact 5, burning trail 6 s × 8 dps · Horn hook cone 450, 0.75, 45, impact 4, parryable · Buck self 350, 0.8 (only when you're behind), 40, `low` · Ash stomp self 500, 1.1, 35 + burn 40 | Wall or basalt-pillar impact = stunned 4.0 s with the weak-point mark. Horn part stance 50: broken → no trail. A frost proc on him stops the trail 20 s | Bait charges into pillars; frost the horn |
| **Old Tusker** (GAP-COOKED, roaming, snow flats) | Tusk sweep cone 500, 120°, 0.8, 50, impact 4, parryable · Trample charge line 2000, 1.0, 55, impact 5 · Rear and crash self 450, 0.9 + hold 0.3, 65, impact 5 · Snow fling target circle 1400, r 200, 1.0 + flight 900 cm/s, 30 + slow · Herd bellow 1.2 windup, once at 60%: 2 Woolly drudges join | Tusk parts stance 45 each: both broken → no sweep and the trample stops at its first hit. A burn proc sends him to water 6 s with his back open (stance ×1.5) | Break the tusks; burn him |

#### 2.17.5 Majors and minis of the ten new biomes
Each biome cook writes its bosses' move tables on this template: phase 1 = 5-6 moves including one 3+ hit combo and one grab; a transition attack; phase 2 = 2 new delayed attacks + 1 new combo + 1 arena change + 1 delayed variant; one tame-punish move; stance 140-180; the opposite-element counter from FULL-GAME 3.2 visible in the fight. Minis: 4-5 moves and one counter mechanic. Names, arenas and counters are in FULL-GAME 3.2.

### 2.18 Creature attack rows (GAP-COOKED)

#### 2.18.1 The row (data schema, CAL 1.13 extended)

| Field | Meaning |
|---|---|
| `id` | `cinderfang.bite` |
| `motion` | `snap`, `lunge`, `charge`, `spit`, `stomp`, `sweep`, `grab`, `burst` (from ground, lava, snow, water), `dive`, `tail`, `roar` (call or buff), `explode` |
| `range` | min / max cm to pick it |
| `windup` | s from the first tell frame to the first active frame; bands lowland 0.45-0.65, midland 0.40-0.80, highland 0.34-0.90 (3.3) |
| `trackUntil` | Fraction of windup spent turning (default 0.5, turn 180°/s, SPEC) |
| `delayHold` | 0-0.6 s at the top (midland ≤ 0.3, highland ≤ 0.6, lowland 0); a final flash + sound always 0.2 s before contact |
| `active` | s the damage shape is live, only while the jaw, claw, horn or body visibly travels |
| `punish` | Recovery after it (lowland ≥ 0.8, midland ≥ 0.7, highland ≥ 0.6 after a combo's last hit) |
| `damageMult` | × tier hit (lowland 12, midland 20, highland 32, SPEC) |
| `poiseDamage`, `impact` | 2.7 and 2.5 |
| `staminaDamage` | Default damage × 1.2 |
| `buildup` | `{status: amount}`; elemental creatures carry their element (15 on bites by default) |
| `physType` | slash / strike / pierce |
| `flags` | `parryable`, `low`, `unblockable`, `grab`, `ignoreHyperarmor`, `projectile`, `behindOnly` |
| `projectile` | speed (≤ 1000 cm/s early biomes, ≤ 1600 late), radius, gravity; never homes |
| `comboNext` | `{id, chance}`; at most 4 hits per string |
| `readsAction` | Answers drink / cast / charged heavy within 0.4 s (highland and bosses only) |
| `weights` | Pick weight per range band |
| `cooldown` | s |
| `tell` | Sound cue id, glint (none / white / crimson) on the attacking part, pose |

#### 2.18.2 Rows for the 19 built creatures (moves SPEC from `creatures.json` + `tuning.creatureAI`; rows GAP-COOKED)

W / H / A / P = windup / delay hold / active / punish, seconds.

| Creature (tier) | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| Cinderfang (low, pack 3-4) | Bite | 0.50 / 0 / 0.15 / 0.80 | 1.0 | 20 (2) | parryable, burn 15 | 40% → Bite 2 |
| | Bite 2 | 0.45 / 0 / 0.15 / 0.80 | 1.0 | 20 (2) | parryable, burn 15 | — |
| | Lunge (SPEC 300-650 cm) | 0.60 / 0 / 0.25 / 0.90 | 1.4 (SPEC) | 30 (3) | parryable, burn 20 | Flankers lunge from the side |
| Gloomhowl (low, pack 3-4) | Bite, Bite 2 | as Cinderfang | 1.0 | 20 (2) | parryable, frost 15 | — |
| | Lunge | 0.60 / 0 / 0.25 / 0.90 | 1.4 | 30 (3) | parryable | Prefers your back |
| | Howl | 0.80 / 0 / — / 1.0 | — | — | — | Calls packmates within 40 m |
| Magmaw (high) | Bite | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable, burn 20 | 50% → Claw |
| | Claw | 0.45 / 0 / 0.18 / 0.60 | 0.9 | 30 (2) | parryable | 30% → Lunge |
| | Lunge (SPEC) | 0.60 / 0.3 / 0.25 / 0.80 | 1.4 | 45 (4) | parryable, burn 25 | — |
| | Spit (SPEC 450-1600 cm) | 0.75 / 0 / — / 0.60 | 0.8 | 10 (1) | projectile 1000 cm/s (was 1800), burn 25, 150 cm burning splash 3 s | ≥ 1.2 s warning at min range |
| | Lava burst | 0.90 (pool bubbles and glows) / 0 / 0.30 / 1.2 | 1.5 | 50 (5) | from lava within 600 cm; blockable | Ambush opener only |
| | readsAction | Lunge within 0.4 s of a drink or charged heavy | | | | |
| Scorchwing (mid, flyer) | Dive | 0.80 (screech) / 0.2 / 0.30 / 1.5 grounded | 1.4 | 40 (4) | parryable | Grounded 1.5 s = the bring-down window |
| | Fireball spit | 0.75 / 0 / — / 0.60 | 0.8 | 10 (1) | projectile 1000 cm/s, burn 30 | ≥ 450 cm only |
| | Wing buffet | 0.45 / 0 / 0.20 / 0.70 | 0.7 | 25 (2) | push 300 | Close range |
| Basaltback (mid, neutral) | Stomp (SPEC r 320) | 1.00 / 0 / 0.20 / 1.20 | 1.5 | 40 (4) | `low` | — |
| | Shell bash | 0.70 / 0 / 0.25 / 0.90 | 1.2 | 45 (4) | blockable | — |
| | Withdraw | — | — | — | After 25% of its stance in 2 s it shells up 3 s: damage ×0.2, strike ×1.5 | — |
| Emberhorn (mid, neutral) | Charge (SPEC) | 0.90 (hoof scrape) / 0 / 1.2 run / 1.30 | 1.6 | 45 (5) | knockback 900 (SPEC); wall hit stuns it 2.5 s | — |
| | Horn toss | 0.55 / 0.2 / 0.20 / 0.70 | 1.2 | 40 (5) | parryable | — |
| | Rear kick | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | `behindOnly` | — |
| Kilnmite (low, neutral) | Bite | 0.45 / 0 / 0.15 / 0.80 | 1.0 | 15 (1) | parryable | — |
| | Burrow pop | 0.80 (dirt mound moves) / 0 / 0.25 / 1.0 | 1.2 | 25 (3) | — | Hitting a nest wakes 4 |
| Ashstrider (low, passive) | Kick (cornered or hit) | 0.50 / 0 / 0.15 / 0.90 | 1.0 | 30 (3) | — | Then flees |
| Glowmoth (low, passive) | Dust burst | 0.60 / 0 / 0.30 / 1.0 | 0 | — | blind 20 (from R5; until then a 1.5 s vignette dim) | Then flees |
| Pumicehog (low, passive) | Hop bump (cornered) | 0.50 / 0 / 0.15 / 1.0 | 0.5 | 10 (1) | — | — |
| Rimeclaw (high) | Claw 1, Claw 2 | 0.38 / 0 / 0.16 / —, then 0.34 / 0 / 0.16 / 0.60 | 0.9 each | 30 (2) | parryable, frost 15 | 50% → Saber bite |
| | Saber bite | 0.70 / 0.4 / 0.18 / 0.70 | 1.5 | 50 (4) | parryable, frost 25 | — |
| | Lunge (SPEC) | 0.60 / 0.3 / 0.25 / 0.80 | 1.4 | 45 (4) | parryable | — |
| | Spine shake | 0.55 / 0 / 0.20 / 0.80 | 0.8 | 25 (2) | `behindOnly`, r 250, frost 20 | — |
| | readsAction | Saber bite with no hold, within 0.4 s | | | | |
| Glacimaw (mid, worm) | Snow burst | 0.90 (snow ripple + rumble) / 0 / 0.30 / 1.2 | 1.5 | 50 (5) | from under you; blockable | — |
| | Ice spit (SPEC 450-1600) | 0.75 / 0 / — / 0.60 | 0.8 | 10 (1) | projectile 1000 cm/s, frost 25 | — |
| | Body slam | 0.80 / 0.3 / 0.30 / 1.0 | 1.3 | 45 (4) | line 600 | — |
| Woolly drudge (mid, neutral) | Stomp (SPEC) | 1.00 / 0 / 0.20 / 1.20 | 1.5 | 40 (4) | `low` | — |
| | Tusk sweep | 0.65 / 0 / 0.25 / 0.90 | 1.2 | 40 (4) | cone 120°, parryable | — |
| | Rear and crash | 0.90 / 0.3 / 0.25 / 1.30 | 1.7 | 60 (5) | — | — |
| Frostantler (mid, neutral) | Charge (SPEC) | 0.90 / 0 / 1.2 run / 1.30 | 1.6 | 45 (5) | knockback 900 | — |
| | Antler rake ×2 | 0.45, 0.40 / 0 / 0.20 / 0.80 | 1.0 each | 30 (3) | parryable | — |
| | Rear kick | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | `behindOnly` | — |
| Rimeshell (mid, neutral) | Shell roll | 0.80 (tucks and rocks) / 0 / 1.0 roll / 1.4 | 1.4 | 45 (4) | line 1200, blockable | — |
| | Pinch | 0.50 / 0 / 0.15 / 0.80 | 1.0 | 25 (2) | parryable | — |
| Snowpuff (low, passive) | Peck (cornered) | 0.45 / 0 / 0.12 / 1.0 | 0.3 | 5 (1) | — | — |
| Aurora fox (low, passive) | none | — | — | — | Flees, never attacks | — |
| Wisp (Ignessa's) | Swell and burst | 1.00 (glow swells, hiss rises) / 0 / 0.20 / dies | 25 flat (SPEC) | 30 (3) | r 250, burn 30 | Killed before it bursts = no burst |

Built creature stance: lowland 20, midland 35, highland 55 (2.10). `physResist`: Basaltback, Rimeshell, Kilnmite shells strike 1.2 / slash 0.8; Woolly drudge, Gloomhowl, Rimeclaw fur slash 1.15 / strike 0.9; Magmaw scale pierce 1.1; the rest 1.0. Lock points: Magmaw, Rimeclaw head and chest; Glacimaw head and mid-body; Woolly drudge and Basaltback head, chest, back.

#### 2.18.3 Templates for the 87 new creatures (each biome cook fills names, clips and numbers inside these bands)

| Archetype | Rows | Typical W / A / P | Notes |
|---|---|---|---|
| Pack hunter | bite, bite 2, lunge, (call) | 0.45-0.60 / 0.15-0.25 / 0.8-0.9 | Flank roles (2.18.4) |
| Ground predator | claw ×2, heavy (delayed), lunge, one special | 0.34-0.90, hold ≤ 0.6 / 0.15-0.25 / 0.6-0.8 | readsAction if highland |
| Ambusher | burst opener, bite, retreat | opener 0.90 with a ground or canopy tell | Rotmaw, Murkjaw, Dunejaw; Thornlynx drops from canopy (shadow + rustle 0.9 s) |
| Constrictor / grabber | grab (unblockable, dodge only), bite, tail | grab 0.9-1.1 | Rootcoil; Bogstalker tongue pulls 600 cm |
| Flyer | dive, spit or gust, buffet | dive 0.8 with a screech | Grounded 1.5 s after a dive; wings are lock points |
| Burrower | burst, bite, tunnel | burst 0.8-0.9 with a ground tell | Coilback's ring tell 1.0 |
| Herd charger | charge, rake, rear kick | charge 0.9 with a hoof scrape | Wall stun 2.5 s |
| Herd heavy | stomp (`low`), sweep, crash | 0.65-1.0 | Hyperarmor on every attack |
| Swarm | bite (×0.5), latch (bleed or drain 1 s), burst on death | 0.45 | 4-12 at once, 25 HP each |
| Swimmer | bite, ram, drag (pulls under 2 s) | 0.5-0.8 | Underwater tells by bubble trail and sound |
| Caster beast | projectile (≥ 0.9 s total), area (crimson if unblockable) | 0.75-1.0 | Hexcrow, Echobat, Prismback (reflects player projectiles) |

Every new creature: real clips, feet planted, a voice fused from real animal calls, a terrifying readable silhouette for predators (FULL-GAME 3.3).

#### 2.18.4 How groups fight (GAP-COOKED; "flank, surround, retreat" RAVEN-PLANNED for R2; "no attacker limit" Aaron)

- **No attack token.** Each pack member picks attacks on its own timer; three Cinderfangs can bite in the same second.
- **Roles by position, not permission:** the leader (largest) presses from the front at 2-3 m; flankers circle at 4-7 m and drift to 90° and 150° off the player's facing and attack when they get there; packs of 4+ keep one on the far side.
- **Breathing:** after its combo and punish, a creature backs to 5-8 m for 1.0-2.5 s, circling, then returns. It's a gap a skilled player uses, not a turn order.
- **Retreat:** below 25% HP flee (SPEC `fleeHealth`); a pack that lost a member in 15 s flees below 50% (SPEC `packFleeHealth`), regroups 30 m away and returns if you're still in its territory.
- **Pulling:** aggro radius and line of sight are the player's tools. A thrown stone (new: X on a loose stone, 1 dmg, 20 m) or an arrow pulls the one it hits plus any packmate within 6 m that sees it.

### 2.19 Mounted combat (RAVEN-PLANNED; details GAP-COOKED, built in R2)

| Item | Value |
|---|---|
| Rider attacks | RB swings to the side the camera faces; RT heavy (rear-up strike). Count as running attacks (stance ×1.2 / ×2.8) |
| Couched charge | Spear held, mount above 900 cm/s: RT hold = couched lance, ×2.0 damage, stance ×4, impact 5 |
| Not on a mount | Guard, parry, Wildbrands unless flagged `mounted` (Flame slash, Ice slash, Static strike, Sun strike, Wind cut), drinking while moving |
| Spells | Bolt, Lance and Chain shapes only |
| Mount's attack | The tame attacks on its own under the attack order (SPEC wheel); the rider's lock target becomes its target |
| Unhorsed | Rider hit with impact ≥ 4, or mount staggered: thrown, 1.2 s knockdown (wake-roll allowed) |
| Mount HP | Under the rider's HP; 0 = dead (SPEC permadeath) |
| Dismount | X hold 0.5 s; A while moving jumps off |

### 2.20 PvP (GAP-COOKED; base CAL 1.18)

| Item | Value |
|---|---|
| Global | Player-to-player damage ×0.6; status buildup ×0.7 |
| Per class (on top) | Dagger 1.0, sword 1.0, axe 1.0, pick-hammer 0.95, spear 1.0, greatsword 0.9, great hammer 0.85, scythe 0.9 |
| Wildbrands / spells / capstones | ×0.85 / ×0.85 / ×0.7 |
| Crits | Backstab arc 75°; riposte and backstab ×0.7; victim gets 0.3 s i-frames on rising |
| Dodge-skill i-frames vs players | Bolt dash, Air dash, Blink, Phase, Burrow ×0.7 |
| Homing | Player spells vs players 2000 (SPEC) |
| Tames vs players | Damage ×0.5; tames can't riposte or backstab |
| Guard-broken player | Riposte-able for 1.0 s |
| Judge | 250 ms server rewind only; no client hits |
| Artifacts | Never change PvP damage (SPEC) |
| Combat tag | Hitting or being hit by a player or their tame tags you 15 s: no fast travel, no wake-point swap, no clan storage; logging out while tagged leaves your body in the world 30 s, killable and lootable |

### 2.21 HUD, prompts, input isolation, F1, F8 (GAP-COOKED; base CAL L4-L10)

- **HUD:** HP, stamina and path resource bars top-left (Raven's HUD kept); status meters above stamina; belt and quick-slot cross bottom-left; enemy bar over a creature only once damaged or locked, status meters under it; boss bar bottom centre.
- **Prompt diet:** one prompt at most; the riposte/crit prompt once per species, then never; state words only (GUARD BROKEN, STANCE BROKEN).
- **Colour law:** gold = yours and block/parry (guard edge, parry flash, weak-point mark, your own ability marks); crimson = unblockable (enemy glints and weapon trails); nothing decorative in either; body glow stays off (Raven 11-01).
- **Held-input isolation (CAL 1.16):** entering a menu, build mode, taming, mount or death screen drops held inputs and clears the buffer; on leaving, held buttons are ignored until released and presses are blocked 0.14 s.
- **F1:** controls page for the device in use, mouse and pad navigable, B/Esc backs out.
- **F8 (View on pad):** bug report with a screenshot, a text box, build version, map position and the last 60 s of the combat log (hits, parries, rolls with their judged times). Saved to `Saved/Reports` and sent through the F35 report relay our other games' F8 uses.

### 2.22 Not copied (CAL 1.19)
Spirit summons (taming is the ally system); stat points and scaling letters; FP and memory slots; sleep; dodge on release; peer-to-peer hit authority; ER's two-hand Strength ×1.5 (replaced by +15%).

---

## 3. Plan (how to build it in Raven's repo)

Raven's patterns, followed everywhere: every ability is a `UWRGameplayAbility` subclass plus a `UWRAbilityRegistry::ClassForNode` entry; all damage goes through `UWRCombatLibrary::ApplyHit` on the server; every number lives in `Data/*.json` read through `WRTune`; clients send intent, never results. Gates are compile, cook and package; Aaron and testers test through F8.

### 3.1 Data

**`Data/tuning.json` (change and add blocks)**

| Block | Keys |
|---|---|
| `player` | `staminaRegenPerSecond` 45, `staminaRegenDelay` 0.5 (old 30 / 0.8 kept under `_raven`), `poise` 0 (from armour), `poiseResetSeconds` 30; drop `poiseRegenPerSecond` |
| `stamina` | `roll` 12, `backstep` 8, `jump` 8, `sprintPerSecond` 5, `graceFloor` -20, `weaponArt` 18, `spell` 12; remove `blockMin`/`blockMax` (block cost comes from the hit) |
| `combat` (new) | `bufferSeconds` 0.33, `dodgeBufferMax` 0.6, `comboWindowFraction` 0.4, `cancelFraction` {light 0.5, heavy 0.65, great 0.75}, `chargeMin` 0.35, `chargeFull` 0.8, `chargeMax` 1.0, `counterHit` {damage 0.15, pierceDamage 0.3, stance 0.3}, `stealth` {stance 0.2, buildup 0.2}, `chainHeavy` {damage 1.1, stance 1.2}, `twoHand` {damage 0.15, lightStance 0.3, heavyStance 0.1}, `inputGateSeconds` 0.14, `stepIn` {min 45, max 130, cone 60, range 300} |
| `defense` | `load` {light 0.3, medium 0.7; per tier distance, iFrameStart, iFrameEnd, recovery}, `backstep` {distance 250, iFrames 0.10, recovery 0.22}, `jumpLegInvuln` 0.8, `blockAngleDegrees` 70 (kept), `blockStun` {light 0.2, heavy 0.35}, `guardBreakStaggerSeconds` 1.2 (kept), `guardCounterWindow` 0.6, `parryTapSeconds` 0.30, `parrySpamWindow` 0.5, `parrySpamShrink` 0.3, `perfectDodgeSeconds` 0.20, `maxEquipLoad` 60 |
| `crit` (new) | `ripostePaired` {total 1.6, damageAt 0.9}, `backstabPaired` {total 1.4, damageAt 0.75}, `backstabArc` 90, `backstabRange` 120, `backstabMaxRadius` 70, `stanceCritRange` {normal 100, big 150}, `syncDistance` 110, `warpSeconds` 0.12, `parryVulnerableSeconds` 2.0, `parryDamageTaken` 1.5 |
| `stance` (new) | per tier {lowland 20, midland 35, highland 55, mini 90, major 140}, `breakSeconds` {2.5, 2.5, 3.0, 4.0, 5.0}, `regenDelayDivisor` 13, `regenPerSecond` 13, `lowHealthRegen` 0.5, `group` [1.0, 0.6, 0.45, 0.35], `parriesToBreak` {normal 1, mini 2, major 3}, `nonBreakParryStance` 0.25, `headStrikeMult` 1.5, `partBreakBodyStance` 0.3, `partStaggerSeconds` 1.2 |
| `poise` (new) | `reactions` {1: 0.25, 2: 0.55, 3: 0.85, 4: {fall 0.6, down 1.2, wakeRollAfter 0.5, wakeRollIFrames 0.4, riseIFrames 0.35}}, `hyperarmor` {heavy 1.5, charged 2.0, great 2.5}, `hyperarmorDamageTaken` {normal 0.075, great 0.175}, `shudderSeconds` 0.12 |
| `status` | Keep every built key. Add `thresholds` {lowland 100, midland 120, highland 150, mini 250, major 400, player 100}, `procGrowth` 1.3, `resetAfterCombat` 30, `bossParticipantResist` 1.25, `pvpBuildup` 0.7, and one block per status with the 2.11 numbers (`bleed`, `poison`, `rot`, `shock`, `soak`, `blind`, `decay`, `mark`, `curse`, `frostbite` burst, `rimed`), `reactions` table, `infusedBuildup` {light 8, heavy 14, charged 22} |
| `fx.hitStop` | {light 0.06, heavy 0.10, charged 0.14, parry 0.09, crit 0.20, bossKill 0.30} (replaces `seconds`/`parrySeconds`, old values map to light/parry); `fx.flash` ms per tier; `fx.trauma` {base 0.04, scale 0.22}; `fx.fovPunch` {base 0.2, scale 1.3}; `fx.rumble` {base 35, scale 85}; `fx.victimShake` {min 3, max 6, hz 30}; `fx.musicDuck` per tier |
| `camera` | Keep all. Add `flickThreshold` 0.6, `flickRearm` 0.25, `mouseFlickPx` 300, `giantRatio` 3, `giantBoom` 150, `giantPitch` 10, `pullInSeconds` 0.12, `holdSeconds` 0.15, `easeOutSeconds` 0.4, `hideUnder` 100, `blockMinExtent` 160 |
| `potions` (new) | `drinkSeconds` 0.9, `healAt` 0.5, `walkMultiplier` 0.4, `sharedCooldown` 1.5, `beltSlots` 4 |
| `bosses` | Add `transition` {windup 1.4, damageTaken 0.5}, `idleGap` {p1 [0.6, 1.4], p2 [0.3, 0.9]}, `bands` {close 400, mid 1200}, `maxRepeats` 2, `projectilesInFlight` {p1 1, p2 2}, `runbackStone` {min 4000, max 6000}, `firstKillFlySeconds` 6 |
| `pvp` (new) | `damage` 0.6, `buildup` 0.7, `classes` {…2.20}, `wildbrand` 0.85, `spell` 0.85, `capstone` 0.7, `critArc` 75, `critDamage` 0.7, `riseIFrames` 0.3, `dodgeSkillIFrames` 0.7, `tameDamage` 0.5, `guardBrokenRiposte` 1.0, `combatTagSeconds` 15, `logoutBodySeconds` 30 |
| `mounted` (new, R2) | `couchSpeed` 900, `couchDamage` 2.0, `couchStance` 4, `unhorseImpact` 4, `unhorseKnockdown` 1.2, `dismountHold` 0.5 |
| `spells` | Keep every built block. Add per new spell the 2.13 numbers when its path round comes; add `shapes` {bolt, lance, nova, wall, pool, strike, storm, chain, buff, ally, summon, pull, dash, transform} each {windup, recovery, roots, walkMultiplier}; built blocks gain `fits` (Wildbrands) and `windup`/`active`/`recovery` |
| `creatureAI` | Keep. `spit.speed` 1000 (was 1800: projectile rule). Add `breathe` {min 1.0, max 2.5, distance [500, 800]}, `flank` {inner 400, outer 700, angles [90, 150]}, `regroupDistance` 3000, `pullAlertRadius` 600, `offscreenAudibleRadius` 800 |

**`Data/weapons.json` (new; CAL 1.17 "move to its own file")**
```json
{ "classes": [
  { "id": "sword", "hands": "1h", "weight": 3.0, "reach": 220, "trace": 24,
    "damage": { "light": 12, "heavy": 22 }, "types": { "light": "slash", "heavy": "pierce" },
    "stamina": { "light": 10, "heavy": 20, "charged": 30 }, "stanceLight": 5, "poiseLight": 30, "guardStamina": 30,
    "crit": { "backstab": 2.30, "riposte": 3.45 }, "guard": { "boost": 25, "phys": 0.6, "element": 0.3, "parry": [0.13, 0.10, 0.50] },
    "great": false, "pvp": 1.0,
    "moves": [
      { "id": "light1", "windup": 0.25, "active": 0.18, "recovery": 0.30, "dmg": 1.0, "stance": 1.0, "next": "light2", "montage": "AM_Sword_L1" },
      { "id": "light2", "windup": 0.22, "active": 0.16, "recovery": 0.30, "dmg": 1.0, "next": "light3" },
      { "id": "light3", "windup": 0.30, "active": 0.20, "recovery": 0.42, "dmg": 1.15 },
      { "id": "heavy", "windup": 0.55, "active": 0.22, "recovery": 0.45, "ha": true },
      { "id": "chainHeavy", "...": "..." }, { "id": "charged", "...": "..." },
      { "id": "runLight" }, { "id": "runHeavy" }, { "id": "rollLight" }, { "id": "backstepLight" },
      { "id": "jumpLight" }, { "id": "jumpHeavy" }, { "id": "crouchLight" }, { "id": "guardCounter" } ] } ],
  "offhands": [ { "id": "buckler", "weight": 1.5, "guardBoost": 35, "boostPerUpgrade": 1, "phys": 0.75, "element": 0.45, "parry": [0.13, 0.17, 0.45] } ] }
```
Every class and off-hand of 2.4 gets a row; every move of 2.4.2 gets a row. `items.json` weapons gain `"class": "<id>"` (copper/bronze/iron swords → `sword`; hatchets → `axe` with tool damage; picks → `pick_hammer`); armour gains `poise` and `resist`; shields gain `"offhand": "<id>"`.

**`Data/attacks.json` (new; every creature and boss row, 2.18.1 schema)** keyed `<creature>.<attack>`. `creatures.json` rows gain `stance` (or take the tier's), `physResist`, `physMaterial`, `lockPoints` [{socket, name}], `critSocket`, `parts` [{id, socket, stance, hp, drop, onBreak}], `attacks` → list of row ids (replacing today's `["spit","lunge"]`, which stay as the `motion` of their rows), `voice` (sound set id).

**`Data/paths/<path>.json`** (DATA_SCHEMAS shape): ability nodes gain `"art": { "fits": [...], "resourceMode": "gain|cost|hp|maxhp" }` or `"spell": { "shape": "...", "chargeable": bool, "roots": bool }`. Fire and Frost first; each later path in its round.

**`Data/combos.json`** (DATA_SCHEMAS, not built): needed only for the reaction rule's 50% non-owner effect; created in R1 as FULL-GAME already plans.

**`Data/Audio/sounds.json`**: four-layer hit sets `hit.<class>.<surface>` (transient, body, material), swing whooshes per class, `tell.<creature>.<attack>`, voices `voice.<species>.{tell,strike,pain,death,call}`, `parry`, `guard_break`, `stance_break_crack`, `crit_<class>`, `drink`, `status_proc_<status>`.

**`Data/fx.json`**: recipes for sparks per surface (11), weak-point mark (gold), crimson glint, white glint, slam shockwave ring, parry flash, status procs (12), guard edge glow.

### 3.2 C++ (classes and functions to add or change)

| Area | File | Change |
|---|---|---|
| Hit struct | `WRCombatTypes.h` | `FWRHit` adds `StanceDamage`, `StaminaDamage`, `EWRPhysType PhysType`, `uint8 Impact`, `TMap<FGameplayTag,float> Buildups` (Burn/Frost fields stay and feed the map), flags `bUnblockable`, `bGrab`, `bLow`, `bIgnoreHyperarmor`, `bParryable`, `bCrit`, `bProjectile`, `FName AttackId`, `float ServerTime` (rewind). `EWRHitResult` adds `PerfectDodged`, `Grabbed`, `StanceBroken`. `EWRAbilityInput` adds `Interact`, `Drink`, `TwoHand`, `LockOn` |
| One resolver | `WRCombatLibrary.cpp` `ApplyHit` | Fixed order: rewind → i-frames / jump-low / perfect dodge → parry (count, stance, group reward) → guard (GB stamina, chip by negation, guard break, block stun) → hyperarmor/poise → damage (counter-hit, crit multiplier, weakness 1.5×, phys resist, status modifiers, PvP block) → stance (group scaling, parts, head daze) → buildups (thresholds, growth, PvP) → reactions → multicast hit feel. `BlockStaminaCost(Damage)` becomes `GuardStaminaCost(Hit, GuardBoost)`. New statics: `StanceTaken`, `PoiseBroken`, `CritMultiplier`, `PvpScale` (pure, testable as Raven likes) |
| Attributes | `WRAttributeSet.h/.cpp` | Add `Stance`, `MaxStance` (creatures and tames), `EquipLoad`, `MaxEquipLoad`. Path resources added per round (Charge, Radiance, …) as Raven did Heat/Chill |
| Melee | `WRAbilities.cpp` `UWRGA_Melee` | Reads its move row from `weapons.json` (class from the held item) instead of `weapon.lightTiming`; `PickMove()` picks contextual rows from state tags (`State.Sprinting`, `State.RollRecovery`, `State.BackstepRecovery`, `State.Airborne`, `State.Crouched`, `State.GuardCounterWindow`, `State.ComboWindow`); step-in warp; charge hold via `UAbilityTask_WaitInputRelease`; waist split flag. Light/heavy GAs stay as the two entry points |
| Windows | new `WRAnimNotifies.h/.cpp` | `UWRANS_Window` (one notify state class with a `FGameplayTag Window` property: ComboWindow, CancelWindow, IFrames, Hyperarmor, ParryActive, Tell, Tracking). Until a clip carries notifies, `UWRGameplayAbility::After()` timers apply the same tags from the row's numbers, so data timing works with any clip |
| Buffer | new `UWRInputBuffer` (component on `AWRPlayerController`) | Stores the last pressed action and time; flushes when `State.CancelWindow` / `State.ComboWindow` appears or the ability ends; 0.33 s life, dodge 0.6 s; `Clear()` on mode changes. `UWRInputGate` handles held-input isolation (0.14 s) |
| Dodge | `UWRGA_Dodge` | Load tier from `EquipLoad/MaxEquipLoad`; backstep branch (stick at rest); per-tier distance as root-motion warp scale; perfect-dodge detection sends `Event.PerfectDodge`; ledge guard |
| Jump | `WRCharacter.cpp` | `State.JumpLegsInvuln` for 0.8 s; hit zones on the capsule split by height (legs < 45% height) |
| Guard / parry | `UWRGA_Block` → `UWRGA_Guard` | Press: guard tag + parry window by off-hand row; release before 0.30 s → parry recovery; spam shrink; regen pause; guard-counter window tag after block stun; guard poke for spears |
| Crits | new `UWRGA_Critical` + `UWRCritLibrary` | `FindCritTarget(Attacker)` checks riposte (Staggered-by-parry, guard-broken player), backstab (arc, range, size, not windup), stance break (weak-point socket range), knockdown crit; plays paired montages with `UMotionWarpingComponent` on both; attacker i-frames to the damage frame; damage via `ApplyHit` with `bCrit` |
| Stance | new `UWRStanceComponent` (creatures, bosses, tames) | Stance meter, regen rule, parts array, parry count, group scaling from the boss ledger's participants, break → `Status.StanceBroken` + gold mark (multicast) |
| Poise | `UWRStatusComponent` | 30 s reset, reaction tiers, hyperarmor multiplier from tag, knockdown with wake-roll window |
| Status | `UWRStatusComponent` | Buildups move to a replicated `FFastArraySerializer` (`FWRBuildupArray`) keyed by status tag (12 attributes per actor would bloat replication); `FWRStatusDef` table from tuning; per-status `OnProc` handlers; growth ×1.3; reactions table; Frozen → Rimed on bosses |
| Creature attacks | `WRCreature.cpp` + new `UWRGA_CreatureAttack` | One ability reads an `attacks.json` row: tell (sound + pose + glint), tracking, delay hold with the 0.2 s final cue, motion kind (snap, lunge, charge, spit, stomp, sweep, grab, burst, dive, tail, roar, explode), damage from bone-socket traces on the jaw, claws, horns or body (the row shape only limits reach, 2.17.1b), punish, `comboNext`. Existing bite/lunge/charge/spit/stomp code becomes motion handlers. Off-screen rule check before a windup starts |
| Creature AI | StateTree tasks (Raven's M4) | `STT_Circle`, `STT_Breathe`, `STT_Flank`, `STT_ReadPlayerAction`, `STT_PickAttackByBand`, `STT_Regroup`. No attack token anywhere |
| Bosses | `WRBoss.h/.cpp` | `ResolveAttack()` stops applying damage by `InShape`: each attack row names its hit sockets (`HitSockets`, radius) and a trace notify window drives damage; `InShape` stays as the pick-range test and reach clamp; `AWRGroundMarker` is kept only for real ground effects (lava cracks, floods, landing shadows). `FWRBossAttack` gains `HitSockets`, `DelayHold`, `Active`, `Punish`, `Impact`, `Flags`, `ComboNext`, `BandWeights`, `TamePunish`; `BuildAttacks()` reads `attacks.json` rows with today's `MakeAttack` numbers as fallbacks; `PickAttack()` by band and repeat rule; `StartTransition()` at 50%; lock points; parts via `UWRStanceComponent`; run-back stone actor `AWRRunbackStone` (a bed-type wake point bound to the arena); first-kill drop actor `AWRFirstKillDrop` |
| Projectiles | `WRProjectile.cpp` | Enemy projectiles: `bHoming` forbidden by code for non-player instigators; launch target fixed at windup start; min range 400 cm assert |
| Wildbrands | `WRAbilities.cpp` | `UWRGA_Wildbrand : UWRGA_Melee` with fits check, resource mode, charge, follow-up window; built arts (Flame slash, Ice slash, Molten edge, Flurry, Molten cleave) re-parent to it. New arts per round = a subclass (or a data row on the generic one) + `ClassForNode` line |
| Spells | `UWRGA_Spell`, `UWRGA_AreaSpell` | Cast-while-walking slow and `roots` from the shape row; charge; new shapes added to `EWRAreaShape` (Chain, Pull) and new classes `UWRGA_SelfBuff`, `UWRGA_AllyTarget`, `UWRGA_Summon`, `UWRGA_DashSpell`, `UWRGA_Transform` as their rounds need them |
| Potions | `WRInventory.cpp` + new `UWRGA_Drink` | 4-slot belt, 0.9 s commit, heal at 0.5 s, `State.Drinking` (read by AI), shared 1.5 s cooldown, stagger cancels and consumes |
| Two-handing | `WRInventory.cpp`, `WRHeldTool.cpp` | Toggle, shield to back socket, guard from weapon |
| Lock-on | `WRPlayerController.cpp` | Target = {Actor, LockPointIndex}; flick switching; giant framing request to the boom |
| Camera | `WRCameraBoom.cpp` | Trace channel `WRCameraBlock` in `DefaultEngine.ini`; props set to ignore it; pull/hold/ease timings; hide-under rule |
| Hit feel | `WRFxCombat.cpp`, `WRFx.cpp`, `WRSound.cpp` | Hitstop table, flash tiers, trauma/FOV formulas, rumble via `UForceFeedbackEffect` assets per tier, victim shake, 4-layer sound play, music duck call into `WRAudioDirector` (≤ 4 dB) |
| PvP | `WRCombatLibrary.cpp`, `WRSocial.cpp` | `pvp` block applied when instigator and target are players (or a player's tame); combat tag; logout body |
| Mounted (R2) | `WRCreature.cpp` riding code | Rider attack GAs with side choice, couch charge, unhorse rule |
| UI | new `WRUIControls.cpp` (F1), `WRUIBugReport.cpp` (F8); `WRUIHud.cpp` | Status meters, boss bar, belt cross, prompt diet, stamina refusal flash |
| Input | `WRKeyBindings.cpp`, Enhanced Input mapping contexts | The 2.1 table for pad and KB+M; hold/tap triggers via `UInputTriggerHold`/`UInputTriggerTap` |
| Lag | `WRCombatLibrary` + new `UWRRewindComponent` | 250 ms ring buffer of capsule and hit-zone transforms at 30 Hz on every pawn; melee and projectiles rewind to `Hit.ServerTime`. C0 checks whether any of it exists |

### 3.3 Assets and the art route

| Need | Count | Route |
|---|---|---|
| Hero attack clips per class (lights, heavy, chain heavy, charge loop + release, running ×2, rolling, backstep, jump ×2, crouch, guard counter, guard idle / hit / break, parry + recovery, riposte, backstab, stance crit, Wildbrand base swings ×3) | About 26 per class × 9 classes ≈ 230 | Fab melee animation packs retargeted to the MetaHuman skeleton with UE's IK Retargeter (GASP rig), cleaned in Blender or UE's anim editor; Mixamo fills gaps; mocap from video (Move.ai or Rokoko Vision) for anything missing. Root motion kept and warped, never baked travel played in place (CAL T3) |
| Victim clips on the hero | Flinch ×4 directions, stagger, heavy stagger, knockdown + get-up + wake-roll, launch, grab victim per boss grab, riposte/backstab victim (PvP) | Same route |
| Creature clips | Per creature: every attack row, flinch, stagger, stance break (kneel/slump, 2.5-5 s), knockdown + get-up for highland and up, parried recoil, crit victim (paired), death | Real keyed or mocap-sourced clips per species (Fab animal packs retargeted to the creature rigs where the body fits; hand-keyed in Blender on `Tools/art` rigs otherwise). No procedural attack clips |
| Weapon meshes | 9 classes × 5 metals + 7 off-hands × tiers + torch | Concept → Hunyuan3D 2.1 base → Blender hard-surface finish (clean topology, bevels, real materials: copper patina, bronze, forged iron, steel, starsteel) → grip socket and `Grip` offset (CAL T5); or a Fab medieval weapons pack where its look fits the survival tone. Element infusion is a material layer (glowing fire cracks, frost rime, etc.), not a new mesh |
| VFX | Sparks ×11 surfaces, element bursts ×12, status procs ×12, weak-point mark, parry flash, guard edge, crimson and white glints, slam shockwave rings, slash trails per class | Niagara, built once each, preloaded via `preload.json` (CAL T2) |
| Sound | Hit sets: 9 classes × 11 surfaces × 3 layers (variations ×4); whooshes per class; tells and voices for the 19 built creatures + 6 bosses; parry, guard break, stance crack, crit stingers, drink | Commercial-OK libraries (Sonniss GDC bundles, royalty-free), Freesound CC0/CC-BY, own Foley; creature voices fused from real animal calls (2.17 voices for bosses; each built creature gets one: Cinderfang = African wild dog chitter + coal crackle; Gloomhowl = arctic wolf howl pitched down + bat click; Magmaw = Komodo hiss + boiling mud; Scorchwing = fruit bat shriek + blowtorch; Rimeclaw = leopard growl + ice creak; Glacimaw = elephant-seal belch + ice grind; Basaltback = tortoise grunt + rockfall; Emberhorn = bison snort + forge bellows; Woolly drudge = musk ox + elephant rumble; Frostantler = elk bugle; Kilnmite = stag beetle stridulation; Rimeshell = lobster click + ice; small passives from their nearest real animal) |
| Music | Combat layers per biome are the music cook's; this lane only calls the ≤ 4 dB duck and the boss-kill sting |
| Map dressing | Arenas: basalt pillars for Cinderhorn's ash plains route (6 to 8 pillars along his roam), 3 run-back shrine-stones (Vulkarn, Skaldrith, each Fire/Frost mini arena door), loose throwing stones near pack spawns | Placed in `L_Island` by the world lane's dressing pass; positions in `Data/island_dressing.json` |
| Combat lab | `L_CombatArena`: auto-spawn the chosen creature 6 m away, respawn 1.5 s after death, D-pad question cards (CAL L11), dummies with stance/poise readouts | Raven's existing map |

### 3.4 Build order inside this scope (CAL C0-C12, one Development cook each)

**Proof of one (first, inside C2):** the copper sword light chain against one Cinderfang in `L_CombatArena`: real clip with the data timings, coil, 0.33 s buffer, step-in, full hit stack (hitstop 0.06, flash, sparks on fur, trauma, rumble, 4 sound layers), the Cinderfang's bite row with its tell, punish window and parryable flag. When that's in Aaron's build and his F8 reports say it feels right, the same code paths take every other class and creature.

| Step | Build | Data | Code | Assets |
|---|---|---|---|---|
| C0 | Read and check: what's built vs this file; does the 250 ms rewind exist for melee | — | Read only, compile | — |
| C1 | F8 report, F1 controls, pad layout (2.1), held-input isolation, combat lab cards | `tuning.combat.inputGateSeconds` | `WRUIBugReport`, `WRUIControls`, Enhanced Input contexts, `UWRInputGate` | Pad glyphs |
| C2 | Feel: hit stack, hitstop table, swing weight, step-in, waist split, windows, buffer. **Proof of one** | `fx.*`, `combat.*`, `weapons.json` sword rows | `UWRANS_Window`, `UWRInputBuffer`, `WRFxCombat`, Motion Warping, `UWRGA_Melee` reads rows | Sword clips on the hero, Cinderfang bite clip, spark/sound sets for fur |
| C3 | Stamina and roll: 45/s after 0.5, grace, load tiers, backstep, jump legs, perfect dodge | `player`, `stamina`, `defense.load` | `UWRGA_Dodge`, attributes, jump tag | Roll ×3 tiers, backstep |
| C4 | Guard, parry, crits: GB, chip, guard break, guard counter, tap/hold parry, riposte, backstab, paired crits, rewind judge | `weapons.json` off-hands, `crit`, `defense` | `UWRGA_Guard`, `UWRGA_Critical`, `UWRCritLibrary`, `UWRRewindComponent` | Shields (crude, wooden, metal), parry, riposte and backstab pairs |
| C5 | Stance and poise: split, break + gold mark, group scaling, parts, hyperarmor, reactions, 1-HP rule, prompt diet | `stance`, `poise`, creature `stance`/`parts` | `UWRStanceComponent`, poise in `UWRStatusComponent` | Stance-break clips for the 19 creatures, hero reaction clips |
| C6 | Full attack set: charged, running, rolling, jump, crouch, chain heavy, counter-hit, stealth, two-handing; then the other 8 classes | `weapons.json` all classes | `PickMove()`, charge task, two-hand toggle | Class clips, weapon meshes (axe and pick first: they exist as tools) |
| C7 | Status: thresholds, growth, decay, Frostbite/Frozen/Rimed, Burn, fire ends frostbite, reactions, meters | `status.*` | `FWRBuildupArray`, `FWRStatusDef` handlers, HUD meters | Proc VFX and sounds for burn and frost |
| C8 | Wildbrands and spells for Fire and Frost: every node, fits, resource mode, charge, follow-ups, cast-while-walking, shapes; the unbuilt Fire/Frost nodes | `paths/fire.json`, `paths/frost.json`, `spells.*` | `UWRGA_Wildbrand`, shape rows, registry lines | Wildbrand swings, spell VFX |
| C9 | Creatures: `attacks.json` rows for all 19, tells, off-screen rule, delay cue, combos, punish, readsAction, colour law, flanks and breathing | `attacks.json`, `creatures.json` fields, `creatureAI` | `UWRGA_CreatureAttack`, StateTree tasks, projectile rule | Creature attack, parried and crit clips; voices |
| C10 | Bosses: visible-contact rewrite (2.17.1b: bone-socket traces replace InShape damage, full-area AWRGroundMarker decals become glints and real ground effects), rules, Vulkarn and Skaldrith tables, transitions, lock points, parts, the 4 built minis, then Cinderhorn and Old Tusker, run-back stones, first-kill drop, boss bar | `attacks.json` boss rows, `bosses.*` | `WRBoss` changes, `AWRRunbackStone`, `AWRFirstKillDrop` | Boss clips for new moves, Cinderhorn and Old Tusker models (concept → Hunyuan3D 2.1 → Blender → `Tools/art` rig) |
| C11 | Camera and healing: spring-arm channel, giant framing, flick switching, lock points; drink commit, belt, AI reading, new potions and cures | `camera.*`, `potions`, items | `WRCameraBoom`, `WRPlayerController`, `UWRGA_Drink` | Drink clip, potion meshes |
| C12 | PvP pass: `pvp` block, crit arcs, dodge-skill i-frames, tame damage, combat tag, logout body | `pvp` | `PvpScale`, `WRSocial` combat tag | — |
| Each biome round | Its path's status (2.11 Round column), Wildbrands and spells (2.12, 2.13), its creatures' rows from the 2.18.3 templates, its bosses on the 2.17.5 template. R2 adds mounted combat (2.19) and smart groups (2.18.4 retreat/regroup is built in C9; R2 adds formation leaders) | | | |

C1 to C5 are Raven's M1 rule: one boss fight that feels good first.
