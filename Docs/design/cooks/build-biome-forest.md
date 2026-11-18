# Build lane: the Ancient forest (Nature) into the game

Lane **build-biome-forest**, 2026-11-14. Builds `cooks/biome-forest.md` (the design cook, every section) into Raven's
repo `C:\Dev\Wildreach`, with `cooks/world-map.md` (belt B5 east, the Pine Sound and the south-east neck, camps,
village, Rootgrave, weather weights), `cooks/combat.md` (row schema, boss template, bleed/poison), and
`cooks/survival-crafting.md` (Heartguard, briarproof leathers, meadow oats, Iðunn apple, beekeeping, Heartwood lodge,
Grove press: most already in data from earlier lanes, hidden until the land is live). Third land off the Old Island,
built on the second-landmass plumbing the Storm peaks lane made (`cooks/build-biome-lightning.md`).

Order (cook rule): 0 ledger → 1 research for the build → 2 plan (gaps solved, contracts, every step) → 3 build log →
4 spot-check + proof → 5 NOT finished.

**Repo state found at start (logged in LATER-COOKS).** The Swamp lane ended with its work uncommitted and not compiling
(duplicate `AWRSwampFlood`, creature/boss code half written). It is left exactly where it is. This lane snapshots that
state, builds the forest in the same tree, and commits only the forest's own lines (its new files whole, its hunks in
shared files), checked by a full unity compile of HEAD + the forest alone in a clean worktree
(`C:\Dev\WildreachForestVerify`). HEAD itself did not compile in full unity (file-scope `using namespace` in
`WRRaidLook`/`WRStormBosses`/`WRStormWorld` leaking `Char`/`Bronze`/`Bone` into other files of the same blob); fixed
here by renaming those three constants, because the packaging build is full unity.

---

## 0. Requirement ledger

Brief = lane brief; Q = an Aaron quote in the brief; DC = design cook `biome-forest.md` section. Proof column is filled
in section 4.

