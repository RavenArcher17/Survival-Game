# Build cook: build-biome-graveyard (the Graveyard into Aaron's build)

Lane: build-biome-graveyard, desktop, 2026-11-15. Design source (SPEC for this lane, not re-decided): `cooks/biome-graveyard.md`
(read in full), plus `world-map.md` (B5 west of the Spine, the SW neck, Rootgrave), `combat.md` (attack-row schema, boss template,
decay), `survival-crafting.md` (corruption, lights, Rest, Gravecloth, crops, soul engine, bone altar), `fire-frost.md` (shrine /
master / event frame). Built on what landed: the void lane's land framework (`WRLand`, `Data/lands/`), and the forest lane's
heightfield land, scenery kit, creature bodies and boss arenas (the Graveyard's neighbour across Rootgrave).

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, the LANE NOTE, the LATER-COOKS lines naming this biome, and every section of
biome-graveyard.md. "Plan" points into this doc; "Proof" is filled at the end with file:line or a commit. No proof = not done.

| # | Requirement (source) | Plan | Proof |
|---|---|---|---|
| R1 | Full cook: research, plan, build, spot-check (brief "EVERY LANE IS A FULL COOK") | 1, 2, 3, 5 | |
| R2 | Research names real games, places, systems and why they work, plus real-world reference (brief) | 1 | |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases before building (brief) | 2 | |
| R4 | Every build step listed (brief) | 3 | |
| R5 | Design with intention: each object's job, what it connects to, how it meets ground, walls, water, player, light, sound (Aaron 11-14) | 2.3-2.6 | |
| R6 | Machines engineered (parts mesh, shafts lead somewhere); moving things move continuously on a believable mechanism, never snap (Aaron 11-14) | 2.6 (bell, lift, chandelier, crane) | |
| R7 | Requirement ledger, every row proven (Aaron 11-14) | 0, 5 | |
| R8 | Reuse the void lane's land/biome framework, no new parallel systems (LANE NOTE) | 2.1 | |
| R9 | Build the Graveyard links the void lane couldn't (Graveyard Riftborn; LATER-COOKS 173) | 2.11.6 | |
| R10 | The land on the map: terrain at B5 west of the Spine, joined to the island (SW neck) and the forest (Rootgrave) (brief; GY 2.1; world-map) | 2.1 | |
| R11 | 4 surface sub-zones G1-G4 + G5 underground, each with ground layers, palette, prop kits, silhouette, micro-hazard, ambient bed; 60-120 m blends; Volcanic and Void border kits (GY 2.2) | 2.2 | |
| R12 | 5 skyline landmarks L1-L5 (GY 2.3) | 2.4 | |
| R13 | POIs every ~150-250 m: 13 hand sites H1-H13, ~36 kit POIs by the 150/125 m rule (brief; GY 2.4) | 2.4 | |
| R14 | Decoration rule: grave rows on splines (2 m spacing, 4 styles, 8% leaning, 5% fallen, tōrō every 6th), lantern roads (tōrō every 30 m), yews / hawthorns / elms, asphodel, mortsafes (brief; GY 3.5) | 2.5 | |
| R15 | Three camps by castaway wrecks, safe zones, Necromancy spawns there (GY 2.1; world-map) | 2.4 | |
| R16 | Hidden sub-biome: the catacombs, 3 levels, entrances (6 crypt mouths, 2 subsidence pits, chapel crypt stair), 12 hand rooms, kit rooms, ore bands (GY 2.5) | 2.7 | |
| R17 | 5 secrets S1-S5 (GY 2.6) | 2.8 | |
| R18 | Resources: soulstone (fresh glow -> blue in air), dust, bone, yew, limestone, quicklime, grave iron, silver, wax, black rye, asphodel, star iron (GY 2.7) | 2.9 | |
| R19 | Crops black rye and asphodel placed and obtainable (brief; GY 2.7) | 2.9 | |
| R20 | Darkness and corruption: unlit double aggro, corruption steps, Necromancer buff, how it reads at 50/75/100 (GY 2.8) | 2.10 | |
| R21 | Micro-hazards: grave subsidence, lime pits, crypt draught, each with tell >= 0.9 s, each can kill (GY 2.8) | 2.10 | |
| R22 | Creatures resist their own hazards (GY 2.8) | 2.10 | |
| R23 | Weather: Clear 15 / Overcast 35 / Drizzle 15 / Ground mist 35, Corpse fog = night mist, Gravehounds rise from mist (brief; GY 2.9) | 2.11 | |
| R24 | Biome event: the Blood Moon (sign, Rising, processions, camp vigil, claim raids by setting, minis' event changes, dawn bone piles, the Mourning x3) (brief; GY 2.10) | 2.12 | |
| R25 | Shrines: Great Shrine of the Lantern + 4 lesser with guards, tōrō lighting, XP, fast travel (brief; GY 2.11) | 2.13 | |
| R26 | Path master Morwen, look, idle, lines, 6 quests (GY 2.11) | 2.13 | |
| R27 | Run-back stones 40-60 m from every arena door (GY 2.11) | 2.15 | |
| R28 | 9 creatures with real models where packs allow, real animation, behaviour, attacks with visible contact (brief; GY 2.12.1-2.12.2) | 2.14 | |
| R29 | Taming methods and utilities: Palestrider no-tire mount, Tombback hauler + slab cover + burying, Gloamcrow finder, Wickbeetle light, Marrowgoat farm, Gravehound path magic (GY 2.12.1) | 2.14 | |
| R30 | Day/night tables per sub-zone and living-world beats (GY 2.12.3) | 2.14 | |
| R31 | The Risen (mourner, hound, bearer): bone, no corpse, bone piles, strike x1.3 (GY 2.12.4) | 2.14.4 | |
| R32 | No zombies: bone, shade and carrion only (DD SPEC) | 2.14 | |
| R33 | 3+ mini bosses with arenas: the Bell-Ringer, Crypt Mother, Ossuary Lord, Gravedigger (roaming) with counters and drops (brief; GY 2.13) | 2.15 | |
| R34 | Major boss Nhal: chapel arena, tombs and sealing (Nature hits + heartwood stakes), P1, the Opening, P2, Lay to rest, hidden P3, drops (brief; GY 2.14) | 2.16 | |
| R35 | Creature and boss voices (GY 2.15) | 2.17 | |
| R36 | Music cues, the bell on F, planned silences (brief; GY 2.16) | 2.17 | |
| R37 | Sounds and fx as table rows (brief) | 2.17 | |
| R38 | The Necromancy path: Souls, Deathless, raising, 22 nodes, abilities + passives, proc feedback (GY 2.17) | 2.18 | |
| R39 | Necromancy combos with the paths that exist (GY 2.17) | 2.18 | |
| R40 | Body change for Necromancy (GY 2.18) | 2.18 | |
| R41 | Gravecloth armour set placed and obtainable (brief; GY 2.19) | 2.19 | |
| R42 | Building set (Mourning chapel) placed and obtainable (brief; GY 2.19) | 2.19 | |
| R43 | Graveyard items: heartwood stake, lime pot, grave lantern, mourning ring, yew longbow, palp-spine arrows, silvered, Sexton's pick, moth-dust tonic, Mourner's Scythe and Mourning chapel blueprints (GY 2.19) | 2.19 | |
| R44 | Story through the environment + 7 lore items (GY 2.20) | 2.4, 2.8 | |
| R45 | Invasions Nature <-> Necromancy through Rootgrave (GY 2.21; world.json rows waiting on this lane) | 2.12.4 | |
| R46 | Portal slots (brief) | 2.13.3 | |
| R47 | Data-driven like Fire and Frost: numbers in Data/*.json + tuning, abilities as UWRGameplayAbility + ClassForNode, all damage through ApplyHit, screens via WR_REGISTER_SCREEN with mouse and pad, sounds/fx as rows (brief) | all | |
| R48 | Server authority and replication on every new system; multiplayer edge cases (brief) | all | |
| R49 | No placeholders shown as finished; anything unfinished stays hidden (brief) | 4 | |
| R50 | Combat law: projectiles never home, >= 0.9 s warning, max 1,600 cm/s, nothing inside 4 m; crimson only on unblockables; visible contact; no attacker limit (combat KEPT; COMBAT-AND-LESSONS) | 2.14-2.16 | |
| R51 | Compile gate clean of new warnings; no tests run (brief) | 3 | |
| R52 | CHANGELOG Unreleased + HANDOFF in every commit; land in pieces, commit+push after each part; doc pushed early and at the end (brief) | 3 | |
| R53 | Watch LFS size; report new binary sizes (brief) | 3 | |
| R54 | LANDED line + LATER-COOKS lines (brief, desktop overrides) | 3 | |
| R55 | Heartwood stake needs heartwood in items.json (LATER-COOKS 42) | 2.19 | |
| R56 | Light registry that creatures/hazards query (Wraithmoth brightest light; draughts put out torches) (LATER-COOKS 43) | 2.10 | |
| R57 | The Risen as a shared archetype reusable by other raiser bosses (LATER-COOKS 44) | 2.14.4 | |
| R58 | Rift Storm border seed reaching Graveyard bases noted (LATER-COOKS 91) | 2.12 | |
| R59 | Raise-through-walls path raiding (tuning raiding.paths.necromancy) works once the tree exists (LATER-COOKS 165; DD) | 2.18 | |
| R60 | Necro raising + bone altar ritual for permanent undead tames (LATER-COOKS 153; SC 2.22) | 2.18.5 | |
| R61 | Graveyard is narrow: bands squeeze; no underground claims (world-map) | 2.1, 2.7 | |

---

## 1. Research (build-side; the design research is biome-graveyard.md section 1)

How the best games build these exact things, and what each gives this build.

| Game / system | How it's built, why it works | What this build takes |
|---|---|---|
| **Elden Ring catacombs** (Stormfoot, Black Knife, Minor Erdtree catacombs) | Each catacomb is a hand-assembled kit dungeon: one entrance stair, 6-10 rooms on a 4 m grid, a single gimmick per dungeon (fire jets, a lever, imps), a boss door with fog. Rooms are modular (corridor, cross, stair, hall) so a small team made ~20 of them | **The catacombs are a room graph in data** (rooms with size, floor depth, kind; corridors between door points), built from one stone kit in code (floor flags, coursed walls, barrel vaults, niches). **One idea per room** (GY 2.5 already says this) |
| **Valheim Burial Chambers / Sunken Crypts** (`DungeonGenerator`, `Room`, `RoomConnection`) | A dungeon is a seeded set of prefab rooms snapped at connection points, the entrance a stair from a surface "skull mound". Darkness inside makes torches the gear check; body piles spawn until broken | Crypt mouths are small surface buildings (a stair down under a stone hood). **Break the source**: Crypt Mother's sacs, Bell rope, Nhal's tombs, Blood Moon processions re-form only after 6 min |
| **Dark Souls, Tomb of Giants and Catacombs** | Total darkness unless you carry the Skull Lantern; skeletons re-form unless a divine weapon or the Necromancer is killed; the bonfire is deep inside | **Light is the gear check** (lanterns can't be blown out, torches can). **The Ossuary Lord re-forms** until you bind or scatter the pile |
| **Bloodborne, Hemwick Charnel Lane** | A short lane of graves, carts, lanterns, Hemwick witches who keep raising helpers; killing the witch stops the raising | The counter is shown before it is needed (the staked graves on the chapel road) |
| **V Rising, Unholy (Corpse Explosion, Raise skeletons)** | Raised units are short-lived summons that draw aggro; corpses are world objects | Raised minions are the creature itself (the AWRCreature body is reused), 30 s, `bRaised`, never count as tames |
| **Diablo IV Necromancer (Book of the Dead)** | Corpses are consumables; each skill spends them; minions take orders | Bodies stay down 3 min (built carcass state); raising consumes the body (no butcher after) |
| **Hollow Knight, Resting Grounds / Ghost of Tsushima (guiding wind)** | Lanterns and birds pull the player without UI | Tōrō lantern roads and Gloamcrows circling secrets |
| **Shadow of the Colossus / Elden Ring Godskin** | A tall slender boss reads by silhouette and stride; telegraphs are large body motions | Nhal moves on real stride clips (Ignessa rig, scaled), every tell a large body motion (scythe raised behind the shoulder) |
| **Minecraft/Valheim heightfield + prefab placement** | Terrain is data, props placed by seeded passes, only player changes saved | `Tools/terrain/graveyard.mjs` writes the heightfield and the content file; the game builds everything at runtime |

**Real-world reference used for details (the build's measurements):**
- **Churchyard layout:** Christian burials lie east-west with the head to the west so the dead face the rising sun; headstones stand
  at the head (west end). Rows are ~2.0-2.4 m apart with 0.9-1.2 m between graves; Victorian headstones 0.6-1.2 m tall, 8-15 cm
  thick, slate or limestone; kerbed plots 2.1 x 0.9 m; old stones lean back (the soil under the grave sinks, the head tips the
  stone). Lichens grow on the north side. -> rows run north-south, stones face east, 8% leaning back 6-14 degrees, 5% fallen flat.
- **Tōrō anatomy (kasuga-dōrō):** hōju (finial jewel), kasa (roof cap, curled corners), hibukuro (fire box, square with a window each
  side), chūdai (platform), sao (pillar), kiso (base); 1.5-2.5 m tall. -> the kit's tōrō is all six parts with a lit fire box.
- **Mortsafe:** an iron cage 1.9 x 0.8 x 1.1 m, bars 2.5 cm at 10 cm centres, a lid of crossed straps, sometimes over a stone slab;
  hired by families for 6 weeks until the body was "of no use to the anatomists" (Aberdeen). -> mortsafe kit row, prised one has a
  bent bar.
- **Lychgate:** a roofed timber gate (two posts each side, tie beam, rafters, clay tiles or stone slates) where the coffin rested;
  a coffin stone inside. -> the Lychgate Chapel's gate.
- **Gothic chapel:** bays ~5-6 m, clustered piers, pointed lancets 1.2 x 5 m, buttresses outside every bay, a stone rib vault (here
  only the ribs survive), an apse to the east, a west tower. -> Nhal's chapel: 70 x 26 m nave in 12 bays of 5.8 m (pillars every
  other bay = 8 pillars inside the 12 tomb bays), flying ribs, lancets.
- **Draw lime kiln:** a stone tower 5-8 m tall, square, with a pointed draw arch at the base where burnt lime was raked out, the pot
  (bowl) charged from the top by a ramp. -> the Lime Kilns.
- **Paris catacombs / Roman catacombs:** galleries 0.8-2 m wide and ~2 m high; loculi (niches) 0.4-0.6 m high, 1.8-2 m long, 4-6
  rows high; bones stacked as "hagues" (walls of femurs with skull bands). -> the catacomb kit's niche walls and skull bands.
- **Church bells for full-circle ringing:** a 2-tonne bell is ~1.4 m across, hung from a headstock on a wheel ~2 m across, the
  rope running round the wheel's groove and down through a hole to the ringing chamber, ending in a woollen sally. A bell swings
  through a full circle, so the rope travels ~1.5 m each stroke. -> the Belfry's bell hangs on a wheel; the rope runs from the
  wheel's groove down through the arch; each Toll swings the bell on its wheel (continuous rotation, never a snap).
- **English yew:** 15-20 m tall, girth to 10 m+, trunks hollow with age; dark needles; red arils; ancient churchyard yews. ->
  the Great Yew 30 m crown, 9 m trunk with a walk-in hollow.
- **Vivianite:** colourless when dug, blue in air within minutes to hours. -> soulstone nodes glow pale green; dropped/carried
  soulstone darkens blue-green (item icon tint and the bone-pile crust after dawn).
- **Chalk downland:** thin turf on white chalk; worn paths show bright white; sheep-grazed; low sea cliffs (Seven Sisters ~50-70 m,
  ours 10-25 m) of white chalk with flint bands. -> path surface class "chalk" (bright), cliffs white with dark flint lines.

---

## 2. Plan (every gap solved before building)

### 2.1 The land (the gap: where B5 west goes in today's build, how it meets the island, the forest and the Spine)

**Decision: a heightfield land, `Data/lands/graveyard.json` + `graveyard_ground.bin`, generated by
`Tools/terrain/graveyard.mjs`, at its real map metres** (game cm = map m x 100, exactly like the forest), the same `WRLand`
framework (R8). Frame: x -2,800..-150, y +1,900..+4,750 m, 4 m samples (713 x 713, about 1.5 MB, plain git, not LFS).

- **The Spine.** The crest and its heights are the forest's (copied from `forest.mjs` CREST / CREST_H, one source of the
  Spine's shape). East of the crest my samples are hidden (the forest's ground). West of the crest my ground falls from the crest
  to the Chapel Heights, never below the forest's west face (the generator reads `forest_ground.bin` and keeps
  `max(mine, forest + 0.4 m)` in the overlap, so no forest triangle shows through). The Spine's west face: 40-55 degrees,
  limestone grey with ledges (slope rock), unclimbable above 35 degrees (world-map hard wall).
- **Rootgrave opens.** The forest's rockfall plug (230 m) stays in the forest's file (it seals the pass whenever the Graveyard
  isn't live). New `cuts` rows on a land: `{ "land": "forest", "box": [x0,y0,x1,y1] }`: while this land is live, the other land
  drops its triangles inside the box (`WRLand` chunk build), and this land's samples there make the pass floor. The pass: a
  1,200 x 200 m corridor at the saddle (190 m) from the forest's Rootgrave road end (-930, +3,300, its own height read from the
  forest file at the seam so the two grounds meet exactly) west down to the Root-Split Tombs (-1,250, +3,380).
- **The SW neck** (world-map: x -700..-250, y +1,950..+2,350): land from the island's south shore along the Spine's west skirt;
  the island's own landscape wins where it exists (samples under it hidden, as the forest does); the neck road with smiths' slag
  stones and tōrō (both peoples' markers).
- **Overlap priority.** `WRLand::At` picks, among live lands containing a point, the one whose ground is not hidden there, and
  on ties the one with the higher `priority` (graveyard 1) so the Spine's west face reads as Graveyard.
- **Height shape (band rule, squeezed: the biome is 1.5 km wide):** crest (190-750 m) -> Spine foot 140-170 m at 200-300 m
  from the crest (Chapel Heights G4: limestone pavement terraces at 120-150 m) -> midland downs 45-110 m (G2 north, G3 south) ->
  lowland grave fields 12-40 m rolling (G1) -> white chalk sea cliffs 10-25 m at the west coast (a ragged coastline, coves at the
  three camps with shingle beaches and the castaway wrecks). The Great Yew's hill rises 14 m out of G1. The north coast faces the
  Bone Sound (the island's volcanic shore 350-450 m away) as low chalk cliffs; the south edge runs into the Void border.
- **Shaped features (the generator):** the chalk stream (H4 ford) running from Chapel Heights' foot west to the sea through G1
  (bed surface, 2-4 m wide, 1-2 m deep), the Lantern Pond (H11: 60 x 40 m basin, 1.2 m water, WRWater plane), lime pits (G3, 6-14
  m basins 0.8 m deep with a crust surface), the Bone Pits (5 open burial pits 20-30 m across, 4-6 m deep, bone scree surface),
  the subsidence pits under hazard cells (3 m deep, 4 m across, lidded at runtime), the Starfall shaft cap (sealed mound), crypt
  mouth holes (hidden samples 6 x 4 m at each crypt mouth's stair), chapel terrace (flatten 110 x 70 m at 140 m), the Belfry
  yard (flatten 50 m), camp terraces, roads (graded, chalk surface: bright white worn paths).
- **Surfaces (per sample):** turf (zone colour), chalk path #E6E1D3, grave soil #3B3530 (fresh-turned dark patches), asphodel
  meadow (white specks), flagstone (G2 streets), bone scree #D9CFB8, lime crust #EDEBE0, limestone pavement #B4AFA4, yew litter
  #2A2420, shingle, chalk cliff white with flint bands, stream bed.
- **Zone grid letters:** `1` G1 Grave Fields, `2` G2 Mausoleum Rows, `3` G3 Bone Pits, `4` G4 Chapel Heights, `s` Spine face,
  `n` the neck, `r` Rootgrave, `~` sea. Borders domain-warped (lobes 80-150 m) like the forest, pinned at the landmark sites.
- **Biome:** `world.liveBiomes` gains `graveyard`; `WRBiome::At` answers `graveyard` on the land; crops, fish, pieces, sets and
  recipes tagged graveyard come out of hiding.

### 2.2 Sub-zones (GY 2.2 as data)

Each zone row: id, letter, name, ground / groundAlt / speck, region (spawn table), bed (ambient row), decor density. G1 turf
grey-green #6E7A5E with asphodel white specks; G2 grey stone streets #8A8580 with moss #4F5B3A; G3 lime white #EDEBE0 and bone
#D9CFB8 with grey dust; G4 limestone #B4AFA4 with yew black-green litter. Blends 60-120 m by the domain warp. **Border kits:** G2's
north edge (y < +2,550) carries smiths' ash-graves (slag headstones, anvils) at half density; G3's south edge (y > +4,450) runs
grey: the ground desaturates over 150 m, grave rows stop mid-row, dug graves with no stones (H10).

### 2.3 Composition: why each place is where it is (R5)

The Graveyard is one story told as a walk from the sea to the Spine: **the castaways land on the west coast (camps) -> the
cenotaph fields (G1: everything is empty) -> the mausoleum streets and the Belfry (G2: the families, the bell that called the
dead) -> the bone pits (G3: the overflow, the watchers who tried to stop the robbing) -> the chapel heights (G4: Nhal still
mourning, the Stair everyone climbed)**. Roads tie it: the **Coast Road** (camps 1-2-3 along the lowland), the **Funeral Road**
(from the Lychgate Chapel east through the Row of the Named, past the Broken Procession, to the chapel), the **Watchers' Road**
(Bone Pits to the Watch House to the Funeral Road), the **Smiths' Road** (the SW neck down G2 to the Belfry), the **Rootgrave
Road** (Chapel Heights north-east up to the pass), the **Stair Road** (chapel to the Foot of the Stair). Every road is lit by
tōrō every 30 m (green grave-light that never goes out), so at night the roads read as dotted green lines from 1 km.

### 2.4 Places (GY 2.3, 2.4): every one built in code from the kit at real scale, with its night light

Landmarks (each read from 1 km by silhouette):
- **L1 the Chapel of Nhal** (-1,500, +3,800): 70 x 26 m nave of 12 bays (5.8 m), west tower broken and leaning 8 degrees, transept
  46 m wide at bays 9-10, apse to the east (radius 9 m) with the lantern rack and the Necromancy altar; buttresses every bay; bare
  rib arches across the open roof; lancet windows (glow green at night from the lanterns inside); 12 floor tombs, 8 piers, broken
  pews. Built by the boss track (it's the arena).
- **L2 the Great Yew** (-2,300, +3,150): a 14 m chalk hill; trunk 9 m across fluted and split open on the east into a walk-in
  hollow 4 m wide; crown a black-green dome 30 m across (tree-kit "yew" built from the tree kit's broadleaf crown with yew colours);
  roots gripping a ring of 9 headstones lifted off the ground; bones in the root crotches; Gloamcrow roost.
- **L3 the Belfry** (-1,800, +2,900): a 35 m square tower (7 m footprint), 4 stages with string courses, an open belfry at the top
  with 4 pointed openings, the great bronze bell (1.6 m mouth) on its headstock and wheel (2.2 m), the rope from the wheel's groove
  down the tower's inside to the arch at its base, the frayed section 4 m up glowing; walled yard 50 m across, 16 grave mounds,
  6 tall headstones (cover).
- **L4 the Bone Giant of Rootgrave** (-1,300, +3,250): a 40 m skeleton slumped against the Spine (skull 5 m, ribcage 10 m tall
  with a forest of roots through it), lantern-poles driven through its bones like spears.
- **L5 the Lantern Stair** (-1,200, +4,150): a zig-zag stone stair cut into the Spine face, 1.6 m wide, tōrō both sides every
  12 m, climbing to 700 m where it breaks off in mid-air; the last 50 m of lanterns unlit; the one unlit lantern on the edge facing
  up (secret S3). The stair is walkable (steps 0.3 m on a collision ramp) to the break.

Hand sites H1-H13 at the cook's coordinates (each built to its table line: the Lychgate Chapel with Morwen's lantern rack, the
Row of the Named's 120 lantern-topped stones on a 12 x 10 grid with name tags, the Dog Graves with collars and two graves pushed up
from below, the Ford of Washing on the chalk stream, Smith's Row with slag stones, hammers and an anvil, the Seven Names mausoleum
with six filled plates, the blank one and the chisel, the Broken Procession's cart and dropped poles pointing east, the Watch House
3-storey tower with barred door and tally wall and mortsafes stacked, the Lime Kilns with three draw kilns, barrows and the lime
pit, Where the Rows End, the Lantern Pond covered in floating lanterns, the Root-Split Tombs with coffins lifted on roots, the
Foot of the Stair gate with the sandal pile).

Kit POIs (36) placed by the generator's seeded pass (150 m from any POI; any road point more than 125 m from every POI gets one):
mausoleum (6, 4 variants), mortsafe cluster (5: 3/5/9 cages, one prised), crypt mouth (6: these are the catacomb entrances),
soulstone outcrop (5), predator on a kill (3), wayside tōrō shrine (4: coins, doll, bowl, hair), burned cottage + ergot field (1),
asphodel meadow with a half-built bone altar (1), ossuary kitchen (1), bone cairn (2), collapsed subsidence pit with a rope (2).

Camps (world-map): (-2,450, +2,750), (-2,450, +3,500), (-2,350, +4,300), each by a castaway wreck on its cove's shingle (a
broken-backed hull of the newcomers' build kit), a PlayerStart tagged `graveyard`, a safe zone 40 m, starter stations at the
middle one (beside the Lychgate Chapel).

### 2.5 Decoration (between places)

- **Grave rows:** generator rows (north-south polylines in G1/G2/G3, 2 m spacing along the row, 2.2 m between rows), each stone
  a style (slab 45%, cross-ring 20%, tōrō-topped 15%, wooden marker 20%), size 0.6-1.2 m, 8% leaning back 6-14 degrees, 5% fallen;
  a tōrō every 6th stone; kerbs on 30%. Drawn as instanced look blocks per row (one draw per material), culled at 250 m.
  Collision: one low box per stone (players walk between rows).
- **Lantern roads:** a tōrō every 30 m along every road (both sides alternate), each lit at night by the shared grave-light (one
  point light per lantern within 120 m of the viewer, the rest emissive only).
- **Trees:** churchyard yews (G1 and G4, by chapels and walls), leaning grey hawthorns (windswept east, G1/G3), dead elms (G3),
  as island_dressing-style instances (`graveyard_*` rows) from the tree kit; asphodel clumps and grass in G1. No forest.
- **Walls:** dry-stone walls along field edges (0.9 m, coursed rubble with a cope), iron railings round family plots in G2.

### 2.6 Machines and moving things (R6)

- **The Belfry bell:** headstock + wheel on gudgeons in a timber frame; it swings through a full circle on each Toll on a
  server-timed curve (replicated toll start time), the rope paying out round the wheel's groove (rope length on the ground side =
  wheel angle x radius). Never snaps; when the rope is cut the bell swings down to rest and stops.
- **The chandelier (Chandelier Hall):** hangs on an iron chain over a pulley in the vault; when its chain breaks it falls on a
  gravity curve from the replicated break time (no teleport) and lies broken.
- **Crane at the Bone Pits:** a timber jib crane with a rope and a cradle of bones; the cradle sways on a time function.
- **The Gravedigger's walk:** his route is a spline; he walks it on real locomotion (AI), stopping at dig points to play the dig.

### 2.7 The catacombs (GY 2.5)

A room graph in `Data/graveyard.json` `catacombs`: rooms {id, level, kind, at (map m), size (m), floor depth (m below the
ground there), yaw} and links {a, b, width}. Built in code on every machine (collision on all, look on clients) from one stone
kit: flagstone floors, coursed limestone walls, barrel vaults (rib every 2 m), loculi niche walls (4-6 rows, lanterns in some),
skull bands, quarry pick-marked faces in L3. Entrances: 6 crypt mouths (a stone hood over a stair cut down through a hidden
patch of the heightfield, the stair's walls built down to the room), 2 subsidence pits (rope down), the chapel crypt stair (sealed
until Nhal's first kill on the server). `underground.ways` mark every room and corridor so the game mode never "rescues" a body
from under the ground. The build: L1 Crypt Gates (C1 Watchers' Vault, C2 Niches of the Unnamed, C12 Chapel Crypt, kit rooms),
L2 Ossuary (C4 Skull Wall of the Wheel, C5 Chandelier Hall, C6 Bone River, C7 Sorting Room, kit rooms), L3 Quarry (C8 Starfall
Pit, C9 Brood Gallery, C10 Three Cisterns, C11 Quarry Face), joined by C3 the Draught Stair (L1 -> L2) and a quarry ramp (L2 ->
L3). Kit rooms by the cook's mix (niche corridors, family vaults, grave seams, silver veins, skull-wall puzzles, Wickbeetle nests,
Cryptstalker lairs, flooded stairs, squeezes, cistern banks, draught corridors). Ore bands as resource nodes per level. No claims
underground (the claim totem refuses inside a room box).

### 2.8 Secrets and lore (GY 2.6, 2.20)

S1 grave-slab in the Yew's hollow (dig 3 s with a pick: coffin cache: Yew longbow blueprint + First Lantern), S2 the one full
grave in the Row of the Named (a dig spot; a tamed Gloamcrow lands on it), S3 the unlit lantern at the Stair's break (hold E 3 s
with a lit torch or lantern: every Stair tōrō turns gold for a game day, corruption doesn't build on the Stair, 1,000 path XP
once), S4 the seventh name (bring the chisel, hold 5 s: the seventh crypt opens, Sexton's pick), S5 the drowned bell's air pocket
(C10; a dry chamber: Bell-founder's mark, soulstone 8, shards 4). Lore items picked up at their sites (E): Lantern tag,
Washerwoman's bat, Watcher's tally, Ossuary ledger, First Lantern, Bell-founder's tally (Bell-Ringer drop), Bell-founder's mark.

### 2.9 Resources and crops (GY 2.7)

Server resource nodes (replicated, harvested with the existing `AWRResourceNode`): soulstone outcrops (kit) and grave seams
(catacombs; pick, iron+ for seams), bone piles (hand), yews (axe: 6 yew logs, 2 game days), limestone (pick: Spine foot and L3),
mortsafes (pick/hammer, 4 hits -> grave iron 2), silver veins (L2), Wickbeetle nests (wax, hand), black rye and asphodel wild
patches (hand, sickle), star iron (C8, weekly). Node looks: soulstone (pale-green glowing crystal crust on a bone bed), bone pile,
mortsafe (the cage itself), yew. Quicklime from a lime kiln piece (workbench 2) or the Lime Kilns' barrows.

### 2.10 Darkness, corruption, hazards (GY 2.8)

- Corruption and darkness are built (`WRSurvival`, `weather.json graveyard corruption.path necromancy`); this lane adds the
  reading: colour drain at 50 (post-process saturation x0.85 at the edges), whispers at 75 (sound row `graveyard.whispers`), at 100
  tōrō within 15 m flicker as you pass (client: lanterns near a Marked player dip).
- **Light registry (R56):** the survival lane's `UWRLightSubsystem` is the registry; this lane adds a query
  `BrightestNear(At, R)` (held torches/lanterns, placed lights, tame auras) for the Wraithmoth, and `Extinguish(Area)` for held
  torches and campfires in a draught.
- **Grave subsidence (G1):** hazard cells (12% of grave-soil cells) are pre-carved 3 m pits under a turf lid actor (collision +
  look). A player stepping on a lid: 1.0 s tell (crack ring decal, chalk dust, crunch), then the lid breaks (replicated), you drop
  3 m; 50% of pits hold a sleeping Gravehound den (2-3 wake). The lid re-forms after 10 game min when nobody's inside. A tamed
  Gloamcrow screams 1 s before you step on one.
- **Lime pits (G3):** basins with a slurry surface; standing in one: 0.9 s crackle/hiss tell, then 8 damage/s through ApplyHit
  (x2 in drizzle) and armour durability -1%/s. Plank bridges over two of them.
- **Crypt draught (C3 and 2 kit corridors):** a volume; every 8-15 s a 1.2 s rising whistle then a gust that puts out held
  torches and campfires inside (lanterns, Wickbeetles, grave-light fine); you're unlit until you relight (hold E at a flame 1 s).
- Every Graveyard creature ignores corruption, lime and subsidence (creatures.json `resists`).

### 2.11 Weather (GY 2.9)

`weather.json graveyard` becomes: overcast 0.35, drizzle 0.15 (torches burn half as long, lime x2), ground mist 0.35 (day: knee
mist, night: Corpse fog chest-high, every light x0.7, Gravehounds may rise from any mist cell within 40 m), clear the rest.
`nightOnly` corpsefog row folds into the ground-mist row as its night form (`night` sub-block). Weather never stops the music.

**2.11.6 Graveyard Riftborn (R9):** the Void's Riftborn roll (UWRVoidBody::RollRiftborn) extended to Gravehounds, Wraithmoths
and Tombbacks spawned within 250 m of the Void border (y > +4,450): 15% Riftborn (desaturated, violet eyes, curse 15).

### 2.12 The Blood Moon (GY 2.10) and invasions (GY 2.21)

A world-systems event row `bloodmoon` (biome graveyard, one game day, ~every 3 real days, the Biome events setting): -5 min sign
(rust-red sky #8A2A1A and a red moon over the chapel, every bell tolls once, music layer), 20:00-05:00 the Rising (mist forced,
corruption x2, processions of 4-8 Risen behind a lantern-bearer walk the roads at 2 m/s toward the nearest light within 600 m, one
per 150 m of road in G1/G3, re-forming 6 min after destroyed; they stand vigil at camp safe-zone edges; they attack lit claims as a
creature raid through `WRRaids` only if the server's Creature raids setting allows and never on PvE), the Gravedigger's two extra
stops, Bell-Ringer tolls every 15 s, Necromancer Souls x2 and raises x2, dawn: every Risen collapses into a bone pile (bone 4,
soulstone 1-3), the Mourning to day's end (mist till noon, seams x3). **Invasions** (world.json rows the forest left waiting):
Graveyard -> Forest led by the Gravedigger (2 Gravehound packs, 2-3 Wraithmoths, a Cryptstalker, 4 Risen mourners), dusk start
(waits in the Root-Split Tombs until 20:00); Forest -> Graveyard comes out of the Root-Split Tombs, sealing graves on its path (no
Gravehound rising in a 20 m band for a game day). Rift Storm seed at (-1,600, +4,620) is on this land now (R58: its storm rift
opens on the Graveyard's south edge; noted in HANDOFF).

### 2.13 Shrines, Morwen, quests, portals (GY 2.11)

- world.json `biomes.graveyard` (path necromancy, signature soulstone, essence grave_essence, waymark toro, palette), 5 shrines
  at the cook's coordinates (`land: graveyard`), lighting = a tōrō lantern (the shrine dais kit's graveyard variant).
- Morwen (master, Lychgate Chapel; day: trimming wicks at the lantern rack; dusk: carrying a lantern to the Row of the Named;
  dawn: on the lychgate step), lines, her 6 quests in the existing kinds (event `toro_lit` x5, kill+event `raised`, event
  `mortsafe_broken` x6, event `yew_grave`, bring `ossuary_ledger`, kill `gravedigger` + bring `diggers_lantern`).
- **2.13.3 Portals:** `portals.json biomes.graveyard` gets its land (`land: true`) only if a graveyard portal room kit exists;
  it doesn't (LATER-COOKS 49), so the slot row stays hidden and the gap is logged (R46).

### 2.14 Creatures (GY 2.12)

Stand-in rigs from our packs (no Graveyard sculpts exist; ComfyUI is off-limits), real clips from those rigs, the cook palette as
a tint (M_CreatureTint) and silhouette parts modelled in code on bone frames (the forest's `UWRForestBody` pattern as
`UWRGraveBody`):

| Creature | Rig | Parts built on it |
|---|---|---|
| Gravehound | Barghest (lean canine, real Fab clips) | bone-pale skull face plates, knuckled spine ridge, rib lines, soul-green eyeshine |
| Cryptstalker | Kilnmite scaled to 4 m span, flattened | two 5 m whip legs (14-segment sweeps that fan on the sweep), spined raptorial palps, limestone mottle |
| Wraithmoth | Glowmoth scaled to 2.6 m span | swept ash-brown wings with bark mottle, bruise-violet underwing, the skull mark on the thorax, coiled proboscis |
| Palestrider | Frostantler (tall gaunt quadruped) | antlers hidden by a bone-pale skull face shell, ragged black mane ribbons and tail |
| Tombback | Basaltback (domed body) | black elytra with rust-orange bands, the grave slab cemented on its back (worn name, moss) |
| Gloamcrow | Griffon scaled to 1.3 m span | black body, ash-grey hood, silver eyes |
| Mournhare | Snowpuff | long black-tipped ears, grey coat |
| Wickbeetle | Kilnmite scaled to 50 cm | waxy white dome with drips, candle-yellow tail light (a real point light) |
| Marrowgoat | Woolly drudge scaled to 0.9 m | chalk-white long hair, back-swept horns, a bone in its mouth |
| Risen mourner | Pale hunter (Mixamo humanoid, real clips) | the body rendered as bone (tint), ribcage/skull/pelvis parts, grave wrappings, iron bands, tōrō lantern pole |
| Risen hound | Barghest | bone parts only (ribs, skull, spine), grave-light in sockets |
| Risen bearer | Grimjaw | two fused skeleton parts and a coffin carried as a ram |

Attack rows in `attacks.json` from GY 2.12.2 / 2.12.4 (motions the runner has: bite, lunge, sweep, slam, grab, spray, call;
new hooks in code: Rising (from buried), Grim howl (wakes buried), Whip sweep `found`, Palp snatch pin to wall, Ceiling drop (in
rooms with a ceiling), Light dive (brightest light, snuffs torch), Scale dust (chilled), Tear-drinker (downed only), Death scream
(herd rally), Slab slam / carrion spray, Mob dive (crow funeral), Grave grasp (low)). Day/night spawn tables per sub-zone as
dens (`spawn_regions.json` rows with `land: graveyard, zone: Gn`) + G5/L2/L3 tables in the catacombs. Living-world beats in
`UWRGraveLife` (hound packs run down hares at dusk, Tombbacks bury kills, crow funerals, moths on lanterns, Palestriders on the
downs). Gravehounds by day are buried (hidden, a soil mound) and rise at dusk or on a Grim howl; in Corpse fog they rise from mist.

### 2.15 Minis (GY 2.13) with arenas and run-back stones

Rows in creatures.json (biome graveyard, boss), attacks.json, tuning `bosses.<id>`; code in `WRGraveBosses.cpp`:
- **Bell-Ringer** (Grimjaw scaled to 4.2 m, bell-helm, hand-bell, long arms): the Toll raises 3 Risen mourners from yard
  graves (max 9), the rope is a part (400 HP; slash/fire x2) and when cut the bell falls silent, stance 90 -> 60, Hand-bell ring.
- **Crypt Mother** (Kilnmite at 9 m span, brood cloak): 8 egg sacs (150 HP, fire x3, torch-hold 2 s) hatching 2 young / 30 s,
  Brood shed 6 young, fire on her back kills riders.
- **Ossuary Lord** (Obsidian warden, 6 m, bone tint, skull crown, 3 keystone skulls): re-forms in 8 s at 50% unless a Nature root
  lands on the pile, 2 lime pots break on it, or all 3 keystones in the pile are smashed; the chandelier chain (300 HP) drops the
  chandelier on him for 400 and a 4 s stance break, once.
- **Gravedigger** (Pale hunter, 5.2 m, horse-skull + sheet, mortsafe cage on his back, spade, lantern): sleeps in the Watch House
  by day, walks the night route with 6 dig stops; Bury (allies dig you out 1 s), Mortsafe grab (cage door 300 HP), the lantern
  part (150 HP: blind 10 s, relights once).
Drops: shards x4 + trophy (Bell clapper, Mother's fang, Ossuary crown, Gravedigger's spade) + the cook's extras.

### 2.16 Nhal (GY 2.14)

Ignessa rig scaled to 9 m (female, real clips), the moth-veil as a layered cone of moth cards over her (code mesh, flutter on
a time function), lantern-horn crown, the 8 m scythe; 12 `AWRGraveTomb`s (closed / open / sealed), sealing by a Nature-element
hit within 2 m or a heartwood stake (hold E 1.5 s); P1 six moves + Lay to rest; the Opening at 50% (lanterns out, 14 m
unblockable ring, tombs open, the veil lifts to the roof ribs, dark arena); P2 raises a Risen every 25 s per open tomb (max 8),
two delayed moves and Requiem; hidden P3 at 25% behind the artifact flag (the 40-shade procession ring with gaps, reversing on
the bell's 10 s toll, Nhal's shade). Drops: Nhal's core, soulstone 12, scales 10, silver 6, Mourner's lantern (20%, first kill
always), trophy head, Mourner's Scythe + Mourning chapel blueprints. The altar (subclass) in the apse. Run-back stone at
(-1,555, +3,800). The chapel crypt stair opens after the first kill.

### 2.17 Sound and music (GY 2.15, 2.16)

`sounds.json` rows for every creature and boss voice (tell, strike, hit, pain, idle, aggro, death) and the world (bell toll on F,
spade at 40 m, subsidence crunch, lime hiss, draught whistle, tōrō hum, crow funeral silence), sub-zone beds G1-G5 and the deep
catacomb bed, music pools (explore day/night, catacombs, combat, mini, Nhal P1-P3, Blood Moon layer, camp, Rootgrave duet).
Recordings: real CC0 / CC-BY recordings where the repo's audio pipeline can fetch them (the forest lane's
`Tools/audio/build_*_audio.py` route); where none exist, the row points at the nearest existing family pitched (the void lane's
rule) and the gap is logged (composed cues are a later cook).

### 2.18 The Necromancy path (GY 2.17-2.18)

`Data/paths/necromancy.json` (22 nodes, trunk + Horde / Reaper / Lich, gates as the cook), Souls on the path component (max 20 +5
a Soul capacity rank; gains by tier within 30 m for a credited kill; lost on death; HUD row of bone-green flames), Deathless
(root, main path only: a killing blow leaves 1 HP once per Rest; ash flash + toll), raising (`AWRCreature::RaiseFromBody`: a body
under 3 min old and not butchered stands up at 60% for 30 s on your side, `bRaised`, never a tame, max 3 / 5, crumbles to bones),
abilities `UWRGA_RaiseDead`, `UWRGA_CorpseExplosion`, `UWRGA_ArmyOfTheDead`, `UWRGA_ShadowBolt`, `UWRGA_CurseOfWeakness`,
`UWRGA_LifeDrain`, `UWRGA_LichForm`, `UWRGA_GraveStrike`, `UWRGA_ScytheSweep` in `ClassForNode`; passives in ApplyHit (Shared
pain, Soul shield, Final harvest, Soul siphon, Reaping stride, Death mark, Harvest, Wither chance); combos with Fire, Frost,
Lightning, Blood and Nature (`combos.json`); body change rows in `player_art.json paths.necromancy`; raise-through-walls raiding
reads the tree (R59). **2.18.5 the bone altar ritual** (permanent undead tames: Souls 10/25/60, soulstone 3, asphodel 2, 60 s
channel, counts as a tame, 1 soulstone dust a real day): a `bone_altar` piece and its use.

### 2.19 Gear and building (GY 2.19)

Gravecloth exists in gear.json (biome graveyard) and the Mourning chapel set in building_sets.json: both become obtainable once
`graveyard` is live (their recipes and material sources exist: soulstone, Mournhare fur, linen, chapel stone). New items and
recipes: heartwood stake (heartwood exists from the forest lane), lime pot (thrown, ballistic), grave lantern (lantern row in
`lights`), mourning ring (found), yew longbow (blueprint), palp-spine arrows, silvered extra, Sexton's pick (found), moth-dust
tonic, Mourner's Scythe and Mourning chapel blueprints, lime kiln + tōrō pieces, the 7 lore items, creature parts, trophies.

---

## 3. Build steps (in order; each compiled, committed and pushed before the next)

1. This doc (pushed).
2. Land: `graveyard.mjs` -> `Data/lands/graveyard.json`, `graveyard_ground.bin`, `Data/graveyard.json`; WRLand `priority` and
   `cuts`; liveBiomes; camps; biome lookups (wildlife, sky, audio zone beds).
3. World: `WRGraveTypes` (content parse), `WRGraveWorld` (subsystem, scenery queue, server nodes, tombs, lore pickups),
   `WRGraveScenery*` (kit, landmarks, hand sites, kit POIs, grave rows, tōrō roads, walls, wrecks).
4. Items, recipes, pieces, resources, world.json (biome, shrines, Morwen, quests), crops/gear/building live.
5. Hazards: subsidence lids, lime pits, draughts, torch extinguish, light registry query, weather rows; Graveyard Riftborn.
6. Creatures: 9 + Risen rows, attacks, `UWRGraveBody` looks and hooks, dens and tables, living-world beats.
7. Catacombs: room graph, kit build, entrances, ore nodes, secrets S5, rooms C1-C12.
8. Minis and arenas; Nhal and the chapel.
9. Necromancy path (souls, Deathless, raising, abilities, passives, HUD, combos, body change, bone altar).
10. Blood Moon, invasions, audio rows and pools.
11. Spot-check (5), CHANGELOG / HANDOFF, LANDED, LATER-COOKS.

## 4. Hidden until finished (no placeholders)

Anything unfinished stays hidden: data rows carry `live: false` / `waitsFor`, bosses not built don't spawn, nodes without an
ability show "[not built yet]" in the Paths menu as Raven's rule already does.

## 5. Spot-check and proof

Landed on Wildreach master in seven pieces: f15b0b1 (1) land, places, catacombs; 349acfd (2) items, recipes, node looks,
shrines, Morwen; 5d3fc1e (3) creatures and the Risen; fdce5e7 (4) hazards, weather, air, Riftborn; f754b90 (5) the five
bosses, arenas, machines, lime pots, fx/sound rows; cc79e40 (6) the Necromancy path; 7f0f3f7 (7) the Blood Moon, the
invasion, beds and music pools. Each compiled (editor target, no new warnings); nothing was played here (Aaron tests).
Spot-checked by reading the code against this plan and THE BAR; fixes made before each push are in the commits (e.g.
piece 5: the undefined helper for a tame's owner, The Opening forced at phase 2, the lantern swing off while it's
smashed; piece 5 also found that pieces 1-4 called 40+ fx and sound names with no rows, so they had none: all added).

| # | Done | Where (Wildreach repo) |
|---|---|---|
| R1-R4 | yes | sections 1-3 of this doc; built pieces 1-7 |
| R5 | yes | each place / piece commented with its job in code (WRGraveScenery*, WRGraveBossLook.cpp header comments) |
| R6 | yes | WRGraveBossLook.cpp TickLook: bell + wheel turn on the headstock, rope sally rises with the wheel; chandelier pendulum + gravity fall from FellAt; WRGraveScenery.cpp:878 Bone Pits crane; Gravedigger walks his route (WRGraveBosses.cpp TickDigger) |
| R7 | yes | this table |
| R8 | yes | WRLand priority/cuts (f15b0b1), Data/lands/graveyard.json |
| R9 | yes | fdce5e7: UWRGraveBody::RollRiftborn |
| R10-R14 | yes | f15b0b1: Tools/terrain/graveyard.mjs, WRGraveScenery*.cpp, Data/graveyard.json |
| R15 | yes | f15b0b1: camps with wrecks, safe zones, PlayerStarts |
| R16 | yes | f15b0b1: WRGraveCatacombs.cpp (96 rooms, entrances, the chapel crypt opened by OpenChapelCrypt on Nhal's first kill) |
| R17, R44 | yes | f15b0b1/349acfd: AWRGraveSpot (S1-S5, keepsakes), lore items |
| R18-R19 | yes | f15b0b1/349acfd: resource nodes, node looks; black rye / asphodel in farming.json, live with liveBiomes graveyard |
| R20-R22, R56 | yes | fdce5e7: WRGraveHazards.cpp, WRSky corruption read-outs, UWRLightSubsystem::BrightestNear, resists rows |
| R23 | yes | fdce5e7: weather.json graveyard rows |
| R24 | partly | 7f0f3f7 WRGraveEvents.cpp: sign, Rising, processions, vigil, re-form, dawn piles, corruption x2, Souls x2 / raises x2; f754b90 minis' Blood Moon changes. NOT built: processions raiding claims, the Mourning's seams x3 |
| R25-R26 | yes | 349acfd: world.json shrines, Morwen + 6 quests (the Gravedigger quest's diggers_lantern now drops: f754b90 creatures.json) |
| R27 | yes | f754b90: FWRGraveBoss::PlaceRunback (data runback rows + catacomb hall doors) |
| R28-R32, R57 | yes | 5d3fc1e: WRGraveCreatures.*, the Risen archetype (creatures.json "risen") |
| R33 | yes | f754b90: WRGraveBosses.cpp (Bell-Ringer, Crypt Mother, Ossuary Lord, Gravedigger), WRGraveBossLook.cpp, WRGraveArenas.cpp |
| R34 | yes | f754b90: Nhal (tombs, sealing by Nature / stakes, P1, the Opening, P2 raises, hidden P3 Procession, drops, altar in the apse) |
| R35-R37 | stand-in | f754b90 + cc79e40 + 7f0f3f7: every voice / world / proc sound and fx is a row (sounds.json grave.* / necro.*, fx.json grave.* / necro.*, creature voice families, beds, pools); the sounds play existing recordings pitched (_standin). NOT built: Graveyard recordings and the composed score |
| R38-R40 | yes | cc79e40: Data/paths/necromancy.json, WRNecroPath.*, WRAbilitiesNecro.*, HUD Souls row (WRUIHud.cpp SoulsRow), body change (player_art row + WRNecro::ApplyLook) |
| R39 | partly | cc79e40: Ash wraith, Funeral pyre, Preserved, Frozen legion, Storm lich (Shadow bolt), Death curse, Cycle of life. NOT built: Jolt of life, Blood bond, Rot and bloom (combos.json text says so) |
| R41-R43, R55 | yes | 349acfd: gear.json gravecloth, building_sets mourning_chapel, items/recipes (heartwood stake recipe), lime pot thrown (f754b90) |
| R45 | partly | 7f0f3f7: Graveyard -> Forest filled (Gravedigger leads); Forest -> Graveyard row already live. NOT built: the invasion signs and the sealed band |
| R46 | hidden by plan | portals.json graveyard land:false (no graveyard portal kit: LATER-COOKS 49) |
| R47-R48 | yes | data rows + tuning for every number; abilities in ClassForNode; damage only through ApplyHit; server authority with replicated state for every piece |
| R49 | yes | unbuilt parts hidden or labelled (combos' "not built yet" text, portal row off) |
| R50 | yes | lobs on gravity arcs (WRGraveBosses Lob), no homing at players (FireBolt), crimson tells on unblockables (attacks.json rows), the Keen blocked by pillars |
| R51-R53 | yes | every piece compiled clean; CHANGELOG + HANDOFF in each commit; no new binaries in pieces 5-7 |
| R54 | yes | LANDED and LATER-COOKS lines written at the end |
| R58 | yes | HANDOFF (f15b0b1): the Rift Storm seed on the Graveyard's south edge |
| R59 | yes | cc79e40 + 7f0f3f7: Raise dead raises through walls at raiding.paths.necromancy's cost and reach |
| R60 | NO | the bone altar ritual (permanent undead tames) is not built |
| R61 | yes | f15b0b1: narrow bands; no claims underground |

