# Wildreach cook: MULTIPLAYER, CLANS, CLAIMS, RAIDS, PvP, SERVERS

Lane: multiplayer. Written 2026-11-14. Scope: FULL-GAME section 7, plus the wake (respawn) and Hearth rules from sections 2 and 6.1. Hosting and joining, the dedicated server, clans, claims, raids, PvP rules and zones, server settings, persistence, basic anti-cheat, scale, friends joining mid-game.

**Tags**
- **SPEC**: BUILT or RAVEN-PLANNED. Kept exactly as written, with the source cited. **SPEC + numbers** means Raven named the system and this cook adds only the numbers.
- **KEPT**: designed by another cook lane. Used exactly as that lane wrote it (lane and section cited).
- **GAP-COOKED**: the docs say nothing. Fully designed here.

All numbers are first drafts in `Data/*.json`, following Raven's CLAUDE.md.

**What I read:** FULL-GAME.md (whole). Raven's `server-settings.md`, `M7_server.md`, design-doc "Multiplayer, PvP & raiding" and "Ruled out", `DESIGN_REVIEW.md` #22, #31 and #32, `TECH_PLAN.md`, and `Data/server_presets.json` and `tuning.json` (`raid`, `death`, `beds`, `player`, `significance`, `world`). The code: `WRSocial.h/.cpp`, `WRWorlds.h`, `WRBeds.h`, `WRRaids.h`, `WRGameMode.h/.cpp` (InitNewPlayer), `WRBuilding.h/.cpp` (TakeBuildDamage, AWRClaimTotem), `WRPlayerController.cpp` (ServerChat) and `WRMainMenu.cpp` (join). Our docs: COMBAT-AND-LESSONS, FULL-RELEASE-PLAN, QUALITY-PLAN. Other cook lanes: `combat.md` 2.20, `biome-crystal.md` 2.21 (the PvP pass), `survival-crafting.md` (explosives, rest, comfort) and `world-map.md` 2.11 (claim-free zones, villages).

**What the code really does today** (this decides SPEC vs GAP):

| Fact | Proof |
|---|---|
| A player is identified only by the name typed into the join URL. Anyone who types your name gets your character on any world | `WRGameMode.cpp:86-95` (`?WRName=` → `ChangeName` → `LoadPlayer`) |
| There is no password, player cap, kick or ban | grep finds no `PreLogin`, `Kick` or `Ban` in `Source/Wildreach` |
| Chat is global only. FULL-GAME 7 says "global, local, clan" BUILT, but that is wrong: M7 plans the three channels and they are not built | `WRPlayerController.cpp:87-103` sends every line to every controller |
| The death bag and the spilled-chest bag are an engine sphere with the basic-shape material: a primitive | `WRSocial.cpp` `AWRLootBag::AWRLootBag` (`/Engine/BasicShapes/Sphere`) |
| Offline protection turns on the instant the last clanmate leaves, so logging off mid-raid protects the base | `WRBuilding.cpp:816-819` (`IsAnyoneOnline`) |
| The Hearth is not a wake point. You wake only at beds, clan beds and your path's camps | `WRBeds.h` header comment, `FillWakeChoices` |
| Claims belong to one player (`OwnerName`). A clan shares them only through `SameClan` | `WRBuilding.h:205`, `WRSocial.cpp` `CanShare` |
| Saves are plain JSON written in place, with no temp-file swap and no rotating backups | `WRGameMode.h` SavePlayer/SaveWorld; `WRSocial.cpp` `FWRClans::Save` |
| A friend who joins streams the whole island (×24) | `tuning.json` `world.streamingRangeScale` 24 |

---

## 0. Requirement ledger

The brief and Aaron's quotes in it, split into single requirements. A design lane's proof is the section of this file that answers each one. The build rounds in section 3 then turn each into code.

