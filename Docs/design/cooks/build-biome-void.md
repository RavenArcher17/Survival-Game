# Build cook: build-biome-void (the Corrupted wasteland into Aaron's build)

Lane: build-biome-void, desktop, 2026-11-14. Design source: `cooks/biome-void.md` (read in full), plus `world-map.md` (where it sits),
`combat.md` (attack rows, boss template), `survival-crafting.md` (Rest, Corruption, Hollowplate, nullward, Withergoat, mandrake),
`fire-frost.md` (shrine / master / event frame). This doc is the build lane's research, requirement ledger and plan; the design
itself is not re-decided here (biome-void.md is SPEC for this lane).

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, and every section of biome-void.md. "Plan" points into this doc; "Proof" is
filled at the end with file:line or a commit. A row with no proof is not done.

| # | Requirement (source) | Plan | Proof |
|---|---|---|---|
| R1 | Full cook: research, plan, build, spot-check (brief, Aaron 11-13) | 1, 2, 3, 5 | this doc; 9b5ad94, c5c9513, 0295547, b6135cc, f770c82, 7a32dd0, 0753b81, fc4b301, 7485955 |
| R2 | Research names real games, places, systems and why they work; real-world reference (brief) | 1 | section 1 |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases before building (brief) | 2 | sections 2, 3b |
| R4 | Every build step listed (brief) | 3 | sections 3, 3b |
| R5 | Design with intention: why each thing is here, what it connects to, how it meets what it touches (Aaron 11-14) | 2.2, 2.4, 2.6 | every builder comments its place in the land (WRLandSites.cpp, WRVoidUnmade.cpp Build, WRVoidBosses.cpp Build) |
| R6 | Machines look engineered; moving things move continuously on a believable mechanism, never snap (Aaron 11-14) | 2.6, 2.9 | AWRNullGate (pulleys, chains, counterweights), AWRDescentLift (drum, ratchet, pawl, jib, rope), AWRVorhollowStage::PieceNow: all on replicated clocks |
| R7 | No missed requirements; ledger rows proven (Aaron 11-14) | 0, 5 | this table |
| R8 | The land on the map: terrain (brief; void 2.1) | 2.1, 2.2 | WRLand.cpp / wasteland.json (9b5ad94); the pit feature (0753b81) |
| R9 | 5 sub-zones V1-V5 with their own ground, palette, props, creature mix, bed (void 2.2) | 2.2, 2.3 | wasteland.json zones; WRLand ZoneAt |
| R10 | POIs every ~150-250 m (brief; void 2.4: 138 surface) | 2.4 | WRLandSites kit pass |
| R11 | Decoration rule (brief; fire-frost POI spacing, void 2.4 night lights) | 2.4 | WRLandSites Decorate |
| R12 | 10 landmarks (void 2.3) | 2.4 | WRLandSites landmarks |
| R13 | 3 camps, 4 caves, 30 story sites, 6 secrets (void 2.4.1-2.4.4) | 2.4 | WRLandSites camps/caves/stories; secrets X5/X6 only (later cook: the rest) |
| R14 | Hidden sub-biome: the Unmade Place, the Descent and the lift (void 2.5) | 2.5 | WRVoidUnmade.cpp (fc4b301) |
| R15 | Signature resource voidglass + the floating rule (void 2.6) | 2.6 | WRVoid.cpp rings; WRVoidWorld glass sea; U5 garden; floating keels: later cook |
| R16 | Other resources: greywood, grey salt, anchor salt, void essence, ashen grain, mandrake, iron/onyx/star iron, creature parts (void 2.7) | 2.7 | WRVoidWorld.cpp SpawnResources + items.json/recipes.json (b6135cc) |
| R17 | Rift zones drain max HP (MaxHealthLost), passes, the grey band, Unmade death (void 2.8) | 2.6, 2.8 | WRVoid.cpp; Unmade drain x1.5 (fc4b301); void floor kill (WRVoidBosses Stage) |
| R18 | Micro-hazards: rift pull, rubble drop, litter fire, hollow crust (void 2.8) | 2.8 | WRVoidHazards.cpp litter fire + crust (0753b81); rift pull/rubble WRVoid.cpp |
| R19 | Weather: Clear 20 / Overcast 40 / Grey wind 10 / Dead calm 30 (void 2.9) | 2.10 | weather.json (c5c9513) |
| R20 | Biome event: the Rift Storm (void 2.10; brief) | 2.11 | WRVoidHazards.cpp TickStorm/OpenStorm/Wave/HeartDied; world.json events.riftstorm (0753b81) |
| R21 | Shrines (1 great + 4 lesser) and guards (void 2.11; brief) | 2.12 | world.json shrines; WRWorldSystems land placement; WRWorldSites wasteland dais (b6135cc) |
| R22 | Path master Maren Vey and her 6 quests (void 2.11) | 2.12 | world.json masters.maren + quests.maren (b6135cc); events fired in WRVoidWorld/WRVoidBosses/WRVoidUnmade |
| R23 | 9 creatures, real models where packs allow, real animation, behaviour, attacks with visible contact (brief; void 2.12) | 2.13 | creatures.json + attacks.json rows; WRVoidCreatures.cpp (b6135cc) |
| R24 | Riftborn corrupted variants (void 2.12.5) | 2.13 | UWRVoidBody::RollRiftborn (7485955) |
| R25 | Day / night spawn tables per sub-zone (void 2.12.3) | 2.13 | wasteland.json spawns; UWRVoidLife::PlaceDens |
| R26 | Taming methods and utilities: Voidstrider blink mount, Riftback Rift-chest pocket, Driftmote sense, Withergoat farm, Hollowfang path magic (void 2.12.1, 2.12.4) | 2.13 | Voidstrider ride blink (WRCreature ServerRideBlink), Driftmote rift pulse, Withergoat farm data, Hollowfang path_magic; Riftback pocket: LATER COOK |
| R27 | 3 mini bosses with arenas (brief) — the cook has 5 (4 stations + the Echo) | 2.14 | WRVoidBosses.cpp: Unravelled, Mother of Rifts, Starving One, Null Warden (+AWRNullGate), Echo (7a32dd0) |
| R28 | Major boss Vorhollow with arena, P1 / transition / P2 / hidden P3 (brief; void 2.14) | 2.15 | WRVoidBosses.cpp Vorhollow + AWRVorhollowStage; P3 Gullet behind AnyArtifactModifiers (7a32dd0) |
| R29 | Creature voices (fused real calls, 4 layers) (void 2.15) | 2.16 | sounds.json creatures rows (pitched families): fused recordings LATER COOK |
| R30 | Music cues and the Dead calm planned silence (void 2.16) | 2.16 | sounds.json pools.*.wasteland + boss.vorhollow (borrowed tracks), Dead calm silence (c5c9513); composed cues LATER COOK |
| R31 | The Void path: resource, tree, 12 abilities, passives, stat nodes (void 2.17) | 2.17 | paths/void.json; WRAbilitiesVoid.cpp; WRVoidPath.cpp (f770c82) |
| R32 | The 11 Void combos (void 2.17.5) | 2.17 | combos.json 4 of 11 pairs (the partner paths that exist); 7 wait for their paths |
| R33 | Body change (void 2.18) | 2.17 | WRVoidPath::ApplyLook + player_art paths.void |
| R34 | Hollowplate armour set placed and obtainable; weapons, tools, consumables (void 2.19; brief) | 2.7 | items/recipes (b6135cc); Hollowplate data pre-existing; Riftback shield LATER COOK |
| R35 | Building set (Hollow bastion) placed and obtainable (brief) | 2.7 | building_sets hollow_bastion + greywood in voidstone_block |
| R36 | Crops: ashen grain, mandrake (brief; survival-crafting) | 2.7 | farming.json crops + wild stands (WRVoidWorld) |
| R37 | Portal slots (brief) | 2.12 | portals.json biomes.void (pre-existing, land false: hidden until its kit) |
| R38 | Story told only through the environment, lore items (void 2.20) | 2.4 | keepsakes WRVoidWorld + lore items |
| R39 | Invasions Arcane <-> Void (void 2.22) | 2.11 | not added: the Crystal caves do not exist (LATER COOK) |
| R40 | Data-driven like Fire and Frost: numbers in Data/*.json and tuning, abilities as UWRGameplayAbility + ClassForNode, all damage through ApplyHit, screens via WR_REGISTER_SCREEN mouse + pad, sounds/fx as rows (brief) | all | all numbers in tuning void* / bosses.*; abilities in ClassForNode; damage through ApplyHit |
| R41 | Server authority and replication on every new system (brief) | all | replicated: UWRVoidBody, AWRVoidTrail, AWRVoidField, AWRVoidMaw, AWRNullGate, AWRVorhollowStage, AWRVoidState, AWRDescentLift, AWRVoidFloatStone |
| R42 | No placeholders shown as finished; unfinished stays hidden (brief) | 4 | unbuilt parts listed in CHANGELOG "Not in this build" |
| R43 | Projectiles never home, >= 0.9 s warning; crimson on unblockables; small cues; no attack token; no floors, no helpers (combat KEPT) | 2.13-2.15 | void spit / mimic never home; crimson on grabs (row flags); tells >= 0.9 s on projectiles |
| R44 | Compile gate clean of new warnings; no tests (brief) | 3 | compile gate green after every commit |
| R45 | CHANGELOG Unreleased + HANDOFF in the same commit; LANDED line; LATER-COOKS lines (brief) | 3 | CHANGELOG + HANDOFF in every commit; LANDED + LATER-COOKS appended |

---

## 1. Research (build-side; the design research is biome-void.md section 1)

How real games put a large new land into a running world, and how they make it read. What each one gives this build:

| Game / system | What it does, why it works | What this build takes |
|---|---|---|
| **Valheim's world generator** (`WorldGenerator.GetBiomeHeight`, Iron Gate dev talks) | Every land's height is a pure function of (x, y): a base from distance to the world centre, biome masks, then layered Perlin. No stored heightmap: any machine computes the same ground. Mistlands' spires and Ashlands' lava rivers are extra terms added only inside that biome's mask | **The Wasteland's ground is a pure function of map metres**: a base from distance to the Spine crest (the world-map band rule), coast falloff, low noise, then **feature terms from data** (the Great Rift hole, the viaduct ravine, the Glass Sea flat, the Severance gorge, cave hollows). Server and every client build the same mesh with no file |
| **Minecraft / Valheim chunking** | Ground is built and collided per chunk so a big area costs only what is near | 200 m chunks, 5 m grid, collision per chunk, built in a time-sliced queue so the first frame never hitches |
| **No Man's Sky** (planet terrain, GDC 2017 "Continuous World Generation") | A terrain function plus a separate "object placement" pass seeded from position, so props are the same for everyone and never stored | **Kit POIs and decoration are placed by a seeded pass over the land** (the 150 m / 125 m POI rule, decoration density by sub-zone), never saved; only what players change (harvested nodes) is saved |
| **Sea of Thieves / Valheim boats** | Crossing water is the threshold to a new region; the trip itself tells you you've left | **The Grey Crossing**: a newcomer longboat at the island's south shore and a beached one at Lantern Cove. Hold E: you row across the Broken Race (a short faded crossing). It's the world-map's "newcomers came by sea" rule as the way in until the 12-wedge map lands |
| **Elden Ring, Lands Between legacy dungeons** | Landmarks on the skyline pull you; each is a separate set-piece hand-dressed | 10 landmarks are hand-built compositions in code from the look kit (the Throat, the Colossus, the Viaduct, the Beacon...), each with its night light |
| **Enshrouded Shroud / Once Human contamination** (biome-void 1.1) | A max-HP cost shown on the health bar as a band, restored by sleep | `MaxHealthLost` attribute; grey hatched band at the right of the health bar; Rest clears it |
| **STALKER bolts** (biome-void 1.1) | Thrown objects reveal anomalies | Stones and dust float inside rift zones (motes FX); the Driftmote pulses toward rifts |
| **Destiny 2 / Diablo IV dungeon arenas** | Mini-bosses as gates on a road, each its own short room with one mechanic | The four Gauntlet stations as fixed arenas on the road, each boss with one counter mechanic (biome-void 2.13) |
| **Shadow of the Colossus / Elden Ring Placidusax** | A boss on a stage that breaks apart; the arena itself is the second phase | Vorhollow's island: 7 pieces whose positions are a pure function of server time (no snapping), chains that snap at a link |

Real-world references used (from biome-void 1.2): Chernobyl Red Forest (grey dead pines that never rot, litter that burns),
trinitite (black fused glass rings), gravitational lensing (tear rims), aye-aye / naked mole-rat / lamprey anatomy (creatures),
Hal Saflieni acoustics (horn towers, hollow columns).

---

## 2. Plan (every gap solved before building)

### 2.1 Where the land goes (the gap: the 12-wedge map does not exist in the build)

The build's world is `L_Island` (two lands, ~3 x 3 km) and its sea (WRSea, 65 km across). The world-map's B6 belt can't be
placed without the whole map. Rebuilding `L_Island` needs the GUI editor (forbidden) and ~80 MB of LFS per rebuild.

**Decision:** the Wasteland is a **runtime land** (`AWRLand`, data `Data/lands/wasteland.json`), built at sea level in the open
sea **9 km south of the island** (land origin cm (0, 900000, 0)), the same way the war-halls and portal folds are built at runtime
(Raven's pattern: nothing in the map). Its data uses **the world-map's own metres** (x −2,950..+1,250, y +4,600..+6,400), mapped to
the game by `mapCentre` (−850, +5,500) → land origin. When the 12-wedge map exists, changing `origin` and `mapCentre` moves the
whole land with every POI; no data is rewritten.

- `WRBiome::At`: inside a land's bounds → that land's biome (`wasteland`). `WRBiome::IsUnderground`: inside a land cave.
- `tuning world.liveBiomes` gains `wasteland`, so its crops, fish, pieces, sets and recipes come out of hiding (they were data-ready).
- The island's horizon: at 9 km the Great Rift's light column (400 m) shows over the sea from the island's south coast: the
  first sight of the Void (biome-void 2.1 "seen from the Graveyard and the Crown": here, from the island).

### 2.2 The ground (terrain function, mesh, material)

Height h(x, y) in map metres:
1. **Base from the Spine crest** (world-map band rule): d = distance to the crest polyline (−900, +4,200) → (+1,250, +5,400).
   h = 210 at d 0, 140 at 300, 95 at 620, 55 at 1,050, 22 at 1,500, 8 at 1,900 (piecewise smooth).
2. **Coast**: the land outline polygon (data); outside it the ground falls to −12 m over 140 m (beaches: black glass sand). The
   west and south coasts carry the Grey Strand; the north edge (y 4,600) is a land border, not a coast (the Graveyard is not built:
   the land runs 250 m further north then falls into a steep sea cliff, so the border reads as a headland, never a wall).
3. **Noise**: 2-octave value noise ±4 m (lowland), ±7 m (V4 ridged).
4. **Features (data rows, each a term):** `hole` (the Great Rift: r 160 m lip, the ground breaks off in a 6 m lip and the hole is
   cut out of the mesh; walls of fractured rock run down 180 m to black), `ravine` (the viaduct's dry ravine: a line, 40 m wide,
   18 m deep), `flatten` (the Glass Sea disc at its level; station courts; camp yards), `gorge` (the Severance west mouth: 80 m wide,
   20 m saddle, walls 60 m), `pit` (cave mouths, the Listening Wells).
5. **Mesh:** 200 m chunks, 5 m grid (41 x 41 verts), normals from the function, vertex colour = ground by sub-zone; collision per
   chunk (procedural mesh, async cook). Built time-sliced (6 chunks a frame) on server and clients. Far chunks (> 1.2 km from the
   local player) skip collision on clients.
6. **Material `M_LandGround`** (new, built by `Tools/terrain/build_void_materials.py`, commandlet): base colour = vertex colour x a
   world-aligned rock/dirt detail from the MW auto-material's textures (desaturated), roughness from vertex alpha (glass sand and
   voidglass sheets 0.15, ash 0.95). **`M_VoidTear`** (new): unlit black-violet core #2B1840 with lensed star points (#E8E4FF,
   world-position noise), a violet fresnel rim #7B4FD6, slow WPO breathing.
7. **Ground colour by sub-zone (palette law: grey land, black glass, violet only at void edges):** V1 litter grey #9A958C with
   needle grey #A39E95 patches; V2 black sand #2A2A2E to salt crust #ECEAE4 behind; V3 bone-grey paving tint #C9C3B8 in courts,
   grey elsewhere; V4 grey dust #B9B4AC, black voidglass #0B0A10 in rift rings (4-8 m), the Glass Sea black; V5 bone-grey road.
   Blends 80-120 m (void 2.2 Blends).

### 2.3 Sub-zones as data

`zones` in the land file: V1..V5 polygons (map metres from void 2.2), each with `ground`, `decor` (what the decoration pass places
and how dense), `bed` (ambient sound row), `region` (wildlife region id). `WRLand::ZoneAt(pos)` answers for spawns, sound, POI
generator and the Driftmote.

### 2.4 POIs, landmarks, story sites, decoration (the 150 / 125 m rule)

- **Placed POIs** in `wasteland.json pois`: 10 landmarks, 3 camps, 4 caves, 30 story sites, 6 secrets, 5 shrines, 5 mini lairs,
  the Great Rift set. Each has `kind`, `at` (map m), `yaw`, `zone`, optional `light`. Each kind is a **builder** in
  `WRLandSites.cpp` that composes it from the look kit (stone, glass, wood, cloth, lit surfaces) at full detail, with its night
  light (rift glow, fire, beacon, lit stone: every POI has one).
- **Kit POIs (81)** are placed by a seeded pass: for each zone, candidate points on a 60 m jittered grid, rejected within 150 m of
  any POI, accepted when a route point is > 125 m from every POI, until the zone's count (void 2.4.4) is met; kinds by the zone's
  kit weights (ruin, nest, kill set-piece, outcrop, cache, hazard teacher, vista, wreck, offering niche, cairn stack).
- **Decoration (between POIs):** withered pines (CharTree meshes re-tinted silver-grey, needles kept as grey), leaning hawthorns,
  root plates, SimpleRocks boulders tinted grey, point-down cairns every 150 m on routes and every 30 m on the Gauntlet Road, grey
  grass tufts (GV shrubs re-tinted, desaturated) only at V4 edges and in V3 cracks, salt cones in V2. ISM per mesh. Culled at 250 m.
- **Lore items**: 10 lore items as items (`category: lore`), picked up at their sites.

The 10 landmarks, built at the cook's scale: the Great Rift column (L1: hole + 400 m light column + Einstein ring sprite), the
Throat (L2: 90 m stacked dolomite rings tapering, mouth aimed at the Great Rift, spiral stair, 70 m gallery), the Hanging Wood
(L3: 40 floating dead pines over a mid rift, keels of voidglass, 120 s drift), the Hollow Colossus (L4: 45 m singer statue built
of stone blocks, the chest a walk-in hollow with an inner stair), the Hollow Fort (L5: two curved horn-walls, slits, staves),
the Unravelling Viaduct (L6: five spans, the middle two broken, their stones hanging and turning on keels: a jump route), the
Ash-cast cage (L7: eight crystal bars, the ash Cantor inside), the Broken Knee (L8: a 60 m white bone ridge sheared flat on its
east face with voidglass sheen, the marrow crack), the Glass Sea (L9: 300 m black glass disc with fused footprints), the Black
Beacon (L10: watch-tower with a 6 m voidglass lens, a violet cone out to sea at night).

### 2.5 The Unmade Place

A pocket inside the Great Rift 60-160 m below the lip: floating stones (look kit, keels down) on fixed time-driven bobs, the
8 POIs (U1-U8), the Choir's Descent spiral stair cut into the north wall (steps every 0.35 m, 120 m down, ending at the rock's
end), the **Descent lift** (drum on an axle in two bearing blocks, ratchet + pawl, two cranks, rope over a jib to a basket cage;
repair rope x10 + iron x4 by holding E 5 s; crank by holding E, 1.2 m/s, the rope visibly winding; the pawl holds the cage where
you let go; server-owned cage height replicated, clients ease to it). Falling below the void floor (lip −190 m) inside the hole
kills you (`Unmade`): your bag is left at U1. Drain ×1.5 inside.

### 2.6 Rifts, voidglass, the floating rule

`AWRRift` (data `rifts`: 24 stable, `{at, size: large/small, zone r}`): a vertical tear (procedural lens mesh, `M_VoidTear`),
lensing rim sprite, a 4-8 m voidglass ring laid on the ground (black glass plates from the look kit, edges violet, shards),
floating motes inside the zone (FX row `rift.motes`), floating rubble (3-8 SimpleRocks stones with black spike keels, bobbing on
a pure time function, dropping during the pulse), voidglass nodes on the ring (harvestable, 4 hits, 6 game h respawn).
Server: zone drain into `MaxHealthLost`, the 6 m pull (180 cm/s toward the tear, applied on the server and the owning client from
the same rule so movement doesn't correct), touch damage 40 through `ApplyHit`, the pulse timestamp (40-90 s) replicated: 1.0 s
silence tell, then the stones drop for 2 s (45 damage only where a stone lands). Mined voidglass is inert (an item).

### 2.7 Resources, items, sets, crops, building

Already in data and hidden until `wasteland` is live: Hollowplate (gear.json set), nullward (talisman), Hollow bastion building
set, rift chest, ashen grain and mandrake crops, Withergoat farm animal, void fish. This lane adds: voidglass / greywood / grey salt /
anchor salt / void essence / mote gel / creature parts / lore items (items.json), the recipes of void 2.19.2 (Gnashblade,
Hushfinger spear, Nullwing bow, Voidglass dagger, Riftback shield, Beacon lantern, anchor salt, curse resist potion), and the world
sources: greywood from withered pines (harvest: axe), grey salt from salt pans (hand, 4 per pan, 2 h), voidglass from ring nodes,
star iron from the Hollow Ground's meteor bowl, ashen grain and mandrake wild stands. `riftDrain` gear stat (already listed in
gear.json) halves the drain.

### 2.8 Hazards

Rift zone (2.6). Rift pull (2.6). Rubble drop (2.6). **Litter fire** (V1): a 6 m grid over V1's litter; any fire hit on the ground
inside V1 (fire spells, lava pool, fire bolt impact, a burning creature) starts a 0.9 s smoulder (smoke FX) then a burning cell;
spread to the downwind neighbour every 1/1.5 s (upwind 1/0.5), each cell burns 60 s then stays black 2 game days; 3 dps + 8 burn
buildup through `ApplyHit` to anyone standing in it; replicated as a compact list of burning cells. **Hollow crust** (V4 data
patches): a hollow drum sound within 6 m; a heavy landing, roll or heavy attack inside the patch breaks it (the crust collision
is removed on all machines), dropping you 4-8 m into a pocket cut under it.

### 2.9 Motion law

Everything that moves is a pure function of server time or eases toward a replicated target: floating stones, the Hanging Wood,
viaduct stones, rubble pulses, the lift cage, the Null Gate counterweights, Vorhollow's island pieces. Nothing sets a position on a
tick without that function.

### 2.10 Weather

`weather.json wasteland`: overcast 0.4 (exists), greywind 0.1 (was 0.3; the dust row = Grey wind, void 2.9), **deadcalm 0.3**
(new: no wind, no ambient bed, footsteps x2 in the mix, `hearingMult 1.5` read by hearing hunters, music fades out over 8 s and back,
fights in it bring only the combat pulse). Clear is the remainder (0.2). Rain never.

### 2.11 Rift Storm and invasions

World-systems event row `riftstorm` (biome wasteland, one game day, ~every 3 real days): 5 min warning (the Great Rift column
thickens, every inrush howls), 6 of 10 seeds open as storm rifts with a rift heart (1,500 HP; Arcane x3, void x0.5), 3 waves 4 min
apart that walk to a fixed target chosen at spawn (nearest claim within 1.5 km, else nearest shrine), voidglass x3 on closed rings,
1,500 path XP split by heart damage. Invasions Arcane <-> Void: data row, `live: false` until the Crystal caves exist (hidden).

### 2.12 Shrines, master, quests, portals

world.json rows: biome `wasteland` (path void, signature voidglass, essence void_essence, faction hollow_choir, waymark
"cairn_pointdown"), 5 shrines at the cook's positions (Shrine of the Edge great, Greywood, the Strand, Severance West, the East
Gate) with their guards; Maren Vey (master, biome wasteland, at the Great Shrine) and her 6 quests in the existing quest kinds
(`show` throw stones -> `sneak`/`kill`/`bring` as the kinds allow). Portals: `portals.json biomes.wasteland` slot row hidden
until its portal kit exists (`land` false).

### 2.13 Creatures

Real models: no sculpted void creature exists and ComfyUI is off-limits, so each borrows the nearest real rig from our packs with
the existing overlay system (desaturate, darken, violet glow line), real clips from that rig. Mapping (body plan first):
Hollowfang = Barghest (lean canine), Riftmaw = Glacimaw rig (maw ambusher), Nullwing = Scorchwing (flyer), Gnawer = Kilnmite
(small swarm), the Hush = Pale hunter (tall stalker biped), Voidstrider = Frostantler (tall gaunt quadruped), Riftback =
Basaltback (domed tortoise: exactly the cook's suggested base), Driftmote = Glowmoth (floating glow), Withergoat = Woolly drudge
(farm herbivore). Attack rows in `attacks.json` from void 2.12.2 (motions the runner has; new motions `blink`, `latch`). Riftborn
= a spawn-time variant flag (tier up, +Blink row, curse 15 on hits, overlay desaturated with violet eyes). Spawn regions V1-V5
day/night from 2.12.3 (`land: wasteland, zone: Vn`).

### 2.14 Mini bosses (the four stations + the Echo)

Station arenas built by builders (Cantor's Stair, Rift Pens, Alms House, Null Gate with its counterweight machine). Bosses as
boss rows with move rows from void 2.13 on borrowed rigs (Unravelled = Obsidian warden, Mother of Rifts = Magmaw scaled, Starving
One = Grimjaw, Null Warden = Obsidian warden variant, Echo = Ignessa rig) plus their counter mechanics in code.

### 2.15 Vorhollow

Mountain-dragon rig (the only 13 m-capable quadruped) with overlay; the island arena (72 m disc, 4 chains to anchor stones,
124 m chain bridge, 6 voice pillars), P1 rows, the Gnashing transition, P2 time-driven pieces, hidden P3 behind the artifact flag.

### 2.16 Sound and music

`sounds.json` rows: rift inrush (3 sizes), pulse silence, rubble grind, litter smoulder / fire front, hollow drum, chain clink /
snap, pillar ring, lift drum / pawl, gate counterweights; voice sets per creature (pitched from existing recordings until the
fused recordings exist: reported); beds per sub-zone. Music: the cues need composed recordings that don't exist; rows are written
with `waitsFor: music` so the existing music director keeps playing the nearest calm bed and the Dead calm silence works now.

### 2.17 The Void path

`Data/paths/void.json` (tree from void 2.17.2), attributes `VoidCorruption` / `MaxVoidCorruption` and `MaxHealthLost`
(`WRAttributeSet`), `FWRVoidCost::Pay` (tolerance, Empty vessel, Hollowplate / gear `voidSelfDrain`, self-cost limit 75 %, refusal
flash), decay 5/s after 4 s, +0.3 % void damage per bar point, Rest clears both, HUD Void bar + grey hatched band. Abilities:
Void bolt (bolt), Null strike (Wildbrand, armour ignore 30 %), Void rift (pool), Collapse (new pull shape), Blink (new dash),
Shadow step (dash behind lock target), Phase (self buff, i-frames 0.6 s), Nowhere (untargetable 3 s, next hit crits), Consume
(finisher < 25 %), Sacrifice (self buff), Black hole (strike + pull field), Maw of the void (cone). Passives and stat nodes via
`HasNode` / `nodeEffects`. Combos: rows in the combo table where the pair system exists. Body change: `player_art.json paths.void`.

---

## 3. Build steps (in order; each compiled before the next)

1. `Data/lands/wasteland.json` (frame, zones, features, POIs, rifts) + `WRLand.h/.cpp` (land subsystem, terrain function, chunked
   mesh, biome lookup) + `WRBiome::At` / `IsUnderground` hooks + `world.liveBiomes`.
2. `Tools/terrain/build_void_materials.py` -> `M_LandGround`, `M_VoidTear` (commandlet).
3. The Grey Crossing (island quay + Lantern Cove longboat).
4. `WRLandSites.cpp`: landmark, camp, cave, story-site, shrine-yard, station builders; kit POI pass; decoration pass.
5. `MaxHealthLost` + grey band + Rest; `AWRRift` (zone drain, pull, pulse, rubble, ring, motes) + voidglass nodes.
6. Hazards: litter fire grid, hollow crust.
7. Weather rows (deadcalm), sounds/fx rows.
8. Items, recipes, resource sources.
9. Creatures: rows, attacks, motions, Riftborn, spawn regions.
10. World systems rows: biome, shrines, master, quests, Rift Storm.
11. The Void path (resource, cost, abilities, HUD).
12. Minis and arenas; Vorhollow and the island.
13. Spot-check (5), compile gate, CHANGELOG / HANDOFF, commit, push, LANDED.


### 3b. Resume plan (2026-11-15, second run; steps 1-5 and 7 landed in 9b5ad94 / c5c9513 / 0295547)

Gaps found on resume and how they're solved (decided here, before building):
- **Voidglass nodes had no look** (WRResourceLook fell to a basalt boulder). New node looks in `WRResourceLook.cpp`: voidglass
  (a black glass plate fused into the ground with 5-9 standing shards, a violet rim), greywood (a withered silver pine), grey salt
  (a salt-pan crust with cones), ashen grain (a grey grain stand), mandrake (a dark rosette over a forked root), star iron
  (exists).
- **Where the land's resources come from** (server, replicated nodes, `WRVoidWorld.cpp`): greywood nodes on a seeded 45 m pass
  through V1 stands (the decoration pines stay scenery); grey salt nodes on every V2 salt-pan kit POI (4 each, 2 h); ashen grain
  stands in V1 clearings; 6 mandrakes in the Quire's courts (a 0.9 s shiver, then a 12 m scream: stagger 0.8 s unless wax
  earplugs); 3 star iron nodes in the Hollow Ground's meteor bowl; 20 voidglass on the Glass Sea. Mandrake / voidglass hooks
  through `WRVoidWorld::Harvested` (called from `AWRResourceNode::Harvest`).
- **Thrown things**: `throwing_stone` (hand, stone 1 -> 3; kind `riftstone`): inside a rift zone it floats (the quest "Count the
  stones" counts a new rift each time), on hollow crust it cracks it. `anchor_salt` (eat: brew riftDrain x0.5) and
  `anchor_salt_bag` (thrown, kind `anchorsalt`): closes a brood rift, -300 on a rift heart, wards an island piece 10 s. Both land
  through `WRVoidWorld::ThrownLanded` (the Swamp's pattern in WRRanged / WRProjectile).
- **Shrines and Maren Vey on a runtime land**: world.json rows carry `"land": "wasteland"` and `at` in the land's map metres;
  `WRWorldSystems` places them on the land's height function (no trace: the chunks may still be building).
- **Quests**: the existing kinds: event `stone_floated`, event `voidglass_ring`, bring (grey salt 4 + crystal dust 2: crystal dust
  comes from the Crystal caves, not built, so step 3 takes grey salt 4 + voidglass 2 and pays the anchor salt recipe; the trade
  rule comes back when the Crystal caves land), kill unravelled + mother_of_rifts (two kill steps folded into event
  `choir_road` counted per boss), event `last_note` (reach U8), kill null_warden.
- **Creatures on a runtime land**: the island's walk-map wildlife can't reach 9 km south, so the land places **den spawners**
  (AWRCreatureSpawner, ActivationRadius, bTraceGround) on a seeded 170 m pass per zone, day / night kinds from the zone's table
  (Shift 1 / 2), Riftmaws fixed at the large rifts. Riftborn = a spawn-time flag on the body (15% V4-V5, 5% elsewhere).
- **Stand-in rigs** (no void sculpts; ComfyUI is off-limits): body-plan matches from our packs, the cook palette as a tint over
  the rig texture (M_CreatureTint) and silhouette parts built in code on bone frames (UWRVoidBody), as the Swamp did.

## 4. Hidden until finished (no placeholders)

Anything not finished in this lane stays hidden: data rows carry `live: false` / `waitsFor`, the path file isn't added until its
abilities work, bosses not built don't spawn.

## 5. Spot-check and proof

Filled at the end of the build (2026-11-15, second run): the proof column of section 0. Spot-check fixes made while
reading the code back: voidglass nodes had no look (now WRResourceLook); the Riftborn roll ran before the den set
itself on the creature (moved to the first server tick); unity-build name clashes (every file-scope helper prefixed);
the shock-ward notice said "shock" for the curse ward (WRStorm::GrantBuff). Not built here and logged as later cooks:
the Riftback pocket, the Riftback shield, the fused calls and composed cues, Graveyard Riftborn, Arcane <-> Void
invasions and the Arcane weakness, the null field's half refund, floating keels, the Sea Throat seam, the Driftmote's
cache / crust / seed sensing, creatures fleeing litter fire, the Unmade body at U1, X1-X4 secrets.
