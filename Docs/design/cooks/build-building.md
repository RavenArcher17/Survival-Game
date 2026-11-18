# Wildreach cook: BUILD-BUILDING (FULL-GAME 6.4)

Lane build-building, 2026-11-14. Scope: metal tier and biome material tier, freeform structural pieces and structural
integrity, biome-themed building sets, comfort buff from decor, powered turrets; Raven's rules kept (no traps in
bases, no underground bases, no NPC guards). Spec sources: FULL-GAME 6.4, cooks/survival-crafting.md 2.2 / 2.17 / 2.18
(the numbers below come from there unless marked GAP-COOKED here).

## 0. Requirement ledger

| # | Requirement (split from the brief, FULL-GAME 6.4 and the Aaron quotes) | Done where |
|---|---|---|
| R1 | Metal tier: Hearth 4, span 8, wall 2,500 / foundation 3,500 / door 2,800 HP, wall = steel plate 8 + stone 10, upkeep 0.1 steel plate a piece an hour | `Data/building_sets.json` tiers.metal; `WRBuildSets.cpp` FWRBuildSets::Expand |
| R2 | Metal look: brushed dark steel panels, rivet lines, on a stone footing (all 12 shapes) | `WRBuildLookSets.cpp` style `panels` |
| R3 | Biome material tier: Hearth 5, span 8, wall 3,000 / foundation 4,200 / door 3,200, 20 biome blocks a wall, upkeep 0.1 block | building_sets.json tiers.*; Expand |
| R4 | Biome block per land: workbench L5, stone 4 + signature resource + steel plate -> 4 | `Data/recipes.json`, `Data/items.json` |
| R5 | 12 biome-themed sets, each 12 structure shapes with its own look (Basalt hall ... Hollow bastion) | building_sets.json looks; WRBuildLookSets.cpp |
| R6 | Each set: 6 decor pieces (comfort group "biome set" 2) + 1 biome rug (floor 2), modelled | building_sets.json decor; WRBuildLookDecor.cpp |
| R7 | Set decor blueprint drops from that land's major boss | `Data/gear.json` blueprints; FWRBlueprints learn pieces; build bar lock |
| R8 | Sets of lands not in the game stay hidden (liveWith) | Expand sets LiveWith; IsPieceLive |
| R9 | Build bar: a Metal page and one page per biome set | `WRUIBuild.cpp` material-major order + page tag names the material |
| R10 | Freeform structural pieces per tier: beam 2 m, beam 4 m, post 3 m, brace 45 deg; any angle; snap only at their ends to any surface | building_sets.json freeform; WRPlayerBuild beam placement; pitch on the place command and save |
| R11 | Integrity: Raven's span rule kept; a beam or post adds 1 span per 2 m; a brace touching a wall and a pillar gives the wall +1 span | `WRBuildIntegrity.cpp` FWRIntegrity |
| R12 | Stability colours in build mode: blue grounded, green, yellow, orange, red (one step from failing); the ghost shows the colour it would be | WRBuildIntegrity.cpp tint + ghost colour |
| R13 | Collapse: 4 to 8 rubble chunks, dust, stone-or-timber crack sound, chunks fall 3 s, 50 damage via ApplyHit (Collapse source), vanish; checks what it held | AWRRubble in WRBuildIntegrity.cpp; RecomputeSupport |
| R14 | Comfort buff from decor: counted at the bed, under a roof, best per group, cap 20 (BUILT by build-survival) | `WRSurvival.cpp` FWRComfort::At (proof) |
| R15 | Every comfort-table decor piece exists: bench, carved throne, long table, fur bed, tapestry, fireplace, candles, lantern, crystal lamp, sunstone lamp, potted plant, flowering pot, wooden tub, hot spring bath, hand drum, lyre (one music counts), mirror | pieces.json + WRBuildLookDecor.cpp |
| R16 | Hot spring bath only on a volcanic vent (lava within 8 m) or a tundra hot spring | FWRBuildSets::PlaceCheck |
| R17 | Hearth offering: a cooked meal on a new Hearth in its first game day: +2 to every bed in that claim for good | `WRBuildSets.cpp` offering + FWRComfort |
| R18 | Decorator perk live: comfort +3, cap 23 | lifeskills.json; WRSurvival |
| R19 | Build card shows a comfort number on every decor card | WRUIBuild Describe (BUILT) + biome set |
| R20 | Bolt turret: Hearth 4, steel plate 8 + crossbow + bronze 4, 1,500 HP, 6 P, crossbow bolts from its box, 30 m, hostile creatures / non-clan players in the claim on PvP, 0.9 s red aim line, bolt straight at 4,000 cm/s never homing, 45 damage, a shot every 1.5 s, ApplyHit | `WRTurret.cpp`, machines.json, pieces.json |
| R21 | Element turret: Hearth 5, 8 P, a path material per 50 shots, 20 m, fire = 6 m cone 30 dmg + burn 20, frost = slowing bolt, other paths their status, same tell | WRTurret.cpp; turret table in machines.json |
| R22 | Turrets are buildings: never move, need power and ammo; tames guard; no NPC guards | WRTurret.cpp (no pawn) |
| R23 | Turret tech perk (+20% turret damage) live | lifeskills.json; WRTurret |
| R24 | Turrets look engineered: slewing ring on a worm, a windlass the shaft winds, the bow, a bolt box feeding, the port where a shaft joins | WRMachineLook.cpp Turret model |
| R25 | No traps: nothing hurts you for touching it | WRTurret targets only; no contact damage anywhere |
| R26 | No underground bases: no structure, storage, bed, station, turret or claim piece places in a cave (lights and drills may) | FWRBuildSets::PlaceCheck |
| R27 | Building XP: freeform structural 8; metal and biome pieces 12 | WRPlayerBuild place |
| R28 | Upkeep for metal and biome pieces paid from the totem in their items; the claim screen shows them | WRBuilding UpkeepPerHour/CurrentUpkeepPerHour; WRUIBuild claim panel |
| R29 | Materials take damage by tier (weapons/tools on pieces: wood 10%, stone 2%, metal and biome 0%) | building_sets.json materialDamage; TakeBuildDamage callers |
| R30 | Pad and mouse on every screen touched (build bar, machine screen) | WRUIBuild / WRUIMachine via the shared input processor |
| R31 | Data-driven: numbers in Data/*.json + tuning.json | building_sets.json, tuning `building.*`, `turrets.*` |
| R32 | "Design with intention": every piece answers why it is here, what it connects to, how it meets what it touches | plan 2.x |
| R33 | Woodcutting Ancient wood / Building Master builder perks: their system (biome sets) ships | lifeskills.json |
| R34 | Report LFS size of new binaries | final line |

## 1. Research

**Valheim (Hearth & Home, Mistlands).** Structural integrity is a per-piece support value shown only while the hammer
is out: blue grounded, green, yellow, orange, red, and red pieces break by themselves. Support spreads outward from
the ground and drops with each piece; wood carries 4 steps, core wood 6, iron poles further, and stone barely
cantilevers. The lesson: the colour tells you the rule without a tutorial, and a single post under a red span turns it
green again. That is exactly our span rule, so we keep Raven's integer steps and paint them. [xgamingserver Valheim
guide](https://xgamingserver.com/blog/?p=21416), [mobalytics building guide](https://mobalytics.gg/gamebase/guides/valheim-general-building-guide),
[gamerevolution supports](https://www.gamerevolution.com/?p=674799). Valheim comfort: one piece per group counts, so
variety beats stacking (already our rule).

**Rust (build tiers and turrets).** Twig -> wood -> stone -> sheet metal -> armored: each tier multiplies raid cost
(sheet metal ~1,000 HP, armored ~2,000 on current cheat sheets), so a base upgrades in place over weeks. The auto
turret needs 10 power and a box of matching ammo, sees ~30 m and is a building, not an NPC; the flame turret is a
5-6 m cone. Lessons: turrets live on the power grid you already built; ammo is a real cost; ranges near 30 m for a
gun and ~6 m for flame read right. [Rust cheatsheets](https://rust-survival.pages.dev/10_Cheatsheets.html),
[xgamingserver Rust defence](https://xgamingserver.com/blog/rust-base-defense-turrets-traps/),
[Rust building tiers](https://xgamingserver.com/blog/rust-base-building-guide/).

**Conan Exiles / Enshrouded / V Rising.** Conan's stability percentage drops with distance from foundations and
pillars restore it; Enshrouded lets any block be any material and changes only the look and strength; V Rising's castle
floors raise a base's "comfort" style buff with decor. Lesson: a material tier should be the same shapes in a new look
(players keep their floor plan and upgrade it), and themed sets should feel like the land they came from.

**Real-world reference.** Basalt hall: the Giant's Causeway's hexagonal columns (40-50 cm across, tops at stepped
heights). Crag hold: Welsh slate and granite with copper flashing that weathers to verdigris (Cu2CO3(OH)2, a pale
blue-green). Sun court: Andalusian and Maghrebi horseshoe and round arches of sandstone voussoirs, shaded arcades.
Heartwood lodge: cruck and box timber framing with braces (English and Norse halls), sod and moss roofs. Cloud spire:
whitewashed Cycladic stone with blue tile. Geode sanctum: amethyst geodes (druse crystals set in dark basalt).
Rime lodge: Norse log houses on a stone sill with dragon-head gable prows (Borgund stave church), snow left on the roof as
insulation. Stilt house: Southeast Asian and Louisiana stilt houses, reed thatch. Coral keep: coral-rag stone houses of
Zanzibar and Bermuda, shell-tile, porthole windows. Mourning chapel: Gothic pointed arches, lead-came stained glass,
iron ridge cresting. Mycelium den: bracket fungi (Ganoderma shelves), gilled caps. Hollow bastion: obsidian and dark
stone with a violet edge glow for the rift's warp.

**Turret mechanism (real reference).** A springald / ballista is spanned by a windlass: a drum with a crank winds a cord
that draws the bow string back to a trigger nut; heavy crossbows used a windlass of rope and pulleys. A siege engine
traverses on a turntable. So our bolt turret: a powered shaft turns a worm that drives the turntable (slew) and a
second windlass drum that spans a steel bow; the bolt drops from a gravity box over the groove; the trigger releases.
[Springald](https://wikipedia.com/wiki/Springald), [Crossbow spanning](https://wikipedia.com/wiki/Crossbow).

## 2. Design (every gap solved)

### 2.1 Tiers and blocks (SPEC numbers; shapes GAP-COOKED)
Shapes and their share of a wall's cost / HP (stone ratios): foundation 1.5 / 1.4 (metal and biome foundations use the
spec's 3,500 / 4,200), wall 1 / 1, doorway 0.8 / 0.9, door (lockable for metal and biome: a code lock) 1.2 / spec door,
window 0.8 / 0.8, half wall 0.5 / 0.6, floor 0.8 / 0.9, stairs 1.1 / 0.8, roof 0.8 / 0.8, gable 0.5 / 0.7, upper wall
0.7 / 0.8, pillar 0.5 / 0.8. Costs round up, minimum 1.

Biome blocks (12 items, stack 50, weight 1.2): basalt block (sulfur crystal), crag block (stormglass), sandstone block
(sunstone), heartwood block (heartwood), skystone block (levistone), geode block (resonant crystal), rime block
(lichen), mangrove block (bloodmoss), coralstone block (tidecoral), chapel stone (soulstone), mycel block (deepcap),
voidstone block (voidglass). Workbench L5, 20 s, Building life skill XP on making them is the crafting rule already.

Upkeep: each tier names its upkeep item (metal -> steel plate, a set -> its block) at 0.1 a piece an hour; paid from
the totem as wood and stone are.

Damage by material (R29): `materialDamage` in building_sets.json: weapons and tools on pieces wood 0.1, stone 0.02,
metal 0, biome 0 (raid tools and paths keep the raid lane's numbers).

### 2.2 Looks (why, what it meets)
Every tier piece is drawn by the same block kit as wood and stone (instanced blocks over the unchanged collision), with
one new per-block material `M_BuildTinted` that takes colour, metalness, roughness, glow and a surface pattern (grain,
brushed, mottle) per block, so 13 tiers cost one material. Joints meet as on wood and stone: corner posts and quoins
sit on the cell corner so two walls share one; wall tops are level at 300 so floors land on them; gables cut each row
at the slope so nothing pokes through the roof deck.

| Tier | Wall | Openings | Floor / foundation | Roof | Pillar | Accent |
|---|---|---|---|---|---|---|
| Metal | 45 cm stone footing courses; four riveted brushed-steel panels (2 cm seams) between H-section steel posts at the corners; a steel mid-rail; rivet rows 12 cm apart on every seam | Steel box frame, glazing bars, dark glass; door: riveted steel leaf with three straps | Riveted deck plates on joists; foundation: stone courses with a steel kerb angle on the top edge | Standing-seam steel panels (raised seam every 50 cm) | H-section steel column on a stone plinth, base plate with 4 bolts | Rivets |
| Basalt hall | Hexagonal basalt columns (44 cm across flats) packed along the wall, tops stepped 0-12 cm, an obsidian lintel on top | Obsidian frame, ember-glass (glowing orange) panes | Causeway hex pavers; plinth of columns | Basalt slates, obsidian ridge | One big hex column | Ember-glass glows at night |
| Crag hold | Pink-grey granite ashlar with a dark slate band every third course; verdigris copper cap flashing | Granite jambs, dark storm shutters hung open on iron straps | Slate flags | Blue-grey slate, verdigris copper ridge | Granite drum column | Copper weathered green |
| Sun court | Large pale sandstone blocks, a sunstone inlay band (amber glow) at 2 m | Round arches of voussoirs on a keystone | Sandstone flags | Clay barrel tiles in rows | Round sandstone column with capital | Sunstone band |
| Heartwood lodge | Heartwood box frame (sill, posts, rail, two braces) with plank infill, carved notch in amber at each joint | Heartwood frame, amber glass | Wide heartwood boards | Moss mound over shingles | Carved heartwood post | Amber notches glow faintly |
| Cloud spire | White ashlar, a blue tile band | Open round arches with a levistone keystone hovering 6 cm over the arch (pale glow) | White flags with blue border | Blue glazed tiles | White column, blue capital | Hovering keystones |
| Geode sanctum | Dark stone courses with violet crystal druses set in at random courses | Faceted crystal panes in a diamond lattice | Dark flags with crystal veins | Dark slates | Dark pillar with a glowing crystal core strip | Crystal glow |
| Rime lodge | Stone sill 1 m, horizontal pine logs above, saddle-notched log ends crossing past the corner | Pine frame, frost-glass (pale blue) | Pine boards | Shingles under a packed-snow mound | Pine post on a stone | Carved prow on each gable peak |
| Stilt house | Black mangrove frame, woven reed panels | Reed screens, lacquered frame | Split mangrove poles on joists; foundation on four mangrove stilts | Thick reed thatch | Mangrove pole | Red-lacquered bone pegs at corners |
| Coral keep | Coral-rag stone with coral lumps (pink, orange) | Round porthole: a ring of stones round sea-glass | Shell-tile floor | Overlapping shell scallops | Coralstone column | Coral clusters |
| Mourning chapel | Grey ashlar with buttress quoins | Pointed arches, stained glass in bruise colours with lead cames | Grey flags | Dark slate with iron cresting on the ridge | Clustered grey column | Stained glass glow |
| Mycelium den | Cream mycelium felt panels between root stems | Gill-lined round openings | Layered bracket-fungus shelves | Smooth cap with glowing gills under the eave | Stem column | Gill glow |
| Hollow bastion | Grey stone courses with black voidglass slabs, violet edge strips | Voidglass frame, black panes, violet rim | Grey flags with voidglass inlay | Voidglass slates | Voidglass shard column | Violet rim glow (the air "bending") |

### 2.3 Freeform structural pieces and integrity (RAVEN-PLANNED; mechanics GAP-COOKED)
- Pieces per tier (wood, stone, metal, 12 sets): beam 2 m, beam 4 m, post 3 m, brace (2.8 m on a 45 deg rise). Cost per
  beam 2 m = 0.3 wall, beam 4 m 0.5, post 0.4, brace 0.3. HP 0.6 of a wall. Section 24 x 24 cm (metal: an H-section).
- Placing: the piece's start is the point you aim at (its end snaps there, on any piece's surface or the ground); with
  snap on it snaps onto another beam or post end within 40 cm, so beams chain. A post stands straight up. A beam lies
  along where you look, its slope following your look in 15 deg steps (5 with snap off), flat to 60 deg; the brace
  rises 45 deg away from you. R / the wheel turn it. The place command carries the pitch; the save keeps it.
- Support: a beam or post takes its support from what its ends touch (the ground = 0, a piece = its support), plus 1
  per 2 m of its length (rounded up). It passes support on: a snapped piece whose box the beam runs under or through
  (25 cm) is supported at the beam's value + 1; a piece standing on a post's top end gets the post's value + 0.
- Brace: a brace whose one end touches a wall (or any edge piece) and whose other end touches a pillar or a post lets
  that wall go one step further than its material's span.
- Colours: while the build bar is open, every structure, beam and post within 25 m wears a see-through overlay by how
  much span is left: blue on the ground, then green / yellow / orange as it is used, red at the last step. The ghost of
  a structural piece you're placing wears the colour it would be (invalid stays the dark red with the chip saying why).
- Collapse: the server checks what a destroyed or demolished piece held (built). Each piece that falls breaks into 4 to
  8 rubble chunks (by its size) of its material and colour, flung a little outward with dust (`build.collapse` fx) and a
  crack (`build.collapse.stone` / `.wood`), falling for 3 s; a chunk landing on someone deals 50 damage once through
  ApplyHit (no instigator: the building); then they vanish (shrinking out the last 0.3 s).

### 2.4 Biome set decor (6 + rug each; comfort "biome set" 2, rug "floor" 2)
Each is modelled from blocks in the set's palette at real furniture sizes (seat 45 cm, table 76 cm, bed 60 cm, bath
60 cm deep). Placement is free (DR #28), on any floor. Collision is its own box rows in building_sets.json.

| Set | Pieces (archetype) |
|---|---|
| Basalt hall | Obsidian throne (throne), forge-hearth fireplace (fireplace: fire comfort 2 too? no: the set group), lava-glass lamp (lamp), anvil table (table), ash-wood bench (bench), hot spring bath (bath, vent only) |
| Crag hold | Copper lightning-rod lamp, fur-draped bench, slate table, wind chime, horn rack, storm drum |
| Sun court | Tiled fountain, cushioned divan, brass lantern, low table, rug loom, sun-dial |
| Heartwood lodge | Root throne, carved long table, hanging herb rack, lantern tree, harp, flower trough |
| Cloud spire | Floating lamp, wind harp, feather bed, sky map table, open-air bath, kite rack |
| Geode sanctum | Crystal lamp, rune-carved desk, crystal throne, chime tree, star globe, reading alcove |
| Rime lodge | Fur bed, antler chandelier, carved bench, ice-clear window seat, wolf-pelt rug (its rug), hot spring bath |
| Stilt house | Cauldron hearth, bone chimes, moss bed, lantern-toad lamp, carved stool, reliquary shelf |
| Coral keep | Shell basin, net hammock, pearl lamp, ship's-wheel table, aquarium, coral throne |
| Mourning chapel | Candle rack, carved pew, lectern, ossuary shelf, bell, velvet chaise |
| Mycelium den | Glowcap lamp, spore-silk hammock, stump table, puffball seats, moss bath, lantern-grub jar |
| Hollow bastion | Null throne, floating rubble table, rift lamp, black mirror, broken crown stand, silence bell |

Rule (GAP-COOKED): a set piece counts in "biome set" (2), so the set's 6 pieces count once together; a set's bed / bath /
lamp do not also count in bed / bath / light (one group each: no double count). Rugs count in floor (2). Lamps among
them give light (kind light) and lamps that burn glow at night. Rime lodge's rug is the wolf-pelt rug; every other set
gets "<set> rug" woven in its colours. Blueprint `bp_<set>_decor` (one per set) teaches all 7; it drops from the land's
major boss (Vulkarn, Kharvox, Ahrumet, Sylvaroth, Aeolith, Lyrathis, Skaldrith, Vharra, Thalassor, Nhal, Mycelorn,
Vorhollow) by the built blueprint rule. Cost: 6 of its biome block + its signature resource 2 (+ hide / linen for soft
pieces).

### 2.5 Comfort decor (survival-crafting 2.2 groups)
Bench (seat 1, wood 8), carved throne (seat 2, heartwood 20 -> Hearth 3: wood 30 + bronze 2 where heartwood isn't live:
GAP-COOKED as "wood 24, hide 4, bronze ingot 2"), long table (table 2, wood 16), fur bed (bed 2, wood 20, wool 6, hide 6),
tapestry (wall 2, linen 8, dye 2 -> no dye item: linen 8 + fiber 4), fireplace (fire 2, stone 40, Hearth 2; a real fire
light), candles (light 1, tallow 2 + wood 2), standing lantern (light 1, iron 1 + tallow 1), crystal lamp (light 2,
resonant crystal 4; crystal land), sunstone lamp (light 2, sunstone 4; desert), potted plant (green 1, clay 4 -> stone 4
+ any seed: uses fiber 2), flowering pot (green 2, stone 4 + a rare crop's seed: roseroot seeds 1), wooden tub (bath 2,
wood 20, copper 2), hot spring bath (bath 3, stone 30, on a vent), hand drum (music 1, hide 2, wood 4), lyre (music 1,
wood 4, sinew/fiber 4), mirror (mirror 1, wood 20, stone 5, copper ingot 2). The Hearth offering: E on your Hearth with
a cooked meal in your bag in its first game day leaves the meal on it (+2 comfort to every bed in its claim, for good;
saved). After the first day it says "the Hearth has had its first meal" or "too late".

### 2.6 Turrets (SPEC numbers; mechanism GAP-COOKED)
**Bolt turret** (Hearth 4, 1,500 HP, 6 P). A stone plinth 1.6 m square; a bronze turntable ring on it driven by a worm
off the drive shaft port at the back (the shaft from your network joins there, 60 cm up); on the turntable, two oak
cheeks carry a steel bow 1.6 m wide on a trunnion (it pitches), a windlass drum with a crank wheel at the back that the
power turns, cords to the claw, a bolt box over the groove with two slots (bolts drop in one at a time). Behaviour:
- Looks for a target every 0.2 s within 30 m with a clear line from the muzzle: hostile wild creatures (not tames, not
  your clan's); on a PvP server any player not of the owner's clan inside the claim the turret stands in.
- Turns at 120 deg/s (pitch 60 deg/s, -20..+45) to the target's lead point; once on it (3 deg) and spanned, it tells:
  a thin red aim line from the muzzle 30 m along the shot for 0.9 s with a ratchet click (`turret.tell`); it tracks at a
  quarter speed for the first 0.6 s, then holds still for the last 0.3 s; then looses: a straight bolt at 4,000 cm/s,
  no gravity, never homing, 45 damage (+20% Turret tech of its builder) through ApplyHit, a thrum (`turret.loose`).
- Spanning takes 1.5 s at full power; short of power it takes 1.5 / speed; no power or no bolts: it can't fire (state
  NoPower / "no bolts"). Draws 6 P while switched on with bolts in it.
**Element turret** (Hearth 5, 2,000 HP, 8 P): the same turntable and plinth, a brass siphon nozzle over a pressure
vessel the windlass pumps; a path material burns one per 50 shots (the slot shows how many shots are left).
Ember essence: a 6 m, 30 deg cone, 30 fire damage + 20 burn buildup to all in it after the tell (red cone of
light on the ground for the 0.9 s, `turret.flame`). Frost essence: a slowing frost bolt (20 damage, 40 frost buildup,
straight, 3,000 cm/s). The other paths' materials (their lands' essences) are rows with their status (shock, soak,
blind, decay, mark, curse, bleed, poison...) and wait for their items. 20 m. Cost: steel plate 10, ember essence 2,
frost essence 2, basalt block 3, rime block 3 (resolved: the spec's "the path material 4, biome block 6" split across
the two live paths and lands).
Turrets never move off their plinth, never chase, never target someone for standing next to them: no traps, no
guards. Turret tech (Engineering 50) +20% damage.

### 2.7 Rules
No underground bases: structure, beams, doors, beds, chests, stations, machines but the drill, turrets, the totem and
the Hearth can't be placed in a cave ("no building underground"); lights, torches and drills can. No traps: no piece
deals contact damage (rubble from a collapse is the building falling, not a trap).

## 3. Plan (build steps)
1. Data: `Data/building_sets.json` (tiers, shapes, freeform, decor, materialDamage); items (12 blocks, 12 blueprints,
   comfort materials), recipes (12 blocks), gear.json blueprints (12, `pieces`), pieces.json (comfort decor, turrets),
   machines.json (turret rows + turret table), lifeskills.json (decorator, turret_tech, master_builder, ancient_wood),
   tuning (building.*, turrets.*, comfort.decoratorCap), sounds/fx rows.
2. `WRBuildSets.h/.cpp`: FWRBuildSets (load, expand into FWRPieceDef rows inside FWRBuildRegistry, tier info, palette,
   decor archetypes, place checks, Hearth offering).
3. FWRPieceDef: `Set`, `Blueprint`, `Archetype`, `Variant`, `Tinted` look keys, `Free` (beam) length; IsStructural().
4. Look: `EWRLookMat::Tinted` + colour fields; `M_BuildTinted`, `M_BuildOverlay` in `Tools/make_build_materials.py`
   (run headless once); `WRBuildLookSets.cpp` (13 tier styles x 12 shapes + beams), `WRBuildLookDecor.cpp` (archetypes).
5. Integrity: `WRBuildIntegrity.cpp` (LinkCost, beams, brace bonus, colours, rubble).
6. Placement: beams in UpdatePlacement / place command (pitch), save pitch.
7. Turrets: `WRTurret.h/.cpp` (subsystem, targeting, firing), machine hooks (Demand, Takes, state words), model in
   WRMachineLook with yaw/pitch motions, aim line.
8. UI: build bar tabs (Beams), material-major order + page tag, locked blueprint cards, descriptions, claim upkeep.
9. Comfort: Decorator, offering, biome set group.
10. Compile; spot-check against this doc; CHANGELOG + HANDOFF; commit.

## 4. Build notes (what changed from the plan while building, and why)
- **A post's reach**: "a beam or post adds 1 span per 2 m of length" is applied to the member's horizontal run, so a post
  standing straight up bears like Raven's pillar (0) and a 4 m flat beam costs 2. Otherwise a post would be weaker than
  the pillar it stands beside.
- **Bolt box**: the spec's "50-slot box" is read as the box's capacity; the box is 2 slots of crossbow bolts (80).
- **Element turret cost**: "the path material 4, biome block 6" split over the two live paths and lands (ember 2, frost
  2, basalt block 3, rime block 3) so it can be built on the island.
- **No underground bases**: lights, coil lamps, greenhouse lights, dark crops and the drill stay allowed in caves (the
  farming and machinery cooks depend on them); walls, floors, beds, chests, stations, turrets, the totem and Hearth don't.
- **Weapon damage on pieces** (2.18's wood 10%, stone 2%, metal / biome 0%) is applied now through `materialDamage`
  so a metal wall can't be cut down with a sword; the raid lane keeps the charges and rams.
- **Turret shots** carry the turret as their instigator and hurt only what that turret may target
  (`UWRTurretSubsystem::MayHit` in the projectile), so a bolt never hits its builder, their clan or their tames.

## 5. Proof (requirement -> where it is)
| # | Where |
|---|---|
| R1 | Data/building_sets.json:29 (metal tier), WRBuildSets.cpp:132 (Expand: shapes, HP, cost, span, upkeep) |
| R2 | WRBuildLookSets.cpp:131 (panels fill), CornerAt / Cap / Surface / Roof / Stairs / Leaf / Beam `panels` branches |
| R3 | Data/building_sets.json:32-67 (12 tiers), WRBuildSets.cpp:132 |
| R4 | Data/items.json (12 blocks), Data/recipes.json:109-120 (workbench L5, biome-gated) |
| R5 | WRBuildLookSets.cpp:131-470 (13 styles' fills), 1318 (TierBlocks dispatch) |
| R6 | Data/building_sets.json:69 (decor), WRBuildLookDecor.cpp:90 (55 archetypes) |
| R7 | Data/gear.json:308 (bp_*_decor with boss), WRGear.cpp:395 (learn text; portal pool excludes them), WRBuildSets.cpp:265 (place check), WRUIBuild.cpp:904 (card says who drops it) |
| R8 | WRBuildSets.cpp Expand sets LiveWith = biome; FWRFarm::IsPieceLive hides them |
| R9 | WRUIBuild.cpp:784 (page tag names the material), InTab material-major sort |
| R10 | WRPlayerBuild.cpp:469 (beam placement), 811 (pitch on the place command), WRBuilding.cpp:1247 / WRGameMode.cpp:378 (saved) |
| R11 | WRBuildIntegrity.cpp:101 (LinkCost), 145 (Allowed: brace +1), WRBuilding.cpp SupportAt / RecomputeSupport |
| R12 | WRPlayerBuild.cpp:695 (UpdateBuildTint), 687 (ghost colour), WRBuildIntegrity.cpp:180 (colours) |
| R13 | WRBuildIntegrity.cpp:188 (Collapse), 330 (AWRRubble::Tick, 50 dmg via ApplyHit) |
| R14 | WRSurvival.cpp:872 (FWRComfort::At, built by build-survival; now + offering + Decorator) |
| R15 | Data/pieces.json:94-110 (17 comfort pieces), WRBuildLookDecor.cpp archetypes |
| R16 | WRBuildSets.cpp:281 (vent rule) |
| R17 | WRBuildSets.cpp:319 (TryOffer), WRBuilding.cpp Interact (hearth), OfferingComfort |
| R18 | Data/lifeskills.json:79 (Decorator live), WRSurvival.cpp Cap / At |
| R19 | WRUIBuild.cpp Describe (comfort line, BUILT) + set note |
| R20 | Data/pieces.json:86 (bolt turret), Data/machines.json:62 (turret rules + ammo), WRTurret.cpp:225 (Tick), 335 (Fire) |
| R21 | Data/machines.json ammo rows (fire cone, frost bolt, 10 waiting), WRTurret.cpp Fire cone branch |
| R22 | WRTurret.cpp (no pawn, no movement; needs power + ammo: WRMachine.cpp:548) |
| R23 | Data/lifeskills.json:85 (turret_tech live), WRTurret.cpp Fire (TechOf) |
| R24 | WRMachineLook.cpp:1047 (Turret model: worm, ring, spindle, cross shaft, chain, windlass, bow / vessel), 1506 (TickTurret: head, aim line, sounds) |
| R25 | WRTurret.cpp:94 (Eligible), WRProjectile.cpp:271 (MayHit) |
| R26 | WRBuildSets.cpp:277 |
| R27 | WRPlayerBuild.cpp:870 (placeHighTier 12 / place 8) |
| R28 | WRBuilding.cpp UpkeepPerHour (by item), 1578 (Deposit takes every upkeep item), WRUIClaim.cpp:323 |
| R29 | Data/building_sets.json materialDamage, WRAbilities.cpp:987 |
| R30 | WRUIBuild / WRUIClaim / WRUIMachine go through the shared pad processor (no new screens) |
| R31 | building_sets.json, machines.json turrets, tuning building.* / comfort.* / lifeSkills.xp |
| R32 | 2.2 / 2.3 / 2.6 above; code comments at each model |
| R33 | Data/lifeskills.json master_builder / ancient_wood live; WRPlayerBuild.cpp:138, WRWorldDressing.cpp:921 |
| R34 | Content/Building/M_BuildTinted.uasset 39 KB + M_BuildOverlay.uasset 8 KB |
