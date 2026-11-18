# Build lane: the Swamp (the Mire, Blood & curse) into the game

Lane **build-biome-swamp**, 2026-11-14. Builds `cooks/biome-swamp.md` (every section) into Raven's repo
`C:\Dev\Wildreach`, with `cooks/world-map.md` (B3 east half, camps, neighbours), `cooks/combat.md` (row schema, boss
template, bleed/curse), `cooks/survival-crafting.md` (Mirehide, waders, crops, stilt-house set) and the patterns the
Storm peaks lane set (`cooks/build-biome-lightning.md`: a second landscape, a runtime world from one data file,
stand-in rigs with code-built parts, a path in its own files). Second land off the Old Island.

Order (cook rule): 0 ledger → 1 research for the build → 2 plan (gaps solved, contracts, every step) → 3 build log →
4 spot-check + proof → 5 NOT finished.

---

## 0. Requirement ledger

Brief = lane brief; Q = Aaron quote in the brief; DC = design cook `biome-swamp.md` section. Proof is filled in (4).

| # | Requirement | Source |
|---|---|---|
| R1 | Research how the best games solve this exact thing, written here with the details used | Q 11-13 |
| R2 | Plan every detail before building (layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases), every build step listed | Q 11-13 |
| R3 | Build the plan at full detail: no placeholders, no primitives, no "good enough" | Q 11-13 |
| R4 | Spot-check own work against plan and THE BAR by reading code; fix before push | Q 11-13 |
| R5 | "Already done" only with file:line proof | Q 11-13 |
| R6 | Design with intention: each object/creature/mechanism has a why, what it connects to, how it meets ground/water/walls/player/light/sound | Q 11-14 |
| R7 | Machines look engineered (rods, cages, chains, boardwalk posts, the Walking House legs, the bell); moving parts move smoothly on a believable mechanism | Q 11-14 |
| R8 | Ledger with proof per row; final message lists every requirement | Q 11-14 |
| R9 | The land on the map: B3 east of the Spine under the Crown's east face, joined to Storm peaks at the cliff foot, to the Old Island's tundra by the Rime Sound frozen-bog neck, to the sea/Ocean by the mangrove estuary north | Brief; DC 2.1 |
| R10 | Five sub-zones M1 Reed Flats, M2 Mangrove Thickets, M3 Rotting Boardwalks, M4 Murk Pools, M5 Bone Mounds: ground, palette, props, ambient bed; 60-120 m blends; Frost and Ocean borders | DC 2.2 |
| R11 | Read rules: height draws the eye; knee fog 0-2.5 m on calm days; four water looks (murk, duckweed, bleedwater, quag); night lights (toad rings, corpse candles, Throat mist, bone lanterns, Murkjaw eyeshine) | DC 2.1 |
| R12 | 7 skyline landmarks (Weeping Wall, Mother's Throat, Bled Roc, Walking House, Hanging Cypress, Mangrove Cathedral, Corpse-Candle Mere) each with its night read | DC 2.3 |
| R13 | POIs every ~150-250 m: 24 hand sites + ~49 kit POIs (13 kit kinds with variants) + camps/caves/shrines/lairs = 92 surface | Brief; DC 2.4 |
| R14 | Decoration rule: full and alive, never cluttered: mangrove, bald cypress + Spanish moss, reeds, duckweed, pitcher plants, cypress knees; trees/rocks/shrubs harvestable | Brief; DC 2.1/2.2 |
| R15 | Hidden sub-biome the Sunken Coven: 4 ways in, 10 rooms in order, dark + Lanterntoads, its cue | DC 2.5 |
| R16 | Four karst caves (Reedmouth, Eel Hole tidal, Peat Cellar dig-through, the Gullet behind the third fall) | DC 2.6 |
| R17 | Resources placed and gatherable: bloodmoss (respawn 2 game days), bog iron (3 hits, regrows 4 days, 3:1 smelt), peat (2x fuel), salt, leeches (wading), feathers, hides/teeth, slime, clay, cypress (rot-proof) | DC 2.7 |
| R18 | Bleedwater: 10 bleed buildup/s wading or swimming, threshold x1.3 per proc, no slow, oxblood + leech ripples + 0.9 s edge tell + suck sound; passes: sealed waders, swamp mount, Mirehide 4, leech tar x0.3; creatures ignore | DC 2.8 |
| R19 | Quag: run x0.5, 3 s still sinks to knee (no roll, backstep/jump only), glossy grey + bubbles + suck; Mirestrider full speed; plank piece bridges it | DC 2.8 |
| R20 | Marsh gas: bubble clusters, blue flicker at night, 0.9 s "whoomp"; fire within 3 m lights the field: 4 m flash, 40 + burn 40, chain 0.15 s vent to vent | DC 2.8 |
| R21 | Rotten boardwalk planks: shelf fungus + sag, 0.9 s creak, break on run/roll/heavy landing/2 players, drop 1-2 m; repair 2 wood, holds 4 game days | DC 2.8 |
| R22 | Leech boil (Red Tide): 15/s + 2 latch leeches per 10 s wading, 1.0 s hum tell | DC 2.8 |
| R23 | Weather: Clear 15, Overcast 20, Rain 25 (pools +10%, louder creaks, crops watered), Fog 25 (30 m, ambush 2x), Miasma 15 (oxblood fog 40 m, Murkjaws hunt by boardwalks, curse cries 1.5x, corpse candles by day) | DC 2.9 |
| R24 | The Red Tide: warning 5 min (falls oxblood, mist column x2, crows take off, Coven pillar red), M3-M5 murk turns bleedwater + leech boil, M1/M2 stay clean, leech spawn x3, Bloat walks the causeway, Murkjaws hunt by day, the Hex-Warden's night walk + hexed beasts, Reliquary opens, bloodmoss x3, Vharra's water all bleedwater; camps safe | Brief; DC 2.10 |
| R25 | Five shrines (Tithe Mound Great Shrine + Salt Cross, Root, Cage, Falls) with guards, looks, fast travel | Brief; DC 2.11 |
| R26 | Ruslana the Mire-witch (look, Walking House by night / Tithe Mound by day, short lines within 6 m) + 6 quests with rewards | DC 2.11 |
| R27 | 10 creatures (Murkjaw, Gorgeleech, Hexcrow, Bogstalker, Mirestrider, Mudhulk, Bogsnout, Lanterntoad, Reedfowl, Slimeshell): tier, pack, size, silhouette, palette, taming method, drops | Brief ("9"; the cook has 10: all 10 built); DC 2.12.1 |
| R28 | Real models where Fab/our packs allow, real animation, no gliding, readable terrifying silhouettes | Brief; THE BAR |
| R29 | Every attack row of DC 2.12.2 (incl. latch leech) with tells, damage only on visible contact, the three grab verbs (death roll / tongue+gulp / latch) with their answers; physical resists; lock points; no attacker limit | Brief; DC 2.12.2 |
| R30 | Murkjaw blood-path taming (hold Interact 4 s with 10+ Blood points, bleed 5 HP/s into its mouth) | DC 2.12.1 |
| R31 | Utilities: Lanterntoad light 10 m + shoulder pouch, Bogsnout finds + bloodmoss x2, Mirestrider mount (quag/water 1.4 m, passes bleedwater, sprint 1,300), Mudhulk hauler 600 (wades 2.5 m, passes bleedwater) | DC 2.12.1 |
| R32 | Day and night spawn tables per sub-zone | DC 2.12.3 |
| R33 | Living-world beats: leech packs drain Mudhulks + herd defends; Hexcrows circle <40% HP + land on carcasses; Murkjaws take birds at the edge at dawn; Mirestriders stand and stab fish; Mudhulks at the Salt Lick at dawn, wallow by day | DC 2.12.3 |
| R34 | Mother Maw: nest arena, 5 attacks, boardwalk counter (rears on planks: belly x1.3; slams break sections; repair 3 s), nest fire draws her out 8 s, young at 60/30%, drops | Brief; DC 2.13.1 |
| R35 | The Hex-Warden: Ossuary arena, four totems (crow/bone/blood/voice) 400 HP x2 blunt/fire, scatter, sickle dance, hex bolt, mimic call, blood harvest, mask break, drops | Brief; DC 2.13.2 |
| R36 | The Bloat: roaming 2.2 km route, sleeps by day, gorge meter on the body, 5 attacks incl. burst, salt/fire/frost counters, 3 vein parts, drains Mudhulks, drops | Brief; DC 2.13.3 |
| R37 | Vharra: the look, the Throat arena (terraces, mounds, 6 rods, run-back stone, mist, cages), the lightning counter (rods / water / stormglass, convulse), P1 6 moves + Brood shed, the Sinking, P2 (flood, swim, new moves), hidden P3 (blood water, clot crust), sacs + disc lock points, drops | Brief; DC 2.14 |
| R38 | Creature voices: 14 sets from 3+ real calls + a material layer, signature moments | Brief; DC 2.15 |
| R39 | Music: real compositions per cue (explore A/B/C, night, combat, mini, boss, Sunken Coven, Red Tide layer, camp), always audible, ducks <=4 dB, credited | Brief; DC 2.16 |
| R40 | Ambient beds per sub-zone + Sunken Coven + falls bed | DC 3.4 |
| R41 | The Blood & curse path: 22 nodes, the Spilled bar (HP costs, rally x2 for 8 s, no floor), Bloodthirst, passives, proc feedback, Fire/Frost/Lightning + Blood combos | DC 2.17 |
| R42 | Blood body change (lips, veins, canines 2 mm, iris ring/fill, pulse, lifesteal flush, Sanguine lord glow) | DC 2.18 |
| R43 | Armour set Mirehide placed and obtainable here (recipe inputs reachable in the swamp); swamp items (poultice, leech tar, salt pouch, thinblood oil, hex-ward charm, stormglass jar, Murkjaw-tooth arrow, swamp lantern), trophies, Mire-witch's ring | Brief; DC 2.19 |
| R44 | Building set (stilt house) obtainable here; bloomery and stilt foundation pieces | Brief; DC 2.19 |
| R45 | Crops (bog rice, bloodroot) obtainable here (wild plants/seeds at the Stilt Village paddies and the swamp) | Brief; survival-crafting |
| R46 | Story only through the environment (the 9 beats) + 6 lore items | DC 2.20 |
| R47 | Secrets M-S1 candle path, M-S2 cellar, M-S3 fourth bog body, M-S4 behind the third fall, M-S5 Reliquary | DC 2.20 |
| R48 | Invasions Lightning <-> Blood through Stormgut (Blood invaders leave bleedwater puddles, led by the Bloat; Storm invaders bring strikes that shock water) | DC 2.21 |
| R49 | Portal slots for the swamp | Brief |
| R50 | Data-driven the way Fire and Frost are (Data/*.json, tuning.json, attacks.json, ClassForNode, ApplyHit, WR_REGISTER_SCREEN mouse+pad, sounds/fx rows) | Brief |
| R51 | Server authority + replication for every new actor/system; multiplayer edge cases | Brief |
| R52 | Nothing unfinished shown as finished; unfinished stays hidden | Brief |
| R53 | Colour law: swamp/Blood red is garnet #7A0A14 / oxblood #4A0A0F / rust #8A3B1E, crimson only for unblockable cues | DC 1.3 (7) |
| R54 | Three camps with spawns, safe zones, starter stations; fish (bog eel, snakehead, the Old Gar in the Sunken Coven) | DC 2.1/2.5, world-map |
| R55 | Compile gate only; no tests/check scripts; one UE job at a time; assets headless with all six packs | Brief |
| R56 | Git: pull before/after, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, no release/tag, LFS sizes reported | Brief |
| R57 | LANDED.md line; out-of-scope findings to LATER-COOKS.md | Brief |

---

## 1. Research for the build

The design cook's research (Valheim's swamp, Blighttown/Lake of Rot lessons, the Rotten Vale's one-idea ecology,
Bloodborne's rally, Elden Ring hemorrhage, Ark's grab predators, sphagnum, bog iron, will-o'-the-wisp, bolotnik,
Jenny Greenteeth, electric eels, shoebill, medicinal leech anatomy, bog bodies) stays the source for *what* things
are. This section is what the BUILD needs: how good games make a swamp work in an engine.

### 1.1 Swamp terrain and water in engines

| Source | What it does | What we take |
|---|---|---|
| **Valheim swamp** (heightmap biome under a global sea level; swamp ground sits 0-1 m around the water line so water fills the hollows "for free"; the swamp's water is the same ocean plane, tinted by biome) | One water plane, ground shaped around it | The swamp's ground is built AROUND the sea level (the drawn surface at +0.6 m): dry ground +0.9 to +3 m, murk floors -0.5 to -3 m, channels to -4 m. Every hollow below +0.6 m is water from the sea's own volume (`sea_mask.mjs` floods every low cell inside the swamp frame, not only the ones the ocean reaches). One water system: swimming, drinking, fishing, splash all work at once |
| **Red Dead Redemption 2, Bluewater Marsh / Lemoyne bayou** (Rockstar: dense bald cypress with Spanish-moss cards, low mist volumes held near the water, boardwalk fishing camps on posts, duckweed and lily mats breaking up reflections, everything at human eye height reads through fog because the canopy is above it) | Silhouettes above low fog; boardwalks on posts; mats on water | Knee fog under 2.5 m (sky.fog layer, swamp row); every POI has a height cue (cage gallows, stilt huts, mounds, dead cypress); boardwalk modules stand on paired posts every 2.4 m sunk into the mud with cross-bracing; duckweed / bleedwater / quag are drawn as surface mats over the sea plane from a water-kind grid |
| **Horizon Forbidden West, the Isle of Spires / Horizon Zero Dawn swamps** (water hazard read by surface: oily sheen, bubbling) and **Elden Ring Lake of Rot** (one colour = one rule) | The surface tells you the rule | Four surface looks each with its own material: clear murk (sharp, dark), duckweed (lime #8DB040 mat), bleedwater (oxblood #4A1A14 with moving ripple normals and a metal film), quag (glossy grey #6E6A60 with slow bubbles). The look IS the rule; the server reads the same grid |
| **Dark Souls Blighttown lesson** (framerate collapsed in stacked geometry) | Budget | The swamp is flat; instanced (HISM) cypress, mangrove, reeds and boardwalk planks; decoration culls at ~250 m like the island; the fog is a sky parameter, not volumes |
| **Monster Hunter subspecies / Dark Souls reskins** | Same skeleton, new hide + silhouette parts | As Storm peaks: each creature takes the nearest real rig and its clips, a new palette material and code-built silhouette parts (teeth bar, moss ridge, three-jaw disc, hood plates, tusks, bill, throat sac). Rows, voices, behaviour and drops are its own. Own sculpts stay on the art PC list |

### 1.2 Water predators and grabs in games

| Source | Detail | Use |
|---|---|---|
| **Ark Kaprosuchus / Sarco** (ambush from water, grab-and-drag) | The grab is the threat; an ally can break it | Death roll: paired grab; ally head hit of 120 frees; mash shortens |
| **Monster Hunter Wilds Uth Duna, Jyuratodus** (mud/water boss; the arena changes as it floods) | Arena state changes are the phase | Vharra's flood is a real water-height change (the arena's own water volume rising 1.2 m over 6 s) and her swimming is checked against it |
| **Zelda TotK / Genshin electro-on-water** | Lightning spreads through the water you stand in | Rod or jar shocks a connected water patch for 1.5 s: everyone standing in it is shocked (Grounded immune), the boss in it convulses |
| **Elden Ring Rennala / Godrick** (summon/young adds at HP thresholds, at most a few, readable) | Adds at thresholds | Mother Maw's young at 60/30%, Vharra's brood shed once per 45 s, going for the furthest tame |

### 1.3 Placement in the built world (a gap the design cook could not see)

The Storm peaks lane built the Crown's east face further east than the world-map guessed: sampling
`Art/Terrain/storm_height.r16`, the face (where the ground falls from ~700 m to the sea) runs at about x +450 at
y -4,200, +650 at -4,000, +750 at -3,800, +850 at -3,400..-3,600, +800 at -3,000, +650 at -2,800, +600 at -2,600, and
Stormgut's east mouth opens at about (+680, -4,900). The cook's swamp says "land from the cliff foot at about x +300".
**Decision:** the swamp keeps every distance the cook set from the cliff foot. Each cook point (x, y) maps to
`x' = cliffFoot(y) + (x - 300)` and `y' = y`, where `cliffFoot(y)` is read from the storm heightmap (easternmost ground
above 40 m, smoothed over 150 m, + 25 m of talus). The coast moves out with it (to ~+3,000 at the Crown's latitude),
so the 4 km² stays 4 km², the Bloat's loop stays 2.2 km and POI spacing stays as designed. Everything downstream reads
world coordinates from `Data/swamp.json`, never the cook numbers.

---

## 2. Plan

### 2.1 Frame and join

- **Landscape** `Landscape_Swamp`: 1,891 × 1,891 samples, 2 m apart (15 × 15 components of 126 quads), corner
  (x0 +250, y0 -5,684) → (x1 +4,030, y1 -1,904). Heights 16 units/m, Z scale 800 (as Storm peaks). Its WP proxies sit on
  a 504 m grid offset from Storm peaks' (250 + 504k vs -2,900 + 504k), so each import script removes only its own.
- **Overlap with Storm peaks** (x +250..+1,132): where the storm ground is above the swamp's, the swamp writes the storm
  height minus 3 m (hidden under it); where storm is sea, the swamp's own land rises. The Weeping Wall's five falls pour
  off the storm cliff into swamp plunge pools.
- **Overlap with the island** (y -2,016..-1,904): as Storm peaks did, island-land cells get island height minus 3 m;
  the **Rime Sound neck** (cook point (+1,250, -2,460) → world via the mapping, ~x +1,300..+1,700) rises as a frozen-bog
  saddle 1-3 m above sea level from the swamp's south edge to the island's tundra shore.
- **Sea and estuary:** east coast ~+2,450 cook (mapped ~+2,900..+3,050), ragged channels and bays; north edge y -5,400
  (cook) is the mangrove estuary into the sea (the Ocean lane extends it).
- **Biome lookup:** `WRBiome::At` returns `swamp` for y < island north edge and x east of the swamp boundary (the
  Crown/Spine crest line east face top, `swamp.json boundary` as [y, x] pairs), when the swamp is live. Storm peaks keeps
  the rest north of the island.

### 2.2 Heightfield (`Tools/terrain/swamp.mjs`)

Layers, each with a job (cook coordinates, then mapped):
1. **Land mask:** cliff foot (west) to coast (east ~+2,450 cook ± 90 m fbm, with channels cut inland as tidal creeks
   every ~400 m), north estuary (land breaks into mangrove islands north of -4,900), south frozen-bog neck.
2. **Base:** dry ground +1.2..+2.4 m (fbm 8 m features), raised-bog domes (M3: +3..+4.5 m, 100-200 m across), bone
   mounds (M5: cones 6-25 m), reed causeways (+1.4 m ridges along routes).
3. **Water:** murk sheets (M3 open murk 35% area: floor -0.8..-1.6 m), channels (M2: -2..-4 m, 20-40 m wide, braided),
   pools (M4: 30-80 m, floor -1..-2.5 m), the Deep Pool (40 m round, -20 m; a peat-mat lid drawn over it), the
   Corpse-Candle Mere (300 m, -2 m), Mother Maw's pool (60 m, -1.5..-3 m) round a nest mound (+2.2 m, 12 m), the Bloat's
   Wallow (sunken basin 50 m, -1 m), plunge pools at the falls (-3 m).
4. **The Throat** (cook (+900, -3,600)): 110 m sinkhole; upper ring at +1.5 m (r 45-55 m), middle ring floor +0.4 m
   (20 cm water, r 32-45), lower ring floor 0.0 m (60 cm water, r 20-32), central pool to -8 m (r < 20); four ossuary
   mounds +3.6 m on the middle ring (N/E/S/W-ish, leaving the west wall to her anchor), six rod plinths +2.1 m between
   the rings; the Throat Stair a bone-paved ramp from the east rim down; run-back stone 50 m east.
5. **Sites and anchors** levelled (camps +1.8 m, Walking House pad, Tithe Mound +3 m, Ossuary mound 25 m high with a
   flat top 20 m, Hanging Cypress mound, Salt Lick pan +0.8 m, Peat Cut trench, Thorn Gate, Rod Line).
6. **Detail:** small fbm (0.3 m) on dry ground, hummocks (0.5-1 m) in M4, pneumatophore roughness in M2.
7. **Routes** (reed causeways and Coven boardwalk lines): the Drowned Causeway (coast camp 2 → Tithe Mound → Throat
   Stair, broken in 4 places), the reed causeway along the coast (camp 1 → 2 → 3), the bone causeway (Ossuary → Throat →
   Rookery), boardwalk spurs to the Walking House, the Peat Cut, the Hanging Cypress, Mother Maw's square, the Mere's
   punt jetty; root paths in M2. Each route is a polyline: causeway segments carve a +1.4 m ridge; boardwalk segments
   leave the ground alone (the runtime builds planks on posts over water).
8. **Outputs:** `Art/Terrain/swamp_height.r16`, `Data/swamp.json`, `Docs/terrain/swamp.png`, swamp rows appended to
   `Data/island_dressing.json` (trees/reeds/shrubs as harvestables), and the sea mask / walk map / journal map re-run.

### 2.3 `Data/swamp.json` (the contract every part reads; world coordinates in metres, z = ground metres)

```
frame   {x0, y0, step, samples, unitsPerMetre, zScale}
boundary [[y, x], ...]                       west edge of the swamp biome (the Crown/Spine east-face top)
zones   {cell: 32, x0, y0, width, height, rows: ["~1122..."]}   1..5 = M1..M5, f = frost border, ~ = sea/out
water   {cell: 8, x0, y0, width, height, rows}   per cell: '.' dry, 'm' murk, 'd' duckweed, 'b' bleedwater, 'q' quag,
                                                  'k' brackish (salt: Red Tide never turns it), 'g' gas field (dry or wet)
seaZ    0.6                                    the drawn water surface (m)
sites   [{id, kind, x, y, z, yaw, name, landmark?, zone, loot?{item: n}, extra?...}]   7 landmarks + 24 hand + camps +
                                                caves + lairs; kinds listed in 2.4
shrines [{id, x, y, z, name, guard[]}]         5
kit     [{kind, variant, x, y, z, yaw, zone}]  ~49+
routes  [{id, kind: causeway|boardwalk|bone|root, w, pts:[[x,y,z]...], rotten:[segment indices]}]
gas     [{x, y, z, vents:[[x,y]...]}]           marsh-gas fields
arenas  {vharra:{x,y,z, radius, rings:{upper,middle,lower,pool}, mounds:[[x,y,z]...], rods:[[x,y,z]...], anchor:[x,y,z],
           stair:[[x,y,z]...], runback:[x,y,z]},
         mother_maw:{x,y,z, nest:[x,y,z], square:{half, z, gaps:[...]}, runback},
         hex_warden:{x,y,z, top, totems:[[x,y,z]x4], runback},
         bloat:{lair:[x,y,z], route:[[x,y,z]...], runback}}
hexwalk [[x,y,z]...]                           the Hex-Warden's Red Tide walk (Thorn Gate → Tithe Mound)
candle  {from:[x,y,z], pts:[[x,y,z]...], island:[x,y,z]}   M-S1 line (hidden boardwalk 30 cm under the duckweed)
coven   {deepPool:[x,y,z], chimney:[x,y,z], cellar:[x,y,z], rooms:[{id, x, y, z, w, d, h}]}   the Sunken Coven layout
falls   [{x, y, top, bottom, width}]           5 waterfalls of the Weeping Wall
camps   [{x, y, z, yaw}]
```

### 2.4 The runtime world (`WRSwampWorld.cpp/.h`, the Storm peaks pattern)

Server spawns stateful replicated actors; every machine builds the scenery locally from the same data. Looks are
code-built from the build-look kit (`WRLookKit`), pack meshes (GV shrubs, SimpleRocks, AI trees in /Game/Props), and
tinted materials, at real scale, never bare primitives: a cage is bars + hoops + a chain to a branch; a fetish post is
a pole, a crow skull (beak, sockets), a bottle charm on a cord.

| Kind | What's built (why it is there / what it meets) |
|---|---|
| `weeping_wall` | 5 falls: ribbon sheets from the storm cliff top to the plunge pool, rust-red #9A3B22 (oxblood in the Red Tide), spray FX and pink mist at the base, red-stained cliff decals; the falls bed rises toward the cliff (audio) |
| `throat` | 12 dead cypress 40 m leaning inward on the upper ring (trunks + knees sunk in the bone/peat; branches reach over the pool), rusted cages hung on chains from 9 of them, the red mist column (sprite column lit from the water, 150 m), tithe basins along the stair with stone channels running down to the water |
| `bled_roc` | A roc skeleton 45 m span sprawled over the gut mouth (spine, ribs arching 8 m, wing bones fanned), Coven bone hooks in the wing bones with chains running to stakes in the bog |
| `walking_house` | A hut 15 m up on two bird legs of black cypress (thigh, shin, ankle, four clawed toes sunk in the bog), rope ladder to a trap door, chimney smoke, warm window light (the only home light); cellar trapdoor in the floor (M-S2) |
| `hanging_cypress` | A 60 m cypress (trunk, buttress, knees, lower branches you climb), hundreds of bottle charms and bone chimes swaying, 9 iron cages on chains; the key cage 18 m up with a skeleton holding the Coven key (interact) |
| `mangrove_cathedral` | One giant red mangrove: prop roots arching 20 m over the channel crossroads, a root-altar at the centre (Root Shrine) |
| `corpse_mere` | The giant fish-weir skeleton on the far shore (stakes + woven panels), a half-sunk punt at the near jetty; corpse candles at night |
| Hand sites (24) | Each as DC 2.4, with its story props and its loot cache (`AWRWorldCache`), e.g. the Rod Line (9 verdigris rods leaning, skeletons with leech pits, the 9th bent double), Thorn Gate (stake palisade all pointing west, a gate burnt from the west), Peat Cut (trench, upright spade, turf stacks, 3 bog bodies laid out with cords), Stilt Village (stilt huts, paddies with wild bog rice, slime still dripping, hex-marked door), Salt Lick (pan, broken rake), Estuary Wreck (hull to its rails in the mangroves, salvage chest in the cabin) ... |
| Kit (13 kinds) | fetish post, stilt hut, bloodmoss hummock, bog-iron shallows, cage gallows, slimeshell egg reeds, kill set-piece, marsh-gas field, sinkhole, peat stack + lean-to, reedfowl nest island, broken punt + cache, lanterntoad ring |
| Boardwalks (`AWRSwampBoardwalk`, replicated) | Planks 2 m wide on paired posts every 2.4 m (posts sunk 1 m into the mud, cross-braced), sound planks grey-green, rotten ones black with shelf fungus and a 6 cm sag; break/creak/repair state replicated per plank; mended planks lighter for 4 game days |
| Water mats | Duckweed, bleedwater, quag and brackish surfaces drawn as merged quads over the water-kind grid just above the sea surface; bleedwater with moving ripple normals; the Deep Pool's peat-mat lid with its central gap |
| Dressing | `island_dressing.json` rows: bald cypress (with Spanish moss), red mangrove, reeds, pitcher plants, cypress knees, dead cypress, bone piles: harvestable as the island's trees/rocks/shrubs |
| Nodes | bloodmoss (hand, 2, respawn 2 game days), bog iron (pick, 3 hits, regrow 4 days), peat (spade/pick), salt (hand), clay (KEPT set-piece); copper/tin/iron in caves |
| Caves | 4 `AWRCave` rows with swamp look (limestone, wet, drips); the Gullet behind the third fall |
| Night lights | toad rings, bone lanterns on lit shrines, Walking House window, Throat mist glow, corpse candles over the Mere |

### 2.5 Hazards, events and systems (`WRSwampSystems.cpp/.h`)

- **Water kind at a point** (`WRSwamp::WaterKindAt`, grid lookup; Red Tide turns `m`/`d` to `b` in M3-M5).
- **Bleedwater tick** (server, 4 Hz): a player whose feet are below the surface on a `b` cell takes 10 buildup/s
  (`hazards.bleedwater`) through the status component (x0 with sealed waders / swamp mount / Mirehide 4, x0.3 leech tar);
  the 0.9 s edge tell (ripple FX toward the foot + suck sound) plays client-side on entering a `b` cell.
- **Quag**: `q` cells: CMC speed x0.5 (server-driven tag as Raven's slowed), still 3 s → `Status.Sunk` (no roll), cleared
  on leaving; Mirestrider riders exempt.
- **Marsh gas** (`AWRMarshGas`, replicated per field): vents with bubble FX and night flicker; any fire within 3 m
  (torch held, burning actor, fire projectile hit, burn proc) → 0.9 s whoomp tell → flashes vent to vent at 0.15 s
  (4 m, 40 + burn 40 through ApplyHit, players, creatures, the Bloat), recharge 60 s.
- **Rotten planks**: on the boardwalk actor (above).
- **Weather**: `weather.json` swamp row set to the cook's five states with their effects; Miasma: oxblood fog colour and
  40 m vision in `WRSky`, Murkjaw/curse modifiers read by the creature code from the weather id.
- **Red Tide** (`world.json events.red_tide` + `WRSwampSystems` step): schedule, 5 min warning (falls oxblood, mist x2,
  crow takeoff, Coven pillar red), the tide (kinds, leech boil, spawn x3, Bloat on the causeway, Murkjaws by day,
  bloodmoss x3, Reliquary door), the night walk and hexed beasts (aura on Bogstalkers/Murkjaws within 30 m of the
  Hex-Warden: garnet eyes, curse 25, aggro x1.3), never in camps.
- **Sunken Coven**: ten rooms built under the Deep Pool as a sealed volume (peat-mat roof, stilt houses in knee water,
  the Tithe Hall basins, the Bog-Body Gallery's 22 niches, the Birthing Pools with egg cases, the Witch's Library loft,
  the Old Gar's chapel pool (fishing ledge, `fish.json` old_gar), the Ringing Room chimes (noise wakes the pools), the
  Bell Pit bell (once a game day), the Sealed Reliquary (Red Tide only), the Weeping Gate one-way into the Throat).
  Ways in: dive (12 s), cellar shaft (Coven key), Bogsnout dig at the chimney (20 s), the Gate from inside.
- **Secrets**: M-S1 candle path (night: a candle drifts the hidden line; stepping off puts it out; the line's planks are
  a hidden boardwalk 30 cm under the duckweed), M-S2 cellar, M-S3 Bogsnout uncovers the fourth bog body (gold torc),
  M-S4 the ledge behind the third fall (seen thin at sunset) to the Gullet + smith's cache, M-S5 the Reliquary.
- **Shrines, Ruslana, quests**: `world.json` rows (shrines with swamp looks and guards; master `ruslana` with day/night
  places; 6 quests with the new task types `bleed_cured`, `kill_on_land`, `cursed_kills`, `fetch_key`, `beat_with`).
- **Invasions**: the `storm_peaks -> swamp` pair goes live; a `swamp -> storm_peaks` pair (Bloat leads; Gorgeleech /
  Bogstalker+Mudhulk / Hexcrows / Murkjaw; bleedwater puddles left by invaders standing 4 s).
- **Portals**: `portals.json` swamp row slots on swamp ground (`land` stays false until a swamp fold kit exists: R52).
- **Items/recipes/pieces**: every swamp item of DC 2.19 and the cook's 3.1 list; bog iron → iron at the bloomery or
  furnace 3:1; Mirehide, sealed waders (KEPT), stilt foundation, bloomery.

### 2.6 Creatures (`WRSwampCreatures.cpp/.h`, Storm creatures pattern)

Rows in `creatures.json` (biome `swamp`, `resists [bleedwater, quag, marsh_gas]`), every DC 2.12.2 row in `attacks.json`,
`spawn_regions.json` swamp regions M1-M5 with day/night tables. Rigs (nearest real body + its clips; palette + code
parts make the silhouette):

| Creature | Rig | Code-built parts |
|---|---|---|
| Murkjaw (7 m) | Magmaw | the interlocking teeth bar (splayed outside the closed jaw), osteoderm ridge with bloodmoss tufts, red eyeshine at night |
| Gorgeleech (1.4 m) | Glacimaw (worm/burrower body) small | segmented ring bands, ten-eye crescent, three-jaw Y disc that opens on the tell, orange-red paired stripes |
| Hexcrow (2.4 m span) | Griffon small (real flight) | bone-white rook face plate, ragged hackle crest, garnet sheen |
| Bogstalker (2.2 m) | Rimeshell (low, squat) | horned brows, the 60% mouth (lip ridge), pale throat sac that pumps, spline tongue on the lash |
| Mirestrider (3.5 m) | Ashstrider (long legs) | the shoe bill, S-neck plumes, splayed toes |
| Mudhulk (3 m shoulder) | Basaltback | box head with short tusks, wet garnet-orange sweat sheen |
| Bogsnout (1.1 m) | Pumicehog | babirusa tusks curling back over the face, sparse bristle |
| Lanterntoad (40 cm) | Snowpuff / Kilnmite small | throat sac that glows (light component) |
| Reedfowl (1 m) | Griffon tiny | red brow shield, long toes |
| Slimeshell (1 m) | Rimeshell small | coiled banded shell, gliding foot; pink egg clutches on reeds (kit) |

### 2.7 Bosses (`WRSwampBosses.cpp/.h`, Storm bosses pattern)

Mother Maw (Magmaw x1.9 + duckweed/moss mat, scars, hook), Hex-Warden (Ignessa rig: crow-skull mask, feather cloak,
corset, two bone sickles; mask breaks on stance break), The Bloat (Glacimaw x4.3 + swell scale + emissive garnet volume
by gorge), Vharra (MountainDragon front + a 14-segment ringed trunk built in code, tapering 18 m to the anchor disc; the
three-plate hood; three sacs with a fill parameter). Arenas from `swamp.json arenas`; rods `AWRSwampRod` (copper,
verdigris; shock the connected water patch); the Throat's own water volume (rises for the flood); boardwalk sections of
Maw's square as breakable actors; totems; the Bloat's route; drops and lore items; run-back stones.

### 2.8 The Blood path (`WRBloodPath.cpp/.h`, `WRAbilitiesBlood.cpp/.h`, Lightning path pattern)

`Data/paths/blood.json` (22 nodes, ST text), Spilled (replicated on the path component), Bloodthirst, every passive,
the 10 ability classes in `ClassForNode`, proc FX rows, combos Fire/Frost/Lightning + Blood in `combos.json`, the main
menu picker gets Blood from the registry, the HUD's garnet Spilled segment, Blood body change in `WRPathLook`,
Murkjaw blood taming hook.

### 2.9 Sound and music (`Tools/audio/sources_swamp.py`, `build_swamp_audio.py`)

Storm's route: Kevin MacLeod (incompetech, CC-BY) cues chosen for slow, minor, cello/drone/voice character; Freesound
CC0/CC-BY recordings fused per the 2.15 recipes (croc bellow + hiss, lamprey/suction, raven, horned frog, shoebill
clatter, hippo, babirusa/warthog, toad trill, swamphen, plus bosses), hazard events (plank creak/break, gas whoomp,
bleedwater suck, quag suck, rod shock, chimes, bell), beds (M1-M5, falls, Sunken Coven), credited in `Docs/CREDITS.md`.

### 2.10 Edge cases decided

- A player on the swamp/storm seam: `WRBiome::At` by the boundary line; the cliff face belongs to Storm peaks.
- Bleedwater never ticks on a mounted player whose mount passes it; a dismount in the water starts the tick at once.
- Marsh gas never chains into a claim; a field relights only 60 s after it burned.
- Planks: two adjacent planks are never rotten on the Drowned Causeway; a broken plank mends itself after 4 game days
  if nobody repairs it; repair needs the player not on that plank.
- The Red Tide never changes M1/M2 (brackish) or the camps; a player logging in mid-tide sees the current water kinds
  (replicated state).
- Vharra's flood resets on wipe/reset; rods re-arm on reset; the Bloat resets to its lair path if pulled past 65 m.
- The Hex-Warden's night walk only happens with no player fighting her at the Ossuary; her hex aura ends at her death.

### 2.11 Who builds what (parallel tracks; file ownership so nobody fights)

| Track | Owns (new files) | Shared files it may add to (Edit, small anchored edits) |
|---|---|---|
| Lead (this lane) | `Tools/terrain/swamp.mjs`, `import_swamp.py`, `build_swamp.ps1`, `Data/swamp.json`, sea mask / walk / world map re-runs, final integration, CHANGELOG, HANDOFF | `Tools/terrain/sea_mask.mjs`, `storm_walk.mjs`, `world_map.py`, `import_storm.py`, `build_terrain_materials.py` |
| World | `WRSwampWorld.cpp/.h` | `WRSurvival.cpp` (`WRBiome::At`), `WRWildlife.cpp`, `WRSky.cpp`, `WRWorldDressing.cpp`, `WRWorldSites.cpp`, `WRUIMap.cpp`, `WRPortals.cpp`, `tuning.json` key `swampWorld`, `world.json liveBiomes` |
| Systems | `WRSwampSystems.cpp/.h` | `world.json` (events, shrines, masters, quests, invasions), `items.json`, `recipes.json`, `pieces.json`, `weather.json`, `portals.json`, `fish.json`, `WRWorldSystems.cpp` quest hooks, `WRInventory.cpp`, `tuning.json` keys `swampHazards`, `redTide` |
| Creatures | `WRSwampCreatures.cpp/.h` | `creatures.json`, `attacks.json`, `spawn_regions.json`, `WRCreature*.cpp` hooks, `tuning.json` key `swampCreatures` |
| Bosses | `WRSwampBosses.cpp/.h` | `creatures.json`/`attacks.json` boss rows, `WRBoss*.cpp` hooks, `tuning.json` key `swampBosses` |
| Blood path | `WRBloodPath.cpp/.h`, `WRAbilitiesBlood.cpp/.h`, `Data/paths/blood.json` | `combos.json`, `WRCombos.cpp`, `WRAbilities.cpp` (ClassForNode), `WRCombatLibrary.cpp`, `WRPathComponent.cpp`, `WRPathLook.cpp`, `WRUIHud.cpp`, `WRMainMenu.cpp`, `tuning.json` key `blood` |
| Audio | `Tools/audio/sources_swamp.py`, `build_swamp_audio.py`, `Data/Audio/*` swamp files | `Data/Audio/sounds.json`, `Docs/CREDITS.md`, `WRAudioDirector.cpp` swamp beds/pools |

Shared ids (everyone uses these): biome `swamp`; creatures `murkjaw, gorgeleech, hexcrow, bogstalker, mirestrider,
mudhulk, bogsnout, lanterntoad, reedfowl, slimeshell, latch_leech`; bosses `mother_maw, hex_warden, bloat, vharra`;
items (existing or added by Systems): `bloodmoss, bog_iron, peat, salt, leech, leech_sac, hexcrow_feather,
murkjaw_hide, murkjaw_tooth, frog_skin, toad_gland, thick_hide, cypress_log, slimeshell_slime, blood_essence,
core_vharra, mire_heart, trophy_maw_tooth, trophy_hex_mask, trophy_bloat_sac, maws_hook, bloats_blood, coven_key,
coven_ring, mire_witch_ring, bp_three_jaw_glaive, bp_coven_decor, bloodmoss_poultice, leech_tar, salt_pouch,
thinblood_oil, hex_ward_charm, stormglass_jar, murkjaw_arrow, swamp_lantern, sealed_waders, gorgeleech_egg,
slimeshell_egg, lore_tithe_bowl, lore_braided_cord, lore_tithe_list, lore_tally_cord, lore_first_tooth,
lore_tithe_recipe, gold_torc`.

### 2.12 Build order

1. This doc. 2. In parallel: terrain generator + swamp.json (lead); World, Systems, Creatures, Bosses, Blood path,
Audio tracks. 3. Integration: sea mask, walk map, journal map, compile until clean. 4. Landscape material
`M_SwampAuto` + headless import (one UE job, after all compiles). 5. Spot-check by reading, fix. 6. CHANGELOG, HANDOFF,
commit by name, push, LANDED.

---

## 3. Build log

1. This doc (ledger, research, plan, contracts). Shared contract `WRSwampTypes.h/.cpp` written first (FWRSwampData,
   WRSwamp::Contains / WaterAt with the Red Tide, AWRSwampState) so six tracks could build at once.
2. Six tracks launched in parallel (World, Systems, Creatures, Bosses, Blood path, Audio), each owning its files.
3. `Tools/terrain/swamp.mjs` (lead): the cliff-foot mapping from the storm heightmap; land mask (coast with tidal
   creeks, the estuary's mangrove islands, the Rime Sound neck over the island's sea to the tundra shore); per-zone water
   fields (M3 open murk between bog domes, M4 pools, M5 bleed channels between bone mounds, M2 braided channels, M1
   creeks); the Throat's terraces, mounds, rod plinths, stair and anchor wall; Mother Maw's pool and nest; the
   Ossuary's 25 m mound and ramp; the Deep Pool; the falls' plunge pools; the Stormgut scree ramp from the gut's mouth
   120 m up down to the swamp; routes (boardwalks at 2.4 m plank stations with rotten/broken planks, causeway, bone and
   root ridges); 76 kit POIs + 42 fetish-post waymarks, 34 sites, 5 shrines; gas fields; water kinds (bleedwater 13.5%,
   duckweed 18.5% of the water); arenas, the Bloat's 3.5 km loop, the Hex-Warden's walk, the candle line, the Sunken
   Coven's rooms and lanes; dressing rows (cypress 2,289, mangrove 1,745, dead cypress 864, reeds 4,450, pitcher 158,
   knees 1,659, bone piles 685, shrubs 3,334). Iterated on its preview three times (dome/mound max() swallowed all M3/M5
   water; open-sea cells counted as swamp; straight zone edges).
4. `sea_mask.mjs` (three landscapes; swamp hollows under the surface are water even where the ocean doesn't reach),
   `storm_walk.mjs` (square grown east to the swamp; shallow swamp water is wading ground), `world_map.py` (the swamp
   drawn with its water looks; the neck over the island picture's sea), `ui.mapMin/mapSize` updated,
   `import_swamp.py` + `build_swamp.ps1`, `import_storm.py` now removes only its own grid's proxies, `M_SwampAuto`.
   Layout decision: the Bloat's loop through the cook's five named stops measures 3.5 km, not the cook's 2.2 km; the
   stops win (they are what the fight and the herd beats hang on).
5. **Cut off.** The lane's process exited with the six tracks mid-work; nothing compiled or committed.
6. **Resume (build-biome-swamp-finish, 2026-11-14 evening).** Inventory against the ledger (three read-only sweeps):
   data, audio (14 beds, 18 music cues, 256 voice files, credits), the scenery builders, the creatures file and the
   Blood path were real; missing were all of `WRSwampWorld.cpp` (spawner, boardwalks, water mats, cave tide, peat
   wall), half of `WRSwampSystems.cpp` (the subsystem, the Coven, the ways, the prompts), all ~130 boss methods, every
   hook from the game into the swamp code, the swamp dressing kinds, shrine positions, boss data rows, 25 `mire.*` sound
   rows, 7 fx rows, Blood lips/mixed looks. The forest lane's half-built files (they break the build) were moved to
   `C:\Dev\WildreachArt\forest_hold` with patches to restore them after the swamp commit. Compile errors in the existing
   files fixed (UHT `bHidden` shadow → `bConcealed`, friend decls for `FWRSwampAI`/`FWRSwampBoss`, unity-name clashes).
   Five tracks relaunched with the same file ownership: World, Systems, Creatures+voices+Blood look, Bosses (Maw,
   Warden, Bloat), Vharra (own file `WRSwampVharra.cpp`, forwarding contract `FWRSwampBoss::Vharra*`).

## 4. Spot-check and ledger proof

All in commit `ede1b5b` (origin/master). Compiled: editor, adaptive and full unity (`-DisableAdaptiveUnity`), clean.
Landscape imported (`L_Island` + `Landscape_Swamp`, M_SwampAuto). Spot-checked by reading: data files parse; every
`mire./swamp./swampboss./vharra./blood.` sound/fx id in the swamp C++ has a row; world.json keeps both tracks' edits
(5 shrines with `at`, invasions mouths/passName, Ruslana); tuning blocks present; Coven coordinates agree between
scenery and systems (`ToCoven` = +`coven.lift`); the secret ways are drawn once (scenery), anchored by `AWRSwampWays`.

| # | Proof |
|---|---|
| R1-R2 | this doc 1-2 |
| R3-R5, R8 | this table; NOT finished in 5 |
| R6-R7 | boardwalk posts/bracing `WRSwampWorld.cpp` AWRSwampBoardwalk (~125-610); rods/flood/trunk `WRSwampVharra.cpp`; Walking House, cages, chains `WRSwampScenery.cpp` BuildLandmark |
| R9 | `swamp.mjs`, `Data/swamp.json`, `Landscape_Swamp` in L_Island, `WRSurvival.cpp` biome lookup, `world.liveBiomes` |
| R10-R11 | zones/water grid `swamp.json`; water mats `WRSwampWorld.cpp` AWRSwampWaterMats (~796-935); sky `WRSky.cpp`; night lights `UWRSwampWorld::StepLights` |
| R12-R14 | `WRSwampScenery.cpp` BuildLandmark (~952), BuildSite (~1394, + 4 new sites ~3217), BuildKit (~2060); dressing kinds `WRWorldDressing.cpp` |
| R15-R16 | Coven rooms `WRSwampScenery.cpp` ~2557-3100, `AWRSwampCoven` `WRSwampSystems.cpp` ~1360-1912; caves `UWRSwampWorld::SpawnCaves`, AWRSwampCaveTide, AWRSwampPeatWall |
| R17 | `UWRSwampWorld::SpawnNodes`; `recipes.json smelt_bog_iron`; bloomery `WRSwamp::BloomeryBlocks` |
| R18-R22 | `UWRSwampSystems::StepWaders/StepGas/StepClientTells`, `AWRMarshGas`, quag `WRCharacterBase.cpp` GetSpeedMultiplier + roll block `WRAbilities.cpp`; planks AWRSwampBoardwalk |
| R23-R24 | `weather.json` swamp row; `UWRSwampSystems::StepTide`, `AWRSwampState`, `world.json events.red_tide` |
| R25-R26 | `world.json` shrines (with `at`) + `ShrinePlace`; Ruslana `MasterPlace`, `StepRuslana`, 6 quests with real events |
| R27-R33 | `WRSwampCreatures.cpp` hooked in `WRCreature.cpp` (~1229/1295/1554/2226/2770/3199), `WRCreatureCombat.cpp` (~455-1347); `attacks.json`, `creatures.json`, `spawn_regions.json swamp_m1..m5`; rite `FWRSwampAI::Feed` + `WRPlayerMenus.cpp` ~235 |
| R34-R36 | `WRSwampBosses.cpp` (Maw, Warden, Bloat, arenas, props, shots); hooks `WRBoss.cpp`, `WRBossCombat.cpp` |
| R37 | `WRSwampVharra.cpp` (look ~3177, arena ~2215, rods ~2388, flood ~2698, phases) |
| R38-R40 | `sounds.json` (14 voice sets, 25 `mire.*`), tells `WRCreatureCombat.cpp` ~455, `FWRSwampAI::Signature`; music/beds `WRAudioDirector.cpp`; `Docs/CREDITS.md` |
| R41-R42 | `Data/paths/blood.json`, `WRBloodPath.cpp`, `WRAbilitiesBlood.cpp`, `ClassForNode` `WRAbilities.cpp`; look `WRBloodPath.cpp` ApplyLook (~963-1100) |
| R43-R45 | `items.json`, `recipes.json`, `pieces.json` (bloomery, stilt_foundation), `farming.json` bog rice / bloodroot via wild plants |
| R46-R47 | lore items `items.json`; secrets `UWRSwampSystems::StepSecrets`, candle `AWRSwampCandle`, ledge/candle path scenery |
| R48 | `world.json` invasions both ways; `StepPuddles`, `FWRSwampBoss::LeadInvasion` |
| R49 | `WRPortals.cpp` swamp slots; `portals.json land: false` (hidden until a fold kit: R52) |
| R50-R51 | rows/tuning per track; server-spawned replicated actors, scenery local from data |
| R53 | garnet/oxblood/rust palettes in scenery/bosses; crimson only on unblockable tells |
| R54 | 3 camps by `import_swamp.py` (+ runtime safety net `SpawnStarts`); `fish.json` bog_eel, snakehead, old_gar (chapel pool) |
| R55-R57 | compile only, one UE job at a time; commit by name, CHANGELOG + HANDOFF; LANDED line |

## 5. NOT finished

- Creature and boss bodies are retinted existing rigs + code-built parts (no own sculpts: art PC).
- Bald cypress draws with the island's pine meshes + moss; mangrove, dead cypress, reeds, pitchers, knees, bones are
  drawn shapes until swamp art exists.
- Three-Jaw Glaive blueprint (needs the boss-weapon cook's moveset); Vharra's brood drain 6/s vs 4/s; salted weapons;
  the Bloat resets when the herd fights back with no players near; salt pouch / jar have no in-hand model; Coven pillar
  red waits for the Ring pillars; vein reach by stage and the throat-only flush need a new hero mask channel; one swamp
  combat track (no A/B/C stems, no mini intros); swamp portal folds off. All in LATER-COOKS.md.
- Not played, not packaged.
