# Wildreach build lane: COMBAT CORE (C0, C2, C3, C4, C5)

Lane `build-combat-core`, 2026-11-14. Builds on `cooks/combat.md` (the full combat design) and COMBAT-AND-LESSONS (CAL).
C1 (F8, F1, pad layout, input isolation) shipped in `build-f8-controls`. Scope here: read-and-check, feel, stamina and
roll, guard / parry / crits with server rewind, stance and poise, for the player and the Fire and Frost creatures and
bosses. Gate: compile only. Aaron and the testers test (F8).

---

## 0. Requirement ledger

Every line of the brief, every Aaron quote and every CAL/combat.md row the brief names (C0, C2-C5), one requirement per
row. The "Done / where" column is filled at the end (section 6).

| # | Requirement | Source |
|---|---|---|
| R1 | C0: read WRCombatLibrary, melee GAs, WRCreature, WRBoss, WRFx against the bible; list built / differs / missing, incl. melee rewind | CAL Part 4 C0 |
| R2 | Full hit stack on every connected hit: victim reacts (flinch or shudder) | CAL L1, BAR |
| R3 | White flash on the victim 70 ms light → 260 ms crit; parry gold rim 180 ms | CAL L1, combat.md 2.14 |
| R4 | Sparks / blood by surface along the swing | CAL L1, 2.14 |
| R5 | Camera trauma 0.04 + 0.22 w², FOV punch 0.2 + 1.3 w degrees | CAL L1 |
| R6 | Rumble 35 + 85 w ms, strong motor off on lights | CAL L1 |
| R7 | Four sound layers per hit: transient, body, material, creature voice | CAL L1, BAR |
| R8 | Music ducks 2-4 dB on the biggest hits, never more | CAL L1, L13 |
| R9 | Hitstop table: light 0.06, heavy 0.10, charged / guard counter 0.14, parry 0.09, riposte / crit 0.20, boss kill 0.30; nothing over 0.3 | CAL L1, BAR |
| R10 | Hitstop freezes at the blade's deepest point, attacker creeps at ~2%, victim mesh shakes 3-6 cm at 30 Hz (not the camera), montage catches up over 0.1 s, never server time or the CMC | CAL L1, 1.17 |
| R11 | Swing weight: visible coil ≥ 0.14 s light / 0.20 s heavy, follow-through ×1.0, settle 0.26-0.34 s; never speed-scale wind-ups past ×2.2 | CAL L2, BAR |
| R12 | Motion-warp step-in toward the lock target / nearest enemy in a 60° cone within 3 m, clamped 0.45-1.3 m | CAL 1.17, combat.md 2.2 |
| R13 | Waist split: lights keep the legs walking at 0.35×; heavies root | CAL C2, combat.md 2.2 |
| R14 | Notify windows drive combo, cancel, i-frames; server and client read the same thing | CAL 1.16 |
| R15 | Light chain 1-2-3 (sword 0.25/0.18/0.30, 0.22/0.16/0.30, 0.30/0.20/0.42; dmg 1.0/1.0/1.15; stamina 10; stance 5/5/6); combo window = last 40% of recovery | CAL 1.1, combat.md 2.4.2 |
| R16 | Animation commitment: nothing (dodge included) cancels before the CancelWindow (50% of recovery light, 65% heavy) | CAL 1.1, 2.4.0 |
| R17 | Input buffer 0.33 s for attacks, roll, Wildbrand, items; last input wins; dodge pressed mid-swing waits up to 0.6 s for the CancelWindow; buffer clears on mode changes | CAL 1.16, BAR |
| R18 | Stamina regen 45/s after 0.5 s | CAL 1.6 |
| R19 | Regen paused while guarding, sprinting, and from an attack's press until its CancelWindow | CAL 1.6, combat.md 2.3 |
| R20 | Roll 12, backstep 8, jump 8, sprint 5/s; hunger/thirst empty halves regen (kept) | CAL 1.6 |
| R21 | Last-swing grace: an action starts if stamina > 0 and may take it to −20; while negative nothing starts | CAL 1.6 |
| R22 | A refused action shows: the stamina bar flashes white and shakes 3 px | combat.md 2.3 |
| R23 | Equip load (worn + held kg ÷ max 60 kg + bonuses), separate from the bag | CAL 1.7 |
| R24 | Load tiers: light < 30% 500 cm, i-frames 0.03-0.46, recovery 0.27; medium < 70% 420 cm, same frames; heavy < 100% 330 cm, 0.03-0.43, recovery 0.53; overloaded: slow step 120 cm, no i-frames; all cost 12 | CAL 1.7, combat.md 2.6 |
| R25 | Backstep: B with the stick at rest, 250 cm, 0.10 s i-frames, recovery 0.22, cost 8 | CAL 1.7 |
| R26 | Jump: `State.JumpLegsInvuln` 0.8 s; attacks flagged `low` miss a jumping player; ground waves / tail sweeps get `low` | CAL 1.7 |
| R27 | Dodge fires on press; 8-way relative to camera; perfect dodge (hit absorbed in the roll's first 0.20 s) fires an event nodes listen to | CAL 1.7, combat.md 2.6 |
| R28 | Rolls stop at ledges over 2 m unless the stick holds toward it | combat.md 2.6 |
| R29 | Guard Boost per item; stamina per blocked hit = staminaDamage × (1 − GB/100), only when a blow lands; holding costs nothing | CAL 1.2 |
| R30 | Guarded damage negation per item and element (chip damage) | CAL 1.2 |
| R31 | Guard break: stamina would go below 0 → 1.2 s stagger, riposte-able (creatures, PvP) | CAL 1.2 |
| R32 | Block stun 0.20 / 0.35 s; guard counter = heavy within 0.6 s of block stun ending (×1.3, stance 30, hitstop 0.14, no HA) | CAL 1.2, combat.md 2.8 |
| R33 | A guard pressed early still blocks; visible guard = faint gold edge on the guarding item | CAL 1.2, L4 |
| R34 | Parry: guard tap; startup/active/recovery by off-hand (0.13/0.17/0.45 small shield, 0.13/0.10/0.50 sword); release < 0.30 s = committed tap; spam within 0.5 s shrinks active 30% | CAL 1.3, combat.md 2.8 |
| R35 | Parry only attacks flagged `parryable`; boss grabs (Vulkarn tongs) parryable; no projectiles | CAL 1.3 |
| R36 | Parries to break: normal 1, mini 2, major 3; non-breaking parry deals 25% stance and interrupts | CAL 1.3, combat.md 2.9 |
| R37 | Group reward: a parried creature takes ×1.5 from everyone for 2.0 s | combat.md 2.9 |
| R38 | Riposte 3.45× (sword), paired 1.6 s, damage at 0.9 s, attacker i-frames to the damage frame | CAL 1.3 |
| R39 | Backstab 2.3× from a 90° rear arc, ≤ 1.2 m, human-size targets not attacking; big ones only on stance break; PvP 75° | CAL 1.3, 1.18 |
| R40 | Paired crits with motion warping to a sync point 1.1 m apart over 0.12 s, no popping | CAL 1.3, 1.17 |
| R41 | Server rewind: parries, i-frames and crits judged at the attacker's rewound time (≤ 250 ms ring buffer); no client-reported hits | CAL 1.3, 1.17, 1.18 |
| R42 | Players get Poise (0-50 from armour, + nodes), creatures and bosses get Stance | CAL 1.4, 1.5 |
| R43 | Stance per tier: lowland 20, midland 35, highland 55, mini 90, major 140 (Skaldrith 160, Vulkarn 180); break 2.5/2.5/3/4/5 s | CAL 1.4 |
| R44 | Stance regen after stance/13 s at 13/s; below 50% HP ×0.5 (Sekiro) | CAL 1.4, combat.md 2.10 |
| R45 | Stance break: break clip, gold weak-point mark on the crit bone; light within 1.0 m (1.5 m big) from the front → crit 3.0× riposte-class, hitstop 0.20 | CAL 1.4 |
| R46 | Group stance scaling: ×1.0 / 0.6 / 0.45 / 0.35 by participants, tames count | CAL 1.4 |
| R47 | The 1-HP rule: nothing floors HP; a blow to 0 kills | CAL 1.4, L5, BAR |
| R48 | Phase transitions never start during a crit or stance break | CAL 1.4 |
| R49 | Poise damage accumulates; full reset 30 s after the last poise hit and at once after a stagger | CAL 1.5 |
| R50 | Hyperarmor window (wind-up to end of active): poise ×1.5 heavy (×2 charged, ×2.5 great); damage taken −7.5% (great −17.5%) | CAL 1.5 |
| R51 | What breaks hyperarmor: grabs, Frozen, Shatter, stance break, `ignoreHyperarmor`, status procs | CAL 1.5 |
| R52 | Creatures with poise (Basaltback, Woolly drudge, bosses) get hyperarmor on their own attacks; the punish window is the recovery after | CAL 1.5 |
| R53 | Player hit reactions by impact (flinch 0.25 / stagger 0.55 / heavy 0.85) when poise breaks, else a 0.12 s shudder | combat.md 2.5 |
| R54 | Prompt diet: at most one prompt; the riposte / crit prompt once per species then never (saved per player) | CAL L6 |
| R55 | No hit words (PERFECT, BLOCKED...); only state words GUARD BROKEN, STANCE BROKEN | CAL L6 |
| R56 | Damage numbers beside the creature, never over its tell; can be turned off (off by default, combat.md 2.14) | CAL L6 |
| R57 | Colour law: gold = yours and block/parry (guard edge, parry flash, weak-point mark); nothing decorative gold | CAL 1.13, L3 |
| R58 | All damage through ApplyHit; numbers in data; abilities as UWRGameplayAbility; sounds / fx as table rows; server authority | Brief, Raven's rules |
| R59 | Every part fully: devices (mouse + pad), states, edge cases, multiplayer (replication) | Brief |
| R60 | No placeholders presented as finished; what's unfinished stays hidden and is listed | Brief |
| R61 | CHANGELOG Unreleased + HANDOFF updated in the same commit; LFS sizes reported | Brief |

---

## 1. C0: read and check (what's built, what differs, what's missing)

Read: `WRCombatLibrary.cpp` (612 lines), `WRAbilities.cpp` melee / dodge / block, `WRStatusComponent.cpp`,
`WRCreature.cpp` attacks and telegraphs, `WRBoss.cpp` attacks and phases, `WRFxCombat.cpp`, `WRFxHud.cpp`,
`WRAnimNotifies.*`, `WRCharacter.cpp` input and body anims, `WRAttributeSet`, `Data/tuning.json`, `items.json`,
`creatures.json`, `player_art.json`, `fx.json`, `sounds.json`.

| Area | Built today | Differs from the bible | Missing |
|---|---|---|---|
| Damage path | One `ApplyHit`, server only, all sources | Block = flat 10-25 stamina, 0% chip; parry = any melee in the first 0.2 s of a block; riposte = any melee hit on a Staggered target ×3 | Guard Boost, negation, block stun, guard counter, parry flags/counts, crits, backstab, rewind |
| Melee | `UWRGA_Melee`: timers windup → active (sphere sweeps along the facing every 30 ms) → recovery | No chain (the 3 slashes just cycle), the clip is stretched to windup+active (its contact frame lands anywhere), State.Attacking blocks nothing but other attacks, so a **dodge cancels any swing at any time** | Combo / cancel windows, buffer, step-in, root on heavies, hyperarmor |
| Windows | `UWRANS_Window` notifies on the hero's clips (combo, cancel, trace) from `anim_windows.json` | Tags only on machines that draw the body; nothing reads them | Gameplay use |
| Hit feel | Impact sprites by creature (`fx.json impacts`), one hit sound, hit-stop 0.06 s on heavy hits only (`bPauseAnims`), named shakes, pad rumble by kind | No light hitstop; hard pause (no creep, no catch-up); no victim shake; flash only on placeholder bodies; one sound layer; hit words "Parried / Blocked / Dodged / riposte / critical" | FOV punch, flash on real meshes, 4 layers, duck, tiers |
| Stamina | 100-150, regen 30/s after 0.8 s, paused while blocking; costs light 10 heavy 20 dodge 15 | Costs need ≥ 1; floor 0 | Grace to −20, pause mid-swing, jump cost, refusal cue |
| Roll | Root-motion constant force 450 cm / 0.45 s, i-frames 0.05-0.35; no input = roll backward | No load | Tiers, backstep, ledge guard, perfect dodge (Frozen moment fires on any i-frame hit) |
| Poise / stance | One Poise attribute for all; 0 → Stagger 1.5 s (= riposte ×3 for 1.5 s) | Creatures and players share it; regen 10/s after 2 s | Stance, break, weak point, group scaling, HA, impact reactions |
| Bosses | Attacks by shape (cone / line / circles) with `bMelee` = blockable + parryable; phases in `OnDamageTaken` | Every melee boss move parryable | Flags (`parryable`, `low`), phase gate on crits |
| Rewind | **None.** Melee sweeps the server's present; parries and rolls are judged when the server learns of them | — | Ring buffer, rewound sweeps, defender-time judgement |
| 1-HP rule | No floors found (`Health` only clamps at 0 and Max) | — | — (keep) |

Conclusion: C2-C5 replace the block / parry / riposte / poise rules in `ApplyHit`, give the melee ability data rows and
windows, and add a buffer, a crit ability, a stance layer and a lag judge. Nothing new bypasses `ApplyHit`.

---

## 2. Research: how the best games solve these exact things

### 2.1 Hit feel
- **Elden Ring / Dark Souls 3**: hitstop on the attacker's weapon only for a few frames (~2-6 at 30 fps) scaled by
  weapon weight; greatswords "dig in". The victim plays a flinch keyed to the attack's poise damage. The camera barely
  moves on your own hits; it shakes on big enemy slams. Lesson: hitstop scales with weapon weight; shake belongs to
  slams and the victim.
- **Monster Hunter World / Rise**: the famous "hit stop" per weapon class (great sword charged 3 ~ 0.15-0.2 s, dual
  blades almost none), sharpness changes the spark colour and a white "bounce" spark on bad hits. Both models freeze,
  but the attacker's effects keep animating, so the frozen frame reads as impact not lag. Lesson: freeze the *bodies*,
  keep sparks and sound running (WRFx sprites keep ticking during our hit-stop: good).
- **God of War (2018)** (GDC 2019 "Evolving Combat in God of War", Mihir Sheth): hitstop on both actors, the attacker
  creeps at a fraction of speed instead of a dead stop so it never reads as a hitch; camera shake and rumble authored
  per attack; the victim gets a positional jitter. We take the creep (2%), and the victim jitter (3-6 cm, 30 Hz).
- **Street Fighter / Smash (Sakurai, "hitstop" videos)**: hitstop is longer for heavier hits and *the victim shakes*
  during it while the attacker is still; the freeze happens at contact, then motion resumes at full speed (we add the
  0.1 s catch-up so the server-timed swing isn't left behind).
- **Vlambeer "The Art of Screenshake" (Jan Willem Nijman, 2013)** and **Squirrel Eiserloh, GDC 2016 "Juicing your
  cameras with math"**: trauma that decays linearly, shake = trauma² (small knocks barely move, big ones jolt), Perlin
  noise not random. Already built in `UWRFxCameraModifier`; we feed it the CAL formula.
- **Hades** (Supergiant): a white flash on the victim for 1-2 frames on every hit, longer on crits; damage numbers
  beside, not over, the target. Sound is layered (impact + material + enemy grunt). We take the flash tiers and the
  layer stack.
- **Real world, sound**: a sword cut on a body has a sharp transient (edge contact, 2-6 kHz), a low thud (mass, 60-150
  Hz), the material's own texture (flesh slap, armour ring, stone crack) and the animal's voice. Film Foley layers
  exactly these (cabbage / celery for flesh, a struck plate for armour).

### 2.2 Swing weight, step-in, waist split, commitment, buffer
- **Elden Ring**: no cancel before the recovery cancel point; inputs "queue and are performed at the next available
  chance" (Bandai guide); ~10 frames of buffer. The first frames of a light are a visible coil; step-in tracks the
  lock target and is capped so it never teleports.
- **Dark Souls 3**: rolling from a swing is only possible in its recovery tail; a roll pressed earlier is buffered
  (FromSoft's "input queue" is ~0.6 s for dodge), which is the 0.6 s CAL takes.
- **For Honor / Sekiro**: a lock-on step-in pulls you within striking range (Sekiro ~1 m) and stops short of the body.
- **Monster Hunter**: heavy weapons root you; light weapons let the legs walk. The upper body plays the swing over
  locomotion (layered animation): our hero already layers swings on the Upper channel.
- **Animation timing**: Disney's anticipation / action / follow-through; a swing reads heavy when the anticipation is
  at least ~4-6 frames at 30 fps (0.14-0.20 s) and follow-through runs at full speed. Time-warping a clip so its
  contact frame lands on the gameplay frame (rather than stretching the whole clip) is what Naughty Dog / Ubisoft call
  "warp to the hit frame".

### 2.3 Stamina and roll
- **Elden Ring**: regen ~45/s after ~0.5 s; roll 12 at every load; load tiers 30 / 70 / 100% change distance,
  i-frames (13/12 frames) and recovery (8/16 frames); overloaded can't roll; stamina may dip below zero on the last
  action ("negative stamina" is visible in ER datamines as a grace).
- **Dark Souls 1**: "fat roll" at heavy load; players read load from the roll itself. Lesson: show the tier where load
  is managed (inventory).
- **Breath of the Wild**: perfect dodge = dodge just before contact → reward. We fire `Event.PerfectDodge` for nodes.
- **Real world**: a judo / parkour forward roll covers ~4-5 m from standing at speed, ~0.7 s ground to feet.

### 2.4 Guard, parry, crits, netcode
- **Elden Ring guard**: stamina lost = attack stamina damage × (1 − Guard Boost/100); chip HP = damage × (1 −
  negation); guard break at 0; guard counter R2 after a block; parry = small shield 4f startup / 5f active.
- **Sekiro**: deflect window ~0.2 s; spamming shrinks it (the "deflect spam penalty"); every deflect adds posture;
  low vitality slows posture recovery. We take the spam rule and the low-HP regen rule.
- **Valheim**: a parried enemy takes double damage briefly (a group reward). We take ×1.5 for 2 s.
- **Dark Souls 3 / ER ripostes**: the attacker warps to a fixed offset in front of the victim, both play paired clips,
  the attacker is invulnerable through the stab; the damage frame is late in the clip so the move reads.
- **Netcode** (Valve "Source Multiplayer Networking / Lag compensation"; Bungie GDC 2011 "I Shot You First", David
  Aldridge; Overwatch GDC 2017 "Overwatch Gameplay Architecture and Netcode", Tim Ford):
  - The server keeps ~1 s of past hitboxes and rewinds them to *what the shooter saw* (its ping + interpolation
    delay), then tests the hit. Cap the rewind (Overwatch caps favour-the-shooter at ~250 ms of extrapolation).
  - Defensive abilities (Overwatch's Genji deflect, Tracer recall) are "favour the defender" exceptions: a defence
    the client already started protects it even if the hit was confirmed for the shooter, within the cap.
  - Elden Ring is peer-to-peer and judges enemy hits on the victim's own machine, so a roll on screen always works
    (and PvP backstabs lag). A server-authoritative equivalent is to wait up to the defender's lag for its defence
    before resolving an AI hit on a remote player, with the defence's time back-dated to when it was pressed.

### 2.5 Stance and poise
- **Elden Ring stance**: hidden meter; regen after stance/13 s at 13/s; break → long stun + crit from the front; boss
  stance damage shrinks with summons (×0.6 / ×0.35).
- **Sekiro posture**: the visible bar, faster fill from deflects; low HP slows recovery.
- **ER poise**: shown/10 internal; reset 30 s after the last hit and right after a stagger; hyperarmor multiplies it
  for the swing; grabs and status ignore it.
- **Monster Hunter**: a "tenderized" / stunned monster flops with a long recovery; the stun is read by the body, not a
  bar. Lesson: the break must read from the body (clip + sound + mark), not a UI bar.

### 2.6 Prompts
- **Elden Ring**: no "PARRY!" text; a crit prompt never shows: the game shows the state (the enemy slumps). **Sekiro**
  shows a red deathblow dot (the mark) and no words. **Ghost of Tsushima** shows a button prompt the first few times
  then trusts the player. We show the crit prompt once per species and a gold mark always.

---

## 3. Plan (every detail decided before building)

### 3.1 Data (all numbers first drafts in data)

**`Data/weapons.json` (new)**: the move rows and guard tools.
- `classes.sword`: `stamina` {light 10, heavy 20, guardCounter 20}; `stance` {light 5, heavy 10}; `poiseVsPlayers` 30
  (heavy ×1.6); `guardStamina` {light 30, heavy 45}; `crit` {backstab 2.30, riposte 3.45, stanceCrit 3.0};
  `guard` {boost 25, phys 0.60, element 0.30, parry [0.13, 0.10, 0.50]} (no shield);
  `moves`: `light1` 0.25/0.18/0.30 ×1.0 stance 5 clip 0 next light2; `light2` 0.22/0.16/0.30 ×1.0 stance 5 clip 1 next
  light3; `light3` 0.30/0.20/0.42 ×1.15 stance 6 clip 2; `heavy` 0.55/0.22/0.45 ×1.0 stance 10 HA; `guardCounter`
  0.30/0.20/0.50 ×1.3 stance 30 no HA hitstop tier "counter"; `riposte` total 1.6 dmg at 0.9; `backstab` total 1.4
  dmg at 0.75.
- Items map to a class by `items.json` `class` (default `sword`; tools use sword timing until C6 builds axe / pick).
- `offhands`: `crude_shield` (the hero's carried shield today: GB 45, phys 0.85, element 0.40, parry 0.13/0.17/0.45,
  weight 2.0), plus `buckler`, `wooden_shield`, `metal_shield`, `greatshield` (can't parry), `torch` rows for the
  crafting cook (only `crude_shield` is reachable now: the hero carries it; shields as items are a later cook).
  `carried`: `crude_shield`.

**`Data/tuning.json`**
- `player`: `staminaRegenPerSecond` 45, `staminaRegenDelay` 0.5, `poise` 0, `poiseResetSeconds` 30 (drop the 10/s
  regen).
- `stamina`: `dodge` 12, `backstep` 8, `jump` 8, `graceFloor` −20, `refuseFlashSeconds` 0.35, `refuseShake` 3.
- `combat` (new): `bufferSeconds` 0.33, `dodgeBufferMax` 0.6, `comboFraction` 0.4, `cancelFraction` {light 0.5,
  heavy 0.65}, `stepIn` {range 300, cone 60, min 45, max 130, ideal 0.55, turnRate 540, turnFraction 0.6},
  `lightMoveMultiplier` 0.35, `clipWarp` {maxCoilRate 2.2, minRate 0.6, activeMin 0.8, activeMax 2.0},
  `windowsFromClips` 0.
- `defense`: `load` {lightBelow 0.3, mediumBelow 0.7, light/medium/heavy {distance, travel, iStart, iEnd, recovery},
  overloaded {distance 120, travel 0.6}}, `backstep` {distance 250, travel 0.25, iStart 0.03, iEnd 0.13, recovery
  0.22}, `maxEquipLoad` 60, `jumpLegInvuln` 0.8, `blockStun` {light 0.2, heavy 0.35}, `guardCounterWindow` 0.6,
  `parryTapSeconds` 0.30, `parrySpamWindow` 0.5, `parrySpamShrink` 0.3, `perfectDodgeSeconds` 0.2, `ledgeDrop` 200,
  `creatureStaminaDamage` 1.2, `bossStaminaDamage` 1.6.
- `crit` (new): `syncDistance` 110, `warpSeconds` 0.12, `ripostePaired` {total 1.6, damageAt 0.9},
  `backstabPaired` {total 1.4, damageAt 0.75}, `backstabArc` 90, `pvpBackstabArc` 75, `backstabRange` 120,
  `backstabMaxRadius` 70, `backstabMaxHalfHeight` 110, `stanceCritRange` {normal 100, big 150}, `frontArc` 60,
  `parryOpenSeconds` 1.6, `exposedSeconds` 2.0, `exposedDamage` 1.5, `guardBrokenRiposte` 1.0, `riseIFrames` 0.3.
- `stance` (new): `tiers` {lowland 20, midland 35, highland 55, mini 90, major 140, none 60}, `breakSeconds` {lowland
  2.5, midland 2.5, highland 3, mini 4, major 5}, `regenDelayDivisor` 13, `regenPerSecond` 13, `lowHealthRegen` 0.5,
  `group` [1, 0.6, 0.45, 0.35], `parriesToBreak` {normal 1, mini 2, major 3}, `nonBreakParryStance` 0.25,
  `spellPerDamage` 0.3, `creaturePerDamage` 0.3, `shatterMult` 2.
- `poise` (new): `reactions` {1: 0.25, 2: 0.55, 3: 0.85}, `hyperarmor` {heavy 1.5}, `hyperarmorDamageTaken` 0.075,
  `shudderSeconds` 0.12, `creatureFlinchSeconds` 0.45, `creatureHyperarmorPoise` 100.
- `fx.hitStop`: {light 0.06, heavy 0.10, counter 0.14, parry 0.09, crit 0.20, bossKill 0.30, creep 0.02, catchUp 0.1,
  deepFraction 0.3, deepMax 0.04}; `fx.flash` {light 0.07, heavy 0.14, counter 0.2, crit 0.26, parry 0.18,
  opacity 0.55}; `fx.trauma` {base 0.04, scale 0.22}; `fx.fovPunch` {base 0.2, scale 1.3, seconds 0.18};
  `fx.rumble` {base 35, scale 85, strength 0.55}; `fx.victimShake` {min 3, max 6, hz 30}; `fx.weight` {light 0.3,
  heavy 0.6, counter 0.8, crit 1.0}; `fx.guardEdge` {r 214, g 170, b 70, glow 1.2, opacity 0.22}.
- `net` (new): `rewindMax` 0.25, `interpSeconds` 0.1, `history` 1.0, `sampleHz` 30.
- `creatureAI.<kind>`: `parryable` (bite / lunge 1; charge, stomp, spit 0), `low` (stomp 1), `impact` (bite 2, lunge
  2, charge 3, stomp 3, spit 1), `staminaDamage` (default damage × 1.2).
- `bosses.<id>.<attack>`: `parryable` (vulkarn hammer_combo, tongs_grab; skaldrith claw, lunge_bite; obsidian_warden
  fist; ignessa swipe; grimjaw / pale_hunter melee cones), `low` (tail_sweep, ground_pound, anvil_slam), `impact`
  (3 default, slams 3), `unblockable` none yet (C10).

**`Data/creatures.json`**: `stance` override for bosses (vulkarn 180, skaldrith 160) and `hyperarmor` true for
basaltback, woolly_drudge (bosses always). Tier → stance from tuning.

**`Data/items.json`**: armour `poise`: hide 8, copper 18, bronze 24, iron 32.

**`Data/fx.json`**: new recipes `crit.mark` (gold glint: flare + slow ring, loop, small gold light), `stance.break`
(gold-white burst, ring, falling grit), `guard.break` (big white-gold spark fan + ring), `hit.flash`? no: the flash
is the overlay material (below).

**`Data/Audio/sounds.json` + OGG**: `hit.transient.flesh / armor / stone / ice` (edge crack), `hit.body.light /
heavy` (low thud), `hit.ice`, `hit.magic` material layers, `stance.break` (deep crack + exhale), `guard.break`
(shield ring + crack), `crit.stab` (wet stab + bone), `hit.crit.duck` (no file: duck via events). Built with ffmpeg
from the CC0 / licensed sources already in `Data/Audio/combat` (Tools/audio/make_combat_layers.py, credits rows).

### 3.2 Code

**Hit struct (`WRCombatTypes.h`)**: `StanceDamage`, `StaminaDamage`, `Impact` (1-5), `HitTier`
(Light/Heavy/Counter/Crit/Parry for presentation), `bParryable`, `bLow`, `bUnblockable`, `bIgnoreHyperarmor`, `bCrit`,
`bGrab`. `EWRHitResult` gains `StanceBroken` (presentation). `EWRAbilityInput` unchanged.

**Weapon data (`WRWeapons.h/.cpp`, new)**: `FWRMove`, `FWRWeaponClass`, `FWROffhand`; `FWRWeapons::Get()` loads
`weapons.json` once (and on `WR.TuneReload`); `ClassFor(AWRCharacterBase*)` from the equipped weapon item's `class`;
`GuardFor(Character)` → carried offhand or the class guard; `EquipLoad(Character)`.

**Combat windows on the character (`AWRCharacterBase`)**: `FWRSwingWindows {double Start, ActiveAt, ActiveEnd,
ComboAt, CancelAt, End; FName Move; uint8 Chain; bool bHA}` set by the melee ability on server and owning client.
Tags `State.ComboWindow`, `State.CancelWindow`, `State.Hyperarmor`, `State.Windup` set by the ability's timers (server
replicated TagOnly, owner local) so the UI, AI and other clients read them. `UWRANS_Window` stops applying tags unless
`combat.windowsFromClips` is 1 (the clips are warped to the data instead).

**Rules (`UWRGameplayAbility::CanActivateAbility` + `FWRCommit`)**: one function `FWRCommit::Allows(Character, Kind,
Tolerance)`: Light/Heavy need (no swing) or (swing and Now ≥ ComboAt − tol); Dodge / Guard need (no swing) or (Now ≥
CancelAt − tol); nothing during Dodging (except the end), BlockStun (no attack / dodge), ParryRecovery, being crit.
Server tolerance = min(owner RTT/2 + 0.05, 0.25) so a press the client made inside its window isn't refused by a
server whose swing started later. A new melee / dodge / guard cancels the running swing (`CancelAbilitiesWithTag
Ability.Melee`).

**Melee (`UWRGA_Melee`)**: picks its move: Light → chain step (`light1..3`: next only if the previous light ended in or
after its combo window within 0.35 s; the 3rd wraps to 1), Heavy → `guardCounter` when `State.GuardCounterWindow`, else
`heavy`. Commit: costs from the row (grace). Faces the step-in target or the control yaw. Step-in: target = lock (the
client's lock / server's ServerLock) else nearest hostile in a 60° cone within 300 cm; distance = clamp(dist − (r_t +
0.55 × reach), 0, 130), < 45 → none; `UAbilityTask_ApplyRootMotionMoveToForce` over 80% of the wind-up (predicted).
Heavy and guard counter set `bAttackRoot` (new CMC flag, FLAG_Custom_3 → speed 0); lights keep `bAttackSlow` (×0.35).
Timers: Windup tag 0 → windup; HA tag for HA rows 0 → active end; sweeps through the active window; combo window opens
at recovery × (1 − 0.4); cancel window at recovery × 0.5 (light) / 0.65 (heavy); end. Regen pause: the stamina
regen skips while State.Attacking and no CancelWindow. Sweep: rewound targets for player attackers (below), stance /
stamina damage / impact / parryable from the row.

**Clip warp (`UWRAnimInstance`, `AWRCharacter::OnSwingVisual`)**: `FTrack` gains `RateB` and `SwitchAt`;
`PlayOneShotWarped(Clip, Layer, Start, RateA, SwitchAt, RateB, Delay)`. The swing replicates `{Kind, Variant, Windup,
Active, Recovery}` (`FWRSwingRep`). Each machine reads the clip's `trace` span from `anim_windows.json`: coil rate
R1 = T0 / windup (≤ 2.2, extra length skipped by starting later; < 0.6 holds the first frame), active rate R2 =
(T1 − T0) / active clamped 0.8-2.0, held through the follow-through. The light chain picks clip `Variant` (0/1/2), so
the 3-hit chain is the 3 slashes in order on every machine.

**Input buffer (`AWRCharacter`)**: `Buffered {Input, Time}`; a press that `FWRCommit` refuses is stored (last wins),
attack / item 0.33 s, dodge 0.6 s; each tick it's retried; a buffered Guard only fires if still held. Cleared by
`DropHeldInputs` (menus, build mode, taming, death: C1's isolation) and by any accepted press.

**Hit stack (`WRFxCombat.cpp`)**: tier from flags (light / heavy / counter / crit / parry / boss kill). Hit-stop:
delayed by min(active × 0.3, 0.04) (deepest point), `GlobalAnimRateScale` 0.02 on both bodies, the victim's mesh
offset by a 30 Hz shake (3 cm light → 6 cm crit) along the blow, then rate (1 + stop / 0.1) for 0.1 s (catch-up), then
1. Flash: overlay `M_WindupGlow` in white (gold for parries) on the victim's visible skeletal meshes for the tier time,
previous overlay restored. Trauma `0.04 + 0.22 w²` and FOV punch `0.2 + 1.3 w` degrees (decaying over 0.18 s) for
the people in it. Rumble `35 + 85 w` ms at strength `w`-scaled; lights use the small motor only (WRInput gains a
`bLight` flag → left motor 0). Four sounds: `hit.transient.<surface>`, `hit.body.<weight>`, material
(`hit.flesh` / `hit.armor` / `hit.dust` / `hit.ice` / `hit.magic`), and the creature's `hurt` voice (its species) on
its first hit in 0.6 s. Duck: heavy 2 dB, counter / parry 3 dB, crit / boss kill 4 dB (sounds.json `duck` on the
body layer events). Surfaces from `fx.json impacts`.

**Prompt diet (`WRFxHud.cpp`, `WRUIHud.cpp`)**: no words for parried / blocked / dodged / crit / riposte / weak /
shatter (the numbers stay, weak and crit by colour); state words GUARD BROKEN (on a guard break you cause or take)
and STANCE BROKEN (on a break you cause) as the centre notice, once per event. Damage numbers: `FWRKeyBindings
DamageNumbers` (Settings → Controls, default off), drawn beside the target's bar (offset right of the body, never
over the head). Crit prompt: when the local player can crit a target of a species not yet seen, the HUD's one prompt
slot shows "[Light] Critical" with that species; on the crit the species is saved (`[Wildreach.Prompts]
CritSeen=...`) and it never shows again.

### 3.3 Stamina, load, roll (C3)
- Attribute clamp: Stamina ∈ [graceFloor, Max]. `CheckCost` = stamina > 0. A refused press (locally) stamps
  `StaminaRefusedAt`; the HUD stamina row flashes white for 0.35 s and shakes ±3 px (sin 40 Hz, decaying).
- Regen: 45/s after 0.5 s; paused while guarding, sprinting (drain), attacking before the CancelWindow; deprived ×0.5.
- Jump: `CanJumpInternal` needs stamina > 0 (client + server agree), server spends 8 on `OnJumped`; server applies
  `State.JumpLegsInvuln` 0.8 s; `ApplyHit`: a `bLow` hit on a target with the tag is Dodged.
- Equip load (server, `UWRInventoryComponent::RefreshEquipLoad` on equip / unequip / load): worn armour + weapon +
  ranged + carried shield kg; max = `defense.maxEquipLoad` + path / perk stat `equipLoad`. Replicated
  `EquipLoadPct` (uint8, COND_OwnerOnly) on the character; the inventory screen shows "Equip load 14 / 60 kg · Light
  roll" under the armour.
- Dodge: tier from the load ratio (server and owner both have it). Stick at rest → backstep. Roll root-motion force
  over `travel`, i-frames [iStart, iEnd] (server), ends at iEnd + recovery. Overloaded: slow step, no i-frames. Light
  step node shortens travel and frames together (existing stat). Perfect dodge: server records `DodgeStartAt`; a hit
  absorbed by i-frames within 0.20 s of it sends `Event.PerfectDodge` (gameplay event) and Frozen moment uses it.
  Ledge guard: at start, probe the path every 40 cm for ground within 200 cm below; at the first missing ground,
  schedule a check at the time the roll reaches it: if the stick no longer points along the roll (dot < 0.7), end
  the root-motion force there (the roll stops at the edge).
- Backstep visual: the roll clip played from 0 at 1.6× over the backstep (a short crouched hop); a real backstep
  clip is listed as not built.

### 3.4 Guard, parry, crits, rewind (C4)
- Guard ability (`UWRGA_Block`, kept name): press → State.Blocking at once; parry window opens at startup for active ×
  (spam shrink if the last press was < 0.5 s ago); release before 0.30 s → tap: State.ParryRecovery for the off-hand's
  recovery (Blocking removed at release, nothing else starts until it ends); release later → guard drops, no penalty.
- Blocked hit (in front, 70°, melee, not unblockable): stamina −= StaminaDamage × (1 − GB/100) × Bastion; if that would
  go below 0 → GuardBroken: stamina 0, Staggered 1.2 s, `Status.GuardBroken` 1.0 s (players: riposte-able), full damage
  × (1 − negation); else chip = damage × (1 − negation for its element) and block stun 0.20 / 0.35 s
  (`State.BlockStun`), then `State.GuardCounterWindow` 0.6 s (server applies both; tags replicate).
- Gold edge: while State.Blocking, `ShieldMesh` (or the sword when no shield shows) wears the `M_WindupGlow` overlay in
  gold at 0.22 opacity, on every machine (the tag replicates).
- Parry (`bParryable` hit, target in its parry window, in front): attacker → `Status.StanceBroken`-path: parry count +1;
  count reaches the tier's need → crit open (StanceBroken for `parryOpenSeconds` 1.6 s, Staggered, the parried
  recoil); else Staggered 0.6 s (combo interrupted) + 25% max stance. `Status.Exposed` 2.0 s (×1.5 damage from
  everyone). Searing parry and the tongs rule (boss staggered `grabParryStagger`) stay; the tongs parry counts toward
  the 3.
- Crit ability `UWRGA_Critical` (granted to players; the Light press activates it when `FWRCrit::Find(Attacker)`
  returns a target): kinds `Riposte` (StanceBroken / GuardBroken target, front 60°, within stanceCritRange + radii),
  `Backstab` (rear 90° / PvP 75°, ≤ 120 cm + radii, radius ≤ 70, half height ≤ 110, not in State.Windup; big creatures
  only while StanceBroken). Server re-runs `Find` on its own (rewound) positions with the lag tolerance; mismatch →
  the ability ends (the client's swing becomes a plain light).
  - Warp: `ApplyRootMotionMoveToForce` to the sync point (target centre + facing × (r + 110 cm)), 0.12 s.
  - Attacker State.Invulnerable from start to the damage frame (server). Target: `Status.BeingCrit` (blocks abilities
    and AI: added to the shared blocked list) until damage + 0.3 s.
  - Clips: riposte = the heavy thrust `A_SwordAndShieldAttack` warped so its trace lands at 0.9 s; backstab =
    `A_SwordAndShieldAttack2` at 0.75 s; the victim plays its hit clip at the damage frame (hitstop 0.20).
  - Damage: WeaponDamage(light) × class crit multiplier (riposte 3.45 / backstab 2.30; stance crit 3.0) through
    `ApplyHit` with `bCrit` + `bUnblockable`; ends StanceBroken; PvP ×0.7 and the victim gets 0.3 s rise i-frames.
- **Rewind (`UWRLagSubsystem`, new, server)**: every character's capsule (location, half height, radius) sampled at
  30 Hz for 1 s. `SeenDelay(PlayerController)` = min(RTT/2 + interp 0.1, 0.25).
  - Player melee sweeps test each hostile within reach + 300 against its capsule rewound by the attacker's delay
    (segment-vs-capsule distance), not the present.
  - Defence history: Blocking / Parrying / Invulnerable / JumpLegs intervals per character, a remote player's start and
    end back-dated by its RTT/2 (the press happened that long before the server learned of it).
  - Hits on a remote player from creatures, bosses and other players' projectiles: if the target isn't defended now,
    the hit waits `min(RTT + interp, 0.25)` s, then resolves with the defence judged at the time the player saw the
    blow (`T + RTT/2 + interp`), using the history. `ApplyHit` gains an optional `OnResolved` callback; callers that act
    on the result (boss tongs / Skaldrith freeze, creature knockback) pass it.
  - Crits: `FWRCrit::Find` on the server uses positions rewound by the attacker's delay.

### 3.5 Stance and poise (C5)
- Attributes `Stance`, `MaxStance` (replicated). Creatures: MaxStance from `creatures.json stance` or the tier
  (`stance.tiers`), bosses major 140 / mini 90 with overrides. Players keep Poise (MaxPoise = Σ armour poise + Tempered
  + gems; `player.poise` 0).
- Stance damage per hit (rows; spells damage × 0.3; creature / tame hits damage × 0.3; shatter ×2); boss scaling by
  participants `group[n−1]`. `UWRStatusComponent`: `SinceStanceDamage`; regen after MaxStance / 13 s at 13/s (× 0.5
  below 50% HP); break at 0 → `Status.StanceBroken` for the tier seconds + Staggered (AI stops) + stance refilled when
  it ends; multicast `MulticastStanceBreak` → every machine: the "hit" clip played and held at its deepest frame
  (rate 0.6, hold) + a slump (head down 8°, sink 6% of half height over 0.3 s), `stance.break` sound + fx, the gold
  `crit.mark` attached to the head bone (searched by name) until the break ends or the crit lands, STANCE BROKEN notice
  for the player who broke it.
- Poise (players and creatures): poise damage subtracts; at ≤ 0 → reaction: players by the hit's Impact (lock times
  0.25 / 0.55 / 0.85 via Staggered of that length + the hit clip), creatures a flinch (0.45 s interrupt) unless they
  have hyperarmor on an attack (State.Attacking and `hyperarmor` / boss). Poise resets to full 30 s after the last
  poise hit and at once after a reaction. Hyperarmor window on the player's heavy (State.Hyperarmor): effective poise
  ×1.5 and damage taken −7.5%. HA breakers: `bGrab`, `bIgnoreHyperarmor`, Frozen / Shatter (status procs cancel
  abilities already), stance break.
- Creature riposte: the old "any melee on Staggered ×3" goes; Staggered still takes the `vsStaggered` node bonus.
- Phase gate: `AWRBoss::OnDamageTaken` skips SetPhase while StanceBroken / BeingCrit; `Tick` applies the pending phase
  when it ends.
- 1-HP rule: verified no floors (section 1); the crit and stance code add none.

### 3.6 Edge cases decided
- Swinging out of a held guard: the guard drops for the swing (kept), comes back if held.
- A buffered dodge during a swing fires at the CancelWindow; a buffered light during a roll fires after it.
- Being staggered / crit / frozen cancels the swing (existing CancelAllAbilities) and clears the windows.
- Two players hitting one boss: stance scaling by participants; the crit consumes the break (first crit wins, the
  server's `BeingCrit` refuses the second).
- Crit on a target that dies of something else mid-crit: the ability ends at the damage frame (target invalid).
- PvP crits obey `PlayersCanFight` (ApplyHit refuses non-hostile).
- A dedicated server never plays clips: windows are timers, so timing is identical there.
- Pad and mouse: no new buttons (Light / Heavy / Guard / Dodge as C1 mapped); the crit is Light; the prompt shows the
  device's key (`WRInput::ActionKey`).

### 3.7 Build steps
1. Data: weapons.json, tuning blocks, creatures / items fields, fx recipes, sounds + OGG layers, credits.
2. Combat types + WRWeapons + attributes (Stance) + tags.
3. Windows / commit rules / CMC root flag / melee rewrite / chain / step-in / swing rep + clip warp.
4. Input buffer + stamina refusal + grace + regen rules + jump.
5. Equip load + dodge rewrite + backstep + perfect dodge + ledge guard + inventory line.
6. Guard rewrite + ApplyHit rewrite (guard, parry, chip, stun, counter window, exposed, stance, poise, HA, low).
7. Lag subsystem (samples, history, rewound sweep, deferral + callbacks).
8. Crit ability + FWRCrit + prompt.
9. Stance break presentation + boss phase gate + creature hyperarmor / flinch.
10. Hit stack rewrite + prompt diet + damage numbers option + gold guard edge.
11. Compile, fix warnings, spot-check against this plan, CHANGELOG + HANDOFF, commit, push, LANDED line.

---

## 4. Spot-check against the plan and THE BAR (read through, fixed before push)

- Found and fixed while reading: a guard held through a swing expired from the buffer after 0.33 s (now waits while
  held); creature specials ignored a flinch (now a broken poise cuts them, hyperarmor bodies swing through); bosses could
  be flinched mid-attack (now hyperarmor from tell to blow); the parry gold flash went on the parrier (now the parried
  attacker, the victim row); every hit played the player flinch clip (now only when poise breaks, by impact level);
  the clips' own window notifies would have fought the move-row windows (now off unless combat.windowsFromClips is 1).
- Unity build checked (-DisableAdaptiveUnity): clean, so packaging will not trip on joined files.
- Colour law: gold only on the guard edge, the parry flash and the weak-point mark; the refused-stamina flash is white.

## 5. Later (outside this lane, logged in wildreach-night/later-cooks.md)

Knockdown / launch / get-up and wake-roll clips (impact 4-5); a backstep clip; creature stance-break (kneel / slump)
clips and paired crit victim clips; shields and other off-hands as craftable items; PvP defender-lag waiting; breakable
parts and head daze (C10); the rest of the attack set and weapon classes (C6).

## 6. Requirement ledger: done and where (Source/Wildreach unless a Data/ path)

| # | Done | Where |
|---|---|---|
| R1 | Done | Section 1 of this doc |
| R2 | Done | Reaction or shudder: WRCombatLibrary.cpp:647 (poise), WRCharacter.cpp:1134, WRCreature OnHitVisual (ReactionOf), shake WRFxCombat.cpp:252 |
| R3 | Done | WRFxCombat.cpp:383 Flash, tiers tuning fx.flash; gold parry rim in ShowHit |
| R4 | Done | Impact by creature surface (fx.json impacts) + element burst, WRFxCombat.cpp:69 |
| R5 | Done | ShowHit Feel (trauma), WRFxCombat.cpp:510 ModifyFov |
| R6 | Done | ShowHit Feel, WRPadInput.cpp RumbleLight (small motors on lights) |
| R7 | Done | WRFxCombat.cpp:164 (transient, body, material) + WRAudioDirector OnHitShown (voice); sounds.json hit.transient.*, hit.body.* |
| R8 | Done | sounds.json duck 0.2 / 0.29 / 0.37 (2 / 3 / 4 dB) on body / counter / crit / boss kill / breaks |
| R9 | Done | tuning fx.hitStop tiers; WRFxCombat.cpp:252 caps at 0.3 s |
| R10 | Done | WRFxCombat.cpp:252 (deep delay), :298 (creep 2%, catch-up 0.1 s, victim shake 30 Hz); GlobalAnimRateScale only |
| R11 | Done | Clip warp WRCharacter.cpp:1087 (max coil 2.2x, follow-through 1.0x), WRAnimInstance.cpp:1120; coil floor WRWeapons.cpp:91 |
| R12 | Done | WRAbilities.cpp:341 StepTarget, step-in root motion in UWRGA_Melee::ActivateAbility |
| R13 | Done | WRAbilities.cpp:508 root / slow; WRCharacterMovementComponent.cpp:48 |
| R14 | Done | WRAbilities.cpp:381 SetWindows (tags on server + owner), WRCharacterBase.cpp:284 SetCombatTag |
| R15 | Done | Data/weapons.json sword moves; WRAbilities.cpp:320 PickMove |
| R16 | Done | WRCombatRules.cpp:16 Allows; UWRGameplayAbility::CanActivateAbility |
| R17 | Done | WRCharacter.cpp:1223-1299 (buffer), :1691 (cleared on mode changes) |
| R18 | Done | tuning player.staminaRegenPerSecond 45 / staminaRegenDelay 0.5 |
| R19 | Done | WRStatusComponent.cpp:282 (mid-swing), guard and sprint as before |
| R20 | Done | tuning stamina; jump WRCharacter.cpp:1383; backstep WRAbilities.cpp:918 |
| R21 | Done | WRAttributeSet.cpp:59 (floor -20), WRAbilities.cpp:121 (stamina > 0) |
| R22 | Done | WRAbilities.cpp:121 stamps StaminaRefusedAt; WRUIHud.cpp:836 flash + shake |
| R23 | Done | WRInventory.cpp:798 GetEquipLoadKg / GetMaxEquipLoad; replicated EquipLoadPct; WRUIInventory.cpp:1103 |
| R24 | Done | tuning defense.load; WRAbilities.cpp:910 LoadTier, :918 |
| R25 | Done | tuning defense.backstep; WRAbilities.cpp:918 |
| R26 | Done | WRCharacter.cpp:1383 (State.JumpLegsInvuln); WRCombatLibrary.cpp:397; low flags creatureAI.stomp.low, bosses.*.low |
| R27 | Done | Fires on press (unchanged); perfect dodge WRCombatLibrary.cpp:369 (Event.PerfectDodge, Frozen moment) |
| R28 | Done | Ledge guard in UWRGA_Dodge::ActivateAbility (WRAbilities.cpp:918) |
| R29 | Done | WRCombatLibrary.cpp:473 (GuardStaminaCost), Data/weapons.json guard / offhands |
| R30 | Done | WRCombatLibrary.cpp:473 chip by physical / element negation |
| R31 | Done | WRCombatLibrary.cpp:473 guard break + Status.GuardBroken; FindTarget accepts it |
| R32 | Done | Block stun WRCombatLibrary.cpp:473; window WRStatusComponent.cpp:90; guardCounter row + PickMove |
| R33 | Done | Guard up at press WRAbilities.cpp:1069; held guard buffered through swings; gold edge WRCharacter.cpp:1395 |
| R34 | Done | WRAbilities.cpp:1069 / :1108 (startup / active / recovery by off-hand, tap, spam shrink) |
| R35 | Done | WRCombatLibrary.cpp:435 (bParryable, not projectiles); flags tuning creatureAI / bosses; WRBoss.cpp:284 |
| R36 | Done | WRCombatLibrary.cpp:435 (counts, 25% stance on a non-break) |
| R37 | Done | Status.Exposed WRCombatLibrary.cpp:435 / :532 |
| R38 | Done | WRAbilities.cpp:1248 (riposte x3.45, 1.6 s, damage at 0.9, attacker i-frames) |
| R39 | Done | WRAbilities.cpp:1181 FindTarget (rear arc 90 / PvP 75, size, not winding up, big only when broken) |
| R40 | Done | WRAbilities.cpp:1248 (sync point warp 0.12 s, victim held and turned) |
| R41 | Done | WRLag.cpp (samples :208, defence history :179, held blows :291); rewound sweep WRAbilities.cpp:593; rewound crits; held blows WRCombatLibrary.cpp:337 |
| R42 | Done | Stance / MaxStance attributes (WRAttributeSet.h); players' poise WRCombatLibrary.cpp:232 RefreshPoise |
| R43 | Done | tuning stance.tiers / breakSeconds; creatures.json vulkarn 180 / skaldrith 160; WRCombatLibrary.cpp:259 |
| R44 | Done | WRStatusComponent.cpp:322 |
| R45 | Done | WRCombatLibrary.cpp:285 BreakStance; look WRCreature.cpp:955 / :1003 (held recoil, slump, gold crit.mark); tuning crit.stanceCritRange |
| R46 | Done | WRCombatLibrary.cpp:706 (stance.group, participants incl. tames) |
| R47 | Done | No floors (section 1; WRCombatLibrary.cpp:639) |
| R48 | Done | WRBoss.cpp:948 CheckPhase (held during StanceBroken / BeingCrit) |
| R49 | Done | WRStatusComponent.cpp:313 (30 s), ClearStatus Staggered (at once) |
| R50 | Done | State.Hyperarmor from the heavy row; WRCombatLibrary.cpp:532 (damage) / :647 (poise x1.5) |
| R51 | Done | bGrab / bIgnoreHyperarmor skip it; Frozen and status procs cancel abilities; a stance break staggers |
| R52 | Done | creatures.json hyperarmor; WRCombatLibrary.cpp:647; specials WRCreature.cpp:2327; bosses WRBoss.cpp:1180 |
| R53 | Done | tuning poise.reactions; WRCombatLibrary.cpp:647; WRCharacter.cpp:1134 (clip by impact); shudder = hit-stop shake |
| R54 | Done | WRUIHud.cpp:687 (the one prompt slot), WRCharacter.cpp:1339 (once per species, saved) |
| R55 | Done | WRFxHud.cpp:43 (no hit words), WRFxCombat.cpp:183 (state words) |
| R56 | Done | WRFxHud.cpp:155 (beside the body), WRFxCombat.cpp:212 + WRUISettings.cpp:883 (off by default, toggle) |
| R57 | Done | Gold only: guard edge, parry flash, crit.mark (Data/fx.json); white refusal flash |
| R58 | Done | All blows through ApplyHit; numbers in Data/weapons.json + tuning; UWRGA_Critical is a UWRGameplayAbility; fx and sounds are rows |
| R59 | Done | Mouse and pad drive the same actions (Light / Heavy / Guard / Dodge; prompts by device); server authority and replication throughout |
| R60 | Done | Not-finished items listed in the CHANGELOG entry and section 5 |
| R61 | Done | CHANGELOG.md Unreleased, Docs/HANDOFF.md, Docs/plan/DATA_SCHEMAS.md (weapons.json) |
