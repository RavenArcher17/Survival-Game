# wild-hotfix-menus: every screen fits every screen (then 0.8.1)

Lane wild-hotfix-menus, desktop, 2026-11-16. Aaron 11-16 on the laptop with 0.8.0 (2560x1600 16:10 panel at high
Windows scaling, also a 1920x1080 screen): "I can't even start the game, the character covers up the character
preview, and the continue button is off the screen. Terrible design it can't even be played."

## REQUIREMENT LEDGER

| # | Requirement (from the brief / Aaron's quote) | Done + where |
|---|---|---|
| R1 | Character select / creation screen lays out right on 16:9, 16:10, 21:9, 4:3 | see Proof |
| R2 | ... from 1280x720 up to 3840x2160 | see Proof |
| R3 | ... at Windows scaling 100-200% | see Proof |
| R4 | ... windowed and fullscreen | see Proof |
| R5 | The character never covers the preview (and no panel covers the character) | see Proof |
| R6 | Continue / Start / Back always on screen | see Proof |
| R7 | ... reachable with mouse, keyboard and pad | see Proof |
| R8 | Fixed at the cause (DPI rule / UserInterfaceSettings, anchors, safe zone, scale boxes, size boxes, preview camera framing), not by nudging one screen | see Proof |
| R9 | Audit every menu and HUD screen the same way: main menu, settings, server browser / host / join, inventory, crafting, building, map, players / clans, god-mode panel, pause, death / respawn; fix each | see Proof |
| R10 | Verify by rendering each screen at each size / scale and reading the pictures back (layout only) | see Proof |
| R11 | Run Tools/art/electric_dreams_forest.ps1 on the desktop first so the package has the new forest | see Proof |
| R12 | 0.8.1 includes everything on master since 0.8.0 (answers-1116: map pins, re-sited places, forest; strongholds; 100 players) | see Proof |
| R13 | Package with the repo's script (D: engine, -Out E:\Dev\WildreachBuild<N>), keep the newest 2 build folders | see Proof |
| R14 | Playable gate on the packaged game at 2560x1600 (150%) and 1920x1080: menu -> New Game -> play; Continue -> play | see Proof |
| R15 | Installer tested on a copy; the updater path from 0.8.0 works (0.8.0's updater reads the parts index) | see Proof |
| R16 | GitHub release v0.8.1 via the REST API; every asset digest = local SHA-256; latest -> v0.8.1 | see Proof |
| R17 | Email Aaron and Raven one line each: "Wildreach 0.8.1" + the release link | see Proof |
| R18 | RELEASED.txt / EMAILED.txt in wild-desktop; "WILDREACH RELEASED 0.8.1 <url>" reported (the coordinator writes DESKTOP-SAYS) | see Proof |
| R19 | CHANGELOG + HANDOFF in the same commits; files added by name; LANDED line | see Proof |
| R20 | Wildreach rule: no music UI on screen (remove any found in scope) | see Proof |

## 1. What is actually wrong (read from the code, 0.8.0 = 0f4fdd1)

The UI is Slate (not UMG), added with `UGameViewportClient::AddViewportWidgetContent`, so the engine's DPI rule scales
it: `UIScaleRule=ShortestSide` with the stock curve (480->0.444, 720->0.666, 1080->1.0, 8640->8.0), which is exactly
`height / 1080`. So every screen lays out on a canvas **1080 logical px tall** and `width x 1080 / height` wide:

| Screen | Logical canvas |
|---|---|
| 1280x720, 1920x1080, 2560x1440, 3840x2160 (16:9) | 1920 x 1080 |
| 2560x1600, 1920x1200, 1680x1050, 1440x900 (16:10) | **1728** x 1080 |
| 3440x1440, 2560x1080 (21:9) | 2580-2560 x 1080 |
| 1024x768, 1600x1200 (4:3) | **1440** x 1080 |
| any of them with "Text and menus" at 130% (ApplicationScale) | 1329-1477 x **831** |

Windows scaling: the game process is per-monitor DPI aware (`bEnableHighDPIAwareness = 1`,
Engine/Source/Runtime/ApplicationCore/Private/GenericPlatform/GenericPlatformApplicationMisc.cpp:25, set in
WindowsPlatformApplicationMisc.cpp:292) and `bAllowHighDPIInGameMode=False` (BaseEngine.ini), so Windows' 150% / 200%
never multiplies the game's UI: the window is in real pixels and the curve above decides. A 2560x1600 panel at 150% is
the 1728x1080 canvas, the same as 1440x900 at 100%. That is what lets this desktop (1920x1080, 100%) reproduce it.

The faults, each found in the code:

1. **The main menu is placed with fixed offsets** (WRMainMenu.cpp: the panel at `Padding(72U, 226U)`, the look picker at
   `Padding(572U, 226U)`, U = 1.2). Since 0.8.0 the path picker lists **all 12 paths in one column** (12 x 34U rows):
   the panel is ~1160 logical px tall from y 271, so its bottom (Host a game's row, Join, Settings, Quit) is below the
   1080 canvas **at every resolution**, worst with a bigger interface size. That is "the button off the screen".
2. **The look picker sits where the survivor is framed.** `FrameSurvivor` puts the survivor in the middle of the
   picture (FOV 40, 3 m); the picker spans x 686-1082 and the character panel 72-648, so on the 1728-wide canvas the
   panels cover the body and the body sits behind the picker: "the character covers up the character preview".
3. **Big windows are wider than the narrow canvases**: the journal (inventory / crafting / map / paths / skills / clan /
   quests / deeds tabs) is 1440U x 812U = **1728 x 974**: exactly the whole 16:10 width, 288 px too wide at 4:3; Settings
   (pause) 1560 x 936; the tame card 1344 x 1008; the worlds window (Host a game) 1344 x 864; cooking 1416 x 816. At
   130% interface size most of them are taller than the 831 canvas.
4. Nothing ever shrinks a screen that doesn't fit: no scale box anywhere (only the HUD's own size scaler).

## 2. Research: how games that get this right do it

- **Unreal's own answer**: the DPI curve + a *Scale Box* set to "Scale to Fit, Down Only" around each window (UMG). The
  scale box only ever shrinks, and only when the content doesn't fit, so text stays at the designed size wherever it can.
  Its weakness: it centres its child at its desired size, which breaks anything anchored to the screen's edges.
- **Unity's CanvasScaler "Scale With Screen Size, Match 0.5"** and **Godot's "canvas_items, expand"**: a design canvas
  (1920x1080) that is *expanded* on the long axis, never cropped: a narrower screen gets a taller canvas, not a cut one.
  This is the rule we lack for 4:3 and 16:10: our curve only looks at height, so a narrow screen loses width.
- **Hades / Hades II (Supergiant)**: one 16:9 design canvas; on 16:10 (Steam Deck) the canvas grows taller and the
  background art extends, no window ever goes off; ultrawide keeps the windows at 16:9 in the middle and lets the world
  fill the sides.
- **Elden Ring / Dark Souls III** menus: the inventory and status windows are a fixed 16:9 block centred; the HUD's
  corner pieces anchor to the corners. Corners anchor, windows centre, nothing is placed by absolute offset.
- **Steam Deck Verified** (Valve's checklist, 1280x800 16:10): text must be legible (~9 px minimum at 1280x800), no UI
  cut off at 16:10, every screen reachable on the controller. 16:10 is exactly Aaron's laptop shape.
- **Xbox Accessibility Guideline 101/102** and console "title safe" (90%): keep interactive elements away from the
  outer edge; never rely on the player scrolling to find the main button.
- **Character creators**: *Baldur's Gate 3* (the body stands in the middle, the option panels left and right, the camera
  frames the body into the space between them, zooms to the face on face options); *Monster Hunter Wilds / World*
  (panels on the left third, the hunter framed in the right two thirds, full body, turntable); *Dragon's Dogma 2* (the
  Arisen framed right of centre, menus left); *Elden Ring* (the body centred and the panels left, with the camera
  pulled right so they never overlap). The common rule: **the camera frames the character into the space the panels
  leave**, measured from the layout, and the framing is full body for build choices, upper body for faces.
