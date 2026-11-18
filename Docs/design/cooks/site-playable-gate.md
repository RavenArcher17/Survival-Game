# fable-site-playable-gate: no game goes live unless it plays

Lane: fable-site-playable-gate (Fable, desktop PC). Repo: freedom35-studios. Date: 2026-11-15.

Aaron 11-15: "every time I want to try the game after a session of building, it is always unplayable, especially if I run out of usage, what I'm building is never left in a functional state" / "ive told you this hundreds of times before you have never been successful with doing it" / "All games on the site need to meet this expectation".

## REQUIREMENT LEDGER

Every row's "done + where" is in the PROOF table at the foot of this file (same row numbers), written when the lane landed.

| # | Requirement | Done + where |
|---|---|---|
| R1 | The expectation, browser: Play -> ONE loading screen -> main menu -> New Game / Continue reaches real gameplay | PROOF R1 |
| R2 | The expectation, PC download: launch -> loading -> menu -> play | PROOF R2 |
| R3 | Every game on the site is held to it | PROOF R3 |
| R4 | A build not PROVEN to do that never goes live and never replaces the live build | PROOF R4 |
| R5 | If anything fails or a lane dies mid-way (usage limit), the last proven build stays live | PROOF R5 |
| R6 | Mechanical, not a rule in a brief | PROOF R6 |
| R7 | Browser boot check: generic, headless, serves the built game | PROOF R7 |
| R8 | ...opens it fresh AND with a real save | PROOF R8 |
| R9 | ...fails on any uncaught error | PROOF R9 |
| R10 | ...fails on a never-ending loading screen | PROOF R10 |
| R11 | ...fails on a blank/black canvas | PROOF R11 |
| R12 | ...confirms the menu, then New Game reaches play | PROOF R12 |
| R13 | Per-game "reached menu" / "reached play" signal: what the game already exposes, or a tiny standard one through the Freedom35 SDK (ready('menu') / ready('play')) | PROOF R13 |
| R14 | Start from what exists (waveraider-night bootcheck logic = .boot-ok gate in publish-waveraider.mjs; waveraider tools/playtest/driver.mjs) | PROOF R14 |
| R15 | PC boot check: launch the packaged exe with a log, confirm menu and play from the log (same SDK-style markers), then close it | PROOF R15 |
| R16 | Server refuses: admin publish (functions/api/admin/games) | PROOF R16 |
| R17 | Server refuses: builds / build-promote routes (promote, rollback, ticket publish, MCP) | PROOF R17 |
| R18 | Server refuses: PC release paths (download publish, rollback) | PROOF R18 |
| R19 | Every tools/publish-*.mjs goes through the gate | PROOF R19 |
| R20 | Proof recorded per build id by the boot check through an admin endpoint; the proof names the build hash | PROOF R20 |
| R21 | No proof -> 409 with the exact command to run | PROOF R21 |
| R22 | publish-waveraider.mjs local gate (.boot-ok, 3323cef/dd13d8a) folds into this | PROOF R22 |
| R23 | Creators' uploads (studio-builds): the same gate | PROOF R23 |
| R24 | A creator build we can't check automatically stays in preview/beta until it passes | PROOF R24 |
| R25 | Never break a live game: grandfather the current live builds (proven-by-being-live) | PROOF R25 |
| R26 | Publish paths updated: Waveraider, lab, soundtrack, Card Forge, Music Freedom, Tavern's Mettle, Tiny Coliseum, Fleshcrafter, Black Monolith (PC), Wildreach (PC), creator pipeline | PROOF R26 |
| R27 | DEPLOY.md: five lines | PROOF R27 |
| R28 | Lane gate: site builds/deploys clean with its own deploy tool | PROOF R28 |
| R29 | Lane gate: every live game still opens (new boot check against each live game once, read-only) | PROOF R29 |
| R30 | No game code touched except tiny ready markers | PROOF R30 |
| R31 | Other lanes' edits to js/gamesave.js, js/gamehost.js, functions/play/[slug].js kept (pull/rebase first) | PROOF R31 |
| R32 | Commit only own paths; push; deploy | PROOF R32 |
| R33 | Questions written to QUESTIONS-FOR-TOMORROW.md with the pick built | PROOF R33 |
| R34 | Email "Freedom35: no game goes live unless it plays" through the notify relay, links only | PROOF R34 |

## RESEARCH: how the best platforms make "it boots" mechanical

