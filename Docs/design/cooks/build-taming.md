# Wildreach cook: BUILD-TAMING (FULL-GAME 6.5)

Lane build-taming, 2026-11-14. Scope (brief): creature levels and skill trees, craftable saddles and creature armour,
flying / swimming / pack mounts (the Floeback swimming mount), breeding with stat inheritance, eggs and pregnancy,
raising babies, mutations; tames keep their element. Numbers follow `cooks/survival-crafting.md` 2.19-2.21 (SPEC +
numbers already cooked there); this doc fills what that cook left open and plans the build in Raven's repo.

## 0. Requirement ledger

| # | Requirement (one each) | Source | Done where (filled at the end) |
|---|---|---|---|
| R1 | Creature levels: every wild creature has a level 1-30 by its band (lowland 1-10, midland 11-20, highland 21-30), each level a random stat point, at most ±10% around its tier numbers | 6.5 RAVEN-PLANNED; SC 2.19 || DONE: WRTaming.cpp:462 RollWildGenes, :491 RecomputeStats (clamp), WRCreature.cpp BeginPlay; label Lv in GetDisplayLabel |
| R2 | Bond levels 1-20 after taming from the tame's own use (fight, ride, haul, utility, farm), cost 100 + 40n, +2% every stat and 1 skill point each | SC 2.19 || DONE: WRTaming.cpp:577 GainBondXP; XP sources WRTaming.cpp:815 (ride/haul/walk/light), :1114 AfterHit (fight/kill), WRCreature.cpp:2097 (dig), WRFarm.cpp:1518 (farm) |
| R3 | Role skill trees: one 20-point tree per role (fighter, mount, hauler, utility, farm): 4 nodes in a line (2/3/3/4) then one of two capstones (8); every node does what it says | 6.5 GAP; SC 2.19 || DONE: Data/tame_trees.json; WRTaming.cpp:1327 CardCommand learn; effects WRTaming.cpp:491/1055/1078/1155/1167/1183, WRCombatLibrary.cpp:856/899/1010, WRFarm.cpp:1508-1550, WRSurvival.cpp:574, WRCreature.cpp:2070-2110 |
| R4 | Craftable saddles: hide, bronze-fitted, iron, steel riding saddles and a pack saddle; one saddle per body rig fitted per species by offsets; bareback: no attacks, +25% damage taken | 6.5; SC 2.20 || DONE: items.json saddle_*; recipes.json; WRTameLook.cpp:233/295 RidingSaddle/PackSaddle; seat fit creatures.json seat/back; bareback WRCreatureCombat.cpp:470/1140, WRAbilities.cpp:119 |
| R5 | Creature armour (barding): iron 20%, steel 30%, biome barding = steel + that land's hazard pass for the mount; −5% speed | 6.5; SC 2.20 || DONE: items.json barding_*; WRTameLook.cpp:397 BardingBody + neck/head plates; speed WRTaming.cpp:665; lava/ice pass WRTaming.cpp:491 (Resist tags). 10 lands hidden (biome) |
| R6 | Saddle master perk: saddles +25% value; Creature armorer perk: barding +30% (both live) | LS; SC 2.20 || DONE: lifeskills.json saddle_master saddleValue, creature_armorer bardingValue; WRTaming.cpp:369 TackValue |
| R7 | Ground mounts get a real sprint with mount stamina (200), shown on the HUD | SC 2.20 || DONE: WRCreature.cpp RideSprintPressed; WRTaming.cpp:653 MountStaminaRates; WRStatusComponent.cpp hook; HUD vitals show the mount (pawn) stamina |
| R8 | Flying mounts: take off, climb / level / glide stamina rules, ceiling 400 m, forced landing near boss arenas, landing | 6.5; SC 2.20 || DONE (code; Windmane hidden until Sky islands): WRTaming.cpp:712 TickMount, ServerFly, WRCreature.cpp RideJumpPressed; HUD wings bar WRUIHud.cpp TameCard |
| R9 | Swimming mounts: the Floeback (tundra shore, offer fish, easy, passive): surface swimmer 900 cm/s, hauls out onto ice; carries the rider through icy water | 6.5; FULL-GAME 3.2 tundra; DR #14 || DONE: creatures.json floeback mountKind swim; WRTaming.cpp:665 ApplyRideSpeeds; WRCreature.cpp ApplyDef swimmer + flee to sea |
| R10 | The Floeback exists as a real creature: body, motion, voice, spawns, taming, loot | 6.5 || DONE: WRSealBody.cpp:93 Build, :307 SpineAt, :369 TickComponent; spawn_regions.json + WRWildlife.cpp:119 shore cells; sounds.json voice |
| R11 | Pack mounts: haulers (Basaltback, Woolly drudge) carry cargo (packs by carry weight, pack saddle +10 slots) and can be ridden at a walk | 6.5; SC 2.20 || DONE: creatures.json carry; WRTaming.cpp CargoSlots/AddCargo/CardCommand put/take; WRUITameCard.cpp packs grid; overload slow WRTaming.cpp:665 |
| R12 | Breeding: sex on every tame; two of one species, opposite sexes, mating on, within 10 m, fed: 2 min mating bar; female cooldown 4 h | 6.5; SC 2.21 || DONE: WRTaming.cpp:1658 TickPairs; WRCreature.cpp TickTame mate walk; card mate toggle |
| R13 | Eggs or pregnancy by species (`birth: egg | live`); pregnancy easy 30 min / medium 2 h / hard 6 h then live birth | SC 2.21 || DONE: creatures.json birth; WRTaming.cpp:1658 (eggs via AWRLaidEgg::Lay :1808 / pregnancy) and :815 birth |
| R14 | Incubator (workbench L3) with element fuel: fire eggs charcoal or sulfur, frost eggs ice per hour, else wood; no fuel pauses; stolen eggs use it too | SC 2.21 || DONE: pieces.json incubator; WRBuildLookDecor.cpp Incubator(); WRTaming.cpp:1732 TickIncubators, IncubatorCommand; WRUIIncubator.cpp; WRTameLook.cpp:583 egg + warmth |
| R15 | Stat inheritance: each of 6 stats from one parent, 55% the higher, 45% the lower | SC 2.21 || DONE: WRTaming.cpp:178 FWRTameGenes::Child |
| R16 | Mutations: 3 rolls at 2.5% (Breeder ×1.5), +2 points to a random stat and a visible colour change; 20-per-side cap; Rare finder makes wild mutants 2× as common | SC 2.21; LS || DONE: WRTaming.cpp:165 Mutate, :178 rolls/cap, :462 wild mutants + Rare finder; M_TameMorph + WRTameLook.cpp:569 overlay |
| R17 | Raising babies: 10% to full size over easy 2 h / medium 8 h / hard 24 h (server maturation scales); babies eat from a trough, stay near it, can die | SC 2.21 || DONE: WRTaming.cpp:815 growth/scale/stats, trough feeding; WRTameLook.cpp RefreshTameLook scale on every machine |
| R18 | Imprinting: a care prompt every 1/8 of growth (feed a named food, walk it 50 m, cuddle); +12.5% each; +20% all stats when the imprinter rides or fights beside it; bonded to that player | SC 2.21 || DONE: WRTaming.cpp:815 care prompts, :1005 AnswerCare, StartCuddle, ImprintScale; WRPerkHud.cpp bubble; card care button |
| R19 | Tames keep their own element (attacks, resists, hazards, eggs, babies) | 6.5 RAVEN-PLANNED rule || DONE: WRTaming.cpp:491 Weapon.Element; WRCreatureCombat.cpp:723 RowHit and mount bite; resists kept (BeginPlay tags) |
| R20 | Tames save and load everything above (levels, tree, gear, cargo, genes, growth, imprint, pregnancy, eggs in incubators) | brief "every path" || DONE: WRTaming.cpp:1246/1281 SaveTame/LoadTame via WRGameMode.cpp:287/419; laid eggs WRGameMode.cpp; incubators WRBuilding.cpp:1278/1349; tack sockets WRInventory.cpp:1264 |
| R21 | Multiplayer: all server-side, replicated to every machine; clients send intent only | brief || DONE: all state server-side; FWRTameRep/Cargo replicated (WRCreature.cpp GetLifetimeReplicatedProps); commands carry positions (WRPlayerMenus.cpp tamecard/incubator) |
| R22 | Screens with mouse and pad, no dead states: a tame card (stats, tree, gear, care, breeding, cargo) and an incubator screen | brief, BAR || DONE: WRUITameCard.cpp:454, WRUIIncubator.cpp:252 (buttons/slots driven by the pad processor); wheel Tend WRPlayerUI.cpp:965/1040; prompts WRUIHud.cpp |
| R23 | Sounds and fx as table rows; numbers in Data/*.json | brief || DONE: tuning.json tame, tame_trees.json; fx.json tame.levelup/heart/hatch; Audio/sounds.json tame.*, incubator.fuel, mount.* |
| R24 | Tame hunger (needed by babies, breeding "fed", Content node): eats its food from its packs or a trough within 20 m | SC 2.19 || DONE: WRTaming.cpp:815 hunger, starving via ApplyHit; card feed |
| R25 | Unfinished parts stay hidden (other lands' mounts and barding wait for their lands) | brief || DONE: ten lands barding + Windmane carry biome/never spawn; LATER-COOKS lines |
| R26 | Design with intention: every new object's job, connections and contacts written down (saddle on the spine, barding plates on their bones, incubator firebox, Floeback ice crust) | Aaron 11-14 || DONE: design notes in WRTameLook.cpp header, RidingSaddle/PackSaddle comments, WRBuildLookDecor Incubator(), WRSealBody.h |

## 1. Research

| Game / reference | The specific system | What makes it work | What we take |
|---|---|---|---|
| **Ark: Survival Evolved / Ascended** | Wild levels as random stat points; per-stat inheritance 55/45 toward the higher parent; 3 mutation rolls at 2.5% each, +2 levels in one stat and a colour-region change; 20 per side mutation counter; gestation vs eggs and incubation temperature; imprinting care requests (cuddle, walk, feed a named food) every slice of growth, imprint bonus to stats only for the imprinter; per-species saddles with armour value [guildorder.com Ark breeding guide; xgamingserver ASA breeding & imprinting guide] | Breeding is a long, readable puzzle: you can always see which parent gave what, and every mutation is visible on the body | All of SC 2.21's numbers; stat points shown per stat with the parent each came from; mutations visible as a body pattern; care prompts as a bubble over the baby |
| **Palworld** | Work suitability levels and passives; eggs to an incubator, mutated eggs ~1% [palpedia.com breeding guide; guildorder Palworld breeding optimization] | A creature's job is its identity: it gets better at the thing you use it for | Role trees: a tame grows in its own job; bond XP only from that job |
| **Red Dead Redemption 2** | Horse bonding 1-4 from riding, brushing, feeding and calming; bond raises health/stamina cores; saddles carry stamina/speed stats | Bond comes from use, never from a menu; tack is a visible stat choice | Bond XP from use; saddles with a stat and a look per tier |
| **Zelda: Breath of the Wild** | Horse soothing, stables, gear swaps at the stable | Physical, short care moments | "Cuddle" is a 2 s hold on the baby with a real nuzzle and sound |
| **Valheim** | Lox saddle, wolves and boars breed when fed ("love" hearts), offspring inherit star levels, grow up over time | Feeding is the breeding trigger: no menus | Breeding needs both fed; a heart burst and call while the bar fills |
| **Horizon Forbidden West** (Sunwing) | Hold jump to climb, glide when not flapping, dive to land, can't fly into certain zones | Flight reads instantly; limits are geographic, not arbitrary | Space/A climbs (stamina), release glides, Crouch/B dives; forced landing near boss arenas |
| **Ark** (Ichthyosaurus, Basilosaurus) | Surface and underwater swimming mounts, insulation vs cold water | A sea mount turns the sea from a wall into a road | The Floeback is the tundra's road over the Rime Sound; riding it the cold water doesn't touch you |
| **Medieval horse tack** | Saddle tree, pommel, cantle, seat, skirt, girth, stirrup leathers, stirrups, breast collar; sawbuck pack saddle with side bars, panniers and lash ropes; barding: chanfron (face), crinet (neck lames), peytral (chest), flanchards (flanks), crupper (rump) | Every part has a job: the girth holds the tree down, the breast collar stops it sliding back, panniers hang off the bucks | Saddles and barding are built from those parts, sized to the creature's back and fixed to its spine bone |
| **Phocid seals** (harp, Weddell) | Swim by side-to-side sweeps of the hind flippers, fore flippers steer; on land they "gallumph" on the belly in vertical waves; harp seals' dark saddle mark; Weddells rasp breathing holes in sea ice with their teeth; countershading | Motion tells you where it lives | The Floeback swims with a lateral body wave driven from the hips, gallumphs on land, carries a crust of sea ice on its back (the floe) where the rider sits |
| **Real genetics / morphs** | Leucism (pale), melanism (dark), aberrant banding, bioluminescent lines | One readable change per mutation | Four morph kinds: pale, dark, bands, glow line, drawn as an overlay on any creature's own texture |
| **Incubation** | Birds ~37.5 °C; reptile eggs warm sand; ice-adapted eggs kept cold | The fuel tells the egg's nature | Fire eggs on charcoal or sulfur, frost eggs on ice, others on wood |

Lessons: (1) everything the player does with a tame feeds it forward (bond from use); (2) every genetic number is visible
and traceable; (3) gear is a visible object on the body, fitted, with its own parts; (4) mounts change the map (sea, air).

## 2. Design (gaps solved)

### 2.1 Levels and stats (R1, R2)
- Six stats: **health, stamina, damage, carry, speed, torpor**. Genes = points per stat (6 ints) + mutations per side.
- Wild level L (band) = L points, each to a random stat. Stat multiplier = 1 + `taming.statPerPoint` (0.02) × (points − expected), expected = band-middle / 6 (lowland 0.92, midland 2.58, highland 4.25), clamped to ±10% for wild-born. Bred stock is not clamped (mutations push past wild).
- Level shown = wild level + bond level ("Lv 24").
- Bond XP: fighting 1 per 10 damage dealt; a kill it hit in the last 10 s gives its kill XP (lowland 20, midland 40, highland 80, boss 300); riding 1 per 50 m; hauling 1 per 100 m carrying ≥ 25% capacity; utility 3 per ore dug, 1 per 5 min lighting the dark; farm 2 per product. Cost 100 + 40n. Each level +2% every stat, +1 skill point; owner's Taming skill +20 XP a level-up. Level-up: a gold ring burst and its own call, a toast "Cinderfang reached bond 7 · 1 skill point".

### 2.2 Role trees (R3) (`Data/tame_trees.json`)
Exactly SC 2.19's table. Role picks the tree (first word of `role`; "farm animal" → farm). Nodes in a line need the one before; a capstone needs all four (12 points) and costs 8; the other capstone locks. Respec: none (a bond decision). Behaviour:
- Fighter: Hardened (+15% HP), Bite force (+15% dmg), Pack tactics (+10% per other ally who hit its target in the last 3 s, max 3), Guardian (within 4 m of its owner it takes 30% of a blow meant for them through ApplyHit), Berserk (< 30% HP: +40% dmg), Stalwart (stance damage taken −40%, never staggered).
- Mount: Sure-footed (sprint stamina −15%), Stride (+8% speed), Charge (sprinting into an enemy: 0.4 s lowered-head tell (wind-up glow + call) then 40 damage + knockback through ApplyHit, 4 s cooldown), Steady seat (rider isn't unhorsed by blows under 80 stance), Warhorse (rider's mounted attacks +20%), Courser (+15% speed, +50% stamina regen).
- Hauler: Broad back (+25% carry), Packing (+8 slots), Steady (no slowdown loaded), Forager (anything that doesn't fit in the owner's bag goes into its packs if it's within 8 m), Caravan (+50% carry), Bulwark (+200 HP; allies within 6 m take 10% less).
- Utility: Keen sense (sense range +50%), Tireless (work interval −25%), Bright (light radius +50% / dig yield +25%), Lucky find (+15% gem chance on its digs), Seeker (what it digs or senses is pinged to your clan for 5 min: a toast to clan members with the place), Lodestar (players within 10 m gather +15%).
- Farm: Content (eats 25% less: pen feed skipped 1 in 4), Bountiful (+20% product), Fertile (breeds ×1.5 faster; mating cooldown ÷1.5), Calm herd (pen-mates +10% product), Prize stock (5% rare product), Prolific (25% twins).

### 2.3 Saddles and barding (R4, R5, R6)
Items with `tack` data (items.json): `{ "kind": "saddle|pack|barding", "reduction": 0.1, "slots": 10, "speed": -0.05, "hazard": "lava" }`.
| Item | Station | Inputs | Value |
|---|---|---|---|
| Hide saddle | workbench L2 | hide 10, fiber 10, wood 4 | 10%, mounted attacks |
| Bronze-fitted saddle | workbench L2 | hide 8, bronze ingot 4, linen 4 | 20% |
| Iron saddle | workbench L3 | hide 8, iron ingot 6, linen 6 | 30% |
| Steel saddle | workbench L4 | leather 8, steel plate 4, linen 6 | 40% |
| Pack saddle | workbench L2 | hide 10, wood 10, rope 4 | 10%, +10 slots, haulers only |
| Iron barding | armorer L3 | iron ingot 14, hide 8, linen 4 | 20%, −5% speed |
| Steel barding | armorer L4 | steel plate 10, leather 6, linen 4 | 30%, −5% speed |
| Forgeplate barding | armorer L5 | steel plate 10, forgeplate's biome block 10, ember essence 4 | 30% + lava pass |
| Rimeguard barding | armorer L5 | steel plate 10, rimeguard's biome block 10, frost essence 4 | 30% + slick-ice grip |
| (10 other lands' barding) | hidden until their lands | | |
Fit (`creatures.json` `seat`: [forward, up] in body units, default [0, 0.95]; `back`: half-width in body units, default 0.55). Attached to `spine2` (AI rigs; a bone found by hint) so it rides the spine's motion. Rider sits on the saddle's seat. Bareback: no Light/Heavy from the saddle (a notice "No saddle: you can't fight from its back"), mount takes ×1.25. Saddle master ×1.25 reduction, Creature armorer ×1.3 barding.
**Look (built in code with the held-tool kit, M_HeldTool):** riding saddle = a padded seat between a raised pommel and a taller cantle on a tree, two skirt flaps down the flanks, a girth band under the belly (closes the loop round the body at the back's width), stirrup leathers to stirrups, a breast collar strap forward; bronze / iron / steel tiers add metal-bound pommel and cantle plates, rivets and rings in that metal. Pack saddle = two crossed sawbuck frames on side bars, two panniers (laced hide boxes) hanging off them, a lashed bedroll on top, a breeching strap back. Barding = a peytral plate over the chest (spine3), three crinet lames along the neck (neck), a chanfron on the face (head), flanchard plates both flanks (spine1/spine2) in the barding's metal; biome barding adds its set colour band (Forgeplate glowing seams, Rimeguard frosted rim).

### 2.4 Mounts (R7-R11)
`creatures.json` `mount`: true/"ground" | "swim" | "fly" | "pack". Ground: ride sprint (Sprint key / pad's sprint) ×1.35 speed for 20 stamina/s from the mount's pool (200 × stamina stat), regen 25/s after 1 s; out of stamina it can't sprint. Pack: walk-only riding at its rideSpeed, cargo packs.
**Fly** (Windmane; hidden with the Sky islands): Jump on the ground takes off (MOVE_Flying); held Jump climbs (15 stamina/s), nothing levels (4/s), Crouch/Dodge dives (0/s, ×1.3 speed); landed regen 30/s; empty: it glides down; ceiling 400 m above the ground under it; within 150 m of a boss arena it lands and won't take off. Its body waits for its land (no Windmane art yet: LATER-COOKS).
**Swim** (Floeback): in water it swims at the surface (900 cm/s × speed stat; the rider is above water and isn't swimming), it may not dive; on land/ice it gallumphs at 250 cm/s. Riding it, the rider is never "in the water" (no drowning, no icy-water build-up).
**Pack** (Basaltback 300 kg, Woolly drudge 280 kg): carry kg × carry stat; packs = carry / 20 slots (+10 pack saddle, +8 Packing); weight beyond 75% slows it 30% (Steady: none). Cargo moves in the tame card's Cargo tab.

### 2.5 The Floeback (R9, R10)
**Why it's here:** the tundra's coast is broken by the Rime Sound; the Floeback is the tundra's road over cold water (Raven: "carries a rider through icy water"). It hauls out on the shore ice of the tundra coast and fishes the shallows: it lives on the boundary between land and sea, so that's where you meet it. Passive: it flees into the water. Tamed by offering fish (any raw fish from fishing), easy (1 min).
**Body:** 3.2 m long, harp-seal build: blunt round head with large black eyes, a short whiskered muzzle (16 whiskers each side), no ear flaps, a thick neck flowing into a barrel body that tapers to the hips; fore flippers with five dark claws, hind flippers fanned and webbed. Coat: silver-grey with darker grey dappling, a dark harp-shaped "saddle" mark over the shoulders, a pale belly (countershading). **The floe:** a crust of sea ice grown over its back from the shoulders to the hips, 7 thick faceted slabs, pale blue-white and glassy, rimed at the edges; the rider sits on the crust (a saddle straps over it). Built at runtime from lofted rings (smooth shaded) in M_HeldTool's vertex-colour surface: fur grain runs along the body.
**Motion (deformed per frame):** swimming = a lateral wave from the chest to the hind flippers (amplitude rising toward the tail, 1.4 Hz at full speed), fore flippers tucked, head steady; idle in water = slow sculling, head up and turning; on land = gallumph (a vertical wave down the body, chest lifting, fore flippers planting) at 1.6 Hz scaled by speed; idle on land = breathing, head lifts and looks; flinch = a body jerk; death = rolls onto its side, flippers limp.
**Voice:** reused CC0 recordings (low barks / trills pitched) as `voice.floeback.*` rows. **Loot:** raw meat 4, hide 3, fat? (no new item: "tallow" exists? if not, bone 1). **Spawns:** a tundra region along the coast (lowland band, tundra side) where the walk map is shore.

### 2.6 Breeding, eggs, babies (R12-R18)
- Sex 50/50 at tame or birth (♂/♀ on the card). Mating toggle on the card (both must have it on). Server checks pairs every 5 s: same species, opposite sex, both adults, mating on, both hunger > 0, within 10 m, female not cooling down or pregnant → the bar fills over 120 s while they stay within 10 m (they walk to each other and stand nuzzling; a heart fx every 4 s and their calls). Done: egg layer drops a **fertilised egg** at her feet (an item pickup bag at her feet holding `fertile_<species>`), a mammal becomes **pregnant** (easy 30 min, medium 2 h, hard 6 h × `server.maturation`) and gives birth beside herself.
- Genes in the egg item (stack 1, encoded in the stack's Sockets list so it survives bags, chests, saves).
- **Incubator** (workbench L3; stone 20, iron ingot 4, linen 4): a clay-and-stone firebox under a straw-lined tray in a brass-banded hood. E opens its screen: put an egg in (from your bag), fuel slot (charcoal/sulfur for fire and sun eggs; ice blocks for frost and wind; wood otherwise; tide/blood: water). It burns 1 fuel per in-game hour of incubation; out of fuel the egg pauses (never dies). Done: the baby hatches beside it, tamed to the incubator's owner... no: to whoever put the egg in (the imprinter is whoever answers the care prompts). Stolen wild eggs (`egg_kilnmite`) go in too (no genes: wild genes rolled at hatch); right-click hatching from the bag stays (a stolen egg in hand still hatches: Raven's built path).
- **Baby**: scale 0.1 → 1.0 over easy 2 h / medium 8 h / hard 24 h × `server.maturation` (playtest preset 10× faster). Can't be ridden, mate or wear tack until adult (≥ 100%). Follows / stays like any tame; hungry babies walk to the nearest trough within 20 m.
- **Care prompts**: at each 1/8 of growth a prompt appears (bubble icon above it, on the card, and a toast to the owner): feed (one of its foods, given with E from your bag), walk (follow you 50 m), cuddle (hold E 2 s). Answered: +12.5% imprint, Taming +10 XP, a heart fx + happy call. Missed (the next prompt replaces it): lost. Imprinter = the first player to answer; only they can answer later prompts.
- **Imprint bonus**: + imprint% × 20% to every stat while its imprinter rides it or is within 15 m.
- **Stats**: each stat from one parent: 55% the parent with more points in it. **Mutations**: 3 rolls at 2.5% (× 1.5 Breeder of the egg's/birth's owner), each: +2 points to a random stat and a morph (pale, dark, bands, glow line) in a random tint; counter goes up on the side it came from; a side at 20+ can't mutate. **Wild mutants**: 1% of wild spawns carry one morph and +2 points (2% if any player within 100 m has Rare finder).
- Twins: Prolific 25%.

### 2.7 Element (R19)
A tame keeps `Def->Element` and `Resists` (its bites carry its element's build-up; volcanic mounts cross lava; tundra mounts keep their grip on ice). Babies and eggs carry their species' element; the incubator's fuel follows it; the card shows it.

### 2.8 Screens (R22)
- **Tame card** (`EWRMenu::TameCard`, screen `tamecard`): opened from the tame wheel's spare wedge "Tend" (and pad the same). Stone panel, tabs **Overview · Skills · Gear · Family · Cargo** (LB/RB switch tabs; buttons all reachable with the stick cursor and D-pad). Overview: name, sex, level, element, bond bar, the six stats (points, multiplier, mutations), hunger. Skills: the tree (4 + 2 capstone cards), points left, Learn buttons. Gear: saddle slot and barding slot (fit from bag / take off). Family: mating on/off, cooldown or pregnancy timer, growth and imprint bars, the current care prompt with its "Answer" button, parents' mutation counters. Cargo: packs and your bag, click to move.
- **Incubator** (`EWRMenu::Incubator`, screen `incubator`): the egg (species, sex, element, time left), fuel and burn time, eggs in your bag to put in, fuel in your bag to add, Take out.

### 2.9 Edge cases
Tame limit counts babies; a full limit blocks hatching/birth (the egg waits / the birth waits until room). Owner offline: the world ticks only near players (unloaded tames don't age), incubators tick while loaded. Rider disconnect: dismount. A dead mount drops its saddle, barding and cargo in a loot bag. Release: drops tack and cargo. Pregnant mother killed: the young dies. Imprinter leaves the server: bonus inactive. Hatching inside a wall: spawn adjust. Baby in water: swims to shore. Flying mount rider unhorsed: falls. Swim mount on land carrying rider: slow gallumph.

## 3. Plan (build steps)
1. Data: `tame_trees.json` (5 trees), tuning `taming.*` (stats, bond, breeding, growth, imprint, mutations, mounts, flight, swim, hunger, incubator), creatures.json (`birth`, `mount` kinds, `seat`, `back`, `carry`, Floeback row, Windmane row hidden), items.json (saddles, pack saddle, barding ×4 + 10 hidden, fertile eggs ×8, ice block if missing), recipes.json, pieces.json (incubator), lifeskills.json (Saddle master, Creature armorer, Breeder, Rare finder live), spawn_regions.json (Floeback shore), sounds.json + fx.json rows.
2. `WRTameTypes.h`: `FWRTameGenes`, `FWRTameState` (replicated).
3. `WRTaming.h/.cpp`: trees registry; genes roll/inherit/mutate/encode; stats recompute; bond XP; tree effects; hunger; mating/pregnancy/birth; growth/care; tack equip/unequip; cargo; incubator logic (`UWRTamingSubsystem` ticks pairs, incubators); save/load JSON of a tame.
4. Hooks: ApplyHit (tame damage scale, saddle/barding/bareback taken, guardian, bulwark, stalwart, steady seat, fight & kill XP), Ride (seat, sprint, fly, swim), rider attacks (bareback block, warhorse), farm TickPen (farm tree), gather (lodestar), inventory overflow (forager), utility clock (tireless, lucky, bright, seeker), WRGameMode save/load, wild spawn (levels, wild mutants), egg hatching.
5. Looks: `WRTameLook.cpp` (saddle, pack saddle, barding meshes; morph overlay; baby scale; bubble), `M_TameMorph` overlay material (python commandlet), `WRSealBody.cpp` (Floeback body).
6. Screens: `WRUITameCard.cpp`, `WRUIIncubator.cpp`, wheel "Tend" wedge, HUD bubble, flight HUD line.
7. Icons: `Tools/art/make_tame_icons.py` (PIL, flat style) for saddles, barding, eggs.
8. Compile; spot-check against this ledger; CHANGELOG + HANDOFF; commit; LANDED.

## 4. Spot-check (against the plan and THE BAR, by reading the code)

- Fixed in the spot-check: the card / incubator commands now carry the tame's / incubator's position (the server never
  sees the client's open menu); Taming skill XP keys are their own (tameBond / tameBirth / tameImprint) instead of
  husbandry's "birth"; the pannier's inner face clears the barrel's widest point; the incubator hood's brass band is a
  ring of segments (a solid disc hid the egg); tames' row bites and the mount's bite carry their element.
- Honest gaps (in CHANGELOG and LATER-COOKS): the Windmane's own body and land; the Floeback is code-modelled; no
  rider sit / courtship / nuzzle clips; ten lands' barding passes wait for their hazards; stable, necro raising,
  revive, Soulbond, tame names are other items of 6.5 not in this lane's brief.