| # | Requirement (source) | Answered in |
|---|---|---|
| 1 | Read CLAUDE.md first (brief) | Done before research; rules applied throughout (nothing routed to anyone but Aaron, no hardware purchase, plain words) |
| 2 | Research how the best games solve this exact thing, naming games, places and systems (brief, Aaron 11-13 night) | 1.1 |
| 3 | Real-world reference where it applies (brief) | 1.2 |
| 4 | Research written down with the specific details used (brief) | 1.1 to 1.3 |
| 5 | Hosting and joining (scope) | 2.1, 2.2 |
| 6 | Dedicated server (scope) | 2.3 |
| 7 | Clans (scope) | 2.6 |
| 8 | Territory / claims (scope; "territory control points" are ruled out, DD) | 2.7 |
| 9 | Raids, player and creature (scope) | 2.8 |
| 10 | PvP rules (scope) | 2.9 |
| 11 | PvP zones (scope) | 2.10 |
| 12 | Server settings (scope) | 2.11 |
| 13 | Persistence (scope) | 2.4 |
| 14 | Anti-cheat basics (scope) | 2.5 |
| 15 | Scale, 4 to 16 players (scope) | 2.12 |
| 16 | Friends joining mid-game (scope) | 2.2.4 |
| 17 | Hearth and respawn rules (scope) | 2.13 |
| 18 | Every GAP designed, concrete, no TBD (brief) | All of 2; each item is tagged |
| 19 | SPEC items restated briefly with their source, kept exactly (brief) | Every SPEC row cites its source |
| 20 | Plan: data rows (which JSON files, which fields) | 3.1 |
| 21 | Plan: C++ classes and functions in Raven's patterns (UWRGameplayAbility + ClassForNode, all damage through ApplyHit, data-driven) | 3.2 |
| 22 | Plan: assets per item and the art route | 3.3 |
| 23 | Plan: sound and music cues | 3.4 |
| 24 | Plan: map dressing | 3.5 |
| 25 | Plan: build order with the proof-of-one first | 3.6 |
| 26 | Design with intention: each object's job, what it connects to, and how it meets everything it touches (Aaron 11-14) | 2.14 |
| 27 | Machines look engineered; moving parts move smoothly, never snap (Aaron 11-14) | 2.14 (the raid bell's swing; siege KEPT from biome-crystal 2.21.5) |
| 28 | UI: mouse and pad on every screen, no dead states, F1 controls page (THE BAR) | 2.15 |
| 29 | Combat bar holds in PvP: full hit stack, damage only on visible contact, projectiles never home (THE BAR) | 2.9 (all PvP goes through ApplyHit with combat 2.20 KEPT) |
| 30 | No attacker limit: packs aggro freely (THE BAR) | 2.8.2 (creature raids have no attacker cap), 2.9 |
| 31 | Players die; no safety floors; no helpers after a fail (THE BAR) | 2.9.5 (no spawn shield), 2.2.4 (no catch-up helper), 2.10.4 (sparring is opt-in, not a floor) |
| 32 | Story only through the environment, no staged scenes (THE BAR) | 2.10.2 (frith posts), 2.13.5 (the death satchel) |
| 33 | Music always audible, ducks ≤ 4 dB, only planned silences (THE BAR) | 3.4 |
| 34 | People are attractive and never sanitized (THE BAR) | 2.2.3 (the creator is the hero lane's, KEPT); nothing in this lane covers bodies up |
| 35 | Taming stays as Raven designed; no spirit summons; weapon skills are Wildbrands (THE BAR) | 2.8.1 (tame raiding is Raven's, KEPT), 2.9 |
| 36 | Edge cases planned (Aaron 11-13 night) | The edge-case rows in each subsection, and 2.16 |
| 37 | Out-of-scope finds go to the later-cooks file, one line each (brief) | Appended to `C:\Users\acars\wildreach-night\LATER-COOKS.md` |
| 38 | Don't edit Raven's repo or FULL-GAME, no git, no tests, no builds (brief) | Only this file was written |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / system | What it does | Lesson for Wildreach |
|---|---|---|
| **Valheim** dedicated and listen servers | Characters live on each player's PC, not on the server, so a character can be edited offline and brought back with extra items. Server-side save mods exist only to fix this. 1.0 worlds save as a folder with backups in their own folders ([GameServerKings save editor](https://www.gameserverkings.com/knowledge-base/valheim/valheim-save-editor/); [ServerSideSave mod](https://thunderstore.io/c/valheim/p/UpdatingOldPlugins/ServerSideSave/)) | Raven already did this right: characters are saved **on the server** per world (`players/`). Keep it. Add an identity key so a name can't be stolen, and rotating backups |
| **Enshrouded** (16-player servers) | On 16-player servers one player can push the quests for everyone. Character data synced through Steam Cloud has rolled characters back to old saves ([GamesRadar](https://www.gamesradar.com/enshrouded-players-frustrated-to-discover-that-one-player-can-progress-quests-for-everyone-on-the-survival-rpgs-16-person-servers-even-if-youre-not-actually-online); [Supercraft sync note](https://www.supercraft.host/wiki/enshrouded/steam_cloud_character_sync_fix/)) | One source of truth per character, on the server only, never cloud-synced. Personal progress (paths, life skills) stays personal; shared progress (the Hearth level) is per claim, as Raven built it |
| **ARK** Offline Raid Protection | Turns on only after the whole tribe has been offline for **15 minutes** (900 s default) and ends the moment anyone logs in. The countdown stops players logging off mid-raid to save their base ([Supercraft ARK ORP](https://supercraft.host/wiki/ark/ark_pvp_offline_protection/); [GuildOrder base defense](https://guildorder.com/ko/games/ark/guides/base-defense)) | Raven's offline protection gets a **15-minute arming delay**, plus a rule that a claim attacked in the last 10 minutes doesn't arm until 10 quiet minutes have passed |
| **Rust** sleepers and the tool cupboard | Logging out leaves a body others can kill and loot, which stops combat logging. Building rights are given in person, never by proxy. "Insiding" means a trusted member strips the cupboard and takes the base ([Facepunch wiki TC](https://wiki.facepunch.com/rust/The_Tool_Cupboard); [GuildOrder insider theft](https://guildorder.com/games/rust/guides/tool-cupboard-authorization-and-insider-theft); [Steam thread on sleepers](https://steamcommunity.com/app/252490/discussions/0/630799997580405143)) | Combat-tagged logout leaves a body (combat 2.20, KEPT). Untagged logout in the open takes a short "make camp" sit that damage interrupts. **Claims become clan-owned** and per-object minimum ranks apply (biome-crystal 2.21.1, KEPT), so one Recruit can't take a base |
| **V Rising** server settings | Offline protection is replaced by raid windows; castles decay when their blood essence runs out; default clan size is 4; presets set loot, damage and raid time in one pick ([Freakhosting](https://help.freakhosting.com/games/v-rising/offline-raiding-settings); [Supercraft settings](https://supercraft.host/wiki/v-rising/server-game-settings/)) | Raid windows are **ruled out** by Raven (DD), so offline protection does the job. Presets lock a full set of values (Raven's model, kept) |
| **Conan Exiles** | Official servers use raid windows instead of offline protection. Guard thralls stop fighting in offline raids and players feel cheated ([GuildOrder offline raiding](https://guildorder.com/games/conan_exiles/wiki/offline-raiding-and-server-settings); [Funcom forum](https://forums.funcom.com/t/thralls-utterly-useless-dont-defend-against-pvp-theft-of-stuff/70824)) | No NPC guards (Raven rules them out). Tames on guard inside a claim get the same offline cut as the walls |
| **Palworld** dedicated server | One ini for every world setting, an admin password, and settings copied into the world save once the world exists. Changes need a restart ([Palworld wiki ini](https://palworld.wiki.gg/wiki/PalWorldSettings.ini); [GameServerKings](https://www.gameserverkings.com/knowledge-base/palworld/world-settings/)) | Raven's `settings.json` per world is the better model: the host changes settings live in game (BUILT `ChangeWorldSettings`). Add an admin list and a password to the same folder |
| **Dune: Awakening** security zones | Leaving a PvP security zone used to take 5 s; patch 1.1.15 raised it to **30 s** to stop players hopping the border to escape fights ([Method.gg PvP overview](https://www.method.gg/dune-awakening/dune-awakening-pvp-overview-how-does-it-work-and-can-you-avoid-it); [PC Gamer patch](https://www.pcgamer.com/games/mmo/todays-dune-awakening-patch-takes-another-sidestep-away-from-the-pvp-heavy-endgame)) | A combat-tagged player who runs into a safe zone **isn't protected** until the tag runs out (15 s, combat 2.20). No border hopping |
| **Once Human** | PvE servers have an opt-in "Chaos Mode" PvP flag; players choose PvE or PvP worlds; territories can be packed up and moved to a friend's world ([VideoGamer](https://www.videogamer.com/guides/once-human-pve-or-pvp-server-key-differences); [Mein-MMO](https://mein-mmo.de/en/once-human-change-worlds-and-servers,1149974/)) | On PvE and co-op worlds, friends can still spar: **consensual duels** (2.10.4). No base moving (Raven's handcrafted persistent map) |
| **Monster Hunter Wilds** SOS flare | You fire a flare after taking a quest and anyone can answer it, joining the hunt in progress ([Icy Veins co-op](https://www.icy-veins.com/monster-hunter-wilds/co-op-guide); [GuildOrder SOS](https://guildorder.com/games/mh_wilds/wiki/sos-flares-and-support-hunters)) | Mid-fight joining already works (boss HP rescales on join, BUILT). Add a **clan call**: a ping friends see on the map and can fast travel toward (shrine rules) |
| **Elden Ring Seamless Co-op** mod | The mod's whole appeal is a co-op session that never ends at a boss kill: 4 players, one shared world, entry by password ([PC Gamer](https://pcgamer.com/is-elden-ring-co-op-multiplayer); [Neowin](https://www.neowin.net/news/elden-ring-seamless-co-op-mod-makes-cooperative-play-a-hassle-free-experience/)) | A persistent shared world is our base (Raven). A **world password** is the simple, trusted friend gate |
| **Unreal / EOS networking** | EOS P2P lets a listen server be joined without port-forwarding, falling back to relays when NAT punch-through fails; dedicated servers can't use EOS P2P. Reports conflict on whether Epic's own plugin sets this up out of the box ([Redpoint P2P vs IP](https://docs.redpoint.games/eos-online-subsystem/docs/networking_p2p_vs_ip); [UE forum on ports and EOS](https://forums.unrealengine.com/t/question-about-networking-ports-and-eos/598259)) | Two steps: **UPnP auto port-map** now (miniupnpc, BSD licence, $0), then **EOS P2P join codes** with relay. Direct IP stays forever as the fallback |
| **Unreal movement anti-cheat** | Character Movement has built-in time-discrepancy (speed-hack) detection, **off by default**. Custom movement must be validated by your own code ([UE answers](https://answers.unrealengine.com/questions/428329/view.html); [AccelByte](https://accelbyte.io/blog/server-authoritative-logic-to-prevent-cheating)) | Turn on `bMovementTimeDiscrepancyDetection` and `Resolution`; validate our own movement (mantle, glide, mount dash) on the server |
| **Fortnite building dormancy, Iris** | Epic made every building actor dormant from the start and avoided `bAlwaysRelevant` at their actor counts. Epic staff say Iris is not guaranteed to beat classic replication or RepGraph ([UE forum dormancy](https://forums.unrealengine.com/t/proper-use-of-netdormancy/438916); [UE forum Iris](https://forums.unrealengine.com/t/how-best-to-go-about-implementing-component-dormancy-in-iris/2655306); [Epic perf tips](https://dev.epicgames.com/documentation/unreal-engine/performance-and-bandwidth-tips-for-unreal-engine)) | Build pieces `DORM_Initial`, flushed on change (TECH_PLAN already says "dormancy for buildings and tames"). Stay on classic replication plus push-model; measure Iris only at the dedicated-server step |

### 1.2 Real-world reference

| Source | Used for |
|---|---|
| **Þingfriðr (thing-peace)**, Norse assembly law. The peace covered the assembly and the road to and from it; anyone who killed or maimed someone there was outlawed, and weapons were taken from those who carried them ([Lexicon of Medieval Nordic Law, þingsfriðr](https://www.dhi.ac.uk/lmnl/nordicheadword/displayPage/1841); [Landslov jubilee, "Holy and solemn"](https://landslovjubileet.uib.no/en/tingfarebolken/hellig-hoytidelig)) | Safe zones are **frith grounds**, marked by carved peace posts. A Hunted player (infamy 60+, biome-crystal KEPT) is turned away at the edge, just as criminals were left out of the peace |
| **Grið (truce, sanctuary for a set time)**, which a killer couldn't be attacked during; breaking it was a *níðingsverk*, the worst kind of shame (same lexicon) | Sparring duels on PvE worlds are a spoken truce with a start and an end. The duel ring (a cord on the ground) is the visible grið |
| **Hestia and the Roman Vesta, Brigid's ever-burning flame at Kildare, the Domovoi carried in embers to a new home** (survival-crafting 1.2 KEPT) | The **Hearth is a wake point**: your clan's fire is where you come back to. A broken Hearth (cold) isn't a wake point |
| **Ship's bells and fire bells** (bronze, struck by a swinging clapper; heard about 1.5 km on calm air) | The **raid bell** on every claim totem: a real bronze bell hung from an iron bracket, whose clapper swings when the claim is hit |
| **The traveller's pack-roll** (a hide satchel tied to a bedroll with a tump-line) | The **death satchel** that replaces the sphere |

### 1.3 Lessons in one line each
1. Characters live on the server only, keyed to an identity the player can't type their way into.
2. Offline protection needs an arming delay, or it rewards logging off mid-raid.
3. Fleeing into safety mid-fight must not work.
4. Clan-owned bases plus per-object ranks stop insiders.
5. Joining a friend has to work with no router setup.
6. Buildings must be dormant on the network, or 16 players with big bases will choke the host.
7. Every save is atomic and backed up, because a persistent world that loses a night of building loses its players.

---

## 2. Design

### 2.1 Hosting

| Item | Tag | Design |
|---|---|---|
| Host-and-play listen server; worlds saved per folder on the host | SPEC (BUILT `WRWorlds.cpp`, `WRMainMenu.cpp`) | Kept |
| Installer and auto-update from GitHub releases | SPEC (BUILT `Tools/installer/`) | Kept. A client of a different version is turned away with the reason (BUILT, `WRMainMenu.cpp:462`) |
| **Host screen additions** | GAP-COOKED | The worlds list (BUILT) gets a **Hosting** panel per world: **Max players** (2 to 16, default 8); **Password** (blank = open; shown as dots with an eye toggle; stored only as a salted hash); **Who can join** (Anyone with the address / Password / Clan and friends list only); **Open my router** (UPnP, default On). Each control is a full-width row with a big hit target, reached by Tab/arrows and the pad D-pad, and changed with the mouse wheel or the pad's left/right |
| **Address card** | GAP-COOKED | When a world starts hosting, the pause menu shows an address card: the host's public IP and port (looked up once via UPnP's `GetExternalIPAddress`, else "Unknown: see F1 page Hosting"), a **Copy** button, a router status chip (green "Router opened by Wildreach", amber "Open UDP 7777 on your router: F1 shows how", red "Port closed"), and from round MP-5 a **Join code** (6 letters, no 0/O/1/I) |
| **UPnP auto port-map** | GAP-COOKED | On host start: discover the gateway (2 s timeout), map UDP 7777 external → 7777 internal with a 2-hour lease, renew every 60 min, and remove it on shutdown. On failure, fall back to the manual F1 page. Library: miniupnpc (BSD-3) as a ThirdParty module, $0 |
| **Join codes with relay (EOS P2P)** | GAP-COOKED (Raven's M7 names EOS as the free identity route) | EOS Connect with a **device ID login** (no Epic account asked of the player, free). The host creates an EOS lobby whose attribute is the 6-letter code. A joiner types the code and the client resolves it and connects over EOS P2P sockets, relayed when NAT punch-through fails. Direct IP stays as a second tab. **Edge case:** if EOS is unreachable, the code tab greys out with "Join codes offline: use the address" (never a dead button) |
| **Host leaves** | GAP-COOKED | A listen world ends when the host quits. On **Quit / Return to menu** with others connected, the host gets a confirm: "4 players are here. Close the world for everyone?" (Close in 30 s / Close now / Cancel). Closing broadcasts a banner, "The host is closing the world in 30 s", ticking every 10 s and every second for the last 5. All players are saved, the world is saved and backed up, and each client lands on the main menu with "The host closed the world. Everything was saved." A host crash gives clients "Lost the host: the world keeps its last save (at most 10 s of your character, 60 s of building)" |
| **No host migration** | GAP-COOKED (decision) | The world's files live on the host's disk, so a world can't migrate. Instead, any world folder runs unchanged on a dedicated server (2.3). The F1 Hosting page says how to copy it |

### 2.2 Joining

#### 2.2.1 The join screen (GAP-COOKED; extends BUILT `WRMainMenu` Join)

| Part | Design |
|---|---|
| Tabs | **Address** (BUILT box: "Friend's IP or IP:port") and **Join code** (from MP-5). LB/RB or Q/E switch tabs |
| Recent | Up to 10 servers joined before: world name, host's character name, address, players n/max, ping (one UDP query, 1 s timeout; "—" if no answer), a lock icon if it needs a password. Each row has **Join**, **Favourite** (star; favourites pin to the top) and **Forget** |
| Password prompt | If the server answers "password needed", a modal asks for it, with the eye toggle. A wrong password says "Wrong password" and keeps the box filled. Three wrong tries in 60 s from one IP: 60 s lockout, "Too many tries: wait 60 s" with a countdown |
| Rejections (all with plain reasons and a Back button) | "World full (16/16)" · "Banned from this world" · "Different version (theirs 0.8.2, yours 0.8.1): update from the menu" (BUILT) · "That name belongs to someone else on this world: pick another" (2.2.2) · "Host closed the world" · "Couldn't reach the host: they may need to open UDP 7777 (F1 shows how)" |

#### 2.2.2 Identity (SPEC M7 Identity, numbers GAP-COOKED)

M7 says: "First Playable: platform-less login via a player-chosen name + generated token in the client's Saved dir. Steam/EOS later." Built as:

| Piece | Design |
|---|---|
| The key | On first launch the client makes 32 random bytes (OS crypto RNG) and saves them to `Saved/Identity/key.bin`. It never leaves the PC as plain bytes |
| On join | The client sends `?WRName=<name>&WRKey=<base64 of SHA-256(key + worldId)>`. Hashing with the world id means a host can't reuse your key on another world |
| On the server | `players/<name>.json` gets `"keyHash"`. The first join with a free name claims it. Later joins with a different key are refused at `PreLogin` with the name message. Old saves without `keyHash` take the first key that comes (a one-time claim, logged) |
| Lost key (new PC) | The world's admin uses **Player list → Release name** to clear the `keyHash`. The next join claims it. Logged in `admin.log` |
| EOS later | When EOS Connect lands (MP-5), the EOS product user id becomes the key. Old names are kept by linking: the first EOS join that presents the old key binds the EOS id |

#### 2.2.3 The first time on a world

| Step | Tag | Design |
|---|---|---|
| Name, body (MetaHuman male or female, short creator), main path | SPEC (BUILT name/body/path; creator from the hero lane, KEPT) | Kept. A fresh character in each world (BUILT `WRWorlds` players/) |
| Wake at one of three camps in the path's biome | SPEC (BUILT `WRGameMode.cpp`) | Kept |
| **Clan invite first** | GAP-COOKED | If a clan already invited this name (leaders can invite a name before that player has ever joined; the invite sits in `clans.json`), the creator's last page shows "Clan Emberline invited you". Accept makes you a Recruit at once, so the clan's beds and Hearths join your wake choices (2.13). You can also **wake at an Emberline Hearth** for your first wake instead of a camp |
| Arrival notice | GAP-COOKED | Everyone sees a notice: "Mira washed ashore" (first time) or "Mira is back" (returning). Leaving shows "Mira left" |

#### 2.2.4 Friends joining mid-game (GAP-COOKED)

| Rule | Design |
|---|---|
| The world doesn't pause or reset | It's persistent (Raven). The joiner loads with the "Loading the island" cover held until the ground around them has streamed in (BUILT `flow.streamRadius`) |
| Mid-boss join | Boss HP rescales on join, as BUILT. A joiner who walks in gets their own loot if they deal 10% (SPEC fixed rule) |
| Catching up | **No catch-up helper** (THE BAR: no helpers). What helps is Raven's own design: the clan's Hearth sets station levels for every member (BUILT), so a newcomer crafts at the clan's tech level from minute one. Path XP and life skills are earned by playing |
| Clan call | Hold **Ping** (middle mouse; pad D-pad up held 0.4 s) over the map or the world: a gold flare marker any clanmate sees on the compass and map for 60 s, with the caller's name and distance. With "Call" chosen on the ping wheel, it's a **clan call**: clanmates also get a banner, "Mira calls: Vulkarn's crater, 840 m". Fast travel still follows shrine rules (world-map, KEPT); there's no teleport to players |
| Joining while the world is busy | Raids in progress, boss fights and portal runs carry on. A joiner inside a claim under raid gets the raid banner at once (2.8) |
| Edge case: the joiner's saved spot is now inside someone else's walls | On login, if the saved position is inside another clan's sealed building (a trace in four directions all hit their pieces within 6 m), you wake at your default wake point instead, with "Your last spot was walled in: you woke at your bed" |

### 2.3 Dedicated server

| Item | Tag | Design |
|---|---|---|
| Dedicated server for 10 to 100 players; packaged Windows and Linux server targets; host on a PC by IP, then maybe a free cloud VM | SPEC (M7 sections 4 and 5; on hold) | Kept, on hold as the spec says. Everything below is built so a listen world and a dedicated world run the **same code and the same world folder**. Packaging the server target is the only step that waits |
| Source engine needed (~200 GB) | SPEC (M7 4) | No new hardware (Aaron 11-04). The packaging round starts when 200 GB is freed on an existing drive (T: has 129 GB: clearing old Saved/Intermediate and DerivedDataCache gets there) |
| **Start line** | GAP-COOKED | `WildreachServer.exe -wrworld=<id> -port=7777 -log` (the `-wrworld` switch is BUILT). The first start on an empty folder makes the world from `-wrpreset=<name>` (BUILT) |
| **Admins** | GAP-COOKED | `Saved/Worlds/<id>/access.json`: `admins` (names with key hashes), `bans`, `password` (salted hash), `maxPlayers`, `motd`, `whitelist` (empty = off). On a listen server the host is always an admin. Admins get Player-list actions (2.15) and the world settings screen (BUILT, host-only today; opened up to admins) |
| **Message of the day** | GAP-COOKED | Up to 300 characters, shown once per login as a parchment card after the loading cover (Continue: E / pad A) |
| **Scheduled restart** | GAP-COOKED | `access.json` `restartEvery` hours (0 = off; dedicated default 24). Warnings at 15, 5 and 1 min, then 10 s, as a banner. Everyone is saved, the world is backed up, and the process exits with code 0 so a service wrapper restarts it. `Tools/server/run.ps1` is a 20-line loop that restarts it, $0 |
| **Bot load test and 8-hour soak** | SPEC (M7 5) | Kept as Raven wrote. These are Raven's tools and run in his pipeline. Our gates stay compile, cook and package (Aaron 11-12) |

### 2.4 Persistence

| Item | Tag | Design |
|---|---|---|
| Characters on the server, one JSON per character; world.json; clans.json; autosave every 10 s (dirty players) and 60 s (world) | SPEC (BUILT `WRGameMode.h`, `WRWorlds.h`) | Kept |
| SQLite with WAL, batched writes, migrations, hourly backups | SPEC (M7 1; FULL-GAME conflict 17: built JSON works today, SQLite matters for the 100-player server) | Comes with the dedicated-server round (MP-7). The rest of this table hardens the JSON saves now, so nothing is lost before then |
| **Atomic writes** | GAP-COOKED | Every save writes `<file>.tmp`, flushes it, then renames it over the old file (`IFileManager::Move` with replace). A crash mid-write leaves the old file whole |
| **Rolling backups** | GAP-COOKED | `Saved/Worlds/<id>/backups/`: an **hourly** copy of world.json, clans.json, access.json and players/ while anyone plays (keep 24) and a **daily** one at the first save after midnight (keep 7). They're zipped (`FZipArchiveWriter`), about 1 to 5 MB each. Also one **on every clean shutdown** (keep 5) |
| **Corrupt-file recovery** | GAP-COOKED | If a file fails to parse at load, it's renamed `<file>.bad-<time>`, the newest backup that parses is used, and the host or admin gets a banner: "world.json was damaged: restored the 14:00 backup (lost up to 52 min)". Never a silent empty world |
| **Save on big moments** | GAP-COOKED | Saves the player at once (not waiting for the 10 s tick) on: a boss kill credit, level-up, a tame finishing, a Hearth core fed, a trade between players, and logout. The world saves at once on: claim placed or removed, Hearth core fed, a raid's end |
| **Restore a backup** | GAP-COOKED | In the worlds list, a world's **⋯ → Backups** shows every backup (time, size). **Restore** asks first ("Replace the world with 13 Nov 22:00? Your current world is backed up first") and only works while the world isn't running |
| Death bags, spilled bags and dropped items | SPEC (BUILT, not saved) | Kept: they live 30 min or 10 min and aren't saved. **Edge case:** a restart within the window would lose a souls bag, so a scheduled or host-closed shutdown saves open death bags into world.json `bags` with their time left, and restores them. A crash loses them (stated on the F1 Death page) |

### 2.5 Anti-cheat basics

Spec (M7 6): "Server authority everywhere already covers most of it. Add: RPC rate limits, movement speed sanity checks, and validation on every inventory/build/point RPC." Numbers and behaviour, GAP-COOKED:

| Guard | Design |
|---|---|
| Authority | Clients send intent, never results; every hit is judged by ApplyHit on the server with the 250 ms rewind (combat 2.20, KEPT) |
| **Movement** | `AGameNetworkManager`: `bMovementTimeDiscrepancyDetection` = true, `bMovementTimeDiscrepancyResolution` = true, `MovementTimeDiscrepancyMaxTimeMargin` 0.25 s, `MovementTimeDiscrepancyMinTimeMargin` −0.25 s, `MovementTimeDiscrepancyDriftAllowance` 0.05. `bServerAcceptClientAuthoritativePosition` stays false. Our own moves (mantle, glide, mount dash, Wind dash, roll) run as CMC custom modes or root motion, so the server simulates them too. A teleport outside fast travel or a server move is corrected and logged |
| **Range checks** | Every Interact, loot, build, door, chest, station, tame-order and bag RPC checks that the actor is within `reach + 150 cm` of the player's server position (reach from tuning, 300 cm default; 1,500 cm for build placement, BUILT `dropReach`) and that a line trace isn't blocked by world geometry. A failure does nothing and adds a strike |
| **Validation** | Every inventory move checks the slots exist and hold what's claimed; crafting checks inputs on the server (BUILT); path-point spends check the cap (150/100, fixed rule); build placement runs the same `CanBuild` and support checks as the ghost (BUILT) |
| **RPC rate limits** | `UWRNetGuard` keeps a token bucket per player per RPC family: chat 1/s (burst 5); interact 8/s (burst 12); inventory 20/s (burst 40); build 6/s (burst 10); emote and ping 2/s (burst 4). Over the limit, the call is dropped and adds a strike |
| **Strikes** | 30 strikes in 60 s → kicked with "Kicked: too many invalid actions" and an `admin.log` line with the counts per kind. Three kicks in 24 h → a 24 h automatic ban (the admin can lift it). Strikes never ban on their own beyond that, so a laggy honest player is only ever kicked |
| **Chat** | 200 characters (BUILT), rate-limited as above, and mutable per player by admins |
| **Save trust** | The client never sends a save; the server holds the only copy (BUILT design). Identity key (2.2.2) |
| **Out of scope** | No kernel anti-cheat and no client memory scanning ($0, and a server-authoritative game doesn't need them) |

### 2.6 Clans

| Item | Tag | Design |
|---|---|---|
| Found, invite, remove, leave; shared claims, doors, chests, beds, tames; no friendly fire; size limit setting; Clan tab (O) | SPEC (BUILT `WRSocial.cpp`, `WRUIClan.cpp`) | Kept |
| Ranks (Leader, Officer, Member, Recruit), per-object minimum rank, clan log; alliances (3 clans, ×1.5 size cap, 24 h leave cooldown); outnumbered | KEPT (biome-crystal 2.21.1 to 2.21.3) | Kept exactly. They ship in R9 with the PvP pass |
| **Clan look: tag, colour, sigil** | GAP-COOKED | A **tag** of 2 to 4 letters, shown in brackets before members' names on nameplates, chat and the player list. A **colour** from 8 dyes (madder red, woad blue, weld yellow, oak-gall black, lichen purple, walnut brown, verdigris green, bone white): real natural dyes, so they read as cloth, not neon. A **sigil** from 24 carved marks (wolf head, antler, ember, snowflake, axe, raven, hand, eye, wave, sun disc, spiral, tree, hammer, serpent, moon, mountain, boar, stag, horse, ship, knot, spear, bone and star). Sigil and colour show on the clan's **banner** (a buildable decor piece: a dyed linen banner on an ash pole with an iron finial, sigil painted in the colour) and on the map pins of clan claims |
| **Clan-owned claims** | GAP-COOKED (solves insiding; fits crystal's ranks) | A totem placed by a clan member is owned by the **clan** (`ClanOwner`), and so is every piece in its claim. A member who leaves or is kicked loses access to all of it. A **solo** player's claims are personal. When a solo player founds or joins a clan, a prompt asks: "Bring your 2 claims into Emberline?" (Bring them / Keep them mine). Kept claims stay personal: clanmates still can't build there |
| **Leader goes quiet** | GAP-COOKED | If the Leader hasn't logged in for 14 real days, leadership passes to the Officer with the most play time in the last 14 days (else the longest-standing Member). Everyone gets a notice at next login, and it's logged |
| **Disband** | GAP-COOKED | Leader only, typed confirm (the clan's name). Clan-owned claims pass to the Leader as personal claims; tames go back to whoever tamed them |
| **Chat channels** | SPEC (M7 3: "global, local by distance, clan") | Not built today (see the table at the top). **Global** to all; **Local** 40 m around you (both on the ground); **Clan**; **Alliance** (crystal); **Whisper** (`/w name`). Channel chosen by a chip left of the box (Tab cycles; pad LB/RB). Lines are coloured: global ink, local warm grey, clan the clan colour, alliance teal, whisper lilac. The history keeps 50 lines (BUILT keeps 8) |
| **Quick chat (pad, and anyone)** | GAP-COOKED (needed: chat without a keyboard on pad is a dead state) | The ping wheel (hold D-pad up, or middle mouse) has 8 slots: **Ping here**, **Call clan**, **Help!**, **Follow me**, **Wait**, **Raid!**, **Thanks**, **Yes / No** (a second press flips). Phrases go to clan chat (local if you're clanless) with your position |
| Edge case: the clan size limit drops below a clan's size | GAP-COOKED | Nobody is kicked; the clan can't invite until it's under the limit |

### 2.7 Claims (territory)

| Item | Tag | Design |
|---|---|---|
| Totem: only your clan builds inside; 30 m, +10 m per upgrade, to 60 m; unlimited claims; upkeep in materials; out of upkeep the claim drops and the base decays | SPEC (BUILT `WRBuilding.cpp`, `WRUIClaim.cpp`; DD) | Kept |
| Territory control points | SPEC rule: **ruled out** (DD) | Not built. Claims are the only territory |
| No claims in passes, near shrines (200 m), camps and villages (safe zone + 150 m), or within 250 m of boss arenas and run-back stones | KEPT (world-map 2.11; biome-crystal 2.21.8) | Kept |
| **No overlaps** | GAP-COOKED | A new totem is refused if its full radius (at max upgrade, 60 m) would overlap another claim's, plus a 10 m gap: "Too close to another claim (needs 70 m)". This stops claims nesting inside each other and upgrades reaching into a neighbour |
| **Empty-claim lapse** | GAP-COOKED (stops land-blocking with unlimited free totems) | A totem with fewer than **4 structure pieces** in its radius 24 real hours after placement lapses. It crumbles (a 3 s break clip), drops its upkeep storage as a spilled bag, and tells the owner "Your claim at Ashen Ford lapsed: nothing was built". From then on, a claim whose structure count falls below 4 for 24 h lapses the same way |
| **Claim names** | GAP-COOKED | The Claim tab (BUILT) gets a name box (24 characters; default "<clan> holding"). The name shows on the map, in raid banners and in notices |
| **Seeing the edge** | GAP-COOKED | With the hammer out or in build mode, the claim's edge shows as a ring of faint gold motes 1 m off the ground every 4 m (allies teal, others' claims grey), drawn only within 40 m of you. On the map (M), claims are filled circles: yours and your clan's gold, allies' teal, others' a grey outline (positions only; owners show only for yours) |
| Upkeep and decay | SPEC (BUILT, `DecayPerHourWhenUnpaid` 0.1) | Kept. The Claim tab shows "Upkeep lasts 3 d 4 h" from stored materials ÷ upkeep per hour |
| Edge case: a claim crosses a road | KEPT (world-map: claims may sit beside roads, never on road cells) | Pieces can't go on road cells |

### 2.8 Raids

#### 2.8.1 Player raids

| Item | Tag | Design |
|---|---|---|
| Bases raidable any time owners are online; offline protection 0 to 100% less damage | SPEC (DD; BUILT `WRBuilding.cpp:816`) | Kept |
| Explosives, raid tools, siege weapons, path-based raiding, tame raiding, raid bell, code breaker, siege ladder | KEPT (survival-crafting 2.16 and 2.18; biome-crystal 2.21.4 to 2.21.6) | Kept exactly |
| Raiders can damage the Hearth; broken = no crafting, cores safe | SPEC (BUILT `WRBuilding.cpp`) | Kept |
| Raid windows, scheduled sieges | SPEC rule: **ruled out** (DD) | Not built |
| **Offline protection arming** | GAP-COOKED (ARK's lesson) | Protection arms **15 min** after the last member of the owning clan (or the solo owner) goes offline, and disarms the instant one logs in. A claim that took player or siege damage in the last 10 min won't arm until it has had **10 quiet minutes**. The Claim tab shows the state: "Unprotected (owners online)", "Arming: 11 min", "Protected 75%". Raiders see a state word on their first hit of a protected claim: "PROTECTED 75%". Tames on Guard or Stay inside the claim's radius take the same cut from players |
| **Raid notice to the offline** | GAP-COOKED (extends crystal's raid bell) | Members who log in after a raid see a summary card before play: "While you were away: Ashen Ford was attacked 03:12 to 03:40 by [VKG] (3 players). 14 pieces lost, Hearth broken, chest 'Ore' emptied". The card is built from the claim's raid record (the last 10 raids are kept in world.json) |
| **Raid music** | GAP-COOKED | 3.4 |

#### 2.8.2 Creature raids

| Item | Tag | Design |
|---|---|---|
| Off / rare / normal / frequent; due time per totem; waits until a member is within 250 m; 60 s warning with a direction; raiders from the claim's region, worth points by Hearth level; they break pieces and fight; end after 6 min or when all are dead | SPEC (BUILT `WRRaids.cpp`, `tuning.json` `raid`) | Kept |
| No attacker cap | THE BAR | Raiders all aggro freely; no limit on how many attack one player at once (BUILT has none; keep it that way) |
| **The raid bell rings for creature raids too** | GAP-COOKED | The 60 s warning (BUILT) also swings the totem's bell (2.14), so the warning is heard in the world, not just read |

### 2.9 PvP rules

| Item | Tag | Design |
|---|---|---|
| Open PvP everywhere but safe zones; PvE-only option; PvP inside boss arenas is a setting | SPEC (BUILT `WRSocial.cpp` `PlayersCanFight`) | Kept |
| PvP numbers: ×0.6 player damage, buildup ×0.7, crit arcs, 250 ms rewind, combat tag 15 s (no fast travel, no wake-point swap, no clan storage), logout while tagged leaves your body 30 s | KEPT (combat 2.20) | Kept. All PvP damage goes through ApplyHit with the full hit stack. Projectiles never home |
| Death penalty: Gentle, Souls-like, Drop inventory, Full loot | SPEC (BUILT `AWRLootBag::ApplyDeathPenalty`; SS) | Kept as BUILT. Souls-like keeps carried items in a bag only you can take; die again first and the old bag is gone |
| Bounties and infamy; contested bosses | KEPT (biome-crystal 2.21.7, 2.21.8) | Kept |

#### 2.9.1 Logging out (GAP-COOKED, extends combat 2.20)

| Case | What happens |
|---|---|
| Combat-tagged | Body stays 30 s, killable and lootable (combat KEPT) |
| Inside your own or your clan's claim, in a safe zone, or lying in a bed you've set | Gone at once |
| Anywhere else | **Make camp**: the hero sits and unrolls a pack (real clip, 6 s, `logout_camp`). Moving, being hit, or taking any damage cancels it and you stay in. When it finishes you're gone. Pressing Quit to menu or Alt+F4 during it, or losing connection, leaves the body sitting for the rest of the 6 s plus 10 s (16 s at most), killable |
| PvE worlds | Make camp is skipped (nobody to escape from), unless creatures are in combat with you. In that case the body stays the 16 s and creatures can hit it |

#### 2.9.2 Fleeing into safety (GAP-COOKED, Dune's lesson)
A combat-tagged player who enters a safe zone is **not protected** until the tag runs out (15 s with no PvP hit). They can't attack from inside. A thin crimson rim on the HUD says "TAGGED: NOT SAFE 9 s".

#### 2.9.3 PvP inside boss arenas
SPEC setting. Uses the BUILT arena radius (`bosses.arenaPvpRadius` 30 m from the boss home while it lives).

#### 2.9.4 Friendly fire
None within a clan (BUILT) or between allies (crystal). Area spells and explosives skip clanmates and allies too, and tames skip them. **Edge case:** a fire pot's burning ground still burns clanmates' wooden *pieces* (fire is fire), but not the clanmates.

#### 2.9.5 Waking in PvP
**No spawn shield** (THE BAR: no safety floors). Camps are safe zones anyway. A bed in the open is a choice.

### 2.10 PvP zones

| Zone | Tag | Rule |
|---|---|---|
| Spawn camps (3 per biome, 30 m) | SPEC (BUILT `AWRSafeZone`, radius 3000) | Safe: no PvP, no building, creatures don't chase in |
| Trading hubs | SPEC (DD "safe zones at spawn and trading hubs") | **Villages** are the trading hubs (world-map, KEPT: one newcomer village per biome, 250 m from the middle camp). Safe zone radius **60 m** |
| Passes | SPEC (MP: borderland, PvP hotspot) | Open PvP, no claims (world-map) |
| Boss arenas | SPEC setting | 2.9.3 |
| Everywhere else | SPEC | Open PvP on PvP worlds |

#### 2.10.1 Who the peace turns away
Hunted and Reviled players (infamy 60+) can't enter camp or village safe zones (biome-crystal 2.21.7 KEPT). They're stopped at the post line and turned with a state word.

#### 2.10.2 The frith posts (GAP-COOKED: the edge is shown in the world, not only on the HUD)
- **What they are.** Every safe zone's edge is marked by **frith posts**: waist-high oak posts (1.1 m tall, 18 cm square, the top cut to a low pyramid), every **8 m** around the circle. Each is carved on its outward face with an open hand over a crossed-out blade, rubbed with red ochre that has weathered to rust-brown, and has a strip of undyed wool tied below the head. Camp posts are driftwood-grey oak from the wrecks the camps grew out of (world-map: every camp sits by its ship's wreck). Village posts are newer, oiled and darker.
- **Why they're there.** The castaways who made camp drove the posts as their thing-peace. The wool strips are the newer arrivals' offerings, so the oldest posts carry the most strips. That's the whole story, told by the posts.
- **How they meet the ground.** Each post is set 40 cm into the terrain (no visible base) and leans up to 3° on slopes, as real posts settle. A post that falls on a rock moves along the circle by up to 2 m to find soil. It never floats or sinks into stone. They're collidable (you walk round them), and they don't block creatures' paths.
- **Cue.** Crossing the line shows the state word **FRITH** (entering) or **OPEN GROUND** (leaving) on PvP worlds only, once per crossing, after 3 s outside so it doesn't flicker on the edge.

#### 2.10.3 PvE and co-op worlds
PvP is off (SPEC). No frith words, though the posts still stand (they're the camp's story).

#### 2.10.4 Sparring duels (GAP-COOKED, server setting **Duels**, default On for PvE and Casual co-op, Off on PvP standard where it means nothing)
- **Starting one.** Face another player within 5 m and pick **Challenge** from the interact wheel. They get "Mira challenges you" (Accept / Decline, 15 s). On accept, a 12 m ring of rope is pegged out on the ground around the midpoint (a real rope mesh with ash pegs every 1.5 m, laid in 1 s by a client-side unrolling spline). Then a 3-2-1 count, and the grið holds.
- **Rules.** Full PvP rules and numbers (combat 2.20). Only the two duellists can hit each other; nobody else can hit them, or be hit by them. Leaving the ring by 3 m, a creature hit, or 3 minutes passing ends it as a draw.
- **The end.** At 0 HP the loser drops to one knee (real clip, `yield`) with 1 HP and no death penalty, and the ring is pulled up. This isn't a safety floor in the game's fights: it only exists inside a fight both players agreed to, on a world with PvP off.
- **Not allowed** in safe zones, boss arenas, portals, while tagged, or while mounted.

### 2.11 Server settings

**SPEC (server-settings.md, M7 2, BUILT `server_presets.json`):** three locked official presets (PvP standard, PvE, Casual co-op), Playtest (internal), and Custom. The world settings screen is BUILT. Fixed on every server: the 10% loot rule, the 150/100 path cap, and artifact rules on the wearer only. Full release adds portal break timers, artifact limits, invasion frequency and raid group sizes (SPEC, their own lanes).

The full list, with every new key (new rows are GAP-COOKED unless KEPT is written):

| Key | Range | PvP std | PvE | Casual | Playtest | Tag |
|---|---|---|---|---|---|---|
| `pvp` | on/off | On | Off | Off | Off | SPEC |
| `deathPenalty` | gentle/souls/drop/full | souls | souls | gentle | gentle | SPEC |
| `offlineProtection` | 0 to 1 | 0.75 | 0 | 0 | 0 | SPEC |
| `offlineArmMinutes` | 0 to 60 | 15 | — | — | — | GAP-COOKED |
| `offlineQuietMinutes` | 0 to 60 | 10 | — | — | — | GAP-COOKED |
| `pvpInBossArenas` | on/off | On | — | — | — | SPEC |
| `bounties` | on/off | On | Off | Off | Off | SPEC |
| `clanSizeLimit` | 2 to 50, 0 = none | 8 | 12 | 0 | 0 | SPEC |
| `alliances`, `allianceSizeMult`, `outnumbered` | | On, 1.5, On | — | — | — | KEPT (biome-crystal) |
| `xpMultiplier`, `gatherMultiplier`, `tamingMultiplier`, `cropMultiplier`, `craftMultiplier` | 0.5 to 10 | 1 | 1 | 2/2/3/2/2 | 10/2/10/2/2 | SPEC |
| `bossRespawnHours` | 1 to 24 | 4 | 4 | 2 | 1 | SPEC |
| `upkeepMultiplier` | 0.5 to 3 | 1 | 1 | 0.5 | 0.5 | SPEC |
| `raids` | off/rare/normal/frequent | normal | normal | rare | rare | SPEC |
| `duels` | on/off | Off | On | On | On | GAP-COOKED |
| `combatTagSeconds` | 5 to 60 | 15 | 15 | 15 | 15 | KEPT (combat 2.20) |
| `logoutCamp` | on/off | On | Off | Off | Off | GAP-COOKED |
| `deathBagMinutes` | 5 to 120 | 30 | 30 | 30 | 30 | SPEC (BUILT `death.bagMinutes`, now settable) |
| `emptyClaimLapseHours` | 0 = off, 6 to 168 | 24 | 24 | 0 | 0 | GAP-COOKED |
| `globalChat` | on/off | On | On | On | On | GAP-COOKED |

**Not presets; per world in `access.json`:** `maxPlayers` (listen 2 to 16, dedicated 2 to 100), `password`, `whitelist`, `admins`, `bans`, `motd`, `restartEvery`.

**Locking:** official presets lock every value (SPEC); a change on the screen turns the world Custom (BUILT `IsCustom`). Edge case: a setting changed mid-game takes effect at once (BUILT `ChangeWorldSettings`). Offline-protection timers already counting keep their start time.

### 2.12 Scale

| Item | Tag | Design |
|---|---|---|
| Servers of 10 to 100 | SPEC (DD Overview; M7) | The dedicated target |
| **Listen worlds: 2 to 16, default 8** | GAP-COOKED (the brief's 4 to 16) | The host's PC renders and serves, so the cap is 16. Tuned for 4 to 8 and holding at 16 |
| Networking scale: push-model replication, dormancy and relevancy for buildings and tames; evaluate Iris before M7 | SPEC (TECH_PLAN) | Numbers below |
| **Server tick** | GAP-COOKED | `NetServerMaxTickRate` 30 (listen and dedicated); the listen host renders uncapped |
| **Bandwidth** | GAP-COOKED | `MaxClientRate` and `MaxInternetClientRate` 100,000 B/s; `TotalNetBandwidth` 1,600,000 on listen worlds (16 × 100 kB) |
| **Build pieces** | GAP-COOKED | `NetDormancy = DORM_Initial`; `FlushNetDormancy()` on health, door, lock, storage or crop changes; `NetCullDistanceSquared` = (300 m)²; `NetUpdateFrequency` 2; push-model `MARK_PROPERTY_DIRTY` on every replicated field. Doors update at once on open (`ForceNetUpdate`) |
| **Creatures** | SPEC + numbers (BUILT `significance`, 150 m for remote players) | Kept. Tames: dormant while on Stay with nobody within 100 m |
| **Players** | GAP-COOKED | `NetUpdateFrequency` 30, `MinNetUpdateFrequency` 10; `NetCullDistance` 500 m (nameplates fade from 40 m and are hidden past 60 m, except clan and allies, who show on the compass at any range) |
| **World streaming for joiners** | SPEC (BUILT ×24, whole island) | Kept while the map is the island. On the 49 km² continent the whole map can't stream to every client: server streaming plus HLODs are needed (FULL-RELEASE-PLAN, "needed for full release"). That belongs to the world-map lane, and the line is in LATER-COOKS |
| **Per-player world load** | SPEC (BUILT): wildlife packs come out near any player; creature raids wait for a member at home | Kept. With 16 players spread out, up to 16 activation bubbles of 250 m. The significance subsystem (BUILT) keeps far creatures cheap |
| **Boss scaling** | SPEC (BUILT: HP rescales per player; tames count) | Kept |

### 2.13 Hearth and wake (respawn) rules

| Item | Tag | Design |
|---|---|---|
| Death screen: a map of where to wake; Wake works after 10 s | SPEC (BUILT `WRFlowDeath.cpp`, `player.respawnSeconds`) | Kept |
| Beds (rest 60 s after a wake), bedrolls (used up on waking), your clan's beds, your path's three camps | SPEC (BUILT `WRBeds.h`, `beds.restSeconds`) | Kept |
| Allies' beds, if the owner clan allows | KEPT (biome-crystal 2.21.2) | Kept |
| Hunted players can't wake at camps | KEPT (biome-crystal 2.21.7) | Kept |
| No wake-point swap while combat-tagged | KEPT (combat 2.20) | Kept |
| **The Hearth is a wake point** | GAP-COOKED (FULL-GAME 2 lists it; the code doesn't) | Every **unbroken** Hearth is a wake point for its claim's clan (and the solo owner), named "<claim name> Hearth". After you wake there it **rests 300 s for you** (Rust's sleeping-bag cooldown; longer than a bed because a Hearth can't be destroyed, only broken). A broken Hearth is greyed on the death map ("Cold: repair it to wake here") |
| **Wake order on the death map** | GAP-COOKED | Default first (the one you last set with E, BUILT), then your beds, Hearths, clan beds, ally beds, then camps. Each shows its distance from where you died, and resting ones show seconds left |
| **Waking in a claim under attack** | GAP-COOKED | Allowed (defenders coming home is the game), but the death map marks it "Under attack" in crimson so it's a choice |
| **Respawn location for a new clan member** | GAP-COOKED | 2.2.3 |

#### 2.13.5 The death satchel (GAP-COOKED: replaces the sphere primitive)
- **What it is.** A hide satchel strapped to a rolled fur bedroll, lying where you fell, with the strap loose and a few things spilled out. One mesh (about 4,000 tris, 2K textures). It settles with a 0.3 s physics drop onto the ground under you (trace down, aligned to the ground's slope).
- **Souls-like (owner only).** A thin column of path-coloured light rises 6 m from it (Fire ember-orange, Frost pale blue, each path's colour from the paths data). Only the owner sees the column, from up to 200 m; others see just the satchel. A soft heartbeat-like hum plays within 8 m. The owner's map and compass show a satchel pin with minutes left.
- **Drop or full loot.** No light; everyone sees the satchel. The owner still gets the pin.
- **Spilled chest.** A different mesh: a split crate with its contents tumbled out.
- **How it meets the world.** No collision with players (BUILT); it does collide with the ground. It floats on water (bobbing gently) and drifts 1 m a minute with the current at most.

### 2.14 Design with intention: what each new object is for

| Object | Why it's here | What it connects to | How it meets what it touches |
|---|---|---|---|
| **Raid bell** on each totem | Tells everyone near, defenders and raiders alike, that the claim is hit. Sound, not only UI | It's hung from an iron bracket bolted through the totem's top (BUILT carved post, `pieces.json` mesh). Struck by its own clapper whenever the claim takes player, tame or siege damage (crystal's trigger) and at the creature raid warning (BUILT) | A bronze bell 28 cm across. The bracket's two bolts go through a drilled iron strap wrapped round the post. The bell swings on a pin: a damped pendulum (±18°, 1.1 s period, decaying over 4 s, so never snapping) and the clapper (a separate bone) lags it. The sound is a real bronze strike, layered with a hum tail. Heard at 300 m (attenuation in 3.4). It rings at most once every 2 s, so a long raid doesn't make a wall of noise |
| **Frith posts** | Show the safe zone in the world, and tell the camps' story | The camp's own wreck timber (world-map); the safe-zone radius (`AWRSafeZone`) | 2.10.2 |
| **Death satchel** | Show a death spot as a believable thing that someone can recover | The owner's map pin; the death penalty mode | 2.13.5 |
| **Clan banner** (decor piece) | Mark whose base this is, in a way readable from 50 m | The clan's sigil and colour; it can only be placed in that clan's claim | An ash pole set into a foundation socket or 40 cm into the ground. The linen moves with the world wind (cloth sim LOD 0 within 30 m; vertex wind past that). It never clips walls, because placement needs 1.2 m clear around the pole head |
| **Duel ring** | Make the duel's grið visible: who's in it, and where it ends | The two duellists; it's pegged at their midpoint | Rope pegs trace down to the ground every 1.5 m, so the rope drapes over the ground's shape. It's pulled up (reverse unroll, 0.8 s) when the duel ends |
| **Address card, join screen, player list** | Make friend-joining work without help (QUALITY-PLAN area 11) | UPnP, EOS, `access.json` | Screens only |

### 2.15 Screens (mouse and pad on every one, no dead states, F1 page each)

| Screen | Open | Contents | Pad |
|---|---|---|---|
| **Player list** | Hold **Tab** (pad: hold View) | Rows: clan tag, name, path icon, ping (green < 80 ms, amber < 160, red), "Host" or "Admin" badge. Click a row (pad A) for a menu: **Whisper**, **Invite to clan**, **Challenge** (if duels are on and they're within 5 m), **Mute** (local, for you only). Admins also get **Kick**, **Ban 24 h**, **Ban**, **Make admin**, **Release name** | D-pad moves, A opens the row menu, B closes |
| **Hosting panel, address card** | Worlds list; pause menu | 2.1 | Full focus path |
| **Join screen** | Main menu → Join | 2.2.1 | LB/RB tabs; A joins; Y favourites; X forgets |
| **Backups** | Worlds list → ⋯ | 2.4 | A restores (with a confirm) |
| **Chat** | Enter (pad: on-screen keyboard via the Steam/Windows keyboard if present; otherwise the quick-chat wheel is the pad's way to talk) | 2.6 | The wheel always works, so there's no dead state |
| **Ping and quick-chat wheel** | Middle mouse / hold D-pad up | 8 slots (2.6) | Stick picks, release sends |
| **Raid summary card** | At login after a raid | 2.8.1 | A continues |
| **Duel challenge** | Interact wheel | 2.10.4 | A accepts, B declines |
| **F1 pages added** | F1 | **Hosting** (what UPnP does; how to open UDP 7777 on a router in 5 numbered steps; how to copy a world to a server), **Joining**, **Clans and claims**, **PvP and death** (death modes, the satchel, make camp, tags, offline protection timing), **Chat and pings** | |

All screens follow the ui-controls lane (KEPT): prompts for the last device used, B/Esc backs out, no screen without an exit.

### 2.16 Edge cases (all handled)

| Case | Handling |
|---|---|
| Two people join with the same new name at the same moment | The server claims names under one lock; the second gets the name-taken message |
| A player is kicked mid-craft | The station queue carries on and delivers to the tray (BUILT) |
| A player is banned while their tames guard a claim | The tames stay the clan's (clan-owned) or go to Stay if personal; bans don't touch the world |
| The host changes PvP on while players stand in the open | Takes effect at once, with a banner to everyone: "PvP is now ON on this world" |
| A death satchel falls into lava or below the kill depth | It's placed at the last safe ground point the player stood on in the past 10 s (the server keeps a ring of 10 positions) |
| Clock or timezone differences | Every timer uses server UTC; clients show local time |
| A world copied from a listen host to a dedicated server | The same folder runs unchanged; `access.json` keeps the admins (the old host stays admin through their key hash) |
| UPnP opened the port but the ISP uses CGNAT | Public IP ≠ the router's WAN IP → the address card shows amber: "Your internet provider shares your address: use a Join code (coming) or a free tunnel (F1)" |
| A clan leader leaves | BUILT passes leadership to the first member; with ranks it goes to the senior Officer (crystal), else the longest-standing Member |
| Full-loot death inside a claim of yours | Your own clan's pieces don't shield the satchel; anyone who can reach it can loot it |

---

## 3. Plan

Raven's patterns throughout: **data-driven** (every number in `Data/*.json`, read through `WRTune`/`FWRTuning`), **all damage through `UWRCombatLibrary::ApplyHit`** on the server, abilities as **`UWRGameplayAbility` + `UWRAbilityRegistry::ClassForNode`**, screens registered with `WR_REGISTER_SCREEN` / `WR_REGISTER_JOURNAL_TAB`, every control in `FWRKeyBindings` for mouse and pad.

### 3.1 Data rows

| File | Rows and fields |
|---|---|
| `Data/server_presets.json` | Add to all four presets: `offlineArmMinutes` 15, `offlineQuietMinutes` 10, `duels` (pvp_standard false, others true), `combatTagSeconds` 15, `logoutCamp` (pvp_standard true, others false), `deathBagMinutes` 30, `emptyClaimLapseHours` (24, 24, 0, 0), `globalChat` true. Crystal's `alliances`, `allianceSizeMult`, `outnumbered` as that lane wrote them. Update the `_comment` |
| `Data/tuning.json` new `net` | `{ "listenMaxPlayers": 16, "listenDefaultPlayers": 8, "dedicatedMaxPlayers": 100, "serverTickRate": 30, "clientRate": 100000, "pieceCullCm": 30000, "pieceUpdateHz": 2, "playerCullCm": 50000, "playerUpdateHz": 30, "playerMinUpdateHz": 10, "tameDormantRangeCm": 10000, "upnp": 1, "upnpLeaseSeconds": 7200, "upnpRenewSeconds": 3600, "port": 7777, "pingTimeout": 1.0 }` |
| `tuning.json` new `guard` | `{ "reachSlackCm": 150, "buckets": { "chat": [1,5], "interact": [8,12], "inventory": [20,40], "build": [6,10], "ping": [2,4] }, "strikesToKick": 30, "strikeWindow": 60, "kicksToBan": 3, "autoBanHours": 24, "passwordTries": 3, "passwordLockSeconds": 60, "timeMarginMax": 0.25, "timeMarginMin": -0.25, "drift": 0.05 }` |
| `tuning.json` new `session` | `{ "hostCloseSeconds": 30, "restartWarnMinutes": [15,5,1], "motdMax": 300, "leaderQuietDays": 14, "joinNotice": 1 }` |
| `tuning.json` new `chat` | `{ "localRangeCm": 4000, "history": 50, "maxLen": 200, "colors": { "global": "#E8DFC8", "local": "#B8AE9C", "alliance": "#3FB8B0", "whisper": "#C7A6E0" } }` |
| `tuning.json` new `claims` | `{ "gapCm": 1000, "minPieces": 4, "edgeMoteSpacingCm": 400, "edgeShowRangeCm": 4000 }` |
| `tuning.json` `beds` | Add `"hearthRestSeconds": 300` |
| `tuning.json` new `logout` | `{ "campSeconds": 6, "lingerSeconds": 10 }` (the tagged body time is combat's `pvp.logoutBodySeconds`) |
| `tuning.json` `death` | Add `"beaconHeightCm": 600, "beaconRangeCm": 20000, "humRangeCm": 800, "safeHistory": 10` |
| `tuning.json` new `duel` | `{ "rangeCm": 500, "ringRadiusCm": 1200, "leaveCm": 300, "maxSeconds": 180, "acceptSeconds": 15, "countdown": 3 }` |
| `tuning.json` new `backups` | `{ "hourlyKeep": 24, "dailyKeep": 7, "shutdownKeep": 5 }` |
| `tuning.json` new `ping` | `{ "seconds": 60, "holdSeconds": 0.4 }` |
| `tuning.json` new `raidBell` | `{ "swingDeg": 18, "periodSeconds": 1.1, "decaySeconds": 4, "minGapSeconds": 2, "hearRangeCm": 30000 }` |
| `Data/pieces.json` | `clan_banner` (decor, freeform, Hearth 1: linen 6, ash pole 1, dye 1; `clanOnly`: true, `clearRadius`: 120) |
| `Data/recipes.json` | `clan_banner` at the workbench |
| `Data/items.json` | `linen` (if missing), and the 8 dyes as items, each from a real source: madder root, woad leaves, weld, oak galls, lichen, walnut husk, verdigris (copper + vinegar), bone ash. Recipes at the alchemy table, 1 source → 2 dye. Item ids `dye_madder`, `dye_woad`, `dye_weld`, `dye_oakgall`, `dye_lichen`, `dye_walnut`, `dye_verdigris`, `dye_boneash` |
| `Data/clan_sigils.json` (new) | 24 rows `{id, name, texture}`; 8 dye rows `{id, name, color}` |
| `Data/Audio/sounds.json` | Cues in 3.4 |
| World data (`world_pois.json`, world-map's file) | `frith_posts` rings auto-generated per safe zone (3.5) |
| Saved per world | `access.json` (new); `world.json` gains `bags`, claim `raidLog` (last 10), totem `ClanOwner`, `Name`, `PlacedAt`, `LastAttackedAt`, clan `LastOnlineAt`; `clans.json` gains `tag`, `color`, `sigil`, `lastSeen` per member; `players/<name>.json` gains `keyHash` |

### 3.2 C++ (follow Raven's file pattern)

| File | Add / change |
|---|---|
| `WRGameMode.h/.cpp` | Override `PreLogin(Options, Address, UniqueId, ErrorMessage)`: password check (salted SHA-256 against `access.json`), `maxPlayers`, ban (by key hash and by IP), whitelist, key-vs-name check (`WRIdentity::Verify`), with a per-IP try counter. `InitNewPlayer`: stores the key hash on first claim. `PostLogin`: join notice, MOTD, raid summary, clan invite-first. `Logout`: leave notice, logout-camp body (`AWRLogoutBody`). New `CloseWorld(float Seconds)` with the broadcast; the restart schedule timer |
| New `WRIdentity.h/.cpp` | Client: `LoadOrMakeKey()` (32 bytes from the OS crypto RNG: `BCryptGenRandom` on Windows, `/dev/urandom` on Linux, behind one `WRIdentity::RandomBytes`), `KeyForWorld(WorldId)` (SHA-256 via the engine's `FSHA256Signature` helper in `PlatformCrypto`, base64 out). Server: `Verify(Name, KeyB64, OutWhy)`, `Release(Name)` |
| New `WRAccess.h/.cpp` | `FWRAccess` singleton for the current world: load/save `access.json` atomically; `IsAdmin`, `Ban`, `Unban`, `Kick`, `SetPassword`, `CheckPassword`, `Motd`; `admin.log` append |
| New `WRNetGuard.h/.cpp` | `UWRNetGuardSubsystem` (world subsystem, server): `bool Allow(APlayerController*, EWRRpcFamily)` token buckets; `Strike(PC, Kind)`; kick and auto-ban. Every Server RPC in `AWRPlayerController` calls `Allow` first and `WRNet::InReach(PC, Actor)` for range |
| `Config/DefaultGame.ini` / `DefaultEngine.ini` | `[/Script/Engine.GameNetworkManager]` time-discrepancy flags and margins (2.5); `[/Script/OnlineSubsystemUtils.IpNetDriver]` `NetServerMaxTickRate=30`, `MaxClientRate`, `MaxInternetClientRate`; `[/Script/Engine.Player]` `ConfiguredInternetSpeed`, `ConfiguredLanSpeed` |
| `WRWorlds.h/.cpp` | `SaveAtomic(Path, Text)` used by every save (GameMode, Clans, Access); `Backup(EWRBackupKind)`; `ListBackups(Id)`; `Restore(Id, Backup)`; `LoadWithRecovery(Path, OutText, OutNote)` |
| `WRSocial.h/.cpp` | `FWRClans`: `Tag`, `Color`, `Sigil`, `LastSeen` per member, `SetLook`, `Disband`, `LeaderHandover()` (daily check), invite by name before first join (already possible: Invites holds names). `FWRSocialRules::IsProtectedNow(Claim)`: arming and quiet timers using `LastClanOnlineAt` / `LastAttackedAt`. `PlayersCanFight`: the tagged-in-safe-zone rule, duel pairs. `AWRLootBag`: the satchel mesh and spill-crate mesh (`Data/player_art.json`-style path rows), owner-only beacon (a Niagara column made visible only to the owner via `bOnlyOwnerSee` on an owned component, with owner = the player's controller), hum, safe-history placement, float-on-water |
| `WRBuilding.h/.cpp` | `AWRClaimTotem`: `ClanOwner`, `Name`, `PlacedAt`, `LastAttackedAt`, `RaidLog`; `CanBuild` uses the clan owner; placement checks the 70 m gap; lapse check in `Tick`'s minute clock; the raid bell component (`UWRRaidBellComponent`: a skeletal bell with a pendulum spring solved in `TickComponent` on clients from a replicated `RingAt` time, so all clients swing the same and nothing snaps). `TakeBuildDamage`: `IsProtectedNow` replaces `IsAnyoneOnline`, records `LastAttackedAt` and the raid log. `AWRBuildPiece`: `NetDormancy = DORM_Initial`, `FlushNetDormancy` in every setter, push-model dirties |
| `WRBeds.h/.cpp` | Hearths as wake choices in `FillWakeChoices` (unbroken only, `hearthRestSeconds` per player), the wake order, the "Under attack" flag |
| `WRPlayerController.h/.cpp` | `ServerChat(Channel, Target, Message)` with channels; `ClientChat` keeps 50; `ServerPing(Location, Kind)`; `ServerChallenge`, `ServerAnswerChallenge`; admin RPCs (`ServerAdminKick/Ban/Mute/MakeAdmin/ReleaseName`) gated by `FWRAccess::IsAdmin` |
| New `WRDuel.h/.cpp` | `AWRDuelRing` (replicated: two player states, centre, start time; the rope spline unrolls client-side); `UWRDuelSubsystem` (rules, end conditions, the yield at 0 HP via `ApplyHit`'s death branch checking `IsDuelPair` before death: health set to 1 and the `yield` montage instead of death, server-side) |
| New `WRGA_MakeCamp` | `UWRGameplayAbility` (Raven's pattern; registered in `UWRAbilityRegistry` as a non-tree ability by tag `Ability.MakeCamp`, as other non-node abilities are). Plays `logout_camp` via `AbilityTask_PlayMontageAndWait`; any damage tag cancels it; on finish, the server ends the session for that player |
| New `WRUPnP.h/.cpp` + `ThirdParty/miniupnpc` | `FWRUPnP::OpenPort(Port, Lease)`, `Renew`, `Close`, `ExternalIP()`; async on a worker thread; results posted to the game thread |
| New `WREOS.h/.cpp` (round MP-5) | EOS Connect device-id login; lobby create with the code attribute; resolve code → P2P address; the `NetDriverEOS` definition in `DefaultEngine.ini` used only when joining or hosting by code, so the IP driver stays as it is |
| UI: `WRUIWorlds.cpp` (hosting panel, backups), `WRMainMenu.cpp` (join tabs, recent and favourites in `Saved/Config/recent_servers.json`, password modal, rejection reasons), new `WRUIPlayers.cpp` (player list), `WRUIClan.cpp` (look editor: tag, 8 dye swatches, 24-sigil grid), `WRUIClaim.cpp` (name, protection state, upkeep time), `WRUIWheel.cpp` (ping wheel reuse), `WRUIFlow.cpp` (join and leave notices, raid summary card, MOTD card, closing banner, FRITH words, TAGGED rim), `WRUIMap.cpp` (claims circles, pings, satchel pin), `WRUIControls.cpp` (F1 pages) | Each registered with `WR_REGISTER_SCREEN` or `WR_REGISTER_JOURNAL_TAB`; every action bound in `FWRKeyBindings` for keys and pad |

### 3.3 Assets and the art route

| Asset | Route | Notes |
|---|---|---|
| Death satchel (satchel + rolled fur + spilled items) | Concept → Hunyuan3D 2.1 base → Blender finish (retopo about 4k tris, 2K PBR) | One mesh, path colour only in the beacon |
| Spilled crate | Fab pack (a medieval broken-crate set), else the same Hunyuan route | |
| Raid bell + iron bracket | Fab pack (bronze bell) + Blender bracket; bell and clapper as a 2-bone skeletal mesh (Tools/art rig scripts) | The clapper swings separately |
| Frith post | Concept → Hunyuan3D → Blender (carved hand-and-blade face, ochre mask channel, a separate wool-strip card) | Two material instances: wreck-grey and village-oiled |
| Clan banner | Blender (pole, finial, cloth plane with Chaos Cloth) | Sigil textures: 24 hand-drawn marks, carved-woodcut style, 512² masks |
| Duel rope and pegs | Blender (rope tile mesh for the spline, ash peg) | |
| Beacon column | Niagara (one ribbon plus soft embers), tinted per path | |
| Animations | Hero lane's MetaHuman/GASP set: `logout_camp` (sit, unroll pack, lie back: 6 s), `yield` (drop to one knee, 1.2 s), `duel_ready` (0.8 s), `challenge` gesture (1.0 s). Mocap-pack clips retargeted (Tools/art rig scripts) | Real clips, no gliding |
| UI icons | Clan sigils, ping wheel slot icons, router chips, satchel pin | WRUI icon set style (stone and leather) |

### 3.4 Sound and music cues (`Data/Audio/sounds.json`)

| Cue | Sound | Mix |
|---|---|---|
| `mp.raid_bell` | Real bronze bell strike (3 velocities) + a 4 s hum tail; clapper buzz on fast swings | Spatial, falloff to 300 m (log), reverb send by biome |
| `mp.join` / `mp.leave` | Soft wooden knock + low reed note (join); the reverse (leave) | UI bus, −12 dB |
| `mp.claim_attacked` | The bell, plus a horn call (cow horn, 1.2 s) on the HUD bus for members far away | UI bus |
| `mp.frith_in` / `mp.frith_out` | Wool rustle + a faint low drone in / a cut-off out | UI, −18 dB |
| `mp.satchel_hum` | Low heartbeat-like pulse, 60 bpm, filtered | Spatial, 8 m, owner only |
| `mp.make_camp` | Pack straps, unrolling fur, a sigh | Foley on the hero |
| `mp.duel_ring` / `mp.duel_count` / `mp.yield` | Rope slap and peg knocks / three drum hits / armour settling plus a breath | Spatial |
| `mp.host_closing` | A sea-horn, one long note per 10 s tick | UI |
| **Music: player raid** | A **raid stem**: taiko-style frame drums and low brass, layered over the biome's composed track in key and tempo (the biome music system's stem slots). Starts on the first raid bell for members within 300 m of the claim; fades out 30 s after the last hit | The biome track ducks at most 4 dB under it (THE BAR); never silent |
| **Music: creature raid** | Same stem slot, with the biome's own combat stem | Same rules |
| **Music: duel** | None of its own; the biome's combat stem plays | Same rules |

### 3.5 Map dressing

| Item | Where | How |
|---|---|---|
| Frith posts | Every camp (radius 30 m: 24 posts) and village (60 m: 47 posts) | Generated from each `AWRSafeZone` radius by `Tools/terrain` POI tooling into `world_pois.json` as instanced static meshes, with the soil-and-slope rules in 2.10.2. Wool strips: 0 to 4 per post, random seed per camp, more on posts facing the camp's wreck |
| Bounty boards | Camps and villages | KEPT (biome-crystal) |
| Raid bells | On every totem | Part of the totem actor, not world data |
| No claim-free markers in the world | Passes, shrines, arenas | The rule shows only in build mode (the claim ghost turns red, with the reason chip from ui-controls) |

### 3.6 Build order (proof-of-one first)

| Round | Build | Why this order |
|---|---|---|
| **MP-1 proof of one: Identity + PreLogin** | `WRIdentity`, `WRAccess` (password, maxPlayers, bans, admins), `PreLogin` with every rejection reason on the join screen, the password modal. One world, one password, one wrong-name rejection | Closes the open name-theft hole. Everything else (admins, kicks, bans) stands on it |
| MP-2 Saves you can trust | `SaveAtomic` everywhere, rolling backups, corrupt-file recovery, save-on-moments, the Backups screen, bags saved on clean shutdown | Persistent worlds must never lose a night |
| MP-3 Join without help | UPnP, the address card and router chips, recent and favourite servers, ping query, F1 Hosting and Joining pages, host-closing flow and messages | QUALITY area 11: "a friend installs it, joins and plays without help" |
| MP-4 Guarding the server | `WRNetGuard` buckets and strikes, range checks on every RPC, movement discrepancy flags, the Player list with admin actions, MOTD, scheduled restart | Needs MP-1's admins |
| MP-5 Join codes | EOS Connect device id, lobby codes, P2P with relay, the CGNAT message | Removes port-forwarding entirely |
| MP-6 Living together | Chat channels and the 50-line history, ping and quick-chat wheel, clan call, join/leave notices, clan look (tag, dye, sigil, banner piece), clan-owned claims with the bring-your-claims prompt, leader handover, disband | Social layer for 4 to 16 |
| MP-7 Claims and death | Claim gap, lapse, names, edge motes, map circles; offline arming and quiet timers; raid summary card; raid bell (mesh, pendulum, sound); Hearth as a wake point and the new wake order; death satchel and spill crate replacing the sphere; walled-in login rescue | Every rule the base game needs, before the PvP pass |
| MP-8 PvP edges | Make camp (`WRGA_MakeCamp`), linger body, tagged-not-safe in safe zones, frith posts and their words, villages as 60 m safe zones (when villages ship, R3), duels with the ring | Some parts of the PvP rules, before the R9 PvP pass (crystal) adds ranks, alliances, bounties and siege |
| MP-9 Scale | Build-piece dormancy and push-model, cull distances and rates, tame dormancy, tick and bandwidth ini | Needed before 16-player bases fill up |
| MP-10 Dedicated server (on hold per spec) | Package the server target once 200 GB is free on an existing drive; `run.ps1` restart loop; SQLite (M7 1) with migrations; `access.json` admin on a headless server | The only step that waits |

**Gates for every round:** compile, cook and package (Aaron 11-12). Then a spot-check against this file by reading the code. Aaron and his testers test.
