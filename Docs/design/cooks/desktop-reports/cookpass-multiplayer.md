# cookpass-multiplayer: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-multiplayer.out).

LANDED cookpass-multiplayer 798cffb d649695 fd15e34 fe6f1b9 a50e4a6 e929728 7d3482c 31794c4

The multiplayer cook pass is in Raven's repo, landed in seven parts plus one fix, each compiled and pushed to master. Nothing was played or tested, per the gates. It compiles cleanly with no warnings, including on top of the crafting lane's commits that landed during the night. No tag or release was made.

**What changed**
- **Doors** turn smoothly on their hinge instead of jumping 90° in one frame. They open away from whoever opens them, including through the keypad and the code breaker. They won't swing through a body ("Someone's in the way" / "Someone's in the doorway"), and an open door now blocks you like a shut one. The shutting sound plays when the latch drops.
- **Offline raid protection** used to start the moment the last clanmate logged off, so logging out mid-raid saved the base. Now it starts 15 minutes after the last owner leaves and needs 10 minutes without an attack. It ends as soon as an owner logs in, and a server restart doesn't switch it on early. The Claim tab shows the live state, raiders are told when a base is protected, and both timers are new world settings.
- **Chat** has Everyone, Nearby (40 m), Clan and whispers (`/w name`, `/r` to reply). A channel button and Tab switch between them, each channel has its own colour, and the server allows a line a second per player.
- **Hosting and joining:** a new Hosting page per world sets the player cap (2–16, default 8), a password, admins and bans. Joiners are turned away with a plain reason (banned, full, password), and a password box appears on the join card. The Clan tab's player list now shows ping, Host/Admin labels, and Kick / Ban (24 h or for good) / Make admin. Everyone sees "Mira washed ashore", "is back" or "left". When the host leaves, the others read "The host closed the world. Everything was saved" instead of "lost connection".
- **Death bags** were an engine sphere. They are now models built in code: a satchel and fur bedroll for a death, a stove-in crate for a broken chest, a tied sack for other drops. Only the owner sees a 6 m column of light over their satchel, in their path's colour. The compass shows minutes left, and death bags survive a clean restart.
- **Wake at your Hearth:** any unbroken Hearth of yours or your clan's is on the death screen. You wake beside it facing the fire, and it then rests for 5 minutes.
- **Claims** must stand 130 m apart, so they can't overlap or grow into a neighbour. The red build preview shows this before you place. Claims can be named, and the name appears in raid alerts and Hearth wake lines.
- **Backups:** the world list can now restore a backup. It asks first, and saves the current world as a backup before restoring.
- **Fix:** a dedicated server defaults to 32 players (listen worlds stay at 8).

**Requirement ledger (all done; file:line proofs are in section 6 of `showcase-notes/.../cooks/cookpass-multiplayer.md`)**
- R1 Research: §1 (Source-engine doors, Rust doors and chat, Valheim tombstone, admin tools, Seamless Co-op password, Dark Souls disconnects; hung doors, Viking satchel, crates).
- R2 Plan before building: §3–4. R3 Build: the commits above. R4 Spot-check: §5, with 10 fixes made.
- R5 Every proof has file:line. R8 Ledger: §0 and §6.
- R6 Design with intention: §3.1, 3.5, 3.6. R7 Smooth motion: door swing, `WRBuilding.cpp:563`.
- R9 Hosting: `WRUIWorlds.cpp:539`, `WRAccess.cpp`, `WRPlayerController.cpp:264`.
- R10 Joining: `WRGameMode.cpp:118`, `WRAccess.cpp:191`, `WRMainMenu.cpp:1408/1447`.
- R11 Clans: `WRPlayerController.cpp:95`, `WRUIClan.cpp:435–473`.
- R12 Doors: `WRBuilding.cpp:511/563/626/666`.
- R13 Claims: `WRBuilding.cpp:1824`, `WRUIClaim.cpp:490`, `WRBeds.cpp:94/118/284/401`.
- R14 Raids: `WRSocial.cpp:347`, `WRBuilding.cpp:1258/1895/1913`, `WRUIClaim.cpp:904`, `WRUISettings.cpp:2022`.
- R15 Persistence: `WRStore.cpp:731/755`, `WRUIWorlds.cpp:694/752`, `WRSocial.cpp:562`.
- R16 Grades: §2. R17 Music on screen: none found in these systems.
- R18 Data-driven: new tuning blocks `door`, `chat`, `access`, `claims`, plus new keys under `net`, `death`, `beds`, `store`, `raid`, and two new preset keys.
- R19 No character damage added. R20 Mouse and pad on every new control. R21 Sounds are existing table rows.
- R22 Server decides everything. R23 Nothing half-built is on screen. R24 Compile clean.
- R25 Git rules, with CHANGELOG and HANDOFF in each commit (the chat part's CHANGELOG/HANDOFF went in a follow-up commit, fe6f1b9). R26 No binaries. R27 LANDED and LATER-COOKS written.
- R28 No stops. R29 No emails anywhere. R30 Unique helper names.

**Not finished** (each is a line in LATER-COOKS):
- Automatic router port opening and join codes (join codes need Epic online-services IDs).
- Message of the day and scheduled restarts.
- Rate limits on actions other than chat, and server-side movement checks.
- A short "make camp" delay before logging out, and fleeing into a safe zone while in a fight still protects you.
- Marker posts round safe zones, and duels.
- Clan tag, colour, sigil and banner, and clans owning claims.
- The ping / quick-chat wheel.
- Empty claims lapsing, and claim edges in build mode and on the map.
- Network dormancy for building pieces.
- Moving a satchel that falls into lava.

**New binary files:** none (0 bytes).

**Questions for tomorrow**
- The players list lives in the Clan tab (O) rather than a hold-Tab overlay, because Tab is lock-on. The other option is a new key plus the View-hold on the pad, which is already reserved for it.
- Claims now stand 130 m apart. On the island that may be too far; the other option is spacing only between different owners.
- The Hearth rests 5 minutes after you wake there, and a dedicated server defaults to 32 players. Both are first drafts in the tuning file.
EXIT 0
