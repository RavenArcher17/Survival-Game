# Wildreach build cook: MACHINERY AND POWER (FULL-GAME 6.3, machinery row)

Lane build-machinery, 2026-11-14. Scope: water wheels, windmills, power (water, wind, lava, soul energy), conveyors,
sorting, drills, production chains, biome machines (lava forge, soul engine and the other ten), engineering life skill
XP. The spec is `cooks/survival-crafting.md` 2.15 (machines and numbers), 2.17 (piece levels, HP), 2.23 (engineering XP,
perks, every-level boost), 2.25 (machine sound). This doc adds the requirement ledger, research, the gaps those sections
left, and the build plan. Built to compile only (Aaron and the testers test, F8).

---

## 0. Requirement ledger

Split from the brief, the Aaron quotes in it, FULL-GAME 6.3 / 6.6 and survival-crafting 2.15 / 2.17 / 2.23 / 2.25.
Proof column is filled at the end (section 5).

| # | Requirement | Source |
|---|---|---|
| R1 | Power is a number (P) on a network of sources and machines joined by drive shafts (4 m) or touching | SC 2.15 |
| R2 | Load over supply: every machine on the network runs at supply ÷ load speed | SC 2.15 |
| R3 | Machines run while a player is within 250 m; queues already in a station finish by timestamp | SC 2.15 |
| R4 | Water wheel: Hearth/workbench 2, wood 40 + bronze 6, 10 P, on moving water | SC 2.15, 2.17 (HP 1,000) |
| R5 | Windmill: level 3, wood 50 + linen 10 + iron 6, 6 P; storm ×2; Storm peaks and Sky islands ×1.5; open ground, nothing taller within 10 m | SC 2.15 |
| R6 | Geothermal turbine: level 4, steel plate 12 + stone 30, 25 P, within 6 m of lava or a vent | SC 2.15 |
| R7 | Soul engine (Graveyard biome machine): level 5, steel plate 10 + soulstone 12 + bone 20, 40 P for 1 h per soulstone, anywhere | SC 2.15 |
| R8 | Powered station (forge, armorer, alchemy, workbench, oven, smokehouse): 5 P, crafts ×1.5 | SC 2.15 |
| R9 | Sawmill: L3, 6 P, 10 wood → 15 planks; planks replace wood 1:1 in building | SC 2.15 |
| R10 | Grinder: L3, 4 P, grain → flour, bones → bone meal, stone → stone dust | SC 2.15 |
| R11 | Loom: L3, 3 P, fiber or flax → linen ×1.5 | SC 2.15 |
| R12 | Smelter: L3, 8 P, ore → ingot by itself (ingot recipes only), 6 s each | SC 2.15 |
| R13 | Irrigation pump: L3, 3 P, feeds a channel 40 m from any water | SC 2.15 |
| R14 | Conveyor (2 m piece): L3, 1 P per 4 pieces, moves one stack every 2 s from one container/machine to the next; items ride visibly | SC 2.15 |
| R15 | Sorting chest: L3, 2 P, takes from a conveyor, sends each item to the chest set for it within 20 m | SC 2.15 |
| R16 | Drill: L4, 8 P, on an ore outcrop or vein node, 1 ore / 60 s into a 40-slot hopper; node never runs out while drilled | SC 2.15 |
| R17 | Greenhouse lights: L4, 4 P, greenhouse plots grow in the dark | SC 2.15, 2.7 |
| R18 | Lamps (wired): L3, 1 P per 4, light without fuel | SC 2.15 |
| R19 | Production chains (medium): any line of machines joined by conveyors, 4 machines long (Chief engineer 6); drill → conveyor → smelter → conveyor → sorting chest | SC 2.15 |
| R20 | 12 biome machines (L5, each needs its biome's signature resource; built in that biome unless stated): Lava forge, Storm coil (+stormglass battery 2 h at 20 P), Sun mirror array (+sunstone battery 4 h at 10 P), Grove press, Updraft lift, Resonance pylon, Cold store, Blood still, Tide mill, Soul engine, Mycelium vat, Rift chest | SC 2.15; FG 6.3 |
| R21 | Biome machines of lands not in the game stay hidden until their land is live (no placeholders shown) | brief ("anything unfinished stays hidden") |
| R22 | Engineering XP: placing a machine 30; each running machine you own 1 per 5 min (up to 10 counted); repairing a machine 10; crafting machine parts 15 | SC 2.23 |
| R23 | Engineering every level: machine speed +0.3% (to +30%) | SC 2.23 |
| R24 | Engineering perks live: Efficient power (−20% power), Tinkerer (machines −20% cost), Long lines (shafts/conveyors 6 m), Overclock (×1.5 at ×2 power), Self-repair (no durability loss), Chief engineer (chains 6 long); Turret tech / Siege engineer stay hidden (turrets, siege not in scope) | SC 2.23; lifeskills waitsFor |
| R25 | Machine sounds: water wheel creak and splash, windmill sweep, turbine roar, conveyor clatter, drill knock; seamless, fall off over 30 m | SC 2.25 |
| R26 | Machines look engineered: parts mesh, shafts and chains lead somewhere, nothing passes through anything | Aaron 11-14 |
| R27 | Moving parts move continuously and smoothly on a believable mechanism, never snap or teleport | Aaron 11-14 |
| R28 | Every object answers why it is here, what it connects to, how it meets everything it touches | Aaron 11-14 |
| R29 | Every screen works with mouse and pad, no dead states; F1 controls page lists new keys | brief THE BAR |
| R30 | Multiplayer: server authority, replication (speeds, belts, states), save/load | brief |
| R31 | Data-driven: numbers in Data/*.json and tuning | brief |
| R32 | Icons for every new item; build cards for every new piece | THE BAR |
| R33 | Research → plan → build → spot-check, no placeholders, no primitives as finished work | brief (Aaron 11-13) |
| R34 | Medium automation, no factory lines | SC 2.15 (DD) |
| R35 | Machine wear (implied by Self-repair "machines lose no durability") and repair | SC 2.23 |
| R36 | The 2.17 build costs and HP for machine pieces (400 to 1,200; turbine/drill/lights 1,500; biome 2,500) | SC 2.17 |

---

## 1. Research

### 1.1 Games (what they do, what we take)

| Game, system | What works / fails | We take |
|---|---|---|
| **Factorio** power ([wiki Electric system](https://wiki.factorio.com/Electric_system)) | Under-supply is shared: every machine slows by the satisfaction %, never a hard stop; the pole tooltip shows the network's made / used; machines show "Low power / Output full / No ingredients" | Spec R2 already says this. Every machine shows one state word, the network panel shows supply, load and the % bar (yellow under 100%, red under 50%) |
| **Factorio** belts and filter inserters | Every item on a belt is drawn; filters whitelist | Items ride visibly (R14). The sorting chest sends by what a chest already holds (1.3) |
| **Satisfactory** ([Power](https://satisfactory.wiki.gg/wiki/Power), [Clock speed](https://satisfactory.wiki.gg/wiki/Clock_speed)) | Overclock costs more power than it gains; the power panel graphs production and draw | Overclock ×1.5 at ×2 power (spec); the network panel lists each source's output now and why (wind 0.8, storm ×2) |
| **Create mod** ([Kinetic stress](https://create.fandom.com/wiki/Kinetic_Stress)) | Rotation is the power: shafts, cogwheels and gearboxes all spin, so a network explains itself; the stressometer reads use / capacity | Power is mechanical rotation: every source ends in a turning output shaft, every load takes one; shafts and gears spin at the network's speed (slower when under-supplied); the gearbox turns 90° and branches |
| **Timberborn** ([HP](https://timberborn.wiki.gg/wiki/HP), [power guide](https://timberborn.org/articles/power-systems-guide)) | Shafts must physically connect; water wheel output follows the flow; short runs and overbuilding | Ports that shafts snap to; the water wheel reads its water's motion |
| **Valheim** windmill ([hostedgg](https://hostedgg.com/wiki/valheim/windmill)) | Wind is felt: more in storms, blocked by terrain and walls | Windmill clearance (nothing taller within 10 m) and storm ×2 |
| **Manor Lords** windmill ([exputer](https://exputer.com/guides/manor-lords-windmill-not-working/)), **Dune Awakening** ([sportskeeda](https://www.sportskeeda.com/mmo/dune-awakening-building-power-generation-explained)) | Players can't tell why a machine is idle; bases go dark with no warning | Every state has a plain reason on the prompt and the screen ("No power: the network gives 4 P of 12") |
| **Enshrouded** magic chests ([itechguides](https://www.itechguides.com/enshrouded-magic-chests-how-to-share-materials-across-crafting-stations/)) | Hand-feeding stations gets tedious; players asked for chest feeding | Conveyors pull from chests and machine trays; the sorting chest puts things away |
| **Ark** generator ([ark.wiki.gg](https://ark.wiki.gg/wiki/Electrical_Generator)), **Once Human** ([sportskeeda](https://www.sportskeeda.com/mmo/once-human-power-generation-explained)) | Fuel-burning generator; hydraulic generators must sit by water | The soul engine burns soulstone; the water wheel must stand in moving water |

### 1.2 Real-world engineering (what the models copy)

| Reference | Detail used |
|---|---|
| Watermill drive train ([Glossary of mill machinery](https://en.wikipedia.org/wiki/Glossary_of_mill_machinery), [Norfolk Mills](https://norfolkmills.co.uk/watermill-machinery.html)) | Wheel axle → pit wheel → wallower → upright shaft → spur wheel → stone nut → runner stone over a bed stone; wheel ~10 rpm, runner ~120 rpm (12:1). Our grinder's spindle turns 12× its input |
| Water wheel types ([Water wheel](https://en.wikipedia.org/wiki/Water_wheel)) | Undershot wheels run in fast shallow flow, 4-12 rpm. Ours: an undershot paddle wheel 4 m across, 16 floats, 6 rpm, axle running into the bank through a stone bearing like a mill wall |
| Tower / smock mill ([Brixton windmill glossary](https://www.brixtonwindmill.org/about/how-the-mill-works/glossary-of-terms/), [windmills.org.uk](https://windmills.org.uk/?p=704)) | Only the cap turns; a fantail at right angles turns it into the wind; common sails are cloth over a lattice on crossed stocks; windshaft → brake wheel → wallower → upright shaft. Sails ~15 rpm. Ours is a timber smock mill: octagonal body, turning cap, fantail, four common sails |
| Flatrods / Stangenkunst ([Flatrod system](https://en.wikipedia.org/wiki/Flatrod_system)) | Water-wheel power carried kilometres to mine pumps on timber lines. Our drive shafts on trestles carry a wheel's power into a cave to a drill: the same idea |
| Agricola, *De re metallica* ([Gutenberg](https://gutenberg.org/files/31024/31024-h/31024-h.htm)) | Piston pumps lifted by cams; bellows pressed by cams and returned by counterweights; iron-shod stamps raised by cams and dropped. Our pump, smelter bellows and stamp drill copy these |
| Sash sawmill ([Leonard's Mills](https://www.maineforestandloggingmuseum.org/wp-content/uploads/Leonards-Mills-Sawmill-Description-With-QR-Code.pdf)) | A crank and pitman rod drive a vertical frame saw; 2 ft stroke, ~36-120 strokes a minute, cutting on the down stroke. Our sawmill: flywheel crank, pitman, sash saw, log carriage |
| Oliver Evans's automated mill ([Oliver Evans](https://en.wikipedia.org/wiki/Oliver_Evans)) | Bucket elevators and screw conveyors joined machines in the 1780s: the period model for our conveyors |
| Square-pallet chain pump, force pump | We use Agricola's crank → rocking beam → piston pump, spouting into a stone trough that feeds the irrigation channel |

### 1.3 Lessons (short)

1. **Power is rotation you can see.** Every shaft, gear, wheel and belt turns at the network's real speed; a starved network visibly slows (R2, R26, R27).
2. **Never make players guess.** One state word and its reason on the prompt, the screen and the build ghost (Manor Lords / Dune complaint).
3. **The environment is the input.** Moving water for wheels, open sky for windmills, lava for turbines; the place matters.
4. **Graded slowdown, not a fuse.** Factorio's rule (the spec's).
5. **Medium automation.** Chains of 4 machines (6 with the perk), no factory sprawl.

### 1.4 Sound sources (CC0 unless marked; all pages checked by licence)

| Event | Source |
|---|---|
| Water wheel loop | BigSoundBank *Mill wheel, bladed* (https://bigsoundbank.com/mill-wheel-bladed-s2768.html, CC0) |
| Windmill loop (inside, wood and stones) | freesound pcwvdmark 345846 (CC0) + BigSoundBank *Wind and tree squeaks* 1051 (CC0) |
| Turbine roar / steam | BigSoundBank *Steam engine foley 2* 3311 + *Hiss of steam* 0227 (CC0) |
| Conveyor clatter | BigSoundBank *Manual chain hoist* 1134 + freesound hisoul 461166 wooden gears (CC0) |
| Drill / stamp knock | freesound leinadsorihcak 449241 trip hammer (CC0) |
| Millstones | freesound PLukx 430589 (CC0) |
| Sawmill | BigSoundBank *Sawmill 3* 1538 (CC0) |
| Loom | freesound Naïma 510266 shuttle (CC0) |
| Gear / ratchet click (gearbox, sorting chute) | BigSoundBank *Large ratchet* 0795 (CC0) |
| Pump | BigSoundBank *Manual water pump* 0542 (CC0) |
| Machine start / stop | BigSoundBank 3021 start, 3015 stop (CC0) |

---

## 2. Design

### 2.0 The rules of power (GAP-COOKED around the SPEC numbers)

- **Power is rotation.** Every source ends in an output shaft stub; every load takes an input stub. A stub is a
  **port**: a point and an axis on the piece. Two power pieces are joined when a port of one lies within 50 cm of a
  port of the other (the shaft's end meets the machine's stub). Shafts, long shafts, gearboxes and conveyors carry
  rotation; conveyors also carry items. Stations (forge, armorer, alchemy table, workbench, oven, smokehouse) have no
  stub of their own: a shaft port within 50 cm of a station's body powers it, and the shaft's end shows a **bronze
  drive pulley** with a leather belt down into the station's side (so the power visibly goes in).
- **Placing a shaft or conveyor snaps** its nearest end to a free port within 120 cm (location, height and the
  port's axis), so lines join exactly. The trestle legs under shafts and belts reach down to the ground wherever they
  stand (traced on every machine that draws).
- **Network**: every piece joined through ports. **Supply** = sum of running sources' output now. **Load** = sum of
  loads of machines that are switched on and have work (an idle smelter draws nothing; a lamp at night draws; a
  station with an order draws). **Speed** = min(1, supply ÷ load) (R2). A network with no load shows 100%.
- **Engineering speed**: a machine's work rate = Speed × (1 + owner's machineSpeed every-level 0.3%/level) ×
  overclock 1.5 (R23, R24). Every-level is read from its owner when online, else the value stored at the owner's last
  visit.
- **Awake**: a network advances only while a player is within 250 m of any of its pieces (R3). Asleep machines hold
  their progress; their parts stop turning.
- **Level gate**: machines are placed if the **builder owns (or their clan owns) a Hearth of that tech level
  anywhere** (it's knowledge, not the local Hearth, so a water wheel on a beach or a drill in a cave works). Biome
  machines are placed only in their biome unless stated.
- **Wear (R35)**: a running machine loses `machines.wearPerHour` 2% of its max health per hour of running; at 25%
  or less it runs at half speed ("Worn: repair it"). **Repair** on its screen: back to full for half its build cost
  scaled by what's missing, +10 Engineering XP. Self-repair perk: its owner's machines don't wear.
- **No damage to players**: wheels, saws and stamps never hurt anyone (no traps rule).

### 2.1 Sources

| Piece | Lvl | Cost | HP | Output | Where | SPEC/GAP |
|---|---|---|---|---|---|---|
| Water wheel | 2 | wood 40, bronze 6 | 1,000 | 10 P × flow | Its wheel 3 m out from the bank in moving water ≥ 60 cm deep: the surf line (shore within 15 m: flow 0.8, pulsing ±0.15 with the swell) or a river (flow 1.0, when the map has them) | SPEC numbers; GAP flow rule |
| Windmill | 3 | wood 50, linen 10, iron 6 | 1,200 | 6 P × wind (0.6-1.2, gusting); storm ×2; Storm peaks / Sky islands ×1.5 | Open ground: no terrain or piece above its hub (8.5 m) within 10 m | SPEC |
| Geothermal turbine | 4 | steel plate 12, stone 30 | 1,500 | 25 P | Its bore within 6 m of lava | SPEC (vents: no vents on the map yet, lava is the lava) |
| Soul engine | 5 | steel plate 10, soulstone 12, bone 20 | 2,500 | 40 P while burning (1 soulstone / h; 10-slot fuel box) | Anywhere | SPEC (Graveyard, hidden until live) |
| Storm coil | 5 | steel plate 8, stormglass 12, copper 20 | 2,500 | 60 P while a thunderstorm is over its biome; banks the surplus to its stormglass battery (holds 2 h at 20 P = 40 P·h), gives up to 20 P from it after | Storm peaks | SPEC |
| Sun mirror array | 5 | steel plate 8, sunstone 12, bronze 20 | 2,500 | 20 P × daylight; sunstone battery 4 h at 10 P | Desert | SPEC |
| Tide mill | 5 | steel plate 6, tidecoral 12, wood 40 | 2,500 | 20 P, rising to 30 at high tide (tide = sin over 12.42 game hours) | A tidal channel: wheel in sea ≥ 100 cm deep within 10 m of shore, Ocean | SPEC |

### 2.2 Transmission

| Piece | Lvl | Cost | HP | Shape |
|---|---|---|---|---|
| Drive shaft | 3 | wood 6, bronze 1 | 400 | A 4 m octagonal oak shaft Ø16 cm with iron bands every metre, on two A-frame trestles with bronze plummer-block bearings; ports at both ends (+ coupling flanges) |
| Long drive shaft (Long lines) | 3 | wood 9, bronze 2 | 500 | The same, 6 m, three trestles |
| Gearbox | 3 | wood 10, gears 2 | 600 | A 1 m oak cage on a stone pad: a horizontal bevel crown wheel on a vertical arbor meshes four bevel pinions on stub shafts out of each side: four ports. Turns 90° and branches |

### 2.3 Loads (machines)

Each has one input port (its drive stub) unless noted. Rates at 100% speed.

| Piece | Lvl | Cost | HP | Load | Does |
|---|---|---|---|---|---|
| Sawmill | 3 | wood 30, iron 6, stone 10, gears 2 | 1,000 | 6 P | 10 wood → 15 planks in 20 s (input 4 slots, output 4) |
| Grinder | 3 | stone 40, wood 20, gears 2 | 1,200 | 4 P | grain (any of the six grains) 1 → flour 1, 6 s; bone 1 → bone meal 2, 6 s; stone 2 → stone dust 1, 6 s |
| Loom | 3 | wood 30, fiber 10, gears 2 | 600 | 3 P | fiber 8 → linen 3 (×1.5 the bench's 4 → 1), 12 s; windflax 2 → linen 3, 12 s |
| Smelter | 3 | stone 40, iron 4, copper 4, gears 2 | 1,200 | 8 P | Every forge recipe whose output is an ingot (copper, bronze, iron, steel with charcoal, star iron's smelt), 6 s each |
| Irrigation pump | 3 | wood 20, bronze 4, hide 2 | 600 | 3 P | Water within 40 m: its trough is a channel source (channels join it; water runs on from it) |
| Conveyor | 3 | wood 4, hide 2, bronze 1 | 300 | ¼ P each | 2 m belt; moves one stack every 2 s (1 m/s); pulls from what its tail touches, gives to what its head touches |
| Long conveyor (Long lines) | 3 | wood 10, hide 5, bronze 2 | 400 | ¾ P | The same, 6 m |
| Sorting chest | 3 | wood 20, bronze 4, iron 2 | 500 | 2 P | 20 slots; takes from belts; every 2 s sends one stack to a chest within 20 m that already holds that item (its chute turns to it; the item drops in with a thunk); what no chest holds stays |
| Coil lamp | 3 | copper 2, iron 1, wood 4 | 200 | ¼ P | A post lantern lit by a little spinning coil at its foot: light without fuel while powered, night only (draws only when it's lit) |
| Drill | 4 | steel plate 6, wood 30, gears 4 | 1,500 | 8 P | Placed over an ore vein (within 1.5 m): 1 of its ore every 60 s into a 40-slot hopper; the vein never runs out while drilled; 1 Mining XP per 10 ore taken |
| Greenhouse lights | 4 | copper 4, iron 2, wood 6 | 1,500 | 4 P | Plots within 8 m under a greenhouse grow at night and underground |

**Machine parts (R22):** **Gears** (forge, level 2): bronze 2 + wood 1 → 1 gear set, 4 s, Engineering +15.
**Powered stations (R8):** a station a shaft runs into draws 5 P while it has an order and runs ×1.5 (× network speed).

### 2.4 Production chains (R19, R34)

A **chain** is machines joined by conveyors. Counting from where items enter (a chest or a drill), each machine a belt
delivers into adds 1. A belt won't deliver into the 5th machine (7th with Chief engineer, judged by the chain
owner's perk: the network owner): its head shows "Chain too long (4; Chief engineer: 6)". Reference chain: **drill →
belt → smelter → belt → sorting chest** (3 machines).

### 2.5 Biome machines (R20; all L5, 2,500 HP; hidden until their land is live)

| Biome | Machine | Built | Does (as built) |
|---|---|---|---|
| Volcanic | **Lava forge** | in the volcanic land within 3 m of lava; sulfur crystal 10, obsidian 6, steel plate 6, stone 40 | A forge station (every forge recipe) at ×2 speed, no fuel; **steel from 2 iron with no charcoal** (its own recipe) |
| Frozen tundra | **Cold store** | anywhere; lichen 12, ice block 10, steel plate 4, stone 30 | Every container under the same roof within 8 m keeps food ×0.05. In the tundra it needs nothing; elsewhere it's a 6 P load (its fan turns) |
| Storm peaks | Storm coil | 2.1 | 2.1 |
| Desert | Sun mirror array | 2.1 | 2.1 |
| Ancient forest | **Grove press** | in the forest; heartwood 12, wood 30, iron 4 | spoiled food 2 → compost 2 (×2 the bin), fiber 8 → compost 1; 10% a batch: a rare seed of a live land |
| Sky islands | **Updraft lift** | in the sky islands; levistone 12, wood 40, linen 10 | Two linked lifts ≤ 120 m apart; a levistone-buoyed platform rides the guide cable between them in 10 s a trip, eased; E on either calls / sends it |
| Crystal caves | **Resonance pylon** | anywhere; resonant crystal 12, bronze 10 | Joins every network with a piece within 40 m into one (no shafts) |
| Swamp | **Blood still** | in the swamp, standing in 10-50 cm of bog water; copper 10, bloodmoss 12, stone 10 | 1 bloodmoss → 1 slime every 5 min (no power) |
| Ocean | Tide mill | 2.1 | 2.1 |
| Graveyard | Soul engine | 2.1 | 2.1 |
| Fungal | **Mycelium vat** | in the fungal land, in the dark; deepcap 12, wood 30, iron 4 | 1 Oyster cap or Lion's mane a minute (alternating), eats 1 compost per 10 grown, only in the dark |
| Wasteland | **Rift chest** | anywhere; voidglass 12, iron 6, wood 10 | 60 slots shared by every rift chest of your clan (or you), on any claim |

### 2.6 Each object: why, connects, meets (R26-R28)

All models are composed in code from the building kit's blocks (wood and stone materials, iron = stone at 0.2,
bronze/copper/cloth flat colours) like the stations; moving parts are separate assemblies turning about their real axles.

- **Water wheel.** *Why:* the first power: it lets an early base on the coast run a sawmill and powered forge.
  *Connects:* the axle runs from the wheel back into the bank through a stone bearing pier to the output stub (port)
  at the land end, at the axle's height. *Meets:* a timber headstock on two stone piers sits on the bank; the wheel
  (4 m, two rims, 8 arms each side, 16 floats) hangs 3 m out, its lowest 50 cm in the water; a trestle post in the
  water carries the outer bearing, its foot traced to the sea bed. Water churns white where the floats enter (fx).
  Turns 6 rpm × speed of its own output use (a source turns as long as water runs; it free-wheels when nothing draws).
- **Windmill.** *Why:* power anywhere open, the mid-game generalist. *Connects:* sails → windshaft → (inside) upright
  shaft → spur gear at the base → a layshaft out through the wall: the port stub at the base, -X side, 90 cm up, in
  a little timber gear house. *Meets:* an octagonal smock body (4 m wide at the stone base, 2.6 m at the top, 6 m
  tall) of weatherboarding on a 60 cm stone plinth; a boat cap that turns to face the wind (the fantail at its back
  spins while it turns); four common sails (lattice of bars on the stocks, cloth across) sweeping 4.5 m, the tips
  clearing the ground by 1.8 m. Wind's direction veers slowly on one shared clock; the cap follows (no snap).
- **Geothermal turbine.** *Why:* the big mid-late source, tied to the volcanic land's lava. *Connects:* a stone
  well-head over a bore; an iron steam pipe rises to a riveted drum on a cradle; the turbine spindle comes out of the
  drum to a flywheel and the port. *Meets:* the bore head on the ground; steam vents from the exhaust stack (fx).
- **Soul engine.** A black-iron frame on a bone-and-stone plinth; a soulstone hopper drops into a glass-walled burn
  chamber (pale green glow while burning); its flywheel and port at the side.
- **Drive shaft.** *Why:* carries power from where it's made to where it's used (Stangenkunst). *Meets:* two A-frame
  trestles whose legs reach the ground; bearing blocks clamp the shaft; flanged couplings at the ends meet the next
  shaft's flange (no gap, no overlap).
- **Gearbox.** Turns a line 90° and splits it. Four stub shafts each with a flange; the bevels visibly mesh.
- **Sawmill.** *Why:* planks stretch wood 1.5× for building. *Meets:* a 3 m timber frame on sills; at the port end a
  flywheel with a crank pin; the pitman rod drives a sash (frame with a vertical blade) up and down in guide posts; a
  log on the carriage beside it; a plank pile at the far end. Strokes 1 per input turn.
- **Grinder.** A hurst frame carrying a pair of stones in a round wooden tun; a hopper above on a horse; the input
  shaft's bevel drives the stone spindle at 12× (the runner stone turns visibly fast); the damsel shakes the shoe.
- **Loom.** A floor loom frame: warp beam, cloth beam with linen rolled on it, two heddle frames lifting alternately,
  the batten swinging, all driven by a cam shaft from the port.
- **Smelter.** A stone shaft furnace (bloomery stack) 2.4 m tall, its tapping arch glowing; two leather bellows pressed
  by cams on a shaft from the port and lifted back by counterweights (Agricola); a stone ingot table at the arch.
- **Irrigation pump.** A crank on the input shaft rocks a beam; the beam's far end works a piston rod in a wooden pump
  barrel standing in a stone sump; the spout pours into a stone trough whose lip is where a channel joins.
- **Conveyor.** A timber frame on legs; a leather belt over a drive drum at the head and a tail drum, two idlers;
  cleats on the belt loop round the drums; items ride on top; the head drum turns (it carries the line's rotation).
- **Sorting chest.** A bound chest with a turning bronze chute on a spindle on its lid: it points at the chest it sends
  to; the item thunks into that chest.
- **Coil lamp.** A 2.3 m post, a bronze-capped glass lantern; at its foot a little iron housing with a copper coil
  spinning between two dark lodestones (the fantasy dynamo) on the stub that takes power.
- **Drill.** A timber headframe over the vein: a cam shaft from the port lifts an iron-shod stamp rod (two cams a turn),
  which drops onto the rock (a puff of rock dust and a knock each blow); broken ore slides down a chute into the hopper box.
- **Greenhouse lights.** A standing iron lamp tree with four glass globes on arms, a coil housing at its foot.
- **Lava forge.** A basalt forge whose hearth is open to a lava trough running through it; iron hood and chimney;
  an anvil on a stump; lava light.
- **Cold store.** An insulated plank cabinet packed with ice blocks, frost on its front, a fan wheel on the side on the
  power stub (turns only outside the tundra).
- **Storm coil.** An 8 m iron mast on stays, its copper crown and spiral coil; a stormglass battery drum at its foot
  whose glass glows by charge; the spark motor's flywheel and port.
- **Sun mirror array.** Six polished bronze mirrors on a curved frame tilting to follow the sun (pitch and yaw from
  the time of day) toward a boiler on a mast; a piston engine and flywheel at the foot; a sunstone battery block.
- **Tide mill.** A heavier wheel like the water wheel on a stone causeway head, sluice boards.
- **Grove press.** A heartwood screw press: a great wooden screw turning down a platen into a slatted vat.
- **Updraft lift.** A stone pad and timber gantry with a guide cable anchor; the platform (plank deck, rail, levistone
  floats under it glowing faintly) rides a trolley on the cable to the linked lift, eased in and out over 10 s.
- **Resonance pylon.** A crystal obelisk on a bronze tripod, a ring turning slowly round it.
- **Blood still.** A copper alembic over a peat fire, its worm coiling into a jar, a stirring paddle.
- **Mycelium vat.** A great oak vat of dark compost with mushroom flushes growing over its rim (by its output).
- **Rift chest.** A chest banded in voidglass, a slow purple swirl in a glass eye on its lid.

### 2.7 States (one word + reason; prompt, screen, ghost)

Running · Idle (nothing to do) · No power (network gives a of b) · Slow (network a of b: 62%) · Output full ·
Switched off · Worn (repair it) · Asleep (no one near) · No water / No wind / Blocked (sources' reasons) ·
No fuel · Needs the dark · Chain too long.

### 2.8 Screen (EWRMenu::Machine, WRUIMachine.cpp; R29)

Stone-and-leather window like the Hearth/Door screens: title band (name, owner, state chip), a **power panel**
(supply / load, the % bar, how many sources and machines on the line, this piece's own output or draw now and why),
**controls** (On/Off; Overclock if you have the perk; Repair with its cost; Link for lifts), **slots** (input and
output, fuel, hopper, the sorting chest's 20, rift chest's 60: click a bag slot to put in, click a machine slot to take,
Take all), a **progress bar** and its recipe list for processors. Mouse and pad: every control is a button or slot
(the pad input processor drives them). Esc / B closes. F1 page: no new keys (E opens it like a chest).

### 2.9 Engineering (R22-R24)

| What | XP / effect |
|---|---|
| Placing a machine (any power piece or station) | 30 (key `machinePlace`, built) |
| Each running machine you own | 1 per 5 min running, up to 10 counted (`machineRun5m`) |
| Repairing a machine | 10 (`machineRepair`) |
| Making gears | 15 (`machinePart`) |
| Every level | machine speed +0.3% (data: perLevel 0.003; stations keep it too) |
| Efficient power | your machines draw 20% less |
| Tinkerer | stations, code-lock doors and machines cost 25% less (built value kept) |
| Long lines | the 6 m shaft and 6 m belt |
| Overclock | your orders 60% faster burning wood (built) and your machines can be set to overclock: ×1.5 at ×2 power |
| Self-repair | stations and doors mend (built); your machines don't wear |
| Chief engineer | bigger orders, a queue slot (built); chains 6 long |
| Turret tech, Siege engineer | stay hidden (turrets, siege weapons aren't built) |

### 2.10 Sound (R25)

Continuous loops attached to the machine, 3D, falling off over 30 m (`radius 3000`), volume and pitch follow its
speed (pitch 0.85-1.05), fading in over the spin-up: `machine.waterwheel`, `machine.windmill`, `machine.turbine`,
`machine.conveyor`, `machine.grinder`, `machine.sawmill`, `machine.loom`, `machine.pump`, `machine.smelter`
(bellows: the steam chuff slowed), `machine.coil` (lamps, storm coil, soul engine: a low hum from the gear rattle).
One-shots synced to the mechanism: `machine.drill` on each stamp blow, `machine.sort` (ratchet) per send,
`machine.start` / `machine.stop` when a network wakes / stops.

---

## 3. Plan (build steps)

### 3.1 Data

| File | Rows |
|---|---|
| `Data/machines.json` (new) | `rules` (portSnap 50, snapRange 120, activeRange 25000, stepSeconds 0.5, stationLoad 5, stationSpeed 1.5, overclockSpeed 1.5, overclockPower 2, efficientPower 0.8, chain 4, chiefChain 6, wearPerHour 0.02, wornAt 0.25, wornSpeed 0.5, repairCostFraction 0.5, runXpMinutes 5, runXpMax 10, beltSeconds 2, sortRange 2000, sortSeconds 2, drillSeconds 60, drillReach 150, hopperSlots 40, flow{surf 0.8, swell 0.15, river 1}, wind{min 0.6, max 1.2, storm 2, biomes{storm_peaks 1.5, sky_islands 1.5}, clear 1000, period 5400}, tideHours 12.42, ...) and `machines` per piece id: role (source/shaft/gearbox/processor/conveyor/sorter/drill/pump/lamp/growlight/coldstore/lift/pylon/chest/forge), supply, load, ports, rpm, slots, recipes station, special numbers |
| `Data/pieces.json` | 26 new pieces, category `machines`, kind `machine` (or `station` for the lava forge), costs/HP/levels above, `biome` for biome machines, `note` text for the build card |
| `Data/items.json` | planks, flour, bone, bone meal, stone dust, gears |
| `Data/recipes.json` | gears (forge 2, skill engineering), sawmill / grinder / loom rows (stations `sawmill` etc.), smelter reuses forge ingot recipes, lava forge steel |
| `Data/creatures.json` | bone in the loot of the island's beasts |
| `Data/cooking.json` | flour in Grain |
| `Data/lifeskills.json` | engineering perLevel 0.003, perks un-hidden, new texts and stats; XP keys |
| `Data/tuning.json` | lifeSkills.xp machineRun5m 1, machineRepair 10, machinePart 15 |
| `Data/Audio/sounds.json` + `Data/Audio/machine/*.ogg` | the 2.10 rows from the 1.4 sources (Tools/audio/build_machine_audio.py) |
| `Data/UI/Icons` | planks, flour, bone, bone_meal, stone_dust, gears (Tools/art/make_machine_icons.py) |

### 3.2 C++

| Where | What |
|---|---|
| New `WRMachine.h/.cpp` | `FWRMachineDefs` (machines.json); `UWRMachineSubsystem` (server: register, ports, networks, step, conveyors, sorting, drills, chains, wear, XP; client: nothing); `FWRMachines` statics: PlaceCheck, Interact, Describe, commands (`mon`, `moverclock`, `mput`, `mtake`, `mtakeall`, `mrepair`, `mlink`), save/load |
| New `WRMachineLook.h/.cpp` | Frame blocks per machine (into `FWRBuildLook::Blocks`), moving assemblies (spin / slide / swing / loop / aim), the client subsystem that turns them smoothly at the replicated speed with inertia, belt items, loops sounds, lights |
| New `WRUIMachine.cpp` | The machine screen (2.8) |
| `WRBuilding.h/.cpp` | Piece fields: `MachineRep` (replicated struct: speed, state, flags, supply, load, progress, battery), `BeltItems`; `Note` def field; collision parts for every machine; save/load hook; register with the machine subsystem; Interact → machine |
| `WRCrafting.cpp` | Powered station speed (rebased timestamps), lava forge alias + ×2, recipe `skill` field |
| `WRFarm.cpp` | Pump as a channel source; greenhouse lights; cold store; stone dust as fertiliser |
| `WRPlayerBuild.cpp` | Level gate by owned Hearth; port snap for shafts/belts; planks pay wood; machine XP; Tinkerer on machines; placement checks |
| `WRPlayerMenus.cpp`, `WRPlayerController.h`, `WRPlayerUI.cpp` | EWRMenu::Machine, E opens it, screen sync |
| `WRUIHud.cpp` | Prompt line for machines |
| `WRUIBuild.cpp` | "Machines & power" tab, note text, planks count as wood |
| `WRSound.h/.cpp` | `StartLoop` / loop volume-pitch for machines |
| `WRLifeSkills` | nothing new (stats read by name) |

### 3.3 Build order

1. Data (machines.json, pieces, items, recipes, lifeskills, tuning, creatures, cooking).
2. `WRMachine` core: defs, subsystem, ports, networks, sources, loads, processors, stations, conveyors, sorting, drill,
   pump, lamps, wear, XP, biome machines, save/load, commands.
3. Building hooks (fields, collision, register, interact, save), placement (gate, snap, checks, planks, cost).
4. Looks: frames + moving parts + client turner + belt items + light + sounds.
5. Screen + menu + prompt + build tab.
6. Farm hooks, crafting hooks.
7. Icons, audio.
8. Compile, fix warnings, spot-check against this plan, CHANGELOG / HANDOFF, commit, push.

---

## 4. Spot-check (read against the plan and THE BAR, 2026-11-14)

Fixed while reading the code back:
- The plan's station drive (2.0) had no visual: added the bronze pulley and leather belt where a shaft's port runs into a
  station (`WRMachineLook.cpp` 1416).
- Sending a lift from the far end toggled the wrong platform: either end now drives the owner's (`WRMachine.cpp` SendLift).
- A new rift chest started empty: it now opens onto its clan's shared slots (`UWRMachineSubsystem::Register`).
- Storm coil / sun array batteries drained while the sun still gave power at dusk: the live share is kept apart (`LiveP`).
- A dedicated server never built the lift platform's collision: `AWRBuildPiece::ApplyLook` builds it there too.
- Geometry: the sawmill's sash always passes under / over the log (crank 26 cm, rod 72 cm, log on bolsters at 148-192,
  head beam at 350); the gearbox crown's cogs reach the lanterns (crown at -22, ratio 10/26); the grinder's crown wheel
  meets the stone nut's staves at its top (crown at x -22, nut on the spindle at x 0, ratio 4); the pump's beam follows
  its link exactly (Beam / BeamTip motions from the same crank kinematics).
- Sounds play on the machines that hear them (client visuals), not from the server.

Known limits (written in CHANGELOG "Not finished"): no rivers or vents on the island yet (wheels run in the surf line,
turbines by lava); turret and siege perks stay hidden; ten biome machines are built but hidden with their lands.

## 5. Ledger proof

| # | Done | Where |
|---|---|---|
| R1 | Yes | Ports + union-find networks `WRMachine.cpp:933` (Rebuild); ports `WRMachineLook.cpp:1141`; drive shaft 4 m / gearbox models `WRMachineLook.cpp:254, 270` |
| R2 | Yes | `WRMachine.cpp:1790` (speed = min(1, supply / load)); rate per machine `Step` |
| R3 | Yes | `WRMachine.cpp:828` IsAwake; asleep branch in `Step`; station queues keep timestamps `WRCrafting.cpp` AdvanceQueue |
| R4 | Yes | `Data/pieces.json` water_wheel; flow `WRMachine.cpp:1139`, FlowAt; placement (bank, moving water, axle clear) `WRMachine.cpp:317`; model `WRMachineLook.cpp:307` |
| R5 | Yes | `WRMachine.cpp:1155` (wind, storm x2, biome x1.5, blocked look-round); WindAt; placement clearance `PlaceCheck`; model `WRMachineLook.cpp:349` (cap turns on wind, fantail) |
| R6 | Yes | `WRMachine.cpp:1180` (lava within 6 m), placement; model `WRMachineLook.cpp:430` |
| R7 | Yes | `WRMachine.cpp:1187` (1 soulstone / h, 40 P, burns only while drawn); hidden until graveyard (`liveWith`); model `WRMachineLook.cpp:458` |
| R8 | Yes | Station on a line `WRMachine.cpp` Rebuild (stations) + `:1891`; `WRCrafting.cpp:403` SpeedHere, retiming in AdvanceQueue; pulley `WRMachineLook.cpp:1416` |
| R9 | Yes | `Data/recipes.json` saw_planks; planks pay for wood `WRMachine.cpp:465` (Pay / CanPay), `WRPlayerBuild.cpp:760`, `WRUIBuild.cpp` Have |
| R10 | Yes | recipes grind_* (grains hidden with their lands, bone, stone); bone loot `Data/creatures.json`; model `WRMachineLook.cpp:536` |
| R11 | Yes | recipes loom_linen, loom_windflax; model `WRMachineLook.cpp:576` |
| R12 | Yes | recipes smelt_* (6 s); `WRMachine.cpp:1311` StepProcessor; model `WRMachineLook.cpp:611` |
| R13 | Yes | Pump demand `WRMachine.cpp:395` (water within 40 m); channel source `WRFarm.cpp:1275`; model `WRMachineLook.cpp:665` |
| R14 | Yes | `WRMachine.cpp:1377` StepConveyor (a stack / 2 s, jams stop it); riders `WRMachineLook.cpp:1631`; cleats `:1566`; load 0.25 `machines.json` |
| R15 | Yes | `WRMachine.cpp:1495` StepSorter (chest within 20 m holding the item); chute `WRMachineLook.cpp` Chute |
| R16 | Yes | `WRMachine.cpp:1530` StepDrill (1 / 60 s, 40-slot hopper); vein never runs out `WRResourceNode.cpp:260`; Mining XP on emptying `WRMachine.cpp` mtake; cam stamp `WRMachineLook.cpp:1516` |
| R17 | Yes | Grow lights `WRFarm.cpp:523` (lit walled room = greenhouse, not dark) |
| R18 | Yes | Coil lamp demand at night `WRMachine.cpp:533`; light `WRSurvival.cpp:556`, tuning lights.coil_lamp |
| R19 | Yes | `WRMachine.cpp:1349` ChainDepth; limit 4 / Chief 6 in StepConveyor; state ChainLong |
| R20 | Yes | 12 machines: lava forge (station, x2, `lava_steel`, `WRCrafting.cpp:191`), cold store (`WRMachine.cpp:872`, `WRFarm.cpp:1364`), storm coil / sun (`WRMachine.cpp:1204` + batteries), grove press / still / vat / pylon / rift (`WRMachine.cpp:1548`, `:902`), lift (`WRMachine.cpp:1664`, `WRMachineLook.cpp:1229`), tide mill (`WRMachine.cpp:1145`), soul engine (R7); models `WRMachineLook.cpp:836-1024` |
| R21 | Yes | `biome` / `liveWith` in pieces.json; `WRFarm.cpp` IsPieceLive; build tab hides them |
| R22 | Yes | machinePlace `WRPlayerBuild.cpp:774`; machineRun5m `WRMachine.cpp:1710`; machineRepair `WRMachine.cpp:753`; machinePart (gears recipe `skill`) `WRCrafting.cpp` MakeOne |
| R23 | Yes | `Data/lifeskills.json` perLevel 0.003, applied to machines (`OwnerSpeed`) and stations |
| R24 | Yes | `Data/lifeskills.json` perks live; effects: Efficient `Demand`, Tinkerer `WRPlayerBuild.cpp:141`, Long lines `needsStat` + PlaceCheck, Overclock `moverclock`, Self-repair (no wear in Step), Chief (chain 6) |
| R25 | Yes | `Data/Audio/machine/*.ogg`, sounds.json machine.*; loops `WRSound.cpp:776`, `WRMachineLook.cpp:1681`; drill knock on each blow `:1516`; sort, start / stop |
| R26 | Yes | Models with real drive trains: gears, lanterns, crank and rod, cams, bellows, belts (`WRMachineLook.cpp:254-1024`); ports defined with the models |
| R27 | Yes | One eased drive angle per machine, exact crank / beam kinematics, cap slews, lift platform eased (`WRMachineLook.cpp:1377`) |
| R28 | Yes | Section 2.6 (why / connects / meets) built as written; legs and posts traced to ground / sea bed |
| R29 | Yes | `WRUIMachine.cpp:384` (buttons and slots: mouse and pad); `-oldui` panel `WRPlayerMenus.cpp:559`; E opens it `WRPlayerMenus.cpp:211` |
| R30 | Yes | Server-run subsystem; `FWRMachineRep` / `Belt` replicated (`WRBuilding.h`); saved `WRBuilding.cpp:1224, 1278`; lift collision on every machine |
| R31 | Yes | `Data/machines.json` rules and rows; costs in pieces.json; recipes; tuning XP |
| R32 | Yes | Icons `Tools/art/make_machine_icons.py` (planks, flour, bone, bone meal, stone dust, gears); every piece's card from its `note` (`WRUIBuild.cpp`) |
| R33 | Yes | This doc (research 1, design 2, plan 3, spot-check 4) |
| R34 | Yes | Chains capped at 4 (6) |
| R35 | Yes | Wear in `Step` (`WRMachine.cpp:1824`), Worn state, repair command |
| R36 | Yes | `Data/pieces.json` costs and HP as 2.17 (biome machines 2,500; turbine / drill / lights 1,500) |
