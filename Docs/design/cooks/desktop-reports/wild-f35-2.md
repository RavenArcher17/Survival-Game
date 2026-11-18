# wild-f35-2: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/wild-f35-2.out).

You've hit your weekly limit · resets 12am (America/New_York)

You've hit your weekly limit · resets 12am (America/New_York)
EXIT 1
You've hit your 5-hour limit · resets 1:40am (America/New_York)
EXIT 1
LANDED wild-f35: Wildreach 2682187, 260820f · site 06475f8, 6f1cd69 · notes 55f7768

Wildreach is live on Freedom35 at **https://freedom35studios.pages.dev/g/wildreach**, listed under Raven and the Play Pass, at version 0.8.2.2. The download is a 2.1 MB installer. On first start it downloads the game's 2,953 files (3.09 GB), checks each one and starts the game; after that it fetches only the files that changed. It is installed on this PC for Aaron, and there is a "Wildreach" shortcut on his desktop.

**What I did since the restart.** The earlier run had already shrunk the game, built the small installer, made the store page and installed 0.8.2. I continued from those commits:
- **One-file update proven.** The 0.8.2.1 test version changed only `version.txt`. The updater log says "1 of 2953 files differ (0.0 MB)" and it applied that one file.
- **Quit crash found and fixed.** Every quit, in 0.8.1 as well, ended in a heap-corruption crash (0xC0000374), so F35Update sent a crash report each time a player closed the game. Under the debugger the cause was the main menu, kept in a static variable, being destroyed after the engine's text library had already shut down. The fix releases it while the engine is still running (`SWRMainMenu::ReleaseAtExit` in `WRMainMenu.cpp`, hooked up in `Wildreach.cpp`).
- **Fix tested.** The game exits with code 0 when quit from the menu and from a world, with no crash under the debugger.
- **Fix published as 0.8.2.2.** Only 9 files were uploaded. Aaron's install updated 6 files (184.9 MB), and its next quit logged "game exited 0". I deleted the two crash reports my tests sent.
- **Cleanup.** I removed the test copies, the extra Development build folder, and the "Boot check" world my checks had left in this account's real save folder.

**Sizes**
| | 0.8.1 | 0.8.2.x |
|---|---|---|
| Package on disk | 4,368 MB | 3,086 MB (2,953 files) |
| Debug symbols (.pdb) | 499 MB, sent to players | not shipped (344 MB kept locally) |
| Exe | 350 MB | 184 MB |
| Game content | 3,082 MB in one file | 2,545 MB in 47 files of up to 60 MB |
| Installer | 3.58 GB setup | 2.1 MB (2,223,621 bytes) |
| First-install download | — | 3,085.5 MB |
| Update for a one-file change | — | 5 bytes (1 file) |

The per-item content breakdown is in `CHANGELOG.md`.

**Boot proof:** the site's boot check passed at the "play" level for each installer (0.8.2.2 setup sha `4563ea07…`). From the installed copy's check: `F35READY menu` → `F35BOOT new world 'Boot_check'` → `F35READY play`, then `F35READY menu` → `F35BOOT continue` → `F35READY play`.

**Requirements:** all 31 rows are done. File:line proof for each is in `cooks/wild-f35.md` in showcase-notes.
- **Listing and store page (R1–R3, R16–R20):** `db/wildreach.sql`. The page lists Windows 10/11 64-bit, a DirectX 12 graphics card, and the installer and download sizes. Rated teen, with a cover and 10 screenshots taken from the packaged game.
- **Smaller game (R5–R9):**
  - Players get the Shipping build by default (`Tools/package.ps1:13`) and debug symbols stay on this PC (`:12`).
  - Only the shrubs the game uses are packaged, and the face-capture tool is left out (`DefaultGame.ini:14`, `:31`).
  - Pack textures are capped at 2K (`DefaultDeviceProfiles.ini:9`).
  - The forest kit is included because every one of its 57 meshes is used by the forest.
  - Content is cut into files of at most 60 MB (`DefaultGame.ini:39`). Only the exe is over 64 MB, and it uploads in parts.
- **Site file store and updater (R4, R10–R14, R23):**
  - Large files can be stored in parts, and a version can now hold up to 5,000 files (`desktopfiles.js:24` plus a database migration).
  - `f35 installer --files-only` builds the small installer.
  - The download-everything mode in `F35Update.cs` is off unless a game's config asks for it (`:77`). Inside it the updater links the PC, checks free disk space, joins a big file's parts and checks the whole file's SHA-256.
- **In-game update and no GitHub (R15, R27, R28):**
  - On a Freedom35 install, "Update now" hands over to F35Update and only Freedom35 is asked for new versions (`WRMainMenu.cpp:1652`, `:1749`).
  - There is no mention of GitHub on the store page, in the installer, in the installed files or in anything the game shows.
- **Proof install and cleanup (R21, R22, R24–R26):** done as described above. The install reused this PC's link to Aaron's account, made through the normal link step on 2026-10-04; nothing was faked.
- **Other games and process (R29–R31):** the new installer mode is used only by Wildreach, so other games keep their old path. Every commit updated CHANGELOG and HANDOFF.

**Not finished**
- There is no GitHub release of 0.8.2. The lane brief forbids one, so I did not follow the SHIP-7 note's GitHub-release, release-email and questions-page steps, which contradict this brief.
- Making the GitHub repo private is Raven's step.
- I didn't re-run Cupcake or other games through the new F35Update. Their path is unchanged in the code, which only switches mode when a game asks for it.
- Packaged builds started with the boot-check option still write their "Boot check" world into the player's real save folder, so it has to be deleted after a check (now in HANDOFF's release steps).

**New binary files:** `Tools/installer/Wildreach.ico` is 101 KB in the Wildreach repo. The cover and 10 screenshots (about 4 MB together) went into the site repo, not Wildreach's LFS.

**Emails:** none sent and none drafted; this lane's brief names no email.
EXIT 0
