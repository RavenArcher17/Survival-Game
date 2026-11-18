# Build cook: build-biome-sky (the Sky islands into Aaron's build)

Lane: build-biome-sky, 2026-11-15. Design source (SPEC for this lane, not re-decided): `cooks/biome-sky.md` (read in full),
plus `world-map.md` (B1 Hollow Cape, the Flue, Kitewright lifts, glider table, updraft table, weather row), `combat.md`
(attack-row schema, boss template, knockback, fall damage), `survival-crafting.md` (Skysilk, anchor boots, crops, Windmane),
`fire-frost.md` (shrine / master / event frame). Built on what landed: the void lane's land framework (`WRLand`,
`Data/lands/`), the forest lane's scenery kit (`FBuild`, `AWRForestScenery`) and its counterweight lift
(`AWRCanopyLift`), the Graveyard lane's world / spot / body pattern, the storm lane's gust tells, the taming lane's fly
mount, and the procedural-body pattern of `UWRSealBody`.

---

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, the LATER-COOKS lines naming this biome, and every section of biome-sky.md.
"Plan" points into this doc; "Proof" is filled at the end (section 5) with file:line or a commit.

| # | Requirement (source) | Plan |
|---|---|---|
| R1 | Full cook: research, plan, build, spot-check (brief) | 1, 2, 3, 5 |
| R2 | Research names real games, places, systems and why they work, plus real-world reference (brief) | 1 |
| R3 | Plan solves layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases (brief) | 2 |
| R4 | Every build step listed (brief) | 3 |
| R5 | Design with intention: every object's job, what it connects to, how it meets ground / walls / water / player / light / sound (Aaron 11-14) | 2.3-2.7 |
| R6 | Machines engineered; moving things move continuously on a believable mechanism, never snap (Aaron 11-14): lifts, drifting islands, Aeolith's platforms, sinking stones, the Fallen Span, the Wind Wheel, the Great Kite | 2.5, 2.8, 2.12 |
| R7 | Requirement ledger, every row proven (Aaron 11-14) | 0, 5 |
| R8 | The land on the map: Windward Cliffs ground, the Flue, 38 islands + Aeolith's ring + 6 Undercloud islands (brief; SKY 2.2) | 2.2 |
| R9 | Sub-zones W1-W5: own ground, palette, props, micro-hazard, bed, silhouette; 60-120 m blends; the Chain Cliffs border (SKY 2.2) | 2.2 |
| R10 | Six skyline landmarks L1-L6 (SKY 2.3) | 2.3 |
| R11 | POIs every ~150-250 m: 16 hand sites H1-H16 + 37 kit POIs, the 150/125 m rule; every island >= 1 POI, islands > 150 m >= 3 (brief; SKY 2.4) | 2.4 |
| R12 | Decoration rule: wind-leaned cedars and junipers, combed grass, lung ta wind-poles every 150 m on bridges and lips, waterfalls fading to mist, keels glowing at night (brief; SKY 2.1, 3.5) | 2.4 |
| R13 | Three camps on the cliffs, each with a Kitewright lift (40 s crank, 4 people), safe zones, Wind spawns there (world-map) | 2.5 |
| R14 | Island rules: >= 40 m, <= 220 m apart, glide-reachable; claims only on islands >= 120 m (SKY 2.2, 2.6) | 2.2, 2.6 |
| R15 | Updrafts: ridge lift (wind-gated), island columns, the Flue; visible (Mistlarks, dust, spores) (world-map, SKY 2.1) | 2.6 |
| R16 | Gliders: crude + levistone, the world-map table (open, speeds, stamina, Featherfall, combat closes, wind drift, durability); keep world-map's two recipes (LATER-COOKS 61) | 2.6 |
| R17 | Fall damage: none under 6 m, lethal at 18 m; Featherfall none; Featherdown draught x0.5; Skysilk 2 -50% (combat; SKY 2.5) | 2.6 |
| R18 | Gusts: 1,200 cm/s for 0.8 s, 1.5 s tell (whistle, grass wave, cloth snap, Mistlarks scatter, 1 cm shake), lanes every 8-15 s, Gale above 300 m every 4-8 s; brace x0.5, anchor boots / Skysilk 4 immune; creatures pushed except Sky-born; edge credit 6 s (SKY 2.5) | 2.7 |
| R19 | No safety nets, invisible walls or ledge assist (SKY 2.5) | 2.7 |
| R20 | Micro-hazards: lee rotors (W1), sinking stones (W2), downbursts (W3), each tell >= 0.9 s, each can kill (SKY 2.5) | 2.7 |
| R21 | The Undercloud's fog: vision 25 m, lights carry 40 m (SKY 2.5) | 2.7 |
| R22 | Weather: Clear 45 / Overcast 10 / Rain 10 / Cloud fog 15 (under 800 m) / Gale 20 and their play effects (brief; SKY 2.6) | 2.9 |
| R23 | Resources: levistone shards (float up, grab in 3 s), keels, cores; Skysilk from gossamer (hold Interact 4 s); cloudpine; iron/tin in the Weeping Isle's caves, copper on W1; water; fish; Puffwing feathers and eggs (SKY 2.6) | 2.8 |
| R24 | Crops Windflax and Cloudberry wild on W2's terraces, placed and obtainable (brief; SKY 2.6) | 2.8 |
| R25 | The Undercloud: entry gap (Mistlark column), two columns + the Last Kite line out, U1-U8 (SKY 2.7) | 2.10 |
| R26 | Six secrets (SKY 2.8) | 2.10 |
| R27 | Nine creatures: rows, looks, attacks with visible contact, numbers, drops, tame uses, flyer rule, living-world beats (brief; SKY 2.9) | 2.11 |
| R28 | Kyrra the Tempest Harrier, roaming, wing-break grounding, snatch and drop (SKY 2.10) | 2.12 |
| R29 | The Bridge Sentinel: deck weak sections, the fall to Winch Isle (SKY 2.10) | 2.12 |
| R30 | Old Gulper: inhale, swallow, choke on fire (SKY 2.10) | 2.12 |
| R31 | Aeolith: the Ring, Lid Gate, run-back, altar, plumes, Gale Crown, spore counter, P1, the Unbinding, P2 drift and columns, hidden P3 ride, drops (brief; SKY 2.11) | 2.13 |
| R32 | Creature and boss voices (SKY 2.12) | 2.14 |
| R33 | Music: F Lydian 100 BPM cue sheet, always audible (SKY 2.12) | 2.14 |
| R34 | Five shrines with guards; the Great Shrine's Wind Rose (SKY 2.13) | 2.15 |
| R35 | Path master Yara Kesh (look, voice, place) and her 7 quests (SKY 2.13) | 2.15 |
| R36 | Story only through the land; the lore items (SKY 2.14) | 2.4, 2.10 |
| R37 | Biome event: the Turning Wind (SKY 2.15) | 2.16 |
| R38 | Invasions Wind / Spore through the Flue (Sky side) (SKY 2.16) | 2.16 |
| R39 | Gear: anchor boots, gliders, Featherdown draught, Gale and Hush arrows, Kitewright glaive, Sailbell lantern, trophies and talismans, Gale plume, Wind Mask; Skysilk wired (SKY 2.17) | 2.17 |
| R40 | Skysilk armour set placed and obtainable (brief) | 2.17 |
| R41 | Building set (Cloud spire) placed and obtainable (brief) | 2.17 |
| R42 | Portal slots (brief) | 2.15 |
| R43 | The Wind path: wind.json, Momentum, 22 nodes, abilities, passives, half-effect rule (SKY 2.18) | 2.18 |
| R44 | Wind combos with the 8 paths (SKY 2.18.4) | 2.18 |
| R45 | Wind body change (SKY 2.18.5) | 2.18 |
| R46 | Aerial combat rules: jump attacks, flyers target gliders, edge kills and the parachute rule (SKY 2.19) | 2.6, 2.11 |
| R47 | Data-driven: numbers in Data/*.json + tuning, abilities as UWRGameplayAbility + ClassForNode, all damage through ApplyHit, screens with mouse and pad, sounds/fx as rows (brief) | all |
| R48 | Server authority and replication everywhere; multiplayer edge cases (brief) | all |
| R49 | No placeholders shown as finished; unfinished stays hidden (brief) | 4 |
| R50 | Combat law: tells over 0.5 s with a rising sound, projectiles never home and never over 1,400 cm/s, never inside 4 m, crimson only on unblockables, visible contact, no attack token (combat; SKY 3 spot-check list) | 2.11-2.13 |
| R51 | Compile gate clean of new warnings; no tests run (brief) | 3 |
| R52 | CHANGELOG Unreleased + HANDOFF in every commit; pull before and after (brief) | 3 |
| R53 | Watch LFS size; report new binary sizes (brief) | 3 |
| R54 | LANDED line, later-cooks lines, questions file (brief) | 3 |
| R55 | The Windmane's own four-winged body, its nest and egg, and it spawns (LATER-COOKS 152) | 2.11 |
| R56 | The Flue exhales spore plumes every 40 s (30 in the Sporefall) for Aeolith's counter (LATER-COOKS 65, 70) | 2.13 |
| R57 | Wind impulses through one entry point with every immunity, and the edge-fall kill credit, generic (LATER-COOKS 66) | 2.7 |
| R58 | Galespine (Aeolith's bow) adopted as its boss weapon (LATER-COOKS 105) | 2.13 |
| R59 | Windmane flight speed decided (LATER-COOKS 62) | 2.11 |
| R60 | Skysilk palette decided (LATER-COOKS 63) | 2.17 |
| R61 | The thrown spore pot (+150 spore buildup on Aeolith) (LATER-COOKS 70) | 2.13 |
| R62 | The biome answers on the map: WRBiome::At says sky_islands over the cape, the islands and the north-east sea; live in world.liveBiomes (brief) | 2.1 |

---

## 1. Research (build side; the design research is biome-sky.md section 1)

How the best games build these exact things, and what each gives this build.

| Game / system | How it's built, why it works | What this build takes |
|---|---|---|
| **Tears of the Kingdom, sky islands** (Great Sky Island, the Sky Archipelagos, Rising Island Chains) | Each archipelago is a handful of large hand-shaped islands with a flat walkable top, a sheer rim and a dark rock "root" underneath that reads against cloud. Updrafts are columns of visible swirling leaves and a constant whoosh. You only rise in one if your paraglider is open; you never need speed to enter. Islands carry small Zonai ruins with one idea each | Islands are a **top surface + sheer rim + tapered rock root** with a levistone keel at the tip; the top is turf and the rim is bare rock so the edge always reads. Updrafts lift only an open glider (or a flyer), always on entry, and show as rising dust and gossamer spirals with Mistlarks circling at their tops |
| **TotK paraglider** (Link's glide) | Open with jump in mid-air; forward ~6 m/s, sink ~2 m/s; stamina drains steadily, none in updrafts; any hit or bow draw is allowed only on stamina; landing closes it | Glide as a custom movement mode opened by **Jump while falling >= 2 m over ground**: forward and sink from the item row, stamina per second, none in a column, any hit closes it, Jump again or landing closes it |
| **Enshrouded glider** (and its updraft patch backlash) | The glider sits in its own slot; updrafts (some 130 on the map) lift instantly; the patch that made them need a run-up was hated | Updrafts never need a run-up. The glider works from the bag (the best one you carry) so no new UI slot is needed; it shows on your back while you carry it |
| **Elden Ring, Spiritspring jumps and fall deaths** | Falls kill past a height; the game never stops you at a ledge; spiritsprings are visible columns | No ledge-grab, no invisible walls, no nets: **lethal falls are real** and fall damage is height based, read from the highest point since you last stood (a glider or a column resets it) |
| **Valheim, the Moder's wind / Ashlands lava ledges** | The world wind is one vector everyone reads (sails, smoke, particles), and its direction changes over hours | One wind vector (weather) drives the Great Kite's yaw, the wind-pole cloth, gust direction, glider drift and the Turning Wind's 180° swing |
| **Sea of Thieves, the Ferry and ships** | Standing on a moving platform works because the platform's motion is a pure function the whole server shares | **Moving things are a pure function of the server's clock**: the lifts (`AWRCanopyLift`'s rule), the sinking stones, the drifting islands, the Fallen Span's swing, Aeolith's platforms. Every machine places them the same; collision moves with them so riders are carried |
| **Dragon's Dogma 2, Griffin** | The griffin's flight is long readable arcs; grabbing it is a real risk at height | Flyers path on a **sky graph** of island lips, centres and columns; they circle a home node, stoop on a locked line, then sit grounded |
| **Monster Hunter, Kushala Daora / Legiana** | Wind pressure is a status that trips you unless you wear the skill; a gale breath is a push cone, not a nuke | All pushes go through **one entry point** (`WRSky::Push`) that checks brace, anchor boots, Skysilk 4 and roll i-frames, and records the pusher for the edge credit |
| **Shadow of the Colossus, Phalanx; TotK Colgera** | You ride the flying boss; its weak points are on its back; it rolls to shake you | Aeolith's phase 3: attach riders to its spine, crest vents as parts, the barrel roll braced with Guard |
| **Ghost of Tsushima, the guiding wind** | The world itself points | The Great Kite always streams downwind; wind-pole cloth always shows the wind; Mistlarks show columns |
| **Breath of the Wild, the Divine Beast Vah Medoh's platforms** | Big platforms in the air read by silhouette; the ground under them is a long way down and you see it | Aeolith's eight platforms with wind-stones on their rims; the Flue's throat glowing cyan 1,290 m below |

**Real-world reference used for the build's measurements:**
- **Sea stacks and cliffs (Hebrides, Faroe):** granite sea cliffs 150-320 m, near-vertical with a talus apron and a wave-cut
  platform; turf runs to the very lip and overhangs it. -> the Windward Cliffs: turf to the lip, a 70-85° rock face, a
  shingle-and-shard apron at the foot.
- **Floating-island silhouettes (Hallelujah Mountains, Zhangjiajie quartz-sandstone pillars):** sheer sides, rounded tops,
  trees clinging at the rim. -> island tops domed 3-12 m, rims sheer for 8-25 m, then the root tapers 1.5-3x the radius.
- **Krummholz / flag trees:** trees in constant wind grow branches on the lee side only and lean downwind 10-35°. -> every
  cedar and juniper on the islands leans downwind (west, since the wind blows from the east) by 12-30°.
- **Lung ta (Tibetan wind horse flags):** strings of 5 colours (blue, white, red, green, yellow) on poles 3-6 m; faded
  cloth. -> wind-poles 4.5 m with 5-colour strings run to a stake, cloth stream downwind.
- **Counterweight cage lifts (the Bom Jesus funicular, the Lacerda lift, mine cages):** a cage on two chains over a
  sheave at a headframe, a counterweight of equal mass on the far fall; a crank geared down to a drum. -> the Kitewright
  lift: a timber headframe on the cliff lip, a 3.2 m sheave, a 2.4 x 2.4 m iron cage on twin chains, a stone counterweight
  in an iron cradle on the far fall, a crank-and-drum winch beside the headframe; the cage runs from the cliff-foot landing
  to the First Isle's landing.
- **Chain suspension bridges (Ji Hong bridge, Luding chain bridge, 1706):** decks of planks laid on 9-13 iron chains
  anchored in stone tower-houses each end; side chains as handrails. -> Kitewright bridges: plank decks on chains between
  stone toll towers; broken ones end in stubs.
- **Angel Falls / Staubbach:** a fall over ~300 m becomes mist. -> island waterfalls: a ribbon from the lip that widens and
  fades to mist 150-300 m down.
- **Manta rays, harpy eagles, frigatebirds, shrikes, nightjars, Portuguese man o' war** as in biome-sky 1.2.

---

## 2. Plan (every detail decided before building)

### 2.1 Where the Sky sits and how the game knows it

- **Frame:** map metres (world-map continent frame; game cm = m x 100, +y south). The biome box is x -700..+3,700,
  y -10,450..-7,950, any altitude: `WRSky::Contains`. `WRBiome::At` asks it **before** the Storm peaks catch-all (which
  claims everything north of the island) and after the runtime lands (the cliff ground is a land, so it answers there too).
- **`world.liveBiomes`** gains `sky_islands`. `WRSky::Live()` = live and `Data/sky.json` loaded.
- **Two data files, generated** by `Tools/terrain/sky.mjs` (edit the script, not the json):
  - `Data/lands/sky.json` + `sky_ground.bin`: the Windward Cliffs and the Flue's crown as a heightfield land (the
    forest/graveyard pattern), 5 m samples over x -700..+1,900, y -10,000..-7,900 (a 2,600 m square, 521 samples a side:
    0.81 MB). The ground west of x -650 (the Fungal surface's, a later lane) and the sea are hidden samples.
  - `Data/sky.json`: everything that isn't ground: islands, sites, kit, bridges, lifts, updrafts, gust lanes, hazards,
    shrines, resources, the sky graph, the arena, routes, the Undercloud, the event's splines.

### 2.2 The land and the islands

**The cliff ground (W1 ground):** the cape's east coast. From the Flue's crown (a broad volcanic dome rising to the rim
at 700 m round (0, -8,800), its throat 250 m across) the land falls east in long turf downs to the cliff tops at 150-320
m along x +1,600..+1,800, then drops in a 70-85° granite face to a shingle-and-shard apron 10-40 m wide at the sea. The
north coast (y -9,700..-10,000) is lower cliffs (80-150 m). The south edge (y -7,950) meets the Ocean's Blowhole Shelf
(not built: the sea) at the **Chain Cliffs** (L6). Surfaces: turf, thrift-pink cliff tops, granite face, shingle, shard
fields (levistone blue specks), the Flue's crown in dark volcanic rock with spore crust inside the throat.

**The Flue:** a throat 250 m across (radius 125 m) from the rim (700 m) down. The heightfield can't hold a vertical shaft,
so its samples are hidden inside 135 m and the shaft is a scenery piece: a 48-sided rock wall, wavering radius, from the
rim down to the cavern floor at -40 m (the Fungal lane's caverns open off it later), with fungus shelves low down, bird
ledges high up, and a cyan spore glow from the floor. The floor is a disc of mycelium and rock (collision) so Featherfall
lands you alive (SPEC). The Flue's updraft is a column (+12 m/s to 1,300 m).

**Islands:** 38 + the ring + 6 Undercloud, exactly the SKY 2.2 table, centres placed by the generator inside each
sub-zone's area so the island rules hold (>= 40 m, <= 220 m horizontal gaps, every island reachable by glide from a
higher island or a column; the generator reports any break). Each island is built in code from its row (shape seed,
size, yaw, altitude, kind): an outline from a noisy ellipse (aspect 1.0-1.8), a domed turf top with terraces where the
row says, a sheer rim 8-25 m, and a rock root tapering 1.5-3x the radius below, its tip and flanks carrying the
**levistone keel** (pale blue crystal beards that glow cyan at night). Collision: the top and the rim (procedural
mesh), so walking off the lip is a real fall. Vertex colours carry turf / rock / spore / levistone (one material: the
land's ground material). Islands under 120 m are `drift` islands (2.16).

**Sub-zones:** W1 Windward Cliffs + Lift Steps, W2 Cloud Gardens, W3 Ruined Sky-bridges, W4 Floating Shrines, W5 the
Undercloud, each with SKY 2.2's palette (top colour, rim colour), props and bed; `WRSky::ZoneAt` answers by island (an
island belongs to one sub-zone) or by the cliff ground (W1).

### 2.3 Landmarks (L1-L6)
- **L1 the Serpent's Ring:** eight white platforms 14-18 m, 28 m out round the Flue's axis at 1,250 m, a wind-stone 1.5 x
  4 m on each outer rim, braziers lit at night; the chains between them (phase 1) are real chain catenaries. Lenticular cap
  (a lens of cloud, sky card) over it in calm weather.
- **L2 the Weeping Isle:** the 420 m island; three waterfalls off its south lip (ribbons fading to mist 250 m down), a
  60 m levistone beard, three caves (2.8).
- **L3 the Fallen Span:** a 160 m bridge broken in the middle; Span South's half lies stub-out; Span North's half hangs
  straight down on its chains, its lower end 120 m under the island, swinging slowly (a pure-function pendulum, 0.6°
  amplitude, 22 s period); lanterns on it once the Toll Shrine is lit.
- **L4 the Paper Titan:** the 40 m mushroom titan kneeling on Titan's Crown, grey paper gills, Kitewright spears through
  it, a kite line round its neck.
- **L5 the Great Kite:** 60 m, faded 4-colour panels, tethered to the anchor drum at the Titan's feet; its line a 300 m
  chain-and-silk catenary with 12 lanterns; it flies 150-300 m downwind; its yaw follows the wind vector (the weathervane).
- **L6 the Chain Cliffs:** the three lift headframes on the cliff tops, chains down to the First Isle.

### 2.4 POIs and decoration
- **Hand sites H1-H16** as SKY 2.4, each built round its one idea, at its island or ground point; **Undercloud U1-U8**
  (2.10).
- **Kit POIs (37)** by kind and count (SKY 2.4): round house (6), wind-pole cairn (7), keel outcrop (6), gossamer drift
  (3), Galeharrier nest (3), Windmane ledge (2), waterfall pool (4), kill set-piece (2), vista stone (2), hazard teacher
  (2); placed by the generator on islands by sub-zone, >= 150 m apart, then a fill pass adds kit wherever a route or glide
  line point is > 125 m from any POI. Every island gets >= 1, islands > 150 m >= 3 (the generator tops up with round
  houses, cairns, pools).
- **Decoration (every island):** cloudpine cedars and junipers (tree kit species, leaned 12-30° downwind = west),
  wind-combed grass tufts, wind-poles every 150 m along bridges and island lips with 5-colour cloth strings streaming
  downwind, boulders at rims, levistone glints in the rim. Waterfalls on the islands with pools (ribbon fx fading to mist).

### 2.5 Camps and the Kitewright lifts (moving machines)
- Camps at (+1,600, -8,300), (+1,500, -8,900), (+1,000, -9,300) on the cliff tops (the land's `camps`: PlayerStarts
  tagged `sky_islands`, a 40 m safe zone each, stations at camp 2).
- **Kitewright lift (`AWRSkyLift`, the canopy lift's rule):** a timber headframe on the cliff lip (two A-frames 9 m, a
  cross-head carrying a 3.2 m iron sheave), the chain falls hanging over the sea side; the **cage** (2.4 x 2.4 x 2.6 m,
  iron frame, plank floor, waist rails, a gate) on two chains; the **counterweight** (a 2 m stone block in an iron cradle)
  on the far fall; a winch (drum + crank + pawl) beside the frame. The cage travels from the cliff-foot landing... the
  First Isle is above, so the lift runs **up**: the headframe stands on the First Isle's lip (the arrival), the chains fall
  to the cliff-top landing below it. The bottom landing is a timber stage on the cliff top; the top landing a stage on the
  First Isle. Hold Interact (in the cage or at either landing) to crank: 40 s for the whole rise at constant speed (eased
  1.5 s at each end), the sheave turning by the chain's travel, the counterweight going the opposite way. Position =
  f(server time, start, direction): no snaps. Cage floor and rails are moving collision. Up to 4 riders (the cage is the
  size for 4; more can squeeze, by design nothing stops them). One trip at a time; a crank mid-trip reverses.
- Each camp's lift rises to the First Isle; the three headframes stand 60 m apart on its east lip (the First Isle sits
  right above the camps' cliff, so the camps are placed along x 1,500..1,600 under it... the generator places the First
  Isle at (+1,560, -8,700), 520 m, and the three lifts' foot stages on the cliff top under its east lip. The camps' own
  positions stay SPEC; a stone path runs from each camp to its lift foot).

### 2.6 Air: updrafts, gliders, falls, aerial play (shared systems this biome needs)
- **Updrafts** (`WRSky::LiftAt(World, Pos)`, a pure function both the server and the owning client compute, the rift-pull
  pattern): columns from `sky.json updrafts` (kind, centre, radius, lift, top); ridge lift over the cliff tops
  (+8 m/s up to 400 m over the cliff top) only while the wind blows onto the cliff (its angle to the cliff normal < 60°);
  island columns (+10 m/s, 150 m over the island; 3-5 per big island, at the lip on the windward side); the Flue (+12 m/s
  to 1,300 m). Lift fades over the top 30 m and the outer 20% of the radius. Visible: rising dust and gossamer spirals
  (fx loop per column, `sky.updraft`), Mistlarks circling at the top, a rising whoosh bed near one.
- **Gliders:** items `crude_glider` (Workbench, Hearth 2: wood 12, fiber 20, hide 6, copper ingot 2) and
  `levistone_glider` (Workbench, Hearth 3: levistone 6, skysilk 4, iron ingot 2, crude glider 1), each a `glider` row
  (forward, sink, stamina/s, drift, durability seconds). Works from the bag (the best one carried, any slot); shown on
  the back while carried. **Open:** Jump while falling with >= 2 m of air under you. **Glide** (`CMOVE_Glide`, predicted:
  decided from the jump press and replicated state): horizontal speed eases to `forward` along the camera's facing (you
  steer with move input), sink `sink`, plus the wind drift and the column lift; stamina per second on the server, none in
  a column; out of stamina it closes. **Close:** Jump again, landing, any hit taken, a stoop, or out of stamina/durability.
  Bows and thrown items work while gliding (the aim works as normal); melee doesn't.
  - **Path Glide (Wind node):** glide with no glider (950 / 230 cm/s, 4 Momentum per s, then 3 stamina per s). **Skysilk 4:**
    6 s of glide from any jump (fall 300, forward 900) with no glider.
  - **Featherfall (Wind root, main path):** no fall damage; halves glider stamina.
- **Fall damage** (game-wide, combat SPEC): measured from the highest point since you last stood (reset by a glide or a
  column). None under 6 m; from 6 m to 18 m, max HP x (h - 6) / 12; 18 m or more kills. x the gear stat `fallDamage`
  (Skysilk 2: -0.5), x0.5 under the Featherdown draught; none with Featherfall, in water, or within 0.5 s of a glide closing
  under 20 m for a Featherfall player. Unblockable, through `ApplyHit`.
- **Edge kills:** a creature that falls more than 18 m dies, the kill (and its loot, where it lands) credited to whoever
  hit or pushed it in the last 6 s (`FWRFallCredit`). Sky-born ground creatures (Gallowshrike, Puffwing) parachute
  instead (out of the fight, home after 120 s); flyers don't fall; bosses and minis recover onto the nearest island
  after 3 s.
- **Aerial attacks:** the built jump-attack rows (weapons.json "jump") stand; flyers (Galeharrier, Hushwing, Kyrra) pick
  gliding survivors first in range, and a stoop on a glider closes it.

### 2.7 Wind, gusts and the micro-hazards
- **`WRSky::Push(Who, Velocity, Seconds, Source, Pusher)`**: the one entry point for every push in the Sky (gust lanes,
  rotors, downbursts, Undertow, Aeolith's breath and lifts, the Sentinel's spin, Gulper's belch): brace (hold Block or
  crouch, not attacking) x0.5; anchor boots (`worn.passGusts`) or Skysilk 4 (`passGusts`) = none; a roll's i-frames = none;
  a tame mount with `gustImmune` = none; sky-born creatures (`resists` "gust") = none. It records `Pusher` for the edge
  credit and launches the character (server; it replicates like any launch).
- **Gust lanes** (`sky.json gustLanes`: polyline, drop side): every 8-15 s while someone is on one (Gale: every exposed
  point above 300 m every 4-8 s). The tell (1.5 s, multicast): a rising whistle from upwind, a wave of flattened grass and
  torn cloud racing down the lane (fx `sky.gustWave` along the lane toward the drop side), every wind-pole within 40 m
  snaps taut (the cloth's flap rate x3 for 2 s), Mistlarks in 30 m scatter, 1 cm camera shake at 1.0 s. Then 1,200 cm/s
  for 0.8 s toward the drop.
- **Lee rotors** (W1): volumes on the lee (downwind) side of each Lift Steps island and the cliff lips, live only while
  the wind puts them in the lee. A glider entering: 1.0 s tell (tumbling wisps fx + a low whump-whump), then a sideways
  shove 600 cm/s for 0.6 s and +3 m/s sink for 2 s. On foot at a lee lip: a down-gust 400 cm/s toward the edge for 0.5 s.
- **Sinking stones** (W2): lines of 5-9 m flat levistone stones floating between garden islands (`sky.json stones`). A
  stone you land on: its keel glow flickers and dims, dust trickles and it groans (1.0 s tell), then it dips 1 m, sinks at
  4 m/s for 6 s, holds, and rises back over 20 s. Position is a pure function of the server time it was triggered (every
  machine places it the same; its collision carries riders).
- **Downbursts** (W3): fixed sites over the glide lines between bridge islands. Each cycles: 20-40 s quiet, then the
  virga streaks show (visible 300 m) for 8 s with a 1.2 s hiss before it bites; while live, a 30 m column: gliders inside
  sink +6 m/s; anyone on a bridge deck under it gets a 0.8 s push straight down (brace or stagger 0.6 s).
- **The Undercloud's fog:** inside its bank the weather look forces fog (vision 25 m; held lights 40 m).
- **No nets, no invisible walls, no ledge assist** anywhere in this land.

### 2.8 Resources, crops, gathering
- **Levistone shards** (W1 cliff foot shard fields, 1-3 per node, respawn 2 game days): a struck shard floats up 1 m and
  hangs there glowing; grab it within 3 s (Interact or walk into it) or it drifts off up into the sky and is gone
  (`AWRSkyShard`, replicated, server-timed). **Keels** (island undersides, reached from hanging ledges and chain walks
  under islands: 4-6 a node), **cores** (the Weeping Isle's caves, 8 a node, glowing).
- **Gossamer drifts** (W2 cedars and fog-net frames): hold Interact 4 s to wind Skysilk onto a spindle (2-4; x2 in rain);
  respawn 1 game day per survivor.
- **Cloudpine** (every island's cedars: an axe node, `cloudpine` item), **ore** (iron and tin in the Weeping Isle's 3
  caves, copper on the W1 ground), **water** (pools), **wild Windflax and Cloudberry** on W2's terraces (hand nodes: the
  crop's produce + its seed), Puffwing feathers (rookery nests: a feather node) and eggs.

### 2.9 Weather
`Data/weather.json` sky_islands row (exists: gale, fog, rain, overcast) made the SPEC table: Clear 45, Overcast 10, Rain
10, Cloud fog 15 (islands under 800 m in cloud, vision 40 m), Gale 20 (gusts everywhere above 300 m every 4-8 s; the
gust effect's `minAltitude` 300; gliders drift at wind x 0.3). Play hooks: Rain doubles gossamer; Cloud fog lets
Hushwings hunt by day and Mistlarks call; Gale makes Skyrays breach at dusk and the Great Kite stream.

### 2.10 The Undercloud and the secrets
- The bank: a fog volume (radius 500 m, 500-600 m altitude) at (+2,600, -9,600); from outside, a cloud bank that never
  moves (cloud cards). The **gap** on its top under the Kitefield's lip (a +6 m/s column that keeps a 20 m hole: the
  Mistlarks dive into it). Two +10 m/s columns inside (top 150 m over the bank) and **the Last Kite's line** (a climbable
  silk-and-chain line from the Last Kite island to the Kitefield: Interact to grab, climb at 3 m/s, no stamina, jump to let
  go; `AWRSkyLine`).
- U1-U8 as SKY 2.7 (the Kite Graveyard's thousand kites on lines into the fog; the Bone Tubes; the Bonebell (strike it:
  every kite shakes, 300 m sound, wakes the roost); the Hushwing Roost; the Gardener's Grave; the First Kite (1,500 XP,
  the Serpent anchor rubbing); the Newcomer's Kite; the Keel Vault under Tube Isle).
- Secrets (SKY 2.8): the Keel Vault (chain bolts over Tube Isle's lip lead to its ledge), the Weeping Isle's third cave
  (behind the middle waterfall, a 2 m ledge), the Wind Rose door (stand on the carved wind the Great Kite points away from
  at noon: the floor plate turns, a cache), the Bonebell's echo (ring it in the Turning Wind: the Kitewright's last kite
  talisman), the Wheel's chain (a 180 m chain walk from Wheel Isle to the Lid Gate), the Gulper's pearl lagoon (a sunken
  skiff with a bronze chest at the lagoon's floor).

### 2.11 The nine creatures (rows, rigs, looks, behaviour)

Every row in `creatures.json` (biome sky_islands), every attack in `attacks.json` with SKY 2.9's timings, numbers and
drops; voices per 2.14. Rigs: the house method (a real rig with real clips, recoloured with `M_CreatureTint`, the
silhouette parts modelled on frames riding its bones), or a body built in code where no rig of the animal exists (the
`UWRSealBody` method: every vertex on a spine that moves the way the real animal moves).

| Creature | Body | Silhouette parts | Motion |
|---|---|---|---|
| Galeharrier | Griffon rig at 0.42 (2.8 m span) | Double crest (flares on the stoop tell), white face ring on a black hood, barred belly (tint two-tone), big yellow feet | Griffon FlyNormal / Glide / Swooping (stoop) / FlyStationaryClawsAttack (rake) / Landing |
| Skyray | Code body (`UWRSkyBody` "ray"): a 9 m diamond, wings undulating in a travelling wave root to tip, horn-lobes that unroll before the Undertow, a whip tail with a barb, a white belly with black blotches, violet rim glow at night | (built in) | Drift, bank, hover-flap, breach |
| Windmane | Frostantler rig (deer) + four modelled feathered wings on the shoulder and thigh bones (fore 6 m span, hind 3.5 m), silver mane | Antlers bone-pale dark-tipped | Deer clips on the ground; in flight the wings beat (fore and hind out of phase), legs tucked (idle clip) |
| Cloudback / Old Gulper | Code body "whale": 7 m (22 m) whale with throat grooves, stubby fins, a fluke, the barnacled levistone hump glowing | (built in) | Floats 2 m up, the fluke beating slowly, rolls, spouts |
| Mistlark | Griffon rig at 0.06 | Fanned tail, mist-blue throat (tint) | Flocks circling columns |
| Puffwing | Snowpuff rig at 1.0 | Oversized soft wings at the sides, red bill and frontal shield, blue breast | Waddle; parachutes off edges (wings spread, slow fall) |
| Gallowshrike | Ashstrider rig at 0.85 (2.2 m upright) | Shrike head with a black bandit mask and hooked beak, short stiff black wings, 12 cm black dagger claws | Ashstrider walk/run; hop-glide with wings spread |
| Hushwing | Glowmoth rig at 1.0 (5 m span, broad soft wings) | Flat frogmouth head that splits (gape), pale facial disc, amber eyes | Glowmoth flight, silent |
| Sailbell | Code body "bell": a translucent gas bell with a violet crest-sail and a 12 m curtain of beaded tentacles that sway | (built in) | Drifts at 45° to the wind (handedness), pulses |
| Windmane foal / egg | the Windmane at 0.5; `egg_windmane` item in its ledge nest | | |

**Windmane flight speed (LATER-COOKS 62):** 1,600 cm/s (the built `flySpeed`; world-map's 14 m/s is the cruise feel,
1,600 the top). Decision logged.

**Flyer rule:** wild flyers hold a home node and circle it (soar) in `MOVE_Flying` with a ring path 25-40 m across at
15-40 m over the island (`FWRSkyAI::TickWild`), stoop on targets in range, and sit grounded after a stoop. Ground creatures
stay on their island (their wander target is checked against the island's top); Gallowshrikes hop-glide only along short
graph edges (<= 40 m, level or down).

### 2.12 The three minis
- **Kyrra** (Griffon rig at 1.35 = 9 m span, white crest, a chain stump): roaming route over W3 (the generator's 2.4 km
  loop), lands every 6 min on the widest island for 60 s; wing parts (stance 250, ranged x2), wing break grounds her 10 s;
  rows per SKY 2.10 (Tempest stoop, Snatch and drop grab, Talon combo, Gale wing, Screech). Trophy Harrier plume.
- **The Bridge Sentinel** (Obsidian_warden rig at 3.3 = 6 m, granite tint, a halo of eight stone vanes on its back that
  turn as it walks): on the standing bridge Tollgate to Span North (180 x 8 m deck, 5 weak sections that flicker); its
  Vane slam on a weak section breaks it and it falls 35 m to Winch Isle, takes 25% HP and kneels 10 s; deck throw leaves a
  hole for 2 game hours (the deck stone's collision removed). Winch Isle's chain ladder (20 s climb) back up.
- **Old Gulper** (whale body at 22 m, scarred, half-blind): floats over Gulper's Bight; Inhale cone pull, swallow (4 s
  inside, 12/s, spat 15 m random), a fire hit or thrown lit torch / fire bomb into the open mouth during Inhale = Choke
  (sinks 10 s, mouth open, x1.5 damage); two chokes in 60 s crack the hump (no barnacle drop).

### 2.13 Aeolith (the major)
- **Body:** code-built serpent (`UWRSkyBody` "serpent"): 60 m, 2.2 m thick at the chest, 60 segments along a path the
  boss's head has flown (the body follows the head's trail, so its coils are real), one pair of feathered wings 34 m span
  behind the head (beating; spread like a hood on the tells), a bone wind-mask on the snout, a jaw that opens 120°, the red
  throat sac that swells (the tell) and glows at night, quetzal plumes along the spine and a 6 m tail fan. Palette per
  SKY 2.11.
- **The Ring:** eight platforms (moving collision; P1 fixed with chains; P2 drifting: radius and angle a pure function of
  the server time since the Unbinding), wind-stones that block the gale breath (tag `WindStone`), braziers, the Lid Gate
  50 m south joined by a chain walk, the run-back stone at the Lid Gate, the Wind altar in its temple.
- **Plumes:** three vents take turns every 40 s (30 in the Sporefall): 1.0 s tell (cyan glow brightens, deep huff), then a
  6 m cyan column up the Flue's axis for 6 s. Aeolith passing through a live plume gains 150 spore buildup; 400 =
  Spore-weighted (grounded 8 s on the nearest platform, x1.25 damage, Crown off). The thrown **spore pot** (new item,
  deepcap spores + clay) adds 150 on a hit.
- **Gale Crown:** while up, projectiles entering 3 m of it are deflected (sent off sideways); off for 20 s after a stance
  break, a wing break or spore weighting.
- **P1 rows** (Tail sweep, Gale breath, Coil strike x3, Crosswind dive, Feather volley, Snatch) per SKY 2.11; **the
  Unbinding** at 50%; **P2** (Updraft Inhale, Downburst, Spiral combo, Held gale, the P1 rows, platforms drifting 4-14 m
  gaps on a 20 s cycle, columns in the gaps); **hidden P3 at 25% with artifacts** (low passes every 20 s, Interact within
  4 m to grab on, crest vents x3 at 900 HP, barrel roll every 12 s braced with Guard at 30 stamina/s, all vents broken =
  crash 10 s, regrow once at 50%).
- **Drops:** core_aeolith, gale_essence x8, boss part Gale plume (chance `bosses.partChance`), first kill the Wind Mask of
  Aeolith (trophy + talisman), Aeolith plumes x6, and **Galespine** (the boss bow, LATER-COOKS 105) as its boss weapon.

### 2.14 Sound and music
- `sounds.json`: `creatures.<id>` families for the nine + four bosses (idle, aggro, attack, hurt, death, tell, signature;
  Hushwing has no flight loop by design); events for gusts (`sky.gust_whistle` rising 1.5 s), rotors, stone groans,
  downburst hiss, plume huff, chain singing, aeolian harps, kite flutter, lift chain and winch, glider cloth flap;
  beds per sub-zone (`sky.w1`..`sky.w5`); music pools `explore.sky_islands.a/b/c`, `night.sky_islands`,
  `combat.sky_islands`, `mini.sky_islands`, `boss.aeolith.p1/p2/p3`, `sub.undercloud`, `camp.sky_islands`,
  `layer.turning_wind`, `layer.weather.gale.sky_islands`.
- **Audio files:** the repo's audio pipeline (`Tools/audio`) builds OGGs from CC0 sources; where a sound has no file yet
  the row points at the closest built file of the same family (wind, birds, whales, chains, bells) so nothing is silent,
  and the gap is logged for the audio cook.

### 2.15 Shrines, Yara, quests, portals
- Shrines (`world.json shrines.list`, land `sky`, or `island` + local point for island shrines): the Great Shrine of the
  Still Pool (Still Pool Isle), Gardens (Moss Stair), Toll (Last Toll), Rose (Rose Temple), Barrow (Kite Barrow), with
  SKY 2.13's guards. Island shrines need a new placement branch (`sky` + island id): the shrine stands on the island top.
- **Yara Kesh** (master row: sex f, the build/face ids, Skysilk armour pieces, lines; stands at her open workshop on Still
  Pool Isle; her missing left forearm and brass hook modelled in the scenery's workshop + a held-item stand-in) and her 7
  quests (types: bring, event `glide_first_flight`, event `updraft_top`, bring skysilk, event `broken_way`, bring rubbing,
  show trophies).
- **Portal slots:** `portals.json` biome `sky` gets `"land": true`-style spot picking on the islands (the Sky's slot spots
  are island tops).

### 2.16 The Turning Wind and the invasions
- `world.json events.turning_wind` (biome sky_islands, every 72 h, 3,600 s, signature x3), its effects in
  `WRSkyEvents`: the sign (lenticular cap, K-H billows, wind-pole cloth swings against the wind, the Great Kite turns), the
  wind override (the weather's wind for sky_islands swings 180° over 120 s), drift islands moving 40-120 m along their
  splines at 1 m/s and holding (pure function of time since the event start: every machine the same; riders carried), six
  cloud bridges (walkable collision along splines, lit from inside; fray tell 1.0 s before a section fades at the end),
  20 shard falls (shard clusters that float away unless grabbed in 3 s), the Undercloud gap widening, levistone x3.
- Invasions: `invasions.pairs` Wind <-> Spore through the Flue (mouths at the Flue's rim and floor; the Sky side's roster
  SKY 2.16 with Kyrra as leader).

### 2.17 Gear and sets
- Items: levistone (exists), levistone_core, skysilk, cloudpine, gale_essence, galeharrier_feather, harrier_talon,
  ray_leather, ray_barb, mane_hair, blubber, levistone_barnacle, shrike_claw, hush_feather, hushwing_eye, sail_membrane,
  sting_oil, kyrra_feather, worked_granite, aeolith_plume, core_aeolith, part_gale_plume, trophy_wind_mask (+ talisman),
  featherdown_draught, gale_arrow, hush_arrow, kitewright_glaive, sailbell_lantern, anchor_boots, crude_glider,
  levistone_glider, spore_pot, egg_windmane, lore items. Recipes per SKY 2.17 and world-map.
- **Skysilk palette (LATER-COOKS 63):** white, pale teal #9FD6CF, silver (SKY 2.17's merge).
- Skysilk set and Cloud spire exist in data (gear.json, building_sets.json); they become obtainable when their inputs
  drop here (levistone, skysilk, feathers, skystone blocks): the recipes are biome-gated and the biome goes live.

### 2.18 The Wind path
- `Data/paths/wind.json` per SKY 2.18.3 (trunk Gust, Momentum capacity, Off balance, Wind cut; Skyborne, Cyclone,
  Windblade), the Momentum resource (attribute pair, tick: +14/s airborne, +8/s sprinting, +4 per air hit, decay after
  2.5 s at 10/s; >= 60 knockback x1.25 and stamina regen +15%), abilities as `UWRGameplayAbility` classes in
  `WRAbilitiesWind.cpp` registered in `ClassForNode`, passives through `HasNode` in `ApplyHit`, Featherfall, the
  half-effect rule, the 8 combos in `combos.json`, the body change row in `player_art.json paths.wind`.

---

## 3. Build steps (each a commit; compile gate after each)

1. **Data frame + land:** `Tools/terrain/sky.mjs` -> `Data/lands/sky.json`, `sky_ground.bin`, `Data/sky.json`; tuning
   `skyWorld` / `glide` / `fall`; `world.liveBiomes`; `WRSkyTypes.h/.cpp` (`FWRSkyData`, `WRSky::Contains/ZoneAt/LiftAt/
   Push`); `WRBiome::At`.
2. **Air:** fall damage, gliders (items, recipes, `CMOVE_Glide`, the back look), updrafts, `WRSky::Push`, edge credit.
3. **The world:** `WRSkyWorld.h/.cpp` (subsystem, islands, scenery pieces), `WRSkyIsland.cpp` (island mesh), `WRSkyScenery*.cpp`
   (landmarks, hand sites, kit, decoration, bridges, waterfalls, the Flue, the Great Kite), `AWRSkyLift`, gathering,
   caches, spots, shards, the Undercloud.
4. **World rows:** items, recipes, world.json (biome, shrines, Yara, quests, event, invasions), weather, spawn regions,
   portals, audio rows.
5. **Hazards + event:** gust lanes, rotors, sinking stones, downbursts, fog, the Turning Wind.
6. **Creatures:** rows, attacks, `WRSkyCreatures.*` (body component, AI hooks, code bodies), spawn regions.
7. **Minis + Aeolith:** `WRSkyBosses.*`, arenas, the Ring.
8. **The Wind path.**
9. Spot-check, CHANGELOG, HANDOFF, LANDED.

## 4. What stays hidden if unfinished
Anything not finished in this lane keeps its biome gate or a `waitsFor` so it never shows half-built; listed in the
CHANGELOG's "not yet" and LATER-COOKS.

## 5. Proof (filled at the end, 2026-11-15)

Commits on master: (1) bb04900 land and air, (2) ecce26f Wind path, (3) 8e02afe items / world rows, (4) 15ebac1
creatures, (5) 62fab47 bosses, (6) 60e3d21 sound; unity fixes 8f6edc0, 8b28a0a. Compiled (editor target, clean tree)
after each; not played (Aaron tests). Paths are Source/Wildreach unless they start with Data/.

| # | Done | Where |
|---|---|---|
| R1-R4 | yes | this doc 0-3; build in commits 1-6 |
| R5-R6 | yes | sites and machines placed with their jobs (WRSkySites.cpp, WRSkyScenery*.cpp); lifts / stones / drift / Ring as pure functions of server time: WRSkyWorld.cpp, WRSkyBosses.cpp:146 (PlatformAt) |
| R7 | yes | this table |
| R8-R12 | yes | Data/sky.json via Tools/terrain/sky.mjs; WRSkyIsland.cpp; WRSkyScenery*.cpp; WRSkySites.cpp (commit 1) |
| R13 | yes | camps + AWRSkyLift (commit 1) |
| R14-R15 | yes | sky.mjs island rules; WRSkyIsleTypes.cpp LiftAt (commit 1) |
| R16-R17 | yes | gliders WRSkyAir.cpp:82, items/recipes Data/items.json crude_glider / levistone_glider, Data/recipes.json; fall damage WRSkyAir.cpp:645 |
| R18-R21 | yes | gusts WRSkyAir.cpp:204, one push entry WRSkyAir.cpp:182, rotors / stones / downbursts / fog (commit 1); creature and boss knockback through it WRCreatureCombat.cpp, WRBossCombat.cpp |
| R22 | yes | Data/weather.json sky_islands (Clear 45 / Overcast 10 / Rain 10 / Cloud fog 15 / Gale 20); gale torches out WRSurvival.cpp:1348 |
| R23-R24 | yes | nodes and shards WRSkyWorld.cpp (commit 1); items Data/items.json (commit 3) |
| R25-R26 | yes | Undercloud, U1-U8, secrets (commit 1); the Bonebell echo talisman item talisman_last_kite (commit 3) |
| R27 | yes | 9 rows Data/creatures.json + Data/attacks.json; looks WRSkyCreatures.cpp:204; code bodies WRSkyBeast.cpp:151 (ray), :280 (whale), :411 (bell); brains WRSkyCreatureAI.cpp:386 / :193 / :706; populations :1105; beats :1237, :1259 |
| R28-R30 | yes | WRSkyBosses.cpp: Kyrra TickIdle :498 + Tick :588, the Sentinel's deck BeginStrike :911 + AWRSkyState::OnRep_Deck WRSkyWorld.cpp:106, Gulper's inhale / choke TickStrike + Incoming :1244 |
| R31 | yes | Aeolith: body WRSkyBeast.cpp:496; Ring :146 / plumes :167; Crown, spore, vents Incoming :1244; Unbinding, inhale, volley BeginStrike :911; Last Circle in Tick :588; altar + stones UWRSkyArenas::Place :1516; riders Command :1641 |
| R32-R33 | stand-in | voices and events Data/Audio/sounds.json (stand-ins marked _standin); beds and pools WRAudioDirector.cpp:428; real recordings and the F-Lydian score: LATER |
| R34 | yes | Data/world.json shrines :61 (five, on their islands via WSkyIslandSpot WRWorldSystems.cpp:62; aggressive guards WRWorldSites.cpp) |
| R35 | yes | Yara Data/world.json:87; quests :91; deeds WRSkyAir.cpp:608; quest need WRWorldSystems.cpp:2324; map mark WRUIMap.cpp:211 |
| R36 | yes | lore items Data/items.json (commit 3); keepsakes (commit 1) |
| R37 | yes | Data/world.json:255 turning_wind; machinery (commit 1); music layer (commit 6) |
| R38 | partial | Wind -> Spore pair Data/world.json:265 (dormant until Fungal is live); Spore -> Wind is the Fungal lane's |
| R39-R41 | yes | Data/items.json, Data/recipes.json (commit 3); Skysilk set recipes to the cook's totals; Cloud spire via skystone_block + bp_cloud_spire_decor (Aeolith) |
| R42 | NO | portals.json sky is land:false (no Sky fold kit): logged in later-cooks |
| R43-R45 | yes | commit 2 (WRWindPath.*, WRAbilitiesWind.*, Data/paths/wind.json, Data/combos.json) |
| R46 | yes | flyers pick gliders WRSkyCreatureAI.cpp TickWild; a stoop closes a glider (NoteHit) |
| R47-R48 | yes | all numbers in Data/*.json + tuning skyLife / bosses.*; server-authoritative, replicated flags / Ring / deck |
| R49 | yes | unfinished parts hidden or stand-in-marked; listed in CHANGELOG "Not finished yet" |
| R50 | yes | every row has its tell; quills 1,400 cm/s never homing; crimson only on unblockables (Data/attacks.json) |
| R51-R53 | yes | compile clean each piece; no tests run; CHANGELOG + HANDOFF in each commit; no new binaries |
| R54 | yes | LANDED, later-cooks, QUESTIONS lines |
| R55 | yes | Windmane body WRSkyCreatures.cpp (four wings), nests WRSkyWorld.cpp:1481, egg item, spawns (skyLife.populations manes) |
| R56 | yes | plumes every 40 s AWRSkyRing::PlumeNow WRSkyBosses.cpp:167 |
| R57 | yes | WRSkyAir::Push / NoteHit / FallCredit (commit 1) |
| R58 | yes | galespine item + recipe + gear.json bp_galespine (boss aeolith) |
| R59 | yes | Windmane flySpeed 1600 (Data/creatures.json) |
| R60 | yes | Skysilk palette white / pale teal / silver (items notes, Yara) |
| R61 | yes | spore_pot item + recipe; FWRSkyBoss::SporeLanded WRSkyBosses.cpp:1465 |
| R62 | yes | WRBiome::At sky_islands, world.liveBiomes (commit 1) |
