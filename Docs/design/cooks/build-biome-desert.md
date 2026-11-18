# Build cook: build-biome-desert (the Desert / Sun & sand into Aaron's build)

Lane: build-biome-desert, 2026-11-15. Design source (SPEC for this lane, not re-decided): `cooks/biome-desert.md`
(read in full), plus `world-map.md` (B2 west of the Spine, Saltgate, the camps), `combat.md` (attack-row schema, boss
template, blind), `survival-crafting.md` (Dunewrap, sand veil, prickly pear / date palm, Sandhen eggs, oasis fish, the
tagine, rock salt / nitre, sunstone lamp, Sun mirror array, Sun court set, blueprints), LATER-COOKS lines 45-50, 55-56,
145, 180. Built on what landed: the land framework (`WRLand`, `Data/lands/`), the Ocean lane's world / scenery / bodies /
bosses pattern (`WROcean*`, which this lane mirrors file for file), the Fungal lane's caverns under the Sieves (the
Desert's north border), the Storm lane's rib spur (its south border), the combat lanes' blind status and attack rows,
the world-systems lane's shrines / masters / quests / events, the portals lane's slots, the crafting lane's Dunewrap,
Sun court and talismans.

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, the LATER-COOKS lines naming this biome, and every section of
biome-desert.md. "Plan" points into this doc; "Proof" is filled at the end (section 5).

