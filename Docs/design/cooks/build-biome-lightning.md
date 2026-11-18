# Build lane: Storm peaks (the Lightning biome) into the game

Lane **build-biome-lightning**, 2026-11-14. Builds `cooks/biome-lightning.md` (the design cook, all sections) into
Raven's repo `C:\Dev\Wildreach`, using `cooks/world-map.md` (frame, Crown, necks, weather), `cooks/combat.md` (row
schema, boss template) and `cooks/survival-crafting.md` (crops, sets, materials; most already in data from earlier lanes).
First biome built off the Old Island: this lane also builds the "second landmass" plumbing every later biome reuses.

Order of this doc (the cook rule): 0 ledger → 1 research for the build → 2 plan (design gaps solved, every step) →
3 build log → 4 spot-check against the plan and THE BAR → 5 what is NOT finished.

---

## 0. Requirement ledger

Brief = the lane brief; DC = design cook `biome-lightning.md` section; Q = an Aaron quote in the brief.
"Done where" is filled in at the end (section 4) with file:line or commit.

| # | Requirement | Source |
|---|---|---|
| R1 | Research how the best games solve this exact thing, written to cooks/ with the details used | Q 11-13 cook rule |
| R2 | Plan every detail before building (layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases) and list every build step | Q 11-13 |
| R3 | Build exactly the plan at full detail, no placeholders, no primitives, no "good enough" | Q 11-13 |
| R4 | Spot-check own work against the plan and THE BAR by reading code; fix before push | Q 11-13 |
| R5 | "Already done" only with file:line proof | Q 11-13 |
| R6 | Design with intention: every object/creature/mechanism has a why, what it connects to, how it meets ground/walls/water/player/light/sound | Q 11-14 |
| R7 | Machines look engineered; moving parts move smoothly on a believable mechanism (bridges, winches, bells) | Q 11-14 |
| R8 | Requirement ledger with proof per row; final message lists every requirement | Q 11-14 |
| R9 | The biome's land on the map: terrain in the continent frame (west half of B3, the Crown's west half, Stormgut west mouth), joined to the Old Island by the north-west neck | Brief; DC 2.1, world-map 2.2/2.3 |
| R10 | 5 sub-zones (Wind Passes, Cliff Paths, Rope Bridges, Struck Crags, Cliff Shrines) with own ground, palette, props, ambient bed, silhouette, one micro-hazard max | DC 2.2 |
| R11 | 10 skyline landmarks readable from 1 km, each with its night read | DC 2.3 |
| R12 | POIs every ~150-250 m: 29 hand sites + ~84 kit POIs, triangle rule, light or skyline shape at night | Brief; DC 2.4 |
| R13 | Decoration rule: full and alive, never cluttered; biome palette; trees/rocks/shrubs as harvestables | Brief; DC 2.1, THE BAR |
| R14 | Strike zones: above 600 m in a Thunderstorm, 3.0 s body pre-tell (hair lift, St Elmo's corona, buzz) + 1.0 s ground tell, on the spot, never homing; 60 dmg + 0.8 s stun; ground current on wet ground; pick weights (heavy ×1.25, Stormweave ×0.85); Grounded/charm/Stormweave-4/Rodcrest/salve/rod shadow counters; compass altitude line violet above 600 m in storms | DC 2.5 |
| R15 | Cliff-path gusts (1.0 s whistle tell, 900 cm/s push 0.6 s every 8-15 s toward the drop; brace ×0.3, crouch ×0.5, Galeback immune) | DC 2.5 |
| R16 | Rotten planks on rope bridges (1 in 12, never two in a row, darker, creak 1.0 s, break, repairable with wood ×2 for 2 game days) | DC 2.5 |
| R17 | Rockfall under overhangs (1.2 s trickle tell, 3 m, 45 dmg, knockdown; more in rain) | DC 2.5 |
| R18 | Ball lightning in storms (forms 1.5 s at a rod/crag, straight line 200 cm/s up to 20 s, never turns, pops 40 dmg + shock 60 in 3 m; can be popped from range) | DC 2.5 |
| R19 | Weather states: Clear 25, Overcast 20, Rain 15, Fog 10, Thunderstorm 30; Gale above 300 m in storms; Plenty after a storm | DC 2.6, world-map |
| R20 | What a Thunderstorm changes: strikes live, Raiju appear, Rodcrests climb rocks and take strikes, Voltfangs always charged, Stormkites fly the front, Puffram wool charged | DC 2.6 |
| R21 | Stormglass rules: 35% of strikes on sand/granite leave a node (3, pickaxe 2 hits) lasting 2 game h after the storm; fulgurite fields + Glass Scar refill after each storm; Rodfield rod feet; Great Storm ×3 | DC 2.6 |
| R22 | Other resources placed: copper (rich, ×1.3), tin, iron by depth, rock salt (Halite Cave), mountain pine/krummholz/struck pine, wild Stormrye/Gentian, storm essence drops, thunderstone, creature materials, three fish | DC 2.6 |
| R23 | Hidden sub-biome: the Hollow Spire (shaft, 4 ways in, 12 rooms, lore items, creatures, light rule, music) | DC 2.7 |
| R24 | 5 secrets (Broken Span, Bell Rod crown, behind the Copperfall, the Choir, the Thunderstone's hollow) | DC 2.8 |
| R25 | 9 creatures: Voltfang, Thunderhorn, Stormkite, Raiju, Cragstrider, Galeback, Coilburrow, Rodcrest, Puffram — tiers, taming, size, silhouette, palette, where by day/night | Brief; DC 2.9 |
| R26 | Real models where Fab/our packs allow, real animation (no gliding), readable silhouettes | Brief; THE BAR |
| R27 | Attack rows for all 9 (combat.md schema), damage only on visible contact, projectiles ≤1,400 cm/s never homing, not inside 4 m, tells with rising sound, packs aggro freely | Brief; DC 2.9 |
| R28 | Creature specials: Voltfang charge-up + storm charged + crackle-howl pull; Thunderhorn charge stuck on rock + reads action; Stormkite dive/darts/grounded after 3 wing hits; Raiju strike arrival / rebound leap / discharge curl / storm return; Galeback herd ring; Coilburrow nest swarm; Rodcrest/Puffram flee unless cornered | DC 2.9 |
| R29 | Living-world beats (Voltfangs hunt Puffram at dusk, Galeback ring, Rams' Ring clash at dusk, Stormkite snatch-drop, Raiju hunts in storms, Cragstrider alarm) | DC 2.9 |
| R30 | Drops for all 9 (butchered carcass pattern) and tame uses (Cragstrider 60° climb, Galeback no gust, Coilburrow ore sense + digging, Rodcrest strike ground, Voltfang shock bites, Thunderhorn charge, Stormkite scout mark, Raiju storm +20%) | DC 2.9 |
| R31 | Mini-boss the Bellwether: roaming 2.6 km loop, herd, 5-move table, counters (charge into rock 5 s stun, bleed blocks charge, breakable bell), trophy + extra | Brief; DC 2.10 |
| R32 | Mini-boss Galvara: Eyrie arena, 3 shootable cloud cores, air phase (rain of strikes, kite call, tail-lash pass, gust beat), drop on last core, ground phase, retake the sky | Brief; DC 2.10 |
| R33 | Mini-boss Coilback: Copperfall arena, underground ring pulses along copper veins, surface burst, star lash, claw sweep, scree spray, tunnel ridge; frost slows tunnel + rime line, nose part, metal targeting | Brief; DC 2.10 |
| R34 | Major boss Kharvox: Summit Nest arena (bowl, nest, 12 rim rods, ossuary slab, always-storm, run-back stone, altar); Charge bar shown on feathers; bleed/curse counters; P1 6 moves; Thunderclap transition (rods fall); P2 called strikes + rod shadow + storm pillar + delayed swipe + sky combo + double eye bolt; hidden P3 eye of the storm; wing parts; drops; altar subclass | Brief; DC 2.11 |
| R35 | Arenas for the boss and 3 minis, with run-back stones | Brief; DC 2.10/2.11 |
| R36 | Creature voices: 3 fused real calls + material layer per species, signature moments | Brief; DC 2.12 |
| R37 | Music: real compositions per cue (explore A/B/C, night, storm stem, combat, mini, boss P1-3, Hollow Spire, Monastery, Great Storm, camp, Stormgut duet), always audible, ducks ≤4 dB, credited | Brief; DC 2.12 |
| R38 | Ambient beds per sub-zone + Hollow Spire + Rodfield wind-song | DC 2.12 |
| R39 | 5 shrines (Great Shrine of Strikerock + 4 lesser) with guards, copper rod-tip braziers, fast travel | Brief; DC 2.13 |
| R40 | Path master Vesna Groza (look, scar, voice, day/night place) + 7 summit quests | DC 2.13 |
| R41 | Story told only by the environment (Rodwardens, feeding the sky, the struck were holy, the Coven war, Kharvox took the rods, the last keeper, the twelve, the newcomers) | DC 2.14 |
| R42 | Biome event: the Great Storm (warning, Front, Storm, Clearing, After; strikes everywhere with tells, Raiju, ball lightning, Choir opens, coils ×3, payout ×3; never in claims/camps) | Brief; DC 2.15 |
| R43 | Invasions Storm↔Swamp through Stormgut (Bellwether leads; invasion brings the storm) | DC 2.16 |
| R44 | Portal slots for this biome | Brief |
| R45 | Building set (Crag hold) and armour set (Stormweave) placed and obtainable here | Brief; DC 2.17 |
| R46 | Crops (Stormrye, Gentian) obtainable here; weather states; biome materials | Brief; DC 2.6/2.17 |
| R47 | Gear: grounding charm, grounding salve recipe, shock-resist potion, stormglass arrows, Rodwarden spear, storm essence, thunderstone, trophy talismans, Stormheart, Crown Plume | DC 2.17 |
| R48 | The Lightning path: 22 nodes, Charge resource, Grounded root passive, proc feedback, Spark never homes at players | DC 2.18 |
| R49 | Fire+Lightning and Frost+Lightning combos (Plasma, Meltdown, Brittle shock, Blizzard storm) | DC 2.18.4 |
| R50 | Lightning body change (Lichtenberg scars, iris ring, hair lift) + strike pre-tell drives the same hair lift | DC 2.18.5 |
| R51 | Everything data-driven the way Fire and Frost are (Data/*.json, tuning.json, attacks.json rows, ClassForNode, ApplyHit, WR_REGISTER_SCREEN with mouse and pad, sounds/fx as rows) | Brief |
| R52 | Server authority and replication for every new actor/system; multiplayer edge cases | Brief |
| R53 | Nothing unfinished shown as finished; unfinished stays hidden | Brief |
| R54 | Compile gate only; no tests/check scripts; one UE job at a time; assets headless with all six packs | Brief |
| R55 | Git: pull before/after, add by name, CHANGELOG "Unreleased" + HANDOFF in the same commit, no release/tag, LFS sizes reported | Brief |
| R56 | Append one line to LANDED.md; out-of-scope findings to LATER-COOKS.md | Brief |

---

## 1. Research for the build

The design cook's research (BotW thunderstorms, Horizon's cut random strikes, MH Wilds' storm apex, Zinogre's visible
charge, Stormbird parts, Lansseax colour law, real fulgurites, Lichtenberg figures, St Elmo's fire, stepped leaders,
lammergeier, sky burial, Raijū, Perun/Veles, thunder marks, bidental, Meteora) stays the source for what things are.
This section adds what the BUILD needs: how good games make mountains, routes, storms and stand-in creatures work in
an engine, and the exact choices taken from them.

### 1.1 Mountain terrain that reads as real

| Source | What it does | What we take |
|---|---|---|
| Ridged multifractal noise (Musgrave; ModDB heightmap tutorial https://www.moddb.com/news/procedural-heightmap-terrain-generation) | `1 - |noise|` octaves weighted by the previous octave give sharp crests and soft valleys: mountains, not hills | Crag detail on the Struck Crags and the Crown's shoulders is ridged fbm, amplitude by sub-zone (S4 40 m, S2 25 m, S1 6 m) |
| Leadwerks procedural terrain (https://www.leadwerks.com/community/blogs/entry/1124-procedural-terrain/) and the TU Delft survey (erosion is slow, hundreds of iterations) | Layered: macro shape, then fractal roughness, then thermal erosion | Macro shape is AUTHORED from the cook's landmark heights (anchor plateaus), then noise; thermal erosion is replaced by explicit talus aprons (scree fans under every cliff band), which is what thermal erosion produces and costs nothing |
| Breath of the Wild (Gamedeveloper "gravity to go forward" analysis) | Topography itself steers: you see the next goal from every crest; triangles hide and reveal | Every route bend and rise frames the next landmark; kit POIs sit on rises or round bends (the cook's triangle rule) |
| Ghost of Yōtei (Sucker Punch prototyped free climbing, shipped authored climbing routes each "meticulously crafted to deliver a payoff") | Authored routes beat free-for-all on cliffs | Cliff bands are unwalkable (>35°) on purpose; the Rodwardens' cut paths, stairs and bridges are the authored routes, carved into the heightfield as graded shelves (≤ 20°), each ending on a vista or a site |
| Real terraced cliffs (slate and granite step down in benches with talus) | Benches + risers + scree | S2 Cliff Paths: 70 m benches, risers 75-85° (heightfield cliffs, triplanar rock), 25 m scree fans at each riser foot |
| Meteora / Paro Taktsang / Hanging Temple | Buildings cling to a cliff on ledges; stairs and galleries link them | The Hanging Monastery is a cliff band on the Crown's west face with 4 carved ledges (680, 700, 780, 860 m) linked by stairs and chain walks |

### 1.2 Storms and lightning in games

| Source | Detail | Use |
|---|---|---|
| Thunder delay = distance / speed of sound (~3 s per km; thunder synthesis paper https://arxiv.org/pdf/2204.08026.pdf, Big Think) | Flash first, thunder after, quieter and lower with distance | `storm.thunder` plays per listener after d / 340 m/s, volume and low-pass by distance; close strikes play the crack at once |
| Stepped leader then return stroke (real lightning) | A faint branching channel crawls down, then the bright stroke | The strike FX draws a dim branched ribbon for the last 0.3 s of the ground tell, then the white stroke + flash light 0.15 s |
| BotW metal sparks before a strike | The tell is on YOUR body | Pre-tell: corona sparks on weapon tip and armour, hair lift, rising buzz — 3 s before the ground glow |
| Horizon cut random strikes as unfair | Never from nowhere | Only in a visible storm, only above 600 m (compass line), 4 s warning, always on a spot |

### 1.3 Stand-in creature bodies (the art PC isn't on this laptop)

The local AI art pipeline (`C:\Dev\WildreachArt`: SDXL + Hunyuan3D + Blender rig) is not installed on this PC, and
there is no Blender. Real models + real clips available: our 20 AI-pipeline creatures (each rigged with Idle/Walk/Run/
Attack/Hit/Death) and the Fab QuadrapedCreatures pack (Barghest, Griffon, MountainDragon with many clips). How shipped
games reuse bodies: Monster Hunter subspecies (same skeleton, new hide and silhouette parts), Dark Souls' reskins with
added horns/armour, Ark's variants. The silhouette changes come from scale, proportion, hide and added parts. So each
Storm peaks creature takes the closest real rig, a new material (palette from the cook), its own scale and speeds, and
code-modelled parts that make its silhouette (horn curls, the Bellwether's bell, the Rodcrest's sail, the Coilburrow
star, the Raiju's forked tail and fern glow, Kharvox's beard). Rows, voices, behaviour and drops are its own. Own
sculpts stay on the list for the art PC (section 5).

| Creature | Rig | Why that one |
|---|---|---|
| Voltfang | Cinderfang (wolf, long legs, deep chest) | Same anatomy as the cook's wolf; slate coat + white tips that glow |
| Thunderhorn | Emberhorn (bison-ram bulk, horns) | Heavy head-butting herbivore; code-modelled 1.5-turn curled horns |
| Bellwether | Emberhorn ×1.7 | The herd's old ram; triple-curl horns + bronze bell on a collar (bell swings on a spring) |
| Stormkite | Griffon (Fab, real flight clips) small | Feathered wings, hooked beak, flight set |
| Galvara | Griffon ×2.6 | The matriarch of the same kind |
| Raiju | Pale hunter (long low snow-cat) | Low, long, arched hunter; charcoal + Lichtenberg glow |
| Cragstrider | Ashstrider (long-legged climber) | Leggy, sure-footed; white coat, black dagger horns |
| Galeback | Woolly drudge (shaggy, humped hauler) | The yak shape: hair skirt, hump, head low |
| Coilburrow | Pumicehog (low digger) | Low, stocky; black velvet + pink star nose |
| Coilback | Rimeshell (armoured, plated) | Plated hump; copper/verdigris plates + star nose |
| Rodcrest | Magmaw ×0.45 (lizard) | Reptile gait; copper sail crest |
| Puffram | Woolly drudge ×0.4 (white) | Round wool body, curled horns, small legs |
| Kharvox | MountainDragon (Fab, real fly/dive/hover clips) ×3 | The only rig with a full flight set at that size; slate primaries + rust chest + bearded head parts |

---

## 2. Plan

### 2.1 Where the land goes (the frame and the join)

- **Frame (continent metres, +x east, +y south, origin = island centre):** a second landscape, **4,032 m square at
  2 m per sample** (2,017 samples, 16 × 16 components of 126 quads), from x −2,900 to +1,132 and y −5,936 to −1,904.
  Heights encode 16 units per metre (±2,048 m; the Spire is 1,650 m), Z scale 800.
- **Why 2 m:** the slopes are mountains (2 m reads fine under triplanar rock), it keeps the LFS cost near 20 MB
  instead of ~80 MB, and it streams in the same 2 × 2 component WP grid as the island.
- **The join (world-map 2.3 neck):** the island's landscape ends at y −2,016 and its north shore is at y ≈ −1,940 by
  x −400. The new landscape overlaps the island's top 112 m strip. In that strip the generator writes the island's own
  height minus 3 m wherever the island is land (hidden under it) and raises the **north-west neck** (x −700 to −250)
  above the island's seabed to a 5-25 m grassy saddle, so the neck meets the island's shore and climbs north into the
  Crown's south skirts. The Ash Sound (sea, 350-450 m) stays open west of the neck, as the cook says.
- **East of the Spine** (the Swamp's half, R2) stays sea until that lane: the Crown's east face drops into it.
- **North** of y −5,400 the Desert rib spur (200-350 m) with its two river gaps, then lowland toward the frame edge
  (the Desert lane extends it).

### 2.2 The heightfield (Tools/terrain/storm_peaks.mjs)

Built in layers, each with a job:
1. **Coast and land mask:** west coast x ≈ −2,600 ± 120 m noise with headlands (the Bell Rod headland at y −3,950) and
   three coves where the camps' wrecks lie; south shore y ≈ −2,400 (Ash Sound) west of the neck; east shore at the
   Spine's east foot. Sea floor −14 m.
2. **Trend:** height rises inland from the coast: 35 m sea cliffs, 0-300 m across S1 (700 m), 300-650 m across S2
   (700 m), 650-1,000 m in S4; the north gorge country 200-750 m; the Desert rib 250 m.
3. **The Crown:** a massif centred (−200, −3,300): the Spire horn (1,650 m, gaussian 170 m), the upper shoulder
   1,150 → 950 m, the **Ring terrace flattened to 900 m** on its south and west sides (radius 380-560 m), the Hanging
   Monastery cliff on its west face (900 → 650 m), and the Spine north (Stormgut saddle 420 m at (+200, −4,900); north
   spine 900 m) and south (shoulder 600 m at (0, −2,100), falling to the neck).
4. **Anchors:** every landmark and site of DC 2.3/2.4 with a cook height is an anchor plateau (core radius flat, smooth
   falloff), applied in order, so each site sits at its cook height on level ground where it needs to (camps, Rams'
   Ring meadow, the Rodfield, the Rust Pools ledge, the Summit Nest bowl 60 m across at 1,600 m with a 30 m rim).
5. **Terraces (S2):** 70 m benches with steep risers between u = 700 and 1,400 m inland; the riser foot gets a 25 m
   scree apron.
6. **Gorges (S3):** Thundergorge (Copperfall head at (−1,350, −3,950), north past the Bridge of Nine Spans, where it is
   a 340 m wide canyon with 8 rock pillars standing in it, then east to Stormgut), Kitecut (y ≈ −4,850, 50-70 m slot),
   Saltslot (y ≈ −5,180, 40-60 m slot); floors 150-250 m, rims 420-650 m.
7. **Needles:** Galvara's Eyrie (−1,250, −4,600), 760 m, flat oval top 70 × 45 m, sheer sides; the 8 bridge pillars;
   Strikerock tor (−1,880, −3,350) 280 m split by a 6 m notch.
8. **Detail:** ridged fbm by sub-zone, plus fine fbm; wind-flattened smooth coast.
9. **Routes carved last (the authored ways up):** each route is a polyline with heights; the generator cuts a graded
   shelf 8 m wide (stairs 4 m), ≤ 20° along its length, blending 10 m to the native surface, so routes always walk:
   - Coast road: camp 1 (−2,250, −2,700) → camp 2 (−2,300, −3,700) → camp 3 (−2,150, −4,700).
   - The rod-road (Vesna's quest 2): camp 2 → Strikerock → Goatherd's cave → Rams' Ring → Thunderstone Arch.
   - Southgap road: camp 1 → Bellwether's Fold → Arch; the Neck road: neck (−475, −2,150) → (−900, −2,550) → Fold.
   - Cliff path to the crags: Arch → Struck Climbers (600) → Rodfield (820) → Glass Scar foot (900) → Glass Scar
     Shrine (960); branch Rodfield → Monastery kitchens (680) [down] .
   - Monastery stair: kitchens 680 → Feeding Stones 700 → gallery 780 → Hanging Shrine 880 → the Ring 900.
   - The Last Stair: the Ring (−600, −3,450) → Rust Pools ledge (1,050) → spiral round the Spire's west and north →
     run-back stone (1,550) → the Summit Nest (1,600). Thunder marks every 10th step (kit).
   - North road: camp 3 → Windgap Shrine → Bridge of Nine Spans west tower (−1,500, −4,350, 420 m); east tower
     (−1,120) → Eyrie stair foot → Kitecut nests → Stormgut Fort (−600, −4,900) → the gut.
   - Copper road: bridge west tower → Copperfall bowl (500) → Copper Adit (780) → Rodfield.
10. **Outputs:** `Art/Terrain/storm_height.r16` (the landscape), `Data/storm_peaks.json` (sites with z, sub-zone
    polygons, routes, bridges, gust paths, overhangs, kit POIs, rod positions, arenas: what the runtime builds), and
    `Docs/terrain/storm_peaks.png` (shaded layout preview for the HANDOFF).

### 2.3 Landscape material (M_StormAuto)

Slope + height + sub-zone driven, triplanar on cliffs (no stretch on gorge walls):
- Flat low ground (S1): wind-flattened grey-green grass #7C8A6A with thrift dots; rye gold #C9B26A on terraces.
- Mid slopes: slate #3E4650 wet-black in rain #1F242A; scree #6E6F70 at cliff feet.
- High (> 600 m): granite #8A8D91 with black lichen and ochre lichen #B79A4A blotches; ice/snow in shadowed hollows
  above 1,200 m; fulgurite sheen #C9D4E8 where `GlassScar` mask (a world-position band) sits.
- Sand by the Desert rib (y < −5,250): pale sand with fulgurite glints.
Built by `Tools/terrain/build_storm_material.py` (commandlet) from the MW auto-material pack textures, same route as
`build_terrain_materials.py`.

### 2.4 Plumbing so a second landmass is a biome (from the code map)

| Piece | Change |
|---|---|
| `WRBiome::At` | Region test first: `Data/biomes.json` (new) lists land regions as polygons in metres with a biome; storm_peaks polygon covers the new land. Falls back to the ridge split on the island. One place, so fishing / weather / survival / farm / machines / HUD follow |
| `world.liveBiomes` | `volcanic,tundra,storm_peaks` |
| Wildlife | `side` generalised to a biome id; band for non-island biomes by `bandHeights` (lowland < 300 m, midland 250-750 m, highland > 600 m) from `biomes.json`; walk grid gains `originX/originY/width/height` (still reads the old square one) and is regenerated over the union frame |
| Sea and water | `sea_mask.mjs` reads both heightmaps over the union frame |
| Sky | a third biome look (storm: iron-dark clouds, cool grey light, violet flicker) blended by region distance |
| Audio director | biome weights per biome id; storm beds by sub-zone; storm music pools |
| Map tab | `map.png` redrawn over the union frame by `Tools/terrain/world_map.mjs`; `MapMin/MapSize` from data |
| Spawns | PlayerStarts tagged `storm_peaks` at the three camps (placed by the import script) + safe zones + starter stations |
| Main menu | path buttons built from the path registry (Fire, Frost, Lightning) |
| Streaming | `streamingRangeScale` 24 → 32 (8 km covers the island + Storm peaks from either end) |
| Portals, events, meteors, invasions | sample boxes from `biomes.json` regions instead of fixed squares; invasion mouths taken toward the biome centre |
| Resource / cave looks | storm look rows (struck pine, granite rock, slate tint) |

### 2.5 Runtime-built world (Raven's dressing pattern: data → actors at runtime, no map edits)

`UWRStormWorld` (new `WRStormWorld.cpp/.h`, a world subsystem, server spawns replicated actors, clients build looks)
reads `Data/storm_peaks.json` and places:
- **Landmarks (L2-L10)** as `AWRStormSite` actors with a `Kind`; each kind's look is code-modelled from the build kit
  (`WRLookKit`, tinted blocks, rods, discs, spikes) at real scale with materials from the palette, and each has its night
  light. L1 (the Spire) is terrain + the 12 rim rods + nest.
  - Bell Rod: 60 m tapered copper mast (verdigris), guy chains to 4 rock anchors, bronze bell-cages at 20/38/56 m,
    keeper's hut at its base, ladder to 20 m (broken above). Takes a strike every storm (a scripted strike every
    storm's 2nd minute on its tip), bells ring after.
  - Strikerock: terrain tor + glass faces (glass slabs on both faces of the notch), climbers' camp ropes and pitons,
    Vesna's lean-to, the Great Shrine brazier in the split.
  - Thunderstone Arch: a two-legged granite arch spanning the cliff path (legs anchored on the path's two sides), 8 m
    polished stone axe-head hanging from the keystone on two copper chains (chains are links of rods), offering hollow
    with small thunderstones beneath. ST-S5 niche.
  - Galvara's Eyrie: terrain needle + spiral stair cut outside (stone steps bolted with copper chain rail) + nest flat.
  - Bridge of Nine Spans: 9 spans of planks on two hand ropes and two foot ropes slung between 8 terrain pillars +
    2 rim towers; each pillar carries a winch hut (crank wheel + drum the foot ropes wrap); span 5 hangs broken down
    the wall (ST-S1 rope). Planks are individual collision boxes so a rotten plank can break.
  - Copperfall: a falling water sheet (translucent blue-green ribbon FX + spray), verdigris pools (emissive 0.3),
    half-buried mine props.
  - Glass Scar: 400 m glass groove of fused slabs in the terrain line, emissive after storms for 2 game hours.
  - Hanging Monastery: whitewashed cells (box rooms with timber balcony galleries on beams socketed into the cliff),
    stairs, chain walks, the Feeding Stones (6 slabs + iron hooks), the Melted Cell (open roof, burn decal), the
    kitchens (tables, bowls, oven, chain winch whose rope goes down the cliff to the kitchen path).
  - Storm Mill: stone tower, 4 torn sail frames on the cap, copper rod with melted tip.
- **Hand sites and kit POIs** (29 + ~84) the same way, each kind with variants (rod waymarks, bidental rings, fulgurite
  fields, outcrops, basking rocks, Puffram folds, tarns, waystations, short rope bridges, kite roosts, mound fields,
  kill set-pieces, struck trees).
- **Rods:** every rod (waymarks, Rodfield ~200, rim rods, fort towers) is one instanced copper rod; actors tagged so the
  strike picker finds "a standing rod within 3 m".
- **Hazard actors:** `AWRGustPath` (spline volumes along marked cliff paths), `AWRStormBridge` (planks with rotten
  slots), `AWRRockfall` (overhang spots), `AWRBallLightning` (spawned in storms at rods/crags).
- **Arenas:** creature spawners for the bosses, run-back stones, the altar, Kharvox's 12 `AWRArenaRod`s, Galvara's
  cloud, the Copperfall's copper-vein lines.
- **Shrines, Vesna, quests, events, invasions:** `world.json` rows (the world-systems frame), not code.

### 2.6 Creatures, bosses, path, events (data first, code where a mechanic is new)

- `creatures.json`: 9 creatures + Bellwether, Galvara, Coilback, Kharvox (rows per DC 2.9-2.11, rig per 1.3,
  `biome storm_peaks`, `resists [strike, shock]`, `activity storm` for the Raiju).
- `attacks.json`: every row in DC 2.9-2.11 in the existing schema (motion, windup, track, hold, active, punish, dmg,
  buildup shock, flags, tell bone + glint, weights, next).
- C++ for the new mechanics: Voltfang charge state; Raiju strike arrival / storm return; Thunderhorn/Bellwether stuck on
  rock (reuse wallStun); Stormkite grounded after 3 wing hits; Coilback underground state; Galvara cores; Kharvox charge,
  rods, called strikes, eye, bone drop; strike pre-tell, pick weights, rod shadow, ground current, fulgurite nodes.
- Lightning path: `Data/paths/lightning.json`, Charge attribute, Grounded, abilities in `WRAbilities` registered in
  `ClassForNode`, menu button.
- Great Storm: `world.json events.great_storm` + its step in the world systems.

### 2.7 Edge cases decided

- A player standing exactly on the island/storm seam: `WRBiome::At` decides by the region polygon (the neck belongs to
  Storm peaks north of y −2,016).
- Strikes never land in claims or within 50 m of a camp (Great Storm), never on a player in a safe zone.
- A Raiju's storm return never takes a tamed Raiju.
- Planks: a broken plank repairs itself after 2 game days if nobody repairs it; two adjacent planks are never rotten.
- Bosses: one strike rain per arena; rods that melt stay melted for that fight and stand again on reset.

### 2.8 Build order (each step compiled before the next)

1. Ledger, research, plan (this doc).
2. Terrain generator + preview; landscape material; import into L_Island (headless editor run, one UE job).
3. Plumbing: biomes.json + WRBiome::At, liveBiomes, wildlife regions/walk grid, sea mask, map, sky, audio weights,
   streaming, PlayerStarts/camps, menu path buttons.
4. Runtime world: storm_peaks.json reader, landmarks, sites, kit POIs, rods, dressing (trees/rocks/shrubs).
5. Strike hazard upgrades + gusts, planks, rockfall, ball lightning; fulgurite nodes.
6. Creatures (rows, attacks, looks, spawns, behaviours, drops, tame uses).
7. Minis + Kharvox + arenas + altar.
8. Shrines, Vesna, quests (world.json).
9. Lightning path.
10. Great Storm, invasion rows, portal slots.
11. Items/recipes; Hollow Spire; secrets.
12. Audio hookup (files from the audio pass), CHANGELOG/HANDOFF, compile, commit, push.

---

## 3. Build log

1. Doc (ledger, research, plan). 2. `Tools/terrain/storm_peaks.mjs` written; layout iterated on its preview three
times (flat inland plateau, the Crown's fade ring, an always-1,000 m horn floor, no east fall, hollow camp anchors, no
north coast: each fixed); landscape material `M_StormAuto`; headless import into L_Island (twice: the second with the
Eyrie's spiral stair, two more monastery ledges and the north/east coasts). 3. Plumbing (biome lookup, live biomes,
wildlife regions + grown walk map, sea mask, sky, audio, map picture and bounds, portals, meteors, invasion guard,
streaming 32). 4. `WRStormWorld` (scenery, rods, bridges, hazards, strikes, nodes, caches, caves, secrets, lights).
5. world.json rows, items, recipes, fx. 6. Two parallel tracks: the Lightning path, and the creatures/bosses (reports
folded in below). 7. Real audio sourced (42 MB, credited). 8. Compile: Result Succeeded.

## 4. Spot-check and ledger proof

Spot-checked by reading: strikes always land on the spot (`Land` uses `Call.Spot`, never the pawn); every hit goes
through `ApplyHit`; scenery is local on every machine, stateful things are server-spawned and replicated (bridges'
planks/lanterns, caches, nodes, ball lightning on the server clock); no projectile from this land homes. Fixed during the
check: ball lightning used the local clock on clients (now the server clock); a dead pre-tell hair lift (now a public
hero call); an unsigned `Abs`.

| # | Done where |
|---|---|
| R1-R2 | This doc, sections 1-2 |
| R3 | Sections 2-3; unfinished parts listed in 5, hidden where they'd show |
| R4 | Above |
| R5 | Every row here is file:line in Wildreach `fff` commit (see LANDED) |
| R6-R7 | Plan 2.2/2.5 (why each site is where it is: cook coordinates, routes carved to reach them); bridge planks hang on a sagging deck between towers with winch drums the foot ropes wrap (`WRStormWorld.cpp` AWRStormBridge::BuildLook); the Bell Rod's guy chains run to rock anchors; the kitchens' winch chain runs down the cliff |
| R8 | This table; final message |
| R9 | `Tools/terrain/storm_peaks.mjs`, `import_storm.py`, `build_storm.ps1`; `Landscape_StormPeaks` in L_Island; neck in the overlap strip (storm_peaks.mjs "the strip over the island's north shore") |
| R10 | Zone grid in `Data/storm_peaks.json`; beds per zone `WRAudioDirector.cpp` LayerWeight; material bands `build_terrain_materials.py` M_StormAuto; zone-filtered spawns `WRWildlife.cpp` InRegion |
| R11 | `WRStormWorld.cpp:663` (bell_rod) ... :867 monastery etc.; night/storm lights `StepLights` :2312 |
| R12 | 30 hand sites + 99 kit POIs placed by `storm_peaks.mjs` (150 m spacing, 125 m route fill, triangle rule); built by `AWRStormScenery::Build` :649 |
| R13 | Dressing kinds in `WRWorldDressing.cpp` (storm_pine, granite_rock, slate_scree, storm_shrub; harvestable); placement rule in storm_peaks.mjs dressing block |
| R14 | Picker `WRSurvival.cpp:532`; `CallStrike` :2136 / `Land` :2157; pre-tell `AWRStormState::MulticastPreTell` + `WRHeroLook.cpp:545`; compass line `WRUIHud.cpp:166`; Grounded/charm/salve `WRStormDetail::IsGrounded`; rods `NearestRod` |
| R15 | `StepGusts` :2343 (Galeback immune via `storm.gust.immune`) |
| R16 | `AWRStormBridge` (planks, creak/break :1753, `Mend`, prompt `WRWorldNpc.cpp`, command `WRWorldSystems.cpp:2314`) |
| R17 | `StepRockfall` :2401 |
| R18 | `AWRBallLightning` :1836, spawns `StepBalls` |
| R19 | `Data/weather.json` storm_peaks row (build-survival); Gale/wet effects in that row |
| R20 | Strikes (R14); Raiju in storms, Rodcrests, Voltfang charge, Stormkites, Puffram static: `WRStormCreatures.cpp` (creature track) |
| R21 | `Land` fulgurite 35%; `StepNodes` :2478 (fields, Rodfield, Glass Scar, 2 h after) |
| R22 | Outcrops (copper x1.3, tin) in `SpawnServerActors` :1946; caves `WRWorldDressing.cpp:1304`; rock salt look `WRResourceLook.cpp`; items in `items.json` |
| R23 | NOT built (section 5) |
| R24 | ST-S3 :2001, ST-S5 :2025; ST-S1/S2/S4 not built (section 5) |
| R25-R30 | `Data/creatures.json`, `attacks.json`, `spawn_regions.json`; `WRStormCreatures.cpp` (see the creature track's lines in the HANDOFF / report: Voltfang :674/:1316/:1747, Raiju :849/:1842/:318, Cragstrider :994/:209, Coilburrow :941/:275, beats :2111/:2133/:2172) |
| R31-R35 | `WRStormBosses.cpp` (Bellwether :684, Galvara :883, Coilback :992, Kharvox :1067, arenas :2293) |
| R36-R38 | `Data/Audio/sounds.json` creature families, events, beds; `Docs/CREDITS.md`; `WRAudioDirector.cpp:462` |
| R39 | `Data/world.json` shrines; storm look `WRWorldSites.cpp:268`; storm guards :469 |
| R40 | `world.json` masters/vesna + quests/vesna; `WRWorldSystems.cpp:1367` (event quests), `StepRodRoad` :2235, lanterns :1728, pre-tell quest in `Land` |
| R41 | Sites table (Feeding Stones, Torc lore, Melted Cell, Burnt Drake, fort hooks, Struck Climbers, Warden Door, nest) with lore items in caches |
| R42 | `world.json events.great_storm`; `WRStorm::GreatStorm`; picker intervals; ball lightning S5; layer music; nodes x3 |
| R43 | `world.json invasions.pairs` storm_peaks->swamp (dormant until the Swamp: guard in `StepInvasion`) |
| R44 | Portal row id fixed to storm_peaks, spot sampling `WRPortals.cpp:84`; fold kit NOT built (section 5) |
| R45 | Crag hold set / Stormweave already in data (build-building / build-crafting), now live with the land |
| R46 | Stormrye/Gentian wild seeds by biome (farming.json); weather (R19); materials (R22) |
| R47 | `items.json` / `recipes.json` rows; buff `WRInventory.cpp:1099`; Stormheart `WRLightningPath.cpp:353` |
| R48-R50 | Lightning path track: `Data/paths/lightning.json`, `WRLightningPath.*`, `WRAbilitiesLightning.*`, `WRCombos.*`, `Data/combos.json`, `WRPathLook.*`, `WRMainMenu.cpp` MakePathPicker |
| R51-R52 | Data files above; server spawns + replication as noted |
| R53 | Portal row stays `land: false`; Hollow Spire absent (nothing points at it) |
| R54-R56 | Compile only; one UE job at a time; commit/LANDED |

## 5. NOT finished

- **Creature bodies** are the nearest existing rigs retinted with code-built parts (no sculpts: the art PC with SDXL /
  Hunyuan3D / Blender isn't on this laptop). Many voice recordings are stand-ins for animals with no free recordings.
- **The Hollow Spire** (2.7: shaft, 12 rooms, 4 doors, the Choir) is not built; the Warden Door stands closed-dark.
- **Secrets** ST-S1 (no rope climbing), ST-S2 (no mast climbing), ST-S4 (needs the Spire) not built.
- **Portals** in Storm peaks need their fold kit (32 rooms, traps, trials, 5 portal bosses): the row stays `land: false`.
- Music: no fujara/cimbalom pieces in the libraries, not matched to 92 BPM / E Phrygian, Kharvox has two whole tracks
  rather than P1/P2/P3 sections.
- Vesna doesn't move her camp to the Ring after "The Crown"; Coilburrow/Stormkite marks are world beacons, not on the
  compass; the summit's own storm during the Kharvox fight isn't forced.
- Rodwarden thrust Wildbrand scroll (ST-S5) is a cache of essence and thunderstones instead (no cosmetic clip system).
- Tarns are drawn water, not fishable water (storm fish come from the sea there).
- Not played, not packaged.