- **Real-world reference (portrait photography)**: lead room: a figure facing into the frame is placed on the third
  away from where they look; head room ~8-10% of the frame above the head; full-length portraits keep the feet above the
  bottom edge by ~5%. A 35-50 mm-equivalent lens (vertical FOV ~27-35 deg) avoids wide-angle distortion of the face.

What we take: (a) a canvas rule that guarantees a minimum logical width as well as height (Unity/Godot "expand"),
(b) a "fit, down only" wrapper that keeps edge anchoring (UMG scale box without its weakness), (c) layout by flow
(stacks and columns), never by absolute offsets, (d) the creator camera framing the survivor into the measured free
space, full body, with head room and lead room.

## 3. Plan (every detail decided before building)

### 3.1 The canvas rule (the cause, for every screen at once)
- New `UWRDPIScalingRule : UDPICustomScalingRule` (WRUIScale.h/.cpp). `DefaultEngine.ini`
  `[/Script/Engine.UserInterfaceSettings] UIScaleRule=Custom, CustomScalingRuleClass=/Script/Wildreach.WRDPIScalingRule,
  bAllowHighDPIInGameMode=False` (written out so nobody turns Windows scaling on top later).
- Scale = min( shortest side / 1080, width / 1760 ). 1760 = the journal (1728) + 16 px each side, the widest window. So:
  16:9 is unchanged (1920x1080 canvas, same text size as 0.8.0); 16:10 gets 1760x1100 (2% smaller); 4:3 gets
  1760x1320; 21:9 unchanged. The two numbers are tuning (`ui.canvas.height` 1080, `ui.canvas.minWidth` 1760), read
  once. Portrait or odd windows fall out of the same min().
