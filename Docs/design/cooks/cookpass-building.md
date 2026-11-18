# Wildreach cook pass: BUILDING AND RAIDS

Lane cookpass-building, 2026-11-15 (desktop). A cook pass on what is already built for building and raids: research
how the best games do it, grade every built piece against THE BAR, design the raise, build it. Built before this pass:
build-building (tiers, sets, beams, integrity, decor, turrets; cooks/build-building.md), build-server-raids (raid
economy, petard, ladder, code breaker, raid bell, path and tame raiding; cooks/build-server-raids.md), Raven's snap
building, claims, upkeep, creature raids (`WRRaids.cpp`). Specs followed: FULL-GAME 6.4 / 7, biome-crystal 2.21.5
(siege engines, KEPT numbers), ui-controls 2.1 / 2.11 (pad build mode, build bar), survival-crafting 2.18.

## 0. Requirement ledger

Split from the brief, every Aaron quote in it, the LATER-COOKS lines that name this scope, and the spec rows the grade
found unbuilt. Proof column is filled in section 6.

| # | Requirement | Source |
|---|---|---|
| L1 | Research how the best games do building and raids: name games, places, systems, what makes them work; real-world reference; written here with the details used | brief, Aaron 11-13 night |
| L2 | Plan every detail and solve every gap in writing before building; every build step listed | brief |
| L3 | Grade every built building / raid piece against THE BAR | brief |
| L4 | Design the raise and build it at full detail; no placeholders, no primitives, no "good enough" | brief |
| L5 | Spot-check own work against the plan and THE BAR by reading code; fix before push | brief |
| L6 | "Already done" only with file:line proof | brief |
| L7 | Design with intention: every object/mechanism says why it's here, what it connects to, how it meets what it touches | Aaron 11-14 |
| L8 | Machines look engineered: parts mesh, shafts/chains lead somewhere, nothing passes through anything | Aaron 11-14 |
| L9 | Moving parts move continuously and smoothly on a believable mechanism, never snap or teleport on a tick | Aaron 11-14 |
| L10 | No missed requirements: ledger, proof per row, final message lists all | Aaron 11-14 |
| L11 | Wildreach is not a music game: no music shown on screen; no mechanic that needs playing to the music; remove any found in scope | Aaron 11-15 |
| L12 | Data-driven: numbers in Data/*.json + tuning.json | THE BAR |
| L13 | All damage to characters through ApplyHit | THE BAR |
| L14 | Screens / inputs: mouse AND pad | THE BAR |
| L15 | Sounds and fx as table rows | THE BAR |
| L16 | Server authority, replication, multiplayer edge cases | THE BAR |
| L17 | Nothing unfinished shown as finished | THE BAR |
| L18 | Compile gate, no new warnings; unity-safe unique prefixes | brief, lane note |
| L19 | Git rules: pull, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, land in pieces, no release/tag | brief |
| L20 | Report new binary sizes; LANDED line; out-of-scope finds to LATER-COOKS | brief |
| L21 | Siege weapons (FULL-GAME 7 "Explosives, raid tools, siege weapons" RAVEN-PLANNED; biome-crystal 2.21.5 KEPT): battering ram | grade: unbuilt |
| L22 | Catapult (counterweight trebuchet) with the arc shown to everyone, 3 range pins, two crew | biome-crystal 2.21.5 |
| L23 | Ballista: windlass draw 2 s, 0.9 s red aim line, straight bolt, never homes | biome-crystal 2.21.5 |
| L24 | Haulers push rams | survival-crafting 2.18, build-server-raids R11 (left open) |
| L25 | Aegis stops ballista bolts; a catapult stone breaks the dome | biome-crystal 2.17 / 2.21.6 (Aegis side built) |
| L26 | Siege engineer perk (Engineering 100) live | lifeskills.json waitsFor "siege weapons" |
| L27 | Path-based raiding for every path, not only Fire/Frost (lightning metal x1.5, spore wood x1.5, void x1.25, arcane stone x1.25...) | build-server-raids 2.6 "apply the day those paths' spells exist" |
| L28 | Pad build mode: RT place, LB/RB rotate -/+22.5 deg (hold repeats 0.12 s), D-pad L/R piece, D-up category, D-down snap, Y demolish, X copy, B put away | ui-controls 2.1, LATER-COOKS (cookpass-ui) |
| L29 | Copy: the piece you aim at, with its rotation (X on pad, middle mouse) | ui-controls 2.1 / 2.11 |
| L30 | Build costs also count nearby storage inside your claim | ui-controls 2.11, LATER-COOKS 11-13 (ui-controls lane) |
| L31 | "Survival" category first for new characters (workbench, lean-to, bedroll, campfire, claim totem); after the first claim the bar opens on the last category used | ui-controls 2.11 |
| L32 | The placement chip gives the reason in 1 to 4 words ("Needs support", "Outside your claim", "Too steep", "Blocked", "Missing 2 Wood") | ui-controls 2.11 |
| L33 | Lean-to exists (FULL-GAME 2's first hour: "built a workbench and a lean-to") | FULL-GAME 2, ui-controls 2.11 |
| L34 | The hot spring bath also on a tundra hot spring (Steam Ground) | build-building R16, LATER-COOKS (cookpass-world) |
| L35 | The ladder climb has a climbing body, not the idle pose | HANDOFF known gap (build-server-raids) |
| L36 | Siege crew have a body pose that matches the work (push, crank, aim) | L4 / L9 applied to L21-L23 |

## 1. Research

### 1.1 Siege engines in games

| Game / place / system | What it does | What makes it work | What we take |
|---|---|---|---|
| **Valheim, Ashlands (2024): Battering ram and Catapult** at the Charred Fortresses ([TechRaptor guide](https://Techraptor.net/gaming/guides/valheim-siege-weapons-guide), [wiki](https://breezewiki.discard.no/valheim/wiki/Battering_ram)) | The ram is pulled like a cart by one player; it burns wood/ashwood from a fuel slot (one every 20 s); its arms punch (600 pickaxe damage) only while it is being moved; the catapult is also cart-pulled and is built at the attack site; both are repaired with the hammer | Siege is a slow, visible, *crewed* object you bring to the wall; the ram must get close and is exposed, the catapult works from a safe distance: two roles, two risks | Ram = exposed, close, door-breaker; trebuchet = far, slow, visible. Engines are hit-pointed objects anyone can break |
| **Conan Exiles, Update 23 trebuchet** ([gamerguides](https://gamerguides.com/conan-exiles/guide/getting-started/basics/how-to-build-and-use-a-trebuchet-conan-exiles), [mein-mmo](https://mein-mmo.de/en/conan-exiles-trebuchet,148595/)) | Assembled in place from base, frame, arm; immovable once built; aimed by turning it; range set by loaded counterweight; fired with a lever; boulders cost 150 stone | Range is a physical setting you choose, and the ammo is a material cost; the engine can't chase | Range by the sling's release pin (3 settings), turned on a turntable, ammo = a dressed siege stone made from stone. Immovable once placed |
| **V Rising, Siege Golem stone** ([wiki](https://vrising.wiki.spellsandguns.com/Siege_Golem_Stone), [g-portal](https://www.g-portal.com/wiki/en/v-rising-siege-golem/)) | 150 s to activate in the open, can't be placed inside a castle, server announces it | The defenders get time and a warning; siege is never instant | Engines need a crew working them over seconds (draw 1.5 s, winch 10 s, draw 2 s), and their first blow rings the claim's raid bell (built) |
| **Rust raid economy** ([cheat sheets](https://rust-survival.pages.dev/10_Cheatsheets.html)) | Every wall tier has a fixed price per raid tool | Raiders can plan | A published cost per wall: siege stone 500 → wood 1, stone 2, metal 5, biome 6 (below) |
| **Mount & Blade II: Bannerlord sieges** | Rams roll on wheels pushed by a crew walking inside; trebuchets/onagers fire on a visible arc; ballistae are aimed directly by a gunner | Each engine has one clear posture: pushers lean on handles, winders crank, the gunner aims along the stock | Crew poses: push, crank, aim (L36) |

### 1.2 Build mode on a pad and from storage

| Game / system | What it does | What we take |
|---|---|---|
| **Fortnite / Palworld / ARK console build modes** | Place on the right trigger, rotate on the shoulders, D-pad cycles pieces | ui-controls 2.1 (already spec): RT place, LB / RB rotate, D-pad pieces |
| **Enshrouded** ([wiki controls](https://enshrouded.wiki.gg/wiki/Controls), [feature board](https://enshrouded.featureupvote.com/suggestions/525049/comment/616353)) | R rotates 90, X toggles snapping, Y undoes; players ask for copy-the-part-you-hover | Copy what you aim at, with its rotation (X / middle mouse) |
| **Valheim "Craft from containers" (the most-installed QoL mod)** and **Enshrouded base storage** | Stations take from chests nearby | Building takes from your chests within 20 m inside your claim: the bag first, then the nearest chest |
| **Valheim's first night** | A workbench and a two-post lean-to make "shelter"; resting needs a roof | Lean-to: the first roof; counts as under a roof for the bedroll's rest and comfort |

### 1.3 Real-world reference

| Source | Detail used |
|---|---|
| **Battering rams** (Roman *aries*, medieval *testudo arietaria*) | A log hung by chains or ropes from an A-frame inside a wheeled shed with a hide roof (wet hides against fire); the crew swing the log back and let it fall forward: it's a pendulum, the blow is at the bottom of the arc. Iron (here steel) cap on the head. Solid plank wheels on axles in bearings |
| **Counterweight trebuchet** (Warwolf 1304; reconstructions: Caerphilly, Middelaldercentret) | Arm on an axle between two A-frame towers, beam ratio ~4:1 to 6:1; a hinged box counterweight of stones; a sling of rope and leather on the long end, one sling loop on a release pin at the tip (the pin's angle sets the release, so the range); a winch with a ratchet pawl hauls the arm down; a trigger pin holds it. Shot: 40-140 kg dressed stones, 50 to 90 cm |
| **Ballista / springald** | Two arms in torsion skeins or a steel bow on a stock; a windlass with a ratchet draws the string to a claw; a trigger releases; the stock swivels on a tripod or post (yaw) and a pivot (pitch). Iron-headed bolt ~1.2 m |
| **Ladder climbing** (rung climbing technique) | Three points of contact: one hand reaches up while the opposite foot steps; a full cycle climbs two rungs (2 x 30 cm) |

## 2. Grade of what is built (against THE BAR)

| Piece | Where | Grade | Raise |
|---|---|---|---|
| Snap building, ghost, snap dots, demolish | WRPlayerBuild.cpp | Good: server checks, ghost colours, dots | Pad mode incomplete: RB places, RT rotates, D-pad still fires abilities, no copy (L28/L29) |
| Build bar | WRUIBuild.cpp | Good cards, pages, details | Chip shows a sentence with item ids ("need 3 wood"); no Survival tab; bar always opens on Foundations; costs count the bag only (L30-L32) |
| Tiers, sets, beams, integrity, collapse | WRBuildSets / WRBuildIntegrity / WRBuildLookSets | Good (build-building proof) | none |
| Comfort decor, Hearth offering | WRBuildLookDecor, WRBuildSets | Good | Hot spring bath only by lava (L34); no lean-to: the first night has no roof short of a full hut (L33) |
| Turrets | WRTurret.cpp | Good: tells, straight bolts, ApplyHit | none |
| Claim totem, upkeep, decay, claim screen | WRBuilding, WRUIClaim | Good | none |
| Creature raids | WRRaids.cpp | Good: warned, rings the bell, record | none |
| Petard, mining charge, fire pot + arc, bell, code breaker | WRRaiding.cpp, WRRaidLook.cpp | Good; code breaker live now (resonant crystal's land is live: tuning world.liveBiomes) | none |
| Siege ladder | WRRaiding.cpp, WRCharacterMovementComponent.cpp | Mechanism good | The climber hangs in the idle pose (L35) |
| Path raiding | WRRaiding::Scale, WRAbilities AreaSpell, WRProjectile | Fire/Frost and every bolt (AWRProjectile) hit buildings | The ten later paths' own area blasts (Arcane blast, Starfall, Thunderclap, Blood boil, Crimson tide, Corpse explosion, Collapse, Maw, Solar flare, Sunburst, Maelstrom, High tide, Plunge, Detonate) never touch a building, so lightning / spore / void / arcane multipliers do nothing for area casts (L27) |
| Necromancy raise through walls | WRAbilitiesNecro.cpp:258 | Built by build-biome-graveyard | none |
| Siege engines | none | Missing: FULL-GAME 7 row, biome-crystal 2.21.5 | Build all three (L21-L26, L36) |
| Music on screen | WRUI*.cpp grep (no staff, notes, beat UI; captions only for world sounds, WRSound.cpp:851) | None found in scope | Nothing to remove (L11) |

## 3. Design (every gap solved)

### 3.1 Pad build mode and copy (L28, L29)
- While placing with a pad as the last device: **RT** (Heavy) places; **RB** (Light) turns +22.5 deg, **LB** (Block) -22.5;
  held, each repeats every 0.12 s (`building.padTurnStep` 22.5, `building.padRepeat` 0.12, after `building.padRepeatDelay`
  0.3). With a mouse: unchanged (LMB place, RMB turn, wheel fine turn).
- **D-pad left / right**: the previous / next piece of the open category (wrapping), skipping pieces you can't build
  (blueprint not known, land not live). **D-pad up**: the next category (its first buildable piece). D-down snap (built),
  Y demolish (built), B put away (built).
- **Copy** (pad X, mouse middle button, key `BuildCopy`, rebindable): aiming at a built piece within 8 m starts placing
  the same piece facing the same way (and a beam at its pitch); "You can't build that" for a piece you couldn't place
  (blueprint, Hearth level, land). Works whether or not you were placing; opens build mode.
- The keys panel shows these buttons on a pad.

### 3.2 Build bar (L30-L32)
- **Survival tab** first: workbench, lean-to, bedroll, campfire, storage chest, torch, claim totem (pieces.json
  `"starter": true`; they stay in their own tabs as well). The bar opens on Survival while you own no claim; once you do,
  on the tab you last used (kept per PC in `GameUserSettings [Wildreach.Build] LastTab`).
- **Chip**: 1 to 4 words, the full reason on the line under it. Map: no aim → "Aim at the ground"; too far → "Too far";
  no support → "Needs support"; overlap / blocked → "Blocked"; outside / someone's claim → "Outside your claim" /
  "Someone's claim"; underground → "Not underground"; hearth level → "Needs Hearth N"; blueprint → "Needs its blueprint";
  steep → "Too steep"; cost → "Missing 2 Wood" (the item's name).
- **Nearby storage**: chests (kind `chest`, not the ice box's food) of your claim within `building.storageReachCm` 2000 of
  you that you may open count toward a piece's cost. The card says "you have 12 · +30 in chests". The server pays the bag
  first, then chests nearest first (planks pay for wood in both). A chest outside your claim or not yours never pays.

### 3.3 Lean-to (L33)
- **Why**: the first night's roof, before a hut: it makes a bedroll "under a roof" (rest + comfort counting) and keeps
  rain off a torch under it. **Connects**: wood and fiber you gather in the first ten minutes; Survival tab; no Hearth.
- **Cost** wood 10, fiber 8 (hand-built: no station), 250 HP, wood material, decor kind `shelter`, placed freely on the
  ground, collision: the sloped roof slab and its two posts.
- **Look** (real: a bushcraft lean-to): two forked uprights 1.7 m set 2.4 m apart, a ridge pole lashed in the forks, five
  rafter poles from the ridge down to the ground 2.0 m behind at ~40 deg, their butts dug in; overlapping rows of conifer
  boughs (dark green, layered from the bottom up like shingles so rain runs off); fiber lashings at each fork; a log
  laid at the back foot as a wind block. Faces +X (open side); a bedroll fits under it (2.4 x 1.8 m).
- `WRBiome::IsUnderBuiltRoof` counts a `shelter` piece's roof.

### 3.4 Hot spring bath on the tundra (L34)
The bath's rule (lava within 8 m) also passes inside a **hot spring**: the tundra Steam Ground (Data/island_places.json
`steam_ground_t`) and any `hotSpring` place within its radius. Rule: `building.hotSpringPlaces` lists place ids; the
check reads their centre and radius from island_places.json.

### 3.5 Climb and crew poses (L35, L36)
- **Climb** (ladder, `CMOVE_Ladder`): arms reach up alternately (upper arm forward-up 150 deg at the top of its reach,
  elbow 30-80 deg), the opposite leg steps (thigh flex 20-70 deg, knee 30-95 deg); phase from height climbed: one cycle
  per 60 cm (two rungs), so each hand lands on a rung; still on the ladder the pose holds where it is. Every machine
  computes it from the replicated movement mode and the capsule's height.
- **Push** (ram handle): torso leaned 28 deg, arms forward 70 deg, elbows 35 deg, legs in a long stride: the walk cycle
  under it plays as it moves.
- **Crank** (trebuchet winch, ballista windlass): arms forward 55 deg; elbows 40-95 deg and shoulders rising/falling on
  the crank's angle (the hands go round with the handle), torso leaned 12 deg.
- **Aim** (ballista gunner, trebuchet aimer): arms forward 50 deg, elbows 70 deg on the grips, spine pitched to the aim.
- Pose numbers: tuning `anim.crew.*`, `anim.climb.*`.

### 3.6 Siege engines (L21-L26; KEPT numbers from biome-crystal 2.21.5, mechanisms GAP-COOKED here)

**Common rules**
- Engines are **items** (`battering_ram`, `trebuchet`, `ballista`, category `siege`), made at the workbench at
  **Hearth 4**: ram steel plate 6 + heartwood 10 (+ hide 8 for the roof, rope 4 for the chains: GAP, the KEPT recipe has
  no roof or chains); trebuchet wood 60 + steel plate 6 + rope 10; ballista steel plate 8 + wood 30.
- **Use** from the bag/belt: the engine stands 3 m in front of you on ground flatter than 20 deg (refused: "Too steep",
  "No room", in a safe zone "Not in a safe zone", in a cave "Not underground"). It **may** stand inside another clan's
  claim (that's what it's for). Ram and trebuchet need a PvP world ("No raiding on this world"); the ballista works on
  any world (it shoots creatures).
- **Health**: ram 2,000 (+ roof 800), trebuchet 1,500, ballista 800. Hurt by weapons x0.25, blasts (petard x1, its
  charge number for wood), fire pots and burning ground x1 (wood engines burn; the ram's wet hide roof x0.5), spells
  x0.5, tames x0.5, siege stones x1; anyone hostile to its owner may hurt it (clanmates can't). At 0 it breaks: planks
  and parts fly (`siege.break` fx/sound), it drops a third of its materials.
- **Pack up**: the owner (or clanmate), not crewing, holds Interact 3 s on an engine at 90%+ health: the item comes back.
- **Saved** in the raid document (kind, place, yaw, health, owner); restored at load.
- **Crew**: E at a free post within 2.5 m takes it (the prompt names the post). On a post you're held to it (attached,
  movement off; every machine eases the body from where it stood onto the post over 0.35 s, never snapped); E again, a
  stagger, death, or the engine breaking lets you off onto clear ground beside the post. While crewing: Move / Look /
  Attack go to the engine (below); no abilities, no swings; a pad uses the same buttons (Move stick, RT/RB act).
- **First blow on a claim** from an engine rings its bell (the raid rule, `FWRRaiding::DamagePiece`).
- **Siege engineer** (Engineering 100): engines you built hit 20% harder (`siegeDamage` 0.2).

**Battering ram** (2.21.5: doors and gates only; 250 a hit every 3 s; pushed by 2 players or 1 hauler; 100 cm/s; roof 800).
- *Why*: the cheap answer to a gate or door you can't charge open; it must be walked up to the wall, so it's the exposed
  role. *Connects*: heartwood (Forest), steel (Hearth 4), a hauler tame. *Meets*: the ground on four solid wheels; the
  door or gate its head strikes; the crew at two rear handles under the roof's eave; a hauler in a harness behind.
- **Mechanism**: a frame of two long sole beams on two axles (solid three-plank wheels 90 cm across, iron tyres, the
  axles turning in oak bearing blocks bolted under the soles); four corner posts and two A-frames rise from the soles to a
  ridge beam 2.4 m up; the roof is rafters with wet hides lapped over them, down to 1.3 m at the eaves on both sides; from
  the ridge beam hang two iron chains to two eye bolts on a 4.5 m heartwood log (40 cm thick) with a steel cap on its
  front end; the log hangs at 1.0 m (door height). Behind: two push handles (crossbars across the rear posts at 1.0 m) and
  a harness beam with traces for a hauler.
- **Motion**: the wheels turn by distance travelled (angle = distance / radius), never slide. The log is a pendulum on
  its chains: Attack held draws it back (to -35 deg over 1.5 s, the chains swinging with it), release lets it swing
  forward through the bottom (where the cap strikes, at the strike angle 0 deg) and on to +12 deg, damped back to rest.
  A strike lands when the swing passes 0 with the cap within 60 cm of a door or gate. The swing angle replicates as a
  start time + kind (draw/release), so every machine draws the same arc.
- **Pushing**: crew on handles push with Move forward (back = pull back); turn with Move left/right at 25 deg/s. Speed
  100 cm/s x min(1, power / 2): a player is 1 power, a harnessed hauler 2. A harnessed hauler walks behind on its own
  legs (its movement steered to the harness point, so its walk plays), and drops out of the harness if it falls 4 m
  behind or is hurt badly (stagger). The ram follows the ground (each wheel traced; the body pitches to them).
- **Blow**: 250 (x siege engineer) as source Ram to a door / gate / doorway's door only (walls refuse: "A ram is for
  doors and gates", told once); the cap's thud, splinters of the door's material; no damage to people (it's a door
  breaker; people step aside).
- **Roof**: blows from above (fire pots, stones, arrows landing on it) hit the roof first (800); while the roof stands,
  the crew under it can't be hit from above by lobs (stopped by the roof's collision). Wet hides: fire x0.5.

**Trebuchet** (2.21.5: 15 s reload; 20 / 40 / 60 m; arc for everyone; 500 to a piece, 80 to anyone it lands on; 2 crew).
- *Why*: the far role: breaks stone and metal walls from outside a defender's bow range; slow and seen by everyone.
  *Connects*: siege stones (stone dressed at the workbench), rope, steel. *Meets*: the ground on a squared timber base
  frame with a turntable; the stone in its sling; the crew at the winch and the aiming lever.
- **Mechanism**: a base frame of four timbers on the ground; on it a turntable (a ring of oak with an iron track, the
  upper frame on rollers); the upper frame's two A-frame towers (4.2 m) carry an iron axle; the throwing arm (7.5 m,
  pivot at 1/5 of its length: 1.5 m short end, 6 m long end) on the axle; a hinged box counterweight (1.4 m cube of planks
  bound in iron, full of stones) hangs from a pin at the short end; the sling (2.4 m of rope to a leather pouch) hangs
  from the long end; one sling loop rides on the release pin at the tip. A winch drum on the frame between the towers'
  feet with two capstan wheels and a ratchet pawl hauls the long end down by a rope to a hook; a trigger pin holds it.
- **Motion** (all from replicated state + times, every machine the same): Winding: the crew holds Interact at the winch
  (10 s): the drum turns, the pawl clicks every 15 deg, the arm comes down from rest (+65 deg) to cocked (-35 deg), the
  counterweight rises. Loading: the aimer loads a siege stone (one from his bag) into the pouch: it sits in the trough.
  Firing: the aimer pulls the trigger (Attack): the arm swings up over 0.9 s (ease-in, a real accelerating fall of the
  weight), the sling whips round behind it and lets go at the pin's angle, the stone flies; the arm overshoots, rocks back
  and settles with the counterweight swinging in the frame (damped over 3 s). Turning: Move left/right turns the upper
  frame on its turntable at 10 deg/s (rollers turn). Range: Move forward/back sets the pin (20 / 40 / 60 m), shown on
  the pin's angle at the tip.
- **The arc**: the stone's path for the current yaw and range is drawn as a thin glowing line on **every** machine for
  the whole time it is loaded and cocked, and during the flight, with a ring where it lands: nobody is shelled by surprise.
- **Shot**: a 70 cm dressed stone (GAP: 2.21.5's "2 m" is the strike's footprint, a real trebuchet threw 50-90 cm
  shot), under gravity, never homing. Lands: 500 (x siege engineer) to the piece it hits through DamagePiece (source
  Siege, every material x1), x0.5 to other pieces within 2 m; 80 to every character in 2 m (ApplyHit, unblockable heavy
  impact, origin the landing; clanmates of the crew spared, the crew not), a dust burst and a crater mark. Aegis: the dome
  breaks and the stone stops (built in WRAbilitiesArcane).
- Siege stone: workbench L4, stone 25 → 1 (weight 20).
- Cost check: wood wall 400 → 1 stone; stone 1,000 → 2; metal 2,500 → 5; biome 3,000 → 6 (published on the item card).

**Ballista** (2.21.5: steel plate 8, wood 30; 2 s draw visible; 0.9 s aim line; straight; never homes; 220 creatures / 120 players; 800 HP).
- *Why*: the anti-tame and anti-big-creature gun of a siege (or a hunt); one crew. *Connects*: bolts (iron, wood),
  steel bow. *Meets*: the ground on a tripod; the gunner behind its stock.
- **Mechanism**: a tripod of three oak legs braced by a ring, an iron swivel pin at the top; on it a yoke (yaw) carries
  the stock on a trunnion (pitch -10 to +25 deg); a steel bow (two leaves, 1.8 m tip to tip) at the stock's front; the
  string (cord) to a claw on a slider; a windlass at the stock's back (a drum with two handles, a ratchet pawl).
- **Motion**: aim with Look (the stock follows your view at 60 deg/s yaw, 30 deg/s pitch, smoothly). Attack held draws:
  the windlass turns, the slider and string come back, the bow's tips bend (2 s); drawn, release Attack: the 0.9 s red
  aim line shows along the stock (everyone sees it), then the claw lets go: the string snaps forward, the bow springs
  back, the bolt flies at 6,000 cm/s with no drop, never homing. A bolt from the bag (`ballista_bolt`) is laid in the
  groove at the draw's start; none: "No bolts".
- **Hit**: 220 to creatures, 120 to players (ApplyHit, pierce impact 3), pieces x0.1 (spell-like: it's not a wall
  breaker), Aegis stops it (projectile). Bolt: workbench L4: wood 4, iron ingot 1, fiber 2 → 2.

### 3.7 Path raiding for every path (L27)
Each area blast of the later paths calls `FWRRaiding::SplashPieces` once per cast with its centre, radius and damage
(source Spell, the caster's path): Arcane blast, Starfall, Thunderclap, Blood boil, Crimson tide, Corpse explosion,
Collapse, Maw of the void, Solar flare, Sunburst, Maelstrom, High tide, Plunge, Detonate. The path multiplier table
(tuning raiding.paths) then gives lightning x1.5 on metal, spore x1.5 on wood, void x1.25 on all, arcane x1.25 on stone.

### 3.8 Sound and fx rows
`siege.wheel` (wood creak loop step), `siege.ram_draw` (chain rattle), `siege.ram_hit` (heavy wood thud), `siege.winch`
(ratchet click), `siege.release` (arm whoosh + frame knock), `siege.stone_land` (rock impact + debris), `siege.windlass`
(ratchet), `siege.ballista` (string thrum), `siege.break` (timber crash), `siege.place` (wood set down). Recordings: the
existing real wood / stone / chain / crossbow recordings in Data/Audio (no synth: Aaron 11-15 "only real instrument
sounds" applies to music; effects reuse real recordings). Fx: `siege.dust` (ground dust), `siege.splinters`,
`siege.stone_burst`, `siege.break`.

### 3.9 Edge cases
| Case | Handling |
|---|---|
| Crew member dies / logs out / staggered | Off the post (detached, movement back); the engine stays |
| Engine breaks with crew on it | Everyone off onto clear ground |
| Two players want one post | The server gives it to the first; the prompt shows the next free post |
| Ram pushed into a wall / player | It stops (a sweep of its box ahead); never passes through |
| Ram on a slope over 20 deg | Won't climb it ("Too steep") |
| Trebuchet fired with no stone | Refused: "Load a siege stone" |
| Trebuchet fired uncocked | Refused: "Wind it down first" |
| PvE world | Ram and trebuchet refuse to place; ballista allowed |
| Safe zone / cave | Refused to place |
| Engine owner offline | Stays; anyone hostile may break it |
| Hauler harnessed and its owner leaves the ram | The harness holds until someone pushes; it follows the ram |
| Server restart | Engines saved and restored (raid doc), crew cleared |

## 4. Build steps
1. Part A, build mode: pad mapping in WRCharacter/WRPadInput (verbs BuildPrev/BuildNext/BuildCategory/BuildCopy, Data/gamepad.json), copy (controller + key binding BuildCopy, middle mouse), cycle pieces/categories (WRUIBuild helpers), Survival tab + starter flag + last tab, short chip, nearby storage (WRBuildStock in WRPlayerBuild: client count + server pay), keys panel. Compile, commit.
2. Part B, path raiding: SplashPieces in the 14 area casts. Compile, commit.
3. Part C, the first night and the body: lean-to piece + decor archetype + roof rule; hot spring rule; climb + crew poses in WRHeroAnimInstance + WRCharacter. Compile, commit.
4. Part D, siege: WRSiege.h/.cpp (AWRSiegeEngine base + ram / trebuchet / ballista), WRSiegeLook.cpp (code-modelled parts), items/recipes/tuning/sounds/fx, use/crew/prompts/commands, damage hooks (melee, projectile, blast), stone + bolt projectiles, arc, save/load, perk. Compile, commit.
5. Spot-check against this doc; CHANGELOG + HANDOFF each commit; LANDED.

## 5. Build notes (what changed from the plan while building, and why)
- **Copy and beams**: a copied beam keeps its yaw; its slope follows your look as every beam's does (the beam rule), so
  the plan's "at its pitch" isn't carried.
- **Survival tab**: there was no lean-to; it was built (3.3). The tab holds workbench, storage chest, lean-to, bedroll,
  torch, campfire, claim totem.
- **Path raiding**: Blood boil (bursts inside wounds), Crimson tide and the Tide fields (water that heals / pulls) and
  Solar flare (a flash of light) stay off walls by design; Detonate's spores pop on bodies only.
- **Ram geometry**: the log hangs on two *parallel* chains (a parallelogram), so it swings forward and back staying
  level, as real two-chain rams did; it rides at 1.08 m so it clears the frame's cross ties, and its collision front is
  where the cap is at the bottom of the swing, so a cap meets a door and bounces back off it (never into it).
- **Trebuchet**: the cocked angle is -33 deg (the tip rests just over the trough's top); the sling lies along the trough
  when cocked. Stone shot is 70 cm (the "2 m" of 2.21.5 is its footprint). The stone spares the crew's clan (crew included).
- **Ballista**: players take the PvP share of the 220 (combat 2.20 x0.6, ~130) rather than a second number.
- **Packing up** needs Dodge twice within 2 s (a reflex dodge mid-raid never packs a ram).
- **Engines on PvE worlds**: ram and trebuchet refuse to stand; the ballista stands and can't be hurt by players there.
- **Models** are code-built (WRSiegeLook.cpp), not the Fab pack biome-crystal named: every moving part is its own mesh
  round its own pivot, which a refitted pack mesh wouldn't give.

## 6. Spot-check (read against this plan and THE BAR)
| Found | Fixed |
|---|---|
| Ram cross ties ran through the log; chain length didn't meet the eye bolts | log raised to 1.08 m, chain 1.0 m to the eye at +38 cm (WRSiegeLook RamChainLength / RamEyeZ) |
| Trebuchet pawl post stood in the ratchet wheel; stays hung in the air off the frame; rollers sank into the track | pawl moved in front of the wheel, stays removed (the A-frame and brace carry it), upper sills raised onto the rollers |
| After a throw the arm snapped to cocked then eased up | winding resets to 0 on the throw's end (ClientStep) |
| A new engine would fall apart at once on a long-running server (abandon clock from 0) | clock starts at BeginPlay |
| The owning client kept predicting walking while held on a post | `TickLocalCrew` turns movement off / back on locally |
| A stone hitting an engine square counted twice (splash + direct) | the direct share is the remainder |
| Ram swing left player names in the actor's Tags | removed |
| The ballista gunner stayed put while the stock swung round | the gunner's post turns with the stock, every frame on every machine |

## 7. Proof (requirement -> where)
| # | Done | Where |
|---|---|---|
| L1 | Yes | section 1 (Valheim Ashlands ram/catapult, Conan trebuchet, V Rising golem, Rust raid costs, Bannerlord crews, Enshrouded, Valheim craft-from-containers; rams, trebuchets, ballistae, ladder technique) |
| L2 | Yes | sections 3, 4 (written and pushed before building: showcase-notes 90a5990) |
| L3 | Yes | section 2 |
| L4 | Yes | commits d98a437, ebbb16d, 6597147 |
| L5 | Yes | section 6 |
| L6 | Yes | "built" rows in section 2 carry file:line |
| L7 | Yes | 3.3 / 3.6 (why, connects, meets per object); comments at each model (WRSiegeLook.cpp:87, 242, 430; WRBuildLookDecor.cpp:985) |
| L8 | Yes | WRSiegeLook.cpp (axles in bearing blocks, chains to eye bolts, ratchet + pawl, sling on its pin, windlass); 6 fixes in section 6 |
| L9 | Yes | WRSiege.cpp:937 ClientStep (pendulum, falling arm, sling, windlass from Rep + server time; eased location; wheels by distance) |
| L10 | Yes | this table; final message |
| L11 | Yes (none found) | grep of WRUI*.cpp / WRPlayerUI.cpp: no staff, notes, beat UI; captions only for world sounds (WRSound.cpp:851) |
| L12 | Yes | tuning `siege`, `anim.climb`, `anim.crew`, `building.pad*`, `storageReachCm`, `hotSpringKinds`, `raiding.material.ram/siege`; items/recipes rows |
| L13 | Yes | WRSiege.cpp:1347 StoneLanded (ApplyHit); ballista bolt through AWRProjectile |
| L14 | Yes | pad build mode WRPadInput.cpp:574, WRCharacter.cpp:1343; crew on the same buttons; gamepad.json page rows |
| L15 | Yes | sounds.json `siege.*` (14), fx.json `siege.*` (5) |
| L16 | Yes | server decides (WRSiege.cpp:326 ServerTake, 590/742/810 steps); Rep replicated; edge cases 3.9 |
| L17 | Yes | nothing shown that doesn't work; PvE refusals say why |
| L18 | Yes | editor build succeeded after each part and after the final pull; namespaces WRSiegeDetail / WRSiegeLookDetail / WRBuildStockDetail / WRBuildSetsHot |
| L19 | Yes | three commits, CHANGELOG + HANDOFF in each, added by name |
| L20 | Yes | no new binaries; LANDED + LATER-COOKS lines |
| L21 | Yes | WRSiege.cpp:590 StepRam, WRSiegeLook.cpp:87 RamFrame |
| L22 | Yes | WRSiege.cpp:742 StepTrebuchet, :1133 DrawArc, WRSiegeLook.cpp:242 |
| L23 | Yes | WRSiege.cpp:810 StepBallista, :1166 DrawAimLine, WRSiegeLook.cpp:430 |
| L24 | Yes | WRSiege.cpp:506 ServerHarness, :1382 TickHauler; WRCreature.cpp hauler hook |
| L25 | Yes | AWRProjectile kinds `trebuchet` / `ballista` meet WRAbilitiesArcane.cpp:946 |
| L26 | Yes | Data/lifeskills.json:87 (stat siegeDamage), WRSiege.cpp UseItem BuiltMult |
| L27 | Yes | WRAbilitiesArcane.cpp:480, 642; WRAbilitiesLightning.cpp:306; WRAbilitiesNecro.cpp:318; WRAbilitiesVoid.cpp:301, 374; WRAbilitiesSun.cpp:748; WRAbilitiesSpore.cpp:225 |
| L28 | Yes | WRPlayerBuild.cpp:305-345, WRPadInput.cpp:574, WRCharacter.cpp:1343, Data/gamepad.json verbs |
| L29 | Yes | WRPlayerBuild.cpp:347 CopyAimedPiece, WRKeyBindings.cpp:95, WRPlayerController.cpp:180 |
| L30 | Yes | WRUIBuild.cpp:1628 WRBuildStock::Pay, WRPlayerBuild.cpp:971 |
| L31 | Yes | WRUIBuild.cpp:97 (tab), :1545 OpeningTab; pieces.json "starter" |
| L32 | Yes | WRUIBuild.cpp:176 ShortWhy |
| L33 | Yes | Data/pieces.json lean_to, WRBuildLookDecor.cpp:985, WRBuilding.cpp:616, WRSurvival.cpp:177 |
| L34 | Yes | WRBuildSets.cpp:271 InHotSpring |
| L35 | Yes | WRHeroAnimInstance.cpp:517, WRCharacter.cpp:1016 |
| L36 | Yes | WRHeroAnimInstance.cpp:546, WRCharacter.cpp:1025 |

## 8. Not finished
- Siege engines are compiled, not played (Aaron tests); their sounds are real recordings re-pitched from the library
  (no dedicated siege foley).
- The crews' and climber's poses are procedural (no clips in the hero set).
- Tames don't bite siege engines (the "Break that" order targets buildings only).
- Building from storage counts chests; crafting from chests is still the crafting cook's.
