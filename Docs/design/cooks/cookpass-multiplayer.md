# Wildreach cook pass: MULTIPLAYER (hosting, joining, clans, doors, claims, raids, persistence)

Lane: cookpass-multiplayer (desktop). Written 2026-11-15. Repo: E:\Dev\Wildreach (Raven's repo, master).
Built features get the full cook (Aaron 11-13). This doc builds on two earlier lanes and does not redo them:
`cooks/multiplayer.md` (the full design for section 7, 2026-11-14; its research table 1.1 stands and is used here) and
`cooks/build-server-raids.md` (built the SQLite world store, identity keys, backups, explosives, the raid bell and the
siege ladder). Where this pass takes a design from multiplayer.md it says which section. Numbers are first drafts in
`Data/tuning.json` and `Data/server_presets.json`.

---

## 0. Requirement ledger

The brief, every Aaron quote in it and the lane rules, split into one requirement per row. The proof column is filled
at the end (section 6) with file:line or the commit.

| # | Requirement | Source |
|---|---|---|
| R1 | Research how the best games handle each piece (games, places and systems named) plus real-world reference, written down with the details used | Aaron 11-13 night |
| R2 | Plan every detail and design gap in writing before building, with every build step listed | Aaron 11-13 night |
| R3 | Build exactly the plan at full detail: no placeholders, no primitives, nothing "good enough" | Aaron 11-13 night |
| R4 | Spot-check against the plan and THE BAR by reading the code, then fix what falls short before pushing | brief |
| R5 | "Already done" only with file:line proof that it shows in his build | brief |
| R6 | Design with intention: for each object, its job, what it connects to and how it meets everything it touches | Aaron 11-14 |
| R7 | Machines look engineered; moving parts move smoothly and never snap or teleport | Aaron 11-14 |
| R8 | A requirement ledger, with every row proved at the end | Aaron 11-14 |
| R9 | Cook pass: **hosting** | scope |
| R10 | Cook pass: **joining** | scope |
| R11 | Cook pass: **clans** | scope |
| R12 | Cook pass: **doors** | scope |
| R13 | Cook pass: **claims** | scope |
| R14 | Cook pass: **raids** | scope |
| R15 | Cook pass: **persistence** | scope |
| R16 | Grade every built piece against THE BAR | scope |
| R17 | No music on screen and no music-driven mechanic in this scope; remove any found | Aaron 11-15 |
| R18 | Data-driven: numbers in Data/*.json and tuning.json | BAR |
| R19 | All damage through ApplyHit | BAR |
| R20 | Screens through WR_REGISTER_SCREEN / JOURNAL_TAB, with mouse AND pad | BAR |
| R21 | Sounds and effects as table rows | BAR |
| R22 | Server authority, replication and multiplayer edge cases | BAR |
| R23 | Anything unfinished stays hidden | BAR |
| R24 | Compile gate only; fix every error and warning introduced; no tests | brief |
| R25 | Git: pull before and after, add files by name, CHANGELOG "Unreleased" + HANDOFF in the same commit, land in pieces, never a release or tag | brief |
| R26 | Report the size of any new binary files | brief |
| R27 | A LANDED line; out-of-scope finds go to LATER-COOKS | brief |
| R28 | Never stop for a question: decide, build, and list the questions in the final message | brief |
| R29 | Email addresses stay private | lane note |
| R30 | Unity build: file-scope helpers get unique prefixes | lane note |

---

## 1. Research

### 1.1 Already researched (multiplayer.md 1.1, used as written)
ARK's Offline Raid Protection (it arms only after the whole tribe has been offline for 15 min), Rust's sleepers, tool
cupboard and insiding, V Rising's presets, Conan's raid windows, Palworld's server ini, Dune: Awakening's 30 s
security-zone exit, Once Human, Monster Hunter's SOS flare, Elden Ring Seamless Co-op's password gate, EOS P2P, UE
movement checks, and Fortnite building dormancy. Its lessons 1 to 7 (multiplayer.md 1.3) stand.

### 1.2 New for this pass

| Game / system | What it does | What we take |
|---|---|---|
| **Source engine `prop_door_rotating`** (Half-Life 2, Portal, CS:S) ([Valve dev wiki](https://developer.valvesoftware.com/wiki/Prop_door_rotating)) | A hinged door normally swings **away from whoever opened it**, unless something blocks that side. It rotates over a set time, and a blocked door stops instead of passing through the blocker | Our doors swing away from the opener, over a fixed time, and never through a player. If that side is blocked they swing the other way; if both sides are blocked they don't open, and say why |
| **Rust doors** (community: a door should open away from the placer, so its strong face points out ([Steam thread](https://steamcommunity.com/app/252490/discussions/0/490125103628700293))) | Doors are the core of base defence, and which way they open matters to how players build | The leaf's hinge stays on the side it was built with. Only the side it swings to changes, and the closed leaf is the same whichever side opened it |
| **Valheim tombstone** ([mein-mmo](https://mein-mmo.de/en/valheim-what-happens-when-you-die-the-death-in-the-game-explained,655047), [gamewatcher](https://www.gamewatcher.com/valheim-death-penalty)) | Death leaves a tombstone with your name that holds everything. It floats at sea, and a map marker points to the newest one. The lost-tombstone patch (thegamer) shows how much players care about it surviving | The death bag becomes a modelled object you can recognise (the satchel and bedroll, multiplayer.md 2.13.5). Only you see the column of light, and you get a map pin with time left. It survives a clean restart (2.4 "bags saved on clean shutdown") |
| **Rust chat** ([Facepunch wiki](https://wiki.facepunch.com/rust/global_text_chat)) | Global and team chat; Tab switches channel while typing; team chat is private to the team; global can be muted | Global, Local (40 m), Clan and Whisper. Tab cycles the channel in the chat box, typed `/g /l /c /w name /r` work on any keyboard including the pad's, and each channel has its own colour (multiplayer.md 2.6) |
| **Rust / Valheim admin tools** ([vibegames moderation](https://vibegames.com/help/rust/how-to-moderate-your-rust-server)) | An admin player list shows each player's ping and time online, with kick and ban on the row | A Players list in the Clan tab (rows already exist): ping, Host/Admin chips, and admin Kick / Ban 24 h / Ban / Make admin, plus a Banned list with Unban |
| **Elden Ring Seamless Co-op, Valheim server password** | One shared password is the trusted way friends get in | The world's password lives in its `access.json`, stored as a salted hash. The joiner's game sends a hash, never the typed text. Three wrong tries from one address in 60 s locks that address out for 60 s |
| **Dark Souls / Elden Ring disconnects** (cookpass-ui 1.1) | One plain sentence for each failure | New sentences: "This world needs a password", "Wrong password", "World full (8/8)", "Banned from this world (21 h left)", "Kicked by the host", "The host closed the world. Everything was saved." |

### 1.3 Real-world reference

| Source | Detail used |
|---|---|
| **Hung doors** (ledged-and-braced plank doors on strap hinges; a medieval gate leaf weighs 150-400 kg) | A hand-pushed door gets a quick push and then slows (ease-out), coming to rest at about 90° against its stop with a small bounce back (~3°). Pushed shut, it speeds up (ease-in), hits the frame, the latch drops, and it bounces about 1.5° for a tenth of a second. A plank door takes ~0.9 s; a 3 m gate leaf, three times the mass, ~1.6 s. The latch sound plays **when it shuts**, not when you press |
| **Norse thing-peace / outlawry** (multiplayer.md 1.2) | A ban is an outlawry from one world: the name and the address are both turned away, for a set time or for good |
| **Ships' bells** (multiplayer.md 1.2) | The raid bell already built (build-server-raids) |
| **The traveller's pack-roll** (a hide satchel strapped to a rolled fur bedroll; Viking-age leather satchels with a flap and toggle, as found at Hedeby; tump-lines) | The death satchel: a hide body with a flap and a bone toggle, two straps around a fur bedroll, a loose strap trailing, and a waterskin and a pouch spilled beside it |
| **Packing crates / split barrels** (nailed board crates that burst along the nails) | The spilled chest: a split crate with a stove-in side, two boards fallen off, and a hide sack beside it |

---

## 2. Grades: what's built, against THE BAR

| Piece | Where (as found) | Grade | What falls short |
|---|---|---|---|
| Host a world (listen, worlds list, settings) | `WRUIWorlds.cpp`, `WRMainMenu.cpp:1110` OnHost | B | No password, no player cap, no admin, no kick or ban (no `access.json`; `PreLogin` only checked identity, `WRGameMode.cpp:95`) |
| Host leaves | `WRPlayerController::ReturnToMenu` | C | Clients just lose the connection and read "Lost the connection to the host." They're never told the host closed the world or that it was saved |
| Join by address, Rejoin card, version gate | `WRMainMenu.cpp` (cookpass-ui) | B+ | No password prompt; no reason text for full / banned / kicked |
| Identity (name keys) | `WRIdentity.cpp` (build-server-raids) | A | Fine. Admins can't release a name yet (multiplayer.md 2.2.2), see section 5 |
| Clans (found, invite, join, leave, kick; Clan tab) | `WRSocial.cpp`, `WRUIClan.cpp` | B | Leader-only roles (ranks are biome-crystal's R9 PvP pass, not this one). The clan has **no chat of its own**, and chat is global only (`WRPlayerController.cpp:95`) |
| Chat | `ServerChat` / `SWRChatBox` / HUD 6 lines | C | One channel; 8-line history; no local, clan or whisper; no rate limit (a client can flood everyone) |
| **Doors** | `AWRBuildPiece::OnRep_Door` `WRBuilding.cpp:507` | **D** | **The leaf teleports 90° in one frame** (BAR: never snap). It always swings to the same side, even into the person opening it. The open leaf has no collision, so you walk through it. The close sound plays on the press, not when it shuts. Nothing checks for a player standing in the swing |
| Claims (totem, radius, upgrade, upkeep, decay, bell) | `WRBuilding.cpp` AWRClaimTotem, `WRUIClaim.cpp` | B | Claims can nest or overlap (no gap rule). Claims have no name |
| **Offline raid protection** | `TakeBuildDamage` `WRBuilding.cpp:1053` (`IsAnyoneOnline`) | **D** | Turns on **the instant the last clanmate logs off**, so logging out mid-raid protects the base (ARK's lesson, multiplayer.md 2.8.1). The Claim tab shows only a fixed rule line, not the state, and raiders aren't told |
| Raids (creature raids, player raiding, bell, away card) | `WRRaids.cpp`, `WRRaiding.cpp` | A- | Good. The only gap is the protection above |
| Persistence (SQLite store, backups, import) | `WRStore.cpp` | A- | No way to restore a backup from the game (multiplayer.md 2.4 "Restore a backup"). Death bags are lost on any restart |
| **Death bag / spilled chest** | `AWRLootBag` `WRSocial.cpp` ctor | **F** | **An engine sphere with the basic-shape material** (a primitive: BAR fail). No owner beacon, no map pin. The same sphere stands for boss loot, overflow drops and spilled chests |
| Wake at the Hearth | `WRBeds::FillWakeChoices` | C | The Hearth isn't a wake point (FULL-GAME 2 lists it; multiplayer.md 2.13) |
| Music UI in scope | grep: no staff, beat or rhythm UI in hosting, joining, clans, doors, claims, raids or persistence code | n/a | Nothing to remove (R17) |

---

## 3. Design (every gap solved)

### 3.1 Doors that swing (R12, R7)
- **Why the door is there:** it's the only part of a wall you can pass through, the one place a raid comes in
  (charges, the ram, the code breaker), and the thing friends open many times an evening. It has to feel solid.
- **What it connects to:** a leaf on strap hinges at the frame's −X jamb (`LeafBlocks`, the frame in
  `ApplyLook`). Its code lock (the keypad, built), the ram and charges (`WRRaiding.cpp:648`, which blow it open), and build copy.
- **How it moves:** the server decides `bOpen` and `DoorSide` (+1 swings to the leaf's +Y side, −1 to −Y). Every
  machine (server included, for collision) turns the leaf about the hinge line from its current angle toward the
  target over `door.swingSeconds` 0.9 (a gate: `door.gateSwingSeconds` 1.6).
  - Opening: ease-out cubic to `door.openDeg` 90, overshooting by `door.openBounceDeg` 3 and settling back over the
    last 25% of the time.
  - Closing: ease-in quad into the frame; at 0° the latch drops (sound `door.close`), then a rebound of
    `door.latchBounceDeg` 1.5 that dies out over `door.latchBounceSeconds` 0.12.
  - The leaf never jumps. A new order mid-swing turns it back from where it is, over the share of the time that's left.
- **Away from the opener:** the side is the opposite of where the opener stands in the door's own space (the sign of
  local Y). Before opening, the server checks that quarter-circle (hinge-centred, radius leaf width + 30 cm, on that
  side) for any pawn other than the opener. If it's blocked it tries the other side, and if both are blocked it
  refuses: "Someone's in the way". Closing is refused while a pawn stands in the swept quarter or in the doorway
  (|local Y| < 45 cm across the opening): "Someone's in the doorway".
- **Collision:** the leaf collides when it's at rest, open or shut, so nobody walks through an open door. While it
  swings it doesn't collide, so it never shoves a body. Raiders' charges stay on the closed leaf as built.
- **Sound:** `door.open` when the swing starts; `door.close` at the latch moment. Both are existing rows in
  `sounds.json`, so there are no new files.
- **Joining or first sight:** a door that replicates in already open is drawn open at once (no swing on first appearance).
- **Other openers** (the raid blast, build copy): the +1 side unless it's blocked, then −1, with no refusal (a blast doesn't wait).

### 3.2 Offline protection that arms (R14, multiplayer.md 2.8.1)
- **Rule:** a claim (or an unclaimed piece) is protected only when none of its owners (the owner and their clan) is
  online, **and** at least `server.offlineArmMinutes` (15) have passed since the last of them left, **and**
  `server.offlineQuietMinutes` (10) have passed since the claim was last attacked. It disarms the moment one of them logs in.
- **When they left:** the server notes each logout (UTC). For anyone not seen leaving this session, it uses their
  account's LastSeen from the world store, which is now written on logout as well as at join
  (`WRIdentity::NoteLeft`). Worlds with no accounts (JSON test worlds) use the server's start time. A restart is never a free arm.
- **Attacked:** any damage to a piece in the claim from a source that isn't an owner sets the totem's
  `LastAttackedUtc`, which is saved with the world.
- **Shown:** the totem replicates `ProtectState` (0 off on this world, 1 an owner is home, 2 arming, 3 protected) and
  `ProtectArmsAt` (server world time). The Claim tab's rule line becomes live:
  - "Unprotected: one of you is home."
  - "Arming: protected in 11 min (15 min after the last of you leaves)."
  - "Quiet time: it was attacked, so protection waits 10 quiet minutes (7 min left)."
  - "Protected: 75% less damage while you're all away."
- **Raiders told:** the first hit on a protected claim, at most once a minute per raider, notifies them:
  "Protected (75%): its owners are away." Raiders can always see whether a base is protected.
- **Settings:** two new rows under Offline raid protection on the world settings screen (0 to 60 min, in steps of 5).
  The presets: pvp_standard 15 / 10; the rest have protection 0, so the timers don't matter there.

### 3.3 Chat channels (R11, multiplayer.md 2.6)
- **Channels:** Global (everyone), Local (`chat.localRangeCm` 4000 around you), Clan (refused with a reason if you
  have no clan), Whisper (`/w name text`; `/r text` replies to the last whisper).
- **Chat box:** a channel chip at the left of the line, which you can click (mouse). Tab cycles Global → Local →
  Clan (Clan only in a clan), and the hint names the channel. Typed prefixes `/g /l /c /w /r` work everywhere,
  including on the pad keyboard, whose hint says "Global: /c clan, /l nearby, /w name". The channel you used last is remembered.
- **Lines:** 50 kept (`chat.history`); the HUD shows the last 6 for 25 s. Colours (sRGB): global ice (217,235,250),
  local warm grey (196,186,168), clan green (WRUI::Good), whisper lilac (199,166,224), system brass. Tags: "[Local]
  Mira: ...", "[Clan] Mira: ...", "Mira whispers: ...", "To Mira: ...".
- **Server guard:** at most `chat.perSecond` 1 per second with a `chat.burst` 5 burst per player. Over that you get
  "Slow down: chat is limited to one line a second". 200 characters.

### 3.4 Hosting and joining access (R9, R10, multiplayer.md 2.1 / 2.2.1 / 2.5 / 2.15)
- **`Saved/Worlds/<id>/access.json`** (outside the store so the menu can edit it without the world running; written
  atomically, tmp then move): `maxPlayers` (listen 2 to 16, default `net.listenDefaultPlayers` 8; dedicated up to
  100), `passSalt` + `passHash` (SHA-256 of salt + the wire hash), `admins` (names, held by identity), `bans`
  (`{name, ip, until (UTC, 0 = for good), by, why}`).
- **On the wire:** the joiner sends `?WRPass=<SHA-256("wildreach-pass-1:" + typed)>`. The typed text never leaves the
  PC and is kept in memory only, for Rejoin.
- **PreLogin order:** banned (name or address) → full → password → identity (built). Reasons, prefixed so the menu
  knows to show the password box: `PASSWORD|This world needs a password.` / `PASSWORD|Wrong password.`; "Too many
  wrong passwords: wait 60 s"; "World full (8/8)"; "Banned from this world (21 h left)" / "Banned from this world".
  The engine's GameSession cap is set to the same number.
- **Host panel (worlds window):** for the selected world, a Hosting block with **Players** (a − / + stepper from 2 to
  16) and **Password** (a box shown as dots, with Set and Clear). The menu writes `access.json`. Pad: the window's
  focus path; the stepper takes D-pad left / right.
- **Join card password box:** when the failure reason starts `PASSWORD|`, the Disconnected card shows a password box
  and a Join button (A on a pad; the pad keyboard opens on the box). The rest of the line is the card's text.
- **Admins:** the listen host is always the host; names in `admins` are admins. `AWRPlayerState::AccessRole`
  (0 player, 1 admin, 2 host) replicates.
- **Players list (Clan tab's right column, renamed "Players in this game"):** every row shows a ping chip (green
  < 80 ms, amber < 160, red above), a Host or Admin chip, and its clan. An admin also gets **Kick**, **Ban** (a
  one-row choice: 24 h / For good / Cancel) and **Make admin** / **Remove admin** (host only). Below that, admins see
  **Banned** with Unban. Commands go through `admin kick|ban|unban|op|deop`, checked on the server. The bans reach
  admins only (`COND_OwnerOnly`, filled for admins).
- **Kicked or banned player:** `GameSession->KickPlayer` with the reason, so their card reads "Kicked by the host" /
  "Banned from this world (24 h)".
- **Arrivals:** everyone else is told "Mira washed ashore" (a new character), "Mira is back" or "Mira left".
- **Host closes the world:** when the listen host leaves with others connected, every client is told
  `ClientHostClosing` first, and everyone is saved and the store flushed. The client's card then reads "The host
  closed the world. Everything was saved." with no Rejoin (there's nothing to rejoin).

### 3.5 The death satchel and the dropped bags (R3, R6, multiplayer.md 2.13.5)
- **Kinds** (`AWRLootBag::Kind`, replicated): Death (your belongings), Spill (a broken chest's contents), Drop (loot,
  overflow, a boss's spoils).
- **Modelled in code** with the tool kit (`FWRToolMesh` lathe, loft and tube, `M_HeldTool` vertex colour and roughness).
  No engine shapes.
  - **Death satchel** (~70 cm long): a rolled fur bedroll (a lathe with a spiral seam showing the roll, a fur
    colour in two tones), a hide satchel lying against it (lofted with rounded corners, a flap lofted over it, a bone
    toggle), two straps around the roll (tubes) with one strap trailing loose on the ground, and a small cord-tied
    pouch and a stoppered waterskin spilled at its side.
  - **Spill:** a split crate (planks lofted with nail rows, the front stove in at an angle, two loose boards on the
    ground) with a hide sack slumped against it.
  - **Drop:** a tied hide sack (lathed with a pinched neck and a cord band) with a fur bundle beside it.
- **How it meets the ground:** at spawn the server traces down from 1 m above to 3 m below, sits it on the hit and
  tilts it to the slope. There's no collision with pawns (built behaviour); it's found by distance, as before.
- **Owner's beacon** (Souls-like: a bag only you can take): a column of soft light `death.beaconHeightCm` 600 tall
  over the satchel. A tapered lathe tube in `M_WindupGlow` (Color in the main path's colour, low opacity) with a
  gentle pulse, and a small point light. **Only its owner's game draws it** (the local player's name against
  `OwnerName`), out to `death.beaconRangeCm` 20000. Others see just the satchel.
- **Map / compass pin:** the HUD compass shows "Your belongings" with minutes left for your own Death bag (from the
  bag's replicated `ExpiresAt`).
- **Saved across a clean restart:** SaveWorld writes every Death bag (owner, owner-only, items, transform, seconds
  left) into world doc `bags` and LoadWorld brings them back. A crash loses at most the last minute's changes, as with everything else.

### 3.6 Wake at the Hearth (R13, multiplayer.md 2.13)
- Every **unbroken** Hearth whose piece you share (`CanShare`) is a wake choice, "Hearth (<claim name or owner>)", listed after
  beds and before camps. After you wake there it rests `beds.hearthRestSeconds` 300 for you (per player). A broken
  Hearth is left out (it's cold).
- **Where you stand:** the first of 8 points 160 cm around the Hearth (starting from its front) with clear
  standing room: a capsule sweep that's free, with ground under it within 120 cm. Failing that, on top of the Hearth's piece (`SpotAbove`).

### 3.7 Claims: no nesting, and names (R13, multiplayer.md 2.7)
- **Gap:** a new totem is refused if its full radius at max upgrade (`ClaimMaxRadius`) plus `claims.gapCm` 1000 would
  reach another claim's max radius: "Too close to another claim (needs 130 m between totems)". An upgrade can then
  never reach into a neighbour.
- **Name:** the totem piece's `CustomName` (already saved and replicated) is the claim's name. The Claim tab gets a
  Rename button (the chest's Rename box). The name is used in the Claim tab title, the Hearth wake label and the
  raid notice.

### 3.8 Persistence: restore a backup (R15, multiplayer.md 2.4)
- In the worlds window, the selected world gets **Backups**: the newest 12 snapshots in `backups/` (when, kind, size).
  **Restore** asks once ("Replace <world> with the 14 Nov 22:00 backup? The world as it is now is kept as a backup
  first.") and works only while the world isn't running (the menu). Restoring copies the current `world.db` to
  `backups/before-restore-<time>.db`, then copies the backup over `world.db` and removes any `-wal` / `-shm` files.

### 3.9 Edge cases (all handled in the build)

| Case | Handling |
|---|---|
| Two players press E on one door in the same frame | The server takes the first; the second toggles it back from wherever it is (no snap) |
| A door is destroyed mid-swing | Tick stops with the actor; nothing is left behind |
| An admin bans themselves or the host | Refused: "You can't ban the host" / "You can't ban yourself" |
| Kicked while carrying a souls bag claim | Bags are world objects; nothing changes |
| A clanless player picks Clan in the chat | "You're not in a clan" notice, and the line is kept unsent in the box |
| Whisper to someone not here | "Nobody called Mira is here" |
| The password is changed while players are in | Only new joins are checked |
| Max players lowered below the current count | Nobody is kicked; new joins are refused until there's room |
| A restart while owners are offline | Arming counts from their LastSeen, so it's never a free instant arm |
| Two totems placed at once within the gap | The server checks at placement, one after the other, so the second is refused |
| A satchel's owner renames? | Names are held by identity; the bag uses the owner's name |
| Restore while the world is running in another game | The menu only restores worlds it isn't hosting (store files are open exclusively; the copy fails safely and says so) |

---

## 4. Plan: build steps (one commit and push per part)

**Part A: doors.** `WRBuilding.h`: `DoorSide` (ReplicatedUsing OnRep_Door), `DoorAngle`, `DoorFrom`, `DoorTo`,
`DoorT`, `DoorDur`, `bDoorSnapNext`; `Tick` override (disabled unless swinging); `PoseLeaf(angle)`; `TryOpenFrom(Pawn, Why)`;
`DoorQuarterBlocked(side, ignore)`. `WRBuilding.cpp`: constructor tick setup, `OnRep_Door` → start swing, Tick
curve + sounds + collision, Interact uses the side and blocking rules, `SetOpen` picks a free side. Tuning `door.*`.

**Part B: offline protection.** `WRSocial.h/.cpp`: `NoteLeft/NoteJoined`, `LastLeftUtc`, `ProtectionState(World,
Owner, LastAttackedUtc, OutArmsAtUtc)`. `WRIdentity`: `NoteLeft` writes the account's LastSeen. `WRBuilding`: totem
`LastAttackedUtc` (saved "attackedAt"), `ProtectState`, `ProtectArmsAt`, computed in Tick; `TakeBuildDamage` uses
it, records attacks and tells raiders. `WRGameMode` Logout / PostLogin note it. `WRUIClaim` live line. Presets,
`WRWorlds` keys, `WRUISettings` rows.

**Part C: chat channels.** `WRPlayerController`: `ServerChat(Channel, Target, Message)`, `ClientChat(From, Message,
Channel)`, `FChatLine.Channel`, 50 lines, token bucket, `ChatChannel` memory, `LastWhisperFrom`. `SWRChatBox`: chip,
Tab, prefixes, pad hint. HUD colours. Tuning `chat.*`.

**Part D: access and admin.** New `WRAccess.h/.cpp`. `WRGameMode::PreLogin`, `InitGame` (session cap),
`InitNewPlayer` (role, fresh flag), `PostLogin` / `Logout` notices, kick/ban helpers. `AWRPlayerState`: `AccessRole`,
`AdminBans`. `WRPlayerMenus` commands `admin ...`. `WRUIClan` right column. `WRMainMenu`: `?WRPass=`, password box
on the card, kicked/banned/closed lines, `ClientHostClosing`. `WRUIWorlds`: Hosting block. Tuning `net.*`, `access.*`.

**Part E: bags.** New `WRLootLook.h/.cpp` (FWRToolMesh builds). `AWRLootBag`: `Kind`, `ExpiresAt`,
procedural mesh, beacon, light, ground settle, Tick for beacon visibility; save/load in `WRGameMode` (doc `bags`).
HUD compass pin. Callers: the chest spill → Spill kind.

**Part F: Hearth wake.** `WRBeds.cpp` FillWakeChoices / Choose / OnWoke; `WRHearth` access; tuning `beds.hearthRestSeconds`.

**Part G: claims and backups.** Gap rule in `CanBuild` / placement; Claim tab Rename + title; the worlds window's Backups list and Restore.

Each part: compile with the D: engine (adaptive unity), fix warnings, read it back against this section, update
CHANGELOG Unreleased + HANDOFF, commit by name, pull --rebase, push.

---

## 5. Spot-check notes (read against this doc and THE BAR after building)

| Found | Fixed |
|---|---|
| The door's open position had no collision (walk through the open leaf) | The leaf collides at rest, open or shut, and never while it moves (`OnRep_Door` / `Tick`) |
| The keypad and the code breaker opened doors with no person (always the +Y side) | Both swing away from the person at the door (`SwingFor`, WRPlayerBuild keypad, WRRaiding breaker) |
| Upkeep decay was reduced by offline protection (the totem's own damage) | Decay is never protected and never counts as an attack (`TakeBuildDamage` skips a totem source) |
| A restart would have handed out a free, instant arm (nobody "left" this session) | Leaving times fall back to the account's LastSeen (now written on logout), else the world's start |
| Chat lines from the world (system) read "Name: Name washed ashore" | System lines with no sender show the line alone |
| The bag doc saved every 60 s could hand a looted death bag back after a crash | Looting saves the taker and the bag doc at once; a bag gone (taken, expired) rewrites the doc; teardown never writes an empty list |
| Shipping builds would have blocked `admin` and `claimname` (allowed-commands list) | Added to the list |
| A dedicated server would default to 8 players | Dedicated default is `net.dedicatedDefaultPlayers` 32 (listen 8) |
| Lid of the spilled crate leaned away from the crate (roll sign); the sack and satchel touched or sank into their neighbours | Lid roll -26 (rests on the back wall's top), sack moved out and tilted toward the crate, satchel set against the roll's curve |
| The raid alert said "your claim" even for a named claim | It uses the claim's name |

Not built in this pass (designed in multiplayer.md, beyond the built pieces this cook pass raises; one line each in LATER-COOKS):
UPnP and EOS join codes (multiplayer.md 2.1), MOTD and scheduled restart (2.3), the NetGuard RPC buckets and movement
time-discrepancy flags (2.5; chat has its own limit now), make-camp logout and the tagged-not-safe rule (2.9.1-2.9.2),
frith posts (2.10.2), duels (2.10.4), clan look (tag, dye, sigil, banner) and clan-owned claims (2.6), the ping /
quick-chat wheel (2.6), empty-claim lapse, claim edge motes and map circles (2.7), build-piece dormancy (2.12).

## 6. Ledger proof

| # | Done | Where |
|---|---|---|
| R1 | Yes | Section 1 (games: Source `prop_door_rotating`, Rust doors and chat, Valheim tombstone, Rust/Valheim admin, Elden Ring Seamless Co-op, Dark Souls disconnects; real-world: hung doors, thing-peace, pack-roll, Hedeby satchel, crates) + multiplayer.md 1.1 |
| R2 | Yes | Sections 3 (design and edge cases) and 4 (build steps), written before the build |
| R3 | Yes | Parts A-G, commits 798cffb, d649695, fd15e34/fe6f1b9, a50e4a6, e929728, 7d3482c (+ the follow-up fix commit) |
| R4 | Yes | Section 5 |
| R5 | Yes | Every row here has file:line |
| R6 | Yes | 3.1 (door: hinge, jamb, keypad, ram), 3.5 (satchel: what it lies on, what touches what), 3.6 (standing by the fire) |
| R7 | Yes | Door swing curves `WRBuilding.cpp:563` Tick (ease-out, settle, ease-in, latch rebound; turned back from where it is `:511`) |
| R8 | Yes | Section 0 + this table |
| R9 Hosting | Yes | Hosting page `WRUIWorlds.cpp:539`; access.json `WRAccess.cpp`; host closing `WRPlayerController.cpp:264` |
| R10 Joining | Yes | PreLogin `WRGameMode.cpp:118` -> `WRAccess.cpp:191` Refuse; password card `WRMainMenu.cpp:1408, 1447`; arrivals `WRGameMode.cpp:724, 745` |
| R11 Clans | Yes | Clan chat `WRPlayerController.cpp:95`; players list with roles, ping, kick/ban `WRUIClan.cpp:435-473`; `WRAccess.cpp:262` Command |
| R12 Doors | Yes | `WRBuilding.cpp:511` OnRep_Door, `:563` Tick, `:626` DoorSweepBlocker, `:666` SwingFor |
| R13 Claims | Yes | Spacing `WRBuilding.cpp:1824`; name `WRUIClaim.cpp:490`, `claimname` in WRPlayerBuild; Hearth wake `WRBeds.cpp:94, 118, 284, 401` |
| R14 Raids | Yes | Arming protection `WRSocial.cpp:347`, `WRBuilding.cpp:1258, 1895, 1913`; Claim tab state `WRUIClaim.cpp:904`; settings rows `WRUISettings.cpp:2022`; named raid alert in WRRaiding |
| R15 Persistence | Yes | Restore `WRStore.cpp:731, 755`, `WRUIWorlds.cpp:694, 752`; bags saved `WRSocial.cpp:562` SaveNow; leaving times `WRGameMode.cpp:745` |
| R16 Grades | Yes | Section 2 |
| R17 No music UI | Yes (nothing found in scope) | Section 2 last row (grep: no staff, beat or rhythm UI in these systems) |
| R18 Data-driven | Yes | tuning `door`, `chat`, `access`, `claims`, `net.*Players`, `death.beacon*`, `beds.hearth*`, `store.backupsShown`, `raid.protectedNoticeSeconds`; presets `offlineArmMinutes` / `offlineQuietMinutes` |
| R19 ApplyHit | Yes (no character damage added) | Only building damage goes through `TakeBuildDamage` |
| R20 Mouse and pad | Yes | Journal tab (Clan) and the worlds window use buttons and text boxes the pad processor drives; the password box opens the pad keyboard; chat's channels by prefix on the pad keyboard |
| R21 Sounds as rows | Yes | `door.open` / `door.close` rows (latch timing); no new sounds |
| R22 Server authority | Yes | Doors, protection, chat routing, access, admin, bags and wake points are all decided on the server; roles and protection state replicate |
| R23 Unfinished hidden | Yes | Nothing half-built is shown; designed-but-not-built items aren't on any screen |
| R24 Compile gate | Yes | Editor target compiled after every part, with no warnings from this lane |
| R25 Git rules | Yes | Pulled before and after each part, files added by name, CHANGELOG Unreleased + HANDOFF in each commit, no tag or release |
| R26 Binary sizes | Yes | No new binary files |
| R27 LANDED / LATER-COOKS | Yes | Appended at the end |
| R28 No stops | Yes | Questions in the final message |
| R29 Emails private | Yes | None in code, docs or commit messages (the commit identity is git metadata only) |
| R30 Unity prefixes | Yes | `WRSocialProtect`, `WRClaimProtect`, `WRAccessDetail`, `WRLootLookKit` namespaces; no file-scope statics added |
