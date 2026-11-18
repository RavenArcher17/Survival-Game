# Wildreach cook: UI AND CONTROLS (and the first hour)

2026-11-13. Lane `ui-controls`. Research, design and plan only; nothing in `C:\Dev` was changed.

**Scope:** FULL-GAME section 2 (the first hour) and every screen and control: main menu, character creator, HUD, journal, inventory, crafting, build bar, map, settings, F1 controls page, F8 bug report, the pad layout, and onboarding through play.

**Read for this cook:** FULL-GAME.md (all; sections 2, 5.2, 9 in detail), COMBAT-AND-LESSONS L4, L6, L8, L10, 1.15, 1.16; `cooks/combat.md` 2.1; `cooks/HERO-RESEARCH.md`; Raven's `CLAUDE.md`, `Docs/design/*`, and the UI code as it stands today (`WRMainMenu`, `WRUIWorlds`, `WRUIHud`, `WRUIJournal`, `WRUIInventory`, `WRUICrafting`, `WRUIBuild`, `WRUIMap`, `WRUISettings`, `WRUIFlow`, `WRUIWheel`, `WRKeyBindings`, `WRPadInput.h`, `WRReport.h`).

**Already in Raven's repo or being built today (not reopened):**
- The stone-and-leather Slate UI, every screen listed above, the journal with I/J/P/K/M/O tabs, hints and moments, the death map, worlds list, Host/Join, key rebinding, Video and Audio pages, the host's World page.
- The C1 round, landing now (`WRPadInput.h`, `WRReport.h`, both dated 11-13): `Data/gamepad.json` one-verb pad layout, a pad cursor and D-pad navigation on every screen, last-used-device prompts, held-input isolation (0.14 s), the F1 controls page, and the F8 report with a frozen frame, a 60 s ring, 8 kinds, a note, and a POST to F35 `/api/feedback` with a copy in `Saved/Reports`.

This cook keeps all of that and designs what's missing on top of it.

---

## 1. Research

### 1.1 What the best games do

