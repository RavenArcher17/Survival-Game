# build-godmode: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/build-godmode.out).

**LANDED build-godmode 7098076 (freedom35-studios) · 5aa7e69, f7c7623 (Wildreach)** (cook doc: showcase-notes a317f21, b093d06)

Wildreach now has a god mode that only Aaron, Raven and Colin get. Both builds compile with no errors or warnings, and the site is deployed. Nothing has been played. The Ed25519 signature check inside the game has never seen a real proof from the site; a node check confirms the site's proofs verify as Ed25519 the way the game checks them.

**How it decides who gets it:** there's no new sign-in. The game reads the PC link that Freedom35's updater already saves (the same one You're the Cupcake signs in with). At each join, the server sends a one-time code. A tester's game asks the new `POST /api/pc/tester` endpoint for a signed proof tied to:
- their account,
- this PC's identity key,
- that one connection (10-minute expiry).

The server checks the signature with a public key built into the game, then marks the player as a tester for everyone to see. The allow list is a new site table edited with `node tools/testers.mjs`; no email is written anywhere in either repo. Without the Freedom35 link, the game asks for nothing and god mode never turns on.

**Decisions of mine to know about:**
- **Old console commands are now locked:** commands like `tp`, `give` and `spawn` were open to every player in packaged builds. They now need a verified tester too. Editor runs and the test scripts are unchanged.
- **Builder bars aren't pinned:** "Endless vitals" leaves Heat, Chill and Charge alone. Pinning them full would set off Overheat's blast on every hit.

**Requirement ledger** (each row has its file:line in section 5 of `cooks/build-godmode.md`):
- **R1–R3 Who gets it, never by a name, PC, config or flag:** done (WRGodAuth.cpp:172, WRGod.cpp:651).
- **R4–R6 Allow list:** Aaron, Raven, and both of Colin's accounts are in `game_testers`.
- **R7–R10 Nothing for anyone else (no menu, key, command; server refuses):** done (WRPlayerUI.cpp:212, WRPadInput.cpp:533, WRUISettings.cpp:1191, WRKeyBindings.cpp:124, WRGod.cpp:617/698).
- **R11–R19 Identity from Freedom35 and the signed proof:** done (WRGodAuth.cpp:76/219/221). The site side uses the existing PC link code and was deployed with its own tool. Afterwards `/play/corelode` still redirects to login (303), the new endpoint answers 401 without a link, and the key was set without being printed.
- **R20–R21 Server decides, every action re-checked and logged:** done (WRGameMode.cpp:685/142, WRGod.cpp:687/698).
- **R22 Research:** done (Valheim, ARK, Elden Ring, Minecraft, Terraria, Satisfactory, 7 Days to Die, Rust).
- **R23–R29 Invulnerable, endless stamina/mana/hunger/thirst, one-hit kill, fly with speed, noclip:** done (WRCombatLibrary.cpp:947–948, WRCharacterBase.cpp:235, WRGod.cpp:389/312, WRCharacter.cpp:1777).
- **R30–R34 Teleport to any land, place, shrine or boss arena, listed from game data:** done (WRGod.cpp:95, :793).
- **R35–R43 Spawn any creature or boss, item, recipe; unlock paths, abilities, recipes, pieces, armour sets:** done (WRGod.cpp:812–885).
- **R44–R46, R48–R53 Level/XP, time, weather, events and invasions, quest steps, revive, kill all nearby, freeze AI, hitboxes and aggro:** done (WRGod.cpp:885–937, :748, :762, :415).
- **R47 Season:** not possible. Seasons aren't in the game yet (Dream tier, FULL-GAME.md:114), so nothing shows for them; added to LATER-COOKS.
- **R54–R56 Panel (mouse and pad), F9 hotkey, GOD badge:** done. The panel has seven pages; on a pad it's the last journal page. It shows above the death screen so Revive works while dead (WRUIGod.cpp:182/849, WRPlayerUI.cpp:252/368). Other players see "GOD" over a tester's name (WRHUD.cpp:488).
- **R57–R61 Just for testing:**
  - Any use marks the character as a test character for good; world changes mark the world too (WRGod.cpp:493).
  - A test character earns no achievements and no kill-time records (WRDeedsAchieve.cpp:102, WRRecords.cpp:202).
  - Public dedicated servers refuse it all (WRGod.cpp:590).
  - F8 reports carry a `godMode` block (WRReport.cpp:219).
- **R62–R70 Process:** done (no secrets or emails committed, compiles clean, CHANGELOG and HANDOFF updated, pushed in pieces, numbers in `Data/tuning.json:1000`, LANDED line and questions written).

**Not finished:**
- Seasons, as above.
- No "free build / free craft" switch. The panel hands out materials and a builder's kit instead; this is noted in LATER-COOKS.
- Not played or tested.

**Questions in QUESTIONS-FOR-TOMORROW.md, each with my pick already built:**
- Colin has two accounts; I added both.
- Public servers refuse god mode entirely; the alternative is allowing self-only powers there.
- Whether to keep the old testing commands locked to testers.

**New binary files:** none.
EXIT 0
