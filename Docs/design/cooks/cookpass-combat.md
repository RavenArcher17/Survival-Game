# Wildreach cook: COOKPASS-COMBAT (every built combat system, graded and raised)

Lane cookpass-combat, 2026-11-15, desktop. A cook pass on what build-combat-core (C0, C2-C5), build-combat-full
(C6-C12) and the twelve biome lanes left: the melee ability and its hit, the hit stack, guard / parry / crits, stance
and poise, lock-on, the creature and boss attack rows of every land, their projectiles, the HUD's combat bars.
Research how the best games do it, grade every built piece against THE BAR, design the raise, build it.

Spec sources: COMBAT-AND-LESSONS (CAL) Part 1 and L1-L13; cooks/combat.md (the combat design, 2.1-2.22); THE BAR
(preamble.md: full hit stack, hitstop 0.06-0.30 s, visible coil 0.14 / 0.20 s, one verb per button, 0.33 s buffer,
attacks show the real weapon, damage only on visible contact, small-cue telegraphs read at 15 m, projectiles >= 0.9 s
warning and never home, no attacker limit, players die, no floors); the share's LATER-COOKS lines in this scope.

## 0. Requirement ledger

| # | Requirement (one each) | Source | Done where (filled at the end) |
|---|---|---|---|
| Q1 | Research how the best games solve combat (named games, places, systems) plus real-world reference, written here with the details used | brief, Aaron 11-13 | |
| Q2 | Grade every built combat piece against THE BAR | brief | |
| Q3 | Design the raise in writing before building; every detail, edge cases, build steps | brief, Aaron 11-13 | |
| Q4 | Build exactly the plan at full detail; no placeholders; anything unfinished stays hidden | brief, BAR | |
| Q5 | Spot-check against plan and BAR by reading the code; fix before push | brief | |
| Q6 | "Already done" only with file:line proof | brief | |
| Q7 | Design with intention: each piece says why it is there, what drives it, how it meets what it touches (ground, walls, bodies, light, sound); nothing passes through anything; nothing snaps | Aaron 11-14 | |
| Q8 | No missed requirements: ledger first, every row proven at the end | Aaron 11-14 | |
| Q9 | Data-driven: numbers in Data/*.json and tuning.json; abilities as UWRGameplayAbility + ClassForNode; all damage through ApplyHit; sounds / fx as table rows | brief, Raven's rules | |
| Q10 | Every path: devices (mouse and pad), states, menus, edge cases, multiplayer (server authority, replication) | brief | |
| Q11 | Full hit stack on every connected blow (reaction, flash, sparks, trauma, rumble, 4 sound layers) | BAR | |
| Q12 | Hitstop 0.06-0.30 s | BAR | |
| Q13 | Visible coil 0.14 s light / 0.20 s heavy | BAR | |
| Q14 | One verb per button | BAR | |
| Q15 | 0.33 s input buffer | BAR | |
| Q16 | Attacks show the real weapon / claw / projectile | BAR | |
| Q17 | Damage only on visible contact | BAR | |
| Q18 | The telegraph is a small cue (glint, voice, coil), not a full-area decal | BAR | |
| Q19 | Tells read at 15 m | BAR | |
| Q20 | Projectiles give >= 0.9 s warning | BAR | |
| Q21 | Projectiles never home | BAR | |
| Q22 | No attacker limit: packs aggro freely | BAR | |
| Q23 | Players die; no safety floors; no helpers added after a fail | BAR | |
| Q24 | Weapon skills are called Wildbrands; no spirit summons; taming as Raven designed | BAR, Aaron 11-10 | |
| Q25 | Git: pull before and before push, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, no release / tag, LFS sizes reported | brief | |
| Q26 | Compile gate only (editor target), fix every warning introduced; no tests run | brief | |
| Q27 | Questions decided and logged to QUESTIONS-FOR-TOMORROW; out-of-scope finds to LATER-COOKS; LANDED line | brief | |
| L1 | Boss telegraphs: the full-area ground decals and InShape damage rewrite to visible contact + small cue for all bosses, "not just Fire / Frost" | LATER-COOKS line 4 | |
| L2 | Projectile `reflectable` flag set on every creature and boss projectile row in all biomes; combat lane to own | LATER-COOKS line 78 | |
| L3 | Hitstop scaled by weight within 0.06-0.30 s | LATER-COOKS line 2 | |

(G rows come from the grades in sections 2 and 2b; every row is proven in section 5.)

## 1. Research

### 1.1 Games

| Game / place | The system | What makes it work | What we take |
|---|---|---|---|
| **Elden Ring**: any straight sword in Stormveil's corridors; the Crucible Knight at Stormhill Evergaol | Each weapon has hit capsules on the blade that ride the attack animation; a horizontal slash hits what the blade passes through, in the order it passes, and a light swing clanks off a wall in a tight corridor (sparks, a recoil, the swing ends). Thrusts and straight swords are the corridor weapons | What you see hit is what hits: the blade's real path. Walls matter: fighting in a corridor changes your weapon choice | Blade-path hits from the clip; wall recoil for swings (thrusts slide past); corridor-friendly classes are the thrusters |
| **Elden Ring / Dark Souls 3**: the HUD | The player's HP bar drops to the new value at once, and the lost part stays as a pale chunk that drains after ~0.6 s; the boss bar does the same, and a running damage number sits at the bar's right end while hits keep landing, gone 2-3 s after the last | You read how big a blow was without numbers on the body; the boss's running total rewards a combo | A trailing chunk on the player's HP and stamina bars, the lock target's bar and the boss bar; the boss's running damage beside its bar |
| **Elden Ring**: lock-on | A small white dot sits on the locked point (head, chest), not a frame around the body; switching to another point of a big body (Godrick's arm, a dragon's head) moves the dot | The dot says exactly where the camera aims and which part you're on | The lock mark sits on the lock point, small; flicking to a lock point moves it |
| **Sekiro**: Genichiro on the castle top | Every deflect and every blow lands with a distinct spark at the blade contact point, not at the body centre; posture bars on both fighters | The spark at the meeting point is the read of the clash | Impact effects at the blade's contact point |
| **Monster Hunter World / Wilds**: Great Sword true charged slash on a Rathalos head | Hitstop on contact with the weak spot, ~0.2 s on the biggest blows; Wilds' beta shortened hitstop and players called it weightless; Capcom put it back for release | Hitstop is the weight; cutting it is felt at once | Keep the tiers (0.06-0.30 s), already built |
| **God of War (2018)**: the axe on draugr | Camera kicks toward the blow's direction, the rumble pattern differs per weapon, the enemy reacts at the hit location | Direction of the blow is felt | Trauma by tier (built); keep |
| **Dark Souls**: wall recoil | A light attack into a wall plays a recoil with the weapon bouncing back; stamina is spent; a short vulnerable moment | Fighting in a corridor is a real choice | The recoil costs the swing and a short pause (0.35 s), never damage |

### 1.2 Real-world reference

- **Swordsmanship (HEMA, Liechtenauer)**: a cut lands with the last third of the blade (the "foible" to the middle);
  the strong near the hilt binds and doesn't cut. An axe or hammer delivers only through its head; a spear only
  through its point. So a blade-path hit uses the class's live part of the blade, not the whole weapon.
- **Steel on stone**: an edge striking rock rings and throws orange sparks and grit and the hand is jarred back; the
  swing doesn't follow through. That's the wall recoil.

### 1.3 Lessons

1. The hit must come from where the blade is, when it's there.
2. Walls take part in a fight.
3. The size of a blow reads on the bar (chunk) and the boss total.
4. The lock mark tells exactly what the camera holds.
5. The weight of a blow shows in its tier: a big spell, a full-draw arrow and a lucky crit land like the blows they
   are (God of War's runic attacks and Leviathan throws, ER's charged spells, MH's crit hits all get the heavy stack).
6. You must feel where you were hit from (God of War 2018's off-screen attack arcs, Ghost of Tsushima's red edge,
   ER's hurt grunt and stagger): with no attacker limit, packs come from behind.

## 2. Grades (built pieces vs THE BAR)

Read: `WRAbilities.cpp` UWRGA_Melee (StartSwing :807, Sweep :1055-1388), `WRCharacter.cpp` OnSwingVisual :1146-1230,
FistFrame :743, body load :503-591, `WRToolMesh.cpp` Weapon :372 (the modelled weapons' real lengths),
`WRPlayerController.cpp` lock-on :805-1079, `WRFxHud.cpp` lock corners :240, `WRUIWidgets.cpp` SWRBar :265,
`WRUIHud.cpp` VitalRow :1274 and the boss bar :630-660, `WRHUD.cpp` enemy bars :455-500, `WRFxCombat.cpp`
(HitFlagsFor :23, Feel :99, HitStopEx :252, AddHudHit :202, AddTrauma / ModifyCamera :480), `WRCombatLibrary.cpp`
ApplyHit (:1300 ReportHit), all 201 ApplyHit callers (79 files), `Data/tuning.json fx`, `Data/weapons.json`.

| # | Built piece | Grade | Why (file:line) |
|---|---|---|---|
| G1 | Where a survivor's swing hits | **FAILS "damage only on visible contact"** | `Sweep` (WRAbilities.cpp:1082-1087, :1113-1116) tests a straight 45 cm tube from the body's centre to `reach` ahead for the whole active window. A horizontal slash that visibly passes a wolf at your left misses it; a body dead ahead is hit on the first active frame though the blade reaches it last; the spear's point and the hammer's head are no different from the haft. combat.md 2.4.3 calls for the blade's real path ("UWRWeaponTraceNotifyState, BUILT"): no such class exists in Source (grep), so that row was never built |
| G2 | A blade swung into a wall | **MISSING** | Sweep meets only pawns and harvest things: a light swing in a fold corridor passes through the stone. ER / DS: a swing clanks off a wall and recoils; thrusts slide past |
| G3 | The size of a blow on the bars | **BELOW** | SWRBar (WRUIWidgets.cpp:273) jumps to the new value; the HP bar, the boss bar (WRUIHud.cpp:642) and the canvas enemy bars (WRHUD.cpp:496) show no chunk of what a blow took |
| G4 | Your damage on a boss | **MISSING** | ER shows your running damage beside the boss bar while hits land; here only floating numbers, off by default |
| G5 | The lock mark | **BELOW** | WRFxHud.cpp:241 frames the actor's centre with gold corners; the camera aims at the lock point (WRPlayerController.cpp:1068), so flicking from a dragon's head to its wing (FlickLock :905-928) changes nothing on screen |
| G6 | The swing you see beside a tree | **BELOW** | OnSwingVisual (WRCharacter.cpp:1156-1193) turns any swing near a node / tree / shrub into a chop or gather clip, even with a wolf in front: the blow then lands from a clip you didn't see |
| G7 | Tier of spells, arrows and lucky crits | **FAILS the full hit stack** | Tier defaults Light (WRCombatTypes.h:75); HitFlagsFor (WRFxCombat.cpp:36-39) promotes only bHeavy / bWeaponArt / crit / shatter. ~25 single-blow spells (Void Collapse WRAbilitiesVoid.cpp:308 Impact 3, Maw :364 Impact 4, Arcane Blast WRAbilitiesArcane.cpp:486, Starfall :650, Blood Spear WRAbilitiesBlood.cpp:302, all Wind spells WRWindPath.cpp:664...), every bow / crossbow shot even at full draw (WRRanged.cpp:553-569), random / gem crits (`bCrit` WRCombatLibrary.cpp:788 sets the flag, not the tier), biome creature specials and turrets land as light: 0.06 s stop, 0.07 s flash, no FOV punch |
| G8 | Discrete bursts labelled as ticks | **FAILS the full hit stack** | `bDamageOverTime` cuts impact, sounds, hitstop, flash, trauma (WRFxCombat.cpp:143). Set on blows that are one burst: the frostbite burst (WRStatusComponent.cpp:427), hemorrhage (:448), shock arcs (:496), the giant's-ember shockwave (WRCombatLibrary.cpp:1052), the barbs burst (WRNaturePath.cpp:928), fall damage (WRSkyAir.cpp:661 / 679), the Sky deck fall (WRSkyBosses.cpp:695) |
| G9 | Knowing you were hit, and from where | **MISSING** | No direction cue and no hurt edge (only blind / underwater / glare vignettes, WRFxHud.cpp:150-190); the canvas "-12" text is hidden under the Slate UI (WRHUD.cpp:85); a lava tick or a grab's squeeze gives nothing at all |
| G10 | Hitstop on a shooter far away | **WRONG** | ShowHit freezes the Source on every hit (WRFxCombat.cpp:175 HitStopEx(Target, Source)): an archer 30 m off, a caster, a turret's piece freeze when the shot lands. Only a blade in the body should stop the wielder (MH, ER) |
| G11 | Hitstop past 0.30 s | **FAILS the cap** | HitStopEx keeps the first Start and pushes Until (WRFxCombat.cpp:275-283): a chain of hits or multi-hit spell stretches one freeze past 0.3 s |
| G12 | Camera trauma | **BELOW** | Trauma = 0.04 + 0.22 w² then squared again in ModifyCamera (WRFxCombat.cpp:495): a light hit moves the view 0.009 degrees, a heavy 0.035, a crit 0.17: everything below a crit is invisible. CAL L1's numbers were meant as the felt jolt; Eiserloh's trauma model (GDC 2016 "Juicing your cameras") adds 0.2-0.5 per hit before squaring |
| ok | Hitstop tiers, flash tiers | MEETS | tuning fx.hitStop 0.06-0.30, fx.flash 0.07-0.26 |
| ok | Coil | MEETS | clip warp WRCharacter.cpp:1210-1223 (coil never faster than 2.2x); weapons.json windups >= 0.14 / 0.20 |
| ok | Buffer 0.33 s, one verb per button | MEETS | WRCharacter.cpp buffer (build-combat-core R17); controls cook |
| ok | Players die, no floors | MEETS | ApplyHit clamps only at 0 (build-combat-core R47) |
| ok | Wildbrand blows | MEETS | bWeaponArt promotes them to heavy (WRAbilities.cpp:1379) |

## 2b. Grades: the enemy side (every land's creature and boss rows, bespoke boss moves, projectiles)

Read: `WRBossCombat.cpp` (StartAttack :230-275 tells and marks, BeginStrike :277-395, TickStrike :530-660, EndStrike
repeats :745-790), `WRCreatureCombat.cpp` (StartRow, glint size :459, spit :917-944, burst rows :953-970, :1040),
`WRProjectile.cpp` (Init :136, HomeOn :145-164), every biome boss file's AttackStarted / BeginStrike, `Data/attacks.json`
(379 creature rows, 447 boss rows), `tuning creatureAI / bosses`.

| # | Built piece | Grade | Why (file:line) |
|---|---|---|---|
| G13 | Far "point slams" | **FAILS visible contact** | BeginStrike :376-391: a `target` row not thrown bursts a ring at the aim point though the boss never goes there: echo_warden fork_slam (up to 30 m, WRCrystalBosses.cpp:881), vorhollow nullstones, ahrumet glassfall, hrimgerdr avalanche (24 m), kilnmite_broodqueen and glacimaw_elder burrow_erupt, sylvaroth sweep_slam (20 m), ssarrok breach, glass_empress burrow_emerge, reef_tyrant breach, mould_born pour, rime_witch frost_circle, mother_of_rifts sink_burst, lyrathis held_facet, skaldrith ice_spikes, aeolith downburst, thalassor bubble_net; Vulkarn / fallen_smith / bodvildr / brannoch overhead_slam and quench_feint reach 7-10 m, past their arms |
| G14 | A string's repeat hits | **FAILS tells** | EndStrike :765-781 re-aims and waits only the repeat gap with no glint or voice: nullstones 0.0 s, siren_eel shatter_shriek 0.25, thalassor spout_volley 0.3, bramblequeen mandible_shear 0.3, grey_matron paw_swipe 0.3; a repeated lob re-aims with no warning |
| G15 | Full-area marks from the wind-up | **FAILS small-cue** | StartAttack :254-262 lays a dark 0.7x mark under every thrown row and dive from the start of the wind-up (before anything is in the air) and a full frost circle for ice_spikes (renewed each repeat :778) |
| G16 | Projectile warning | **FAILS >= 0.9 s** | the generic spit (WRCreatureCombat.cpp:917-944): gemsniffer tongue_flick 0.5-0.7 s (0.35 s wind-up under the fold's frenzy), wickbeetle wax_squirt 0.6-1.0, dromad dulla_spit 0.9-1.0; the Echo mimic (WRVoidBosses.cpp:338-355) flies at 1600 cm/s past the cap, aimed at release: 0.25 s |
| G17 | Spit element | **BUG** | the generic spit's element falls through to fire for an arcane creature (gemsniffer, wickbeetle get a burning glob) |
| G18 | Reflectable flags | **PARTLY** (LATER-COOKS 78) | Init sets reflectable on every shot including stones and the mud lob (WRProjectile.cpp:136); InitLob / InitPhysical never do: boss globs, hex_bolt (AWRSwampShot), Vharra's clots, fungal webs, turret bolts can't be reflected |
| G19 | needle_fan | **BELOW** | a cone row thrown as one lob with a 25 cm splash, not a fan |
| G20 | Bespoke instant hits (no front, or no tell) | **FAILS visible contact / tells** | Ahrumet's gnomon uproot (WRDesertBosses.cpp:837-851, no tell) and the Noon Lock pulse (:479-490, no tell); Thalassor sluice_break (WROceanBosses.cpp:840-866); Mycelorn root_lash_line (WRFungalBosses.cpp:1161-1172), cap-falls (:835), rot_belch frozen cone (:993-1016); grave toll (WRGraveBosses.cpp:978-988), Gravedigger bury (:1069-1097) and spade_slam (:1099-1113), grave_hands (:1115-1130), Nhal keen / unhinged_keen (:1227-1253); Aeolith updraft lash (WRSkyBosses.cpp:1050-1066, 0.2 s glint), barnacle_shed (:939-965), downburst's extra ring (:1171-1182); Lyrathis echo copies (WRCrystalLyrathis.cpp:906-949), sundering_flare (:1143), shard_rain timers (:1155-1192); Sylvaroth heart-tree limb sweep (WRForestBosses.cpp:1510-1537); Vharra anchor_heave / drowning_coil / ambush_rise (WRSwampVharra.cpp:1480-1557); Bloat burst (WRSwampBosses.cpp:2872-2881); Vorhollow Gullet crush (WRVoidBosses.cpp:1420-1447, sound-only tell); Vulkarn's phase-2 flood every 22 s (WRBoss.cpp:1225, no warning) |
| G21 | Bespoke full-area marks | **FAILS small-cue** | Grey Matron bridge_leap lane box (WRForestBosses.cpp:2128) and canopy_drop shadow from the wind-up (:2105); grave_hands green circles (WRGraveBosses.cpp:880) and lay_to_rest (:896); Kharvox dive shadow from the wind-up (WRStormBosses.cpp:1411); Vharra clot_spit marks before the clots fly (WRSwampVharra.cpp:1302); Lyrathis shard_rain (WRCrystalLyrathis.cpp:1177) |
| G22 | Creature `burst` rows on visible bodies | **FAILS visible contact** | anvil_mite burrow_pop (3-12 m) and risen_mourner grave_grasp (8 m) burst at the remembered aim point while the body stays (WRCreatureCombat.cpp:953-970) |
| G23 | Tells on hidden bodies | **BELOW** | Coilback ring_pulse / surface_burst, the Weaver's starfall_drop, Vorhollow rift_dive: the glint plays on a hidden mesh (WRStormBosses.cpp:1584; WRFungalBosses.cpp:918; WRVoidBosses.cpp:256) |
| G24 | Wind-ups under 0.34 s | **FAILS coil** | attacks.json: thornlynx paw_swipe2 0.15, raiju claw2 0.30, bogstalker gulp 0.30, hex_warden sickle_b 0.30 (damage 25) |
| G25 | Rows with no glint that launch | **BELOW** | glint `none` on 16 creature / 9 boss rows, among them the Fungal lobs (stalk_volley, web_shot, web_volley, egg_toss, spore_mortar) |
| ok | Homing | MEETS | no enemy projectile homes: HomeOn returns unless the instigator is a player or a tame (WRProjectile.cpp:145) |
| ok | Attacker limit | MEETS | packs attack freely (no token code; build-combat-full R32 group spots) |
| ok | Creature / boss median wind-ups | MEETS | creatures median 0.6 s, bosses 0.9 s |

## 3b. Plan: the enemy side

### 3.10 A front for every blow that isn't a body (G13, G20, G22)

Why: every blow lands where something visibly is (BAR). When the thing is a shockwave, a split in the ground, a bore,
a ripple of sound, the front must travel and hit only where it is (Godrick's and Radahn's ground eruptions in ER,
Malenia's waterfowl, MH's Zinogre slam waves: the ground cracks run out from the impact and you jump or roll the front).

- New `AWRStrikeFront` (server-only actor, `WRStrikeFront.h/.cpp`, no replication; effects through `WRFx::Broadcast`):
  - `Ring(Instigator, Centre, MaxRadius, Speed, Depth, Hit, Fx, Sound)`: a ring rolling out; a body is hit once when the
    front (Depth thick) passes its capsule. A jump over it (State.JumpLegsInvuln) clears it if the row is `low` (every
    ground front is `low`).
  - `Line(Instigator, From, To, Speed, HalfWidth, Hit, Fx, Sound, EndRing)`: a head running along the ground (traced
    down onto the floor every step) hitting bodies within HalfWidth of the head; an optional ring where it ends.
  - `Arc(Instigator, Centre, Radius, FromYaw, Sweep, Seconds, Hit, Fx)`: a limb or tail sweeping round: hits bodies in
    the sector between the start and the current angle.
  - The look: the named fx row every `fx.stepCm` along the head (dust and grit, embers, snow, spores by the row's
    `frontFx`, default `shockwave` small), the sound at the start, every 4th step and at the end.
  - All damage through ApplyHit with the template FWRHit; once per body per front; filter callback for bespoke rules.
- **Far point slams (G13)**: a `target` row that isn't thrown, aimed farther than the boss's reach (`bosses.slamReach`
  = capsule radius + 1.4 x half height, min 250 cm): the weapon comes down at its reach on the line to the target
  (the shockwave there, its small inner hit), and an **eruption line** runs on the ground from there to the aim point at
  `bosses.eruptionSpeed` 1500 cm/s, 160 cm wide, then the row's ring bursts at the aim point. A burrowing row
  (burrow_erupt, breach, burrow_emerge) runs a dirt mound the same way. Within reach: unchanged (the slam lands at the
  aim point, which the arm reaches).
- **Creature `burst` rows on a visible body (G22)**: the same eruption line from the body to the aim point.
- **Bespoke (G20)**, each turned into a front or given a body and a tell:
  - Ahrumet gnomon uproot: 1.2 s tell (crimson glint on the gnomon's hand, the ground cracking ring fx around him, the
    voice), then a Ring from him (1200 cm/s). Noon Lock pulse: a 2.0 s warning (the sun-flare fx over the plaza and the
    rising tone) before each pulse; the pulse itself is light, so it stays instant, but now always warned.
  - Thalassor sluice_break: the bore runs as a Line across the Pound from the broken sluice at 600 cm/s.
  - Mycelorn root_lash_line: a Line along the vein from its root end at 2000 cm/s. Cap-falls: each cap is a real lob
    falling from the ceiling (1.0 s fall). Rot_belch: the cone follows the head's present facing each tick.
  - Grave toll: a Ring from the tower base (1200). Gravedigger bury: the clod is thrown (a lob) and buries where it
    lands; spade_slam: a Line (1600). grave_hands: a 0.9 s floor-crack cue at each spot, then the hands burst (the
    hands are there); the green circles go. lay_to_rest: same. Nhal keen: a Ring of sound in its cone (1500).
  - Aeolith updraft lash: tail glint 0.9 s before; downburst's extra ring a Ring from the body (1200). Barnacle_shed:
    each barnacle a real lob from the body.
  - Lyrathis echo copies: their own glint, and an echo hits at the copy's own aim; sundering_flare a Ring (2500) after
    its tell; shard_rain: each shard a real lob from above.
  - Sylvaroth heart-tree limb sweep: an Arc round the tree (360 degrees over 0.8 s) after the limb lift.
  - Vharra anchor_heave a Line, drowning_coil a Ring, ambush_rise a water-bulge cue at the spot 0.8 s before.
  - Bloat burst: a Ring from the body (1400). Vorhollow Gullet crush: 2.0 s tell (the walls' flesh pulsing fx with the
    sound), then the walls press in: an inward Ring from the arena edge
    (`Ring` with negative speed: the front runs from MaxRadius toward the centre), alcoves still safe.
  - Vulkarn's phase-2 flood: 2.0 s of warning (the forge's rumble sound and lava welling fx at the rim) before it runs.
  - The small instant rings round a visible body (the Swamp flop, the Storm discharge, the tunnel toad croak, Kyrra's
    dive landing): Rings from the body.

### 3.11 Tells (G14, G15, G21, G23, G24, G25)

- **Every repeat** gets the row's glint and voice at its wind-up (MulticastTell), and its gap is floored at
  `bosses.repeatMinGap` 0.4 s; a gap of 0 stays a volley (all fired together off the one tell).
- **Marks**: the wind-up's dark mark under thrown rows goes; a thrown lob carries its own landing shadow from the
  moment it leaves the hand (shown for its flight only, `AWRGroundMarker` at the landing point, flight + 0.1 s). A dive
  shows its shadow from the moment the body drops (the strike, not the wind-up). Ice spikes lose the frost circle: they
  burst at the aim point after the frost creeps (a small cue: frost fx at the spot, `ice_spikes` row fx, 0.6 m).
  Bespoke marks (G21) follow the same rule: Grey Matron's lane box goes (her leap is the line), her canopy shadow and
  Kharvox's dive shadow show from the drop, Vharra's clot marks from the throw, Lyrathis's shards with each shard.
- **Hidden bodies (G23)**: the cue plays at the ground over the body (a dust or water bulge fx and the voice at that
  spot), not on the hidden mesh.
- **Wind-ups (G24)**: thornlynx paw_swipe2 0.36, raiju claw2 0.36, bogstalker gulp 0.4, hex_warden sickle_b 0.4.
- **Launching rows with glint none (G25)**: a white glint at the launching part (the mouth / the stalk) when the row
  throws something; rows that only move (burrow, dive) keep none.

### 3.12 Projectiles (G16-G19)

- **Warning floor**: every creature and boss shot (spit, lob, volley, the Echo mimic) flies so that wind-up + flight is
  at least `creatureAI.minWarning` 0.9 s: if the target is close, the shot is slower (speed = distance / (0.9 - the
  wind-up it had), at least 250 cm/s), so a spit at 2 m is a slow glob you can see. Aimed where the target stood when
  the wind-up began (never at release; the Echo mimic too).
- **Element**: the spit's element is the creature's own (arcane, nature, void...), fire only for fire.
- **Reflectable**: one rule in AWRProjectile: an enemy shot is reflectable unless it's a heavy physical body (a
  boulder, a headstone, a stump, a branch: InitLob with a mesh). attacks.json `reflectable` overrides per row. Turret
  bolts reflect like a survivor's bolt.
- **needle_fan** (cone rows thrown): fires `count` (default 5) lobs spread evenly across the row's cone.

### 3.13 Build steps (each compiled, committed, pushed)

1. Blade path + wall recoil + the chop rule (G1, G2, G6): WRBladePath.h/.cpp, UWRGA_Melee::Sweep, weapons.json
   `blade`, tuning `combat.recoil` / `combat.blade`, fx / sound rows `hit.recoil`.
2. Hit feel (G7-G12): HitFlagsFor, FWRHit bFullDraw, LaunchProjectile, the burst sites, HitStopEx, ShowHit, tuning
   fx.trauma, the hurt edge in WRFxHud.
3. HUD (G3-G5): SWRBar trail, VitalRow / boss bar, canvas enemy bars, the boss tally, the lock mark.
4. Enemy shared (G13-G19, G22, G24, G25): AWRStrikeFront, point slams, repeats, marks, creature bursts, projectile floor,
   element, reflect, needle fan, attacks.json wind-ups and glints.
5. Bespoke bosses (G20, G21, G23).
6. Spot-check, doc, LANDED.

## 3. Plan

### 3.1 The blade's path (G1)

Why: the hit must come from where the drawn weapon is, when it's there (BAR; ER's blade hit capsules; HEMA: a cut lands
with the last part of the blade, an axe with its head, a spear with its point).

- **Where the path comes from**: the swing clip itself. `FWRBladePaths` (new `WRBladePath.h/.cpp`) loads the hero's body
  mesh and the light / heavy clips from `player_art.json` on any machine (a dedicated server too, which never loads the
  body to draw it), and for each clip samples 16 poses across its strike frames (`anim_windows.json` "trace", the same
  frames the clip warp fits onto the move's active window, WRCharacter.cpp:1206): the weapon socket's component-space
  transform (`UAnimSequence::GetBoneTransform` down the bone chain, the skeleton's ref pose for bones without a track,
  the root's horizontal travel removed because the ability moves the body itself), then into the actor's space (the
  body's yaw `meshYaw` and its base height, as WRCharacter.cpp:584-591 sets them). Cached per clip and body (m / f).
- **What part cuts**: weapons.json `blade` per class: two points in the fist's frame (X out of the fist past the index
  finger, Y toward the fingers; WRCharacter.cpp:743) and a radius, read off the modelled weapons (WRToolMesh.cpp:372):
  fists knuckles -4..8 cm r14; dagger 10..36 r18 (the seax); sword 20..95 r24; axe head 48..66 r26; pick-hammer head
  50..66 r26; spear 116..166 r16 (lugs to point); greatsword 25..130 r30; great hammer head 56..74 r40; scythe the blade
  from the snath top (98, 0) to its tip (80, 75) r22.
- **When**: at active fraction f the blade is the sample at clip time T0 + f (T1 - T0). Each 30 ms sweep tests the
  blade from the last fraction to this one in sub-steps so the tip never moves more than 20 cm between tests (max 16):
  blade segment to the target capsule's axis <= blade radius + capsule radius. Rewound bodies for a remote survivor's
  swing (WRLag BodyAt, as today), present bodies otherwise.
- **Impact point**: the contact point on the blade, so sparks, head strikes (stance.headRadius) and breakable parts are
  judged where the edge met the body.
- **What stays**: world things (nodes, carcasses, fold things, arena pieces, buildings, ladders, the dressing) keep the
  reach tube: that's the harvest / raid reach, not a fight. Jump attacks keep the tube (the blade comes down over the
  whole fall). Creatures using UWRGA_Melee keep it (their rows have their own contact). Mounted blows are separate.
- **Never a dead swing**: if the clip isn't an `UAnimSequence`, the mesh, socket or blade row is missing, or the sampled
  blade sits behind the body on average (a wrong frame), that clip falls back to the tube and the log says so once.

### 3.2 Wall recoil (G2)

Why: walls take part in a fight (ER corridors, DS recoil); steel on stone rings, sparks and jars the hand back.

- While a **light** (tier light, not a thrust: move type `pierce` slides past) is in its first 70% of the active window
  and has hit no body yet, the blade tip's travel since the last test is traced against world-static geometry. A hit on
  a surface facing sideways (|normal.Z| < 0.55: a wall, a boulder's flank, not the floor) that isn't something a swing
  works on (a node, the dressing's trees and rocks, a build piece, a fold thing, an arena piece, a pawn) is a **recoil**:
  - the swing's sweeps stop (nothing past the wall is hit); sparks (fx row `hit.recoil`) and the clang (sound row
    `hit.recoil`, the block transient over grit) at the contact point on every machine; the pad's light motor;
  - the hero is held `combat.recoil.seconds` 0.35 s (the light flinch clip, the ability cancelled); stamina spent stays
    spent; no damage.
- Heavies, charged, jump, run and roll attacks never recoil (committed, like ER's heavier swings).
- Tuning `combat.recoil` (enabled, seconds, wallNormalZ, activeShare).

### 3.3 The size of a blow (G3, G4)

- SWRBar gains an opt-in **trail**: when the fill drops, the lost part stays as a pale chunk (the bar's colour lifted 65%
  toward warm white), holds 0.5 s, then drains at 1.2 bar-widths/s; a rise (a heal) shows at once and clears it. On the
  HP and stamina rows and the boss bar. The canvas enemy bars get the same chunk (per body).
- **Your damage on the boss**: AddHudHit (WRFxCombat.cpp:202) adds your blows (bMine, not ticks) on a boss into a
  running total; the boss bar's title row shows it at the right in pale ink while blows keep landing, gone 2.5 s after
  the last (`fx.hud.tallySeconds`). Independent of the damage-number setting (ER always shows it). The canvas enemy
  bars show the same small number at their right end.

### 3.4 The lock mark (G5)

- ER's mark replaces the corners: a small dot (6 px, warm white-gold #FFE9B0, dark 1 px ring) on the lock point the
  camera aims at (head, chest, a wing; 30 cm over the centre for a body without points); a ring eases in from 22 px to
  10 px over 0.18 s whenever the lock lands or flicks to another point, so every change is seen. Gold stays "yours".

### 3.5 What the swing shows (G6)

- OnSwingVisual picks the chop / gather clip only when no hostile living body is within 4 m in a 60 degree cone ahead.

### 3.6 Every blow at its weight (G7, G8)

- HitFlagsFor's tier rule: Light becomes **Heavy** when `Impact >= 3`, or the blow is a full-draw shot (`bFullDraw`,
  new on FWRHit, set by LaunchProjectile at full draw), or its damage is at least 20% of the target's max health; a
  random / gem crit (`bCrit` in ApplyHit) is **Crit** tier (flag already set; now the tier too).
- The discrete bursts lose `bDamageOverTime`: frostbite burst, hemorrhage, shock arcs, the ember shockwave, the barbs
  burst, falls and the deck fall get the full stack (falls: Impact 3+, so a long fall lands heavy).

### 3.7 Feeling where you were hit (G9)

- **Hurt edge**: a hit on you (Result Hit / GuardBroken, ticks too) paints a dark-red edge (#7A0A14 at the screen's
  border, 0 at 18% in) on the side the blow came from (its origin relative to the camera's yaw: a left, right, front or
  back band; back = the bottom edge), strength by the blow's share of your max health (0.25 + 3 x share, max 0.85),
  fading over 0.6 s. A tick (lava, burn, a squeeze) pulses the full edge faintly (0.18). Never red text, no numbers
  unless the damage-number setting is on.
- Turned down with the HUD's hints mode? No: it's a hit read, like ER's grunt; it follows Settings > Camera shake = 0
  off (one comfort switch for screen motion and flashes).

### 3.8 Hitstop and trauma (G10, G11, G12)

- The Source freezes only for a melee hit (`Hit_Melee`); a shot or spell freezes only its victim.
- A mesh's freeze is never longer than 0.30 s from its start, however many hits extend it.
- Trauma retuned for trauma²: `fx.trauma.base` 0.17, `scale` 0.45: a light hit 0.21 (0.11 degrees), heavy 0.33 (0.27),
  counter 0.46 (0.53), crit 0.62 (0.96), boss kill 0.62 + more from the kill shake. The `fx._comment` rewritten.

### 3.9 Edge cases (part 1)

- A remote survivor on a dedicated server: the server loads the mesh and clips once (FWRBladePaths) and judges with them.
- Male and female heroes: each their own cache; the server uses the swinger's BodyStyle.
- The two-handed hold and the off-hand don't change the clip: same path.
- A light swing that already hit a body doesn't recoil.
- Recoil in a fold corridor during a boss: no exception (a corridor is a choice).
- Pad and mouse: no new input; the hurt edge and lock mark are drawn the same for both.

### 3.14 Leftovers from the biome lanes that name the combat lane (decided here)

- Summons and boss health (LATER-COOKS 39, 71): one rule, no summon counts as a tame (built, section 5 L4).
- The shield layer's order (LATER-COOKS 79): guard first (WRCombatLibrary.cpp:631), then the shield takes what passes
  the guard (WRArcanePath.cpp:370 `ShieldOf`), then health. Kept: it matches ER (a block happens before anything soaks).
- The Void self-cost limit (LATER-COOKS 89): a refusal of your own ability past 75% max health lost, never a floor on
  damage taken: it meets "no safety floors" (enemy blows still kill; health clamps only at 0, WRAttributeSet.cpp:101).
- Fire in a spore cloud flashes it, frost settles it (LATER-COOKS 69): already built by the fungal lane
  (WRFungalHazards.cpp:479 Settle, :498 the flash's 40 + burn 40).

## 4. Spot-check (read-through against the plan and THE BAR, fixed before each push)

- Blade path: the fist frame is applied in socket space before the bone chain (`Fist * SocketLocal * CS`), the same
  composition the weapon mesh uses (WRCharacter.cpp FistFrame / weapon attach), and the body's place on the capsule is
  the art's (`-HalfHeight - min(MinZ, 0)`, `meshYaw`), so the server's blade sits where the drawn one does. A clip whose
  blade averages behind the body is refused (tube fallback, logged once). Root travel stripped (the ability's step-in
  moves the capsule; no double step).
- Recoil: checked only after this sweep's world things (a node, an arena piece or a build piece struck this sweep
  means no recoil), only before any body is cut, only in the first 70% of the strike, never off the floor; the stagger
  is the last call (it cancels the ability) and Sweep returns at once.
- Found and fixed while reading: a stray line in the first draft of the arc branch (rewritten before compile); `Owner`
  shadowed AActor::Owner in WRStrikeFront (renamed); the enemy-bar trail map would grow forever (pruned at 64); the boss
  repeat glint ignored "a throw always glints" (fixed); the Gravedigger's bury lambda reused outer names (renamed);
  Lyrathis's echo left an unused local (removed); Kharvox's two bolt locks read one key with two meanings (now
  `boltLead`, the leader's length, for both); far-slam eruptions threw grit for every boss (now embers / rime / dirt by
  element and burrowing).
- Every new blow still goes through ApplyHit (fronts call the owner's StrikeHit / RowHit / the land's own hit helper, or
  ApplyHit with a template); no damage path added outside it.
- Server authority: fronts run only on the server (Make refuses on a client); every machine sees their fx rows. The
  hurt edge, bar trails, tally and lock mark are client-only reads of replicated health and local hits.
- Colour law: gold only for the lock mark (yours); the hurt edge is dark red #7A0A14 (bleed's red, never the crimson of
  an unblockable glint); the recoil sparks orange-white.
- Unity build: helpers stay in their files' own namespaces; editor target compiled clean after every step.

## 5. Requirement ledger: done and where (Source/Wildreach unless a Data/ path)

| # | Done | Where |
|---|---|---|
| Q1 | Done | Section 1 (ER blades and recoil, ER / DS3 bars and boss number, ER lock dot, Sekiro sparks, MH hitstop incl. the Wilds beta reversal, GoW, DS; HEMA cut mechanics, steel on stone) |
| Q2 | Done | Section 2 (G1-G12) and 2b (G13-G25), each grade with file:line |
| Q3 | Done | Sections 3 / 3b, pushed before any code (showcase-notes df7e7f9) |
| Q4 | Done | Wildreach 58df47a, 752c3f1, 130404e, bbff301, b8c64a4; nothing shown that isn't built |
| Q5 | Done | Section 4 |
| Q6 | Done | Every row here carries file:line |
| Q7 | Done | Each front says what it is and what it meets (WRStrikeFront.h header comment; every call site's comment); fronts run on the floor (`Floor`, WRStrikeFront.cpp) and miss bodies in the air; the blade cuts only where the drawn edge is; nothing snaps (rings, lines and arcs advance every tick) |
| Q8 | Done | Ledger first (section 0); this table |
| Q9 | Done | Tuning `combat.blade / recoil`, `fx.trauma / heavyShare / hud.hurt* / tallySeconds`, `ui.bar`, `bosses.slamReach* / eruption* / frontStepCm / frontClear / repeatMinGap / coneLobs`, `creatureAI.minWarning / glintNear`, kharvox `boltLead`; weapons.json `blade`; attacks.json windups; fx.json and sounds.json `hit.recoil`; damage only through ApplyHit (section 4) |
| Q10 | Done | No input added (mouse and pad unchanged); fronts and blade judging on the server only; replicated health drives every HUD read |
| Q11 | Done | The ShowHit stack now reached at full weight by spells, aimed shots, crits and bursts (WRCombatLibrary.cpp:1288, WRFxCombat.cpp:33 / 40, WRRanged.cpp:559) |
| Q12 | Done | fx.hitStop 0.06-0.30 kept; the cap holds across extensions (WRFxCombat.cpp:318) |
| Q13 | Done | Clip-warp coil kept; four rows raised to at least 0.36 s (attacks.json thornlynx paw_swipe2, raiju claw2, bogstalker gulp, hex_warden sickle_b) |
| Q14 | Done (kept) | No input changed |
| Q15 | Done (kept) | Buffer untouched (WRCharacter.cpp PressAbilityInput) |
| Q16 | Done | Blade path from the drawn weapon (WRBladePath.cpp:58-165); the clod, the slabs and the barnacles are modelled rocks in flight (InitLob with SM_SimpleRock_05A) |
| Q17 | Done | Survivors: WRAbilities.cpp:1426 BladeSweep. Enemies: far slams WRBossCombat.cpp:419-440; creature bursts WRCreatureCombat.cpp:990; front call sites WRBossCombat.cpp:430, WRCreatureCombat.cpp:996, WRCrystalLyrathis.cpp:955 / 1181, WRDesertBosses.cpp:860, WRForestBosses.cpp:1538, WRFungalBosses.cpp:1194, WRFungalCreatures.cpp:1134, WRGraveBosses.cpp:990 / 1149, WROceanBosses.cpp:860, WRSkyBosses.cpp:1175 / 1184, WRStormCreatures.cpp:1629, WRSwampBosses.cpp:2882, WRSwampCreatures.cpp:1971, WRSwampVharra.cpp:1504 / 1571, WRVoidBosses.cpp:1444 |
| Q18 | Done | Wind-up marks removed WRBossCombat.cpp:264; shadows from the throw / drop :340 / :412; grave circles replaced by soil cracks (WRGraveBosses.cpp); Matron lane gone, her canopy shadow and Vharra's clot shadows timed to the drop / spit (WRForestBosses.cpp, WRSwampVharra.cpp); Kharvox's shadow body-sized (WRStormBosses.cpp) |
| Q19 | Done | Glints grow past 10 m (WRCreatureCombat.cpp:458); buried bodies cue at the surface (:465); every repeat re-tells (WRBossCombat.cpp:830) |
| Q20 | Done | creatureAI.minWarning: boss lobs WRBossCombat.cpp:321, spits WRCreatureCombat.cpp:945, the clod (WRGraveBosses.cpp bury), the Echo mimic at the shot cap (WRVoidBosses.cpp:347); Kharvox's leader 0.9 s (WRStormBosses.cpp:1426 / 1825, tuning kharvox.boltLead) |
| Q21 | Done (kept) | No enemy shot homes (WRProjectile.cpp HomeOn); the Echo mimic now aims at the wind-up spot (WRVoidBosses.cpp:347) |
| Q22 | Done (kept) | No attacker token; packs take group spots and all attack (WRCreatureCombat.cpp:584 GroupSpot) |
| Q23 | Done (kept) | Health clamps at 0 only (WRAttributeSet.cpp:101); no helper added; the Void limit is a refusal, not a floor (3.14) |
| Q24 | Done (kept) | Wildbrands keep their name; no summon added; taming untouched |
| Q25 | Done | Pulled before work and before each push (rebased over the audio cook pass); files added by name; CHANGELOG Unreleased + HANDOFF in the commits (bbff301's HANDOFF rule landed in b8c64a4); no tag or release; no new binaries (LFS +0) |
| Q26 | Done | Editor target compiled clean after every step; no tests run |
| Q27 | Done | QUESTIONS-FOR-TOMORROW, LATER-COOKS and LANDED lines on the share |
| L1 | Done | 2b G13 / G20 / G21: every land's bosses, not only Fire / Frost |
| L2 | Done | WRProjectile.cpp:145 IsEnemyShot, :185, :210; swamp shots WRSwampBosses.cpp:3986, WRSwampVharra.cpp:1476 |
| L3 | Done (by tier) | fx.hitStop 0.06 / 0.10 / 0.14 / 0.20 / 0.30 by tier, now reached by every heavy blow (Q11) |
| L4 | Done | WRBoss.cpp UpdateParticipants (WRSporePath::IsSummon, WRNecro::IsRaised) |
| G1 | Done | WRBladePath.cpp; WRAbilities.cpp:1426; weapons.json blade |
| G2 | Done | WRAbilities.cpp:1494; WRCharacter.cpp:1228; fx.json / sounds.json hit.recoil; tuning combat.recoil |
| G3 | Done | WRUIWidgets.cpp:281; WRUIHud.cpp VitalRow and boss bar `.Trail(true)`; WRHUD.cpp:498 |
| G4 | Done | WRFxCombat.cpp:236; WRUIHud.cpp:650 / 673; WRHUD.cpp tally |
| G5 | Done | WRFxHud.cpp:279; WRPlayerController.cpp:1024 |
| G6 | Done | WRCharacter.cpp:1138 |
| G7 | Done | WRCombatLibrary.cpp:1288; WRRanged.cpp:559 |
| G8 | Done | WRCombatTypes.h bBurst; WRFxCombat.cpp:33 / 40; WRStatusComponent.cpp:351; WRCombatLibrary.cpp:1054; WRNaturePath.cpp:929; WRSkyAir.cpp:680; WRSkyBosses.cpp:697 |
| G9 | Done | WRFxCombat.cpp hurt state in ShowHit; WRFxHud.cpp:195; tuning fx.hud.hurt* |
| G10 | Done | WRFxCombat.cpp:176 |
| G11 | Done | WRFxCombat.cpp:318 |
| G12 | Done | tuning fx.trauma 0.17 / 0.45 |
| G13 | Done | WRBossCombat.cpp:419-440; WRStrikeFront.cpp |
| G14 | Done | WRBossCombat.cpp:830 |
| G15 | Done | WRBossCombat.cpp:264, :340, :412 |
| G16 | Done | WRCreatureCombat.cpp:945; WRBossCombat.cpp:321; WRVoidBosses.cpp:347 |
| G17 | Done | WRCreatureCombat.cpp:961; WRBossCombat.cpp:361 (ElementOf :63) |
| G18 | Done | as L2 |
| G19 | Done | WRBossCombat.cpp:327 |
| G20 | Done | Ahrumet WRDesertBosses.cpp:860 + the pulse warning; Thalassor WROceanBosses.cpp:860; Mycelorn WRFungalBosses.cpp:1194 + slabs + live belch; Graveyard WRGraveBosses.cpp:990 / 1149 + clod + keen wave; Aeolith, barnacles, downburst, Kyrra WRSkyBosses.cpp; Lyrathis WRCrystalLyrathis.cpp:955 / 1181; Sylvaroth WRForestBosses.cpp:1538; Vharra WRSwampVharra.cpp:1504 / 1571 (ambush_rise already had its bubbles cue, VharraTickWindup); Bloat WRSwampBosses.cpp:2882; Vorhollow WRVoidBosses.cpp:1444; Vulkarn's flood warning WRBoss.cpp; small rings WRSwampCreatures.cpp:1971, WRStormCreatures.cpp:1629, WRFungalCreatures.cpp:1134. Kept with reasons: Lyrathis's shard rain is a falling shard with its own shadow from its fall; Sylvaroth's uprooting cracks and the root hand ring are its own roots and its buried hand prop |
| G21 | Done | as Q18 |
| G22 | Done | WRCreatureCombat.cpp:990 |
| G23 | Done | WRCreatureCombat.cpp:465 |
| G24 | Done | attacks.json (4 rows) |
| G25 | Done | WRCreatureCombat.cpp:752; WRBossCombat.cpp:262 |

## 6. Leftovers written to the share

- LATER-COOKS: the blade path follows whatever clip plays, so a spear still swings in the sword's arc until its own
  thrust clips exist (line 123 stands); each land's own falling meshes (a clod of grave soil, a fungal roof slab, a
  barnacle cluster) instead of the shared SimpleRocks rock; the shared grab framework (line 35) stays its own cook.
