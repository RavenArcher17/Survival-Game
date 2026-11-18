# Wildreach cook: COOKPASS-CREATURES (every built creature and boss: look, animation, voice, behaviour)

Lane cookpass-creatures, 2026-11-15, desktop. A cook pass on what the twelve biome lanes and the Fire and Frost build
left: 202 creature rows in `Data/creatures.json` (54 bosses and minis, 148 creatures), drawn by 164 skeletal meshes
(most of them shared), 34 code-built bodies and 4 orbs; voiced through `Data/Audio/sounds.json` (259 voice rows);
driven by `AWRCreatureAI` and eleven biome AI hooks. Research how the best games make animals and monsters feel alive,
grade every built piece against THE BAR, design the raise, build it.

Spec sources: FULL-GAME 3.3 ("terrifying monsters with readable silhouettes, real animation clips, unique creature
voices"), QUALITY-PLAN rows 3, 4, 7, COMBAT-AND-LESSONS 1.13 (tells, holds, punish, packs attack freely), the share's
LATER-COOKS lines in this scope (listed in the ledger).

## 0. Requirement ledger

| # | Requirement (one each) | Source | Done where (filled at the end) |
|---|---|---|---|
| Q1 | Research how the best games do creature look / animation / voice / behaviour (named games, places, systems) plus real-world reference, written here with the details used | brief, Aaron 11-13 | DONE: section 1 (RDR2, Horizon, Monster Hunter, Shadow of the Colossus, TLOU2, BotW, Elden Ring, MH subspecies, Ark; vigilance, flight distance, alarm and contact calls, stalking, panting, neck split, startle, stun) |
| Q2 | Grade every built creature and boss (look, animation, voice, behaviour) against THE BAR | brief | DONE: section 2 (systems, file:line) and section 6 (all 202 rows, look / anim / voice / behaviour) |
| Q3 | Design the raise in writing before building: every detail, edge cases, build steps | brief, Aaron 11-13 | DONE: section 3, pushed before any code (showcase-notes 723098a) |
| Q4 | Build exactly the plan at full detail; no placeholders; anything unfinished stays hidden | brief, BAR | DONE: Wildreach 2e77a00, 02063ce, b8ca56d, 1c42d17, c44214f, 8039632; no placeholder shown (no sleep sound rather than a stand-in) |
| Q5 | Spot-check the build against plan and BAR by reading the code; fix before push | brief | DONE: section 4 (the read-through found the telegraph overlay wipe and the ridden-look gap; both fixed: 8039632, c44214f) |
| Q6 | "Already done" only with file:line proof | brief | DONE: every row here carries its file:line |
| Q7 | Design with intention: every motion / call / behaviour says why, what drives it, how it meets what it touches; nothing passes through anything; nothing snaps | Aaron 11-14 | DONE: each pose / call says its job and source in tuning _comment / _why and in the code comments (WRCreatureBody.cpp:1-5, tuning anim.gesture _why per motion, creatures.json shape _why per species); feet are never touched by the layer (applied before PlantFeet / PlaceCreatureFeet, WRAnimInstance.cpp:528-531); everything eases, nothing snaps (FInterpTo / springs, WRCreatureBody.cpp:353-387 eased aim and the flinch spring, :477-505 the gesture curves) |
| Q8 | Data-driven: numbers in tuning / creatures.json / sounds.json; sounds as table rows; damage only via ApplyHit | BAR | DONE: tuning anim.look / flinch / pant / wounded / slump / hungry / beat / gesture / sleepDip, creatureAI.mood / beat / wary / threat, necromancy.raisedLook; sounds.json voiceFallback + ambient alertEvery / answer*; creatures.json shape; no damage touched |
| Q9 | Multiplayer: server decides behaviour, clients draw it from replicated state; every machine sees the same mood | BAR | DONE: moods decided only on the server (WRCreature.cpp:3264 TickBeat, :3323 TickWary, :3202 display) and replicated (MoodRep / WatchRep, DOREPLIFETIME in WRCreature.cpp); gestures by multicast (WRCreatureCombat.cpp:730); flinch from MulticastHit (WRCharacterBase.cpp:283); poses drawn on every machine, nothing on a dedicated server |
| Q10 | Screens with mouse and pad | BAR | N/A: this pass adds no screen (no UI was in scope; nothing to drive by mouse or pad) |
| Q11 | Land in pieces: compile clean, commit + push each; CHANGELOG (Unreleased) + HANDOFF in the same commit; doc pushed after the plan and at the end | brief | DONE: six code commits, each compiled clean (editor target) and pushed with CHANGELOG Unreleased + HANDOFF; this doc pushed after the plan and at the end |
| Q12 | Unity build: unique prefixes for file-scope helpers; `git pull --rebase`; files added by name; never leave master unable to boot | lane note | DONE: helpers in WRCreatureBodyDetail / WRCreatureBodyAnim namespaces; pull --rebase each push; files added by name; every commit compiles (boot-safe: no asset or map changed) |
| Q13 | LANDED line on the share; out-of-scope finds as LATER-COOKS lines | brief | DONE: LANDED line and LATER-COOKS lines on the share (section 7) |
| L1 | Distinct attack motions per attack row (each creature has one attack clip; every row plays it at its own timing) | LATER-COOKS build-combat-full (126) | PARTLY (procedural): row gestures WRCreatureCombat.cpp:730 + WRCreatureBody.cpp:477-505, 28 motions in tuning anim.gesture; real per-row clips stay an art job (LATER-COOKS line kept) |
| L2 | Creature stance-break kneel / slump (today: held hit clip + a procedural slump) | LATER-COOKS build-combat-core (120) | DONE (procedural): the slump WRCreatureBody.cpp:404 (neck -30, spine -10, head tilt; tuning anim.slump) over the held recoil clip; paired crit-victim clips stay art (LATER-COOKS kept) |
| L3 | Farm animals need a hungry look (thin / head-down idle) and a pen graze idle | LATER-COOKS build-farming (140) | DONE: hungry head WRCreatureBody.cpp:466 (TameRep.Hunger < anim.hungry.below), graze beats for tames standing by WRCreature.cpp:3729 with hungry ones grazing twice as often (TickBeat); a thin body stays art |
| L4 | Each species' own whistle-answer call (tames and Call your mount answer with voice.idle) | LATER-COOKS build-finish (102), cookpass-taming (200) | PARTLY: the whistle and Call your mount answer with voice.answer (WRPlayerMenus.cpp:1094, WRTaming.cpp:2459) through sounds.json voiceFallback; each species' own recorded answer needs the voice pass (a creature.<id>.answer row) |
| L5 | Portal Alphas / Elders (Rimeclaw, Glacimaw) movesets listed | LATER-COOKS endgame (108) | DONE (already built): Data/attacks.json rimeclaw (claw1, claw2, saber_bite, lunge, spine_shake) and glacimaw (snow_burst, ice_spit, body_slam) rows, line 70 |
| L6 | Raised minions of non-Graveyard creatures keep their own hide colour (only threads and eyes show) | LATER-COOKS build-biome-graveyard (183) | DONE: WRGraveCreatures.cpp:414 bone-grey wash on raised bodies with no palette row (tuning necromancy.raisedLook) |
| L7 | Hearing on one shared noise model (Gloomhowl, Sporestalker hear outside the Crystal caves) | LATER-COOKS build-biome-crystal (77) | NOT BUILT: the noise model lives in the Crystal land (WRCrystalTypes.h:164); moving all hearing onto one world-wide model is a systems change; LATER-COOKS line 77 stands |
| L8 | Fused creature voices and composed cues for every land (recordings) | LATER-COOKS 172, 181, 185, 190 | NOT BUILT (recordings): fused voices and composed cues need recording / composing work no code lane can do; the voice system now takes a species row the moment one exists (WRSound.cpp:909-941) |
| L9 | Sculpted bodies: Floeback, Windmane, portal bosses, Slag thrall / Rimecrawler | LATER-COOKS 152, 154, 160 | NOT BUILT (art): sculpted Floeback / Windmane / portal bodies need the art pipeline (no ComfyUI in lanes); lines stand |
| L10 | Real riding / courtship clips | LATER-COOKS 156, 199 | NOT BUILT (art): riding / courtship clips; lines stand |
| G1..G9 | Grade findings (section 2), one row each in section 3's build list | grade | DONE: G1 WRCreatureBody.cpp:227 + WRAnimInstance.cpp:579; G2 WRCreature.cpp:3264; G3 WRCreature.cpp:3323, :1547; G4 WRCreature.cpp:3202; G5 WRCreatureBody.cpp:193; G6 WRCreatureBody.cpp:389-410, 466; G7 WRCreatureCombat.cpp:730; G8 WRCreatureBody.cpp:106 + creatures.json shape x73; G9 WRSound.cpp:909, WRAudioDirector.cpp:1190-1220 |

## 1. Research

### 1.1 Games

| Game / place | The system | What makes it work | What we take |
|---|---|---|---|
| **Red Dead Redemption 2**: a whitetail herd on the Heartlands, wolves at Cumberland Forest | Deer graze head-down and lift their heads every few seconds to check; on noticing you they freeze and stare, ears and head locked on you; a doe snorts and stamps, the tail flags, and when one bolts the herd bolts with it a beat later. Wolves circle and growl with heads low before the first lunge; every animal's head turns to watch you as you pass | Awareness is shown before it acts: you can read and play the animal (approach slowly, crouch). A stare is the most alive thing an animal does | A wary stage (head up, locked on you, body still) before flight; the herd bolts together with a short stagger; the predator's threat display before its first charge; head tracking on everything |
| **Horizon Zero Dawn / Forbidden West**: Grazer herds in the Embrace, Watchers | States Relaxed / Suspicious / Alarmed / Searching, readable with no UI: grazers rake the ground, a Watcher's head swings in slow arcs and snaps to a sound, the herd's Watcher calls the alarm and everything flees | Behaviour is a loop of small believable beats, and each state has its own body shape | Calm beats (graze, sniff, scan, preen) chosen by diet; a scan is head sweeps with held stops, not a smooth pan |
| **Monster Hunter World / Rise**: Anjanath in the Ancient Forest, any monster's exhaustion | Monsters eat, drink, sleep and patrol; on first sight they roar (a threat display that gives the hunter a read); when tired they drool and pant; near death they limp off with the head low; hits make parts flinch and the body jerks away from the blow | The monster's body tells you its state: fresh, angry, tired, beaten | Exertion panting after running and fighting; a wounded posture (head low) below a third of its health; a directional flinch on every hit, scaled by the blow and the body's size |
| **Shadow of the Colossus**: the colossi, Agro | The colossus's head turns to follow Wander, the neck carrying most of the turn; Agro looks where you steer | A head that follows you is the strongest "it sees me" signal there is | Head aim spread over the neck chain (neck 60%, head 40%), clamped, eased, dropped when you're behind it; a ridden mount looks where its rider looks |
| **The Last of Us Part II**: the dogs, the WLF callouts | Dogs work a scent with head-down sniffing sweeps; enemies call and answer each other | Call and answer makes a group feel like a group | Packmates answer each other's contact calls; carnivores sniff the ground in their calm beats |
| **Breath of the Wild**: deer at the Great Plateau, horses at Outskirt Stable | Animals notice you by distance and your speed; walking or crouching lets you get close, running flushes them; whole groups scatter | The rule is simple and learnable | Flight starts closer for a slow approach and further for a sprint; walking up slowly to offer food still works (taming by offering stays possible) |
| **Elden Ring / Dark Souls**: the wolves of Limgrave, Crucible Knights | Wolves howl to bring the pack; enemy reuse with new proportions and colour (Bloodhound Knight / Rennala's knights; ER's giant variants) | A shared skeleton reads as a different animal once its proportions change | Per-species shape (head size, neck length, tail length, wing size) on the shared meshes |
| **Monster Hunter subspecies** (Pink Rathian, Azure Rathalos, Brachydios / Raging) | The same skeleton and clips under new proportions, colours and plates | Silhouette is proportion first, colour second | The shape table changes proportion; the existing overlay/tint keeps the colour |
| **Ark: Survival Ascended** | Every species has its own call set; individuals of one species sound alike | Shows what we must beat: clones in a pack sound like a loop | Each individual's voice is pitched a little off its species (a seeded spread), so a pack is several animals |

### 1.2 Real-world reference

- **Vigilance and the group-size effect** (Lima 1995; Elgar 1989): grazers lift their heads to scan every 5-20 s; the
  bigger the herd, the less each one scans. Head-up bouts last 1-3 s. Used: graze beats lift the head every 4-7 s for
  1-1.6 s; scan chance shrinks with pack size (x 3 / pack count, at least 0.3).
- **Flight initiation distance** (Stankowich 2008, ungulates): flight starts further when the approach is faster and more
  direct; animals first become alert (stare) at roughly twice the flight distance. Used: wary at 2800 cm; flushed inside
  900 cm by anything moving faster than 250 cm/s, inside 1800 cm by a sprint.
- **Alarm signals**: white-tailed deer snort and stamp, the tail flags; prairie dogs and birds give alarm calls that make
  the group flee. Used: an alarm call at the stare's start; the herd bolts 0.15-0.6 s after the first.
- **Contact calls**: flocks, wolves, elephants keep in touch with call and answer. Used: an idle call is answered by
  one packmate in earshot 45% of the time, 0.5-1.4 s later.
- **Stalking posture** (cats, canids): head low and level, shoulders down, eyes fixed. Used: the threat display pose.
- **Panting** (canids, birds' gular flutter): fast shallow chest movement after exertion, 2-4 Hz in medium animals,
  faster in small ones. Used: a spine-and-neck heave at 2.4 Hz x (60 / radius)^0.35, built by running and fighting,
  gone over 10 s.
- **Head turn split** (vertebrate cervical spine): long-necked animals turn mostly with the neck; the head adds the last
  third. Head yaw comfort about 70 degrees each way before the body turns. Used: neck 60 / head 40; yaw clamp 70, pitch
  +35 / -40; beyond 110 degrees the look fades out and the body turns instead.
- **Flinch** (startle reflex): the body jerks away from the blow in under 100 ms and settles with a small overshoot.
  Used: an underdamped spring (omega 18, damping 0.45), yaw away from the side hit, pitch up when struck in front.
- **Stance break / exhaustion**: a stunned animal's head drops and the back sags. Used: the slump (neck -30, spine -10,
  head roll 12) held for the break.

### 1.3 Lessons

1. An animal is alive when it **looks at things**, and the thing it looks at says what it thinks.
2. Behaviour is **readable beats**: graze, scan, stare, threaten, flee, each its own body shape.
3. Groups are groups: they **flee together** and **call and answer**.
4. Clones break the spell: each individual a little different in **voice**, each species different in **silhouette**.
5. Every blow **lands on the body** (flinch), every effort shows (**pant**), every defeat shows (**slump, head low**).

## 2. Grades (built pieces vs THE BAR)

| Piece | Where | Grade | Why |
|---|---|---|---|
| Locomotion: gait speed match, turn steps, feet and spine on slopes, out-of-step packs | WRAnimInstance.cpp, WRCreature.cpp:560-760 | A- | Solid; the clips are generated (QUALITY-PLAN row 3) |
| Attack timing: clip fitted to the strike, rear-back telegraph by move kind | WRCreature.cpp:834-1010 | B | Good timing, but every row of one creature shows the same motion: a bite, a sweep and a kick look the same (L1) |
| Head / eyes | none | F | No creature or boss ever turns its head to anything: it looks through you |
| Calm behaviour | WRCreature.cpp TickWild idle (wander every 4-9 s) | D | Walk, stop, walk: no grazing, sniffing, scanning; nothing shows diet or temperament |
| Awareness of the player | PickTarget (aggressive only); passive flee at 6 m only while you move fast, for one tick of input | D | No stare, no alarm, no herd flight; a doe runs only while you're touching it |
| First contact with a predator | aggro -> instant chase | C | The aggro call plays but the body gives no read: the threat display every reference game uses is missing |
| Hit reaction | hit clip on poise break only (142 of 164 meshes have one), nothing otherwise | C | Most blows land on a statue; the 22 with no hit clip never react |
| Exertion / wounds | none | F | A creature at 5% after a long chase looks and moves as at 100% |
| Stance break | held hit clip; TickStanceSlump is an empty function (WRCreature.cpp:1129) | D | The comment promises a slump that isn't there (L2) |
| Hungry tames / farm animals | none | F | L3 |
| Look: shared meshes | 15 base meshes carry ~130 species, told apart by tint, overlay and scale | C | Gravehound, Duskjackal, Hollowfang and Gloomhowl are the same dog; Bramblewasp, Cryptstalker and Tombback the same mite. Proportion is the first read of a silhouette and it never changes |
| Look: own art (Fire and Frost roster, bosses' code-built looks) | Content/Creatures, WR*BossLook.cpp | B+ | Raven-reviewed bodies; bosses' looks have their own code |
| Voice: rows and families | sounds.json creatures table, WRSound.cpp:846 | B- | Per-species pitch and families; but every member of a pack has the identical voice, no alarm / flee / answer calls, whistle answers are the idle call (L4) |
| Voice: recordings | Data/Audio/creature | C | Stand-ins pitched from families for most lands (L8, recordings: out of a code lane's reach) |
| Bosses: behaviour | WRBoss*.cpp + 11 biome boss files | B+ | Phases, rows, arenas; no head tracking (shared F above), no flinch |
| Portal Alpha / Elder movesets | attacks.json rimeclaw, glacimaw rows | A- | Already built (L5): proof in section 5 |

Grade findings to build (G rows):

- **G1** head aim on every skinned creature and boss (F -> A-)
- **G2** calm beats by diet: graze, sniff, scan, preen, with vigilance head-ups (D -> A-)
- **G3** wary stage, alarm, herd flight with stagger; slow approach still lets you offer food (D -> A-)
- **G4** threat display before an aggressive creature's first chase at a survivor or tame (C -> A-)
- **G5** directional flinch on every landed blow, any body with a skeleton (C -> A)
- **G6** exertion panting, wounded head-low posture, stance-break slump (F / D -> A-)
- **G7** row gestures: every attack motion its own coil and release over the one clip (B -> A-; real clips stay L1)
- **G8** per-species shape for the shared meshes (C -> B+)
- **G9** voice: per-individual pitch, alarm / flee / answer calls with data fallbacks, packmates answer (B- -> A- within the recordings we have)

## 3. Plan

### 3.1 The body-language layer (G1, G5, G6, G7; WRAnimInstance)

One procedural layer over the clips, in the proxy after the pose lifts and before the feet (so feet still plant):

- `FWRCreatureBodyInput` in `FWRAnimInput`: `LookYaw, LookPitch, LookAlpha` (head aim), `NeckPitch` (dips and lifts,
  unclamped), `HeadRoll` (the curious / stunned tilt), `SpineYaw, SpinePitch, SpineRoll` (gesture, flinch, pant, slump).
- Chains found once at setup (`UWRAnimInstance::SetBodyChains`): the **neck** = the head bone and its parents whose
  names hold "neck" (generated rigs: `neck, head`; pack rigs: `Neck, Neck1, Head`; Mixamo: `Neck, Head`); the **spine**
  = the neck's parents whose names hold "spine" or "chest". The head bone is `AWRCreature::HeadBone()`.
- Applied in component space about the mesh's up (yaw), side (pitch, nose-up positive, as the pose lifts) and forward
  (roll) axes, each bone turning its share and carrying its children: look and NeckPitch 60% over the neck bones, 40%
  on the head (100% on the head with no neck bone); spine terms shared evenly over the spine bones.
- **Shape** (G8) applied first in the same pass: per bone uniform scale and child-translation length (3.5).

The creature (`AWRCreature::TickCreatureBody`, every machine but a dedicated server, only while its pose updates):

- **Look target** by mood (3.2): engaged / threat -> its target's chest (AggroRep); wary -> the watcher (WatchRep);
  ridden -> the local rider's view direction (others: straight ahead); tamed and calm -> a glance at its owner for
  1.5-3 s every 4-9 s; calm -> its beat's look (scan holds) or a curious glance at a survivor inside 12 m; asleep /
  unconscious / dead / stance broken -> none.
- Yaw and pitch from the head's world position to the target in the actor's frame; clamped to `anim.look.maxYaw` 70,
  `maxPitchUp` 35, `maxPitchDown` 40; alpha fades to 0 between 90 and 120 degrees off (behind it: the body turns).
- Eased with `anim.look.speed` 5 (calm) / 9 (engaged); during an attack or hit clip the alpha drops to
  `anim.look.oneShotAlpha` 0.35 so the clip's own head move reads; bosses x `anim.look.bossAlpha` 0.6.
- **Flinch** (G5): `MulticastHit` stores the blow's origin and damage before `OnHitVisual`; the creature kicks an
  underdamped spring per axis (`anim.flinch.omega` 18, `damping` 0.45): yaw away from the struck side up to
  `anim.flinch.yaw` 10, pitch up 7 when struck in front / down when from behind, roll 5 toward the struck side; strength
  = clamp(damage / max health x `anim.flinch.damageGain` 8, 0.35, 1); bosses x `anim.flinch.bossScale` 0.35, hyperarmor
  x 0.6; a poise-breaking hit doubles it (the hit clip plays as well). Every blocked / no-damage result: none.
- **Pant** (G6): exertion builds over `anim.pant.buildSeconds` 6 while running faster than 60% of its speed or engaged,
  falls over `recoverSeconds` 10; a heave on spine pitch (`anim.pant.spine` 1.8) and neck (`anim.pant.neck` 2.5) at
  `anim.pant.hz` 2.4 x (60 / radius)^0.35, clamped 1.2-4.5 Hz. Not asleep, not dead.
- **Wounded** (G6): below `anim.wounded.below` 0.35 health the head carries `anim.wounded.dip` 14 lower (bosses 6), the
  pant never drops under 0.4.
- **Slump** (L2, G6; fills `TickStanceSlump`): while stance-broken the slump eases in over 0.4 s: neck -30, spine -10,
  head roll 12 (`anim.slump.*`), out over 0.5 s when it rises.
- **Hungry** (L3): a tame that is hungry carries its head `anim.hungry.dip` 12 lower and grazes / sniffs more often
  (hunger replicated as a flag on the tame rep).

### 3.2 Moods (G2, G3, G4; server decides, replicated)

`AWRCreature::MoodRep` (replicated byte, `EWRCreatureMood`: Calm, Graze, Sniff, Scan, Preen, Wary, Threat, Flee, Eat)
and `WatchRep` (replicated actor), set by the generic wild brain (`TickWild`; biome hooks that take a turn leave it at
what it was, and the mood falls back to Calm after `creatureAI.mood.staleSeconds` 2 without a refresh).

- **Calm beats** (G2): when the idle wander reaches its spot it stops for a beat (`creatureAI.beat.seconds` [3, 8]):
  herbivores / herds / critters graze 55%, scan 30%, preen 15%; carnivores sniff 45%, scan 40%, preen 15%; a flyer on
  the ground preens 50% / scans 50%; scan chance x clamp(3 / pack count, 0.3, 1) (the group-size effect). It stands
  still for the beat, then wanders on. Drawn as: graze = neck down `anim.beat.grazeDip` 50, chewing bob 3 at 1.6 Hz,
  a vigilance head-up every 4-7 s for 1-1.6 s; sniff = dip 30 with yaw sweeps 10 at 0.8 Hz; scan = held looks at
  -50 / 0 / +45 degrees, 1-1.5 s each (saccades, 0.15 s moves); preen = head 65 to one side, dip 25, a little roll.
- **Wary** (G3; passive and neutral, wild, awake): the nearest survivor inside `creatureAI.wary.notice` 2800 cm (x the
  sneaking `aggroRadius` stat, x0.35 asleep: asleep ones don't stare) becomes the watcher; the creature stops, turns its
  body if the watcher is more than 70 degrees off, and stares (head high: NeckPitch +10). The wary state holds 2 s after
  the watcher leaves the radius.
- **Flush** (G3; passive only): flees (`StartFleeing`) when the watcher is inside `creatureAI.wary.flush` 900 cm moving
  faster than `flushSpeed` 250 cm/s, or inside x2 sprinting faster than 600. Walking slowly up to it (the taming offer)
  never flushes it, as today. Packmates of the same pack within `herdRadius` 2500 cm bolt after
  `herdDelay` [0.15, 0.6] s from the same threat (the herd goes together). Neutral herds stare but hold (they defend).
- **Threat display** (G4; aggressive, wild, generic brain): when it first turns on a survivor or a tame (not when it was
  hit in the last 5 s, not a raider, not a boss, not a hunt for prey) it holds for `creatureAI.threat.<archetype>`
  (pack_hunter 0.5, ground_predator 0.9, flyer 0.4, others 0.7 s; highland x1.3): faces the target, head low and level
  (NeckPitch -12), shoulders down (SpinePitch -6), a slow sway; the aggro call is already its growl. Cut short when the
  target comes inside its attack range or hits it.
- **Flee / Eat**: MoodRep Flee while fleeing (head forward and high, looks back at the threat on the first 0.6 s);
  Eat while over a kill (dip 45, tearing bob 6 at 2 Hz).

### 3.3 Row gestures (G7, L1)

`StartRow` (server) sends `MulticastRowGesture(Motion, Windup+Hold, Side)`; each machine eases a coil pose over the
wind-up and a release pose at the strike (0.15 s), back to rest over 0.35 s. Poses are tuning rows
`anim.gesture.<motion>` = `{ "coil": [spineYaw, spinePitch, spineRoll, neckPitch, headYaw], "strike": [...] }`, side
mirrored by `Side` (+-1 picked by the server so every machine mirrors the same way):

| Motion | Coil | Strike | Why |
|---|---|---|---|
| bite | neck +10 (head draws back) | neck -14 | the snap forward |
| claw | spine yaw 12, roll 10 to the side | yaw -18 | the forequarter swings across |
| sweep | spine yaw 28 to the side | yaw -38 | a horn / tail sweep |
| kick | spine pitch -8 (weight onto the front) | pitch +6 | the hind legs fire |
| lunge, pounce | spine -6, neck -10 (crouch) | neck +6 | the spring |
| charge, roll | neck -20 (head down) | neck -14 | horns first |
| slam, bodyslam, stomp | spine +12 (rear) | spine -14 | weight comes down |
| spit, mudspit, heatball, darts | neck +12, head back | neck -10 | the throw |
| howl, call, cry, roar, bugle, scream | neck +35 (head up) | neck +30 | the call |
| burst, discharge, dust, puff | spine -6 (gather) | spine +8 | the release |
| buffet | roll 12 | roll -12 | the wing beat |
| withdraw, retreat | neck -18 (tuck) | neck -18 | it pulls in |

Bosses' rows carry no motion (their own code and clips): no gesture.

### 3.4 Voice (G9, L4)

- **Individual pitch**: each creature's voice pitched by a seeded `1 +- audio.voice.spread` (0.06) on every machine
  from its id and spawn spot (packmates differ, the same animal stays itself).
- **New kinds** `alert`, `flee`, `answer` with fallbacks in `sounds.json` `voiceFallback` (kind -> [kind, pitch,
  volume]): alert -> idle x1.12 at 0.85; flee -> hurt x1.06 at 0.7; answer -> idle x1.0 at 0.8. A species' own row
  (`creature.<id>.alert`) wins when the voice pass adds it.
- **When**: the director plays `alert` when a creature turns Wary (once per 8 s), `flee` when it starts to flee; after
  an idle call, one packmate within 2500 cm answers (45%, 0.5-1.4 s later).
- **Whistle / Call your mount** answer with `voice.answer` (falls back to the idle call until the voice pass gives
  each species its own; L4 partly).
- Sleep breathing: no recording exists; nothing plays (no stand-in).

### 3.5 Shape (G8)

`creatures.json` `shape` = `{ "head": s, "neck": l, "tail": l, "wings": s }`: head = the head bone's uniform scale; neck
= the neck chain's length (each child's offset along the chain x l, thickness kept); tail = the tail chain's length;
wings = each wing root's uniform scale (the whole wing). Legs never change (the feet stay planted). Authored for the
shared-mesh species from each one's own description (the table in 3.7), the mesh's owner stays 1.

### 3.6 Edge cases

- Proc bodies (34) and orbs (4) have no skeleton: none of the layer applies (their code bodies move themselves); the
  Floeback keeps its own flinch.
- Telegraph rear-back moves the whole mesh; the gesture moves bones: they add.
- Dedicated server: no pose work at all (moods replicate; nothing else runs there).
- A creature arriving on a machine mid-gesture (relevancy): the multicast is unreliable and old ones are skipped
  (no gesture if it arrives later than its wind-up).
- Riding: a ridden mount never stares, flushes or displays; its look follows the local rider's camera.
- Tamed: no wary / flush / threat (a tame's fights are ordered); calm beats on Stay and while its owner idles.
- Raiders, bosses, minions, portal creatures: no threat display, no beats.
- Unity build: all new file-scope helpers in `WRCreatureBodyDetail` / `WRCreatureMoodDetail` namespaces.

### 3.7 Shape table (authored values)

73 species, each with its `_why` beside its numbers in `Data/creatures.json` (`shape`). By base mesh: Barghest (Duskjackal,
Hollowfang, Gravehound, Risen hound), Griffon (Carrion kite, Stormkite, Hexcrow, Reedfowl, Nullwing, Gloamcrow,
Galeharrier, Mistlark), Magmaw (Murkjaw, its young, Prismback, Tunnel toad), Mountain dragon (Echobat), Basaltback
(Mudhulk, Riftback, Shroomback), Emberhorn (Burlbeast), Kilnmite (Bramblewasp, Cryptstalker, Tombback, Wickbeetle,
Crypt young, Spindleglass, Geodemite, Chimecrab, Mycomite, Webspinner, Sporeling), Ashstrider (Mirestrider, Palestrider,
Alicanto, Gallowshrike), Glowmoth (Driftmote, Hiveback, Wraithmoth, Hushwing), Pumicehog (Bogsnout, Gnawer, Withergoat,
Truffler, Gemsniffer, Truffle mole), Rimeclaw (Mossmaul, Sporestalker), Glacimaw (Gorgeleech, Vharra brood, Riftmaw,
Rootcoil), Woolly drudge (Rime-calf, Puffram, Fernsheep, Marrowgoat), Frostantler (Windmane, Voidstrider, Elder elk,
Dappledoe, Lumenhorn), Rimeshell (Bogstalker, Slimeshell, Druseback, Rotmaw, Glowgrub), Snowpuff (Lanterntoad,
Glimmerfinch, Mournhare, Puffwing, Capling), Pale hunter (Hush, Thornlynx). Left as made: each mesh's own species, the
humanoid rigs (their bosses have their own look code), the rift hearts / brood tears (not animals), and the eleven with no
description to design from (Fold wardens, Rodcrest, Voltfang, Thunderhorn, Cragstrider, Coilburrow, Galeback, Raiju,
Anvil mite, Latch leech, Puffball): their own cook writes theirs.

### 3.8 Build steps (each part compiled, committed and pushed)

1. **Part 1, the body layer**: FWRCreatureBodyInput, SetBodyChains, ApplyBody + ApplyShape in the proxy; creature
   look target / flinch / pant / wounded / slump / hungry; MulticastHit stores origin and damage; tuning `anim.look`,
   `anim.flinch`, `anim.pant`, `anim.wounded`, `anim.slump`, `anim.hungry`.
2. **Part 2, moods**: MoodRep / WatchRep, beats, wary, flush, herd flight, threat display, eat / flee moods; tuning
   `creatureAI.beat`, `creatureAI.wary`, `creatureAI.threat`, `creatureAI.mood`; the beats' and moods' poses.
3. **Part 3, gestures**: MulticastRowGesture from StartRow, gesture curves, tuning `anim.gesture.*`.
4. **Part 4, voice**: individual pitch, fallbacks, alert / flee / answer, packmate answers, whistle answer.
5. **Part 5, shapes**: shape parse + apply; the authored table in creatures.json.
6. Spot-check, doc final, LANDED.


## 4. Spot-check (read-through against the plan and THE BAR)

- **Found, fixed (8039632):** the wind-up glow's `EndTelegraph` set the overlay to nothing, so every creature coloured
  by an overlay (13 portal creatures on shared rigs) and every mutated tame lost that look after its first attack.
  `TelegraphCovered` keeps and restores it (WRCreature.cpp:959, :1036).
- **Found, fixed (c44214f):** when the rider's controller drives the mount pawn, the look used the rider's controller
  (null): the mount looked ahead only. It now takes whichever controller drives (WRCreatureBody.cpp BodyLookAt).
- **Checked:** the body layer runs after the pose lifts and before the feet are planted (WRAnimInstance.cpp:510, 528), so
  feet stay on the ground under every pose; proportions never touch legs.
- **Checked:** taming by offering still works: a slow approach (under 250 cm/s) never flushes, and only an approach
  straight at it counts (WRCreature.cpp TickWary); a creature gets used to someone who keeps their distance (12 s).
- **Checked:** the threat display never hands a free window: it ends the moment you close to its reach or strike it,
  and never plays when it was hit in the last 5 s, for bosses, minions, raiders or portal creatures.
- **Checked:** a mood can't stick: unrefreshed for 2 s it falls back to calm (TickMoodStale), so a biome hook taking
  the brain's turn never leaves an old pose.
- **Checked:** no sound stands in where there is no recording (sleep): nothing plays. The new kinds' stand-ins are
  data rows a species' own recording replaces.
- **Not covered by this pass (honest):** the 34 code-built bodies (desert, ocean, sky) draw themselves in their own
  code: no head aim, flinch, beats or gestures on them (LATER-COOKS line added). Bosses get head aim and flinch but no
  gestures (their rows carry no motion; their machines have their own clips).

## 5. What changed, where (summary)

| Part | Commit | Files |
|---|---|---|
| 1 Body language | 2e77a00 | WRAnimInstance.h/.cpp (FWRCreatureBodyInput, ApplyCreatureBody / Shape), WRCreatureBody.cpp (new), WRCreature.h/.cpp, WRCharacterBase.h/.cpp, tuning anim.* |
| 2 Moods | 02063ce | WRCreature.h/.cpp (MoodRep, WatchRep, TickBeat, TickWary, threat display, herd flight), tuning creatureAI.* |
| 3 Gestures | b8ca56d | WRCreatureCombat.cpp (MulticastRowGesture), tuning anim.gesture (28 motions) |
| 4 Voices | 1c42d17 | WRSound.cpp, WRAudio.h, WRAudioDirector.cpp, WRTaming.cpp, WRPlayerMenus.cpp, sounds.json voiceFallback / ambient |
| 5 Shapes | c44214f | creatures.json shape x73, WRCreature.h/.cpp, WRCreatureBody.cpp |
| 6 Looks that stay | 8039632 | WRCreature.h/.cpp (TelegraphCovered), WRGraveCreatures.h/.cpp (RaisedGrey), tuning necromancy.raisedLook |

No new binary files (code and JSON only).

## 6. Every built creature and boss, graded after the pass

Look: own art B+, shared mesh with its own shape B, shared mesh without one C+, code-built B-. Anim: skinned bodies
A- (clips + the body layer), code-built C. Voice: own rows B / B-, a family stand-in C+ (recordings: LATER-COOKS).
Behaviour: attack rows + moods A-, bosses B+ (their machines), the rest B.

| Creature | Biome / tier | Body | Look | Anim | Voice | Behaviour |
|---|---|---|---|---|---|---|
| ssarrok | desert / boss | code-built (skink) | B- | C (code body: no head aim / flinch layer) | C+ (lizard) | B+ (boss machine) |
| mirage_matriarch | desert / boss | code-built (oryx) | B- | C (code body: no head aim / flinch layer) | C+ (goat) | B+ (boss machine) |
| matriarch_mirage | desert / midland | code-built (oryx) | B- | C (code body: no head aim / flinch layer) | C+ (goat) | A- (2 attack rows, moods) |
| glass_empress | desert / boss | code-built (scorpion) | B- | C (code body: no head aim / flinch layer) | C+ (bug) | B+ (boss machine) |
| ahrumet | desert / boss | code-built (colossus) | B- | C (code body: no head aim / flinch layer) | C+ (golem) | B+ (boss machine) |
| duskjackal | desert / lowland | shared (SK_BARGHEST) + shape | B | A- (body layer) | B- (1 own rows + canine) | A- (4 attack rows, moods) |
| glasstail | desert / midland | code-built (scorpion) | B- | C (code body: no head aim / flinch layer) | B (5 own rows) | A- (6 attack rows, moods) |
| carrion_kite | desert / midland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | B- (2 own rows + raptor) | A- (3 attack rows, moods) |
| dunejaw | desert / highland | code-built (skink) | B- | C (code body: no head aim / flinch layer) | B (5 own rows) | A- (7 attack rows, moods) |
| gildback | desert / midland | code-built (devil) | B- | C (code body: no head aim / flinch layer) | B- (1 own rows + lizard) | A- (3 attack rows, moods) |
| dromad | desert / midland | code-built (camel) | B- | C (code body: no head aim / flinch layer) | B- (1 own rows + yak) | A- (4 attack rows, moods) |
| shimmerbuck | desert / midland | code-built (oryx) | B- | C (code body: no head aim / flinch layer) | B- (1 own rows + goat) | A- (3 attack rows, moods) |
| scarab_digger | desert / lowland | code-built (scarab) | B- | C (code body: no head aim / flinch layer) | B- (1 own rows + bug) | A- (1 attack rows, moods) |
| sandhen | desert / lowland | code-built (grouse) | B- | C (code body: no head aim / flinch layer) | C+ (reedfowl) | B (moods; biome hooks) |
| wellfrog | desert / lowland | code-built (frog) | B- | C (code body: no head aim / flinch layer) | C+ (lanterntoad) | B (moods; biome hooks) |
| old_barnacle | ocean / boss | code-built (crab) | B- | C (code body: no head aim / flinch layer) | C+ (bug, marked stand-in) | B+ (boss machine) |
| siren_eel | ocean / boss | code-built (eel) | B- | C (code body: no head aim / flinch layer) | C+ (wisp, marked stand-in) | B+ (boss machine) |
| reef_tyrant | ocean / boss | code-built (shark) | B- | C (code body: no head aim / flinch layer) | C+ (murkjaw, marked stand-in) | B+ (boss machine) |
| thalassor | ocean / boss | code-built (leviathan) | B- | C (code body: no head aim / flinch layer) | C+ (mother_maw, marked stand-in) | B+ (boss machine) |
| reefjaw | ocean / highland | code-built (shark) | B- | C (code body: no head aim / flinch layer) | B- (4 own rows + murkjaw) | A- (7 attack rows, moods) |
| tidecoil | ocean / midland | code-built (eel) | B- | C (code body: no head aim / flinch layer) | B- (3 own rows + rootcoil) | A- (4 attack rows, moods) |
| spinecrab | ocean / midland | code-built (crab) | B- | C (code body: no head aim / flinch layer) | B (7 own rows) | A- (5 attack rows, moods) |
| gullraider | ocean / lowland | code-built (gull) | B- | C (code body: no head aim / flinch layer) | B- (2 own rows + reedfowl) | A- (4 attack rows, moods) |
| wavecharger | ocean / midland | code-built (seahorse) | B- | C (code body: no head aim / flinch layer) | B- (2 own rows + herd) | A- (4 attack rows, moods) |
| driftback | ocean / midland | code-built (turtle) | B- | C (code body: no head aim / flinch layer) | C+ (mother_maw, marked stand-in) | A- (3 attack rows, moods) |
| glowray | ocean / lowland | code-built (manta) | B- | C (code body: no head aim / flinch layer) | B- (1 own rows + wisp) | A- (3 attack rows, moods) |
| pearldiver | ocean / lowland | code-built (otter) | B- | C (code body: no head aim / flinch layer) | B- (2 own rows + mustelid) | A- (2 attack rows, moods) |
| kelpgrazer | ocean / lowland | code-built (seacow) | B- | C (code body: no head aim / flinch layer) | C+ (mother_maw, marked stand-in) | A- (1 attack rows, moods) |
| tidefowl | ocean / lowland | code-built (puffin) | B- | C (code body: no head aim / flinch layer) | C+ (reedfowl, marked stand-in) | A- (1 attack rows, moods) |
| ripplefin | ocean / lowland | code-built (school) | B- | C (code body: no head aim / flinch layer) | C+ (critter, marked stand-in) | B (moods; biome hooks) |
| training_dummy | None / none | orb / none | B | n/a | C+ (size class) | B (moods; biome hooks) |
| magmaw | volcanic / highland | own (magmaw) | B+ | A- (body layer) | C+ (beast) | A- (4 attack rows, moods) |
| cinderfang | volcanic / lowland | own (cinderfang) | B+ | A- (body layer) | C+ (canine) | A- (3 attack rows, moods) |
| scorchwing | volcanic / midland | own (SK_MOUNTAIN_DRAGON) | B+ | A- (body layer) | C+ (flyer) | A- (3 attack rows, moods) |
| basaltback | volcanic / midland | own (basaltback) | B+ | A- (body layer) | C+ (herd) | A- (3 attack rows, moods) |
| emberhorn | volcanic / midland | own (emberhorn) | B+ | A- (body layer) | C+ (herd) | A- (3 attack rows, moods) |
| kilnmite | volcanic / lowland | own (kilnmite) | B+ | A- (body layer) | C+ (bug) | A- (2 attack rows, moods) |
| ashstrider | volcanic / lowland | own (ashstrider) | B+ | A- (body layer) | C+ (herd) | A- (2 attack rows, moods) |
| glowmoth | volcanic / lowland | own (glowmoth) | B+ | A- (body layer) | C+ (critter) | A- (1 attack rows, moods) |
| pumicehog | volcanic / lowland | own (pumicehog) | B+ | A- (body layer) | B- (4 own rows + critter) | A- (1 attack rows, moods) |
| rimeclaw | tundra / highland | own (rimeclaw) | B+ | A- (body layer) | C+ (beast) | A- (5 attack rows, moods) |
| glacimaw | tundra / midland | own (glacimaw) | B+ | A- (body layer) | C+ (bug) | A- (3 attack rows, moods) |
| gloomhowl | tundra / lowland | shared (SK_BARGHEST) | C+ | A- (body layer) | C+ (canine) | A- (4 attack rows, moods) |
| woolly_drudge | tundra / midland | own (woolly_drudge) | B+ | A- (body layer) | C+ (herd) | A- (3 attack rows, moods) |
| frostantler | tundra / midland | own (frostantler) | B+ | A- (body layer) | C+ (herd) | A- (4 attack rows, moods) |
| rimeshell | tundra / midland | own (rimeshell) | B+ | A- (body layer) | C+ (bug) | A- (3 attack rows, moods) |
| snowpuff | tundra / lowland | own (snowpuff) | B+ | A- (body layer) | C+ (critter) | A- (1 attack rows, moods) |
| aurora_fox | tundra / lowland | own (aurora_fox) | B+ | A- (body layer) | C+ (critter) | B (moods; biome hooks) |
| floeback | tundra / lowland | code-built (seal) | B- | C (code body: no head aim / flinch layer) | C+ (canine) | B (moods; biome hooks) |
| windmane | sky_islands / highland | shared (frostantler) + shape | B | A- (body layer) | B- (3 own rows + flyer) | A- (3 attack rows, moods) |
| wisp | volcanic / lowland | orb / none | B | n/a | B (5 own rows) | B (moods; biome hooks) |
| vulkarn | volcanic / boss | own (vulkarn) | B+ | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| skaldrith | tundra / boss | shared (SK_MOUNTAIN_DRAGON) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| obsidian_warden | volcanic / boss | own (obsidian_warden) | B+ | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (golem) | B+ (boss machine) |
| ignessa | volcanic / boss | own (ignessa) | B+ | A- (body layer) | C+ (wisp) | B+ (boss machine) |
| grimjaw | tundra / boss | own (grimjaw) | B+ | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| cinderhorn | volcanic / boss | shared (emberhorn) | B (boss look code) | A- (body layer) | C+ (size class) | B+ (boss machine) |
| old_tusker | tundra / boss | shared (woolly_drudge) | B (boss look code) | A- (body layer) | C+ (size class) | B+ (boss machine) |
| pale_hunter | tundra / boss | own (pale_hunter) | B+ | A- (body layer) | C+ (humanoid) | B+ (boss machine) |
| cinderfang_alpha | volcanic / boss | shared (cinderfang) | B (boss look code) | A- (body layer) | C+ (canine) | B+ (boss machine) |
| kilnmite_broodqueen | volcanic / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | C+ (bug) | B+ (boss machine) |
| mould_born | volcanic / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| brannoch | volcanic / boss | shared (grimjaw) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| bodvildr | volcanic / boss | shared (vulkarn) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| rimeclaw_alpha | tundra / boss | shared (rimeclaw) | B (boss look code) | A- (body layer) | C+ (beast) | B+ (boss machine) |
| glacimaw_elder | tundra / boss | shared (glacimaw) | B (boss look code) | A- (body layer) | C+ (bug) | B+ (boss machine) |
| frostbound_thurs | tundra / boss | shared (grimjaw) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| rime_witch | tundra / boss | shared (ignessa) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| hrimgerdr | tundra / boss | shared (ignessa) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| anvil_mite | volcanic / lowland | shared (kilnmite) | C+ | A- (body layer) | C+ (bug) | A- (2 attack rows, moods) |
| rime_calf | tundra / midland | shared (woolly_drudge) + shape | B | A- (body layer) | C+ (herd) | A- (3 attack rows, moods) |
| fold_warden_volcanic | volcanic / highland | shared (magmaw) | C+ | A- (body layer) | C+ (beast) | A- (4 attack rows, moods) |
| fold_warden_tundra | tundra / highland | shared (rimeclaw) | C+ | A- (body layer) | C+ (beast) | A- (5 attack rows, moods) |
| fallen_smith | volcanic / boss | shared (vulkarn) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| burnt_wyrm | tundra / boss | shared (SK_MOUNTAIN_DRAGON) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| voltfang | storm_peaks / midland | shared (cinderfang) | C+ | A- (body layer) | C+ (stormwolf) | A- (4 attack rows, moods) |
| thunderhorn | storm_peaks / highland | shared (emberhorn) | C+ | A- (body layer) | C+ (ram) | A- (5 attack rows, moods) |
| stormkite | storm_peaks / midland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | C+ (raptor) | A- (3 attack rows, moods) |
| raiju | storm_peaks / highland | shared (pale_hunter) | C+ | A- (body layer) | C+ (mustelid) | A- (7 attack rows, moods) |
| cragstrider | storm_peaks / midland | shared (ashstrider) | C+ | A- (body layer) | C+ (goat) | A- (2 attack rows, moods) |
| galeback | storm_peaks / midland | shared (woolly_drudge) | C+ | A- (body layer) | C+ (yak) | A- (2 attack rows, moods) |
| coilburrow | storm_peaks / midland | shared (pumicehog) | C+ | A- (body layer) | C+ (mole) | A- (2 attack rows, moods) |
| rodcrest | storm_peaks / lowland | shared (magmaw) | C+ | A- (body layer) | C+ (lizard) | A- (1 attack rows, moods) |
| puffram | storm_peaks / lowland | shared (woolly_drudge) + shape | B | A- (body layer) | C+ (sheep) | A- (1 attack rows, moods) |
| galvara_core | storm_peaks / midland | orb / none | B | n/a | C+ (size class) | B (moods; biome hooks) |
| bellwether | storm_peaks / boss | shared (emberhorn) | B (boss look code) | A- (body layer) | C+ (ram) | B+ (boss machine) |
| galvara | storm_peaks / boss | shared (SK_Griffon) | B (boss look code) | A- (body layer, 3 attack clips) | C+ (raptor) | B+ (boss machine) |
| coilback | storm_peaks / boss | shared (rimeshell) | B (boss look code) | A- (body layer) | C+ (mole) | B+ (boss machine) |
| kharvox | storm_peaks / boss | shared (SK_MOUNTAIN_DRAGON) | B (boss look code) | A- (body layer, 3 attack clips) | C+ (roc) | B+ (boss machine) |
| murkjaw | swamp / highland | shared (magmaw) + shape | B | A- (body layer) | B (7 own rows) | A- (7 attack rows, moods) |
| murkjaw_young | swamp / midland | shared (magmaw) + shape | B | A- (body layer) | C+ (murkjaw) | A- (7 attack rows, moods) |
| gorgeleech | swamp / midland | shared (glacimaw) + shape | B | A- (body layer) | B (7 own rows) | A- (4 attack rows, moods) |
| vharra | swamp / boss | shared (SK_MOUNTAIN_DRAGON) | B (boss look code) | A- (body layer, 3 attack clips) | B (7 own rows) | B+ (boss machine) |
| vharra_brood | swamp / lowland | shared (glacimaw) + shape | B | A- (body layer) | C+ (size class) | A- (2 attack rows, moods) |
| latch_leech | swamp / lowland | shared (glacimaw) | C+ | A- (body layer) | C+ (gorgeleech) | A- (1 attack rows, moods) |
| hexcrow | swamp / midland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | B (7 own rows) | A- (4 attack rows, moods) |
| bogstalker | swamp / midland | shared (rimeshell) + shape | B | A- (body layer) | B (7 own rows) | A- (5 attack rows, moods) |
| mirestrider | swamp / midland | shared (ashstrider) + shape | B | A- (body layer) | B (7 own rows) | A- (4 attack rows, moods) |
| mudhulk | swamp / midland | shared (basaltback) + shape | B | A- (body layer) | B (7 own rows) | A- (3 attack rows, moods) |
| bogsnout | swamp / lowland | shared (pumicehog) + shape | B | A- (body layer) | B (7 own rows) | A- (2 attack rows, moods) |
| lanterntoad | swamp / lowland | shared (snowpuff) + shape | B | A- (body layer) | B (7 own rows) | A- (1 attack rows, moods) |
| reedfowl | swamp / lowland | shared (SK_Griffon) + shape | B | A- (body layer, 2 attack clips) | B (7 own rows) | A- (1 attack rows, moods) |
| mother_maw | swamp / boss | shared (magmaw) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| hex_warden | swamp / boss | shared (ignessa) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| hex_totem | ossuary / midland | orb / none | B | n/a | C+ (size class) | B (moods; biome hooks) |
| bloat | swamp / boss | shared (glacimaw) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| slimeshell | swamp / lowland | shared (rimeshell) + shape | B | A- (body layer) | B (7 own rows) | A- (1 attack rows, moods) |
| hollowfang | wasteland / highland | shared (SK_BARGHEST) + shape | B | A- (body layer) | C+ (canine) | A- (6 attack rows, moods) |
| riftmaw | wasteland / highland | shared (glacimaw) + shape | B | A- (body layer) | C+ (murkjaw) | A- (5 attack rows, moods) |
| nullwing | wasteland / midland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | C+ (roc) | A- (5 attack rows, moods) |
| gnawer | wasteland / lowland | shared (pumicehog) + shape | B | A- (body layer) | C+ (mole) | A- (2 attack rows, moods) |
| hush | wasteland / highland | shared (pale_hunter) + shape | B | A- (body layer) | C+ (raptor) | A- (7 attack rows, moods) |
| voidstrider | wasteland / midland | shared (frostantler) + shape | B | A- (body layer) | C+ (herd) | A- (3 attack rows, moods) |
| riftback | wasteland / midland | shared (basaltback) + shape | B | A- (body layer) | C+ (lizard) | A- (4 attack rows, moods) |
| driftmote | wasteland / lowland | shared (glowmoth) + shape | B | A- (body layer) | C+ (wisp) | A- (1 attack rows, moods) |
| withergoat | wasteland / lowland | shared (pumicehog) + shape | B | A- (body layer) | C+ (goat) | A- (1 attack rows, moods) |
| unravelled | wasteland / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (golem) | B+ (boss machine) |
| mother_of_rifts | wasteland / boss | shared (magmaw) | B (boss look code) | A- (body layer) | C+ (mother_maw) | B+ (boss machine) |
| starving_one | wasteland / boss | shared (grimjaw) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (beast) | B+ (boss machine) |
| null_warden | wasteland / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (golem) | B+ (boss machine) |
| echo | wasteland / boss | shared (ignessa) | B (boss look code) | A- (body layer) | C+ (hexcrow) | B+ (boss machine) |
| vorhollow | wasteland / boss | shared (SK_MOUNTAIN_DRAGON) | B (boss look code) | A- (body layer, 3 attack clips) | C+ (boss) | B+ (boss machine) |
| brood_rift | wasteland / highland | shared (glowmoth) | C+ | A- (body layer) | C+ (wisp) | B (moods; biome hooks) |
| void_heart | wasteland / highland | shared (glowmoth) | C+ | A- (body layer) | C+ (wisp) | B (moods; biome hooks) |
| rift_heart | wasteland / highland | shared (glowmoth) | C+ | A- (body layer) | C+ (size class) | B (moods; biome hooks) |
| mossmaul | forest / highland | shared (rimeclaw) + shape | B | A- (body layer) | B (7 own rows) | A- (7 attack rows, moods) |
| thornlynx | forest / midland | shared (pale_hunter) + shape | B | A- (body layer) | B (7 own rows) | A- (6 attack rows, moods) |
| rootcoil | forest / highland | shared (glacimaw) + shape | B | A- (body layer) | B (7 own rows) | A- (5 attack rows, moods) |
| bramblewasp | forest / midland | shared (kilnmite) + shape | B | A- (body layer) | B (7 own rows) | A- (2 attack rows, moods) |
| elderelk | forest / midland | shared (frostantler) + shape | B | A- (body layer) | B (7 own rows) | A- (4 attack rows, moods) |
| burlbeast | forest / midland | shared (emberhorn) + shape | B | A- (body layer) | B (7 own rows) | A- (3 attack rows, moods) |
| truffler | forest / lowland | shared (pumicehog) + shape | B | A- (body layer) | B (7 own rows) | A- (2 attack rows, moods) |
| hiveback | forest / midland | shared (glowmoth) + shape | B | A- (body layer) | B (7 own rows) | A- (2 attack rows, moods) |
| fernsheep | forest / lowland | shared (woolly_drudge) + shape | B | A- (body layer) | B (7 own rows) | A- (1 attack rows, moods) |
| dappledoe | forest / lowland | shared (frostantler) + shape | B | A- (body layer) | B (7 own rows) | A- (1 attack rows, moods) |
| glimmerfinch | forest / lowland | shared (snowpuff) + shape | B | A- (body layer) | B (7 own rows) | B (moods; biome hooks) |
| old_gnarl | forest / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | B (7 own rows) | B+ (boss machine) |
| bramblequeen | forest / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| grey_matron | forest / boss | shared (pale_hunter) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| hornwarden | forest / boss | shared (frostantler) | B (boss look code) | A- (body layer) | B (7 own rows) | B+ (boss machine) |
| sylvaroth | forest / boss | shared (grimjaw) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | B (7 own rows) | B+ (boss machine) |
| gravehound | graveyard / midland | shared (SK_BARGHEST) + shape | B | A- (body layer) | B- (2 own rows + canine) | A- (7 attack rows, moods) |
| cryptstalker | graveyard / highland | shared (kilnmite) + shape | B | A- (body layer) | B- (4 own rows + bug) | A- (7 attack rows, moods) |
| wraithmoth | graveyard / midland | shared (glowmoth) + shape | B | A- (body layer) | B- (2 own rows + bramblewasp) | A- (4 attack rows, moods) |
| palestrider | graveyard / midland | shared (ashstrider) + shape | B | A- (body layer) | B- (1 own rows + elderelk) | A- (3 attack rows, moods) |
| tombback | graveyard / midland | shared (kilnmite) + shape | B | A- (body layer) | C+ (hiveback, marked stand-in) | A- (3 attack rows, moods) |
| gloamcrow | graveyard / lowland | shared (SK_Griffon) + shape | B | A- (body layer) | B- (1 own rows + hexcrow) | A- (1 attack rows, moods) |
| mournhare | graveyard / lowland | shared (snowpuff) + shape | B | A- (body layer) | C+ (mustelid, marked stand-in) | A- (1 attack rows, moods) |
| wickbeetle | graveyard / lowland | shared (kilnmite) + shape | B | A- (body layer) | C+ (bug, marked stand-in) | A- (1 attack rows, moods) |
| marrowgoat | graveyard / lowland | shared (woolly_drudge) + shape | B | A- (body layer) | C+ (goat, marked stand-in) | A- (1 attack rows, moods) |
| risen_mourner | graveyard / lowland | shared (obsidian_warden) | C+ | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (hex_warden, marked stand-in) | A- (3 attack rows, moods) |
| risen_hound | graveyard / lowland | shared (SK_BARGHEST) + shape | B | A- (body layer) | C+ (canine, marked stand-in) | A- (2 attack rows, moods) |
| risen_bearer | graveyard / midland | shared (grimjaw) | C+ | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (mudhulk, marked stand-in) | A- (2 attack rows, moods) |
| bell_ringer | graveyard / boss | shared (grimjaw) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (mossmaul, marked stand-in) | B+ (boss machine) |
| crypt_mother | graveyard / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | B- (1 own rows + bramblequeen) | B+ (boss machine) |
| crypt_young | graveyard / lowland | shared (kilnmite) + shape | B | A- (body layer) | B- (2 own rows + bug) | A- (1 attack rows, moods) |
| ossuary_lord | graveyard / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (mudhulk, marked stand-in) | B+ (boss machine) |
| gravedigger | graveyard / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (old_gnarl, marked stand-in) | B+ (boss machine) |
| nhal | graveyard / boss | shared (ignessa) | B (boss look code) | A- (body layer) | B- (3 own rows + grey_matron) | B+ (boss machine) |
| prismback | crystal / highland | shared (magmaw) + shape | B | A- (body layer) | C+ (lizard, marked stand-in) | A- (5 attack rows, moods) |
| spindleglass | crystal / highland | shared (kilnmite) + shape | B | A- (body layer) | C+ (bug, marked stand-in) | A- (4 attack rows, moods) |
| echobat | crystal / midland | shared (SK_MOUNTAIN_DRAGON) + shape | B | A- (body layer) | C+ (flyer, marked stand-in) | A- (3 attack rows, moods) |
| geodemite | crystal / lowland | shared (kilnmite) + shape | B | A- (body layer) | C+ (bug, marked stand-in) | A- (2 attack rows, moods) |
| lumenhorn | crystal / midland | shared (frostantler) + shape | B | A- (body layer) | C+ (elderelk, marked stand-in) | A- (5 attack rows, moods) |
| druseback | crystal / midland | shared (rimeshell) + shape | B | A- (body layer) | C+ (bug, marked stand-in) | A- (2 attack rows, moods) |
| gemsniffer | crystal / lowland | shared (pumicehog) + shape | B | A- (body layer) | C+ (truffler, marked stand-in) | A- (2 attack rows, moods) |
| alicanto | crystal / lowland | shared (ashstrider) + shape | B | A- (body layer) | C+ (reedfowl, marked stand-in) | B (moods; biome hooks) |
| chimecrab | crystal / lowland | shared (kilnmite) + shape | B | A- (body layer) | C+ (bug, marked stand-in) | A- (1 attack rows, moods) |
| mirrored_twins | crystal / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (boss) | B+ (boss machine) |
| mirrored_twin_double | crystal / highland | shared (obsidian_warden) | C+ | A- (body layer, 2 attack clips, no hit clip: flinch only) | C+ (boss) | B (moods; biome hooks) |
| echo_warden | crystal / boss | shared (frostantler) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| geode_tyrant | crystal / boss | shared (magmaw) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| lyrathis | crystal / boss | shared (ignessa) | B (boss look code) | A- (body layer) | C+ (boss) | B+ (boss machine) |
| lyrathis_copy | crystal / highland | shared (ignessa) | C+ | A- (body layer) | C+ (boss) | B (moods; biome hooks) |
| galeharrier | sky_islands / midland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | B- (2 own rows + raptor) | A- (4 attack rows, moods) |
| skyray | sky_islands / highland | code-built (ray) | B- | C (code body: no head aim / flinch layer) | B (5 own rows) | A- (5 attack rows, moods) |
| cloudback | sky_islands / midland | code-built (whale) | B- | C (code body: no head aim / flinch layer) | B- (2 own rows + mudhulk) | A- (3 attack rows, moods) |
| mistlark | sky_islands / lowland | shared (SK_Griffon) + shape | B | A- (body layer, 3 attack clips) | B- (1 own rows + glimmerfinch) | B (moods; biome hooks) |
| puffwing | sky_islands / lowland | shared (snowpuff) + shape | B | A- (body layer) | B- (1 own rows + reedfowl) | A- (1 attack rows, moods) |
| gallowshrike | sky_islands / midland | shared (ashstrider) + shape | B | A- (body layer) | B- (2 own rows + hexcrow) | A- (5 attack rows, moods) |
| hushwing | sky_islands / highland | shared (glowmoth) + shape | B | A- (body layer) | B- (2 own rows + reedfowl) | A- (4 attack rows, moods) |
| sailbell | sky_islands / lowland | code-built (bell) | B- | C (code body: no head aim / flinch layer) | B- (2 own rows + wisp) | A- (1 attack rows, moods) |
| kyrra | sky_islands / boss | shared (SK_Griffon) | B (boss look code) | A- (body layer, 3 attack clips) | C+ (raptor, marked stand-in) | B+ (boss machine) |
| bridge_sentinel | sky_islands / boss | shared (obsidian_warden) | B (boss look code) | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (golem, marked stand-in) | B+ (boss machine) |
| old_gulper | sky_islands / boss | code-built (whale) | B- | C (code body: no head aim / flinch layer) | B- (4 own rows + mother_maw) | B+ (boss machine) |
| aeolith | sky_islands / boss | code-built (serpent) | B- | C (code body: no head aim / flinch layer) | B- (3 own rows + roc) | B+ (boss machine) |
| sporestalker | fungal / highland | shared (rimeclaw) + shape | B | A- (body layer) | B- (2 own rows + thornlynx) | A- (6 attack rows, moods) |
| rotmaw | fungal / highland | shared (rimeshell) + shape | B | A- (body layer) | B- (4 own rows + murkjaw) | A- (7 attack rows, moods) |
| mycomite | fungal / lowland | shared (kilnmite) + shape | B | A- (body layer) | B- (1 own rows + bug) | A- (2 attack rows, moods) |
| webspinner | fungal / midland | shared (kilnmite) + shape | B | A- (body layer) | B- (1 own rows + bug) | A- (5 attack rows, moods) |
| tunnel_toad | fungal / midland | shared (magmaw) + shape | B | A- (body layer) | B- (1 own rows + lanterntoad) | A- (4 attack rows, moods) |
| shroomback | fungal / midland | shared (basaltback) + shape | B | A- (body layer) | B- (1 own rows + mudhulk) | A- (2 attack rows, moods) |
| truffle_mole | fungal / lowland | shared (pumicehog) + shape | B | A- (body layer) | C+ (mole, marked stand-in) | A- (2 attack rows, moods) |
| glowgrub | fungal / lowland | shared (rimeshell) + shape | B | A- (body layer) | B- (1 own rows + slimeshell) | B (moods; biome hooks) |
| capling | fungal / lowland | shared (snowpuff) + shape | B | A- (body layer) | C+ (glimmerfinch, marked stand-in) | B (moods; biome hooks) |
| puffball | fungal / lowland | shared (snowpuff) | C+ | A- (body layer) | C+ (bloat, marked stand-in) | B (moods; biome hooks) |
| sporeling | fungal / lowland | shared (kilnmite) + shape | B | A- (body layer) | B- (1 own rows + bug) | A- (1 attack rows, moods) |
| sporemother | fungal / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | B- (1 own rows + bramblequeen) | B+ (boss machine) |
| rot_king | fungal / boss | shared (rimeshell) | B (boss look code) | A- (body layer) | C+ (mother_maw, marked stand-in) | B+ (boss machine) |
| weaver | fungal / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | B- (1 own rows + hexcrow) | B+ (boss machine) |
| mycelorn | fungal / boss | shared (kilnmite) | B (boss look code) | A- (body layer) | C+ (sylvaroth, marked stand-in) | B+ (boss machine) |
| fungal_behemoth | fungal / highland | shared (grimjaw) | C+ | A- (body layer, 3 attack clips, no hit clip: flinch only) | C+ (mossmaul, marked stand-in) | A- (2 attack rows, moods) |

## 7. Leftovers written to the share's LATER-COOKS

- The 34 code-built bodies (desert, ocean, sky) need the body layer's head aim / flinch / beats / gestures drawn in their
  own code (WRSealBody-style bodies), reading the same MoodRep / WatchRep / MulticastRowGesture.
- Species' own `alert` / `flee` / `answer` recordings for the voice pass (rows `creature.<id>.<kind>`), and a sleep
  breath per size class.
- Shapes for the eleven shared-mesh species with no description (Fold wardens, Rodcrest, Voltfang, Thunderhorn,
  Cragstrider, Coilburrow, Galeback, Raiju, Anvil mite, Latch leech, Puffball) once their looks are written.