| # | Requirement | Source |
|---|---|---|
| R1 | Research how the best games solve this exact thing (named games, places, systems, why they work) + real-world reference, written here with the details used | Q 11-13 |
| R2 | Plan every detail before building (layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases); list every build step | Q 11-13 |
| R3 | Build exactly the plan at full detail: no placeholders, no primitives, no "good enough" | Q 11-13 |
| R4 | Spot-check own work against the plan and THE BAR by reading code; fix before push | Q 11-13 |
| R5 | "Already done" only with file:line proof that it shows in the build | Q 11-13 |
| R6 | Design with intention: every object, creature, mechanism has a why (its job in route/puzzle/story), what it connects to, how it meets ground, walls, water, other props, player, enemies, light, sound | Q 11-14 |
| R7 | Machines look engineered (Canopy Lift counterweight, mill race wheel, walkways and rope anchors, bell): parts mesh, ropes lead somewhere, nothing passes through anything; moving parts move smoothly and continuously on a believable mechanism | Q 11-14 |
| R8 | Requirement ledger with proof per row; final message lists every requirement with done + where | Q 11-14 |
| R9 | The land on the map: belt B5 east of the Spine (x −850..+2,500, y +2,350..+4,600), the Spine's cliffs and the Rootgrave saddle on its west edge (sealed until the Graveyard), the Pine Sound and the south-east neck (x +250..+700, y +1,950..+2,350) joining the tundra, Crystal border at y +4,600 | Brief; DC 2.1; world-map 2.2/2.3 |
| R10 | Bands: highland x −850..−400, midland −400..+450, lowland +450..coast | DC 2.1 |
| R11 | Read rules: canopy cover gradient 0% coast → 40% F2/F3 → 75% F4 → 90% F5 with light and fog following; trunks are the skyline; POIs sit in light gaps; ground tells the zone; night lights (foxfire banks, glowworm curtains, Glimmerfinch roost glow, lit windows, the Four Hearts' amber sap) | DC 2.1 |
| R12 | Five sub-zones F1 Meadows and Farmland, F2 Streams and Falls, F3 Grovekin Villages, F4 Old-Growth Groves, F5 Elder Canopy: ground, palette, props, ambient bed each; 60-120 m blends; tundra-neck, Crystal-border and Rootgrave-mouth blends | DC 2.2 |
| R13 | 12 skyline landmarks (Four Hearts, the Eldest, Great Falls, Rotted Treant, Clear-cut + Old Gnarl, Paper Hollow, the Bower, Hermit's Oak, Strangler Arch, Antler Barrow, Bell-Tree, Ringfort), each read from 1 km, each with its night read | DC 2.3 |
| R14 | POIs every ~150-250 m: 28 fixed + 36 hand-made sites (contents and pays as listed) + ~97 kit POIs (12 kit kinds with variants) = 161 surface | Brief; DC 2.4 |
| R15 | Decoration rule: full and alive, never cluttered; tree density follows the canopy gradient; giants only in F4/F5; every giant has a thornbrake ring; fallen giants first, light gaps round POIs; living trees/rocks/shrubs harvestable | Brief; DC 3.5 |
| R16 | The Hollow Root hidden sub-biome: 4 ways in (Eldest's Door behind thornbrake, the Sinkroot squeeze, a Truffler dig, the one-way Heart Root), 14 rooms in order, dark with foxfire 12% + glowworms, its music cue | DC 2.5 |
| R17 | 5 caves C1 Badger Sett, C2 Alder Grotto (behind the falls), C3 Root-Mine (gem pocket), C4 Foxfire Cellar (rhizomorph), C5 Sinkroot (deepest, squeeze to the Hollow Root) with ore and creatures | DC 2.6 |
| R18 | Resources placed and gatherable: heartwood (fallen giants only, axe, 8 a hit, respawn 3 game days, Burlbeast +25%), resin, honey/wax/royal jelly (smoke first), truffles (Truffler only), bootlace rhizomorph (glows a day), foxfire wood, thorns (knife), shed antler, ores by depth, oak/beech/alder wood; no felling within 60 m of a giant or heart-tree ("The roots hold it."); crops meadow oats + Iðunn apple | DC 2.7 |
| R19 | Thornbrakes: 6 bleed/s, run ×0.5, first tick 0.6 s after contact, crackle + scrape + arms-up, passes (any Nature points part the brambles, briarproof leathers, Heartguard 4, briar balm ×0.3, a torch/fire burns a 4 m gap open 5 game hours); creatures ignore | DC 2.8 |
| R20 | Micro-hazards: widowmakers (F4: crack + bark shower + growing shadow 1.0 s, 70 dmg knockdown r 2 m; shoot-down 30 dmg crushes creatures; roar within 6 m drops them), leaf pits (F5: sag ring, hollow drum, 4-9 m falls, some into caves; Truffler snorts), Bramblewasp nests (F3: hum within 15 m, hit or 5 s within 4 m → scout mark + swarm 6-10; smoke 60 s; water clears mark), fords in spate (F2 after Rain: 900 cm/s push; falls lip lethal) | DC 2.8 |
| R21 | Weather: Clear 35 (light shafts, finch flocks), Overcast 20 (F5 near-dark, foxfire by day), Rain 30 signature (plots irrigated, crops +25%, Truffler ×2, burns ×0.5, tracks fade ×2, canopy drips 20 s late, the Alder spates, Mossmaul/Rootcoil hunt by day), Morning mist 15 (05-10 only, 70 m sight, Thornlynx 1.5× drop range, plenty block resin +50%) | DC 2.9 |
| R22 | The Bloom event: 5 min warning (pollen haze from giants, finches ring the Hearts, sap veins brighten, Ring pillar green), growth ×2, nodes ×2, 10% rare seeds, 3 Elder Falls (5 s crack tell, 90 m crushing line, fresh fallen giant with 12 heartwood + 6 resin), Hiveback swarms honey ×2, noon wasp raids on hives, the Wild Hunt at night (10 elk loop, avoids safe zones, second trophy + Huntsman's crown), Sylvaroth lines ×1.5, heartwood ×3, safe zones stay safe | DC 2.10 |
| R23 | 5 shrines (Grove Circle Great Shrine in the Long-hall, Hedge, Falls, Nurse, Canopy) with guards, look, fast travel | DC 2.11 |
| R24 | Path master Rowan Ashby (look, Hermit's Oak home at night, Grove Circle by day, short lines within 6 m) + his 6 quests with rewards | DC 2.11 |
| R25 | 11 creatures: Mossmaul, Thornlynx, Rootcoil, Bramblewasp, Elderelk, Burlbeast, Truffler, Hiveback, Fernsheep, Dappledoe, Glimmerfinch: tier, pack, silhouette, palette, size, taming, drops | Brief; DC 2.12.1 |
| R26 | Real models where Fab/our packs allow; real animation, no gliding; readable silhouettes | Brief; THE BAR |
| R27 | Attack rows for every creature (combat schema), damage only on visible contact, tells read at 15 m, no attacker limit, projectiles never home | Brief; DC 2.12.2 |
| R28 | Creature specials: Mossmaul roar drops widowmakers + Maul hug grab + bull-rush trunk stun; Thornlynx canopy drop from perches with shadow + tree escape; Rootcoil log rest with breathing + constrict ring; Bramblewasp scout mark (water/smoke clear) + no death burst; Elderelk bugle rallies herd; Burlbeast hyperarmor; Hiveback heat ball (on wasps ×3, cling −10% move, roll knocks one off, fire knocks all off); physical resists and lock points | DC 2.12.2 |
| R29 | Path magic tame for Mossmaul (hold Interact 4 s with 10+ Nature points, 40 Growth) | DC 2.12.1 |
| R30 | Utilities: Elderelk mount (1,350 sprint, thin ridden capsule, 2 m leap, passes thornbrakes), Burlbeast hauler (300 carry, +25% heartwood within 15 m, log drag 40 heartwood/day), Truffler (truffles, caches, leaf-pit snorts, Hollow Root dig, seeds), Hiveback hive + heat ball, Glimmerfinch pollinator + whistle sweep | DC 2.12.1 |
| R31 | Day and night spawn tables per sub-zone | DC 2.12.3 |
| R32 | Living-world beats: Mossmaul larders, Thornlynx dusk ambush on Dappledoe, Rootcoils across fords, noon wasp raids with heat-balls, Glimmerfinch flocks over kills/ripe patches, Hornwarden herd routine, Rootgrave dusk fights (dormant until Gravehounds) | DC 2.12.3 |
| R33 | Mini-boss Old Gnarl: Clear-cut arena (stumps cover, run-back stone), 5 moves + regrow, strip and burn counter (4 bark plates 400 part HP, axes ×1.5, fire ×2 on stripped wood, burning blocks regrow, two burning spots = 3 s stagger every 10 s) | Brief; DC 2.13.1 |
| R34 | Mini-boss the Bramblequeen: Paper Hollow arena (3 paper floors + shell, 4 brood combs 300 HP, water butts per floor), 5 moves, combs/smoke/Hiveback counters, poison ×0.5 | Brief; DC 2.13.2 |
| R35 | Mini-boss the Grey Matron: the Bower (6 oaks, 6 walkways with 2 rope anchors each 100 HP), canopy-only until 4 of 6 walkways are down, ground limp ×0.6, kit scream at 60%/30%, arrows make her jump, walkways regrow 4 h | Brief; DC 2.13.3 |
| R36 | Roaming mini the Hornwarden: 3.2 km route with herd of 6, 5 moves, never-hurt-the-herd enrage, root snares hold him 3 s (3 holds = stance break), breakable antlers | DC 2.13.4 |
| R37 | Major boss Sylvaroth: Clearing of Four Hearts arena (4 heart-trees, root lines, Root Knots 900 part HP, standing stones, altar, run-back stone, Heart Root); healing lines + sever/regrow 45 s + blight 90 s; P1 6 moves; Uprooting transition; P2 4 new + vine sectors + Re-root; hidden P3 heart-trees wake; numbers; lock points; drops; altar lights | Brief; DC 2.14 |
| R38 | Arenas for the boss and 3 minis with run-back stones | Brief; DC 2.13/2.14 |
| R39 | Creature and boss voices: 3+ fused real calls + a material layer, tells rise 3 dB over the bed, signature moments | Brief; DC 2.15 |
| R40 | Music: real music per cue (explore A/B/C, night A/B, combat stems, mini, boss P1-P3, Hollow Root, Bloom + Wild Hunt layers, camp/village), always audible, ducks ≤ 4 dB, credited; sub-zone ambient beds, falls bed, birdsong by day, owls by night | Brief; DC 2.16, 3.4 |
| R41 | The Nature path: 22 nodes (trunk + Grove/Thorn/Bloom), Growth resource sampled by ground (lush 6, normal 3, barren 1), Rooted root passive, Entangled status, plant summons (Sprout turret, World tree, Ancient guardian) not counting as tames, proc feedback, 4 combos with Fire/Frost/Lightning/Blood | DC 2.17 |
| R42 | Nature body change (bark patches at 0.3/0.6/1.0, iris green, temple buds, Rooted green tint, blends with Fire/Frost/Blood) | DC 2.18 |
| R43 | Gear: Heartguard set obtainable (2/4-piece bonuses), briarproof leathers, Bootlace blight, smoke pot, briar balm, snare stake, thorned arrows, heartwood longbow, heartwood shield, foxfire lantern, Huntsman's crown, Hermit's ring, trophies → talismans | DC 2.19 |
| R44 | Building set (Heartwood lodge) and armour set (Heartguard) placed and obtainable here | Brief |
| R45 | Crops (meadow oats, Iðunn apple) obtainable: wild nodes and seeds in the forest; farm set-pieces | Brief; DC 2.4 #2/#28 |
| R46 | Story told only by the environment (pact, roots took the dead, the heart-tree god, the pass war, the newcomers' clear-cut, the hermit) | DC 2.20 |
| R47 | Invasions Nature ↔ Necromancy rows (dormant until the Graveyard) | DC 2.21 |
| R48 | Secrets F-S1..F-S6 | DC 2.22 |
| R49 | Portal slots for this biome | Brief |
| R50 | Camps (3), the newcomer village site (largest), safe zones, PlayerStarts | world-map; DC 2.1 |
| R51 | Everything data-driven the way Fire and Frost are (Data/*.json + tuning.json, attacks.json rows, ClassForNode, ApplyHit only, WR_REGISTER_SCREEN with mouse and pad, sounds/fx as rows) | Brief |
| R52 | Server authority and replication for every new actor and system; multiplayer edge cases | Brief |
| R53 | Nothing unfinished shown as finished; unfinished stays hidden | Brief |
| R54 | Compile gate only; no tests/check scripts; one UE job at a time; assets headless with all six packs present | Brief |
| R55 | Git: pull before and before pushing, add by name, CHANGELOG "Unreleased" + HANDOFF in the same commit, no release/tag, report new binary sizes | Brief |
| R56 | One line to LANDED.md; out-of-scope findings to LATER-COOKS.md | Brief |

---

## 1. Research for the build

The design cook's research (MHW's Ancient Forest food chain, Valheim's Black Forest and the Elder, Elden Ring's
Erdtree Avatars and Ulcerated Tree Spirit, BotW's Lost Woods, Ghost of Tsushima's guiding wind, Pando, *Armillaria*,
mother trees, the strangler fig, the giant hornet and the bee ball, the leshy, Herne, Black Annis, the hulder, the
Elder Mother, pleaching and Meghalaya root bridges) stays the source for what things are. This section adds what the
BUILD needs: how good games make a forest land work in an engine, and the exact choices taken.

### 1.1 Forests that read in games

| Source | What it does | What we take into the build |
|---|---|---|
| **Horizon Zero Dawn, GPU procedural placement** (GDC 2017, Jaap van Muijden, "GPU-Based Run-Time Procedural Placement in Horizon Zero Dawn") | Every asset kind has a density map built from rules (height, slope, distance to roads/water/other kinds, "ecotope" masks); placement is a dither over density, never noise alone. Trees thin at road edges and riverbanks; undergrowth follows tree cover | Dressing is **rule-driven per sub-zone**: a canopy target (0/40/40/75/90 %) sets tree density; trees are pushed off roads (8 m), river channels and POI clearings; ferns/shrubs follow cover; giants keep 40-70 m apart so their crowns don't merge |
| **Far Cry 5, procedural world** (GDC 2018, Etienne Carrier) | Forest edges get dedicated "edge" assets (smaller trees, shrubs) so a wood never ends in a wall of trunks; freshwater carved first, everything else reacts to it | Every clearing and road gets an **edge ring** of saplings/hazel/brambles; the Alder is carved first and the falls, fords, mill race and banks are placed from its line |
| **Ghost of Tsushima** (GDC 2021 "Blowing from the West", procedural grass and wind) | Foliage colour fields are big and simple (one hue per field), wind moves everything in one direction, birds/foxes point the way | One dominant hue per sub-zone (meadow green, fern green, beech silver, umber, deep moss); Glimmerfinch flocks lift over POIs and fresh kills as signposts |
| **Valheim, the Black Forest; Yggdrasil in the sky** | Dark pines and fog make the forest feel deep at low cost; one huge tree on the skyline is the world's compass | The Four Hearts (140 m) and the Eldest (130 m) are the skyline compass from every meadow; forest fog density rises with canopy |
| **Monster Hunter World, Ancient Forest** | Vertical layers with a full floor route; the Ancient Tree's inside is a climbable shaft | Canopy walkways only in F5 and at the Bower; the floor is always a full route; the Strangler Arch is a walk-inside lattice with a vista |
| **Elden Ring, Limgrave/Weeping Peninsula woods and the Erdtree Avatar arenas** | A boss arena is a readable clearing with cover and a clear ring edge; a tree boss reads by body swings | Sylvaroth's clearing is a clean 140 m ring with 4 standing stones on the 40 m ring and 4 root lines you can see from the Fern Stair |
| **Sekiro, the Guardian Ape** | A huge ape body that knuckle-walks, rears, charges and changes completely at its phase break | Sylvaroth phase 2 locomotion borrowed from an ape gait: knuckle-walk, rear, charge with a stop |
| **Dark Souls, Darkroot Garden** (cats and ents dropping, ambushes from above) | Ambushes you can learn: a shadow, a rustle | Canopy drop has a growing ground shadow + leaf shower 0.9 s before the drop; widowmakers 1.0 s |
| **Zelda, the Lost Woods** (BotW/TotK: follow the torches/the wind or the fog sends you back) | A loop puzzle solved by reading the world | The Lost Path: foxfire loops you back to its start unless you follow the trail trees' knee-bends; done in data as a waypoint chain with a reset volume |

### 1.2 Real-world reference used in the build

| Reference | Build detail |
|---|---|
| Old-growth temperate rainforest (Olympic NP Hoh valley, Białowieża): nurse logs with trees in a row on them, buttress flares, epiphyte moss on every limb, deep litter | Fallen giants are long logs with a row of saplings on top; giant trunks flare at the base (radius ×1.8 over the lowest 6 m) with 5-7 buttress roots; moss on the upward faces (vertex-colour moss band) |
| Coast redwood scale (Hyperion 116 m; trunks 4-8 m wide) | Giants 60-100 m with 3-6 m trunks in F4, 90-130 m with 5-9 m trunks in F5; crowns start at 55-65 % height |
| Oak/beech/lime/alder forms | Oak: short thick bole, wide spreading crown; beech: smooth silver bole, dense dome; lime: tall narrow dome; alder: thin, by water; hazel/holly understory |
| Waterfalls on a basalt scarp (Svartifoss, Aldeyjarfoss): columnar basalt, round plunge pool, spray | The Great Falls: a 45 m basalt scarp cut across the Alder's valley, a plunge pool 60 m round, the grotto behind |
| Hedgerow landscape (English Midlands, Normandy bocage) | F1 fields bounded by hedges with field trees every 30-50 m, stiles at gaps, drystone walls on the higher meadows |
| Pleaching and Meghalaya living root bridges | The Root Bridge is a woven root span between two trees on the gorge rims; the Ringfort wall is a pleached beech hedge 12 m tall |
| Counterweight lifts (the funicular/"water-balance" principle; Grovekin: a stone counterweight on a rope over a pulley wheel) | The Canopy Lift: a 30 m rope over a wheel in a fork, basket one side, stone the other; the basket rises as the stone sinks, both visibly moving at 1.5 m/s, never teleporting |

### 1.3 Stand-in creature bodies (same constraint as the Storm lane)

The art PC (SDXL + Hunyuan3D + Blender rig) is not on this laptop. Real rigs with real clips here: our AI-pipeline
creatures (each rigged with Idle/Walk/Run/Attack/Hit/Death) and the Fab QuadrapedCreatures pack (Barghest, Griffon,
MountainDragon, Centaur). Shipped games reuse bodies for variants (Monster Hunter subspecies, Dark Souls reskins with
added parts, Ark's variants): silhouette comes from scale, proportion, hide and added parts. Each forest creature takes
the closest real rig, its own material (palette from DC 2.12.1), its own scale and speeds, and code-modelled parts
that make its silhouette (Mossmaul's moss-and-fern hump and hook claws, the Thornlynx ear spikes and sabre canines,
Elderelk's palmate antlers with ivy, the Burlbeast's bark plates, the Truffler's long snout, the Hornwarden's 8 m crown
with banner rags). The rig choice per creature is recorded in section 3 with the reason. Own sculpts go on the art-PC
list (section 5), like the Storm creatures.

### 1.4 Trees in code (no forest Fab pack on the laptop)

The packs on this PC have pines, a fir, a birch and charred trees; nothing for oak, beech, lime, alder or 100 m giants.
Engine primitives are out. So the forest's trees are **generated meshes**, the way `WRToolMesh` already builds held
tools: a species grammar (bole taper, root flare and buttresses, branch whorls by species angle, crown envelope, leaf
clusters as crossed leaf cards with alpha-free silhouettes cut into the card geometry), built once per species variant
at load into a static mesh and instanced. Real-world proportions from 1.2. Bark colour by vertex colour with a moss band
on upward faces; leaves two-tone by height in the crown.

---

## 2. Plan

### 2.1 Where the land goes (frame and joins)

- **Frame (continent metres, +x east, +y south):** a third landscape, **4,032 m square at 2 m a sample** (2,017
  samples, 16 × 16 components of 126 quads, same as Storm peaks), x **−1,450 to +2,582**, y **+1,904 to +5,936**.
  16 height units a metre, Z scale 800.
- **Join to the Old Island (the south-east neck, world-map 2.3):** the island's landscape ends at y +2,016. In the
  overlap strip (y +1,904..+2,016) the generator writes the island's own height minus 3 m wherever the island is land
  (hidden under it, as Storm did), and raises the **south-east neck** (x +250..+700) from the island's south shore
  into a 5-25 m saddle of pine hollows that grades into alder at y +2,350. The **Pine Sound** (open sea 350-450 m wide)
  stays east of the neck; the **Bone Sound** west of the Spine stays sea.
- **The Spine** runs through the frame's west side on the world-map polyline: (−600, +2,600) 750 m → Rootgrave saddle
  (−1,150, +3,300) 190 m → (−900, +4,200) 600 m → toward the Severance. Its east face is cliff (> 35°, zone `s`); west
  of the crest the ground falls into the sea (the Graveyard is R4, its lane regenerates the west).
- **Rootgrave** is sealed until the Graveyard opens: a rockfall plug (a terrain bump to 230 m + a scree fan of rocks)
  fills the saddle; the forest side keeps its mouth (leaning headstones, the Rotted Treant, the Lantern Line).
- **The coast** runs x ≈ +2,350..+2,520 (noise ±80 m) with dunes, three coves for the camps and a salt marsh at the
  Pine Sound (+1,800..+2,300, +2,350..+2,450). **South:** the land ends with a rocky coast at y ≈ +4,750 (the
  Crystal border strip +4,450..+4,600 keeps its crystal-veined boulders; the Crystal lane extends the land south).

### 2.2 The heightfield (`Tools/terrain/forest.mjs`)

Layers, each with a job (the Storm lane's order):
1. **Land mask and coast** (1.1 above). Sea floor −14 m.
2. **Trend:** coast 2-8 m (dunes 6-12 m), F1 meadows rolling 10-60 m (gentle fbm, 3 low hills: the Antler Barrow hill
   55 m at (+1,700, +2,700), the Ringfort hill 70 m at (+1,600, +4,200), the Badger Sett hill 40 m), rising inland:
   midland 60-140 m, highland 140-260 m at the cliff foot, then the Spine face to the crest.
3. **The Spine:** crest polyline heights with a 35-60° east face (ridged fbm on the face), talus aprons at its foot.
4. **The Alder (carved first, everything reacts to it):** source spring at (−700, +2,750) 250 m in F5 → east through
   the Root Bridge gorge (−100, +2,800: 15 m wide, 20 m deep) → the **Great Falls scarp** at x +300 (a 45 m basalt
   step running N-S across the valley, top 150 m, pool 105 m, plunge pool 30 m round) → Herne's Ford gravel bar
   (+1,100, +2,750) → meanders to the sea at (+2,450, +2,650). Channel 12-30 m wide, 2-4 m deep, banks 1:3, gravel bars
   on inside bends. **Side streams:** the mill race leat (+700..+1,000, +2,520..+2,560, a dug channel), Otter Steps
   (a stair of five 1.5 m drops from (+500, +2,380) to (+700, +2,470)), and a stream from the Hermit's Oak (+1,100,
   +3,550) north-east to the Alder. Water surfaces are written to `rivers` for the runtime water.
5. **Anchors:** every landmark, site, camp, the village, each shrine and arena gets a plateau at its cook spot (core
   flat, smooth falloff): the Clearing of Four Hearts (140 m flat at 210 m with the Fern Stair cut from the floor
   below at 180 m), the Clear-cut (250 m gently domed), the Bower floor (100 m), the Paper Hollow ring, the village
   (300 m terrace at 20 m), the camps (60 m at 6 m), the Long-hall, the Ringfort hilltop.
6. **Gorges and hollows:** Wasp Ravine (+900, +3,550) a 20 m deep cut with overhangs; the Sunken Orchard hollow; the
   Coil Hollow root tangle dip; the Bear's Larder buttress hollow.
7. **The Hollow Root:** carved as a chasm network 10-60 m below the forest floor from the Eldest (−300, +3,000) to
   the west heart-tree (−555, +3,600): rooms (DC 2.5) as flat-floored pits joined by 6-10 m wide passages, floors
   dropping room to room (the Glowworm Chasm a 25 m drop with a spiral stair route). The ground over it stays a thin
   **roof**: the generator leaves the surface open (the landscape is the floor), and the runtime roofs every room with
   a woven root mat (`root_mat`) so it is dark and reads as underground, open only at the four ways in. This keeps
   players above the landscape (Raven's RescueUnderground rule never fires) and needs no landscape holes.
8. **Caves:** the five cave mouths get a hillside face for the sculpted `AWRCave` shell (Raven's caves are shells on
   the ground), C5 Sinkroot a 60 m shaft pit that joins the Hollow Root's Iron Seam by a 30 m crawl (F-S3).
9. **Routes carved last:** Grovekin roads (6 m, ≤ 12°), elk trails (3 m), the coast road camp-to-camp, the village
   lanes; each a graded shelf blended 8 m into the ground, so routes always walk. Fords lower the road into the river
   bed at Herne's Ford and two more.
10. **Detail:** fbm by zone (F1 3 m, F2 6 m banks, F3 4 m terraces, F4 8 m hummocks and root mounds, F5 10 m), plus
    leaf-pit dips (2-3 m ring hollows) and root mounds round each giant (a 1-2 m crown of ground).
11. **Outputs:** `Art/Terrain/forest_height.r16`, `Data/forest.json` (the contract in `WRForestTypes.h`: frame, zone
    grid, sites, kit, shrines, giants, fallen, routes, rivers, falls, brakes, spots, walkways, arenas, hollow rooms,
    village), `Docs/terrain/forest.png` (shaded preview), and the forest dressing instances added to
    `Data/island_dressing.json` (kinds `forest_*`, 2.6).

### 2.3 Landscape material `M_ForestAuto` (`build_terrain_materials.py`, `WR_MATERIALS=forest`)

Zone and slope driven from the MW auto-material textures: F1 meadow grass #7FA34A with flower specks; F2 river gravel
#5A6870 near water (height band 0-3 m over the river), fern green #4E7A3A; F3 moss #5E8C31 over old stone #8A8478;
F4 leaf litter #4A3A2A/#6A5030 under moss; F5 root-mat deep moss #3A5222; cliffs (> 35°) basalt rock with moss on
ledges; sand on dunes and beaches; a `Wet` scalar for rain. Zone masks come from world-position bands the script
reads from `forest.json` (x bands for the sub-zones, as Storm's GlassScar band).

### 2.4 Plumbing so a south land is a biome (every [N-only] place the code map found)

| Piece | Change |
|---|---|
| `WRBiome::At` (`WRSurvival.cpp`) | `if (WRForest::Contains(Pos)) return "forest"` before the island fallback (Contains = live + south of `world.islandSouthEdge` + inside the frame and not `~`; the neck south of +2,016 m counts) |
| `world.liveBiomes`, `world.islandSouthEdge` | `volcanic,tundra,storm_peaks,forest`; `islandSouthEdge: 201600` |
| `WRForestTypes.cpp` | `FWRForestData::Get` (parse), `ZoneAt`, `Contains`, `Band`, `Canopy`, `NoFelling` |
| Wildlife (`WRWildlife.cpp`) | `RangeOf`/`InRegion`/`RaidersAt`: a forest-biome kind lives where `WRForest::ZoneAt` is `1`-`5`; regions with `side: forest` and `zones` letters; island kinds stay off forest land |
| Walk grid (`Tools/terrain/forest_walk.mjs`) | grows `island_walk.json` **south and east** to cover the frame (cells from the forest heightmap; camps and arena keep-outs) |
| Sea (`sea_mask.mjs`) | reads the forest heightmap; the grid grows south and east |
| Map (`world_map.py`) | draws the forest into `Data/UI/map.png`, pads south and east; `ui.mapMin/mapSize` updated |
| Sky (`WRSky.cpp`) | a `ForestAir` weight by region; forest look keys `sky.forest*` (green-gold light, soft haze) and **canopy fog**: fog density and sky light dim by `WRForest::Canopy` at the view (F5 dusk by day) |
| Resource looks (`WRResourceLook.cpp`) | forest rows: mossy grey rock, oak stump look |
| Portals (`WRPortals.cpp`) | `PsStandAs` forest rows, `FindSpot` forest frame box, `SpotOk` south rule; `portals.json` forest row stays `land:false` (no fold kit, like Storm) |
| Dressing (`dressing.mjs`) | keeps `forest_*` instances and forest POIs when the island dressing is regenerated |
| Streaming | `streamingRangeScale` stays 32 (the forest's far side is 4.6 km from the island centre) |
| Spawns | PlayerStarts tagged `forest` at the three camps + safe zones (r 40 m) + starter stations at the middle camp (`import_forest.py`) |
| Meteors | `DropMeteor` gains a forest share (`events.starfall.southShare`) |

### 2.5 Runtime-built world (`WRForestWorld.cpp/.h`, `WRForestScenery.cpp`)

`UWRForestWorld` (world subsystem; runs when the map is `forest.map` and the forest is live): `BuildScenery()` on every
machine (static looks, no network cost), `SpawnServerActors()` on the server (stateful things, replicated).

**Scenery (local):** one `AWRForestScenery` per landmark, site and kit row, each kind code-modelled at real scale from
the tree kit (`WRTreeKit`) and the build look kit (`FKit`: stone, wood, cloth, glass), each with its night light:
- **L1 the Four Hearts:** four `heart` trees 140 m (trunk r 4.5 m, 7 buttresses), amber sap veins = emissive strips
  up the bark cracks (#D08A1E, brighter at night and in the Bloom), crowns 40 m over the canopy; pale foxfire haze
  sprites over the clearing at night.
- **L2 the Eldest:** `eldest` 130 m broken at 110 m (jagged top), an 8 m shelf of bracket fungus (cream discs in a
  spiral band at 6-9 m), foxfire glowing from the base cracks; the Eldest's Door crack at its foot.
- **L3 the Great Falls:** terrain scarp + a falling water sheet (translucent ribbon mesh with scrolling water material
  from the WaterMaterials pack) + spray sprites + the noon rainbow (a sprite arc lit 11:00-13:00 on clear days), the
  Grovekin fishing platform on stilts (Falls Shrine on it), glowworm curtain under the overhang at night.
- **L4 the Rotted Treant:** a `dead_giant` laid along the ground toward the pass with an arm (a branch) reaching west,
  roots black (#1E1A16), 6 tōrō lanterns (stone, unlit) hung from its dead branches by chains.
- **L5 the Clear-cut:** 250 m of stumps (`stump` 0.5-1 m, 340 of them, axes stuck in 40), the Woodcutters' Camp.
- **L6 the Paper Hollow:** a `dead_giant` oak 50 m wrapped in a 20 m paper hive shell (ribbed lantern of grey paper
  rings, three inner gallery floors with chewed-paper ramps; walkable), amber grub glow inside.
- **L7 the Bower:** six `bower_oak` 70 m on a 30 m ring with six sagging Grovekin walkways (owned by the boss track's
  `AWRForestWalkway` so they can fall), deer and elk hides hung like flags (cloth quads), green eyeshine pairs at night.
- **L8 the Hermit's Oak:** a `hermit_oak` with a Grovekin house in its fork at 12 m (round walls of woven living
  branches, a moss roof, a clay chimney with smoke), a spiral stair of planks pegged round the trunk, lanterns.
- **L9 the Strangler Arch:** `strangler` 60 m, a hollow lattice you walk inside (a spiral root ramp to a vista at the
  top), its lower roots arching over the village road; foxfire inside at night.
- **L10 the Antler Barrow:** a long barrow mound (terrain + turf shell), standing stones at its door, the 6 m elk skull
  on the mound (bone kit), a stone door into the burial chamber (the crude heartwood longbow + "Antler crown").
- **L11 the Bell-Tree:** a `bell_tree` dead giant with a bronze bell hung at 40 m under a beam, root holds up the trunk
  (F-S5), violet crystals in its roots lit at night.
- **L12 the Ringfort:** a 12 m pleached beech hedge in a ring 60 m across (`beech` trunks woven, a living lintel over
  the gate), a well, a merchant stall and fire inside.
- **Hand sites** (36, DC 2.4) and **kit POIs** (12 kinds) the same way; each kind's dressing named in the scenery code
  with its "why" comment (story beat, route, pay).
- **Canopy layer (F5):** platform houses and walkways 20-40 m up between giants (plank decks on beams socketed into the
  trunks, rope rails to iron rings, the deck sag by catenary), the **Canopy Lift**: a 30 m rope over a pulley wheel in a
  giant's fork, the basket on one end and a stone counterweight in a rope cradle on the other; Interact pulls the stone
  and the basket rises at 150 cm/s while the stone sinks the same, both moving every frame from a replicated start time
  (never snapping), with a creak loop.
- **The Root Bridge:** two root masses on the gorge rims and a woven root deck between them (catenary, moss rails).
- **The Mill Race:** a ruined overshot wheel (spokes, buckets, a broken shaft into a gear pit with 2 salvageable gears).
- **Trees (instances):** dressing kinds in `island_dressing.json`, harvestable like the island's: `forest_oak`,
  `forest_beech`, `forest_lime`, `forest_alder`, `forest_ash`, `forest_hazel`, `forest_holly`, `forest_giant` (not
  harvestable: the roots hold it), `forest_fern`, `forest_bramble`, `forest_rock` (mossy boulder). Density by the canopy
  gradient (2.6).

**Server actors (replicated, stateful):** caches (site `loot` rows, refill `forest.siteRefillHours`), caves
(`AWRCave` Biome forest + Theme), resource nodes (heartwood at every fallen giant's broken end: `Nodes` per log;
resin weeping wounds; honey hives in beehive trees; truffle spots (only a Truffler or the Truffle sense perk reveals);
bootlace rhizomorph; foxfire wood; thorns at brake edges; shed antler at Herne's Ford; ore in caves; wild meadow oats
and Iðunn apple nodes), the Canopy Lift, the Bell, offering stones, the Thrumming Root, the Seed Store, the eel trap.

### 2.6 Dressing rule (the decoration rule made into numbers)

Per 32 m cell, by zone, trees placed by Poisson disc with a minimum spacing, density from the canopy target, pushed off
roads (8 m), rivers (channel + 4 m), POI clearings (each POI gets a light gap 25-40 m round) and arenas:
F1 0.0-0.08 cover (hedgerow lines on field edges every 30-50 m: oak field trees + hazel/holly hedge shrubs, orchards in
rows at the farm sites), F2 0.4 (alder and ash on banks, willow-like alder lean over water, ferns 60%), F3 0.4 (beech
and lime, holly understory, herb terraces), F4 0.75 (oaks and beech 25-40 m + giants 60-100 m every 55-70 m, ferns
and brambles), F5 0.9 (giants 90-130 m every 45-60 m, root-mat, ferns). Edge rings of hazel and saplings round every
clearing (Far Cry 5). Fallen giants placed first (12 kit + the Nurse Log), then the light gaps. Every giant's root
crown gets a thornbrake ring (9% of F4/F5 floor in total) with a 2-3 m gap on one side most of the time (a way in for
the player who looks).

### 2.7 Hazards (`WRForestHazards.cpp/.h`)

- **`AWRThornbrake`** (server-spawned per `brakes` row, replicated burn state): looks are bramble-cane clusters (tree
  kit `bramble`, dark red-brown canes with pale thorn tips); an overlap volume per ring/wall; on a pawn inside: first tick
  0.6 s after contact, then 6 bleed buildup/s (× balm 0.3) and run ×0.5 (server-driven move tag as Raven's slowed); the
  arms-up shield layer and crackle/scrape sound; `PassesThornbrake` (Nature points > 0, `briarproof` chest worn,
  Heartguard 4 set) → no slow, no bleed, brambles part (a local bend of the cane instances round the pawn). A torch held
  at it 2 s or any fire hit burns a 4 m gap (canes char black, embers), open 5 game hours, then regrows over 30 s.
  Creatures ignore it.
- **`AWRWidowmaker`** (per `spots` widowmaker): a dead grey limb wedged in a giant's crown over a path. Trigger: a
  pawn under it (r 2 m footprint) running, fighting, or a Mossmaul roar within 6 m: crack + bark shower + a growing
  ground shadow for 1.0 s, then the limb falls (a real fall: rigid body path, 70 damage via ApplyHit, knockdown).
  Shootable: 30 damage on the limb brings it down on whatever is under it. Re-arms after 1 game day.
- **Leaf pits** (per `spots` leaf_pit): a litter ring decal that sags, a hollow drum sound on the edge step; stepping in
  collapses the litter (a visible drop of the litter mat) into the carved 4-9 m hole (fall damage rules). A Truffler
  within 10 m snorts (creature track) and the pit's rim shows for 5 s.
- **`AWRWaspNest`** (per `spots` wasp_nest + nests at the Paper Hollow and Wasp Ravine): a paper nest on a branch; hum
  within 15 m; hit (any) or 5 s within 4 m → a scout marks the pawn (`MarkForWasps`) and 6-10 Bramblewasps leave the
  nest; smoke (a torch held 3 s or a smoke pot) calms it 60 s; asleep at night (a hit wakes it). Steal a larva
  (Interact while calmed) = the tame egg.
- **Spate:** after a Rain block the Alder's water mesh turns brown and loud, logs ride it (floating debris meshes on the
  surface flow), and a flow volume pushes 900 cm/s downstream at fords; at the Great Falls lip a volume carries you over.
- **Rivers:** `AWRForestWater` per river: a surface ribbon mesh (water material), a swimmable water physics volume per
  segment, flow direction for spate push; wading in water or swimming clears a wasp mark.
- **Smoke:** `AddSmoke` keeps a list of live smoke spheres (server) + a smoke FX (`forest.smoke`).
- **No felling:** `WRForest::NoFelling` checked in the tree-harvest path; the axe bounces with "The roots hold it.".

### 2.8 The Hollow Root (`WRForestHollow.cpp`)

The carved chasm network (2.2 layer 7) roofed by `root_mat` meshes over every room and passage (gaps only at the four
ways in), its walls dressed with root pillars (`buttress_root`, `root_arch`), foxfire on every surface (emissive green
streaks, 12% light: dim point lights every 15 m), glowworm curtains (hanging emissive threads). Rooms in DC 2.5 order:
Root Hall (2 Rootcoil), Cradle Vault (bark cradles in root hands), Grove of Bones (giant skeleton, heartwood spear),
Weeping Roots (resin ×10 basins), Mossmaul Den (asleep Mossmaul), Bootlace Gallery (rhizomorph ×6 nodes, glowing
tips), Thrumming Root (a pulsing 6 m root with a heartbeat sound; Interact = a 3 s map flash of the four heart-trees
+ lore item), Drowned Root (15 m swim), Seed Store (one random rare seed a game day), Wasp Grave, Glowworm Chasm
(spiral root stair, a fall = death), Lattice Chamber, Iron Seam (iron ×4 + gem chance), the Heart Root (one-way: a
root door that only opens from inside, into the west heart-tree's roots). Ways in: the Eldest's Door (behind a
thornbrake wall), the Sinkroot squeeze (crouch crawl), the Truffler dig at (−220, +3,080) (a tamed Truffler commanded
there digs 20 s and a root plug collapses), the Heart Root. Music cue `sub.hollow_root`, bed `forest.hollow`. Inside,
WRForest::ZoneAt returns `h`.

### 2.9 Creatures (`WRForestCreatures.cpp/.h`; rows in `creatures.json`, `attacks.json`, `spawn_regions.json`)

Every row from DC 2.12 in the existing schema (`biome: forest`, `resists: [thornbrake, leaf_pit]`, `element: nature`
where it fits). Rigs (closest real rig, own material, own scale, code-built silhouette parts on bone frames, like
`UWRStormBody`); the creature track checks each rig's proportions and records the final pick:

| Creature | Rig (first choice) | Code-built parts |
|---|---|---|
| Mossmaul | Rimeclaw (heavy, maned) scaled to 2.2 m hump | moss-and-fern mat on hump and spine (fern fronds, shelf fungus discs), three 35 cm hooked claws per forearm |
| Thornlynx | Pale_hunter (long low cat) at 2.4 m | black ear spikes, sabre canines, cloud-blotch tint |
| Rootcoil | Glacimaw (long ringed worm) 12 m | bark-keeled scale plates, moss saddles, wedge head plates; log-rest pose |
| Bramblewasp | Kilnmite ×0.25 | orange head + mandibles, banded abdomen, smoky wing pairs (beating) |
| Elderelk | Frostantler at 2.1 m | palmate antlers 3.6 m with ivy |
| Burlbeast | Emberhorn at 2.0 m | ridged bark plates on back and shoulders, white stockings |
| Truffler | Pumicehog at 1.2 m | long flexible snout, short tusks, bristle crest |
| Hiveback | Glowmoth at 1.0 m | fur barrel body bands, pollen baskets, blurring short wings |
| Fernsheep | Woolly_drudge ×0.35 | fern-curl fleece clusters, swept amber horns |
| Dappledoe | Frostantler ×0.45 | white spots, black-edged white rump, bucks' palmate antlers |
| Glimmerfinch | Snowpuff ×0.15 (feathered) | iridescent back, violet throat, red face |

Mechanics in code (server): canopy-drop perches (from `spots` perch rows + any giant within 25 m, a climb to 6-12 m,
the drop with a growing shadow decal + leaf shower 0.9 s), tree escape below 40%, Rootcoil log rest (breathing loop,
looks like a log until 7 m), Maul hug / Constrict grabs (the existing grab handling in attack rows: `grab` flag, mash,
ally damage frees), roar → `DropWidowmakers`, bull-rush trunk stun (wallStun), scout mark (`MarkForWasps`), heat-ball
cling (max 4, −10% move each, roll knocks one off 40%, fire knocks all off; ×3 vs wasps), Elderelk bugle (herd turns),
herds defend each other, path-magic tame (Mossmaul: hold Interact 4 s with ≥ 10 Nature points, 40 Growth), tame
utilities (Elderelk thin capsule + 2 m leap + thornbrake pass; Burlbeast heartwood +25% within 15 m + daily log drag
40 heartwood; Truffler truffle/cache/pit sense + Hollow Root dig + seeds; Glimmerfinch pollinate ×0.85 + whistle
sweep; Hiveback home in the beehive piece), and the living-world beats (DC 2.12.3) in a `UWRForestLife` server
subsystem. Day/night tables per sub-zone in `spawn_regions.json`.

### 2.10 Bosses (`WRForestBosses.cpp/.h`; rows in `creatures.json`, `attacks.json`, tuning `bosses.*`)

| Boss | Rig | Arena build | Counter mechanic in code |
|---|---|---|---|
| Old Gnarl | Obsidian_warden (heavy humanoid) ×3.5 | the Clear-cut stump field; run-back stone 50 m east | 4 bark-plate parts (400), axes ×1.5, stripped = pale wood, fire ×2 + burn, burning blocks regrow, 2 burning = 3 s stagger / 10 s; regrow move; boulder = stump + root ball projectile on a gravity arc with a landing shadow, never inside 8 m |
| Bramblequeen | Kilnmite ×4 (the broodqueen rig) | the Paper Hollow's 3 floors + shell; 4 combs (300) + water butts | combs gate swarm calls (6 wasps / 25 s), all combs down = −20% speed; smoke ×0.7 5 s; 3 Hivebacks hold 2 s; poison ×0.5; pheromone mark; wing gust off edges |
| Grey Matron | Pale_hunter ×1.7, grey-blue, scarred, torn ear | the Bower's 6 oaks + 6 walkways with 2 rope anchors (100) each | canopy-only movement on the walkway graph until 4 of 6 are down or her tree has none; then ground limp ×0.6, every paw combo 1.2 s punish; arrows into her walkway → she jumps; kit scream at 60/30% (kits go for the furthest tame); walkways regrow 4 h after |
| Hornwarden | Frostantler ×2.1 + 8 m antler crown with banner rags | roams a 3.2 km loop (Antler Barrow noon, Hanging Oak dusk, Herne's Ford nightfall, coast) with a herd of 6 | herd damage → 60 s enrage (×1.3 dmg, ×1.2 speed, punish ×0.7, Bellow every 12 s); snares hold 3 s (Entangle with bIgnoreBossRule) and stance ×2 while held, 3 holds in 20 s = stance break; antler part 1,200; the Wild Hunt route + herd 10 in the Bloom's night |
| Sylvaroth | Grimjaw (humanoid boss) ×4 for the torso/arms, rooted in P1 (lower body sunk), knuckle-walk in P2 | the Clearing of Four Hearts: 4 heart-tree actors, 4 root lines (glowing strips) with Root Knots (900, axes ×1.3, fire ×1.5), 4 standing stones, the altar, the Fern Stair, the run-back stone | line heal 0.15%/s each (×1.5 in the Bloom), sever → 45 s regrow with a 3 s light crawl, blight 90 s, decay stops healing; P1 6 moves; Uprooting (20 m crimson ring, 60 + knockdown, lines dark 10 s); P2 knuckle charge stops 6 m short of heart-trees, hammer fists, thrash ×4, vine sectors (6, creep 1.5 s, live 12 s, Entangle; fire clears 10 s), Re-root 6 s; hidden P3 (artifacts) heart-trees sweep every 6 s, amber hearts 1,500; damage ×1.4; drops; altar `PathId=nature` |

Arena placement like `UWRStormArenas` (spawner when a player is near, run-back stones from `forest.json`), roaming
Hornwarden through `roam` (Raven's `StepRoamingBosses`). Sylvaroth's lattice silhouette: code-built root lattice
cage around the rig (root tubes on bone frames), the canopy hump (ferns, saplings, dead branches) on the back, the burl
head with amber eye hollows, foxfire glow inside the lattice.

### 2.11 The Nature path (`Data/paths/nature.json`, `WRNaturePath.cpp/.h`, `WRAbilitiesNature.cpp/.h`)

22 nodes exactly as DC 2.17 (trunk 4, Grove 6, Thorn 6, Bloom 6), costs, ranks and gates. Growth attribute
(`Growth/MaxGrowth` in `WRAttributeSet`), the ground sampler (every 0.5 s within 8 m: lush 6, normal 3, barren 1 by
the landscape layer / biome / plants nearby; caves barren), Sap ranks, Rooted (1.5 s still → 1%/s, ×0.5 on barren,
damage resets), Entangled (`WRForest::Entangle`: the existing `Status.Rooted` movement lock with tier times, boss slow
fallback, 8 s immunity, roots-wrap FX), the 10 abilities in `ClassForNode` (Thorn shot, Briar strike, Regrowth,
Sanctuary, World tree, Vine lash, Root snare, Strangling vines, Sprout turret, Ancient guardian), passives (Entangle
chance, Lifebloom, Purify, Barbs, Thorned skin, Briar spread, Deep roots, Pollen, Overgrowth, Fertile, Growth
capacity), `AWRPlantSummon` (+ turret / world tree / guardian; not tames for boss HP), proc FX + sounds, the four combos
(Smoulder roots / Forest fire, Evergreen / Frozen grove, Quickening / Living conductor, Red sap / Thorn feast) in
`combos.json`, the Growth gauge (moss green #5E8C31 + a lush/normal/barren ground icon), the Paths screen palette, the
body change in `player_art.json paths.nature` (bark at 0.3/0.6/1.0, iris green, temple buds) + the Rooted green tint.

### 2.12 World rows (`Data/world.json`, items, recipes)

- `biomes.forest {name "Ancient forest", path "nature", signature "heartwood", essence "verdant_essence", centre,
  waymark "trail_tree", rock, accent, glow}`.
- **Shrines** (5, DC 2.11) with guards; the Great Shrine inside the Long-hall.
- **Rowan Ashby** (`masters`, MetaHuman, Hermit's Oak at night, Grove Circle by day, lines) + `quests.rowan` (6, DC
  2.11) with rewards (recipes; Hermit's ring).
- **The village** (`villages` row: the newcomers' village at (+2,100, +3,450), safe zone, huts, stall) if the
  villages schema takes a biome look; else its huts as scenery and the safe zone at import.
- **Event `bloom`** (`events.bloom`: everyHours 72 × the Biome events setting, warn 300 s, the steps in 2.13).
- **Invasions** `forest → graveyard` and back (dormant until the Graveyard is live), leader `hornwarden`.
- **Items and recipes** (every id in DC 2.19 and 3.1, reusing what exists: `heartwood`, `resin`, `honey`,
  `hiveback_wax`): heartwood_shavings, truffle, bootlace_rhizomorph, foxfire_wood, thorns, wasp_sting, wasp_paper,
  royal_jelly, mossmaul_claw, spotted_pelt, rootcoil_leather, rootcoil_shed, elk_antler, verdant_essence,
  core_sylvaroth, verdant_core, trophies ×4 + talismans ×4, huntsmans_crown, hermit_ring, bp_world_root_maul,
  bp_grovekin_decor, bp_heartwood_longbow, sturgeon_hook, heartwood_circlet, bootlace_blight, smoke_pot, briar_balm,
  snare_stake, thorned_arrow, foxfire_lantern, heartwood_longbow, heartwood_shield, briarproof_leathers, wasp_larva,
  thornlynx_kit, the 7 lore items. Recipes per DC 2.19.

### 2.13 The Bloom (`WRForestHazards.cpp` event step)

Every `events.bloom.everyHours` (world setting scale): warning 300 s (pollen haze FX from every giant crown in F4/F5,
Glimmerfinches lift and ring the Four Hearts, the Hearts' sap brightens, a world message), then one game day: crop and
wild node growth ×2 (farm and node respawn hooks read `WRForest::Bloom`), rare seed 10% on wild plant nodes, heartwood
yield ×3, honey ×2, three **Elder Falls** at random F4/F5 spots ≥ 150 m from camps/claims/shrines/arenas (5 s: crack
sound heard from 1 km, birds lift, the dead giant leans; then a 90 m crush line via ApplyHit and a fresh fallen giant
with 12 heartwood + 6 resin nodes for the day), noon wasp raids (2 swarms of 12 from the Paper Hollow to the nearest
beehive trees and player beehives within 1.5 km; Hivebacks heat-ball them; a beehive piece loses honey if the wasps
win), the Wild Hunt at night (boss track: herd 10, hunt route, avoids safe zones), Sylvaroth's lines ×1.5. Never in
camps, the village or claims.

### 2.14 Sound contract (owned by the audio track; every other track calls only these ids)

Creature families (`creatures.<id>.family`): mossmaul, thornlynx, rootcoil, bramblewasp, elderelk, burlbeast,
truffler, hiveback, fernsheep, dappledoe, glimmerfinch, old_gnarl, bramblequeen, grey_matron, hornwarden, sylvaroth
(events idle/aggro/attack/hurt/death each as Raven's schema). Events: `forest.thorn_scrape`, `forest.thorn_burn`,
`forest.widow_crack`, `forest.widow_fall`, `forest.leafpit_drum`, `forest.leafpit_fall`, `forest.wasp_hum`,
`forest.wasp_swarm`, `forest.scout_mark`, `forest.heat_ball`, `forest.spate`, `forest.falls`, `forest.lift_creak`,
`forest.bell`, `forest.root_pulse`, `forest.elder_crack`, `forest.elder_fall`, `forest.bloom_warn`,
`forest.canopy_rustle`, `forest.log_breath`, `forest.mossmaul_roar_big`, `forest.hunt_hooves`, `forest.hunt_horn`,
`forest.knot_break`, `forest.line_regrow`, `forest.uproot`, `forest.vine_creep`, `forest.seed_pod`, `forest.bark_strip`,
`forest.bark_regrow`, `forest.comb_break`, `forest.walkway_fall`, `forest.anchor_cut`, `forest.kit_scream`,
`forest.offering`, `forest.dig`, `nature.cast`, `nature.thorn`, `nature.entangle`, `nature.rooted`, `nature.thorned_skin`,
`nature.pollen`, `nature.purify`, `nature.world_tree`, `nature.guardian_rise`, `nature.guardian_slam`, `nature.heal`,
`nature.turret_shot`, `nature.vine_lash`. Beds: `forest.meadows`, `forest.streams`, `forest.villages`, `forest.oldgrowth`,
`forest.canopy`, `forest.hollow`, `forest.falls_near`, birdsong day / owls night. Music pools: `explore.forest`
(A/B/C by zone), `night.forest`, `combat.forest`, `mini.forest`, `boss.sylvaroth` (P1/P2/P3), `sub.hollow_root`,
`layer.bloom`, `layer.wild_hunt`, `camp.forest`, `village.forest`.

### 2.15 Edge cases decided

- A point on the island/forest seam: `WRForest::Contains` decides (forest owns y > +2,016 m inside its frame).
- Felling refused near giants; harvest of giants never possible; fallen-giant heartwood only from the broken end.
- Thornbrakes never block a route with no way round (each road wall has its 80-150 m detour; the generator keeps
  every road cell's neighbours passable on one side).
- Elder Falls and the Wild Hunt never in safe zones or claims; a falling limb or elder fall never lands on a camp.
- Entangle never on a boss except the Hornwarden; immunity 8 s stops chain-locks; PvP players 0.6 s.
- Plant summons don't count as tames for boss HP; bosses' tame-punish targets the World tree/Guardian only with no tame.
- Wasp mark is per-pawn and server-owned; water (swimming or wading) and smoke clear it; it ends on death.
- Canopy Lift: one rider set at a time; the motion is from a replicated start time so every machine draws the same
  basket position; a player stepping off mid-way just falls (the basket has rails).
- Walkways (Bower) are server state; their fall replicates; they regrow 4 game hours after the Matron dies.
- Hollow Root: dark but never pitch black for the eye (foxfire 12%), the rescue rule never fires (players stay above
  the landscape floor).
- Rootgrave: sealed by the rockfall plug until the Graveyard lane; the forest side's sites still stand.
- Portals in the forest stay `land:false` until a forest fold kit exists (as Storm).

### 2.16 Build order and tracks

0. Ledger, research, plan (this doc). Contracts written first: `WRForestTypes.h`, `WRTreeKit.h`, the sound ids above.
1. Seven parallel build tracks, each owning its files (shared data files edited with small in-place edits only):
   - **A Terrain + plumbing:** `forest.mjs`, `forest_walk.mjs`, sea mask / map growth, `import_forest.py`,
     `build_forest.ps1`, `M_ForestAuto`, `WRForestTypes.cpp`, `WRBiome::At`, live biomes, wildlife south rules, sky,
     resource looks, portals plumbing, meteors.
   - **B Tree kit:** `WRTreeKit.cpp` (all species) + the `forest_*` dressing kinds in `WRWorldDressing.cpp`.
   - **C World:** `WRForestWorld.h/.cpp`, `WRForestScenery.cpp` (landmarks, sites, kit, canopy, lift, bridge, wheel),
     server actors (caches, caves, nodes), items/recipes, world.json (biome row, shrines, Rowan, quests, village).
   - **D Hazards + Hollow Root + Bloom:** `WRForestHazards.h/.cpp`, `WRForestHollow.cpp`, rivers/spate, smoke, no
     felling, secrets F-S1..F-S6, `events.bloom`, invasion rows.
   - **E Creatures:** `WRForestCreatures.h/.cpp` + rows + spawns + beats + tame utilities.
   - **F Bosses:** `WRForestBosses.h/.cpp` + rows + arenas + altar + Wild Hunt.
   - **G Nature path:** `nature.json`, `WRNaturePath.h/.cpp`, `WRAbilitiesNature.h/.cpp`, Growth, Entangled, combos,
     HUD, body change.
   - **H Audio:** sourced CC0/CC-BY recordings and music, `Tools/audio/build_forest_audio.py`, `sources_forest.py`,
     `sounds.json` rows, `WRAudioDirector` forest beds/pools, `Docs/CREDITS.md`.
   - **Ownership splits decided:** the four arena landmarks belong to the boss track (L1 Four Hearts incl. the
     heart-trees that wake in P3, L5 the Clear-cut stump field, L6 the Paper Hollow hive, L7 the Bower oaks and
     walkways); the world track builds L2-L4 and L8-L12 and every non-arena site. Thornbrake walls with a cache behind
     them, the eel trap, the Lost Path, the barred door, the Bell, the grotto back room and the Sinkroot squeeze are the
     hazards track's; truffle and seed finding is the creature track's (the Truffler finds them; no hidden nodes).
     Quest events fired by the track that owns the moment: `forest_briar_cache` (D), `forest_smoke_honey` (C),
     `forest_lost_path` (D), `grey_matron_walkways` (F).
2. Terrain import (one headless editor run) when track A's files are written.
3. Compile (full unity, HEAD + forest only, in the verify worktree); errors back to their tracks; repeat to clean.
4. Spot-check against this plan and THE BAR by reading the code; fix.
5. CHANGELOG/HANDOFF, commit the forest's own lines, push; LANDED line.

## 3.T7/T8 build notes (resume run, 2026-11-15)

**T7 Nature path, audited against DC 2.17/2.18.**
- `Data/paths/nature.json`: 22 nodes, costs, ranks and gates match DC 2.17 exactly (trunk 3/3r/2r/2; Grove 5,5r@5,4@10,3r@14,3@17,10@20; Thorn 5,5r@5,4@10,3@14,3r@17,10@20; Bloom 5,5r@5,3@10,4@13,3r@17,10@20).
- Growth (`WRAttributeSet` Growth/MaxGrowth: init, owner rep, clamp, OnRep each once beside the wasteland's Void/MaxVoid/MaxHealthLost; no duplicates). Sampler in `WRNature::ClassifyGround` reads `WRBiome::At` + `WRForest::ZoneAt` (no landscape layers: works on the WRLand runtime ground), plants within 8 m, caves/floors barren, lava/ice/snow barren (Evergreen: Normal). Added: the wasteland's void ground is Barren.
- `ClassForNode` has the 10 abilities; **fixed** `IsBuilt`: Purify, Thorned skin, Pollen, Overgrowth (kind "ability" in the file, built as passives) showed "not built yet".
- Rooted, Entangled (tier times, players 0.6, bosses slow, Hornwarden exception, 8 s immunity, no roll/jump), passives, 4 combos in `combos.json`, plants not counted as tames (`AWRBoss::UpdateParticipants` only counts `AWRCreature`), Growth gauge (Slate + `-oldui`), Paths palette, 22 path glyphs + `leaf` mask present.
- **Added the temple buds** (DC 2.18 at 1.0, were missing): `WRNature::ApplyTempleBuds`, two modelled buds (FWRToolMesh lathe: bark collar, bud, two leaflets) on the head bone, placed from the bone's reference pose in the body's own frame, grown over the last 5 points before 1.0, hidden with the face; data `player_art.json paths.nature.look.templeBud`. Called from `AWRCharacter::ApplyPathMarks` after the canines.
- Unity safety: `ApplyLook`'s `using namespace WRNatureLook` removed and its helpers qualified (WRFx.cpp's file-scope `using namespace WRFxDetail` brings an identical `Num(const TSharedPtr<FJsonObject>&, const TCHAR*, float)` into the global scope: an ambiguous call in a unity blob).
- Open, not T7's: no forest `PlayerStart` (tag `forest`) exists yet, so a Nature starter spawns at the default start until the land track places them (the main menu lists Nature from the registry). `WRForestTypes.cpp` (ZoneAt, Contains, Band...) not written yet at my sweep (T1). Tame-punish moves aiming at the World tree / Guardian are the boss track's.

**T8 Audio, audited against plan 2.14 / DC 2.15-2.16.**
- Every id in 2.14 has a row; the 16 families each have idle/aggro/attack/hurt/death/tell/signature and a `creatures` row; beds forest.day/night (birdsong by day, owls by night), meadows(+day), streams(+night), villages, oldgrowth, canopy, hollow, falls_near; pools explore.forest.a/b/c, night, combat, mini, boss.sylvaroth p1-p3, sub.hollow_root, camp, village, layer.bloom, layer.wild_hunt. Every referenced file exists (0 missing). All 444 new OGG files are credited in `Docs/CREDITS.md`. Music ducks capped at 4 dB in the forest (`music.forestDuckMax` 0.37), gaps 3-10 s.
- `WRAudioDirector` forest code matches the current `WRAudio.h` (no duplicate fields); removed `StepForestLayer` (a copy of `StepMusicLayer`), the Bloom / Wild Hunt layers use `StepMusicLayer`.
- Last sweep of every `WRSound::*` / WRFx sound argument in WRForest*, WRNature*, WRLand*, WRAbilitiesNature, WRPlantSummon, WRTreeKit: no id without a row (none mapped). `creatures.json` has no forest rows yet (T5); any creature id that isn't a family name needs a `creatures` row in sounds.json when T5 adds it.

## 3.T2 build notes (resume run, 2026-11-15): trees

**Audit at start.** `WRTreeKit.cpp` (2,738 lines) had every species of the header as a real grammar (bole, flare, fins,
forks, limbs, leaf clusters cut to leaf shapes; fern fronds, bramble canes, boulder) but nothing drew the 555 giants and
29 dead snags in `forest.json`, `island_dressing.json` had no `forest_*` rows and `WRWorldDressing.cpp` had no forest
kinds: the forest floor would have been bare ground.

**Built.**
- `Tools/terrain/forest_dressing.mjs` (new): reads `Data/forest.json` + `Art/Terrain/forest_height.r16` (frame from the
  json: x0 -1450, y0 +1696, 2 m) and writes the `forest_*` kinds into `Data/island_dressing.json` (removes old forest
  rows first, one `JSON.stringify` of the whole file like storm_peaks/swamp; re-run after `forest.mjs`). Order: giants and
  snags from the json rows (untouched; never placed by this script) -> F2 bank alders -> edge rings (hazel + saplings,
  every forest light gap) -> F1 hedgerows (Voronoi field edges ~150 m, 2 in 3 hedged, a forced hedge line through every
  hedgerow-gap kit along its local Y; oak standards every 30-50 m; hazel/holly/bramble/sapling between) and lone field
  oaks -> canopy trees on a 6 m jittered grid, chance per zone from cover = 1-exp(-n pi r^2) (F2 .2, F3 .1, F4 .17, F5 .1
  per 36 m^2; giants supply F4/F5's top cover), zone drawn from a 112 m Gaussian of the zone grid (60-120 m blends) ->
  understory (F3 holly, F4 hazel/holly/bramble clumps, a few stumps F1/F3) -> mossy boulders (river edges, Crystal
  border y>4450, Spine talus) -> fern patches (F2 banks 75%). Keep-outs: routes (trees 5 m from the edge, incl. the
  Hornwarden's loop and the Wild Hunt), river channel + bank, falls, light gaps (camps 60, village r+15, landmarks 40,
  sites 30, caves 20, Sinkroot 50, arenas r+14, kit by kind), brake rings and walls, giant trunks, fallen logs + root
  plates, leaf pits and spots, Hollow Root rooms, Rootgrave plug, 7 m under canopy walkways, the island-overlap strip.
  Bases sit at the lowest ground round the flare. Leaning alders are yawed toward the water.
- Counts (run 00:48): oak 2521, beech 3402, lime 1433, alder 1672 + 218 leaning, ash 1529, hazel 2863, holly 4003,
  sapling 1033, giant 577, dead 32, fern 22256, bramble 723, rock 1402, stump 260, pine 1187 (tundra neck): 45,111;
  file 3.6 -> 4.8 MB.
- `WRWorldDressing.cpp`: harvest specs for every forest kind (appended last: old ledger keys unchanged); kinds table
  `ForestDressKinds` drawn from `WRTreeKit::Mesh` per variant (instance i = variant First + i % N, the ledger stride),
  trunk collision = hidden instanced cylinders per kit collider capsule (server too); giants and snags never culled and
  cast shadows (the skyline), trees 650 m, shrubs 350 m, ferns 100 m (x `world.decorationDistanceScale`); camera blocked
  only by giants and boulders. Forest kinds are skipped entirely (not built, not gatherable) until `forest` is in
  `world.liveBiomes`. Pines use the island pine meshes (no snow), falling back to kit alders, never shapes. Group
  `giant` never gives: the swing calls `WRForestHazards::RefuseFelling` (bounce + "The roots hold it.") and is spent;
  `FindNearest` with no group skips giants. Trees go through T4's `RefuseFelling` hook (NoFelling within 60 m).
- `WRTreeKit.cpp`: 8 compile fixes (use-before-declare `H_StranglerTop`, unused locals, a shadowed `N`, unity clashes
  `Rings`/`Moss`/`SlotNames` with other files' leaked namespaces); alder lean variants lean along +X (`bLeanX`).
- Materials: `Tools/art/make_tree_kit_material.py` (M_TreeKit, M_TreeKitLeaf, M_TreeKitGlow, instanced usage), lead runs
  it headless once. Until then the kit falls back to M_HeldTool (logged).

**Open / for other tracks.** `WRForestHazards::RefuseFelling` (T4) and `WRForest::NoFelling` (T1) must be defined or
the dressing fails to link. `Tools/terrain/dressing.mjs` keeps only storm kinds when the island is regenerated: it must
also keep `forest_*` (and `swamp_*`) rows (plan 2.4, T1). The Bower oak's foot hollow has trunk collision (not walk-in).
Orchards at farms are the scenery's (`Farmstead`, `SunkenOrchard`), not dressing.

---

## 3.T1 build notes (resume run, 2026-11-15, track T1 Land)

The land is a runtime **heightfield land** on Raven's WRLand system (no landscape import, no editor run).

- **Generator** `Tools/terrain/forest.mjs` (re-run 00:5x):
  - Organic sub-zone borders: a two-scale domain warp (`zoneWarp` :134), typically 80-150 m and up to ~250 m, fading out round the design-fixed places (Four Hearts, Bower, Clear-cut, Paper Hollow, Strangler Arch). The ground colour blends 45 m each way across them.
  - East coast `coastX` :76: ±80 m typical swing, five headlands, a round cove 60 m in front of each camp with points either side, never within ~48 m of the coast road. South coast `southY` :102: ±80 m with a soft floor at 4,660 m.
  - Dunes: two rows, blowouts, none on headlands. Headland shore rock: `headlandAt` :93, used in `base()`.
  - Fixed the kit under-fill: trail trees and giants no longer block POI spacing (`clearOf` :719). Kit 166 (thornbrake caches 0→4, house trees 1→8, fords 1→4). Widowmakers 4→26, perches 1→24.
  - Added the spot names the Hollow Root code reads (`hollow_dig/heart/squeeze`, `grotto_squeeze/room`, `barred_door`, `eel_trap`, :1223-1238). Existing keys are unchanged.
  - `writeLand()` :1344 writes `Data/lands/forest.json` and `Data/lands/forest_ground.bin` (1009² at 4 m, uint16 heights then 1 surface byte each, 2.9 MB):
    - zone grid, palette (plan 2.3 colours), surface classes (river bed, gravel bank, sand, dune, marsh, shore rock, road, trail, Hollow floor, hidden);
    - the frost band, the `underground` no-rescue list :1399, camps.
    - Hidden = under the island's own ground in the overlap strip.
- **WRLand heightfield mode** (`Source/Wildreach/WRLand.h/.cpp`; the wasteland path is untouched):
  - parse `WRLandHfImpl` :106; `HfHeight` :614;
  - `Contains` = frame + `ownFromY` 2,016 m :409;
  - `IsUnderground`/`InHole` :638/:794; `HfGround` colours :652;
  - `BuildHeightfieldChunk` :1106 (drops hidden triangles, no colour work on a dedicated server);
  - camp chunks built synchronously at BeginPlay :918; queue ordered from the camps;
  - `StepWet` :1209 (Rain on the `Wet` scalar);
  - `SpawnCamps` :1234 (PlayerStart tag `forest` facing inland, a 40 m safe zone per camp, forge/armorer/alchemy_table/workbench 25 m inland of camp 2), called from the subsystem :843;
  - WRLandSites is skipped for heightfield lands :994.
- **Ground rescue** `WRGameMode.cpp` ~:288: a heightfield land's ground counts (`WRLand::GroundUnder` :804). It never fires in the Hollow Root's `h` cells, the Sinkroot squeeze or the grotto back room, because `InHole` returns true there.
- **`WRForestTypes.cpp`** (new):
  - `FWRForestData::Get` :114, `ZoneAt`, `FindSite`;
  - `WRForest::Contains` :292, `ZoneAt` :302 (`h` via `WRForestHollowContains`), `Band` :314;
  - `Canopy` :324 (9-sample blend plus light gaps over places, arenas and the village);
  - `NoFelling` :364 (60 m from a giant's root crown; data `forest.noFellingRadius`).
- **Plumbing**:
  - `WRBiome::At` is already covered by `WRLand::BiomeAt` once the land file exists (no edit).
  - Tuning: `world.liveBiomes` + forest, `islandSouthEdge`, `landWetIn/DryOut/StartLift`, `ui.mapMin/mapSize/mapPixels` (-595800 / 1172800 / 2932).
  - `world.json biomes.forest`.
  - `WRWildlife.cpp`: forest zones in `NorthZone`; `IsNorthLand` includes the forest, so island kinds stay off and forest kinds keep to zones 1-5.
  - `WRSky.cpp`: ForestAir, canopy fog colour and density, sky light dimmed by canopy, forest grade.
  - `WRResourceLook.cpp`: tree-kit oak/beech wood, mossy rock.
  - `WRPortals.cpp`: stand-ins truffler/thornlynx/mossmaul, `FindSpot` uses the forest frame, the `SpotOk` ridge rule is island-only. `portals.json` forest stays `land:false`.
  - `WRWorldSystems.cpp DropMeteor`: forest frame.
  - `sea_mask.mjs`, new `forest_walk.mjs` (365² walk grid, Hollow Root cells `x`), `world_map.py` (2932 px), and `dressing.mjs` (keeps forest_*, swamp_*, forest and swamp pois).
- **Material**: `Wet` scalar added to M_LandGround in `Tools/terrain/build_void_materials.py`. Its default is 0, so the wasteland is unchanged. The lead must re-run it.

## 3.T3 build notes (resume run, 2026-11-15, track T3 World)

- **Scenery file merged**: `WRForestSceneryVillages.part2.tmp` (1,310 lines) appended to `WRForestSceneryVillages.cpp` (now 1,617 lines) and the .tmp deleted. There was no `@@PART2@@` cut left in `WRForestScenery.cpp`: it is only the dispatch. Every kind in the regenerated `forest.json` (sites + kit) has a builder; `cave_mouth` was missing and is now dispatched to `CaveMouth`.
- **Compile class of errors**: a scope-aware shadow scan of all seven files (C4456/7/8 against locals, params, lambda params, FBuild/actor/subsystem members and member functions) is clean. The brace-list loops that mix double/float were already `FVector2f` or float-cast. Every `forest.*` tuning read was renamed to `forestWorld.*`, so it can't collide with another track's `forest` block.
- **Runs on the runtime land**: `UWRForestWorld::OnWorldBeginPlay` only arms (the map + forest live + `forest.json` loaded). `Tick` → `StepBuild` waits for `WRForestShape::GroundReady` (the land with biome `forest` from `Data/lands`, `AWRLand::IsGroundReady`; with no land file it means a landscape map). Then it queues every place in a fixed order and builds `forestWorld.buildPerFrame` of them a frame on every machine. After that the server spawns the stateful things.
- **Ground**: `WRForestShape::GroundZ` is used by `FBuild::G` and by the server's node and cache placement. It traces and counts only the land's chunks and (asked for) the forest's own scenery. It looks through dressing trees and replicated actors, so a server holding a shrine or cave builds the same collision as a client that doesn't hold it yet. With no hit it falls back to `AWRLand::GroundZ`.
- **Shrines on what they stand on**: `WRForestShape::ShrineSpot` places the Falls Shrine on the falls pool's deck and the Canopy Shrine on its deck (from forest.json `deck`; `RowDeck` reads `deck` for shrine, kit and site rows). It is hooked into `WRWorldSystems::PlaceShrines` via world.json `"forest": true`. The Falls footing moves onto the pool platform and draws only puddles and the catch, so it no longer duplicates the platform's furniture.
- **Canopy Lift (R7)**: `LiftTopZ` gives the rise from the walkways that start at the lift's giant. `CanopyLiftFoot` builds the gallery round the trunk at that height: joists into the bark, knee braces, rail posts with rope, openings and iron rings where the walkways land, and an open wedge for the stone, the basket and the lift's landing. The machine and the gallery share one number.
- **Caves (R17)**: `AWRCave` handles the forest in `WRWorldDressing.cpp`:
  - the tint per theme, foxfire-green glow, no gas;
  - ores per DC 2.6, including the Root-Mine gem pocket (6 hits, 50%) and Sinkroot gem odds 0.2;
  - packs: Trufflers, Rootcoils, Mossmauls;
  - the floor check accepts land chunks.
- **Nodes (R18)**: thorns now at every wall plus one ring in eight (589 rings would have meant 589 replicated nodes). The rest is as before.
- **Wiring**:
  - `WRWorld::Prompt` / `ClientInteract` / `Command` (`world forest lift|offer`);
  - `UWRInventoryComponent::UseSlot` → `UWRForestWorld::UseItem` (snare stake);
  - Huntsman's crown in `AWRCreature::MountStaminaRates` (rider's head item + gear stat `mountSprintStamina`);
  - Verdant core gem stat `potionTameHeal` in the potion timer (your tames within 15 m healed).
- **Data**:
  - items.json: 41 rows (materials, drops incl. `lynx_fang`, `egg_bramblewasp`/`egg_hiveback`, gear, consumables, 7 lore);
  - recipes.json: 10 rows, quest-gated via `quest_<id>`;
  - gear.json: `bp_world_root_maul` (Sylvaroth) and `bp_heartwood_longbow` (in the Antler Barrow cache). `bp_heartwood_lodge_decor` is the Grovekin decor pack.
  - world.json: 5 shrines with guards, Rowan Ashby (`at` the Grove Circle; night move is `StepRowan`), `quests.rowan` ×6;
  - tuning: `forestWorld` block, `lights.foxfire_lantern`, `buffs.briar_balm`;
  - fx.json: 15 `forest.*` rows.
- **Not done**:
  - the heartwood shield (the game has no shield slot);
  - the Hermit's ring stats (`growthRegen`, `rootedSooner`) need T7 to read `GearStatFor`;
  - the briar balm buff needs T4 to read `WRStorm::HasBuff(.., briar_balm)`;
  - the Sturgeon hook has no fishing effect;
  - the Huntsman's crown and circlet have no head look;
  - `forest_lost_path` must be fired by T4 (Rowan quest 4).

## 3.T6 build notes (resume run, 2026-11-15, track T6 Bosses)

**Audit of the first run.** `WRForestArenas.cpp` was nearly whole (pieces, lobs, scenery, placement) but would not have
compiled: members it used were never declared (`HeartTrees`, `HeartGlowShown`, `Built`, `LastFallHours`,
`FWRForestBoss::LobLanded`), `Local()` called without its `FaBuild`, a `FCreatureSpawnAt` type that doesn't exist, a
local `FHitResult Hit` shadowing `AWRForestLob::Hit` (C4458), a missing `WRCombatLibrary.h` / `WRForestHazards.h`. All
fixed. `WRForestBosses.cpp` stopped after `Died()` (no ticks, no strike hooks, no counters) and called a `KindOf` that
didn't exist; `UWRForestBossBody` (the looks) had no file at all; no boss had a creatures.json / attacks.json / tuning row;
nothing in WRBoss / WRBossCombat / WRCombatLibrary / WRAbilities / WRProjectile called the forest hooks.

**What is built now.**
- `WRForestBosses.cpp` rewritten whole: the state per boss, every FWRForestBoss hook (idle, tick, pick, allowed, started,
  windup, strike begin/tick/end, modify, landed, incoming, damaged, parts, wall stun, phases, run-back, boss part, stance,
  herd damage, arena hits, bola, blight, lobs) and the five fights:
  - Old Gnarl: 4 plates of 400 (axes x1.5 into bark), stripped = pale wood (fire x2, burn buildup x2, burns 8 s), burning
    blocks regrow, two burning = 3 s stagger every 10 s, regrow forced 40 s after a strip (fire in its 1.5 s wind-up = 2 s
    stagger), the boulder (stump + root ball, 1,400 cm/s gravity arc from the hand, shadow from the throw, never inside
    8 m, a stump in the way stops it), the grasp (15/s 2.5 s, thrown 8 m, 100 by an ally frees).
  - The Bramblequeen: flies the hive's middle, clamped inside the shell between trunk and gallery edge, height follows her
    target; swarm call forced every 25 s while a comb lives (6 wasps from it, at the marked survivor first); all combs
    down x0.8; smoke x0.7 5 s; 3 clinging Hivebacks hold her 2 s; poison ticks x0.5; the sting lance is her own 6 m
    lunge in flight (a miss sticks 1.1 s); the pheromone mark marks the 6 m 60 deg cone 30 s; the wing gust pushes 4 m.
  - The Grey Matron: walkway graph (breadth-first over standing walkways), perches, patrols; canopy drop (shadow grows on
    the moss, she falls on the spot, punish, then climbs the nearest oak in view: a bound to the trunk and up the bark);
    the bridge leap (a swooping bound tree to tree dipping over the moss, its line shown on the ground, rakes what's under);
    arrows in her walkway make her leap; 4 of 6 down or her oak bare = down for good (x0.6, paw combos +1.2 s); her
    walkway falling under her drops her (she climbs back); kit screams at 60/30% (2 Thornlynx from the ferns, furthest tame
    first); the bell at night (F-S5) sends her out; walkways regrow 4 game hours after her death or the last fall;
    `grey_matron_walkways` fires for the earners when 4+ were cut by survivors; the larder (hides x10, `egg_thornlynx` =
    the kit, Grovekin jewellery).
  - The Hornwarden: his roam loop (forest.json route copied into his `roam` row for Raven's StepRoamingBosses) with rests at
    the Antler Barrow (11-13.5 h), Hanging Oak (18-19.5) and Herne's Ford (20-21.5); herd of 6 Elderelk; a blow on the herd
    from a survivor or tame (WRCombatLibrary hook) = 60 s enrage (x1.3 dmg, x1.2 speed, punish x0.7, bellow every 12 s);
    roots and bolas hold him 3 s (Entangle bIgnoreBossRule), stance x2 while held, 3 holds in the window = stance break;
    antlers 1,200 each (breaking one ends the toss; the crown half disappears); the Wild Hunt on the Bloom's night (herd
    10, x1.6, the hunt route skipping safe-zone points, he swings only at what's in reach unless held; killed in it: the
    second trophy and the Huntsman's crown). He starts/stops the Hunt himself from the Bloom + night if the hazards'
    event step hasn't.
  - Sylvaroth: rooted (body sunk 6 m, flying with no speed) in phase 1; 4 lines heal 0.15%/s each (x1.5 in the Bloom,
    none while Decayed); P1 moves; the Uprooting at 50% (unblockable 20 m ring, 60 + knockdown, lines dark 10 s); P2:
    knuckle charge that stops 6 m short of a heart-tree, hammer fists + a low 8 m ring, thrash x4 (claws parryable, a 12 m
    spin, low stomp), the vine sectors every 30 s (3 of 6 creep 1.5 s, live 12 s, 1 s standing = Entangled; fire
    hazards in a sector burn it clean 10 s), Re-root (6 s of line healing, kneeling); hidden P3: hearts wake (1,500 amber
    hearts, limb sweeps 15 m every 6 s with a 0.9 s lift), canopy shake every 20 s; Necromancy (a blow carrying decay) x1.5;
    the arms are parts of stance 70 (collapse 3 s); resin weeps for a day after the kill; the Verdant core part.
- `WRForestBossLook.cpp` (new): UWRForestBossBody: the tint over each rig and the code-built parts listed in its header
  comment (bark plates with pale wood under and fire on burning ones, the oak crown, mandibles/banded abdomen/beating
  wings, sabres and torn ear, the 8 m palmate crown with ivy and banner rags, the root lattice cage with foxfire and
  burials, burl head, lattice arms, canopy hump of tree-kit saplings and ferns, the root mass, the pods), and the vine
  sectors drawn as ground-following vine meshes per sector.
- `WRForestArenas.cpp`: combs re-laid from the floors (forest.json's comb points predate the galleries): each hangs on a
  paper stalk from the floor above with its middle at blade height; the Matron spawns out of the larder oak's hollow, not
  inside its trunk; the camp uses the `woodcutters_camp` site; the root hand slumps/sinks from its own state time.
- Hooks: WRBoss.cpp (begin, end, reset, phase, weakness, incoming, damaged, idle, tick, pick, part, died), WRBossCombat.cpp
  (allowed, started, strike, modify, landed, tick strike, wall stun (replacing a doubled Storm call), end, windup, part,
  run-back), WRCombatLibrary.cpp (stance x2, herd damage), WRAbilities.cpp (a swing on a rope anchor / knot / comb / heart /
  wrist; a torch in the off hand counts as fire), WRProjectile.cpp (arrows on the same; a bola holds the Hornwarden).
- Data: creatures.json 5 boss rows (+ a `_forest_bosses` note); attacks.json 5 boss row sets; tuning `bosses._forest`,
  `bosses.forest`, `bosses.<id>` x5; fx.json 56 rows (`gnarl.*`, `queen.*`, `matron.*`, `warden.*`, `sylvaroth.*`,
  `arena.*`) and impacts for three bosses.

**Decided here.**
- 3 holds "in 20 s" can't happen with the Nature path's 8 s entangle immunity (3 s hold + 8 s = 11 s between holds):
  the window is 24 s in tuning (`bosses.hornwarden.holdWindow`), noted for Raven in DESIGN_REVIEW terms.
- The larder's "kit" is an `egg_thornlynx` item (the incubator hatches it as a tame Thornlynx).
- Old Gnarl's arena chase is the mini rule's 65 m round the clear-cut's middle, not the whole 250 m field.

**Not finished / needs others.**
- Added the two drop items no track had: `egg_thornlynx` (the larder's kit) and `trophy_head_sylvaroth` (its first-kill
  drop) in items.json. `verdant_core`'s `potionTameHeal` gem key has no reader yet (T3's item; potion code).
- Lock-on to Root Knots: the knots are separate actors, so the lock-on (bodies only) can't target them; they're hit by
  swings and arrows.
- No Unreal command needed (no new assets: the looks use M_CreatureTint, M_BuildTinted blocks, M_HeldTool meshes and the
  tree kit).

## 3.T5 build notes (resume run, 2026-11-15, track T5 Creatures)

Audit at start: `WRForestCreatures.cpp` stopped at `UWRForestBody::TickThin` (681 lines: helpers, the mesh primitives,
the skin sampler, Attach/BeginPlay). Missing: the whole look, UWRForestTouch, WRForestGrip, the four WRForest wasp
contract calls, every FWRForestAI hook, UWRForestLife, AWRForestMark, the 11 creatures.json rows, every attacks.json
row, the spawn regions, tuning, fx rows, and every hook in shared code. All built now (file ~4,280 lines).

**Rig picks (checked against each rig's row and its art prompt):**

| Creature | Rig | Fit | Why |
|---|---|---|---|
| Mossmaul | Rimeclaw | hh 110 (2.2 m), radius 115 | long-legged maned body; the hump is modelled (a moss hill with cushions, fronds, shelf fungus), three 35 cm hooked claws per forearm |
| Thornlynx | Pale_hunter | hh 50, `look.flatten` 82 cm, `look.stretch` 1.2 | the wolf-leopard pressed into a low long cat; ear spikes, sabres, cloud blotches modelled |
| Rootcoil | Glacimaw | hh 40 x meshScale 1.9, flatten 60 cm, stretch 2.0 | the flat ringed worm squashed to a 60 cm body and doubled toward 12 m (the head becomes a viper's wedge); keel plates, moss saddles, wedge head plates, coils for the hold |
| Bramblewasp | Kilnmite | hh 14 x 0.45 (hit box 36 cm, body ~12 cm) | six-legged insect; orange head, mandibles, banded abdomen, stinger, two beating wing pairs modelled |
| Elderelk | Frostantler | hh 150 | palmate antlers 3.6 m across with ivy and moss, mane, pale rump modelled |
| Burlbeast | Emberhorn | hh 105 | bark-burl plates, white stockings, pale dark-tipped horns modelled |
| Truffler | Pumicehog | hh 38 | snout, tusks, bristle crest modelled |
| Hiveback | Glowmoth | hh 35 x 0.6, meshWiden 0.5 | narrowed moth wings under a modelled 1 m banded fur barrel, short beating wings, pollen baskets |
| Fernsheep | Woolly_drudge | hh 42 | as the Puffram; fern-curl fleece and amber horns modelled |
| Dappledoe | Frostantler | hh 68 | white spots, black-edged rump, flicking tail; buck antlers by sex (TameRep.Sex) |
| Glimmerfinch | Snowpuff | hh 10 x 0.8 | the lead bird; its flock (look.flock 9) modelled round it, each finch on its own orbit with beating wings |

**Ledger rows (file `Source/Wildreach/WRForestCreatures.cpp` unless named):**
- R25: 11 rows in `Data/creatures.json` (tier, pack, palette `look.tint`, size, taming, drops; `_forest` note row);
  looks `BuildLook` :950 (per kind :1097-1555), tint over M_CreatureTint, `ApplyFit` :898.
- R26: real rigs with their clips; pose layer `TickPose` :1708 (hang, climb, rear, log rest with clips paused and
  breathing, knocked down), moving parts `TickLook` :1597 (wings, flock orbits, tail flick, claws spreading, bloom).
- R27: `Data/attacks.json` creatures.mossmaul..dappledoe (cook 2.12.2 timings, flags, buildups, glints, `_note`s);
  contact only (generic shapes or this file's own: drop landing :3545 TickBody, body slam, roar ring); no projectiles.
- R28: roar + widowmakers `StrikeRow` :2845; hug / constrict `BeginHold` :3377 + `Damaged` :3047 (head 120 frees,
  strike x1.5, fire frees); bull rush trunk stun = row `wallStun`; canopy drop with growing shadow (`forest.cr_shadow`)
  `RowStarted` :2780 + `TickBody`; tree escape `StrikeRow` climb; log rest `TickWild` :2289; scout mark + no burst
  `WRForest::MarkForWasps` :1958; bugle `Rally` :2943; Burlbeast hyperarmor (row data `hyperarmor`); heat ball
  `Cling` :3476 + `UWRForestTouch::TickComponent` :1837 (max 4, -10% each via `WRForestGrip::MoveScale` :1935, roll
  40%, fire all, x3 on wasps `Incoming` :3015); belly x1.3 reared; Burlbeast back x0.8 strike.
- R29: `TryPathTame` :3227 / `FinishPathTame` :3301 / `PathTamePrompt`, hooked in `AWRCreature::Feed`,
  `WRPlayerMenus.cpp` WRInteract + `world fpathtame`, `WRPlayerController::InteractReleased`, `WRUIHud.cpp` prompt.
- R30: thin capsule `TickThin` :661 + `jumpHeight` 200 + `resists thornbrake`; Burlbeast log drag + `haulDrag`
  (TickBody), +25% heartwood is WRForestWorld's; Truffler truffles, caches, pit snorts (`AWRLeafPit::Reveal`), the
  Hollow Root dig through `WRForestHollow::DigSpot` / `TrufflerDigging`, seeds (TickBody); Hiveback stay-in-hive
  (`TickTame` :2644; WRFarm beekeeping) + heat ball; Glimmerfinch pollinate (`FinchPollinates` :3363 in `WRFarm.cpp`)
  and whistle sweep (`Whistled` :3323 from the whistle command).
- R31: `Data/spawn_regions.json` forest_f1..f5 (day `creatures`, `night`), pack radii.
- R32: `UWRForestLife` larders :4062, dusk perches over trails + doe rings :4089, Rootcoils across fords :4145,
  noon scout + raid an hour later :4173 (`WaspRaid` :2036, `TickRaid` :2588), finch signs over kills / ripe patches
  :4216, Rootgrave dusk fight gated on a `gravehound` row :4258. The Hornwarden's herd routine is T6's.
- R52: server-only rules; replicated `Flags`, `Spot`, `PathTamer`, `UWRForestTouch` (mark, clinging, held by); looks
  built on every drawing machine from the replicated state; owner-only `AWRForestMark`.

**Shared hooks:** `WRCreature.h` friend; `WRCreature.cpp` Attach, Died, TickWild, TickTame, Feed, TryPathMagic;
`WRCreatureCombat.cpp` Incoming, RowAllowed, RowStarted, ModifyHit, Resolved, AfterRowHit, StrikeRow;
`WRStormCreatures.cpp` OnDamageTaken chain; `WRCharacter.cpp` / `WRCharacterCombat.cpp` mash; `WRCharacterBase.cpp`
MoveScale; `WRPlayerMenus.cpp`, `WRPlayerController.cpp`, `WRUIHud.cpp`, `WRFarm.cpp`.

**Data:** tuning `creatureAI._forest` + forest / mossmaul / thornlynx / rootcoil / bramblewasp / hiveback / elderelk /
burlbeast / dappledoe / glimmerfinch; `tame.forest`; 20 `forest.cr_*` fx rows. Item ids used from T3's rows
(rootcoil_shed, elk_antler as the doe's shed antler, truffle, egg_hiveback, egg_bramblewasp).

**Not finished / to know:** `Data/farming.json` dappledoe's product is `shed_antler` (no such item; creatures.json
uses `elk_antler`); the paw swipe's second blow is held to `creatureAI.minWindup` 0.34 s by the loader (cook 0.15);
the Glacimaw's head is stretched with its body (an own sculpt stays on the art-PC list, as for Storm).


## 3.T4 build notes (resume run, 2026-11-15, track T4 Hazards, Hollow Root, Bloom)

**Audit.** `WRForestHazards.cpp` stopped at line 1680, right after `AWRForestWater::StepLogs`. Missing were: the Ring
pillar, `AWRElderFall`, the whole `UWRForestHazards` subsystem (nothing spawned any hazard), and ten `WRForestHazards::`
calls (Prompt, ClientInteract, Command, CropScale, GatherScale, Misty, RefuseFelling, BuildThrownModel, ThrownLanded,
IsHazardThing). `WRForestHollow.cpp` didn't exist. No hook was in shared code. Ring gaps ignored the data's `gap`.
Everything below is now written. Line numbers are `WRForestHazards.cpp` unless named.

- **Subsystem** `UWRForestHazards` :2384-2717.
  - `BuildLocal` (every machine): the rivers, the Ring pillar, then `WRForestHollow::BuildLocal`.
  - `SpawnServerActors` spawns from the raw forest.json rows:
    - brakes, with the generator's gap toward the nearest way;
    - the kit `thornbrake_cache` caches, topped up to 7 from the deepest closed F4/F5 rings;
    - widowmakers, leaf pits and wasp nests (bough or overhang);
    - then `WRForestHollow::SpawnServer`.
  - Ground comes from `WRLand::GroundUnder` (no traces before the land has collision).
- **R19 thornbrakes**, `StepPawns`. Players only; creatures ignore them.
  - Contact plays the scrape at once. After 0.6 s: the `Hazard.Thornbrake` tag (run x0.5 in `GetSpeedMultiplier`, the
    arms-up `block` clip in `WRCharacter`), and bleed 6/s.
  - Briar balm x0.3 via `WRStorm::HasBuff(briar_balm)`, the buff the item's `use.buff` grants.
  - Passes: Nature points, briarproof chest, Heartguard 4, an Elderelk mount.
  - A torch held 2 s burns a 4 m lane, open 5 game hours, half that in rain. Fire also burns lanes through
    `FireLanded`: projectile, fire blow and fire blade sweep.
  - Death or leaving the forest clears the tag.
- **R20 micro-hazards.**
  - Widowmaker / pit / nest actors were already written.
  - Melee sweeps (`WRAbilities.cpp`) and projectiles (`WRProjectile.cpp`) now reach them through `IsHazardThing` / `HitThing`.
  - Water or smoke clears the wasp mark.
  - Nest robbing works through Prompt and `world fhaz use`.
- **R16 spate and rivers.** `StepSpate`: rain for 3 min starts the spate; it holds for 30 min after the rain ends.
  `AWRForestWater` pushes 900 cm/s at fords and falls lips. The Drowned Root's pool belongs to the Hollow.
- **R21 weather.**
  - `weather.json` forest: Rain 0.3, Overcast 0.2, Morning mist 0.15 with `hours [5,10]` and vision 70 m. Thornlynx
    aggro is x1.5 in mist. Clear is the remaining 0.35.
  - New `FWRWeatherKind::Hours` (`WRSurvival.h/.cpp`): the mist starts only inside its window and is gone by the end.
  - `CropScale`: rain +25%, Bloom x2 (`WRFarm::GrowthRate`). `GatherScale`: resin +50% in mist (`FWRGathering::Gather`).
  - `StepRainLook`: canopy drips 20 s late.
- **R22 Bloom.**
  - `world.json events.bloom`: a game day; `signatureScale 1`, because heartwood x3 and honey x2 are on WRForestWorld's nodes.
  - `StepBloom`:
    - wild forest nodes respawn x0.5;
    - three `AWRElderFall`s;
    - the noon raid: `WRForest::WaspRaid` from the Paper Hollow to the 2 nearest beehive trees or player hives within
      1.5 km (none in safe zones);
    - the Wild Hunt from 21:00 to 04:00 through `WRForest::StartWildHunt`.
  - `AWRElderFall` :2074:
    - spot picker 150 m clear of camps, village, claims, shrines and arenas; never over the Hollow; nobody within 120 m;
    - a 5 s two-crack lean with birds lifting and a ground line;
    - a rigid-rod fall table from a replicated start;
    - a 90 m crush through `ApplyHit`;
    - a log you can climb, with 12 heartwood (axe) and 6 resin nodes, gone after a game day;
    - late joiners see it already down.
  - Warning: pollen haze from F4/F5 crowns (`StepWarningLook`). The Grovekin pillar on the Ring burns green
    (`AWRForestRingPillar`).
- **R18 no felling.** `AWRDressing::HarvestInFront` calls `RefuseFelling`, which uses T1's `WRForest::NoFelling`. The
  axe bounces with fx and the line "The roots hold it." (throttled).
- **Thrown jars.** `WRRanged`/`WRProjectile` know the kinds `blightjar` and `smokepot`. Code models are in
  `BuildThrownModel`. `ThrownLanded` calls T3's `BlightLanded` / `SmokePotLanded`.
- **R47 invasions.** `world.json` pairs forest->graveyard (leader hornwarden) and graveyard->forest (empty, filled by
  the Graveyard lane), through Rootgrave. They stay dormant until the Graveyard is live. `pays` gains `verdant_essence`.
- **R16 Hollow Root and R48 secrets.** All in `WRForestHollow.cpp`, ~2,750 lines, from a fork of this track.
  - **Rooms:** 14 rooms (`BuildRoom` :1073), roofs with walk slabs and root mats, foxfire lights at 12%, glowworm curtains.
  - **Ways in:**
    - Eldest's Door, behind the `eldests_door` ring;
    - the Sinkroot squeeze, roofed at 2.1 m over T1's new 2 m carve to the Iron Seam (`BuildWaysIn`);
    - the Truffler plug in T1's shaft at (-220,+3080) at the carved level, with its slide roofed into the Cradle Vault
      (`PlugSpot`, `BuildRoofs`). The creature track calls `WRForestHollow::TrufflerDigging(Truffler, Dt)`, `DigSpot`, `IsDugOpen`;
    - the one-way Heart Root door.
  - **Secrets:** F-S1 Lost Path (quest event `forest_lost_path` :2625), F-S2 the grotto back room, F-S3, F-S4 the
    barred door (torch 6 s or `FireAt`), F-S5 the Bell, the eel trap.
  - `WRForestHollowContains` gives `ZoneAt` its 'h' (T1 calls it).
- **Interaction.** `WRWorld::Prompt` / `ClientInteract` (`WRWorldNpc.cpp`) chain into `WRForestHazards`, which asks the
  Hollow first. `world fhaz ...` goes through `WRWorld::Command`. Dev: `fhaz spate`, `fhaz elderfall`.
- **Data.**
  - tuning `hazards.{thornbrake, widowmaker, leafPit, waspNest, spate, water, rain, mist, felling, bloom, hollow, secrets}`.
  - 22 `forest.*` fx rows in fx.json.

**Not done / owed by others.**
- Clear's light shafts and Overcast's darkening are T1's sky.
- Finches ringing the Hearts, Truffler x2 in rain, and Mossmaul/Rootcoil daytime rain hunts are T5's.
- Tracks fading x2 in rain: there is no tracks system.
- The invasion signs (moss trail, lanterns) wait for the Graveyard lane.

---

## 4. Spot-check and proof (finish run, 2026-11-15 05:xx)

**Integration.** The forest WIP (617 files) was committed locally and rebased onto origin's six new wasteland commits
(7485955). 59 conflict hunks resolved by hand: every boss/creature hook chain now calls Storm, Swamp, Void AND Forest
(`WRBoss.cpp` Begin/EndPlay/PhaseChanged/OwnWeakness/Incoming/Damaged/TickIdle/Tick/Died, `WRBossCombat.cpp`
Allowed/AttackStarted/BeginStrike/ModifyHit/HitLanded/TickStrike/EndStrike, `WRCreatureCombat.cpp`, `WRCreature.cpp`
Attach/Died/TickWild); `IsBuilt` passives list and `ClassForNode` keep both Void and Nature; thrown jars keep the
wasteland's stone/salt bag and the forest's blight jar/smoke pot; fire on hit lights the litter AND burns thornbrake;
`WRLand` keeps the wasteland's crust pits, `PlaceOn`/`GroundZAt` and the forest's heightfield `GroundUnder`;
shrines place by `land` (wasteland), `swamp`, or `forest`; HUD shows the Void and Growth gauges; glyph masks keep
`void_eye` and `leaf`; tuning merged 3-way with line endings normalised (Void and Nature path rows, abilities, boss rows,
lights `foxfire_lantern` beside `beacon_lantern`/`marens_lantern`, `forestWorld` beside the void blocks).
- Gate: **WildreachEditor compiled** (another project's open editor holds the shared Live Coding mutex; UBT run with
  `-NoHotReloadFromIDE`, which only skips that check). Link needed `MeshDescription` + `StaticMeshDescription` in
  `Wildreach.Build.cs` (the tree kit builds static meshes at runtime; the monolithic game target had hidden it). Full
  unity game target (`-DisableAdaptiveUnity`) compiled as well (packaging safety).

**Ledger proof** (files under `Source/Wildreach` unless named; per-track line proofs are in 3.T1-3.T8 above):
| # | Done | Where |
|---|---|---|
| R1-R2 | yes | sections 1-2 of this doc |
| R3 | yes, except the NOT list below | the build notes 3.T1-3.T8 |
| R4-R5, R8 | yes | this section; track audits in 3.T* |
| R6-R7 | yes | `WRForestWorld.cpp:462` AWRCanopyLift (replicated TripStart, counterweight, gallery `CanopyLiftFoot`), `WRForestScenery*.cpp` (every builder commented with its why) |
| R9-R12 | yes | `Data/lands/forest.json` + `forest_ground.bin`, `Tools/terrain/forest.mjs`, `WRForestTypes.cpp` ZoneAt/Band/Canopy, `WRLand.cpp` heightfield mode, `WRSky.cpp` forest air/canopy/overcast |
| R13 | yes | `WRForestSceneryLandmarks.cpp` (L2-L4, L8-L12), `WRForestArenas.cpp` (L1, L5-L7) |
| R14 | yes | `WRForestScenerySites.cpp`, `WRForestSceneryVillages.cpp`, `WRForestSceneryKit.cpp`; `Data/forest.json` sites + 166 kit |
| R15 | yes | `Tools/terrain/forest_dressing.mjs` -> 45k `forest_*` rows in `Data/island_dressing.json`; `WRWorldDressing.cpp` forest kinds; `WRTreeKit.cpp` |
| R16, R48 | yes (F-S1..F-S5 + eel trap; F-S6 as the dig) | `WRForestHollow.cpp` (14 rooms, 4 ways in, secrets) |
| R17 | yes | `WRWorldDressing.cpp` AWRCave forest themes/ores/packs |
| R18 | yes | `WRForestWorld.cpp` nodes; `WRForestHazards.cpp:1829` RefuseFelling; `WRForestTypes.cpp` NoFelling |
| R19-R20 | yes | `WRForestHazards.cpp:2731` StepPawns (thornbrakes), widowmaker/leaf pit/nest actors, `:2857` StepSpate |
| R21 | yes (tracks fading: no tracks system exists) | `Data/weather.json` forest, `WRSurvival` Hours window, `WRSky.cpp:301-572` overcast/shafts, creature rain rules `WRForestCreatures.cpp:2421-2496, 3841-3934` |
| R22 | yes | `WRForestHazards.cpp:2990` StepBloom, AWRElderFall, `world.json events.bloom`, finches ring the Hearts `WRForestCreatures.cpp:2575`, Wild Hunt `WRForestBosses.cpp` |
| R23-R24 | yes | `world.json` 5 forest shrines + `rowan` master + `quests.rowan` x6; `WRWorldSystems.cpp` forest shrine spot; `WRForestWorld.cpp:1377` StepRowan |
| R25-R32 | yes (stand-in rigs + code-built parts) | `WRForestCreatures.cpp`, `creatures.json`/`attacks.json`/`spawn_regions.json` forest rows (3.T5) |
| R33-R38 | yes | `WRForestBosses.cpp`, `WRForestBossLook.cpp`, `WRForestArenas.cpp`; boss rows (3.T6) |
| R39-R40 | yes | `Data/Audio` 444 OGG, `sounds.json` forest rows and pools, `WRAudioDirector.cpp` forest beds, `Docs/CREDITS.md` |
| R41-R42 | yes | `Data/paths/nature.json`, `WRNaturePath.cpp`, `WRAbilitiesNature.cpp`, `WRPlantSummon.cpp`, `combos.json` nature rows, `WRHeroLook.cpp` WRNature::ApplyLook |
| R43 | yes except the heartwood shield | items/recipes/gear forest rows; Hermit's ring read `WRNaturePath.cpp:464,483`; sturgeon hook `fish.json hooks`; Huntsman's crown look `WRHeroLook.cpp:291` |
| R44 | yes | Heartwood lodge set + Heartguard rows (earlier lanes, live with `forest` in liveBiomes) |
| R45 | yes | wild oat / Iðunn apple nodes `WRForestWorld.cpp`; `farming.json` |
| R46 | yes | lore items + site dressing (`WRForestScenerySites.cpp`) |
| R47 | yes (dormant) | `world.json` invasions forest<->graveyard |
| R49 | partly: forest portal stand-ins wired, `portals.json` forest `land:false` until a forest fold kit exists | `WRPortals.cpp` |
| R50 | yes | `WRLand.cpp` SpawnCamps (PlayerStarts tag `forest`, safe zones, stations) |
| R51-R53 | yes | data files above; unfinished stays hidden (below) |
| R54 | yes | compile only |
| R55-R56 | yes | commit on master, CHANGELOG + HANDOFF, LANDED line |

**NOT finished (hidden or noted):** heartwood shield (the game has no shield items yet: combat lane), forest portal fold
kit (portals in the forest stay off), creature bodies are retinted rigs with code-built parts (no sculpts), lock-on
can't target Sylvaroth's Root Knots (separate actors; swings and arrows hit them), tracks fading in rain (no tracks
system), not played or packaged.
