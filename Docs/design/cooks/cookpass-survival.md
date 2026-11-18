# Wildreach cook: COOKPASS-SURVIVAL (survival, farming and food: the cook pass on what's built)

Lane cookpass-survival, 2026-11-15 night. A cook pass on what the build lanes landed for survival (`cooks/build-survival.md`,
4e90554), farming and food (`cooks/build-farming.md`, e1aa0d0), and Raven's own hunger / thirst / water (WRStatusComponent,
WRWater). Research how the best games do it, grade every built piece against THE BAR, design the raise, build it.
Spec sources: FULL-GAME 6.1, 6.2 and the first-hour walk (FULL-GAME line 66: "You eat berries, fill a waterskin at the
steaming river"), `cooks/survival-crafting.md` 2.2-2.11, COMBAT-AND-LESSONS 1.10 (committed drinks), the LATER-COOKS lines
the two build lanes left (134-140).

## 0. Requirement ledger

| # | Requirement (one each) | Source | Done where (filled at the end) |
|---|---|---|---|
| Q1 | Research how the best games do survival, farming and food (named games, places, systems) plus real-world reference; written here | brief, Aaron 11-13 | section 1 |
| Q2 | Grade every built piece against THE BAR | brief | section 2 |
| Q3 | Design the raise in writing (every detail, edge cases, build steps) before building | brief, Aaron 11-13 | section 3 (written before code) |
| Q4 | Build exactly the plan at full detail; no placeholders; unfinished stays hidden | brief, BAR | section 5 |
| Q5 | Spot-check against the plan and THE BAR by reading the code; fix before push | brief | section 4 |
| Q6 | "Already done" needs file:line proof | brief | section 5 |
| Q7 | Design with intention: every object says why it is here, what it connects to, how it meets what it touches | Aaron 11-14 | 3.8 |
| Q8 | Data-driven: numbers in Data/*.json / tuning; sounds and fx as rows; damage through ApplyHit; server authority | brief, BAR | 3.x, section 5 |
| Q9 | Mouse AND pad on every screen touched | BAR | section 5 |
| Q10 | Multiplayer: server authority, replication, every machine sees it | BAR | section 5 |
| Q11 | Wildreach is not a music game: no music shown on screen (staff, notes, beat markers, rhythm meters, on-beat prompts, score), no mechanic that needs playing to the music; remove any an earlier pass added | Aaron 11-15 | 2.0 |
| Q12 | Keep the soundtrack switching smoothly with the situation | Aaron 11-15 | 2.0 |
| Q13 | Allowed: enemy / boss attack timings lining up with the music; keep those if any exist | Aaron 11-15 | 2.0 |
| Q14 | Git: pull, commit by name, CHANGELOG Unreleased + HANDOFF in the same commit, no release / tag, LFS sizes reported, LANDED line | brief | section 5 |
| L1 | A real lie-down / sleeping pose (today the death clip's last pose is held on the bed) | LATER-COOKS 134 (build-survival) | |
| L2 | Rain's Wet status in ApplyHit (fire x0.8, burn x0.7, frostbite x1.15, shock x1.2) | LATER-COOKS 135 | |
| L3 | A held-torch grip (the left arm raised a little; today the torch is placed at the left hand socket) | LATER-COOKS 139 | |
| L4 | Farm animals need a hungry look and a pen graze idle | LATER-COOKS 140 | |
| L5 | Smokehouse and oven need a chimney smoke fx row | LATER-COOKS 140 | |
| L6 | Greenhouse glass wants a real translucent glass material (M_Ice stands in) | LATER-COOKS 140 | |
| G1 | Eating is instant and invisible: no motion, nothing in the hand, no commitment, usable mid-swing | grade 2.1 | |
| G2 | The waterskin is a one-use item crafted from hide; the spec fills it at water | grade 2.2, FULL-GAME line 66 | |
| G3 | Hunger and thirst run down silently: no body cue, no word before the health drain, the 2-minute grace invisible | grade 2.3 | |
| G4 | The kitchen shows nothing of its work: the spit is bare while meat cooks, the pot never steams, cooked food is invisible in the tray | grade 2.4 | |
| G5 | Crops stand rigid in any wind or storm | grade 2.5 (build-farming R3 "no wind sway") | |
| G6 | Potions drink with the spell-cast clip (a hand flung forward), no bottle | grade 2.1 | |

## 1. Research

| Game / reference | The specific system | What makes it work | What we take |
|---|---|---|---|
| **Elden Ring** (Flask of Crimson Tears, boluses, Exalted Flesh) | Every consumable is a committed body motion (~1 s); you walk slowly while it plays; the effect lands partway through, a stagger before then wastes it; bosses read the drink and punish it (Margit's delayed swing after a heal) | Using an item is a decision with a cost, read by the world | Food and drink join the potion's rule: committed, walk slowed, the worth lands at the swallow, a stagger before it loses it, creatures that read drinks read eating (State.Drinking already does all this for potions) |
| **Monster Hunter World / Wilds** (Well-done Steak, Potion) | The hunter sheathes, eats in two big bites, flexes; a Potion is drunk with the head tipped back; the item is visibly in the hand | You see what you consume; the body sells it | The food itself in the hand (meat on the bone, a fish, a bowl, a loaf, a handful of berries, the waterskin, the potion bottle), two bites for food, a long tipped-back pull for drink |
| **Red Dead Redemption 2** (cores, eating from the satchel) | Arthur's stomach growls audibly when hungry; eating plays a hand-to-mouth bite; the core icon pulses | The body tells you before the meter does | A stomach growl under 20 hunger, a dry swallow under 20 water, both only on your own machine, spaced 45-75 s; the gauge pulses when empty |
| **Don't Starve Together** | The character says "I'm hungry" at low hunger; the stomach meter pulses and its arrow shows the drain; starving drains health visibly | Words at the threshold, not just a bar | One line at 25 ("You're getting hungry"), one at empty with the grace ("Starving: in 2 minutes you start losing health"), one when health starts going |
| **Subnautica** | PDA voice at food / water thresholds; drinking fills a bottle; "You are starving" before damage | Fair warning before harm | Notices before harm, each said once per crossing |
| **Conan Exiles** (waterskin, drinking at water) | E at water drinks by hand; a waterskin is filled at any water and has charges; it's the reason to carry water into the desert | Water as a carried resource; refill is a place-based verb | The waterskin fills at water: drink a full skin and an empty skin comes back; E at water when you've drunk your fill fills every empty skin you carry |
| **Ark / Green Hell / Death Stranding** | Canteens and skins refill at water sources (Green Hell: crouch at the river, the bottle glugs; Death Stranding: springs refill the canteen) | The fill is a short kneel with a glugging sound | The fill: kneel (the pickup clip), 1.4 s, glugging, every empty skin filled |
| **Valheim** (cooking station, cauldron) | Raw meat is hung on the spit over the fire and visibly browns; it sizzles; take it off cooked or it chars; the cauldron bubbles while it cooks | The fire shows its work; a camp looks alive | The spit carries the meat or fish being cooked, browning from raw to cooked as the job runs, turning slowly; a cooked piece waits on a flat stone by the fire; the pot's lid lifts and rattles over its steam while it cooks |
| **Breath of the Wild** (cooking pot) | Ingredients go in, the pot bounces and sizzles, a jingle on success, Dubious Food on failure | Discovery is a moment | (Already built: the cook card, Grey mush.) Kept |
| **Stardew Valley** (animals) | Fed animals are happy; unfed ones look glum with a mood icon; animals graze outside | The pen reads at a glance | A pen animal with an empty trough hangs its head (the creature pass's slump stance); a fed one dips to graze now and then |
| **Real world: eating and drinking** | A bite is ~0.5 s hand-to-mouth, a chew of ~0.6 s, the second bite smaller; drinking from a skin: the neck to the lips, the head tips back 20-30 degrees, the throat works; filling a skin in a stream: the mouth held upstream under the surface, bubbles glug until it's full | | Eat 1.5 s (two bites at 0.35 and 0.85, swallow at 1.15); drink 1.1 s (lift 0.25, pull to 0.85 with the head back 22 degrees, lowered by 1.1); the fill 1.4 s |
| **Real world: crops in wind** | Stems flex from the base; the head (grain, fruit, leaves) sways most; gusts arrive as pulses that travel across a field | | Each plant leans from its base, its own phase from where it stands so a gust rolls across the plot; storms and gusts sway it harder |
| **Real world: kitchen fire** | Meat on a spit browns from the outside, fat drips and spits into the coals; a lidded pot's lid lifts and rattles on its steam; a smokehouse breathes thin smoke from its ridge vent, an oven's chimney a steady plume | | The browning, the rattling lid with steam, smoke from the smokehouse vent and the oven chimney while they work |

Lessons: (1) using food is a body action with a cost, like the potion; (2) show the thing itself (in the hand, on the spit);
(3) the body warns before the meter harms; (4) water is carried and refilled at a place; (5) a working kitchen looks
like it's working; (6) living things move in the wind.

## 2. Grades of what's built (against THE BAR)

### 2.0 The music rule (Q11-Q13)
Searched the whole UI and gameplay source for anything that shows the music or asks you to play to it (`WRUI*.cpp`,
`WRHUD.cpp`, `WRSound.cpp`, `WRAudioDirector.cpp`, creature / boss AI): no staff, notes, beat markers, rhythm meter,
on-beat prompt or score exists. The only beat logic is the music director holding a change for the next beat or bar
(`WRAudioDirector.cpp`, cookpass-audio 4.4: the soundtrack switching smoothly) and a stinger drum under the switch into a
fight, both inside the soundtrack. The only "music" in the world is set dressing (the Crystal singing gallery's stone
music stands, `WRLandSites.cpp:520`). Nothing to remove; the soundtrack switching stays; no attack is music-locked today
(allowed, not required by this lane).

### 2.1 Eating and drinking (G1, G6): grade D
`UWRInventoryComponent::UseCategory` (`WRInventory.cpp:1251`) applies food instantly with only a sound: no body motion,
nothing in the hand, usable mid-swing and mid-roll, invisible to other players and to creatures that read item use.
Potions are committed (`StartDrink`, `WRInventory.cpp:1106`) but play the cast clip (`WRCharacter.cpp:1251`): a spell
gesture, no bottle. Fails BAR "real motion, no gliding" and CAL 1.10.

### 2.2 Water (G2): grade C-
E drinks at water (`WRWater.cpp:328`, good: kneel, scoop, gulps). The waterskin (`items.json`, hide at the workbench) is
used up when drunk: a one-shot item, never filled; the spec's first hour fills it at the river.

### 2.3 Hunger and thirst (G3): grade C
Drain, deprivation, death lines are built (`WRStatusComponent.cpp:921`, `WRUIFlow.cpp:316`). The HUD shows the gauges
under 25 (`WRUIHud.cpp:406`) but nothing tells you anything: no body sound, no word when you hit empty, the 120 s grace
before health drains runs silently, and the health drain starts with no notice.

### 2.4 The kitchen (G4, L5): grade C
Stations are well modelled (`WRBuildLook.cpp:559` campfire with spit, pot on a tripod, oven, smokehouse) and their fires
burn (`WRFxAmbient.cpp:73`), but the spit is always bare, the pot never steams, the smokehouse and oven chimney never smoke,
and a finished dish waits invisibly in the tray.

### 2.5 Crops (G5): grade B
Nine modelled plants in four stages (`WRFarmLook.cpp:77`); they never move, in a calm or a thunderstorm.

### 2.6 Pens (L4): grade B-
Pens, troughs, shelves and breeding are built (`WRFarm.cpp:1459`); a hungry animal looks exactly like a fed one.

### 2.7 Sleep, torch, wet, glass (L1, L2, L3, L6)
Sleep holds the death clip's last pose (`WRHeldLight.cpp:155`): grade C. The torch sits in a hand hanging at the side
(grade C: the flame by the knee). Rain has no Wet (open). Greenhouse glass uses M_Ice (grade C).

Fishing (`WRFishing.cpp`: cast, nibbles, plunge, tension, surges, Animal Crossing / RDR2 / Sea of Thieves research),
Rest and comfort, weather, corruption, cooking discovery, spoilage and the farm's data: grade A-/B+, not raised here.

## 3. The raise (design and plan)

### 3.1 Eating and drinking are committed, seen and in the hand (G1, G6)
- **Rule (server).** Food (category `food`) and drinks (category `drink`) go through `UWRInventoryComponent::StartConsume`,
  the potion's frame: refused while riding, staggered, frozen, rolling or already using an item; the running ability is
  cancelled; `State.Drinking` for the use's time (no guard, roll or swing; walk x`potions.walkMultiplier` 0.4; creatures
  that read item use punish it, same tag). The item is taken as it starts (the potion rule); its worth (hunger, thirst,
  Sated, corruption, a brew, the shared meal) lands at the swallow (`food.eat.landsAt` 0.77 of the time for food,
  `food.drink.landsAt` 0.75 for drinks) if the tag is still on; a stagger, freeze or death before it loses it (log line).
- **Times** (`tuning food.eat`): food `seconds` 1.5, drinks 1.1; a quick-eat key press while already eating is refused
  ("already eating").
- **The shared belt cooldown** is not used: food isn't a potion; you can eat back to back.
- **Seen (every machine).** `AWRCharacter::MulticastConsume(Item, Seconds, bDrink)` (unreliable multicast, like the body
  one-shot). Each machine that draws:
  - **The thing in the hand** (`AWRCharacter::TickConsumeLook`, new `WRHeldFood.cpp`): a procedural mesh at `hand_l`
    built by `FWRToolMesh::Food(Shape, Colour)` from `Data/food_look.json`: shapes `meat` (a roast haunch on its bone:
    lathe-turned bone with knuckles, the meat lofted round it, a seared crust colour), `fish` (FWRFarmLook::Fish in the
    fish's own colours, from fish.json), `bowl` (a turned wooden bowl, the dish's surface inside in its colour), `loaf`
    (a scored loaf / pie / cake by colour), `berries` (a cupped handful: 7 berries), `fruit` (one round fruit or root with
    a stalk), `skewer` (a stick with four chunks), `egg`, `waterskin` (a stitched leather bag with a wooden neck and
    stopper), `bottle` (a glass vial with a cork, the potion's colour). Items map exact id first, then by word in the id,
    then by category (`food_look.json byWord`, `byCategory`).
  - **The hand to the mouth** (`FWRHeroAnimProxy::PostLayers`, input `Consume` 0..1 and `ConsumeBite` 0..1, `bConsumeDrink`):
    the left upper arm lifts forward and in, the elbow folds so the hand comes to the mouth (`food.pose.armForward` 58,
    `armIn` 26, `elbowBend` 128 degrees), the head dips 6 degrees to meet a bite and the hand pushes in on each bite
    (`biteDip`); a drink tips the head back 22 degrees (`drinkTilt`) with the hand higher. Eases in over 0.25 s and out
    over the last 0.25 s; while a one-shot owns the upper body it gives way.
  - **The off hand** frees itself: a shield or a lit torch in the left hand is put out of sight for the use and comes back
    after (the light keeps burning in the rules; only its mesh is hidden). The weapon stays in the right hand.
  - **Sound**: `eat.bite` at each bite (recorded bites, Anthousai 398757), `eat.chew` after, `eat.swallow` at the swallow
    (Stasser 679737); drinks `drink` gulps at the pull (existing) and the swallow.
- **Potions** use the same look with the `bottle` shape and the drink pose (the cast clip only when the hero has no drink
  clip goes away; a real `drink` clip in player_art still wins).
- **First person**: the hand and food are owner-visible (they rise into the bottom of the view like the torch).
- **Edge cases**: mounted refusal (like the potion); swimming: allowed (no pose change needed; the arm still lifts); dead:
  nothing; a late joiner just doesn't see a use already playing; the server's timer holds a weak pointer.

### 3.2 The waterskin fills at water (G2)
- **Items**: `waterskin` (full: Water +40) and new `empty_waterskin` ("Empty waterskin", stack 5, 0.2 kg, category
  `container`, note "Fill it at any water: E where you'd drink"). Drinking a full skin gives an empty one back (in the
  bag; dropped at your feet if the bag is full). The workbench makes an **empty** skin (hide 1); the Wellfrog's product
  stays a full one (it fills it); the eel waterskin keeps its own rule.
- **Recipes** that use a waterskin as water give the empty skin back: crafting rows carry `"returns": {"empty_waterskin":
  1}` (FWRCrafting reads it when an order completes, into the bag or tray like the output); the cooking pot's Water group
  gives it back when the dish is ordered.
- **The fill** (`UWRWaterSubsystem::Drink`): E at water, when you're not thirsty (Water at 99.5+) and carry empty skins,
  fills them all: the kneel (pickup clip), 1.4 s (`water.fillSeconds`), `water.fill` glugging (asagatov 323003), "Filled 3
  waterskins". When thirsty, E drinks a gulp as now; the prompt says what E will do: "E: drink" / "E: fill 3 waterskins".
- **Edge cases**: the sea is drinkable on this world (Raven's `water.seaDrinkable` 1), so the sea fills a skin; a world
  that sets it 0 can't fill there either ("The sea is too salty"). A full bag still works (the stacks swap in place).

### 3.3 The body warns (G3)
- **Notices** (owner only, `ClientNotify`, said once per crossing, re-armed when you climb back over the line by 5):
  hunger / water under 25: "You're getting hungry" / "You're getting thirsty"; at 0: "Starving: eat within 2 minutes or
  you start losing health" / "Parched: drink within 2 minutes or you start losing health" (the grace from tuning); the drain
  starting: "You're losing health to hunger" / "...to thirst".
- **Body sounds** (owner only, 2D quiet, `voice` category): `body.stomach` under `food.warn.bodyBelow` 20 hunger every
  45-75 s (Breviceps 447911, UnderASpell 696025 in two); `body.dry` (the dry swallow) under 20 water every 40-70 s.
- **The gauge** pulses (alpha 0.55-1.0 at 1.4 Hz) while its meter is empty.

### 3.4 The kitchen shows its work (G4, L5)
Client-side (`AWRStation::TickKitchenShow`, stations of a kitchen type tick at 10 Hz on drawing machines when within
`farm.show.range` 4000 cm of the camera; no server cost):
- **Campfire spit**: while a job runs whose output has a `meat` / `fish` / `skewer` look, that food hangs on the spit's
  middle (the spit is at 74 cm), turning at 25 deg/s, its colour lerped raw to cooked by `JobProgress()` (raw from
  `food_look.json raw` colours: meat (0.55, 0.12, 0.10), fish (0.62, 0.62, 0.60)); fat-spit sparks are the fire's own.
- **The tray**: a station's first `Output` stack is laid on a flat hearth stone beside it (campfire / pot / oven at
  (70, 50, 0) cm, the stone a modelled slab), in its look, up to 3 pieces.
- **The pot**: while working, the lid lifts and rattles (0-2.5 cm at 7 Hz with jitter), `fx kitchen.steam` from the lid's
  edge, `cook.bubble` loop (the sizzle's lower pitch).
- **Smokehouse / oven** (L5): while working, `fx kitchen.smoke` at the smokehouse ridge vent (0, 0, 276) and the oven
  chimney top (-36, 0, 194).
- fx rows `kitchen.steam` (soft white rising wisps, 1.2 s life, 30 cm) and `kitchen.smoke` (grey-brown column, 3 s life,
  rising 60 cm/s, widening).

### 3.5 Crops move in the wind (G5)
`FWRFarmLook` builds each of a plot's nine plants into its own section-less child component (one procedural mesh per
plant, pivot at its base); the client turns each about its base: lean = `farm.sway.calm` 2.5 degrees x (1 + `gust`) x
the plant's growth, along the world wind direction (`UWRWeatherSubsystem` gusts / storm: x3 in a thunderstorm or
blizzard), at 0.6 Hz with the phase from the plant's world X/Y along the wind (a gust rolls across the plot) plus a
0.17 Hz second wave. Only plots within `farm.sway.range` 3000 cm of the camera move (others keep still: no cost).

### 3.6 Pens read at a glance (L4)
A tamed pen animal (in a pen piece's roster) with an empty trough holds the creature pass's slumped stance with its head
low (`WRCreature` mood pose: `StanceSlump`); a fed one dips into its graze (the eat pose) for 3-5 s every 12-20 s.

### 3.7 Sleep, torch, wet, glass
- **L1 sleep pose**: a procedural lying pose in `PostLayers` instead of the death clip's held frame: on the back, the head
  turned 15 degrees to one side, the arms loose at the sides with the near hand on the belly, one knee raised, the chest
  rising 1.2 cm at 0.22 Hz (breathing). Input `Sleep` 0..1 eased over 1.2 s.
- **L3 torch grip**: the left upper arm lifted forward 32 degrees and the elbow bent 70 so the torch is held at chest
  height ahead of the body (BotW's torch carry); gives way to one-shots.
- **L2 Wet** (decided in build, built): open rain (`weather.json watersCrops` kinds) or being in water sets `WetUntil` =
  now + `wet.drySeconds` 60 on the survival component; a warding fire dries you in 8 s. While wet: fire damage x0.8 in
  ApplyHit, burn buildup x0.7, frostbite x1.15, shock x1.2; the HUD conditions show "Wet · dry in m:ss" with the rule, the
  rain glyph. A short condition with a clear counter, not a temperature meter (Raven's rule).
- **L6 glass** (built): `M_WindowGlass` made by a headless pythonscript run (`Tools/terrain/build_glass_material.py`,
  check_setup passed): translucent, surface-lit, two-sided; pale green-grey crown-glass cast rippling with world position,
  opacity 0.12 face-on to 0.6 at grazing (Fresnel 4), roughness 0.05, specular 0.8.
- **L1 sleep**: the held lying pose is kept (the death clip's last frame lies the body on its back on the mattress, which the
  build lane fitted) and made alive: the chest breathes at 0.22 Hz and the head turns 15 degrees to one side. A real
  lie-down clip stays on LATER-COOKS (it needs a retarget in the editor).

### 3.8 Intent and contacts (Q7)
- The food is held in the off hand because the weapon hand keeps the weapon (Elden Ring, Dark Souls); a shield or torch
  in that hand is set out of sight for the moment rather than lifted into the face.
- The meat hangs from the spit's middle, the spit resting in the two forked posts (already built), clear of the leaning
  logs (the logs' tops meet at 50 cm, the spit is at 74, the meat hangs 74 down to ~58).
- The cooked food waits on a flat hearth stone set on the ground beside the fire ring (outside the ring's 45 cm radius +
  stones), never on the coals.
- The pot's lid rides its rim (82 cm) and lifts only on its steam; the steam leaves at the lid's edge.
- The smoke leaves where a smokehouse vents (the ridge vent cap) and where the oven's chimney is open.
- A crop bends from where its stem meets the soil; it never slides.
- The waterskin fills in the water you're kneeling at; the empty skin you get back after drinking is the same skin.

### 3.9 Build steps
1. Data: `food_look.json`, `items.json` empty_waterskin, recipes `returns`, tuning `food.eat`, `food.pose`, `food.warn`,
   `water.fillSeconds`, `farm.show`, `farm.sway`; sounds rows; fx rows; audio files cut from the recordings
   (`Tools/audio/build_survival_audio.py`).
2. FWRToolMesh::Food shapes. 3. StartConsume + MulticastConsume + TickConsumeLook + anim input. 4. Waterskin return, fill,
   recipe returns, prompt. 5. Warnings + body sounds + gauge pulse. 6. Kitchen show. 7. Crop sway. 8. Pen moods.
9. Sleep pose, torch grip. 10. Compile; spot-check; CHANGELOG, HANDOFF, commit, push.


## 4. Spot-check (read against the plan and THE BAR)

Read through every change after the build:
- Eating: the item is taken at the start and the worth lands at the swallow only while `State.Drinking` is still on (a
  stagger clears it): `WRInventory.cpp` StartConsume timer. Interrupted on a client: `ConsumeFraction` drops the food when
  the tag goes (0.25 s grace for the tag to arrive). Shield / torch come back when the use ends or the body dies.
- Fixed in the read-through: `AddItem` returns what didn't fit (not what did), so the empty skin's "no room" check is
  `> 0`; `FString::Printf` can't take a ternary format (two errors at the first compile); the crop-sway list is per world
  (play-in-editor runs several worlds; a shared static list would have been rebuilt every frame).
- Kitchen show: the station's root is a scaled box, so every show part is placed in world space from the piece's frame
  (station location less `ChildLift`), never relative (it would have stretched the meat and the lid).
- The pot: the lid left the piece's blocks (it lives on the station so it can lift); the chain was shortened to end at
  the new arched bail's crown (104 cm), the lid's knob lifts at most 2.5 cm under it.
- Pens: an animal's flee / threat moods are never overridden (only Calm..Preen become the pen's).
- Wet: one clock (`UWRWeatherSubsystem::Now`) for setting and reading; replicated to the owner only (the HUD); the server
  reads it for hits.
- Music: no music UI exists anywhere (searched every UI and AI file).

## 5. Proof (requirement → where)

Commits on master (Wildreach): fa011f5 (piece 1, merged 055c520), 144e24f (piece 2, merged 6bce978). Paths are
`Source/Wildreach/` unless written.

| # | Done | Where |
|---|---|---|
| Q1 | Yes | section 1 |
| Q2 | Yes | section 2 |
| Q3 | Yes | section 3, committed before code (showcase-notes 5c3847c) |
| Q4 | Yes | pieces below; nothing unfinished shown |
| Q5 | Yes | section 4 |
| Q6 | Yes | this table |
| Q7 | Yes | 3.8; pot chain / bail / lid (`WRBuildLook.cpp:600`), spit food on the spit axis, tray stone outside the ring (`WRKitchenShow.cpp:63`), plants from their base (`WRFarmLook.cpp:462`), food in the free hand (`WRHeldFood.cpp:404`) |
| Q8 | Yes | `Data/food_look.json`; tuning `food.eat/drink/pose/warn`, `water.fillSeconds`, `farm.show`, `farm.sway`, `farm.pen.graze*`, `lights.carry`, `rest.breathHz`, `wet`; sounds.json eat.bite/chew/swallow, water.fill, body.stomach/dry, cook.bubble; fx.json kitchen.steam/smoke; items.json returns/fills; fire through ApplyHit (`WRCombatLibrary.cpp:769`) |
| Q9 | Yes | no new screen; HUD lines (Wet `WRUIHud.cpp:1703`, verbs `WRUIHud.cpp:1178`) need no input |
| Q10 | Yes | server rules (StartConsume `WRInventory.cpp:1329`, DrinkHere `WRWater.cpp:368`, TickPen `WRFarm.cpp:1508`, Wet `WRSurvival.cpp:1189`); every machine draws (MulticastConsume `WRHeldFood.cpp:336`, kitchen show from replicated Queue/Output, MoodRep, WetUntil owner-only) |
| Q11 | Yes (nothing to remove) | 2.0 |
| Q12 | Yes (untouched) | `WRAudioDirector.cpp` |
| Q13 | Yes (none exist, none removed) | 2.0 |
| Q14 | Yes | CHANGELOG Unreleased + HANDOFF in both commits; no tag or release |
| L1 | Partly | breathing + head turn `WRHeroAnimInstance.cpp:507`; real lie-down clip stays on LATER-COOKS |
| L2 | Yes | `WRSurvival.cpp:1189`, `:1615`; `WRCombatLibrary.cpp:769`; `WRStatusComponent.cpp:304`; `WRUIHud.cpp:1703` |
| L3 | Yes | `WRHeroAnimInstance.cpp:466`; `WRHeldFood.cpp:370` (TorchCarry) |
| L4 | Yes | `WRCreature.h:173` Hungry; `WRCreatureBody.cpp:162`; `WRFarm.cpp:1508` |
| L5 | Yes | `WRKitchenShow.cpp:63` (smoke at vent / chimney); fx.json kitchen.smoke |
| L6 | Yes | `Content/Wildreach/Materials/M_WindowGlass.uasset`; `WRFarmLook.cpp:407`; tuning farm.glassMaterial |
| G1 | Yes | `WRInventory.cpp:1329` StartConsume, `:1392` ApplyConsumable; `WRHeldFood.cpp:169` Build, `:404` TickConsumeLook; `WRHeroAnimInstance.cpp:482` |
| G2 | Yes | items.json waterskin returns / empty_waterskin fills; `WRWater.cpp:356`, `:368`; `WRCrafting.cpp:1104`; `WRFarm.cpp:1055`, `:777` |
| G3 | Yes | `WRStatusComponent.cpp:952`; `WRHeldFood.cpp:493`; `WRUIHud.cpp:1569` |
| G4 | Yes | `WRKitchenShow.cpp:63`; `WRBuildLook.cpp:600` |
| G5 | Yes | `WRFarmLook.cpp:462`, `:612` |
| G6 | Yes | `WRInventory.cpp:1139` (potions use the bottle and the drink pose) |