1. **Google Play pre-launch report** (Play Console). Every upload to a test track is installed on real devices by a robot crawler that launches the app, taps around for minutes, and reports crashes, ANRs and screenshots before the build can be promoted to production. What makes it work: it runs on the *uploaded artifact* (not the developer's tree), automatically, and its result hangs on the artifact, so "promote" has something to look at. Used here: the proof is recorded against the build's hash, by a robot, and promote reads it.
2. **Console certification (Xbox XR / PlayStation TRC / Nintendo Lotcheck)**. A title fails cert if it crashes or hangs on boot, shows a black screen longer than a few seconds without a loading indicator, or cannot reach an interactive state from a fresh save *and* from an existing save. What makes it work: two launches (no save, real save) and a fixed definition of "reached the interactive state". Used here: two runs per build (fresh, real save), a fixed menu -> play definition, and a hard time limit on loading.
3. **Steam (SteamPipe)**. Uploading a build never changes what players get; a build becomes live only when someone "sets build live" on the default branch, and the previous build stays one click away. Betas live on other branches. What makes it work: upload and go-live are separate acts, and live is a pointer to an immutable build. Used here: hosted single-file games stop being overwritten in place; each upload is its own immutable key and going live is a pointer move the server does.
4. **itch.io butler channels + Kubernetes readiness probes / blue-green deploys**. A rollout only shifts traffic to a new version after its readiness probe answers; a version that never becomes ready is never served and the old one keeps serving, with no human in the loop. What makes it work: the default on failure is "nothing changes". Used here: every failure path (check fails, tool dies, lane runs out of usage between upload and go-live) leaves the live pointer where it was.
5. **Poki / CrazyGames / GameDistribution web SDKs**. Games call `gameLoadingFinished()` when the menu is up and `gameplayStart()` when play begins; the portal's QA and its analytics both key on those two calls. What makes it work: two tiny, engine-independent calls. Our SDK already has the same two (F35.ready, F35.gameplayStart, sdk/f35.js Tier 1). Used here: `F35.ready('menu')` and `F35.ready('play')` are the standard markers; the check also takes per-game expressions for games that expose their state another way.
6. **Godot / Unreal automated smoke tests in CI** (our own developers/workflows/godot-windows.yml "Check it boots": run the exported exe with a log, grep for "Godot Engine v" and for SCRIPT ERROR; Unreal Gauntlet: launch packaged build with `-abslog`, wait for a log line, kill). What makes it work: the log is the interface. Used here: PC check launches the packaged exe with a log file, waits for `F35READY menu` then `F35READY play` (or per-game log lines such as Unreal's `LoadMap` lines), fails on engine fatal lines, then kills the process tree.

What exists in this repo to start from: `tools/operator/qa.mjs` (temp QA account, headless Edge through /play/<slug>, colour-count blank test, error capture), `cli/godot-bootcheck.mjs` (local static server + headless boot), `C:/Users/acars/waveraider/tools/playtest/driver.mjs` (CDP driver, pointer-lock emulation, d3d11-vs-sleeping-display note), `tools/publish-waveraider.mjs` (.boot-ok = sha256 of index.html).

## PLAN (written before any build step)

### The three ways a game is live today, and the single place each one changes
- **Hosted single file** (waveraider, waveraider-lab, card-forge, music-freedom, tiny-coliseum, corelode; soundtrack to come): bytes in KV `game:<releases.asset_key>`. Today `tools/pack-game.mjs` overwrites the live key directly from a laptop: this is the hole. Every tools/publish-*.mjs for these games calls pack-game.mjs, so it is the one choke point.
- **Pipeline build** (fleshcrafter, taverns-mettle, all creator web games): `channels.live -> builds.id`. One mover: `moveChannel` in functions/_lib/pipeline.js (promote, rollback, admin publish, ticket publish, MCP).
- **PC installer** (black-monolith, youre-the-cupcake, crownfall, ...; Wildreach to come): `releases` row source 'file', KV manifest `dl:<rel>`. One mover: `publishDownload` / `rollbackDownload` in functions/_lib/downloads.js.

### Proof (D1 table `boot_proofs`, migration 2026-11-15-boot-proofs.sql)
One row per (game, kind, subject): kind `hosted` (subject = sha256 of the html), `build` (subject = build id, build_hash = sha256 of its sorted path/sha list), `download` (subject = sha256 of the installer), `external` (subject = the play URL). level: `play` (menu and play both confirmed by explicit signals, fresh and with a save), `boot` (generic: no uncaught error, loading ended, draws, responds to input), `live` (grandfathered: proven by being live on 2026-11-15). The server derives the level from the report itself, never from a field the client sets.
- Needed level: first-party games (ours) need `play`. Third-party games need `boot` or better (their menu is unknown to us unless they call the SDK markers). `live` satisfies both.
- Recorded only through `POST /api/admin/boot-proof` (admin or operator token) and, for a creator's own CI, `POST /api/dev/games/<slug>/boot-proof` (their token, for their build; the hash must match what the server holds).
- `functions/_lib/bootproof.js`: `bootRefusal()` is pure (rule-checkable), `requireProof()`, `recordProof()`, `hashKv()` (streamed sha256 of a KV value with crypto.DigestStream), `buildHash()`, `grandfather()`.

### The gates (each answers 409 `boot_proof_required` with `command`)
1. `moveChannel(..., channel 'live')`: target build needs a proof. Covers promote, rollback, admin publish, ticket publish, MCP.
2. `POST /api/admin/hosted-live` (new, admin token): the only way a hosted game's release row is pointed at new bytes. It hashes the staged KV value itself, needs a proof for that hash, then repoints `releases.asset_key`, remembers the previous key (rollback) and deletes the one before it. pack-game.mjs now writes `game:<key>-<sha12>` (never the live key) and calls this. A dead lane leaves a staged key nobody is served.
3. `publishDownload`: sha256 required, proof for it required. `rollbackDownload`: the previous installer must carry a proof (grandfathered ones do).
4. Ticket publish of a hosted stage key: hash it, need a proof.
5. Admin `publish` (status -> published): whatever the Play/Download button would serve must be proven (hosted pointer set by the gate, live build proven, installer proven, external play link proven).
6. Grandfather: `POST /api/admin/boot-proof {action:"grandfather"}` records level `live` for every current live build, its previous build, every hosted key, every installer and its previous, every external play link. Run once, right after the migration and before the gated code is deployed, so nothing goes dark.

### Browser boot check (`cli/bootcheck.mjs` core, served with the CLI; `tools/bootcheck.mjs` front end)
- No dependencies: headless Edge/Chrome driven over `--remote-debugging-pipe` (works on any Node, no port to collide with another lane's browser). One browser, closed in `finally`.
- Instrument every frame before its scripts: window error + unhandledrejection capture, rAF frame counter, pointer-lock emulation (from driver.mjs), optional storage seed. CDP `Runtime.exceptionThrown` is the uncaught-error source of truth.
- GPU: d3d11 first; if no animation frame arrives in 4 s (sleeping display, driver.mjs note) relaunch with SwiftShader.
- Run = navigate, then poll every 500 ms:
  - menu: explicit signal (`window.__F35_READY__.menu`, or the profile's `menu` expression) else generic (document complete, network quiet 2 s, screen not blank: more than 3 distinct colours, no visible "Loading" text, screen settled).
  - never-ending loading: no menu inside `menuSeconds` (default 90) fails, with the last screenshot kept.
  - blank: colour count of a screenshot, decoded in a helper tab (qa.mjs method).
  - play: run the profile's `start` steps (click text / selector / point, keys) or the generic ones (click centre, Enter, Space); explicit signal (`__F35_READY__.play` or profile `play`) inside `playSeconds` (default 60); generic fallback = the screen changed, frames still tick, no new error.
- Two runs: `fresh` (empty storage) and `save` (a real save: `--save file.json`, or the account save pulled read-only from D1 for the studio's own account, or, when none exists, the storage the fresh run itself wrote). Save run uses the profile's `continue` steps.
- Modes: local build folder / single file (static server; unknown paths fall back to the site's own /audio /images /js /sdk so first-party games find their samples), `--url`, `--slug <s> [--channel preview|beta|live]` through the real /play/<slug> as a temporary QA account (qa.mjs method, cleaned up after), `--all-live` for the lane gate.
- Output: verdict, level, per-run menu/play times, errors, screenshots folder, and `--record` posts the proof (kind + subject + hash). Local builds also get `<dir>/.boot-ok` = sha256 (the old Waveraider gate's file, so nothing that reads it breaks).
- Profiles: `tools/bootcheck/profiles.mjs`, one entry per first-party game: menu / play expressions over what the game already shows (DOM), start / continue steps. Creator games: `boot` key in f35.json, or the SDK markers, or generic.

### SDK markers (R13)
`F35.ready('menu')` (same as today's `F35.ready()`), `F35.ready('play')` (also what `F35.gameplayStart()` sets). Each stamps `window.__F35_READY__ = { menu: ms, play: ms }`, fires `f35:ready`, and prints `F35READY menu|play` to the console once. A game that does not load the SDK may set the same global itself (one line). PC games print the same `F35READY menu` / `F35READY play` line to their log.

### PC boot check (`tools/bootcheck-pc.mjs`; the same core in `f35 installer` for creators' CI)
- Input: the packaged exe (or folder with f35-update.json naming it), optional installer. Engine detected from the folder (Unreal: `Engine/` dir or `*-Win64-Shipping.exe`; Godot: `.pck` or embedded). Launch with a log: Unreal `-abslog=<tmp> -unattended -nosplash -windowed -ResX=1280 -ResY=720`, Godot `--log-file <tmp>`; stdout/stderr captured too. CI (no GPU): `-nullrhi` / `--headless --audio-driver Dummy`.
- Menu = `F35READY menu` or the profile's log regex (Unreal already logs `LoadMap`/`Bringing World ... up for play`; Godot prints nothing by default, so generic menu = engine banner + N quiet seconds with no fatal line). Play = `F35READY play` or profile regex, reached by the game itself when launched with `--f35-boot` / `-F35Boot` (the marker contract), or by profile key steps sent to its window.
- Fatal: `SCRIPT ERROR|Parse Error|Failed to load script` (Godot), `Fatal error|Assertion failed|Unhandled Exception|=== Critical error` (Unreal), or the process exiting before the menu.
- Close: `taskkill /PID <pid> /T /F`. Proof: kind `download`, subject = sha256 of the installer (required to publish); exe hash kept in the report.
- Black Monolith and Wildreach are not packaged on this PC: path and gate built, live installers grandfathered, nothing packaged here.

### Publish paths (R19, R26)
- pack-game.mjs: stage -> boot check must already be recorded for this hash -> `hosted-live`. Refusal prints the bootcheck command. Used unchanged by publish-waveraider (+ --soundtrack), -lab, -card-forge, -music-freedom, -tiny-coliseum.
- Each of those tools runs `tools/bootcheck.mjs <built> --game <slug> --record` before packing; a failing check exits before anything is uploaded.
- publish-fleshcrafter / publish-taverns-mettle: `f35 push <stage> <slug>:live --boot-profile <json>`; the CLI pushes, boot-checks the folder it pushed, records the proof, then promotes. On refusal the build sits un-promoted and live is untouched.
- cli/f35.mjs: `push ...:live` and `promote ...:live` run the check automatically when the server answers boot_proof_required (creators' CI fetches the CLI fresh each run, so their workflows need no edit); `installer` boot-checks the packaged folder and leaves `<setup>.boot.json`; `download` sends it. No Chrome / check fails -> stays in preview, live untouched, exit 1 with the reason.
- Soundtrack is an album page, not a game: generic level is its ceiling, so its profile declares `menu` = track list rendered and `play` = the same page state (no New Game exists).

### Edge cases decided
- Rollback: allowed to any build that carries a proof; everything live today and its predecessor is grandfathered.
- Same bytes re-published: proof is by hash, so no second check needed.
- QA account on the live site: created and deleted by the tool; its plays are removed (qa.mjs cleanup list).
- A proof can be re-recorded (upsert); a failing later run does not delete an earlier pass for the same hash (same bytes, flaky browser).
- Stale checkouts running the old pack-game.mjs write `game:<key>`: after grandfathering repoints each release at its content key, that legacy key is no longer served, so an old tool cannot change live.
- Creator link edits (external play URL on a published game) are outside the build pipeline; gated at admin publish only. Logged as a question.

### Build steps
1. Migration + `_lib/bootproof.js` + rule check tool.
2. Admin endpoints: boot-proof (record, list, grandfather), hosted-live.
3. Gates: pipeline.js moveChannel, downloads.js, ticket publish, admin games publish, dev boot-proof route, mcptools message.
4. SDK markers (sdk/f35.js, f35.d.ts, js/sdkhost.js, developers/sdk.md).
5. cli/bootcheck.mjs, tools/bootcheck.mjs, profiles, tools/bootcheck-pc.mjs.
6. pack-game.mjs + every publish-*.mjs + cli/f35.mjs.
7. Apply migration, grandfather, deploy (tools/deploy.mjs, deploy-play.mjs untouched unless shared files change), verify 409s on a throwaway check.
8. Run the boot check against every live game, read-only. Fix anything the gate itself broke.
9. DEPLOY.md five lines, ledger proof, questions, email.

## WHAT CHANGED FROM THE PLAN WHILE BUILDING (each for a reason found by running it)

- "Network quiet" is "at most two requests begun in the last three seconds", not "none in flight": every game on the real site holds a heartbeat open, so the first run called every live game stuck.
- The second (save) run re-opens the game's address through a blank page instead of reloading: a tool left on its editor (Card Forge, Music Freedom) reloaded into the editor and never showed its menu.
- "The gate put this key live" is a D1 row (boot_proofs kind `hosted-key`), not only the KV pointer: KV can be a minute stale at another edge, which would have refused a game for a minute after every publish.
- Play-route enforcement turns on by a flag the grandfather run sets at its end, so there was no moment between deploy and grandfathering when a hosted game could be refused.
- Fleshcrafter needed no code: it already exposes `window.__game` (mode, run). No game's code was touched in this lane.
- A page that leaves for Google's sign-in, and a game's own "Sign in with Google" door, are their own results (not checkable by the QA account / the limit of an outside check), found on the Soul games, World Builder and Zengania.
- A developer-facing refusal names the CLI's command; Freedom35's own names tools/bootcheck.mjs.
- tools/deploy.mjs refuses a checkout behind its upstream, because two machines deploy this site and a stale deploy would have silently removed the gate.

## PROOF (the ledger, row by row; repo freedom35-studios, commits d6d6cea bc5024e a56f338 b1b4c6d 86a5ead 0ca55d5, deployed 2026-11-15)

| # | Done | Where |
|---|---|---|
| R1 | done | cli/bootcheck.mjs:440 runOnce (open, menu, start, play, first seconds of play); run live against every game, R29 |
| R2 | done | cli/bootcheck.mjs:748 pcBootCheck; tools/bootcheck-pc.mjs; run on a stand-in process (pass, and SCRIPT ERROR refused). No real PC game was launched: none is packaged on the desktop |
| R3 | done | gate is in the three movers every game goes through (R16 to R18) plus functions/play/[slug].js:175; `node tools/bootcheck.mjs --status` = 31 of 31 proven |
| R4 | done | functions/_lib/bootproof.js:59 bootRefusal; live test 11-15: unproven hosted file, unproven Fleshcrafter build, unproven Black Monolith installer each answered 409 and the live row was unchanged |
| R5 | done | tools/pack-game.mjs:127 stages under `game:<key>-<sha12>` (never the live key), :135 the server switches; functions/_lib/pipeline.js:252 refuses before any write; cli/f35.mjs:452 a failed build is left on preview |
| R6 | done | server code, not a brief: functions/_lib/bootproof.js, the routes below, functions/play/[slug].js:175 |
| R7 | done | cli/bootcheck.mjs:572 bootCheck (headless Edge/Chrome over --remote-debugging-pipe), :658 serveBuild; tools/bootcheck.mjs:290 |
| R8 | done | cli/bootcheck.mjs:594 (runs fresh then save); tools/bootcheck.mjs:91 accountSave reads the studio account's save from D1, read only. Run 11-15 on waveraider/dist: "the studio account's real save (9 keys, read only)", both runs play |
| R9 | done | cli/bootcheck.mjs:284 Runtime.exceptionThrown (errors and unhandled rejections, every frame); smoke: a thrown ReferenceError refused |
| R10 | done | cli/bootcheck.mjs:480 "no menu after N s"; smoke: a loader that never ends refused at the limit |
| R11 | done | cli/bootcheck.mjs:504 (3 or fewer distinct colours at the menu, and again in play) |
| R12 | done | cli/bootcheck.mjs:545 (play signal after the start steps), and the 5 s after it (errors, blank, frames stopped) |
| R13 | done | sdk/f35.js:136 mark, :164 F35.ready("menu" / "play"); cli/bootcheck.mjs:290 reads `F35READY`; tools/bootcheck/profiles.mjs for what games already expose (Waveraider `__wr.phase`, Fleshcrafter `__game.mode`, DOM for the rest) |
| R14 | done | pointer-lock emulation and the sleeping-display fallback from driver.mjs (cli/bootcheck.mjs:186, :603); QA account and colour count from tools/operator/qa.mjs; .boot-ok still written (tools/bootcheck.mjs:307) |
| R15 | done | cli/bootcheck.mjs:748 (log file per engine, `F35READY menu` / `F35READY play`, :688 fatal lines, :724 taskkill /T) |
| R16 | done | functions/api/admin/games.js:86 publishRefusal; functions/_lib/bootproof.js:422 |
| R17 | done | functions/_lib/pipeline.js:252 (promote, rollback, admin publish, MCP), functions/api/admin/tickets/[id]/publish.js:21 (staged hosted files) |
| R18 | done | functions/_lib/downloads.js:73 publish, :147 rollback |
| R19 | done | tools/pack-game.mjs:117 check, :127 stage, :135 switch; called by publish-card-forge:50, -music-freedom:58, -waveraider:101 (and --soundtrack), -waveraider-lab:83, -tiny-coliseum:87; publish-fleshcrafter:111 and -taverns-mettle:131 pass their profile to `f35 push ...:live` |
| R20 | done | functions/api/admin/boot-proof.js:83 (admin or operator token); functions/_lib/bootproof.js:183 recordProof stores build_hash and derives the level from the runs; a creator's CI: functions/api/dev/games/[slug]/boot-proof.js:28 |
| R21 | done | functions/_lib/bootproof.js:47 proofCommand, :59 the 409; :80 the developer's wording. Seen live in all three refusals (R4) |
| R22 | done | tools/publish-waveraider.mjs:58 (the local block is gone; the server's gate replaces it for every run, not only --publish) |
| R23 | done | the studio page's promote calls the same route (functions/api/dev/games/[slug]/promote.js:28 -> pipeline.js:252) |
| R24 | done | cli/f35.mjs:452: a build that fails is promoted to preview and the run ends red; preview and beta are not gated (pipeline.js:252 is live only) |
| R25 | done | functions/_lib/bootproof.js:346 grandfather, run 11-15: 30 builds, 6 hosted (each moved to its content key), 17 installers, 1 play link, nothing skipped |
| R26 | done | Waveraider, lab, soundtrack, Card Forge, Music Freedom, Tiny Coliseum: pack-game.mjs. Fleshcrafter, Tavern's Mettle, creators: cli/f35.mjs:452. Black Monolith, Wildreach, every installer: cli/f35.mjs:815 (installer launches the game), :635 (download sends the proof), downloads.js:73. NOT DONE inside it: Tavern's Mettle needs one line (`F35.ready("play")`) before its next build can go live; its source is not on the desktop |
| R27 | done | DEPLOY.md:231, five numbered lines |
| R28 | done | `node tools/deploy.mjs` three times and `node tools/deploy-play.mjs` once on 11-15, each "Deployment complete"; home 200, /play signed out 303, play site without a ticket 403 |
| R29 | done, with three it could not open | `node tools/bootcheck.mjs --all-live` 11-15: of 31 live games, 20 browser games open and reach play (7 at level play: Waveraider, its lab, Card Forge, Music Freedom, Tiny Coliseum, Corelode, Fleshcrafter; 13 at level boot, of which Zengania stops at its own Google sign-in and Soul Tree passed on its second run after the start-label fix); the 8 download-only games' 9 installers are all served (MZ / PK); Soul Craft, Soul Games and World Builder send the QA account to Google's sign-in (their own Firebase), so the browser run cannot open them, and for those the run proved /play answers 200 and the play site serves the build's entry file. Report: %TEMP%/bc/live-final, live-final2, live-final3 on the desktop |
| R30 | done | no game repo was changed (git status clean in waveraider apart from files that were already modified before this lane; Fleshcrafter untouched) |
| R31 | done | functions/play/[slug].js: one import and one block (:175), added on top of 9c57fa5 after `git pull --rebase`; js/gamesave.js and js/gamehost.js not touched |
| R32 | done | six commits, files added by name, pushed (master = origin/master = 0ca55d5), deployed |
| R33 | done | \\192.168.0.150\TrailsRepo\Saved\Handoff\wild-desktop\QUESTIONS-FOR-TOMORROW.md (six questions, each with the pick built, and one note) |
| R34 | done | sent through https://freedom35webdesign.com/api/notify/send after this table was written; the relay's answer is in the lane's final message |