| # | Requirement (source) | Plan |
|---|---|---|
| R1 | Full cook: research, plan, build, spot-check (brief, Aaron 11-13 night) | 1, 2, 3, 5 |
| R2 | Research names real games, places, systems and why they work, plus real-world reference (brief) | 1 |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases (brief) | 2 |
| R4 | Every build step listed (brief) | 3 |
| R5 | Design with intention: every object's job, what it connects to, how it meets ground / water / player / light / sound (Aaron 11-14) | 2.2-2.6 |
| R6 | Machines engineered, moving things move continuously on a believable mechanism (Aaron 11-14): mirrors on pivots, the gnomon's light shaft, the qanat's water, the Lookout's signal mirror, the sand clock, the Hourhold's mirror gallery, Ahrumet's gnomon torn out | 2.4, 2.11, 2.14 |
| R7 | Requirement ledger, every row proven (Aaron 11-14) | 0, 5 |
| R8 | The land on the map: belt B2 west of the Spine between the Fungal plateau and the Storm rib, Saltgate's west mouth (brief; DESERT 2.1, 3.5) | 2.1 |
| R9 | Five sub-zones D1-D5 with ground, palette, prop kits, silhouette, micro-hazard; 60-120 m blends (DESERT 2.2) | 2.1 |
| R10 | Read rules: ground language (soft gold, hardpan white, wet dark, rock), palette hexes, light (cold blue noon shadow, long dawn/dusk, bright moon on the Pan, sunstone glow, Glasstail cyan) (DESERT 1.3, 2.1) | 2.1, 2.3 |
| R11 | Eleven skyline landmarks + the Leviathan Ribs (DESERT 2.3) | 2.2 |
| R12 | POIs every ~150-250 m: 35 hand sites, 98 kit POIs, the 125 m route rule (brief; DESERT 2.4) | 2.2 |
| R13 | Decoration rule: dunes, palms, acacias, sea-grass, prickly pear, rocks, hoodoos; full, never cluttered (brief; DESERT 2.4, 3.5) | 2.2 |
| R14 | Three camps (KEPT coordinates) with safe zones, PlayerStarts and their newcomer ships (world-map) | 2.1 |
| R15 | Night rule: every POI has a light or skyline shape (DESERT 2.4) | 2.3 |
| R16 | The Hourhold: 20 rooms, three entrances with their sand rules, the Mirror Gallery, the Sun-Vault, the Spring's run-back brazier (DESERT 2.5) | 2.6 |
| R17 | Four caves C1-C4 with ore, finds, creatures (DESERT 2.6) | 2.5 |
| R18 | Resources: sunstone (60 nodes, glow by the day's sun), sunglass, rock salt, nitre, sand, copper, acacia, palm fibre, clay and red ochre, gold flakes; wild prickly pear and date palm (crops placed and obtainable) (brief; DESERT 2.7) | 2.5 |
| R19 | Hazards: the belt (vision 25 m, run x0.7, no Radiance, compass hidden), haboob weather, soft dunes, quicksand (sink, dodge lift, held, drown, ally pull) (DESERT 2.8) | 2.8 |
| R20 | Micro-hazards: brine crust, booming slipfaces, dust devils, flash floods with their tells and counters (DESERT 2.8) | 2.8 |
| R21 | No heat meter: heat waves raise thirst, shade stops it (DESERT 2.8) | 2.9 |
| R22 | Weather: Clear 50 / Overcast 5 / Heat wave 30 / Sandstorm 15, mirage water, the post-storm plenty and H-E1 (brief; DESERT 2.9; LATER-COOKS 46) | 2.9 |
| R23 | Biome event: the Long Noon (sign, sky hold for the desert only, no shade, Radiance, sunstone bloom + 20 bloom nodes, the Fata Morgana, the seven tilted stelae and caches, creature changes, Seventh Vault, the 2-minute dusk) (brief; DESERT 2.10) | 2.10 |
| R24 | Six shrines with their guards, the Great Shrine in the ruined sun-temple (brief; DESERT 2.11) | 2.11 |
| R25 | Path master Amastan (look, day and night places, the couched Dromad) and his six quests (DESERT 2.11) | 2.11 |
| R26 | Ten creatures: looks, sizes, real roots, attacks with visible contact, numbers, drops, taming (brief says 9: the cook's 10 are built) (brief; DESERT 2.12.1-2.12.2) | 2.12 |
| R27 | Real models where Fab / our packs allow, real animation, behaviour (brief) | 2.12 |
| R28 | Sand-swimming (Dunejaw, Ssarrok): soft sand only, the fin and hiss, vibration sense, burst from below, beaching on firm ground (DESERT 1.1, 2.12) | 2.12 |
| R29 | Tame utilities: Scarab dig / sense / navigate, Gildback light + Radiance, Wellfrog water, Duskjackal track, Carrion kite scout (DESERT 2.12.1) | 2.12 |
| R30 | Day / night tables per sub-zone and the living-world beats (DESERT 2.12.3) | 2.12 |
| R31 | Ssarrok: roaming route, swallow, beaching on hardpan / rock / wet sand, the Oasis water jar's wet sand (DESERT 2.13.1) | 2.13 |
| R32 | The Mirage Matriarch: the water garden, diurnal, mirages, only the real one casts a shadow, blind breaks them, sun height, overcast (DESERT 2.13.2) | 2.13 |
| R33 | The Glass Empress: six glass plates, strike part damage, frost-then-fire shatter, shards, the bulb, the Prism beam (DESERT 2.13.3) | 2.13 |
| R34 | Ahrumet: look, the Sundial Plaza, the meridian wake, sun-hot cracking and water hits, lock points and kneel, phase 1, the Uprooting, phase 2 (Noon Pulse shadow test), hidden phase 3 (Noon Lock), drops, the dawn song (brief; DESERT 2.14) | 2.14 |
| R35 | Arenas for the boss and each mini (brief) | 2.13, 2.14 |
| R36 | Creature and boss voices (fused recipes) (DESERT 2.15) | 2.15 |
| R37 | Music: 84 BPM D Hijaz cue sheet, the Wall's low-pass, the Long Noon layer, the Hourhold, camp (brief; DESERT 2.16) | 2.15 |
| R38 | The Sun & sand path: Radiance (day fill, shade, night drain, lamps, Gildback, kills, belt, Long Noon), Sandwalker, 22 nodes, 11 abilities, passives, proc feedback, combos (DESERT 2.17) | 2.16 |
| R39 | Body change: Sun thresholds 10/30/50/75/100 and the night glow (DESERT 2.18) | 2.16 |
| R40 | Dunewrap armour set placed and obtainable; sand veil; desert items (lens, jars, Dune oil, drops, flash pot, arrows, beer, talismans, ring) (brief; DESERT 2.19) | 2.17 |
| R41 | Building set: the Sun court placed and obtainable (brief) | 2.17 |
| R42 | Story only through the land; eight lore items (DESERT 2.20) | 2.2, 2.6 |
| R43 | Invasions Sun <-> Tide through Saltgate (DESERT 2.21; the Ocean side is waiting for the Desert) | 2.18 |
| R44 | Portal slots in the Desert (brief; DESERT 2.22.2: 5 slots, 2 Common, 1 Uncommon, 1 Rare, 1 high) | 2.19 |
| R45 | Desert portal fold kit: Sunwatch tomb rooms, traps, puzzles, Ushabti / flesh scarabs / tomb jackal, five rank bosses (DESERT 2.22.3-2.22.4) | 2.19 |
| R46 | Data-driven: numbers in Data/*.json and tuning; abilities as UWRGameplayAbility + ClassForNode; all damage through ApplyHit; sounds / fx as table rows (brief) | 2 throughout |
| R47 | Every path: devices (mouse + pad prompts), states, edge cases, multiplayer (server authority, replication) (brief) | 2.20 |
| R48 | No placeholders presented as finished; unfinished stays hidden (brief) | 4 |
| R49 | Gates: compile only; no tests or check scripts; one UE job at a time (brief) | 3 |
| R50 | Git: pull, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, no tags or releases, LFS sizes reported; LANDED line (brief) | 3 |
| R51 | LATER-COOKS 45 (champions on the wrong side), 46 (weather weights), 47 (Dunewrap look merge), 50 (roster of 10, no walking colossus, 6 shrines, Amastan, Gnomon Maul to the boss-weapons cook), 55 (Old Barnacle leads the Ocean side), 145 (heatwaveThirst, radianceNight, passSandSlow, afterDodge, dodgeDecoy stats read by this land), 180 (the Storm land's north edge at y -5,676 meets the Desert) | 2.1, 2.9, 2.17, 2.18 |
| R52 | Fishing: the oasis fish and the Sandfather in the Cistern Below's deepest well (SC KEPT) | 2.5 |

---

## 1. Research (build-side: how the best games make a desert work in the hand, and what we copy)

The design cook (biome-desert.md 1) researched the content (MH Wilds' Windward Plains and Balahara, BotW's Gerudo and the
Molduga, Ark: Scorched Earth, Elden Ring, Ghost of Tsushima, Memnon, Wadi Al-Hitan, desert glass, the thorny devil,
Sekhmet, real desert biology). This is the build research: how those games make the systems feel right, and the real
numbers we check our shapes against.

| Game / source | The exact thing | Why it works | What we build from it |
|---|---|---|---|
| **Journey** (thatgamecompany; GDC 2013 "Sand Rendering in Journey", John Edwards) | Sand is one height field with three normal layers: big dune shape, mid ripples aligned to the wind, fine grain; a glitter term sparkles in the sun; the ocean-like specular sheen on the lit face | The ground *reads* as sand at every distance; ripples tell wind direction, so you know where the slipfaces are | Our soft sand gets ripple noise across the east wind (the cook's KEPT world wind) baked into its height (5-15 cm at 1.2-2 m wavelength, every sample), a speck colour of bright grains, rough 0.92; slipfaces are smooth (no ripples) and a shade darker. Hardpan is flat with dark crack lines (polygons) |
| **Assassin's Creed Origins** (Ubisoft; the Great Sand Sea and Qattara) | Sandstorms are a wall you see coming, visibility collapses inside, the HUD compass still works but the world is gone; landmarks (pyramids, a lone rock) are placed at the edge of sight | A storm is a threat you plan around; the landmarks carry the route | The haboob is a moving wall seen 3 km off (fog + a dust wall drawn as a curtain of cards along its front); in the permanent belt the compass ribbon and markers hide (cook 2.8) and the landmarks we place inside (the Storm Tower's top, the Faceless Sentinels, the Last Caravan's ring) are 8-30 m so they appear at 25 m as dark shapes |
| **Breath of the Wild** (Gerudo Desert, the Molduga; Gerudo Highlands sandstorm) | Molduga swims under sand with a fin and a spray; stands on rock = safe; the Lost Woods-style storm hides the map until you follow the visible marks | One rule the player sees: sand is its water, rock is its shore | The Dunejaw's fin and hiss, its bursts on the soft-sand surface class only, beached 0.8 s and taking stance x1.3 on firm ground; the Wall Shrine is reached by following the stelae (quest 2) |
| **Monster Hunter Wilds** (Balahara; the Sandtide) | Quicksand pools are created by the monster and shown as a churning, darker ring; a sonic bomb pops a diving monster out | A tell you can learn in one fight | Quicksand: a dark glossy skin that wobbles (its own surface colour + a slow ring of grains), a wet suck 0.9 s before the sink; a loud hit (a strike-class heavy or a flash pot) during the Dunejaw's burst staggers it |
| **Dune: Awakening** (Funcom 2025; wormsign) | The worm comes for rhythm on open sand; a moving ripple and a rising rumble, then the mouth from below; rock outcrops are islands of safety | The terror is in the approach, not the bite | Sprinting on soft sand inside 40 m is a sound stimulus to every Dunejaw (walking isn't); the approach is the fin and a rising hiss; Ssarrok's breach has a 1.0 s dimple-and-pour cone and a deep boom |
| **Shadow of the Colossus** (Team Ico; Phalanx, Avion, the seated colossi) | Each colossus is a building that moves: slow, heavy, readable from the ground; its weak points glow when you're near | Scale reads as threat; the arena is the puzzle | Ahrumet stands up from its throne over 4 s (legs torn from the sand), every move telegraphed 0.8-1.5 s by the whole body; its sun-hot parts glow orange (the weak window) for 6 s |
| **Zelda: Ocarina's Spirit Temple, Twilight Princess's Arbiter's Grounds** | Sun-mirror puzzles: a shaft of light, mirrors on pivots, a lock you light; sand that flows like water in a sealed hall | Light is legible: you see the beam travel | The Mirror Gallery and the mirror relay: a visible beam (a mesh from the roof hole to each mirror, recomputed when a mirror turns 45 degrees over 0.6 s) ending on a sun-disc lock |
| **Elden Ring** (Radahn's arena; the Erdtree's light; Malenia's Waterfowl) | Arena cover that breaks; one boss move that forces a position | Positioning becomes the fight | The eye beam is a swept line trace that stops at the gnomon and the stelae (each stela takes 3 beams, then breaks into a stump that still blocks the knees down); the Noon Pulse forces you into a shadow |
| **Ghost of Tsushima** | Birds circle points of interest | The world points | Carrion kites circle a kill, a wounded beast, or you under 50% HP |
| Real: **star and barchan dunes** (Bagnold, *The Physics of Blown Sand*; NASA Earth Observatory) | Windward slopes 10-15 degrees, slipfaces at the angle of repose 32-34 degrees, star dunes 100-300 m with 3-5 arms, crests sharp | The slopes tell you where the wind blows | Crescent dunes in the Erg: a smooth 10-12 degree windward (east) back, a sharp crest, a 32 degree lee (west) slipface; the Mother Dune 140 m with three arms; slipfaces are the surface class `slipface` |
| Real: **playa desiccation polygons** (Racetrack Playa, Death Valley) | Mud cracks 0.3-2 m across in a hexagon-ish net; salt crust white over them | Reads as "hard, safe" from far | The White Pan's surface `hardpan` draws polygon crack lines (a Voronoi cell edge at 1.4 m in the heightfield as a 3 cm groove, darker speck) |
| Real: **Egyptian pylon, obelisk, colossi** (Karnak pylon proportions; the Lateran Obelisk 32 m; Colossi of Memnon 18 m seated) | A pylon is two battered towers (walls lean in 1:10) with a torus moulding and cavetto cornice; an obelisk is ~10:1 with a pyramidion; seated colossi sit on a block throne, hands on knees | Real proportions make a ruin believable | The Broken Sun Gate pylons 22 m, battered 6 degrees, torus at the corners, cavetto cornice; the gnomon 30 m basalt with a 1:10 taper, a pyramidion and a sighting hole 3 m below the tip; Ahrumet 16 m seated |
| Real: **qanat** (Iran's Gonabad qanat; Oman's aflaj) | Vertical shafts every 20-35 m down to a gently sloping tunnel; spoil rings round each shaft mouth | The line of spoil rings across the desert is the tell of hidden water | The Qanat Line: 9 shaft mouths 30 m apart, each a ring of spoil (a doughnut of rubble 1 m high, 6 m across) with a stone lip; shaft 6 open into C2 |
| Real: **date palm** (Phoenix dactylifera), **umbrella thorn** (Vachellia tortilis) | Date palm 15-25 m, a trunk of diamond leaf-base scars, a crown of 20-30 arching fronds 4-6 m, hanging orange date clusters; acacia 4-15 m, a flat umbrella crown, twisting trunks, white thorns | The two silhouettes everyone reads as "oasis" and "savanna desert" | Both modelled in code (no desert tree in the tree kit): `DsPalm` (scarred trunk, 22 fronds, date bunches) and `DsAcacia` (2-3 forked trunks, a flat crown of leaf discs) |
| Real: **haboob** (Phoenix 5 July 2011; Sudan) | A wall of dust 1-2 km tall moving 35-100 km/h at the front of a storm's outflow | Seen coming from far | The haboob front: 300 m (the cook), 12 m/s, from the Spine west; the permanent Wall a standing curtain 300 m tall at the D5 line |
| Real: **flash floods** in slot canyons (Antelope Canyon 1997; Utah's Buckskin Gulch) | The flood front arrives as a roar, then a brown surge 1-3 m with debris; rain that fell 20 km away | The danger is invisible until it isn't | The flood is a moving water volume down the wadi's spline at 6 m/s: roar 6 s ahead, a trickle 3 s ahead, a 2 m wall; ledges every 40 m |

**Lessons we build to:**
1. The ground is the language: every rule (slow, swimmers, quicksand, brine) is a surface class in the heightfield, so
   what you see is what the movement and the creatures read.
2. Every threat comes with a tell you can learn: fin + hiss, the suck, the boom, the creak, the roar, the crimson glint.
3. Shade is a thing: one trace up (30 m) answers "exposed" for thirst and Radiance alike.
4. The Wall is a door, the landmarks are the way through it.
5. Light is legible: beams, shafts and shadows are drawn, and the rules use the same lines.

---

## 2. Plan (every gap solved before building)

### 2.1 The land on the map (R8, R9, R10, R14, R51-180)

**Where, in this world.** The live world is compressed from world-map's. The Fungal plateau's south coast is at y ~
-7,540 (its south strip y -8,080..-7,540 is the "Desert's edge" dunes its script says this lane replaces); the Storm
landscape's north coast is at y ~ -5,690 (x -2,500..-200), rising to the rib spur (280 m) at y -5,420 with its two river
gaps at x -2,050 and -1,350; the Ocean land's Spine climbs from the west sea at x ~ -330 to its crest at x ~ +60, and
Saltgate's west mouth is at (-320, -6,797). The Desert fills the gap.

**Coordinate rule (decided):** the cook's y goes through the Ocean's own `Y()` (`-7955 + (yD + 8000) x 0.8673`), so the
two halves of B2 share one frame and Saltgate lines up; the cook's x through `X()` (`-3000 + (xD + 3000) x 0.947`), so the
cook's Spine foot (-150) lands on this world's (-300). Every cook coordinate goes through these two in
`Tools/terrain/desert.mjs`.

**Ground as a heightfield land** (`Data/lands/desert.json` + `desert_ground.bin`, written by `node Tools/terrain/desert.mjs`):
x0 -3,104, y0 -7,960, step 4 m, 711 samples (to x -260, y -5,120), priority 2. **Cuts:** the Fungal land's south strip
(box x -2,480..-156, y -7,800..-7,470: its beach and sea go, the dunes north of -7,800 stay the Fungal's with the
Sieves), the Ocean land's west sea (box x -600..-280, y -7,800..-5,700: the Spine face east of -280 stays the Ocean's).
**Seams:** at y -7,800 my samples are the Fungal's own heights (read from `fungal_ground.bin`); at x -280 the Ocean's
(read from `ocean_ground.bin`, Saltgate's floor included); inside the Storm frame my samples are hidden wherever the
Storm's ground stands at or above mine, and everywhere south of y -5,650 (the Storm's north coast ground rises into its
rib). Between those seams my ground blends up to theirs over 120-300 m (a mountain foot, never a cliff of samples).

| Sub-zone | Where (world m) | Height | Ground (surface classes) |
|---|---|---|---|
| D1 the Wreck Strand | west coast x ~ -2,960 to 300-700 m inland, y -7,520..-5,700 | beach 0-3, sabkha flats 0.8-2, sea-grass dunes 4-12 | `sand` beach, `wetsand` (the intertidal band and below +1.2 m), `salt` sabkha with `brine` patches, `grass` dunes |
| D2 the Erg | x -2,300..-1,000, y -7,480..-6,350 | base 18-45, crescent dunes 10-30 on top, the Mother Dune 140 | `softsand` (rippled), `slipface` (lee faces > 28 degrees), `shellgrit` in troughs, `quicksand` at the Mother Dune's foot |
| D3 Oasis Reach and the White Pan | the wadi from the coast at y ~ -6,220 east; the Pan x -1,100..-300, y -7,480..-6,220 | wadi 8-40 (gravel bed 2-4 m under its banks); the Pan flat 64 +-0.4 | `oasis` clay and grass round water, `gravel` wadi bed, `hardpan` with polygons, `salt` cakes, `quicksand` at oasis margins and wadi beds |
| D4 the Red Canyons | x -2,300..-300, y -6,220..-5,650 | mesa tops 70-95 cut by slot canyons 14-30 deep, falling to 30 at the Storm seam | `redrock` mesa, `slot` sand floor, `varnish` (dark streaked walls: slope rock tinted purple-black), `glass` near the Spine, `nitre` crusts |
| D5 the Haboob Wall | y -7,800 (the seam) .. -7,480, x -2,300..-300 | 40-70, scoured | `pavement` (desert pavement: dark polished pebbles), `blown` sand drifts, `glass` fulgurite field |

Wadis: the main wadi (D3) from the coast (-2,960, -6,220) east by the Sevenfold Wells to the Pan's west shore, 30-60 m
wide, its bed 2-4 m under its banks (gravel), and the **two flood wadis** from the Storm rib's river gaps (x -2,050 and
-1,350 at y -5,650) north 600 m into the main wadi: slot canyons 6-12 m wide where they cut the D4 mesa, with ledges.

Dunes (KEPT world wind from the east): crescent dunes whose backs face east (10-12 degrees) and horns point west;
slipfaces west at 32 degrees. Heights from a dune function (rows of crescents on a 160-220 m spacing jittered by noise,
amplitude by sub-zone: D2 18-30 m, D1 4-12 m, D5 drifts 3-8 m). The Mother Dune at (X -1,450, Y -7,250) = (-1,532,
-7,304): a star dune 140 m with three arms at 20, 140 and 260 degrees, each arm's crest sharp.

**Camps (world-map KEPT, through X/Y):** (-2,571, -6,134) the Caravan camp, (-2,571, -6,828) the Strand camp, (-2,382,
-7,608) the Wall camp. Each on a flat sea-grass dune top 6-10 m up, its newcomer ship below it on the beach (world-map
rule: every camp has its ship), a safe zone 35 m, PlayerStarts tagged `desert` (the Sun path), the starter stations at
the middle one.

**Read rules (R10):** zone ground colours from the cook's palette (dune gold #D9A85B, hardpan white #EDE6D6, canyon red
#A8462E, oasis green #4E7A3A); the noon shadow tint (cold blue #3B4A6B) is the sky's: WRSky's desert air sets a cool
shadow colour by day in the desert (the sky's per-biome air row). Nights: the Pan's white reads under the moon (rough 0.6
so it catches it), sunstone nodes glow amber (their emissive by the day's sun hours: brightness = sunHours / 14, KEPT),
Glasstails cyan (their night layer).

### 2.2 Landmarks, sites, kit POIs, decoration (R11-R13, R42, R5)

Code-built on the forest's FBuild (as every land since the Forest), kit `WRDesertLook.h` (helpers `Ds*`). Every piece
answers "why here, what it connects to, how it meets the ground": everything stands on `B.G()` (the land's ground), is
sunk into sand where sand would bury it (a buried thing is drawn whole and the ground covers it: the sand line is the
land's surface, so hulls, caravans and stelae are half under it with drifted sand ramps against their windward side),
and every ruin is Sunwatch sandstone (#C8A070) or basalt with bronze, never generic.

**Landmarks (cook 2.3; built at real scale; each has a night light or a skyline shape):**
- **L1 the Haboob Wall:** a standing curtain 300 m tall along y -7,478 from x -2,300 to -300: 4 staggered rows of
  vertical dust cards (code mesh quads, brown #6E4E32 -> rust #8C5A36 at the top, alpha by height) with a cauliflower
  top of 30-60 m puffs, slowly scrolling west->east (a pure function of the server clock: never snaps). At night the
  lightning flicker inside: the Wall's own light pulses (sky-blue, 0.08 s flashes every 3-9 s) behind the cards. Collision
  none (it's air). The belt behind it is the hazard volume (2.8).
- **L2 Ahrumet's Gnomon:** in the plaza (2.14): the 30 m basalt obelisk, 3 m square at its foot tapering to 1.8 m, a
  bronze-capped pyramidion, the sighting hole 1.2 m across 3 m under the tip (bronze-lined, so it catches the sun), and
  the seated colossus on its throne at the north edge (the boss actor sits there; until the boss exists the throne is
  scenery). Night: the bronze cap and the hole catch the moon (bright metal), and the colossus's eye glows dull amber.
- **L3 the Broken Sun Gate:** two 22 m pylons 18 m apart (battered 6 degrees, torus mouldings at the corners, cavetto
  cornice, a sun disc with two wings carved high on each), the lintel fallen across the gap in three pieces, the Great
  Shrine's brazier in the court behind; Amastan's shade awning against the south pylon. Night: the brazier.
- **L4 the Mother Dune:** terrain; a ring of 3 wind-sculpted ventifacts at its east foot and the Lee Door (H-E1) set into
  its lee face (a carved lintel 4 m wide, buried, that the sand reveals after a storm).
- **L5 the Whale Road:** 9 whale skeletons along a 600 m line from (-1,890, -7,066) to (-1,322, -6,893), each 20-40 m:
  a skull (rostrum, the braincase), 40-60 vertebrae as a line of discs half in sand, ribs as arcs rising 3-8 m, flipper
  bones. Built from `DsWhale` (a code mesh of bone). The Erg Shrine is on the biggest skull.
- **L6 the Keelyard:** 12 sun-barque hulls (cedar #9A5836, high curved prow and stern posts ending in papyrus-bud
  finials) stood on end in an arc like a fence, 8-14 m above the sand, each sunk 3 m.
- **L7 the Tall Wreck:** a newcomer three-master (32 m hull, masts at 30 degrees) on a dune crest; the rope ladder from
  the rail, the hold of empty casks (bungs out), 6 driftwood crosses facing the sea. The Strand camp's ship.
- **L8 the Fallen Hour:** a 12 m colossus head on its side on the Pan, sand to the eyes, the face broken, ropes still
  round the neck (the tow-sledges under it), the Jaw entrance (H-E2) under the chin.
- **L9 the Needles:** 34 hoodoo spires (stacked red sandstone drums with caprock, 30-80 m), kite nests on the tops, two
  rope bridges between the tallest. Each spire a collision column.
- **L10 the Glass Canyon:** the canyon mouth's walls lined with green-black glass sheets (FSurf::Glass) that flash in
  the sun; the Empress's lair (2.13).
- **L11 the Sevenfold Wells:** 40 date palms round a stone qanat tower (12 m, square, a bucket wheel on top), a walled
  water garden 70 m across of channels and seven stone-lipped pools (the Matriarch's arena, 2.13).
- **The Leviathan Ribs:** at Saltgate's west mouth (-300, -6,828): a 90 m sea-serpent skeleton bleached chalk-white, its
  vertebrae in an S through the pass mouth, bronze spear heads in the spine.

**Hand sites (35, cook 2.4 table, each built as the table says, placed through X/Y):** S1-S35 kinds as their ids. Each
is a function in `WRDesertSites.cpp` that builds exactly the scene in the table (the props named there, nothing else),
every take / cache / lore item a server spot (2.7).

**Kit POIs (98 + the route rule + the density fill, the cook's table by sub-zone):** `wreck_fragment` (newcomer or
ancient, 3 variants), `caravan` (half-buried: wagon or pack train, 3), `stela` (gnomon stela waymark on the old roads,
4 m, leaning in its variant, 2), `ruin` (wall / hut / cistern, 3), `outcrop` (sunstone / copper / salt / sunglass:
what it holds is its node, 4), `creature_set` (nest / kill / burrow / herd at water, 4), `seep` (a palm or two and a
spring, 2), `cave_mouth` (a slot entrance, 2), `bones` (whale / Dromad / giant, 3). Poisson >= 150 m per sub-zone; the
route rule along the wadi road, the Avenue of Hours, the caravan routes and the stelae roads: any point > 125 m from a
POI gets the fitting kit; the density fill (> 200 m anywhere).

**Decoration rule (R13):** per surface: sea-grass tufts on `grass` dunes (22%), driftwood on the beach band, saltbush on
the sabkha, date palms and reeds round water (`oasis`), acacias on the wadi banks and D3/D4 flats (2.5%), prickly pear
clumps on D3/D4 (1.5%), red rocks (the SimpleRocks set tinted #A8462E) on `redrock` (1.2%), black pebble scatters on
`pavement`, hoodoo stubs in D4. One decoration actor per 250 m cell (as the Ocean), instanced parts.

### 2.3 Night lights (R15)

Every POI: a lamp or a skyline shape. Shrines: braziers (orange); camps: newcomer lanterns; ore outcrops: the sunstone
glow (amber, `N` blocks); oases: the pools catch the moon (glaze surface); Sunwatch waystations: a bronze lamp niche; the
Wall: its lightning; the Hourhold: lit throughout by sunstone sconces (it's dark down there: always on).

### 2.4 Moving machines (R6)

Everything that moves is a pure function of the server clock (or a replicated time), turned every frame on every
machine: the Lookout's signal mirror swings on its pivot toward Saltgate's mouth (it tracks the sun's bearing across the
day, so the flash points at the pass at 15:00; X3 below), the qanat tower's bucket wheel turns (6 rpm, its chain of
pots dipping into the shaft), the Wall's cards scroll, dust devils wander (their path a function of time), the
Hourhold's Mirror Gallery mirrors turn on their bronze pivots (45 degrees over 0.6 s from a replicated angle), the
gnomon's light shaft is drawn from the hole to the throne when someone stands on the meridian strip. Ahrumet's gnomon
leaves its socket on the boss's hand (2.14).

### 2.5 Caves, resources, crops, fish (R17, R18, R52)

Caves (cook 2.6) as the Ocean's: rock round rooms (the scenery's code mesh), ways in the land's `underground.ways` (no
rescue there), ore nodes inside: C1 Nitre Caves (copper, tin, nitre), C2 the Qanat (copper, clay, a flooded run you swim:
a water volume; catfish), C3 Ssarrok's Hollow (sunglass, gold flakes, sand floor: `softsand`, Dunejaws), C4 the Sieve
Throats (iron, sunglass, a deepcap patch; opens down into the Fungal caverns' halls under the Sieves).

**Nodes (tuning `desertWorld.nodes.<item>`: give, hits, perHit, respawn):** sunstone 60 on sun-facing D3/D4 rock and the
Pan's salt cracks (+50% yield the block after a sandstorm; x3 in the Long Noon; glowing at night by the day's sun),
sunglass (S33's field, the Glass Canyon, C3/C4), rock salt (the Pan's cakes, D1's sabkha), nitre (D4 crusts, C1),
copper (D4 malachite seams, C1, the Hourhold), acacia (the acacias are trees: gathered as wood `acacia_log`), palm fibre
(date palms), clay and red ochre (oasis banks, wadi cuts), gold flakes (wadi gravel, C3), sand (any soft sand: the hand
gathers it). **Wild crops:** prickly pear (D3/D4 clumps: a node that gives the fruit and its seeds 30%) and date palms
(the oasis palms give dates and seeds 6%; KEPT wildSeeds). **Fish:** oasis catfish and sun koi at the oases, the
Sandfather in the Cistern Below's deepest well (fish.json `desert` waters already: the cistern is a water volume tagged
for it).

### 2.6 The Hourhold (R16, R42)

At (X -1,500, Y -7,000) = (-1,579, -7,087), floor levels 20-70 m under the Erg's surface. Built like the Ocean's Drowned
Hall: rooms as rock-walled chambers (code mesh) at given depths, joined by corridors; sunstone sconces light them.
Rooms 1-20 as the cook's table (the Hall of Hours' 24 pillars and moving light shafts from roof holes: the shafts are
drawn from the real sun's direction; the Cistern Below a water volume; the Mirror Gallery's 4 mirrors and the sealed
Sun-Vault door; the Drowned Stair's quicksand; the Seventh Vault's door open only in the Long Noon; the Collapse's rope;
the Workshop of Faces with Ahrumet's unfinished face; the Lens Forge; the Spring with its run-back brazier). Entrances:
H-E1 the Lee Door (open for 3 game hours after every Sandstorm block: its sand bank slumps away like the Ocean's Low
Gate), H-E2 the Jaw (a quicksand floor before it), H-E3 the Dig (hold E 4 s with a shovel or any pick to clear the pit;
once per server, then it stays open). 12 Ushabti in room 9 are a fight: portal-creature stats (midland), built in the
creatures step. Lore items (8): 4 tablets (room 3), the expedition journal (S13), the Glass Scarab (room 14), the eye
mould (room 19), the last king's sun disc (room 16).

### 2.7 Spots, caches, secrets (R42)

`AWRDesertSpot` (the Ocean's spot, its kinds): take, lore, dig (H-E3, the Scarab's dig spots: 1 per 200 m over D1-D3 as
sand mounds; a tamed Scarab digger near shows them and digs one up), mirror (the Mirror Gallery), stone (the seven
tilted stelae's caches, Long Noon only), squeeze (a Wellfrog in its clay: thirst +30, 2 game hours each), lens (S10 and
S32's lens recipe), rubbing (S25's blue hands for quest 3), runback (the plaza's stone), meridian (the plaza's strip).
Caches: wrecks, caravans, the Sun-Vault (sealed until the gallery's beam reaches it), the Seventh Vault, the seven tilted
stelae's caches. Secrets (the cook's X3 signal mirror at the Lookout: at 15:00 clear, turning it flashes Saltgate's mouth
shrine and opens a ledge cache).

### 2.8 Hazards (R19, R20)

`UWRDesertHazards` (every machine, numbers in tuning `hazards.desert`): **the belt** is a pure function (inside the D5
polygon or the Fungal strip north of it): vision 25 m (the sky's fog distance in the belt, WRSky desert air), run x0.7 and
sprint stamina x1.3 through the movement component's surface multiplier, Radiance fill 0, the compass ribbon and
waypoint markers hidden unless you hold the Sunwatch lens or your tamed Scarab digger is within 15 m (the HUD asks
`WRDesert::CompassHidden`). **Soft sand** (`softsand`, `slipface`, `blown` surface classes): run x0.85, sprint stamina
x1.3; the Dromad and Shimmerbuck ignore it; Sandwalker, the sand veil (halves the slow) and Dune oil (x0.85 storm slow)
counter. **Quicksand** (`quicksand` class): the suck (0.9 s) then sinking over 4 s (the capsule lowers through the ground:
a sink amount replicated on the character, movement x0.3), each dodge lifts 25%, at full you're held (no actions) and
drown 5 s later (ApplyHit, kind `drown`); an ally holds E 2 s on you to pull you out. **Brine crust** (`brine`): creak
1.0 s then the break: you drop 1.5 m (the crust is a skin over a 1.5 m pit: the ground there is the pit floor, the crust
a breakable scenery slab per patch), run x0.4, no roll, 4/s until you climb out (2 s at the edge). Rope planks
(pieces.json `rope_plank`) bridge it. **Booming slipfaces:** standing or running on a `slipface` sample for 1.2 s starts
the boom (100 Hz) and the crest grains flowing, then an avalanche carries you 12 m down-slope (a launch along the fall
line), buries you 2 s (stamina -40, 20 damage), and pops any Dunejaw within 20 m. **Dust devils:** 2-4 on the Pan
11:00-16:00, their paths pure functions of the server clock (a slow figure-eight over the Pan at 4 m/s), a spinning
column of sand cards 3 m wide 20 m tall; touching one lifts you 1.2 s and throws you 6 m (20 damage, blind 30) and
scatters loose ground items (outside claims). **Flash floods:** after any rain / storm block in the Storm peaks (the
weather's Storm-peak state) the two flood wadis run: the roar 6 s ahead, a trickle 3 s ahead, a 2 m brown wall moving
down the wadi spline at 6 m/s (a water volume that moves: its front a function of the replicated start time), 50
damage at the front, carried 15 m with 3-5 debris hits of 10; ledges and rope points every 40 m.

### 2.9 Weather and shade (R21, R22, R51-46)

`Data/weather.json` desert: Clear 50 / Overcast 5 / Heat wave 30 (thirst x2 in sun, Radiance x1.2, mirage water on the
Pan) / Sandstorm 15 (the haboob front from the Spine, the belt's effect over the whole desert, the next block's sunstone
+50% and H-E1's opening). Shade: "exposed" = no blocking hit in a 30 m trace straight up (roofs, overhangs, palm crowns:
the palms' crowns have a no-collision shade box the trace hits on its own channel); the heat wave's thirst asks it.

### 2.10 The Long Noon (R23)

world.json `events.long_noon` (scheduled by the world systems like the King Tide): sign at -5 min (the sun goes white,
the shimmer column over the Pan, every gnomon stela's shadow shrinks). For one game day the desert's sky holds at 12:00
(WRSky's per-biome hour override for `desert`, blended over 300 m at the border); shade only under roofs; Radiance never
drains and fills x1.5; every sunstone node regrows at x3 yield and 20 bloom nodes rise on the Pan; the Fata Morgana: an
upside-down image of the Hourhold's towers in the sky over (-1,579, -7,087) (a code mesh of towers, inverted, faded by
distance, gone within 300 m); the seven tilted stelae cast the only shadows at true noon, each pointing to a cache (a
spot open only during the event); Glasstails and Dunejaws dig in (half spawns), Carrion kites x2 and circling hurt
players, Gildbacks bask (tame x2); Ahrumet's part chance doubles; after it a 2-minute dusk.

### 2.11 Shrines, Amastan (R24, R25)

world.json shrines `desert_*` (6, `land: desert`), guards as the cook's table. Amastan in world.json masters (path `sun`,
`land: desert`, at the Great Shrine; MetaHuman male look as the hero rig allows: the hero's male body with the
tagelmust as a head cloth, white tunic, the takouba on the belt; his night place on the pylon top), his 6 quests in
world.json quests.amastan (events: water_jars, belt_walk, blue_hands, shadow_road, show the Empress stinger, kill
Ssarrok).

### 2.12 The creatures (R26-R30)

Ten creature rows in creatures.json (`biome: desert`), their attack rows in attacks.json, numbers in tuning
`desertLife`. **Bodies:** no desert animal in our packs except the fox: the **Duskjackal is the aurora_fox rig** (real
clips) tinted black with gilt dust and scaled to 0.9 m at the shoulder; the other nine are code-built bodies posed
every frame like the Ocean's (`WRDesertBeast.cpp`, kinds by `procBody`): `scorpion` (Glasstail; the Glass Empress at x4),
`vulture` (Carrion kite), `skink` (Dunejaw with its sail fin; Ssarrok at x3.7), `devil` (Gildback: spines, false-head
hump), `camel` (Dromad: hump, dulla sac), `oryx` (Shimmerbuck; the Matriarch at x2), `scarab` (Scarab digger with its
ball), `sandgrouse` (Sandhen), `frog` (Wellfrog). **Brains:** `FWRDesertAI` hooks beside the Ocean's in AWRCreatureAI
(fin stalk + burst from soft sand only, re-dive, vibration sense from sprinting players; the Glasstail's glare by
camera facing; the kite's circle and call; the jackal's flank hamstring and chorus; the Gildback's false head and roll;
the Dromad's dulla spit; the Shimmerbuck's mirage; the Scarab's ball). **Populations:** `UWRDesertLife` spawns by
sub-zone with the cook's day / night tables (tuning `desertLife.tables`), the living beats (Dunejaws hunting
Shimmerbucks, jackals harrying Dromads, kites circling kills, Gildbacks basking at noon, Sandhen flights at dawn, the
Dromads turning east at Ahrumet's dawn song). **Taming** by the cook's methods; utilities as 2.12.1.

### 2.13 The minis (R31-R33, R35)

`WRDesertBosses.*` (`FWRDesertBoss` beside `FWROceanBoss` in the boss machine). **Ssarrok** on its route (the Erg
loop, down to the strand's wet sand at dusk, resting in C3 by day in a heat wave): fin wake, breach, swallow (inside 4 s,
throat lock point, mash), tail breach ring, geyser roar; beaches on any non-soft-sand ground and on wet sand (the Oasis
water jar's splash makes 6 m of wet sand for 30 s: a temporary surface override the Dunejaw / Ssarrok read). **The
Mirage Matriarch** in the Sevenfold Wells' garden (diurnal; at night asleep in the deepest pool and unfightable): 3
mirages (no shadow: `bCastShadow` off), 50% damage, one-hit pop, reform 12 s; blind on the real one breaks all for 15 s;
crossfire charge from four angles. **The Glass Empress** in the Glass Canyon: six plate parts (300 each, x0.3 while on,
strike x2 part damage, frost then fire within 4 s shatters, shards 15 in 4 m), the Prism beam (daylight, rock blocks it).

### 2.14 Ahrumet and the Sundial Plaza (R34, R35)

The plaza at (X -600, Y -6,600) = (-727, -6,741): a 140 m limestone disc flattened into the Pan at 64 m, sand drifting
over its edge; the gnomon (2.2); 12 hour-stelae 4 x 2 m on a 50 m ring (actors with 3 beam-hits each); the throne on the
north edge; the bronze meridian strip from the gnomon to the throne; the run-back stone at the Avenue of Hours' foot
50 m south-west. The fight as the cook's 2.14 tables (phase 1 beam / stomps / palm sweep / grab / sand wave / Gaze; the
Uprooting at 50%: the gnomon actor attached to its right hand, the centre collapsing into a 20 m quicksand funnel;
phase 2 club combo / Noon Pulse with the shadow test (a trace from the pulse's sun direction to each player: blocked by
the gnomon, a stela or Ahrumet = safe) / held beam / Glassfall; hidden phase 3 behind the artifact flag). Sun-hot parts
(orange glow 6 s after each beam and pulse) crack under water hits (the Oasis water jar, Tide, frost at half). Drops:
the core, radiant essence, sunstone, the Colossus eye part chance, the Sun court furnishings blueprint (set rule), the
trophy head. The Gnomon Maul blueprint waits for the boss-weapons cook (LATER-COOKS 50: no weapon behind it yet, so not
dropped). The dawn song: a world sound at 06:00 heard within 2 km.

### 2.15 Sound and music (R36, R37)

Rows in `Data/Audio/sounds.json`: every creature's voice set, the bosses', the hazards' tells (suck, creak, boom, hiss,
roar), the beds `land.desert` (day / night) and `zone.D1..D5`, the Wall's storm bed, the Hourhold's bed, music pools
(explore day / night, combat, mini, camp, the Wall low-passed, the Hourhold, the Long Noon layer, Ahrumet's three
phases). Stand-ins from the repo's recordings where no real recording exists (`_standin: true`, as every land lane has
done); the cook's 84 BPM D Hijaz cue sheet is the audio cook's.

### 2.16 The Sun path and the body (R38, R39)

`Data/paths/sun.json` (fire.json's schema, the cook's 3.1 row), `WRSunPath.*` (Radiance on the attribute set beside Heat
/ Chill / Flow..., its tick on the server every 0.25 s: exposure trace, day / night from the sky, weather, the belt, the
Long Noon, lamps / Gildbacks / Beacon in 6 m, kills, Dawn; Sandwalker flags the movement component reads; Gleam on
parry; Zenith; Searing light; Brilliance / Sun-forged in the damage pipeline; Blind chance), `WRAbilitiesSun.*` (the
eleven: Sunbeam, Solar flare, Beacon, Sunburst, Sandstorm, Desert's wrath, Burrow, Quicksand, Sun strike, Blazing arc,
Noon strike) registered in `ClassForNode` "sun", the six combos with Fire / Lightning / Nature / Frost / Blood /
Necromancy, the HUD's Radiance bar (Sun gold #E8B230, dimming edge at night), the look (player_art.json paths.sun:
skin warmth, gold flecks, hair tips, iris).

### 2.17 Gear, items, the Sun court (R40, R41)

Items and recipes the cook lists that don't exist (sunglass, nitre, acacia log, palm fibre, red ochre, gold flakes, the
creature drops, the lens, water jar, Oasis water jar, Dune oil, Clear-eye drops, flash pot, sunglass arrows, red ochre
beer, sand veil, Empress's lens ring, the core and eye, the trophy head, the 8 lore items); talismans exist (crafting
lane), their effects read `GearStatFor` (afterDodge, dodgeDecoy, crit blind). Dunewrap and the Sun court exist and become
obtainable as the desert goes live (sunstone, Dromad leather, sandstone blocks; the Sun court's boss is Ahrumet). The set
stats this land reads: heatwaveThirst, radianceNight, passSandstorm, passSandSlow.

### 2.18 Invasions (R43)

world.json invasions: the Desert -> Ocean row (Duskjackal packs at night, Glasstail-driven Dromad stampedes by day,
Dunejaws, Ssarrok leading), live with the Desert; the Ocean -> Desert row already written by the Ocean lane wakes now
(Old Barnacle leads: LATER-COOKS 55).

### 2.19 Portals (R44, R45)

Slots in desert.json (5: 2 Common in D1 / D2, Uncommon D2, Rare D3, high D4 or the Hourhold's edge). The fold kit
(`Data/portal_rooms/desert.json`: the Sunwatch tombs) on the portals lane's room schema with the Desert's traps and
puzzles, Ushabti / flesh scarab / tomb jackal and the five rank bosses; `portals.json desert land:true` only once the kit
and its bosses exist (until then the slots stay dark, as the Ocean's do).

### 2.20 Multiplayer and devices (R47)

Server authority: nodes, spots, caches, hazards' damage, sinking, the belt's rules, bosses; replicated state on
`AWRDesertState` (the Lee Door's open-until, the Long Noon stage, flood start times, dust devil seed, the gallery's mirror
angles, the Sun-Vault's opened, the H-E3 dig done); everything that moves on clients is a function of that state and the
server clock. Prompts through WRWorldNpc's hooks (E / pad X shows the same prompt, "world desert spot").

---

## 3. Build steps (each piece compiled, committed, pushed)

1. **Desert (1) the land and its places**: `desert.mjs` -> land + `desert.json`; `FWRDesertData`; `UWRDesertWorld` +
   `AWRDesertScenery` (landmarks, sites, kit, decoration, caves, the Hourhold's rooms), nodes / caches / spots, crops;
   the world.json biome row; weather rows; not live yet.
2. **Desert (2) the Sun path**: sun.json, Radiance, abilities, passives, combos, HUD, look.
3. **Desert (3) the creatures**: bodies, AI, rows, populations, beats, taming; `desert` joins `liveBiomes`.
4. **Desert (4) the world**: hazards, weather, the Long Noon, shrines, Amastan + quests, the Hourhold's ways and
   secrets, items / recipes / gear, invasions.
5. **Desert (5) the bosses**: three minis, Ahrumet, the plaza's machines.
6. **Desert (6) sound and music** rows; the portal slots / fold kit; CHANGELOG / HANDOFF; LANDED.

---

## 5. Proof (filled at the end; commits on master, files in C:\Dev\Wildreach)

Commits on master: 681b139 Desert (1) the land, 7eca241 Desert (2) the Sun path, a14bccc Desert (3) the creatures (the Desert goes
live), 09c2451 Desert (4) the world, 8c1f73a Desert (5) the bosses, 0510ab5 Desert (6) the sound (+ a final text fix). Each piece
compiled (editor target) before it was pushed; the merged tree compiled after each pull. Not played, not packaged (gates:
compile only).

| # | Done? | Where |
|---|---|---|
| R1 | Yes | This doc 1-5; six pieces built and compiled |
| R2 | Yes | 1 (Journey, AC Origins, BotW Molduga, MH Wilds, Dune Awakening, SotC, Zelda, Elden Ring, Ghost of Tsushima; real dunes, playa, qanats, haboobs, flash floods) |
| R3 | Yes | 2.1-2.20 |
| R4 | Yes | 3 |
| R5 | Yes | `WRDesertScenery.cpp`, `WRDesertSites.cpp` (every place modelled at real scale, ground-matched by `B.G`) |
| R6 | Yes | `AWRDesertScenery::Tick` (`WRDesertWorld.cpp`: qanat wheel, Lookout signal mirror, Mirror Gallery pivots, Sun-Vault and Seventh Vault doors, Lee/Dig sand, light shafts, the gnomon hidden while Ahrumet carries it), `FWRDesertBoss::PhaseChanged` (the Uprooting) |
| R7 | Yes | This table |
| R8 | Yes | `Tools/terrain/desert.mjs` -> `Data/lands/desert.json` (cuts against Fungal and Ocean, seams to Storm) |
| R9 | Yes | `desert.json` zones D1-D5 + `WRDesert::ZoneAt` (`WRDesertTypes.cpp`) |
| R10 | Partly | Ground colours and the surfaces in `lands/desert.json`; sky haze and the belt's dust in `WRSky.cpp` (Desert block); Glasstail cyan and sunstone glow (`WRDesertBeast.cpp`, `UWRDesertNodeGlow`). The cold-blue noon shadow tint isn't set (the sky has one sun for every land) |
| R11 | Yes | `WRDesertScenery.cpp` landmarks (12) |
| R12 | Yes | `desert.mjs`: 35 sites, 97 kit, max POI gap 176 m |
| R13 | Yes | `WRDesertScenery.cpp` Decor cells (palms, acacias, grass, pear, rocks, hoodoos, shells) |
| R14 | Yes | `lands/desert.json` camps (3) + `WRDesertScenery.cpp` camps and their ships |
| R15 | Yes | Night blocks (`B.N`) on every place in `WRDesertScenery.cpp` / `WRDesertSites.cpp` |
| R16 | Yes | `desert.json` hourhold + `WRDesertScenery.cpp` rooms / links / ways; `AWRDesertState` (Lee Door, Dig, Gallery, Vault, Seventh Vault) |
| R17 | Yes | `desert.json` caves C1-C4 + `WRDesertScenery.cpp` caves |
| R18 | Yes | `UWRDesertWorld::SpawnServerActors` nodes (tuning `desertWorld.nodes`), sunstone glow by the day's sun (`UWRDesertNodeGlow`), crops (prickly pear, date palm seeds in items/recipes) |
| R19 | Yes | `WRDesertHazards.cpp` (`StepBody`: belt, soft sand, quicksand with dodge lift / held / drown / ally pull), `WRUIHud.cpp` compass, `WRSky.cpp` belt fog |
| R20 | Yes | `WRDesertHazards.cpp` (brine, slipfaces, `DevilAt`, `StepFloods`) |
| R21 | Yes | `weather.json` heatwave (thirst x2, sunOnly, shade counters); no heat meter |
| R22 | Partly | `weather.json` desert row (Heat wave 30, Sandstorm 15, Overcast 5, Clear the rest); post-storm plenty and H-E1 in `UWRDesertWorld::StepServer`. Mirage water on the Pan isn't drawn |
| R23 | Yes | `world.json` events.long_noon; `UWRDesertHazards::StepLongNoon`, `WRDesertHz::SkyHour`, the Fata Morgana (`StepClient`), the Seventh Vault spot + door |
| R24 | Yes | `world.json` shrines desert_* (6) |
| R25 | Partly | `world.json` masters amastan + quests.amastan (6). His night place on the pylon and the couched Dromad aren't placed (masters have no night places yet) |
| R26 | Yes | `creatures.json` (10), `attacks.json`, `WRDesertCreatures.cpp`, `WRDesertBeast.cpp`, drops in `items.json` |
| R27 | Partly | Duskjackal on the Barghest rig and Carrion kite on the Griffon rig with their clips; the other eight are code-built bodies posed every frame (no desert animals in the packs) |
| R28 | Yes | `FWRDesertAI` (dunejaw dive / vibration / burst / beach), `CanDive` (soft dry sand only, wet sand from a jar blocks it) |
| R29 | Yes | `FWRDesertAI` tame jobs + `WRDesertHz::CompassHidden` (Scarab), `WRSunPath` (Gildback Radiance), Wellfrog water product |
| R30 | Yes | tuning `desertLife.tables` + `UWRDesertLife::StepBeats` |
| R31 | Yes | `WRDesertBosses.cpp` Ssarrok (route / dusk / lair, swallow, beaching), Oasis water jar (`WRDesertHz::ThrownLanded`) |
| R32 | Yes | `WRDesertBosses.cpp` Matriarch (asleep at night, mirages without shadow, blind breaks, overcast halves the re-form) |
| R33 | Yes | `WRDesertBosses.cpp` Empress (plates, strike x2, frost-then-fire, shards, bulb, Prism beam) |
| R34 | Partly | `WRDesertBosses.cpp` + colossus body in `WRDesertBeast.cpp`: meridian wake, sun-hot cracks, phase 1, the Uprooting + funnel, phase 2 with the Noon Pulse shadow test, the Noon Lock, drops. The ring waves are instant rings (not travelling fronts) and Glassfall leaves no slick glass |
| R35 | Yes | `UWRDesertArenas::Place` (all four), the plaza's 12 `AWRDesertStela` |
| R36 | Partly | `sounds.json` creature families + desert moments: stand-ins from the repo's recordings |
| R37 | Partly | `sounds.json` pools + `WRAudioDirector.cpp` (zone pools, the Wall's low-pass, the Long Noon layer, the Hourhold, camp): stand-ins; no D Hijaz score |
| R38 | Yes | `Data/paths/sun.json`, `WRSunPath.*`, `WRAbilitiesSun.*`, combos. Desert bloom, Desert graves and Sacrifice to the sun aren't built |
| R39 | Yes | `WRHeroLook` sun look (Desert 2) |
| R40 | Yes | Dunewrap recipes (crafting lane) obtainable now the Desert is live; sand veil, lens, jars, Dune oil, drops, flash pot, arrows, beer, talismans, ring in `items.json` / `recipes.json` |
| R41 | Yes | `building_sets.json` sun_court + `sandstone_block` recipe; Ahrumet drops its furnishings blueprint |
| R42 | Yes | Places tell it (`WRDesertSites.cpp`); lore items in caches and spots |
| R43 | Yes | `world.json` invasions pair desert -> ocean (Ssarrok leads); the ocean -> desert row wakes |
| R44 | Partly | `desert.json` portalSlots (5); `portals.json` desert stays land:false (dark) until the kit exists |
| R45 | No | The Sunwatch tombs fold kit isn't built (LATER-COOKS) |
| R46 | Yes | Numbers in tuning (`desertWorld`, `desertLife`, `hazards.desert`, `desertEvents`, `bosses.*`, `sun.*`), abilities in `ClassForNode`, every hit through `ApplyHit`, fx/sounds as rows |
| R47 | Yes | Server-authoritative state on `AWRDesertState` (replicated), replicated tags for the slows, prompts through `UWRDesertWorld::Prompt` (E / pad X) |
| R48 | Yes | Unbuilt things are listed here and in the LANDED line, not shown as finished |
| R49 | Yes | Compile only; no tests or scripts run |
| R50 | Yes | Pulled before work and before each push, files added by name, CHANGELOG Unreleased + HANDOFF in each commit, no tags; `desert_ground.bin` 1.45 MB (plain git), no LFS files |
| R51 | Yes | 46 (weather weights), 50 (roster, 6 shrines, Amastan, no Gnomon Maul drop), 55 (Old Barnacle's row wakes), 145 (gear stats read: passSandSlow, passSandstorm, radianceNight, critBlind), 180 (Storm seam at y -5,652) |
| R52 | Yes | `fish.json` desert rows incl. the Sandfather (already present) |
