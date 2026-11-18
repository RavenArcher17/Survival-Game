# Wildreach build lane: COMBAT FULL (C6 to C12)

Lane `build-combat-full`, 2026-11-14. Builds on `cooks/combat.md` (the combat design), `cooks/build-combat-core.md`
(C0, C2-C5, landed acda516) and COMBAT-AND-LESSONS (CAL). Scope: C6 full attack sets per weapon class, C7 every status
plus element reactions, C8 Wildbrands and spells, C9 every creature's attack rows with real tells and group fighting,
C10 bosses and minis (incl. the third minis Cinderhorn and Old Tusker), C11 lock-on / camera and flasks, mounted
combat, C12 PvP. Gate: compile only. Aaron and the testers test (F8).

---

## 0. Requirement ledger

Every line of the brief and every CAL / combat.md row it names, one requirement per row. Column "Done / where" is filled
in section 6 at the end.

| # | Requirement | Source |
|---|---|---|
| R1 | Full attack set per weapon class: 9 classes (fists, dagger, sword, axe, pick-hammer, spear, greatsword, great hammer, scythe), each with its row table | Brief C6, combat.md 2.4.1-2.4.2 |
| R2 | Charged heavy: hold Heavy ≥ 0.35 s, full at 0.80 s, auto-release 1.0 s; damage 1.0 → charged value linear; windup after release | combat.md 2.4.2, CAL 1.1 |
| R3 | Chain heavy: Heavy inside a light's combo window = ×1.1 damage, ×1.2 stance at heavy cost | combat.md 2.4.0 |
| R4 | Running light / heavy (while sprinting) | CAL 1.1 |
| R5 | Rolling light (≤ 0.30 s after a roll ends), backstep light (≤ 0.30 s after a backstep) | CAL 1.1, combat.md 2.6 |
| R6 | Jump light / heavy in the air; active until landing + 0.10, recovery on landing; jump heavy HA | CAL 1.1 |
| R7 | Crouch light + stealth bonus (+20% stance and buildup on an unaware target) | CAL 1.1 |
| R8 | Counter-hit: +15% damage, +30% stance on a target in its wind-up; pierce +30% damage | CAL 1.1, combat.md 2.4.0 |
| R9 | Physical types slash / strike / pierce per move; creature `physResist`; head-strike stance ×1.5 | combat.md 2.4.0, 2.10 |
| R10 | Two-handing toggle (pad D-pad down hold / KB V hold): +15% damage, stance +30% lights / +10% heavies, guard from the weapon, shield off the arm; great weapons always 2H | combat.md 2.4.0 |
| R11 | Spear guard poke: Light while guarding with a shield keeps the guard up | combat.md 2.4.2 |
| R12 | Per-class stamina, poise vs players, guard stamina, crit multipliers, reach, trace radius, hyperarmor, cancel fraction (great 0.75) | combat.md 2.4.1-2.4.3 |
| R13 | Weapons of every class reachable: craftable items with a class, held models | combat.md 2.4.1, CAL B7 |
| R14 | Hyperarmor ×2 charged, ×2.5 great; great weapons damage taken −17.5% | CAL 1.5, combat.md 2.7 |
| R15 | Status shared rules: thresholds by tier (100/120/150/250/400, players 100 + resist), ×1.3 growth per proc (reset 30 s after combat), decay 8/s after 3 s, stealth +20%, PvP ×0.7, boss ×1.25 per extra participant | Brief C7, combat.md 2.11 |
| R16 | Burn (keep) and Frostbite to the new numbers: burst 8% + 20 (bosses 4% + 20), +20% damage taken, slow, stamina regen ×0.6, 12 s, no re-proc while active | combat.md 2.11 |
| R17 | Frozen 2.5 s with mash-out for players (−0.15 s per press, min 1.2 s); minis and majors get Rimed (stance taken ×1.5 for 4 s) instead | combat.md 2.11 |
| R18 | Bleed (12% + 15 burst, bosses ×0.7), poison (0.4% + 2 /s for 30 s), rot (0.8% + 4 /s 20 s, healing ×0.5) | Brief C7, combat.md 2.11 |
| R19 | Path statuses: shock (stagger 0.4 s + chain 25% to 2 targets in 4 m; players −20 stamina), soak (slow ×0.8 10 s, fire −50%, shock +50%), blind (AI memory loss 3 s / bosses 1.5 s, players vignette + lock off), decay (max HP −10%, healing ×0.5 20 s), mark (+15% taken 10 s), curse (−25% resource, −15% damage dealt 15 s) | Brief C7, combat.md 2.11 |
| R20 | Fire ends frostbite; a proc breaks hyperarmor; every status can kill | CAL 1.12, combat.md 2.11 |
| R21 | Element reactions: fire then frost = steam burst; frost then fire on Frozen = Thaw; lightning on soaked; frost on soaked; reaction needs two elements from two hits within 4 s; non-owners at 50% | Brief, FULL-GAME 4.2, combat.md 2.11 |
| R22 | Status meters under enemy health bars (status colours) and above the player's stamina bar | combat.md 2.11 |
| R23 | Status cures as drinkable items (Ember salve, Warming draught, Bloodmoss poultice, Grounding tonic, Clear-eye drops, Hex-ward charm, antidote for poison and rot) | combat.md 2.16 |
| R24 | Wildbrands: fits per class (greys out with the reason), resource mode (Heat / Chill gained), stamina 18, frames from data, HA or i-frames from data | Brief C8, combat.md 2.12 |
| R25 | Wildbrand charge (Molten cleave hold to 1.0 s: ×2.6, lava ×1.3) and follow-ups (Molten edge second edge ≤ 0.5 s; Flurry second flurry ≤ 0.4 s) | combat.md 2.12 |
| R26 | Spells: cast while walking ×0.5 for bolts / lances / pools, rooted for walls / novas / Meteor / Blizzard; chargeable spells ×1.5 at 1.0 s; enemy spells never home | combat.md 2.13 |
| R27 | All Fire and Frost nodes built (none show "[not built yet]") | CAL C8 |
| R28 | Every creature's attack rows: windup, track, delay hold with final cue 0.2 s before contact, active, punish, damage, poise / impact, flags, buildup, phys type, combo next, readsAction, cooldown, tell | Brief C9, combat.md 2.18.1-2.18.2 |
| R29 | Real tells: a rising sound cue on every windup > 0.5 s; a glint (white blockable / crimson unblockable) on the attacking part; no floor paint | CAL 1.13, combat.md 2.17.1b |
| R30 | Off-screen rule: a creature starts a windup only while on its target's screen or audible within 8 m | CAL 1.13 |
| R31 | readsAction (highland and bosses): answers a drink / cast / charged heavy start within 0.4 s | CAL 1.13 |
| R32 | Enemy groups flank (90° / 150° at 4-7 m), surround (4+ keep one on the far side), breathe (back off 5-8 m 1-2.5 s after a combo), retreat (flee < 25%, pack loss < 50%, regroup 30 m and return); no attack token | Brief, combat.md 2.18.4 |
| R33 | Pulling: a ranged hit alerts packmates within 6 m that see it | combat.md 2.18.4 |
| R34 | Stomps and slams: a visible shockwave travelling at 1200 cm/s, damage rides the front, `low` | combat.md 2.17.1b |
| R35 | Projectiles: ≤ 1000 cm/s in Fire / Frost, wind-up + travel ≥ 0.9 s, never home, nothing fired inside 4 m | CAL 1.9, 1.13 |
| R36 | Bosses: rows for every Vulkarn and Skaldrith move (combat.md 2.17.2-3) with windup / hold / active / punish / impact / flags | Brief C10 |
| R37 | Transition at 50% = an unblockable telegraphed attack (Forge roar, Take-off gust), boss takes 50% during it, never during a crit / break | combat.md 2.17.1 |
| R38 | Phase 2 changes timing: new delayed attacks, a new combo, a delayed variant, shorter idle gaps (0.3-0.9 vs 0.6-1.4) | combat.md 2.17.1 |
| R39 | Attack choice by range band, never the same move 3 times running, punish always plays, projectile cap 1 / 2 in flight | combat.md 2.17.1 |
| R40 | Visible contact: boss and creature damage only from the attacking part's reach at the strike, shape only as pick test; small cues not full-area decals; ground marks only for real ground effects | combat.md 2.17.1b |
| R41 | Breakable parts: Vulkarn hammer arm, Skaldrith wings, Warden shell, Cinderhorn horn, Old Tusker tusks; part stagger 1.2 s, 30% body stance, drop once, on-break move change | combat.md 2.10 |
| R42 | Lock points on bosses and big creatures (head, chest, parts); flick switches between them; aim at the point's height | combat.md 2.15 |
| R43 | Fire / Frost minis to the bar (stance 90, 2 parries, flags, punish, lock points) | combat.md 2.17.4 |
| R44 | Cinderhorn (roaming, ash plains): charge / horn hook / buck / ash stomp, pillar stun 4 s, horn part, frost stops the trail | Brief, combat.md 2.17.4 |
| R45 | Old Tusker (roaming, snow flats): tusk sweep / trample / rear-and-crash / snow fling / herd bellow, tusk parts, burn sends him to water | Brief, combat.md 2.17.4 |
| R46 | Tame-punish move per major (Vulkarn slag throw, Skaldrith ice spikes target the furthest tame first) | combat.md 2.17.1 |
| R47 | First kill: named drop, visible, flies to the killer within 6 s or at 4 m | combat.md 2.17.1, CAL L14 |
| R48 | Run-back shrine-stones 40-60 m from each arena door | combat.md 2.17.1 |
| R49 | Boss health bar bottom centre: name, bar, phase pip, no stance bar | combat.md 2.17.1 |
| R50 | Camera: one spring arm colliding only with walls / cliffs / big rocks (second-largest extent ≥ 160 cm), pull in ≤ 0.12 s, hold 0.15 s, ease out 0.4 s, hide the player under 100 cm | Brief C11, CAL 1.15 |
| R51 | Giant framing: target taller than 3× the player → boom +150 cm, pitch +10° over 0.4 s | CAL 1.15 |
| R52 | Flick switching: right stick (built) and mouse flick ≥ 300 px/s; 0.25 s re-arm | CAL 1.15 |
| R53 | Flask: drink 0.9 s committed, heal at 0.5 s, walk ×0.4, shared cooldown 1.5 s, stagger cancels and consumes; AI reads the drink | Brief C11, CAL 1.10 |
| R54 | Mounted: rider light (side the camera faces) / heavy (rear-up), counted as running attacks; couched spear charge above 900 cm/s; no guard / parry / unflagged Wildbrands mounted; bolt / lance spells only; unhorsed on impact ≥ 4 or mount stagger (1.2 s knockdown); mount takes the rider's lock target; dismount | Brief, combat.md 2.19 |
| R55 | PvP: global ×0.6, per-class multipliers, Wildbrand / spell ×0.85, status buildup ×0.7, crits ×0.7 and 75° (kept), rise i-frames 0.3 (kept), tames ×0.5 vs players | Brief C12, combat.md 2.20 |
| R56 | PvP combat tag 15 s: no fast travel, no wake-point swap, no clan storage; logout while tagged leaves the body 30 s, killable | combat.md 2.20 |
| R57 | PvP defender-lag judging (left open by build-combat-core) | LATER-COOKS 11-14 |
| R58 | Everything data-driven (weapons.json, attacks.json, tuning blocks), abilities as UWRGameplayAbility + ClassForNode, damage only through ApplyHit, sounds / fx as table rows, server authority, replication | Brief |
| R59 | Mouse and pad on every screen / input; edge cases; multiplayer | Brief |
| R60 | No placeholders shown as finished; unfinished stays hidden and is listed | Brief |
| R61 | Every object has intent: why it's here, what it connects to, how it meets what it touches (pillars for Cinderhorn, the Tusker's water, shrine-stones) | Brief (Aaron 11-14) |
| R62 | CHANGELOG Unreleased + HANDOFF in the same commit; LFS size reported; LANDED line | Brief |