- Text-size check: the smallest body text (12 x 1.2 = 14.4 logical) is 13.0 px at 1440x900, 10.5 px at 1280x960 (4:3),
  8.4 px at 1024x768 (still above Steam Deck's 9 px at 1280x800: 1280x800 gives 10.5 px).

### 3.2 Fit, down only, edges kept (for interface size and anything still too big)
- `SWRFit` (WRUIWidgets.h): one child, arranged across the whole space it is given; if the child's desired size is
  bigger than that space less a 16 px margin each side, it is laid out at scale s = min(availW / wantW, availH / wantH)
  (never > 1, never < 0.55) on a canvas of space / s. Because the child still gets the *whole* (scaled) space, corner
  anchoring keeps working (unlike a scale box) and a centred window stays centred. Text renders at the real scale
  (`GetRelativeLayoutScale`, like `SDPIScaler`), so it stays sharp.
- Put at every viewport root: the screen host (every `WR_REGISTER_SCREEN` screen: journal and its tabs, settings /
  pause, build bar, god panel, hearth, claim, storage, taming, tame card, stable, cook, door, portals, altar, world
  screens...), the HUD (outside its HUD-size scaler, so a 130% HUD that would collide shrinks instead), the wheel, the
  death / loading / respawn cover and the moments layer (WRUIFlow), the main menu (its layout, its card and its
  settings separately), the F1 controls page, the F8 report panel, the pad keyboard. Focus and nav roots stay on the
  inner widgets (SWRFit takes no focus).
- What it does to the "Text and menus" setting: windows grow with it until they reach the screen, then stop growing
  (the HUD and small screens still grow). The settings row's sub line says so.

### 3.3 The main menu, rebuilt as a flow layout
- Root overlay: the two gradient fills (unchanged look), then one `SWRFit` holding a **three-column row** inside a
  48U / 40U screen margin:
  - **Left column** (auto width 480U): the title block, 18U gap, the character panel. No absolute y: the panel starts
    where the title ends.
  - **Look column** (auto, 330U + 20U gap, only while the picker is open): the "Your look" picker, top aligned with the
    panel.
  - **Stage** (fill): empty; its measured screen rectangle is where the survivor is framed. The tip (top) and the
    update banner + version (bottom) live in a right-aligned stack *over* the stage edges; the stage rectangle used for
    framing excludes the tip's and banner's rows.
- **Path picker for 12 paths**: a 3-column grid of short names (glyph + "Fire"), 4 rows of 30U (was 12 rows of 34U):
  132U instead of 444U. Under it the info line becomes live: "You wake in the Volcanic land" (path's biome), replacing
  the static "Your main path sets where you spawn". Two paths only (old data) keeps the pair.
- **Buttons**: Host a game (big, primary) stays; Join + address row stays; **Settings and Quit share one row**
  (Settings fill, Quit 150U) to save a row. Total panel height ~600U = 720 logical: on the 1080 canvas from y ~230 the
  bottom is at ~950, 130 px clear; at 130% interface size SWRFit shrinks it to fit.
- The tip hides when the canvas is too narrow for title + tip side by side (measured: its desired width + the title's
  desired width + margins > canvas width), and while the look picker is open (the stage needs the top right).
- Pad / keyboard: unchanged nav (WRInput spatial nav over the buttons); the grid is spatially navigable (D-pad left /
  right inside a row, up / down between rows); Esc closes the card / picker as before.

### 3.4 Framing the survivor into the stage (the "preview")
- `SWRMainMenu` keeps a pointer to the stage widget; each tick while the picker is open it reads the stage's cached
  geometry and the menu root's geometry and makes a normalised rectangle (0-1 of the viewport). Shrink it by 4% each
  side (breathing room).
- Camera math (UE FOV is horizontal): tanH = tan(FOV/2), tanV = tanH / aspect. Required distance so the body (feet to
  crown, `flow.lookCamera.bodyHeight` 185 cm, plus 10% head room, 5% foot room) spans the stage's height fraction f:
  d = (bodyH x 1.15) / (2 f tanV). Clamp 180-900 cm. Aim point = body centre (actor location + `flow.lookCamera.height`).
- Offset so the aim point lands on the stage centre (nx, ny in -1..1, y up): the camera's yaw is turned by
  atan(nx tanH) *away* from the stage side, pitch by atan(ny tanV), so the body appears at (nx, ny). Same rock test as
  before (the 6 yaw tries) now at distance d. FOV `flow.lookCamera.fov` 32 (a 50 mm-like portrait lens) instead of 40.
- The survivor faces the camera's side 20 deg off (unchanged yaw offset), so they look into the frame (lead room).
- Glide: the camera eases to the framing (as before); a resize re-frames on the next tick.

### 3.5 The audit (each screen, each canvas)
Shots in the game build with the bot (`shot showui`), at: 1920x1080 (16:9), 1440x900 (= 2560x1600 16:10 logical),
1024x768 (4:3), 2560x1080 (21:9), 1920x1080 with "Text and menus" 130%. Screens: main menu (closed and look picker
open), main menu settings, worlds (Host a game), in game: HUD, wheel, journal (inventory, crafting, map, paths, skills,
clan), build bar, settings / pause, storage, taming, hearth, claim, god panel, death and respawn, loading. Each picture
read back for: nothing off the canvas, no overlap, the primary button visible, the survivor clear of panels.
The packaged gate repeats the main menu at 2560x1600 and 1920x1080.

### 3.6 Edge cases
- Window resized while a screen is open: SWRFit and the stage framing are recomputed every layout pass / tick.
- Interface 85%: SWRFit never scales up; the canvas rule is unchanged.
- A screen whose desired size is huge on purpose (a long unscrolled list) would shrink to 0.55: the audit pictures
  show any such screen; it gets a scroll box instead.
- Gamepad navigation and mouse hit-testing go through layout transforms (as SDPIScaler), so nothing changes for them.
- Split-screen: not supported by the game (one local player), nothing to do.

### 3.7 Build steps
1. WRUIScale.h/.cpp: `UWRDPIScalingRule`; DefaultEngine.ini rule; tuning `ui.canvas.*`.
2. SWRFit in WRUIWidgets.h/.cpp; wire it at the 10 viewport roots.
3. Main menu: flow layout, path grid, live biome line, Settings/Quit row, stage + framing math; tuning
   `flow.lookCamera.*`.
4. Compile (D: engine), fix warnings; commit + push (CHANGELOG Unreleased, HANDOFF).
5. Shots at the 5 canvases; read back; fix; repeat until clean; commit + push.
6. Ship 0.8.1: version bump, release notes, package, installer, gate at 2560x1600 + 1920x1080, release, digests,
   emails, proof files, LANDED.

## Proof (filled at the end)

Requirements added from the addendum (Aaron 11-16 18:33): R21 Update now never opens a web page; R22 every package
carries WildreachUpdater.exe, found beside the game, up from the exe or in the installed copy; R23 no updater at all: the
game downloads the release's Setup, starts it with its PID to wait for, and closes; R24 the flow Update -> "Updating..."
-> close -> install (saves kept) -> new version starts, tested 0.8.0 -> 0.8.1 on the real release.

Commits (Wildreach master): 126ea86 (canvas rule, SWRFit, main menu flow + framing, update flow), 70ddb14 (render audit
fixes: Hearth, Crafting, Settings), c7e7311 (0.8.1 bump + notes), b420575 (crash card ignores ensures), 496e4fa (released).

- R1-R4, R8: done. `Source/Wildreach/WRUIFit.h/.cpp` (UWRDPIScalingRule = min(short side / 1080, width / 1760); SWRFit),
  `Config/DefaultEngine.ini` UserInterfaceSettings (Custom rule, bAllowHighDPIInGameMode=False), `Data/tuning.json`
  ui.canvas. Wired in WRPlayerUI.cpp (HUD, wheel, every screen host), WRUIFlow.cpp (covers, moments), WRUIControls.cpp,
  WRReport.cpp, WRUIPadKeyboard.cpp, WRMainMenu.cpp (layout, card, settings). The game is per-monitor DPI aware, so
  2560x1600 at 150% is a 2560x1600 pixel window and a 1760x1100 canvas (checked in the package).
- R5: done. WRMainMenu.cpp StageRect + FrameSurvivor: the camera frames the survivor into the measured stage. Shots
  m1024, m1920u130 and packaged q2560 / q1920 show the survivor head to toe to the right of the panels, with nothing
  over the body.
- R6/R7: done. Flow layout, 12-path grid, Settings/Quit row: Host / Join / Settings / Quit are on screen in every shot
  (1440x900, 1024x768, 1920x1080 at 130%, packaged 2560x1600 and 1920x1080). Navigation is the same as before (WRInput
  spatial nav over the same buttons; the grid moves by position).
- R9/R10: done. Rendered audits (editor -game, shot showui): 41 screens at 1440x900 and 41 at 1024x768; the main menu
  at 1440x900, 1024x768 and 1920x1080 at 130%; the packaged front end at 2560x1600 and 1920x1080. Fixed what the shots
  showed: Hearth (levels scroll, cores wrap as tokens; WRUIHearth.cpp), Crafting categories scroll (WRUICrafting.cpp),
  Settings nav at 262U (WRUISettings.cpp; 'Look and controller' still clips one letter), the path grid font. The clan
  and players screen fits (1024 run). The god panel is tester-only and was not rendered.
- R11: NOT DONE. There is no Electric Dreams project on the desktop (it needs the Epic launcher's Fab "Create Project").
  0.8.1 ships the forest's tree-kit fallback. Logged in LATER-COOKS.
- R12: done. The package is built from b420575, which has everything on master (answers-1116, strongholds, 100 players).
- R13: done. Tools/package.ps1 -Engine D:/UnrealEngine/UE_5.8 -Out E:/Dev/WildreachBuild6. build.json: 0.8.1, commit
  b420575, uncommittedChanges false. Only WildreachBuild5 and 6 are kept.
- R14: done. On the installed copy: F35READY menu -> F35BOOT new -> F35READY play, and F35BOOT continue -> F35READY play,
  at both 2560x1600 and 1920x1080.
- R15/R24: done (path verified). Whole Setup installed silently onto a fresh folder. A fresh 0.8.0 install's updater
  moved to 0.8.1 from the live release (19 files, 286 MB, 23 s) and reached play. The in-game click itself was not
  driven, because no newer release exists yet.
- R16: done. Release 447829422 has 4 assets; every digest equals the local SHA-256; releases/latest points to v0.8.1.
- R17/R18: done. Both emails returned HTTP 200. RELEASED.txt and EMAILED.txt are in wild-desktop, and the LANDED line
  is written.
- R19: done. CHANGELOG and HANDOFF updated in each commit; files added by name.
- R20: done. No music UI in any screen audited.
- R21: done. WRMainMenu.cpp OnUpdate / WRMenuUpdate: there is no LaunchURL anywhere in the update path (ReleasesPage
  removed).
- R22: done. Tools/package.ps1 builds WildreachUpdater.exe and version.txt into every package (verified in Build6).
  WRMenuUpdate::FindUpdater tries the root folder, each folder up from the exe, then LOCALAPPDATA/Programs/Wildreach.
- R23: done. WRMenuUpdate::DownloadSetup streams the release's Setup to TEMP/Wildreach-Update and runs it with
  `--update --wait <pid> --dir <game folder>`. Tools/installer/WildreachSetup.cs waits for that PID before installing.

