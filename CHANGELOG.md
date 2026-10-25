# Changelog

## 2026-10-25: test pass

- The older bot tests now check what they test and end like the rest (a PASS/FAIL line per check, "N checks, M
  failed", exit 1 on a failure, no errors in the logs): `bosstest` (16 checks), `buildtest` (16), `mptest` (9),
  `persisttest` (7), `socialtest` (14), `spelltest` (19), `terrain/walktest` (5) and `terrain/hazardtest` (6). Each has
  its own fresh world save (`-wrworld=<test>`) and stops once its bots are done. On the way: buildtest's crop plot was
  placed out of reach and never built, and spelltest learned a branch node before its trunk.

## 2026-10-25: creatures that hunt, flee, sleep and fight differently

- **Attack variety** (`attacks` in `creatures.json`, numbers in `tuning.json` `creatureAI`), each telegraphed: a
  **lunge** that leaps where its target will be (Cinderfang, Gloomhowl, Rimeclaw, Magmaw), a **charge** down a marked
  strip that knocks you back (Emberhorn, Frostantler), a **stomp** that hits everything in a marked ring (Basaltback,
  Woolly drudge), and **spit**, a glob of fire or frost from range (Magmaw spits first then leaps, Scorchwing,
  Glacimaw). Bites are unchanged; tames use their specials too. A rooted or frozen creature can't leap or charge.
- **Food chain**: carnivores get hungry every few minutes and hunt the nearest wild non-carnivore they can take (up
  to 1.3x their size, pack hunters 2x), run it down (pursuit speed, no flanking on running prey, a longer leash on a
  hunt), and the pack shares the kill and eats. Prey that's hunted bolts in bursts (fast ones like the Ashstrider can
  get away); neutral prey fights back. Basaltbacks and Woolly drudges are too big to hunt. Wild creatures otherwise
  still never fight each other.
- **Morale**: a creature below 25% health runs from what's hurting it, or below 50% when a packmate just died;
  passive animals bolt when hit. Bosses and their minions never run.
- **Day and night** (`activity`): day animals sleep at night and night hunters (Gloomhowl, Glowmoth, Aurora fox) by
  day. Asleep, a creature lies still and notices you at a third of the distance ("(asleep)" on its label), and a hit
  wakes it; night hunters notice you from 40% further at night.
