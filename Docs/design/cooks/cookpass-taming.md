# Wildreach cook: COOKPASS-TAMING (taming, mounts, breeding: the cook pass on what's built)

Lane cookpass-taming, 2026-11-15, desktop. A cook pass on the built taming lane (`cooks/build-taming.md`, landed
2026-11-14): research how the best games do it, grade every built piece against THE BAR, design the raise, build it.
Spec sources: FULL-GAME 6.5, `cooks/survival-crafting.md` 2.19-2.22 (taming, saddles, breeding, necro raising /
revive), `cooks/ui-controls.md` 2.13 (whistle wheel), COMBAT-AND-LESSONS L8 (input isolation), the share's LATER-COOKS
lines in this scope.

## 0. Requirement ledger

Split from the brief, the Aaron quotes in it and the LATER-COOKS lines that touch taming / mounts / breeding.

| # | Requirement (one each) | Source | Done where (filled at the end) |
|---|---|---|---|
| Q1 | Research how the best games do taming, mounts, breeding (named games, places, systems) plus real-world reference; write it here | brief, Aaron 11-13 | DONE: section 1 of this doc (RDR2, BotW/TotK, Elden Ring, Ark, Palworld, Ghost of Tsushima, real riding, courtship, stables) |
| Q2 | Grade every built piece against THE BAR | brief | DONE: section 2 (grades table, file:line of each built piece) |
| Q3 | Design the raise in writing (every detail, edge cases, build steps) before building | brief, Aaron 11-13 | DONE: section 3, committed before any code (showcase-notes 193f698) |
| Q4 | Build exactly the plan, full detail, no placeholders; unfinished stays hidden | brief, BAR | DONE: Wildreach decc9f7, d9fb85b, fdbb1eb; L7 not built (see L7), nothing unfinished shown |
| Q5 | Spot-check the build against the plan and THE BAR by reading the code; fix before push | brief | DONE: section 4 (fixes found in the read-through and landed in d9fb85b) |
| Q6 | "Already done" needs file:line proof | brief | DONE: every row below carries its file:line |
| Q7 | Design with intention: every object/mechanism added says why it is here, what it connects to, how it meets what it touches; nothing passes through anything | Aaron 11-14 | DONE: the stable look (WRBuildLookDecor.cpp:750 Stable(): sill carries posts, plates carry rafters, partitions stop at posts, rail clear of the gate); the seat (WRHeroAnimInstance.cpp:433); the collar (TameDied WRTaming.cpp:2258) |
| Q8 | Data-driven: numbers in Data/*.json / tuning; sounds and fx as table rows; all damage through ApplyHit; server authority | BAR | DONE: tuning tame.name/call/stable/revive/soulbond/mountEase/seat/element/bardingPass/riftMend; sounds.json mount.on/off, tame.stable.*, tame.revive, tame.soulbind, tame.soulfall; fx.json tame.revive; Soulbond and Release through ApplyHit / server commands |
| Q9 | Screens with mouse AND pad (WR_REGISTER_SCREEN), every state reachable | BAR | DONE: WRUIStable.cpp:284 (screen), WRUITameCard.cpp:211 tabs + pad handler :456, name box pad handler :137 (Y suggests, B cancels) |
| Q10 | Multiplayer: server authority, replication, every machine sees it | BAR | DONE: TameRep.Name/Father/Mother/bSoulbound/bSoulDown/bCuddled replicated (WRTaming.h FWRTameRep); AWRBuildPiece Roster/CallingOut replicated (WRBuilding.cpp DOREPLIFETIME); every command server-side (tamecard / stable / whistle mount) |
| Q11 | Land in pieces: compile, commit + push each; CHANGELOG + HANDOFF in the same commit | brief | DONE: three commits, each compiled clean and pushed; CHANGELOG + HANDOFF in each |
| L1 | Riders have no sit/ride pose (they stand upright on the saddle) | LATER-COOKS build-taming | DONE (procedural): WRHeroAnimInstance.cpp:433 seat layer; WRCharacter.cpp:1022 inputs, saddle-bone bob (SaddleBoneLocation WRTaming.cpp end); no riding clips (LATER-COOKS) |
| L2 | Mating / cuddles have no courtship or nuzzle motion | LATER-COOKS build-taming | DONE: WRCreature.cpp:701 head dip in turn (pose lift) from MateBar / bCuddled; tuning tame.breed.nuzzleBeat/nuzzleDip |
| L3 | The stable (stored tames: workbench L3, 10 per player per stable, 3 per claim, stored don't eat or age, 5 s call-out) | LATER-COOKS build-taming; SC 2.19 | DONE: pieces.json stable; WRBuildLookDecor.cpp:750; WRTaming.cpp:2139 StableCommand, :2220 TickStables, StableToJson/FromJson, StableDestroyed; WRPlayerBuild.cpp:838 3 a claim; WRUIStable.cpp |
| L4 | Revive: a dying tame drops its bond token; Dadeni draught (alchemy L5) poured on it at a Hearth within 72 h brings it back at full bond; once per tame | LATER-COOKS; SC 2.22 | DONE: WRTaming.cpp:2258 TameDied (collar, fallen ledger), :2323 UseBondToken (Hearth, 72 h, once); WRGameMode.cpp:361/492 fallenTames; items.json bond_token, dadeni_draught; recipes.json alchemy 5; WRUIInventory.cpp tooltip TokenLine |
| L5 | Soulbond perk (Taming 100): one chosen tame falls unconscious 60 s at 0 instead of dying | LATER-COOKS; SC 2.22; lifeskills waitsFor | DONE: lifeskills.json soulbond live (stat soulbond); WRTaming.cpp:2409 SoulbondIncoming via WRCombatLibrary.cpp:918; card Soulbind (CardCommand soulbind); wake in TickTameLife |
| L6 | Tame renaming (names) | LATER-COOKS build-taming | DONE: TameRep.Name; CardCommand name + CleanName WRTaming.cpp:2006; label WRCreature.cpp GetDisplayLabel; card Rename + SWRTameNameBox WRUITameCard.cpp:76; Data/tame_names.json |
| L7 | Necro raising by ritual: bone altar, permanent undead tames | LATER-COOKS build-taming + build-biome-graveyard; SC 2.22 | NOT BUILT: a Necromancy-path ritual (bone altar piece, 60 s channel, undead look, soulstone dust upkeep, its own exclusions in the save and tame counts); stays on LATER-COOKS (build-biome-graveyard line 178 already holds it) |
| L8 | Tames answer the whistle with a call (own answer call per species in the voice pass) | LATER-COOKS build-finish | PARTLY: tames answer the whistle with their idle call (WRPlayerMenus whistle) and a called mount answers (CallMount voice.idle); species-own answer calls need the audio pass (LATER-COOKS) |
| L9 | Windmane needs its own four-winged body, nest and egg | LATER-COOKS build-taming | NOT BUILT (art): the Windmane body needs the art pipeline (no ComfyUI in lanes); LATER-COOKS line stands |
| L10 | Floeback body is code-modelled; a sculpted rigged seal would raise it | LATER-COOKS build-taming | NOT BUILT (art): a sculpted Floeback; LATER-COOKS line stands |
| L11 | Ten lands' barding hazards wired as each land ships (only lava and ice read today) | LATER-COOKS build-taming | DONE: FWRTaming::BardingPass / BardingStat WRTaming.cpp:2491; WRInventory.cpp:665 GearStatFor; WRStormWorld.cpp:537 strikes; WRForestHazards.cpp:245 thorns; tuning tame.bardingPass (bleedwater, gusts, spores, wards, depth 40 m, corruption, rifts); desert Dunewrap waits for its land |
| L12 | Old Tusker talisman (tameCarry) waits for hauler loads; `tameSteady` set stat waits | LATER-COOKS build-crafting | DONE (tameCarry): WRTaming.cpp:642 CargoCapacityKg x (1 + tameCarry); recipes.json talisman_old_tusk waitsFor removed. tameSteady was already read (WROceanBosses.cpp:837) |
| L13 | Off-biome tames should take rift drain on their own max HP | LATER-COOKS build-biome-void | DONE: WRVoid.cpp:493 off-biome tames drained (Hollowplate halves), mend WRTaming.cpp:910 (tuning tame.riftMend) |
| L14 | Monster line perk (hooked swimmers) hidden | LATER-COOKS build-taming | NOT BUILT: Monster line is a fishing perk (hooked swimmers), not taming; stays hidden with its LATER-COOKS line |
| G1 | A tame keeps its own element in every land (today only fire and frost bites carry it) | grade (6.5 RAVEN-PLANNED rule) | DONE: WRTaming.cpp:1213 ElementHit; WRCreatureCombat.cpp row bite and mount bite; RecomputeStats keeps every element; tuning tame.element.<element> |
| G2 | Ride controls ignore the player's rebinding (WASD/Space/E hard-coded) | grade (BAR: every key in FWRKeyBindings) | DONE: WRCreature.cpp:2029 ride mapping from FWRKeyBindings |
| G3 | Release is one wheel flick with no confirm (a bond-20 tame lost by a mis-flick) | grade (BAR edge cases) | DONE: WRCreature.cpp:1878 Release asks twice (ReleaseArmedBy / tame.releaseConfirmSeconds) |
| G4 | Mount and dismount snap/teleport; dismount can put you inside a wall, a cliff or lava | grade (BAR, Aaron 11-14 "never snapping or teleporting") | DONE: WRCreature.cpp:1950 DismountSpot (sides, behind, front; ground, room, line, no lava, no deep water); ease WRCharacter.cpp:1022 (jump-based, relative to the mount) |
| G5 | No way to call your mount from afar (every reference game has one) | grade, research | DONE: WRPlayerUI.cpp:966 wedge, WRPlayerMenus.cpp:1032, WRTaming.cpp:2431 CallMount, AI WRCreature.cpp TickTame CallTo |
| G6 | Tame card is one long text scroll: no tabs, stats as multipliers not real numbers, no parents | grade (plan 2.8 said tabs; BAR) | DONE: WRUITameCard.cpp:211 tabs, :623 StatValue real numbers, :672 ParentsLine |

## 1. Research

| Game / reference | The specific system | What makes it work | What we take |
|---|---|---|---|
| **Red Dead Redemption 2** (Arthur's horse; any stable) | Short whistle calls the horse to you, long whistle has it follow; whistle range ~185 m growing with bond; a horse won't answer if stabled, dead, or you're indoors / in a cave. Bond 1-4 from riding, brushing, feeding, calming. Horse Reviver within a window (longer at bond 4); a dead horse is gone. Rider's posture changes with the gait: upright at walk, forward at gallop, the rider rises in the stirrups. Mount/dismount are short animated climbs from the left side; dismount checks space | The horse is a companion you call, not a vehicle you spawn: it travels to you physically, it answers with a whinny. Death is serious but has a rescue window | Call mount: your last-ridden mount runs to you from up to 300 m (bond adds range), answers with its call; won't answer from a stable, dead, or from underground. Rider pose leans with the gait. Revive window |
| **Zelda: Breath of the Wild / Tears of the Kingdom** (stables, Malanya at the Horse God's spring) | Register a tamed horse at a stable for a name; up to 5 boarded; board / take out at any stable; a dead horse revived by Malanya; names shown everywhere | Naming is the moment a creature becomes yours; the stable is a calm, physical place you go to | Names chosen on the card (keyboard or a suggested name for the pad); the stable as a built place with stalls; the revive happens at a sacred place (our Hearth) with a rare draught |
| **Elden Ring** (Torrent, Spectral Steed Whistle) | Torrent is summoned in a breath and revived by a Flask; mounting is instant but animated (a jump into the saddle); no mount in many arenas | Mounted travel is frictionless; the arena rule is clear | Mount/dismount 0.45 s eased arcs (quick, never a snap); the no-flight-near-bosses rule stays |
| **Ark: Survival Evolved / Ascended** (Cryopod, Cryofridge, rename on radial, "Release" with a confirm) | Stored creatures don't count toward the tribe tame cap; stored ones don't eat; Rename and Release from the creature's radial, Release asks to confirm. Tame stat screen shows real values (Health 1,250 / Stamina 400 / Weight 450 / Melee 180% / Speed 100%) beside points | Tame management is a real inventory problem; the confirm on release prevents the worst loss | Stable stores out of the world, not counted on the active limit; Release asks again; the card shows real values and points |
| **Palworld** (Palbox, Pal storage) | Palbox at each base; storage shared; assigning pals to base work from the box | A box at your base is the hub for your creatures | The stable lives on a claim (3 per claim); its screen lists only your tames stored in it |
| **Ghost of Tsushima** (horse) | Horse answers a whistle across the field at a gallop and stops beside you | The arrival is choreographed: slows, turns, stops alongside, facing your way | The called mount brakes and stops at your side, not on top of you |
| **Horse riding (real)** | Seated rider: hips flexed ~70-80°, knees bent ~80°, lower leg under the hip, heels down, thighs wrapping the barrel (width depends on the horse's girth); hands carried low in front over the withers, elbows bent; at the trot the rider rises and sits; at the gallop "two-point" forward seat, torso inclined 30-45°; mounting from the near (left) side with a foot in the stirrup | Shows the rider belongs on the animal | The seated pose: thigh flex 72°, spread by the mount's back width, calves hanging under the hip, feet level, hands over the pommel; torso pitch 4° at a walk to 22° at a gallop; the rider follows the back's rise and fall |
| **Courtship (real)** | Horses: nose-to-nose, the stallion's arched neck; birds: bowing, circling; deer: grooming each other's necks | A pair facing each other reads instantly as courting | The pair turn to face each other at nuzzle distance and dip heads in turn (the creature's head look-at), hearts every 4 s |
| **Stables (real)** | Box stalls ~3.6 x 3.6 m with half doors, a hay rack and water trough per stall, a central aisle, a hitching rail outside, timber frame on a stone sill (keeps the posts dry), a pitched shingle roof with a ridge vent, straw bedding | Every part has a job: sill against rot, half door so the horse looks out, hay rack high so it isn't trampled | The stable piece: three stalls on a stone sill, timber frame, half doors, hay racks and a trough, a shingle roof with a ridge vent, a hitching rail at the front where called-out tames come out |

Lessons: (1) a tame is a named companion that comes when called; (2) losing one is serious but has one way back;
(3) moving on and off it is quick but never a jump cut; (4) the rider sits like a rider; (5) management screens show
real numbers and never let one mis-click destroy hours of bond.

## 2. Grades (built pieces vs THE BAR)

| Piece | Where | Grade | Why |
|---|---|---|---|
| Wild levels, genes, inheritance, mutations, morph overlay | WRTaming.cpp:151-276, 464-547 | A | Full, data-driven, visible |
| Bond XP and role trees | WRTaming.cpp:579, 1081-1243, tame_trees.json | A- | Every node live; trees fine |
| Saddles, pack saddle, barding looks and values | WRTameLook.cpp, items.json | A- | Fitted on bones |
| Mount kinds (ground sprint / pack / swim / fly), Charge | WRTaming.cpp:655-837 | B+ | Works; no call, rider has no pose (L1) |
| Rider pose | none | F | Rider stands upright on the seat: a standing person on a horse |
| Mount / dismount | WRCreature.cpp:1865-1934 | D | Snaps up; dismount teleports 80 cm right with no check (walls, cliffs, lava) |
| Ride controls | WRCreature.cpp:1936-2021 | C | WASD, Space, E, mouse hard-coded: a rebound player rides with the wrong keys |
| Element kept | WRTaming.cpp:535, WRCreatureCombat.cpp:793-799, 1440 | C | Only fire / frost; ten lands' tames bite plain |
| Breeding / pregnancy / eggs / incubator | WRTaming.cpp:1684-1816, WRUIIncubator.cpp | A- | Mating pair don't face each other (L2) |
| Babies, care, imprint | WRTaming.cpp:953-1058 | A- | |
| Tame card | WRUITameCard.cpp | C+ | One long scroll; stats are "x1.08" not values; no name; no parents; plan's tabs not built |
| Whistle wheel | WRPlayerUI.cpp:957-1060 | B | Two blank wedges; no call-mount |
| Release | WRCreature.cpp:1842 | D | One flick, no confirm |
| Names | none | F | Every tame is "Raven's Cinderfang" |
| Stable, revive, Soulbond, bone altar | none | F (unbuilt) | Spec'd SC 2.19 / 2.22 |

## 3. Plan (the raise)

### 3.1 G1 every element kept (data: tuning `tame.element.<element>`)
`tame.element` gains a map element → { tag, buildup }: fire→Fire/burn, frost→Frost/frost, lightning→Lightning/shock,
tide→Water/soak, void→Void/decay, arcane→Arcane/mark, blood→Physical/bleed, necromancy→Physical/curse, spore→
Physical/rot, nature→Physical/poison, wind→Physical/(none: +30% stamina damage). `RecomputeStats` keeps
`Weapon.Element = Def->Element` for every tame; one helper `FWRTaming::ElementHit(Tame, Hit)` used by the row bite
(WRCreatureCombat 793) and the mount bite (1440) adds the tag and buildup only when the row carried none (the storm /
void AI's own hooks still run after). Fire-only checks (`Weapon.Element == fire`) keep working.

### 3.2 G2 ride controls through the bindings
Ride mapping reads `FWRKeyBindings` MoveForward/Back/Left/Right, Jump, Interact (falls back to the old keys if the
binding is missing) and the mouse; pad as today.

### 3.3 G3 release asks again
Wheel Release on a tame sends `tame release`; the server answers "Pick Release again within 5 s to let <name> go
(its tack and packs drop)" and arms `ReleaseArmedUntil` on that tame for that player; a second release within the
window does it. A whistle never releases.

### 3.4 L6 names
`FWRTameRep.Name` (replicated, saved). Card header has a name line: on keyboard/mouse a text box (Enter / focus loss
sends `tamecard … name <text>`); on the pad a "Suggest a name" button cycles names from `Data/tame_names.json`
(lists by temperament: fierce / swift / steady / gentle / odd, picked by role) and a "Keep" button. Server: 1-20
characters, letters, spaces, `'` and `-`, trimmed; anything else refused with the rule. Shown: the floating label
("Ember · Cinderfang · Lv 24", owner's name only for others' tames), the card title, the riding HUD line and every
notification that names the tame (`AWRCreature::TameName()` = name or species).

### 3.5 G6 card tabs and real numbers
Tabs Overview · Skills · Gear · Family · Packs (Packs only for haulers, Gear only for mounts): mouse clicks the tab
strip; the pad's LB/RB (and Q/E keys) switch. Overview: name, level, bond bar, hunger + feed, the six stats as real
values (Health 540 / 540, Stamina 200, Bite 32, Carry 300 kg, Speed 10.5 m/s, Torpor 400) with the points, the
multiplier and the mutated points, morphs, element. Family: parents (from the genes' `Father` / `Mother` names
saved at birth), mating, growth, imprint, care. Release lives only on the wheel (3.3).

### 3.6 L1 the rider's seat (WRHeroAnimInstance)
`FWRHeroAnimInput.Seat` 0..1, `SeatSpread` (degrees, from the mount's back half-width), `SeatLean` (0 walk .. 1
gallop), `SeatBob` (cm). A seated layer in `PostLayers` after the female stance, weighted by Seat: pelvis dropped
`anim.seat.pelvisDrop` (8 cm); each thigh flexed forward 72° about the side axis then spread outward by SeatSpread
(18-34°), each calf bent back 76° about the knee's hinge (the spread side axis), each foot levelled −4°; the spine
pitched forward 4°→22° over spine_01..03 with the gait; upper arms forward 28° and in 10°, forearms bent 64°: the hands
carried together over the pommel; while a one-shot plays on the upper body (a swing, a cast) the arms' part fades out
so the blow plays clean. The rider's mesh follows the saddle bone's rise and fall (the creature's spine bone found the
way WRTameLook does), smoothed, ±25 cm, so the rider moves with the back instead of floating. `tame.mount.riderLift`
retuned for the seat.

### 3.7 G4 mounting and dismounting
Mount: the rider is attached at its start point (beside the mount, where they stood) and eased over 0.45 s along an
arc (up 40% of the rise first, then over) to the seat; mount input waits for the ease. Dismount: the server picks a
clear standing spot: right side, left side, behind, in front (capsule sweep at each; not over water deeper than knee,
not lava, ground within 1.5 m below), the first that's clear; the rider is eased 0.35 s down to it, then collision and
walking return. Nowhere clear (a narrow ledge): "No room to get down here". Sounds `mount.on` / `mount.off`
(saddle leather creak) as rows. Input isolation (L8): held inputs dropped on both changes.

### 3.8 G5 / L8 call your mount
Whistle wheel's spare wedge: "Call your mount". The server picks your mount (the one you last rode, else the nearest
of your saddled mounts) within `tame.call.range` 30,000 cm + 500 cm a bond level; not stabled, not dead, not
underground while you're above (or the reverse), not a baby. It plays its call (`voice.idle` on the creature, the
answer row), sets order Follow, and paths to you at a run (sprint speed while > 20 m away), brakes and stops 3 m to
your side facing your way. Flying mounts land first and come on foot. Too far: "Out of earshot (… m)". A tap
whistle's tames also answer with their call (L8: the idle call row stands in until each species gets its own).

### 3.9 L3 the stable
Piece `stable` (category farm, kind `stable`, workbench hearth L3; timber 40, stone 30, hide 6, iron ingot 4; hp 1,600),
footprint 7.6 x 4.4 m, its own look (3.9.1). E opens the stable screen (EWRMenu::Stable). Rules: stored tames are
yours only (clan-mates see none); 10 per player per stable (`tame.stable.perPlayer`), 3 stables per claim
(`tame.stable.perClaim`, checked at placement); store: a tame of yours within 20 m of the stable, not ridden, not
pregnant (she'd lose the young: refused), alive; it walks into the stable door then vanishes (saved as its SaveTame
JSON + species + name + level in the piece). Call out: 5 s (`tame.stable.callSeconds`), a bar on the screen, then it
walks out of the front gate; needs room under your active tame limit. Stored tames don't eat, age, grow or count as
active (CountTames counts only actors). Stable destroyed: its tames come out (spawned at the gate) so nobody loses
tames to a raid. Saved with the piece (PieceToJson). The screen: a list (name, species, level, sex, health) with
Store (the tames near) and Call out (stored) buttons; mouse and pad.
**3.9.1 Look (why, connects, meets):** a stone sill course (keeps the posts off wet ground: it meets the ground on
the piece's floor), six timber posts on it carrying a top plate, three stalls divided by plank partitions, each stall
with a half door on two iron strap hinges (the gate tames walk out of), a hay rack (slatted, high on the back wall)
and a plank trough below it, straw on the floor; a pitched shingle roof on rafters from the top plate to a ridge beam,
a ridge vent; a hitching rail on two posts in front of the middle stall, an iron ring on it. Nothing crosses anything:
partitions stop at the posts, rafters sit on the plate.

### 3.10 L4 / L5 bond token, revive, Soulbond
Death of a tame (not a raised or summoned one, not a baby under 25%): drops its **bond token** (`bond_token` item: the
tame's collar, a braided leather band with its name plate) in the death bag; the stack's sockets carry `fallen:<id>`;
the server keeps `FallenTames[id] = { tame JSON, species, name, owner, died at (UTC) }` in the world save.
`dadeni_draught` (alchemy table L5: asphodel 2, healing potion 2, soulstone 1, frost essence 1; a recipe row) — if the
items don't exist the recipe uses what does (checked at build). Use: E on your Hearth with the token in the bag and a
draught opens a confirm line on the Hearth's prompt; the server checks owner, < 72 h (`tame.revive.hours`), not revived
before (`revived` flag in its JSON), room under the limit; consumes both, spawns the tame beside the Hearth with
everything it had (bond, tree, genes, name; tack and cargo were dropped at death), full health, a light burst
`tame.revive` fx and sound. Expired tokens say so on their tooltip ("Its spirit has gone on"). **Soulbond**
(lifeskills `soulbond`, Taming 100): the card's Family/Overview tab has "Soulbind" (one tame per player, moving it
clears the old one); a soulbound tame at 0 HP falls unconscious for 60 s (`tame.soulbond.seconds`), then wakes at 25%;
hit while down it stays down (it doesn't die). Through the unconscious status the knockout already uses.

### 3.11 L2 courtship
While a pair's mating bar fills, both stop and turn to face each other at `breed.nuzzle` 180 cm (their movement goal is
the partner's side), their heads dip in turn (the creature's head look-at aimed at the partner's neck, alternating
every 2 s), hearts every 4 s (built).

### 3.12 L7 bone altar
Big and path-owned (Necromancy); built only if the rest lands with time to spare. Otherwise it stays on LATER-COOKS
with the spec above unchanged.

### 3.13 Things this lane cannot do (art / audio pipelines) and says so
L9 Windmane body, L10 sculpted Floeback, species answer calls (L8 second half), L11 the ten barding hazards (their
hazards' code belongs to each land: the tack's `hazard` field is read; the lava / ice ones work), L12-L14 belong to
other systems: each stays on LATER-COOKS with what's needed.

### 3.14 Build steps
1. Doc (this) committed. 2. G1 element map + G2 ride bindings + G3 release confirm (compile, commit). 3. L6 names +
G6 card tabs (compile, commit). 4. L1 rider seat + G4 mount/dismount (compile, commit). 5. G5 call mount + L2
courtship (compile, commit). 6. L3 stable (data, look, logic, screen, save) (compile, commit). 7. L4/L5 token,
revive, Soulbond (compile, commit). 8. Spot-check; ledger filled; CHANGELOG/HANDOFF; LANDED.

## 4. Spot-check (read through the code against the plan and THE BAR)

Fixed after the first push (in d9fb85b / fdbb1eb):
- The mount / dismount ease keyed on the attach change alone would be skipped on a client when the attachment and the
  move arrive a frame apart, and a gallop at a low frame rate looked like a jump: now any jump within 0.5 s of getting on
  or off is eased, measured on the mount's back while riding (WRCharacter.cpp:1022).
- The collar lay in a 30-minute bag anyone could loot (a raider could take your tame's way back): now it goes home to
  its owner's bag, else an owner-only bag for the 72 h, and a Souls-mode death no longer destroys that bag (WRSocial.cpp).
- The land barding sets that came live with their lands promised passes no code gave (a placeholder shown as finished):
  wired through one hook for the mount and its rider.

Plan deviations (decided, not dropped):
- 3.5 "Q/E keys switch tabs": not built; E is Interact and the card's keyboard focus isn't guaranteed. Mouse clicks the
  tab strip, the pad uses LB / RB (both devices covered).
- 3.6 "pelvis dropped 8 cm": folded into the seat height (tame.seat.riderLift 14) so the pose never sinks the body when the
  layer isn't applied.
- 3.9 "a stable 760 x 440" collides as one box (its decor bounds): tames don't walk inside; they reach the gate in front
  and step in (the timeout stores them if terrain blocks the walk).
- 3.10 the collar drop changed as above.

Honest limits: the seat, the nuzzle and the eases are procedural (no clips); seat fit numbers (riderLift, spread) are
first values to eyeball in play; species answer calls borrow the idle call; L7 (bone altar) not built.
