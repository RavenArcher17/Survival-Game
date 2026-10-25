# Changelog

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
- `Tools\survivaltest.ps1` (22 checks): a fresh fire survivor on the island gathers bare-handed, crafts the crude
  tools, gathers more with them, builds a hut (foundation, three walls, a door, a roof) and a workbench out of what it
  gathered, opens the door and walks in, crafts the stone tools at the bench and gathers more again. `-Shots` adds
  screenshots (a tree before and after felling, the hut from three sides and through the door). It backs up the
  island's world save and puts it back.
- Known: bots swing with the Flame slash weapon art (bot commands can't press LMB; it's the same melee sweep); the chop
  clip only plays at the old resource nodes, not at island trees; with a workbench inside a small hut, E at the door
  can open the bench menu instead of the door. Tools don't wear out yet.

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
