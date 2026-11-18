# cookpass-ui: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-ui.out).

LANDED cookpass-ui eb464d6 5918156 0343a21 4833f2d a37d79d 6efb4ea 661b476 0007f9b 147b14e 483536e 62b0694 f7dbe00

The UI cook pass is in: 12 commits, all pushed to master. Each part compiled clean on the editor target, and after the last pull the full unity build compiled with every file back in its blob. I ran no tests, as the brief says; you test. The research, grades, plan, filled ledger and the read-through are in `cooks/cookpass-ui.md` (showcase-notes commits af11d13 for the plan and 52af21f for the results).

**Requirement ledger** (Q = brief and BAR rules, L = LATER-COOKS lines, G = gaps from grading)
| # | Status | Where |
|---|---|---|
| Q1 research | done | doc §1: TLOU2, Horizon Forbidden West, Ghost of Tsushima, Monster Hunter Wilds, Xbox accessibility guidelines 101/107, Elden Ring, Valheim, Sea of Thieves / Destiny 2, console keyboards, character creators, item wheels, recipe pinning; plus real-world reference |
| Q2 grade every built piece | done | doc §2, with file:line for each piece |
| Q3 plan before building | done | doc §3, committed before any code (af11d13) |
| Q4 build to the plan, nothing unfinished shown | done | the 12 Wildreach commits above |
| Q5 spot-check | done | doc §4; fixes in f7dbe00 and inside parts 2, 3, 4, 9, 10 |
| Q6 file:line proof | done | every row in the doc ledger |
| Q7 design with intention | done | doc §3.10 |
| Q8 ledger | done | doc §0 |
| Q9 data-driven | done | tuning `ui.*`, `look.padCurve*`, `report.chips`, `flow.lookCamera`; `gamepad.json`; sounds.json `gear.setComplete` |
| Q10 mouse and pad | done | B/Esc handlers on the leave strip, the cards and the pin card; the pad keyboard covers every text box |
| Q11 server authority | done | helmet through `ServerSetHideHelmet`; the map reveal grid is server-side; the look preview refuses any shared world |
| Q12 land in pieces | done | 12 compiled commits (one gap noted below) |
| Q13 unity prefixes | done | full unity build after the pull compiled |
| Q14 out-of-scope finds | done | 6 lines added to the share's LATER-COOKS |
| L1 first-person FOV 75–110 | done | `WRUISettings.cpp:1572`, `WRRanged.cpp:344` |
| L2 hide-helmet toggle | done | `WRUISettings.cpp:1605`, `WRMainMenu.cpp:899` |
| L3 fill-in map at 8 m cells | done | `WRMapReveal.cpp:71/95`, `WRUIMap.cpp:261/348`, saved at `WRGameMode.cpp:560` |
| L4 reserved pad holds | done | `Data/gamepad.json:37` |
| L5 face/build/helmet screen | done | `WRMainMenu.cpp:960/1032`, `WRHeroLook.cpp:396` (live preview on the menu's survivor) |
| L6 set-complete moment | done | `WRPlayerUI.cpp:992`, `WRUIInventory.cpp:891` |
| L7 Riftback remote rift-chest screen | **not built** | see below |
| L8 Necromancy node glyphs | done | `Tools/ui_glyphs.mjs`, 44 PNGs |
| G1 camera settings (FOV, shake, blur, shoulder) | done | `WRUISettings.cpp:1572`, `WRRanged.cpp:337`, `WRFxCombat.cpp:484` |
| G2 hold or toggle for sprint/guard/aim | done | `WRCharacter.cpp:1807/1901`, `WRRanged.cpp:112` |
| G3 aim speeds, invert X, dead zone, response, vibration strength | done | `WRUISettings.cpp:1491`, `WRPadInput.cpp:191/209` |
| G4 Interface page (Desk/Couch, text and HUD size) | done | `WRUISettings.cpp:1605`, `WRKeyBindings.cpp:239`, `WRPlayerUI.cpp:53` |
| G5 HUD Full/Fade/Minimal | done | `WRUIHud.cpp:390` |
| G6 nameplates (centred, names setting, no hard-coded "E") | done | `WRHUD.cpp:431/438` |
| G7 Disconnected card with Rejoin | done | `WRMainMenu.cpp:450/543/1277` |
| G8 leave/quit asks once | done | `WRUISettings.cpp:2427` |
| G9 crash card | done | `WRReport.cpp:504/575` |
| G10 F8 quick notes | done | `WRUIReport.cpp:237` |
| G11 pad keyboard | done | `WRUIPadKeyboard.cpp:265/330`, `WRPadInput.cpp:592` |
| G12 chat on a pad | done | `WRUISettings.cpp:2495` |
| G13 map pins | done | `WRUIMap.cpp:170/1100/1715`, compass `WRUIHud.cpp:274` |
| G14 pad item wheel | done | `WRPlayerUI.cpp:1027`, `WRUIWheel.cpp` |
| G15 track a recipe | done | `WRUICrafting.cpp:1610`, `WRUIHud.cpp:741` |

**Things you'll want to know before testing:**
- **Everyone's map starts blank.** Nothing was recorded before this, so players who already played will only see the land around where they stand. A LATER-COOKS line suggests a one-off reveal around their claims, beds and found shrines if you want their known land back.
- **The default HUD is now Fade.** When nothing is happening for 6 s, the bars and plates fade out. Players who want the old always-on HUD pick Full in Settings > Interface.

**Choices I made that differ from the design doc:**
- Field of view stays 90° in both views by default; the design said 80° for third person. The combat camera, aim zoom and sprint widening were all tuned at 90.
- Map pins are saved per PC. Sharing them with a clan needs the map-table piece and a server command.
- The look picker does face, build and helmet only. Hair, beard, colours and body sliders need the art work that bakes the body-shape morphs, which stays with the hero lanes.

**Not finished (all on LATER-COOKS):**
- **L7, the Riftback's pocket.** It isn't just a screen. Rift chests far from the player aren't sent to their game at all, so the server would have to pass the shared rift inventory to the player first. I didn't build half of it; nothing of it shows in the game.
- **The rest of the earlier UI design not covered here:** creator sliders, hair and colours; 3D-anchored hint chips; the first hour's camp kits; the inventory right-click menu and compare; crafting from nearby chests; the pad build mode; the F1 controller diagram; graphics presets for 8 GB cards with auto-detect by video memory.
- **Bookkeeping gap:** commits 7 and 12 updated the CHANGELOG but not HANDOFF in the same commit. Part 7's HANDOFF note went in with part 8; part 12 is a small fix and has none.

New binary files: 44 glyph PNGs, 45 KB in total, in Git LFS.

Sources:
- [PlayStation: The Last of Us Part II accessibility](https://playstation.com/en-us/games/the-last-of-us-part-ii/accessibility/)
- [GameSpot: TLOU2 accessibility options](https://gamespot.com/articles/the-last-of-us-2-accessibility-difficulty-options-/1100-6478756/)
- [Player2: TLOU2 accessibility](https://www.player2.net.au/2020/06/exploring-the-last-of-us-part-iis-accessibility-options/)
- [Gamepressure: Horizon Forbidden West HUD](https://www.gamepressure.com/horizon-forbidden-west/hud/z6f628)
- [player.one: HFW update 1.07 HUD options](https://player.one/horizon-forbidden-west-update-107-brings-new-options-hud-146094)
- [Freakhosting: Valheim cartography table](https://help.freakhosting.com/games/valheim/using-the-cartography-table-map-sharing)
- [Better Cartography Table (Valheim mod)](https://thunderstore.io/c/valheim/p/nbusseneau/Better_Cartography_Table/v/0.1.0/)
- [GamingOnLinux: Steam Deck keyboard fixes](https://www.gamingonlinux.com/2022/09/valve-finally-fixes-up-the-steam-deck-on-screen-keyboard-some-more/page=1/)
- [Steam forum: Steam Deck keyboard placement](https://steamcommunity.com/app/1675200/discussions/2/3428948355369628271)
EXIT 0
