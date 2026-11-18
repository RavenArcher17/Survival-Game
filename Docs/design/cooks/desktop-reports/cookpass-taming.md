# cookpass-taming: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-taming.out).

LANDED cookpass-taming decc9f7 d9fb85b fdbb1eb

I did the full cook pass on taming, mounts and breeding: research, grades, requirement ledger and plan are in `showcase-notes/notes/Wildreach/cooks/cookpass-taming.md` (plan committed before any code). The work landed in three commits, each compiled clean with no warnings and pushed. Nothing was played or tested here, per the gates.

**What changed in the game:**
- **Riding:** the rider now sits on the saddle instead of standing on it. It's a code-built pose (no animation clips exist for it), and the body follows the mount's gait. Getting on and off is eased instead of snapping. Getting off always lands on clear ground, never in a wall, lava, deep water or off a ledge.
- **Ride keys** now follow the player's own key bindings.
- **Call your mount:** a new whistle-wheel wedge. Your last-ridden mount runs to your side from up to 300 m.
- **Tame names**, and **Release now has to be picked twice** so one mis-flick of the wheel can't lose a tame.
- **The tame card** has tabs (switch with LB/RB on a pad), real stat numbers and the tame's parents.
- **The stable** is a new build piece. Stabled tames don't eat, age or count toward your tame limit.
- **The way back:** a dead tame drops its collar. A new Dadeni draught used at your Hearth within 72 h brings it back once. The Soulbond perk is now live.
- **Elements:** tames' bites now carry their own element in every land, not just fire and frost.
- **Barding:** ten land barding sets were craftable but did nothing beyond armour. Every live land's set now gives its hazard pass, for the mount and its rider.
- **Also:** courting pairs and cuddled babies now nuzzle. The Tusker talisman's +15% carry works. Tames from other lands now take rift drain in the Wasteland and slowly recover afterwards.

**Requirement ledger:**

| # | Requirement | Status | Where |
|---|---|---|---|
| Q1 | Research | done | cook doc §1 |
| Q2 | Grade every built piece | done | cook doc §2 |
| Q3 | Plan before building | done | doc §3, showcase-notes 193f698 |
| Q4 | Build to plan | done | all three commits; L7 not built |
| Q5 | Spot-check and fix | done | doc §4; fixes in d9fb85b |
| Q6 | file:line proof | done | every ledger row |
| Q7 | Design with intention | done | `WRBuildLookDecor.cpp:750` stable model; `WRHeroAnimInstance.cpp:433` seat |
| Q8 | Data-driven, ApplyHit, table rows | done | tuning `tame.*`; sounds.json / fx.json rows; Soulbond through `ApplyHit` |
| Q9 | Mouse and pad screens | done | `WRUIStable.cpp:284`; `WRUITameCard.cpp:211`, `:456` |
| Q10 | Multiplayer | done | `FWRTameRep` fields replicated; stable `Roster` replicated; all server commands |
| Q11 | Land in pieces | done | 3 commits, CHANGELOG + HANDOFF in each |
| L1 | Rider sit pose | done (in code, no clips) | `WRHeroAnimInstance.cpp:433`, `WRCharacter.cpp:1022` |
| L2 | Courtship / nuzzle | done (head dip) | `WRCreature.cpp:701` |
| L3 | Stable | done | `WRTaming.cpp:2139`, `:2220`; `WRPlayerBuild.cpp:838`; `WRBuildLookDecor.cpp:750` |
| L4 | Bond token + Dadeni revive | done | `WRTaming.cpp:2258`, `:2323`; `WRGameMode.cpp:361`, `:492` |
| L5 | Soulbond | done | `WRTaming.cpp:2409`; `WRCombatLibrary.cpp:918` |
| L6 | Names | done | `WRTaming.cpp:2006`; `WRUITameCard.cpp:76` |
| L7 | Bone altar undead tames | **not built** | Necromancy-path ritual; re-filed on LATER-COOKS |
| L8 | Whistle answer calls | partly | tames answer with their idle call; species' own calls need the audio pass |
| L9 / L10 | Windmane body / sculpted Floeback | **not built** | need the art pipeline |
| L11 | Land barding passes | done | `WRTaming.cpp:2491`; `WRInventory.cpp:665`; `WRStormWorld.cpp:537`; `WRForestHazards.cpp:245` |
| L12 | Tusker talisman carry | done | `WRTaming.cpp:642` |
| L13 | Off-biome rift drain | done | `WRVoid.cpp:493`; recovery at `WRTaming.cpp:910` |
| L14 | Monster line perk | **not built** | a fishing perk, out of scope |
| G1 | Every element kept | done | `WRTaming.cpp:1213` |
| G2 | Ride keys from bindings | done | `WRCreature.cpp:2029` |
| G3 | Release asks twice | done | `WRCreature.cpp:1878` |
| G4 | Smooth, safe mount/dismount | done | `WRCreature.cpp:1950`; `WRCharacter.cpp:1022` |
| G5 | Call your mount | done | `WRPlayerUI.cpp:966`; `WRTaming.cpp:2431` |
| G6 | Card tabs, real stats, parents | done | `WRUITameCard.cpp:211`, `:623`, `:672` |

**Not finished:**
- The bone altar's undead tames.
- Real riding and courtship animation clips; today's seat and nuzzle are built in code.
- Each species' own answer call.
- The Windmane and Floeback art.
- The seat height and leg spread (tuning `tame.seat`) are first guesses to eyeball in play.

I added three LATER-COOKS lines (bone altar, riding/courtship clips, species answer calls) and the LANDED line on the share.

**New binary files:** `Data/UI/Icons/bond_token.png` (8.9 KB) and `Data/UI/Icons/dadeni_draught.png` (8.3 KB), about 17 KB through Git LFS.
EXIT 0
