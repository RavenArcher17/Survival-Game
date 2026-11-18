# Wildreach cook: COOKPASS-UI (every screen, the HUD, menus, settings, controls: the cook pass on what's built)

Lane cookpass-ui, 2026-11-15, desktop (E:\Dev\Wildreach). A cook pass on the built UI: research how the best games do
it, grade every built piece against THE BAR, design the raise, build it. Spec sources: `cooks/ui-controls.md` (the UI
cook of 11-13; its C1 round is built, its U0-U7 rounds mostly aren't), FULL-GAME 2 / 9, COMBAT-AND-LESSONS L4 / L6 / L8,
1.15 / 1.16, the share's LATER-COOKS lines in this scope.

## 0. Requirement ledger

Split from the brief, the Aaron quotes in it and the LATER-COOKS lines that touch screens, HUD, menus, settings and
controls. "Done where" is filled at the end.

| # | Requirement (one each) | Source | Done where |
|---|---|---|---|
| Q1 | Research how the best games do this (named games, places, systems) and write the details used | brief, Aaron 11-13 | DONE: section 1 (TLOU2, Horizon FW, Ghost of Tsushima, MH Wilds, XAG 101/107, Elden Ring, Valheim, Sea of Thieves / Destiny 2, console keyboards, DD2 / MH / BG3 creators, Enshrouded / BotW wheels, Grounded / Subnautica pins) + 1.2 real-world |
| Q2 | Grade every built piece against THE BAR | brief | DONE: section 2 (grades with file:line of each built piece) |
| Q3 | Design the raise in writing, every detail and edge case, build steps, BEFORE building; commit + push the doc after the plan | brief, Aaron 11-13 | DONE: section 3, committed before any code (showcase-notes af11d13) |
| Q4 | Build exactly the plan at full detail; no placeholders; anything unfinished stays hidden | brief, BAR | DONE: Wildreach eb464d6, 5918156, 0343a21, 4833f2d, a37d79d, 6efb4ea, 661b476, 0007f9b, 147b14e, 483536e, 62b0694, f7dbe00; nothing half-built shown (L7 not built, nothing of it in the game) |
| Q5 | Spot-check the build against the plan and THE BAR by reading the code; fix before pushing | brief | DONE: section 4 (read-through; fixes in f7dbe00, and the chat focus / card nav / tick placement fixes inside parts 3, 4, 10) |
| Q6 | "Already done" only with file:line proof | brief | DONE: every row here carries its file:line |
| Q7 | Design with intention: every element says its job, what it connects to, how it meets what it touches | Aaron 11-14 | DONE: section 3.10; one pref store read where it acts (WRKeyBindings.cpp:208); fade never hides what asks for action (WRUIHud.cpp:390); confirm strip in the footer's place (WRUISettings.cpp:2427); cards where the status line was (WRMainMenu.cpp:450); keyboard docks clear of its box (WRUIPadKeyboard.cpp:330 bDockTop) |
| Q8 | Requirement ledger; final message lists every row with done + where | Aaron 11-14 | DONE: this table |
| Q9 | Data-driven: numbers in Data/tuning.json / Data/*.json, sounds as table rows | BAR | DONE: tuning ui.couchScale / toggle / hud / names / rejoin / padKeyboard / pins / itemWheel / setComplete / reveal, look.padCurve*, report.chips / crashLogLines, flow.lookCamera; gamepad.json reserved + F1 rows; sounds.json gear.setComplete |
| Q10 | Every screen and control with mouse AND pad; no dead states | BAR, ui-controls 2.18 | DONE: every new control is a kit button / segment / switch the pad processor reaches; B handlers on the confirm strip (WRUISettings.cpp:2305), the cards (WRMainMenu.cpp PushPadHandler + nav root), the pin card (WRUIMap.cpp PushPadHandler); text boxes by the pad keyboard (WRPadInput.cpp:592) |
| Q11 | Multiplayer: server authority for anything that changes the world or others see | BAR | DONE: helmet via ServerSetHideHelmet; map reveal server-side with owner tiles (WRMapReveal.cpp:95, ClientTile / ServerSendAll); PreviewLook refuses any non-standalone world (WRHeroLook.cpp:396); pins / track / prefs are this PC's own by design |
| Q12 | Land in pieces: compile, commit + push each part; CHANGELOG (Unreleased) + HANDOFF in the same commit | brief | DONE: 12 commits, each compiled clean (editor target) and pushed; CHANGELOG in each; HANDOFF in parts 1-6 and 8-11 (part 7's note went in with part 8; part 12 is a fix with no new system) |
| Q13 | Unity build: unique prefixes for file-scope helpers | lane note | DONE: helpers in WRKeyPrefs / WRRevealDetail / WRPadKb / WRReportCrash / WRSettingsUI (Pref*) / WRMapTab; the full unity build after the pull compiled with every file back in its blob |
| Q14 | Out-of-scope finds go to the share's LATER-COOKS, one line each | brief | DONE: LATER-COOKS lines appended (section 5) |
| L1 | Settings: first-person FOV slider 75-110 | LATER-COOKS (hero lane) | DONE: Settings > Camera FovFirst 75-110 (WRUISettings.cpp:1572; prefs table WRKeyBindings.cpp; WRRanged.cpp:344) |
| L2 | Settings: hide-helmet toggle | LATER-COOKS (hero lane) | DONE: Settings > Interface Show my helmet (WRUISettings.cpp:1605; ServerSetHideHelmet + SWRMainMenu::SetLook WRMainMenu.cpp:899) |
| L3 | The fill-in map's reveal grid continent-sized at 8 m cells (2,250 x 975); ui-controls owns it | LATER-COOKS (world-map lane) | DONE: UWRMapRevealComponent, 8 m cells over ui.mapMin / mapSize (grows with the continent) WRMapReveal.cpp:71 / :95; save WRGameMode.cpp:560; fog WRUIMap.cpp:261; places once seen WRUIMap.cpp:348 |
| L4 | Pad layout reserves D-pad up hold (ping / quick-chat wheel) and View hold (player list) so nothing else claims them | LATER-COOKS (multiplayer lane) | DONE: Data/gamepad.json:37 "reserved" DPadUpHold / ViewHold |
| L5 | A screen that picks face, build (hair) and the hide-helmet option (runtime ready: `?Look=<face>.<build>[.h]`) | LATER-COOKS (build-hero-look-2) | DONE: main menu Look row + Your look panel (faces by name, build, helmet) WRMainMenu.cpp:960; live preview on the menu survivor WRMainMenu.cpp:1032 + WRHeroLook.cpp:396. Hair / beard / colours / sliders need the creator art (morph bake) and stay with the hero lanes |
| L6 | A set-complete moment: a chime and a glow on the doll's pieces when a 4-piece bonus comes on | LATER-COOKS (build-crafting) | DONE: FWRPlayerUI::TickSetComplete WRPlayerUI.cpp:992; doll flash WRUIInventory.cpp:891; sounds.json gear.setComplete |
| L7 | Remote container screen for a far rift chest (Riftback pocket) | LATER-COOKS (build-biome-void) | NOT BUILT: a far rift chest isn't replicated to the client (net relevancy), so the pocket needs a server relay of the shared rift inventory, not only a screen; the LATER-COOKS line stands with that finding |
| L8 | Necromancy node glyphs (path screen shows the 22 nodes without icons) | LATER-COOKS (build-biome-graveyard) | DONE: Tools/ui_glyphs.mjs necrop set; 44 PNGs Data/UI/Glyphs/path_<node>_a/_p.png (45 KB, LFS) in 62b0694 |
| G1 | Camera settings: FOV (third / first person), camera shake, motion blur, shoulder side | grade + ui-controls 2.14 | DONE: Camera page WRUISettings.cpp:1572; FOV / side WRRanged.cpp:337-344; shake WRFxCombat.cpp:484; blur WRKeyBindings.cpp:239 |
| G2 | Hold or toggle for sprint, guard, aim (XAG 107) | grade + ui-controls 2.3 | DONE: WRCharacter.cpp:1901 sprint toggle (+ TickSprintToggle), :1807 guard / aim toggle; weapon swap lets go WRRanged.cpp:112 |
| G3 | Pad: aim sensitivity (mouse and pad), invert X, dead zone, response curve, vibration strength | grade + ui-controls 2.14 | DONE: Look page WRUISettings.cpp:1491; WRPadInput.cpp:191 (mouse aim) / :209 (dead zone, curve, aim, invert X); rumble scale in WRInput::Rumble |
| G4 | Interface page: Desk / Couch, interface size, HUD size, HUD mode Full / Fade / Minimal, compass, names over players; crosshair, damage numbers and hints moved here | grade + ui-controls 2.7 / 2.14 | DONE: Interface page WRUISettings.cpp:1605; UI scale WRKeyBindings.cpp:239; HUD scale WRPlayerUI.cpp:53 |
| G5 | HUD modes in the HUD: Fade (calm 6 s fades vitals, compass, hotbar, plates; any change brings them back in 0.1 s), Minimal | grade + ui-controls 2.7 | DONE: SWRHud::TickHudMode WRUIHud.cpp:390 |
| G6 | Nameplates: centred, names setting, no hard-coded "E" (pad shows its button) | grade | DONE: AWRHUD::DrawWorldBars WRHUD.cpp:438, DrawCentred :431, ActionKey on the tame line |
| G7 | Disconnect: plain-language reason and Rejoin (retries 60 s), pad A / B | grade + ui-controls 2.17 | DONE: SWRMainMenu::MakeCard WRMainMenu.cpp:450, NoteFailure :1277, Rejoin :543 |
| G8 | Leave / Quit ask once, saying whether you're in a safe zone | grade + ui-controls 2.8 | DONE: ConfirmStrip WRUISettings.cpp:2427 (safe zone, host) |
| G9 | Crash card on the title after a crash: send the crash report in one press | grade + ui-controls 2.16 | DONE: WRReport.cpp:504 CheckLastRun, :575 SendCrash; the card WRMainMenu.cpp:450 |
| G10 | F8: quick chips that add a line to the note; a clear sent / saved line | grade + ui-controls 2.16 | DONE: chips WRUIReport.cpp:237 (tuning report.chips); the sent line was already built (WRReport.cpp Notify "Report sent: thank you") |
| G11 | Pad keyboard: every text box (chat, report note, name, address, world name, tame name) can be typed on a pad | grade + ui-controls 2.16 | DONE: WRUIPadKeyboard.cpp:265 / :330; hook WRPadInput.cpp:592 |
| G12 | Chat reachable on a pad | grade + ui-controls 2.17 | DONE: Settings > Chat WRUISettings.cpp:2495; the chat line's keyboard target in SWRChatBox::Open (WRMainMenu.cpp) |
| G13 | Map pins: several, glyphs, labels, compass shows them, pad and mouse | grade + ui-controls 2.12 | DONE: WRUIMap.cpp:170 pins store, :1100 pin card, :1715 glyphs; compass WRUIHud.cpp:274 |
| G14 | Pad route to the item hotbar (item wheel, D-pad right hold) | grade + ui-controls 2.13 | DONE: FWRPlayerUI::ItemWheelKey WRPlayerUI.cpp:1027; wheel mode 3 WRUIWheel.cpp:55+ |
| G15 | Track a recipe: one pinned checklist on the HUD | grade + ui-controls 2.10 | DONE: Track button WRUICrafting.cpp:1610; SetTrackedRecipe WRPlayerUI.cpp:953; TrackedCard WRUIHud.cpp:741 |

---

## 1. Research

### 1.1 How the best games solve it (specific systems, what we take)

| Game | The system | What makes it work | What we take |
|---|---|---|---|
| **The Last of Us Part II** (Accessibility menu, 60+ options) | Every button hold can be switched to a toggle and every rapid press to a hold; aim (L2) is the example. HUD scale Default / Large; HUD background. Motion-sickness group: camera shake and motion blur as intensity sliders, camera distance, FOV, a persistent centre dot. [PlayStation](https://playstation.com/en-us/games/the-last-of-us-part-ii/accessibility/), [GameSpot](https://gamespot.com/articles/the-last-of-us-2-accessibility-difficulty-options-/1100-6478756/), [Player2](https://www.player2.net.au/2020/06/exploring-the-last-of-us-part-iis-accessibility-options/) | Options are per verb, not one global "toggle mode"; shake and blur are sliders, not on/off, so a player can take the edge off without losing the hit's weight | Hold / Toggle per verb (sprint, guard, aim). Camera shake 0-100% slider (scales our trauma). Motion blur Off / Low / High. Crosshair already a setting (centre dot) |
| **Horizon Forbidden West** (General > HUD Visibility: Always On / Dynamic / Custom / Always Off; patch 1.07 added Always Off per element) | Dynamic hides most HUD (health, items, ammo, XP) until needed; Custom picks per element. [Gamepressure](https://www.gamepressure.com/horizon-forbidden-west/hud/z6f628), [player.one](https://player.one/horizon-forbidden-west-update-107-brings-new-options-hud-146094) | The default lets the world breathe; the HUD comes back the instant it matters | HUD mode Full / Fade / Minimal. Fade is the default: calm for 6 s fades the panels; a hit, a drawn weapon, a stat change or a new item brings them back in 0.1 s |
| **Ghost of Tsushima** ("light HUD, not HUD-less") | The HUD shows only what changed and fades again; an expert mode cuts it further | The fade must be fast back in and slow out, or players lose trust | Fade out over 0.8 s after 6 s calm; back in over 0.1 s |
| **Monster Hunter Wilds** (Options > Display: HUD size and per-element visibility; desk / living-room presets) | One switch sets HUD and text size together for the couch; per element toggles under it. [Destructoid](https://www.destructoid.com/all-accessibility-settings-in-monster-hunter-wilds/) | One choice gets you right; the fine controls stay below for those who want them | Setup Desk / Couch sets Interface size 100 / 115% and HUD size 100 / 115%; touching either reads "Custom" |
| **Xbox Accessibility Guidelines** 101 (text) and 107 (input) | Body text at least 26 px at 1080p on a TV, 18 px on a PC; remap; hold-or-toggle for held inputs. [XAG 101](https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/101) | Couch text sizes are a rule, not a taste | Couch = 115% interface: our 13-15 px mockup body text x U 1.2 x 1.15 lands at 18-21 px; the HUD's 11-12 px caps at Couch land at 15-17 px: Couch is the minimum, and the size slider goes to 130% |
| **Elden Ring / Dark Souls III** (Network: "Disconnected from the server"; Quit Game asks "Quit game?" and saves) | A dropped session lands you back at the title with one clear line; leaving asks once | One clear sentence; never a raw error code | Plain sentences per failure ("The host closed the game", "Lost the connection to the host", "The host has another version"), and Rejoin |
| **Valheim** (connection lost; map pins; cartography table) | Pins: fire, house, cave, mine, X, each with a label, placed by double-click; the map table records and reads everyone's discoveries; only the last writer's pins survive on the table. [Freakhosting guide](https://help.freakhosting.com/games/valheim/using-the-cartography-table-map-sharing), [Better Cartography Table](https://thunderstore.io/c/valheim/p/nbusseneau/Better_Cartography_Table/v/0.1.0/) | A handful of glyphs covers what players mark; labels make them memory | 6 glyphs (camp, ore, danger, creature, loot, flag), a 24-letter label, up to 30, compass shows pins within 400 m; clan sharing is the server's (later: map table) |
| **Sea of Thieves** (Insider bug reports), **Destiny 2** (error codes "Beaver", "Weasel") | A report gets a reference back so a tester can quote it; error names are memorable | The player must know it was sent | "Sent. Thanks." / "Saved: it sends when you're online", already built in WRReport; quick chips make a report one press + A |
| **Steam Deck / Xbox / PS on-screen keyboards** | QWERTY grid driven by D-pad / stick + A; B backspace, X space (Xbox: X backspace, Y space), Menu = done; the keyboard sits clear of the box it types into. [GamingOnLinux](https://www.gamingonlinux.com/2022/09/valve-finally-fixes-up-the-steam-deck-on-screen-keyboard-some-more/page=1/), [Steam forum](https://steamcommunity.com/app/1675200/discussions/2/3428948355369628271) | Long typing is slow: keep it for short text; never cover the line you're typing | `SWRPadKeyboard`: 4 rows + a row of Space / Shift / Backspace / Done, a preview line above, placed at the bottom third; A types, X backspace, Y space, LT shift, Menu or Done commits, B cancels |
| **Dragon's Dogma 2 / Monster Hunter / Baldur's Gate 3** (character creators at the start and a mirror later) | Choose from a few strong presets, preview live on the body, re-edit later for free | The preview IS the creator; names and numbers mean nothing without it | The look picker previews on your survivor live behind the menu (the menu's camera frames them); faces by name, three builds, helmet shown or hidden |
| **Enshrouded / Palworld / Breath of the Wild** (item wheels: hold a D-pad direction, the world keeps going or slows) | A held D-pad direction opens a radial of tools/weapons; release over a wedge picks | A pad needs a direct route to every item without opening the bag | Hold D-pad right 0.40 s: the 8 hotbar items on our built wheel art; release picks, centre cancels; the tap stays "next ability" |
| **Grounded / Subnautica / Satisfactory** (pin a recipe: the HUD lists its parts with counts) | One pinned recipe's checklist ticks as you gather | The checklist answers "what am I still missing" without the menu | Pin on the crafting details; a checklist top-right under the clock, ticks from the bag, clears on craft |

### 1.2 Real-world reference (where it applies)

- **Camera / eyes:** a human's comfortable horizontal field on a 16:9 monitor at desk distance is 80-100° hFOV in
  game terms; below ~75° in first person causes motion sickness for many (the TLOU2/Doom/Apex ranges 70-110 / 120). We
  clamp first person 75-110 and third person 70-100 (ui-controls 2.14) and keep the built 90 as the default for both
  (the combat lane's camera, aim zoom 60 and sprint widen were tuned at 90).
- **Reading at a distance:** text at 3 m on a 55" TV needs ~1.45x the pixels of text at 0.6 m on a 24" monitor for the
  same visual angle; Couch's 115% interface + 115% HUD is the step the XAG numbers ask for, the slider covers the rest.
- **Thumbsticks:** worn pads drift 0.05-0.12 at rest; a 0.15 inner dead zone is the industry default; players with
  worn sticks need up to 0.30. Response curves: linear 1.0 (fast flicks), 1.8 (our built default), 2.5 (precise aim).

---

## 2. Grades: every built piece against THE BAR

Grades: A = at the bar; B = works, short of the bar on a detail; C = a real gap a tester will hit.

| Piece | Where | Grade | Finding |
|---|---|---|---|
| Stone-and-leather kit, fonts, panels | WRUIStyle.h:12, WRUIWidgets, WRUIWindowKit.h | A | Consistent look; `U` is a constexpr 1.2 (WRUIStyle.h:15), so no size setting can scale it: scaling must sit above it (engine ApplicationScale, an SDPIScaler on the HUD) |
| Settings window: nav, Controls, Audio, Video, World | WRUISettings.cpp:2051-2216 | B | Clean; Esc / Resume / Main menu / Quit. Damage numbers and crosshair sit under Controls (:879, :884), hints under Video > Game (:1622): misfiled. Main menu and Quit have no confirm (:2184, :2195) |
| Look and controller page | WRUISettings.cpp:1439 | B | Mouse / pad sensitivity, invert Y, button names, vibration on/off. Missing: aim sensitivity, invert X, dead zone, curve (tuning-only, `look.padDeadZone` / `padCurve`), vibration strength, hold / toggle |
| Camera | WRRanged.cpp:308 TickAimCamera | C | FOV fixed at 90 for both views (:328), shoulder fixed right (+75), shake strength fixed (WRFxCombat.cpp:482), motion blur never set: no player control (L1, G1) |
| Hold vs toggle | WRCharacter.cpp:1735 (sprint), :1798 InputPressed (block / aim) | C | Holds only (XAG 107, TLOU2) |
| HUD | WRUIHud.cpp:319 | B | Full and readable, every land's bar; always fully on (no Fade / Minimal), no compass toggle, no HUD size; vitals panel is 360 px x U wide always |
| Nameplates over creatures and players | WRHUD.cpp:424 DrawWorldBars | C | Canvas text starts at the bar's left edge (not centred over the head), always on, and the tame line hard-codes "E to tame" (:488): wrong on a pad and after rebinding |
| Pad support (menus, prompts, isolation) | WRPadInput.h, Data/gamepad.json | A | One input processor, device-follow prompts, isolation 0.14 s. Text boxes can't be typed on a pad (G11); no route to the item hotbar (G14); no chat on a pad (G12) |
| F1 controls page | WRUIControls.cpp | A | Both devices, LB/RB pages |
| F8 report | WRReport.cpp, WRUIReport.cpp | B | Frozen frame, 8 kinds, note, send, outbox. No quick chips; a pad player has to type the note (no keyboard) |
| Main menu | WRMainMenu.cpp | B | Name, body, path, Host / Join, update banner, tips. A network failure shows the engine's raw text ("Disconnected: ConnectionLost", :889), no Rejoin; no crash card; no way to pick a face or build (`Look` is read at :690 but nothing writes it: L5) |
| Worlds list | WRUIWorlds.cpp | A | |
| Journal and its tabs (inventory, crafting, build, paths, skills, map, clan, deeds, artifacts) | WRUIJournal.cpp, WRUI*.cpp | A/B | Inventory has sort (WRUIInventory.cpp:400), weight, split / drop pickers; crafting quantity (WRUICrafting.cpp:716); no recipe tracking (G15) |
| Map | WRUIMap.cpp | B | Markers, legend, one waypoint (:142); no pins (G13); no fill-in reveal (L3: RAVEN-PLANNED) |
| Wheels | WRUIWheel.cpp | A | Ability, tame orders, whistle (hold X at nothing) |
| Death / loading / moments / hints | WRUIFlow.cpp, WRFlowDeath.cpp | A | |
| Chat box | WRMainMenu.h SWRChatBox, WRPlayerController.cpp:91 | C | Keyboard only (T / Enter) |
| Station screens (Hearth, claim, cook, machine, drive, door, stable, incubator, portals, endgame, world) | WRUI*.cpp | A | Built by their lanes to the kit; pad works through the processor |

---

## 3. Plan (written before any code)

Nine parts, landed in order, each compiled, committed and pushed with CHANGELOG + HANDOFF. Every new number goes in
`Data/tuning.json` under `ui` or the existing `look` / `pad`; every new sound is a `Data/Audio/sounds.json` row.

### 3.1 Part 1: Settings (camera, look and controller, interface) and their wiring

**Where prefs live.** `FWRKeyBindings` already owns every per-PC input/look pref and saves to GameUserSettings.ini
`[Wildreach.Keys]`; the new prefs join it (one load, one save, one `OnChanged`), so every reader has one place to ask.

| Pref | Values | Default | Read by |
|---|---|---|---|
| FovThird | 70-100 step 1 | 90 | `TickAimCamera` (WRRanged.cpp): base FOV of the third-person camera; aim FOV keeps its ratio (60 / 90 x FovThird) |
| FovFirst | 75-110 step 1 | 90 | the first-person camera (same function), aim ratio the same (L1) |
| ShakeScale | 0-100% step 10 | 100 | `UWRFxCameraModifier::AddTrauma` multiplies the added trauma (0 = no shake; the FOV punch also x scale) |
| MotionBlur | Off / Low / High | Off | `r.MotionBlurQuality` 0 / 2 / 4 and `r.MotionBlur.Amount` 0 / 0.25 / 0.5 set on change and at start |
| CameraSide | Right / Left / Centre | Right | `TickAimCamera`: the boom's socket Y x +1 / -1 / 0 (aim offset likewise) |
| MouseAimSens | 0.2-1.5 step 0.05 | 0.70 | `WRInput::MouseLook` while `IsAimingRanged` |
| PadAimSens | 0.2-1.5 step 0.05 | 0.60 | `WRInput::PadLook` while aiming |
| InvertPadX | on / off | off | `PadLook` |
| PadDeadZone | 0.05-0.35 step 0.01 | tuning `look.padDeadZone` (0.15) | `PadLook` |
| PadCurve | Linear 1.0 / Standard 1.8 / Precise 2.5 | Standard | `PadLook` |
| RumbleStrength | Off / 25 / 50 / 75 / 100% (replaces the on/off switch: Off = 0) | 100 | `WRInput::Rumble / RumbleLight` scale strength |
| SprintMode / GuardMode / AimMode | Hold / Toggle | Hold | `AWRCharacter::InputSprint / StopSprint`, `InputPressed / InputReleased` for Block (guard when no ranged weapon out, aim when one is) |
| Setup | Desk / Couch / Custom (derived) | Desk | sets UiScale and HudScale together |
| UiScale | 85-130% step 5 | 100 | `UUserInterfaceSettings::ApplicationScale` (every screen, menus included) at start and on change |
| HudScale | 75-130% step 5 | 100 | an `SDPIScaler` around the HUD widget (WRPlayerUI.cpp:49) |
| HudMode | Full / Fade / Minimal | Fade | the HUD (part 2) |
| Compass | on / off | on | the HUD's compass (part 2) |
| Names | Always / Near (40 m) / Off | Near | `AWRHUD::DrawWorldBars` for other players (part 2) |
| ShowHelmet | on / off | on | `ServerSetHideHelmet` on the pawn now + `[Wildreach.Menu] Look`'s `.h` for the next join (L2) |

**Toggle behaviour (edge cases):**
- Toggled sprint ends when you stop moving (speed under 50 cm/s for 0.25 s), when stamina runs out (the movement's own
  rule already drops it), on any menu (DropHeldInputs, built), and on a second press.
- Toggled guard / aim: the first press is a real press (parry window opens, guard up / bow drawn); the second press lets
  go. A menu, a mount, death or the report lets go (DropHeldInputs, built). Switching weapon set while toggled lets go.
- The verb used is decided at the press: a ranged weapon out = Aim mode, else Guard mode.
- Hold stays the default for all three (the combat cook's parry feel is a tap of the guard).

**The pages** (`WRUISettings.cpp`; same `SettingRow` / `Segmented` / `Stepper` / `SWRSwitch` kit, so the pad works
through the processor with no new code):
- **Look and controller** (built page, extended): Mouse: sensitivity, aim sensitivity, invert. Controller: look speed,
  aim speed, invert up/down, invert left/right, dead zone, response, button names, vibration (Off / 25 / 50 / 75 / 100).
  Buttons: hold or toggle for Sprint, Guard, Aim.
- **Camera** (new page; two new code-drawn line icons join `SWRLineIcon`: `Camera` (a body with a lens ring) for this
  page and `Layout` (a frame with a bar top-left and a strip bottom) for Interface): rows FOV third person, FOV
  first person, camera shake, motion blur, shoulder (Right / Left / Centre). Sub-lines say what each does in plain words.
- **Interface** (new page, icon `Layout`): Setup (Desk / Couch, "Custom" shown when either size was moved), Interface
  size, HUD size, HUD (Full / Fade / Minimal with a sub-line describing the choice), Compass, Crosshair (moved from
  Controls), Damage numbers (moved), Hints (moved from Video), Names over players (Always / Near / Off), Show my helmet.
- Reset to defaults on each new page resets that page's prefs only.
- Nav order: Resume · Controls · Look and controller · Camera · Interface · Audio · Video · World · Main menu · Quit.
- The `WRSettings` test command learns `look`, `camera`, `interface`.

### 3.2 Part 2: the HUD (modes, size, compass, nameplates)

- **HUD size:** `FWRPlayerUI` wraps the HUD widget in `SDPIScaler` with `DPIScale_Lambda(HudScale)`.
- **Fade** (default). The HUD keeps `CalmFor` seconds: it resets to 0 on any of: health below 99% of max, stamina below
  99%, a path resource gauge moving, a status buildup or status on you, lock-on, a drawn weapon held in guard/aim,
  a boss bar up, a raid on your claim, a screen or wheel opening/closing, the hotbar or belt changing, a pickup line,
  build mode, riding, under water. Fadeable groups: level plate, compass, clock plate, vitals, hotbar, ability plate,
  tame card. Opacity = 1 while `CalmFor < ui.hud.fadeAfter` (6 s), then eases to 0 over `ui.hud.fadeOut` (0.8 s); any
  reset brings it to 1 over `ui.hud.fadeIn` (0.1 s). Never fades: prompts, pickups, notices, chat, the crosshair, boss
  bars, hazard lines, toasts, the breath arc (each already shows only when it has something to say).
- **Minimal:** level plate, clock plate, compass, ability plate and tame card hidden; vitals only while health < 100%
  (or a path gauge/status is live); hotbar only for 4 s after it changes; everything else as Fade.
- **Full:** as built (always on).
- **Compass off:** the compass strip collapses; the boss bar keeps its place.
- **Nameplates** (`AWRHUD::DrawWorldBars`): every line centred on the head point (measure text with `GetTextSize`);
  other players' names follow the Names pref (Off: no name line for players, their bars stay in a fight; Near: within
  40 m (`ui.names.nearCm` 4000), Always: up to the 25 m bar range and the name alone out to `ui.names.farCm` 8000);
  the knocked-out line reads "knocked out: <key> to tame" with `WRInput::ActionKey("Interact")`.

### 3.3 Part 3: flow (leave, disconnect, crash card, F8 chips)

- **Leave / Quit confirm.** In the settings window (in game) Main menu and Quit open an inline confirm strip over the
  footer: "Leave this world?" + a line: in a safe zone "You're in a safe zone." else "You're not in a safe zone: your
  body stays for a moment." (`UWRSocial::IsInSafeZone`); buttons [Leave] [Stay]. Pad: the strip's buttons are D-pad
  reachable; B = Stay (pad handler). Quit's says "Quit to desktop?".
- **Disconnect card** on the main menu (`SWRMainMenu`): `WatchNetworkFailures` maps the failure to a plain line and
  remembers whether it can rejoin (we were a client and we know the address):
  - ConnectionLost / ConnectionTimeout: "Lost the connection to the host."
  - NetDriverAlreadyExists / NetDriverCreateFailure / NetDriverListenFailure: "Couldn't start the connection."
  - PendingConnectionFailure: "The host didn't answer." (with the error text when the server gave a reason, e.g. "Server full", a kick)
  - Outdated client/server: the built version line (no Rejoin: it can't work).
  - Travel failures: "Couldn't reach that world."
  The card: title "Disconnected", the line, buttons **Rejoin** (primary) and **Close**. Rejoin starts joining the last
  address (`[Wildreach.Menu] Address`) with the menu's options; if that fails again the card returns with a counter
  "Try 2 of 6"; after 6 tries over about 60 s it says "The host isn't answering. Try again later." and only Close stays.
  Pad: A on Rejoin (cursor jumps to it; D-pad), B closes (pad handler).
- **Crash card.** `WRReport` writes `Saved/Wildreach/clean_exit` (a timestamp) on a normal quit (`FCoreDelegates::OnExit`
  when not `GIsCriticalError`) and `Saved/Wildreach/session` at start. On start, if `session` exists from the last run
  and there's no `clean_exit` newer than it, and `Saved/Crashes` has a folder newer than the session stamp, the main menu
  shows the card: "Wildreach closed unexpectedly last time." [Send the crash report] [Not now]. Send posts through the
  same report path (kind "Crash" = the "Bug" kind with label `crash`, the note "Crash on the last run", context: the
  crash folder's `CrashContext.runtime-xml` first 3000 chars and the last 80 lines of the previous log
  `Saved/Logs/Wildreach-backup-*.log`), then a line "Sent. Thanks." Either button removes the stamp so it shows once.
- **F8 quick chips** (`WRUIReport.cpp`): a wrap of 12 small chips under the note ("Just happened", "Happens every
  time", "Hit didn't land", "Got hit by nothing", "Can't move", "Fell through", "Too hard", "Too easy", "Looks wrong",
  "Sounds wrong", "Music stopped", "Love this"); a click appends the chip's text as its own line (no duplicates; a
  second click removes it). Texts in `Data/tuning.json` `report.chips` so they're edited without code.

### 3.4 Part 4: the pad keyboard (every text box) and chat on a pad

- `SWRPadKeyboard` (new `WRUIPadKeyboard.cpp`): a stone panel docked to the bottom third, a parchment preview line
  with a caret, then keys: `1-0`, `q-p`, `a-l '`, `z-m - .`, and a last row Shift · Space · Back · Done. Letters follow
  Shift (one-shot; double-press locks). Max length from the target box (chat 160, name 20, address 64, else 64).
- Pad: the D-pad / left stick moves the focus key (wraps), A types the focused key, X backspace, Y space, LT shift,
  RT / Menu = Done, B cancels (the text box keeps what it had). Mouse: click a key; Esc cancels, Enter commits.
- **Every text box:** the pad processor, on A over an editable text (`SEditableText` / `SMultiLineEditableText` type
  names) while the device is a pad, opens the keyboard on it (its text in, Done writes it back with
  `SetText` and commits as Enter would via `OnTextCommitted` on `SEditableText`). That covers chat, the report note,
  the main menu name and address, the world name, tame names, sign / message boxes, door codes' text boxes.
- **Chat on a pad:** the settings window (in game, the journal's last page on the pad) gets a nav item **Chat** that
  opens the chat line; with a pad the keyboard opens on it at once.

### 3.5 Part 5: the look picker (face, build, helmet) on the main menu

- Under Body on the menu's "Your character" panel, a **Look** row: [◀] "Wren" [▶] (the face's name from
  `player_art.json hero.<sex>.faces[].name`) and a build segment (Lean / Athletic / Strong) and "Helmet: Shown / Hidden".
  Changing the body (sex) clamps the face index to that sex's list.
- It writes `[Wildreach.Menu] Look = <face>.<build>[.h]` (the format GameMode already parses, WRGameMode.cpp:142).
- **Live preview:** while the menu is open the menu world's own survivor exists (standalone, authority); after each
  change the menu calls `SetHeroAppearance(face, build)` and `ServerSetHideHelmet` on it (both authority-side and
  replication-free in standalone), and a **"Look at me"** toggle swings the menu camera from the island view to a
  2.6 m framing of the survivor (height 1.0 m, 20° off the face, `flow.lookCamera.*`), so you see the face you pick.
  Body (sex) changes apply at the next join (the body is chosen at login), and the row says so.

### 3.6 Part 6: map pins

- Up to 30 pins per world per PC (client-side: `Saved/Wildreach/Pins/<world>.json`, `{x, y, glyph, label}`), so they
  need no server change and survive restarts; the world id is the host's world (`WRWorlds::Current()`, or the joined
  address for clients).
- Right-click (or X on a pad) on empty map: a small pin card at the cursor: the 6 glyphs (camp, ore, danger, creature,
  loot, flag) as buttons, a label box (24 letters; the pad keyboard), [Place] [Cancel]. On a pin: [Remove] [Rename].
- Drawn in the map's marker layer with its glyph (drawn in code, line icons) in brass; label under it when zoomed in.
- The compass shows pins within `ui.pins.compassCm` (40,000 cm) as small brass diamonds with the glyph.

### 3.7 Part 7: item wheel on the pad, reserved holds

- D-pad right: a tap is "next ability" as built (fires on release before `ui.itemWheel.holdSeconds` 0.40 s); a hold opens
  the item wheel over the built wheel art: 8 wedges = the 8 hotbar slots (icon, count, key cap), the right stick picks
  (past 0.5), release uses the slot (`UseHotbar`), centre cancels; isolation on open/close (built `Transition`).
- `Data/gamepad.json` gains `"reserved": { "DPadUpHold": "ping / quick chat wheel (multiplayer cook)", "ViewHold":
  "player list (multiplayer cook)" }` and the F1 page lists the item wheel (L4).

### 3.8 Part 8: track a recipe

- Crafting details panel: a **Track** button (pin icon); one tracked recipe per player (local pref per world).
- HUD, top right under the clock plate: a leather card "Tracking: <name>" and each ingredient "Copper ingot 3 / 8",
  green when met, from the bag counts; it clears itself when the tracked item's count in the bag goes up after a craft
  (or by Untrack). Fades with the HUD mode like the clock plate.

### 3.9 Part 9: the fill-in map (if the night allows)

L3 as ui-controls 2.12 with the continent numbers: a server-side per-player 1-bit grid at 8 m cells over the map
bounds (`ui.mapMin / mapSize`), reveal radius 150 m, sent to the owner in changed 64x64 tiles, saved RLE in the player
save, the map texture multiplied by the reveal mask. This is the biggest piece; it lands last and only whole.

### 3.10 Design intention (Q7)

- Every new setting writes to one pref store that the one system it governs reads each frame: no setting is a
  label without an effect, none duplicated across pages (crosshair, damage numbers, hints moved, not copied).
- The fade never hides something that asks for action (prompts, notices, boss bars) and never hides your health while
  anything is hurting you: it only removes what's settled.
- Confirm strips sit over the footer of the same window (no new pop-up layer), the buttons in the same place each time.
- The disconnect and crash cards sit where the menu's status line was (the place a player already looks after a failure).
- The pad keyboard docks clear of the line it types into, and is the same panel everywhere.

### 3.11 Build steps

1. Doc committed + pushed (this).
2. Part 1: `WRKeyBindings.h/.cpp` prefs; `WRUISettings.cpp` pages; wiring in `WRRanged.cpp`, `WRFxCombat.cpp`,
   `WRPadInput.cpp`, `WRCharacter.cpp`, `WRPlayerUI.cpp` (scales), motion blur cvars; tuning `ui`; compile; commit.
3. Part 2: `WRUIHud.cpp` fade / minimal / compass; `WRHUD.cpp` nameplates; compile; commit.
4. Part 3: `WRUISettings.cpp` confirm; `WRMainMenu.*` disconnect card + crash card; `WRReport.*` stamps + crash send;
   `WRUIReport.cpp` chips; compile; commit.
5. Part 4: `WRUIPadKeyboard.cpp` + processor hook in `WRPadInput.cpp`; chat nav item; compile; commit.
6. Part 5: `WRMainMenu.*` look row + preview; compile; commit.
7. Parts 6-9 in order, as time allows, each whole.
8. Spot-check every part against this plan; fill the ledger; push the doc; LANDED line.


---

## 4. Spot-check (read-through against the plan and THE BAR)

- **Found and fixed:** the map fog texture was a function-static strong pointer (released after the GC at quit: a
  crash on exit), now rooted (f7dbe00); the crash check could run after the menu first asked, now it runs on first ask
  (f7dbe00); the chat line closed itself when the pad keyboard opened (focus moved), now it waits while the keyboard
  is up and the keyboard never takes focus (4833f2d); the D-pad could reach menu buttons behind the Disconnected card,
  now the card is the top nav root while it shows (0343a21); the HUD fade read a drained max health as "something
  happening" (it would never fade), now it reads against the effective max (5918156); the tracked-recipe and set
  checks ran from the HUD widget's tick, moved to FWRPlayerUI::Tick (483536e); the item wheel's tap fires on release
  so the hold can exist (661b476); client map-reveal tiles are asked only by the owning client (147b14e).
- **Against THE BAR:** every number in tuning; every new element from the kit; every control reachable by pad and
  mouse with B / Esc backing out; server authority where others see it (helmet, the reveal grid); nothing unfinished
  shown (L7 has no button anywhere).
- **Plan deviations, with reasons:** FOV defaults stay 90 / 90 (the combat camera, aim zoom and sprint widen were
  tuned at 90; ui-controls said 80 third person); map pins are this PC's own (clan sharing needs the map table piece);
  the look picker covers face, build and helmet (hair, beard, colour and body sliders need the morph-bake art route,
  ui-controls 3.3); crash reports post through the F8 outbox (category Bug, label bug, body "[Crash] ...") so the
  inbox's filters work.

## 5. Not finished (on LATER-COOKS)

- L7: the Riftback's pocket (a server relay of the shared rift inventory to a far player, then the machine screen).
- The rest of the ui-controls design not in this pass: the creator's sliders / hair / colours (morph bake), verb chips
  anchored in 3D (hints.json), the first-need HUD reveal and the equipment cross, the camp kits / message-stone notes /
  get-up of the first hour, the inventory context menu and compare, crafting and building from nearby chests, the pad
  build mode (LB / RB rotate, copy), the F1 pad diagram, DefaultScalability.ini presets with the 8 GB Medium and VRAM
  auto-detect, the loading screen's line from a read note, and map pins shared with a clan (the map table).