---

## 1. Research: how the best games solve these exact things

### 1.1 Weapon classes and contextual attacks (R1-R14)
- **Elden Ring** (FromSoftware). Every class has the same input grammar and a different body: R1 chain (dagger 5, straight
  sword 3, colossal 2), R2, charged R2 (hold, the charge glows on the weapon at full), running R1/R2 (step-in lunge),
  rolling R1 (fast poke out of a roll, the roll-catch tool), backstep R1, jumping R1/R2 (jump R2 is the stance tool:
  ~2× R2 stance), crouch R1 (low poke), guard counter. Great weapons and colossal weapons have hyperarmor on most
  swings; colossal two-hand R1s trade through light hits. Spear + shield: R1 while guarding thrusts past the shield
  (the "turtle poke") and the guard stays up. **What makes it work:** one grammar the player learns once, so a new
  weapon class is a new rhythm, not new controls. We take the grammar exactly and per-class rows.
- **Monster Hunter World / Rise** (Capcom). Hammer: the charge has 3 levels you can hold and walk with; a charged
  hammer to the head stuns (KO); blunt types do "KO" (stance) damage to heads only. Great sword: charged slash with
  three visible charge flashes (true charge). Cutting vs blunt vs shot: hitzones resist each type separately (a shell
  takes less cut, more blunt). **What makes it work:** the body tells you what the weapon does (blunt breaks heads,
  cuts sever tails). We take: strike to a head lock point = ×1.5 stance; per-creature `physResist` by body (shell,
  fur, scale); a charged heavy that shows its charge on the weapon (a growing glint at the full mark).
- **Dark Souls 3**: two-handing toggles by holding Y (Triangle); a 2H weapon gets more stance and the shield goes to the
  back. A hold on a button that taps for something else is its own verb (hold vs tap), not a chord. We map two-handing
  to D-pad down hold (tap cycles the belt), V hold on keyboard.
