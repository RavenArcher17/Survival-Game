# build-finish: finish what is built but half-done (Wildreach)

Lane build-finish, 2026-11-13. Repo `C:\Dev\Wildreach` at fe95c6a. Research, then the plan, then the build. Every
ledger row ends with its proof (file:line or commit) when the lane lands.

## 0. Requirement ledger

From the brief (Aaron 11-13 / 11-14 quotes included) and the scope line.

| # | Requirement | Source |
|---|---|---|
| R1 | Find every Fire and Frost skill node that is "[not built yet]" or does nothing, and build it | brief scope |
| R2 | Life-skill perks that are not active: make them work, and hide (not show) any whose system is not in the game | brief scope; FULL-GAME 4.4 "hidden until their system ships"; BAR "anything unfinished stays hidden" |
| R3 | Fishing gets an XP source (so fishing must exist) | brief scope |
| R4 | Engineering gets an XP source | brief scope |
| R5 | Swimming gets an XP source | brief scope |
| R6 | The tame whistle gets a screen (mouse AND pad) | brief scope; HANDOFF open item; FULL-GAME 5.x "Tame whistle has a screen" |
| R7 | Bounties: build fully or keep hidden, with proof | brief scope; DESIGN_REVIEW #32 |
| R8 | Held tools as real modelled items if possible | brief scope; QUALITY-PLAN row 6 |
| R9 | Build-piece detail matching collision | brief scope; QUALITY-PLAN row 6 |
| R10 | Every new screen works by mouse and pad, no dead states | BAR (UI) |
| R11 | Data-driven: numbers in Data/*.json / tuning.json, sounds as table rows, damage through ApplyHit | brief THE BAR |
| R12 | Multiplayer: server authority, replication, every path (devices, states, menus, edge cases) | brief THE BAR |
| R13 | No placeholders shown as finished; unfinished stays hidden | brief THE BAR |
| R14 | Design with intention: each new thing has a job, connects to the loop, meets what it touches | Aaron 11-14 |
| R15 | CHANGELOG "Unreleased" + HANDOFF updated in the same commit; pull before/after; add by name; no release/tag | brief GIT |
| R16 | Compile gate only, no tests; fix every warning I add | brief GATES |
| R17 | LANDED.md line: what's finished and what remains | brief |
| R18 | Report the size of new binary files | brief GIT |

## 1. What is actually unfinished (audit, from the code)

- **Skill nodes.** `UWRAbilityRegistry::IsBuilt` (WRAbilities.cpp:1071) marks 14 ability nodes and 12 passives built,
  and every one of the 12 passives has code (`HasNode` calls in WRCombatLibrary / WRProjectile / WRStatusComponent).
  The dead ones are three **stat** nodes the screen calls "Stat" but nothing reads: **Fire Shockwave** ("bigger blast
  radius"), **Frost Biting cold** ("stronger slows"), **Frost Wide storm** ("larger area effects"). No `nodeEffects`
  row, no `Stat()` reader.
- **Perks.** 120 perks in `Data/lifeskills.json`; 25 have a `stat` someone reads. 95 do nothing, and the screen says
  "Takes effect once its system is in" (WRUISkills.cpp:522) and the chat says the same (WRLifeSkills.cpp ChoosePerk):
  a placeholder shown to the player. Four every-level bonuses (fishing, tracking, building, engineering, swimming) do
  nothing either.
- **XP.** Fishing, engineering and swimming have no `Award` call (grep of `UWRLifeSkillsComponent::Award`).
- **Whistle.** `tameall` (WRPlayerMenus.cpp:718) exists only as a console command.
- **Bounties.** The `bounties` preset flag (server_presets.json) is read by nothing; the world-settings screen leaves it
  out on purpose (WRWorlds.cpp:58).
- **Held tools.** WRHeldTool.cpp builds hatchets and picks from engine cubes, cylinders and cones on the flat
  BasicShapeMaterial. No Blender, no art env (`C:\Dev\WildreachArt` absent) on this PC.
- **Build pieces.** Walls, floors, roofs, stairs, doors, windows already have collision shaped to the openings (doorway
  and window holes, stair ramp). The mismatches: the **bed, table, chair, brazier, pen** are solid boxes around legs,
  rails and gaps; the **banner and rug** have no detail at all (a flat red block is their look); the **Hearth** collides
  as a 1.2 m cone under an 80 cm stone ring; **stations and the totem** collide as a tinted engine shape that is not
  the size of their prop.

## 2. Research: how the best games do each piece

**Fishing.**
- *Animal Crossing: New Horizons*: the float nibbles 1-4 times with a soft tick before the real bite, which is a
  deep plunge with a heavy "plop" and a pad rumble. Press on the plunge. The fakes are the skill: you learn to wait.
  The fish's shadow size hints its size.
- *Minecraft*: a bobber, a particle trail racing toward it, then the dip with a splash sound; about 1 s to react.
  Clear cue, zero UI.
- *Valheim*: rod + bait; after the hook, holding reel drains stamina while the fish pulls away; let go to recover
  stamina while the fish runs. Stamina is the tension meter, so fishing uses the same body budget as fighting.
- *Stardew Valley*: a bar minigame; difficulty per fish (fast darters vs slow sinkers). Too abstract for an action game,
  but the lesson is per-fish fight data (strength, erraticness) in a table.
- *Red Dead Redemption 2*: tension shown by the rod bending; reeling while the fish pulls hard snaps the line. Fish
  weight is shown on the catch card; legendaries are map-placed with a one-off look.
- *Sea of Thieves*: the fish pulls left or right; you hold the opposite direction to tire it. Clear, physical, no meter
  numbers.
Taken: fakes then a true plunge (sound + splash + rumble + float under), ~0.8 s to hook; after hooking, **hold to
reel** against a fish that surges and rests; reeling during a surge builds tension; at full tension for a moment the
line snaps. Stamina pays for reeling (Valheim). Per-fish table rows set strength, surge rhythm, weight range, where,
when and rarity. A catch card (name, weight, record) through the existing notice flow. No floating bars beyond one
small tension arc near the crosshair.

**Swimming.** *Breath of the Wild*: normal swimming is free, a swim dash costs stamina, empty stamina sinks you. *Valheim*
drains stamina all the time in water, harsh. *Subnautica* levels breath with gear. Taken: plain swimming stays free;
**Sprint in the water is a fast stroke** (1.6x) that costs stamina (Sprint is already the verb for "faster, costs
stamina"); the skill raises swim speed per level, Strong stroke halves the fast stroke's cost, Seaborn removes it,
Water fighter makes hits land harder from the water. XP per metre swum. No diving exists, so breath / depth / current /
salvage perks stay hidden.

**Engineering without machines yet.** *Valheim* and *V Rising* treat crafting stations, smelters and kilns as the
"machines" of the early game: they run while you're away and are upgraded. *Factorio's* early game levels you by
running assemblers. Wildreach's stations already run timed queues (0.7.0), which is exactly a machine: input, power
(the Hearth), output tray. Taken: engineering levels by **running station orders** (XP per item a station finishes for
you) and by **building machines** (placing a station, fitting a code lock). Every level: stations run your orders a
little faster. Tinkerer: stations and code-lock doors cost less. Overclock: your orders run much faster but burn fuel
(one wood per item made). Self repair: your stations and doors mend themselves out of combat. Chief engineer: longer
production chains (bigger orders, an extra queue slot). Power, conveyors, turrets and siege weapons don't exist: those
perks stay hidden.

**Perks that wait for a system.** *Skyrim* only shows perks that work; *Valheim* hides content until its biome ships;
*Ark* greys engrams you can't use yet but they all work. The rule taken (FULL-GAME 4.4 + BAR): a perk whose system isn't
in the game is **not shown**; a tier whose two perks both wait is not shown either; a tier with one working perk shows
just that one ("take it"). The data keeps every perk (Raven's names), each waiting one marked `"waitsFor": "<system>"`,
so turning one on later is a data change. Saved choices stay valid (indices don't move).

**Whistle.** *Ark*: whistles are hotkeys (follow, stay, attack target, passive, aggressive) to every tame in earshot,
with a voice line and a chat line "3 tames follow". *Palworld*: one key recalls, a radial picks the order. *Red Dead 2*:
whistle the horse with one tap; it comes. Taken: **Whistle key (H; pad: hold X with nothing to use in front)**. Tap =
"come to me" (every tame of yours within earshot follows). Hold = the order wheel in whistle mode: same stone wheel,
centre shows "Whistle · 3 tames in earshot", wedges Follow / Stay / Passive / Aggressive (no Ride, no Release: those
are one-tame acts). A two-note whistle plays from you for everyone near (so friends hear it), and each tame that heard
it answers with its own call. Earshot from tuning (30 m). With no tame in earshot the tap says so and the wheel shows
"No tames in earshot" with every wedge dimmed (no dead state: Esc / release closes it).

**Held tools.** *Valheim* tools read from silhouette: a thick haft, a big wedge head, visible lashing; *Ark*'s stone
pick is a curved stone bar lashed into a split haft. Real reference: Neolithic hafted axes (the Ötzi copper axe;
Danish flint axes) are a tapered ash or yew haft swelling at the butt, a socket or split holding a ground stone celt,
bound with rawhide or sinew in criss-cross wraps; flint heads show conchoidal facets and a ground bit. Antler and stone
picks are a double point on a mortised haft. Taken: modelled in code as real meshes (no engine shapes): a lathe-turned
haft (oval section, taper, butt knob, slight bend), a knapped head (facet-shaded, chipped edge, ground bit), a criss-
cross lashing (two helices crossing over the head), all one runtime mesh with vertex colour wood / stone / hide and a
material that draws grain and grit from that colour. Crude tools: flint, rough, darker, thinner lashing. Stone tools:
ground basalt head, broader, neater wraps.

**Detail matching collision.** *Valheim* and *Rust* building: what you see is what you bump; doorways are walkable,
fence gaps are see-through but block, furniture collides as its top and legs so items fall and arrows pass under a
table. Taken: furniture collision rebuilt from the same measurements as its detail (top + legs, seat + back, bed frame
+ headboard, brazier bowl on legs), the pen's walls follow its rails, the banner and rug get real detail, and the
Hearth / stations / totem / chest collide as the box their prop actually fills (fitted from the prop's bounds on every
machine, the server included, so movement prediction agrees).

**Bounties.** *Rust* and *Ark* have none; *Albion*, *EVE* and *Mortal Online* bounties need a working economy, criminal
flags and a law system to be fair. Wildreach has no currency or crime flag yet (FULL-GAME R9 puts bounties with the PvP
pass). A half bounty system would be a placeholder. Decision: **keep hidden** (nothing reads the flag, no screen shows
it) and record it for the R9 PvP cook.

## 3. Plan (design every detail, then the build steps)

### 3.1 Skill nodes (R1)
- `nodeEffects` gains `fire.shockwave: { blastRadius: 0.1 }`, `frost.biting_cold: { slowStrength: 0.08 }`,
  `frost.wide_storm: { areaRadius: 0.08 }` (per rank, ranks from the tree data).
- Shockwave: every fire blast's radius x (1 + blastRadius): Explosive bolt's burst, Overheat, Wildfire, Lava pool,
  Meteor (EWRAreaShape spells with Element fire), the Giant's-ember shockwave.
- Biting cold: the Slowed status the attacker applies slows harder: speed x slowMultiplier^(1 + slowStrength) on the
  victim... simpler and replicated: Slowed is a tag; the strength rides a per-target "slow strength" float set by the
  last slower (server) and read in GetSpeedMultiplier on server and owning client. Owning client must agree for
  prediction: store it in a replicated attribute? Plan: `UWRStatusComponent` keeps `SlowExtra` (replicated) set when
  Slowed is applied by a source with biting_cold. GetSpeedMultiplier multiplies by (1 - SlowExtra).
- Wide storm: Frost area spells (Frost nova, Blizzard, Ice wall length, Ice lance width, chain shatter radius, Whiteout
  area by Blizzard's radius) x (1 + areaRadius).

### 3.2 Perks (R2): build list and hide list
Data: each perk keeps id/name/text; working ones get a `stat` (+ `value`); waiting ones get `waitsFor`. Text fixed
where Raven's line promised more than the build (Beast lord: "One more active tame").

| Skill | Perk (tier) | Decision | How |
|---|---|---|---|
| Mining | Prospector (25) | BUILD | ore nodes within 35 m show a small ore glyph through rock (HUD marker, own screen only) |
| | Strong back (25) | BUILD | ores and stone weigh half (`oreWeight -0.5`) |
| | Deep delver (75) | BUILD | lava and hazard damage -50% (`hazardDamage -0.5`) |
| | Vein breaker (75) | BUILD | a vein with a third or less left breaks all at once |
| | Rich veins (100) | BUILD | a vein you empty comes back 40% sooner |
| | Unbreaking (100) | BUILD | picks don't wear |
| Woodcutting | Light load (25) | BUILD | wood weighs half |
| | Clean cut (50) | BUILD | the last hits of a tree come at once (a tree with a third left falls) |
| | Bark harvest (50), Timber, Seedsaver (75), Ancient wood (100) | HIDE | resin/bark items, falling trees, saplings, biome sets |
| | Endless edge (100) | BUILD | hatchets don't wear |
| Fishing | every level | BUILD | bites come 0.5% sooner per level |
| | Patient angler (25) | BUILD | rare fish x1.6 |
| | Net caster (25) | BUILD | 20% chance of two |
| | Deep line (50) | BUILD | open-sea fish (cast 12 m+ from the shore into deep water) can bite |
| | Bait maker (50) | BUILD | bait recipes (Ember grub lure, Rime lure) target rare fish |
| | Treasure hook (75) | BUILD | 8% of catches are a find from a table (ore chunks, gems, an old tool) |
| | Spear fisher (75), Monster line (100) | HIDE | underwater, sea taming |
| | Legend catch (100) | BUILD | each biome's legendary fish can bite |
| Tracking | every level | BUILD | (no tracks exist) re-pointed: kills drop 0.3% more meat and hide per level |
| | Scent, Spoor reader (25) | HIDE | creature search, levels |
| | Hunter's mark, Apex hunter, Rare finder | HIDE | map marking, roaming minis on map, mutations |
| | Predator sense (75) | BUILD | "Hunted" warning + direction pip when an aggressive creature targets you |
| | Beast lore (75) | BUILD | your lock-on / target shows its weakness and resistances |
| Smithing | Efficient smith (25) | BUILD | weapon recipes take 20% fewer materials (each input rounded down, min 1) |
| | Quick hands (25) | BUILD | weapon orders 30% faster |
| | Infuser (50), Reforger, Careful hands (75), Signature (100) | HIDE | infusion strength, rerolls, gem removal, maker's marks |
| Armorcraft | Efficient armorer, Quick fit (25) | BUILD | as smithing, for armorer recipes |
| | Reinforced (50) | BUILD | +10 poise |
| | Light weave (50) | BUILD | armor weighs 30% less |
| | Set crafter, Creature armorer (75) | HIDE | sets, tame armor |
| | Bastion (100) | BUILD | blocking costs 25% less stamina |
| Alchemy | Long brew (50), Mixologist (75), Philosopher (100) | HIDE | timed brews, mixing, elixirs |
| | Deep belt (100) | BUILD | one more belt slot |
| Cooking | Recipe sense (25), Preserver (50), Camp chef (75), Banquet (100) | HIDE | discovery, spoilage, campfires, clan buffs |
| | Bulk cook (25) | BUILD | food recipes make two |
| | Shared meal (50) | BUILD | eating feeds clan mates within 10 m half as much |
| | Hearty (75) | BUILD | hunger drains 25% slower |
| Farming | Grafting, Rare seeds, Water wise, Companion planting, Golden crop | HIDE | greenhouses, seed rarity, irrigation |
| Husbandry | Large pens, Bee whisperer, Shearer, Selective breeding, Content herd, Prize stock, Royal jelly | HIDE | pens, bees, wool, breeding |
| Building | every level | BUILD | (building is instant) re-pointed: pieces you place have 0.5% more health per level |
| | Quick repair (25) | BUILD | repairs cost half |
| | Load bearing (50), Decorator (75), Master builder, Siege proof (100) | HIDE | per-player spans, comfort, biome sets, explosives |
| | Fortifier (50) | BUILD | your walls, doors, gates, palisades +25% health |
| Engineering | every level | BUILD | stations run your orders 0.5% faster per level |
| | Tinkerer (25) | BUILD | stations and code-lock doors cost 25% less |
| | Efficient power (25), Long lines, Turret tech (50), Siege engineer (100) | HIDE | power, conveyors, turrets, siege |
| | Overclock (75) | BUILD | your orders 60% faster, one wood burned per item (no wood: normal speed) |
| | Self repair (75) | BUILD | your stations and doors mend 2% a minute after 60 s without damage |
| | Chief engineer (100) | BUILD | orders up to 99, one extra queue slot for you |
| Taming | Breeder (75), Soulbond (100) | HIDE | breeding, permanent death |
| | Pack leader (75) | BUILD | your tames within 20 m hit 20% harder |
| Swimming | every level | BUILD | swim 0.4% faster per level |
| | Strong stroke (25) | BUILD | fast stroke costs half |
| | Water fighter (50) | BUILD | your hits from the water +25% |
| | Seaborn (100) | BUILD | fast stroke is free |
| | Diver, Pressure, Current rider, Salvager, Abyss diver | HIDE | diving, depth, currents, sunken loot |
| Sneaking | Shadow (25) | BUILD | noticed 30% later in the dark |
| | Backstab (50) | BUILD | x1.5 from behind on a creature that hasn't noticed you |
| | Ambush (50), Silent charges (75) | HIDE | hidden tames, explosives |
| | Vanish (75) | BUILD | a dodge breaks the sight of creatures on you for 2 s |
| | Ghost (100) | BUILD | standing still in the dark, creatures don't notice you |
| | Assassin (100) | BUILD | first hit on a creature that hasn't noticed you is critical |

Screen (WRUISkills): hidden perks and empty tiers are skipped; a tier with one perk shows one card and "Take";
pending counts only tiers with a visible perk; the "takes effect once its system is in" line and the chat suffix are
removed. Server: `ChoosePerk` refuses a waiting perk. Every stat read through `UWRLifeSkillsComponent::StatFor`.

### 3.3 Swimming (R5)
- XP: server watches each swimming player (WRWater subsystem, 0.5 s): metres swum horizontally x `lifeSkills.xp.swim`
  (0.5 per metre), capped per tick (no teleport farming).
- Fast stroke: Sprint while swimming = `water.fastSwimMultiplier` 1.6 x swim speed, drains `water.fastSwimStamina`
  8/s x (1 + swimStamina). Prediction: the sprint flag is already a saved-move flag; IsSprinting() learns swimming;
  GetMaxSpeed uses MaxSwimSpeed x skill speed x fast. Stamina drain lives where sprint drains it (server).
- Water fighter: ApplyHit, attacker swimming or wading (feet in water) -> damage x (1 + waterDamage).

### 3.4 Engineering (R4)
- XP `lifeSkills.xp.machineItem` 3 per item a station finishes for its owner; `machinePlace` 30 for placing a station,
  `machineLock` 20 for setting a door code.
- Speed: job seconds / (1 + machineSpeed + overclock + quick hands/fit) computed when queued, with the maker.

### 3.5 Fishing (R3) — GAP-cooked (FULL-GAME puts fishing in R6; the skill exists now so it's built now)
- **Rod**: `fishing_rod` (hand recipe: 3 wood, 6 fiber, 1 stone), a tool of kind `rod`, held in the weapon hand.
  Durability 60, 1 wear per catch. Modelled like the tools (haft + line guide + reel spool of wound fibre).
- **Bait** (optional): `cut_bait`: 1 raw meat -> 6 strips at hand (no grubs: shrubs don't drop them).
  Bait maker unlocks `ember_lure` (volcanic) and `rime_lure` (tundra) at the workbench. Bait sits in the belt? No: a
  rod uses the first bait in your bag automatically (one per cast that bites), shown on the prompt.
- **Where**: aim at water within 4-18 m (cast distance from how long Light is held, 0.2-1.2 s); the float lands on the
  sea or a river surface (the sea mask / water volumes; `FWRWaterRules` water test). "Deep" = water point 12 m+ from
  any land cell and depth > 6 m. Biome from x (west volcanic, east tundra; `FWRWorld` biome lookup).
- **Flow (server state machine on `UWRFishingComponent`, replicated to everyone for the float; owner gets tension)**:
  Idle -> Casting (0.5 s arc) -> Waiting (bite after `fishing.biteMin..biteMax` 6-20 s / (1+biteSpeed), bait x0.7) with
  1-3 nibbles (small dip, soft tick) -> Bite (float plunges, splash fx, `fish.bite` sound, rumble; `fishing.hookWindow`
  0.9 s) -> Hooked (hold Light to reel: line shortens `reelSpeed` m/s; the fish surges every 1.5-3 s for 0.6-1.4 s by
  its `fight`; reeling in a surge adds tension, out of a surge it bleeds; tension 1.0 for 0.5 s snaps the line (lose
  bait, 3 wear); reeling costs 6 stamina/s; out of stamina you can't reel) -> Landed (catch card, fish into the bag,
  XP) or Lost. Moving away 3 m, swimming, attacking, blocking or switching items reels in and cancels.
- **Fish table** `Data/fish.json` (name, biome, water sea/river/deep, time day/night/any, rarity weight, kg range,
  fight 0-1, item, legendary):
  - Volcanic sea: Cinder snapper (common), Ashfin mackerel (common), Obsidian ray (uncommon), Magma eel (rare, night).
  - Volcanic river (steaming north river): Steamfin trout (common), Sulfur carp (uncommon).
  - Tundra sea: Rime cod (common), Glassfin herring (common), Ice halibut (uncommon), Aurora char (rare, night).
  - Tundra rivers/lakes edge: Frostjaw pike (uncommon), Silver grayling (common).
  - Deep (deep line): Cinder marlin (volcanic), Pale sturgeon (tundra).
  - Legendary (legend catch): Emberking (volcanic deep, night), Old Whitegill (tundra deep, day).
  Each fish is an item `fish_<id>` (food: hunger 8-20 raw at reduced value... raw fish edible with low value),
  cooked at the workbench into `cooked_fish` (hunger 35). Treasure table: copper/tin/iron ore x2-4, a gem (5%), sulfur
  or lichen.
- **Look & sound**: the float is a small two-tone carved float (runtime mesh, red cap, pale body) on the surface, bobbing
  with a sine and dipping on nibbles; the line is a thin cable from the rod tip to the float (`UCableComponent`? not
  a dependency today -> a thin stretched mesh segment updated per frame). Sounds (table rows, synthesized OGGs): `fish.cast`
  (whoosh), `fish.plop` (float lands), `fish.nibble`, `fish.bite` (deep plunge), `fish.reel` (ratchet loop ticks),
  `fish.snap`, `fish.land` (splash + flop). Fx rows: splash ring on land / bite.
- **HUD**: one small arc near the crosshair while hooked: tension (pale -> ember red), the fish's distance in metres;
  prompts "Hold {Light} to reel"; catch card through `ClientNotify` + moment ("Ice halibut 6.4 kg · new record").
  Pad: Light is RB; same flow.
- **Multiplayer**: server decides bite, fish, surges, snap, catch; the float position and state replicate (everyone
  sees your float dip); the client sends cast (with charge), hook press, reel held/released.

### 3.6 Tame whistle (R6)
As researched. Keyboard action `Whistle` (H) in WRKeyBindings (rebindable, on the F1 page); pad: hold X with nothing to
use in front. Server command `whistle <order>`: tames within `taming.whistleRange` 3000; plays `tame.whistle` at the
player (broadcast) and each answering tame's own call. Wheel mode 3 reuses SWRWheel with its own order list.

### 3.7 Build pieces (R9)
- ApplyLook collision: table = top slab + 4 legs; chair = seat + back; bed = frame (0-40) + headboard; brazier =
  bowl box on 4 legs; pen = rails plane walls 0-114 thick 8; banner = pole + cloth; rug stays 2 cm.
- Detail: banner = a pole with a finial, a crossbar, a hanging cloth in three panels with a fringe; rug = a woven
  border, a field, two bands and fringe ends (cloth tones).
- Child props: `FWRBuildLook::FitCollision` sizes a box component to the prop's fitted bounds on every machine; the
  plain shape stops colliding.

### 3.8 Held tools (R8)
`WRToolMesh` (new, runtime procedural mesh): `Haft`, `Head(hatchet|pick|rod)`, `Lashing`; vertex colours carry the
material zone; material `M_HeldTool` (built headless with a python script) reads vertex colour as base colour, adds
grain (stretched noise along the haft) and grit; no texture over 512. If the material asset can't be built (no Fab
check pass), the tools use the flat shape material with vertex colour off — never cubes.

### 3.9 Build order
1 nodes, 2 perk data + screen + server, 3 perk effects, 4 swimming, 5 engineering, 6 whistle, 7 fishing, 8 build
pieces, 9 held tool meshes, 10 docs, compile, commit, push, LANDED.

## 4. Build notes (where the plan moved while building)
- Deep belt (alchemy 100) is hidden too: the game has no potion belt slots (potions go on the hotbar), so it waits for
  "potion belt slots". Alchemy's 50 / 75 / 100 tiers show only what works (Quick drink, Potent; 100 is hidden).
- Fish are edible raw (they're category food) and grill at the workbench from the six commonest kinds; no generic
  "any fish" recipe input (the crafting screen shows one item per input).
- Fish bag weight is a per-kind average (a marlin is 25 kg in the bag), not the rolled weight on the catch card.
- Tames answer the whistle with their own "idle" call, through the characters' multicast ("voice.idle").
- Held tools: built in code as one runtime mesh (WRToolMesh) on M_HeldTool (built headless by
  Tools/art/make_held_tool_material.py, engine noise only, 16 KB). No AI pipeline on this PC, so no sculpted art; the
  shapes are modelled from the hafted-axe reference rather than engine primitives.
- Icons for the 21 new items drawn by Tools/art/make_fish_icons.py in the UI's flat style.

## 5. Requirement ledger: proof (commit in LANDED.md)
| # | Done | Where |
|---|---|---|
| R1 | Yes. The 12 passives already worked; the 3 dead stat nodes now work | `WRAbilities.cpp:875-880` (area spells), `WRProjectile.cpp:270`, `WRCombatLibrary.cpp:125` (Shockwave); `WRStatusComponent.cpp:165`, `WRCharacterBase.cpp:79,119` (Biting cold, replicated); `tuning.json` nodeEffects |
| R2 | Yes: 41 more perks built (66 of 120 work), 54 hidden with `waitsFor` until their system ships; screen, old menu and server honour it | `Data/lifeskills.json`; `WRLifeSkills.h` (WaitsFor, LiveCount), `WRLifeSkills.cpp:69,229`; `WRUISkills.cpp` (PendingTier, TierState 3, Take/Choose); `WRPlayerMenus.cpp`; effects in `WRInventory.cpp` (weights, shared meal), `WRResourceNode.cpp` / `WRWorldDressing.cpp` (wear, vein/tree break, rich veins), `WRCombatLibrary.cpp:314,354,407` (deep delver, bastion, backstab, assassin, water fighter, pack leader), `WRCreature.cpp` PickTarget (shadow, ghost, vanish), `WRAbilities.cpp` dodge (vanish), `WRCrafting.cpp` (materials, speed, overclock, chief engineer, bulk cook, quick repair), `WRPlayerBuild.cpp` (tinkerer, sturdier pieces, fortifier), `WRBuilding.cpp` (self repair, maxHp saved), `WRStatusComponent.cpp` (hearty), `WRPathComponent.cpp` (reinforced), `WRPerkHud.cpp` (prospector, predator sense, beast lore) |
| R3 | Yes: fishing built | `WRFishing.h/.cpp` (XP `WRFishing.cpp:367`), `Data/fish.json`, items / recipes / sounds / fx rows |
| R4 | Yes | `WRCrafting.cpp:661`, `WRPlayerBuild.cpp:745,796` |
| R5 | Yes | `WRCharacter.cpp:192`; fast stroke `WRCharacterMovementComponent.cpp`, `WRStatusComponent.cpp` |
| R6 | Yes: Whistle key H + hold Interact with nothing in front (pad X), wheel mode, server order, sound | `WRPlayerMenus.cpp:733`, `WRPlayerUI.cpp:900`, `WRUIWheel.cpp`, `WRPlayerController.cpp` (TamesInEarshot), `WRKeyBindings.cpp`, `Data/gamepad.json` page |
| R7 | Kept hidden (no crime flag or currency to build a fair one on); nothing reads the flag and the settings screen leaves it out | `WRWorlds.cpp:58`; LATER-COOKS line for the R9 PvP cook |
| R8 | Yes, modelled in code (no art pipeline on this PC) | `WRToolMesh.h/.cpp`, `WRHeldTool.cpp:69`, `Content/Wildreach/Materials/M_HeldTool` |
| R9 | Yes | `WRBuilding.cpp` ApplyLook (bed, table, chair, brazier, torch, pen, banner, trophy, rug), `WRBuildLook.cpp:604` (banner / rug detail), `WRBuildLook.cpp:819` FitCollision (Hearth, stations, totem) |
| R10 | Whistle wheel: mouse (release / click) and pad (hold X, stick); perks screen uses the existing pad processor; fishing is RB/RT on pad | as R6, R2 |
| R11-R13 | Numbers in `tuning.json` (fishing, lifeSkills perks + hud, water fast stroke, taming whistle, toolLook rod), sounds and fx as rows; perk hits through ApplyHit; unfinished perks hidden | as above |
| R14 | Design intent written per system in section 3 | this doc |
| R15-R18 | See LANDED.md line and the commit | CHANGELOG "Unreleased", HANDOFF |
