# Wildreach: proposal for a "final" build

To: Aaron. Read this before any cook.
Written 2026-11-10 against v0.7.1 (commit 372557e). A proposal only: nothing here is decided until Aaron says so. Wildreach is in Aaron's hands now, so where this says "Raven decides", that describes Raven's docs and the call comes to Aaron.

## The short version

Raven's docs never define a "final game" build. They define one finish line for this stage, in the build plan: **a group of friends plays a full evening on the server and wants to come back.** That has not been tried yet.

v0.7.1 is already published, with an installer. The two commits since it change only the changelog, docs and a setup check ("the game is unchanged"). So the finish line can be tried **without a new cook**.

Proposal: "final" for this round means the build a group of friends plays for that evening and wants to come back to. It is not a public release.

## Proposed definition of "final", inside First Playable

Fire and Frost only. Nothing from the other 10 biomes or paths, or the Full release and Dream tiers.

### Met (from the docs)

| Item | Where it says so |
|---|---|
| Biomes, paths, levelling, combat | Build plan "Where it stands": Done |
| Six bosses (1 major, 2 mini per biome) | Build plan: Done |
| Enemies and wildlife regions | Build plan: Done; 0.6.2 |
| Taming, survival, crafting | Build plan: Done |
| Building, with door code locks a player can actually use | Build plan: Done; keypad in 0.7.1 |
| PvP, safe zones, death penalty, PvE option, clans | Build plan: yes (inside the "Partly" Multiplayer row); Clan tab in 0.7.1 |
| Installer and auto-update | HANDOFF; small updates since 0.6.1 |
| Every system has a bot test | HANDOFF; 0.7.1 passed 39 of 39, one of them on a rerun alone |
| A friend who joins sees the whole island | 0.7.0 |
| First Playable design questions answered | DESIGN_REVIEW: all decided 2026-10-22. The open list (#13, #17 to #26) is all later biomes or Full release |

### Open

| Item | State | Needed for this round? |
|---|---|---|
| **The evening with friends** | HANDOFF: hasn't been run. It's the whole point | **Yes** |
| **Tuning from real play** | CLAUDE.md: all numbers are first drafts, to be tuned in playtests. Follows the evening | Yes, after the evening |
| **Packaged dedicated server** | HANDOFF: none, on hold, Raven's call. The host's PC is the server. Build plan counts Multiplayer as "Partly" and says "on the server" | Raven decides (question 2) |
| More than a few players | Build plan: nothing tested past a few. M7 asks for 100 bots before inviting friends and an 8-hour soak; neither is recorded, and its `Tools/bots.ps1` isn't in the repo | Raven decides (question 5) |
| GPU hang | PERFORMANCE: happens now and then under heavy GPU load, "not looked into yet". It closes the game for whoever it hits | Watch for it during the evening |
| HLODs (far detail) | The docs disagree. CLAUDE.md lists them as left to do, HANDOFF's open list doesn't, and 0.7.0 says they aren't needed for far terrain. Trees and rocks at a distance aren't settled | Proposed no (question 6) |
| Volcanic shrubs green | Raven, 0.7.0: "leave the shrubs green if it's a problem" | No |
| Tame whistle has no screen | HANDOFF: console only | No, unless Shipping |
| Unfinished skills and perks | Fishing, engineering and swimming had no XP source, and some perks did nothing (noted 10-23). Any change is not stated in the docs | No; look during the evening |
| Bundled shader cache | PERFORMANCE: recommended once at release time | Only if there's a new cook |
| Unsigned installer, UDP 7777 port-forward | README: both remain. Friends can click through the warning | No |
| Hunyuan3D licence limit | Release notes: the build isn't licensed in the EU, UK or South Korea | Raven should know where the friends are |
| Creature raids in DESIGN_REVIEW | #31 says "setting only, Full release", but raids shipped in 0.7.0 on a plan Raven approved. Left open here | Raven updates it (question 7) |
| Game title | DESIGN_REVIEW #26: still "Untitled"; Wildreach is the working title | Only for a public release |

## Proposed order of work

Smallest useful step first. Stop as soon as Raven says it's done.

| # | Step | Who |
|---|---|---|
| 1 | Answer the questions below | Aaron |
| 2 | Pick a group and an evening. Everyone installs the published v0.7.1 | Raven; Aaron helps with the group |
| 3 | Host and play the full evening on the Playtest preset. One person hosts and forwards UDP 7777. No dev setup needed, only the installer | Raven chooses the host |
| 4 | Write down what people said, what broke, and whether they want to come back | Aaron collects, Raven decides what matters |
| 5 | If they want to come back and nothing needs fixing: v0.7.1 is this round's final. **No cook** | Raven calls it |
| 6 | If numbers need changing: change `Data/*.json` only. No art or map work | Raven OKs the list; Raven or Aaron does it |
| 7 | If a new build is needed: follow HANDOFF "Releasing an update" (full tests, package, test the installer, publish). Agree the version number first | **Raven's OK required.** Raven's PC cooks, unless question 4 says otherwise |
| 8 | Before that cook, consider the shader cache steps in PERFORMANCE. They need play time and a second cook | Raven decides |
| 9 | Play the evening again if step 6 or 7 changed much | Raven |
| Later | Dedicated server, Shipping build, HLODs, the Quality Plan bars | Raven decides when |

## Shipping or Development

**What the script does.** `Tools\package.ps1` defaults to Development. Its own note says why: "dev console stays on for playtests". Shipping is available with `-Config Shipping`.

**What the docs and release process assume.** They conflict.
- Development: CLAUDE.md says so. The README and HANDOFF release steps call `package.ps1` with no config, so every release goes out as Development. Every release note since 0.5.0 has a "Console (playtest builds)" section.
- Shipping: only `Docs/plan/M7_server.md`, the original plan, says "client (Win64 Shipping)".
- No doc records a Shipping build ever being made or tested.

**What Shipping would change.**

| | Development (today) | Shipping |
|---|---|---|
| Console | Open with the key left of 1 | Gone |
| Cheats | Anyone in the game can spawn items and creatures on the host's world. "Play with people you trust" | Blocked. The code has a list of allowed player actions (`WRPlayerController.cpp`, line 864) |
| Tame whistle | Works by console | Unreachable: it has no screen |
| Frame rate | Runs an extra memory check each garbage-collection pass (PERFORMANCE) | Without that cost. No measured figure in the docs |
| Testing | Bot tests run on the packaged game before release (0.6.3, 0.7.0, 0.7.1) | Test setup commands (give items, teleport, start a raid) are blocked and some are compiled out, so those checks wouldn't run as they do today. Bugs get harder to look into |
| Paperwork | README.txt and release notes describe the console | `package.ps1` writes the console section either way, so it would be wrong until edited |
| Update size | About 235 MB for a code change | The size of the first update after switching: not stated in the docs |

**Recommendation: stay on Development for this round.** The goal is a playtest among friends, and every release so far has been Development. A first-ever Shipping build would add untested risk on the night that matters. Shipping becomes the right choice before strangers play, and that's a separate decision for Raven after the evening has been passed. `M7_server.md` is left as it is for Raven to update or not.

## Blockers on this desktop PC

None of these matter for the recommended path (steps 1 to 6), since playing needs only the installer. They matter only if this PC is going to cook.

| Blocker | Facts | Way through |
|---|---|---|
| Six Fab packs missing | Not in git; `Content\` has none. About 4.8 GB in total (README). Without them the terrain, sea and some creatures lose their look, and saving an asset breaks its link to the pack for everyone | Aaron adds each from Fab to this project (README lists them), then moves out the four Barghest files. Copying from Raven only if the pack's licence allows |
| Art not downloaded | No LFS files here. About 755 MB in the current tree, on Raven's account, and every clone and pull counts against it. The allowance is not stated in the docs | Pull only once, and only if this PC will cook. Tell Raven first |
| Disk | About 38 GB free on C:. `check_setup.ps1` wants about 60 GB; the README says about 100 GB for a new PC. D: has about 86 GB, T: about 129 GB | Free up C:, or move the project off C: and send builds elsewhere with `package.ps1 -Out` |
| Unreal Engine | Not at `C:\Program Files\Epic Games\UE_5.8`, the path about 60 scripts in `Tools` expect, including `check_setup.ps1`, `package.ps1` and `make_installer.ps1`. There's a 5.8.3 on D:\UnrealEngine\UE_5.8; Raven builds with 5.8.4 | A folder link at the default path, so Raven's scripts stay untouched. Update to 5.8.4 if anything odd happens (`check_setup.ps1` note) |
| Visual Studio | `check_setup.ps1` asks Microsoft's locator tool for "Game development with C++". This PC has only Build Tools 2022 and 18, and neither reports it | Install what the README lists. Whether Build Tools alone can build the game: not stated in the docs |
| Dedicated server | Needs Unreal built from source, about 200 GB. That exists only on Raven's PC | Raven's PC only, if ever |

**Rules that apply throughout (CLAUDE.md).** Never publish a release without Raven's OK. Ask Raven before anything big. Open inconsistencies in DESIGN_REVIEW are not to be silently resolved: use the listed default and leave the item open for Raven. If `check_setup.ps1` fails, stop: no opening or saving assets, no builds, no tests with a pack missing.

## Questions for Aaron

1. **Is "final" for this round the build friends play for that evening, rather than a public release?** Recommended: yes.
2. **Does the evening count if the host's PC is the server, with the dedicated server still on hold?** Recommended: yes. It's what players have today, and Raven's on-hold call stands.
3. **Can v0.7.1 be the build for the evening, with no new cook?** Recommended: yes. This is the biggest decision here.
4. **If a new build is needed, whose PC makes it?** Recommended: Raven's. It has the packs, the art and the engine, and every release so far came from it. Setting up your desktop can wait.
5. **How many friends, and must the 100-bot test and 8-hour run come first?** Recommended: a small group, and skip both for this round. Those targets were written for a dedicated server.
6. **Are HLODs still wanted for First Playable?** Recommended: no, unless players complain about trees and rocks popping in. 0.7.0 solved far terrain without them.
7. **Raids are in the game, but DESIGN_REVIEW #31 says Full release. Should it be updated?** Recommended: yes, mark #31 as changed by Raven's 0.7.0 plan. It stays open until you decide.
8. **Development or Shipping for this round?** Recommended: Development, for the reasons above.
9. **Should skills and perks with nothing behind them be hidden?** Recommended: not before the evening, since hiding them needs a new build. Decide from what players notice.