- **For Honor / Sekiro**: a counter-hit (hitting during the enemy's wind-up) does more and interrupts; it's the reward
  for reading the tell. ER gives pierce +30% on counter-hits. We take counter-hit +15% damage / +30% stance, pierce
  +30% damage.
- **Real world (HEMA, Fiore / Meyer)**: thrusts are fast and reach; cuts sweep wide; a pollaxe / hammer strike is slow
  and delivers through armour (blunt trauma). That gives the type table: sword lights slash, heavy thrust (pierce);
  axe slash; pick-hammer strike (the hammer face) and pierce (the beak); spear pierce; great hammer strike; scythe
  slash with a hooking pull.

### 1.2 Status and reactions (R15-R23)
- **Elden Ring status buildup**: hidden meter per status, proc at resistance, resistance goes up after each proc
  (Malenia's bleed 421, 519, 809, 1266), meter decays when not hit. Hemorrhage = % max HP burst; poison long weak DoT;
  scarlet rot strong DoT; frostbite burst + damage taken + slower stamina regen; madness / death blight special.
  Player meters show above the bars only while they fill. **What works:** the burst is the payoff you see; the rising
  threshold stops perma-proc. We take it all (bosses ×0.7 on bleed).
- **Genshin Impact** (miHoYo) elemental reactions: an element "aura" sits on a target for a few seconds; a second
  different element consumes it and triggers a reaction (Melt = pyro + cryo ×1.5/×2; Vaporize = hydro + pyro;
  Superconduct; Electro-Charged on wet; Frozen = hydro + cryo). The *order* matters (pyro on cryo ≠ cryo on pyro).
  **What works:** order-sensitive pairs and a visible aura window. Our brief names "fire then frost = steam burst": an
  ordered pair. We take: an aura from the last element hit (4 s), a reaction when a different element lands inside
  it, the reaction consumes the aura.
- **Divinity: Original Sin 2** (Larian): fire on a frozen / wet target makes steam clouds that blind; electricity on
  wet stuns; surfaces matter. We take steam as a blinding cloud: Steam burst = a scalding burst (damage + soak) and a
  short blind on everything inside 3 m (the R5 blind status), which reads in the world as a white jet of steam.
- **Breath of the Wild / Tears of the Kingdom**: fire melts ice and thaws frozen enemies, frozen enemies take ×3 on
  the shattering blow (ice break). Lightning on wet shocks. We keep Raven's Thaw and Shatter and add soak + shock.
- **Real world**: water on hot metal / rock flashes to steam with ~1700× volume (the "steam explosion" of a quench in
  a smithy); steam scalds at 100 °C+ and blinds. Frostbite: skin tissue freezes, then thaw is worse (pain, swelling)
  — fire ending frostbite matches thawing.

### 1.3 Wildbrands and spells (R24-R27)
- **Elden Ring Ashes of War** (reference only; our name is Wildbrand): each skill has its own frames, hyperarmor or
  i-frames, FP cost, and many have a follow-up (Unsheathe's second R1/R2), or a hold to charge (Lion's Claw, Prelate's
  charge). Skills only mount on weapon classes they fit; a skill that doesn't fit greys out. Spells: casting while
  walking (light incantations), rooted heavy casts (Comet Azur); many spells charge (Glintstone Pebble doesn't; Rock
  Sling does). **What works:** the frames are the skill's identity. We take follow-ups, charge, fits, frames.
- **Monster Hunter Rise silkbind skills**: a skill gauge, short commitment, each skill fits one weapon type only.
  Lesson: show *why* a skill can't be used (the slot icon greys and says "needs a spear").

### 1.4 Creature attacks, tells, groups (R28-R35)
- **Elden Ring enemy design**: long readable wind-ups; held swings (Margit's staff delay); combos whose last hit is
  delayed or further; recovery after the combo = the punish window; enemies answer heals (Zullie's "input reading"
  analysis: the AI checks the player's animation start). Wolves in ER (Limgrave packs) circle at mid range, one
  lunges from the flank while another holds your front; they back off after a bite.
- **Ghost of Tsushima** (Sucker Punch): enemy telegraphs are a blue glint (parryable) and a red glint (unblockable,
  dodge). A small glint on the weapon at the start of the swing, not floor paint. Enemies attack "freely" but take
  turns by positioning (they hang at the edge of the circle until a gap opens).
- **Sekiro**: the red kanji "perilous attack" = unblockable (sweep, thrust, grab); sound "ting" on the tell.
- **Horizon Zero Dawn / Forbidden West** (Guerrilla): machines telegraph with a body pose plus a sound sting (the
  Sawtooth's crouch-and-growl before a pounce; Thunderjaw's tail sweep has a hydraulic hiss). Packs of Watchers
  flank; a Sawtooth retreats into tall grass and re-engages. Parts break off (Thunderjaw's disc launcher) and change
  the fight.
- **Monster Hunter**: monsters have "turf" patterns: bait-and-punish rhythm; a herbivore herd (Aptonoth) flees,
  a Bulldrome charges in a straight line and is stunned if it hits a wall; KO and part breaks.
- **Real animal behaviour** (references used for the rows): African wild dogs (Cinderfang inspiration) hunt by
  relay — one harasses the front, others bite flanks and hindquarters; wolves (Gloomhowl) test from the rear and
  break off; bison / musk oxen charge in a straight line and stop to recover, a herd bull bellows to call; big cats
  (Rimeclaw) stalk, pause (the "delay hold" of a crouched cat with twitching tail) then explode; elephants rear and
  crash (Old Tusker), toss with tusks, and a hot elephant goes to water to cool off (the burn → water behaviour).
- **Audio**: humans react ~40 ms faster to sound than sight (CAL 1.13). Film tell sounds rise in pitch toward impact
  (Shepard-like riser). We build the tell as a rising cue played at windup start, and a short final sting 0.2 s before
  a delayed strike.

### 1.5 Bosses (R36-R49)
- **Elden Ring**: Margit's held staff and delayed daggers (phase 2 adds the golden hammer combo), Radahn's phase
  transition is itself an attack (the gravity pull and meteor), Malenia's phase 2 starts from a non-crit hit and
  begins with Scarlet Aeonia. Stakes of Marika put a respawn by the boss fog. Boss bar bottom centre with name.
  First kill drops the remembrance with a named card.
- **Monster Hunter**: part breaks (horns, tusks, wings) each drop a material once and change moves (a broken horn
  can't gore as far). Wall stun after a charge into a wall.
- **Dark Souls 1 Taurus Demon / Bed of Chaos**: the arena is part of the fight (pillars, ledges). Cinderhorn's pillars
  and Old Tusker's water are that.
- **God of War (2018)**: the boss's tells are glints on the weapon (yellow = blockable, red = unblockable) — the
  colour law our cook already uses (white blockable / crimson unblockable).

### 1.6 Lock-on and camera (R50-R52)
- **Elden Ring / DS3**: lock-on to the target nearest screen centre, flick the right stick to switch, big bosses have
  several lock points (dragons: head, chest, tail). The camera pitches up on giants. The camera collides only with
  real walls; grass and small props are ignored (the camera passes through branches).
- **The Last of Us Part II / God of War (2018)**: camera collision uses a dedicated channel; small props are marked
  not to block the camera; the spring arm eases out after a short hold to avoid "breathing" against walls, and hides
  the player when the camera gets inside 1 m.
- **Monster Hunter World**: the "focus camera" frames large monsters by pulling the camera back.

### 1.7 Flasks (R53)
- **Elden Ring flasks**: drinking ~1.1 s committed, heal lands partway, you can walk slowly; enemies punish the drink
  (input reading). **Sekiro gourd**: same, walk slow. **Bloodborne vials**: fast (0.5 s) — the reason Bloodborne needs
  rally. We take ER's commit with Raven's crafted potions.

### 1.8 Mounted combat (R54)
- **Elden Ring Torrent**: R1 swings to the right, L1 to the left, R2 charged; no guard on horseback; heavy hits
  knock you off ("unhorsed"), lying on the ground with a wake-roll. **Mount & Blade II: Bannerlord**: couched lance
  above a speed threshold (hold, the lance lowers, damage from relative speed), side swings chosen by the camera
  direction. **Ghost of Tsushima**: horse slash in the camera direction. We take: side from the camera, rear-up heavy,
  couched spear above 900 cm/s, unhorse on big hits.

### 1.9 PvP (R55-R57)
- **Elden Ring patch 1.07**: per-weapon / skill / spell PvP multipliers; patch 1.10 hyperarmor damage reduction.
- **Rust / Conan Exiles / Dune Awakening**: a combat tag (Dune "combat mode", Conan logout body stays in the world)
  stops players escaping a fight by logging out or fast-travelling.
- **Overwatch GDC 2017 (Tim Ford)** and **Valve lag compensation**: favour the shooter with a capped rewind, favour the
  defender for defensive abilities already started. For PvP melee: the attacker's swing is judged at its rewound view
  (built); the defender's guard / roll pressed on its own screen is judged back-dated by its one-way lag.

---

## 2. Design decisions (gaps solved before building)

1. **Moves per class (R1, R12)**: `weapons.json` gets every class with a full row set: `light1..N`, `heavy`,
   `chainHeavy`, `charged`, `runLight`, `runHeavy`, `rollLight`, `backstepLight`, `jumpLight`, `jumpHeavy`,
   `crouchLight`, `guardCounter`, `guardPoke` (spear only), `riposte`, `backstab`, plus class fields `stamina`,
   `stanceLight`, `poiseLight`, `guardStamina`, `types`, `trace`, `hands`, `weight`, `great`, `pvp`, `buildup`
   (innate status, e.g. dagger bleed 10). Numbers from combat.md 2.4.1-2.4.2; stance / poise / guard stamina come from
   the class light values × the ratio table (combat.md 2.4.1 footnote), written into each row so the file reads
   plainly. Each row names the hero clip it plays (`clip`, variant within light / heavy sets); the hero has 3 light
   and 2 heavy sword clips today, so every class plays those warped to its own frames (not finished: per-class clips).
2. **Contextual pick (R2-R7, R11)**: `UWRGA_Melee::PickMove` reads state in this order: guard counter window → charged
   (Heavy held past 0.35 s) → airborne (jump) → sprinting (run) → within 0.30 s of a roll / backstep end (rolling /
   backstep light) → guarding with a shield and a spear (guard poke) → inside a light's combo window (chain heavy /
   next light) → plain. The owning client and the server pick the same row from the same replicated state (sprint
   flag, CMC falling, `LastDodgeEnd` stamped on both, held-input charge time sent with the activation as the held
   seconds via the ability's input-release wait).
3. **Charged heavy (R2)**: Heavy press starts `UWRGA_HeavyAttack` in a *charge* phase when the class has a `charged`
   row: the hero plays the heavy clip's coil and holds it (rate 0.05 after the coil); a release before 0.35 s plays the
   plain heavy (windup minus the time already held); a release after plays `charged` with damage
   lerp(1.0, row.dmg, (held − 0.35)/(0.80 − 0.35)); auto-release at 1.0 s. A gold-free white glint grows on the weapon
   at full charge (fx row `charge.full`, the colour law: white = yours to read, not gold, because gold = block/parry and
   the UI). Stamina for the charged row is paid at release (grace rules hold). Charging is hyperarmored (CAL 1.5 "hold →
   active"). `State.Charging` is set so readsAction creatures answer it.
4. **Jump attacks (R6)**: in the air, Light / Heavy play `jumpLight` / `jumpHeavy`: windup in the air, the sweep is
   live until the CMC reports landing (cap 1.2 s) + 0.10 s, then the row's recovery starts on landing. Jump heavy
   lands with a ×1.25 radius sweep and does the row's 4× light stance.
5. **Crouch (R7)**: the game has no crouch stance or crouch clips (no sneaking stance at all). Building a crouch whose
   body stands upright would be a placeholder. The **stealth bonus** is built on its own (any attack on an unaware
   target: +20% stance, +20% buildup), and `crouchLight` rows are written in the data so the crouch cook only has to
   add the stance. Crouch itself → LATER-COOKS (needs crouch locomotion clips).
6. **Two-handing (R10)**: a replicated `bTwoHanded` on the hero (server RPC `ServerSetTwoHanded`), toggled by D-pad down
   *hold 0.5 s* (tap still cycles the belt) and `TwoHand` key (V, rebindable) held 0.5 s. While two-handed: damage
   ×1.15, stance +30% lights / +10% heavies, guard = the weapon's own (`FWRWeapons::GuardFor` honours it), the
   carried shield mesh hides from the arm (the shield-on-back socket doesn't exist on the MetaHuman hero: hidden is the
   honest choice; LATER-COOKS adds a back socket). Great weapons are always two-handed (no shield shown).
7. **Physical types and counter-hits (R8-R9)**: `FWRHit` gains `PhysType` (0 none, 1 slash, 2 strike, 3 pierce). The
   melee row's type (light rows use the class's light type, heavy rows the heavy type). `ApplyHit` multiplies damage by
   the creature's `physResist[type]` (creatures.json), counter-hit (+15% / pierce +30%, stance +30%) when the target
   has `State.Windup`, and a strike hit inside 70 cm of a `head` lock point does ×1.5 stance.
8. **Weapons reachable (R13)**: `items.json` adds copper / bronze / iron daggers, spears, greatswords, great hammers,
   scythes (with `class`), axes stay the hatchet line, picks the pick line; recipes at the workbench (copper) and forge
   (bronze / iron) with the same costs scale as swords (great weapons ×2 metal). Held meshes come from `WRToolMesh`
   (the code-modelled tool system the build-finish lane made) with new shapes per class (dagger, spear, greatsword,
   great hammer, scythe). Not finished: sculpted art.
9. **Status system (R15-R20)**: one table `status.defs` in tuning with a row per status (`burn`, `frost`, `bleed`,
   `poison`, `rot`, `shock`, `soak`, `blind`, `decay`, `mark`, `curse`): colour, proc effect numbers, seconds. Meters
   live in the status component on the server as floats per status plus the proc count (threshold growth), and
   replicate to every machine as `uint8` percent per status in a compact replicated array on the character
   (`BuildupPct[11]`, ~11 bytes, only changes replicate). Burn and frost keep their attributes for Raven's code paths;
   the new array mirrors them. `FWRHit` gains `Buildups` (status id → amount) beside Burn / Frost. Procs:
   - frostbite: burst `pct × max HP + flat` (bosses 4%), tags Frostbite + Slowed 12 s, stamina regen ×0.6 on players,
     no re-proc while active (buildup still fills toward Frozen: a proc while Frostbitten = Frozen, Raven's chain).
   - frozen: creatures 2.5 s; players 2.5 s minus 0.15 s per button press (mash, min 1.2 s); minis / majors get
     `Status.Rimed` 4 s (stance taken ×1.5) instead.
   - bleed: burst 12% + 15 (bosses ×0.7) once.
   - poison / rot: DoT ticks each second (rot also halves healing taken while active).
   - shock: stagger 0.4 s (bosses in hyperarmor: +15 stance instead), chain 25% of the proc hit to 2 others in 4 m;
     players lose 20 stamina.
   - soak: Slowed ×0.8 10 s; fire buildup ×0.5 and shock ×1.5 while soaked; standing by fire 3 s clears it.
   - blind: creatures forget their target 3 s (bosses 1.5 s) and attack the last seen spot; players get a 40% edge
     vignette and lock-on turned off 3 s.
   - decay: max HP −10% until the fight ends (creatures) / until rest (players); healing ×0.5 20 s.
   - mark: +15% damage taken 10 s.
   - curse: −25% of the path resource and −15% damage dealt 15 s.
   A proc on a creature in hyperarmor cancels its attack (breaks HA). Fire damage on a Frostbitten target ends it.
   Thresholds by tier, ×1.3 after each proc, back to base 30 s after the last hit.
10. **Reactions (R21)**: the character remembers its last element aura (element, instigator, time) for 4 s. When a
    hit of a different element lands inside it, the reaction table fires and consumes the aura:
    - **fire → frost = Steam burst**: 30 damage (scaled by 0.5 if the two hits came from the same player who doesn't
      own the Thaw combo... no: the brief's group rule — full strength when two *different* players' elements meet or
      the attacker owns the pair, 50% otherwise), soak 30 and blind 40 to everything within 300 cm, a white steam jet
      fx + hiss sound. Ends Burn on the target.
    - **frost → fire on a Frozen target = Thaw** (Raven's combo, kept: 40 damage; non-owners 50%).
    - **frost → fire (not frozen)**: ends Frostbite (the thaw rule) — no burst.
    - **lightning on soaked**: +25% damage, 30% free shock proc.
    - **frost on soaked**: frost buildup ×1.5.
    - **fire on soaked**: fire buildup ×0.5.
    Lightning, tide etc. hits don't exist yet in play (their paths come with their biome rounds); the table is data so
    the rounds only add rows.
11. **Meters (R22)**: the enemy bar (HUD `WRUIHud.cpp` creature bars) draws a thin 3 px row per active buildup under
    the health bar in the status colour (bleed `#7A0A14`, never crimson); the player HUD draws the same above the
    stamina bar. Colours are data (`status.defs.<id>.color`).
12. **Cures (R23)**: items `ember_salve` (burn), `warming_draught` (frostbite + frozen chance), `bloodmoss_poultice`
    (bleed), `grounding_tonic` (shock), `clear_eye_drops` (blind), `hexward_charm` (curse), antidote (poison, rot,
    kept). They drink like potions (the drink commit, R53). Recipes at the alchemy table if it exists, else the
    workbench (checked in recipes.json; Fire / Frost ingredients only: ember salve = ash moss + fat, warming draught
    = frost lichen... the ingredients used must already exist in items.json).
13. **Wildbrands (R24-R25)**: data rows `wildbrands.<node>` in tuning: `fits` (class list or "all"), `frames`
    [windup, active, recovery], `dmg`, `stance`, `buildup`, `ha`, `charge` {max, dmg, area}, `followUp` {window, dmg},
    `resource` {heat|chill: amount}, `stamina` 18, `mounted` flag. `UWRGA_Melee` gains `bWildbrand` handling: a fit
    check in `CanActivateAbility` (and the wheel / slot greys with the reason text "needs a dagger, sword, …"), frames
    from the row, charge via input-release wait (hold D-pad up / the key), follow-up: a second press of the same slot
    inside the window re-triggers the ability as its follow-up row with no cooldown.
14. **Spells (R26)**: shape rows `spells.shapes.<shape>` {windup, recovery, roots, walk}; each built spell names its
    shape. Bolts / lances / pools cast walking at ×0.5 (attack slow flag), walls / novas / Meteor / Blizzard root
    (attack root flag). `chargeable` spells (Ice lance, Meteor, Fire bolt: no — bolts are quick; Frost nova and Ice
    lance and Meteor: yes) hold to charge to ×1.5 at 1.0 s. Enemy projectiles: `AWRProjectile::HomeOn` refuses a
    non-player instigator (code rule).
15. **Creature attack rows (R28-R35)**: `Data/attacks.json`, keyed `<creature>.<attack>`, rows per combat.md 2.18.2
    for all 19 creatures + the wisp. `creatures.json` rows get `attacks` = row ids. One executor in `AWRCreatureAI`
    replaces the bite ability + the kind-wide specials: pick (range band, cooldown, weights, behindOnly), tell (rising
    sound + glint on the attacking part's bone: jaw / head / claws by name search), track (turn 180°/s for
    `trackUntil` of the windup), hold (delayHold, final glint + sting 0.2 s before contact), strike by motion:
    `snap` / `bite` / `claw` / `sweep` (sweep the reach in front at the strike frames, cone half-angle from the row),
    `lunge` / `pounce` (leap, hits the first body it reaches), `charge` (run the line, wall stun), `spit` (projectile
    ≤ 1000 cm/s, lands where the target stood at windup start, min range 400), `stomp` (a visible ring travelling at
    1200 cm/s from the feet, damage on the front, `low`), `burst` (from under the ground/snow/lava, dirt/snow ripple
    tell at the target's feet), `slam` / `roll` (line), `buffet` (push), `kick` (`behindOnly` rear arc), `howl`
    (calls packmates within 40 m), `dust` (blind cloud), `withdraw` (shells up). Then `punish` (no attack, faces the
    target, open), then `comboNext` by chance (max 4 per string), then **breathe** (pack hunters / predators back off
    to 5-8 m for 1-2.5 s circling).
    - **Clips**: the creature's real attack clips (attack / attack2 / attack3, rows pick `clip`), warped so the strike
      frame lands at contact (existing `OnSwingVisual`). AI-pipeline creatures have one attack clip: every row plays
      it at its own timing (not finished: a clip per row — LATER-COOKS).
    - **Off-screen rule (R30)**: before a windup the server checks the target player's view: the control rotation's
      forward vs the direction to the creature within the camera's half FOV (55°), or distance < 800 cm (audible: the
      tell sound plays 3D, so its direction is heard). Tames and creatures target creatures freely.
    - **readsAction (R31)**: rows flagged on highland creatures (Magmaw lunge, Rimeclaw saber bite) and bosses: when
      the target gets `State.Drinking`, `State.Casting` or `State.Charging` and is in the row's range, the creature
      starts that row within 0.4 s, skipping its cooldown once per 6 s.
    - **Groups (R32-R33)**: no token. Each pack member gets a role by size and arrival order: the biggest is the
      *leader* (presses at 2-3 m), the rest are *flankers* with an assigned angle (90°, 150°, −90°, −150° off the
      target's facing) at 400-700 cm, and with 4+ one *far-side* member at 180°. A flanker attacks when it reaches its
      angle or when the target faces away. After a combo + punish: breathe. Flee < 25% HP (kept), pack-loss flee < 50%
      (kept) → regroup 30 m from the target at the pack centre, return after 6 s if the target is still within its
      territory. A ranged hit alerts packmates within 6 m with line of sight.
16. **Bosses (R36-R49)**: `attacks.json` boss rows `<boss>.<attack>` add `hold`, `active`, `punish`, `impact`,
    flags, `comboNext`, `bands`, `phase`, `tamePunish`, `bone` (the attacking part: hammer hand, jaw, tail, claws).
    `AWRBoss::BuildAttacks` reads them (today's numbers stay the fallback). Changes:
    - **Visible contact (R40)**: `StartAttack` no longer spawns a full-area `AWRGroundMarker`. It shows a glint on the
      row's bone (white blockable / crimson unblockable) and plays the tell sound; delayed rows add a second glint 0.2 s
      before contact. At resolve, melee rows (cone / line / self) hit only bodies within the reach of the attacking
      part: the bone's position at the strike ± (row radius) swept along the row's arc; the SPEC shape is still the
      pick test and hard limit. Thrown rows (slag, boulder, snow fling, ice spikes) keep their target circle but show
      the real falling object's shadow on the ground (a small dark disc growing as it falls, the "real ground effect"
      exception) — the projectile itself is drawn by the fx system. Slams send the travelling shockwave (R34).
    - **Transition (R37)**: at < 50% (and not in a break or crit), the boss queues its transition row (Vulkarn
      `forge_roar`, Skaldrith `takeoff_gust`): 1.4 s windup, crimson glint, damage taken ×0.5 during it, then phase 2.
    - **Phase 2 (R38)**: new rows with `phase: 2`: Vulkarn `late_hammer` (delayed variant), `quench_feint`
      (0.6 hold), `bellows_into_slam` (combo); Skaldrith `late_tail`, `claw_claw_lunge`, air breath with 0.5 hold.
      Idle gaps by phase from tuning.
    - **Choice (R39)**: bands close < 400 / mid 400-1200 / far > 1200 with per-row band weights; a row can't be picked
      a third time running; punish always plays (BusyUntil = punish); projectile rows capped 1 in flight in phase 1,
      2 in phase 2.
    - **Parts (R41)**: rows `parts` in creatures.json {id, bone, stance, onBreak, drop}. A hit within 120 cm of the
      part's bone (world space at the hit) deals its stance to the part too; at 0 the part breaks: 1.2 s stagger, 30% of
      max stance, the drop once per fight to everyone in the ledger, and its `onBreak`: `noHammer` (Vulkarn drops the
      hammer 4 s: only tongs and breath), `grounded` (Skaldrith wing: SPEC wing hits rule kept, the part makes it
      visible), `shell` (Warden SPEC rule kept), `noTrail` (Cinderhorn), `noSweep` (Old Tusker both tusks).
    - **Lock points (R42)**: `lockPoints` [{bone, name}] in creatures.json. The lock target is (actor, point); the
      camera aims at the point's bone; flick moves to the next point on the same target before the next target.
    - **Tame-punish (R46)**: rows with `tamePunish` pick the furthest tame inside the chase radius as their target
      when one exists.
    - **First kill (R47)**: on a boss death, each ledger player who had never killed it (saved per player in their
      save: `bossKills`) gets a named drop actor (`AWRFirstKillDrop`: a gold light — gold = yours — with the trophy
      name) that rises, waits, and flies to them at 6 s or when they come within 4 m, then adds the boss's trophy item
      and shows the name card. The trophy item rows exist? (checked: `trophy_*` in items.json; if not, the boss part is
      the named drop).
    - **Run-back stones (R48)**: `AWRRunbackStone` placed by data (`Data/island_dressing.json`? no — spawned by the boss
      at BeginPlay from `bosses.<id>.runback` offset rule): a shrine-stone at 50 m from the arena centre toward the
      approach path (the direction to the nearest road / the island centre), on the ground. Interact = bind your wake
      point there (like a bed). Its mesh: an existing rock prop with a carved ring — the dressing's SimpleRocks pack
      stone, scaled, with the gold-free white rune glow (colour law). **Intent**: it sits on the approach so the run
      back walks the same path you took in, it faces the arena door, it is on flat ground and doesn't block the path.
    - **Boss bar (R49)**: already drawn? (checked in WRUIHud: boss bar exists) — add the phase pip.
17. **Cinderhorn (R44)**: a roaming mini on the ash plains, the old bull of the Emberhorn herd (same species body,
    scaled 1.7×, darker hide, charred horn tint). **Why here**: the ash plains are wide and flat — the only place a
    2500 cm charge is a threat you can read and bait. **What it connects to**: 8 basalt pillars spaced along its roam
    loop (it roams between Emberhorn grazing grounds; pillars are the natural basalt columns of the plains, 2.5-3.5 m
    tall, 1.2 m across, rooted 60 cm into the ash). **How it meets things**: a charge into a pillar stops it dead —
    4.0 s stun, the gold weak-point mark (stance break) and the pillar cracks (it can take 2 charges, then breaks into
    rubble and is gone for that fight, so the player has to move the fight along the loop); its burning trail is a real
    ground hazard (lava-fire patches) along the charge line; a frost proc puts the trail out for 20 s. Moves per
    combat.md 2.17.4. Horn part stance 50: broken → no trail.
18. **Old Tusker (R45)**: roaming mini on the snow flats, the herd bull of the Woolly drudges (same species body
    ×1.8, grey-white guard hair, long tusks). **Why**: the snow flats hold the drudge herds and the frozen lakes; he
    guards them. **Connects to**: the nearest open water (a frozen lake's broken edge or the sea shore — found at
    spawn by the water subsystem); 2 Woolly drudges join at 60% (herd bellow). **Meets**: a burn proc makes him run to
    the water (6 s, back open: stance taken ×1.5) — the burn fire hisses out as he wades in; tusk parts stance 45 each,
    both broken → no sweep and trample stops at its first hit. Moves per combat.md 2.17.4.
19. **Camera (R50-R52)**: a new trace channel `WRCamera` (DefaultEngine.ini) — the boom probes on it; `WRDressing`
    small instances (shrubs, small rocks), build-piece furniture, creatures and pawns ignore it; landscape, big rocks
    (second-largest bounds ≥ 160 cm), cliffs and walls block. Pull in at once (≤ 0.12 s), hold 0.15 s, then ease out
    over 0.4 s. Hide the hero's meshes (owner) when the camera is within 100 cm of the head. Giant framing in the
    hero's camera tick: lock target with half-height × 2 > 3 × the hero's height → boom +150 cm and pitch +10° eased
    over 0.4 s. Mouse flick: with a lock on, a mouse X speed ≥ 300 px/s calls `FlickLock` (0.25 s re-arm).
20. **Drink (R53)**: using a potion or a cure from the belt starts the drink on the server: the item is used up at
    start; `State.Drinking` 0.9 s (replicated; walk ×0.4 via the speed multiplier; abilities blocked); the hero plays
    its drink clip (the `eat` / `drink` set in player_art if present, else the cast clip... checked in code); at 0.5 s
    the heal / cure lands if still drinking. A stagger, freeze or death clears it (the potion is lost). Shared cooldown
    1.5 s replaces each item's 6 s (`potions.sharedCooldown`).
21. **Mounted (R54)**: on the ridden creature's input: Light (LMB / RB) and Heavy (RMB / RT) call
    `ServerRideAttack`. The rider (attached hero) plays its light / heavy swing (replicated through the hero's
    PlaySwing), the side is the camera's side (left if the camera yaw is left of the mount's facing): the server sweeps
    the rider's weapon reach to that side at the strike. Light counts as running light (stance ×1.2), heavy = rear-up
    strike (mount plays its attack clip, stance ×2.8). Couched charge: holding Heavy with a spear while the mount is
    above 900 cm/s lowers the lance: for up to 1.5 s, the first hostile in a 60 cm-radius line 260 cm ahead takes
    ×2.0 damage, stance ×4, impact 5. Mounted, the hero's GAS abilities except bolt / lance spells and `mounted`
    Wildbrands are blocked by `State.Mounted`. Unhorsed: a hit on the mount with impact ≥ 4, or the mount staggered →
    dismount, rider knocked down 1.2 s (stagger). The mount bites its rider's lock target in reach (aggressive tames).
22. **PvP (R55-R57)**: a `pvp` tuning block applied inside `ApplyHit` when attacker and target are players (or a
    player's tame vs a player, ×0.5): global ×0.6, class ×, Wildbrand / spell ×0.85, buildup ×0.7. Combat tag 15 s on
    both (server time on the player state, replicated owner-only for the HUD tag icon): blocks fast travel, wake-point
    change (bed set) and clan storage (the commands refuse with "In combat"). Logout while tagged: the game mode leaves
    a copy body? — the hero pawn is kept 30 s (not destroyed on logout), killable; its inventory drops if killed (the
    death penalty rules run as for any death). Defender lag: a player's melee / projectile hit on a remote player who
    isn't defended now waits for the defender's back-dated guard / roll the same way creature blows do (extend
    `UWRLagSubsystem::ShouldHold` to player attackers, capped 250 ms).

---

## 3. Plan: build steps (in order)

1. **Data**: `weapons.json` 9 classes; `items.json` weapons per class + cures; `recipes.json`; `attacks.json`;
   `creatures.json` (physResist, lockPoints, parts, attacks lists, cinderhorn, old_tusker); `tuning.json` blocks
   (`combat.charge/counterHit/stealth/twoHand/chainHeavy/jumpAttack/contextWindow`, `status.defs/thresholds/...`,
   `reactions`, `wildbrands`, `spells.shapes`, `creatureAI.groups/offscreen/readsAction`, `bosses.*` transitions /
   idle gaps / bands, `camera` additions, `potions`, `mounted`, `pvp`); `island_spawns` for the two minis + pillars.
2. **C6 code**: `FWRMove` gains `PhysType`, `bCharged`, `bJump`, `bGuardPoke`, `bAir`; `FWRWeaponClass` gains
   `Hands`, `bGreat`, `Trace`, `CancelFraction`, `PvpScale`, `Buildups`; PickMove contexts; charge phase; jump attack
   landing; two-hand state + input; stealth / counter-hit / phys / head in ApplyHit.
3. **C7 code**: status defs, buildup array + replication, procs, reactions, frozen mash, HUD meters, cures.
4. **C8 code**: Wildbrand rows, fits + grey reason, charge + follow-ups, spell shapes walk / root / charge, projectile
   homing rule.
5. **C9 code**: creature attack executor from attacks.json, tells, off-screen, readsAction, groups, shockwave, pull
   alert.
6. **C10 code**: boss rows, glints, visible contact, transition, phase-2, choice, parts, lock points, tame-punish,
   minis, Cinderhorn / Old Tusker behaviours, first-kill drop, runback stones, boss bar pip.
7. **C11 code**: camera channel + hold + hide + giant framing; mouse flick; lock points; drink.
8. **Mounted**; 9. **PvP**; 10. CHANGELOG / HANDOFF; compile after each step; spot-check; commit; LANDED.

Edge cases decided: a charge released during a stagger is lost (no damage, stamina already 0 cost); a follow-up press
after the window does nothing (the cooldown runs); a creature whose target dies mid-windup finishes the row into the
air (no snap retarget); a blind creature mid-row keeps its committed swing; a mount killed under the rider unhorses
the rider (knockdown); a boss transition queued while the boss is frozen waits until it thaws; first-kill drops are
per player and never stolen (a drop only flies to its owner; others don't see the name card); logout bodies don't
duplicate items (the logged-out player's save is written at logout; if the body is killed the death drop is written
into the world and the player's save is marked dead on next login).


---

## 4. Changes to the plan made while building (and why)

- **Old Tusker cools off in snow, not water**: the snow flats' nearest open water is 588 m away (island_walk.json shore
  cells), too far for a 6 s run. A burning bull throwing himself into deep snow does the same job with what's there
  (an elephant dust-bathes; the fire is smothered), back open (stance x1.5) for 6 s.
- **The boss bar stays top centre** (Raven's approved mockup) with its phase shown in text; combat.md's "bottom centre"
  was a GAP default, the built UI wins.
- **Belt stays the 8-slot hotbar** (Raven's built belt); the drink rules (commit, shared cooldown) apply to it.
- **Cures for bleed, shock, blind and curse** wait for their biomes' ingredients (no Fire / Frost source of those
  statuses on players except PvP daggers / scythes; the healing potions still heal through them).
- **Spells of the ten other paths**: their path trees aren't in `Data/paths` yet, so their nodes can't be learned; the
  shapes, Wildbrand rows, statuses and reactions they need are built, so each biome round adds rows + its tree.
- **Charged boss lines run on their legs** (real movement at the row's speed); a lunge faster than 16 m/s is a leap, so
  nothing is ever placed at its end (no teleports, Aaron 11-14).
- **Logout body** (a tagged player's body staying 30 s): the player's inventory lives on the player state, which leaves
  with the player; doing it without duping or losing items needs the save to own a death bag. Left for its own cook.

## 5. Spot-check against the plan and the bar (read, not played)

- Every blow still goes through `ApplyHit` (rows, bosses, lobs, steam, statuses, the saddle, the couched spear).
- Every number is in `Data/weapons.json`, `Data/attacks.json`, `Data/tuning.json` (`combat`, `status.defs`, `reactions`,
  `wildbrands`, `spells.shapes`, `creatureAI`, `bosses`, `camera`, `potions`, `mounted`, `pvp`), `creatures.json`,
  `items.json`, `recipes.json`, `fx.json`, `sounds.json`.
- Server authority: rows, strikes, parts, statuses, drink, saddle attacks, first-kill drops and shrine-stones all run on
  the server; clients get tags, meters (`BuildupPct`), multicast tells, replicated part masks, replicated actors.
- Fixed while checking: a boss charge would have taken a player's capsule for a wall (now static world only, real
  movement); a hotbar cure would have drunk the first potion in the bag; `UWRGA_Spell` left `State.Casting` on when cut.
- Colour law: gold only for yours (the first-kill light, the charge glint is white, the shrine rune pale blue); crimson
  only for unblockable glints; bleed is a dark red (#7A0A14).

## 6. Requirement ledger: done / where

| # | Done | Where |
|---|---|---|
| R1 | Done | `Data/weapons.json` 9 classes (generator rows), `WRWeapons.cpp` parse |
| R2 | Done | `WRAbilities.cpp` `BeginCharge` (593), `OnChargeReleased` (620); `combat.charge` |
| R3 | Done | `PickMove` (330) chainHeavy |
| R4 | Done | `PickMove` run rows (sprint + speed) |
| R5 | Done | `PickMove` + `UWRGA_Dodge::EndAbility` stamps `LastDodgeEnd` |
| R6 | Done | `JumpTick` (746), jump rows |
| R7 | Stealth done (`WRCombatLibrary.cpp` stance / buildup x1.2); crouch stance not built (no clips): LATER-COOKS |
| R8 | Done | `WRCombatLibrary.cpp:595` |
| R9 | Done | `ApplyHit` physResist, head strike; `creatures.json physResist / lockPoints` |
| R10 | Done | `ServerSetTwoHanded` (`WRCharacterCombat.cpp`), `BeginTwoHandHold` (`WRPlayerController.cpp:929`), Sweep bonus (`WRAbilities.cpp:1074`), `FWRWeapons::HasShield` |
| R11 | Done | guardPoke row, `PickMove`, `ApplyHit` `bKeepsGuard` |
| R12 | Done | weapons.json class fields; `SetWindows` cancel fraction |
| R13 | Done | 15 items + recipes; `FWRToolMesh::Weapon` (`WRToolMesh.cpp:372`), `UpdateToolLook` |
| R14 | Done | `haMult`, `poise.hyperarmor.great`, `hyperarmorDamageTakenGreat` in `ApplyHit` |
| R15 | Done | `UWRStatusComponent::Threshold` (217), procs / reset in `TickStatuses`, `ApplyHit` stealth / pvp scale |
| R16 | Done | `Proc` frost branch (`WRStatusComponent.cpp:299+`), `status.defs.frost` |
| R17 | Done | `MashFrozen` (495), `AWRCharacter::PressAbilityInput` → `ServerMashFrozen`; Rimed in `Proc` + `ApplyHit` |
| R18 | Done | `Proc` bleed / poison / rot, `HealingTakenScale` |
| R19 | Done | `Proc` shock / soak / blind / decay / mark / curse; blind vignette `WRFxHud.cpp:149`, lock-off `ToggleLockOn` / `UpdateLockOn` |
| R20 | Done | `ApplyHit` fire ends frostbite (750); proc flinch in `Proc`; statuses kill (no floors) |
| R21 | Done | `ApplyHit` reactions (865), `SteamBurst` (305), group Thaw |
| R22 | Done | `WRHUD.cpp` enemy bars, `SWRStatusMeters` (`WRUIHud.cpp:73`) |
| R23 | Partly | Ember salve, Warming draught, antidote poison / rot (items, recipes, `Cures`); bleed / shock / blind / curse cures → their biomes |
| R24 | Done | `wildbrands` rows, `WildbrandFits` (840), greyed slot `WRPlayerUI.cpp:703` |
| R25 | Done | charge (`BeginCharge` art path), follow-ups `FollowUpReady` (803), `CheckCooldown` |
| R26 | Done | `ShapeKey` (1925), `CastNow`, `OnSpellChargeReleased` (1969), `HomeOn` rule (`WRProjectile.cpp:117`) |
| R27 | Done (already) | `UWRAbilityRegistry::ClassForNode` covers every Fire / Frost ability node (Meteor, Ice wall, Blizzard, Flurry...) |
| R28 | Done | `Data/attacks.json` creatures, `FWRAttackRows`, `StartRow` (607), `TickRow` (835) |
| R29 | Done | `MulticastTell` glints + voice + `tell.final`; burst ground tell; creature floor markers gone (rows) |
| R30 | Done | `WRRowFight::CanSee` (473) |
| R31 | Done | `TickRowFight` readsAction (1030), bosses `WRBoss.cpp:1126` |
| R32 | Done | `GroupSpot` (521), breathe in `TickRowFight` / `EndRow`; flee kept |
| R33 | Done | `WRCreature.cpp:1276` |
| R34 | Done | creature stomp wave (`TickRow`), boss waves (`TickStrike`), `shockwave` fx |
| R35 | Done | `creatureAI.maxProjectileSpeed`, lobs aimed at the wind-up spot, `HomeOn` rule, spit min ranges 450 |
| R36 | Done | `attacks.json` bosses (Vulkarn 11, Skaldrith 12 rows) |
| R37 | Done | `CheckPhase` pending (`WRBoss.cpp:818`), `ChooseAttack` (97), damage taken (684) |
| R38 | Done | phase-2 rows, `Idle()` gaps |
| R39 | Done | `ChooseAttack` (92) |
| R40 | Done | `BeginStrike` / `TickStrike` / `EndStrike`; ground marks only for shadows / frost |
| R41 | Done | `DamagePart` (`WRCreatureCombat.cpp:336`), `AWRBoss::OnPartBroken` (765); no visible piece falls off (LATER) |
| R42 | Done | `lockPoints`, `SetLockTarget` / `FlickLock` / `UpdateLockOn` aim |
| R43 | Done | minis' rows (flags, punish), stance 90 via tier, lock points |
| R44 | Done | `cinderhorn` row + attacks, basalt props, trail, pillar stun, horn part |
| R45 | Done | `old_tusker` row + attacks, bellow, snow wallow, tusk parts |
| R46 | Done | `tamePunish` rows (slag_throw, ice_spikes), `ChooseAttack` furthest tame |
| R47 | Done | `FirstKillDrops` (912), `AWRFirstKillDrop` (1042), `bossKills` save |
| R48 | Done | `SpawnRunbackStone` (873), `AWRRunbackStone::Bind`, wake choices in `WRBeds.cpp` |
| R49 | Kept | Raven's top-centre boss bar with the phase (section 4) |
| R50 | Done | `WRCameraBoom.cpp:38` hold / ease, `WRWorldDressing.cpp:451` channel filter, hide in `TickCombatCamera` |
| R51 | Done | `TickCombatCamera` (49) + `UpdateLockOn` pitch |
| R52 | Done | `MouseFlick` (959), lock-point flick |
| R53 | Done | `StartDrink` (`WRInventory.cpp:606`), `State.Drinking`, readsAction |
| R54 | Done | `ServerRideAttack` (1110), `TickRider` (1202), `Unhorse` (1278), `State.Mounted` gating |
| R55 | Done | `ApplyHit` PvP (696), `pvp` block |
| R56 | Partly | combat tag + chest / bed / shrine blocks done; logout body → LATER-COOKS (section 4) |
| R57 | Done | `WRLag.cpp:276` |
| R58 | Done | section 5 |
| R59 | Done | keyboard + pad for every new input (two-hand, charge holds, saddle, mash) |
| R60 | Done | unfinished parts listed in CHANGELOG and LATER-COOKS |
| R61 | Done | sections 2.17-2.18 and 4 (why each object is there, what it meets) |
| R62 | Done | CHANGELOG Unreleased, HANDOFF, LANDED |