| Game | What they do | Lesson for Wildreach |
|---|---|---|
| **Breath of the Wild** (Great Plateau) | No exposition. A walled starter area teaches cooking, cold, stamina and physics through objects placed in your path. It ends when you've earned the glider. A 2025 focus-group study found players still got overloaded at points. [Wikipedia](https://en.wikipedia.org/wiki/Great_Plateau), [Game Rant](https://gamerant.com/zelda-breath-of-the-wild-great-plateau-tutorial-introduction-design-effective/), [Utrecht thesis](https://studenttheses.uu.nl/handle/20.500.12932/46740) | Teach with **placement**: put the right object in reach at the right moment. One new thing at a time. The spawn camp is our plateau, but with no walls. |
| **Valheim** (Hugin) | You spawn at the stones with rags and a torch. A raven pops up the first time you meet something; you can ignore him. Within 30 minutes you have tools and a roof. [Windows Central](https://www.windowscentral.com/valheim-tips-and-tricks-beginners-guide-your-first-days-and-beyond), [Mobalytics](https://mobalytics.gg/gamebase/guides/valheim-beginners-guide) | Raven's one-time hints are already this. Make them shorter (a verb and a button, not a sentence) and pin them to the object. |
| **Elden Ring** | Messages on the ground. Map fragments at steles reveal regions. Sites of Grace light a ray toward the next place. Default PC keys: RMB guard, Space dodge, F jump, Shift+RMB skill (a chord). [Fextralife](https://eldenring.wiki.fextralife.com/Controls), [Shacknews](https://shacknews.com/article/128968/elden-ring-controls-and-pc-keybindings) | In-world notes from "the last one here". Reveal the map by place, not by a menu. Keep RMB = guard. **No chords** (our one-verb rule). |
| **Ghost of Tsushima** | The guiding wind replaces waypoints. The HUD hides itself except when needed: "HUD-less, no. Light HUD, yes." An expert mode cuts it further. [TechRadar](https://www.techradar.com/news/ghost-of-tsushima-interview-nature-sprawls-in-ps4s-epic-samurai-swan-song), [Android Central](https://www.androidcentral.com/ghost-tsushima-creative-director-interview) | A HUD that fades when calm and returns on any change. Three HUD modes. Point with the world (smoke, light, birds), not arrows. |
| **Monster Hunter Wilds** | Options set the size and visibility of 19 HUD elements. A desk/living-room preset scales HUD and text together. [Destructoid](https://www.destructoid.com/all-accessibility-settings-in-monster-hunter-wilds/), [Icy Veins](https://www.icy-veins.com/monster-hunter-wilds/news/best-settings-to-improve-your-monster-hunter-wilds-experience/) | One "Couch / Desk" switch that sets HUD scale and text size together, plus per-element toggles. |
| **Dragon's Dogma 2** | A map without icon clutter. Quests aren't marked until someone tells you. The journey is meant to be enjoyed. [Game8](https://game8.co/articles/featured/how-dragons-dogma-2-gets-the-open-world-genre-right) | The fill-in map shows only what you've seen. No checklist icons. |
| **Enshrouded / Palworld** | Build menus are radial or list-based on pad. Shoulder buttons switch categories. Enshrouded ships two pad presets plus Xbox, PS and Nintendo glyphs; its crafting pulls from base chests. [Enshrouded wiki](https://enshrouded.wiki.gg/wiki/Controls), [Palworld controls](https://www.palworld.tools/controls) | Pad build bar: bumpers for category, D-pad for piece. Glyph sets for Xbox and PS. Stations draw from nearby chests. |
| **Conan Exiles, Dune, Once Human, Dragon's Dogma 2** (creators) | Deep body sliders. Dragon's Dogma 2 lets body build change how armour sits. Once Human made re-edits free after players hated paying. See `cooks/HERO-RESEARCH.md` 1.1. | Short presets plus a few body sliders with **clamped ranges**, so every result looks good. Free re-edits at a mirror. |
| **MetaHuman, UE 5.6 to 5.8** | Parametric body controls exist **in the editor only**. There's no documented runtime body API. The supported runtime route is Mutable (Epic's sample was updated for 5.8 in 10-2026). Mutable plus MetaHuman reshape has reported morph bugs. [Epic body controls](https://dev.epicgames.com/documentation/metahuman/body-controls), [Mutable sample 5.8](https://www.unrealengine.com/news/mutable-sample-for-runtime-character-customization-now-updated-for-ue-5-8), [forum](https://forums.unrealengine.com/t/how-to-edit-metahumans-at-runtime-using-blueprints-ue-5-6/2565006) | **Bake slider extremes into morph targets offline** (body and every garment), then drive them at runtime with `SetMorphTarget`. That's cheap, replicates in a few bytes, and needs no Mutable. |
| **In-game reporters** (Sea of Thieves guidance; industry write-ups) | Take the frame **before** the overlay opens. Gather build, scene, position, state, device and the last 20 to 50 events automatically. Give an ID on send. Crashes need their own path, since a crashed game can't open the reporter. [Game Developer](https://www.gamedeveloper.com/blogs/in-game-bug-reporter-best-practices), [Sea of Thieves](https://support.seaofthieves.com/articles/26032567156242-Raising-Insider-Retail-Bug-Reports-for-the-Team), [Bugnet](https://bugnet.io/blog/in-game-bug-reporter-design-guide) | `WRReport.h` already does the first two. Add: an ID shown on send, and a **crash card on the next start** that sends the crash log in one press. |
| **Xbox Accessibility Guidelines** (XAG 101, 107) | Body text at least 26 px at 1080p on a TV, 18 px on PC. Remapping, and a hold-or-toggle choice for held inputs. [XAG 101](https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/101) | Body text minimum 18 px (Desk) or 26 px (Couch). Every hold has a toggle option. Full remap on both devices. |
| **UE5 on 8 GB GPUs** | Epic's own Medium scalability **turns Lumen off**. Nanite, VSM and Lumen all share VRAM outside the texture pool. The Nanite streaming pool doesn't follow scalability and must be set by hand. [Lumen performance guide](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine), [forum](https://forums.unrealengine.com/t/scaling-nanite-streamingpoolsize-vram-usage-via-scalability-settings-or-at-runtime/2712963) | Our own `DefaultScalability.ini`. Medium keeps **cheap Lumen GI** (the island is lit by it; without it the look goes flat), drops Lumen reflections, and sets the pools by hand for 8 GB. |

### 1.2 Lessons we take

1. **Teach by placement, confirm with a verb chip.** No paragraphs. A hint is a button glyph plus 1 to 4 words, pinned to the thing.
2. **One verb per button in play.** Holds are allowed as a separate verb on the same button (ER's own pattern). Chords never.
3. **The HUD earns its place.** It's full in combat or on any change, and fades when calm.
4. **The creator can't make anyone ugly.** Presets plus clamped sliders.
5. **Every screen, both devices, no dead states.** Every screen opens, works and closes by mouse and by pad. Nothing ever needs a keyboard while playing on a pad.
6. **Reports are the test channel.** F8 must be one press from anywhere, crash included.

---

## 2. Design

Tags: **SPEC** = BUILT or RAVEN-PLANNED, or being built in C1, restated with its source. **GAP-COOKED** = designed here.

### 2.0 Scope map

| # | Item | Tag |
|---|---|---|
| 2.1 | Pad layout (play, menus, build) | SPEC (C1 `gamepad.json`, CAL L4, combat.md 2.1) + GAP-COOKED (holds, item wheel, build mode) |
| 2.2 | Keyboard and mouse layout | SPEC (`WRKeyBindings`) + GAP-COOKED (new verbs) |
| 2.3 | Input rules (buffer, isolation, device, hold/toggle) | SPEC (CAL 1.16, L8, C1) + GAP-COOKED (toggles) |
| 2.4 | Main menu and front-end flow | SPEC (`WRMainMenu`, `WRUIWorlds`) + GAP-COOKED (characters, path wheel, crash card) |
| 2.5 | Character creator | GAP-COOKED (Aaron 11-13) |
| 2.6 | The first hour: onboarding through play | SPEC beats (FULL-GAME 2) + GAP-COOKED (camp kit, notes, hint chips, the first night) |
| 2.7 | HUD | SPEC (`WRUIHud`) + GAP-COOKED (modes, fade, cross, layout) |
| 2.8 | Journal shell | SPEC (`WRUIJournal`) + GAP-COOKED (pad, System tab) |
| 2.9 | Inventory | SPEC + GAP-COOKED (context menu, sort, pad) |
| 2.10 | Crafting | SPEC + GAP-COOKED (pulls from nearby storage, tracked recipe) |
| 2.11 | Build bar | SPEC + GAP-COOKED (pad build mode) |
| 2.12 | Map | SPEC (built map, RAVEN-PLANNED fill-in) + GAP-COOKED (reveal, pins, shrine travel UI) |
| 2.13 | Wheels (ability, tame orders, whistle, items) | SPEC + GAP-COOKED (whistle, item wheel) |
| 2.14 | Settings | SPEC (Controls, Video, Audio, World) + GAP-COOKED (Look & controller, Interface, graphics presets with 8 GB Medium) |
| 2.15 | F1 controls page | SPEC (C1) + GAP-COOKED (pad diagram, pages per mode) |
| 2.16 | F8 bug report | SPEC (C1 `WRReport.h`) + GAP-COOKED (ID, crash card, pad keyboard, quick chips) |
| 2.17 | Flow screens (loading, death, disconnect, chat) | SPEC + GAP-COOKED (pad chat, disconnect) |
| 2.18 | Look and text rules for all screens | GAP-COOKED |

---

### 2.1 Pad layout (one verb per button)

**In play.** C1's `gamepad.json` verbs are SPEC. Combat.md 2.1 adds crouch, the holds and the two-hand toggle. This cook adds the item wheel, the whistle and the view hold. "Hold" means 0.40 s unless stated; a tap fires on press unless the button also has a hold, in which case the tap fires on release before 0.40 s.

| Button | Tap | Hold | Tag |
|---|---|---|---|
| Left stick | Move | | SPEC |
| L3 | Crouch / sneak toggle (fires on release) | **0.40 s: first / third person** | GAP-COOKED (combat.md crouch; Raven's L3 view moves to the hold) |
| Right stick | Look | | SPEC |
| R3 | Lock on / off; flick the stick to switch | | SPEC |
| RB | Light attack | | SPEC |
| RT | Heavy | ≥0.35 s: charge (combat.md) | SPEC |
| LB | Parry + guard up | Guard; with a ranged weapon out, aim | SPEC (combat.md 2.1) |
| LT | Sprint (hold) | Settings: toggle instead | SPEC + GAP-COOKED toggle |
| A | Jump | | SPEC |
| B | Roll (stick at rest: backstep) | | SPEC |
| X | Use / interact / mount | 0.50 s on a tame: order wheel. On a mount: dismount. On your door: code lock. **At nothing: whistle wheel** (2.13) | SPEC (Raven's hold E) + GAP-COOKED whistle |
| Y | Drink / use the selected belt item | | SPEC |
| D-pad up | Fire the selected quick slot (Wildbrand or spell) | Charge, where the slot allows | SPEC |
| D-pad left | Previous quick slot | 0.50 s: swap melee ↔ ranged | SPEC (combat.md) |
| D-pad right | Next quick slot | **0.40 s: item wheel** (the 8 hotbar items: tools, weapons, torch, placeables) | GAP-COOKED |
| D-pad down | Next belt item | 0.50 s: one-hand / two-hand | SPEC (combat.md) |
| Menu (Start) | Journal, on the last tab used | | SPEC (C1) |
| View (Back) | F8 bug report | | SPEC (C1) |

**Why the item wheel lives on D-right hold:** today a pad has no way to pick a hotbar item (hatchet, pick, torch, sword). Holding D-left already swaps weapons, so holding D-right for items mirrors it. Release over a wedge to pick it; release in the centre to cancel. The game keeps running.

**In menus** (SPEC, C1): left stick moves the cursor, D-pad jumps to the next control, A click, X second click (context menu), B back, Y the F1 controls page, LB / RB journal tabs, right stick scroll, Menu close, View report. **Added (GAP-COOKED):** LT / RT switch sub-tabs inside a tab (crafting categories, build categories, the map legend, settings pages), through `PushPadHandler` verbs `SubPrev` / `SubNext`.

**In build mode** (GAP-COOKED; C1 has snap, demolish and cancel):

| Button | Does |
|---|---|
| Sticks, A | Move, look, jump (you keep moving, as today) |
| RT | Place the piece |
| LB / RB | Rotate −/+ 22.5° (hold: repeats every 0.12 s) |
| D-pad left / right | Previous / next piece in the category |
| D-pad up | Next category |
| D-pad down | Snap on / off (C1) |
| Y | Demolish mode (C1) |
| X | Copy: pick the piece you're looking at, with its rotation (Valheim and Enshrouded habit) |
| B | Put the piece away; again leaves build mode (C1) |

**Glyphs:** Xbox and PlayStation sets (C1 `names`). Auto picks from the pad's USB vendor ID (Sony 0x054C → PlayStation); a setting can force either.

### 2.2 Keyboard and mouse

Raven's built keys stay (`WRKeyBindings`), because his players know them and they follow the survival norm (RMB = guard and aim, as in Valheim and Enshrouded; ER's PC default is RMB guard too). **Combat.md 2.1's keyboard column is replaced by this table:** it moved heavy to RMB and guard to Mouse 4, which drops RMB aim for bows and needs a 5-button mouse. The pad columns of the two tables agree.

| Key | Action | Tag |
|---|---|---|
| W A S D | Move | SPEC |
| Space | Jump | SPEC |
| Left Shift | Sprint (hold; toggle in settings) | SPEC |
| C | Roll; no direction held: backstep | SPEC |
| Left Ctrl | Crouch / sneak toggle | GAP-COOKED |
| LMB | Light attack / fire | SPEC |
| R | Heavy (hold ≥0.35 s: charge) | SPEC |
| RMB | Tap: parry. Hold: guard, or aim with a ranged weapon | SPEC |
| Tab, Middle mouse | Lock on; a mouse flick of 300 px/s switches | SPEC + combat.md |
| V | First / third person | SPEC |
| E | Use. Hold 0.5 s: tame orders / code lock / dismount; at nothing: whistle wheel | SPEC + GAP-COOKED whistle |
| F | Drink / use the belt item (in build mode F stays snap) | GAP-COOKED |
| X | Next belt item (in build mode X stays demolish) | GAP-COOKED |
| H | One-hand / two-hand | GAP-COOKED |
| G | Swap melee ↔ ranged | SPEC |
| Q / Z | Ability wheel 1 / 2 (hold); cast modes Wheel / Own keys / Both | SPEC |
| 1 to 8, mouse wheel | Item hotbar | SPEC |
| I J B P K M O | Inventory, Crafting, Build, Paths, Skills, Map, Clan | SPEC |
| T, Enter | Chat | SPEC (Enter added) |
| Esc | Settings window, or back out of any screen | SPEC |
| F1 | Controls page | SPEC (C1) |
| F8 | Bug report | SPEC (C1) |
| F10 | Leave to the main menu | SPEC |
| Build mode keys | Raven's built build keys stay as they are (place, rotate, snap F, demolish X, cancel); new: middle mouse = copy the piece you look at | SPEC + GAP-COOKED copy |

Belt drink on F rather than R, because R is heavy (built).

### 2.3 Input rules for every mode

| Rule | Number | Tag |
|---|---|---|
| Input buffer for attacks, roll, Wildbrand, items; last input wins | 0.33 s | SPEC (CAL 1.16) |
| Dodge pressed mid-swing waits for `ANS_CancelWindow` | up to 0.6 s | SPEC |
| Mode changes drop held inputs and block new presses (menus, wheels, build, mount, death, report, controls page, creator) | 0.14 s (`look.isolationSeconds`) | SPEC (C1) |
| Prompts only for the last-used device; stick movement over 0.35 switches to pad, mouse movement over 6 px switches to keyboard and mouse | | SPEC (C1) + GAP-COOKED mouse threshold |
| Hold or toggle per held verb: sprint, guard, aim, crouch | default hold, except crouch toggle | GAP-COOKED (XAG 107) |
| Hold time for the "hold" verbs | 0.40 s (`input.holdSeconds`), 0.50 s for tame / dismount / two-hand / weapon swap | GAP-COOKED |
| Every gameplay verb rebindable on both devices; a clash asks "Replace?" (built for keys; pad rebinding is new) | | SPEC + GAP-COOKED pad remap |
| Invert Y off by default on both devices | | SPEC (CAL 1.15) |

---

### 2.4 Main menu and front-end flow

**SPEC today:** a single menu with name, body (male/female), path (Fire/Frost), Host (worlds list), Join by IP, Settings, Quit, update banner, version, rotating tips, and the reason a join failed.

**GAP-COOKED flow:**

```
Title ──A / click──► Characters ──Play──► Host (Worlds) or Join (IP)
  │                    │
  │                    └─New / Edit──► Creator (2.5) ──Done──► Characters
  ├─ Settings (same window as in game)
  ├─ Report a problem (F8 works here too)
  └─ Quit
```

**Title screen**
- **Backdrop:** a live 3D scene, not a picture. It's the spawn coast of the last path you played (Fire: black-sand cove at dusk, lava glow inland; Frost: a snow shore under aurora). A slow camera drift of 8 cm/s over 40 s, then a cut back. It's built as a small streaming level `L_Front` with two sub-levels (fire, frost; one more per biome as biomes ship), so no full island loads behind the menu.
- **Layout:** the carved WILDREACH title upper-left (built `MakeTitle`), with the tagline changed from "A SURVIVAL ISLAND OF FIRE AND FROST" to "TWELVE LANDS. ONE WILD." once a third biome ships. A button column left: **Play**, **Join a friend**, **Settings**, **Report a problem**, **Quit**. Version bottom-left, update banner top-right (built). Music: the main theme (music lane's cue `mus_menu_theme`), always playing.
- **Crash card** (GAP-COOKED, 2.16): if the last run crashed, a card sits over the column: "Wildreach closed unexpectedly last time. [A] Send the crash report · [B] Not now". It shows once per crash.

**Characters screen** (new; replaces the name and path boxes on today's menu)
- Up to **8 local characters** per PC, in `Saved/Wildreach/Characters/<id>.json` (name, appearance, main path, created date). A row is a portrait card with name, path sigil and last world played. The portrait is rendered once by the creator stage into a 512 px PNG, stored beside the json.
- Buttons on the selected card: **Play**, **Edit look** (creator; the name and path lock once the character has played in any world, since the host keeps its progress by name), **Delete** (asks: "Delete Kaia? Worlds keep their copy of her progress."). A last card reads **New character**.
- The worlds list (built `WRUIWorlds`) follows Play. Join follows Join a friend.
- A character's look is sent to the host on join (2.5 Replication). A world keeps progress by name as today.

**Path picker: the wheel of twelve** (GAP-COOKED; in the creator's Path tab)
- The **twelve paths as a carved ring** in wheel order (Fire west/left, Frost east/right, the rest in FULL-GAME 3.1 order), so opposites sit straight across, as on the map.
- Each medallion: the path's sigil, its name, and a one-line feel ("Fire: build Heat by attacking, burn everything", "Frost: hold them still, then shatter them").
- A path whose biome isn't in the build sits **sealed**: the medallion is cracked stone with its name, and clicking it shows "Sealed. Its pass hasn't opened yet." It's still a live control that answers, so there's no dead state.
- Picking a path previews its body marks on the character at 0.3 / 0.6 / 1.0 strength (HERO-RESEARCH 1.2) and names the spawn: "You wake on the Volcanic coast."

### 2.5 Character creator (GAP-COOKED)

**Rule:** short, pretty by default, every value attractive. Built on HERO-RESEARCH's numbers (6 faces, 3 builds, 10 hair, 6 beards per sex), plus **body sliders** (Aaron 11-13), clamped so they never leave a fit, attractive real-world range.

**The stage.** `AWRCreatorStage` in `L_Front`: a basalt ledge at dusk for Fire paths and a snow shelf for Frost. It has a 3-point light rig (warm key at 45°, cool rim, soft fill, skin-tuned exposure), so faces read in close-up. The character stands in the survivor wraps (about 40% skin showing), with an idle from the GASP set (breathing, weight shifts), so nobody stands in a T-pose.

**Camera.**
- Three framings: **Full body** (3.2 m, the default), **Upper body** (1.4 m), **Face** (0.55 m, 35 mm look).
- Each tab picks its framing (Body → full, Face → face, Hair → upper).
- Orbit: drag with the mouse or move the right stick, ±170° yaw and −10° to +25° pitch.
- Zoom: mouse wheel or LT / RT between the framings.
- **Turn-around:** R3 / middle mouse spins the character 180°.

**Tabs** (LB / RB or click; the D-pad moves between rows, left/right changes values):

| Tab | Male | Female | Notes |
|---|---|---|---|
| **Body** | Sex · build preset: Lean / Athletic / Strong · 7 sliders below | Sex · build preset: Slim / Athletic / Curvy-strong · 7 sliders below | A preset sets all sliders; touching a slider makes it "Custom" |
| **Face** | 6 faces (skin tone comes with the face) | 6 faces | Built in the hero lab from strong single presets; light to dark spread |
| **Hair** | 10 hair, 6 beards (incl. stubble, none), 4 brows | 10 hair, 4 brows | Grooms as cards in game; strands only on the stage |
| **Colour** | Hair: 10 swatches from black to platinum and red. Eyes: 8. Brow and lash darkness slider | Same + lip tint 3, eyeliner 3 | Material parameters |
| **Path** | The wheel of twelve (2.4) with mark preview | Same | Sets spawn biome |
| **Name** | 2 to 20 letters, spaces and apostrophes allowed; on a pad, the pad keyboard (2.16) | Same | Unique per PC; worlds key progress by it |

**Body sliders.** Each is 0 to 100 on screen (stored as a byte), default 50 = the build preset. Morph weights are clamped in data (`Data/appearance.json`).

| # | Male slider | What moves (morph pair or bone) | Range at 0 / 100 |
|---|---|---|---|
| 1 | Height | Component uniform scale; the camera follows the head bone | 0.96 / 1.04 (177 to 191 cm from 184) |
| 2 | Muscle | `M_Muscle_Neg` / `M_Muscle_Pos` | Lean (muscularity 0.8) / strong (1.8) |
| 3 | Body fat | `M_Fat_Neg` / `M_Fat_Pos` | Fat −1.2 / −0.3: always fit, never soft-bellied |
| 4 | Shoulders | `M_Shoulder_Neg` / `M_Shoulder_Pos` | Across shoulder −3 cm / +4 cm |
| 5 | Chest | `M_Chest_Neg` / `M_Chest_Pos` | Chest 104 / 116 cm |
| 6 | Waist | `M_Waist_Neg` / `M_Waist_Pos` | Waist 76 / 86 cm (a V-taper of 0.70 to 0.78 holds) |
| 7 | Legs | `M_Thigh_Neg` / `M_Thigh_Pos` | Thigh −3 / +5 cm, calf −1 / +2 cm |

| # | Female slider | What moves | Range at 0 / 100 |
|---|---|---|---|
| 1 | Height | Uniform scale | 0.96 / 1.04 (163 to 177 cm from 170) |
| 2 | Muscle | `F_Muscle_Neg` / `F_Muscle_Pos` | Muscularity 0.2 / 1.0 |
| 3 | Softness | `F_Fat_Neg` / `F_Fat_Pos` | Fat −0.8 / 0.0 |
| 4 | Bust | `F_Bust_Neg` / `F_Bust_Pos` | Chest 86 / 102 cm (natural shape, sag-free at the top) |
| 5 | Waist | `F_Waist_Neg` / `F_Waist_Pos` | Waist 60 / 70 cm |
| 6 | Hips | `F_Hip_Neg` / `F_Hip_Pos` | Hip 94 / 106 cm (the waist-to-hip ratio stays 0.62 to 0.72) |
| 7 | Thighs and glutes | `F_Thigh_Neg` / `F_Thigh_Pos` | Thigh 52 / 62 cm, glute volume −/+ |

- **Why morphs, not Mutable or the MetaHuman body tool:** MetaHuman's parametric body works in the editor only. Mutable is the supported runtime route but has reported MetaHuman reshape bugs and adds a big runtime system. Morph pairs baked from the editor's own parametric bodies give the same shapes at almost no runtime cost. Mutable stays the step-up once there are more than about 40 garments (HERO-RESEARCH 3).
- **Skeleton:** bone lengths don't change, so shoulders and hips widen by morph only. Slider ranges stay inside what reads right on the shared skeleton; the clamps above are set to that.
- **Clothes follow:** every garment and armour mesh carries the same 14 morph names, baked by transfer (Plan 3.3). A garment without them falls back to its neutral shape and logs a warning. That's acceptable only for helmets, gloves and boots, which don't use the body sliders.

**Buttons on every tab:** **Random** (X or the dice icon; random face, hair and colour, sliders inside 30 to 70), **Undo** (Ctrl+Z, or LT tap in the creator), **Done** (Menu / the Done button), **Back** (B / Esc; asks "Leave without saving?" if anything changed).

**Re-edit in the world** (GAP-COOKED): a craftable **Mirror** (workbench, decor; 4 wood, 2 copper ingot, 1 glass or polished obsidian in Fire / clear ice block in Frost). E on it opens the creator in place, over the game with the world blurred. Body, face, hair and colour can change; name and path can't. It's free.

**Replication.** `FWRAppearance` (sex 1 byte, face 1, hair 1, beard 1, brows 1, 4 colour bytes, lip 1, liner 1, 7 sliders = 18 bytes) sits on `AWRPlayerState`, replicated to everyone, sent in the join URL as base64 (`?WRLook=`) and saved by the host with the character (`Saved/<world>/players/<name>.json`). The host checks every byte against the ranges in `appearance.json` and clamps anything outside.

### 2.6 The first hour: onboarding through play

**SPEC beats** (FULL-GAME 2, all BUILT): spawn by main path at one of three outer-coast camps, which are safe zones; bare-hand gathering; crude tools by hand; stone tools at the workbench; light hunger and thirst; day and night; easy creatures at the coast; claim totem, Hearth and bedroll; the death map; hints and moments. Lore through places only.

**GAP-COOKED: how the hour teaches.** Three tools only:
1. **Placement:** the camp kit puts each lesson in reach in the order you need it.
2. **Verb chips:** a glyph plus 1 to 4 words, pinned to the object. This replaces Raven's sentence hints (the same hint ids and once-per-player saves).
3. **Notes from "the last one here":** message stones. The world tells the story.

No pop-up windows, no quest log, no arrows, no voice-over.

#### 2.6.1 Waking

- **New character:** you wake **lying on the camp's bedroll**. A 1.6 s get-up clip plays (a real mocap get-up, retargeted). Any stick or button input after 0.6 s blends straight into control. The camera starts in your chosen view. The HUD fades in over 1.0 s, **vitals only**: the compass, hotbar and the rest appear the first time each is needed (2.7 "first-need reveal").
- **Every later spawn** (respawn, rejoin) skips the get-up unless you wake at a bed, where it's a 0.8 s sit-up.
- **Time of day isn't forced**, since the world clock is shared. If a new character spawns between 18:00 and 05:00 game time, the camp's firepit is already lit (a "last one here" fire, 20 game minutes), so the first minutes are never pitch black.

#### 2.6.2 The spawn camp kit (template for all 36 camps)

Each spawn camp is a **25 m teaching circle** inside its safe zone. The same 14 pieces, in the same relationships, re-skinned per biome. Fire and Frost instances are below; each later biome cook fills its own row from this template (the template goes in `Data/camp_kits.json`).

| # | Piece | Placement | Teaches | Verb chip (pad / KB) |
|---|---|---|---|---|
| 1 | Bedroll (camp-owned, your first respawn point until you place your own) | Centre, under a broken lean-to frame | Respawn | none (the death map shows it) |
| 2 | Message stone: "the last one here" | 2 m from the bedroll, in your first view | The hour's plan | **[X] Read** |
| 3 | Cold firepit, stone ring | 3 m | Fire later; a lit one at night | **[X] Light** (once you have a torch or fiber + flint) |
| 4 | Shrub patch: 5 fiber shrubs | 4 to 7 m, in front of the view | Bare-hand gathering | **[RB] Gather** |
| 5 | Loose stones: 6 pick-ups | Along the shore, 5 to 10 m | Picking up | **[X] Take** |
| 6 | A fallen dead tree | 8 m, a log you can gather from by hand | Wood without a tool | **[RB] Gather** |
| 7 | Flint knapping rock (a flat anvil stone with flakes around) | 6 m | Hand crafting (crude hatchet and pick are `hand` recipes) | **[Menu] Craft by hand** the first time your bag holds a full recipe |
| 8 | Standing trees, 3 | 12 to 20 m | Chopping with the hatchet | **[RB] Chop** (only once a hatchet is in hand) |
| 9 | Berry bush, 2 | 10 m | Eating | **[Y] Eat** once berries are on your belt |
| 10 | Water: the river or a spring | Within 60 m, in sight from the bedroll | Drinking and the waterskin | **[X] Drink** |
| 11 | Pumicehogs / Snowpuffs, 3 | They graze on the camp's edge, 15 to 30 m | Hunting and butchering | **[RB] Harvest** on the body |
| 12 | A ruined claim totem and half a palisade | 60 to 120 m inland, along footprints | Claims and building (it's theirs, failed) | none: the totem's plaque reads "Mine. Kept the dogs out three nights." |
| 13 | The last one's end | 150 to 220 m up the ridge: a torn bedroll, a broken spear, bones, and large clawed tracks leading toward the den | Danger at night; where the pack comes from | none |
| 14 | The den | 220 to 320 m from camp, the biome's pack hunter (Fire: Cinderfang den in a lava-tube mouth; Frost: Gloomhowl den in an ice cave) | The first night's fight | none |

**Message stone text** (one per camp, rotated from three versions per biome so friends at different camps read different notes). Fire, version A:

> Fiber from the brush. Stones on the shore. Knap a hatchet before dark.
> The hounds come down the ridge at night. I didn't build the wall in time.
> Sleep on the roll and you'll wake here.
> — the last one here

Frost, version A:

> Lichen on the rocks feeds the big shaggy ones. Leave them be.
> The pale ones hunt by sound. Stop running and they lose you.
> Fire first. Then a roof. Then a wall.
> — the last one here

Versions B and C change the details (a different name scratched under the dash, a different warning: the lava's warmth at night; the slick ice on the river). Every biome cook writes its three.

#### 2.6.3 Verb chips (replace the sentence hints)

- **Look:**
  - A small stone-and-leather chip, **3D-anchored** 30 cm above the object's top.
  - The button glyph (pad or key, whichever device was used last), then **1 to 4 words**.
  - 20 px text at Desk, 28 at Couch.
  - It fades in over 0.15 s once you're inside 4 m and looking within 20° of the object, and fades out 1.5 s after the first use.
- **Rule:** at most one chip on screen (CAL L6). Each chip id shows until you've done it **3 times**, then never again for that character (saved per character, as hints are today). Settings → Interface → Hints off hides them all.
- **Raven's sentence hints** (`WRUIFlow`) are cut to chips with the same ids. The two that teach a rule rather than a verb keep one short line in the moments layer:
  - "night": "Night. The hunters are out."
  - "carry": "Too heavy: you're slowed."

| Id (built) | Chip (pad / KB shown by device) |
|---|---|
| gather | [RB / LMB] Gather |
| tool | [D→ hold / 2] Take out the hatchet |
| body | [RB / LMB] Harvest |
| station | [Menu / J] Craft here |
| tame | [X / E] Feed |
| hearth | [Menu / B] Build a Hearth |
| ability | [D↑ / Q hold] Cast |
| night | moments line: "Night. The hunters are out." |
| carry | moments line: "Too heavy: you're slowed." |
| new: read | [X / E] Read |
| new: craft_hand | [Menu / J] Craft by hand |
| new: eat | [Y / F] Eat |
| new: drink | [X / E] Drink |
| new: lock | [R3 / Tab] Lock on (the first time an aggressive creature targets you) |
| new: parry | none. Parry is learned by fighting; the F1 page lists it |

#### 2.6.4 The hour, beat by beat (target times for a first-time player)

| Time | What happens | What teaches it |
|---|---|---|
| 0:00 | Wake on the bedroll, read the stone | Kit 1, 2 |
| 0:01 to 0:06 | Fiber, stones, dead wood by hand | Kit 4, 5, 6 + chips |
| 0:06 to 0:10 | Crude hatchet and pick by hand at the knapping rock | Kit 7 + the "craft by hand" chip opens the Crafting tab on the hand recipes |
| 0:10 to 0:18 | Chop trees, mine rock, build a workbench and a lean-to (build bar first reveal) | Kit 8 + the build bar opening on the "Survival" category |
| 0:18 to 0:25 | Eat berries, drink, fill the waterskin; hunt a grazer and butcher it | Kit 9, 10, 11 |
| 0:25 to 0:35 | Follow the footprints to the ruined claim; plant your own claim totem and a Hearth | Kit 12 + the "hearth" chip |
| 0:35 to 0:50 | First fight: the den's pack. At night they come toward camp; by day you can walk up and pick a fight | Kit 13, 14. The pack has no attacker limit; you learn dodge, parry and to drink when they back off. Death sends you to the bedroll |
| 0:50 to 1:00 | Tame an Ashstrider / Frostantler (Ashstrider herds graze 300 to 500 m from Fire camps; Frostantler herds the same in Frost), ride toward the pass, see the boss's landmark (Vulkarn's crater smoke / Skaldrith's glacier) | Placement: the herd sits between camp and the landmark's sightline |

**The first night (GAP-COOKED, no helper):**
- The den's pack runs the built night-hunt behaviour.
- The kit puts the den close enough (220 to 320 m) that its patrol comes within sight of camp on the first night.
- The pack doesn't enter the safe zone, but it does hunt the circle's edge, so leaving camp at night means a fight.
- Nothing is scaled, slowed or held back for new players.

### 2.7 HUD

**SPEC (BUILT `WRUIHud`):** compass, clock, level and statuses, vitals, item hotbar, ability keys, your tame, taming card, prompts, pickups, chat, boss bar, help.

**GAP-COOKED layout** (1080p units; the safe area is 5% at Couch, 2% at Desk):

| Area | Element | Shows | Rule |
|---|---|---|---|
| Top-left | **Vitals:** Health (crimson-red bar), Stamina (green-gold), then the main path bar (Heat ember / Chill ice) and the subclass bar | Values plus a white "loss trail" that drains 0.4 s after a hit | Bar length grows with max value, ER-style, capped at 520 px |
| Top-left under vitals | **Status buildups** (combat.md 2.11): one small bar per building status with its icon; a proc flashes the icon and holds it with a timer | Only while building or active | |
| Top-left under vitals | **Hunger and thirst:** two small arcs | Only below 50% (light pressure) | Pulse below 15% |
| Top-centre | **Compass** strip (built): N/E/S/W, your markers, discovered places within 400 m, tames | | Hides with the HUD mode |
| Top-right | Clock and day, the claim you're in, the raid tracker (built) | | |
| Bottom-left | **Equipment cross** (ER-style): up = quick slot (Wildbrand/spell, its cost), left = ranged/off-hand, right = held weapon or tool with durability, down = belt item with count | Glyphs by device | New. KB players see the keys on the cross too |
| Bottom-centre | **Item hotbar** (built, 8 slots) | KB: always. Pad: 4 s after a change, then fades | |
| Bottom-centre above hotbar | Boss bar(s) (built): name, health, phase pips; a stance bar under the boss's health (combat.md 2.10) | During a boss fight | Two bosses = two bars |
| Over the lock target | Lock dot, target health and stance bar | While locked | Small; never over the tell (CAL L6) |
| Bottom-right | Tame card (built): portrait, health, order | When you have a tame out | |
| Right edge | Pickups (built): "+3 Fiber" lines | 3 s each, 5 max | |
| Centre | Crosshair dot (built setting); verb chips (2.6.3) | | One prompt max |
| Lower-left above cross | Chat (built) | Fades 8 s after the last line | |
| Over creatures | Damage numbers beside the creature, never over its tell (CAL L6) | Option, default on | |

**HUD modes** (Settings → Interface):

| Mode | What shows |
|---|---|
| **Full** | Everything always |
| **Fade** (default) | Vitals, cross, compass and hotbar fade to 0% after 6 s when all of these hold: out of combat, health ≥ 99%, stamina full, no menu change, no new item. They return at once (0.1 s) on any change, a hit, drawing a weapon, or holding LB/RMB |
| **Minimal** | Only health while below 100%, boss bars, chips, and the lock target. Compass off. For players who know the game |

**First-need reveal** (new character only): the compass appears the first time you leave the safe zone, the hotbar on your first item, the cross on your first weapon or tool, the path bar on your first path XP, and hunger/thirst on their first drop below 50%. One-time, saved per character.

### 2.8 Journal shell

**SPEC:** one stone window, leather tab strip, tabs Inventory, Crafting, Paths, Skills, Map, Clan, each with its own key; LB/RB on pad (C1).

**GAP-COOKED:**
- **A seventh tab: System** (gear icon at the strip's end). On a pad this is the only route to settings, so it's the pause-menu equivalent. Big buttons: **Settings**, **Controls (F1)**, **Report a problem (F8)**, **Leave to main menu**, **Quit to desktop**. Leave and Quit ask once: "Leave? You're in a safe zone." or, outside one, "Leave? You're not in a safe zone."
- **Opening:** Menu on pad opens the last tab used; I/J/B/P/K/M/O open their own tab. The same key again closes.
- **The world keeps running.** No pause, even solo, since it's a listen server. The journal dims the world 40% and keeps your vitals visible top-left, so you see a hit coming in.
- **Sub-tabs** on LT / RT (2.1).

### 2.9 Inventory

**SPEC:** worn and held slots, 40-slot bag, details panel, hotbar strip; server commands for slot, unequip, move, split, drop; storage screen shares the parts.

**GAP-COOKED:**
- **Context menu** on X (pad) or right-click, on any item: **Use / Equip / Put on hotbar ▸ 1-8 / Put on belt ▸ 1-4 / Split / Drop / Drop all**. It's one list, so every action has a pad route.
- **Sort** button (and R3 on pad while in the bag): by kind (weapons, tools, armour, consumables, materials, placeables), then by tier, then by name.
- **Quick move** on a pad with the storage screen open: A on an item moves the whole stack to the other side. X opens the context menu, which adds "Move some…".
- **Weight line** under the bag: "38 / 60 kg". It turns amber above 85% and red when over-encumbered (built rule).
- **Compare:** hovering a weapon or armour piece shows its numbers beside the equipped one's, with green and red differences.

### 2.10 Crafting

**SPEC:** the station you stand at plus stations around, categories, recipe cards (ready, short, elsewhere/higher tier), details with quality odds and infusion, quantity, craft queues that run while away, repairs and upgrades.

**GAP-COOKED:**
- **Pulls from nearby storage.** A station counts the ingredients in storage chests **within 20 m in the same claim** that you may open (yours or your clan's). The card shows "8 / 8 (5 in bag, 3 in chests)". Crafting takes from the bag first, then the nearest chest. Enshrouded does this, and it removes most inventory juggling. It's a server rule: `tuning.crafting.nearbyStorageRadius` 2000 cm; 0 turns it off.
- **Track a recipe:** a pin button on the details panel (click, or A on it; also in the X context menu). One recipe at a time is pinned to the HUD, top-right under the clock, as a checklist ("Copper ingot 3/8 · Charcoal 2/4"). It ticks as you gather and clears when you craft it.
- **Quantity:** D-pad left/right ±1, LT/RT ±10, and **Max**. The max respects the bag and the nearby chests.
- **Hand crafting from anywhere:** with no station near, the tab opens on the "By hand" category (built `hand` recipes), so the first-hour chip lands on the right page.

### 2.11 Build bar

**SPEC:** categories, piece cards with cost against your bag, details (cost, upkeep), placing with the ghost, a chip that says whether it will go and why not, the claim you stand in, and demolish mode. Snap and freeform (DR #28).

**GAP-COOKED:**
- The full pad build mode (2.1).
- **Costs also count nearby storage**, as in crafting, inside your claim.
- **"Survival" category first** for new characters (workbench, lean-to, bedroll, campfire, claim totem). After the first claim, the bar opens on the last category used.
- **The placement chip** gives the reason in 1 to 4 words: "Needs support", "Outside your claim", "Too steep", "Blocked", "Missing 2 Wood".
- **Copy** (X / middle mouse in build mode) picks the piece you're looking at, with its rotation.

### 2.12 Map

**SPEC:** the parchment map with you, your heading, places (camps, caves, bosses, passes), Hearth, tames, claims, players; legend filters; wheel zoom; drag to pan; click for details; the death screen's wake map. RAVEN-PLANNED: a fill-in map you complete by exploring; shrine fast travel (DD World).

**GAP-COOKED:**
- **Reveal (fill-in):**
  - A new character's map is blank parchment with only the **coastline** and the **central massif** drawn faintly. Everyone knows the land's outline.
  - Terrain ink appears in a **150 m radius** around everywhere you've walked, ridden or flown, with a soft 30 m edge.
  - Places (camps, caves, shrines, boss arenas, ruins) are added when you come **within 60 m**. Large landmarks (a volcano crater, a glacier, a sky island) are added when they've been **in view inside 800 m** for 2 s.
  - **Shrines reveal their sub-zone**: cleansing or claiming a shrine (DD) inks the whole sub-zone around it. This is ER's map fragment, tied to Raven's shrines.
  - Stored per character per world as a 1-bit grid at 4 m (1009 × 1009 = 127 KB raw, RLE about 5 to 20 KB). It's kept by the host in the character save, so it survives a new PC.
- **Clan map sharing:** none automatic. A **Map table** decor piece (workbench; 6 wood, 2 hide, 1 charcoal) merges every clanmate's reveal when they use it, Valheim's cartography table. The table stays as a piece: no dead state.
- **Pins:**
  - Up to 30 per character, 6 glyphs (camp, ore, danger, creature, loot, flag), each with an optional 24-character label.
  - Placed with X on pad or right-click; removed with X/right-click on the pin.
  - A pin can be **shared with clan** (toggle in its panel).
  - The compass shows pins within 400 m.
- **Shrine travel** (with the fire-frost lane's shrine framework):
  - Standing at a shrine, the map opens in **Travel** mode. Discovered shrines glow; click or A travels.
  - Shows "Travel to The Glass Steps? 6 s" and a confirm.
  - Not allowed while carrying more than the encumbrance limit, or in combat (hit or hitting in the last 10 s): the button reads why.
- **Pad:** the cursor on the left stick (C1), the right stick zooms (scroll), A selects or travels, X pin, LT/RT legend filter pages, R3 recentres on you.

### 2.13 Wheels

**SPEC:** two ability wheels (Q/Z), tame order wheel (hold E on your tame), eight carved wedges, a centre disc saying what it does.

**GAP-COOKED:**

| Wheel | Open | Wedges | Pick |
|---|---|---|---|
| Ability 1 / 2 | Hold Q / Z (KB); on pad the D-pad quick slots replace them (built PadQuickWheel) | 4 each = 8 quick slots | Release over a wedge (cast-on-release setting, built) |
| Tame orders | Hold E / X on your tame | Follow, Stay, Guard, Attack my target, Passive, Neutral, Aggressive, Rename | Release / click |
| **Whistle** (new; the R0 "tame whistle screen") | Hold E / X looking at nothing | Same orders, to **every tame of yours within 30 m** (`WRTameAll`, built) | Release |
| **Items** (new; pad's route to the hotbar) | Hold D-pad right | The 8 hotbar slots | Release |

All wheels: the world keeps running, the right stick or mouse picks (the stick needs past 0.5 to light a wedge), centre = cancel, held inputs drop on open and close (C1 isolation).

### 2.14 Settings

**SPEC (built pages):**
- **Controls:** keys, cast mode, cast on release, crosshair, reset.
- **Video:** overall quality, window mode, resolution, 3D scale, frame cap, VSync, and per-group view distance, shadows, GI, reflections, AA, textures, foliage, post processing; auto-detect.
- **Audio:** master, music, effects, ambient, UI.
- **World** (host only): presets and rules.

**GAP-COOKED:** two new pages and the presets.

**Look & controller** (new page; the key-binding struct already holds sensitivity, invert, glyph set and rumble):

| Setting | Range | Default |
|---|---|---|
| Mouse sensitivity | 0.10 to 5.00 (log slider) | 1.00 |
| Mouse aim sensitivity (bow, crossbow, throwing) | 0.2 to 1.5 × | 0.7 × |
| Invert mouse Y | on / off | off |
| Pad look sensitivity | 0.10 to 3.00 | 1.00 |
| Pad aim sensitivity | 0.2 to 1.5 × | 0.6 × |
| Invert pad Y / invert pad X | on / off | off / off |
| Pad inner dead zone / outer dead zone | 0.05 to 0.35 / 0.80 to 1.00 | 0.15 / 0.95 |
| Response curve | Linear (1.0) / Default (1.8) / Steep (2.5) | Default |
| Lock-on switch flick strength | 0.4 to 0.9 stick | 0.6 |
| Rumble | off / 25 / 50 / 75 / 100% | 100% |
| Button icons | Auto / Xbox / PlayStation | Auto |
| Sprint / guard / aim / crouch | Hold / Toggle each | Hold, Hold, Hold, Toggle |
| **Pad buttons** | The full remap list for the pad (the same list as keys; a clash asks "Replace?") | `gamepad.json` |

**Camera** (same page, second column):

| Setting | Range | Default |
|---|---|---|
| Field of view (first person) | 75 to 110° | 90° |
| Field of view (third person) | 70 to 100° | 80° |
| Camera shake (hit trauma) | 0 to 100% | 100% |
| Head bob in first person | 0 to 100% | 50% |
| Motion blur | off / low / high | off |
| Third-person camera side | right / left / centre | right |

**Interface** (new page):

| Setting | Values | Default |
|---|---|---|
| Setup | **Desk** (HUD 100%, text 18 px body) / **Couch** (HUD 115%, text 26 px body; XAG 101) | Desk |
| HUD scale | 75 to 130% | from Setup |
| Text size | 16 to 32 px body | from Setup |
| HUD mode | Full / Fade / Minimal | Fade |
| Damage numbers | on / off | on |
| Compass | on / off | on |
| Hints (verb chips) | on / off | on |
| Crosshair | always / aiming only | always (built) |
| Chat | everyone / clan only / off | everyone |
| Show other players' names | always / near (40 m) / off | near |
| Subtitles for in-world notes read aloud | n/a (there's no voice) | none |

**Audio** additions: **Output device** (Windows default or a named device), **Mono** on/off, and **Music in combat** (on, ducks ≤ 4 dB, which is the bar / off). Music is never off by default.

**Graphics presets** (`Config/DefaultScalability.ini`, ours; GAP-COOKED). Targets: Low, 60 fps 1080p on a 4 GB GTX 1650-class card; **Medium, 60 fps 1080p on an 8 GB RTX 3060 / 4060 / RX 6600**, 1440p with TSR at 67%; High, 60 fps 1440p on 12 GB; Epic, Raven's current look.

| Group | Low | **Medium (8 GB)** | High | Epic |
|---|---|---|---|---|
| Resolution scale (TSR) | 50% | 67% at 1440p, 77% at 1080p | 77% | 100% |
| Global illumination | Lumen off, sky light + SSAO | **Lumen GI on, cheap:** `r.Lumen.ScreenProbeGather.DownsampleFactor 32`, `r.Lumen.TraceMeshSDFs 0` (global SDF only), `r.Lumen.ScreenProbeGather.TracingOctahedronResolution 6` | Lumen GI, DownsampleFactor 16 | Lumen GI full |
| Reflections | SSR off | **SSR** (Lumen reflections off) | Lumen reflections, `MaxRoughnessToTrace 0.4` | Lumen reflections full |
| Shadows | Shadow maps, 2 cascades, 1024 | **VSM**, `r.Shadow.Virtual.ResolutionLodBiasDirectional 1.0`, `MaxPhysicalPages 2048` | VSM bias 0.5, 4096 pages | VSM full |
| Textures | pool 1000 MB | **pool 2000 MB**; `r.Nanite.Streaming.StreamingPoolSize 384` (set by hand, not by scalability) | pool 3000, Nanite 512 | pool 4500, Nanite 768 |
| View distance | 0.6 | 0.8 | 1.0 | 1.0 |
| Foliage | 0.4 density, 120 m | 0.7, 180 m | 1.0, 250 m | 1.0, 250 m |
| Post processing | no bloom, no DOF | bloom, no DOF | all | all |
| Effects | low | medium | high | high |
| Hair (MetaHuman cards) | LOD3 cards | LOD1 cards for you, LOD3 others | LOD0 you, LOD1 others under 8 m | same |

**Auto-detect:**
- It reads the GPU's dedicated VRAM (DXGI adapter desc) and runs the engine's hardware benchmark.
- It picks the **lower** of the two answers: VRAM ≤ 6 GB → Low; 8 GB → **Medium**; 10 to 12 GB → High; 16 GB and up → Epic.
- On first run, it shows: "Medium suits this PC (8 GB graphics card). Change it any time."

### 2.15 F1 controls page

**SPEC (C1):** F1 anywhere, or Y in menus on a pad. It lists every action and its key or button for the device in use, from `gamepad.json`'s `page` groups and `FWRKeyBindings`.

**GAP-COOKED:**
- **Pad view:** a drawn controller diagram (Xbox or PlayStation, by glyph setting) with callout lines to every button. **Three pages** on LB / RB: **In play**, **Menus**, **Building**. Holds are listed as "hold" under the tap verb.
- **Keyboard view:** two columns grouped Movement, Combat, Items, Menus, Building, with a **Change keys** button that opens Settings → Controls on the key.
- Opening it from a screen shows that screen's page first (in build mode: Building).
- It closes with F1 again, B, Esc, or Y. Opening it drops held inputs (C1).
- Text 22 px minimum. No paragraphs: verb, button, nothing else.

### 2.16 F8 bug report

**SPEC (C1, `WRReport.h`), restated:**
- F8, or View on a pad, from anywhere, the main menu included.
- It freezes the frame (world and UI) **before** the panel opens.
- It gathers: version, map, net mode, position and facing, health and the player's numbers, open screen, device and pad, fps, resolution and quality, time played, and a **60 s ring** of events, notices and warnings.
- **Kinds:** Bug, Stuck, Controls, Camera, Music-Audio, Visual, Balance, Idea.
- **Send:**
  - POSTs to `https://freedom35studios.pages.dev/api/feedback` with `game: "wildreach"`.
  - Saves `Saved/Reports/<time>_<player>.json` + `.jpg`.
  - Queues in `Saved/Reports/Outbox` and retries every 30 s and at the next start.
  - A client also sends a copy to the host.

**GAP-COOKED:**

| Addition | Design |
|---|---|
| **Report ID on send** | The server answers with an id. The toast reads "Sent · WR-1234 · Thanks". Offline: "Saved · sends when online". The id is stored in the local json so a tester can quote it |
| **Panel layout** | Left 60%: the frozen frame with the moment's position and time burned in the corner. Right 40%: the 8 kinds as big tiles (2 × 4, icon + word), a note box, **quick chips**, Send, Cancel. Send works with no note (a kind is enough) |
| **Quick chips** (tap to add a line to the note) | "Just happened" · "Happens every time" · "Hit didn't land" · "Got hit by nothing" · "Can't move" · "Fell through" · "Too hard" · "Too easy" · "Looks wrong" · "Sounds wrong" · "Music stopped" · "Love this" |
| **Pad keyboard** | `SWRPadKeyboard`: a QWERTY grid under the note box. A types, X backspace, LB/RB move the caret, LT shift, RT space, Menu = Send. It's shared with chat, creator name, world rename and pins |
| **Ring contents** (what `WRReport::Event` is called with, at least) | Device switch; screen open/close; mode transitions; every hit you take or deal (attacker, attack id, damage, blocked/parried, stance); deaths and their cause; level-ups and node learns; crafts; build placements and demolishes; tame orders; fps under 30 for more than 1 s; net hitches over 250 ms; position every 5 s; warnings and errors |
| **Crash card** | On start, if `Saved/Crashes/` holds a crash newer than the last clean exit (a `Saved/Wildreach/clean_exit` stamp written on normal quit), the title shows the crash card (2.4). Send POSTs kind "Crash" with the crash's `CrashContext.runtime-xml`, the last 400 log lines, and the machine's GPU, driver and RAM. It does not include the minidump (too big). One per crash |
| **Panel while in a fight** | The game doesn't pause (multiplayer). The panel takes input; your character stands still. On a solo or listen host with no other players, the world pauses for the panel (`SetGamePaused`), so a report never kills you |

### 2.17 Flow screens

**SPEC:** loading screen, death screen (map of where to wake, 10 s), moments (banners, toasts, boss cards), fades.

**GAP-COOKED:**
- **Disconnect screen:** "Lost the host." with the reason (timeout / host closed / kicked / version), and buttons **Rejoin** (same IP, keeps trying for 60 s with a counter) and **Main menu**. Pad A and B.
- **Chat on pad:** a **Chat** button in the journal's System tab opens the pad keyboard over the chat line. Incoming chat shows on every device (built).
- **Loading screen:** keep the built art. Add **the next place's name** ("The Volcanic coast") and one line from a found note you've read (only notes already read, so no spoilers), instead of a tip.

### 2.18 Look and text rules for every screen

- Raven's stone-and-leather kit stays. Every new screen uses `WRUIStyle` and `WRUIWidgets`.
- **Colour law** (CAL 1.13): gold means yours / parry / confirm; crimson means unblockable and danger. Neither colour is used for decoration on any screen.
- **Text:** 18 px body minimum at Desk and 26 px at Couch (XAG 101); 1 to 4 words on chips and buttons; no paragraph longer than 2 lines anywhere outside notes and item lore.
- **Every screen has:**
  - a visible close or back button;
  - a pad route to every control (D-pad navigation, C1);
  - the device's glyph on its footer actions.
- **No dead states:** a disabled button always says why on hover or focus ("Needs Hearth level 2").
- **People:** the creator presets, the portraits and the loading art show attractive, real-world-sexy men and women, never sanitized. The survivor wraps show skin as HERO-RESEARCH sets out.

---

## 3. Plan

### 3.1 Data (Raven's pattern: numbers and content in `Data/`, read at runtime)

| File | New or changed | Fields |
|---|---|---|
| `Data/gamepad.json` (C1) | Changed | Add verbs `ItemWheel` (DPad_Right hold), `Crouch` (L3 tap), `ViewHold` (L3 hold), `Whistle` (X hold at nothing), `TwoHand`, `SwapRanged` holds; menu verbs `SubPrev` LT / `SubNext` RT; build verbs `BuildPlace` RT, `BuildRotL` LB, `BuildRotR` RB, `BuildPrev` DPad_Left, `BuildNext` DPad_Right, `BuildCat` DPad_Up, `BuildCopy` X; `holds: {verb: seconds}`; `page` groups In play / Menus / Building |
| `Data/appearance.json` | New | `sexes[m,f]`: `faces[6]{id, mhAsset, skinTone}`, `builds[3]{name, sliders[7]}`, `hair[10]`, `beards[6]`, `brows[4]`, `hairColours[10]{melanin, redness}`, `eyeColours[8]`, `lips[3]`, `liners[3]`, `sliders[7]{id, label, morphNeg, morphPos, minWeight, maxWeight}` (height: `scaleMin 0.96, scaleMax 1.04`) |
| `Data/camp_kits.json` | New | `template[14]{piece, mesh, offsetRange, facing}`, `biomes{volcanic, tundra}{pieces overrides, packCreature, denMesh, notes[3]}`. Read by `Tools/terrain/dressing.mjs`, which writes the kits into `island_dressing.json` at the 6 built camps |
| `Data/notes.json` | New | `{id, biome, text, author, placeTag}` for message stones (camp notes now; DD's player messages later use the same actor) |
| `Data/hints.json` | New (moves hint text out of C++) | `{id, verbPad, verbKey, words, kind: chip / line, showCount: 3}` |
| `Data/tuning.json` | Changed | `input{holdSeconds 0.40, longHoldSeconds 0.50, mouseDeviceThresholdPx 6}`; `hud{fadeSeconds 6, fadeBackSeconds 0.1, lossTrailDelay 0.4, barMaxPx 520, chipRange 400, chipAngle 20}`; `creator{orbitYaw 170, framings{full 320, upper 140, face 55}}`; `map{revealRadius 15000, revealEdge 3000, placeReveal 6000, landmarkReveal 80000, landmarkSeconds 2, pinMax 30}`; `crafting{nearbyStorageRadius 2000}`; `onboarding{getUpSeconds 1.6, getUpCancelAfter 0.6, nightFireMinutes 20}`; `report{clientPause true}` |
| `Data/items.json`, `recipes.json`, `pieces.json` | Changed | `mirror` and `map_table` pieces and their recipes (2.5, 2.12) |
| `Config/DefaultScalability.ini` | New | The table in 2.14, as `[GlobalIlluminationQuality@0..3]`, `[ReflectionQuality@..]`, `[ShadowQuality@..]`, `[TextureQuality@..]`, `[ViewDistanceQuality@..]`, `[FoliageQuality@..]`, `[PostProcessQuality@..]`, `[EffectsQuality@..]` |
| `Config/DefaultGame.ini` | Changed | `DirectoriesToAlwaysCook` for `/Game/Front` and `/Game/Characters/Hero` |

### 3.2 C++ (Raven's patterns: Slate screens registered with `WR_REGISTER_SCREEN` / `WR_REGISTER_JOURNAL_TAB`, server commands for every change, all damage through `ApplyHit` (nothing here deals damage), numbers via `WRTune`)

| Class / file | Change |
|---|---|
| `WRPadInput.cpp` (C1) | Hold detection per verb from `gamepad.json` `holds` (tap on release before hold time when a hold exists); `SubPrev`/`SubNext`; build-mode verbs; pad remap save (`[Wildreach.Pad]` in GameUserSettings); the mouse-move device threshold |
| `WRKeyBindings.cpp` | New actions `Crouch` (LeftControl), `UseBelt` (F), `BeltNext` (X), `TwoHand` (H), `BuildRotL/R`, `BuildCopy`; hold/toggle flags; FOV, shake, head bob, aim sensitivity, dead zones, curve, and camera side fields. Remove the "no mouse sensitivity" line from `WRUISettings.cpp`'s header |
| `WRUISettings.cpp` | New pages `look` (Look & controller + Camera) and `interface`; audio additions; preset buttons Low/Medium/High/Epic/Custom; `AutoDetect()` takes min(VRAM rule, benchmark). VRAM from `FPlatformMisc`/RHI `GRHIAdapterDedicatedMemory` (RHI global; check the 5.8 name) |
| `WRMainMenu.cpp` | Split into the title screen (`SWRTitle`), characters screen (`SWRCharacters`, `WRUICharacters.cpp`), crash card; tagline swap by built-biome count |
| `WRCharacterProfiles.h/.cpp` (new) | Load/save `Saved/Wildreach/Characters/*.json` + portrait PNG; the active profile goes in the join URL (`?WRName=`, `?WRLook=`) |
| `WRAppearance.h` (new) | `FWRAppearance` (18 bytes, NetSerialize), `ToBase64/FromBase64`, `Clamp(const FWRAppearanceDefs&)` |
| `UWRAppearanceComponent` (new, on `AWRCharacter`) | Applies an appearance: picks face/hair/beard assets, sets `SetMorphTarget` on the body and every follower mesh (the leader-pose components) for the 14 morph names, sets the uniform scale, sets the colour MIDs. Called on `OnRep_Appearance` and when gear changes (new garments get the same morph weights) |
| `AWRPlayerState` | `UPROPERTY(ReplicatedUsing=OnRep_Appearance) FWRAppearance Appearance`; set by the game mode from the URL on login and by the server command `look <base64>` (mirror re-edit); saved in the player json |
| `AWRCreatorStage` (new actor, `L_Front`) | Spawns the preview hero with `UWRAppearanceComponent`, the 3-point light rig, framing cameras with blend (0.35 s), orbit input, turn-around; `RenderPortrait()` to a 512 px render target → PNG |
| `WRUICreator.cpp` (new) | `SWRCreator`: tabs Body, Face, Hair, Colour, Path, Name; slider rows (`SWRSlider` from the widget kit); Random, Undo (a 20-step stack), Done, Back; `SWRPathWheel` (12 medallions, sealed state from `paths/*.json` presence plus a `biomes_built` list in tuning) |
| `WRUIHud.cpp` | HUD modes and fade timer; the equipment cross (`SWRHudCross`); status buildup rows (fed by combat's status component tags); loss trail; first-need reveal flags (per character, in the player json `hudSeen[]`); tracked recipe checklist; Desk/Couch scale |
| `WRUIFlow.cpp` | `ShowHint` gains an anchor actor and renders a **3D-anchored chip** (project to screen, clamp to the safe area) from `hints.json`; show count 3; old sentence texts removed |
| `AWRMessageStone` (new) | A dressing actor with a note id; E / X opens a small parchment panel (`SWRNote`, text 22 px, close on any back). Read state is saved per character. Later (R2) players write their own on the same actor |
| `AWRCharacter` | Get-up on a new character's first spawn (`bFirstSpawn` from the player json): plays the montage, ignores input until 0.6 s, any input blends out |
| `WRCrafting.cpp` (server) | `GatherAvailable(Player, Station)` adds storage pieces within `nearbyStorageRadius` in the same claim the player may open; consumption order bag → nearest chest; the same for build costs in `WRBuilding.cpp` |
| `WRUICrafting.cpp` | Counts "in bag / in chests"; quantity controls; Track button; By-hand default |
| `WRUIInventory.cpp` | Context menu (`SWRContextMenu`), Sort (server command `sort`), weight line, compare panel |
| `WRUIBuild.cpp` | Pad verbs; Survival-first; reason chips (server returns a reason code; map code → 1-4 words in `hints.json`) |
| `WRUIMap.cpp` | Reveal mask texture (`UTexture2D` 1009², R8, updated in 64-px tiles) multiplied over map.png; coastline/massif layer always on; pins (server commands `pin add/remove/share`); Travel mode hook to the shrine framework (fire-frost lane) |
| `UWRMapReveal` (new, server-side per player) | Each 1 s, marks the 4 m cells within 150 m of the pawn into a bitset; sends changed tiles to the owner; saved RLE in the player json; Map table merge command `maptable` |
| `WRUIWheel.cpp` + `FWRPlayerUI` | Whistle wheel and item wheel |
| `WRReport.cpp` / `WRUIReport.cpp` (C1) | ID toast; quick chips; `SWRPadKeyboard`; crash card check (`CheckLastCrash()` at start; `clean_exit` stamp on normal quit); pause on solo; ring events listed in 2.16 (add `WRReport::Event` calls in `UWRCombatLibrary::ApplyHit` (a log call only, no damage logic), death, crafting, building, taming) |
| `WRUIJournal.cpp` | System tab; world dim 40%; vitals kept on |
| `WRFlowDeath.cpp` / `WRUIFlow.cpp` | Disconnect screen with Rejoin; loading line from read notes |

### 3.3 Assets and art route

| Asset | Route |
|---|---|
| **Body morph pairs** (14 per sex) | `WildreachCharLab`: extend `mh_shape.py` to export, per slider, the body at its 0 and 100 values with the preset's other values held, all on the MetaHuman body topology → Blender script `Tools/art/morph_bake.py` subtracts the neutral body, writes shape keys `M_Muscle_Pos` etc. onto the shipped body FBX → reimport with morph targets on |
| **Garment morphs** | `Tools/art/outfit_morphs.py` (Blender): for each garment, Surface Deform bound to the neutral body, then for each body shape key, evaluate and store as a garment shape key of the same name; fix collisions with a 3 mm offset push. Run on survivor wraps first, then every armour set as it's made |
| **Faces, hair, grooms** | The hero lab (HERO-RESEARCH 2); cards LODs for game, strands for the creator stage only |
| **Get-up clip** | A mocap get-up from bedroll (Mixamo "Getting Up" or the GASP pack's standing transitions), retargeted to the MetaHuman skeleton per sex through IK Retargeter with the root motion op |
| **Creator stage and `L_Front`** | Basalt ledge and snow shelf from existing island meshes and the Fab packs already in the game (SimpleRocks, MW landscape material); a 2-sub-level map built by `Tools/make_map.ps1 front` |
| **Message stone** | Concept (a flat waist-high slab with scratched writing and a wrapped cloth tied to it) → Hunyuan3D 2.1 → Blender finish (decimate to 4k tris, writing as a decal) → `Tools/art` import. One Fire variant (basalt), one Frost (pale granite with rime) |
| **Camp kit pieces** | Knapping rock, broken lean-to, ruined totem, torn bedroll, broken spear, bones, the dens' cave mouths: the same AI route; bedroll and firepit reuse built pieces from `pieces.json` |
| **Pad diagrams** (Xbox, PS) | Drawn as SVG in our UI style (stone outline, leather fill) → PNG by `Tools/ui_glyphs.mjs` into `Data/UI/Pad/` |
| **Button glyphs** | Xbox and PS sets as SVG → PNG `Data/UI/Glyphs/pad_*` (same tool) |
| **Path medallions** (12) | 2D carved-stone sigils in the same pipeline as `Data/UI/Glyphs`; sealed variant = the same with a crack overlay |
| **HUD cross, chips, report tiles, pad keyboard** | Slate drawn from the existing kit (`stone.png`, `leather.png`, `rivet.png`), no new textures |
| **Mirror, map table** | Hunyuan route; the mirror's glass is a planar-reflection-free material (a cube-map shine) for cost |

**Sound and music cues** (named events in `Data/Audio/sounds.json`; the music lane writes the music):

| Cue | Use |
|---|---|
| `mus_menu_theme` | Title, characters, creator (always on) |
| `ui_creator_slider` (soft leather creak, pitch by value) | Slider moves |
| `ui_creator_tab` | Tab change |
| `ui_note_open` / `ui_note_close` | Message stone (parchment and stone scrape) |
| `ui_chip_in` (a quiet wood tick) | Verb chip appears |
| `ui_report_freeze` (a shutter click) / `ui_report_sent` | F8 |
| `ui_wheel_open` / `ui_wheel_pick` | Wheels |
| `ui_map_reveal` (pen scratch, once per 10 s at most) | New ink on the map |
| `ui_travel` | Shrine travel |

**Map dressing:** the camp kits at the 6 built camps (3 Fire, 3 Frost) through `dressing.mjs` → `island_dressing.json`; dens 220 to 320 m out; Ashstrider and Frostantler herd spawn points 300 to 500 m out on the line to the boss landmark (`spawn_regions.json` rows).

### 3.4 Build order inside this scope

Each round: build → spot-check → Aaron tests. The only gates are compile, cook and package.

| Round | Builds | Why here |
|---|---|---|
| **U0, proof of one** | One body slider end to end: **female Muscle** baked from the lab → morph pair on the body → survivor wraps with the same pair → `UWRAppearanceComponent` drives it → replicated to a second client → visible on the creator stage with the slider | The riskiest new pipeline (morph bake + garment transfer + replication). If it holds, the other 13 are repetition |
| U1 | Settings: Look & controller, Camera, Interface pages; `DefaultScalability.ini` with the 8 GB Medium; auto-detect by VRAM; pad remap; hold/toggle | Small, high value, every tester feels it at once |
| U2 | Front end: title in `L_Front`, characters screen, profiles, the creator with all tabs and the remaining 13 sliders per sex, path wheel, appearance replication and save; crash card | Needs U0 |
| U3 | HUD: modes, fade, cross, buildup rows, loss trail, first-need reveal, Desk/Couch; verb chips (3D anchored) replacing sentence hints; `hints.json` | Before the first hour, which leans on chips |
| U4 | The first hour: camp kits at the 6 camps, message stones and notes, the get-up, night fire, den placement, herd placement | Needs U3 chips |
| U5 | Journal: System tab, inventory context menu/sort/compare, crafting from nearby storage + tracking + quantity, pad build mode with copy and reasons, item and whistle wheels | |
| U6 | Map: reveal, pins, map table, shrine travel UI (once the fire-frost shrine framework lands) | Depends on shrines for travel; the reveal stands alone |
| U7 | F8 additions: ID toast, chips, pad keyboard (also used by chat and names from U2 on; if U2 lands first, the keyboard moves into U2), ring events, solo pause; disconnect screen; loading line; mirror re-edit | |

Each later biome cook adds: its camp kit row and three notes (`camp_kits.json`, `notes.json`), its path medallion, its creator stage sub-level and its title backdrop sub-level.

---

## Sources

- [Great Plateau, Wikipedia](https://en.wikipedia.org/wiki/Great_Plateau)
- [Game Rant: why the Great Plateau works](https://gamerant.com/zelda-breath-of-the-wild-great-plateau-tutorial-introduction-design-effective/)
- [Utrecht thesis on the BotW tutorial (2025)](https://studenttheses.uu.nl/handle/20.500.12932/46740)
- [Windows Central: Valheim first days](https://www.windowscentral.com/valheim-tips-and-tricks-beginners-guide-your-first-days-and-beyond)
- [Mobalytics: Valheim beginner's guide (Hugin)](https://mobalytics.gg/gamebase/guides/valheim-beginners-guide)
- [Fextralife: Elden Ring controls](https://eldenring.wiki.fextralife.com/Controls)
- [Shacknews: Elden Ring PC keybindings](https://shacknews.com/article/128968/elden-ring-controls-and-pc-keybindings)
- [TechRadar: Ghost of Tsushima interview](https://www.techradar.com/news/ghost-of-tsushima-interview-nature-sprawls-in-ps4s-epic-samurai-swan-song)
- [Android Central: Ghost of Tsushima, no waypoints](https://www.androidcentral.com/ghost-tsushima-creative-director-interview)
- [Destructoid: Monster Hunter Wilds accessibility settings](https://www.destructoid.com/all-accessibility-settings-in-monster-hunter-wilds/)
- [Icy Veins: Monster Hunter Wilds settings](https://www.icy-veins.com/monster-hunter-wilds/news/best-settings-to-improve-your-monster-hunter-wilds-experience/)
- [Game8: Dragon's Dogma 2 open world](https://game8.co/articles/featured/how-dragons-dogma-2-gets-the-open-world-genre-right)
- [Enshrouded wiki: controls](https://enshrouded.wiki.gg/wiki/Controls)
- [Palworld controls](https://www.palworld.tools/controls)
- [Epic: MetaHuman body controls](https://dev.epicgames.com/documentation/metahuman/body-controls)
- [Epic: Mutable sample updated for UE 5.8](https://www.unrealengine.com/news/mutable-sample-for-runtime-character-customization-now-updated-for-ue-5-8)
- [UE forum: editing MetaHumans at runtime (5.6)](https://forums.unrealengine.com/t/how-to-edit-metahumans-at-runtime-using-blueprints-ue-5-6/2565006)
- [Game Developer: in-game bug reporter best practices](https://www.gamedeveloper.com/blogs/in-game-bug-reporter-best-practices)
- [Sea of Thieves: raising bug reports](https://support.seaofthieves.com/articles/26032567156242-Raising-Insider-Retail-Bug-Reports-for-the-Team)
- [Bugnet: in-game reporter design guide](https://bugnet.io/blog/in-game-bug-reporter-design-guide)
- [Xbox Accessibility Guidelines 101 (text)](https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/101)
- [Epic: Lumen performance guide](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine)
- [UE forum: Nanite streaming pool and scalability](https://forums.unrealengine.com/t/scaling-nanite-streamingpoolsize-vram-usage-via-scalability-settings-or-at-runtime/2712963)
- `cooks/HERO-RESEARCH.md` (creator scope, faces, bodies, outfits) and `COMBAT-AND-LESSONS.md` L4, L6, L8, L10, 1.15, 1.16