- Dev and test commands: `WRSpawnAt <creature> X Y`, `WRCreatureHP <creature> <fraction>`; `Tools/aitest.ps1
  [-Shots]` stages a Cinderfang pack hunt (Pumicehog killed and shared, Ashstrider ignored once they're fed), the
  Gloomhowls asleep by day and hunting at night, a hurt Rimeclaw running, and a charge, stomp, spit and lunges. The
  ranged test now roots its target as it spawns (fresh Cinderfangs lunge at you).

## 2026-10-25: day and night

The world clock (`AWRGameState`, `clock` in tuning.json) now shows on every client, on the island and the arena.

- **Sun and moon**: `AWRSky` (spawned on every machine that renders) turns the map's sun with the hour: it rises in the
  east at dawn (6:00), crosses the south at 65 degrees and sets in the west at dusk (20:00), and the atmosphere reddens
  it on the way down. A moon rides opposite it as a second atmosphere light: a disc in the sky, moonlit clouds and fog.
- **Darker but playable nights**: the sky light is lifted at night so shapes still read, the fog goes deep blue, and
  auto-exposure may only brighten so far (otherwise moonlight looks like day). Numbers in tuning.json `sky`.
- **Lights at night**: the Hearth and the forge have a fire's warm glow (torches and braziers already had theirs).
- **HUD clock**: "Day 3   21:40" top right, pale blue at night.
- **The time carries over a restart**: the world save keeps the clock (`clockHours`); a new world starts at
  `clock.startHour`.
- Building: an **upper wall** (2 m, wood or stone) closes the gap a one-cell sloped roof leaves above its high side.
- `Tools\skytest.ps1` (10 checks): a bot builds a hut with a torch, a brazier, a claim and a Hearth, jumps the clock to
  noon, sunset and midnight and reads the client's sky (`WRSky`) at each, checks the clock runs at clock.dayMinutes and
  that a restart keeps the time. `-Shots` takes the hut and the moon at each time of day; `-Arena` runs it on the arena.

## 2026-10-25: ARK-style taming

- **Knock it out, then feed it.** E on a knocked-out creature opens the **Taming menu**: give it all of a food at once
  (kibble or what it eats) and it eats from that stash on its own, one item every 30 s; taming moves while a meal is
  digesting and stalls when the stash runs dry. The taming times (60 / 300 / 900 s by difficulty) are seconds of eating
  plain food. A creature that **wakes up loses its progress** (the food it hasn't eaten stays in its stash).
- **Kibble** (workbench: cooked meat, 3 berries, an egg, makes 2): every tameable creature eats it, it counts double and
  costs no effectiveness. Eggs come from tamed Snowpuffs, so kibble is a second-tame thing, like ARK.
- **Narcotics keep it under**: each one adds 30% torpor, without the hit that a dart or arrow is. Knocked-out creatures
  drain torpor slower now (250 s from full), so a medium tame fits in one knockout on kibble and a hard one needs
  narcotics.
- **Taming effectiveness** starts at 100%, drops 6% per hit while it's out (tranq ammo too: give narcotics instead) and
  1% per plain-food meal. The tame gets **+health and +bite of up to 50%** from it (`taming.bonusAtFull`); eggs and
  hand-fed calm creatures come out at the full bonus. The bonus is saved with the tame.
- A **taming panel** on the HUD for the nearest knocked-out creature: torpor and when it wakes (from the server's own
  drain, red under 30 s), progress, effectiveness, stash, and "tamed in ~N s", "needs N more raw meat", "it wakes first:
  give it narcotics" or "hungry". `WRTameStat` shows effectiveness, stash, bonus and max health.
- Fixed: your own tames attacked a creature you'd knocked out (they took it as your combat target) and could kill it; tames
  leave knocked-out creatures alone now, and a tame told to stay only fights within 15 m of its post (it crossed the map).
- Fixed: a quick shot could fly the wrong way: the server checked the aim point against its copy of your view, which can
  be a frame behind a flick. The shot now carries the view direction, and the crosshair trace sees creatures (low ones
  sent the shot at the ground behind them).
- Every taming number is in `tuning.json` (`taming`), and the Taming menu / HUD lines are `WRTameGive <food>` and
  `WRNarcotic` for bots. `Tools/tametest.ps1` knocks a Cinderfang out with two tranq arrows, costs it effectiveness with
  a stone arrow, keeps it under with a narcotic and tames it on kibble (+47%), lets a part-tamed Rimeclaw wake up (back to
  0%), and checks the tames and their bonus survive a restart; `-Shots` takes the panel and menu screenshots.

## 2026-10-25: building a house

A two-room, two-storey house with stairs and a gable roof now goes up the way a player expects, and
`Tools\housetest.ps1` builds one. Before, only a one-room box worked: pieces could be stacked into the same spot, the
ghost showed green for things the server then refused, outer corners had holes, the "roof" was a second flat floor,
the stairs a single plank, a wooden building past two storeys collapsed, and nothing could be removed in game.

- **Ceilings sit on walls, a second floor you walk on**: the build ghost picks the storey you're looking into (a floor's
  surface counts as its storey, so aiming at the upper part of a wall no longer jumps to the next one); floors / ceilings,
  roofs and gables go on top of it. Walls stacked straight on walls, pillars or foundations cost no span (they carry
  the weight down), so a wooden building can go up storey after storey; reaching sideways still uses wood's 4 / stone's 6.
- **One piece per spot**: a door, doorway, window, wall or gable can't go on an edge that already has one, a floor or
  roof not into a cell that has one, and stairs keep the opening above them clear. The ghost turns red with the reason
  (no support, something is already there, the stairs need this opening) before you click: support values replicate,
  so the client checks the same rules as the server.
- **New pieces**: a **doorway** without a door (between rooms), a **sloped roof** (rises 2 m across a cell, with eaves;
  R turns which way, two facing ones meet at a ridge) and a **gable** (the triangle that closes a roof's side; R flips
  it). **Stairs** are eight treads between stringers with a hidden ramp you walk up, ending level with the floor above.
  Walls, windows and doorways run 10 cm past each end so corners close.
- **Planks and stone blocks**: built pieces wear world-aligned materials (`Tools/make_build_materials.py`,
  `/Game/Building`): 20 cm boards with staggered joints and grain for wood, 45 x 90 cm blocks in running bond for stone.
  The ghost keeps its green / red tint.
- **Demolish mode**: the top of the build menu; aim at a piece of yours (it turns red), LMB removes it for half its
  cost. `WRDemolish x y z` does the same from the console.
- **Build menu** in groups with headings (wood building, stone building, crafting/storage/beds, land claim, defense,
  decor); names say what pieces are for ("Wood floor / ceiling", "Wood stairs (one storey)").
- `Tools\housetest.ps1` (25 checks): foundations, a wall, a ceiling and an upstairs wall go through the ghost exactly as a
  player aims them (and land where expected); the expected refusals happen; the bot demolishes a pillar, climbs the
  stairs to the first floor, and walks in through the front door and the inner doorway; a server restart loads all 34
  pieces back. `-Shots` adds screenshots. The survival test's hut now gets a sloped roof and two gables.
  Bots: `WRGhost <piece>` + `WRLook` + `WRGhostPlace` place through the ghost. `Wildreach.Building.Rules` covers the
  snapping, slots, support and storey rules.

## 2026-10-25: ranged weapons are crafted

- **By hand** (B menu): throwing spear (4 wood, 2 stone, 2 fiber), bola (6 fiber, 3 stone), 3 stone arrows
  (wood, stone, fiber), tranq arrow (stone arrow + narcotic), tranq bolt (crossbow bolt + narcotic).
- **Workbench**: hunting bow (10 wood, 12 fiber, 2 hide), blowgun (6 wood, 4 fiber, 1 hide).
  **Forge**: 6 crossbow bolts (copper ingot, 2 wood); the crossbow at Hearth tier 2 (3 bronze ingots, 8 wood, 6 fiber).
  **Alchemy table**: the new **narcotic** (3 berries steeped with a raw meat, makes 2), and torpor darts now take
  narcotic + fiber (was 2 berries, 2 fiber), so every knockout goes through it.
- Bows, crossbows and blowguns roll **quality** like swords: quality x roll scales the weapon's own damage and torpor
  (the ammo's share is fixed), each shot wears it by 1 (`ranged.wearPerShot`), and worn out it hits at half.
  It's repaired where it's made: the station menu shows "Repair Hunting bow (x/250)" at the workbench, and the forge
  says where to go. Spears and bolas stack, so they're crafted in stacks like ammo, not one rolled item each.
- `tuning.json` has a `ranged` section with every ranged number the code was defaulting (aiming camera, spread, throw
  release, projectile size and lifetime, bola limits, wear).
- The rules test still checks the crude tools need only wood and stone; every other hand recipe now has to take
  things gathered bare-handed (wood, stone, fiber, berries) or made by another recipe, and every ranged weapon and
  ammo has to have a recipe. `Tools/crafttest.ps1` crafts all of them at the arena's stations, then wears a bow by
  shooting and repairs it at the workbench (the forge refuses).

## 2026-10-25: ranged weapons

- **A ranged slot** next to the sword and shield: a hunting bow (stone and tranq arrows), a crossbow (bolts and tranq
  bolts), a blowgun (torpor darts), throwing spears (stack of 5) and bolas (stack of 10). **G** swaps between the
  sword and shield and the ranged weapon, **right mouse** aims (the camera pulls in over the shoulder, the view
  narrows, you walk at half speed and face where you look), **left mouse** fires or throws at the crosshair, and the
  **mouse wheel** picks the ammo. The HUD shows the weapon and what it's loaded with; firing from the hip spreads 4°.
  A bite that staggers you also stops you shooting.
- Shots are real projectiles with drop. Arrows, bolts, darts and spears **stick** where they land, tip first (and ride
  along on a creature they hit); a thrown spear is left on the ground to pick up again. A **bola** roots small
  creatures (new `Rooted` status, no movement) and slows big ones. Tranq ammo adds **torpor** on the hit, the start of
  taming: two tranq arrows or two torpor darts knock a Cinderfang out.
- Art: AI-made bow, crossbow, blowgun, spear, bola and arrow (`batch_ranged.json`; `normalize_gear.py` bow / pole /
  ball modes). Pole mode now turns an item onto its principal axes first: image-to-3D left the blowgun, spear and
  arrow lying on a slant, so they were held and flew crooked. 13 new Mixamo clips on both bodies: bow idle / walk /
  run / draw / aim / aim-walk / release / equip / put away, rifle aim / fire / reload (crossbow and blowgun), and a
  throw. Each shot plays its fire clip and then its reload.
- Holding them: `ranged.socket / along / face` in `items.json`, worked out from the skeleton. `"pose"` measures those
  directions in a body clip instead of the reference pose (arms hanging), and `"along": "hands"` runs the weapon level
  from the trigger hand toward the other hand: the crossbow and blowgun sit at the cheek in the rifle-aim pose, and the
  spear is carried level, blade forward. Mixamo's rifle aim holds the gun ~25° left of where the body faces, so the
  body turns that much while aiming one.
- Adding clips to a survivor without touching its mesh: `Tools/import_anims.py` imports animations only onto the
  existing skeleton (Interchange, through a pipeline stack override: a bare pipeline in the import task is ignored
  and the default one imports a new mesh and skeleton). `survivor_build.py --rest-from` builds the clip FBX on the
  survivor's own rest pose and strips tracks for bones the survivor lacks (the FBX exporter drops any clip that has one).
- The crosshair and `WRAimAt` use the survivor's own camera, so the art camera can watch a bot shoot.
- Bot and test commands: `WRRanged 0|1`, `WRAim 0|1`, `WRFire`, `WRAmmo 1|-1`, `WRAimAt <creature>` (holds over for
  drop), `WRHold <seconds>` (roots the wild creatures within 30 m), `WRClearCreatures` (removes them and the training
  dummies). `Tools/rangedtest.ps1 [-Body f]` fires every weapon at a fresh, rooted Cinderfang on the arena with
  screenshots: 9/10 hits on the male body (the spear fell short), 10/10 on the female, no errors.

## 2026-10-25: the survival loop works from a fresh spawn

A playthrough as a fresh bot on `L_Island` showed the basic loop didn't hold together: the island's trees, rocks and
shrubs were decoration (swinging at them gave nothing; the only gatherables were ~190 placeholder cones over 16 km²),
there were no tools, nothing could be made without a station, and the workbench only cooked meat and made waterskins.
Now a fresh survivor can do it all, and `Tools\survivaltest.ps1` plays it end to end.

- **Gather from the island itself**: a melee swing chops the tree, breaks the rock or picks the shrub in front of you
  (trees -> wood, rocks -> stone, shrubs -> fiber and sometimes berries). Each takes a number of hits, then vanishes
  for everyone and grows back later (`gathering` in `tuning.json`: per hit, hits, minutes). The used-up ones replicate
  through a small `AWRHarvestLedger`; the decoration itself stays local. Every gather shows a notice ("+12 Wood
  (Crude hatchet)").
- **Tools**: a crude hatchet and a crude pick, made **by hand** from wood and stone (they're at the top of the build
  menu, B), and a stone hatchet and stone pickaxe at the **workbench** (wood, stone, fiber). Tools work from your bag:
  the best hatchet you carry chops, the best pick breaks stone, ore and sulfur. Bare hands 1x, crude 1.5x, stone 2x
  (`"tool": {"kind", "yield"}` in `items.json`). Recipes with station `hand` are made anywhere.
- **Foundations sit into the ground** (`building.foundationSink`, 60 cm) so the floor is a walkable step instead of a
  waist-high ledge, and the slab runs deep, so slopes don't show a gap under it. A new foundation next to one you built
  levels with it; otherwise it sits on the ground at its cell's centre, wherever you aim.
- **Placing a workbench, forge, Hearth or totem shows a preview**: the ghost was invisible (those are child actors only
  the server spawns).
- **Doors swing on their hinge** instead of turning in the middle of the doorway.
- Sulfur now counts as mining (pick, mining XP), as the chop clip already assumed.
- **E uses the nearer** of a door / chest / bed / crop / claim and a station, which is what the prompt already showed:
  a workbench inside a small hut no longer opens its menu when you go to close the door, and the server's interact
  picks the door rather than the wall beside it. The build menu is titled "Build & craft".
- The **chop clip** plays at island trees and rocks too, and the gather crouch at shrubs.
- `WRGather tree|rock|shrub|node|any|off`: the test bot walks to the nearest one and gathers with light attacks.
- `Tools\survivaltest.ps1` (23 checks): a fresh fire survivor on the island gathers bare-handed, crafts the crude
  tools, gathers more with them, builds a hut (foundation, three walls, a door, a roof) and a workbench out of what it
  gathered, opens the door and walks in, crafts the stone tools at the bench, closes the door from inside and gathers
  more again. `-Shots` adds screenshots (a tree before and after felling, the hut from three sides and through the
  door, a chop). It backs up the island's world save and puts it back.
- Known: tools don't wear out yet (durability is only saved for equipment).

## 2026-10-25: the humanoid bosses have bodies

- **Vulkarn, Grimjaw and the Obsidian Warden** are AI-made, rigged on Mixamo and animated with its Creature Pack
  (breathing idle, walking, run, swipe / punch / jump-attack attacks, roaring, dying). Creatures with several attack
  clips (`attack`, `attack2`, `attack3` in `creatures.json`) take them in turn, and every creature attack clip is now
  fitted to the swing's length, so the hit lands with the telegraph. Bosses **roar** when they enter a new phase
  (`roar` clip; `Phase` replicates with a notify).
- **Ignessa** is a winged fire fae: `rig_special.py --body fae` gives her a body, a head and two hinged wings that fan
  while she floats; her fly / hover clips map to Run / Idle.
- Every creature now has art except the training dummy. The **wisp** (Ignessa's summons) is a glowing orb: creatures with a `glow` value and no mesh get an emissive sphere in their colour (`/Game/Creatures/M_Glow`, made by `Tools/make_glow_material.py`) and a point light that bright.
- Image-to-3D brutes often fuse a hand to a thigh: `survivor_build.py --cut-stretch 4` drops faces that stretch past
  4x their length during any clip (only the clips count; the T-pose bind itself is ignored) and the scraps left over,
  then closes each hole it leaves with a patch textured from the faces around it.
- Fixed: the shield could lie flat like a tray. The skeleton's reference pose in Unreal isn't always the T-pose (it
  can be a clip's first frame, arms hanging), so the shield's face now comes from the arm itself: away from it,
  between up and out from the body.
- Mixamo without Raven at the keyboard: the decimated copy goes into the page as text; wide bodies get a tiny
  framing triangle so the marker view shows them whole (`Tools/art/README.md`).
- Fixed: packaging crashed cooking the Barghest pack's mane and tail clips (`Assertion failed: !Hash.IsZero()`: they
  have no skeleton in the pack). The game never used them; they and their meshes moved out of `Content` (kept in
  `C:\Dev\WildreachArt\replaced\barghest_parts`). The pack is git-ignored, so re-adding it from Fab brings them back.

## 2026-10-24: a second body, a real sword and shield

- **Female survivor**: the second AI survivor is rigged on Mixamo with the same 55 animations
  (`/Game/Characters/SurvivorF`). The main menu has a **Male / Female** choice, sent as `?Body=f` on the join URL,
  kept on the PlayerState and replicated with each pawn, so everyone sees the body you picked. Bodies are variants
  under `"bodies"` in `Data/player_art.json`; each overrides any top-level field.
- **Real sword and shield**: an AI-made one-handed sword in the right hand and a round iron-rimmed shield strapped to
  the left forearm, replacing the cube blade. `Tools/art/normalize_gear.py` puts the grip at the origin and trims
  stray struts off the shield's back. The game works out the blade and shield-face directions from the mesh bounds and
  the T-pose skeleton (`weaponMesh`, `shieldMesh`, `shieldSocket`). First person hides the shield; the sword stays.
- **Gathering clips play**: swinging at a resource node right in front does an overhead chop for wood, stone and ore,
  or a crouching gather for plants; looting a bag, taking an egg or feeding a wild creature plays a pick-up for
  everyone.
- **Jump fixed**: the jump clip no longer adds its own hop on top of the real jump, and the fall starts airborne.
- `Tools/art/survivor_build.py --cut-stretch 2.5` removes faces that tear when animated (image-to-3D bodies sometimes
  fuse a hand to a thigh), measured against the model's own pose, plus the scraps it leaves.
- Fixed: `Tools/import_prop.py` re-parented every prop material under `/Game/Props` (the caves' tinted rock
  included) to the plain prop material. It now fixes only what it imports.
- `Tools/playershots.ps1 -Body f` frames the female body.

## 2026-10-24: the player has a body

- **Custom survivor** (Raven's pick over the UE mannequin): an AI-made male survivor (T-pose concept, so Mixamo can rig
  it), rigged on Mixamo and animated with the Sword And Shield pack (49 moves) plus a dodge roll, an overhead chop,
  gathering, picking up, swimming and treading water. 55 clips in `/Game/Characters/Survivor`.
- `Data/player_art.json` drives it: mesh, facing, and which clips play for idle / walk / run (play rate matched to the
  real speed), falling, blocking, dodging, swimming, light and heavy swings (fitted to the swing's length), spell casts,
  hit flinches and death. The placeholder blade is now a sword held in the right hand, its grip worked out from the
  skeleton. First person hides the body.
- `Tools/art/survivor_build.py` copies Mixamo's rig from the decimated copy it was made on onto the full textured mesh,
  moves the rest pose to the animations' T-pose and makes the clips play in place; see `Tools/art/README.md` for the
  Mixamo steps (and the page's quirks). `Tools/playershots.ps1` frames the player with `WRArtCam me`.
- Known: the female survivor and the four humanoid bosses are built and Mixamo-ready but not rigged yet (the bosses'
  arms-down poses will need T-pose versions); gathering, chopping and pick-up clips are loaded but not yet triggered by
  the game; the jump clip's own hop adds to the real jump; the sword is still the placeholder cube.

## 2026-10-24: real art, part 2 (every wild creature, terrain polish)

- **Every wild creature now has art** (19 of 25 creatures; the four humanoid bosses and the wisp remain):
  Glowmoth (flat moth that hovers and flaps), Glacimaw (ice worm that slithers and rears to strike) and Snowpuff
  (fluffy flightless bird) from the AI pipeline, with new rigs (`rig_special.py --body moth|worm`, `--legs 2`);
  Gloomhowl (pale Barghest), Scorchwing (ember Mountain Dragon) and Skaldrith (ice Mountain Dragon, the boss) from the
  QuadrapedCreatures pack with tinted materials (`Tools/art/creature_tints.py`, `wire_pack_creatures.mjs`).
- Creatures can override materials per slot (`materials` in `creatures.json`), and flyers play `fly` / `hover`
  animations while airborne.
- **Lava and ice are one continuous surface** each (procedural mesh with shared corners), so slopes are smooth
  instead of stepped tiles.
- **Caves are sculpted in Blender** (`Tools/art/cave_gen.py`, Raven asked for this over the blocky look): a rough rock
  mound with a domed chamber (~24 m across, ~10 m high) and a ragged, recessed entrance tunnel carved out, three
  variants picked by position, tinted rock per biome. Collision is the mesh itself (`Tools/import_cave.py`) and the
  landscape stays the floor. The old block shell (now boulder-covered) is only a fallback.
- **Ridge cliffs**: 4.7k large boulders on the ridge's steep faces (`basalt_cliff` / `ice_cliff`), hiding the
  stretched landscape triangles there. Decoration only, so the ridge is still a wall. Existing placements unchanged.
- **More trees**: charred trees come in four models (two new: a split snag and an ember-lit dead tree) and the
  tundra mixes five (spruce, wind-bent mountain pine, snow-laden fir and a frosted birch are new).
- **Fixed: trees floated on slopes** (Raven spotted it): every tree, rock and shrub now stands on the lowest ground
  under its footprint (`dressing.mjs`), so the uphill side buries instead of the downhill roots hanging in the air.
  Positions are unchanged; trees dropped 0.9 m on average.
- **Fixed: ice sheets stretched into the sky** at the glacier plateau's edge; surface corners now only join cells
  within 7 m of each other (steep river beds stay joined, plateau edges split).
- Packaging: the pack creatures' meshes and animations are in `DirectoriesToAlwaysCook`.
- Checks: `Tools/terrain/tree_shots.ps1` takes eye-level shots among the trees in both biomes.

## 2026-10-24: real art, part 1 (creatures and terrain)

- **13 creatures made with the local AI pipeline** (`Tools/art/README.md`): Pumicehog, the nine four-legged ones
  (Magmaw, Basaltback, Emberhorn, Ashstrider, Rimeclaw, Frostantler, Rimeshell, Aurora fox, Pale Hunter) and the three
  six-legged ones (Cinderfang, Woolly drudge, Kilnmite). Each has Idle/Walk/Run/Attack/Hit/Death.
  The rigger does six legs (tripod gait), adds a middle leg pair to four-legged models (`--add-mid-legs`), and
  lengthens foreshortened torsos (`--stretch`). `Tools/creatureshots.ps1` + `WRArtCam` screenshot a batch in game.
- **Terrain textures** (`Tools/terrain/build_terrain_materials.py`): the landscape material uses the MW Landscape
  Auto Material pack: dark ash with stony patches and black sand in the west, snow and pale sand in the east, basalt /
  frost rock on steep slopes. New `M_Rock` (boulders, cave walls), `M_Lava` (glowing, flowing) and `M_Ice` (glossy).
- **Dressing uses real meshes**: AI-made charred trees (two kinds) and snowy pines (`Tools/import_prop.py` ->
  `/Game/Props`, Nanite, invisible trunk collision as before), SimpleRocks boulders (same footprint and collision as
  before), GV shrubs, and rock-textured cave walls; falls back to the engine shapes when a pack or prop is missing.
- **Packaging**: creature art and dressing meshes are loaded by path, so they're now listed in
  `DirectoriesToAlwaysCook` (a packaged build would otherwise have shipped placeholder shapes).
- **Fixed**: art creatures whose radius exceeds their half-height floated (the capsule grows to the radius); the
  rigger ignored `--head`; re-importing a creature kept the old texture and animations (the importer now refuses).
- Known: lava tiles step visibly on slopes (needs one continuous surface); shrubs stay green in the volcanic west (the
  pack's leaf colour ignores tints); one pine model for the whole tundra; cave shells are still boxes (now rock-textured);
  the sawtooth shadow on the steep ridge face remains.

## 2026-10-24: playable with friends

- **Main menu** (shown when the game runs on its own): name, Fire or Frost, **Host** (your game becomes a listen
  server on UDP 7777) or **Join** by IP; remembered between runs. F10 returns to it. `Tools/package.ps1` makes a
  shareable Windows build with a players' README.
- **Paths menu (P)**: both trees with ranks, costs, branch gates and points to spend; Enter learns.
- **Every tree node works**: spells Fire wall, Lava pool, Meteor (fire) and Frost nova, Ice lance, Ice wall,
  Blizzard (frost); weapon arts Molten edge, Molten cleave (leaves lava), Flurry; passives spreading flames, fuel the
  fire, wildfire, explosive bolt, overheat, searing parry, brittle, cold return, chain shatter, cold riposte, frozen
  moment, whiteout. Numbers in `tuning.json` `spells.*` / `passives.*`.
- **Map list (M)**: bosses, mini-bosses, passes, camps and caves by distance with a compass direction.
  **Chat (T)**: everyone on the server sees it.
- **Island dressing**: 26k placeholder trees, rocks and shrubs and six midland caves (rock tunnel + dark chamber
  with rich ore at 12% gem odds, sulfur gas or slick ice, a Kilnmite or Glacimaw colony), all generated from the
  heightmap into `Data/island_dressing.json` and built on every machine at runtime.
- **Biome gear checks**: the lava river burns without lava resistance (fire main path, fire-infused armor, or a
  mount); slick ice on the tundra rivers and glacier plateau without ice resistance (frost main path, frost-infused
  armor). HUD warning when you're standing in one.
- **Fixed**: clients used their own default server preset, so lock-on and PvP disagreed with the host; the host's
  settings and everyone's clan now replicate. The tame test no longer races a wandering herd animal.
- Tests: `spelltest.ps1`, `terrain/hazardtest.ps1`; `island_shots.ps1` also covers caves, lava and ice.
- Known: hazard tiles are flat placeholder slabs; decorations and caves stop drawing at ~250 m (the World
  Partition loading range; HLODs would fix distant views); a dedicated-server package waits on the source engine build.

## 2026-10-23/24: the rest of the first playable (batches 1–5)

Everything below is placeholder art (engine shapes, tinted) with numbers in `Data/*.json`, all first drafts to tune in playtests. Each batch has a bot test script in `Tools/`.

- **Crafting & equipment** (`crafttest.ps1`): recipes by station (forge, armorer, alchemy table, workbench) from `Data/recipes.json`; quality tiers Crude→Masterwork with stat rolls; gem sockets; fire/frost infusion with path essences; repair and +1..+10 upgrades with shards; the Hearth's boss cores raise station tier. Weapon/armor durability, armor damage reduction.
- **Building** (`buildtest.ps1`): snap grid, foundations/walls/floors/doors/gates/chests/beds/crops/pens/stations from `Data/pieces.json`; support (wood spans 4, stone 6) with cascade collapse; code locks; land claim totems with upkeep and decay; raids damage pieces; the world saves to `Saved/ServerDB/world_<map>.json`.
- **Creatures & taming** (`tametest.ps1`): the 17-creature roster (pack hunters, flyers, burrowers, herds, critters); knockout + feed, offer food, steal an egg (nests with angry guardians) and path magic; torpor darts; tame orders (follow, stay, aggressive, passive, release); rideable mounts; Glowmoth light, ore-mining tames, pen produce; 2-tame limit; tames saved with the world.
- **Bosses** (`bosstest.ps1`): Vulkarn (anvils vs lava floods, a tongs grab you can parry, white-hot phase 2, hidden phase 3), Skaldrith (ice pillars block breath and shatter, flight and dives, hit his wings to ground him, whiteout phase 3), Obsidian Warden (shell; chill cracks it faster), Ignessa (airborne, exploding wisps, lands when they're gone), Grimjaw (regenerates unless burning), the Pale Hunter (invisible in blizzards, footprints, fire reveals). Telegraphed attacks, HP +80% per extra player or tame, ≥10% of the damage earns your own loot (cores, essences, parts, shards, trophies), boss HUD bar, respawn time from the server preset.
- **Life skills, server settings, clans** (`socialtest.ps1`): 15 life skills (levels 1–100, perks at 25/50/75/100; K opens the menu) wired into gathering, crafting, potions, food, crops, pens, building, taming and loot. Server presets `pvp_standard`, `pve`, `casual_coop`, `playtest` (default: 10× XP) and `custom` (`Saved/ServerDB/server.json`), picked with `-wrpreset=`. Clans (`WRClan create/invite/join/leave/kick/info`) share claims, doors, chests and tames, and can't hurt each other. Safe zones at the spawns. PvP and PvP-in-boss-arenas switches, offline base protection, death penalty gentle / Souls-like / drop / full loot with loot bags.
- **Island rebuilt** with the six boss spawners, spawn safe zones and starter camps; walk test still passes.
- Tuning overrides for tests: `-wrtune=key:value,key:value`.
- Known gaps (caves and foliage arrived 2026-10-24): fishing, engineering and swimming have no XP source; perks marked "not active yet" wait for their systems; the bounty and creature-raid settings exist but nothing uses them; boss loot goes straight to inventory (no loot bags).

## 2026-10-23: the island (M3 step 2)

- **Layout approved** (`Docs/terrain/LAYOUT_PROPOSAL.md`): island, hard-wall ridge, two passes, Vulkarn in the crater, Skaldrith on the glacier bowl, one water + one lava river on the volcanic side, middle spawns moved toward the passes, ~10 km² for the first playtest.
- **`L_Island`**: the full 4 × 4 km landscape (4033 × 4033 samples at 1 m, Z range ±1 km) as a World Partition landscape in 256 streaming pieces. It's now the default map for `play.ps1`, the server and the editor.
  - Built by `Tools/terrain/build_island.ps1`: Node exports the heightmap, then the editor (a window opens and closes by itself) imports it through `UWREditorTerrainLibrary`, waits for the landscape's GPU layer update, and saves.
  - Placeholder auto-material: dark ash west, snow east, rock on steep slopes. Sea plane, sky and lighting.
  - From the layout data: 6 spawns (biome-tagged), fire/frost altars at the two boss sites, 6 creature packs by zone, 124 resource nodes (wood/stone/copper everywhere, sulfur west, lichen east).
- **Walk test** (`Tools/terrain/walktest.ps1`), with real character movement: north-west spawn → north pass in 3.6 min, then onward to the tundra (5.9 min total). Middle spawn → north pass in 5.6 min, fording the river. Walking at the ridge mid-island gets stuck, so the wall holds.
- Fixed: rivers were 22 m deep trenches that trapped players; now 6 m deep, fordable, with gentle banks.
- Dev commands: `WRTeleport x y`, `WRLook yaw pitch`, `WRWalkTo x y`. `play.ps1` takes `-Map island|arena|grid -Path fire|frost -Name You`.
- Known: placeholder material only (no textures); rock barely shows on the ridge; a sawtooth shadow artifact on the steep ridge face at distance; no PCG foliage yet.

## 2026-10-22: M1 combat core + M2 paths (autonomous build session)

### Combat (M1)
- **GAS** set up: ability system on the PlayerState for survivors (survives death), on the pawn for creatures. One replicated attribute set: health, stamina, poise, hunger, thirst, Heat, Chill, burn/frost buildup.
- **One damage pipeline** (`UWRCombatLibrary::ApplyHit`, server only) used by every source: block (front 70°, stamina cost 10–25 by hit size, guard break at 0 stamina), parry (first 0.2 s of a block, staggers the attacker), dodge i-frames, poise → stagger → riposte (×3), Heat damage bonus (up to +30%), frostbite (+15% damage taken), Permafrost (+15% vs slowed).
- **Statuses:** Burn (DOT), Frostbite + Slowed, Frozen (frostbite on an already slowed target), Shatter (heavy hit on frozen; spends all your Chill for up to double the burst), Stagger. Replicated as gameplay tags; bodies tint by status.
- **Abilities:** light attack, heavy attack, dodge roll (root motion), block/parry (hold), Fire bolt, Frost shard, Flame slash, Ice slash, creature bite. Stamina costs and timings from the Starting numbers tab.
- **Combos:** Thaw (25/25: fire on a frozen target bursts and thaws it) and Balance (50/50: Heat and Chill feed each other).
- **Netcode:** attack/block slowdowns and sprint are predicted movement flags (no rubber-banding); hits are confirmed by the server.

### Paths (M2)
- Fire and Frost trees generated from the design doc (`Tools/data/extract_trees.mjs` → `Data/paths/*.json`).
- XP curve (point n costs 100 + 10n), 150 total / 100 per path, trunk-first, branch gates by points spent, root passive on the main path only, quick slots (8, auto-filled), character level = points ÷ 3 driving health/stamina (exactly 250/150 at 50).
- Main path from the join URL until there's a creation screen; spawn in its biome. **Boss altars** (E to interact) take/swap the subclass; progress is kept per path.
- **Persistence v1:** JSON per player in `Saved/ServerDB/players/`, autosave every 10 s, on logout and shutdown.

### Creatures & world
- `Data/creatures.json` (dummy, Cinderfang, Gloomhowl, Rimeclaw, Magmaw); stats from the zone tier's starting numbers.
- Training dummy (never dies, resets), pack-hunter AI (flanking, leash, give-up), creature spawner.
- `L_CombatArena` test map (dummies, both packs, both altars, biome-tagged spawns).

### Tools & tests
- `Tools/test.ps1`: 6 automation suites: XP curve, character level, tree shape, buy rules/caps, damage math, and a 12-step deterministic combat scenario in a real world. All pass.
- `Tools/mptest.ps1`: dedicated server + scripted bot clients (PvE/PvP), summarises hits/statuses/abilities/corrections from the logs.
- `Tools/persisttest.ps1`: build a character, restart the server, rejoin, check it's intact.
- `Tools/shots.ps1`: rendered client takes screenshots from both camera views.
- Dev commands (console / `-wrbotcmds`): `WRGivePoints`, `WRAutoSpend`, `WRBuy`, `WRSetMain`, `WRSetSub`, `WRWeapon`, `WRSpawn`, `WRStatus`, `WRHeal`, `WRKillMe`, `WRStarve`, `WRGoToAltar`, `WRInteract`, `WRView`, `WRBotPause`, `WR.ReloadTuning`.

### Stat nodes
- 13 stat nodes now do something (data-driven via `nodeEffects` in tuning): ignite chance, searing heat, long burn, crushing blast, heated blade, bellows, tempered, frostbite, deep freeze, hard break, keen edge (crits), light step, chilling touch. Ability nodes past the trunk (Fire wall, Meteor, Ice lance...) are still to build.

### Inventory (M5 start)
- 30-slot inventory on the PlayerState (server-authoritative, owner-replicated), items from `Data/items.json`, stacking + weight, starting kit (3 minor healing potions, 2 cooked meat, 2 waterskins).
- **Q** drinks a potion (healing only from potions, 6 s belt cooldown), **Z** eats, **X** drinks. HUD belt line shows counts, cooldown and carry weight. Inventory saves with the player.
- Dev command `WRGive <item> <count>`.
- **Gathering:** resource nodes (`AWRResourceNode`: wood, stone, copper ore, sulfur crystal, lichen). Melee swings harvest them (heavy = double), nodes deplete and respawn. Arena has 8 nodes. Test bot mode `-wrbotgather`.

### Terrain (M3 step 1)
- Landmass proposal for sign-off: `Docs/terrain/LAYOUT_PROPOSAL.md` (island, meandering ridge, two verified passes, volcano + glacier boss sites, rivers, spawns, walk times). Nothing imported yet.

### Known gaps
- Placeholder art only (no downloads this session): greybox bodies, no animations.
- Stat/ability nodes beyond the four trunk abilities and the capacity nodes are tracked and buyable but have no gameplay effect yet.
- No lag compensation for melee yet; ~0.3 small movement corrections/s per client in bot tests, likely pawn-on-pawn collisions.
- Save key is the player name (`?WRName=`); real account ids come with EOS/Steam (M7).
