# Changelog

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
- **Cave entrances** are heaped with big boulders, so they read as rock outcrops rather than boxes.
- **Ridge cliffs**: 4.7k large boulders on the ridge's steep faces (`basalt_cliff` / `ice_cliff`), hiding the
  stretched landscape triangles there. Decoration only, so the ridge is still a wall. Existing placements unchanged.
- **More trees**: two more tundra pine models (spruce, wind-bent mountain pine).
- Packaging: the pack creatures' meshes and animations are in `DirectoriesToAlwaysCook`.

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
