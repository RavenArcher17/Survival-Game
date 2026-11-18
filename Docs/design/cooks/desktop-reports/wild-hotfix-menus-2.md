# wild-hotfix-menus-2: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/wild-hotfix-menus-2.out).

LANDED wild-hotfix-menus 126ea86 70ddb14 c7e7311 b420575 496e4fa

Wildreach 0.8.1 is released and both release emails are sent. The menus now fit every screen size I rendered. "Update now" no longer opens a web page. One thing missed: the Electric Dreams forest kit isn't on this desktop, so the Ancient forest ships its old tree look.

**Release**
- **Version:** 0.8.1. Release: https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.8.1, built from b420575 into `E:\Dev\WildreachBuild6`.
- **Digests:** all 4 assets (.001, .002, .exe, .parts) have a GitHub digest equal to the local SHA-256. `releases/latest` now points to v0.8.1.
- **Playable gate:** I installed the final Setup on a fresh copy and ran it at 2560x1600 and at 1920x1080. Each size logged `F35READY menu` → `F35BOOT new world` → `F35READY play`, and on a second run `F35BOOT continue world` → `F35READY play`.
- **Updating from 0.8.0:** a fresh 0.8.0 install updated itself from the live release (19 files, 286 MB, 23 s) and then reached `F35READY play`.
- **Emails:** Aaron and Raven each got one line, "Wildreach 0.8.1: <link>". Both returned `{"ok":true}` with HTTP 200.
- **Proof files:** RELEASED.txt and EMAILED.txt are written in `wild-desktop`, with the line `WILDREACH RELEASED 0.8.1 <url>` for the coordinator. The LANDED line and a later-cook line are written too.

**Screen fixes**
- **Cause:** the interface scaled by screen height only, so 16:10 and 4:3 screens got a canvas narrower than the biggest windows. Since 0.8.0, the main menu also listed 12 paths in one column, which pushed Host/Join/Settings/Quit off the bottom. The panels also sat where the survivor was framed.
- **Fix, for every screen:**
  - A new scaling rule keeps the interface at least 1760x1080. On 16:9 and 21:9 nothing changes.
  - Every menu and the HUD now shrink to fit when they would otherwise run off the screen.
  - The main menu is laid out by flow: the 12 paths are a 3-across grid and Settings and Quit share a row. The survivor is framed head to toe in the open space beside the panels.
- **Checked by rendering:**
  - 41 in-game screens at 1440x900 (the laptop's 16:10 shape) and again at 1024x768.
  - The main menu at 1920x1080 with 130% interface size.
  - The packaged game's main menu at 2560x1600 and 1920x1080.
- **Fixed from those pictures:**
  - The Hearth's level list ran out of its window; it scrolls now. Its ten boss cores ran over each other; they now wrap in rows.
  - Crafting's category list ran under the window's foot; it scrolls now.
  - The Settings section list was widened.
  - The "Wildreach closed unexpectedly" card mistook a harmless engine check for a crash; it now ignores those.

**Update now**
- It shows "Updating…", closes the game, and the updater installs the new version and starts it again.
- It finds the updater beside the game, in folders above it, or in the installed copy.
- If there is no updater at all, the game downloads the release's Setup itself. The Setup now waits for the game to close before installing.
- Every package now carries the updater. 0.8.0's staging folder didn't, which is why your click opened the GitHub page.

**Requirements (proof per row is in `cooks/wild-hotfix-menus.md`)**
- **R1–R8, done:** layout on every screen shape, resolution and scaling; the survivor framed clear of the panels; buttons always on screen. Main code is in `WRUIFit.h/.cpp`, `DefaultEngine.ini` and `WRMainMenu.cpp`.
- **R9–R10, done:** every screen audited by rendering; fixes in `WRUIHearth.cpp`, `WRUICrafting.cpp` and `WRUISettings.cpp`.
- **R11, not done:** the Electric Dreams forest kit (see below).
- **R12–R19, done:** packaging, playable gate, installer test, release, emails, proof files, CHANGELOG and HANDOFF. The new forest look is the one exception in R12 (see R11).
- **R20, done:** no music shown on screen in anything audited.
- **R21–R24, done:** the Update now fix, in `WRMainMenu.cpp`, `WildreachSetup.cs` and `package.ps1`.

**Not finished**
- **Electric Dreams forest (R11):** the kit isn't on this desktop. Getting it needs the Epic launcher's Fab "Create Project", which can't be done from a script. I logged it in LATER-COOKS for 0.8.2.
- **In-game Update now click:** I tested the update path itself, but not the click in the game. It can only be clicked toward a release newer than 0.8.1.
- **Your installed 0.8.0:** its own Update now still has the old code. Installed copies have the updater beside them, so they update correctly. Only a 0.8.0 run from a staging folder still opens the page.
- **Settings label:** "Look and controller" still loses its last letter at some sizes. Logged.
- **God panel:** not rendered, because it opens only for verified testers. It uses the same journal window as the screens that were checked.

**New binary files:** none in git. The release assets are about 3.76 GB, hosted on GitHub Releases rather than LFS.
EXIT 0
