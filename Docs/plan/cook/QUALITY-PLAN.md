# Wildreach — Quality Plan

Raven's game (`RavenArcher17/Survival-Game`, UE 5.8 C++, v0.7.1). We help; Raven owns it.
Status 2026-11-10: **planning only, brought up to v0.7.1.** Aaron now has write access, so the wait for the collaborator invite is over. The baseline is still the review emailed 11-01 (Trails lessons applied to Wildreach), written against v0.5.0.
Everything marked done below is Raven's own work from the changelog, not work from this plan. Nothing in this plan has been started. Read `FINAL-PROPOSAL.md` (same folder) before any cook.

## The approach
1. **Set the bars.** Write one quality bar for each category below. Each bar names a reference that already meets it, so the bar is something you can see, not an adjective.
2. **Inventory.** List every system, asset, animation and sound in the game from the data files (`creatures.json`, `pieces.json`, `items.json`, `sounds.json`, `recipes.json`, `tuning.json`, and since 0.6.2 `spawn_regions.json`) plus Source/. Each one gets a row.
3. **Grade.** Mark each row *meets bar*, *close* or *below*, with one line on why.
4. **Raise.** Fix rows by category in the order below. Prove each kind of fix on **one** item, then roll it out to the rest (prove before scaling).
5. **Ship.** Release when every row meets its bar and Raven says so (CLAUDE.md: never publish a release without Raven's OK). Aaron and Raven judge that by playing the packaged build.

Wildreach is far smaller than Trails, so steps 1 to 3 fit in one focused session and step 4 in a few.

## Categories and draft bars
Raven signs off on these at the start of the session. They are drafts until then. The docs record no sign-off yet.

| # | Category | Bar (draft) | Reference to match | Done since 0.5.0 | Still open at 0.7.1 |
|---|---|---|---|---|---|
| 1 | **Movement / feel** | Fast and fun on any terrain. Never slides, sticks or falls without reason. Respawn is instant and readable. | — (to be set in the feel pass) | 0.6.0: death is now a map where you pick where to wake, with a 10 s wait and a Wake button (Raven's request). | Whether walk 450 / sprint 750, walkable angle or step height changed: not stated in the docs. No person has been recorded testing them on slopes. The "instant" respawn in the bar no longer matches what Raven asked for in 0.6.0, so Raven should reword it. |
| 2 | **Camera / body** | First person shows arms, torso and legs. The camera rides the head bone. Third person is framed and smooth. | — | 0.6.1: a crosshair, also in first person. | The last word in the docs is "First person hides the body" (changelog, 10-24). Any later change, and the camera offset: not stated in the docs. |
| 3 | **Animation** | No gliding and no loop snap-back. Gait speed matches ground speed. Real clips, with procedural only refining them. | player locomotion (`WRAnimInstance.cpp`) | 0.6.0: new creature clips with feet planted while bearing weight, gaits picked by size and speed, feet and body following slopes, packs out of step. | The new clips are still generated (`anim_creature.py`, listed as procedural in `CREDITS.md`), so the "real clips" line needs Raven's view. The foot system left out the pack creatures and Mixamo bosses; of those, Skaldrith and Scorchwing were added later (0.6.0). A new measure of foot slide: not stated in the docs. |
| 4 | **Creatures (look)** | Readable silhouette, believable anatomy, finished materials. No placeholder shapes. | the 0.5.0 lava flow (surface quality) | 0.6.0: Raven reviewed every creature from front and side. Several rebuilt from new concepts (Cinderfang, Rimeclaw, Basaltback, Ignessa, Vulkarn, Grimjaw), others fixed to their concepts, sizes reset against the survivor, bosses' holes closed. | A final sign-off on the whole roster: not stated in the docs. Scorchwing was kept as it is by Raven's choice. Floeback stays deferred to Full release (design review #14). |
| 5 | **Environment / surfaces** | Every surface holds up close and from a distance, with no visible pop-in. | the 0.5.0 lava flow; the 0.7.0 ice | 0.7.0: flat 6 m ice replaced by a glassy sheet on its own 2 m grid. 0.7.0: a friend who joins now loads the whole island (`streamingRangeScale` x24), as the host always did. | Volcanic shrubs stay green: the ash coat hung the GPU, and Raven said to leave them. Trees and rocks at distance: the docs disagree (CLAUDE.md says about 250 m and "next: HLODs"; 0.4.0 says 2.5 times further; 0.7.0 says HLODs aren't needed for far terrain). HLODs are not built. The ridge's sawtooth shadow was last mentioned on 10-24; whether it remains is not stated in the docs. |
| 6 | **Items / tools / build pieces** | Real modelled objects whose detail matches their collision. | — | 0.6.1: build ghosts show real shapes (stations, Hearth, totem, chest, planks). | Held tools are still engine shapes (CLAUDE.md, `WRHeldTool.cpp`). Build detail still sits over the unchanged collision blocks (CLAUDE.md, `WRBuildLook`). |
| 7 | **Sound** | Every action has a sound. Creature voices are unique, made by fusing real animal calls. Default volume is gentle. | — | No sound work recorded since 0.5.0. | `CREDITS.md` still lists the shared voice classes (`beast_`, `canine_`, `bug_`). Default volume: not stated in the docs. |
| 8 | **Verbs / systems** | Each verb is logical, fun, significant and finished. Anything unfinished is hidden. | — | 0.6.0: roaming wildlife, worlds, world settings, beds. 0.6.3 (shipped in 0.7.0): Hearth repair, chest names, waypoints. 0.7.0: creature raids and timed crafting with station queues, so those two settings now do something. 0.7.1: clans and door codes moved from the console to real screens. | Bounties still have nothing behind them (design review #32) and stay off the screens (0.6.0). Tame whistle has no screen (HANDOFF). Perks "not active yet" and fishing, engineering and swimming having no XP source were last noted on 10-23; any change is not stated in the docs. "[not built yet]" nodes: CLAUDE.md still says some show this, but the changelog (10-24) says every tree node works and CLAUDE.md's notes are marked as written up to 0.4/0.5. Check in game. |
| 9 | **UI / menus** | Every screen works by mouse and later by pad. No dead states. Mouse sensitivity and invert settings exist. | Raven's stone-and-leather Slate UI | 0.6.1: crosshair with an off switch, lock-on corners. 0.6.3: build bar pages, Claim tab, fits 16:10. 0.7.0: crafting fits 4:3 and 5:4. 0.7.1: Clan tab, door keypad, tested by real clicks. | Sensitivity and invert: 0.3.0 listed mouse settings as not done, and no later doc mentions them. Controller: none recorded. |
| 10 | **Performance** | Smooth on a modest PC at Medium. No first-launch hitching. The loading screen holds until the world is ready. | `Docs/PERFORMANCE.md` (4070 Ti only) | 0.7.0: measured again with the whole island loaded for friends (Epic 8.38 ms against 8.33, about 0.3 GB more memory). The new ice costs nothing measurable. | Still measured only on the 4070 Ti. No bundled shader cache: `PERFORMANCE.md` recommends one once at release time. An occasional GPU hang under heavy load (`NodeAndClusterCull`) is "not looked into yet". |
| 11 | **Multiplayer / onboarding** | A friend installs it, joins and plays without help. | the self-updating installer | 0.6.1: updates fetch only what changed (about 235 MB, not 1.7 GB). 0.6.2: four connections at once. 0.7.1: no console needed for clans or door codes. | Unsigned installer warning, UDP 7777 port-forward and Development build all remain. The gate playtest hasn't been run, nothing is tested past a few players, and there is no packaged dedicated server (on hold, Raven's call). |

## No longer applies
- "Waiting on the collaborator invite": Aaron has write access.
- "Unused raid settings" (row 8): raids are built (0.7.0). The crafting-speed setting works too.
- "Flat 6 m ice sheet" (row 5): replaced in 0.7.0.
- "Creature feet slide at about half their speed" (row 3): that note was about the old rigs, which 0.6.0 replaced. It needs a fresh look rather than a fresh fix.
- "No PSO precache" (row 10): `PERFORMANCE.md` says UE 5.8 already precaches and holds a mesh back until ready. The gap is the bundled cache only.
- "About 770 MB of 1 GB" of LFS: CLAUDE.md now says about 755 MB in the current tree. The allowance itself is not stated in the docs.

## Tools we add early
These make grading fast and feedback easy. The docs mention neither, so treat both as not built:
- **Zoo map (`L_Zoo`).** Fills itself from the data JSONs and opens from the main menu. Walk through it as a player and see and hear every asset. Each row is graded here.
- **Note key.** Saves a screenshot, position, version, host or client, and a typed note to `Saved/Reports`. Notes from clients are forwarded to the host. This is how Aaron and Raven report grades and issues.

Since 0.5.0 some of the grading can lean on what Raven already built: `Tools\creatureportraits.ps1` and `creaturevideo.ps1` on `L_CreatureStage` for creatures (0.6.0), `uishots.ps1` for screens and `worldshots.ps1` for the world.

## Order of work (first focused session)
This waits on the desktop setup and on Raven's answers in `FINAL-PROPOSAL.md`.
1. Sign off on the bars with Raven (≈15 min).
2. Build the inventory and grade it from code, data and screenshots, then mark rows that need a person to look.
3. Build the zoo and note key.
4. Make the first proof-of-one fixes, all items marked *do first* in the review:
   - feel pass and movement settings
   - one creature with real gait clips (Raven's 0.6.0 creature pass came first, so grade before deciding this is still needed)
   - the full-body first-person camera

All the rows marked *below* follow, in category order, each proven on one item first.

## Ground rules
- It's Raven's repo: pull before starting and again before pushing, commit to `master`, name your own files (never `git add -A`), never force-push, and keep to its conventions (`CLAUDE.md`, the tests, the changelog, `Docs/HANDOFF.md`).
- Ask Raven before anything big, and never publish a release without Raven's OK. Open items in `Docs/DESIGN_REVIEW.md` stay open for Raven: use the listed default, don't quietly pick an answer.
- Scope is First Playable only: Fire and Frost. Nothing from the other 10 biomes or paths, or the Full release and Dream tiers, unless Raven asks.
- Heavy UE builds run on the desktop, not the laptop. The desktop isn't set up for them yet (see `FINAL-PROPOSAL.md`). Don't open or save any asset with a Fab pack missing: it loses its link to the pack for everyone.
- Maps and `.uasset` files can't be merged, so say before changing one.
- No AI-slop art. Assets meet the bar or stay marked *below*.
- Watch the LFS quota on Raven's account (about 755 MB in the current tree; every clone and pull counts). Terrain experiments stay on a test map.
