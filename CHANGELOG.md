# Changelog

## Unreleased: the UI cook pass (settings, HUD, menus, controls)

Aaron (lane cookpass-ui: research, grades, requirement ledger and plan in showcase-notes cooks/cookpass-ui.md; a cook
pass on the built screens and the ui-controls cook's open rounds). Compiled (editor target); not played here (Aaron
and the testers test, F8).

- **UI (1): Camera, Interface and the rest of Look and controller.** Why: the field of view, shake and blur were fixed
  (motion sickness: TLOU2 / XAG), every hold was a hold (XAG 107), the pad's dead zone and curve were tuning-only, and
  crosshair / damage numbers / hints were filed under Controls and Video.
  - Player prefs are one table in `FWRKeyBindings` (`Pref(id)` / `SetPref` / `ResetPage`, saved as `Pref.<id>` in
    `[Wildreach.Keys]`, only when not the default).
  - **Look and controller**: aiming sensitivity (mouse x0.70, pad x0.60, applied while a ranged weapon is aimed),
    invert left / right, stick dead zone 0.05-0.35, response Linear / Standard / Precise (exponents tuning
    `look.padCurveLinear / padCurve / padCurvePrecise`), vibration Off / 25 / 50 / 75 / 100 %, and **Hold or toggle**
    for Sprint, Guard and Aim. A toggled sprint ends on the next press, when you stop (0.25 s under 50 cm/s, tuning
    `ui.toggle`), on a menu or when stamina drops it; a toggled guard / aim: the first press is a real press (the parry
    window opens), the next lets go, as do a menu, death, a mount or swapping weapon set.
  - **Camera** (new page): field of view third person 70-100 and first person 75-110 (default 90 both, the combat
    camera's; a bow's aim zoom keeps its 60 / 90 share of whatever you pick), camera shake 0-100 % (scales the trauma
    and the FOV punch), motion blur Off / Low / High (`r.MotionBlur.Amount` 0 / 0.25 / 0.5), shoulder Right / Left /
    Centre (eased across).
  - **Interface** (new page): Setup Desk / Couch (Couch = text and HUD at 115 %, tuning `ui.couchScale`), Text and
    menus 85-130 % (the engine's application scale: every screen), HUD 75-130 % (a scaler over the HUD only), HUD mode
    Full / Fade / Minimal and Compass (the HUD reads them from UI (2)), Crosshair and Damage numbers (moved from
    Controls), Names over players Always / Near / Off, **Show my helmet** (the server changes your look now and the
    next join carries it in `?Look=`), Show hints (moved from Video).
  - Each page's Reset puts only that page back.
- **UI (2): the HUD's modes, the compass switch, nameplates.** Why: the HUD was always fully up (Horizon's Dynamic
  HUD / Ghost of Tsushima's "light HUD": let the world breathe, come back the instant it matters); names over heads
  started at the bar's left edge and the knocked-out line said "E" on a pad.
  - **Fade** (the default): the level and clock plates, compass, vitals, hotbar, ability keys and tame card fade over
    0.8 s once all has been calm for 6 s and come back in 0.1 s on any change: hurt or tired, a path resource moving,
    a status building or on you, hungry or thirsty under 25, locked on, aiming, a boss, a raid, a screen or wheel,
    build mode, riding, the bag or hotbar changing (tuning `ui.hud`). Prompts, notices, pickups, chat, the crosshair,
    boss bars and hazard lines never fade. **Minimal**: no plates or compass; the vitals only while something is
    happening to them; the hotbar for 4 s after it changes. **Full**: as before. (`SWRHud::TickHudMode`.)
  - **Compass** off hides the strip; the boss bar keeps its place.
  - **Nameplates** (`AWRHUD::DrawWorldBars`, `DrawCentred`): every line centred over the head; other players' names
    follow Names over players (Always out to 80 m, the name alone past the 25 m bar range; Near 40 m; Off), tuning
    `ui.names`; the knocked-out line shows the device's own button for Interact.
- **UI (3): leaving asks once, the Disconnected card with Rejoin, the crash card, quick notes on F8.** Why: Main menu
  and Quit left at once (a slip of the pad lost a fight outside a safe zone); a dropped join showed the engine's raw
  text ("Disconnected: ConnectionLost") with no way back in; a crash was never reported unless the tester remembered;
  a pad player had to type every report.
  - **Leave / Quit** (the settings window in game): a strip over the footer asks "Leave this world?" / "Quit
    Wildreach?" and says whether you're in a safe zone (and, for a host with others in, that everyone goes back to
    their menu). Stay / Leave; Esc or B is Stay.
  - **Disconnected card** on the main menu (`SWRMainMenu::MakeCard`, `NoteFailure`): a plain line for each failure
    (the connection lost, the host stopped answering, the host didn't let us in and its reason, another version,
    files that don't match, the network couldn't start, the address unreachable), and **Rejoin** when this game was
    joined to the host: it travels to the same address with your name, path and look; a try that doesn't get in
    counts down 10 s to the next ("Try 2 of 6 didn't get in. Trying again in 7 s.") up to 6 tries in 60 s, then says
    the host isn't answering (tuning `ui.rejoin`). Stop trying / Close; Esc or B closes; while it shows the pad's
    D-pad stays inside it.
  - **Crash card** (`WRReport::PendingCrash / SendCrash / DismissCrash`): each run stamps `Saved/Wildreach/session`
    and a normal quit stamps `clean_exit`; a run that didn't quit cleanly and left a folder in `Saved/Crashes` makes
    the menu ask once: Send the crash report (its error message and call stack from `CrashContext.runtime-xml`, the
    last 40 log lines (tuning `report.crashLogLines`), version, GPU, OS; saved in `Saved/Reports` and sent through
    the outbox like any report) or Not now. Never in the editor.
  - **Quick notes** under the F8 note (tuning `report.chips`): "Just happened", "Happens every time", "Hit didn't
    land", "Got hit by nothing", "Can't move", "Fell through", "Too hard", "Too easy", "Looks wrong", "Sounds wrong",
    "Music stopped", "Love this"; one press adds the line, again takes it out.
- **UI (4): the pad keyboard, chat on a pad.** Why: no text box could be typed on a controller (the chat, the report
  note, the menu's name and address, world and tame names), and the chat had no pad button at all.
  - **`SWRPadKeyboard`** (new `WRUIPadKeyboard.cpp`): a stone panel with a preview line (blinking caret) over a QWERTY
    grid: digits, letters, ' - . ? (Shift: ! @ # $ % & * ( ) = and " _ , :), then Shift / Space / Delete / Done. Pad:
    the D-pad (repeating) or the cursor picks a key, A types, X deletes, Y spaces, LT shifts (twice within 0.4 s locks,
    tuning `ui.padKeyboard.lockSeconds`), RT or Menu is Done, B cancels; the mouse clicks keys; touching the real
    keyboard steps it aside. It docks at the bottom, or at the top for a box low on the screen.
  - **Every text box, no screen code:** A over any editable text on a pad opens it on that box (`WRPadInput`); Done
    writes the text back and presses Enter on a one-line box (so a name is taken, a chat line sends); Cancel leaves it.
  - **Chat on a pad:** Settings (in game) has **Chat** (a speech-bubble icon): it opens the chat line with the
    keyboard up on it; Done sends to everyone, B closes the line. The F1 page says where.
  - `Data/gamepad.json` **reserves** D-pad up hold (the ping / quick-chat wheel) and View hold (the player list) for the
    multiplayer cook, so nothing else claims them.
- **UI (5): pick your face, build and helmet on the main menu, seen on your survivor.** Why: the hero look's runtime
  (12 faces, 3 builds, `?Look=`) had no screen, so everyone played face 1, athletic (LATER-COOKS, build-hero-look-2).
  - The character panel has a **Look** row ("Wren · Athletic", Change). Change opens **Your look** beside the panel:
    the faces by name for the body picked (`player_art.json hero.<m|f>.faces[].name`), the build (Lean / Athletic /
    Strong; Slim / Athletic / Curvy), the helmet (Shown / Hidden), Done.
  - While it's open the menu's camera glides to a portrait of your survivor (300 cm, chest height, 20° off the face,
    FOV 40; other angles tried when rock is in the way) with a warm key light beside the camera, and every pick shows
    on the body at once (`AWRCharacter::PreviewLook`: the menu's own standalone world only, never a shared one; the
    body too). Done (or Change again) goes back to the island view. Tuning `flow.lookCamera`.
  - Picks are saved to `[Wildreach.Menu] Look` and travel with Host and Join (`?Look=<face>.<build>[.h]`); switching
    the body keeps the face inside its list. The settings' Show my helmet and this panel are the same value.
- **UI (6): map pins.** Why: the map held one waypoint; players mark ore, dens, loot and camps (Valheim's pins).
  - Right-click on the journal map (X on a pad) opens a small card beside the spot: a name (24 letters, optional; the
    pad keyboard on a pad), the six marks (Camp, Ore, Danger, Creature, Loot, Flag, drawn in code), Waypoint here,
    Cancel. Picking a mark pins the spot. On a pin the card renames it, changes its mark (Save), removes it, or sets
    the waypoint there. Right-click on your waypoint still clears it. B or Esc closes the card.
  - Pins draw as brass-rimmed badges with their mark, the name under them once zoomed in (or hovered); the compass
    shows pins within 400 m as small badges, with the name and distance when you face one.
  - Up to 30 a world (tuning `ui.pins`), this PC's own, saved in `Saved/Wildreach/Pins/<world>.json` (the host's world
    id, or the address a client joined), so each world keeps its own.
- **UI (7): the pad's item wheel.** Why: on a controller the only way to the hotbar's tools, weapons and torch was
  stepping the belt (ui-controls 2.13; Enshrouded / BotW hold a D-pad direction for a radial).
  - **Hold D-pad right** (0.4 s, tuning `ui.itemWheel.holdSeconds`): the wheel opens with your eight hotbar items, each
    wedge its icon and name; the centre shows the one under the stick (its icon, durability or how many, "In hand" or
    its hotbar slot). Release on one takes it in hand (`UseHotbar`), in the middle cancels. The world keeps running;
    held inputs drop on open and close like every wheel.
  - A **tap** stays the next ability (the next ammo while aiming), now on the release (`FWRPlayerUI::ItemWheelKey`).
  - The F1 page says so.
- **UI (8): track a recipe.** Why: gathering for a recipe meant reopening the crafting tab to see what was still
  missing (Grounded / Subnautica pin one recipe's list to the HUD).
  - A **Track** button beside Craft on a recipe's details (again: untrack); one recipe at a time.
  - The HUD shows a **Tracking** card under the clock: the output's icon and name, then each ingredient with its icon,
    "have / need" from your bag, green once there's enough. It clears itself (with a line) once the item lands in
    your bag, and goes with a recipe that leaves the data. Never fades (you asked for it); not in build mode or under
    a screen. This PC's own (`[Wildreach.UI] Tracked`).
- **UI (9): the fill-in map.** Why: RAVEN-PLANNED ("a map you complete by exploring", DD World; ui-controls 2.12), and
  the world-map lane asked for the reveal grid at 8 m cells over the whole map (LATER-COOKS).
  - New `UWRMapRevealComponent` (`WRMapReveal.cpp`) on every PlayerState: a grid of 8 m cells over the map picture
    (`ui.mapMin / mapSize`, so it grows with the continent; 1,466 x 1,466 on today's map), kept by the server. Every
    second the cells within 150 m of your body are seen (walking, riding, swimming, gliding); a shrine you find inks
    600 m around it (the map-fragment idea, tied to the built shrines). Changed 64 x 64 tiles (512 bytes) go to your
    client 4 a frame; a client asks for all of its tiles once when it joins. Saved with the player (`mapReveal`: the
    seen tiles zlib-packed, base64); a grid saved for another map size starts fresh.
  - The journal map draws unseen land under a parchment fog (`#2A241C` at 86%: the coast and mountains show faintly
    through), feathered at the edge (two cells a texel, bilinear), redrawn at most once a second while it changes.
    Camps, caves, passes, mini-bosses and villages show once you've seen their spot; major bosses always (their
    volcano, glacier or spire is seen from afar). Your Hearths, claims, beds, tames and pins show as before.
  - Numbers in tuning `ui.reveal`. **Players who already played start with a blank map** around where they stand
    (nothing was recorded before this).
- **UI (10): the set-complete moment.** Why: putting on the last piece of a biome set did nothing you could see or hear
  (LATER-COOKS, build-crafting).
  - The frame a worn set reaches its last bonus (gear.json sets: the 4-piece), a chime (new sounds.json row
    `gear.setComplete`: the unlock and discovery chimes pitched down to a fuller ring), the centre line "<Set>
    complete: <its bonus>", and that set's pieces on the inventory doll flash brass for 1.6 s (tuning
    `ui.setComplete`). Client-side from the replicated equipment (`FWRPlayerUI::TickSetComplete`); joining in full
    armour is no moment. The tracked recipe's check moved to the same per-frame tick.
- **UI (11): the Necromancy path's node glyphs.** Why: the Paths screen showed the Necromancy tree's 22 nodes without
  icons (LATER-COOKS, build-biome-graveyard).
  - 22 duo glyphs drawn in `Tools/ui_glyphs.mjs` like every path's (`node Tools/ui_glyphs.mjs path_<id> ...` renders
    only the ones named): a skull is the path's mark with its eyes the accent, souls are wisps (a drop with two eyes),
    the Reaper branch draws the scythe, the Lich the crown; capacity / chance / ability shapes follow the other
    paths' (the flask, the four-point star, the sword, the burst). 44 PNGs, 45 KB (Git LFS).
- **UI (12): read-through fixes.** The map's fog texture is rooted for the run instead of held by a function-static
  strong pointer (that one would have been released after the garbage collector at quit: a crash on exit). The crash
  check runs on whichever asks first, the log's install or the main menu's card, so the card can't miss a crash when
  the menu opens before the player's UI.

## Unreleased: the Desert / Sun & sand

Aaron (lane build-biome-desert: requirement ledger R1-R52, build research and plan in showcase-notes
cooks/build-biome-desert.md, building cooks/biome-desert.md). Landing in pieces; compiled (editor target) after each; not
played here (Aaron and the testers test, F8). Not live until `desert` joins `world.liveBiomes` (Desert (3), with its
creatures).

- **Desert (6) the sound**: the beds (`land.desert` with its day and night, `zone.D1`..`D5`: the strand's surf, the Erg's streaming crests, the Pan's ringing quiet, the slots' whistle, the Wall's roar), the storm belt and sandstorms low-pass every tune under the wind (`ambient.beltLowPass`, never muted), the music pools (explore by sub-zone, night, combat, mini, camp, the Hourhold, the Haboob Wall, Saltgate's west mouth, the Long Noon's layer, the three minis and Ahrumet's three phases). All stand-ins from the repo's recordings until the audio cook's score. Portal slots stay dark (`portals.json` desert land false) until the Sunwatch tombs fold kit exists.
- **Desert (5) the bosses** (`WRDesertBosses`, hooks beside the Ocean's in `WRBoss` / `WRBossCombat`; placed by `UWRDesertArenas` once someone is in the Desert): **Ssarrok** (a 22 m Dunejaw on its Erg route, down to the strand's wet sand at dusk, at its lair in a heat wave; fin wake, breach, the swallow (4 s inside, 400 from allies frees you, else spat out: 40 and blind 80), tail slam, geyser roar; a burst that ends off soft dry sand, on rock, hardpan or wet sand from an Oasis water jar, beaches it 6 s). **The Mirage Matriarch** in the Sevenfold Wells (asleep at night, no fight): three shadowless mirages at half damage that pop on a hit and re-form in 12 s; blind on the real one breaks them all for 15 s; the crossfire charge sends all four. **The Glass Empress** in the Glass Canyon: six glass plates (300 each, x0.3 through them, strike blows x2, frost then fire within 4 s shatters one; shards hit everyone near, her too); the bulb gone ends the Prism beam (daylight only, a sweeping 20 m beam that rock blocks) and the sting's blind. **Ahrumet** on his throne at the Sundial Plaza: a code-built 16 m colossus that wakes when you step on the meridian strip; the eye beam sweep (the gnomon and the twelve hour-stelae block it; three beams break a stela), stomps, palm sweep, the grab (300 to the held wrist frees you), sand wave, the Gaze on tames; sun-hot stone after each beam and pulse that cracks under a water hit (an Oasis jar, Tide; frost at half): +60 stance and x1.5 on that part for the fight. At half health he tears the gnomon out of the dial (60 within 14 m, the centre becomes a quicksand funnel) and fights with it as a club: the club combo, the Noon Pulse (80 and blind 100 unless a stela, the planted gnomon or he shades you), the held beam, Glassfall. The hidden Noon Lock with artifacts (a beam turning round the plaza, a pulse every 20 s, shade only under him). Drops: Ahrumet's core, radiant essence, sunstone, the Sun court furnishings blueprint, the Colossus eye (crit blind), trophies (Leviathan tooth, Mirage antler, Empress stinger, Ahrumet's head). Built to compile; not played.
- **Desert (4) the world**: the hazards (`WRDesertHazards`): the belt and Sandstorm weather (run x0.7, eased to x0.85 by a sand veil, Dune oil or Dunewrap 4; sprint stamina x1.3; a 25 m ochre sand fog in the sky, 60 m in a veil; the compass ribbon hides unless you carry a Sunwatch lens or your tame Scarab digger is near), soft sand (x0.85; Sandwalker and the Dromad / Shimmerbuck pass it), quicksand (a wet suck 0.9 s, then you sink over 4 s and the body lowers into the sand on every machine; each dodge lifts 25%, Dune oil frees you on the first; at full you're held and drown 5 s later; an ally pulls you out with E), brine crust (creaks 1 s, breaks: 1.5 m into brine, x0.4, no roll, 4 a second until you wade out and climb), booming slipfaces (1.2 s hum, a 12 m avalanche, 2 s buried, pops any Dunejaw near), dust devils on the Pan 11:00-16:00 (paths from the server clock), flash floods down the two wadis after rain in the Storm peaks (roar, trickle, a 2 m brown wall at 6 m/s; ledges stay dry). The Long Noon (world.json events.long_noon): the sign, the sun held at noon in the Desert's sky with a 2-minute dusk after, sunstone x3 and 20 bloom nodes on the Pan, the tilted stelae's caches, the Seventh Vault's door (the sun-disc key opens the last king's chest), the Fata Morgana over the Erg, Gildbacks tame x2, Glasstails and Dunejaws dug in, kites x2. Weather row to the cook (Heat wave 30 thirst x2, Sandstorm 15, Overcast 5). Six shrines, Amastan and his six quests (Sevenfold Wells water jars, the belt walk with the lens, the blue hands, the shadow's road, the Empress stinger, Ssarrok), the Desert -> Ocean invasion through Saltgate. Items and recipes: Sunwatch lens, water jar, Oasis water jar (thrown: wet sand, a water hit), Dune oil, Clear-eye drops, flash pot (thrown: blind 80), sunglass arrows, red ochre beer, sand veil, Tooth of Ssarrok, Mirage charm, Empress's lens. Fixed the Sun-Vault door (it jumped to the room's middle and its server collision never opened). Built to compile; not played.
- **Desert (3) the creatures**: the Desert goes live (`world.liveBiomes`). Ten creatures with their own brains (`WRDesertCreatures`, hooks beside the Ocean's in `WRCreature` / `WRCreatureCombat`): the Duskjackal (Barghest rig, black and gilt, packs of 2-3 at night, the dusk chorus, a hamstring from your flank), the Glasstail (a 2.2 m scorpion that glares the sun off its sunglass bulb by day, waits under the sand on the paths by night, and grips you 1.5 s before its sting unless an ally frees you), the Carrion kite (Griffon rig, rust and black; circles anything under half health and screams the predators in, drops bones whose shadow grows on the ground), the Dunejaw (a 6 m sand-swimming skink drawn by a sprint on soft sand; its fin and hiss give it away; beached on firm ground), the Gildback (a thorny devil that basks on the Pan at noon, a false head that takes half, tamed with sunstone, its spines light a base and fill a Sun walker's Radiance), the Dromad and Shimmerbuck mounts (the Shimmerbuck leaves a mirage of itself when it bolts), the Scarab digger (tamed by stealing a brood-ball egg; rolls its ball at the thief), the Sandhen (eggs) and the Wellfrog (squeeze a wild one for a drink; a tame one fills a waterskin). The bodies are built in code (`WRDesertBeast.cpp`, posed like the Ocean's) except the jackal and kite. Spawns by sub-zone, day and night (tuning `desertLife.tables`). Attack rows in `attacks.json`, voices (stand-ins) and effects as table rows, 13 drop items. Built to compile; not played.
- **Desert (2): the Sun & sand path** (cooks/biome-desert.md 2.17-2.18; plan build-biome-desert.md 2.16).
  `Data/paths/sun.json` (trunk Sunbeam, Radiance capacity, Blind chance, Sun strike; Radiant, Dune and Solar blade, 100
  points). **Radiance** (`WRAttributeSet` Radiance / MaxRadiance, `WRSunPath`): 100 + 10 a capacity rank; by day +2.5 a
  second exposed to the sky (a 30 m trace straight up: roofs and overhangs are shade), half in shade, +3 a landed melee
  hit, +5 a kill; overcast x0.6, heat wave x1.2, a sandstorm or the belt 0, the Long Noon x1.5 with no drain; at night no
  sky fill and a 1.5 a second drain (Dawn x0.3), refilled 0.5 a second within 6 m of a sunstone lamp, your Beacon or your
  tamed Gildback, half the day rate in Dunewrap's four pieces; underground counts as night. The HUD's Sun-gold bar
  (#E8B230) flashes on a refused press and brightens while it fills. **Sandwalker** (root, main path) is the tag
  `Passive.Sandwalker` the movement and hazards read. **Eleven abilities** (`WRAbilitiesSun`, ClassForNode "sun"):
  Sunbeam (a light bolt, blind 30, homes on creatures only), Solar flare (6 m flash, blind 80), Beacon (a 5 m pool of light
  10 s that burns undead and void creatures and is a lamp for your Radiance), Sunburst (a gold column the party sees 1 s
  before it lands, 70 and blind 100), Sandstorm (7 m on you for 8 s: blind 10 a second, slow, Scour's damage, Solar
  storm's strikes), Desert's wrath (a 10 m vortex pulling at 2 m/s, bosses never), Quicksand (a 3 m pool that roots the
  lowland and midland 2 s, slows everything; Frozen sand's frost), Burrow (8 m under any land's sand, untouchable and
  unseen, a fin of sand over you, a burst where you come up; "needs sand" elsewhere), Sun strike, Blazing arc (220
  degrees, 3.8 m, charged 5 m) and Noon strike (its column falls on the struck point 0.9 s later) as Wildbrands. Fields
  are `AWRSunField`, columns `AWRSunColumn` (replicated; every hit through ApplyHit). **Passives**: Blind chance, Brilliance,
  Searing light (and its gold sparks), Sun-forged, Zenith (+15% at 90% Radiance), Gleam (a parry blinds 100, bosses 60:
  the white starburst), Heat shimmer (the roll's distance and i-frames), Scour, Shifting sands, Dawn. **Combos**
  (combos.json): Desert heat, Supernova (both bars full: a blinding fireball, 90 s), Thunderflash, Solar storm, Sun garden
  (Sanctuary blinds enemies), Frozen sand, Sun and snow (a shatter on a blinded enemy flashes), Dry blood, Scarab swarm
  (Corpse explosion lets out biting scarabs); Desert bloom, Desert graves and Sacrifice to the sun are listed but not built
  (the raise-from-sand and the quick-slot action need their own work). **The body**: the skin's Sun stage (the hero
  lane's pattern) plus the flecks' stored glow at night by Radiance and the full-Radiance warm glow. Proc fx and sounds
  as rows (`sun.*`, stand-in sounds).
- **Desert (1): the land and its places** (cooks/biome-desert.md 2.1-2.7; plan build-biome-desert.md 2.1-2.7).
  `node Tools/terrain/desert.mjs` writes `Data/lands/desert.json` + `desert_ground.bin` (a 4 m heightfield land, 711
  samples, priority 2) and `Data/desert.json` (read by `FWRDesertData`, `WRDesertTypes.cpp`). Where it sits: belt B2 west
  of the Spine, between the Fungal plateau (this land cuts its south strip's beach and sea below y -7,800; the seam row is
  the Fungal's own heights), the Storm land's north coast (hidden south of y -5,652, its row the Storm's heights) and the
  Ocean land's Spine (this land cuts the Ocean's west sea west of x -280; the Spine face and Saltgate's floor are its
  seam); the cook's coordinates go through the Ocean's own `Y()` and an `X()` that puts the cook's Spine foot on this
  world's. **The ground is the language** (cook 1.3): every rule reads a surface class written per sample (soft sand
  rippled gold, slipfaces on the west lee of every crescent dune, hardpan white with polygon cracks, wet sand on the shore,
  sabkha salt with 30 brine patches, 16 quicksand pockets at the Mother Dune's foot, the wadi bed and the oases, oasis
  clay, wadi gravel, red rock mesas with slot canyons, the Glass Canyon's glass, the Wall's scoured pavement and drifts).
  **Five sub-zones**: D1 the Wreck Strand (beach, sea-grass dunes, sabkha), D2 the Erg (barchans 15-30 m: 11-degree east
  backs, 32-degree west slipfaces, horns downwind; the 140 m star Mother Dune with three razor arms), D3 Oasis Reach and
  the White Pan (the dry river's wadi falling from the Pan to the sea; the Pan a flat basin at 26 m, the dry lake the
  river died in), D4 the Red Canyons (two mesa levels 60-95 m, slot canyons, the two flood wadis from the Storm rib's river
  gaps: the west one to the sea, the east one into the main wadi, ledges and rope points every 40 m), D5 the Haboob Wall
  (scoured pavement rising to the Fungal dunes). **Eleven landmarks + the Leviathan Ribs**, code-built at real scale on the
  forest's FBuild with the Desert's kit (`WRDesertLook.h`, `Ds*`): the Haboob Wall (300 m of billowing dust along the D5
  line with lightning inside, `desert.haboob*` fx), Ahrumet's Gnomon and the Sundial Plaza (the kerb half drowned in drift,
  radial joints, the hour ring, the bronze meridian strip to the throne, a 30 m basalt obelisk with its bronze-lined
  sighting hole and gold pyramidion), the Broken Sun Gate (two 22 m battered pylons with torus and cavetto and winged sun
  discs, the fallen lintel, the court and colonnade stumps, the Great Shrine's dais), the Mother Dune's ventifacts, the
  Whale Road (nine whales of 20-40 m), the Keelyard (twelve sun-barques stood on end), the Tall Wreck (a 32 m
  three-master heeled 30 degrees on a crest), the Fallen Hour (a 12 m head on its side, ropes and tow-sledges), the
  Needles (34 hoodoos to 80 m, kite nests, two rope bridges), the Glass Canyon, the Sevenfold Wells (a qanat tower whose
  bucket wheel turns, channels to seven pools, a garden wall with four gates, 40 palms) and the Saltkin serpent's 90 m
  skeleton at Saltgate's mouth. **35 hand sites** S1-S35 each built as the cook's table says (no actors, the story by
  placement). **Kit POIs** (wrecks, caravans, gnomon stelae on the old roads, ruins, outcrops, creature set-pieces, seeps,
  slot overhangs, bones): the cook's counts per sub-zone, the 125 m route rule along the wadi road, the Avenue, the caravan
  routes and the stelae road, and a fill so no ground is over 176 m from a place. **Decoration** in 250 m cells:
  sea-grass, driftwood, shells, saltbush, date palms along the wadi, reeds, umbrella thorns, prickly pear, red rocks,
  pavement pebbles, hoodoo stubs. **Four caves** (the Nitre Caves, the Qanat's shaft and half-flooded tunnel, Ssarrok's
  Hollow, the Sieve Throats down 40 m) and **the Hourhold**: 20 rooms 14-60 m under the Erg joined by dressed galleries
  (the Hall of Hours' 24 pillars and its light shafts that follow the real sun, the Cistern Below, the Barque Dock's 30 m
  sun-barque, the Ushabti Hall's 200 figures, the Weighing Room's balance, the Mirror Gallery's four mirrors on pivots and
  the Sun-Vault's sliding door, the Drowned Stair, the Seventh Vault, the Collapse, the Workshop of Faces, the Lens Forge,
  the Spring) and its three ways in: the Lee Door (its sand slumps away for 3 game hours after every Sandstorm block),
  the Jaw under the Fallen Hour, the Dig (hold E 4 s with a pick or shovel; once for the server). **What you take**: 384
  nodes (60 sunstone on sun-facing rock and the Pan's cracks, glowing at night with the day's stored sun; sunglass, rock
  salt, nitre, copper, tin and iron in the caves and the Hourhold, clay, red ochre, gold flakes, acacias, date palms that
  give palm fibre, prickly pear; the wild crops give their seeds), 25 spots and 13 caches from the sites (lore, the lens
  recipe's two holders, the blue hands' rubbing, the Gallery's mirrors), the kit's chests, the seven tilted stelae's
  caches (hidden until the Long Noon), 43 Scarab digger mounds (hidden until a tamed Scarab finds them), run-back stones
  at the plaza and the Spring. Server state replicated on `AWRDesertState`; everything that moves is a function of it and
  the clock. New items: sunglass, nitre, acacia log, palm fibre, red ochre, gold flakes, bat guano, radiant essence, the
  rubbing, the sun-disc key, 12 lore pieces. Tuning `desertWorld`; world.json `biomes.desert`.
## Unreleased: the taming cook pass (taming, mounts, breeding)

Aaron (lane cookpass-taming: research, grades, requirement ledger and plan in showcase-notes cooks/cookpass-taming.md;
a cook pass on build-taming's work). Compiled (editor target); not played here (Aaron and the testers test, F8).

- **Taming (3): the Tusker talisman, tames in the rifts.** The Tusker ivory talisman's +15% carry is live on your
  haulers (its recipe no longer waits). Tames from other lands lose max health in a Wasteland rift zone as their riders
  do (the Wasteland's own don't; Hollowplate halves it) and mend 5% of it a minute once 30 s clear of the rifts (tuning
  `tame.riftMend`); a tame the rift empties is Unmade (its collar falls).
- **Taming (2): every land's barding works; the seat's ease holds on a client.** The land barding sets that came live
  with their lands promised passes nothing gave: now Stormweave grounds storm strikes, Heartguard walks thornbrake,
  Skysilk ignores gusts, Wardcloth crosses ward fields, Mirehide wades bleedwater, Reefmail takes its mount to 40 m,
  Gravecloth halves corruption and Hollowplate halves rift drain, for the mount and the rider on it (one hook: a mount's
  barding answers the hazards' own gear checks, tuning `tame.bardingPass`). A fallen tame's collar goes to its owner's
  bag when they're on (else an owner-only bag that lasts the 72 h, and a Souls death no longer takes it). The ease onto
  and off the saddle now catches the move whichever arrives first on a client (the attachment or the jump), and a
  gallop isn't mistaken for a jump.
- **Taming (1): riding, names, the stable, the way back.** **The rider sits**: a seated pose over the idle (hips flexed
  72 degrees, the thighs opened round the mount's back by its width, the shins hanging under the knees, feet level, the
  torso upright at a walk and leaning 22 degrees at a gallop, the hands together over the pommel on the reins, let go
  while a swing or a cast plays) and the body rides the saddle bone's rise and fall, where before the rider stood
  upright on the saddle (tuning `tame.seat`). **Getting on and off** eases the body up onto the seat from where you
  stood and down beside it over an arc (0.45 / 0.35 s, a leather creak each way) instead of snapping, and you get off
  onto clear ground: the near side, the far side, behind or in front, never into a wall, off a ledge, into lava or deep
  water (off a swimming mount at sea you swim on). **Ride keys follow your bindings** (they were fixed to WASD, Space and
  E). **Call your mount**: the whistle wheel's new wedge; the mount you rode last (else your nearest saddled one)
  answers with its call and runs to your side from up to 300 m (+5 m a bond level), slowing as it comes and stopping
  alongside facing your way; a flyer lands first; not from a stall, not through rock. **Names**: Rename on the card
  (type one, or Suggest from `Data/tame_names.json` by its role: the pad's way, no typing); the name shows over it
  ("Ember · Cinderfang · Lv 24"), on the card, while riding and in every line about it. **Release asks twice** (a
  second Release within 5 s) so a wheel flick never loses a tame. **Every land's element**: a tame's plain bites carry
  its own element and build-up in every land now (shock, soak, decay, mark, bleed, curse, rot, poison; a wind tame hits
  stamina), not only fire and frost (tuning `tame.element`). **Courting**: a pair filling their mating bar dip their
  heads to each other in turn, and a cuddled baby nuzzles. **The tame card has tabs** (Overview, Skills, Gear, Family,
  Packs; click them or LB / RB): its stats as real numbers (health, stamina, a bite's damage, kg carried, m/s, torpor)
  beside the points and multiplier, and its parents by name and level. **The stable** (farm, Hearth 3: wood 40, stone 30,
  hide 6, iron 4): three stalls on a stone sill under a shingle roof with a ridge vent, half doors on strap hinges, hay
  racks and troughs, a hitching rail clear of the gate. Put a tame of yours within 20 m in a stall (it walks in through
  the middle door) and call it out (5 s, it comes out of the gate); 10 of yours a stable, 3 stables a claim; stabled
  tames don't eat, age or count on your tame limit; torn down, it lets every tame out. **The bond token**: a tame that
  dies drops its collar (a braided band with its name plate); a **Dadeni draught** (alchemy table 5: asphodel 2,
  soulstone 1, healing potion 2) and the collar at your Hearth within 72 h bring it back whole (bond, skills, genes,
  name; once). **Soulbond** (Taming 100) is live: Soulbind one tame on its card and at the end it falls for 60 s and
  gets up at a quarter health instead of dying. New icons (collar, draught: 9 KB each, PIL). Not built: the bone altar's
  undead tames, species' own answer calls, riding clips (the seat is a procedural pose).

## Unreleased: the Ocean & reefs (the Tide land)

Aaron (lane build-biome-ocean: requirement ledger R1-R54, build research and plan in showcase-notes
cooks/build-biome-ocean.md, building cooks/biome-ocean.md). Landing in pieces; compiled (editor target) after each; not
played here (Aaron and the testers test, F8). Live from Ocean (4) (`ocean` in `world.liveBiomes`); underwater play is live everywhere water is.

- **Ocean (6): the sea's sound** (cooks/biome-ocean.md 2.15-2.16; plan build-biome-ocean.md 2.15). **Under the water**
  every bed, every tune and every 3D sound goes thick and low through a low-pass (700 Hz, eased in 0.4 s as your head goes
  under; sounds.json `ambient.underwaterLowPass` / `underwaterFade`), while your own breath, heart and the ear pop stay
  clear, and the underwater bed (the sea's hum under the surface) comes up. **Beds** (`land.ocean` by day and night,
  `zone.O1`..`O5`: the strand's long breakers, the shelf's wind and distant booms, the terraces' dry wind, the kelp's
  lapping, the reef's heavy surf). **Music** pools: explore A (the strand and the shelf), B (the terraces), C (the kelp
  and the reef), night, combat, mini, camp, the Saltgate pass, the Drowned Hall's own cue, Thalassor's three phases and
  each mini's, and **the King Tide's layer** over whatever plays on the coast while the sea is out. All stand-ins from
  the repo's recordings (`_standin`; the cook's Eb major 76 BPM cue sheet is for the audio cook). Thalassor drops the
  Coral keep furnishings blueprint through the set rule (its own duplicate row and the Tidebreaker blueprint, which has
  no weapon behind it yet, are gone).
- **Ocean (5): the bosses** (cooks/biome-ocean.md 2.13-2.14; plan build-biome-ocean.md 2.13-2.14). `WROceanBosses.*`:
  `FWROceanBoss` hooks beside the other lands' in the boss machine, `UWROceanArenas` placing them, their rows in
  attacks.json (`bosses.<id>`) and numbers in tuning `bosses.<id>`; all four built in code (`WROceanBeast`). **Old
  Barnacle** in Barnacle Cove: the crab at x2.5 under an upturned newcomer longboat (keel up, planked, barnacle-crusted,
  a mast stub, a dangling anchor), red-and-cream striped legs. Crusher pinch (parryable; 40% into the pinch and hold:
  18 a second for 2 s in the claw, then thrown 8 m), hull slam (a 5 m low ring), debris toss (at anyone past 6 m, the
  landing ring shown the whole flight), shell charge (15 m sideways, a rock stuns him 2.5 s), hull fortress (6 s
  withdrawn: slash and pierce x0.2; below 60% or flanked by two). The hull has its own 1,200 (blunt x2; a plank torn off
  with splinters each 300; he takes a fifth through it); fire on the tarred hull 3 s and he bolts out naked; naked 12 s
  he takes x2 and uses only the pinch and the charge, then runs for a spare shell on the beach: the bronze bell (800, he's
  slower in it) or the Driftback carapace (400, faster): break them first (they take blows) or stand on one and he won't
  take it; with none left he stays naked. **The Siren Eel** in Siren's Hollow: the eel at x5 as a silver oarfish (flat as
  a blade, dark bars) with a crest of scarlet-orange rays that blazes when she sings. Her song only from by a rock island
  (a 1.2 s windup, 8 s): every tame within 30 m (furthest first) leaves the fight and swims to her unless its owner wears
  the Siren scale; every player within 20 m without beeswax plugs is drawn toward her at 250 cm/s; 150 to her crest in the
  windup breaks it and staggers her 3 s. Coil whip (low then high), the second jaw at the water's edge (15 a second for
  2 s, dragged 4 m under, breath held), crest lash, shatter shriek (5 stalactites with their falling shadows). **The Reef
  Tyrant**: the shark at x2.3, bars faded to ghost lines, a white-scarred snout, a Saltkin harpoon still in its jaw hinge
  trailing bronze chain, a saw-notched dorsal; it swims its 3.2 km loop (lighthouse, the Blue Wall, the Whale Fall, the
  drop, the reef, out along the dyke) and rests at the Mooring Stones 13:00-15:00. Slingshot bite (bleed 40), ram,
  thrash-drag (18 a second for 3 s, 15 m down), breach (from below at the surface: the bubble column and thrum first;
  landing in under 2 m beaches it), tail scythe. Harpoon it and tie the line off on a mooring stone: it can't get 10 m
  from the stone (it strains the line); two tethers in under 2 m of water beach it 8 s (x1.3, stance open). Beaten after
  being tied off counts for Hae-won's sixth for everyone in the fight. **Thalassor, the Tide Leviathan** in the Pound: a
  new code-built body, 52 m (a sperm whale's squared ram of a head with white sucker scars and an underslung jaw of 40
  ivory cones, the blowhole off to the left, snapped bronze collar chains, then 38 m of serpent bristling with broken
  Saltkin harpoons, a 9 m fluke, 14 blue-green photophores a side), the trunk waving like an eel's, the jaw dropping. It
  clicks the whole fight; the clicks speed into a creak before every move, and stop dead before a grab. Phase 1 in the
  pool (it lunges out onto the flat and swims back): head ram, jaw snap x3 (the slam's low splash), swallow lunge (inside
  it 3 s at 20 a second, spat 10 m; 150 to its head frees you), fluke slam (the low ring), spout volley (5 lobbed water
  bombs with rings), wave wash, and from 75% the bubble net round the furthest tame (inside it swimmers sink, waders slow;
  then it lunges up through it). A lunge that ends 10 m out from the pool or into a tower beaches it 5 s (x1.3, stance
  open, beam counts double). **The Sluice Breaks** at 50% (it rears 16 m, every photophore flares; the transition move,
  half damage taken): it rams the east sluices, a surge crosses the flat (60 and a knockdown unless you're up high or in
  the air), and the Pound floods 4 m over 10 s (a water volume and a surface on every machine from the replicated burst
  time). Phase 2: breach (a tower in its line is broken: the eight stumps on the flat are breakable props now), undertow
  (swimmers within 15 m pulled in 3 s, swim sideways), coil-snap-lash, and every 30 s it surfaces 4 s to breathe (a 12 m
  plume; the blowhole takes x1.5 and breaks at 1,000: no more spout volleys this phase). The hidden **Maelstrom** (25%,
  with artifacts): the Pound spins, swimmers are drawn to the eye (20 a second there), it rides the vortex wall and rams
  out at a perch every 6 s. **The Sun**: three Sunwatch mirrors stand on the wall's plinths: E takes one and it follows
  your aim; in daylight (06:00-20:00; Overcast half; Sea fog, Rain, Squall none) or with a sunstone loaded (E again, 30 s
  each, day or night) it throws a gold beam drawn on every machine; 2 s of beam on a part (head, blowhole, flank, fluke)
  dries it 20 s (x1.5 damage, cracks and dust), once a part a phase; 3 s of beam breaks the Maelstrom 8 s. A sunstone
  flare's burst is a second of beam; Sun path hits count a second each and take x1.5. The Tide altar and the run-back
  stone 50 m out from the north stair. **Drops**: Thalassor's core, tide essence x5, ambergris, the Coral keep
  furnishings blueprint (the set rule), its head (first kill), the Leviathan pearl (part: a dodge in the water gives back 10 stamina, once
  a second); the minis their trophies, shards and essence. Stand-in voices and every boss sound and effect as rows.
  Not yet: the Tidebreaker (Thalassor's weapon) waits for the boss-weapons cook.
- **Ocean (4): the coast's world, and the Ocean goes live** (cooks/biome-ocean.md 2.8-2.11, 2.19-2.21; plan
  build-biome-ocean.md 2.8-2.11, 2.17-2.19). `ocean` joins `world.liveBiomes`: the coast, its creatures, hazards, shrines
  and Hae-won are in the game (the three minis and Thalassor come with Ocean (5)). **Hazards** (`WROceanHazards.*`): five
  **rips** at the White Strand's bar gaps, an 8 x 120 m lane pulling 250 cm/s out to sea on the falling tide (170
  rising), a pure function of the tide and where you swim so it never corrects; swimming straight against one costs the
  fast stroke double, and out by its side is the way (a line on screen the first time; Hae-won's second quest checks the
  side). Foam and kelp drift out along each lane. The shelf's **blowholes** and the **Spouting Horn** breathe from
  mid-rising to high water, each on its own 9-16 s beat: a 1 s tell (boom, hiss, spray, the rock shaking), then the
  column: 40 and thrown 6 m up (the Horn 18 m); jump just before and you ride it unhurt, and the Horn's ride lands you on
  secret O-S3's ledge. The six **surge channels** send a wave up every ~14 s (a 1 s heave of white at the mouth): 30, a
  knockdown and dragged 6 m out. Two **man o' war fleets** drift a slow loop with the wind on the reef's sea (code-built
  blue-violet floats with their crest and 8 m tentacles, every machine drawing the same); a touch stings 25 + poison 50
  (half on a swim mount or in Reefmail 2), a glassy tick within 6 m; a Squall blows one sail of each fleet onto the
  strand, still stinging. The **air pockets** (the Pearl Cave, the Drowned Hall's dome, bell chamber, King's tower and
  archive) are air over the water: your breath comes back there. **The King Tide** (world.json `events.king_tide`, every
  ~3 real days for a game day): the warning (the sea draws back, every chime beacon rings with no wind, the drowned bell
  under the reef, the flocks lift inland); the Low (-6 m; 3-5 Kelpgrazers and Driftbacks stranded on the flats, tamed by
  offering food three times as fast; 4-6 Reefjaws bound in the deep pools, x1.3 damage taken; Spinecrabs out on the
  flats; nodes x3, pearls x2); a spawning night if the Low falls 20:00-06:00 (glowing clouds off the coral, every
  tidecoral regrows, Glowrays tame twice as fast); the Return (a white line and a roar out past the reef at T-10 min;
  the 3 m bore at T-3 min running in at 6 m/s: below the Tsunami Line 80, carried 20 m, soaked; build pieces below the
  line 200 once; standing above the stones counts for Hae-won's fifth). **Weather** (weather.json ocean): Squall 15
  (rain and gale, surface swimming x1.5 stamina, man o' war inshore, Reefjaws x2 at sea; no lightning on land: the
  open-sea mast strike waits for boats), Sea fog 15 (70 m, most things notice you later), Rain 15, Overcast 15, Clear
  the rest. **Shrines** (world.json): the Great Shrine of the Mirror Pool, the Driftwood Shrine, the Gate Shrine at
  Saltgate's mouth, the Kelp Stack Shrine on its sea stack's top, the Lighthouse Shrine in the broken lamp room (their
  guards per the cook). **Hae-won**, the Harpooner (the Tide path's master, on the porch of her stilt house over the
  Mirror Pool) and her six quests: First breath (the Kelp Cathedral's anchor stone: the Diver's draught), Sideways (out
  of a rip by its side: swim fins), A line on the reef (5 harpoon or thrown-spear hits under the water: the Whaler's
  harpoon), Coral for the helm (show her a diving helm: shark-tooth arrows and the helm's recipe), The stones say (all
  nine tsunami stones read, or above their line when a King Tide comes back: the sun mirror), Tie the Tyrant (the Reef
  Tyrant beaten after tying it off: Hae-won's knife; it completes once the Tyrant lands in Ocean (5)). Event quests sent
  as "event" + the deed now count everywhere (the Fungal caverns' two were silently not counting). **Secrets** O-S1..S6
  as spots, each open only in its window or with what it needs, and saying so plainly: the bell window (midnight at the
  top of the flood), the dyke vault (the sluice key), the Horn ledge, the mirror line (noon under a clear sky), the
  Lazarus ledge (your Pearldiver finds the way), the Low Gate (the King Tide's Low). **Tie-off**: E by a mooring stone
  with a harpoon line in hand ties it off (the struck body is tethered). **Gear** (items, recipes): the Diver's draught
  (breath x3 for 10 min), swim fins (swim +25%, land -10%), the Whaler's harpoon (a thrown spear on an 18 m line: small
  things are hauled in, anything can be tied off), beeswax plugs (no Siren lure, no Gullraider mob call), the sunstone
  flare (a 2 m burst of daylight: blind 25), shark-tooth arrows (bleed 20, x1.1 on swimmers), the eel-skin waterskin (60),
  the pearl ring (+3% Flow, +5 s breath), the pearl circlet, the diving helm (breath x2, no crush to 40 m), Hae-won's knife
  (soak 20 a hit in anyone's hand), the harpoon head, gold coin, sun disc, ambergris. **Life skills**: Diver (breath
  x1.5), Pressure (15 m more before the crush), Current rider (a rip carries you x1.5, against it x0.5), Salvager (sunken
  caches within 40 m show you a thread of bubbles) and Abyss diver (no crush) are live. **The Lazarus Fish** bites only
  in the Lazarus Deep's black water (fish.json water `lazarus`). **No building in the Drowned Hall.** **Invasions**: the
  Ocean's side through Saltgate (Old Barnacle leading Spinecrab ranks and Gullraider flocks), dormant until the Desert
  is live. Effect and sound rows for all of it (stand-in sounds `_standin`).
  Not yet: the minis and Thalassor, the Pound's mirrors and sluices (Ocean (5)); fire popping a man o' war's sail (no hook
  for fire on a non-character yet: LATER-COOKS); the Ocean's portal slots stay hidden (no Ocean fold kit).
- **Ocean (3): the sea's creatures** (cooks/biome-ocean.md 2.12; plan build-biome-ocean.md 2.12). No sea pack exists in
  `Content/`, so all eleven are built in code the Sky's way (`UWROceanBeast`, `WROceanBeast.cpp`): every vertex keeps its
  part and its place along the body and is re-placed each frame from how the real animal moves. The **Reefjaw** (a 6 m
  tiger shark: bronze-grey back with faded bars, bone belly, tall dorsal and scythe tail beating side to side, the wave
  growing toward the tail; its goblin-shark jaw a part of its own that shoots 120 cm forward on the bite), the
  **Tidecoil** (a 3.5 m honeycomb moray ribbon undulating its whole length, a pale inner jaw in the throat, a row of
  organs down each flank that light blue-white on the shock pulse), the **Spinecrab** (a spined rust-red dome on eight
  stilt legs stepping in alternating fours with planted feet, a crusher and a blue-green club that cocks back; buried,
  only its blue eyestalks show; it turns over on its back when its guard breaks), the **Gullraider** (a dark skua body,
  white wing flashes, hooked bill, a scarlet-orange throat pouch that swells), the **Wavecharger** (an upright seahorse
  head on ring plates, olive-gold fronds streaming, a dolphin's tail stock that drives and curls), the **Driftback** (a 5 m
  leatherback dome with seven keels, a barnacle-and-weed garden, long rowing flippers), the **Glowray** (a 4 m midnight
  manta whose wings ripple root to tip, curled horns, a barb that arches; blue-green spots brighter at night and pulsing
  in sync in a ring), the **Pearldiver** (a sea otter on its back, silver head, its stone on its chest, cracking shells),
  the **Kelpgrazer** (a 7 m bark-hide sea cow, whiskers, a forked fluke stroking up and down), the **Tidefowl** (a
  puffin-billed eider) and the **Ripplefin** school (thirty silver-and-green mackerel darting, balling into a spinning
  sphere when a Reefjaw is near). All die belly up. **Their state** rides a replicated `UWROceanBody`; **their brains**
  are `FWROceanAI` hooks beside the other lands' (wild, tame, row allowed / started / strike, resolved, incoming,
  damaged, died, mash): swimmers hang at neutral buoyancy (`bHoldDepth` on the movement) and steer up and down inside
  their depth band, never into the shallows on their own, and circle at the edge facing someone on the land. The
  Reefjaw's slingshot bite; its thrash-drag (caught, you're dragged 6 m down at 15 a second for 2 s with no breath back;
  100 damage from allies or mashing frees you); the breach strike (only at someone at the surface: a bubble column and
  thrum 1 s ahead, then up through it); the surf lunge (at someone on the wet sand: out of the surf and stranded belly
  up 1.5 s taking x1.3). The Tidecoil waits in its hole and strikes what comes within 4 m; its second jaw drags you to
  its hole (any ally's blow, fire or a shock frees you); the shock pulse hits everything in the same water, its own kind
  too (2 m on land; grounded immune); at low tide it slithers to another pool. The Spinecrab buries and bursts out at 5
  m; its shell guard (when you swing at its face) takes x0.3 from the front and 60 stance flips it 2 s (x1.3); the club
  snap's cavitation pop is a second hit 0.06 s behind (a block takes both; under water it hits all within 1.5 m). The
  Gullraider's snatch takes one food or fish stack (5 at most, never gear) and flies it to the Gullery: kill it within a
  minute and it drops where it falls, or it's left in a bag at the colony; its mob call brings every Gullraider within 50
  m, once a fight. Wavechargers, Driftbacks and Glowrays defend each other (the herd's click chorus); Glowrays flare
  (blind) and glide off. **Taming**: the Reefjaw's path magic (10+ Tide points, in the water with it, knocked out or
  soaked under 30%: hold E 4 s, 30 Flow, then feed it; the prompt, the hold and the release work like the Prismback's);
  a Wavecharger egg eased out of a sleeping male's pouch at the Kelp Cathedral (hold E 3 s; not sneaking or swimming, or
  one time in four anyway, the herd wakes and comes for you). The Wavecharger is a swim mount that dives (hold Dodge to
  go under to 40 m on your own breath, Jump to rise, nothing held keeps its depth; `diveDepth` / `depthRange` are new
  creature fields). A tame Glowray shows hostile swimmers' bubble lines within 30 m; a tame Pearldiver brings you pearls
  from clams within 20 m and marks sunken caches within 40 m with bubbles. **Populations** (`UWROceanLife`, tuning
  `oceanLife.tables`): each sub-zone's day and night weights from the cook (2.12.3), about 12 kept round each survivor,
  filled 60-110 m away out of sight (swimmers in water deep enough for them, crabs and Tidefowl on the sand and shelf,
  Gullraiders over the cliffs, Tidecoils in holes on the floor; at night the herd at the Kelp Cathedral, Driftbacks
  hauled out on the Turtle Beach, Glowrays in rings over the reef), taken in when nobody is within 220 m. Beats: hunting
  Reefjaws go for Kelpgrazers and schools; a kill in the water leaves a blood cloud and the Gullraiders come circling it.
  **Rows**: 11 creature rows, 10 attack sets (2.12.2), 14 items (raw fish, sharkskin, shark tooth, eel skin, shock
  gland, crab meat, frond, turtle hide, ray wing, photophore gland, otter pelt, fat, Wavecharger egg, tide essence).
  **Effects and sounds**: 48 effect rows and 54 sound rows for the whole Ocean so far (the water's bubbles, kick,
  splash and shock; the Beacon Stack's fire, surf crests and wash, spray over the Pound, god-rays, drips, gulls wheeling;
  every Tide spell's look and sound; the creatures' moments) and 11 voice families: pieces 1 and 2 named them in code
  without table rows, so they were silent and invisible until now. Sounds are stand-ins (`_standin`).
  Not yet: the minis and Thalassor (piece 5); the hazards, King Tide, shrines, Hae-won, gear (piece 4); nothing here
  shows until the Ocean goes live.
- **Ocean (2): the Tide path** (cooks/biome-ocean.md 2.17-2.18; plan build-biome-ocean.md 2.16). `Data/paths/tide.json`
  (22 nodes: trunk Water bolt / Flow capacity / Soak chance / Wave cut; Undertow, Harpooner and Tidecaller branches; root
  passive Gills), numbers in tuning `tide.flow`, `tide.path`, `tide.combos`, `spells.<key>`, `wildbrands.wave_cut` /
  `harpoon_throw`, `nodeEffects tide.*`, `passives.anchor` / `tow` / `leviathansHook`. **Flow** is a new owner-replicated
  attribute (`Flow` / `MaxFlow`): 100 + 15 a capacity rank; +5 a second moving over 300 cm/s on the ground, +8 sprinting,
  swimming or on a swim mount (Gills x1.5 in water), +10 a dodge or kick-dodge, +15 a perfect dodge (the water ribbon),
  +4 a weapon hit while moving; after 1.5 s under 150 cm/s it drains 8 a second (a rooted cast holds it). A sea-green bar
  under your health that ripples while it fills and flashes when a press is refused (and the `-oldui` bar; Mana's was
  missing there too and is added). **Gills** (main path): no breath limit, crush only past 80 m, x1.25 under water, every
  weapon at full speed and charged heavies under water, Flow x1.5 in water. **Abilities** (`WRAbilitiesTide.*`, under
  `tide` in `ClassForNode`): Water bolt (full speed under water, homes on creatures), Riptide pull (a 30 degree cone pulls
  5 m, +1 m a Drag rank, the highland half way at Drag 3, never bosses, lowland stagger as they arrive), Whirlpool (holds
  lowland / midland, slows the highland, soaks), Maelstrom (a 10 m vortex dragging everything to the eye at 3 m/s, x2 at
  the centre, bosses only soaked), Harpoon throw (a spear on a line, `AWRHarpoonLine`: press again within 1.5 s to reel
  small bodies in; held: Anchor pins the highland 3 s / slows a mini; Tow pulls you to a big body or to any wall, rock or
  hull the harpoon struck within 18 m, i-frames on the last 0.2 s; Leviathan's hook makes it a chain that drags a boss 3
  m with 60 stance and a 1.2 s stagger, once each 30 s), Healing wave (a 6 m wave rolling 12 m that heals every ally it
  passes and passes enemies harmlessly), High tide (a 10 m wave: heals 60, knocks lowland / midland 6 m, staggers the
  highland, 40 stance on bosses), Cleanse (every status and buildup off the ally under your crosshair), Tidal shield (35
  HP on the shared shield layer; it bursts when broken, soaking attackers within 2 m), Wave cut (the Wildbrand). The line
  is a sagging cable drawn on every machine that pulls straight when reeled, glows sea-green when anchored or tied, and
  is a chain with the hook; it breaks past 27 m or at 600 pull. Passives: Soak chance (+10% soak, +6 soak on weapon hits a
  rank), Drowning (soaked survivors lose 6 stamina a second; soaked creatures regain no stance and take +15% stance),
  Heavy water (a deeper soak slow and +1 s a rank, on creatures), Barbed tip, Quick reel (`<node>_cooldown` node effects
  now shorten any ability's cooldown), Mending tide, Current (+6/12/18% speed tags, so the owner predicts them). **Combos**
  (combos.json): Hot spring, Boiling sea, Live water, Thunder tide, Rain, Swamp guardian, Flash freeze, Frozen maelstrom,
  Life tide, Shark harpoon, Tidal dead, Depths. **The body**: Flow brightens the sheen; at full strength it glows faintly
  under water and the gills flutter (`WRTidePath::ApplyLook`; the skin's own Underwater switch opens them). Paths screen
  colours for Tide (and Arcane's, which fell back to Fire's), deeds count water hits as Tide's (and arcane as Arcane's).
  Node glyphs for all 22 nodes and the path's wave mark (`Tools/ui_glyphs.mjs`, 45 PNGs). The harpoon is modelled in code
  (`code:harpoon` / `code:harpoon_chain`); a modelled shot striking the world now calls its `OnLanded`.

- **Ocean (1): diving, breath and depth (every water), the tide, the coast's land and its places.**
  **Underwater play** (`WRUnderwater.*`, numbers in tuning `underwater`): a new rebindable **Dive** (Left Ctrl; on a pad,
  hold Dodge) sinks you, Jump rises (hold: surface fast), neither holds your depth; under the surface you swim where you
  look. Dive / Rise ride the saved move's two spare engine bits, so they are predicted and the server agrees
  (`UWRCharacterMovementComponent::PhysSwimming`: neutral buoyancy wholly under). 300 cm/s, fast stroke 450; Gills x1.25;
  worn `swimSpeed` gear. The Dodge in the water is a 400 cm 3D **kick-dodge** (i-frames 0.2 s, 14 stamina) under the
  surface, a 250 cm surface roll on it; a tap kicks, a hold dives. **Wading** 60-140 cm: run x0.85, no sprint over 110 cm.
  **Breath and depth** (`UWRDiveComponent` on every survivor; server-run, owner-replicated): 30 s + 0.6 s a Swimming level,
  Diver x1.5, helm / Reefmail's four / Deepbreath x2, the Diver's draught's 90 s floor, the pearl ring's +5 s; refills in
  3 s at the surface or in an air pocket (any non-water volume over the water); out of breath 10 a second through
  `ApplyHit`, no floor; below 25 m (40 helm / Pressure, 80 Gills, none Abyss diver) 5 a second. The HUD draws a thin
  sea-green arc round the crosshair under water (pulsing below a quarter, with a heartbeat), the depth beside it past 10 m
  (coral orange past your limit), the screen's edges darken with each tick; a pressure hum past 20 m, an ear-pop at 25 m,
  a gasp coming up. **Under-water rules**: thrust weapons full speed, others x0.8 (Gills none), no charged heavies for
  greatswords / hammers / axes, guard stamina x1.2, bows won't draw, crossbows half range, bolts x0.6 (Tide's 1), fire x0.3
  and no burn, lightning spreads 4 m through the same water (`WRUnderwater::SpreadLightning`), only `underwater`
  Wildbrands (Flurry, Skaldrith's soul art, Static strike flagged); the camera comes in to 2.5 m; the sky's fog becomes
  the water's murk round your head (40 / 25 / 12 m by band; halved in Sea fog) with a blue grade. F1 page: "In the water".
  **The tide** (`WRTide.*`, tuning `tide`): highs 00:00 / 12:00, lows 06:00 / 18:00, +-1.2 m, plus the King Tide's -6 m
  (`AWROceanState`, replicated); local to the B2 coast's box with a 300 m ramp; every machine works it out from the clock.
  The swimmable tide water is a set of water + air volumes over the box (`AWRTideWater`, priority over the still sea)
  that follow the level; the drawn sea's tiles there are re-placed (`AWRSea::SetRegionOffset`; its squares 32 m over the
  box). `UWRWaterSubsystem`'s depth, drinking and the creatures' wading read the tide (`WRTide::SurfaceAt`).
  **The land** (`node Tools/terrain/ocean.mjs` -> `Data/lands/ocean.json` + `ocean_ground.bin`, a 5 m heightfield): the
  strait between the Sky's Chain Cliffs and the Storm crown / Swamp estuary: the Spine's east face and Saltgate's 60 m
  saddle, the Salt Terraces' six benches, the Pound's basin and the Sluiceway, the Blowhole Shelf with its raised beach,
  the White Strand's beach, dunes and bars with rip gaps, the Gate Inlet's salt marsh, the kelp shelf to 25 m, the reef
  crest, the slope, the Blue Wall to 70 m and the open floor; the sea floor is drawn and coloured by depth, so the water
  reads its depth; three camps above the Tsunami Line. The cook's y is squeezed into this world (`Y()` in the script).
  **The places** (`Data/ocean.json`, `FWROceanData` in `WROceanTypes.cpp`; built in code on every machine by
  `UWROceanWorld` / `AWROceanScenery`: `WROceanScenery.cpp`, `WROceanSites.cpp`, kit `WROceanLook.h`): the eight
  landmarks (the Pound's ring wall with its gates, stairs, towers and mirror plinths; the Drowned Colossus; the Beacon
  Stack with its stair, causeway and clan-lit fire; the Spouting Horn and its ledge stack; the Dyke Head with the empty
  key-hook and the vault door; the Long Dyke's eleven stumps and wall top at -1 m; the leaning Reef Lighthouse; the Bell
  Tower), 27 hand sites, 146 kit POIs (a POI within 241 m of anywhere), five caves (the Saltkin Mine's adit, the Blowhole
  Grotto, the Pearl Cave's outcrop, Siren's Hollow's headland, Lazarus Deep in the Blue Wall), the Drowned Hall's eight
  rooms (the bell that swings, the sand that slumps at the Low Gate), the nine tsunami stones on the 6 m contour, and the
  decoration rule in 250 m cells (sea-grass, driftwood, tamarisk, terrace walls, barnacle bands, kelp to the surface,
  coral, clams, urchins, mangroves). Moving parts glide on every machine from the clock and `AWROceanState` (the Tide
  Clock's float, the drowned bell, the Low Gate's sand, the Pound's sluice leaves, the beacon's fire).
  **What you take** (server): 495 nodes from `ocean.json resources` (tuning `oceanWorld.nodes`): tidecoral breaks off
  only under the water and whitens 10 s after the tide leaves it in air (`UWROceanCoralLife`), the wild sea kelp and sea
  grapes give their seeds, x3 in the King Tide; 224 giant clams opened by hand (clam meat, a pearl 2%, x5 with your
  Pearldiver near); keepsakes, the Sluice key, the drowned bell's rope, the nine stones to read, the Beacon Stack's fire
  (4 wood, for your clan), the Tidefowl burrows; caches at the larder, the fallen span, the market, the Impaled Ship, the
  barque, the Whale Fall and in every castaway hull, drowned house and sunk barrel (`AWROceanSpot`, `world ocean spot`).
  New items: pearl, sea-glass, driftwood, oyster, urchin, clam meat, Tidefowl egg, kelp ash, bronze scrap and sheet, iron
  nails, gold leaf, glass, the worn harpoon, the Sluice key, eight lore keepsakes, four recipe scraps; recipes glass
  (kelp glass), bronze from scrap, iron from nails. The `ocean` row in `world.json biomes`.
## Unreleased: the Fungal caverns (the Spore land)

Aaron (lane build-biome-fungal: requirement ledger R1-R60, build research and plan in showcase-notes
cooks/build-biome-fungal.md, building cooks/biome-fungal.md). Landing in pieces; compiled (editor target) after each;
not played here (Aaron tests). Live from Fungal (1): `fungal` is in `world.liveBiomes`.

- **Fungal (7): the caverns' sound** (fungal 2.15-2.16; plan 2.15-2.16). `Data/Audio/sounds.json`: 73 events (the
  hazards' tells and flashes, lines, springcaps, cap-falls and the knockers, the mole's knocks, the vents, the webs, the
  creatures' and bosses' signature moments, the Spore path's proc feedback: the plant's wet tick, the popping chain),
  16 creature voices (the nearest family, pitched), 8 beds and 19 music pools, all stand-ins from the recordings we have
  (`_standin`: the fused calls and the 70 BPM Bb Locrian cues of the cook are a later audio job). `UWRAudioDirector`:
  the caverns' beds by zone (`fungal.F1.day` / `.night`, `fungal.F2`-`F5`, `fungal.HG`), the Flue's roar on the Fall
  Field, and its one **planned silence** (stepping onto the Fall Field the music fades over 4 s, 20 s of only the roar,
  then `sub.fall_field`); the cues by zone (`explore.fungal.surface` / `.a` Gardens / `.b` Halls / `.c` Webs / `.d`
  Grottos, `night.fungal`, `combat.fungal`, `mini.fungal` and each mini's, `boss.mycelorn.p1`-`p3`,
  `sub.heart_grotto`, `camp.fungal`) and `layer.sporefall` (the Heart pulse over whatever plays). Compiled; not played.
- **Fungal (6): the Spore path** (fungal 2.17-2.18; plan 2.17). `Data/paths/spore.json` (trunk, Rot, Colony,
  Symbiote; gates 0/5/10/14/17/20), tuning `spores`, `sporePath`, `spells.*` (sporeCloud, detonate, blight, sporeling,
  fungalBehemoth, sporeArmor, livingColony, infectStrike), `wildbrands.infect_strike`, `nodeEffects spore.*`. **Spores**
  (`UWRAttributeSet::Spores/MaxSpores`, cyan #39E6C2 bar in both HUDs, a refused press flashes it): 100 +20 a capacity
  rank, 5 a second, x2 in any spore cloud, +3 a detonated Spore. **Planted Spores** (`UWRSporeHost`: replicated counts by
  planter, cyan nodules on the body every machine sees; 20 s, 10 a target +1 a capacity rank, 5 on players).
  `WRAbilitiesSpore.*` (registered under `spore`): Spore cloud (a real `AWRSporeCloud` that plants 1 a second),
  **Detonate** (free with Spore cloud: its own quick slot; the chain of bursts 0.05 s apart, 8 + rot 10 each, x0.6 on
  players), Infect strike (Wildbrand, plants 3), Blight (a rot cloud spreading 4 to 7 m), Sporeling (60 HP, bite plants
  1), the Fungal behemoth (a new `fungal_behemoth` row on Grimjaw's heavy biped rig with a cap for a back: slams plant 2),
  Spore armor (-15%), Living colony (a walking spore cloud: plants, poison, heals 1.5% an enemy). `WRSporePath.*`:
  Immune system (main path: no poison or rot, so clouds are harmless; `Passive.ImmuneSystem`), Rot chance, Decay (rot's
  damage), Corrode (x1.15 on the rotting, bosses 1.10), Lingering (rot's seconds), Burst, Pandemic (rot jumps every 2 s,
  threads shown), Swarm (cap 1 + a rank, 6), Host (a free sporeling out of a Spore-carrying kill), Hive mind, Thick
  spores, Regrowth, Toxic blood, Adrenal, Bloom body; summons are never tames (not counted, not saved, no body, not
  counted for boss HP). Combos (`Data/combos.json`): Spore drift / Plague wind, Combustion / Ash colony, Static cloud /
  Neural spores, Mycorrhiza / Forest floor, Hardy colony / Ice bloom, Infected wounds / Blood colony, Bog / Swamp colony,
  Fungal puppets (the Necromancer's raise) / Endless colony; Arcane and Void pairs shown only. The Sporestalker's
  **path magic** (hold E 4 s with 10+ points and 10 Spores on one knocked out or under 30% and rotting: your Spores
  burst softly in its stalks, a full narcotic dose). The body (`WRSporePath::ApplyLook` over the existing freckle
  pattern): the freckles glow only in the dark, the slow 0.3 Hz wave at full strength, blazing in Living colony; the
  path's colours in the paths screen; 47 glyphs (`Tools/ui_glyphs.mjs`, rendered for the new ids only). Compiled; not
  played.
- **Fungal (5): the bosses** (fungal 2.13-2.14; plan 2.13-2.14). Rows in `creatures.json` and `attacks.json` (39
  moves from the cook's tables), numbers in tuning `bosses.<id>`. `WRFungalBosses.*` (`FWRFungalBoss`, hooked beside
  the Crystal caves' in `WRBoss.cpp` / `WRBossCombat.cpp`; `UWRFungalArenas` places them when a survivor is within 300 m)
  and `WRFungalBossLook.cpp` (the arena things and the bodies, built in code). **The Sporemother** (Kilnmite rig x5.2:
  the translucent abdomen with its glowing eggs, the cap crown, the spear hypostome) in the Brood Doline: six wall sacs
  (`AWRFungalBossProp` sac, 150 HP, any fire kills one) hatch 3 Mycomites every 25 s, each burnt sac staggers her; the
  Egg toss lobs two sacs that hatch 1.5 s after landing (duds once every sac has burnt); the Brood press pins you 2.5 s
  with four mites latched (150 on her abdomen frees you); her Spore vent is a real cloud: light it with her in it for 200
  and a 3 s stagger. **The Rot King** (Rimeshell rig x5.6: the 3 m crown of black caps dripping slurry, the bronze hooks)
  in the Steeping Vats: the full vats and the pools he spills (crown slam 6 m, Rot spread 8 m, the Shell spin spreads
  them) heal him 1.5% a second and rot you; frost freezes a pool 15 s, frostbite stops his healing 12 s, a wind hit dries
  his crown 10 s (no new pools); the Rot belch's five ticks; the Engulf (150 on his head frees you). **The Weaver**
  (Kilnmite rig x2.6, eight 4.5 m banded legs, mirror eyes, the constellation abdomen) walks her 1.8 km loop of the Webs
  at night, rests 4 min at the False Sky and sleeps hanging there by day; she webs where she settles (a sheet every 10 s,
  up to six): x1.6 on web, x0.6 off it and no Starfall drop; fire burns web (a flashed cloud all inside it), wind tears 8
  m (`WRFungal::Wind`, the Gale flask); the Web volley's three nets, the Starfall drop out of the dark with its thread
  tell, the Cocoon (fire frees you; else hauled up and dropped 6 m); she eats Glowgrubs and Caplings and kills
  Sporestalkers she meets. **Mycelorn** (all code: the 24 m amber stipe round a titan's ribs, the summited host, the torn
  veil, the 36 m cap, its gills lit or dark, a hundred swaying tendrils and twelve bone-clubbed ones, the Heart-seed beating
  35 a minute): rooted in phase 1 (Club slam, Lash x3, Gill spores that leave clouds, the Spore mortar's pods, the
  Sporeling wave, the Engulf with its five bursting Spores, the Summit call on the furthest tame from 80%); the **choke**
  (its own cloud pushed onto it by wind in the last 2 s: 4 s stagger, stance open, the cloud spent); four **Draught gates**
  (plugs of 600 HP, fire x2: broken, a 12 m lane pushes every cloud in it 15 m for 10 s; they regrow in 45 s); its gills
  (1,500: torn, no breath 30 s); weak to Wind x1.5. The Sinking Root at 50%; phase 2 it travels the veins (the shortest
  path lit, `FWRFungalData::VeinNodes/Edges`) to a fairy ring and erupts (into its own cloud: a 5 s choke), Root lash
  line, Erupt-spin-slam, twelve mound pods exhaling its clouds every 20 s (burnable); hidden phase 3 the Whole Cavern
  every 8 s (lines web a third of the floor, the walls exhale, cap-falls over three of you, the elders puff), the
  Heart-seed shown (x1.25), Lash x4. Run-back stones, summoning stones on the caverns' floors, the Spore path altar on
  the south terrace, the Hive spore part (a kill bursts in a 4 m cloud your party breathes unharmed). 14 new `fx.json`
  rows. Compiled; not played.
- **Fungal (4): the ten creatures and the Sporelings** (fungal 2.12; plan 2.12). Rows in `creatures.json` (biome
  `fungal`, element spore, resists spore_cloud / foul_air; rigs borrowed and recoloured through `M_CreatureTint`):
  **Sporestalker** (Rimeclaw rig), **Rotmaw** (Rimeshell x2.6), **Mycomite** (Kilnmite x0.5, 25 HP swarm),
  **Webspinner** (Kilnmite x1.9), **Tunnel toad** (Magmaw rig, mount), **Shroomback** (Basaltback rig, hauler 220),
  **Truffle mole** (Pumicehog rig), **Glowgrub** (Rimeshell x0.45, light), **Capling** (Snowpuff rig), **Puffball** (its
  whole body in code) and the bosses' **Sporeling**; 30 attack rows in `attacks.json` from the cook's table (2.12.2).
  `WRFungalCreatures.*`: `UWRFungalBody` (replicated flags + the look built in code in `M_FungalGlow`: the stalk crest
  with cyan tips that swells for the volley, the gill ruff that flares for the bite, the Rotmaw's cap mound and its
  glowing deepcap lure, the mite caps, the Webspinner's eight 4 m banded legs stepping with its speed, eyes and dotted
  abdomen, the toad's throat sac, the oyster garden and club tail, the star nose, the grub's lantern and light, the
  Capling's spotted cap, the floating Puffball sac and tendrils) and `FWRFungalAI` (hooked beside the other biomes' in
  `WRCreature.cpp`, `WRCreatureCombat.cpp`, `WRStormCreatures.cpp`, `WRCharacterCombat.cpp`): ceiling hunters (they
  climb a cave hall's roof, drop on what walks under with grit or a bright thread as the tell, climb back after a fight);
  the Rotmaw's **mound** (reach for the lone cap: the lure snap's 1.5 s hold; near the mound: the burst; strike the lure
  from range and it comes out stunned 2 s, credit for Ilmari's "The lone cap"; its belly x1.3 while it rears); the
  **latch** (one per limb, four at most, x0.9 speed each through `WRFungal::MoveScale`, drain and poison a second, off
  by a roll half the time, fire, a hit or its death); the **stalk volley** (five lobbed pellets, each a 2 m cloud), the
  **web net** (webbed 2 s, mash frees sooner), the **croak blast** (lines within 8 m snap, the colony rallies), the
  **leap slam** (12 m, a low ring: jump it), the **rot belch** cone (four ticks); reactions (the spore shake, the
  garden puff and the Shroomback's tuck, the Glowgrub's flash, the Capling's puff, the Puffball's 6 m burst, at once in
  fire); warm light draws Mycomites from twice as far (twice again in the bloom); a Webspinner answers a twanged line;
  the Shroomback's garden halves spore buildup within 3 m; the tamed mole senses ore, deepcap and the hidden doors within
  15 m, knocks toward them and marks cap-falls 10 s early; the toad's held Jump charges its leap (0.6 s), a ridden
  landing slams 3 m. Living beats: Rotmaws eat Caplings at their lures, Webspinners reel Glowgrubs up into the False
  Sky, Sporestalkers stalk the Toad Leaps and the croak scatters them, Caplings ride Shroombacks. **Dens**
  (`UWRFungalWorld::StepDens`): the generator's 70 dens fill from their own or their sub-zone's day / night table
  (underground night 22-04) when a survivor is near. 16 new `fx.json` rows, tuning `fungalLife`, the Shroomback's oyster
  cap every 10 min (`farming.json`). Compiled; not played.
- **Fungal (3): the hazards and the Sporefall** (fungal 2.8-2.10, 2.21; plan 2.8-2.10). `WRFungalHazards.*`:
  **spore clouds** (`AWRSporeCloud`, replicated pockets 6-20 m: 12 poison a second after a 0.9 s grace, rot too after 10 s
  without a 3 s break, through the status component; the Immune system, a filter mask or Sporeweave 4 pass, the spore wad
  takes x0.3, the caverns' own creatures ignore them; **wind** moves them 6 m (a wind spell or the Gale flask 15 m) eased
  over 0.8 s, **fire** (a fire blow, a fire bolt's impact, a burning body, a torch or a flame lantern carried in) flashes
  them after a 0.9 s whoomp for 40 + burn 40 to all inside, gone 2 game hours, **frost** settles them as spore snow;
  the Spore bloom swells the caverns' own half again; rain lays the surface's down). **Lure lines** (`AWRLureLines`:
  the Lure Gallery's, the False Sky's low lines over its routes, the web pockets'; beaded glowing threads; a brush twangs
  0.9 s, then grips 2 s, then reels you 3 m up; a light swing cuts 1.5 m, fire burns 3 m, wind tears 8 m; crouch passes
  under; cut lines regrow in a game hour). **Springcaps** (`AWRSpringcap`: creak 0.6 s, jump for 9 m with 20 degrees of
  aim, a 6 m drop fires it, a blow throws its rider sideways, 20 s to recharge, its cap squashing and springing back).
  **Cap-falls** (`AWRCapFall`: loud work within 15 m sets a Webs ceiling knocking, the dust shaft its last second, 70 and
  a knockdown in 3 m, a Knocker idol nearby gives a second more). **Foul air** (no sprint, stamina regen x0.5, 3 a second
  after 20 s, fire spells gutter out). **The Breath vents** on the Fall Field (`AWRBreathVent`: the bronze flap on its pin
  lifts on each exhale; the plume is a cloud column up the shaft; the Sky lane's Aeolith ring now reads the same clock,
  30 s in the Sporefall). **Doors and secrets** (`AWRFungalSpot`, E: the Sealed Gate's web burns 3 s or tears in wind,
  the mole wall a tame Truffle mole digs, the fifth tunnel a mole knocks and a pick breaks, the Ring of Caps' altar
  slides at midnight for a toad tame, the Heart Gate opens from inside, the Seed Vault in the Sporefall). The glow paste's
  cold light, the gold Breath, the **Sporefall** (`WRFungalEvents.cpp`: the 35-a-minute beat, flush patches with their
  clouds over the cape, the Desert strip and the three seeded sky islands, deepcaps on the dead in them, bloomed beasts
  with a cap on the back whose bites carry poison 20 and who burst on death), Spore drift on the surface, the
  invasions' marks, Ilmari's day and night, the Lure Gallery walk for his quest, the Gale flask (thrown) and the spore
  pot's real cloud. 34 fx rows. Numbers: tuning `hazards.fungal`, `fungalEvents`. Compiled; not played.

- **Fungal (2): the caverns' things, the master and his road** (fungal 2.4, 2.7, 2.9, 2.11, 2.19-2.21). Items (47:
  glow-moss, amadou, spore sac, capling cap, the creatures' parts, spore essence, the queen's jelly / king's slurry /
  star-thread extras, Mycelorn's core, the Hive spore part, the Heart-seed husk trophy, the filter mask (head:
  `passSpores`), the spore wad, the poison- and rot-resist draughts (`poison_ward` / `rot_ward`: half buildup, in
  `UWRStatusComponent`), the glow-cap lantern (a cold hip light, tuning `lights`), glow paste, the Gale flask, amadou
  tinder (two torches from one, no flint), chitin arrows, the knocker idol, the Farmer's and Deepfarmer rings, three
  effigies, ten lore keepsakes) and 16 recipes. The server's nodes (`WRFungalThings.cpp`, tuning `fungalWorld.nodes`):
  deepcap only on the dead (a blade doubles it, 3 game days to regrow, the Sporefall x3), glow-moss, amadou, rhizomorph
  (an axe's), silk, saltpetre, the four galleries' ores, wild oyster caps and lion's mane with their spawn, clay, each
  modelled in code (`WRFungal::NodeLook`: a deepcap on the bones it grew from, a hoof fungus on its stump...). Caches
  where the hand sites say (the terraces' tool chest, the caravan's pots, the diving bell...), the summited's leavings,
  the four secrets' pay, and the lore keepsakes (they refill, so everyone finds one). World rows: the fungal biome, five
  shrines (the Great Shrine of the Garden Doline on its floor, the Ring, Starfall, Cable and Flue Floor shrines with their
  guards), **Ilmari Kask** at the Great Shrine with his six quests (First flush fires from any harvest:
  `harvest_<crop>`; the Lure Gallery walk and the lone cap are the hazards' and creatures' events), three effigies,
  the Sporefall event row, the Spore invasion of the Sky islands through the Flue (led by the Weaver). Weather: the
  surface's Overcast / Rain / Spore drift (Clear the rest); the Spore bloom is the caverns' own clock, not a weather.
  The Stipe Maul blueprint waits for its weapon (the boss-weapons cook): not dropped. Compiled; not played.

- **Fungal (1): the land, the caverns and their places** (fungal 2.1-2.6, 2.20; plan 2.0-2.6). `node
  Tools/terrain/fungal.mjs` writes `Data/lands/fungal.json` + `fungal_ground.bin` (0.97 MB: the Sinkhole Rim, a karst
  plateau of limestone pavement cut by grykes on the Hollow Cape's west half, sea cliffs west and north, the Desert's
  north strip of dunes, the Spine's foot rising to meet the Sky land's ground) and `Data/fungal.json` (the caverns, every
  place, kit POI, resource node, cloud, lure-line hall, springcap, cap-fall, foul-air volume, Breath vent, den, arena and
  secret: read by `FWRFungalData`, `WRFungalTypes.*`). The land is priority 1 over the Sky land's west downs and the
  Flue's outer crown (`cuts`: the boxes' edges lie on both grids and the samples there are the Sky's own heights, so the
  grounds meet edge to edge; the Sky keeps a stepped ring about 300 m round the Flue's shaft, which buries its wall).
  The caverns (57 halls on three levels, 70 tubes, 7 shafts: the Spore Gardens' daylit dolines, the Glow-moss Halls with
  the 400 m Starfall Hall, the Mycelium Webs' chasms, the Flooded Grottos, the Heart Grotto's ten rooms round the Heart
  chamber, and the river passage into the Flue) are one signed-distance union meshed by surface nets
  (`AWRFungalCaves`), drawn in the new `M_FungalGlow` (`Tools/terrain/build_fungal_materials.py`, with `M_FungalVeil`;
  two small uassets): the rock's colour by sub-zone in its vertex colours and its glow (moss carpets, starthread
  ceilings, amber fissures, the teal waterline, the city's membrane) in their alpha. The glow follows the surface clock
  (0.35 at noon, 1.0 at midnight) through shared material instances (`UWRFungalWorld`). The Sky lane's Flue wall leaves
  out the one panel the river's mouth chamber covers (`WRFungal::FlueMouth`); the mesher treats the shaft as air and
  drops the skin that would double its wall. 120 POIs (35 surface, 85 under), every tube within 125 m of a place
  (alcoves). Scenery in code on the forest's FBuild with the kit `WRFungalLook.h` (lathed caps of every species with
  glowing gills, rhizomorph cables with their twist, bracket shelves you climb, the summited Deepfarmers in their four
  poses with the stalk from the skull, knocker idols, glow-cap rings, cap-houses, log beds, granaries, drying racks, jars,
  kites): the three camps on their sinkhole rims with wrecks in the coves and a gully up from each, the Ancestor Cap
  (90 m, its gills lit, the Breath rising from fissures round its foot), the Kneeling Titan, the Ring of Caps, the Fall
  Field and the Kitewright champion, the descents (the Garden Doline's spiral path with cap-terraces, the Brood
  Doline's rope stair on bronze pins, the Springcap Shaft's shelves, the Long Stair's worn treads and knocker idols, the
  Sea Cave, the Great Doline's ledge spiral), S1-S8, C1-C17, the False Sky (about 6,000 beaded starthreads and its milky
  way, the dark star's shelf stair), the Shoestring Bridge (a walkable 3 m cable), the Drowned Granary's nine silos and
  rope bridges, the Sand Falls with the caravan and the ledge behind the middle fall, the Heart Grotto's city (the
  Lantern Street's roofs of the summited, the Market of Hands, the Feeding Hall, the Elders' Tower, the Last House),
  the Heart chamber's terraces, pool and gate frames, and the plateau's juniper, boulders and foxfire deadwood. The
  caverns' lakes, river and flooded crawls are swimmable water volumes. `WRBiome::IsUnderground` is true in the caverns
  (no building, no rescue, dark), the sky light fades under rock (not on a doline's open floor), and the Sky islands'
  air over the cape stays the Sky's biome. The Spore Crossing (a longboat) rows from the island's north shore to North
  Tip's cove. Numbers: tuning `fungalWorld`. No diving exists yet, so the moat and the two sumps are flooded crawls you
  wade (LATER-COOKS). Compiled; not played.

## Unreleased: the Crystal caves (the Arcane land)

Aaron (lane build-biome-crystal: requirement ledger R1-R50, build research and plan in showcase-notes
cooks/build-biome-crystal.md, building cooks/biome-crystal.md). Landing in pieces; compiled (editor target) after each;
not played here (Aaron tests). Live from Crystal (4): `crystal` is in `world.liveBiomes`; the Arcane path from Crystal
(5); the four bosses from Crystal (6). Not built yet: the composed music (stand-ins play), the two crystal shields (no
shield items exist in the game yet), the Prismglass spear's Soul art.

- **Crystal (6b): the three trophy talismans do what the cook says** (crystal 2.13): the Twin shard (`dodgeEcho` 0.4:
  the first blow you land within 1.5 s of a dodge lands again at 40%, `WRArcane::AfterHit`), the Warden's tuning fork
  (`quietSteps` 0.5: your steps, rolls and landings make half the noise, `WRCrystal::Emit`), the Tyrant plate
  (`shieldBonus` 0.2 on Barrier and Aegis; `blockStaminaNonArcane` -0.1 on your guard's stamina if you don't walk the
  Arcane path). They carried older stand-in stats. Compiled; not played.

- **Crystal (6): the four bosses and their arenas' machines** (cooks/biome-crystal.md 2.13-2.14; plan
  build-biome-crystal.md Part 2 P2.3-P2.7). `WRCrystalBosses.*` (`FWRCrystalBoss`, a friend of `AWRBoss` called beside
  the Graveyard's hooks; `UWRCrystalArenas` places them once the caves are built near a survivor), `WRCrystalBossLook.cpp`
  (the machines and bodies), `WRCrystalLyrathis.cpp` (her hall and fight), `WRCrystalBossState.h`; rows in
  `creatures.json` / `attacks.json` / tuning `bosses.<id>`. **The Mirrored Twins** (the Archive of Wards): one real
  knight (the Obsidian Warden rig in crystal plate: hexafoil helm, crystal longsword, kite shield) and the Spar Lens's
  double (a second actor that takes nothing, that you walk through, whose blows can't be parried); only the real one has
  a reflection in the water (its pose copied under the floor, drawn by the new `M_MirrorWater` only where you look at the
  floor; its steps ring the water, the double's don't). The lens hangs in a bronze gimbal under the light vent and turns
  a quarter every 12 s (the chain over its sprocket, the ring swinging and settling): the two trade places along its light
  over 1 s, the real one taking nothing. Three shots on the flaring lens shatter it: one knight while the magazine on
  the north wall lowers a spare down its chain, over the ceiling pulley and along the track (20 s, its counterweight
  rising in the wall slot); after three spares, one knight for good. The double makes every move at the same instant from
  its own side of you (its Mirror lunge converges from the other side); Ward guard throws shots back 3 s. **The Echo
  Warden** (the Hall of the Door): a bronze construct on the Frostantler rig with a tuning-fork head, two ear-horns that
  swivel to every sound and humming back plates; blind, it wakes to a noise and goes to the loudest one within 30 m
  (crouching is silent; a thrown stone pulls it); its Fork slam lands on the noise, not on you; a struck chime stone (the
  four are machines now: they swing, crack and break) sends it to hammer the chime 4 s with its back to you (stance open);
  three hammerings break a chime; Listening holds it still 2 s and a noise then brings the Snap lunge; every blow that
  lands brings the Resonance blast (a low ring you jump). **The Geode Tyrant** (roaming): a Prismback 12 m long with six
  rows of split geodes (basalt rind, violet druse) and a clouded eye, walking a real round along the tubes' floors (the
  Wallow, the Hall of Beams, the Chandelier Gallery, the Guano Mines, the Organ's east end, the Agate Rinds), feeding at
  its stops and resting 2 game hours in the Wallow's hot pool; a row's plates throw shots back from its side until
  strikes break it (a broken row falls away); Mana burn cracks rows instead; the Void passes; a Plate charge into a
  selenite beam shatters the beam and stuns it 3 s (chandeliers already drop on it); Glare (needs a light within 15 m),
  Shard shrug (eight lobbed shards, each landing spot glinting the whole flight). **Lyrathis, the Prism Sovereign**:
  the Ignessa rig at 9 m in violet glass over basalt with a brow gem, a crown of eight spires, ~40 chiming selenite
  needles for hair, a turning heart crystal, wrist blades (parts: a broken one is gone) and a selenite mantle. Her hall's
  eight prisms (calcite rhombs with a phantom inside) stand on bronze turntables 22 m round the dais: a disc on sixteen
  rollers in a carved race, a 48-tooth ring gear meshing a 12-tooth pinion on a vertical shaft up to a lever with a
  hexafoil hand-plate and a counterweight, a ratchet and pawl; strike the hand-plate (blade, arrow or spell) and the disc
  comes round exactly 45 degrees clockwise over 0.8 s, the pinion half a turn, the lever out and back, the pawl home.
  Prism beam: a pale guide thread from her brow gem to the prism nearest her, then three rays fanned 30 degrees out of its
  far face sweeping 40 (`AWRPrismBeam`, traced the same on every machine, stopped by walls, pillars, prisms, the dais; the
  centre ray bends toward the furthest tame); a prism left facing its own shaft throws the light up it and its spire on
  the surface beams. Shard rain (real shadows 1 s), Sovereign's grasp (2 s crush, then thrown; 150 from the others frees
  you), the Prism ward at 85% and 65% (her dome: shots and spells stop on it, blades reach through at a quarter; her own
  split beam turned onto it eats 400 a second; Mana burn x2, the Void x3, a void rift at once; broken: 3 s stagger;
  unbroken it fades at 25 s), the Sundering Flare at 50% (line of sight to her heart: prisms, pillars and the dais are
  cover), phase 2 copies standing on the prisms' lines 15 m out (they repeat her blows 0.5 s later and take nothing; turn
  the prism so the line meets a wall or a prism and the copy goes out for 20 s), Held facet, Glint lance (it stands in the
  floor 20 s as a prism; three hits break it), Prism waltz, Late Shardblade, Double prism beam; hidden phase 3 (artifacts):
  on the dais, a beam turning 30 degrees a second through every prism, all eight spires blazing, a permanent dome that
  breaks only when three prisms are turned to face her within 10 s (8 s stagger). Two run-back stones (the West Door and
  the Severance Door), the Arcane altar on the dais, her summoning stone (effigy: resonant crystal x20 + prism plate x4,
  learned on her first kill), drops (core, arcane essence x5, the Prismglass spear blueprint, the Geode sanctum decor
  blueprint, her crowned head; the Prism heart as her first-kill part and her boss-part chance). Voices, the machines'
  sounds and the bosses' music are stand-ins from the nearest existing recordings and composed tracks, marked
  (`_standin`, `_waitsFor`). **Saga Venn's fifth quest** ("Two of one") is back; a boss kill now counts for everyone who
  dealt their share. **Invasions through the Severance** (`world.json invasions.pairs`): the Crystal caves west led by the
  Geode Tyrant, the Void east led by the Echo, paying arcane and void essence (a pair may now name its own pay). **The
  Lumenhorn's bellow on order**: the whistle wheel's new Call wedge (or its rider): a 40 m noise that calls Echobats and
  holds the crystals in reach lit 20 s. New material `M_MirrorWater` (26 KB). Compiled (editor target); not played.

- **Crystal (5): the Arcane path** (cooks/biome-crystal.md 2.17-2.18; plan build-biome-crystal.md Part 2 P2.9).
  `Data/paths/arcane.json` (22 nodes: trunk Arcane missile / Mana capacity / Mark chance / Runic strike; Ward, Sorcery and
  Rune branches; root passive Deep well), numbers in tuning `arcane.*`, `spells.<key>`, `wildbrands.runic_strike`.
  **Mana** is a new attribute (owner-replicated): 100 + 15 a Mana capacity rank, refilling 6 a second in and out of a
  fight (Deep well x1.5 on the main path; Venn's ring's `manaRegen`; nothing in the Silent Vault); a violet bar under
  your health that flashes and shakes when a press is refused. **Shields** are a new shared layer (`Shield` /
  `MaxShield` attributes, `WRArcane::Incoming` after guard in `ApplyHit`): the bigger shield wins, never stacked; while
  one holds no status builds (`UWRStatusComponent::AddStatusBuildup`); Absorb gives the caster Mana back; Mana burn eats
  shields twice as fast; a faceted violet film is drawn round anyone shielded on every machine (`UWRArcaneWorld`, from a
  replicated tag) and a thin violet film runs along the top of your health bar. **Reflect** throws a reflectable shot
  back along its line at x0.8 as a real bolt that never homes (`WRArcane::Reflect`, shared with the bosses); hits now
  carry `bReflectable` (spell bolts, spits, bows and wind arrows; never lobs, beams or siege). Abilities
  (`WRAbilitiesArcane.*`, registered under `arcane` in `ClassForNode`): Arcane missile, Runic strike (Wildbrand), Barrier
  (yourself, or with Ward allies the ally or tame nearest your crosshair within 15 m in sight), Arcane blast (chargeable
  to x1.5), Rune trap (a visible hexafoil on the ground, at most three, never inside someone else's claim; Rune link
  joins your runes within 8 m with violet threads that bite whatever crosses them), Inscribe (+8 arcane a weapon blow
  for 30 s), Aegis (a faceted dome with the eight-armed stave that stops every shot crossing it from outside, bounces
  them back with Reflect, and is broken by a catapult stone), Starfall (12 stars, each landing point glinting 1.2 s
  first), Rune circle (allies inside +15% damage, enemies inside mark 10 a second). Passives: Mark chance, Focus, Echo
  (20%: the spell again 0.6 s later, free, at the same aim), Quick cast, Hardened ward, Glyph power, Lasting runes.
  **Combos** (combos.json): Mana flame, Meteor storm, Quick mind, Mana wings, Frost missile, Frozen rune, Blood magic,
  Blood star, Mana sap, Faerie ring, Bound dead (raised minions last twice as long); Ball lightning, Rune volley and
  Phylactery are shown only (not built yet). **The body** (2.18): the `player_art.json paths.arcane` row (rosettes, the
  2 s cast pulse) plus, at full strength, a soft crystal chime and a fingertip trace on every cast; a shield lifts the
  rune lines to their resting glow. The hard light of shields, runes and Aegis is a new material `M_ArcaneWard`
  (additive, a fresnel rim; `Tools/terrain/build_crystal_materials.py`, 24 KB). Node glyphs for all 22 nodes and the
  path's hexafoil mark (`Tools/ui_glyphs.mjs`, 45 PNGs, about 40 KB). **The Prismglass spear**: a charged heavy throws
  two more thrust-lines of light 15 degrees either side as the point goes home (40% of the blow, Mark 25 each).
  **The Prismback's path magic**: an Arcane walker with 10+ points holds E 4 s with 30 Mana on a Prismback knocked out
  or under 30%: a Barrier wraps it (a full narcotic dose; taming goes on as knockout and feed); arcane hits no longer
  calm it the generic way. **Quest pay**: a master's quest XP goes to their path if you walk it, else half of it to each
  path you do walk (`world.json questOtherPathShare`), so Saga Venn's XP is never silently lost. **Fixed**: the caves'
  passages with no middle points (the Prism Hall's West Door, the Lens Grinder's door, the Silent Vault's rooms) were
  generated with no air (a NaN point in `crystal.mjs tube()`); regenerated. Compiled (editor target); not played.

- **Crystal (4): the nine creatures, and the Crystal caves go live** (`crystal` joins `world.liveBiomes`). Rows in
  `creatures.json` / `attacks.json` (24 attack rows from biome-crystal 2.12.2, mark on bites and claws) on borrowed rigs
  with the cook's palette (`M_CreatureTint`): Prismback (Magmaw), Spindleglass (Kilnmite core), Echobat (Mountain
  Dragon), Geodemite (Kilnmite), Lumenhorn (Frostantler, a mount), Druseback (Rimeshell, widened), Gemsniffer
  (Pumicehog), Alicanto (Ashstrider), Chimecrab (Kilnmite). `WRCrystalCreatures.*`: `UWRCrystalBody` (replicated flags,
  broken legs, the latched victim) builds every silhouette part point by point in `M_CrystalGlow` (so they light by
  sound): the Prismback's four rows of mirror-plates, the Spindleglass's body hung 3 m up on eight selenite-banded legs
  whose feet stay planted and step on an arc when the body has moved 1.6 m past them (neighbours never lift together),
  the Echobat's ears and leaf-nose, the Geodemite's honeypot, the Lumenhorn's palmate spread and 15 m rider light, the
  Druseback's split-geode segments, the Gemsniffer's brow gem, the Alicanto's glowing wing fans and light, the
  Chimecrab's geode shell. `FWRCrystalAI` (friend of the creature AI, called beside the Graveyard's hooks): Echobats
  hunt only what they hear (`OnNoise`; x0.7 in the upper halls in rain) and go deaf 30 s as a Resonance peal passes;
  a Spindleglass feels only steps, rolls and landings within 8 m (its body clouds pink-grey), spears only what is
  under its span, and its legs break (120 each, strike x1.3): a broken leg falls ringing (a 12 m noise), 3 broken it
  collapses, 5 it crawls; a Prismback turns its plated flank to a shot or a cast for 3 s and throws projectiles back
  (x0.8 through `ApplyHit`), its Glare needs a light within 15 m and whites out your view and drops your lock 1 s
  (`Status.Crystal.Glare`, not Blind); Geodemites latch (mark 10 a second, at most four, a hit shakes one off) and
  burst in shards when they die; a Druseback struck at impact 4+ curls 4 s (damage x0.3); the Lumenhorn herd bellows
  when hit (a 40 m noise); a Chimecrab's shell rings; the Alicanto dims and runs for the nearest drop edge when it sees
  you following; tamed, a Gemsniffer points and scratches toward gems, crystal and caches within 20 m and an Alicanto
  leads to the richest ore within 40 m while you stand still. **Dens** (`UWRCrystalWorld::StepDens`, tables in tuning
  `crystalLife.dens`, biome-crystal 2.12.3): every hall a den of its sub-zone, the dome rims and sites the plateau's,
  day and night tables (underground nights 22-04), filled near survivors and emptied when none is near. Saga's "quiet
  road" is tracked across the Organ. The Void's creature rows that were in `creatures.json` three times are now once.
  Saga's fifth quest (the Mirrored Twins) waits in `_saga_venn_later` until the Twins are built.

- **Crystal (3): the hazards, the Silent Vault, the Resonance** (`WRCrystalHazards.*`, numbers in tuning
  `hazards.crystal` and `crystalEvents.resonance`). **Ward fields** (34, `AWRWardField`): a sheet across each doorway,
  only its edges shimmering; an unattuned character within 3 m brings up the tell on every machine over 0.9 s (the
  floor's crystal dust lights into Chladni lines along the sheet, the hexafoil over the door glows, a hum rises two
  octaves); crossing it unattuned is 25 arcane through `ApplyHit` (unblockable, no stance) and a throw 6 m back (a void
  creature 12 m), then 1.5 s before it can throw you again (the Wardcarver compass adds 1.5 s). Attuned (the Arcane path,
  the ward charm, Wardcloth's four, Venn's ring, crystal-born creatures) pass with a soft chime; attuning chalk (now
  drawn on from the bag: `Status.Crystal.Attuned`, 5 min) halves it and stops the throw. Each field's **lead keystone
  plate** (`AWRWardKeystone`, a real blocking box) drops it 90 s when an arrow, bolt, spell bolt or thrown stone strikes
  it (Saga's "Drop the door" counts it); a **ward fork** from the bag quiets the nearest within 10 m for 60 s. Pattern
  A normally, B in the Resonance. **Shard chandeliers** (`AWRChandelier`): a 15 m noise reaching the floor under one
  makes it shiver 1 s, then fall on a gravity curve on the server's clock (every machine draws the same fall): 70
  damage, impact 5 in 3 m (the Geode Tyrant takes 400 and a 6 s stagger); its shards lie 2 game days, then the chain
  draws it up over 20 s. **Thin geode floors** (`AWRThinFloor`): a crust disc with violet-lit cracks over each cup's
  drop; your steps boom hollow within 6 m; a roll, a landing, a heavy blow or a heavy creature's tread breaks a 4 m hole
  (collision off on every machine); it reforms after 2 game days when no one is in it. **Scalding pools**: wading is 4
  fire a second and 15 burn buildup, immersed 12 a second; Burning blood passes, fire resistance halves. **The Silent
  Vault's door** (`AWRSilentDoor`): stand on the hexafoil before it and let nothing within 20 m make a sound for 10 s,
  and its six petals go dark one each 1.7 s (any sound relights all six); at the sixth the lead-plated round door rolls
  3 s along its rail into the wall, stays 60 s and rolls back once the doorway is clear; it won't open in the
  Resonance. Inside, every spell fizzles in a violet puff and the audio director holds the planned silence. V3's rack
  gives three ward forks, V6's Last Carver the Carver's Chisel and the Rune of Silence (once each). **The Resonance**
  (`world.json events.resonance`, about every 3 real days for a game day): the warning's 40 Hz swell and self-ringing
  chime stones, then a peal every 4 minutes rolling out of the Prism Hall at 300 m/s (every crystal flares as it passes
  you, a shake, chandeliers fall as it reaches them, `OnPeal` for the creatures), the first peal regrows every resonant
  node (`AWRResourceNode::RegrowNow`) and splits the six singing geodes (a split basalt geode with its violet druse: 12
  resonant crystal, an amethyst, 10% each of a found rune, first to it), resonant crystal x3. The **Lumen lantern**
  flares 20 m for 5 s when lit (`lights.<id>.flare`, a 10 m noise). New tags `Element.Arcane`, `Status.Crystal.Attuned`.

- **Crystal (2): the gathering, sound makes light, the keystone chimes, the rows.** The noise model
  (`WRCrystalThings.cpp`, radii in tuning `crystalWorld.noise`): every blow that lands (12 m), path ability (10 m), roll
  (8 m), landing from a jump (6 m), strike on crystal (20 m), keystone chime (25 m), arrow or thrown stone landing (6 / 15
  m; a Glassleg bow's arrows half) is heard by the server (`UWRCrystalWorld::OnNoise`, for the listeners to come) and sent
  to every machine as one unreliable message (`AWRCrystalState::MulticastNoise`); footsteps (walk 4 / run 8 / sprint 15
  m, crouched silent; a big creature louder) are sampled on the server for the listeners and worked out on each machine
  for the light. Each hall's lining carries four rings of noise (`M_CrystalGlow` RingA..D / RingS, fading over 6 s) and
  up to three point lights at its densest clusters that follow them; the drip song lifts the upper Geode Halls' resting
  glow in rain. **Keystone chimes** in every hall that has one: E (or an arrow landing on the bar) strikes it, the hall
  holds lit 20 game minutes (`HallLitUntil`, replicated). **The gathering**: 50 resonant crystal nodes plus dust,
  selenite, doubling spar, amethyst, saltpetre, ochre, wax, copper, tin, iron, coal seams (charcoal), silver, star iron,
  gem clusters, wild candle rhubarb and glowmelon (their seeds come with them), from `Data/crystal.json resources`;
  resonant crystal is **picked on the ring** (a strike while the last one still rings, 1.2 s, gives one more, a brighter
  shimmer and note), +50% for 20 min after rain; every crystal is modelled point by point in `M_CrystalGlow`
  (`WRCrystal::NodeLook`) so it lights with its hall. The seven lore keepsakes at their sites, the Wardcarver compass at
  the Dropped Hammer, the Runes of the Prism, Warding and Doubling in the Ward Chambers' sealed rooms (once each per
  survivor), the three chime caches. **Rows**: 37 items (materials, creature parts, amethyst, the four found runes,
  attuning chalk, ward fork, silver bait, the Lumen lantern (no fuel: `lights.<id>.noFuel`), the Glassleg bow, the
  Prismglass Spear and its blueprint, the compass, the Carver's Chisel (runes in the sockets at 125%: `GemStat`), Venn's
  ring, the seven lore items); 7 recipes (the ward charm, attuning chalk, silver bait and wax earplugs from wax are Saga's
  to teach); the biome row, five shrines (two on cave floors: shrine and master rows take a `floor` height now), **Saga
  Venn** at the Great Hexafoil (by day carving, at night at the hexafoil's edge with her fork) and her six quests; the
  sea fog (10%) in the weather; fx and sound rows (stand-ins pitched from the wisp's chimes and the swamp's bells).

- **Crystal (1): the land, the caves' rock and what stands on it.** `node Tools/terrain/crystal.mjs` writes the
  plateau (`Data/lands/crystal.json` + `crystal_ground.bin`, 4 m samples: columnar basalt tilting from the Spine's foot
  to 80-100 m black sea cliffs, the three geo coves, four descents, ten collapsed domes, the Organ's eleven vents, the
  Severance's east mouth meeting the Wasteland's gorge floor exactly) and the places contract `Data/crystal.json`
  (27 halls, the lava tubes and hewn passages between them, shafts, the Silent Vault's rooms, sites, kit POIs, chime
  stones, chandeliers, pools, selenite beams, thin floors, ward fields, resource nodes). `FWRCrystalData::Air` is the
  caves' air as one signed distance; `AWRCrystalCaves` meshes its boundary with collision in 31 m cubes on every
  machine (agate bands, quartz, amethyst, lava glaze, guano and hewn courses in the vertex colours) and grows each
  hall's crystal lining as one mesh with its own `M_CrystalGlow` instance (built by
  `Tools/terrain/build_crystal_materials.py`: vertex colour, fresnel rim, rest / lit glow and four rings of noise).
  `WRCrystal::InCave` makes the caves underground (`WRBiome::IsUnderground`) and the sky light fades out under their
  rock (`WRSky`, tuning `crystalWorld.sky`). The scenery (`WRCrystalScenery*.cpp`, kit `WRCrystalLook.h`): the camps'
  geos and wrecks, the Prism Crown, the Hollowed Giantess, the Split Geode, the descents' stairs, vents, domes, chime
  stones, the surface sites, the halls' and tubes' furniture, the plateau's columns and grass by cell. `FWRToolMesh`'s
  `Tri` / `Pack` are public (the crystal kit builds facets with them). `dressing.mjs` keeps the crystal and graveyard
  POIs when the island is regenerated. `WRSealBody.cpp`'s `Rings` constant is `SealRings` (it clashed with a local in
  `WRSkyAir.cpp` once the unity build regrouped).
## Unreleased: the Sky islands (the Wind land)

Aaron (lane build-biome-sky: requirement ledger R1-R62, build research and plan in showcase-notes
cooks/build-biome-sky.md, building cooks/biome-sky.md). Landing in pieces; compiled (editor target) after each; not
played here (Aaron and the testers test, F8). This first piece is the land, the air and everything standing in it.

- **Where it is** (`Data/lands/sky.json` + `sky_ground.bin`, `Data/sky.json`, both from `node Tools/terrain/sky.mjs`,
  parsed by `FWRSkyIsleData` in `WRSkyIsleTypes.cpp`): the Hollow Cape north of the Storm peaks. The Windward Cliffs on
  the east coast (turf to the lip, a 75-85 degree granite face, a shingle-and-shard apron at the sea; tops 150-320 m),
  the downs west to the Flue's crown (a volcanic dome to its rim at 700 m; the throat is a hole in the ground and a
  48-sided shaft down to the cavern floor at -40 m, fungus shelves low, bird ledges high, a cyan spore glow; the shaft
  flares under the rim's turf so there is no slit beside it), the Chain Cliffs to the south and three camps on the cliff
  tops with PlayerStarts (Wind), safe zones and stations. `WRBiome::At` answers `sky_islands` over the cape and the air
  over the north-east sea at any altitude (`WRSkyIsles::Contains`, asked before the Storm peaks' catch-all);
  `world.liveBiomes` gains `sky_islands`; windmills there turn on the Sky's own wind.
- **The 44 islands** (SKY 2.2: the Lift Steps, the Cloud Gardens, the Ruined Sky-bridges, the Floating Shrines, the
  Undercloud's six) built in code on every machine (`WRSkyIsland.cpp`): a domed turf top (the Kitewrights' terraces with
  walkable 35-degree risers, the rain lagoon's 12 m basin, still pools, the Kitefield's hill, the Drowned Garden's
  30-degree tilt), a sheer rim of strata 8-25 m, a rock root tapering 1.5-3x the radius (never reaching the ground under
  it), the levistone keel's crystal beards (pale glass by day, cyan cores at night, a dim keel light under the big ones),
  spore crust and its glow under the islands nearest the Flue; collision everywhere (walking off the lip is a real
  fall); the Weeping Isle's three caves (a chain-railed stair cut down the rim to two of them, the third behind the
  middle fall on a 2 m ledge; tunnels and chambers with their levistone cores). Lagoon and pools are swimmable.
  Islands decorated by rule: wind-sculpted cloud cedars and junipers (new tree-kit species, leaning downwind),
  wind-combed grass and flax, boulders at the rim, levistone glints in it, lung ta wind-poles every 150 m round each lip
  whose streamers turn downwind and flap (three times as fast in a gust's tell).
- **The places** (`WRSkyScenery*.cpp`, `WRSkySites.cpp`, kit `WRSkyLook.h`): the Paper Titan kneeling on Titan's Crown
  with Kitewright spears through it; the Great Kite (60 m) on its 300 m line with 12 lanterns, always streaming downwind
  (the world's weathervane, turning with the wind); the Ring's lenticular cap and spore column; the Chain Cliffs' anchor
  stones and snapped chains; bridges: stone decks on chains between toll towers, plank spans, broken stubs with chains
  hanging into the drop, the Fallen Span's half hanging 120 m straight down from Span North and swinging on a 22 s
  pendulum (lanterns on it once the Toll Shrine is lit), the Sentinel's deck slab by slab (its five weak sections
  cracked), chain walks (the Wheel's chain, the Lid Gate's walk to the Ring); waterfalls thrown off the lips that break
  into strands and mist all the way down; the Undercloud's cloud bank with the Mistlarks' gap. Hand sites H1-H16 and
  U1-U8 each built round their idea (the Shard Fall's upside-down house, the Lift Wreck's two skeletons, the Larder Tree,
  the Feather Rookery, the Lift Station, the Cloud Garden, the Silk-House and its loom, the Kite Field's death-kites and
  the newcomer's shirt-kite still flying, Gulper's Bight's bone ring and skiff, the Toll House's bowls, the Harrier's
  Eyrie, the Glider Graves, the Wind Wheel's hub turning on its tower and its fallen vanes, the Spore Scar's masked
  gardeners, the Serpent Door's temple, the Kite Graveyard, the Bone Tubes, the Bonebell, the Hushwing Roost, the
  Gardener's Grave, the First Kite, the Newcomer's Kite, the Keel Vault), the shrine settings (Yara's open workshop and
  the Wind Rose at the Great Shrine), the camps, and the 48 kit places.
- **What you use there** (`UWRSkyWorld`, server-authoritative, replicated): three Kitewright lifts (headframe on the
  island's lip, 3.2 m sheave, cage on twin chains, a counterweight on the far fall, winches at both stages; E cranks the
  whole rise in 40 s eased at each end while you stay at the winch or in the cage, the pawl holds it if you leave, E
  mid-way reverses; the cage's floor and rails carry riders; positions from the server time, nothing snaps); levistone
  that floats up 1 m when struck and hangs 3 s for you to grab (walk into it or E) before it drifts off on the wind;
  the Last Kite's line, the Winch Isle chain ladder and the Keel Vault's chain (E to take hold, climb 3 m/s, Jump to let
  go); the sinking stones (W2: land on one and its keel flickers, dust and a groan for 1 s, then it dips, sinks 4 m/s for
  6 s, holds and rises back over 20 s, carrying whoever's on it); gossamer drifts (E 4 s for 2-4 Skysilk, x2 in Rain,
  back a game day each); nodes for levistone shards and keels, the caves' cores, iron and tin, the cliff-top copper,
  cloudpine, wild Windflax and Cloudberry (seeds), Puffwing feathers; caches (the round houses, the Lift Wreck, the
  newcomer's pack, the loom, the eyrie, the Wheelwright's chest, the Spore Scar's satchel, the bone tubes, the sixth
  frame, the Newcomer's Kite, the Keel Vault, the pearl lagoon's sunken skiff); vistas (the Kite Field, the anchor drum,
  the First Kite, the lung ta stones) that pay XP and name what you can see; the Bonebell (struck in the Turning Wind:
  the Kitewright's last kite, once each); the Wind Rose (at noon, on the wind the Great Kite points away from); the
  Toll House's copper (wakes the Sentinel); the Lift Station's blueprint; keepsakes; the Glider Graves' message stone.
- **The air, game-wide** (`WRSkyAir`): **fall damage** for every survivor everywhere (combat SPEC: none under 6 m, max
  health x (h - 6) / 12 to 18 m, lethal beyond; x the gear stat fallDamage, x0.5 under the Featherdown draught; none with
  Featherfall; the Kite String artifact keeps its own rule), through `ApplyHit`; creatures that fall 18 m die, credited
  to whoever hit or pushed them in the last 6 s (`NoteHit` from `ApplyHit`); bosses and the Sky-born (resist "fall")
  don't. **Gliding**: a predicted movement mode (`CMOVE_Glide`): Jump while falling with 2 m of air under you opens the
  best glider in the bag (or the Wind path's Path Glide, or Skysilk's four pieces for 6 s); steer with move input, the
  glider's forward / sink / stamina (none in an updraft) / drift on the wind; Jump, landing, a hit, a stagger, or running
  out of stamina or durability closes it; a glider wears a second a second. The glider shows on every survivor's back
  (`UWRGliderLook`: a crude wing of spars and stitched hide, a levistone wing of cloudpine, skysilk panels and floating
  stones; folded as a strapped roll, eased open over 0.25 s, banking into turns; its cloth's flap by speed). **Updrafts**
  (`WRSkyIsles::LiftAt`): the Flue (+12 m/s to 1,300 m), ridge lift over the Windward Cliffs only while the wind blows
  onto them, island columns, the Undercloud's gap and columns. **Gusts** as pure functions of the server time (no
  corrections): 43 gust lanes on bridges, lips and the cliff ridge on their own steady 8-15 s beats, a Gale's gusts
  every 4-8 s above 300 m by 40 m cell, lee rotors' down-gusts at W1 lips; 1,200 cm/s for 0.8 s; brace (crouch or guard)
  x0.5, anchor boots / Skysilk 4 / a roll / riding none; gliders shoved by rotors and sunk by downbursts. Every other push
  goes through `WRSkyAir::Push`. Anchor boots and any armour piece's own `worn` stat now count (`GearStat`).
- **The Turning Wind's machinery** (the event's rows come with the world piece): the wind swings 180 degrees over two
  minutes on every machine; the two free small islands drift 40-120 m at 1 m/s and hold, carrying what stands on them;
  six cloud bridges form from their ends in, fray grey 1 s before each section fades at the end; 20 shard falls.
- New hooks: `AWRResourceNode::Divert` (a land's own yield in place of the bag), `WRSkyAir::NoteHit` in `ApplyHit`, the
  glide drain in `UWRStatusComponent`, `AWRCharacter::GliderKind` (replicated), the Toll Shrine's kindle, the Sky's
  E prompts and `world sky ...`.
- **The Wind path** (piece 2; `Data/paths/wind.json`, `WRWindPath.*`, `WRAbilitiesWind.*`; cooks/biome-sky.md 2.18): the
  Featherfall root (a tag on the main path, like Grounded: no fall damage from any height, gliders at half the stamina,
  a glider closing low never hurts) and 22 nodes. **Momentum** (attribute pair, the HUD's pale-teal ring and the -oldui
  bar): +14 a second airborne, +8 at a sprint, +4 a blow struck from the air, draining 10 a second after 2.5 s without a
  gain; at 60 and over knockback x1.25, stamina regen +15% and streaks of air run round the ring. A Wind ability short
  of its Momentum still goes off at half strength for its stamina (no dead button). Abilities (all `UWRGameplayAbility`,
  in `ClassForNode`): Gust, Wind cut, Air dash, Plunge, Sky lord (hover: light and heavy become air strikes, dodge a free
  air dash, hold Jump to climb), Whirlwind, Vortex, Tornado (steered by holding its slot), Wind arrow (a ranged
  Wildbrand through the bow), Storm volley; the wave, tornado and ground-patch actors; every passive live (Off balance,
  Eye of the storm, Lingering gale, Piercing gale, Crosswind, Swift draw, Tailwind shot, Aerial mastery, Lift's air hop,
  Wind debris, Path Glide through `WRSkyAir`); knockback and wall-slams through `WRSkyAir::Push`; the 8 combos in
  `combos.json` (the Necromancy pair data-ready); the body change (spirals drifting on the skin, hair stirring, racing at
  high Momentum in a fight). Projectiles gain `PierceLeft` / `OnStruck` / `ContinuePast` (the ammo `pierce` field now
  works), modelled shots keep their trail (the fire pot's fuse sparks show now). Tuning `resources.momentum`, `spells.*`,
  `wildbrands.*`, `nodeEffects`, `passives.wind`, `knockback`; 19 fx and 23 sound rows (existing audio files).
- **What the Sky gives and asks** (piece 3; `Data/items.json`, `recipes.json`, `world.json`, `weather.json`, `gear.json`;
  cooks/biome-sky.md 2.6, 2.13-2.17): 50 items (cloudpine, Skysilk, levistone cores, every creature's and boss's drops,
  the boss core and Gale plume, crude and levistone gliders, the glider mending kit, anchor boots, the Featherdown
  draught, Gale and Hush arrows, the Kitewright glaive, the Sailbell lantern, the spore pot, Galespine, the Wind Mask of
  Aeolith and its talisman, the Kitewright's last kite, eggs, seven lore pieces) and 16 recipes; the Skysilk set to the
  cook's totals (steel 6, levistone 8, feathers 12, linen 8) with Skysilk woven in. **Gale arrows** fly 10% faster and
  throw what they hit back 4 m (Galespine 2.5 m) through the Wind path's knock (brace, anchor boots, the edge credit);
  **Hush arrows** are silent (an unaware mark doesn't learn who shot it) and hit an unaware one x1.15; the **glider
  mending kit** mends the most worn glider in the bag; the **Kitewright glaive** is 10% lighter a swing and reaches 2 m
  further from the air. The trophy talismans do what the cook says: the Harrier plume +15% on flying and airborne
  targets, the Sentinel eye +15 poise standing still or braced, Gulper's pearl -20% glide stamina; the Wind Mask
  +20% Momentum and no glide stamina in a wind over 10 m/s. A Gale's gust blows a torch out in the Sky (the Sailbell
  lantern, violet 12 m on sting oil, never goes out). A piece can need a found blueprint (`gear.json` kind "piece" /
  "found", with "from"): the Updraft lift needs the Lift Station's plan; the Wind Rose's Gale arrow plan teaches the
  same recipe Yara's fifth does.
- **The Sky's world rows**: the biome (`world.json biomes.sky_islands`: path Wind, levistone, gale essence), five shrines
  on their islands (the Great Shrine of the Still Pool with no guard; Gardens: a Gallowshrike pack and a Sailbell bloom;
  Toll: two Galeharriers and a Windmane herd gone aggressive while it's dark (`aggressive` guards); Rose: a Skyray and a
  Gallowshrike pair; Barrow: two Hushwings), placed on the island's top (`WSkyIslandSpot`). **Yara Kesh** at her open
  workshop and her seven quests: Feathers and stone (bring several things at once: quests' `need`), First flight (glide
  First Isle -> Terrace Isle without touching ground or riding), Read the wind (three columns ridden to their tops),
  Silk, The bridges (Toll Shrine lit, then Tollgate -> Brink without a lift or mount), The first kite (she marks the Lid
  Gate on your map: quests' `mark`), Grounded (show the three mini trophies, or their talismans). The deeds are counted
  in `WRSkyAir` (`StepDeeds`). The Turning Wind's event row (every 72 h, one game day, levistone x3); the Wind -> Spore
  invasion through the Flue (dormant until the Fungal caverns are live; Kyrra leads). Weather to the cook's table: Clear
  45, Overcast 10, Rain 10, Cloud fog 15 (40 m), Gale 20.
- **The nine creatures** (piece 4; `WRSkyCreatures.*`, `WRSkyCreatureAI.cpp`, `WRSkyBeast.*`; rows in `creatures.json`,
  `attacks.json`, tuning `skyLife`; cooks/biome-sky.md 2.9): the **Galeharrier** (the Griffon rig at 2.8 m: double crest
  tipped teal that flares before the stoop, black hood with a white face-ring, yellow feet with black talons), the
  **Skyray** (a 9 m manta built in code: charcoal flecked back, a belly blotched black in its own pattern, mouth slot and
  gill slits, horn-lobes rolled into spirals that unroll for the Undertow, a barbed whip tail, a violet rim that glows
  at night; its wings move in a travelling wave root to tip, it banks and pitches, dies belly-up), the **Windmane** (now
  its own: the Frostantler rig with four feathered wings, fore and hind beating out of phase in flight and folded on
  the ground, a streaming silver mane, dark antler tips; 200/32/55, flies 1,600), the **Cloudback** (a 7 m whale in code
  floating 2 m up on its glowing barnacle hump, throat grooves, a fluke beating up and down, a jaw that drops), the
  **Mistlark** (flocks circling the updraft columns, roosting at night), the **Puffwing** (soft wings, red shield, blue
  breast; it parachutes off edges and walks home later), the **Gallowshrike** (bandit mask, hooked beak, short black
  wings, dagger claws), the **Hushwing** (frogmouth gape that splits, a pale facial disc, amber eyes; sleeps in the
  Undercloud roost and the third cave by day, hunts above 700 m at night and by day in Cloud fog) and the **Sailbell**
  (a code-built gas float, violet crest-sail and a 12 m curtain of fourteen beaded strands hanging like chains, swaying
  down their length, sweeping out in the Lash). Every attack row from the cook (34) with its tells; the stoop climbs to
  its fold point and comes down a locked line at 36 m/s (grounded after, or hanging on its wings over open air); the
  Undertow pulls 500 cm/s in its 15 m cone through `WRSkyAir::Push` (a 30-stance blow to the mouth breaks it); the
  belly-flop rises 15 m, its shadow grows, an impact ring; the impale pins you to the nearest thorn or kite stake (bleed
  60, mash) or throws you 4 m; the smother wraps you 3 s (allies' 60 stance frees you); the takeoff gust rings out and
  it comes back down with a charge; the spout throws whoever's on the Cloudback 6 m up; the Hushwing's swoop starts only
  on your screen or within 8 m; the Galeharrier picks a gliding survivor first and steals raw meat; the meat theft, the
  rattle (once a fight, 50 m), the hop-glide across gaps (Gallowshrikes never walk off an island; other ground creatures
  watch from the lip). The curtain stings anything passing under it; a Sailbell killed by fire pops, any other death
  leaves its curtain stinging 60 s. Creature knockback in the Sky goes through `WRSkyAir::Push` (brace, anchor boots,
  the edge credit). **Populations** on the islands (`UWRSkyLife`, tuning `skyLife.populations`: soaring pairs over the
  Lift Steps and the bridges, rays off the lips, the Mane Ledges' herd, pods over the lagoon, flocks per column, roosts,
  blooms across the gaps) kept while someone's near and refilled out of sight; the beats: Mistlarks scatter on a gust's
  tell, Skyrays breach at dusk in a Gale, the Windmanes fly to graze Flax Isle at dawn and back. Windmane nests on the
  two ledges with their clutch (E 2 s takes an egg; the herd within 60 m comes for you). Shrine guards now spawn. 11 fx
  rows, 38 sound rows and 8 creature voices (stand-ins from the nearest families: the Sky's own recordings are the audio
  cook's), covering every sound the Sky's earlier pieces call too.
- **The bosses** (piece 5; `WRSkyBosses.*`, Aeolith's body in `WRSkyBeast`; rows in `creatures.json`, `attacks.json`,
  tuning `bosses.kyrra / bridge_sentinel / old_gulper / aeolith`; cooks/biome-sky.md 2.10-2.11). **Kyrra, the Tempest
  Harrier** (the Galeharrier grown to 9 m: white with age, a chain bound round her talon stump) flies her loop over the
  bridges at 16 m/s and lands every 6 min on the widest island on her way for 60 s; the Tempest stoop (the built dive:
  down on where you stood, a 4 m dust ring, grounded 1.6 s), the snatch (20 m up in 3 s and dropped: 120 stance on her
  frees you), the three-talon combo, the gale wing (12 m, the edge kills), the screech (two Galeharriers, once); arrows
  to her wings count twice: a broken wing grounds her 10 s (x1.25), both and she fights on foot. **The Bridge Sentinel**
  (6 m of white granite, its halo of eight stone vanes turning as it walks, stopping before the slam, whining up for the
  spin) kneels on the standing bridge till the Toll House's copper is taken or it's struck; its Vane slam on one of the
  five weak sections breaks the deck under it (every machine: the slabs are their own parts now, the open ones hidden and
  without collision, `AWRSkyState::DeckOpen`) and it falls 35 m to Winch Isle (a quarter of its health, kneeling 10 s);
  the deck throw leaves a hole for 2 game hours; the grab hurls you 12 m sideways; the vane spin rings out every 0.25 s.
  **Old Gulper** (the Cloudback x3.14, scarred, a reef of glowing barnacles) floats 4 m over the Bight; the Inhale pulls
  500 cm/s in its 20 m cone and swallows whoever reaches the mouth (4 s inside, then spat 15 m any way); fire in its
  mouth while it inhales chokes it onto the lagoon 10 s with its mouth open (x1.5), twice in a minute cracks the hump (no
  barnacle levistone); the barnacle shed's five shadows, the belch only after a swallow. **Aeolith, the Sky Serpent**: a
  60 m serpent built in code whose body lies along the path its head has flown (its coils are real), one pair of 34 m
  feathered wings that beat and spread like a hood on the tells, the bone wind-mask with its vents, a jaw that drops,
  the red throat sac that swells before every breath (glowing at night), quetzal plumes down its back and a 6 m tail fan;
  spore-sick grey spreads over it when weighted. **The Serpent's Ring** (`AWRSkyRing`): eight white platforms with jade
  inlay, their wind-stones (they stop the breath and the quills) and braziers lit at night, chains between them; the
  Flue's three vents breathe a cyan plume every 40 s (a 1 s tell). Phase 1: tail sweep, the gale breath (25 m cone, push
  15 m/s through `WRSkyAir::Push`, blocked behind a wind-stone; the empty sac after opens the chest x1.3), the coil
  combo, the Crosswind dive through the centre (through a live plume it takes +150 spore), the feather volley (12 quills
  at 14 m/s, never homing), the tame-punishing snatch. The Gale Crown throws shots aside until a stance break, a broken
  wing or spore-weighting knocks it off 20 s. 400 spore (plumes, spore pots, the Spore path) weighs it down onto the
  nearest platform 8 s (x1.25). The Unbinding at 50% snaps the chains: the platforms drift (gaps 4-14 m on a 20 s cycle,
  the ring turning, every position a pure function of the server time) and updraft columns stand in the gaps (anyone
  stepping in is thrown up and across). Phase 2 adds the Updraft Inhale (lifted 8 m and held, then the tail lash), the
  Downburst, the spiral combo and the held gale (it swings to where most of you are). The hidden Last Circle (25%, with
  artifacts): it never lands, makes a low pass every 20 s (a wing-roar 1.2 s before; E within 4 m grabs its back, E
  again runs to the next crest vent, Jump lets go), three crest vents of 900 each are its only body damage (8% and a
  stagger each), a barrel roll every 12 s (Guard at 30 stamina a second holds you on, else you're thrown), all three
  broken it crashes 10 s onto the largest platform, the vents regrow at half once. The Wind altar in the Lid Gate's
  temple, run-back stones by the arenas (Kyrra roams: none), the spore pot (thrown: a cyan cloud, +150 on Aeolith), boss
  knockback in the Sky through `WRSkyAir::Push`; a boss over an edge flies back onto the nearest island in 3 s. Drops:
  Aeolith's core, gale essence, plumes, the Gale plume part, the Wind Mask (first kill), Galespine and the Cloud spire
  furnishings as its blueprints; the minis' trophies. 5 fx, 14 sound rows and 4 voices (stand-ins).
- **The Sky's sound** (piece 6): the ambient director treats the islands as the Sky's land (beds by sub-zone: the
  cliffs' surf far below, the gardens' falls, the bridges' chains, the shrines' hollow air over the Flue's roar, the
  Undercloud's close fog and kite cloth, the Flue's breath; day and night layers); music pools for the Sky (explore,
  night, combat, mini, camps, Aeolith's three phases), the Undercloud's own piece inside the bank, and the Turning
  Wind's layer over whatever plays while its sign and the event run. All stand-ins from the built library (no Sky
  recordings or F-Lydian score exist yet: that's the audio cook's).
- **Not finished yet (later cooks):** the Sky's own recordings and its F-Lydian score (every Sky sound and pool is a
  marked stand-in); the Sky's portal fold kit (portals.json `sky` stays `land: false`, so no Sky portals open); the
  Spore -> Wind invasion's Sky side waits for the Fungal caverns (Wind -> Spore is in, dormant till Fungal is live);
  tame uses beyond the shared systems (the Skyray carries one rider, not two; the Hushwing's night guard and the
  Gallowshrike's thorn guard use the shared guard behaviour; the Sailbell fence works through its curtain);
  a Hushwing taking a Windmane foal (no wild foals yet); the Last Circle's riders move vent to vent with E rather than
  running free along the spine; boss soul forging (Aeolith's soul) is the souls cook's.

## Unreleased: the Graveyard (the Necromancy land)

Aaron (lane build-biome-graveyard: requirement ledger R1-R61, build research and plan in showcase-notes
cooks/build-biome-graveyard.md, building cooks/biome-graveyard.md). Landing in pieces; compiled (editor target) after
each; not played here (Aaron and the testers test, F8). This first piece is the land and everything standing on it.

- **The land** (`Data/lands/graveyard.json` + `graveyard_ground.bin`, from `node Tools/terrain/graveyard.mjs`, which also
  writes `Data/graveyard.json`, the places contract read by `FWRGraveData` in `WRGraveTypes.cpp`): a heightfield land
  at its real map metres west of the Spine (B5 Rootlands), on the forest's 4 m grid. Rolling chalk downs from the
  Spine's foot (Chapel Heights, 150-215 m) down to white chalk sea cliffs (35-60 m) on the west coast; three coombes
  (dry valleys) run down to shingle coves, each with a camp and its castaway wreck; the Great Yew's hill; a chalk
  stream through the ford vale to the sea; the Lantern Pond; the Bone Pits; lime pits; 44 subsidence pits in the grave
  fields (lids come with the hazards piece). The south-west neck runs north to the island's own shore (the island's
  ground wins there). Sub-zones G1-G4 by a domain-warped grid, the Volcanic border's ash and the Void border's grey as
  bands, chalk paths worn white. `world.liveBiomes` gains `graveyard`.
- **Rootgrave opens**: the forest's rockfall plug stays in the forest's file but a land's new `cuts` row drops the
  forest's triangles in the pass's box while the Graveyard is live (`WRLand` chunk build), and the Graveyard's samples
  make the pass floor, meeting the forest's on the box's edge ring exactly. Lands may now overlap: `WRLand::At` picks the
  land whose ground is there (a new `priority` breaks ties), and `WRForest::Contains` / the forest's ground trace
  answer the land that's actually under a point.
- **What stands on it** (`WRGraveWorld.*`, `WRGraveScenery*.cpp`, kit `WRGraveLook.h` on the forest's FBuild): the five
  landmarks (the roofless Chapel of Nhal with its 12 bays, transept, apse lantern rack, altar, broken leaning tower and
  rib arches; the Great Yew with its walk-in hollow and the ring of lifted headstones; the 35 m Belfry in its walled
  yard; the 40 m Bone Giant slumped against the Spine with roots through its ribs and lantern-poles through its bones;
  the Lantern Stair climbing the Spine to its break), the 13 hand sites (the Lychgate Chapel with its lychgate and
  lantern rack, the Row of the Named, the Dog Graves, the Ford of Washing, Smith's Row, the Seven Names, the Broken
  Procession, the Watch House and its tally, the Lime Kilns, Where the Rows End, the Lantern Pond, the Root-Split Tombs,
  the Foot of the Stair), 57 kit POIs by the 150 m / 125 m rule, 16 grave fields of ~7,000 headstones in four styles
  (8% leaning, 5% fallen, kerbs, a tōrō every 6th, slag stones on the smiths', heartwood-staked graves on the chapel
  road), dry-stone walls, 300 tōrō along every road whose grave-light reads as dotted green lines at night (the nearest
  28 cast real light), hawthorns and dead elms (new tree-kit species with the yew and the Great Yew), the chalk stream.
- **The catacombs**: 96 rooms and 98 galleries on three levels (L1 the Crypt Gates, L2 the Ossuary at 46 m, L3 the Old
  Quarry at 12 m), every floor above the sea and every ceiling under chalk (the generator checks); six crypt mouths
  with stairs down, two subsidence holes with ropes, the Draught Stair and the Quarry Stair (spirals), the chapel crypt
  house (opens after Nhal's first kill). The twelve hand rooms each built round their idea (the lit Niches of the
  Unnamed, the Watchers' Vault, the Skull Wall of the Wheel, the Chandelier Hall, the Bone River, the Sorting Room, the
  Starfall Pit, the Brood Gallery, the Three Cisterns with the drowned bell, the Quarry Face, the Chapel Crypt) and
  kit rooms off the galleries. `WRBiome::IsUnderground` knows them (dark, no building); the land's `underground.ways`
  keep the game mode's rescue off them.
- **Gathering and finds** (server): soulstone outcrops and grave seams, bone piles, limestone, mortsafes (grave iron),
  wild black rye and asphodel, silver veins, wax nests, star iron in the Starfall Pit, young yews (yew logs);
  caches (the smith's anvil, the lime barrows, the family vaults, the niches, the Watchers' crate); spots you use with E
  (`AWRGraveSpot`): keepsakes, digs (S1 under the Yew, S2 the one full grave, three cenotaphs), the Stair's last lantern
  (S3: the Stair burns gold for a game day), the seventh name (S4), the drowned bell's chamber (S5), the Row's five dark
  tōrō. Survival gains `SnuffTorch` / `HoldsLantern` and the light registry `BrightestNear` (for the draughts and the
  Wraithmoth).
- **Items, recipes, the world's rows** (piece 2): 46 Graveyard items (soulstone and its dust, yew logs, limestone,
  quicklime, grave iron, silver, wax, the creatures' parts, grave essence, Nhal's core, trophy head and part, the
  heartwood stake, lime pot, grave lantern, Mourning ring, Yew longbow and its blueprint, palp-spine arrows, the Sexton's
  pick, moth-dust tonic, Asphodel draught, Cleansing draught (items can now take corruption away: `use.corruption`),
  the Mourner's Scythe and its blueprint (Nhal's boss weapon in gear.json), the chisel, seven lore items) and their
  recipes; quicklime burns in the charcoal kiln (a lime kiln and a charcoal kiln are the same draw kiln); the tōrō
  lantern building piece (a light that needs no fuel, modelled in `WRBuildLook`). Node looks for soulstone on its bone
  bed, bone heaps, limestone, mortsafe cages, black rye, asphodel, wax nests, silver ore, young yews. world.json: the
  Graveyard biome row, its five shrines (the Great Shrine of the Lantern at the Lychgate Chapel; the Seven Names, the
  Lime Kilns, the Foot of the Stair; the Watchers' Vault on its catacomb room's floor, guards kept in the room: a shrine
  row's new `room`), Morwen the Lantern-keeper and her six quests (light the Row's tōrō, the first raise, break six
  mortsafes, the grave under the yew, the Ossuary ledger, the Gravedigger). Morwen keeps hours: the lantern rack by
  day, the Row of the Named at dusk, her chapel at night, the lychgate step at dawn (moves when nobody's watching).
- **The creatures** (piece 3; `WRGraveCreatures.*`, rows in creatures / attacks / spawn_regions, tuning
  `graveyardLife`): the nine on stand-in rigs from our packs with the cook's palette over their textures and their
  silhouettes modelled on their bones: the Gravehound (Barghest: bone-pale skull face, knuckled spine ridge, ribs,
  soul-green eyes; it sleeps buried in grave soil by day under a mound with two eyes in the dirt, wakes when you come
  near at night, comes up with the Rising, its Grim howl raises every buried hound within 40 m, and in Corpse fog packs
  rise out of the mist round you in the fields and the Bone Pits), the Cryptstalker (the six-legged Kilnmite rig made
  flat and wide, 5 m whips that sway and fan out on the whip sweep, spined palps; in the dark it goes only for what its
  whips touched; in the catacombs it hangs from the ceiling and drops; the palp snatch pins you 2 s, an ally's 150
  damage or mashing frees you), the Wraithmoth (death's-head mark, violet underwing, proboscis; it flies at the
  brightest light within 30 m, its dive puts out a held torch and leaves it grounded 1.4 s, an arrow brings it down, its
  dust chills your stamina regen, on a downed survivor it lands and drinks with your view darkened), the Palestrider
  (the slender Ashstrider rig: bone face, torn black mane; it screams and the herd turns; ridden it never tires), the
  Tombback (grave slab on its back: frontal blows on the stone x0.3, its belly x1.3 while it rears; it goes to fresh
  kills and sits on them), the Gloamcrow (grey hood, silver eyes; crows gather silent over a death, then mob the
  killer), the Mournhare, the Wickbeetle (wax dome, a real candle-yellow light), the Marrowgoat (horns, a bone in its
  mouth, a long white coat). The Risen (mourner, hound, bearer) are real animated skeletons: the Mixamo / Barghest rigs'
  own clips drive modelled bones, grave wrappings, iron bands, a lit lantern-pole or a coffin; they fall to bones (no
  corpse; their bone and a chance of soulstone dust go to the killer). Spawn regions G1-G4 day and night
  (`graveyard_g1..g4`); the walk map grows over the Graveyard (`Tools/terrain/graveyard_walk.mjs`); a den in most
  catacomb rooms by level. New hooks: `OnDamageTaken`, the mash press, stamina regen (the dust, the moth-dust tonic's
  brew), decay buildup (the Asphodel draught).
- **Hazards, weather, the air** (piece 4; `WRGraveHazards.*`, tuning `hazards.graveyard`, `graveyardWorld.sky`,
  `graveyardWorld.corruption`, `graveyardLife.riftborn`): each with a tell of at least 0.9 s. Grave subsidence: a turf
  lid over each of the 44 pits; stand on one and the turf cracks for 1 s (dust, a crunch from below), then it drops you
  3 m; half the pits wake 2-3 Gravehounds in the dark under you; the turf grows back once the pit is empty; dark
  fresh-turned soil marks the risky ground and a tamed Gloamcrow screams as its owner nears one. Lime pits: 0.9 s of
  hiss and steam, then 8 a second (twice in Drizzle) and armour wear (the Graveyard's creatures stay out). Crypt
  draughts: every 8-15 s in a draught room a 1.2 s rising whistle, then the gust puts held torches out (lanterns burn
  on). weather.json: Ground mist (by day, knee-deep), Drizzle (new: lime boils, torches burn faster, crops watered),
  Overcast, Corpse fog (night, chest-high, lights 0.7 as far). The sky (`WRSky`): a bruise-violet haze, a yellow-green
  dusk rim, a cool washed grade, the mist as a second fog layer at your feet, and no moon over the Graveyard (the
  Blood Moon brings it back red: `WRGraveSky::BloodMoon`). Corruption reads on the land: from 50 the colour drains and the
  edges close, from 75 whispers behind you, at 100 the tōrō gutter. Graveyard Riftborn: 15% of Gravehounds,
  Wraithmoths and Tombbacks south of the neck's line come back changed (voidglass on the spine, black-violet eyes,
  +25% health, curse on every blow). (`WRVoidUnmade.cpp`: a local renamed for a unity-build name clash.)
- **The bosses** (piece 5; `WRGraveBosses.*` the fights, `WRGraveBossLook.cpp` the arena pieces and code-built bodies,
  `WRGraveArenas.cpp` placement and held actions; rows in creatures / attacks, tuning `bosses.<id>` and
  `bosses.graveyard`): the four minis and the major, each placed with its pieces once a survivor comes within 300 m.
  **The Bell-Ringer** (Grimjaw, 4.2 m, half a cracked bell for a helm) in the Belfry yard: every 20 s (15 on a Blood
  Moon) he goes back to the rope and the Toll comes: the great bell turns a full revolution on its headstock with its
  wheel, the rope's sally rising and falling with it, a ring of force round the tower's foot and three Risen mourners
  climbing out of the yard's mounds (9 at most); the rope's frayed section 4 m up takes 400 (blades and fire x2) and
  cut, the bell falls silent, he takes the hand-bell from his belt and his stance drops to 60; thrown headstones stay
  standing where they land as cover. **The Crypt Mother** (9 m span, about twenty young riding her back) in the Brood
  Gallery: eight egg sacs moored on the pillars and end walls (150, fire x3, a torch held to one 2 s burns it) hatch two
  young every 30 s; fire on her back burns the riders off (the Brood shed throws what's left at the furthest survivor);
  she climbs into the dark and drops on the dust; the whip sweep's touch takes the delay out of her next move. **The
  Ossuary Lord** (a 6 m bone giant: bound femurs, stacked ribcages, a crown of skulls, three soulstone keystone skulls)
  in the Chandelier Hall: at 0 it falls into its pile and re-forms in 8 s at half health unless roots bind the pile
  (any Nature snare or vines over it), two lime pots land on it, or its three keystones are smashed (120 each, strikes
  x1.5); the bone chandelier's chain (300) lets it fall once on gravity onto whatever is under it (400 and a 4 s stance
  break on the Lord); the ribcage holds a survivor (allies break it open; from inside you hit its chest keystone x2).
  **The Gravedigger** (Obsidian warden rig, horse skull under a sheet, a mortsafe cage on his back, a lantern over his
  shoulder): asleep at the Watch House by day; from 19:00 he walks his round with three Gravehounds 30 m behind,
  stopping at six graves to dig (the spade carries 40 m; the graves stay open till morning; two more on a Blood Moon);
  Bury heaps earth over a 3 m circle (mash, or a friend holds E 1 s), the Mortsafe shuts you in the cage (break the
  door, or hit his back), smash the lantern (150) and he's blind 10 s and lights a spare once. **Nhal, the Last
  Mourner** (Ignessa rig at 9 m, wings hidden under a veil of moth cards that flutter, a crown of tōrō lantern-horns, an
  8 m yew-and-bronze scythe) in the roofless chapel: twelve chest tombs in the aisles; at half health The Opening
  (forced: every lantern in the chapel gutters out and stays out, a 14 m ring, every tomb cracks open, the veil lifts
  into a wheeling cloud on the roof ribs); in phase 2 each open tomb raises a Risen every 25 s (one in four a bearer, 8
  at most) until it is sealed by a Nature blow or roots within 2 m or a heartwood stake (hold E 1.5 s); Grave hands
  crack the floor green under everyone for 0.9 s, the Keen is a cone that pillars and tombs block, Lay to rest drags the
  furthest tame half into the floor (hold E 2 s on it to pull it up); hidden phase 3 (artifacts): she kneels and forty
  shades pour out and walk a breathing ring down the nave, turned back by the tower's bell every 10 s. Her first fall
  opens the chapel crypt's stair; the Necromancy altar stands in the apse. Run-back stones by the Belfry's gate and the
  chapel; the catacomb halls' stones by their doors. Every blow is `ApplyHit`; lobs fly real arcs and never home.
- **Lime pots** now work as thrown weapons (`code:limepot` modelled in `WRGraveHazards`): a 4 m splash, 20 and then 8
  a second for 3 s (tuning `hazards.graveyard.limePot*`); the Graveyard's own don't burn.
- **Graveyard sounds and effects**: 43 effect rows (`fx.json grave.*`, every tell, crack, toll, burst and cloud the
  pieces above and pieces 1-4 call) and 50 sound rows (`sounds.json grave.*` and the creature moments), plus voice
  families for all eighteen Graveyard creatures and bosses. The sounds are stand-ins: each row plays the nearest
  existing recording pitched (`_standin`, its `_comment` names it); Graveyard recordings are a later cook.
- **The Necromancy path** (piece 6; `Data/paths/necromancy.json`, `WRNecroPath.*` the rules, `WRAbilitiesNecro.*` the
  abilities, tuning `necromancy`, `spells.<key>`, `wildbrands.grave_strike / scythe_sweep`, combos.json): the 22 nodes
  (trunk, Horde, Reaper, Lich) as the cook lists them. **Souls** (new attributes `Souls` / `MaxSouls`): a death within
  30 m that you or your minions hit in its last 10 s gives lowland 1, midland 2, highland 3, a Risen 1, a mini 10, a
  major 25, a player 3 (x Harvest, + Death mark), each a pale-green wisp to your chest; 20 to hold, +5 a Soul capacity
  rank; all lost when you die; the HUD shows a row of bone-green flames under the stamina bar (flashing when a press is
  refused). **Deathless** (main path): once per Rest a killing blow leaves you at exactly 1 HP (ash flash, one toll); the
  HUD says ready or spent; a Rest re-arms it. **Raising**: a creature's body under 3 minutes old and not yet butchered
  stands up for you at 60% for 30 s with its own attacks, following you and fighting your lock, your fight or the
  nearest foe; at most 3 (5 with Second wave), the oldest crumbling first; never a tame (no limit, never saved, no body
  or loot when it falls); it crumbles to bones. Raised bodies of any creature get the raised look (grave-light threads,
  pale eyes; ember threads on an ash wraith, rime on an ice minion). Abilities: Raise dead, Grave strike (any melee,
  decay 40), Corpse explosion (40 in 4 m, decay 50), Army of the dead (every body in 12 m for 20 s), Scythe sweep (a 300
  degree sweep 3.2 m round you with a scythe, greatsword or spear, hyperarmor), Shadow bolt (30, decay 40), Curse of
  weakness (curse 80), Life drain (a 2 s beam, 12 a second, heals half), Lich form (15 s of free spells, the full ash
  look and a crown of grave-light). Passives in the one damage pipeline: Wither chance, Restless dead, Dead march,
  Shared pain, Harvest, Soul siphon, Death mark, Reaping stride, Final harvest (a weapon hit on a non-boss creature under
  20% reaps it; bosses and players take +30% instead), Necrotic power, Soul shield; the Graveyard's corruption buff (+3 to
  +15% path damage at 25-100). Combos with Fire (Ash wraith, Funeral pyre), Frost (Preserved, Frozen legion), Lightning
  (Storm lich), Blood (Death curse), Nature (Cycle of life); Jolt of life, Blood bond and Rot and bloom are listed but not
  built (their text says so). Raising through walls (raiding.paths.necromancy): a body you can't see within 12 m rises at
  3x the Souls. The body change is the hero's player_art row (paths.necromancy); Lich form shows the full ash look at any
  point count and each raise flares the pupil ring.
- **The Blood Moon, the invasion, the Graveyard's sound** (piece 7; `WRGraveEvents.*`, world.json `events.bloodmoon` and
  `invasions`, tuning `graveyardEvents.bloodMoon`, sounds.json beds and music pools): the Blood Moon comes about every
  3 real days for one game day (the Biome events setting). Its sign: the red moon rises over the chapel and the
  rust-red sky with it (`WRGraveSky::BloodMoon`, eased on every machine), the Belfry and the chapel tower toll once. The
  Rising (20:00-05:00): corpse fog forced, corruption x2 on the land, processions of 4-8 Risen behind a lantern-bearer
  form on the procession roads (one every 150 m where a survivor is within 400 m) and walk the road, or toward the
  brightest light within 600 m, standing vigil at a camp's safe-zone edge; one destroyed forms again 6 minutes later;
  the Bell-Ringer tolls every 15 s and the Gravedigger digs two more graves (piece 5); Necromancers' Souls x2 and raises
  two at a time. Dawn: every procession Risen falls into a bone pile (bone, and half the time soulstone). The Graveyard
  -> Forest invasion is filled (the Gravedigger leads 6 Gravehounds, Wraithmoths, a Cryptstalker and 4 Risen mourners
  through Rootgrave; his night round waits while he raids). Ambient beds for the land (wind, night) and G1-G4 and the
  neck; music pools explore / night / combat / mini .graveyard and boss.nhal. Beds and pools play existing recordings as
  stand-ins (`_standin`); the Graveyard's composed score (54 BPM, F minor, organ and choir, the bell on F) is a later
  cook. Not built: the processions' claim raids, the Mourning's x3 seams, the invasion signs (the Lantern Line lighting
  itself, moss along the forest's road), the forest invasion's sealed band.

## Unreleased: the Ancient forest (the Nature land)

Aaron (lane build-biome-forest: requirement ledger R1-R56, research, plan and per-track build notes in showcase-notes
cooks/build-biome-forest.md, building cooks/biome-forest.md). Compiled (editor target, and the game target in full
unity); not played here (Aaron and the testers test, F8). `Wildreach.Build.cs` gains MeshDescription +
StaticMeshDescription (the tree kit builds static meshes at runtime). Not finished: the heartwood shield (no shield items
yet), the forest's portal fold kit (forest portals stay off), sculpted bodies (creatures are retinted rigs with code-built
parts), lock-on to Sylvaroth's Root Knots.

- **The land** (`Data/lands/forest.json` + `forest_ground.bin`, generated by `Tools/terrain/forest.mjs`): a runtime
  `WRLand` like the wasteland, now able to take a **heightfield** (4 m samples + a surface byte: river bed, gravel,
  sand, dune, marsh, road, trail, Hollow floor) with zone colours from the forest's zone grid (F1 meadows, F2 streams
  and falls, F3 Grovekin villages, F4 old growth, F5 elder canopy; noise-warped borders), basalt cliffs with moss on
  the Spine, a wet look in rain (`M_LandGround` gains `Wet`). South of the Old Island (map x -1,450..+2,582,
  y +2,016..+5,936 m), joined by the south-east neck; game cm = map m x 100. `WRForestTypes.cpp` answers zone, band,
  canopy and no-felling; wildlife, sky (forest air, canopy fog and dim), resource looks, portals, meteors, walk grid
  (`forest_walk.mjs`), sea mask and the map picture know the land. Three camps with forest PlayerStarts, safe zones and
  starter stations spawned by the server; Nature players start there.
- **Trees** (`WRTreeKit.cpp`, materials `M_TreeKit*` from `Tools/art/make_tree_kit_material.py`): every species
  modelled in code at real scale; 45k `forest_*` instances in `Data/island_dressing.json` by the decoration rule
  (`Tools/terrain/forest_dressing.mjs`: canopy gradient, giants only in F4/F5 with thornbrake rings, light gaps round
  every place, hedgerows, bank alders); harvestable except giants ("The roots hold it.", no felling within 60 m).
- **The world** (`WRForestWorld.*`, `WRForestScenery*.cpp`): 12 landmarks, the hand sites and kit places, the canopy
  walkways and the Canopy Lift (counterweight, smooth from a replicated start), the Root Bridge, the Mill Race, the
  newcomers' village; caches, 5 caves, resource nodes (heartwood, resin, honey, truffles, rhizomorph, foxfire,
  thorns, antler, ores, wild oats and Iðunn apples); 5 shrines, Rowan Ashby and his 6 quests; 43 items, recipes, gear.
- **Hazards** (`WRForestHazards.*`): thornbrakes (bleed, slow, passes, burning), widowmakers, leaf pits, wasp nests and
  the scout mark, rivers with spate, smoke; weather effects; **the Bloom** with Elder Falls, wasp raids and the Wild
  Hunt; secrets F-S1..F-S5 and the eel trap; Nature<->Necromancy invasions (dormant until the Graveyard).
- **The Hollow Root** (`WRForestHollow.*`): 14 rooms under the forest, roofed with root mats, foxfire and glowworms,
  four ways in (the Eldest's Door, the Sinkroot squeeze, a tamed Truffler's dig, the one-way Heart Root).
- **11 creatures** (`WRForestCreatures.*`, rows in creatures/attacks/spawn_regions): Mossmaul, Thornlynx, Rootcoil,
  Bramblewasp, Elderelk, Burlbeast, Truffler, Hiveback, Fernsheep, Dappledoe, Glimmerfinch on recoloured rigs with
  code-built parts; canopy drops, log rest, grabs, scout marks, heat balls, bugles, the Mossmaul path-magic tame,
  mount/hauler/forager utilities and the living-world beats (`UWRForestLife`).
- **Bosses** (`WRForestBosses.*`, `WRForestBossLook.cpp`, `WRForestArenas.cpp`): Old Gnarl (Clear-cut), the
  Bramblequeen (Paper Hollow), the Grey Matron (the Bower's walkways), the roaming Hornwarden and the Wild Hunt, and
  Sylvaroth at the Clearing of Four Hearts (healing lines, Root Knots, Uprooting, P2, hidden P3), run-back stones, altar.
- **The Nature path** (`Data/paths/nature.json`, `WRNaturePath.*`, `WRAbilitiesNature.*`, `WRPlantSummon.*`): 22
  nodes, Growth by the ground, Rooted, Entangled, 10 abilities, plant summons, 4 combos, the Growth gauge, the body
  change with temple buds.
- **Sound** (`Tools/audio/build_forest_audio.py`, `sources_forest.py`): real beds per sub-zone, creature and boss
  voices, music pools (explore/night/combat/mini/boss P1-P3/Hollow Root/Bloom/Wild Hunt/camp/village), credited in
  `Docs/CREDITS.md`.
- Fixes found on the way: `WRStormWorld`'s file-scope `Char` renamed `CharBlack` (it hid UE's `Char` in full unity).

## Unreleased: the Corrupted wasteland (the Void land)

Aaron (lane build-biome-void: requirement ledger, research and plan in showcase-notes cooks/build-biome-void.md, building
cooks/biome-void.md). Built to compile (editor); not played here (Aaron and the testers test, F8).

- **A runtime land** (`WRLand.*`, `Data/lands/wasteland.json`): the Wasteland's ground is a pure function of the
  world-map's metres (base from the Spine crest, coast falloff, noise, then data features: the Great Rift's hole, the
  viaduct ravine, the Glass Sea flat, the Severance gorge, cave hollows), built as 200 m procedural-mesh chunks with
  collision on every machine (time-sliced, nothing in the map, no LFS). It sits 9 km south of the island until the
  12-wedge map exists (`origin` / `mapCentre` move it whole). `WRBiome::At` answers `wasteland` inside it; the
  biome is in `world.liveBiomes`, so its crops, fish, pieces, sets and recipes are live. Five sub-zones V1-V5 with
  their own ground colours, decoration and beds. Materials `M_LandGround`, `M_VoidTear`, `M_VoidLight`
  (`Tools/terrain/build_void_materials.py`, commandlet).
- **Its places** (`WRLandSites.cpp`): the 10 landmarks (the Great Rift column, the Throat, the Hanging Wood, the Hollow
  Colossus, the Hollow Fort, the Unravelling Viaduct, the Ash-cast cage, the Broken Knee, the Glass Sea, the Black
  Beacon), 3 camps, 4 caves, the story sites, the four Gauntlet stations' yards, the Descent, kit POIs by a seeded
  150 m / 125 m pass and decoration between them; every place with its night light. Floating things move on the
  server clock (`FWRLandMover`), never snapping.
- **The Grey Crossing** (`WRLandThings.*`): a longboat at the island's south shore and one at Lantern Cove; hold E to
  row across (`land cross`).
- **The Void's cost** (`WRVoid.*`): `MaxHealthLost` (health can't rise past MaxHealth - MaxHealthLost: the grey hatched
  band at the right of the health bar; Rest gives it back), `Void` / `MaxVoid` for the path. **Rifts** (`AWRRift`):
  a tear with a drain zone (nullward / Hollowplate / anchor salt cut it), a pull computed the same on server and
  owner (no correction), a touch that burns through `ApplyHit`, a pulse (1 s silence, then the floating stones drop),
  a voidglass ring. Timed brews (`items.json use.brew`). Numbers: tuning `void`.
- **Dead calm** (weather `deadcalm`, 30%): no wind and no beds, your footsteps doubled, the music fades out over 8 s
  (half in a fight). Grey wind is now 10%.
- **Its gathering and things** (`WRVoidWorld.*`, wasteland.json `resources`): greywood from the Withered Forest's
  standing dead (hatchet), grey salt pans on the Grey Strand, wild ashen grain in V1's clearings, the Quire's six
  mandrakes (a 0.9 s shiver, then a 12 m scream that staggers you without wax ear plugs), star iron in the Hollow
  Ground's meteor bowl, twenty voidglass nodes on the Glass Sea; node looks for voidglass (a fused black sheet with
  standing shards), greywood, salt pans, grain and mandrake. Keepsakes at nine story sites (the lore items, E to take).
  Thrown things: a throwing stone floats in a rift zone (Maren's first quest counts three different rifts), an anchor
  salt bag bursts in a 3 m cloud the bosses will listen for. Items: greywood, grey salt, anchor salt (eat: rift drain
  x0.5, 5 min), curse-resist draught, void essence, the creatures' parts, the Gnashblade, Hushfinger spear, Nullwing
  bow, voidglass dagger, Beacon lantern, Gnawer arrows, milk, ten lore items; their recipes.
- **Shrines and Maren Vey**: five shrines on the land (the Shrine of the Edge at the Quiet Rift, Greywood, the Strand,
  Severance West, the East Gate) with their guards and a Wasteland dais (ash, thrown stones, voidglass splinters);
  rows on a runtime land carry `"land"` and stand on its height function. Maren Vey at the Great Shrine and her six
  quests (event / bring / kill kinds).
- **The nine creatures** (`WRVoidCreatures.*`, creatures.json / attacks.json): the Hollowfang (Barghest rig: eye pits,
  black glass teeth, the blink with its shudder, reversed breath and the ripple where it lands), the Riftmaw (lies in a
  large rift, bursts out of the rift nearest you, goes back after three attacks or at a quarter health, stranded and
  slowed 30 m from any rift, the four-jaw catch), the Nullwing (torn membranes showing the void, the laid void trail
  `AWRVoidTrail`, dive and grounding, void spit that never homes), the Gnawer (latches and eats 3 max health a second;
  a roll or a swing shakes every one off), the Hush (blind: hunts by noise, rises to listen, the scream that breaks a
  drink or cast, the lift), the Voidstrider (drifting filament mane, blink kick; ridden, Dodge blinks 5 m every 8 s),
  the Riftback, the Driftmote (hovers, pulses faster near rifts when tame), the Withergoat. Riftborn variants (15% in
  V4-V5, 5% elsewhere: desaturated, black-violet eyes, shards on the spine, +25% health, curse 15 on every blow, a blink
  every 10 s, void essence). Dens on the land by zone with day and night tables (wasteland.json `spawns`), a Riftmaw
  in every large rift, the Hush on the Gauntlet Road at night. Voices are the nearest existing families pitched (the
  fused recordings are a later sound pass); beds for the land and each zone.
- **The Void path** (`Data/paths/void.json`, `WRVoidPath.*`, `WRAbilitiesVoid.*`; biome-void 2.17): the Void bar (HUD ring,
  never called Corruption) filled by every void ability and draining 5/s after a quiet 4 s; under your tolerance (20,
  +10 a Void tolerance rank) abilities are free, past it they cost their max HP into the grey band; the self-cost
  limit (75% of max HP) refuses with a dry crack; +0.3% void damage a point. Empty vessel (root: Void x0.7, costs
  x0.75, rifts pass you). Twelve abilities: Void bolt, Null strike (ignores 30% armour), Void rift (`AWRVoidField`),
  Collapse (pull and crush), Black hole (pull 600 cm/s for 4 s, bosses slowed, then 80), Blink (i-frames the whole
  travel), Phase (blows and grabs pass through), Shadow step (behind the lock target), Nowhere (untargetable 3 s, the
  next hit crits), Consume (the finisher under 25%, Void -20, 10% of the band back; bosses only stance-broken),
  Sacrifice (10 max HP for +30% damage), Maw of the void (`AWRVoidMaw`: four black-glass petals rise and close; foes
  under 30% are eaten). Passives: Unravel chance (Unravelled: armour -20%, stance +15%), Abyssal power, Dark matter,
  Event horizon (your rifts swallow enemy projectiles), Fade (dodges and blinks), Unseen, Gluttony, Feed, Insatiable.
  Combos with the paths that exist: Black flame, Void furnace, Dark lightning, Singularity spark, Frozen blink, Ice
  maw, Void leech, Abyssal hunger (`Data/combos.json`). Body change: the bar and the band move the greying within its
  stage. Node glyphs drawn by `Tools/ui_glyphs.mjs`.
- **The bosses** (`WRVoidBosses.*`, creatures.json / attacks.json / tuning `bosses.<id>`, wasteland.json `bosses`):
  the four Gauntlet stations and the Echo (3,000 HP minis, 4 h respawn) and Vorhollow (12,000). The Unravelled (body
  hits x0.3; past 300 damage or a heavy blow it unravels into a whirling column, travels 8-15 m, its core hangs alone
  0.6 s at x3 / stance x4, then the Reform slam). The Mother of Rifts (opens brood rifts in her six pens: Gnawer swarms
  or a Hollowfang every 15 s, up to 4; a brood rift is a 400 HP body or closes to thrown anchor salt; all closed: 4 s
  stun, stance x2). The Starving One (each blow that lands eats a buff and it grows: +10% size and damage, +5% speed,
  5 stacks; food on the ground within 15 m draws it to eat 2.5 s, stance x2, a stack off). The Null Warden (his 12 m
  null field fizzles spells and drops projectiles; the bell's toll strips a buff and stamina; his death drops his key
  into the Null Gate's lock: `AWRNullGate`, a portcullis on two chains over pulleys to counterweights in stone shafts,
  rising over 6 s; a crank winch on the bridge side, hold E 8 s). The Echo (walks a 2.4 km loop by night, rests on the
  Glass Sea by day; its Mimic casts back the last path ability that hit it, never homing; three different ones in a
  row and it fails: stunned 2 s). Vorhollow on the Choir's last stage (`AWRVorhollowStage`: a 72 m island of seven
  pieces with collision on its keels over the Great Rift, four chains to anchor stones, the 124 m chain bridge, six
  voice pillars): Hook rake into the slam, the Lamprey lunge (swallowed: 15% of max HP into the band, dropped out
  behind it), Inhale (a pull that pillars block), the Rift beam (a sweeping beam pillars shade), Tail flail,
  Nullstones, the Hollow (a shell at 80/60/40%: x0.2, ringing pillars wear it 150/s, void heals it, 20 s then a beam);
  the Gnashing transition snaps the chains one by one and splits the island, whose pieces then orbit on the server
  clock (easing apart, gaps breathing, the two smallest falling away and rising again; anchor salt wards a piece);
  Rift dive, the twin beam with its pause, the P2 combo; hidden phase 3 with artifacts: the Gullet (a pull into its
  chest, a chamber with the Void heart sharing its health x1.5, crushing ribs every 20 s with safe alcoves, a cough
  every 30 s). Falling into the black past the void floor is Unmade. Looks drawn over stand-in rigs (the core, the
  bell helm and bell, the unhinged jaw, the plume tail, the chest hole and its starfield, the hooks and spines).
- **Litter fire, hollow crust and the Rift Storm** (`WRVoidHazards.*`, tuning `voidHazards`, wasteland.json `crust` /
  `riftStorm`, world.json `events.riftstorm`): fire on the Withered Forest's litter (a fire blow, a fire bolt's
  landing, a burning pool) smoulders 0.9 s then walks downwind at 1.5 m/s (0.5 upwind) over 6 m cells, each burning
  60 s (3/s, burn 8/s) and black for 2 game days. Eight hollow-crust patches over pockets cut into the ground (a
  `pit` land feature) drum underfoot within 6 m and break under a roll, a heavy landing, a heavy blow, a heavy creature
  or a thrown stone (one pocket holds a small rift). The Rift Storm (one game day about every 3 real days): every
  inrush howls through the warning; 6 of 10 seeds open as storm rifts with a floating rift heart (1,500; void x0.5;
  anchor salt -300); three waves 4 min apart walk to the nearest claim within 1.5 km as raiders (else the nearest
  shrine); a broken heart closes its rift and leaves voidglass at x3, 1,500 path XP split by heart damage.
- **The Unmade Place** (`WRVoidUnmade.*`, tuning `voidUnmade`, wasteland.json `unmade`): the Choir's Descent (343 cut
  steps spiralling 120 m down the Great Rift's north wall, solid), floating stones on voidglass keels between the eight
  places (gaps about 1.5-2.5 m, never more than a step's rise), U1 the Descent's foot (ledge, cage rest, rope anchor,
  brazier), U2 the Seated Choir (200 dead on four floating terraces facing one point of black), U3 the black pool and
  its false stars, U4 the Inverted Tower, U5 the rubble garden (ten voidglass nodes at x2, 12 game h), U6 the Riftmaw
  drift (three), U7 the Cantor's reliquary (a sealed chapel; void essence 6), U8 the last note (her figure at the
  farthest edge; reaching her is Maren's fifth quest). Drain x1.5 inside. The Descent lift (`AWRDescentLift`): a
  timber drum on an iron axle in two bearing blocks, a ratchet wheel and pawl, two cranks, a jib with its sheave, the
  rope to a basket cage of iron straps; mended with rope 10 and iron 4 (5 s); E on the crank (or in the cage) moves
  the cage 1.2 m/s, the drum winding the rope, the pawl clicking every 20 cm and holding wherever you let go.
- Music: the land's own pools (`explore.wasteland`, `night.wasteland`, `combat.wasteland`, `mini.wasteland`,
  `camp.wasteland`, `boss.vorhollow`) point at the nearest existing drones and the stock boss track until the
  Wasteland's composed cues exist; Dead calm's planned silence works over them. Riftborn now roll after their den has
  set itself on them (so V4-V5's 15% applies), and a storm wave's Riftborn take the same numbers.
- Not in this build (later cooks): the Riftback's pocket into the Rift chest network (needs a remote container
  screen), the Riftback shield (shields aren't items yet), the fused creature calls and composed cues, Graveyard
  Riftborn and the Arcane <-> Void invasions (their lands don't exist), the null field's half refund, floating keels to
  mine, the Sea Throat seam, the Driftmote sensing caches / crust / seeds (it senses rifts), creatures fleeing litter
  fire.
- Unity-build hygiene: every new file's file-scope helpers carry a prefix (Vc / Vb / Vh / Vp / Vw / Va / Um), so no
  unity grouping can make them collide or hide another file's names.
- Build fix: `WRSwampVharra.cpp` turns off C4459 for itself (a unity build can put `WRSwampScenery.cpp`'s file-scope
  palette ahead of it).

## Unreleased: the Swamp (the Mire), the Blood land, and the Blood & curse path

Aaron (lanes build-biome-swamp + build-biome-swamp-finish: requirement ledger, research and plan in showcase-notes
cooks/build-biome-swamp.md, building cooks/biome-swamp.md). Built to compile (editor, adaptive and full unity); not
played here (Aaron and the testers test, F8).

- **The land.** A third landscape (`Landscape_Swamp`, 3,780 m square at 2 m) east of the Crown under its east face,
  joined to Storm peaks at the cliff foot and Stormgut's east mouth, to the island's tundra by the Rime Sound frozen-bog
  neck, and to the sea by the mangrove estuary in the north. `Tools/terrain/swamp.mjs` (keeps every cook distance from
  the cliff foot read off the storm heightmap), material `M_SwampAuto`, imported by `Tools/terrain/build_swamp.ps1`
  (3 camps with PlayerStarts tagged `swamp`, safe zones, starter stations). Five sub-zones (Reed Flats, Mangrove
  Thickets, Rotting Boardwalks, Murk Pools, Bone Mounds); the swamp's hollows are the sea's own water; four water looks
  (murk, duckweed, bleedwater, quag) drawn over a water-kind grid that the server reads too.
- **What stands on it** (`WRSwampWorld.cpp`, `WRSwampScenery.cpp`, all from `Data/swamp.json`): 7 landmarks (the Weeping
  Wall's five rust-red falls, Mother's Throat, the Bled Roc, the Walking House on its bird legs, the Hanging Cypress
  with its cages and the Coven key, the Mangrove Cathedral, the Corpse-Candle Mere), 24 hand sites, 76 kit POIs + 42
  fetish-post waymarks, 4 karst caves (the Eel Hole floods on the tide, the Peat Cellar digs through), boardwalks on
  posts sunk in the mud with rotten planks that creak 0.9 s and break, mendable with 2 wood (hold 4 game days), night
  lights, bald cypress / mangrove / reeds / pitcher plants / knees / bone piles you can harvest; bloodmoss, bog iron (3:1
  at the new bloomery), peat, salt, clay.
- **Hazards and events** (`WRSwampSystems.cpp`): bleedwater (bleed buildup while wading, edge tell, passes: sealed
  waders, swamp mounts, Mirehide 4, leech tar), quag (half speed, sink after 3 s still: no roll), marsh gas that any
  fire lights vent to vent, leech boils, the swamp's five weathers incl. Miasma, the Red Tide (5 min warning, M3-M5 turn
  to bleedwater, leech packs, the Bloat on the causeway, the Hex-Warden's night walk, the Reliquary opens), the Sunken
  Coven under the Deep Pool (ten rooms, four ways in, the Old Gar, chimes, the bell), secrets M-S1..M-S5, five shrines,
  Ruslana the Mire-witch with six quests, Blood <-> Lightning invasions through Stormgut, the stilt foundation piece.
- **Creatures** (`WRSwampCreatures.cpp`, now hooked into the creature code like Storm's): Murkjaw (death roll, blood
  rite taming), Gorgeleech and latch leeches, Hexcrow, Bogstalker (tongue + gulp), Mirestrider (mount, passes quag and
  bleedwater), Mudhulk (hauler), Bogsnout (finds, bloodmoss x2), Lanterntoad (light, shoulder pouch), Reedfowl,
  Slimeshell; day/night tables per sub-zone; living-world beats; voices from real recordings.
- **Bosses**: Mother Maw (`WRSwampBosses.cpp`: nest, boardwalk counter, nest fire, young), the Hex-Warden (four totems,
  mask break), the Bloat (roaming 3.5 km route, gorge meter, salt/fire/frost counters, veins), and Vharra
  (`WRSwampVharra.cpp`: the Throat, six lightning rods, the flood, three phases incl. the hidden third, the Blood altar).
- **The Blood & curse path** (`Data/paths/blood.json`, `WRBloodPath.*`, `WRAbilitiesBlood.*`): 22 nodes, the Spilled bar
  (HP costs, rally, no floor), Bloodthirst, passives, combos with Fire/Frost/Lightning, the body change (veins, lips,
  canines, iris, pulse, lifesteal flush, Sanguine lord glow).
- **Audio**: 18 Kevin MacLeod pieces (explore A/B/C, night, combat, mini, bosses with Vharra's three phases, Sunken
  Coven, Red Tide layer, camp), 14 beds, 14 creature voice sets, hazard and Blood sounds (`Docs/CREDITS.md`).
- Not finished: creature bodies are retinted rigs with code-built parts (no sculpts); bald cypress uses the island's
  pine meshes and mangrove/reeds/knees are drawn shapes until swamp art exists; the Three-Jaw Glaive blueprint waits for
  the boss-weapon cook; swamp portal folds stay off (`portals.json land: false`); the Coven pillar red in the tide warning
  waits for the Ring pillars; vein reach by stage needs a new hero mask channel.

## Unreleased: Storm peaks, the Lightning land (the first land off the Old Island) and the Lightning path

Aaron (lane build-biome-lightning: requirement ledger, research and plan in showcase-notes cooks/build-biome-lightning.md,
building cooks/biome-lightning.md). Built to compile (editor, adaptive build); not played here (Aaron and the testers
test, F8).

- **The land.** A second landscape north of the island (`Landscape_StormPeaks`, 4 km square at 2 m) across the Ash
  Sound, joined by the north-west neck: the Wind Passes' grass coast and three valleys, the slate benches of the Cliff
  Paths, three slot gorges with the Bridge of Nine Spans over the Thundergorge, Galvara's needle, the Struck Crags and
  the Rodfield, the Hanging Monastery's ledges, the Ring terrace at 900 m and the Spire at 1,630 m with the Last Stair
  spiralling to Kharvox's nest; Stormgut's west mouth. Made by `Tools/terrain/storm_peaks.mjs` (landmark heights from the
  cook, the Rodwardens' graded roads and stairs carved last so every route walks), material `M_StormAuto` (grass, slate
  scree, granite with ochre lichen, snow in high hollows, sand by the Desert rib, slate/granite cliffs projected
  sideways), imported by `Tools/terrain/build_storm.ps1`. The sea, the swimming water, the wildlife walk map and the
  journal map now cover both lands.
- **What stands on it** (`WRStormWorld.cpp`, all from `Data/storm_peaks.json`): the ten landmarks (the Bell Rod with its
  guy chains and bell-cages, Strikerock's glass-faced split and Vesna's camp, the Thunderstone Arch with its hanging
  axe-head on copper chains, Galvara's Eyrie and its banner cloud, the Copperfall's green fall and pools, the Glass Scar,
  the Hanging Monastery's whitewashed cells and chain walks, the Storm Mill, the Summit Nest of pine trunks and stolen
  rods, the Lightning pillar on the Ring), 30 hand sites (camps with their wrecks, the Feeding Stones, the Melted Cell,
  the kitchens' winch, the Stormgut fort with Coven hooks, the Burnt Drake, the Rust Pools, the Struck Climbers, the
  Rams' Ring, the Bellwether's fold, the Warden Door...), 99 kit POIs (bidental rings, fulgurite fields, copper and tin
  outcrops, basking rocks, folds, tarns, waystations, kite roosts, mound fields, kill scenes, struck pines) and 228
  Rodwarden rods (waymarks every 150 m, the Rodfield's 176). Six rope bridges with real planks: rotten ones creak and
  go, anyone can lay a new plank (2 wood), gaps mend in 2 game days. Caves (the Copper Adit, the Halite Cave's pink rock
  salt, Thundergorge Grotto), site caches, 12,000 pines, boulders, scree and shrubs. Night lights, storm glow.
- **Lightning** (`WRStorm::CallStrike`): above 600 m in a Thunderstorm the storm picks the highest body in each 60 m cell
  (heavy armour x1.25, Stormweave x0.85; Grounded, a grounding charm or the Grounding salve only if nobody else is
  there, a Rodcrest within 30 m draws it): 3 s of body tell (your hair lifts, St Elmo's fire on your hands and head, a
  rising buzz), 1 s of violet ground glow with the stepped leader crawling down, then the stroke on the spot (never
  following you): 60 damage and a stun, a ground current on wet ground (jump it), thunder after the distance, rods
  within 3 m taking it, fulgurite left on granite and sand. The compass shows your height and turns violet where
  strikes can find you. Gusts on the marked cliff paths and the Last Stair (brace with Block, crouch), rockfall under
  overhangs, ball lightning on the Crags in storms, stormglass after every storm (fields refill, the Rodfield's rod
  feet, the Glass Scar glows 2 game hours).
- **Creatures** (9: Voltfang, Thunderhorn, Stormkite, Raiju, Cragstrider, Galeback, Coilburrow, Rodcrest, Puffram) with
  every attack row of the cook, their storm behaviour, living-world beats, drops and tame uses; **minis** the
  Bellwether (roaming with his herd, the bell part, charge into rock), Galvara (three shootable cloud cores, the drop,
  retaking the sky), Coilback (rings along the copper veins, metal-seeking, frost slows it); **Kharvox, the Storm Roc**
  (Charge on its feathers, bleed and curse counters, wing parts, bone drop, rod chain, eye bolts with stepped leaders,
  the Thunderclap, called strikes with rod shadow, the hidden eye of the storm) at the Summit Nest with its altar.
  Bodies are the nearest existing rigs retinted with code-built parts (sculpts need the art PC).
- **Shrines, Vesna Groza and her quests.** The Great Shrine of Strikerock and four lesser shrines (copper rod-tip
  braziers; the Glass Scar's gains a Raiju in storms); Vesna at her climbers' camp with seven summit quests (Glass, the
  rod-road, Hair on end, Nine spans with the winch-hut lanterns, Banner, the bell, the Crown) and her Unlearning line.
- **The Great Storm** (world event): warning, the Front, forty minutes of strikes everywhere in the peaks (faster above
  600 m, never in claims or camps), ball lightning, war drums over the music, fulgurite x3 after.
- **The Lightning path** (all 22 nodes, Charge, Grounded, procs, Fire+Lightning and Frost+Lightning combos; path files
  drive the main menu's path picker and the UI's path colours).
- **Sound and music**: real compositions and recordings (CC0 / CC-BY, `Docs/CREDITS.md`): Storm pools for explore, night,
  combat, the minis, Kharvox, the monastery, the camps, Stormgut and the Great Storm; ten creature voice families; 23
  storm events; eleven ambient beds by sub-zone (the Rodfield's aeolian harp).
- **Items and recipes**: storm essence, thunderstone, every creature material, trophies and the Crown Plume talisman,
  the Stormheart (every fifth hit chains), Grounding salve (a timed buff), stormglass arrows, the Rodwarden spear, rock
  salt, lore items. Deeds, artifacts and the portal row now use the land's real id (`storm_peaks`).
- Fixes found on the way: `rescueTraceTop` raised to 2,000 m (the Spire is taller than 1,500 m); world settling traces
  from 2,000 m; a unity-build name clash in `WRGameMode.cpp`.

## Unreleased: the endgame (prestige, the Heron's draught, soul forging, mythic rules, boards, deeds, titles, cosmetics, monuments, the Thaw Hall raid)

Aaron (lane build-endgame: research, requirement ledger and plan in showcase-notes cooks/build-endgame.md, following
cooks/endgame.md 2.7-2.12 and biome-void.md 2.23.2-2.23.9, Raven's DD Endgame, DR #9 and DR #22). Built to compile
(editor, adaptive build); not played here (Aaron and the testers test, F8).

- **Prestige** (`WRDeeds`): a path at 100 earned points, at its boss altar, held Return (1.5 s): XP and ranks go back to
  the starter, the layout is saved, and you carry one mark of it for good (3 at most): Keener (+5% that path's damage),
  Deeper (+10% its resource capacity and gain) or Rooted (its root passive at half on the subclass, once a path). Marks
  work only while the path is main or subclass. Gold threads in the path's skin marks (0.12 a mark), "Ember-sworn" /
  "Rime-sworn" at three. **Restore layout** (Paths tab, altar, Deeds > Marks) buys the old layout back as points come;
  its nodes carry a gold dot until then.
- **The Heron's draught** (respec, DD / DR #9): a rare drop (great bosses' repeat kills 3% + 0.5% a party AL, portal
  guardians rank 3/4/5 2/4/6%, war-hall champions 3%) and the last step of each path master's new four-step line, *The
  Unlearning* (title Unlearned). Drink it from the bag: pick your main path or subclass, hold Forget; its ranks go, XP
  stays, every point back. Paths set aside at an altar can't be picked (they keep their ranks).
- **Boss soul forging**: a great boss's soul to every looter (first kill always, then 10% + 1% a party AL, x2 in its
  hidden phase 3, always on a first kill at AL 10+). At its altar (Forge soul page): A awakens its blueprint weapon
  (Cinderbrand / Rimefang: starsteel's metal factor, element fixed, its **Soul art** in a quick slot while held:
  Vulkarn's Flood, Pillar-breaker), B makes its talisman (Forge-heart, Rime-scale); a second soul buys the other, spares
  give core shards. `Data/souls.json` has all 12 majors; only built bosses show.
- **Mythic gear, data-ready**: `Data/mythic.json` (the Ring rule: 10 of each of the 12 signature resources, 3 Champion's
  marks, a +10 masterwork starsteel piece; x1.15, 3 sockets, two elements). Items now carry `mark` (awakened / mythic)
  and `element2` (blows alternate the two), saved, shown on the item card; `live: false` until the Ring exists.
- **Kill-time boards** (`WRRecords`): every great boss, lesser boss, portal guardian and war-hall timed from the first
  survivor blow; bands by group size (tames count), artifact runs in their own column with AL; world document
  `records`, never reset; server setting **Records season** starts fresh boards and keeps old ones readable. Granite
  tablets by each boss's shrine-stone / summoning stone and the war-hall gate show the top three (letters regrind when
  a record falls); E opens the full board.
- **Deeds** (journal tab, key `.`): 150 achievements in 10 categories (`Data/achievements.json`, 20 hidden; only rows
  whose content is in this build are live), progress shown, a brass toast for 3 s when one lands. **Titles** (from
  deeds, prestige, raids, the Banes, the masters' last tasks) under your name and before it in chat. **Cosmetics**
  (`Data/cosmetics.json`, only from hard content): weapon glows (deathless great-boss kills), armour dyes and raid
  marks (war-hall champions, 3%), banner sigils of any boss you've beaten (woven on your banners and drawn on your
  claim's map mark). **Marks** page for prestige.
- **Monuments**: a great boss's trophy head (first kill always, 25% after) + stone + bronze builds its monument: a
  granite plinth with a carved niche, the boss's own head on a bronze collar, a bronze plaque with its owners' best kill
  time, names and highest AL (knot border at 15). Comfort 3. A claim with one shows that boss's sigil on the map.
- **Raids for 6 to 10: the Thaw Hall** (`WRWarHall`, `Data/raids.json`): its gate cut into the ridge by the North pass
  road (stair, oak doors, braziers, the two sigils on the lintel; run-back stone 40 m back, record tablet by the stair).
  The hall (built at runtime in hall space): 112 x 72 m of 4 m basalt flags over a lava sump, basalt ashlar west, glacier
  ice east. The Fallen Smith's slams turn flags to lava, the Burnt Wyrm's sweeps and breath to ice, and a flag flipped
  from one to the other within 3 s bursts in steam (150, champions too). Rivals: their blows hit each other at half; one
  driven under 30% while the other is above gives the other Triumph. The door seals when the first raider crosses the
  bronze line; opens on the win (2 min loot) or when no raider is inside for 8 s (reset; bags go to the gate). Loot per
  champion by the 10% rule (path materials, Champion's mark, 3 upgrade shards, 3% raid cosmetic, 3% Heron's draught).
  Server settings: raid group cap / tuned for (now shown), **Raid respawn** 1-168 h (24). The other five halls are data
  rows, hidden until their lands exist.
- Server: `deeds ...` and `warhall ...` commands (everything checked on the server); the deeds save in each character
  (`deeds`). Hall space counts as its two lands (west / east), sheltered, and its ice flags are slick for movement.

## Unreleased: the world store (SQLite), identity, player raiding, raid group settings

Aaron (lane build-server-raids: research, requirement ledger and plan in showcase-notes cooks/build-server-raids.md,
following Raven's M7 1, server-settings.md and cooks/multiplayer.md, survival-crafting.md 2.16 / 2.18 and
biome-crystal.md 2.21.4-2.21.6). Built to compile (editor, adaptive and full unity); not played here (Aaron and the
testers test, F8).

- **The world store is SQLite** (M7 1): each world keeps everything in `Saved/Worlds/<id>/world.db` (the engine's
  SQLiteCore plugin, now enabled; WAL, exclusive: one server per world folder). Tables from versioned migrations in
  `Data/db/migrations/*.sql` (applied past `PRAGMA user_version` at start): `accounts`, `characters` (one row per
  character, its JSON as the "inventory blob"), `world_docs` (world, clans, wildlife, portals, world_systems, raid),
  `backups`, `meta`. The game thread hands rows to a writer thread that commits everything pending in one transaction
  every `store.flushSeconds` (10) and at once on logout and shutdown; reads see pending writes. `WRStore.h/.cpp`.
- **No wipe**: a 0.7 world's JSON saves (world, clans, wildlife, portals, world systems, players/) are brought in on its
  first start in one transaction, after a pre-import snapshot; the files move to `json-before-sqlite/` (never deleted).
  `info.json` and `settings.json` stay files (the menu reads them).
- **Hourly backups**: `VACUUM INTO` snapshots in the world's `backups/` folder every hour while it runs (keep 24), the
  first of each UTC day kept as a daily (keep 7), one on every clean shutdown (keep 5), and the import one (kept). A
  damaged `world.db` (`quick_check` fails) is set aside and the newest backup that passes comes back, with an Error in the
  log. `-wrstore=json` keeps the old JSON files (Raven's headless test tools read and delete them).
- **Identity** (M7 1: "name + generated token in the client's Saved dir; Steam/EOS later"): every PC makes a 32-byte key
  (`Saved/Identity/key.bin`, OS crypto random) and joins with `?WRKey=<SHA-256 token>`; the world's `accounts` table keeps
  a hash per name. The first join claims a name; another PC's join with it is refused at `PreLogin` with "The name X
  belongs to someone else on this world". Names from before claim on their next join. A Steam or EOS account id, once
  an online subsystem gives one, is bound to the name too and lets that account in from any PC. Steam needs a paid app
  id (not now); EOS is free but needs Aaron's product registration (HANDOFF has the steps). `WRIdentity.h/.cpp`.
- **Player raiding** (`WRRaiding.h/.cpp`, numbers in `tuning.json raiding`): one rule for every player-side source that
  hurts a building (weapons, charges, fire pots, path spells, burning ground, tames): a share per source x material, x
  the path's multiplier (fire x2 on wood: Raven's "fire burns wooden walls"; lightning x1.5 metal, spore x1.5 wood, void
  x1.25, arcane x1.25 stone as data for when they ship), then offline protection as before. Only on PvP worlds and never
  your own or your clan's.
  - **Black powder** is the real 75/15/10: saltpetre 2 + charcoal 1 + sulfur 1 at the alchemy table (L3). **Saltpetre**
    (white nitre crust on a dry cave rock, pick) and **clay** (a wet clay bank, any tool) are new nodes in every cave.
  - **Fire pot** (alchemy L2: clay, sulfur, fat): thrown from the ranged slot; aiming shows its arc (glowing dots and a
    ring where it lands; it never homes); a modelled clay jar in the hand and in flight; it breaks into shards and fire,
    hurts and burns anyone in 2 m (not your clan), hits wood pieces for 150 and leaves 10 s of burning ground that burns
    wood (and hurts) as it goes.
  - **Powder charge, the petard** (workbench L3: powder 4, bronze 2, wood 2, fiber 2): use it facing a wall, door, floor
    or Hearth: a bronze petard on an oak plank sticks to that face, its slow match burns down to the touch hole over 5 s
    (sparks, glow, hiss), then a 4 m blast: wood 800, stone 600, metal 300, biome 250, Hearth 300 to the piece it's on,
    half to the others in reach; 120 to people in 4 m falling to half at the edge, unblockable, behind cover safe, the
    placer not spared, clanmates spared. Raid costs hold: wood wall 1, stone 2, metal 9, biome 12. A charge on a piece
    that breaks first drops and still goes off.
  - **Mining charge** (powder 2, clay, hide): tamped into an ore, sulfur, nitre or rock node: it bursts the lot, x3, into
    a heap on the ground.
  - **Siege ladder** (workbench L3: wood 20, rope 6, iron 1): leaned on a wall, palisade or gate up to 6 m (its foot
    1.6 m out, its iron hooks over the top). Walk into it to climb (a new predicted movement mode on the survivor's
    movement component: up and down the rails, jump to drop, a hop over the top, knocked off by a stagger). 300 HP,
    weapons chop it down; it lasts 10 minutes and falls with its wall.
  - **Code breaker** (workbench L4, needs resonant crystal: hidden until the Crystal caves ship): 60 s held still at a
    code-locked door, ringing 40 m; the owners' bell rings at the start; the lock opens for you at the end.
  - **The raid bell**: every claim totem now carries a modelled bronze bell (a real bell's profile with its sound bow,
    yoke, pin and clapper) on an iron bracket strapped round the post. A player-side hit on the claim (or a creature
    raid's warning and arrival) strikes it: it swings as a damped pendulum with the clapper lagging, the same on every
    machine, and rings (synthesised from bell partials) out to 300 m. Owners who are on get a "Claim attacked" card with
    who and how far; the map shows the claim UNDER ATTACK for a minute; owners who were away get a "While you were away"
    card (who, when, what broke) at their next login. The record (last 10 raids per claim) is in the world store.
  - **Tames in raids**: "Break that" on the whistle wheel (and a tame's own wheel): biting tames in earshot go for the
    building under your crosshair at their bite x0.5 (a fire tame's bite burns wood x2), then the next piece of the same
    owner beside it, until called off.
  - **Perks**: Siege-proof (walls you build take a quarter less from charges, saved per piece) and Silent charges (your
    charges heard at a third of the distance) are live.
- **Full-release server settings** (Raven's server-settings.md): portal break timers, artifact limits and invasion
  frequency were built by the portals and world-systems lanes; **raid group sizes** are new: `raidGroupMax` (0 = no cap,
  DR #22 "tuned for, not capped") and `raidGroupTuned` (8) in every preset, read by `FWRRaiding::RaidGroupMax/Tuned` for
  the endgame war-halls; their settings rows show once those raids exist (`raiding.warHallsLive`).
- New binaries: 8 synthesised OGGs (`Data/Audio/raid`, 235 KB; `Tools/audio/synth_raid.py`) and 8 item icons
  (`Data/UI/Icons`, 60 KB; `Tools/art/make_raid_icons.py`).

## Unreleased: dungeon portals and artifacts

Aaron (lane build-portals-artifacts: research, requirement ledger and plan in showcase-notes cooks/build-portals-artifacts.md,
following cooks/endgame.md 2.5 / 2.6 and biome-desert 2.22). Built to compile (editor, adaptive and full unity); not
played here (Aaron and the testers test, F8). Data: `Data/portals.json`, `Data/portal_rooms/volcanic.json` and
`tundra.json`, `Data/artifacts.json`; a new land adds its kit file and flips `land` on.

- **Portals** open at random in each land (Volcanic and Frozen tundra: four slots each, Common / Uncommon / Rare / a high
  slot that's Epic, Legendary 5%), deeper ranks in deeper bands, ≥150 m from claims, shrines, villages, camps, boss
  arenas and other portals, never on the ridge's passes, on flat ground. The look: 14 obsidian shards (or 12 glacier-ice
  blades) round a turning tear in the rank's colour (bone white, green, blue, violet, flame orange) on a cracked plinth
  set into the ground, a light pillar over it (60 m; a Legendary's 300 m, seen across the land), embers or snow drawn into
  it, a hum by rank. A new Legendary is told to everyone.
- **Breaking** on real days (1 / 3 / 6 / 10 / 18, times the new **Portal break timers** setting: half / normal / double /
  never): cracking at half (the pillar flickers, cracks glow, a pack of its creatures steps out every 30 game min),
  spilling at four fifths (every 15 min, and a 3,000 HP Warden of the Fold at the mouth), then the boss **walks out**, the
  portal closes, and the boss roams 400 m round the spot, holding that rank of that land shut until it's killed (its roll
  and shard to everyone who did 10%). A run inside holds the clock; winning it ends the break.
- **The card** (E at a portal): rank, land, state and time left, the modifiers before you go in, the owner of a key
  portal, who'll come; Enter / Back. Everyone within 10 m gets a 5 s Come / Stay card; others can follow until the boss
  door seals. Up to 8 runs at once (the new **Portal runs** setting).
- **The fold**: one of the land's handmade graphs (4 a rank) filled with its handmade rooms (33 Volcanic: the Smiths'
  forge halls; 32 Tundra: the Rime-folk's Blue Below) in a closed bay high over the sea east of the island (Valheim's
  way): coursed basalt ashlar or glacier-ice walls with door sockets and frames, flagstone floors, beamed ceilings,
  corridors, and each room's pillars, ledges with steps, lava runnels (they burn), hanging crucibles, anvils, kilns, racks,
  statues of smiths or inuksuit, bones, icicles, black-water pits with thick and thin ice, and lamps (iron cages of coals;
  seal-oil lamps in ice niches). The entry stone is the run-back and the way out. A fight's or a trial's door stays shut
  until the room is won and then slides up into its lintel; the boss door drops behind the first one through. Inside
  it's the land's music, the cave's quiet, no weather; the map draws only the rooms you've walked into.
- **Modifiers** (0 / 1 / 1-2 / 2 / 3 by rank, +25% loot each): No healing, Double damage, Darkness, Iron hide, Frenzy,
  Venomous, Brittle gear, No tames, Bloodied, Swarming.
- **Traps** (each tells ≥0.9 s ahead, can kill, hurts creatures too, has an answer): pulse grates over lava channels
  (1.2 s red glow and hiss; plug one with a loose anvil block), slag chutes (a bell 1 s before; cut the rope to pour it
  early), icicles (a crack and a growing shadow; any blow or flame drops them early), thin ice over black water (a creak
  and white cracks; walk the dark blue).
- **Trials**: quench the blade (the lintel's temper colour picks the trough: straw oil, purple water, blue brine), the
  bellows rhythm (pump in the order the hammer automaton strikes), the sun's braziers (light dawn to dusk, east to west;
  fire from a torch, the fire path or the seal-oil lamp), the aurora lock (turn three ice prisms to the bands overhead).
- **Bosses** (one a rank a land, attack rows in attacks.json, numbers in tuning): Cinderfang Alpha (its mane relit at the
  runnels; frost puts it out), the Kilnmite Broodqueen (her clutches lay mites until broken), the Mould-Born (frost
  hardens it, fire feeds it; tip the hanging crucibles on it), Brannoch the Bellows-Keeper (break both chest vents: no
  breath, staggered), Böðvildr the Smith's Daughter (bait her slam onto a quench tank's rim), the Rimeclaw Alpha (ice
  plates until fire melts them), the Glacimaw Elder (erupting under the thick ice stuns it), the Frostbound Thurs (its
  ice shell until it burns or bleeds), the Rime-Witch (her freeze fails in a lit brazier's light; she snuffs them),
  Hrímgerðr (break the frost bolts on the dawn window: in the light she slows and turns toward stone).
- **Rewards**: personal chests (a vault at Legendary), the boss roll and rank shard to everyone who did 10%, upgrade
  shards, blueprints, fine or masterwork gear, artifacts at Rare 3% / Epic 8% / Legendary 15%, each boss's trophy.
- **Keys**: ten (a land's five ranks) at the alchemy table from rank shards and its signature (sulfur crystal, lichen);
  use one in its land and a portal of that rank tears open in front of you, sealed to you, your clan and your band; it
  never breaks.
- **Artifacts** (journal tab, N): three belt hooks, Weight I-V (bone, bronze, silver, blackened iron, starsteel),
  **Artifact Level** = their sum (the new **Artifact level cap** and **Artifact hooks** settings). Per AL: +7% damage
  taken from creatures, bosses and fold traps, +5% loot, +4% path XP, boss parts +1 point, dropped gear a step better
  2%, drops +1 point; Scarred boss parts from AL 5 (their effect ×1.5), Weights III-V from AL 10. The party's highest
  AL counts (clan or **band**: ask anyone in from the same tab); weird rules bind only the wearer; player blows pause it all
  30 s and safe zones pause it. Hidden third phases open when the party with most of the ledger wears AL 1+. Tying a
  charm on takes 2 s, out of a fight, never in a fold. Thirty artifacts, each one geis (no roll, half potions, light
  halved, lightning finds you, no enemy bars, rooted drinking, double knockback, block costs double, regen only moving,
  slow regen, freeze when winded, short parry, healing capped, the Feather, no block, brittle falls, loud spells, tames
  that won't fight, no run-back, no HUD, the hollow coin, never turn back, blood-price, never sunwise, the challenge, the
  Hag's cup, Hive Seed, Forge-brand, Omen of the End). Geis records pay XP once per new highest AL on a great boss, a
  Scarred part at 5, a title at 15. Fuse two of a kind (+1 Weight) or salvage for essence at the alchemy table.
- Code: `WRPortals.*`, `WRPortalFold.cpp`, `WRPortalThings.cpp`, `WRPortalBosses.cpp`, `WRArtifacts.*`, `WRUIPortals.cpp`;
  hooks in ApplyHit, the roll / guard / parry, potions, regen, light, HUD, boss move choice and loot, path XP, the map.
  Dev: `WRCmd portal dev open <land> <rank> | crack | spill | break | clear`, `WRCmd art dev give <id> <weight>`.
- Not finished: portals open only in the two lands that exist (the other ten lands' kits and 50 bosses wait); rank 3-5
  bosses borrow built rigs with their colours (their own sculpts, the Slag thrall and Rimecrawler aren't made); the new
  creatures' voices are pitched shared calls; the Rime-Witch's mirror clones, the Crown flicker and the music's "through
  stone" reverb aren't built; Connla's challenge doesn't double boss chase yet and Connla's third geis isn't wired (raid
  artifacts, no raids yet); only four artifacts can drop on a two-boss island.

## Unreleased: world systems (shrines and fast travel, path masters and quests, villages, factions and merchants, message stones, Starfall, the Eruption and the Whiteout, invasions, day and night tables, the hidden tubes, summoning stones)

Aaron (lane build-world-systems: research, requirement ledger and plan in showcase-notes cooks/build-world-systems.md,
following cooks/fire-frost.md 3.1 / 3.3.x / 3.4.x / 3.8, world-map.md 2.7-2.9, endgame.md 2.8). Built to compile
(editor, adaptive and full unity); not played here (Aaron and the testers test, F8). Everything is `Data/world.json`
rows: a new biome adds shrines, a master and quests, a village, a caravan, events, an invasion pair and its tubes.

- **Shrines** (10: a Great Shrine in each village, four lesser per land): a hexagonal three-tier dais with steps, a
  hexagonal pillar and a riveted iron fire-bowl on three straps, 7 (9) standing stones of the land's rock with a pale
  rune line, slag cairns and sulfur crust (volcanic) or inuksuit and red / blue / yellow prayer strips (tundra). Dark:
  the runes glow (a cold silhouette at night); lit: a fire and a warm light seen across the land. A dark lesser shrine
  is guarded (its sub-zone's packs, in a ring) when you come near; clear them, then Interact to kindle (3 s, standing
  still; a blow or a step off puts it out). Kindling lights it for the world, makes it found for you, your clan and
  everyone standing with you, and pays 1,000 XP to the land's path if it's yours (else 500 to each of yours); a Great
  Shrine pays 2,000 on your first visit; a re-kindle 20%. Four game days later a lesser shrine goes dark and its guards
  come back. Saved in the world (`world_systems.json`).
- **Fast travel**: Interact at a lit shrine (the Shrine screen) or pick a found shrine on the map: travel from a lit
  shrine to any found one, not in a fight, not mounted, not to one invaders hold; a fade, then you arrive on its steps
  with your following tames within 30 m.
- **Path masters**: Sigrun Ashhand (Fire, Emberwick, at her anvil and quench trough) and Áilu the Tracker (Frost,
  Rimehold, at his hide frame), on the MetaHuman hero rig with their path marks, working on a loop (the anvil ring and
  sparks), turning to you when you come near and saying a short line (no camera grab). Talk: their task, progress,
  what it pays, Take it / Hand it in. Six quests each (bring, quench a fire weapon in water, kill while burning /
  slowed, kindle shrines, show a trophy, sneak past sleeping Gloomhowls, tame a Woolly drudge, kill the roaming mini)
  paying path XP, recipes (Cinder salve, Obsidian arrow, Spiked boots: new items) and shards. Journal **Quests** tab
  (U): each master's task and progress, standing, every shrine's state, events and invasions on.
- **Villages and factions**: Emberwick (basalt round houses, turf-and-ash roofs with glowing smoke holes) and Rimehold
  (hide lavvu, drying racks with fish) round their Great Shrines, 250 m inland of each middle camp, each a safe zone
  with door lanterns, a flagstone path and the merchant's stall. The Forgebound and the Rime-kin: standing from
  quests, kindling, trade and defence, tiers Stranger / Known / Trusted / Kin (10% / 20% off).
- **Traveling merchants**: Brannoc (volcanic) and Ivalo (tundra) with an outrider, walking their side's loop (village,
  camps, both pass mouths) at 3 m/s, an hour at each stop, placed by the world clock (there when you arrive), out only
  near players. Trade screen: barter rows by standing tier. Creatures can kill them (players can't): a tenth of the
  stock falls on the road and the caravan's back from home the next game day.
- **Message stones**: craft a Scratch-stone (by hand), use it: pick a template, a word (five lists), optionally a
  conjunction and a second part (all buttons: pad and mouse), and the words are cut into a leaning stone with a cloth
  tied round it. Read: Good heals the author 20% if they're on (told later if not), Poor; net -3 hides it; 10 each,
  the oldest crumbles.
- **Starfall** (world events setting): from dusk every ~2 real days, meteors streak across the whole sky for 5 min,
  then 3-5 fall (a fire-trailing rock, a glow where it lands, the roar) on midland / highland ground away from claims,
  camps, shrines, villages and arenas, each a 20 m crater with thrown rock and scorch round a star holding star iron
  6-10 (a gem 25%); its glow draws a midland pack after 2 game hours; it weathers away after a game day.
- **The Eruption** (biome events setting, volcanic): 5 min of tremors and a warning, then the plume over the crater,
  the `eruption` weather (80 m sight, ash), and from minute 10 to 50 lava bombs at everyone outdoors within 1.2 km: the
  launch boom, a glowing rock on a ballistic arc (never homes), an orange glow where it lands for the last 0.9 s, 70 +
  burn 50 unblockable through ApplyHit; a roof takes it (60 to the piece); never near camps or villages. Bomb cores to
  take (sulfur, iron ore, obsidian 20%); sulfur gathers x3; the Kiln Roads' Sealed Vault opens for the day.
- **The Whiteout** (tundra): the `whiteout` weather (25 m sight, glacier gusts, drifts), the night table runs by day
  (Gloomhowls out), then an aurora night with lichen x3.
- **Invasions** (invasions setting: off / rare / normal / frequent per pair per week): a 5 min warning card to the
  defending land, then 8-14 of the other land (by day or night table) and its roaming mini (Old Tusker / Cinderhorn
  leaves its loop) come out at the far mouth of a pass, march through it and on to the nearest claim (they break it as
  raiders do) or lesser shrine (they hold it: dark, travel shut) for 30 min. The tracker shows how many are left.
  Beaten: everyone who did 10% of their health gets 2,000 path XP, ember and frost essence and standing.
- **Day and night tables**: `spawn_regions.json night` per region (fire-frost 3.3.7 / 3.4.7): a night-only kind lies
  up by day, a day-only kind at night (diurnal herds still sleep in the open), new packs roll from the hour's table.
- **Hidden sub-biomes**: the Kiln Roads (volcanic) and the Blue Below (tundra), built as rock-roofed tubes along their
  route (walls of boulders sunk 3 m, roof slabs, skylights with an ash / snow heap under them, glowing crust seams or
  meltwater ice, icicles, lights every 25 m) with the sculpted cave shells as rooms opening off them: Kilnmites,
  Scorchwings, Rimeshells, iron nodes, stone coffers with lore and loot (refill every 4 game days) and the Sealed Vault
  behind a banded basalt slab that sinks when an eruption shakes it loose.
- **Summoning stones**: an altar stone with the boss's sigil glowing in its element, ringed by slag cairns or inuksuit,
  beside each major's shrine-stone and at each mini's lair. Effigies (8, alchemy table) are learned on your first loot
  from that boss; offering one raises a dead boss now (the stone flares, the call heard far off), and only the
  summoner's party (clan and whoever stood with them) can hurt it.
- Settings: World events / Biome events / Invasions (off / rare / normal / frequent) on the world settings screen and in
  every preset. Dev: `WRCmd world dev event eruption|whiteout|starfall | invasion 0|1 | meteor | found | shrine <id>
  lit|dark | quest <master> <n> | caravan`.
- Data: `Data/world.json` (new), weather.json (`eruption`, `whiteout`, chance 0: events only), spawn_regions.json
  (`night`), items.json (12), recipes.json (12, `blueprint`-gated), server_presets.json, fx.json (12 rows),
  Audio/sounds.json (18 rows, reused CC0 recordings). Icons: 12 PNGs in Data/UI/Icons (82 KB,
  `Tools/art/make_world_icons.py`). New code: `WRWorldSystems.*`, `WRWorldSites.cpp`, `WRWorldNpc.cpp`, `WRUIWorld.cpp`.
- Not finished: masters speak in text (no voice recordings) and Sigrun swings the chop clip with empty hands; merchants
  have no pack beasts; the tubes are boulder corridors on the island's ground (the routes cross terrain nobody has
  looked at yet), without the Flooded Tube's sinking crust, Hekla's Mouth, the moulin drop or the frozen waterfall; the
  eruption's river rise and spill channels, Magmaw / Scorchwing boosts and the event music layer aren't built; invaders
  leave no slick ice or melted snow; the Dimming and the Migration aren't built.

## Unreleased: taming (levels, bond and role trees, saddles and barding, ground / pack / swimming / flying mounts, the Floeback, breeding, eggs and the incubator, babies and imprinting, mutations)

Aaron (lane build-taming: FULL-GAME 6.5; research, requirement ledger and plan in showcase-notes cooks/build-taming.md,
numbers from cooks/survival-crafting.md 2.19-2.21). Built to compile (editor); not played here (Aaron and the testers
test, F8).

- **Creature levels**: every wild creature has a level by its band (lowland 1-10, midland 11-20, highland 21-30), each a
  point in one of six stats (health, stamina, damage, carry, speed, torpor); a stat's multiplier is held to +-10% for
  wild ones so combat tuning holds. Labels show it ("Cinderfang · Lv 14"). 1% of wild spawns are mutants (2% near
  Rare finder).
- **Bond levels 1-20** from the tame's own work: 1 XP per 10 damage it deals (half from the saddle), kill XP for kills
  it helped with (20 / 40 / 80 / 300 by tier), 1 per 50 m ridden, 1 per 100 m hauled loaded, 3 per ore dug or place
  sensed, 1 per 5 min lighting your dark, 2 per pen product; level n costs 100 + 40n, each +2% every stat and a skill
  point; your Taming skill gets 20 a level. A gold ring and its call when it levels.
- **Role trees** (`Data/tame_trees.json`): Fighter, Mount, Hauler, Utility, Farm, four skills in a line then one of two
  capstones, 20 points; every node works: Hardened, Bite force, Pack tactics, Guardian (takes 30% of blows on you
  through ApplyHit), Berserk, Stalwart; Sure-footed, Stride, Charge (sprint into an enemy: a 0.4 s lowered head, then 40
  and a knock), Steady seat, Warhorse, Courser; Broad back, Packing, Steady, Forager (what your bag can't take goes in
  its packs), Caravan, Bulwark; Keen sense, Tireless, Bright, Lucky find (gems), Seeker (calls finds to your clan),
  Lodestar (+15% gathering near it); Content, Bountiful, Fertile, Calm herd, Prize stock, Prolific (twins).
- **The Aurora fox senses** for you now: every minute it names the nearest cave or egg nest within 80 m and where.
- **Saddles and barding**: hide, bronze-fitted, iron and steel saddles (10-40% less damage to the mount; bareback you
  can't fight from its back and it takes 25% more), the pack saddle (+10 packs), iron and steel barding (20 / 30%,
  5% slower), Forgeplate barding (carries it across lava) and Rimeguard barding (full grip on ice); the other ten lands'
  barding is in data, hidden with its land. Quality rolls like armour; Saddle master and Creature armorer perks live.
  All modelled in code from their real parts on each creature's back: blanket, seat between pommel and cantle, skirts,
  girth round the belly, stirrups, breast collar, the tier's metal and rivets; the pack saddle's sawbuck, panniers,
  bedroll, lash ropes and breeching; barding's chest plate, flank plates, neck lames and face plate.
- **Mounts**: ground mounts gallop on their own stamina (Sprint, 200 stamina, shown on the HUD); haulers are pack
  mounts with packs by weight (Basaltback 300 kg, Woolly drudge 280 kg; slow past three quarters loaded), moved in the
  tame card; swimming mounts carry you over water; flying mounts take off (Jump), climb (hold Jump), dive (Dodge),
  glide down when out of wing stamina, 400 m ceiling, won't fly near a boss arena.
- **The Floeback** (tundra shore, offer it tundra fish, easy): a 3.2 m harp-seal build with a crust of sea ice on its
  back where you sit, silver-grey dappled coat, dark harp and muzzle, big dark eyes, whiskers, clawed fore flippers and
  fanned hind flippers, built and moved in code (a side-to-side stroke in water, a gallumph on land, breathing and
  looking about when still, a jerk when hit, rolling onto its side in death). It spawns only on the tundra's shore
  ground and slips into the sea when threatened. Rides through the water at 900, 250 on land.
- **Breeding**: tames have a sex; two of a kind (yours or your clan's), mating on, fed and within 10 m fill a mating
  bar over 2 min (they walk together, hearts and calls); she rests 4 h. Egg layers lay a fertilised egg in a straw
  scrape at her feet (E takes it); the rest carry young (30 min / 2 h / 6 h by difficulty, x the server's maturation:
  playtest 10x) and give birth beside her. Each stat comes from one parent (55% the higher); 3 mutation rolls at 2.5%
  (Breeder x1.5) add 2 points and a visible morph (pale, dark, bands, a glow line along the back), 20 per line at most.
- **The incubator** (Hearth 3: stone, iron, linen): a clay firebox with a stone-arched mouth, an iron grate, a straw
  tray and a brass-banded wicker hood; the egg sits in the straw and the firebox glows while it burns the egg's fuel
  (charcoal or sulfur for fire eggs, ice blocks for frost, wood for the rest). Out of fuel the egg waits. Stolen eggs
  go in too. Its screen shows the egg, the fire and your bag.
- **Babies** grow from 10% to full over 2 / 8 / 24 h (x maturation), weaker while small; every eighth of the way they
  ask for care (a bubble over them, the HUD card, a notice): feed a named food, walk 50 m beside you, or a cuddle.
  Each answered +12.5% imprint; the imprinter riding or fighting beside it gets up to +20% to its damage, toughness,
  speed and carry. Babies can't be ridden, mate or wear tack.
- **Hunger**: tames eat their food from their packs or a pen's trough within 20 m (or by hand from the card); starving
  for 30 min they waste away. Nothing ticks with no player near.
- **Element**: tames keep their own: their bites carry their element's build-up, barding passes stack with their own.
- **The tame card** (the tame wheel's new Tend wedge, mouse and pad): level, bond, the six stats with mutations and
  morphs, hunger and feeding, the role tree with Learn, saddle and barding (click to fit from your bag or take off),
  mating, pregnancy, growth, imprint and the care button, a hauler's packs beside your bag.
- Everything saves: genes, bond, tree, tack, packs, growth, care, pregnancy (and her young's genes), laid eggs, eggs in
  incubators. Dev: `WRTameDev now | bond N | grow X | care | mate | hatch`.
- Data: `Data/tame_trees.json` (new), creatures.json (Floeback, Windmane, birth / seat / back / carry / mountKind /
  habitat / procBody), items.json (5 saddles, 14 barding, 9 fertilised eggs, rope), recipes.json, pieces.json
  (incubator), spawn_regions.json, server_presets.json (maturationMultiplier), lifeskills.json (4 perks live),
  tuning.json `tame`, fx.json and Audio/sounds.json rows (reused CC0 recordings). Assets: `Content/Creatures/
  M_TameMorph.uasset` (31 KB), 29 icons in Data/UI/Icons (284 KB). New code: `WRTaming.*`, `WRTameLook.cpp`,
  `WRSealBody.*`, `WRUITameCard.cpp`, `WRUIIncubator.cpp`.
- Not finished: the Windmane never spawns (Sky islands not on the map) and has only a stand-in body for testing; ten
  lands' barding passes wait for their hazards; the Floeback is code-modelled, not sculpted; no courtship or cuddle
  clips (they walk to each other and face, the hero plays its pick-up); saddle fits per species are first estimates;
  stable, necro raising, revive, Soulbond and tame names aren't built.

## Unreleased: building (metal, the 12 biome sets, beams and posts, structural integrity, comfort decor, powered turrets)

Aaron (lane build-building: FULL-GAME 6.4; research, requirement ledger and plan in showcase-notes
cooks/build-building.md, following cooks/survival-crafting.md 2.2 / 2.17 / 2.18). Built to compile (editor and full
unity); not played here (Aaron and the testers test, F8).

- **Metal tier** (Hearth 4, span 8): the 12 shapes (foundation, wall, half wall, upper wall, doorway, door with a code
  lock, window, floor, stairs, roof, gable, pillar) at wall 2,500 / foundation 3,500 / door 2,800 HP, a wall costs 8
  steel plate + 10 stone, upkeep 0.1 steel plate a piece an hour. Riveted brushed-steel panels on a dark core over a
  45 cm stone footing, H-section corner posts, a steel mid-rail and top channel, riveted deck plates, standing-seam
  roofs, steel stairs with grip ribs, a strapped steel door.
- **The 12 biome building sets** (Hearth 5, span 8, wall 3,000 / foundation 4,200 / door 3,200, 20 biome blocks a
  wall, upkeep 0.1 block): Basalt hall (hexagonal basalt columns under an obsidian lintel, ember-glass windows, causeway
  hex pavers), Crag hold (granite with slate bands, verdigris copper flashing, storm shutters), Sun court (sandstone
  ashlar, round arches of voussoirs, a sunstone inlay band, barrel tiles), Heartwood lodge (a braced heartwood frame
  with amber in its notches, boards infill, moss on the shingles), Cloud spire (white ashlar, blue tile bands, arches
  with a levistone keystone hovering over them), Geode sanctum (dark stone with violet crystal druses, faceted crystal
  windows, pillars with a glowing core), Rime lodge (pine logs crossing at saddle-notched corners on a stone sill,
  carved prows on the gables, packed snow on the roof), Stilt house (mangrove stilts, woven reed walls, lacquered bands
  and bone pegs, thick thatch), Coral keep (coral-rag with coral sprigs, porthole windows of sea-glass, shell scallop
  roofs), Mourning chapel (grey ashlar with buttresses, pointed arches, stained glass in bruise colours, iron cresting),
  Mycelium den (quilted mycelium felt between root stems, shelf-fungus floors, a fungal cap roof with glowing gills),
  Hollow bastion (grey stone with voidglass slabs rimmed in violet). **Biome blocks**: workbench L5, 4 stone + the
  land's signature resource + a steel plate -> 4. One material, `M_BuildTinted`, draws all of them.
- **Each set's 6 furnishings and its rug** (72 + 12, modelled at furniture sizes in the set's colours: thrones,
  benches, tables, lamps, baths, beds, hammocks, racks, chimes, bells, drums, harps, a fountain, an aquarium, a star
  globe...), comfort group "biome set" 2 (the rug floor 2). A blueprint per set (`bp_<set>_decor`) teaches all seven and
  drops from the land's major boss (Vulkarn, Skaldrith on the island); the card says who drops it until you know it.
- **Freeform beams, posts and braces** in every tier (wood, stone, metal, each set): a 2 m and 4 m beam, a 3 m post, a
  45-degree brace, a new "Beams & posts" tab. The start goes where you aim (on a wall a beam runs out toward you, on a
  floor along your look; a post aimed under a floor props it), with snap on it takes another beam's end, a beam slopes
  with your look in 15-degree steps (5 on the fine grid). **Integrity**: Raven's span rule kept; a beam or post costs 1
  span step per 2 m it reaches out (a post standing up bears like a pillar), passes support to what rests on it, and a
  brace from a wall to a pillar or post lets that wall reach one more. **Stability colours** while the build bar is open
  (blue on the ground, green, yellow, orange, red at the last step) on every structure piece within 25 m, and on the
  ghost. **Collapse**: what loses its support falls as 4 to 8 rubble chunks of its own material with dust and a crack,
  a falling chunk deals 50 to whoever it lands on (ApplyHit), then they sink away (`AWRRubble`).
- **Comfort**: every decor piece of the comfort table now exists (bench, carved throne, long table, fur bed, tapestry,
  fireplace, candelabrum, standing lantern, crystal lamp, sunstone lamp, potted plant, flowering pot, wooden tub, hot
  spring bath on a vent, hand drum, lyre, mirror), their lamps light their rooms (`lights.decor_lamp`); the **Hearth
  offering** (a cooked meal left on a new Hearth in its first day: +2 at every bed in its claim, saved); the
  **Decorator** perk (+3, cap 23).
- **Powered turrets**: the **bolt turret** (Hearth 4, 6 P) is a springald on a slewing ring: the drive shaft joins at
  the back, a worm turns the ring, a spindle and cross shaft run a chain to the windlass that spans the steel bow; it
  finds hostile wild creatures within 30 m (on PvP servers, players not of your clan inside your claim), turns onto
  them, shows a red aim line for 0.9 s (following slowly, still for the last 0.3) and looses a crossbow bolt straight
  at 4,000 cm/s, 45 damage, every 1.5 s at full power (slower short of power). The **element turret** (Hearth 5, 8 P):
  ember essence throws a 6 m cone of fire (30 + burn 20), frost essence a slowing frost bolt, 50 shots an essence; the
  other paths' essences are rows waiting for their items. Machine screen: their state and shots left; Turret tech
  (+20%) live. They never move, chase or hurt what only stands near them.
- **Rules**: no base piece in a cave (lights, drills, dark crops and greenhouse lights still go underground); weapons
  and tools barely mark walls (wood a tenth, stone a fiftieth, metal and biome nothing: `materialDamage`). Metal and
  biome pieces give 12 Building XP; Master builder (a quarter off metal and biome pieces) and Ancient wood (a felled tree
  now and then gives its land's block) are live. The claim screen and totem take every upkeep material.
- Data: `Data/building_sets.json` (new), pieces.json, items.json (12 blocks, 12 blueprints), recipes.json, gear.json,
  machines.json (turret rows and ammo), lifeskills.json, tuning.json, fx.json, sounds.json (reused CC0 recordings).
  Assets: `Content/Building/M_BuildTinted.uasset`, `M_BuildOverlay.uasset` (47 KB). New code: `WRBuildSets.*`,
  `WRBuildIntegrity.*`, `WRBuildLookKit.h`, `WRBuildLookSets.cpp`, `WRBuildLookDecor.cpp`, `WRTurret.*`.
- Not finished: tundra hot springs aren't on the map (the bath goes by lava); ten sets and their decor wait for their
  lands; ten element-turret essences wait for their paths' items; turrets reuse the crossbow and spell sounds.

## Unreleased: machinery and power (water wheels, windmills, turbines, shafts, conveyors, sorting, drills, the 12 biome machines)

Aaron (lane build-machinery: FULL-GAME 6.3 machinery; research, requirement ledger and plan in showcase-notes
cooks/build-machinery.md, following cooks/survival-crafting.md 2.15 / 2.17 / 2.23 / 2.25). Built to compile (editor);
not played here (Aaron and the testers test, F8).

- **Power is rotation you can see** (`WRMachine.h/.cpp`, `Data/machines.json`). Sources end in a turning output stub,
  machines take one; pieces whose stubs meet (50 cm) share a **network**. A network runs at supply / load: short of
  power, every machine on it slows by that share and its shafts, gears and belts visibly turn slower (Factorio's rule,
  the spec's). Load counts machines switched on with work (an idle smelter draws nothing). Networks run only while a
  player is within 250 m; asleep, they hold where they were. Placing a **drive shaft**, **gearbox** or **conveyor**
  snaps its end onto the nearest stub in line, and its trestle legs reach the ground. A shaft whose end runs into a
  **station** (forge, armorer, alchemy table, workbench, oven, smokehouse...) powers it: a bronze pulley and a leather
  belt drive it, it draws 5 P while it has an order and runs x1.5 (its order under way is retimed as the power changes:
  `AWRStation::AppliedBoost`).
- **Sources**: the **water wheel** (Hearth 2: an undershot wheel 4 m across set 3 m out from a bank into the surf line,
  axle into a stone-pier headstock, outer bearing post in the water; 10 P x the water's run, pulsing with the swell),
  the **windmill** (Hearth 3: an octagonal smock mill whose boat cap turns into the wind on its curb while the fantail
  spins, four common sails of cloth on lattices; 6 P x a gusting wind, x2 in a storm, x1.5 on the storm peaks and sky
  islands; blocked by anything as tall within 10 m), the **geothermal turbine** (Hearth 4: a steam bore within 6 m of
  lava into a riveted drum, flywheel and stack; 25 P), and the soul engine, storm coil (60 P in a thunderstorm, a
  stormglass battery 2 h at 20 P), sun mirror array (six mirrors that follow the sun onto a boiler, 20 P by day, a
  sunstone battery 4 h at 10 P) and tide mill (20 P to 30 at high tide) for their lands.
- **Machines** (Hearth 3-4): **sawmill** (a sash saw on a crank and pitman rod: 10 wood -> 15 **planks**, and planks
  pay for wood in every build cost, one for one, planks first), **grinder** (millstones in a hurst frame driven through
  a crown wheel and stone nut at 4x: grain -> **flour**, **bone** -> 2 **bone meal**, 2 stone -> **stone dust**), **loom**
  (heddles and batten on a cam shaft: 8 fiber -> 3 linen, half again the bench's), **smelter** (a shaft furnace blown
  by cam-pressed bellows: every ingot recipe by itself, 6 s each), **irrigation pump** (a crank rocks a beam that works a
  piston in a barrel: within 40 m of water its trough is a channel source), **conveyor** (2 m: a stack every 2 s, items
  ride on the leather belt and cleats run round the drums; jams stop it), **sorting chest** (20 slots: sends each stack
  to a chest within 20 m that already holds that item, its bronze chute turning to it), **coil lamp** (light without
  fuel at night), **drill** (a cam-lifted stamp over an ore vein: 1 ore a minute into its 40-slot hopper; the vein never
  runs out while drilled; emptying it gives Mining XP), **greenhouse lights** (a walled room lit by them is a greenhouse
  with no glass, at night or underground).
- **Production chains**: machines joined by belts, 4 machines long (Chief engineer 6); a belt won't feed a 5th and says
  so. Reference chain: drill -> belt -> smelter -> belt -> sorting chest.
- **The 12 biome machines** (Hearth 5): **lava forge** (volcanic, over lava: every forge recipe x2 and steel from 2 iron
  with no charcoal), **cold store** (any land: every chest under the same roof within 8 m keeps food x0.05; free in the
  tundra, 6 P elsewhere), storm coil, sun mirror array, grove press (compost and now and then a rare seed), updraft lift
  (two linked lifts within 120 m; one levistone platform rides the guide cable between them in 10 s, eased, with riders
  standing on it on every machine), resonance pylon (joins every line within 40 m), blood still (bloodmoss -> slime in
  bog water), tide mill, soul engine, mycelium vat (mushrooms in the dark, fed compost) and rift chest (60 slots shared
  by every rift chest of your clan). The lava forge and cold store are live on the island; the other ten are built and
  stay hidden with their land (`biome` / new `liveWith` in pieces.json).
- **Wear and repair**: a running machine loses 2% of its health an hour (never all of it); at a quarter it runs at half
  speed ("Worn"). Repair on its screen costs half its build cost scaled by what's missing.
- **The machine screen** (`WRUIMachine.cpp`, E on any machine; mouse and pad): its state in a word and why, the power
  line (made, drawn, a % bar, sources and machines on it, this one's share and the factor behind it), what it's making
  and how far, a battery's charge or the soulstone burning, its switches (on / off, overclock, repair, link / send for
  lifts), its slots (click to take) and your bag (click to put in what it takes). The HUD prompt names its state too.
  `-oldui` gets a text panel with the same commands.
- **Engineering**: XP for placing a machine (30), every 5 min each of yours runs (1, up to 10 machines at once),
  repairing one (10) and casting **gears** (forge 2: 2 bronze + 1 wood, 15); every level +0.3% machine speed (was 0.5%
  on stations only: the spec's number, survival-crafting 2.23). Perks now live: Efficient power (-20% draw), Long lines
  (the 6 m shaft and 6 m belt), Overclock (also: set a machine to x1.5 at x2 power), Self-repair (also: your machines
  don't wear), Chief engineer (also: chains 6 long), Tinkerer (machines too). Turret tech and Siege engineer stay hidden.
- **Sound** (`Data/Audio/machine/`, `Tools/audio/build_machine_audio.py`, CC0 recordings in Docs/CREDITS.md): a loop
  per machine that follows its speed (fades in on spin-up, out on run-down) over 30 m, the stamp's knock on each blow,
  the sorting chute's ratchet, a line starting and stopping. Beasts now drop **bone**; flour cooks as grain; stone dust
  fertilises a plot like compost. Icons: `Tools/art/make_machine_icons.py`.
- Commands: `mon`, `moverclock`, `mput`, `mtake`, `mtakeall`, `mrepair`, `mlink`, `msend` (each with the machine's
  x y z). New files: `WRMachine.*`, `WRMachineTypes.h`, `WRMachineLook.*`, `WRUIMachine.cpp`, `Data/machines.json`.
- Not finished: the island has no rivers or steam vents, so water wheels run in the surf line and turbines by lava only
  (rivers and vents join `FWRMachines::FlowAt` / `LavaNear` when the map has them); turrets and siege weapons (the
  other two engineering perks) aren't built; ten biome machines wait for their lands.

## Unreleased: crafting (steel and starsteel, the four armour slots and 12 biome sets, rings, talismans, runes, salvage, blueprints)

Aaron (lane build-crafting: FULL-GAME 6.3; research, requirement ledger and plan in showcase-notes
cooks/build-crafting.md, following cooks/survival-crafting.md 2.13 / 2.14 and hero.md 2.5). Built to compile (editor);
not played here (Aaron and the testers test, F8).

- **Steel (Hearth 4) and starsteel (Hearth 5)**: a **charcoal kiln** (Hearth 2 station: a domed stone kiln with an
  arched fire mouth, vent holes round the crown, a chimney stub, split logs stacked to feed it and a heap of lump
  charcoal; 10 wood -> 4 charcoal, 2 min), **steel ingot** (2 iron + 2 charcoal) and **steel plate** at a tier 4 forge,
  **star iron** (a black-blue, silver-flecked meteoric vein in the deepest chamber of the two highland caves, above
  `caves.starIronMinZ`; smelted 2 ore -> 1), **core shards** (a spare major boss core broken at a tier 5 forge -> 10; a
  core fed to a Hearth is never touched) and **starsteel ingot** (2 steel + star iron + a core shard). Steel and
  starsteel in every weapon class (sword, dagger, spear, greatsword, great hammer, scythe: x2.2 / x2.8, `weapon.metal`);
  starsteel blades are modelled blue-black. Upgrades go to +8 / +10 at those tiers (Hearth x 2, as before).
- **Four armour slots**: Head, Chest, Hands, Legs, plus one **Talisman** and two **Rings**. Hide, copper, bronze and
  iron are now four pieces each (15 / 40 / 15 / 30 of the old armour, poise and weight; each piece keeps the old
  durability). Every worn piece wears on each blow (Valheim). Armour, poise, resists, equip load and carry weight sum the
  pieces; a broken piece gives nothing. **Old saves migrate**: the old armour is the chest piece and the matching hood,
  bracers and leggings are made at its quality, roll and infusion and put on. The hero shows **each slot's own piece**
  (mixed sets look mixed; hair hides by the head piece; the Ascendant crest needs an Ascendant helm). The inventory's
  doll: weapon, ranged and talisman by the figure, the four armour pieces in a row under it with the sets' lines, then
  the two rings and the light (it scrolls on short screens).
- **The 12 biome sets** (steel, armorer 4) and their **Ascendant** versions (starsteel, armorer 5, the starsteel inlay):
  bonuses at 2 and 4 worn pieces (`Data/gear.json sets`; Set crafter counts one more piece, now live). Live on the
  island: **Forgeplate** (steel plate, sulfur, obsidian, Magmaw hide): burn builds up half as fast; four pieces walk
  lava unhurt and Heat fills 15% faster. **Rimeguard** (steel plate, lichen, Rimeclaw spines, wool): frostbite half and
  blizzards don't wear you down; four pieces grip slick ice and Chill fills 15% faster. New drops: Magmaw hide, Rimeclaw
  spines, obsidian (Basaltback, the Obsidian Warden). Weather counters now name sets and pieces (`weather.json
  counters.sets`), and a hazard item counts worn as well as carried. The other 10 sets, their materials and bonuses are
  in data and hidden until their land is live.
- **Rings** (forge, at the metal's tier): a band of copper to starsteel set with a ruby, topaz or onyx: 60% of the
  gem's socket value, +10% a metal tier above copper; two ring slots, two of the same ring count once.
- **Talismans** (armorer 2): a mini-boss trophy + 2 of its land's signature resource. On the island: Obsidian heart (+10
  poise while you guard), Wisp crown (your spells build 5% more burn), Cinder horn (attacks out of a sprint +15%
  stance), Troll marrow (8 s unhurt: heal 0.5 HP a second), Stalker fang (a blizzard doesn't close in on your sight).
  Cinderhorn and Old Tusker now drop their trophies every kill like the other minis. All 40 talismans and the three
  hazard charms are in data; gear stats named like a life-skill stat (woodYield, swimSpeed, blockStamina, hungerDrain...)
  add to it through `UWRLifeSkillsComponent::StatFor`; corruption rate reads gear too.
- **Runes** (alchemy 3: resonant crystal + a land's material; hidden until the Crystal caves): sit in a weapon socket,
  one of each to a weapon. Thirst (lifesteal 3%), Haste (buildup +15%), the Anvil (stance +10%), the Tide (+3 stamina a
  hit), Thorns (melee attackers take 10% back), the Grave (kills +5 path resource), Clear Sight (crits +10%), Hollowing
  (ignore 10% armour): all wired in `ApplyHit`.
- **Salvage** (Crafting tab -> Salvage): any piece of gear in the bag at its station (weapons and rings at a forge,
  armour and talismans at an armorer, tools and bows where they're made): 40% of its metal and main material (+10% a
  quality step above crude), a quarter of the upgrade shards spent; its infusion and every socketed extra are listed in
  crimson before you confirm and destroyed. What the bag can't hold waits in the station's tray. +5 XP.
- **Removing extras** (Repair -> "Remove: Ruby" at a forge): frees the socket and destroys the extra; **Careful hands**
  (smithing perk, now live) saves it half the time.
- **Blueprints**: an item; right-click to read it and its recipe is yours for good (saved with you; the Crafting tab's
  **Blueprints** category is your known list). Already known: keep it for a clan mate. A major boss's first kill per
  survivor always drops one of its blueprints, later kills 20% (`gear.json blueprints`); no room in the bag, it waits in
  a bag only you can open. Vulkarn's **Cinderbrand** (great hammer, always fire, a stagger sends out the lava ring) and
  Skaldrith's **Rimefang** (spear, always frost, a blocked blow can freeze its maker): steel, forge 4. Portal odds by
  rank are in data (`FWRBlueprints::PortalDrop`) for the portals.
- New: `WRGear.h/.cpp` (`FWRGear`, `FWRBlueprints`, `AWRStation::Salvage / RemoveExtra`), `Data/gear.json`,
  `Tools/art/make_gear_icons.py` (260 icons). Commands: `salvage <bag slot>`, `unsocket <socket>`, `equipto <bag slot>
  <slot>`; testing: `WRHearthLevel <1-5>` sets the nearest Hearth's tech level.
- Not finished: steel and starsteel need 4 and 6 different major bosses' cores, and the island has 2 (testers use
  `WRHearthLevel`); boss weapons have the class's moveset, not their own (combat lane); each boss's set-decor blueprint
  waits for the biome building sets; the Old Tusker talisman waits for hauler tames that carry loads (its recipe is
  hidden); portals don't exist yet, so nothing calls `PortalDrop`; the other lands' set bonuses and talismans whose
  systems don't exist yet (glide, fall damage, rifts, ward fields...) are hidden with their land and read by nothing yet.

## Unreleased: farming and food (biome crops, water, greenhouses, pens, hives, fishing in every land, the kitchen, spoilage)

Aaron (lane build-farming: FULL-GAME 6.2; research, requirement ledger and plan in showcase-notes cooks/build-farming.md,
following cooks/survival-crafting.md 2.7-2.11 and 2.23). Built to compile (editor); not played here (Aaron and the
testers test, F8).

- **Biome crops** (`Data/farming.json`): 24 crops, a common (20 min) and a rare (2 h) for each of the 12 lands, plus the
  old berry plot. A crop grows in its own land only (the plot says "wrong ground: grows in the frozen tundra"), in the
  plot types it takes, and by its needs: daylight (open sky or a glass roof), dark (a cave, or a roofed room with no
  windows: Yorkshire's forcing sheds), wind (300 m up or the storm peaks), withered ground. On the island today: **Fire
  pepper** and **Ember grape** (volcanic), **Snow turnip** and **Roseroot** (tundra); the rest come alive with their
  land (`tuning world.liveBiomes`). A plot takes **9 seeds** and grows **9 plants**, modelled in code per crop form
  (chilli bush, vine on a chestnut stake, root rosette, grain tufts, flowering herb, cactus pads, young palm, orchard
  sapling, low berry, melon runner, forced stalks, mushrooms, kelp ribbons) through **four stages**: seed leaves, leaf,
  flower, fruit colouring in from green and glossy when ripe. E harvests all nine: yield and seeds back from the data,
  +50 XP the first time you harvest each crop. Seeds come from the land's shrubs (fire pepper 30% / ember grape 6% a
  blow on volcanic shrubs; snow turnip / roseroot on tundra ones).
- **Ash pit** (volcanic slopes only; stone 20): La Geria's vine pit: a round bed of black ash ringed in basalt behind a
  crescent of stacked stone. Ember grapes grow only here. Paddies, kelp lines and log beds are built for the swamp, the
  reefs and the fungal caverns and stay hidden until those lands exist.
- **Water** (no watering can): a plot within 8 m of the sea, a **well** (Hearth 2: a round stone curb, windlass, rope and
  bucket, water in the shaft) or a running **irrigation channel** (Hearth 3: 4 m stone troughs laid end to end from the
  sea's edge or a well; water shows in them when fed and never runs uphill) grows in x0.67 the time; rain on an open plot
  counts while it falls (`weather.json watersCrops`). Wet soil darkens. Ashfall still stops outdoor plots (moved here).
- **Greenhouses** (Hearth 4: glass walls on a stone knee wall with timber mullions, glass roofs on rafters): a plot under
  a glass roof walled in on 80% of its sides grows any land's crop at full speed. Shown with "Needs a Hearth of level 4"
  until steel lands.
- **Fertiliser**: E on a growing plot with compost uses one (+25% at harvest, +3 XP); a mulch ring shows.
- **Farming perks all live**: Green fingers +25%, Seed keeper +50%, Grafting (other lands' crops at half yield), Rare
  seeds 3%, Water wise x0.5 time, Companion planting x0.9 near a different crop, Golden crop 2% (a dish with it fills
  half as much again), Endless field.
- **Animal pens** (`farming.json animals`, 17 farm animals in data, Pumicehog and Snowpuff on the island): up to 4
  animals (Large pens +2), a **feed trough** (a hollowed log on trestles) filled with E from what they eat (pigs: any crop,
  berries, spoiled food; Snowpuffs: snow turnips, lichen, seeds, berries), each product on its own clock eating one feed
  (Pumicehog: animal fat 10 min; Snowpuff: eggs 10 min, feathers 20 min), nothing when the trough is empty ("hungry").
  Products wait on the **shelf by the gate**: eggs in a straw nest box, bundles of feathers, a crock of fat. Two of a kind
  with feed to spare raise a **young** every hour (half size, grown in 30 min, +10 XP). Husbandry perks live: Gentle hand,
  Large pens, Shearer, Selective breeding, Content herd, Prize stock (an ember truffle from a pig, eiderdown from a
  Snowpuff). Animal fat makes tallow.
- **Beehive** (straw skep on a stand under a shingled hood; honey 15 min, wax 30 min, crops within 20 m x0.85 time, no
  feed while crops are near): built and waiting for the Hiveback (Ancient forest); hidden until then, with Bee whisperer
  and Royal jelly.
- **Fishing in every land** (`fish.json`): 30 more fish, a common, a rare and a **legend** for each of the 10 new lands,
  with their bait (a lure per land, Bait maker). Fishing XP by rarity: common 20, rare 60, a legend 500 the first time and
  100 after. A legend bites only with Legend catch **and** its land's lure on, **once a real day** each; the first catch
  gives its **trophy**, mounted at home on an oak board (12 trophy pieces, comfort trophy group). **Fish trap** (Hearth 2:
  a willow funnel basket on a stake, set in 50 cm of water): a common fish of those waters every 20 min, up to 6, +2 XP.
- **The kitchen** (Farm & kitchen tab): **campfire** (roasts meat, fish and vegetables; 10 m of light; meals moved off the
  workbench), **cooking pot** (an iron tripod over its own fire, a copper pot), **oven** (Hearth 2: stone dome, arched
  mouth, chimney, wood store), **smokehouse** (Hearth 2: plank shed, fire pit; 10 at a time, 48 h fresh), **salting
  barrel** (Hearth 2: salt + meat or fish, 24 h), **drying rack** (open air only: jerky, dried fish, dried fruit, raisins,
  72 h), **compost bin** (spoiled food or fish scraps into compost; worms for bait), **ice box** (Hearth 3; food keeps 5x as
  long while an ice block burns, one every 4 h; free in the tundra), **salt pan** (on the upper beach: 1 salt every 10 min
  of daylight). Ice blocks come from ice rocks.
- **Discovery cooking**: E at the pot or oven opens the **Cook screen** (mouse and pad): four places in the pot, the food
  in your bag by group (Meat, Fish, Grain, Veg, Fruit, Herb, Dairy, Sweet, Mushroom, Water, Fat), Cook. A pattern you've
  never made is a **discovery**: a parchment card slides in with its chime, +100 Cooking XP, and it joins your list for
  one-click cooking. No match: Grey mush. 22 dishes (`Data/cooking.json`: porridge, stew, fish soup, broth, omelette,
  skewer, hunter's feast, bread, pie, honey cake, and one per land: fire pepper stew, snow turnip chowder...).
  **Recipe scraps** (from the Treasure hook) hint at one you haven't found. **Sated**: a good meal holds hunger still
  for its minutes (HUD line under Rested; +1% a Cooking level); never a stat buff (Raven's rule). Cooking perks live:
  Recipe sense, Preserver, Camp chef, Banquet (a feast of 12 portions, 45 min sated).
- **Spoilage**: food has freshness on its stack (a thin bar on the icon: green, amber, red; the item card says how
  fresh and when it spoils). Timestamps, no ticking: a 10 s sweep re-stamps food whose container changed pace (bag,
  chest, tundra chest x0.5, ice box x0.2) and turns spoiled food into Spoiled food. Stale (25% and under) gives half.
  Stacks join with averaged freshness; the oldest is eaten first. Raw meat 40 min, fish 30 min, crops 4 h, dishes
  1.5-8 h; grain, honey, dates and raisins never spoil. Saves keep freshness (a logged-out bag doesn't spoil).
- New: `WRFarm.h/.cpp` (`FWRFarmDefs`, `FWRFarm`, `UWRFoodComponent` on the PlayerState, `UWRFarmSubsystem`),
  `WRFarmLook.cpp` (plants, products, water, glass, trophies), `WRUICook.cpp`, `FWRSpoil` in `WRInventory`, kitchen
  and farm pieces in `WRBuildLook` (FarmBlocks), `Tools/art/make_farm_icons.py` (148 icons) and the Sated glyph.
- Not finished: a hungry animal shows it only on its prompt (no thin body or head-down clip); smoke from the smokehouse
  and oven chimneys (their fires burn inside); bone meal waits for the grinder (machinery); young farm animals are full
  grown after a server restart; the 10 new lands' crops, fish, animals, paddies, kelp lines, log beds and the hive wait
  for their land (in data and code, hidden); POI dressing (old kitchens with recipe scraps, ash-pit vineyards) waits for
  the map cooks.

## Unreleased: survival (sleep and Rested, comfort, weather, darkness and light, corruption)

Aaron (lane build-survival: FULL-GAME 6.1; research, requirement ledger and plan in showcase-notes
cooks/build-survival.md, following cooks/survival-crafting.md 2.2-2.6 and world-map 2.6 for the lesser weathers). Built
to compile (editor); not played here (Aaron and the testers test, F8).

- **Sleep and Rest**: E on your bed or bedroll still sets where you wake, and now **lies you down** for a 10 s sleep
  (`rest.sleepSeconds`): you're placed on the mattress along the bed, the view dims, a bar fills. Moving, jumping, a
  blow, hunger or thirst running out, or the bed going away gets you up with nothing. Refused (with the reason) in
  combat, with a creature after you within 15 m (Valheim's rule), when starving or parched, swimming or mounted. Time
  never skips (multiplayer). A finished sleep is a **Rest**: Decay's lost max health comes back, corruption halves, and
  `UWRSurvivalComponent::OnRest` fires (Deathless and Void hang off it when those paths land).
- **Rested**: stamina regen +20%, hunger and thirst drain x0.8, life-skill XP +10% (`rested`); 10 min from a bedroll,
  20 from a bed, **+1 min per comfort** point. Death ends it; it saves across log-outs. HUD: a Rested line under the
  Food / Water gauges with the minutes left (it pulses in the last minute); near your bed it shows the room's comfort
  and how long a sleep there would give.
- **Comfort** (`comfort`, `pieces.json comfort`): decor within 12 m of the bed **under a built roof**, best piece per
  group (seat, table, bed, floor, wall, fire, light; up to 3 trophies; a Hearth close by counts as a fireplace), cap 20.
  Build cards say what each piece adds.
- **Darkness and light**: in a cave (the chamber and the deeper tunnel), or a dark biome's night, with no light
  reaching you, you're **Unlit** (HUD line, one-time hint) and **dark-hunters** (`creatures.json darkHunter`: the
  Gloomhowl today) notice you from twice as far. Lights: placed torches (6 m) and braziers (10 m), the Hearth (10 m),
  glowing creatures like the Glowmoth (12 m), and the new **Light slot**: a **Torch** (by hand: 1 wood, 2 fibre) held
  in the off hand, the shield off that arm while it burns, 20 min, faster in rain, out in water; a **Horn lantern**
  (workbench L2: 2 bronze, 1 hide, 1 tallow) hung at the left hip, both hands free, 9 m, burns **Tallow** (workbench: 2
  raw meat) 90 min each and takes the next from the bag. Both are modelled (`FWRToolMesh::Torch / Lantern`: ash haft and
  pitch-rag head bound with cord; bronze cap, posts and plinth, amber horn panes, a tallow candle), carry their own
  warm light at their play radius, and flame and sparks from `fx.json held.torch / held.lantern`. L lights or puts out
  (on a pad: its belt slot); the bag has a Light cell with the burn time left.
- **Weather** (`Data/weather.json`, all 12 biomes): each biome rolls every 30 min; one weather at a time; a toast and
  the HUD line 60 s ahead ("A blizzard is coming"), the haze closing in from the horizon and its wind bed rising; in
  over 20 s, out over 30. **No temperature meter** (Raven's rule): every effect is a gear check or what you can see,
  each with its counter in the data. On the island now: **Ashfall** (volcanic: ~60 m sight, heavy ash, thirst x1.25 in
  the open, outdoor crop plots stop growing; a roof or Forgeplate) and **Blizzard** (tundra: ~40 m whiteout, sideways
  snow, run x0.85 in deep snow, stamina regen x0.8, Gloomhowls from further; a roof within 6 m or frost-infused armour
  / Rimeguard), plus Snowfall, Freezing fog, Rain and Overcast. Rows for the other ten biomes (Thunderstorm with
  lightning strikes and gusts, Heat wave in direct sun only, Gale, Fog, Squall, Corpse fog, Spore bloom, Grey wind)
  run the same code and come alive when their land does. Strikes: a 1 s ground glow under the highest exposed player
  in a 60 m cell above 600 m, then 60 lightning damage and a stagger **through ApplyHit** (it can kill). Gusts: a
  whistle, then a shove along the storm's wind. Look: `snow.blizzard` / `ash.fall` emitters, thicker base fall, fog
  density and colour, a dimmer sun (`look` in each row); sound: `weather.<kind>` beds; music is never touched.
- **Corruption** (Graveyard and Corrupted wasteland rows): a 0-100 meter, built faster at night, underground or in a
  rift, stopped by a brazier / torch / Hearth, halved by a lit lantern or a glowing creature, halved by Rest, draining
  outside. 25: healing -10%; 50: stamina regen -15% and dark-hunters treat you as unlit; 75: max stamina -20; 100:
  **Marked** (the biome's creatures from twice as far, path resources fill 25% slower). The biome's own path (Necromancy
  / Void) takes none of it and gains path damage +3 / 6 / 10 / 15%. HUD: "Corruption NN" with its bar, only above 0.
  **Not on the island yet**: neither biome exists, so it never builds today.
- Saves: `survival` in the player JSON (Rested left, corruption and where it came from, whether the light was lit);
  the inventory saves the Light slot.
- New: `WRSurvival.h/.cpp` (`WRBiome`, `FWRWeatherDefs`, `UWRWeatherSubsystem`, `UWRLightSubsystem`, `FWRComfort`,
  `UWRSurvivalComponent` on the PlayerState), `WRHeldLight.cpp`; glyphs (rested, dark, corruption, 11 weathers) and the
  torch / lantern / tallow icons drawn by `Tools/ui_glyphs.mjs` (which now takes ids to write only those).
- Not finished: the hero has **no sleep clip** yet: the end of the death clip is held on the mattress (`rest.pose*`
  fit it; a `player_art anims.sleep` key replaces it with no code). Rain's Wet status in ApplyHit (world-map W1), the
  Cleansing draught, grounding charm, sand veil, anchor boots and filter mask items, the Rodcrest's grounding, gale
  stamina for flyers and spore-cloud spread wait for the systems and biomes they belong to.

## Unreleased: the full combat (every weapon class, every status, Wildbrands and spells, creature rows, bosses, camera, flasks, the saddle, PvP)

Aaron (lane build-combat-full: COMBAT-AND-LESSONS C6-C12 and the full-game combat rows; research, requirement ledger and
plan in showcase-notes cooks/build-combat-full.md, following cooks/combat.md). Built to compile (editor and a full unity
build); not played here (Aaron and the testers test, F8).

- **Nine weapon classes** (`Data/weapons.json`): fists, dagger, sword, axe (the hatchets), pick-hammer (the picks),
  spear, greatsword, great hammer, scythe, each with its whole row set from combat.md 2.4: the light chain (dagger 5,
  fists 4, great hammer 2), heavy, **chain heavy** (Heavy from a light's combo window), **charged heavy** (hold Heavy:
  from 0.35 s, full at 0.8 s, lets go at 1.0 s; hyperarmored; a white glint on the weapon at full charge), **running**
  light / heavy, **rolling** and **backstep** lights (0.3 s after a roll / backstep), **jump** light / heavy (the blade is
  live until you land, the recovery on the ground), the guard counter, and the spear's **guard poke** (Light behind a
  raised shield keeps the guard up). Per class: base damage, reach, sweep, stamina, stance, poise, guard cost, slash /
  strike / pierce, crit multipliers, its own guard when there's no shield, a later cancel window for great weapons
  (75%), hyperarmor x2.5 and -17.5% damage taken on great weapons, innate bleed on daggers and scythes, a PvP multiplier.
  Empty hands fight with fists. 15 new weapons (copper / bronze / iron daggers, spears, greatswords, great hammers,
  scythes) at the forge; they're modelled in code like the tools (`FWRToolMesh::Weapon`: a seax, a boar spear with lugs,
  a longsword, a war maul, a war scythe), drawn in the hand's metal.
- **Two-handing** (hold Y, or hold D-pad down; a tap of D-pad down is still the belt): +15% damage, +30% / +10% stance on
  lights / heavies, the weapon's own guard, the shield off the arm. Great weapons are always two-handed.
- **Counter-hits** (+15% damage, +30% stance; thrusts +30%) on a body winding up, **physical types** against
  `creatures.json physResist` (shells take blunt blows, fur turns them), a blunt blow to the **head lock point** x1.5
  stance, **stealth** (+20% stance and buildup on a target that hasn't noticed you).
- **Every status** (`UWRStatusComponent`, `status.defs` in tuning): burn, frost, **bleed** (12% + 15 burst, bosses
  x0.7), **poison**, **rot** (heals halved), **shock** (a short stagger, a chain to two others; a boss mid-swing takes
  stance instead), **soak** (slow; fire builds half, frost and shock half again; a fire dries it), **blind** (creatures
  lose you; players' view closes in and the lock-on drops), **decay** (max health -10% until the fight's over / you
  rest, heals halved), **mark** (+15% taken), **curse** (path resource -25%, damage dealt -15%). Thresholds by tier
  (creatures 100-150, minis 250, majors 400; players 100 + worn resist), x1.3 after each proc until the fight's been over
  30 s, bosses x1.25 per extra fighter. **Frostbite** to the new numbers (a burst of 8% + 20, +20% taken, slowed, players'
  stamina regen x0.6, 12 s; the next fill freezes); **frozen players break free sooner** by pressing buttons; minis and
  majors are **Rimed** (stance x1.5) instead of frozen; **fire ends frostbite**; a proc stops a creature's attack. The
  meters show on every machine: under the bars over creatures and above your stamina bar, in each status's colour.
- **Element reactions**: the last element on a body clings 4 s; **fire then frost = a steam burst** (a scalding cloud:
  damage, soak and blind to everyone close; the burn goes out); Thaw also works for a second player's fire on someone
  else's freeze (at half); lightning on the soaked does more and may shock. Two players' elements meeting react in full.
- **Cures and the drink**: drinking is committed (0.9 s, the heal lands at 0.5 s, a slow walk; a stagger loses it), one
  shared 1.5 s cooldown; creatures and bosses that read actions punish it. New: **Ember salve** (ends a burn),
  **Warming draught** (ends frostbite), **Greater healing potion**; the antidote also ends poison and rot. The belt now
  drinks the exact item you pick (a cure was being swapped for the first potion in the bag).
- **Wildbrands** (`wildbrands` in tuning): each Fire and Frost weapon art has its own frames, hyperarmor, buildup,
  stamina and **fits** (a slot that doesn't fit the weapon in hand greys out with "needs a ..."), **Molten cleave charges**
  (hold the slot: x1.3, D-pad up held on a pad), **Molten edge and Flurry have follow-ups** (press again from the combo
  window: no cooldown). **Spells by shape** (`spells.shapes`): bolts, lances and pools cast walking at half speed; walls,
  novas, Meteor and Blizzard root you; **Ice lance, Frost nova and Meteor charge** to x1.5. Casting and charging are read
  by the creatures that answer them. **Enemy projectiles never home** (a code rule now).
- **Every creature fights by its attack rows** (new `Data/attacks.json`, `AWRCreatureAI::TickRowFight`): for all 17
  fighting creatures, every move of combat.md 2.18.2 with its wind-up, tracking that commits, a **held delay with a final
  glint and sting 0.2 s before contact**, the active window (damage only while the jaw / claw / horn / body travels), a
  **punish window**, **combos** by chance (at most 4), cooldowns and range-band weights. Tells: a small **glint on the
  attacking part** (white blockable, crimson unblockable) and its voice; the ground heaves before a **burst** from below;
  stomps send a **ground wave** at 12 m/s that you can jump. **Off-screen rule**: a wind-up starts only where its target
  can see it (or hear it within 8 m). **readsAction** (Magmaw's lunge, Rimeclaw's saber bite): a drink, cast or charge in
  reach is answered. **Packs** flank and surround with no attack token (the biggest presses, the others take 90 / 150
  degree angles, one keeps the far side), **breathe** (back off and circle 1-2.5 s after a string), retreat when hurt
  (kept). **Pulling**: a hit from range brings the one hit and the packmates close to it that saw it. Basaltback and
  Rimeshell **shell up** after a beating. Spit flies at 10 m/s at where you stood.
- **Bosses** (`WRBossCombat.cpp`): every boss reads its moves from `attacks.json` (Vulkarn and Skaldrith to combat.md
  2.17.2-3, the four minis with their flags and punish). No more full-area floor paint: a **glint on the attacking part**
  and a voice; damage only while the part travels (**arcs sweep** side to side, the **breath sweeps** its cone, **charges
  run** their line on their legs, **slams roll out as waves**, thrown slag, boulders, snow and fire bolts **fly as real
  lobs** with a falling shadow and burst where they land). **Transition attacks** at 50% (Vulkarn's Forge roar floods the
  forge, Skaldrith's Take-off gust): unblockable, half damage taken, then phase 2 with its **delayed variants** (late
  hammer, quench feint, late tail) and **new combos** (bellows into slam, claw-claw-lunge) and shorter gaps. Choice by
  range band, never three in a row, one (phase 2: two) thrown thing at a time; the **tame-punish** moves go for the
  furthest tame; bosses **read actions**. **Breakable parts**: Vulkarn's hammer arm (he fights with the tongs a while),
  Skaldrith's wings (grounded), the Warden's shell; each drops its material once to everyone in the fight.
  **Lock points** on bosses and big creatures (head, chest, wings, horn, tusks).
- **Two new roaming minis** (spawned from data on their loops, no map change): **Cinderhorn**, the Emberhorn herd's old
  bull on the ash plains (25 m charges with a burning trail, horn hook, buck, ash stomp; **eight basalt columns** along
  his loop stop a charge dead and stun him with the weak point open, two charges break one; frost puts the trail out;
  the horn breaks), and **Old Tusker**, the drudge herd's bull on the snow flats (tusk sweep, trample, rear-and-crash,
  snow fling, a herd bellow at 60% that brings two drudges; set burning he **rolls the fire out in the snow**, back open;
  both tusks broken: no sweep). Their trophies: Cinderhorn's horn, Old Tusker's tusk.
- **First kill**: a named boss's first fall for you sends a **gold light** that rises from the body and flies to you
  (6 s, or when you're within 4 m) with its name card: the majors' parts and the minis' trophies (remembered in your
  save). **Shrine-stones** stand 50 m out from each fixed arena toward the way in, facing it: rest at one and you wake
  there (on the death screen's list too).
- **Camera** (CAL 1.15): trees, shrubs and small rocks no longer push the camera in (walls, cliffs and big rocks do); a
  pull-in **holds 0.15 s then eases out over 0.4 s**; the hero hides when the camera is pressed against it; **giant
  framing** (a lock target over 3x your height: the arm pulls back 1.5 m, the view tips up 10 degrees); the lock aims at
  the **lock point** (flick to the next point of the same body first); **mouse flick** switches targets.
- **The saddle** (combat.md 2.19): Light swings to the side the camera faces, Heavy rears and strikes ahead (running-attack
  rows); a **couched spear** at a gallop (hold Heavy above 9 m/s: x2, impact 5); the mount bites your lock target; a
  blow of impact 4+ (or one that staggers the mount) **throws you off**; mounted you can't guard, drink, or use more
  than the mounted Wildbrands and bolt / lance spells.
- **PvP** (`pvp` in tuning): player against player x0.6, then the class's multiplier, Wildbrands / spells x0.85, a tame's
  bite x0.5, status buildup x0.7; the **combat tag** (15 s after hitting or being hit by a player: no shared chests, no
  setting a bed or shrine-stone); the **defender's lag** is judged too (a guard or roll pressed on its screen counts).

Not finished (in LATER-COOKS): per-class hero clips (every class plays the sword's clips warped to its own frames; two
hands aren't on a two-handed haft), a crouch stance (its crouch-light rows are written), a drink clip (the cast clip's
hand-raise stands in), distinct attack clips per creature row (the AI-pipeline creatures have one attack clip each),
Magmaw's lava-burst ambush, the logout body (a tagged player's body staying 30 s), bleed / shock / blind / curse cures
(their ingredients come with their biomes), the other ten paths' Wildbrands and spells (their trees aren't in the data
yet), Cinderhorn and Old Tusker share their herd's textures (bigger, no new hide), the parts break without a visible
piece coming off, mark's "seen through walls".

## Unreleased: the combat core (feel, stamina and roll, guard / parry / crits with server rewind, stance and poise)

Aaron (lane build-combat-core: COMBAT-AND-LESSONS C0 and C2-C5; research, requirement ledger and plan in showcase-notes
cooks/build-combat-core.md, following cooks/combat.md). Built to compile (editor and a full unity build); not played here
(Aaron and the testers test, F8).

- **Read and check (C0)**: what was built, what differed and what was missing is in the cook doc section 1. The big
  gaps it closes: no swing commitment (a roll cut any swing), no input buffer, the swing clip stretched so its strike
  landed anywhere, a flat block cost with no chip, any blow in the first 0.2 s of a block was a parry, any blow on a
  staggered target was a x3 riposte, one poise for everyone, and no lag compensation at all.
- **Swings are move rows** (new `Data/weapons.json`, `FWRWeapons`): the sword's **light chain 1-2-3** (0.25/0.18/0.30,
  0.22/0.16/0.30, 0.30/0.20/0.42; x1.0, 1.0, 1.15; the three slashes in order on every machine), the **heavy** (hyperarmor
  from the press to the end of its strike) and the **guard counter**. Stance, poise, guard cost, impact and how it lands
  are per move. Items pick a class with `items.json` `class` (swords now; axes, picks and the rest come with C6).
- **Commitment and windows** (`WRCombatRules`): nothing starts mid-swing until its window: the next chain attack from
  the **combo window** (last 40% of the recovery), a roll or the guard from the **cancel window** (50% of a light's
  recovery, 65% of a heavy's). The same numbers time the swing on the server, on the predicting owner and in the clip
  everyone sees; window tags (`State.ComboWindow / CancelWindow / Windup / Hyperarmor`) replicate. The server allows a
  remote player's press its lag early (`UWRLagSubsystem::WindowTolerance`).
- **Input buffer** (0.33 s, the last press wins): a press that can't start yet fires the moment it can; a roll pressed
  mid-swing waits up to 0.6 s for the cancel window; a guard held through a swing goes up when it may; menus, build mode
  and death empty it.
- **Swing weight**: the swing clip is warped to the move (its measured contact frames, `anim_windows.json`, land on the
  active window): the coil plays at most 2.2x (a longer lead-in starts later), the strike fits the active time, the
  follow-through runs at full speed. **Step-in**: a swing steps 45-130 cm toward the lock target (else the nearest enemy
  in a 60 degree cone within 3 m) over its wind-up, predicted. **Waist split**: lights keep the legs walking at 0.35x,
  heavies root you (new CMC flag).
- **The hit stack** on every connected blow, by tier (light / heavy / counter / crit / boss kill): **hit-stop** 0.06 /
  0.10 / 0.14 / 0.20 / 0.30 s (parry 0.09) on both bodies, starting at the blade's deepest point, creeping at 2% (no dead
  freeze) and **catching up over 0.1 s**, the struck body **shaking 3-6 cm at 30 Hz** (not the camera); a **white flash**
  on the struck body 70-260 ms (a parried attacker flashes **gold**); camera trauma 0.04 + 0.22 w², an **FOV punch**
  0.2 + 1.3 w degrees, **rumble** 35 + 85 w ms (lights only the small motors); **four sound layers**: the edge's
  transient and the weight's thud by what was hit (new `hit.transient.*`, `hit.body.*`, `hit.counter`, `crit.stab`,
  `boss.kill`), the material, and the creature's hurt call; heavy blows duck the music 2 dB, counters and breaks 3,
  crits and boss kills 4. Bodies only play their **reaction clip when poise breaks** (flinch / stagger / heavy stagger by
  the blow's impact); otherwise it's a shudder and the action goes on.
- **Stamina** (C3): regen **45/s after 0.5 s**, paused while guarding, sprinting and from a swing's press until its cancel
  window; costs from the move rows, roll 12, backstep 8, **jump 8**; the **last-swing grace** (an action starts with any
  stamina and may take it to -20; nothing starts below zero); a refused press **flashes the stamina bar white and shakes
  it**.
- **Equip load and the roll**: worn + held kg (armour, weapon, ranged, the carried shield; not the bag) over 60 kg + the
  `equipLoad` stat: **light** < 30% (500 cm, i-frames 0.03-0.46, recovery 0.27), **medium** < 70% (420 cm), **heavy**
  < 100% (330 cm, 0.03-0.43, recovery 0.53), **overloaded** (a slow step, no i-frames). Shown in the inventory (Equip load
  bar and the roll it gives). **Backstep** with the stick at rest (250 cm, 0.10 s i-frames). **Jump**: blows flagged
  `low` (stomps, tail sweeps, ground pounds, anvil slams) pass under for 0.8 s. **Perfect dodge** (a blow taken by the
  i-frames in the roll's first 0.2 s) fires `Event.PerfectDodge`; Frozen moment now needs one. A roll **stops at a drop
  over 2 m** unless the stick still holds toward it.
- **Guard** (C4): up from the press; a blocked blow costs its stamina damage x (1 - Guard Boost / 100), only when it lands
  (the hero's carried shield: boost 45, 85% physical / 40% element stopped; a weapon alone 25 / 60% / 30%); **chip
  damage** gets through; **block stun** 0.20 / 0.35 s, then a 0.6 s **guard counter** window (Heavy: x1.3, stance 30,
  hit-stop 0.14); out of stamina the **guard breaks** (1.2 s stagger; a guard-broken player is open to a riposte for 1 s).
  The guarding shield wears a **faint gold edge** on every machine.
- **Parry**: a guard tap; the shield's window opens after 0.13 s for 0.17 s (a sword alone 0.10 s); let go within 0.3 s
  and the parry's recovery holds you (spam punished), a press within 0.5 s of the last shrinks the window 30%. Only blows
  flagged **parryable** can be parried (bites and lunges; boss melee as marked, the Vulkarn tongs included; never
  projectiles). Parries to break: creature 1, mini-boss 2, major boss 3 (a parry that doesn't break interrupts and takes
  25% stance); a parried creature takes **x1.5 from everyone for 2 s**.
- **Criticals** (new `UWRGA_Critical`, from Light): a **riposte** on a parried, guard-broken or stance-broken target from
  its front (x3.45), a **backstab** from behind on a body no bigger than a person that isn't winding up (x2.3; PvP arc
  75 degrees, x0.7). The attacker warps to the sync point over 0.12 s, is untouchable until the damage frame (0.9 s /
  0.75 s), the target is held through it; the stab is the heavy thrust warped to the damage frame. The crit prompt shows
  **once per species**, then never (saved).
- **Server rewind** (new `UWRLagSubsystem`, server only): every body sampled at 30 Hz for 1 s. A player's swing and crit
  are judged against bodies **where that player saw them** (ping / 2 + interpolation, at most 250 ms). A creature's or
  boss's blow on a remote player that isn't defended right now **waits for the defence that player pressed on its
  screen** (its guard, parry, i-frames and jump history, back-dated by its ping), then resolves at the moment it saw the
  blow; boss tongs, Skaldrith's freezing breath and knockbacks act on the real result.
- **Stance and poise** (C5): creatures and bosses have a hidden **stance** by tier (lowland 20, midland 35, highland 55,
  mini 90, major 140; Vulkarn 180, Skaldrith 160), refilling after stance / 13 s at 13/s (half as fast below half health);
  bosses take less with more fighters (x0.6 / 0.45 / 0.35, tames count). **A break** stuns 2.5-5 s: the creature's recoil
  is held, it slumps, a **gold weak-point mark** burns on its head and **STANCE BROKEN** shows for who broke it; the crit
  opens. **Phases never turn during a break or a crit**. Players have **poise** from armour (hide 8, copper 18, bronze 24,
  iron 32) + Tempered + Reinforced + gems; it resets 30 s after the last poise hit and at once after a reaction.
  **Hyperarmor**: the heavy's window x1.5 poise and 7.5% less damage; Basaltback, the Woolly drudge and bosses swing
  through blows on their own attacks (and a flinch now cuts a creature's special short). Nothing holds health at 1.
- **Prompt diet**: no hit words (Parried / Blocked / Dodged / riposte / critical / weak are gone); state words only
  (GUARD BROKEN, STANCE BROKEN); **damage numbers** are off by default (Settings > Controls), and sit beside the body,
  never over its head.
- Data: `Data/weapons.json` (new), `tuning.json` (`combat`, `crit`, `stance`, `poise`, `net` blocks; `player`,
  `stamina`, `defense`, `fx` reworked; creature and boss move flags), `creatures.json` (stance, hyperarmor),
  `items.json` (armour poise), `fx.json` (`crit.mark`, `stance.break`, `guard.break`), `sounds.json` (+10 events);
  21 new sounds (172 KB, `Tools/audio/make_combat_layers.py` from the CC0 Kenney impacts; CREDITS row updated). Tests
  updated to the new block cost and parry rules.

Not finished (hidden or noted): **knockdown / launch** reactions (impact 4-5) play as a heavy stagger until there are
get-up clips; the **backstep** plays the roll clip; creatures' **stance break** holds their hit clip with a slump (no
dedicated kneel clips yet) and the crit victims play their hit clip (no paired victim clips); **shields as items** and the
other off-hands are data only (the hero's carried shield is the guard) until the crafting cook; weapon classes beyond the
sword, charged / running / rolling / jump / crouch attacks and counter-hits are C6; breakable parts and head daze are C10;
PvP blows are judged at the attacker's view only (the defender's lag isn't waited for).

## Unreleased: the hero's look (12 faces, 3 builds, armour tiers and the 12 biome sets, path marks on skin, eyes and hair)

Aaron (lane build-hero-look-2; research and plan in showcase-notes cooks/hero-look-2.md, following cooks/hero.md 2.2-2.6).
Built to compile; not played here (Aaron and the testers test, F8).

- **Twelve faces** (hero.md 2.2): M1 Ash, M2 Kade, M3 Ren, M4 Kofi, M5 Varro, M6 Obi; F1 Wren, F2 Nima, F3 Sora, F4 Amara,
  F5 Mila, F6 Kesi, all on their sex's athletic body, 2K. `player_art.json` `hero.<sex>.faces` (face 1 stays the
  default). **Lean and strong builds** go in as body meshes only (`hero.<sex>.builds`); every build wears the picked
  face's skin. The face and build come from `?Look=<face>.<build>[.h]` (`[Wildreach.Menu] Look`, for the creator
  screen); the server clamps them and every machine draws the same.
- **Hair is the engine's**: grooms point at the MetaHumanCharacter plugin's own groom assets with a small binding per
  face (`Hero/Grooms/<face>/`), and every hair, brow, beard and lash option of that sex is bound to every face
  (`faces[].options`, for the creator). Each face's hair colour is data (`hairColor`). The two big groom copies
  (F1's 63 MB hair, M1's 19 MB) are gone from the tree.
- **Armour shows**: the worn armour picks its look (`armorLooks`): hide → **Hidewear** (fire **Cinderhide**, frost
  **Snowhide** by the item's element), copper → **Copperscale**, bronze → **Bronze Lamellar**, iron → **Ironmail**, and
  the steel biome sets are mapped by id for when the crafting cook adds them: **Forgeplate, Stormweave, Dunewrap,
  Heartguard, Skysilk, Wardcloth, Rimeguard, Mirehide, Reefmail, Gravecloth, Sporeweave, Hollowplate**, plus their
  **Ascendant** (starsteel) versions with the halo-crest. Four slots (Head, Chest, Hands, Legs) on all six bodies.
  Plates are solid with rolled rims and move rigid on their bone; pauldrons are articulated lames; helms fit every face
  of the sex. The element sets the trim colour; the seams carry the main path's colour; helms and raised hoods hide
  the hair. Replicated (`AWRCharacter::HeroLook`).
- **Skin never shows through cloth**: each look has body-hide masks (512 px, skin under a piece clipped 1.2 cm inside its
  edge), set on the body skin when it's worn (`looks.<id>.hide`; `M_WR_Skin` is masked).
- **Path marks on every machine** (hero.md 2.6, `WRHeroLook.cpp`, `player_art.json paths`): the server sends each
  player's main/sub path and strength; skin, eyes and hair follow. Fire's veins are darker ember and **exposure-
  relative** (they read by day and don't blow out at night) with **soot-dark nails**; eyes per path (ember pupil ring,
  violet spark ring, gold iris, black Void eyes, Blood's red sclera...); hair per path (sun-bleached, rime-white or
  mossy tips, Necromancy greying, Tide wet, Wind stirring, Lightning lifting). Using a path ability flares the marks
  (Lightning: scar flicker, hair lift and a static crackle, `path.lightning.crackle`). **Blood** grows sharper canines
  from strength 0.6.

Not finished (hidden or noted): no creator screen picks a face, build, hair or the hide-helmet option yet (everyone is
face 1, athletic); capes, veils and strips are skinned, not cloth-simulated; Void follows path strength until
corruption is a stat; the steel set items come with the crafting cook; path Niagara extras (heat haze, frost breath,
spore motes) aren't built.

## Unreleased: finishing what was half-built (skill nodes, perks, fishing, swimming, engineering, the whistle, tools, build pieces)

Aaron (lane build-finish; research and plan in showcase-notes cooks/build-finish.md): finish every built-but-unfinished
feature. Built to compile; not played here (Aaron and the testers test, F8).

- **Fire and Frost nodes**: every ability and passive node already worked; three stat nodes did nothing and now do
  (tuning `nodeEffects`): **Shockwave** (fire area spells, Explosive bolt and Overheat reach 10% further a rank),
  **Biting cold** (your frost slows go 8% deeper a rank; replicated so a slowed player's movement agrees), **Wide
  storm** (Frost nova, Ice wall and Blizzard 8% larger a rank).
- **Life-skill perks**: 41 more perks work (66 of 120); the 54 whose system isn't in the game (diving, breeding, bees,
  machines' power, turrets, explosives, timed brews, spoilage...) are marked `waitsFor` in `Data/lifeskills.json` and
  are **not shown**: a tier with none in the game is skipped, a tier with one offers just it ("Take"), the server
  refuses them. The "takes effect once its system is in" line is gone. New working perks: Prospector (ore marks
  through rock, 35 m), Strong back / Light load / Light weave (lighter ore, wood, armor), Deep delver (half hazard
  damage), Vein breaker / Clean cut (the last third of a vein or tree at once), Rich veins, Unbreaking / Endless edge,
  Predator sense ("Hunted" + a pip toward each predator after you), Beast lore (your lock-on target's weakness,
  what it shrugs off, how it's tamed), Efficient smith / armorer, Quick hands / fit, Reinforced, Bastion, Bulk cook,
  Shared meal (clan mates within 10 m), Hearty, Quick repair (gear and the Hearth), Fortifier, Tinkerer, Overclock,
  Self repair, Chief engineer, Pack leader, Strong stroke, Water fighter, Seaborn, Shadow, Backstab, Vanish, Ghost,
  Assassin, and the fishing perks. Every-level bonuses that did nothing now do: fishing (bites sooner), tracking
  (more meat and hide), building (sturdier pieces, saved), engineering (stations run your orders faster), swimming
  (faster). Beast lord's text no longer promises mini-boss taming.
- **Fishing** (new `WRFishing.cpp`, `Data/fish.json`): craft a **fishing rod** by hand (3 wood, 6 fibre, 1 stone),
  hold it, **hold the attack to wind up a cast and let go**; the float lands on water (sea, river, lake), nibbles one
  to three times, then plunges: **strike** within 0.9 s; then **hold to reel** (stamina) and **ease off when it
  surges** or the line snaps; land it close in. 16 fish by biome, water depth and time, two legendaries (Legend
  catch) and two deep-sea fish (Deep line). Cut bait (meat) brings bites sooner; Ember / Rime lures (Bait maker)
  draw rare fish. Fish are food (raw or grilled at the workbench). Everyone sees the float, its line, dips, plunges
  and splashes; the angler gets a charge ring, a tension arc and prompts (mouse and pad). Fishing XP per catch.
- **Swimming**: XP per metre swum; **Sprint in the water is a fast stroke** (1.6x, 8 stamina a second).
- **Engineering**: XP for every item a station finishes for you, placing a station or code-lock door, and fitting a
  lock; stations are its machines.
- **The tame whistle**: **H** (rebindable) taps to call every tame of yours within 30 m, held opens the order wheel
  in whistle mode (Come / Stay / Passive / Aggressive, the names of who hears it); on a pad, hold X with nothing in
  front. You whistle for everyone near to hear; each tame answers with its call.
- **Held tools are modelled** (new `WRToolMesh.cpp`, material `M_HeldTool` from `Tools/art/make_held_tool_material.py`):
  a turned, oval haft with a knob, a knapped flint (crude) or ground stone head, criss-cross lashing; the pick's
  curved double point; the rod's wand, grip, spool and rings; the float. No engine shapes.
- **Build pieces collide as they look**: the bed (frame + headboard), table (top + legs), chair (seat, legs, back),
  brazier (bowl on legs), torch, pen (along its rails), trophy mount and banner; the **banner and rug get detail**
  (pole, crossbar, panels, emblem, tassels; border, bands, fringe); the **Hearth, stations and claim totem collide
  as their models** (a box fitted to the prop, on every machine).
- **Bounties** stay hidden (nothing reads the setting; the settings screen leaves it out) until the PvP pass.
- New sounds (synthesized, `Data/Audio/item`): whistle, reel ratchet, line snap, nibble (12 files, 112 KB); new icons
  for the 21 new items (`Tools/art/make_fish_icons.py`).

## Unreleased: the MetaHuman hero moves (motion matching, retargeted actions, first person on the head)

Aaron (cooks/hero.md, build lane build-hero-anim; research and plan in showcase-notes cooks/build-hero-anim.md): the
Mixamo survivor is replaced by the MetaHuman hero from the hero lab, and the hero gets GASP motion-matched locomotion,
its action clips, sockets and full-body first person. Built to compile and the assets built headless; not played here
(Aaron and the testers test, F8).

- **The hero** (`Data/player_art.json` schema 2, `AWRCharacter::LoadBodyArt`): the main menu's Male / Female pick is
  now **M1 Ash** or **F1 Wren** (athletic build, Survivor Wraps), each the MetaHuman body, its face with the MetaHuman
  face rig (`ABP_Face`, follows the body), hair / brows / lashes (and stubble on M1) as cards, the wraps' Chest, Hands
  and Legs pieces following the body, and one LOD for all of it (LODSync led by the face). In first person the head,
  hair and head-slot outfit are hidden from you (their shadow stays) and the rest of the body is yours to see.
- **Locomotion is motion matching** (new `UWRHeroAnimInstance`, built on `UWRAnimInstance` so the block / aim loops,
  swings, casts, rolls, hits, deaths, gathering, turn in place and slope feet all work as before). It searches 12
  databases rebuilt from the Game Animation Sample's own (idle, walk / run / sprint starts, loops, stops, turns,
  jumps, lands) on 58 GASP clips retargeted onto the hero, picked by a chooser in `player_art.json` (state x gait x
  stance). The trajectory is the body's last 0.4 s plus the next 1.0 s simulated with the movement's own acceleration,
  braking and turn rate; new matches blend over 0.2 s, clips play at the real ground speed within 0.8-1.25x (stride
  matching), the pose turns up to 45 degrees onto the real travel with the spine turning back (orientation warping),
  and footsteps come from the feet touching down. Your hero searches every frame, other players every 0.1 s, and at
  LOD 3+ they drop to cheap loops (tuning `anim.mm`). No animation blueprint (Raven's rule; anim graphs can't be
  authored headless either), so it runs the same in packaged builds.
- **One clip set for both bodies**: the clips are retargeted once onto the male body; the female plays them with her
  own bone lengths and her hips at her height (what the engine's Skeleton / AnimationScaled translation modes do), so
  no second copy is in the repo. The female idle stance (hero.md 2.3: hips tipped 3 degrees and dropped 2 toward the
  standing leg, shoulders back 2) is procedural.
- **Movement** (FULL-GAME 9): 5000 cm/s/s acceleration (0.15 s to 750), 4000 braking, separate braking friction 6,
  ground friction 8 (tuning `player`).
- **Other players' acceleration replicates** (Lyra's pattern, 3 bytes + gait / stance / lock-on bits, simulated
  proxies only), so their starts and stops show the moment the stick moves.
- **Action clips**: 30 Mixamo clips (block, 3 slashes, 3 attacks, 2 casts, 3 hits, 2 deaths, roll, chop, gather, pick
  up, swim, tread, kick, bow idle / equip / aim / recoil / draw, rifle aim / fire / reload, throw) retargeted through
  an IK Retargeter onto the hero. The Mixamo walk / run / strafe / turn clips are gone (motion matching does those).
- **Combat windows on the clips** (new `UWRANS_Window`, tags `State.ComboWindow`, `State.CancelWindow`,
  `State.IFrames`, `State.TraceActive`): placed on every swing, cast, kick, chop and the roll from
  `Data/anim_windows.json`, measured from each clip (hit frames where the striking hand moves at 55%+ of its top speed;
  combo the last 40% of recovery; cancel from 50% of recovery on lights, 65% on heavies, chops and kicks, 60% on
  casts; the roll's i-frames from tuning). One-shots forward them, so they fire on every machine that draws the body;
  a dedicated server reads the same numbers through `FWRAnimWindows::Find` (for the combat lanes' timers).
- **Sockets**: the sword, shield, held tools and ranged gear sit in the hero's sockets (`weapon_r`, `shield_l`,
  `bow_l`: the palm's frame), not on Mixamo bones; the old palm-down maths is gone. `items.json`'s ranged `socket`s
  are `weapon_r` / `bow_l`; along / face gain `grip`, `palm`, `backhand` and `arm`, so the bow and bola keep their
  old hold on the new rest pose.
- **First person on the head** (hero.md 2.7, tuning `fp`): the eye is the head bone after animation plus 9 cm forward
  and 6 up, 1:1 across the ground, a critically damped spring (half-life 0.05 s, within 2.5 cm) up and down, the
  view's turn always the controller's; a roll only drops the view 30 cm and back with a 0.1 s vignette; near clip
  5 cm. The spine takes the aim pitch (first person and aiming).
- **Removed**: `Content/Characters/Survivor` and `SurvivorF` (92 MB). Plugins enabled: PoseSearch, HairStrands,
  RigLogic, MetaHumanCharacter (all ship with UE 5.8).
- Assets: `Content/Wildreach/Characters/Hero` (372 MB in Git LFS: the two heroes 259 MB, clips and databases 90 MB,
  wraps 17 MB, skin materials 7.5 MB), rebuilt with `Tools/anim/build_hero_anim.py`.

## Unreleased: F8 bug reports, the controller, the F1 controls page, look settings

Aaron: build the in-game bug report first so testers can report (COMBAT-AND-LESSONS L10, C1), with the controls page,
full controller support (L4 one verb per button, L8 held-input isolation) and mouse sensitivity / invert (quality
plan row 9). Built to compile; not played here (Aaron and the testers test, and report with F8).

- **Report a problem: F8, or View on a pad** (new `WRReport.cpp`, panel `WRUIReport.cpp`). The frame on screen is
  frozen with the interface (the engine's screenshot with UI, JPEG at most 1280 wide), and the report gathers version
  (and the package's `build.json` if one sits beside the game), map, net mode, world, preset, players, position and
  facing, health, stamina, hunger, thirst, path and level, the open menu or screen, placing, lock-on, device and pad,
  sensitivity, frame rate, viewport, quality, OS, CPU, GPU, RAM, time played and the **last 60 s**: the game's log lines
  (notices, screens, locks, deaths...), any warnings and errors, and events (screen opened / closed, mode changes,
  device changes). The panel: the frozen picture, the **8 kinds** (Bug, Stuck, Controls, Camera, Music-Audio, Visual,
  Balance, Idea, as Waveraider's), a note, Send / Cancel (Ctrl+Enter / Esc; on a pad LB / RB the kind, Y send, B
  cancel). Alone, the world pauses while you write.
  - **Send** posts to `https://freedom35studios.pages.dev/api/feedback` with `game: "wildreach"`, the body, category,
    label and context exactly as Waveraider's `report.ts` (context trimmed to the inbox's 4000 characters for a player
    who isn't signed in, oldest log lines first). Offline or refused, it waits in `Saved/Reports/Outbox` and is sent
    again every 30 s and at the next start; a picture too big goes again without it.
  - It's also saved in full to `Saved/Reports/<time>_<player>_<kind>.json` + `.jpg`.
  - **A client's report goes to the host too** (`ServerReportPart`, 8 KB a frame), which saves it as
    `<time>_<player>_from-client.json` + `.jpg` and tells the host's player. A player can send one every 10 s, each
    under 2 MB.
  - Numbers in tuning `report`.
- **The controller** (new `WRPadInput.cpp`, layout in new `Data/gamepad.json`). In play, one verb per button (L4): RB
  light, RT heavy, LB block, LT sprint, A jump, B roll, X use (hold: tame orders, a door's keypad), Y use the chosen
  belt item, D-pad up the chosen ability, D-pad left / right the previous / next ability (ammo while aiming), D-pad down
  the next belt item, RS lock on (a flick of the right stick moves the lock to the next target that way), LS first /
  third person, Menu the menus, View the report. Placing a piece: RB place, RT turn, B put it away, Y demolish, D-pad
  down the snap. Riding: left stick, right stick, A jump, X get off.
  - **Every screen works on a pad without each knowing it**: one Slate input processor, while the cursor is up, moves
    it with the left stick (slower over anything clickable), jumps it with the D-pad to the nearest button that way on
    the top screen (with none, the list scrolls), clicks with A (hold and move to drag), second-clicks with X (use,
    unequip, split), scrolls with the right stick, goes back with B (whatever Esc does there), Y opens the controls
    page, LB / RB walk Inventory, Crafting, Build, Paths, Skills, Map, Clan, Settings; wheels point with the stick. The
    main menu, the settings window and the death screen work the same way.
  - **Prompts follow the device last used** (`WRInput::ActionKey`): every key cap in the HUD, journal, build bar,
    hints, taming, storage, paths and wheels shows the pad's button on a pad, and nothing where the pad has none (an
    empty key cap now hides itself). The hotbar shows Y on the chosen belt slot.
  - **Held-input isolation** (L8): opening a menu, page or wheel, starting or stopping placing, mounting and dying let
    go of held block, charge, sprint and jump; screens also flush held keys and sticks; leaving any of them, gameplay
    presses wait 0.14 s, so the press that closed a menu never also swings.
  - Vibration on blows landed and taken (tuning `pad.rumble`).
- **The controls page: F1** anywhere (and Y in any menu on a pad; it replaces the old H overlay, and Help now
  defaults to F1): every action with its key or button, opening on the device in use and switching with it;
  Keyboard and mouse / Controller tabs (LB / RB).
- **Settings > Look and controller**: mouse sensitivity and invert, controller look speed and invert, button names
  (Xbox or PlayStation), vibration, and a button for the controls page. Look speeds and curves are tuning `look`;
  neither device is inverted by default.
- Rebindable: Report (F8) joins the keys; the settings window's key listening takes F1 / F8 when you bind them.

## Since 0.7.1: set up for a second developer

Raven: "make a handoff and a readme making sure his Claude knows it needs the art packs and Visual Studio and the other
things" (Raven's brother, on his own PC with his own Claude account, pulling this repo). Docs and a check script only;
the game is unchanged.

- **`Tools/check_setup.ps1`**: PASS / FAIL for what a PC needs, each FAIL saying how to fix it: Unreal 5.8 in the
  launcher's default folder, Visual Studio with "Game development with C++", a Windows SDK, Git LFS with the art
  downloaded, the six Fab packs the game uses (of the 13 pack folders on Raven's PC, only GV_FreeShrubsPack, LavaMat,
  QuadrapedCreatures, SimpleRocks, WaterMaterials and MWLandscapeAutoMaterial are referenced), the four Barghest files
  that crash packaging, and a pack overwriting `Config\`. Passes on Raven's PC; in a fresh clone without art or packs
  it fails the seven things missing.
- **README "Working on the game on a new PC"**: what to install, the packs with their Fab listings and folders, the
  steps from clone to a passing test.
- **`Docs/HANDOFF.md`**: who decides what, where the game stands, two people on one repo, testing, releasing step by
  step, the known traps, art.
- **`Docs/design/`**: the build plan and every design doc tab exported from Raven's Claude docs (2026-11-08), since
  another Claude account can't open them.
- **CLAUDE.md**: check the setup first and stop if a pack is missing, pull first, design docs from `Docs/design/`,
  status brought to 0.7.1, two garbled lines fixed. README: the full suite takes about 90 minutes, not 40.

## 0.7.1: door keypads, the Clan tab

**Released 2026-11-08** as [v0.7.1](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.7.1) (Latest; commit
a1a542a, `C:\Dev\WildreachBuild8`, the Setup's SHA-256 on GitHub the same as the local file's; notes
`Docs/release-notes/v0.7.1.md`). Raven: "finish what you were doing, push and publish". Checked before release: alltests 38 of 39 at once, the 39th (updater's dropped-connection step, timing under load) passing
alone, 44 of 44; packaged into `C:\Dev\WildreachBuild8` (Setup 1,682 MB); from the Setup a clean install matches all 667
files; through the local stand-in for GitHub, 0.7.0 and 0.6.2 each update with their own updater over four connections,
231 MB and 236 MB of 1,682 MB, every file matching and the save kept; in the updated copy, crafttimetest's steps by
clicks (10 of 10) and a copy of Raven's playtest world from the main menu with its Claim tab and the new Clan tab on O
(Raven's own worlds untouched, 0 of 11 files changed).

- **The Clan tab** (journal, **O**; new `WRUIClan.cpp`, `EWRMenu::Clan`). Raven: "how do clans work and are they in yet?",
  then "yes please, is there any other feature that's console only that needs a screen?". Clans worked but only by
  `WRClan` in the console, and an invite even told the other player to type a command.
  - With no clan: **Found a clan** (a name box that takes the keyboard, as the chest's Rename: 1-24 characters, spaces
    allowed) and the invites you've had, each "from <leader>" with **Accept** / **Turn down**.
  - In one: its name, "You lead it" or "Leader: X", "N members" (or "N of L" with the world's size limit), the members
    (on or away, Leader, You) with **Remove** for the leader, the invites it has out with **Take back**, and **Leave
    clan**, which asks first ("Leave X? Y leads it after you." / "It ends with you.").
  - Right side: the players in this game with their clan and, for a leader, **Invite** on each; **Invite by name** for
    someone who isn't on; and what clanmates share (bases, beds, no fighting, raids).
  - Server: new `clan decline <clan>` and `clan uninvite <player>`; clan names may have spaces; the player invited or
    removed, and the clan someone joins or leaves, are told ("Mason invited you to clan Ash: accept it in the Clan tab
    (O)"). Each player's state carries their own clan's leader, members, invites out and invites to them
    (`AWRPlayerState::ClanLeader` etc., owner only), refreshed with every clan change and on joining.
  - New `Tools/clantest.ps1` (alltests `clan`, 14 checks, all pass): a host with no window and a friend at 1440x900
    by real keys and clicks through all of the above, and clans.json saved.
- **What else was console-only** (every player command checked against the screens that send it): only the tame
  "whistle" (`WRTameAll <order>`: every tame of yours within 30 m takes the order); the tame wheel orders one tame.
  The main path is picked on the main menu and the subclass at a boss altar; the rest all have screens or keys.
- Fixed on the way: a new file moved WRRaids.cpp into another unity file, where its `using namespace` put its state
  `G` beside other files' locals named `G` (C4459): it's `RaidState` now.

From checking the build plan's first playable against the game (Raven: "is the first playable done ... with building?"):
everything in its Building row was in but "doors and code locks", whose codes only the console set or entered
(`WRSetCode` / `WRCode`), so a player couldn't lock a door at all. The dedicated server, the other gap, is on hold
(Raven).

- **A keypad on code-lock doors** (the reinforced door; new screen `WRUIDoor.cpp`, `EWRMenu::Door`). E on a locked door
  that isn't yours brings it up: the prompt says "Enter code". Four digits by click or by the number keys (number row or
  pad), Back / Backspace, Clear / Delete, **Unlock** or Enter. A wrong code shakes the digits ("Wrong code"); the right one
  opens the door and closes the keypad, and the door is yours to open (and shut) from then on, until its code changes.
- **Your own door: hold E** (0.35 s, `ui.doorHoldSeconds`) for its keypad; a tap still opens or shuts it. The prompt
  says "Open · Hold: code lock". Type four digits and **Set code and lock**; **Remove lock** takes it off ("anyone can
  open it"). The same for your clan's doors. Owner and clan are known from the replicated player states, so the hold
  and the prompt work on a friend's machine too; the server decides as before.
- Server: `setcode` / `code` / new `clearcode` take the door's spot (`code 4821 X Y Z`: the keypad's own door, not just
  the nearest piece), the right code opens the door, and `AWRBuildPiece::UnlockedFor` (who has entered it) replicates
  so the prompt and E know who may open it. Every attempt is logged (`DOOR <who> <cmd> at <spot>: <result>`).
- Testing: `WRKey <key> hold <seconds>` holds a key down. New `Tools/doortest.ps1` (alltests `door`, 14 checks, all
  pass): a host with no window and a friend at 1440x900 by real keys and clicks: the host's door, a wrong code by keys
  and the right one by clicks; the friend's own door locked by a held E and keys, the host kept out (his E brings up its
  keypad), a tap still opening it for its owner, the lock removed. buildtest's raider now sends `WRCmd interact` for
  the server's door rules (its E brings up the keypad now). unit, build, survival, house, menus, base and flicker pass.

## 0.7.0: creature raids, crafting time, far views

**Released 2026-11-08** as [v0.7.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.7.0) (Latest; commit
6ac25f3, `C:\Dev\WildreachBuild7`, the Setup's SHA-256 on GitHub the same as the local file's; notes
`Docs/release-notes/v0.7.0.md`). It carries 0.6.3's base fixes too, which never went out on their own. The plan Raven approved after 0.6.3: creature raids, crafting time with station queues, far views, ash-grey
volcanic shrubs, ice rebuilt like the lava. All done but the shrubs, which still hang the GPU and stay green (Raven:
"leave the shrubs green if it's a problem"). Then a polish pass, and boss fights from Raven's play.

- **Bosses chase you, and walk home instead of jumping there** (`AWRBoss::ChaseRadius`, tuning.json `bosses`). Raven:
  "the radius is too small and it's very hard to fight a boss because it can't walk far and it glitches back or doesn't
  pursue". A boss only fought within its arena's floor (`arenaRadius`, 30 m for a major boss, 22 m for a mini one):
  past it, it stopped coming after you, and 8 s with nobody inside it teleported home at full health. Now it chases and
  counts fighters within `chaseRadiusMajor` 90 m / `chaseRadiusMini` 65 m of home (flat distance; the floor stays for
  floods and props). With nobody there for `resetSeconds` it still resets, but walks (or flies) home; only one that
  isn't home after `returnStuckSeconds` (25) is put back. On its way home it only wakes for someone it may fight.
  New `Tools/bosschasetest.ps1` (alltests `bosschase`, 5 checks): Grimjaw follows the player 35 m from home and keeps
  fighting, then resets with them 200 m off and walks home, full health, no teleport. Dev command `bossinfo`.
- **The raid tracker** (`AWRPlayerController::ClientRaidTrack`, `WRUIHud` under the level plate): while a raid is on
  your claim, "Raid on your base" with a clock: "5 coming from the east" until they come, then "4 of 5 raiders left"
  with a bar, until it's over. Sent each raid step to everyone of the claim; it goes when the updates stop.
- **The raid card no longer has a notice showing through it.** The warning came as a notice and a card in the same
  place; the card carries it alone now. And any notice that comes while a card or banner is up (a boss's, a level-up)
  steps down under it instead of sitting behind it ("Your 8 Copper ingot are done" did, in the friend's test).
  The flow screen's toasts move down under the raid tracker while it's up.
- **Shared stations: whose order is whose.** In the forge's strip someone else's order says "Mason's order", and its
  **Cancel** shows only when you may cancel it (yours, or any at your own or your clan's station: the server's rule).
- **The Crafting tab at 4:3 and 5:4:** cards go two a row when three would be too narrow for their names ("Bronze ingot"
  ran under its lock).
- **Two-player testing**: bots keep in step by chat (`waitchat <text>` waits for a chat line). New
  `Tools/cooptest.ps1` (alltests `coop`, 12 checks, all pass): Mason hosts (no window) and builds a base, Bravo joins in
  a 1024x768 window and plays by clicks: Bravo's 8 ingots and Mason's 2 swords on one forge (Bravo sees Mason's order
  without Cancel, then with it once in Mason's clan; each one's things to their own bag or the tray), then a raid with
  Bravo getting the card, the tracker coming and counting, and "Raid over" when Mason kills them.

Checked before release, on commit e6c4fe2:
- the full suite: 36 of 37 passed; `menus`' host hit the known GPU hang (`DXGI_ERROR_DEVICE_HUNG`, VSM Nanite
  `NodeAndClusterCull`, two game windows on the GPU) and passed run alone, so all 37 pass;
- the packaged build (`C:\Dev\WildreachBuild7`, Setup 1,682 MB, 667 files, 16 changed from 0.6.2): a clean install
  from it matches the build in every file; **0.6.2 with its updater** (four connections) and **0.6.1 with its updater**
  each fetched only what changed, 236 MB, all 667 files right, the save kept, nothing left over, the new updater in
  place (a local stand-in for GitHub serving the Setup);
- the packaged game, updated from 0.6.2: crafttimetest's steps by clicks (10 of 10 on its own log) and raidtest's (14
  of 14); a copy of Raven's "playtest 0.6.0" world loads from the main menu (14 pieces, 368 animals, raids "rare", all
  257 landscape pieces, the Claim tab showing the totem and the Hearth), no errors. Raven's real worlds untouched.

- **Ash-grey volcanic shrubs: retried, still off.** The plan was to try `world.ashCoatMin` 0.85 again, since the hang
  it gave in 0.4 (4 of 4 benchmark runs) looked like the Nanite tessellation one fixed in 0.5.0. It isn't: with it, 2 of
  3 Epic runs hung (`DXGI_ERROR_DEVICE_HUNG` in the virtual shadow maps' Nanite `NodeAndClusterCull`), and the 5 runs
  without it the same day were clean (Docs/PERFORMANCE.md). It stays at 0; greying them needs another way than the
  pack's snow coat (the note in `WRWorldDressing.cpp`).

- **The ice, rebuilt like the lava.** It was flat 6 m squares (the hazard cells, 13,576 of them) floating 20 cm over the
  ground. Now it's a sheet on its own 2 m grid (`hazards.iceGrid` in `Data/island_dressing.json`, made by
  `Tools/terrain/dressing.mjs` from the same cells: a distance pass gives how far inside the edge each point is, with a
  ragged, noisy rim; 134,424 points), a thin skin over the ground that sinks under it at the rim, drawn in 454 chunks
  that each cull alone (`AWRDressing::BuildIceSheet`, sharing the lava's `BuildGridSheet`). `M_Ice` (rebuilt by
  `build_terrain_materials.py`, `WR_MATERIALS=ice`) is glassier: deep teal-blue and clear where it's thick, frosted at the
  rim, a mirror-smooth top (roughness 0.03, specular 0.8) with faint frozen ripples, a cool sheen at grazing angles, and
  the lava's far lift. What's slippery is still the 6 m cells, so play is as before. Checked in pictures from above, the
  side and low, by day, dusk and night, on a river and the glacier ring. Everything else in the dressing file is
  byte-for-byte what it was.

- **Far views: a friend who joins sees the whole island, as the host does.** A server loads every cell of the map
  (server streaming is off), so the host always had the whole island; a player who joined loaded the ground within
  `world.streamingRangeScale` x 256 m, 1 km at x4. Standing 1.3 km west of the middle, a friend had 59 of the 257
  landscape pieces and the far peaks were missing from their view (the host, at the same spot, had them all). x24 (6 km,
  the whole island from anywhere: 5.7 km corner to corner) is the default now. Benchmarked on Epic, a joined client:
  8.33 ms / p99 10.7 / 4,361 MB at x4, 8.38 ms / p99 10.7 / 4,654 MB at x24, and nothing left to stream in after the
  2.5 km move (Docs/PERFORMANCE.md). HLODs aren't needed for it. New `WRStreaming` logs what this machine has loaded;
  worldstest checks the friend has all 257 pieces.
- **A crash when the host changes the world's settings, fixed.** The animation workers read two tuning numbers
  (`anim.footMaxDrop` / `footMaxRaise`, not in tuning.json, so the fallbacks) through `FWRTuning::Num`, which adds a
  missing key to its warn-once set: two threads adding at once, or the host's settings change reloading the tuning at
  that moment, corrupted it (`Assertion failed: Index>=0 && Index<NumBits` in a TSet, the worlds test's host, a friend
  in the game). They're read on the game thread now (`FWRAnimFrame::FootMaxDrop / FootMaxRaise`), and `Num` only keeps
  that set on the game thread. An old race, not new in 0.7.

- **Crafting time with a queue at each station** (`AWRStation::Queue` / `Output`, tuning.json `craft`). A station's
  recipe now takes its time (recipes.json `seconds`, 2-7 s, there since the first drafts but read by nothing) over the
  world's crafting speed (`server.craftMultiplier`, also read by nothing until now: 2x in Casual co-op and Playtest, so
  1-3.5 s). Craft with a quantity is one order: its inputs are taken when it's ordered, and the station makes them one
  after another. It keeps going with nobody at it; each one goes into its maker's bag while they're within 20 m, else
  into the station's tray (8 slots). With no room for it, it waits. Up to 6 orders a station, 50 of one recipe an
  order; a broken Hearth stops them where they are. Hand recipes stay instant.
  - The Crafting tab: the details say how long ("Takes 1 s each: 5 s for 5. It keeps going if you walk off."); under the
    cards, a strip with the station's orders (the first with its bar, "4 of 5"), each with **Cancel**, and what's
    **Ready** in the tray with **Take all**. A notice when an order you walked off from is done.
  - Cancel gives back what wasn't made (the one under way too), to its maker or the base's owner and clan; what doesn't
    fit lands in a bag at the station. A station smashed or demolished leaves its tray and its orders' materials in a bag.
  - The quality roll, perks and double brews use the maker's skill when they ordered it, so a queue finishes the same
    after they've walked off or logged out (skill XP comes as each one is made, while they're on). Saved with the
    station (`queue`, `tray` in its piece in world.json); the one under way keeps the time it had left.
  - `FWRCrafting::Make` is split into `Shortfall` / `TakeInputs` / `HandOf` / `MakeOne`, shared by the queue and the
    instant path (`craft.timeScale` 0: crafttest and survivaltest run that way, as they test what's made). New
    commands `craft <id> <element> <times>`, `craftcancel <n>`, `crafttake`, and `saveworld` (tests).
  - `Tools/crafttimetest.ps1` (alltests `crafttime`, 10 checks, all pass): by clicks at a forge, five ingots one at a
    time about a second apart into the bag, ten more walked off from going into the tray with the notice, Take all,
    two swords cancelled with their materials back, and an order under way saved with its forge.

- **Creature raids** (new `WRRaids.cpp/.h`, numbers in tuning.json `raid`). The world setting "Creature raids" (Off /
  Rare / Normal / Frequent; `server.raids`, already in every preset but read by nothing until now) is on the world
  settings screens (new world and Esc > World): a claim's raid comes every 72 / 24 / 12 game hours or so (give or take
  a quarter), counted per claim from when it's first seen (`AWRClaimTotem::NextRaidHours`, saved with the totem as
  `nextRaid`; claims from older saves get their first one interval after this update loads them).
  - Only while someone of the claim (its owner or their clan) is home: a due raid waits until one of them is within
    250 m of the totem (`raid.homeRange`), since creatures only exist near players. Nobody's base is raided while
    they're away, and a raid ends (the rest leave) if everyone of the claim goes offline.
  - A minute's warning (`raid.warnSeconds`) to everyone of the claim who's on, wherever they are: a card like a boss's
    with the raiders' portrait ("RAID / CINDERFANGS / 5 Cinderfangs are coming for your base from the east, in 60 s")
    and a notice. Then they come from 30 m outside the claim's edge, on dry ground.
  - Who: the claim's region's hunters (`Data/spawn_regions.json` new `raiders`: Cinderfangs in the volcanic lowland,
    Cinderfangs and Scorchwings in the midland, Magmaws and Cinderfangs in the highland; Gloomhowls, Glacimaws, Rimeclaws
    on the tundra side), rolled by weight. How many: 3 points plus 2 per Hearth level (no Hearth: 3; a new Hearth at
    level 1: 5; level 3: 9), a lowland hunter costing 1, midland 2, highland 3 (`WRRaids::Plan`).
  - What they do (`AWRCreatureAI::TickRaid`): each goes for the nearest piece of the claim (foundations and the totem
    count as further off, so walls, doors, chests and stations go first; a broken Hearth is left alone) and bites it:
    its bite x 1.5 every 1.6 s (a Cinderfang: 18), with the bite's wind-up and its dust or splinters. Anyone who comes
    near is attacked as any hunter would. They never sleep or go after prey, and their leash runs from the base.
  - The end: all killed ("Raid over: you held them off, 2 pieces lost"), or after 6 minutes the rest walk off and are
    taken away out of sight. The claim's next raid is then one interval away. A raid in progress isn't saved.
  - Testing: `WRCmd raid now [warn s] [from bearing] | due | status | plan | end`.
- **A broken chest leaves what it held in a bag** (`AWRLootBag::Spill`, `building.spillMinutes` 30, anyone's to take).
  A chest smashed, demolished or decayed used to take everything in it with it.
- Tests: `Tools/raidtest.ps1` (alltests `raid`, 14 checks, all pass): the setting read, the first raid an interval
  away, the plan by Hearth level, a raid from the east (the card and notice, 5 Cinderfangs for the level 1 Hearth, 24
  bites, the chest broken first and its things in a bag, the player bitten on walking in, killed: held, next scheduled),
  a due raid waiting while the player is 400 m off and coming when they're back, timing out and the rest taken away, and
  the next raid saved with the claim. Automation `Wildreach.Raids.Plan` / `.Regions`. worldstest clicks "Frequent" on
  a new world's form and finds `"raids": "frequent"` in its settings.json.

## 0.6.3: your base (the Hearth found, repaired; chest names, waypoints)

**Not released on its own: it goes out in 0.7.0** (its notes, `Docs/release-notes/v0.6.3.md`, are folded into 0.7.0's).
From Raven's play of 0.6.2: "there was nowhere I saw to
build the Hearth or upgrade it". The build bar's card row was a horizontal scroll box exactly three cards wide with a
4 px scroll bar; the Stations tab had eight cards and the Hearth was the eighth (the claim totem the seventh), so neither
was ever on screen. Most tabs overflowed the same way (stone walls, floors, roofs). No test caught it: every test placed
the Hearth by command (`WRGhost hearth`), never through the bar.

- **Build bar** (`WRUIBuild.cpp`): a **Claim** tab holds the claim totem and the Hearth. The cards go a page of three at
  a time (`PerPage`): round ‹ › arrows on the row's ends, level with the pictures (shown only when there's a page that
  way), the mouse wheel over the cards (`SWheelArea`), and a "4–6 of 6" tag on the row's top edge. Placing a piece turns
  the row to its page. The tab strip shrinks to fit on narrow screens (`SFitWidth`; at 16:10 the eleven tabs ran into the
  Demolish button, and SScaleBox's ScaleToFitX only clipped them: it sizes from the last frame's paint geometry and
  never scaled down here). The Hearth's card says "E on it: feed it major bosses' cores".
- **First-claim hint** (`FWRFlow::CheckHints`, id `hearth`): standing in your own claim with no Hearth covering it, "Your
  claim is up. Build a Hearth in it (build menu, Claim tab), then E on it: boss cores raise every station's tier."
- **A broken Hearth stays and can be repaired.** The Hearth's build piece took the damage and was destroyed the moment
  it reached 0, cores and all; the Hearth actor's own "broken" state was never seen, and `AWRHearth::Repair` was never
  called (the map even said "repair it to craft"). Now the piece stops at 0 and the Hearth's health follows the piece's
  (`SetHealthFraction`; loaded from the saved `hp` too), so a broken Hearth stands, stops its stations and keeps its
  cores. The Hearth screen has **Repair** under its health while it's damaged: `hearthrepair` → `AWRHearth::RepairFor`,
  half the piece's build cost scaled by the damage (`hearth.repairShare` 0.5; 15 stone + 3 copper ingots when broken),
  owner and clan only.
- **Chest names**: `AWRBuildPiece::CustomName` (replicated, saved as `"name"`), `chestname <words>` (24 characters,
  owner and clan), the chest screen's **Rename** opens a name box over the screen that takes the keyboard (UI only, as
  chat does; focus a frame after it opens, or the Rename button takes it back), Enter / Save / Esc / Cancel.
- **Map waypoints**: right-click the journal map (`WRUIMap::SetWaypoint`; right-click it again clears it); a brass pin
  with its distance on the map and on the HUD compass, waiting at the compass's end with an arrow when it's behind you.
  This client's own, for the world it was set in (not saved). The map's controls row lists it.
- **Your base on the map from anywhere**: claim totems and Hearths (and their build pieces) are `bAlwaysRelevant`, so a
  friend's own Hearth and claim reach them across the island (they replicated only within 150 m and dropped off the
  map). The map shows someone else's claim only within 150 m, as before.
- Testing: `WRMouseClick ... once` (no take-back click first: a second click would undo a toggle like the waypoint).
  New `Tools/basetest.ps1` (alltests `base`, 12 checks) does all of the above by real keys and clicks as the host at
  1440x900, 16:10: 12 of 12 pass.

Checked on the packaged build (`C:\Dev\WildreachBuild6`, Setup 1,681 MB, 667 files, 9 changed from 0.6.2) against a local
stand-in for GitHub serving its Setup:
- a clean install from it matches the build in all 667 files;
- **0.6.2 with its updater** (four connections): 9 of 667 files changed, **231 MB**, all 667 files right, the save kept,
  no working files left, the new updater in place; **0.6.1 with its updater**: 12 files changed, the same 231 MB, all right;
- the packaged game, updated from 0.6.1, passes basetest's 12 checks on its own client log (the same steps by keys and
  clicks), and loads a copy of Raven's "playtest 0.6.0" world from the main menu (14 pieces, 154 packs, the Claim tab
  showing the totem and the Hearth). The only error is the engine's known PSO-precache ensure on the frame Play is
  clicked. Raven's real worlds untouched;
- the full suite on this code: 30 of 33 passed; the three that failed in the loaded run all passed run alone (`menus` 12
  checks, `flow` 22, `roam` 25), so every test passes. The `menus` host's GPU hung (`DXGI_ERROR_DEVICE_HUNG`) 5 s after
  the friend joined, with two game windows on the GPU: not the 0.5.0 hang (Nanite tessellation's `PatchSplit`) but the
  virtual shadow maps' Nanite `NodeAndClusterCull`, the same place as all 7 earlier hangs since 0.5.0 (`menus` on 11-04,
  the creature-video, size-check and portrait tools), every time with the GPU heavily loaded. Not new in 0.6.3; not
  looked into yet.

## 0.6.2: updates download over four connections

**Released 2026-11-07** as [v0.6.2](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.2) (Latest; commit
16984a9, `C:\Dev\WildreachBuild5`). A copy of 0.6.1 updated itself from the real release in 32 s (235 MB, 12 of 667
files, all 667 right, the save kept, nothing left over). The first release players on 0.6.1 get as a small update.
Before release, checked on the real build against a local stand-in for GitHub serving its Setup (1,681 MB, 667 files;
the new one is `Data/spawn_regions.json`):
- a clean install from it matches the build in all 667 files;
- **0.6.1 with its own updater** (what players have): **230 MB** (12 of 667 files changed), 13 s here, all 667 files right,
  the save kept, no working files left, and the new updater in place;
- **0.6.0 with its updater** (Raven's install): the whole Setup, as that updater always does, all 667 files right;
- the packaged game on a copy of Raven's "playtest 0.6.0" world (saved by 0.6.0: 154 packs, 467 animals, packs up to 11)
  loaded as 368 animals, every pack in a region; over a couple of minutes a periodic check added a Magmaw to the volcanic
  highland (6 of 7) and the regions over target thinned; saved as 149 packs, 363 animals, none over 4. Island (never
  played with wildlife) and a new world each filled every region (348 and 351 animals). The only error in the logs is the
  engine's known PSO-precache ensure on the frame Play is clicked. Raven's real worlds untouched.
- the full suite (`Tools/alltests.ps1`) on this build: 30 of 32 passed; the two that failed in the loaded run both passed
  run alone (`worlds` 31 checks, `updater` 44 checks), so every test passes.

GitHub gave this machine about 140 KB/s a connection when 0.6.1 came down this morning (22 minutes for 234 MB), and four
connections at once each got the same.

- **The updater now downloads over 4 connections at once** (`Tools/installer/WildreachUpdater.cs`, `Connections`). The
  changed blocks go in runs of no more than 8 MB, so one big changed file (the game's exe) is shared out too, and four
  threads take the next run as they finish. A dropped connection still picks up at the block it was on.
- **The whole Setup comes down in parts as well** (8 MB each, written into place) whenever the small way only wasn't worth
  it (no index, or over 80% changed). If parts of the Setup came down wrong or the server won't do partial downloads, it
  still asks for the whole file in one go. The Setup checks itself when it runs, as before.
- .NET's default of two connections to a server is raised to eight.
- **Measured:** GitHub at 1, 4 and 8 connections this afternoon: 5.5, 16.9 and 11.6 MB/s in all, so four, not eight. The
  real update off GitHub (a copy of 0.6.0 to v0.6.1, 35 parts) came out matching the 0.6.1 build in all 666 files. This
  afternoon GitHub was quick enough that the line was the limit: the old updater and the new one both took 27 s. With
  every connection held back, as GitHub does at a busy time (the offline test, each about 1.4 MB/s), the four together
  went 3.3 times as fast as one: 4,789 KB/s.
- `Tools/updatertest.ps1` (44 checks, all pass): the small update stays at 4 connections or fewer, the whole Setup in parts
  (with a dropped connection in the middle), and every connection held back.

## 0.6.2: wildlife spawns by region

Raven asked for spawning that works like ARK's ("define regions, give each a weighted creature list plus a population
target, and run a periodic check that refills missing slots away from players and structures"), and small packs: "small
packs like 3-4 feel normal for pack animals".

- **Regions** (`Data/spawn_regions.json`, new): the island's two sides, each in its three bands (lowland, midland, highland),
  so six. Each has a population target and its creatures, each with a weight (the chance a new pack is of that kind). The
  weights are the old site counts, so the mix is as before; the targets are those sites times the new pack sizes: volcanic
  133 / 57 / 7 and tundra 74 / 66 / 7 (low / mid / high), 344 animals in about the same 156 packs. A region can also be cut
  down to a box (`area`), for a valley of its own later. `server.creatureNumbers` still scales every target.
- **A new world fills every region** to its target, each pack's kind rolled by weight, somewhere that kind can live (its
  ground on the walk map, clear of the camps).
- **The periodic check** replaces breeding and wandering in: every `wildlife.spawnCheckSeconds` (60) a region short of its
  target gets up to `wildlife.spawnPacksPerCheck` (2) new packs, never within `wildlife.spawnAwayFromPlayers` (300 m) of a
  player, `wildlife.spawnAwayFromBuildings` (100 m) of anything built, or in a land claim. Each one is logged
  (`WILDLIFE spawned ashstrider x3 in volcanic_lowland at ...; 412 m from a player, nothing built`), and the step log has
  each region's count against its target. A region over target (creature numbers turned down) still thins out of sight.
- **A pack belongs to its region** wherever it roams (saved as `region` in `wildlife.json`), so a herd that's wiped out is
  made up for by a new pack somewhere else in that region, not by the survivors breeding back. A pack that's down to none
  is gone, burrowers too.
- **Small packs** (`creatures.json` `pack`): Cinderfang, Gloomhowl, Ashstrider, Emberhorn and Frostantler 3-4 (they were up
  to 6, 7, 12, 8 and 8); Basaltback, Woolly Drudge, Glowmoth, Pumicehog and Snowpuff 1-3; Kilnmite and Rimeshell 2-3;
  Magmaw, Rimeclaw, Scorchwing and Aurora Fox 1-2; Glacimaw alone. Nothing wild comes in more than 4.
- **Old worlds:** a pack saved bigger than its kind allows now is cut down to the most it can be, and a pack with no region
  joins the one for its kind where it stands. The regions then make up any shortfall with new small packs.
- `wildlife.refillPerHour` and `wildlife.arriveDistance` are gone. `Data/island_spawns.json` stays as reference ground (the
  walk map and the tests stand on its sites).
- **Tests:** `Wildreach.World.SpawnRegions` (every region on its side and band, every wild kind in one, targets);
  `Wildreach.World.WalkMap` now wants packs of 4 or fewer. `Tools/roamtest.ps1` checks a new world against each region's
  target, pack sizes, kinds by region, an emptied region refilling (a player and a building in it), at most 2 packs a
  check, and the distances. `Tools/wildlifetest.ps1` seeds its two herds in a saved world; `Tools/rulestest.ps1` compares the
  share and the animals (the pack count is now only about in proportion).
- **Results:** unit 28/28, roam 25/25, wildlife 11/11, rules 8/8, water 13/13, gather 19/19, survival 25/25, ai 11/11,
  tame 10/10. A new world: 148 to 157 packs, 347 to 353 animals for a target of 344; packs x1 42-52, x2 35-41, x3 49-51,
  x4 16-21. Volcanic lowland emptied with a player in it refilled 0 to 133 in 33 checks, at most 2 packs a check, the
  nearest new pack 309 m from the player and 605 m from a building (600 m asked for the test). A copy of Raven's 0.6.0
  world (154 packs, 467 animals, 31 packs over 4) loaded as 368 animals, every pack in a region, no errors.

## 0.6.1: much smaller updates

Raven asked why an update is another 1.7 GB when the game updates itself. It was downloading the whole Setup every time.
**Released 2026-11-07 as [v0.6.1](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.1)** (Latest, the
same 1.7 GB `Wildreach-Setup.exe` for everyone this once, notes `Docs/release-notes/v0.6.1.md`; GitHub's digest matches the
build). Then the real thing, off GitHub: a copy of 0.6.0 with the new updater fetched **234 MB** (11 of 666 files changed)
and came out matching a clean 0.6.1 install file for file. It took 22 minutes: GitHub served this machine about 140 KB/s a
connection (another site, 2.2 MB/s), and four connections at once each got the same, so fetching in parallel would be about
four times quicker.

- **What actually changes between builds** (0.6.0 against 0.6.1, compared block by block): the content file, 1,383 MB, came
  out byte for byte the same. Nearly all of a code update is `Wildreach.exe` and `Wildreach.pdb`: one change shifts addresses
  all through them, so they're effectively new each time (120 MB and 112 MB compressed). Everything else is small.
- **The Setup is now its own patch source** (`Tools/installer/WildreachPack.cs`). Its game is no longer a zip: every file is
  cut into blocks of about 256 KB at places chosen by the bytes (content-defined chunking, so a change only disturbs the
  blocks around it, even when everything after it moves), each compressed and stored once, with an index and a footer on
  the end of the program. A release is still one file, with nothing else to upload, and a new player's install is the same
  as before. `Tools/installer/WildreachPackTool.cs` makes it, in under a minute.
- **The updater fetches only what's missing**, with exact-range requests to that same file. (GitHub's store serves those,
  through the redirect, but not "the last N bytes", so it asks the length first.) It cuts its own files the same way,
  downloads the blocks it hasn't got, builds each new file beside the old one and checks its hash, then swaps them in with
  `version.txt` last, so a copy that stops part way still says it's the old version and the next start finishes the job.
  Fetched blocks survive an interruption. Files a player made are never touched: only what's on the last version's
  `installed-files.txt` can be removed, and an index that names anything under a `Saved` folder is refused outright.
- **If the small way can't be done** (no partial downloads, a damaged block, an old-style Setup, or over 80% changed) it logs
  why and downloads the whole Setup as before. A file in use puts everything back and says so, without that download.
- **Measured on the real builds, each ending with all 666 files matching a clean install:** a code update (0.6.0 to 0.6.1)
  **235 MB** of 1,681 MB, 13 s here; a settings-only patch (one number in `tuning.json`) **295 KB**, of which 28 KB is the
  change and the rest the index. The real jump from 0.5.0 to 0.6.0, every creature rebuilt, would have been about 316 MB.
- **0.6.1 itself is still the full download,** because the updater every player has is the old one. That old updater was
  tested taking the new Setup, on the real build. Small updates start with the next release.
- **Old Setups no longer pile up.** Every full update left its 1.7 GB Setup in `%TEMP%\Wildreach-Update`: deleting it straight
  after it ran failed silently, most likely because a virus scanner was still reading it. The updater now clears that folder
  when it starts, so leftovers from 0.6.0 and earlier go too.
- `Tools/updatertest.ps1` now covers all of it: the small update, each fallback, a dropped connection, the updater killed part
  way, half swapped, a file in use, the updater and Setup from 0.6.0.

## 0.6.1: aiming and building feel

From Raven's first feedback after 0.6.0.

- **A crosshair**, always up: a dot and four short ticks at the middle of the screen, red over something hostile. Settings >
  Controls can turn it off (it still shows while aiming, placing or in first person). `ui.crosshairFoeRange`.
- **Spells go where it points.** Bolts flew parallel to the camera from the chest, so with the camera behind the shoulder
  they passed beside what you aimed at. They now leave the chest through the point under the crosshair
  (`AWRCharacter::CrosshairPoint`, which works on the server); Lava pool and Meteor land on it. 6 hits from 6 at 15 m.
- **Locked on, spells hit what's moving.** The lock lived only on the client, so the server just fired along a lagging
  camera. Now it knows (`ServerSetLock`): bolts aim ahead of where the target's going (`UWRCombatLibrary::LeadPoint`) and
  curve toward it (`AWRProjectile::HomeOn`; `spells.lockHoming`, less for other players so they can dodge). Lava pool lands
  under the target, Meteor where it'll be when it lands. At a player always running across at 15 m, turning back at each end
  of a 24 m lane: 7 to 10 bolts from 10 and 3 to 4 Meteors from 4; with the lead and the curve turned off
  (`aimtest.ps1 -NoAssist`) 1 bolt from 10 and 1 Meteor from 4.
- **Lock-on took things you couldn't see.** It never checked for buried burrowers, nor for anything in the way. Every
  volcanic cave has a buried Kilnmite colony (2 right in front of the camera in the cave tested), so it could turn the view
  to a bare patch of cave or the lava beside it. Only a host had this: a buried creature is hidden with no collision, so it's
  never sent to a client who joins. It now
  takes what's visible with a clear line (`CanSeeTarget`), nearest the crosshair rather than nearest you, and lets go if the
  target goes underground or stays out of sight (`camera.lockOn*`). What's locked gets four gold corners on screen: the new
  UI had no marker at all.
- **Pieces follow you.** The angle was set once, when you picked the piece. It now faces the way you look, plus however
  far you've turned it. Free (no grid under it), any angle: the rotate key turns it 15 degrees, held it spins, the wheel
  turns it 5 (`building.turnStep`, `spinSpeed`, `wheelStep`).
- **Snap has three settings.** F goes Grid, Fine, Off. Furniture on a foundation or floor now snaps to that piece's marks
  and sits square to it: 100 cm and quarter turns on the grid, 50 cm and eighths on the fine grid. The fine grid also takes
  walls on inner lines 100 cm apart and pillars on their crossings. Marks show on the piece you're aiming at.
- **A foundation placed at an angle has its own grid** (`FWRBuildRules::SnapInFrame`): walls, floors, furniture and
  neighbouring foundations line up with it, and the server accepts them (`IsSnappedIn`) with the same corner rules.
- **Ghosts have real shapes.** Stations, the Hearth, the totem and the chest show their real models and structure pieces
  their real planks, in the tint. Demolish's red copy of a station is its real shape too, not a box.
- **Reach counts from you, not the camera.** Looking past it, or up, a piece that goes where you aim comes down on the
  ground at the end of your reach and keeps following your look. With no ground under it (out over a drop) it's refused:
  a chest could be placed hanging in the air.
- **Tests:** `Tools/aimtest.ps1`, `Tools/buildaimtest.ps1` (in `alltests`), `Wildreach.Combat.LeadPoint`, more in
  `Wildreach.Building.Rules`. New commands: `WRLockOn`, `WRSnap`, `WRGhostInfo`, `WRGhostTurn`.

## 2026-11-06: version 0.6.0, a living island

Everything from Raven's Oct 31 review ships together: the creature pass, beds and waking on the map, worlds, world
settings and roaming wildlife (their sections follow). **Released 2026-11-06 as
[v0.6.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.6.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB, notes
`Docs/release-notes/v0.6.0.md`): installed copies update themselves on their next start. These are the release checks and
what they turned up.

- **Death on the real island offered nowhere to wake, and Wake did nothing.** Found by playing the packaged game on a copy
  of Raven's save: the bot stayed dead for over 80 s. `WRBeds::CampsFor` misread `TSet::Add`'s second argument (it says
  whether the name was already there), so it dropped every camp on the island. Fixed, and Wake now brings you back at
  your start when there are no places at all, so nobody can be stuck dead. Every test had passed because the arena's
  starts share one name and the island preview used made-up places; `Tools/worldstest.ps1` now dies on the real island.
- **The menu's island has no roaming packs.** It's no world and saves nothing, so `WRWildlife::Start` leaves it alone. The
  map's own always-out pack still falls behind the menu until a game starts (`RESCUE` lines in the log, none in play).
  Older than 0.6 and nobody can see it, so it stays for now.
- **An old engine warning, not new.** One `Ensure condition failed ... bPSOPrecacheCalled` from `ULandscapeComponent` in
  the log on the frame you click Play, in the menu's world, which is thrown away the next frame. It costs about 0.25 s
  there and shows nothing. 0.5.0 does the same after the same wait at the menu.
- **The old save carries over** (run on a copy, the real one untouched): the first Host copied it into Island, listed as
  Played 28 Oct, with Raven's level 12 character on the Fire path and all 9 pieces. Their real bed could be set, was
  offered on the death map with three camps, and woke them 13 s after dying. Back at the list it read "Played today, 1
  min played". A new world from the same list started a fresh character.
- **Full suite on the final code:** 28 of 30, and the other two passed when run again alone. `mp` counted 98 movement
  corrections where it allows 90, as the first test while other work was running on the machine, and 69 alone (about
  what it has always been). `walk` now has no roaming packs: it checks the ground, and a Magmaw had caught the unarmed
  walker 1 km out and sent it back to a camp. That's the game working, but it failed the test by chance.
- **Real Windows mouse and keyboard** on the packaged build with no test flags, on a copy of the save. All of these worked,
  with nothing wrong in the log: hover and clicks on the menus and world list; the New world form (typing, preset tiles, a
  toggle, `+`, a dragged slider); walking, the camera, an attack, the ability wheel and the hotbar keys; items dragged in
  the bag and onto the hotbar; the journal's tabs; Esc > World with a dragged slider and Back to the preset; the build bar;
  the console; setting a bed with E; waking at a camp clicked on the map and at the bed clicked on the list; Main menu; a
  new world; Quit. Block on the right button was pressed but not confirmed, as no picture or log line shows it.
- **Installer:** `make_installer.ps1` now refuses a build that has been run, since it zips the whole folder and saves and
  logs would ship. Run tests from a copy. `package.ps1 -SkipEditorBuild` cooks with the editor as built, because
  UnrealBuildTool won't build the editor while another project's editor or `-game` run holds a Live Coding session.
- **Benchmark, quiet machine:** Epic 8.38 / 8.35 ms (119-120 fps; 0.5.0 was 8.34), Medium 5.44 / 5.56 ms, one 141 ms hitch
  in one Medium run. Server 2.37 ms as usual (29 animals in 8 packs; was 2.16), 8.35 ms with every pack out (432 in 156;
  was 6.46 with 328). With packs brought out at 600 m as a stress test: 10.9 ms (92 fps), 188 animals, the worst hitch
  1,073 ms at the start while they all load. No tuning changes.

## 0.6.0: wildlife that roams

From Raven's Oct 31 review: always a small pack standing still.

- **Packs live on when nobody is near** (`WRWildlife`). Each is a record that keeps its place, what it's doing, how many
  are left and how hurt they are. The server moves them every 20 s over a coarse map of where animals can walk
  (`Data/island_walk.json`, from `Tools/terrain/walkmap.mjs`: 32 m cells, the sea, steep ground, lava, arenas and caves).
- **You meet them where they've got to.** A player within 250 m brings a pack out at its record's position, doing what it
  was doing; when everyone has gone it's a record again. A pack in view keeps walking at its own pace with its creatures
  following it, and waits for any caught up in a fight, a hunt, a meal or running away.
- **By kind:** grazers drift between feeding grounds and rest when they sleep; predators keep a territory and go after the
  nearest prey they can reach when hungry, faster, by night unless they sleep through it; a few packs travel far
  (`wanderChance` in `creatures.json`: Basaltbacks, Woolly drudges, Aurora foxes, Scorchwings, some Emberhorns and
  Frostantlers); burrowers and colonies stay home. Everyone keeps to their side of the island and their elevation band,
  and clear of the camps, as the sites always did, so the strong ones don't walk into the starting areas.
- **Hunts nobody sees are settled by odds** (`huntChance`, on the size rule the creatures already hunt by): caught or
  missed, the prey runs. Tames are never touched.
- **Pack sizes vary**, from a lone animal or a pair to herds of 12 (`pack` in `creatures.json`), mostly small. About 470
  animals on the island, up from 328.
- **A death is a death:** a pack no longer replaces its dead a couple of minutes later. Numbers and wounds stay with the
  pack, wounds mend slowly, and numbers come back slowly (10% of a kind's share an hour) by breeding and by new packs
  wandering in out of sight, towards its share: its sites x its usual pack x the world's creature numbers.
- **Creature numbers** now means how many packs: more or fewer at the start of a new world, and a change later comes on
  slowly.
- **Saved with the world** (`wildlife.json`, each minute with the buildings). Without a `world.json` a world starts over,
  and so does its wildlife. A world from before this starts new packs at the sites.
- **A pack never comes out inside a land claim:** it comes out at the edge of it, and doesn't pick a goal inside one.
- **Map-placed packs and bosses** hold their place and replace their dead as before.
- **Tests:** `Tools/roamtest.ps1` (in `alltests`): a player stands still through two fast island days while packs travel,
  rest, hunt and breed around them, then a herd is watched walking at normal speed, a pack is thinned and hurt and stays
  so through a server restart, and one inside a claim comes out at its edge. `Wildreach.World.WalkMap`. Moving them all
  costs about 2 ms each step. New commands: `WRPacks`, `WRHurtPack`, `WRFaceHerd`.

## 0.6.0: world settings

From Raven's Oct 31 review: four presets existed, but only a command-line switch could pick one.

- **The New world form has the rules:** pick a preset on the left and tweak it on the right. Players (PvP, PvP in boss
  arenas, what happens to what you carry when you die, offline raid protection, clan size), Speeds (XP, gathering, taming,
  crops) and The world (day length, hunger and thirst, creature numbers, creature difficulty, building upkeep, boss
  respawn). Sliders with - and + for one step. Picking a preset sets them all to its values.
- **The host can change them while playing:** Esc > World (`SWRWorldPage`, only on the host's menu): the preset, the same
  rows, and Back to the preset. Changes are saved and in force a moment after the last one, go to everyone in the game, and
  other players get a notice when the host is done.
- **Saved with the world:** `settings.json` holds just the preset, or `"custom"` with the base and only what differs from
  it. A changed world shows **Custom** on the list. `FWRWorldSettings`, `WRWorlds::PresetSettings / ReadSettings /
  WriteSettings`; `AWRGameMode::ChangeWorldSettings`.
- **Four new settings reach the game:** `server.dayMinutes` (the clock keeps its time when it changes),
  `server.hungerMultiplier`, `server.creatureNumbers` (under 1x, that share of the wildlife sites; over 1x, bigger packs;
  from the next start of the world, until roaming wildlife replaces sites) and `server.creatureDifficulty` (health and
  damage of wild creatures and bosses as they appear; tames keep their own numbers).
- **Boss respawn** is read when a boss dies, so changing it in game counts from its next defeat.
- **Not on the screen, because nothing is behind them:** crafting speed (crafting is instant), bounties and raids (no such
  systems yet). Their preset values are still in `Data/server_presets.json`. The preset descriptions on the form no longer
  promise them.
- **A client now takes the host's settings whole** (`ImportServerSettings` clears its `server.*` first), so a setting put
  back to its default doesn't linger.
- **Tests:** `Tools/rulestest.ps1` (in `alltests`) plays the island three times with different `settings.json` and compares
  the clock, hunger and thirst, sites and creatures, creature and boss numbers. `worldstest` now also clicks the form's and
  Esc > World's steps, with a friend in the game who gets the new rules and a notice, and whose Esc menu has no World
  page (28 checks). New commands: `WRWheel` (the mouse wheel over a piece of text), `WRVitals`; `WRSpawn` logs what it made.

## 0.6.0: worlds

From Raven's Oct 31 review: only one world save, and characters, clans and settings shared by everything.

- **Each world is a folder**, `Saved/Worlds/<world>/` (`WRWorlds`): `info.json` (name, created, last played, play time,
  version), `settings.json` (what `ServerDB/server.json` was), `world.json` (buildings, tames, clock), `clans.json` and
  `players/`. Characters are per world: everyone starts fresh in a new one.
- **Host a game opens Choose a world** (`WRUIWorlds`): the worlds on this PC, last played first, each with its preset and
  when and how long it was played. **Play** (or a double click), **Rename**, **Delete** (asks first: Keep it / Yes, delete
  it) and **New world**: a name and a preset (PvE first, PvP, Casual co-op, Playtest), then **Create and play**. The last
  world played is picked next time. Joining is unchanged. The full settings screen comes next.
- **The old save moves over by itself:** the first time the list opens, `Saved/ServerDB` (`world_L_Island.json`, every
  character, clans, `server.json`) is copied into a world called **Island**, with its preset kept, and left in place as
  the backup (`Saved/Worlds/moved_from_ServerDB.txt` records it). Hosting without a world named does the same for L_Island.
- **The game picks its world at the start of each game** (`AWRGameMode::InitGame`): `?World=<id>` from the menu,
  `-wrworld=<id>` for tests and dedicated servers, else the map's name without `L_`. It reloads the settings and clans
  then, so a second world hosted in the same run gets its own.
- **The menu's island is no world any more:** it used to load and save `world_L_Island.json` behind the menu, and left a
  character file for every machine-named player. Now it loads and saves nothing (unless a test gives it `-wrworld`).
- **Worlds a server was only pointed at stay off the list** (no `info.json`): a dedicated server's, and all the tests'.
- **Tests:** `Tools/worldstest.ps1` (in `alltests`): one game window, by real clicks, through two worlds and back:
  separate buildings, characters and presets, rename, delete, play time, and the old save copied into Island. Every test
  script now keeps its own world folder; `survivaltest`, `housetest` and `skytest` get their own instead of sharing
  L_Island's. Bot commands take `@2:` (only in the second world this run enters), for runs that pass through the menu.
- **`uiflickertest` no longer sees flashes that aren't there:** it split its frames by the log's own frame counter, which
  can change part-way through a burst of lines, so one look at the Skills tab (136 lines) came out as two jumbled ones and
  failed. `WRUIText` now starts each look with a `UIFRAME` line.

## 0.6.0: beds and respawn

From Raven's Oct 31 review: a bed should be where you wake, and still be there after a restart.

- **Beds are saved** with the world (`sleepers` on each bed in `world_<name>.json`), and your chosen bed with you
  (`bed` in your player save), so a restart keeps both.
- **Placing a bed or bedroll sets it** ("Built bed. You'll wake here"); using one sets it again.
- **Choose where to wake on the map** (Raven: "you die and then you're looking at the map"): the death screen is the
  island map with every place you can wake on it (your beds, your clan's beds, your path's camps) and a red pin where
  you fell, framed around them; click one to pick it (a brass halo), or pick from the list beside it, which gives each
  one's distance and direction. Then press **Wake** ("Wake at your bed"): it works once the 10 s wait is over and you
  wake there, standing on top of the bed. No timer wakes you any more (`player.autoWakeSeconds` > 0 brings that back).
  The default pick is the bed you last set (or another of yours that's ready, else your camp). Names on the map step
  aside for each other; the map zooms up to 12x for beds close together. `WRFlow preview death` shows it with made-up
  beds and that side's camps.
- **Beds rest, bedrolls wear out**: a bed can't be woken at again for 60 s after you use it ("resting, N s" in the list);
  a bedroll is used up when you wake on it ("Your bedroll is worn out"). Both are in `Data/tuning.json` (`beds`).
- **Beds on the map**: a new legend row, "Beds · where you can wake", with your beds in brass and your clan's in frost.
- New `WRBeds.h/.cpp` holds it all; a bed that's destroyed drops out of everyone's list.
- `Tools/flowtest.ps1` has a second phase: restart the server, check the bed was saved, place a bedroll, die, click
  "Your bed" on the death screen's map, check nobody wakes until Wake is clicked, wake on the bed, then die again: the
  bed is resting, so you wake on the bedroll, which wears out (22 checks).
  Test bots can act while dead with a `dead:` prefix (`dead:WRClickMap Your bed`, `dead:WRClick Wake at`); otherwise a
  dead bot presses Wake as soon as it can. New `WRClickMap <name>` clicks a named place on whichever map is up.
- The server gives Wake a second's grace: its world clock runs a little behind the death screen's countdown.

## 0.6.0: better-looking creatures

Raven asked for better, more realistic creature movement and looks, keeping the design doc's fantasy, and reviewed
every creature from front and side pictures; this is the work so far (round 2 of their notes).

- **New creature animation** (`Tools/art/anim_creature.py`): feet planted while they bear weight (two-bone IK), the body
  rising, rolling and swaying with its legs, a steady head, a tail on springs, attacks that wind up and strike in a tenth
  of a second; gaits picked by size and speed (walk, trot or amble, gallop). On Cinderfang, Woolly drudge, Ashstrider,
  Frostantler, Aurora fox, Emberhorn, Basaltback and Rimeclaw. Front legs stand as straight columns under the shoulders
  and fold at the wrist (Raven: "an extra joint" on Cinderfang's and Ashstrider's front legs, Frostantler's "bend towards
  the back feet"); the back stays near level.
- **Four legs** unless the real animal has more: Woolly drudge and Cinderfang lost their middle pair (the Kilnmite keeps
  six). Cinderfang's front right leg, what was left of the sixth, is now a mirror of the front left.
- **Shapes** (`rig_quadruped.py`: `--mirror-leg`, `--straighten-front`, `--legs-longer/--legs-thicker`,
  `--head-forward`, `--widen`): Woolly drudge on longer, thicker legs (its chest no longer meets them); Basaltback's head
  out from under its shell; Glacimaw 40% longer (`rig_special.py --stretch`) and far bigger (`halfHeight` 95, reach 380);
  Rimeshell wider (`meshWiden` 1.3, new in `creatures.json`).
- **Surface detail** (`Tools/art/bake_detail.py`, a normal map and shaded colour baked on each model's UVs): scales all
  over Magmaw, feathers on Snowpuff, fur on the Aurora fox, whose hard green patches are now pale fur under a soft
  teal-to-violet aurora. `M_CreatureGlow` takes a normal map (`MI_<Name>Skin` for creatures with no glow).
- **Ember glow** that really glows (`M_CreatureGlow`) on Magmaw, Cinderfang, Basaltback and Emberhorn; the red attack
  wind-up glow is off (`telegraph.glow` 0: the pose and growl warn).
- **Gloomhowl looks ahead** instead of at the ground: `poseLift` in `creatures.json` turns named bones nose-up over every
  clip (its pack clips hang the head low).
- **Gloomhowl's eyes glow at night** (Raven's pick): its red irises (the Barghest's own glow map) light up as the
  daylight goes, out again at dawn and when it dies. New `nightGlow` in `creatures.json`: the creature's materials'
  `GlowStrength` rises to it with the dark (`1 - daylight`); `MI_Gloomhowl` keeps the iris's own colour.
- **Woolly drudge walks on pillars**: its front shoulders sat 15 cm behind its feet, so each step reached far ahead on a
  slant and the hoof curled under in the swing. Re-rigged with the shoulder over the foot and the front joint up inside
  the body (`rig_quadruped --front-elbow 0.42 --front-hip-x 0.12`, clips `--swing-foot level`); the narrow neck every
  leg had above its hoof (a wine-glass stem once the leg stood upright) filled out to the hoof's width.
- **Woolly drudge is a giant** (Raven: 3x): 5.2 m to the hump (`halfHeight` 258, `radius` 270), reach 420 (its face is
  ~2.6 m ahead of its middle); its stomp ring grows with it (9.6 m). The pale and grey fur tips round its ankles and the
  tip of its beard now take the brown of the fur around them (tusks and horns stay ivory).
- `creatureportraits.ps1 -Frame <sizes>`: a wider camera for giants (the frame's fixed margin shrinks beside them).
- **Frostantler's antlers end in tines** (Raven): each main beam ended in a fat club with a holed knob (a melted blob,
  an S-shaped lump from the side); both are now tall pointed tines, and the dark flecks in the antlers are ice. It is
  **twice the size** too: 3 m to the antler tips (`halfHeight` 150, `radius` 110), reach 300. The pale cyan stripe down
  each flank fades into its body, and the nubs hanging under its belly (a stray piece in the game) are gone.
- **Rimeshell loses its side fin**: the image model gave it a broad flat fin sticking out of its left side under the
  shell (not in its concept), which also put its body ~13 cm off-centre in the game. The fin now folds into a lip under
  the rim (mirrored from the clean side), the body sits in the middle, and the front shoulders sit over the feet
  (`rig_quadruped --front-hip-x 0.34`: its steps reached ahead on a slant).
- **Snowpuff's toes are bird-sized**: the image model gave its feet toes about twice as long as its legs are tall, which
  scissored along the ground like dark planks as it walked. They are now about as long as its shins, so it steps like a
  fowl.
- **Aurora fox stands and trots, with a real fox's brush**: its tail was a ball as wide as its hips resting on the
  ground, so the old rig took it for a foot and mixed up its legs: it crouched with its rump down and crawled. The tail
  is remade as a long brush that leaves the rump nearly level and droops in an arc, bushiest past the middle and closing
  to a point (its old surface and colours swept onto the new shape; where it had grown into the backs of the thighs
  it is cut free), and covered in 3200 fur locks combed toward the tip. Re-rigged on its four real legs with the
  shoulders over the paws, on the new animation (`anim_creature --build light --attack bite`).
- **`hair_locks.py --along`**: locks lie along a line in the side view instead of one way (a tail's fur combed toward
  its tip round the curve).
- **Creature films save a tenth of the frames** (Raven: "do you need to take hundreds of screenshots of the same
  frame?"): `WRRecord` takes windows of frames to keep (`first-last/step,...`; every frame's time is still logged), and
  `creaturevideo.ps1` keeps 88 of the 756 by default (`-Keep all` for every frame): the idle once a second, the walk
  and run every half second with a steady stretch of each at every 2nd frame (f126-161, f234-257), the bites every 4th,
  the ramp, flinches and death every 20th. A film is ~90 MB instead of ~1 GB and records in 30 s instead of ~170 s.
- **Vulkarn's right side is whole again**: the boss build cut out the faces where its right arm had been fused to its
  body, which left a hole down its flank under the armpit (the floor showed through it in every swing), and the skin
  round it, partly weighted to the arm, tore into strips that hung off its back. The hole is closed on the shape of its
  left flank (mirrored), in its skin colour, and the skin round it takes the left side's weights, mirrored
  (`WildreachArt/mixamo/bosses_r2/vulkarn/vk_fix.py` on the game model; rig and clips unchanged). And he's bigger
  (Raven): 6.5 m instead of 5.2 (halfHeight 325, radius 188; his attacks reach from his body's edge, so they grow too).
  His two small holes (a hand, a thigh) are really closed now: `bmesh.ops.holes_fill` had made nothing on them, so a
  face is built from each hole's corners in order. The hand's patch faced inward (the sky still showed through it from
  outside), so both small patches have a back side too.
- **The Obsidian Warden's holes closed, and it's 20% bigger** (Raven): the same boss build left holes under its left
  armpit and on the front of its left shoulder (where the image model's left arm met its body), and a few round its
  neck; the sky showed through its shoulder in some swings. Each is filled with one patch in the skin's colour, with a
  back side (`WildreachArt/mixamo/bosses_r2/obsidian_warden/ow_fix.py` on the game model; rig and clips unchanged).
  Nothing round them tears in any clip, and its walk, run, swings and death play clean. A check that looks through the
  model from 26 directions in four clips (`WildreachArt/mixamo/bosses_r2/seethru.py`) finds nothing see-through on
  Vulkarn and only two pinhead specks on the Warden in one idle; the light patch in its shadow is the sun through the
  gap between its bent arm and its body. halfHeight 204, radius 132 (was 170, 110).
- **Ignessa: a face, real hair, wings on her back, arms and legs that move** (Raven: hair "like actual hair", wings
  joined "by her shoulder blades", "her face looks messed up"). Her face was a blur of the concept's (dark rings for
  eyes, the mouth a round open O): the concept's face is now laid onto it from the front, lined up on the eyes and mouth
  and blended into her own skin. Her hair is 4500 thin locks over the old lump (`Tools/art/hair_locks.py`, which can
  now keep to one material: the built wings share its UV space). Her wings hung off the back of her hair at neck
  height; they now grow out of her back opposite her breasts (Raven: at the shoulders they still looked joined at her
  head), where her chest stands farthest forward (`--wing-z` to set it). She was one rigid piece (a body bone, a head, wings):
  `rig_special.py --body fae --wing` now gives her thighs, shins, feet, upper arms, forearms and hands: legs hang soft
  with pointed toes and trail behind her in flight, both hands draw back and thrust at the target in her attack, arms
  fling out as she falls. Her upright body bone had no roll, so every lean (the lunge, the flight, the fall) tipped her
  sideways: it leans forward and back now. Her flame trousers were flat boards (16 x 11 cm, hip to ankle): deepened to
  round legs, and the concept's white background flecks on her legs and hands painted out
  (`WildreachArt/out/ignessa_v3/ig_fix.py`). Then her head "looked like a mess" (Raven): locks grew out of her crown
  and hung over her forehead, and the crown (a fan standing up from her hairline) was painted with hair streaks and
  white blobs. The crown is one fiery piece now, ember red to flame-yellow tips, and hair grows only behind it, at her
  temples and down her back. Then the crown was "just a flat piece on her head" (Raven): the image model stood it up as
  a flat fan in front of her head (2 cm thick). It is bent round her head into a circlet now, from her brow back to her
  temples, its outer spikes at her sides; the hair on top grows from just behind its ring. And its colours match her
  wings so it stands out of her red hair (Raven): the wing picture's orange at its foot through gold to pale cream at
  the tips, with dark vein lines up its lower half. Size unchanged (halfHeight 110: 2.2 m to the crown).
- **Grimjaw's holes closed**: the same boss build left seven holes (both shoulders, under the left arm, one at his
  hip strap); the floor showed through his shoulder in every step of his walk, and the see-through check found 110 to
  160 spots per clip. Each is filled with a patch and a back side (`ow_fix.py --around 3`: his holes' edges are darker
  than his skin, so the patch takes the skin a few edges out from the hole; with the edge's own colour they came out
  as dark blue blotches). The check finds nothing see-through on him now. The dark shape that crosses his back in his
  swipe is his own raised arm's shadow. Walk, run, swipes, jump attack and death play clean.
- **Grimjaw 30% bigger, and his back cleaned up** (Raven: "his rags/back area is weird"): halfHeight 208, radius 130
  (was 160, 100). The image model made his seat and the back of his fur loincloth one boxy block, its top edge a ledge
  jutting 25 cm past his lower back: it is drawn in to a rounded rump that tapers to his thighs, and its lower half
  partly follows his thighs so it swings with his legs. What looked like dark metal plates on his flanks and shoulder
  were faces left by the boss build's cut with their UVs stretched across a strip of the texture; every face stretched
  like that (245) takes its neighbour's colour. The pale, whitish tatters at the loincloth's hem take the fur's brown
  (the concept's fur is dark to its tips). `WildreachArt/mixamo/bosses_r2/grimjaw/gj_fix.py` on the hole-filled
  model; rig and clips unchanged, nothing see-through.
- **Grimjaw's fur is real fur, his skin one blue, the dark patch on his back mostly gone** (Raven: "the dark patch on
  his back looks off and his rags still don't look right"). His fur pieces (loincloth, forearm cuffs, shin wraps)
  were faceted brown lumps with fur painted on: 6000 brown locks grow on them now (`hair_locks.py --mixamo`, which
  exports the way the Mixamo bosses are built). His skin came in two blues, pale over his shoulders and upper back and
  dark below with a torn edge between (a ragged vest): both are moved to their middle, each keeping its own detail.
  The dark oval on his back while he punched or swiped was a shadow (it goes with shadows off) from a ridge his
  shoulder skin bunched into. The cause: the rig gave his HEAD bone the skin of his trapezius and upper back (up to 94%,
  64 cm out from his head), so every move of his head dragged his back and shoulders into creases and puckers. Outside
  his head's own outline that hold now goes to his shoulders, neck and upper spine, the shoulder weights are smoothed,
  and his upper back and shoulders are smoothed in shape and paint (Raven circled them: "can we smooth this section
  out"). The oval is gone and his back reads as smooth muscle. His hole patches are now domed on the skin round them
  (`ow_fix.py --dome`) instead of one flat face each, and the shadow painted in his armpits is gone.
- **Vulkarn and the Obsidian Warden checked for Grimjaw's faults** (Raven): neither rig ties back or shoulder skin to
  the head (Vulkarn's head bone holds no skin; the Warden's only its head). Both had faces with their UVs stretched
  across a strip of the texture: 127 on Vulkarn, 509 on the Warden (on its shoulder plates, chest and back), which
  showed as faint smears and amber flecks on its dark armour. Each takes its neighbour's texel
  (`WildreachArt/mixamo/bosses_r2/stretch_fix.py`); rig, shape and clips unchanged, both re-filmed clean.
- **The Pale Hunter: whiskers off, re-rigged, a real bite**: its whiskers were a spray of thin wire loops off its
  muzzle (grey plates either side of its mouth in the game): cut out and the roots closed
  (`WildreachArt/out/pale_hunter_v2/ph_fix.py`; small flat stubs are left at the mouth corners). It ran on its October
  rig, whose bite was a slight dip of the head and whose new walk folded the front paws up: re-rigged with the front
  joint high inside the body and the shoulders over the paws (`rig_quadruped.py --head +x --front-elbow 0.4
  --front-hip-x 0.32`), clips from `anim_creature.py --build light --attack bite` (a trot, a gallop, a head strike).
  `anim_creature.py` could not find a boss (the `_bosses` divider in creatures.json has no id): fixed. What looked like a
  see-through sheet under its belly is the pale blue paint of a fold of its skin, so the shape is kept. Size unchanged
  (halfHeight 90).
- **Leftovers tidied** (Raven): the Pale Hunter's flat whisker stubs at the mouth corners and the tufts over its eyes
  are smoothed back into its face (`pale_hunter_v2/ph_smooth.py`; cutting them harder opened dark holes); the notch at
  the back of Grimjaw's neck is smoothed with his back (his nape is in the smoothing, his beard and throat kept); the
  Obsidian Warden's orange cracks glow at night (`MI_Obsidian_wardenGlow`, fire glow 7: they were only painted); the
  stray lock that stood out at each of Ignessa's temples is gone. The Aurora fox's tail was already remade as a brush
  on Nov 3 and is left as it is.
- **The food chain at the new sizes** (Raven: keep the size rule; a hand-drawn web of who-eats-whom was too
  confusing): checked once every creature's size was set. Nothing hunts the two herd giants (Basaltback, Woolly
  drudge), every hunter has prey on its own side, and packs still take an Emberhorn. One pairing was wrong: the small
  Scorchwing could take a 2 m Ashstrider (exactly at its limit). A lone hunter's prey limit is now 1.25 times its own
  width (was 1.3; `creatureAI.preySizeRatio`), which removes that pairing and changes nothing else; packs keep 2.
- **Skaldrith, twice the size, on its feet on slopes, in frost colours** (Raven): halfHeight 360, radius 340 (its back
  about twice the survivor's height). Its body stayed level on a slope with its feet in the air or in the ground (the
  pack dragon's skeleton wasn't one the feet system knew): `creatures.json` can now name a pack skeleton's legs
  (`"legs"`), and Skaldrith's body tilts to the ground with each foot planted on it. Its faint blue tint showed as plain
  grey in the game sun: now ice blue with frost white on its lighter parts (`M_CreatureTint` takes a second, lighter
  tint). Scorchwing, the same pack dragon, names the same legs (Raven): its body tilts on slopes too.
- **Bosses can be filmed**: a boss's brain is a plain AI controller, so the film commands never reached it (Vulkarn stood
  still and "couldn't bite"). Bosses now run the test controls themselves (walk, turn, stand; no fighting meanwhile),
  and `WRCreatureAttack` swings a boss's next attack clip where it stands (no telegraph, no hit).
- **Glowmoth's wings beat up and down together** (they beat in opposition: a mirrored-bone sign).
- **Four rebuilt from new concepts** (Raven's picks): **Rimeclaw** (concept 3 with concept 0's curled tail; the face
  clear of the mane, on the new animation); **Ignessa** with her arms and legs apart (her body cut out of concept 3)
  and wings built as separate pieces from one wing picture (`rig_special.py --wing`), hinged behind the shoulders and
  moving only with the wing bones; **Vulkarn** (concepts 1 and 3: lava cracks, beard, hood) and **Grimjaw** (concept
  1, no horns, hands clear of the legs), both re-rigged on Mixamo with fresh Creature Pack clips.
- **Boss rigs at the right height**: the Mixamo copy was always packed as a 1.8 m figure while the bosses' full models
  are 2 m, so every joint sat 10% low (shoulders sunk into the chest, Raven's "mixed up near the shoulders").
  `pack_for_page.py` takes the model's height now.
- **Cinderfang rebuilt from concept 1** (Raven's pick: flame-lit face and horns, mane, raised tail). The old model came
  from a six-legged concept and had five paws, front legs patched from a leftover leg. Five shapes from different
  generator seeds, paws counted on each (all four clean); seed 42 kept for its horns and straight front legs. Torso
  stretched 1.2 (three-quarter concept), `meshScale` 1.08 so the raised tail doesn't shrink it (the game fits the whole
  height to the capsule). Then, on Raven's notes ("tail not attached to its rear correctly", "front legs still messed
  up", "his leg is disconnected"), `cf_fix.py` (in WildreachArt) on the rigged mesh: the tail had come out as a flat
  sheet down the outside of the left thigh, so it is cut off and a new one grown from the top of the rump (a tapering
  core under 130 flat locks like the mane's, arcing up and back to a curled tip); the right front leg reached forward
  and zig-zagged when stood up, so the body is made symmetric from its left side (upright leg, one connected surface;
  a mirrored copy had shown its seam at the shoulder) and both front legs lean in under the chest. Then ("its front
  legs have an extra joint"): the rig had folded them at 0.16, partway up the forearm above the real wrist, so it is
  re-rigged with `--front-wrist 0.11` (the top of the paw), and a knob halfway down the back of each leg is smoothed
  away; they stand and trot as straight columns, folding only at the paw. Then (Raven circled the paw hooking back
  in the swing: "have it be straight with the lower leg"): a long forearm on a short pastern can only shorten by
  folding the paw right back, so the front joint is now the elbow under the chest (`rig_quadruped.py --front-elbow`,
  apex back) and the forearm and paw swing forward as one straight piece, flat again to touch down.
- **Magmaw walks on two legs** (Raven's pick). Its model is built like a two-legged dinosaur (six new generator seeds
  from concept 2 came out the same): hind legs under the middle of the body, arms hanging just above the ground right in
  front of them, so four-legged walking crouched it (the old picture) and stretched-arm walking crowded the legs.
  `mg_fix.py --fold 55` (in WildreachArt) swings the arms up in front of the chest; rigged with `--legs 2 --hip-z 0.52`
  (its sail had read as the back), new planted-feet clips (it still had the first rig's), same texture and baked scales.
- **Scorchwing kept as it is** (Raven: small, and the pack's Mountain Dragon model, the Skaldrith boss's, tinted).
- **Basaltback cleaned up** (`bb_fix.py` in WildreachArt\out\basaltback_v2): its face (trunk, tusks) had come out turned
  0.1 to one side and is slid back onto the centre line; the side the concept doesn't show (a flat sheet for an ear, a
  loose flap on a front leg) is replaced by making the body symmetric from the good side; the head, painted pale
  blue-grey at 2.5x the legs' brightness, is recoloured to the legs' dark tone (the tusks warmed to ivory, the concept's
  glowing marks on the forehead kept, the shell untouched). Re-rigged with the front elbow, new heavy clips with a ram.
  Then **a giant walking tortoise** (Raven): 2x (halfHeight 70 -> 140, radius 140, reach 400: 280 cm to the shell top),
  legs 1.45x longer (`--legs-longer`), the head and ears moved forward as one piece on a longer neck (`bb_fix.py
  --neck 0.10`; the rig's `--head-forward` had dragged the shell's front along), the flaps hanging off the front legs'
  tops and the tusk that curved back toward the legs cut off. `rig_quadruped.py --belly-z` (that tusk had read as the
  belly). A 2x2 foundation platform on its back is parked: the 4 m grid needs an 8 x 8 m flat back.
  **Then rebuilt from a new concept** (Raven: "its ears now stretch back to its shell", "still looks like the ears are
  messed up": stretching that model into a neck came out boxy, its head was tucked under the shell). New concepts
  (`batch_r2_basalt.json`: an elephant-head set and a tortoise-head set); Raven picked elephant-head 1: an elephant on
  tall legs under a broad tortoise shell with three lava patches a side. Five generator seeds, seed 7 kept (four clean
  feet, roundest shell), the shell's top flattened a little (`flatten_top.py 0.4`), rigged with the front elbow, heavy
  clips with a ram; halfHeight 150, radius 150, reach 430 (300 cm tall). Sources in WildreachArt\out\basaltback_v3.
  **Building on its back** is to be its own feature later (Raven). Then ("make its tusks longer", "the front legs move
  weird just like Cinderfang's"): tusks 3x as long with the tips curving forward (`tusks.py`: the ivory texels, each tusk
  stretched along its own axis from the face), and the front joint moved up inside the body (`rig_quadruped.py
  --front-elbow 0.42`; at the belly it had bent the elephant legs backwards mid-leg): each front leg swings as one pillar.
- **Emberhorn cleaned up** (`eh_fix.py` in WildreachArt\out\emberhorn_v2): its left front leg had come out mid-step
  (lifted, bent back), so the body is made symmetric from the right side; the hide, painted mid-grey and rendering pale
  blue-grey, darkened to the concept's charcoal (orange markings and glowing horns kept); the front legs, slanting back
  from the shoulder to the hooves, sheared upright. Rigged with the front joint inside the chest (`--front-elbow 0.43`)
  and the shoulder set by hand (`rig_quadruped.py --front-hip-x 0.20`: fur hanging off the legs stopped the trace low and
  put the shoulders behind the hooves), so each front leg swings as one straight piece; `anim_creature.py
  --swing-foot level` keeps hooves hanging through the swing (following the leg they pointed straight ahead). It was
  still on its first rig's clips; new heavy clips with a charge.
- **Kilnmite repainted to its concept** (`km_fix.py` in WildreachArt\out\kilnmite_v2): the image model's paint had
  navy-purple legs and belly and a gold shell with baked highlights (it rendered as polished gold), and none of the
  concept's kiln cracks. Legs and belly are now a warm dark brown, the shell dark copper, and a crack network glows
  orange across the shell plates (the edges of 3D Voronoi cells at each texel's position, warped jagged and faded out in
  places, so cracks run on across texture seams and break into separate fissures); `MI_KilnmiteGlow` lights the cracks
  alone. Re-rigged on six legs with the current `rig_quadruped.py` (the old rig's weights were poor). `anim_creature.py`:
  six legs never gallop (they scurry in tripods at every speed; Kilnmite bounded like a dog) and step at most 1.2 leg
  lengths (long strides reached the front legs out like arms).
- **Ashstrider fixed to its concept** (`as_fix.py` in WildreachArt\out\ashstrider_v2): the image model had turned its
  head 28 degrees to its right (every walk carried it crooked), painted the face white with big black goggles round the
  eyes and a black ball of a nose (it read as a skull), and the body near-black between bright white lines. The head
  turns back straight about the top of the neck, the face is mid-grey with dark almond eyes and a dark grey nose, and the
  body's cells are mid-grey between pale lines like the concept's. Re-rigged with the current `rig_quadruped.py`
  (`--belly-z 0.44`: its legs stand mid-stride, so one crossed the centre line and read as the belly at 5% height;
  `--front-elbow 0.5`); new light clips (a trot, a gallop, a horn ram) with hooves hanging in the swing. Raven: "slightly
  bigger than Cinderfang", so 1.15x (halfHeight 85 -> 98, radius 45 -> 52): 196 cm to the horns, back at 145 cm against
  Cinderfang's 130, as long as Cinderfang with its tail.
- **Glowmoth fixed to its concept, and it glows** (`gm_fix.py` in WildreachArt\out\glowmoth_v2): the image model's
  antennae lay flat, straight out in front at mid-body height and as thick as fingers (from the side, two long sticks);
  they're thinner now and rise 35 degrees from the head. Its abdomen was striped black and yellow like a wasp's (the
  concept's is bronze plates over dark slate) and is now bronze and slate. The design's wings "glow soft blue": the
  dull blue eyespots are a clear cyan now and `MI_GlowmothGlow` lights them (a new MOTH glow in `creature_glow.py`,
  bluer and brighter than FROST asks so the pale blue fur stays dark), which shows best at night. The raised antennae
  are now its tallest point, so `meshScale` 0.35 -> 0.54 and `rig_special.py --hover` 2.5 -> 1.6 keep its 1.6 m
  wingspan and flying height. `creatureportraits.ps1` parks the hidden survivor past the 25 m nameplate range (a small
  creature's nameplate sat across it in the picture).
- **Pumicehog fixed to its concept** (`ph_fix.py` in WildreachArt\out\pumicehog_v2; the grey pumice rodent of
  `pumicehop_0.png`): the image model's head was turned 27 degrees to its right (from the three-quarter concept) and
  turns back straight, through a temporary armature heat-weighted to the mesh so the cheeks blend into the shoulders.
  Its front paws' claws touched in the middle (the rigger read them as one foot) and now stand a little apart, and a
  thin white spike above its right eye is gone. Its grey read white and its back yellow in the game's sun: the grey is
  darker and cooler with pumice pores, the back a deeper orange, and the design's "warm orange glow in the cracks" is
  cracks through the orange that `MI_PumicehogGlow` lights (FIRE 4). Re-rigged (`--front-elbow 0.3`: folding at a low
  wrist hooked the paws back) with new light clips; `anim_creature.py --gait hop` (new) makes it hop at a walk and bound
  at a run, as the design's hopping critter (its old clips hopped too).
- **Rimeclaw fixed to its concept** (`rc_fix.py` in WildreachArt\out\rimeclaw_v3; concept 3, the white ice lion): its
  whiskers were thin wire loops standing off the muzzle, which the game drew as flat grey plates either side of the mouth;
  they are cut out (thin both ways, at mouth height, so the mane's flakes stay) and their roots closed. The whole mane
  had come out blue: its front half is the concept's white ruff round the face now, with the blue kept along the crest,
  the back of the mane and the chest. Its dark slit eyes are the concept's glowing ice blue, the same size and set
  evenly (one had come out bigger and lower), with round pupils like a big cat's, lit by `MI_RimeclawGlow` through a
  new `EYES` glow kind in `creature_glow.py` (bluer than any fur, so the blue mane stays dark). Its dark navy lower legs
  (baked shading) are lifted toward the body's white. Re-rigged with an elbow under the chest (`--belly-z 0.40
  --front-elbow 0.36`: the rigger read the belly at 0.20) and new light clips. `rig_quadruped.py` now gives both hips of
  a pair the same place along the body (a leg modelled mid-stride put Rimeclaw's hind hips 14 cm apart), and
  `anim_creature.py --lift` (new) scales paw clearance in the swing: Rimeclaw walks at 0.6, since at the light build's
  full lift its forearms swung up level like a show horse's.
- **Rimeclaw's mane is hair now, and Rimeclaw is Emberhorn-sized** (Raven: the mane looked like part of the body, not
  hair; about as big as Emberhorn, a tad smaller). New `Tools/art/hair_locks.py` grows 2200 locks over the rigged mane
  (`rc_fix.py` now also saves the mane as a mask): tapered, flattened, combed out from the face and back, the crest
  back over the neck and the chest hair down, each drooping at the tip, each taking the colour (white ruff, blue crest,
  back and chest) and skin weights under its root, like the concept's. Locks rooted in the texture's dark creases are
  skipped. Rig and clips from WildreachArt\out\rimeclaw_v4. Size: halfHeight 60 -> 95 (190 cm to the top of the mane,
  about 2.5 m long; Emberhorn stands 210 cm and 2.7 m), radius 88, lunge reach 300; at that size its walk is a trot.
- **Glacimaw rebuilt behind the head** (Raven: the body should be longer, the spikes should look like spikes, the tail
  end looked like a blob). The old `--stretch 1.4` had stretched the whole model along its length, smearing the spikes
  into low fins, and the image model's tail curled under into a lump. `gl_build.py` (WildreachArt\out\glacimaw_v2)
  keeps the image model's head (jaws, teeth, horns; the yellow-brown teeth now ice white, rusty spots in the mouth
  gone) and grows a new body out of the neck's edge, after concept a: about 5.6 m of overlapping pale ice-blue plates
  (was 3.9 m), arching up behind the head, then lying along the ground and tapering to a point; a crystal spike on every
  plate down the back, raked back and tallest mid-body, and a fringe of smaller crystals along the lower flanks, all
  pale to white at the tips. The body is painted into the right half of a texture twice as wide. Glossy skin
  (`MI_GlacimawSkin`: `creature_glow.py` entries take a third, optional set of material scalars now).
  `rig_special.py` gives loose pieces automatic weights skip their nearest skin's weights. Then Raven doubled it:
  halfHeight 95 -> 198 (the crest stands about 4 m, its jaws about the survivor's height), about 11 m long, radius
  90 -> 180, spit range 380 -> 940 (its head sits about 5.6 m in front of its middle). A cave Glacimaw still fits the
  tunnels (5 m wide, 4.5 m high), but its tail reaches into the cave walls.
- **Glacimaw lies flat and moves like a snake** (Raven: no hunch at rest, slither like a snake, and bite by picking up
  its front half and chomping). `gl_build.py --flat` (output in WildreachArt\out\glacimaw_v3) lays the body straight
  along the ground from the neck, which eases down from the head's height; no arch. It also finds the lower jaw (the
  mouth is modelled a little open, the upper teeth hanging past the lower lip, so by distance over the surface from the
  chin versus the upper gums) and saves it for the rig. `rig_special.py --body worm --head-info` gives it a rigid head
  bone and a jaw bone hinged at the corner of the mouth, and new clips: it slithers in waves that run back down the body
  (the neck sways a third as much, the head keeps looking ahead), lies still at rest with a slow sway, a flicking tail
  tip and the jaw breathing, and bites by lifting its front half about 4 m up, jaw gaping, coiling back, then striking
  forward and down as the jaw snaps shut, with a shake, before lowering flat again; its spit uses the same strike.
  About 11 m long still (halfHeight 198 -> 120: lying flat it stands 2.4 m to its horns and spikes), bite range
  940 -> 640 (where its jaws land at the strike), walk and run paced to the waves (`pace` 315 / 480 cm/s), `strikeAt`
  0.53. New creatures.json `ownWindup`: an attack clip that rears on its own skips the telegraph's rear-back (tipping
  the whole worm back sank its tail into the ground).
- **Glacimaw lies along slopes** (Raven: tilt to follow slopes; on the ramp its 11 m body had stayed level, the tail
  floating and the head in the slope). Legless creatures with a spine chain (`rig_special.py` worms) now trace the ground
  under every joint, like the creature feet (same range, smoothing and fade), and each spine bone turns (up to
  `anim.creatureSpine.maxBend`, 35 degrees) so the body lies along the ground: up a slope, over a crest or bent at a
  slope's foot, whatever it is doing (a rearing bite rises from the ground under it). `creaturevideo.ps1 -RampCam 90`
  films the ramp climb side-on (default 110, from behind its right).
- **Glacimaw's body takes its head's colours** (Raven: the new body and the head didn't match or mix). The body had been a pale
  ice blue a quarter brighter than the head's steel blue, and smooth beside the head's mottling; in the game its gloss took
  the sky and it read nearly white. `gl_build.py` now paints the body from the head's own skin (its mean colour, with more
  colour: the head's grey horns and bare patches pull the mean grey), lighter on the back and darker underneath, with
  darker blotches across the plates like the head's, softer plate-edge highlights, and crystals that go from the body's
  blue at the root to a bluer pale ice at the tips (were near white). A softer sheen too (`MI_GlacimawSkin` roughness
  0.45 -> 0.6, specular 0.45 -> 0.35). The head piece's neck takes the same colours, so the join blends.
- **Big creatures' bites reach.** A creature's bite swept only the weapon reach (220 cm) from its middle, whatever its
  size, while the AI stops to bite at its `attackRange`: anything over about 2.6 m (Glacimaw, Basaltback, Emberhorn,
  Magmaw, Cinderfang, Rimeclaw) bit at the air. The sweep now reaches the creature's `attackRange` when that's longer.
  A spit leaves from the creature's jaw (or head) bone when its art has one, not the front of its capsule (a long
  Glacimaw spat from the middle of its body).
- **Sizes against the survivor (180 cm)**, Raven's picks from side-by-side shots: **Magmaw doubled** (halfHeight 55 ->
  110, radius 90, bite reach 340: 220 cm to the sail tip, head level with the survivor's) and **Cinderfang 1.75x**,
  horse-sized (halfHeight 45 -> 79, radius 66, reach 320: head at the survivor's shoulder, about 1.9 m long), and
  **Emberhorn 1.75x**, a mount bigger than its rider (halfHeight and radius 60 -> 105, charge reach 240 -> 360: 210 cm to
  the hump), all re-animated at the new size (slower, longer strides; at the same 480 cm/s an Emberhorn's run is now
  a heavy amble, not a gallop). `anim_creature.py` now counts `meshScale` in a creature's size
  (Cinderfang's strides had been planned 8% short). Between those and the giant Basaltback, Cinderfang packs and Magmaw
  counted as big enough to hunt a Basaltback (prey up to 2x / 1.3x the hunter's radius) and `Wildreach.Creatures.Behaviour`
  "nothing hunts a Basaltback" failed; at radius 150 it is out of reach of both again. Raven will settle the food chain
  once every creature's size is set.
- **A stomp's ring grows with the body** (`creatureAI.stomp.bodyRadius` 90: the 320 ring is a Woolly drudge's, a bigger
  body stomps one as much wider; the giant Basaltback's is 5.3 m). At 3.2 m the Basaltback bit from further out than
  its stomp reached and never stomped (`Tools\aitest.ps1` "a Basaltback stomps" failed).
- **Rig fixes** (`rig_quadruped.py`): a hip traced across the middle takes its partner's, mirrored (Cinderfang's left
  front shoulder had landed on the right of its chest); the belly is measured on the centre line (front legs standing
  close together had read as belly and dropped the hips); automatic weights go on the biggest piece alone and loose
  pieces (fur strands, a separate tail) take their two nearest bones (130 strands had made the weights fail outright,
  leaving the whole body on rough envelopes); a shoulder or hip carries on along the slant of the leg's middle, not
  its top few points (they swing toward the chest the leg merges into: Cinderfang's shoulders had landed in front of
  its legs, tilting them forward when standing); `--front-wrist` sets the front fold's height; `--front-elbow` makes
  it an elbow; `--hip-z` sets the hip height (a sail or crest reads as the back). `anim_creature.py`: a front joint
  above half the shoulder's height is an elbow (bends back, no paw flick, the paw keeps its angle to the forearm through
  the swing); two-legged creatures work (body measured to the chest, legs step in turn at every speed, the body comes
  down level where a foot can't reach and the bite lunges half as far: Magmaw's first bite reared it upright). The
  front-leg middle-slant hip rule applies to front legs only (Magmaw's zig-zag hind legs put their hips in front).
- **Art camera** frames the creature's own model where its animation has it (it had been framing the empty character
  mesh, so a hovering moth sat off the top of the picture); `WRTuneSet <path> <value>` sets a tuning number locally.
  `Tools\creatureportraits.ps1 [ids]`: one picture per creature, front over side, on the 1 m checker stage. Bosses get
  a wider frame and the hidden survivor parks outside their aggro radius (they had been fighting it mid-shot).

## 0.6.0: creature feet on uneven ground, frame-exact creature videos

- **Feet on slopes and bumps** for the generated creatures (`Tools/art/rig_quadruped.py` skeletons: `<leg>_upper /
  _lower / _foot`, 4 or 6 legs; pack creatures and the Mixamo bosses are left alone). Near a local view the game thread
  traces the ground under every foot each frame (`AWRCreature::TickCreatureFeet`, eased); the animation proxy
  (`FWRAnimProxy::PlaceCreatureFeet`) tilts the body to the plane through those points (pitch front to back, roll side to
  side, clamped 30 / 15 degrees) about a point over the feet at hip height, sits it at the ground's height and sinks it
  so the lowest foot reaches, then two-bone IK puts each foot at the clip's own position plus its ground height (a
  stepping foot keeps its lift; the knee bends as the clip bends it; the foot lies to its ground). Off in the air, in the
  water and out of range (fades); once dead the legs let go and the body stays lying to the slope. The survivor's
  standing foot plant now shares the leg solver (`WRAnimDetail::SolveLeg`), unchanged. Numbers: tuning
  `anim.creatureFeet`; `wr.CreatureFeetLog 1` logs each ankle's height over its ground.
- **Packs out of step**: every creature starts its idle and stride at its own point (seeded by its spawn spot, so the
  same spawn replays the same; `anim.creatureRandomPhase`). Before this they all started at 0.
- **Creature videos** (`Tools\creaturevideo.ps1 -Creature <id>`): a standalone game on the new `L_CreatureStage`
  (`Tools\make_map.ps1 stage`: a 1 m checker floor beside a 20 degree ramp) with a fixed time step
  (`-UseFixedTimeStep -FPS=30 -FixedSeed`) records a scripted performance frame by frame: idle, walk, run, two bites,
  a turn, a walk up the ramp, two flinches, death (25.2 s, 756 frames), identical for every creature and run, plus a
  contact sheet and the world time of every frame. New dev commands: `WRRecord <prefix> <frames> [w h]` (a screenshot of
  every frame), `WRCreatureDrive / Face / Attack / Flinch / Free <id>` (a creature does as it's told, its brain off),
  `WRCreatureHP <id> 0` kills, `WRHideMe 1`; the art camera keeps out of the ground.

## 2026-10-28: version 0.5.0, the feedback pass

Raven played 0.4.1 and sent a list. Five agents took it in their own worktrees (world, water, gather, combat, ui) and
the lead merged them; Raven approved the defaults below.

- **Lava** (world). The "lava squares" were scattered 6 m cells drawn as flat quads (edges floating on slopes) in a
  glossy emissive the day exposure washed to pale pink. `Tools/terrain/dressing.mjs` now traces the river's real channel
  into a 2 m grid (`hazards.lavaGrid`: ground height and depth inside the edge), and `AWRDressing::BuildLavaFlow` lays a
  sheet on the landscape's own vertices whose rim sinks under the ground (38 chunks). `M_Lava` is rebuilt: dark crust,
  glowing cracks, a molten middle drifting downstream (`lava.*`, live with `wr.LavaParam`). A spot burns only where the
  drawn surface stands over the ground (`FWRBiomeHazards::FLavaGrid::SurfaceAt`); trees and rocks stay out of it.
- **The green and yellow cones** (world) were the 188 placeholder resource nodes. `WRResourceLook.cpp` gives each item a
  look from packs already in use: charred trees and pines for wood, boulders, ore boulders with coloured lumps, a sulfur
  crystal cluster, lichen pads, berry bushes, fibre plants; the node's own mesh is a hidden collider (`world.nodeLooks 0`
  shows the cones).
- **Caves you can walk into** (world). The landscape filled the tunnels (each cave sat at the lowest ground under it, so
  the floor rose 2-3 m up the tunnel). The meshes were measured in Blender (`Tools/art/cave_measure.py` ->
  `Tools/terrain/cave_hollows.json`) and `dressing.mjs` fits each cave's height, variant, turn and size for at least
  3.6 m of headroom from outside to the back of the chamber; chambers are lit brighter and their ore can be swung at on
  uneven floors. `Tools/terrain/cavetest.ps1`: a bot walks into all six and mines.
- **Denser island** (world): shrubs 8,383 -> 24,435 (clumped, 1.7x bigger; the fibre and berry shrubs with them);
  wildlife 81 sites / 169 creatures -> 156 / 328 (a minute's walk now passes a pack 67-81% of the time).
- **Water** (water). There was no water at all: players walked on the sea floor, and the old MI_Sea plane's collision
  (a 1.2 km square under the island, where fallers landed) is now off. `UWRWaterSubsystem` (`WRWater.cpp`) builds a water
  volume from `Data/sea_mask.json` on the server and every client, at the drawn waterline (80 cm over the mesh,
  `water.surfaceOffset`): swim at 300, tread water with your head out, walk back up the beach. E drinks in the water or
  at its edge (20 Water a gulp; sea water counts, `water.seaDrinkable`). Creatures wade to belly depth and turn back;
  the fall-through rescue now catches swimmers under the island and holds deep fallers until the ground loads.
  Footsteps and the surf start at the drawn waterline (`audio.seaLevel` 60). `Tools/watertest.ps1`.
- **Tools are held** (gather). The hatchet or pick on the hotbar goes in the weapon hand (a model built from engine
  shapes, `WRHeldTool.cpp`) and decides the yield: hatchet on trees, pick on stone and ore, anything else the base rate.
  Every gathering hit costs the held item 1 durability (crude tools 80, stone 160; shown like the sword's); at 0 a tool
  gives no bonus and hits for half until repaired at a workbench (30% of its recipe). As a weapon: 7 / 9 light damage.
- **Bodies to harvest** (gather). A wild creature's kill stays down 180 s (`WRCarcass.cpp`) and nothing goes straight
  into the bag: each hit cuts a share of its loot (a blade keeps all of it, hands or a pick half); the killer and their
  clan have it alone for 30 s. Bosses, tames and wisps die as before. `Tools/gathertest.ps1`.
- **Attack out of a held block** (combat): light or heavy swings at once from the guard, which comes back when the
  swing ends if block is still held (parry still needs a fresh press). Casting while blocking now drops the guard too.
- **Creature wind-ups** (combat): every attack plants, rears back (bite up, stomp on hind legs, charge head down, spit
  head back, lunge crouched), glows red-orange (`/Game/FX/M_WindupGlow`) and makes a deep whoomph (`creature.windup`)
  before it lands; lengths and looks in `telegraph` (bite 0.5 s up to stomp 1.0 s). `Tools/combattest.ps1`.
- **Starter spells** (combat): each path names one (`"starter"`: Fire bolt, Frost shard), granted free at level 1 (and
  when a subclass is taken up), put on wheel 1 so Q casts it; saves without it get it on login, and characters who had
  bought it get the 3 points back.
- **The panel flashing over the map** (ui) was the marker details rebuilding every 0.5 s inside Tick, so new widgets
  drew one frame unmeasured; `WRUI::MeasureNow` / `SetContentMeasured` fix it there and in the other screens that
  rebuild (journal tabs, inventory, paths, skills, storage, crafting, build bar). `Tools/uiflickertest.ps1`. Clicking
  the journal tab that's open no longer closes it.
- **Demolish mode** (ui): a Demolish button in the build bar and a rebindable key (X); the piece under the crosshair
  glows red with its refund ("Demolish Wood wall · 6 wood back"), grey and "Not yours" when it isn't; X, Esc or right
  mouse leave it. `Tools/demolishtest.ps1`.
- **Console** (ui): README.txt now explains the ` key and a few commands (playtest builds: anyone in the game can use
  them).
- **GPU hangs fixed.** The agents' runs hit `DXGI_ERROR_DEVICE_HUNG` now and then, and the merged build hung in 2 of 3
  Epic benchmark runs on a quiet machine, always in the virtual shadow maps' Nanite `PatchSplit` pass: Nanite's runtime
  tessellation, on by default in 5.8. `r.Nanite.Tessellation=0` (DefaultEngine.ini `[SystemSettings]`): 4 of 4 runs
  clean, 8.34 ms (120 fps) on Epic. 0.4.x most likely carried the same risk.
- **A server crash in melee fixed.** The suite's mp run crashed the server in `UWRGameplayAbility::After` called from a
  swing's wind-up step, just after a parry: the step set its re-sweep timers (and the hit could start more), a new timer
  grew the timer manager's storage, and the running closure, `this` included, moved under it. Latent since the combat
  core (2026-10-22); more creatures made it likelier. `After` now keeps each step on the heap and runs it from a local
  reference.
- Tests: the suite gains `combat`, `water`, `gather`, `demolish`, `flicker` and `cave`; several bot tests take `-Port`.
  On the merged build 26 of 27 passed (25 automation tests, 335 checks over 26 bot tests); `mp` was the server crash above, and on the
  fix it passed 3 runs of 9/9, with the ten combat-related suites rerun and passing.
- `ProjectVersion=0.5.0`; packaged and smoke-tested by real input (the starter spell on wheel 1, crafting by click,
  demolish mode on B then X). **Released 2026-10-28 as
  [v0.5.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.5.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB,
  notes `Docs/release-notes/v0.5.0.md`): installed copies update themselves on their next start.
- Known: the volcanic shrubs are still yellow-green (their leaves only recolour through a coat; worth retrying now the
  hangs are understood); ice still uses the old 6 m sheet.

## 2026-10-27: version 0.4.1, menus take clicks again, cave burrowers stay put

Raven's friend reported they couldn't move around the menus, change settings or craft, though they could gather. A new
test plays the menus with a mouse and keyboard the way players do, and it found that in 0.4.0 no menu took a single
click, for the host or anyone who joined.

- **Menus take clicks again.** The polish pass's loading and death cover (`SWRFlowCover`, drawn over the menus) and the
  banner and hint layer (`SWRFlowMoments`) were full-screen widgets left at the default visibility, so even when empty
  they caught every click meant for a menu: tabs, recipes, the Craft button, every setting. Keys still worked (I, J, Esc,
  B open and close them), which is why gathering and opening menus looked fine. Both now let clicks through
  (`SelfHitTestInvisible`); their parts already did.
- **A menu left open under the chat box or the main menu keeps its cursor.** Closing either set game-only input, so a
  menu still open had no cursor and the camera swung with the mouse until it was closed and opened again
  (`FWRPlayerUI::ReapplyInputMode`).
- **Cave burrowers stay put.** A Glacimaw or Kilnmite waiting underground has no collision, so it fell out of the world
  (cave colonies have been empty since they were added); since 0.4.0 the fall-through rescue caught it every second and
  logged it, about 280 times a session. A buried burrower now holds still (`DisableMovement`) and comes out walking, and
  the rescue leaves characters without collision alone.
- `Tools\menutest.ps1` (in `alltests.ps1` as `menus`): a host and a friend who joins from the main menu each open the
  journal with I, click through its tabs, craft a crude hatchet by clicking the recipe and its button, open Settings with
  Esc, flip Vertical sync off and back, click Resume, and open and close the build bar (12 checks). The clicks go through
  Slate as real ones do (`WRClick`, `WRKey`, `WRType`, `WRUIText`, `WRUIState` in `WRUIDrive.cpp`), unlike `WRMenu` and
  `WRSettings`, which call the menus' code directly: that is how a layer eating every click went unnoticed. Bot command
  lists can mark steps `solo:` (on your own, before joining) or `net:` (once in a game), since the list starts over in
  each world.
- Not reproduced: what blocked the friend before 0.4.0, which had neither layer (closing chat over an open menu, fixed
  above, is the one way found). If it happens again, it helps to know whether they host or join, whether a cursor shows
  in the menu, and whether they play with a controller.
- The full suite passes: 21 of 21, 22 automation tests and 246 checks over 20 bot tests. `ProjectVersion=0.4.1`;
  packaged (`C:\Dev\WildreachBuild2`) and smoke-tested by clicking (the packaged game opens Crafting, crafts a crude
  hatchet, opens Settings > Video and resumes). **Released 2026-10-27 as
  [v0.4.1](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.4.1)** (Latest, `Wildreach-Setup.exe` 1.7 GB,
  notes `Docs/release-notes/v0.4.1.md`): installed 0.4.0 and 0.3.0 copies update themselves on their next start.

## 2026-10-27: version 0.4.0, the polish pass

Raven asked for the playtest to feel and look like a real game by polishing what's there, with no new content (plan:
https://claude.ai/artifact/LuauEtwseW8GwRVXie1iM4). Six agents worked side by side in their own git worktrees (animation,
flow and UI, sound, effects and combat feel, world look, performance), on two shared event interfaces that landed
first: `WRSound` and `WRFx` name the moment ("hit.flesh", "gather.wood", "levelup") and data decides what plays.

- **Animation** (`UWRAnimInstance`, C++, no animation blueprint). The player and every creature played one clip at a
  time, so blocking, attacking or casting while moving slid. Now the legs run locomotion (idle / walk / run blended by
  speed and by direction: forward, back, strafes, diagonals) while an upper-body layer from the spine carries block,
  attacks, casts, aiming, reloads and hit reactions; dodge, death, falls, swimming and gathering stay full body. Every
  change crossfades (about 0.2 s), standing turns step the feet round past 70 degrees, feet plant on slopes (two-bone
  IK). Clip paces are measured from the clips' own feet, so steps match the ground speed. Blocking now turns off
  orient-to-movement, as aiming does (the facing used to wobble). 22 new clips (3.7 MB): ranged strafes and back-walks
  built from packs already in the project; the sword-and-shield strafes were already imported.
  - Creatures crossfade, play their loops at their real ground speed, turn at 160 / 260 degrees a second (was 400)
    and arc instead of spinning, bite from the neck up while running, and time each attack's strike to the damage;
    lunge, charge, stomp and spit each move at their own pace. Every creature from `rig_quadruped.py` had its walk and
    run playing backward (moonwalking); the game now plays those loops reversed (the rig script is the real fix).
  - Footsteps fire at each foot plant.
- **Sound** (`WRSound`, `Data/Audio/sounds.json`, `UWRAudioDirector`). The game had none. 118 events from 352 OGG
  files (18 MB, loaded at runtime from `Data/Audio` and decoded with the engine's Vorbis decoder): footsteps by surface,
  swings, hits, blocks, parries, voices for both bodies, creature calls by species and family, gathering, building,
  doors, crafting, spells, ranged weapons, UI, and the moments. Ambient beds crossfade by side of the ridge, time of day,
  coast, caves, lava and altitude; music plays the menu theme, exploring and night tunes now and then, combat while
  something hunts you and the boss track in a boss fight, ducking under big moments. Settings has an Audio page
  (master, music, effects, ambience, interface). All sources are CC0 except two CC-BY 3.0 ones, credited in
  `Docs/CREDITS.md` and the game folder's `CREDITS.md`: "Lava loop" (Blender Foundation) and "Crossbow sounds"
  (HolgiB).
- **Effects and combat feel** (`WRFx`, `Data/fx.json`, a small C++ sprite particle system: Niagara can't be authored
  headless here). Fire and frost bolts with cores, trails and light; Fire wall, Lava pool, Frost nova, Ice lance and the
  rest of the spells; impacts (blood, dust, ice or magic by creature, sparks on blocks); hit-stop (60 ms, 90 on
  parries, animation only), camera shake, a parry flare and screen-edge flash, a crosshair hit marker; damage numbers
  moved to the Slate HUD (gold Cinzel crits, "weak" on boss weaknesses, red for damage you take); wood chips, stone
  dust and plant bits when gathering; dust when building, debris when pieces break; flames on Hearths, torches and
  braziers; embers over lava; snow on the tundra and ash on the volcanic side. Server-only moments reach clients through
  a reliable relay (`AWRFxRelay`).
- **World**: the flat sea is moving water (waves, turquoise shallows, deep water, shore foam; dark at night), meshed
  over the open sea only (`Data/sea_mask.json`). The volcanic side gets a warm ash haze and the tundra a cold blue,
  blended by where the camera is; light shafts at dawn and dusk; mist in 45 valleys; a lifted grade so nights read.
  Build pieces get real shape from code (`WRBuildLook`): plank walls with posts, coursed stone, floorboards and flags,
  shingles and slates on rafters, cut gables, framed doors and windows, palisade points; collision and placement are
  unchanged. The workbench, forge, armorer, alchemy table, Hearth, claim totem and storage chest are modelled props from
  the local AI pipeline (Hunyuan3D 2.0: not licensed in the EU, UK or South Korea). The camera eases back out after a
  wall, widens slightly when sprinting and eases into lock-on.
- **Flow and UI**: dying fades to a carved "You died" card naming what killed you, where your belongings wait and for
  how long, and where you'll wake, then holds "Loading the island…" until the ground around you has streamed in; a
  compass marker leads back to your bag. A loading screen with tips shows when you host, join or travel. Screens fade
  and rise in, buttons and slots warm and dip, pickups stack ("+40 Wood"), notices queue. Banners for level-ups, new
  recipes, first visits to camps, caves, passes and boss lairs, boss title cards and tames. Six one-time hints with the
  real keys ("Show hints" on the Video page). A fixed scenic view behind the main menu.
- **Performance** (`Tools/benchmark.ps1`, results in `Docs/PERFORMANCE.md`): creatures more than 200 m from every
  player think in quarter-second slices (`UWRSignificanceSubsystem`): with all 169 out, server work per frame went from
  14.6 to 6.8 ms. The creature, body and item art preloads in the background at start (`UWRPreloadSubsystem`), which
  removed the two hitches on the walk route (frames over 50 ms: 2-6 per run before, none after). The whole merged pass
  on this PC (RTX 4070 Ti, 1080p): Epic 8.25 ms a frame (121 fps), Medium 5.66 ms (177 fps), p99 9.9 / 7.0 ms, no frame
  over 50 ms; the server 2.0 ms a frame with the usual wildlife and 4.2 ms with all 169 creatures out.
- Fixes found on the way:
  - **Beds never set where you respawn**: the engine reused the controller's first start spot. They work now (and you
    wake standing on the bed, not inside the floor).
  - `M_Rock` lacked the Nanite usage flag, so cooked builds drew the cave boulders in the default material.
  - The sound table had no `hit.dust` or `stomp`, which the effects call.
  - Boss telegraphs and hazard markers were the engine's lit shape material tinted orange, which strong sun blew out
    to flat white or yellow patches; they're now a glowing, see-through, pulsing material (`M_Telegraph`,
    `Tools/fx/make_telegraph_material.py`).
  - New players spawn at 08:00, the peak of the dawn valley mist, so their first look at the island was washed out;
    the dawn mist is about half as strong and clears sooner.
- The full suite passes on the merged build: 22 automation tests and 234 checks over 19 bot tests (`flow` is new);
  `uishots.ps1` captures all 39 screens with no client errors.
- The open items from the first 0.4.0 build, fixed before release:
  - **Falling through the island.** A character that arrived before the ground's collision was there (after a
    teleport, a respawn far away, or a pack spawning) dropped through the terrain onto the map's sea plane 140 m below,
    where the island can't be seen from underneath: the "flat blue ground" in screenshots since 0.3.0. The server now
    checks once a second and puts anyone under the ground back on top (`AWRGameMode::RescueUnderground`; a probe,
    `WRProbe`, logs what's under you). In one test run it caught the player and two creatures.
  - **Distant terrain is drawn.** The ground streamed in only 256 m around you, so from any hill the distance was a
    flat pale void with scalloped edges. Each player now streams four times as far (`world.streamingRangeScale`), and
    trees, rocks and cliffs draw 2.5 times further (`world.decorationDistanceScale`). The benchmark barely moved (Epic
    8.21 ms, Medium 5.74 ms; about 150 MB more memory; streaming after a long jump done in 0.6 s instead of 0.2 s).
  - **Your first spawn follows your path.** The engine chose the start spot before your path was set, so a frost
    player could start (and keep respawning) at a volcanic camp; it's chosen again once your path and save are loaded.
  - **No more washed-out boss arena.** The daytime exposure floor was written in the wrong units (an "EV" of -10 in a
    setting this project reads as luminance), so by day the camera had no floor and brightened dark ground until
    everything on it blew out. It's converted properly now: by day the camera brightens only to an ordinary scene's
    level, under cover (caves, roofs, canopy) it may go further, and nights are unchanged. Daytime valley mist is
    lighter too, so it doesn't read as white blobs from high ground.
- Still known: the volcanic shrubs are bright green; creature feet still slide about half their speed (the procedural
  rigs never fully plant a foot); running strafes show only while aiming or in first person, because third person faces
  where you run.
- `ProjectVersion=0.4.0`. Packaged (`C:\Dev\WildreachBuild2`) and smoke-tested (118 sound events loaded, exposure floor
  -2.0 by day). **Released 2026-10-27 as
  [v0.4.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.4.0)** (Latest, `Wildreach-Setup.exe` 1.7 GB):
  installed 0.3.0 copies update themselves on their next start.

## 2026-10-26: version 0.3.0, the stone-and-leather UI

The whole UI is rebuilt from the mockups Raven approved (dark stone and leather, Cinzel and Alegreya Sans), in Slate
C++ owned per local player by `FWRPlayerUI`. Screens register themselves by name and follow the controller's menu
state, so gameplay still goes through the same server commands (and the bots still drive every menu). Fonts, textures,
item icons, creature portraits and line glyphs load at runtime from `Data/UI` (no editor import). `-oldui` keeps
the old text HUD.

- **Every key is rebindable** (Esc → Controls; saved in `GameUserSettings.ini` `[Wildreach.Keys]`, only what differs
  from the defaults). Click a key and press the new one (keyboard, mouse buttons, wheel); a key already in use asks
  "Replace it?" and the action that loses it shows "Not set". Right-click clears, Reset to defaults. Defaults no longer
  clash: dodge C, crafting J. H shows the current keys in game.
- **Item hotbar (1–8)**: each key holds an item from your bag (drag items onto it in the inventory, right-click or drag
  off to clear; new tools, weapons, food and potions take a free key unless you took them off). Pressing a key equips or
  uses it; the mouse wheel steps along it (it still cycles ammo while aiming). Q/Z/X no longer take a potion, eat or
  drink: the hotbar does that.
- **Ability wheels**: hold Q (or Z for the second wheel) to open a wheel of 8 abilities, pick with the mouse, release to
  keep it; a tap casts the picked one. Or give each ability its own key (cast mode Wheel / Own keys / Both, and
  cast-on-release, in Controls). Fill the wheels from the Paths tab (drag a learned ability onto a slot).
- **Tame orders**: hold E on your tame for an order wheel (follow, stay, ride, release, passive, aggressive); a tap
  opens it to click an order.
- **HUD**: level and path XP, compass with points of interest and the nearest camp or boss, boss bar, clock, vitals with
  food/water/Heat rings, hotbar, ability plate with cooldowns, tame card, taming card, pickups, chat, interaction prompt.
- **The journal** (I / J / P / K / M):
  - Inventory: equipment with a body figure, the 8×5 bag with drag and drop (move, equip, unequip, swap), filters, sort,
    item details (stats, durability, sockets, where it's repaired/made/used), split and drop with a "how many?" picker,
    carry weight, the hotbar strip.
  - Crafting: the station you're at (or by hand) and the others' distance, categories, search, "ready to craft",
    ingredients have/need, quality odds, infusion (melee gear and armor), quantity, repair and upgrade.
  - Paths: the fire/frost tree with learned / available / locked states, node details with real numbers, Learn, and the
    two-wheel loadout.
  - Skills: life-skill cards with level and XP, perk tiers and choices.
  - Map: the island with camps, caves, bosses, passes, your Hearth, claims and tames; zoom, pan, legend toggles, marker
    details (boss loot).
- **Build bar** (B): categories and piece cards with costs, a placing chip with the reason a piece won't go, keys, claim
  status, demolish. **Snap (F) now works**: with it off, pieces go at the aim point (45° turns), checked box against box
  so they can't overlap; client and server check the same spot.
- **Storage** (a chest beside your bag: drag, shift-click a stack, ctrl-click one, "how many?", Store all resources,
  Take all), **Hearth** (tech levels, boss cores, what each level unlocks, stations in the claim, feed a core),
  **Claim** (top-down map of the claim, upkeep per material with time left, stock, radius upgrade), **Taming** (a side
  panel: torpor, taming, effectiveness, foods from your bag to click).
- **Game menu** (Esc): Resume, Controls, Video (window mode, resolution, 3D resolution, frame cap, vsync, quality
  levels, auto-detect), main menu, quit. **Main menu** restyled, with a Settings button and tips that use your keys.
- Gameplay that came with it: the bag has 40 slots; carrying over 80 kg slows you to 70% (`inventory` in
  `tuning.json`); new server commands `move`, `split`, `drop` (a loot bag anyone can open, 10 min), `store`/`take` with
  counts, `storeall`, `takeall`, `hearthfeed`; store/take use the nearest chest (inside a house they found the floor).
- Fixes found on the way:
  - **Claim upkeep cost ten times its rate**: the totem paid each hourly rate rounded up *every minute* (6 wood/h burned
    60). It now pays as it falls due (`FWRBuildRules::PayUpkeepMinute`, tested in `Wildreach.Building.Rules`).
  - **A client crash coming back to a base**: the build pieces' cube mesh and wood/stone materials were held in static
    raw pointers, so after every piece left your client's range they were garbage-collected and the next piece crashed.
  - With Crafting open, Enter also crafted the station's first recipe (the hidden text menu); infusing a bow used up the
    essence for nothing (ranged hits are physical), so the server refuses it.
- Tools: `Tools\uishots.ps1` shoots every new screen (`-MainMenu`: the front end); it and `alltests.ps1` set
  `UE_SKIP_UBT_SDK_SETUP=1`, because every Unreal start otherwise waits on UnrealBuildTool's platform check while any
  build runs. `node Tools/ui_glyphs.mjs` redraws the line glyphs. `menushots.ps1` keeps the old menus (`-oldui`).
- What the mockups showed that the game can't do yet (built the game's way instead): Hearth repair, taking stock back
  from a claim, named chests, map waypoints, audio and mouse settings, the taming stash's contents. Your Hearth and
  claims show on the map only once seen (their locations don't replicate from further than ~150 m).
- Built by five agents in parallel git worktrees (one per screen group) on the shared foundation, then merged. The
  full suite passes on the merged build: 15 automation tests and 222 checks over 18 bot tests; `uishots.ps1` captures
  all 25 in-game screens and the front end with no client errors.
- `ProjectVersion=0.3.0`. Packaged (`C:\Dev\WildreachBuild2`) and smoke-tested: the packaged game loads the UI from its
  `Data/UI` folder (fonts, icons, glyphs) and the menu shows 0.3.0. **Released 2026-10-26 as
  [v0.3.0](https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.3.0)** (Latest, `Wildreach-Setup.exe` 1.6 GB):
  installed 0.2.0 copies update themselves on their next start.

## 2026-10-25: version 0.2.0, the game updates itself

- **Auto-update.** The Wildreach shortcuts now start `WildreachUpdater.exe` (beside `Wildreach.exe`): it asks GitHub
  for the latest release (github.com/.../releases/latest redirects to the tag, so no API limit), and when that's newer
  than `version.txt` it shows a progress window, downloads the release's `Wildreach-Setup.exe`, installs it over the
  folder (saves kept; the running updater renames itself aside to be replaced) and starts the game. Up to date, or no
  network within 5 seconds: it starts the game straight away. "Play without updating" skips it once.
- **The game has a version** (`ProjectVersion=0.2.0` in `Config/DefaultGame.ini`). The main menu shows it, checks
  GitHub too, and offers **Update now** (closes the game, updates, starts it again) when a newer one is out. It's part
  of the network version, so a client only joins a host on the same build; joining a host on a different one says so
  in the menu ("everyone needs the same one") instead of failing mysteriously. Other lost connections are explained
  there too.
- **To release:** bump `ProjectVersion`, `Tools\package.ps1`, `Tools\make_installer.ps1` (writes `version.txt`, builds
  the updater into the game folder), then publish a GitHub release tagged `v<version>` with `Wildreach-Setup.exe`
  attached. Every installed copy picks it up on its next start. Copies installed from the first playtest build
  (before the updater) need this one installed by hand once.
- `Tools\updatertest.ps1`: the update path end to end, offline (two small fake releases built with the real
  installer script, a local stand-in for GitHub): finds and installs the newer one, replaces itself while running,
  starts the game with the shortcut's arguments, then up to date / a non-version tag / no network all start the game.
- `Tools\versiontest.ps1`: a 0.1.9 server turns a 0.2.0 client away, the client's menu says why and (pointed at a
  local feed) offers the newer release; a client on the server's version joins. Automation test `World.Version`
  (which tags count as newer). Both scripts are in `Tools\alltests.ps1`.
- The full suite passes with the wildlife and the updater in: 15 automation tests and 222 checks over 18 bot tests
  (`Tools\alltests.ps1`, about 45 minutes).

## 2026-10-25: the island is full of wildlife

- **Every wild species lives on the island now**, 81 packs and 169 creatures (up from 6 packs plus the cave colonies):
  critters and herds in the lowlands around the camps and along the coasts, predators further out toward the ridge,
  Scorchwings and Magmaws by the volcanic ridge, Rimeclaws and Glacimaws in the tundra highlands, Kilnmite and
  Rimeshell colonies with their egg nests out in the open. Nothing aggressive within 280 m of a player camp, nothing
  in a boss arena, by a mini-boss or at a cave mouth.
- The sites come from **`Data/island_spawns.json`**, generated by `node Tools/terrain/spawns.mjs` from the heightmap
  and the layout (biome by side of the ridge, elevation band by distance from it, flat ground, clear of lava, ice and
  the packs already in the map). The server reads it at start, so the numbers can be changed without rebuilding the
  map.
- **Packs come out near players**: a site spawns its pack when someone comes within 250 m and fades it out 90 s after
  everyone has gone (300 m), so 169 creatures cost only what's around the players. Tamed and knocked-out creatures
  stay; a pack you thinned stays thinned until its respawn time. Tuning: `wildlife` in `tuning.json`
  (`activationRadius`, `leaveRadiusMult`, `despawnSeconds`, `enabled`). Creatures and nests are set on the ground.
- `WRWildlife` (console): sites, how many are out, what lives within 300 m, the nearest quiet site.
- Tests: `World.Wildlife` (every wild species has sites, each in its own biome, on the island, clear of the camps'
  safe zones) and `Tools\wildlifetest.ps1` (a herd comes out on arrival on each side of the island, goes quiet when
  the player leaves, comes back on return; `-Shots` for pictures). Both are in `Tools\alltests.ps1`.

## 2026-10-25: one command runs every test

- **`Tools\alltests.ps1`** builds, then runs the automation tests and every bot test in turn and prints a PASS / FAIL
  table (`-SkipBuild`, `-Only ranged,tame`; each test's full output is kept in the run folder). Every bot test now ends
  with its checks (`PASS` / `FAIL` lines, "N checks, M failed") and exits 1 on a failure.
- New automation tests: `Ranged.Items` (ammo kinds, picking ammo and the wheel, stacking gear, quality, recipes),
  `Taming.Food` (food values, kibble, tame times, every tameable creature eats something), `Creatures.Behaviour`
  (diets, who hunts whom, every hunter has prey, attacks and activity are known), `World.Clock` (night and daylight by
  hour). The rules they check were pulled out into plain functions (`FWRInventoryRules::FindAmmo/NextAmmo`,
  `FWRCreatureDef::FoodValue/CanHunt`, `AWRGameState::IsNightAt/DaylightAt`).
- **`-wrworld=<name>`** gives a server its own world save (`Saved/ServerDB/world_<name>.json`); the arena tests use it,
  so tests running side by side no longer load each other's tames and pieces.
- Fixed: E next to a wild creature and a station opened the station's menu instead of feeding the creature; the nearer
  one wins now.
- Fixed: a creature's lunge, charge or stomp hit any hostile it touched, so a tame's lunge could hit a creature you'd
  knocked out (costing effectiveness); special attacks pass over knocked-out creatures unless that was the target.
- `WRTameAll <order>`: every tame of yours within 30 m takes the order (a whistle: all stay, all follow...).
- The full suite passes: 13 automation tests and 197 checks over 15 bot tests (`Tools\alltests.ps1`, about 40
  minutes). The taming test ran three times in a row clean after its scenario was made deterministic (tames parked,
  the Rimeclaw rooted before it wakes).

## 2026-10-25: test pass

- The older bot tests now check what they test and end like the rest (a PASS/FAIL line per check, "N checks, M
  failed", exit 1 on a failure, no errors in the logs): `bosstest` (16 checks), `buildtest` (16), `mptest` (9),
  `persisttest` (7), `socialtest` (14), `spelltest` (19), `terrain/walktest` (5) and `terrain/hazardtest` (6). Each has
  its own fresh world save (`-wrworld=<test>`) and stops once its bots are done. On the way: buildtest's crop plot was
  placed out of reach and never built, and spelltest learned a branch node before its trunk.

## 2026-10-25: creatures that hunt, flee, sleep and fight differently

- **Attack variety** (`attacks` in `creatures.json`, numbers in `tuning.json` `creatureAI`), each telegraphed: a
  **lunge** that leaps where its target will be (Cinderfang, Gloomhowl, Rimeclaw, Magmaw), a **charge** down a marked
  strip that knocks you back (Emberhorn, Frostantler), a **stomp** that hits everything in a marked ring (Basaltback,
  Woolly drudge), and **spit**, a glob of fire or frost from range (Magmaw spits first then leaps, Scorchwing,
  Glacimaw). Bites are unchanged; tames use their specials too. A rooted or frozen creature can't leap or charge.
- **Food chain**: carnivores get hungry every few minutes and hunt the nearest wild non-carnivore they can take (up
  to 1.3x their size, pack hunters 2x), run it down (pursuit speed, no flanking on running prey, a longer leash on a
  hunt), and the pack shares the kill and eats. Prey that's hunted bolts in bursts (fast ones like the Ashstrider can
  get away); neutral prey fights back. Basaltbacks and Woolly drudges are too big to hunt. Wild creatures otherwise
  still never fight each other.
- **Morale**: a creature below 25% health runs from what's hurting it, or below 50% when a packmate just died;
  passive animals bolt when hit. Bosses and their minions never run.
- **Day and night** (`activity`): day animals sleep at night and night hunters (Gloomhowl, Glowmoth, Aurora fox) by
  day. Asleep, a creature lies still and notices you at a third of the distance ("(asleep)" on its label), and a hit
  wakes it; night hunters notice you from 40% further at night.
- Dev and test commands: `WRSpawnAt <creature> X Y`, `WRCreatureHP <creature> <fraction>`; `Tools/aitest.ps1
  [-Shots]` stages a Cinderfang pack hunt (Pumicehog killed and shared, Ashstrider ignored once they're fed), the
  Gloomhowls asleep by day and hunting at night, a hurt Rimeclaw running, and a charge, stomp, spit and lunges. The
  ranged test now roots its target as it spawns (fresh Cinderfangs lunge at you).

## 2026-10-25: day and night

The world clock (`AWRGameState`, `clock` in tuning.json) now shows on every client, on the island and the arena.

- **Sun and moon**: `AWRSky` (spawned on every machine that renders) turns the map's sun with the hour: it rises in the
  east at dawn (6:00), crosses the south at 65 degrees and sets in the west at dusk (20:00), and the atmosphere reddens
  it on the way down. A moon rides opposite it as a second atmosphere light: a disc in the sky, moonlit clouds and fog.
- **Darker but playable nights**: the sky light is lifted at night so shapes still read, the fog goes deep blue, and
  auto-exposure may only brighten so far (otherwise moonlight looks like day). Numbers in tuning.json `sky`.
- **Lights at night**: the Hearth and the forge have a fire's warm glow (torches and braziers already had theirs).
- **HUD clock**: "Day 3   21:40" top right, pale blue at night.
- **The time carries over a restart**: the world save keeps the clock (`clockHours`); a new world starts at
  `clock.startHour`.
- Building: an **upper wall** (2 m, wood or stone) closes the gap a one-cell sloped roof leaves above its high side.
- `Tools\skytest.ps1` (10 checks): a bot builds a hut with a torch, a brazier, a claim and a Hearth, jumps the clock to
  noon, sunset and midnight and reads the client's sky (`WRSky`) at each, checks the clock runs at clock.dayMinutes and
  that a restart keeps the time. `-Shots` takes the hut and the moon at each time of day; `-Arena` runs it on the arena.

## 2026-10-25: ARK-style taming

- **Knock it out, then feed it.** E on a knocked-out creature opens the **Taming menu**: give it all of a food at once
  (kibble or what it eats) and it eats from that stash on its own, one item every 30 s; taming moves while a meal is
  digesting and stalls when the stash runs dry. The taming times (60 / 300 / 900 s by difficulty) are seconds of eating
  plain food. A creature that **wakes up loses its progress** (the food it hasn't eaten stays in its stash).
- **Kibble** (workbench: cooked meat, 3 berries, an egg, makes 2): every tameable creature eats it, it counts double and
  costs no effectiveness. Eggs come from tamed Snowpuffs, so kibble is a second-tame thing, like ARK.
- **Narcotics keep it under**: each one adds 30% torpor, without the hit that a dart or arrow is. Knocked-out creatures
  drain torpor slower now (250 s from full), so a medium tame fits in one knockout on kibble and a hard one needs
  narcotics.
- **Taming effectiveness** starts at 100%, drops 6% per hit while it's out (tranq ammo too: give narcotics instead) and
  1% per plain-food meal. The tame gets **+health and +bite of up to 50%** from it (`taming.bonusAtFull`); eggs and
  hand-fed calm creatures come out at the full bonus. The bonus is saved with the tame.
- A **taming panel** on the HUD for the nearest knocked-out creature: torpor and when it wakes (from the server's own
  drain, red under 30 s), progress, effectiveness, stash, and "tamed in ~N s", "needs N more raw meat", "it wakes first:
  give it narcotics" or "hungry". `WRTameStat` shows effectiveness, stash, bonus and max health.
- Fixed: your own tames attacked a creature you'd knocked out (they took it as your combat target) and could kill it; tames
  leave knocked-out creatures alone now, and a tame told to stay only fights within 15 m of its post (it crossed the map).
- Fixed: a quick shot could fly the wrong way: the server checked the aim point against its copy of your view, which can
  be a frame behind a flick. The shot now carries the view direction, and the crosshair trace sees creatures (low ones
  sent the shot at the ground behind them).
- Every taming number is in `tuning.json` (`taming`), and the Taming menu / HUD lines are `WRTameGive <food>` and
  `WRNarcotic` for bots. `Tools/tametest.ps1` knocks a Cinderfang out with two tranq arrows, costs it effectiveness with
  a stone arrow, keeps it under with a narcotic and tames it on kibble (+47%), lets a part-tamed Rimeclaw wake up (back to
  0%), and checks the tames and their bonus survive a restart; `-Shots` takes the panel and menu screenshots.

## 2026-10-25: building a house

A two-room, two-storey house with stairs and a gable roof now goes up the way a player expects, and
`Tools\housetest.ps1` builds one. Before, only a one-room box worked: pieces could be stacked into the same spot, the
ghost showed green for things the server then refused, outer corners had holes, the "roof" was a second flat floor,
the stairs a single plank, a wooden building past two storeys collapsed, and nothing could be removed in game.

- **Ceilings sit on walls, a second floor you walk on**: the build ghost picks the storey you're looking into (a floor's
  surface counts as its storey, so aiming at the upper part of a wall no longer jumps to the next one); floors / ceilings,
  roofs and gables go on top of it. Walls stacked straight on walls, pillars or foundations cost no span (they carry
  the weight down), so a wooden building can go up storey after storey; reaching sideways still uses wood's 4 / stone's 6.
- **One piece per spot**: a door, doorway, window, wall or gable can't go on an edge that already has one, a floor or
  roof not into a cell that has one, and stairs keep the opening above them clear. The ghost turns red with the reason
  (no support, something is already there, the stairs need this opening) before you click: support values replicate,
  so the client checks the same rules as the server.
- **New pieces**: a **doorway** without a door (between rooms), a **sloped roof** (rises 2 m across a cell, with eaves;
  R turns which way, two facing ones meet at a ridge) and a **gable** (the triangle that closes a roof's side; R flips
  it). **Stairs** are eight treads between stringers with a hidden ramp you walk up, ending level with the floor above.
  Walls, windows and doorways run 10 cm past each end so corners close.
- **Planks and stone blocks**: built pieces wear world-aligned materials (`Tools/make_build_materials.py`,
  `/Game/Building`): 20 cm boards with staggered joints and grain for wood, 45 x 90 cm blocks in running bond for stone.
  The ghost keeps its green / red tint.
- **Demolish mode**: the top of the build menu; aim at a piece of yours (it turns red), LMB removes it for half its
  cost. `WRDemolish x y z` does the same from the console.
- **Build menu** in groups with headings (wood building, stone building, crafting/storage/beds, land claim, defense,
  decor); names say what pieces are for ("Wood floor / ceiling", "Wood stairs (one storey)").
- `Tools\housetest.ps1` (25 checks): foundations, a wall, a ceiling and an upstairs wall go through the ghost exactly as a
  player aims them (and land where expected); the expected refusals happen; the bot demolishes a pillar, climbs the
  stairs to the first floor, and walks in through the front door and the inner doorway; a server restart loads all 34
  pieces back. `-Shots` adds screenshots. The survival test's hut now gets a sloped roof and two gables.
  Bots: `WRGhost <piece>` + `WRLook` + `WRGhostPlace` place through the ghost. `Wildreach.Building.Rules` covers the
  snapping, slots, support and storey rules.

## 2026-10-25: ranged weapons are crafted

- **By hand** (B menu): throwing spear (4 wood, 2 stone, 2 fiber), bola (6 fiber, 3 stone), 3 stone arrows
  (wood, stone, fiber), tranq arrow (stone arrow + narcotic), tranq bolt (crossbow bolt + narcotic).
- **Workbench**: hunting bow (10 wood, 12 fiber, 2 hide), blowgun (6 wood, 4 fiber, 1 hide).
  **Forge**: 6 crossbow bolts (copper ingot, 2 wood); the crossbow at Hearth tier 2 (3 bronze ingots, 8 wood, 6 fiber).
  **Alchemy table**: the new **narcotic** (3 berries steeped with a raw meat, makes 2), and torpor darts now take
  narcotic + fiber (was 2 berries, 2 fiber), so every knockout goes through it.
- Bows, crossbows and blowguns roll **quality** like swords: quality x roll scales the weapon's own damage and torpor
  (the ammo's share is fixed), each shot wears it by 1 (`ranged.wearPerShot`), and worn out it hits at half.
  It's repaired where it's made: the station menu shows "Repair Hunting bow (x/250)" at the workbench, and the forge
  says where to go. Spears and bolas stack, so they're crafted in stacks like ammo, not one rolled item each.
- `tuning.json` has a `ranged` section with every ranged number the code was defaulting (aiming camera, spread, throw
  release, projectile size and lifetime, bola limits, wear).
- The rules test still checks the crude tools need only wood and stone; every other hand recipe now has to take
  things gathered bare-handed (wood, stone, fiber, berries) or made by another recipe, and every ranged weapon and
  ammo has to have a recipe. `Tools/crafttest.ps1` crafts all of them at the arena's stations, then wears a bow by
  shooting and repairs it at the workbench (the forge refuses).

## 2026-10-25: ranged weapons

- **A ranged slot** next to the sword and shield: a hunting bow (stone and tranq arrows), a crossbow (bolts and tranq
  bolts), a blowgun (torpor darts), throwing spears (stack of 5) and bolas (stack of 10). **G** swaps between the
  sword and shield and the ranged weapon, **right mouse** aims (the camera pulls in over the shoulder, the view
  narrows, you walk at half speed and face where you look), **left mouse** fires or throws at the crosshair, and the
  **mouse wheel** picks the ammo. The HUD shows the weapon and what it's loaded with; firing from the hip spreads 4°.
  A bite that staggers you also stops you shooting.
- Shots are real projectiles with drop. Arrows, bolts, darts and spears **stick** where they land, tip first (and ride
  along on a creature they hit); a thrown spear is left on the ground to pick up again. A **bola** roots small
  creatures (new `Rooted` status, no movement) and slows big ones. Tranq ammo adds **torpor** on the hit, the start of
  taming: two tranq arrows or two torpor darts knock a Cinderfang out.
- Art: AI-made bow, crossbow, blowgun, spear, bola and arrow (`batch_ranged.json`; `normalize_gear.py` bow / pole /
  ball modes). Pole mode now turns an item onto its principal axes first: image-to-3D left the blowgun, spear and
  arrow lying on a slant, so they were held and flew crooked. 13 new Mixamo clips on both bodies: bow idle / walk /
  run / draw / aim / aim-walk / release / equip / put away, rifle aim / fire / reload (crossbow and blowgun), and a
  throw. Each shot plays its fire clip and then its reload.
- Holding them: `ranged.socket / along / face` in `items.json`, worked out from the skeleton. `"pose"` measures those
  directions in a body clip instead of the reference pose (arms hanging), and `"along": "hands"` runs the weapon level
  from the trigger hand toward the other hand: the crossbow and blowgun sit at the cheek in the rifle-aim pose, and the
  spear is carried level, blade forward. Mixamo's rifle aim holds the gun ~25° left of where the body faces, so the
  body turns that much while aiming one.
- Adding clips to a survivor without touching its mesh: `Tools/import_anims.py` imports animations only onto the
  existing skeleton (Interchange, through a pipeline stack override: a bare pipeline in the import task is ignored
  and the default one imports a new mesh and skeleton). `survivor_build.py --rest-from` builds the clip FBX on the
  survivor's own rest pose and strips tracks for bones the survivor lacks (the FBX exporter drops any clip that has one).
- The crosshair and `WRAimAt` use the survivor's own camera, so the art camera can watch a bot shoot.
- Bot and test commands: `WRRanged 0|1`, `WRAim 0|1`, `WRFire`, `WRAmmo 1|-1`, `WRAimAt <creature>` (holds over for
  drop), `WRHold <seconds>` (roots the wild creatures within 30 m), `WRClearCreatures` (removes them and the training
  dummies). `Tools/rangedtest.ps1 [-Body f]` fires every weapon at a fresh, rooted Cinderfang on the arena with
  screenshots: 9/10 hits on the male body (the spear fell short), 10/10 on the female, no errors.

## 2026-10-25: the survival loop works from a fresh spawn

A playthrough as a fresh bot on `L_Island` showed the basic loop didn't hold together: the island's trees, rocks and
shrubs were decoration (swinging at them gave nothing; the only gatherables were ~190 placeholder cones over 16 km²),
there were no tools, nothing could be made without a station, and the workbench only cooked meat and made waterskins.
Now a fresh survivor can do it all, and `Tools\survivaltest.ps1` plays it end to end.

- **Gather from the island itself**: a melee swing chops the tree, breaks the rock or picks the shrub in front of you
  (trees -> wood, rocks -> stone, shrubs -> fiber and sometimes berries). Each takes a number of hits, then vanishes
  for everyone and grows back later (`gathering` in `tuning.json`: per hit, hits, minutes). The used-up ones replicate
  through a small `AWRHarvestLedger`; the decoration itself stays local. Every gather shows a notice ("+12 Wood
  (Crude hatchet)").
- **Tools**: a crude hatchet and a crude pick, made **by hand** from wood and stone (they're at the top of the build
  menu, B), and a stone hatchet and stone pickaxe at the **workbench** (wood, stone, fiber). Tools work from your bag:
  the best hatchet you carry chops, the best pick breaks stone, ore and sulfur. Bare hands 1x, crude 1.5x, stone 2x
  (`"tool": {"kind", "yield"}` in `items.json`). Recipes with station `hand` are made anywhere.
- **Foundations sit into the ground** (`building.foundationSink`, 60 cm) so the floor is a walkable step instead of a
  waist-high ledge, and the slab runs deep, so slopes don't show a gap under it. A new foundation next to one you built
  levels with it; otherwise it sits on the ground at its cell's centre, wherever you aim.
- **Placing a workbench, forge, Hearth or totem shows a preview**: the ghost was invisible (those are child actors only
  the server spawns).
- **Doors swing on their hinge** instead of turning in the middle of the doorway.
- Sulfur now counts as mining (pick, mining XP), as the chop clip already assumed.
- **E uses the nearer** of a door / chest / bed / crop / claim and a station, which is what the prompt already showed:
  a workbench inside a small hut no longer opens its menu when you go to close the door, and the server's interact
  picks the door rather than the wall beside it. The build menu is titled "Build & craft".
- The **chop clip** plays at island trees and rocks too, and the gather crouch at shrubs.
- `WRGather tree|rock|shrub|node|any|off`: the test bot walks to the nearest one and gathers with light attacks.
- `Tools\survivaltest.ps1` (23 checks): a fresh fire survivor on the island gathers bare-handed, crafts the crude
  tools, gathers more with them, builds a hut (foundation, three walls, a door, a roof) and a workbench out of what it
  gathered, opens the door and walks in, crafts the stone tools at the bench, closes the door from inside and gathers
  more again. `-Shots` adds screenshots (a tree before and after felling, the hut from three sides and through the
  door, a chop). It backs up the island's world save and puts it back.
- Known: tools don't wear out yet (durability is only saved for equipment).

## 2026-10-25: the humanoid bosses have bodies

- **Vulkarn, Grimjaw and the Obsidian Warden** are AI-made, rigged on Mixamo and animated with its Creature Pack
  (breathing idle, walking, run, swipe / punch / jump-attack attacks, roaring, dying). Creatures with several attack
  clips (`attack`, `attack2`, `attack3` in `creatures.json`) take them in turn, and every creature attack clip is now
  fitted to the swing's length, so the hit lands with the telegraph. Bosses **roar** when they enter a new phase
  (`roar` clip; `Phase` replicates with a notify).
- **Ignessa** is a winged fire fae: `rig_special.py --body fae` gives her a body, a head and two hinged wings that fan
  while she floats; her fly / hover clips map to Run / Idle.
- Every creature now has art except the training dummy. The **wisp** (Ignessa's summons) is a glowing orb: creatures with a `glow` value and no mesh get an emissive sphere in their colour (`/Game/Creatures/M_Glow`, made by `Tools/make_glow_material.py`) and a point light that bright.
- Image-to-3D brutes often fuse a hand to a thigh: `survivor_build.py --cut-stretch 4` drops faces that stretch past
  4x their length during any clip (only the clips count; the T-pose bind itself is ignored) and the scraps left over,
  then closes each hole it leaves with a patch textured from the faces around it.
- Fixed: the shield could lie flat like a tray. The skeleton's reference pose in Unreal isn't always the T-pose (it
  can be a clip's first frame, arms hanging), so the shield's face now comes from the arm itself: away from it,
  between up and out from the body.
- Mixamo without Raven at the keyboard: the decimated copy goes into the page as text; wide bodies get a tiny
  framing triangle so the marker view shows them whole (`Tools/art/README.md`).
- Fixed: packaging crashed cooking the Barghest pack's mane and tail clips (`Assertion failed: !Hash.IsZero()`: they
  have no skeleton in the pack). The game never used them; they and their meshes moved out of `Content` (kept in
  `C:\Dev\WildreachArt\replaced\barghest_parts`). The pack is git-ignored, so re-adding it from Fab brings them back.

## 2026-10-24: a second body, a real sword and shield

- **Female survivor**: the second AI survivor is rigged on Mixamo with the same 55 animations
  (`/Game/Characters/SurvivorF`). The main menu has a **Male / Female** choice, sent as `?Body=f` on the join URL,
  kept on the PlayerState and replicated with each pawn, so everyone sees the body you picked. Bodies are variants
  under `"bodies"` in `Data/player_art.json`; each overrides any top-level field.
- **Real sword and shield**: an AI-made one-handed sword in the right hand and a round iron-rimmed shield strapped to
  the left forearm, replacing the cube blade. `Tools/art/normalize_gear.py` puts the grip at the origin and trims
  stray struts off the shield's back. The game works out the blade and shield-face directions from the mesh bounds and
  the T-pose skeleton (`weaponMesh`, `shieldMesh`, `shieldSocket`). First person hides the shield; the sword stays.
- **Gathering clips play**: swinging at a resource node right in front does an overhead chop for wood, stone and ore,
  or a crouching gather for plants; looting a bag, taking an egg or feeding a wild creature plays a pick-up for
  everyone.
- **Jump fixed**: the jump clip no longer adds its own hop on top of the real jump, and the fall starts airborne.
- `Tools/art/survivor_build.py --cut-stretch 2.5` removes faces that tear when animated (image-to-3D bodies sometimes
  fuse a hand to a thigh), measured against the model's own pose, plus the scraps it leaves.
- Fixed: `Tools/import_prop.py` re-parented every prop material under `/Game/Props` (the caves' tinted rock
  included) to the plain prop material. It now fixes only what it imports.
- `Tools/playershots.ps1 -Body f` frames the female body.

## 2026-10-24: the player has a body

- **Custom survivor** (Raven's pick over the UE mannequin): an AI-made male survivor (T-pose concept, so Mixamo can rig
  it), rigged on Mixamo and animated with the Sword And Shield pack (49 moves) plus a dodge roll, an overhead chop,
  gathering, picking up, swimming and treading water. 55 clips in `/Game/Characters/Survivor`.
- `Data/player_art.json` drives it: mesh, facing, and which clips play for idle / walk / run (play rate matched to the
  real speed), falling, blocking, dodging, swimming, light and heavy swings (fitted to the swing's length), spell casts,
  hit flinches and death. The placeholder blade is now a sword held in the right hand, its grip worked out from the
  skeleton. First person hides the body.
- `Tools/art/survivor_build.py` copies Mixamo's rig from the decimated copy it was made on onto the full textured mesh,
  moves the rest pose to the animations' T-pose and makes the clips play in place; see `Tools/art/README.md` for the
  Mixamo steps (and the page's quirks). `Tools/playershots.ps1` frames the player with `WRArtCam me`.
- Known: the female survivor and the four humanoid bosses are built and Mixamo-ready but not rigged yet (the bosses'
  arms-down poses will need T-pose versions); gathering, chopping and pick-up clips are loaded but not yet triggered by
  the game; the jump clip's own hop adds to the real jump; the sword is still the placeholder cube.

## 2026-10-24: real art, part 2 (every wild creature, terrain polish)

- **Every wild creature now has art** (19 of 25 creatures; the four humanoid bosses and the wisp remain):
  Glowmoth (flat moth that hovers and flaps), Glacimaw (ice worm that slithers and rears to strike) and Snowpuff
  (fluffy flightless bird) from the AI pipeline, with new rigs (`rig_special.py --body moth|worm`, `--legs 2`);
  Gloomhowl (pale Barghest), Scorchwing (ember Mountain Dragon) and Skaldrith (ice Mountain Dragon, the boss) from the
  QuadrapedCreatures pack with tinted materials (`Tools/art/creature_tints.py`, `wire_pack_creatures.mjs`).
- Creatures can override materials per slot (`materials` in `creatures.json`), and flyers play `fly` / `hover`
  animations while airborne.
- **Lava and ice are one continuous surface** each (procedural mesh with shared corners), so slopes are smooth
  instead of stepped tiles.
- **Caves are sculpted in Blender** (`Tools/art/cave_gen.py`, Raven asked for this over the blocky look): a rough rock
  mound with a domed chamber (~24 m across, ~10 m high) and a ragged, recessed entrance tunnel carved out, three
  variants picked by position, tinted rock per biome. Collision is the mesh itself (`Tools/import_cave.py`) and the
  landscape stays the floor. The old block shell (now boulder-covered) is only a fallback.
- **Ridge cliffs**: 4.7k large boulders on the ridge's steep faces (`basalt_cliff` / `ice_cliff`), hiding the
  stretched landscape triangles there. Decoration only, so the ridge is still a wall. Existing placements unchanged.
- **More trees**: charred trees come in four models (two new: a split snag and an ember-lit dead tree) and the
  tundra mixes five (spruce, wind-bent mountain pine, snow-laden fir and a frosted birch are new).
- **Fixed: trees floated on slopes** (Raven spotted it): every tree, rock and shrub now stands on the lowest ground
  under its footprint (`dressing.mjs`), so the uphill side buries instead of the downhill roots hanging in the air.
  Positions are unchanged; trees dropped 0.9 m on average.
- **Fixed: ice sheets stretched into the sky** at the glacier plateau's edge; surface corners now only join cells
  within 7 m of each other (steep river beds stay joined, plateau edges split).
- Packaging: the pack creatures' meshes and animations are in `DirectoriesToAlwaysCook`.
- Checks: `Tools/terrain/tree_shots.ps1` takes eye-level shots among the trees in both biomes.

## 2026-10-24: real art, part 1 (creatures and terrain)

- **13 creatures made with the local AI pipeline** (`Tools/art/README.md`): Pumicehog, the nine four-legged ones
  (Magmaw, Basaltback, Emberhorn, Ashstrider, Rimeclaw, Frostantler, Rimeshell, Aurora fox, Pale Hunter) and the three
  six-legged ones (Cinderfang, Woolly drudge, Kilnmite). Each has Idle/Walk/Run/Attack/Hit/Death.
  The rigger does six legs (tripod gait), adds a middle leg pair to four-legged models (`--add-mid-legs`), and
  lengthens foreshortened torsos (`--stretch`). `Tools/creatureshots.ps1` + `WRArtCam` screenshot a batch in game.
- **Terrain textures** (`Tools/terrain/build_terrain_materials.py`): the landscape material uses the MW Landscape
  Auto Material pack: dark ash with stony patches and black sand in the west, snow and pale sand in the east, basalt /
  frost rock on steep slopes. New `M_Rock` (boulders, cave walls), `M_Lava` (glowing, flowing) and `M_Ice` (glossy).
- **Dressing uses real meshes**: AI-made charred trees (two kinds) and snowy pines (`Tools/import_prop.py` ->
  `/Game/Props`, Nanite, invisible trunk collision as before), SimpleRocks boulders (same footprint and collision as
  before), GV shrubs, and rock-textured cave walls; falls back to the engine shapes when a pack or prop is missing.
- **Packaging**: creature art and dressing meshes are loaded by path, so they're now listed in
  `DirectoriesToAlwaysCook` (a packaged build would otherwise have shipped placeholder shapes).
- **Fixed**: art creatures whose radius exceeds their half-height floated (the capsule grows to the radius); the
  rigger ignored `--head`; re-importing a creature kept the old texture and animations (the importer now refuses).
- Known: lava tiles step visibly on slopes (needs one continuous surface); shrubs stay green in the volcanic west (the
  pack's leaf colour ignores tints); one pine model for the whole tundra; cave shells are still boxes (now rock-textured);
  the sawtooth shadow on the steep ridge face remains.

## 2026-10-24: playable with friends

- **Main menu** (shown when the game runs on its own): name, Fire or Frost, **Host** (your game becomes a listen
  server on UDP 7777) or **Join** by IP; remembered between runs. F10 returns to it. `Tools/package.ps1` makes a
  shareable Windows build with a players' README.
- **Paths menu (P)**: both trees with ranks, costs, branch gates and points to spend; Enter learns.
- **Every tree node works**: spells Fire wall, Lava pool, Meteor (fire) and Frost nova, Ice lance, Ice wall,
  Blizzard (frost); weapon arts Molten edge, Molten cleave (leaves lava), Flurry; passives spreading flames, fuel the
  fire, wildfire, explosive bolt, overheat, searing parry, brittle, cold return, chain shatter, cold riposte, frozen
  moment, whiteout. Numbers in `tuning.json` `spells.*` / `passives.*`.
- **Map list (M)**: bosses, mini-bosses, passes, camps and caves by distance with a compass direction.
  **Chat (T)**: everyone on the server sees it.
- **Island dressing**: 26k placeholder trees, rocks and shrubs and six midland caves (rock tunnel + dark chamber
  with rich ore at 12% gem odds, sulfur gas or slick ice, a Kilnmite or Glacimaw colony), all generated from the
  heightmap into `Data/island_dressing.json` and built on every machine at runtime.
- **Biome gear checks**: the lava river burns without lava resistance (fire main path, fire-infused armor, or a
  mount); slick ice on the tundra rivers and glacier plateau without ice resistance (frost main path, frost-infused
  armor). HUD warning when you're standing in one.
- **Fixed**: clients used their own default server preset, so lock-on and PvP disagreed with the host; the host's
  settings and everyone's clan now replicate. The tame test no longer races a wandering herd animal.
- Tests: `spelltest.ps1`, `terrain/hazardtest.ps1`; `island_shots.ps1` also covers caves, lava and ice.
- Known: hazard tiles are flat placeholder slabs; decorations and caves stop drawing at ~250 m (the World
  Partition loading range; HLODs would fix distant views); a dedicated-server package waits on the source engine build.

## 2026-10-23/24: the rest of the first playable (batches 1–5)

Everything below is placeholder art (engine shapes, tinted) with numbers in `Data/*.json`, all first drafts to tune in playtests. Each batch has a bot test script in `Tools/`.

- **Crafting & equipment** (`crafttest.ps1`): recipes by station (forge, armorer, alchemy table, workbench) from `Data/recipes.json`; quality tiers Crude→Masterwork with stat rolls; gem sockets; fire/frost infusion with path essences; repair and +1..+10 upgrades with shards; the Hearth's boss cores raise station tier. Weapon/armor durability, armor damage reduction.
- **Building** (`buildtest.ps1`): snap grid, foundations/walls/floors/doors/gates/chests/beds/crops/pens/stations from `Data/pieces.json`; support (wood spans 4, stone 6) with cascade collapse; code locks; land claim totems with upkeep and decay; raids damage pieces; the world saves to `Saved/ServerDB/world_<map>.json`.
- **Creatures & taming** (`tametest.ps1`): the 17-creature roster (pack hunters, flyers, burrowers, herds, critters); knockout + feed, offer food, steal an egg (nests with angry guardians) and path magic; torpor darts; tame orders (follow, stay, aggressive, passive, release); rideable mounts; Glowmoth light, ore-mining tames, pen produce; 2-tame limit; tames saved with the world.
- **Bosses** (`bosstest.ps1`): Vulkarn (anvils vs lava floods, a tongs grab you can parry, white-hot phase 2, hidden phase 3), Skaldrith (ice pillars block breath and shatter, flight and dives, hit his wings to ground him, whiteout phase 3), Obsidian Warden (shell; chill cracks it faster), Ignessa (airborne, exploding wisps, lands when they're gone), Grimjaw (regenerates unless burning), the Pale Hunter (invisible in blizzards, footprints, fire reveals). Telegraphed attacks, HP +80% per extra player or tame, ≥10% of the damage earns your own loot (cores, essences, parts, shards, trophies), boss HUD bar, respawn time from the server preset.
- **Life skills, server settings, clans** (`socialtest.ps1`): 15 life skills (levels 1–100, perks at 25/50/75/100; K opens the menu) wired into gathering, crafting, potions, food, crops, pens, building, taming and loot. Server presets `pvp_standard`, `pve`, `casual_coop`, `playtest` (default: 10× XP) and `custom` (`Saved/ServerDB/server.json`), picked with `-wrpreset=`. Clans (`WRClan create/invite/join/leave/kick/info`) share claims, doors, chests and tames, and can't hurt each other. Safe zones at the spawns. PvP and PvP-in-boss-arenas switches, offline base protection, death penalty gentle / Souls-like / drop / full loot with loot bags.
- **Island rebuilt** with the six boss spawners, spawn safe zones and starter camps; walk test still passes.
- Tuning overrides for tests: `-wrtune=key:value,key:value`.
- Known gaps (caves and foliage arrived 2026-10-24): fishing, engineering and swimming have no XP source; perks marked "not active yet" wait for their systems; the bounty and creature-raid settings exist but nothing uses them; boss loot goes straight to inventory (no loot bags).

## 2026-10-23: the island (M3 step 2)

- **Layout approved** (`Docs/terrain/LAYOUT_PROPOSAL.md`): island, hard-wall ridge, two passes, Vulkarn in the crater, Skaldrith on the glacier bowl, one water + one lava river on the volcanic side, middle spawns moved toward the passes, ~10 km² for the first playtest.
- **`L_Island`**: the full 4 × 4 km landscape (4033 × 4033 samples at 1 m, Z range ±1 km) as a World Partition landscape in 256 streaming pieces. It's now the default map for `play.ps1`, the server and the editor.
  - Built by `Tools/terrain/build_island.ps1`: Node exports the heightmap, then the editor (a window opens and closes by itself) imports it through `UWREditorTerrainLibrary`, waits for the landscape's GPU layer update, and saves.
  - Placeholder auto-material: dark ash west, snow east, rock on steep slopes. Sea plane, sky and lighting.
  - From the layout data: 6 spawns (biome-tagged), fire/frost altars at the two boss sites, 6 creature packs by zone, 124 resource nodes (wood/stone/copper everywhere, sulfur west, lichen east).
- **Walk test** (`Tools/terrain/walktest.ps1`), with real character movement: north-west spawn → north pass in 3.6 min, then onward to the tundra (5.9 min total). Middle spawn → north pass in 5.6 min, fording the river. Walking at the ridge mid-island gets stuck, so the wall holds.
- Fixed: rivers were 22 m deep trenches that trapped players; now 6 m deep, fordable, with gentle banks.
- Dev commands: `WRTeleport x y`, `WRLook yaw pitch`, `WRWalkTo x y`. `play.ps1` takes `-Map island|arena|grid -Path fire|frost -Name You`.
- Known: placeholder material only (no textures); rock barely shows on the ridge; a sawtooth shadow artifact on the steep ridge face at distance; no PCG foliage yet.

## 2026-10-22: M1 combat core + M2 paths (autonomous build session)

### Combat (M1)
- **GAS** set up: ability system on the PlayerState for survivors (survives death), on the pawn for creatures. One replicated attribute set: health, stamina, poise, hunger, thirst, Heat, Chill, burn/frost buildup.
- **One damage pipeline** (`UWRCombatLibrary::ApplyHit`, server only) used by every source: block (front 70°, stamina cost 10–25 by hit size, guard break at 0 stamina), parry (first 0.2 s of a block, staggers the attacker), dodge i-frames, poise → stagger → riposte (×3), Heat damage bonus (up to +30%), frostbite (+15% damage taken), Permafrost (+15% vs slowed).
- **Statuses:** Burn (DOT), Frostbite + Slowed, Frozen (frostbite on an already slowed target), Shatter (heavy hit on frozen; spends all your Chill for up to double the burst), Stagger. Replicated as gameplay tags; bodies tint by status.
- **Abilities:** light attack, heavy attack, dodge roll (root motion), block/parry (hold), Fire bolt, Frost shard, Flame slash, Ice slash, creature bite. Stamina costs and timings from the Starting numbers tab.
- **Combos:** Thaw (25/25: fire on a frozen target bursts and thaws it) and Balance (50/50: Heat and Chill feed each other).
- **Netcode:** attack/block slowdowns and sprint are predicted movement flags (no rubber-banding); hits are confirmed by the server.

### Paths (M2)
- Fire and Frost trees generated from the design doc (`Tools/data/extract_trees.mjs` → `Data/paths/*.json`).
- XP curve (point n costs 100 + 10n), 150 total / 100 per path, trunk-first, branch gates by points spent, root passive on the main path only, quick slots (8, auto-filled), character level = points ÷ 3 driving health/stamina (exactly 250/150 at 50).
- Main path from the join URL until there's a creation screen; spawn in its biome. **Boss altars** (E to interact) take/swap the subclass; progress is kept per path.
- **Persistence v1:** JSON per player in `Saved/ServerDB/players/`, autosave every 10 s, on logout and shutdown.

### Creatures & world
- `Data/creatures.json` (dummy, Cinderfang, Gloomhowl, Rimeclaw, Magmaw); stats from the zone tier's starting numbers.
- Training dummy (never dies, resets), pack-hunter AI (flanking, leash, give-up), creature spawner.
- `L_CombatArena` test map (dummies, both packs, both altars, biome-tagged spawns).

### Tools & tests
- `Tools/test.ps1`: 6 automation suites: XP curve, character level, tree shape, buy rules/caps, damage math, and a 12-step deterministic combat scenario in a real world. All pass.
- `Tools/mptest.ps1`: dedicated server + scripted bot clients (PvE/PvP), summarises hits/statuses/abilities/corrections from the logs.
- `Tools/persisttest.ps1`: build a character, restart the server, rejoin, check it's intact.
- `Tools/shots.ps1`: rendered client takes screenshots from both camera views.
- Dev commands (console / `-wrbotcmds`): `WRGivePoints`, `WRAutoSpend`, `WRBuy`, `WRSetMain`, `WRSetSub`, `WRWeapon`, `WRSpawn`, `WRStatus`, `WRHeal`, `WRKillMe`, `WRStarve`, `WRGoToAltar`, `WRInteract`, `WRView`, `WRBotPause`, `WR.ReloadTuning`.

### Stat nodes
- 13 stat nodes now do something (data-driven via `nodeEffects` in tuning): ignite chance, searing heat, long burn, crushing blast, heated blade, bellows, tempered, frostbite, deep freeze, hard break, keen edge (crits), light step, chilling touch. Ability nodes past the trunk (Fire wall, Meteor, Ice lance...) are still to build.

### Inventory (M5 start)
- 30-slot inventory on the PlayerState (server-authoritative, owner-replicated), items from `Data/items.json`, stacking + weight, starting kit (3 minor healing potions, 2 cooked meat, 2 waterskins).
- **Q** drinks a potion (healing only from potions, 6 s belt cooldown), **Z** eats, **X** drinks. HUD belt line shows counts, cooldown and carry weight. Inventory saves with the player.
- Dev command `WRGive <item> <count>`.
- **Gathering:** resource nodes (`AWRResourceNode`: wood, stone, copper ore, sulfur crystal, lichen). Melee swings harvest them (heavy = double), nodes deplete and respawn. Arena has 8 nodes. Test bot mode `-wrbotgather`.

### Terrain (M3 step 1)
- Landmass proposal for sign-off: `Docs/terrain/LAYOUT_PROPOSAL.md` (island, meandering ridge, two verified passes, volcano + glacier boss sites, rivers, spawns, walk times). Nothing imported yet.

### Known gaps
- Placeholder art only (no downloads this session): greybox bodies, no animations.
- Stat/ability nodes beyond the four trunk abilities and the capacity nodes are tracked and buyable but have no gameplay effect yet.
- No lag compensation for melee yet; ~0.3 small movement corrections/s per client in bot tests, likely pawn-on-pawn collisions.
- Save key is the player name (`?WRName=`); real account ids come with EOS/Steam (M7).
