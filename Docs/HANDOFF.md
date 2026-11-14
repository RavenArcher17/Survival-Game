# Wildreach handoff: working on the game from another PC

For a second developer (and their Claude) joining Raven on Wildreach. Written 2026-11-08, at v0.7.1. Keep it current:
when something here stops being true, fix it in the same commit.

Setting up the PC is in `README.md` ("Working on the game on a new PC"), and `Tools\check_setup.ps1` checks it.
This file is what comes after: where the game stands, how two people share the repo, how a release goes out, and the
traps that already cost a day each.

## Who decides what

- **Raven owns the game.** Scope, names, looks and what goes out are Raven's call. Ask before building anything big,
  and before publishing a release.
- **Scope is the build plan's First playable tier: fire and frost only** (Volcanic + Frozen tundra). Nothing from the
  other ten biomes or paths, or from the Full release / Dream list tiers, unless Raven asks.
- **The design docs are first drafts.** Names, perks and starting numbers are meant to be tuned in playtests, so
  numbers live in data (`Data/*.json`, `Data/tuning.json`), never hard-coded around one value. Where the doc's tabs
  disagree, `Docs/DESIGN_REVIEW.md` lists the open questions and a proposed default: use the default, leave it open
  for Raven.
- **Creature looks:** a remade creature keeps the design doc's fantasy and is realistic only in anatomy and pose; four
  legs unless the real animal it's based on has more.

## The design docs

Raven writes them in Claude Docs. Copies as of 2026-11-08 are in `Docs/design/` (`build-plan.md`, `design-doc.md` and
one file per tab). Raven keeps editing the live ones, so the copies go stale: ask Raven for a fresh export before a
big piece of work. The build plan's "Where it stands" table says what's built.

## Where the game stands (v0.7.1)

Every system in the First playable tier is built and has a bot test: biomes, both paths, levelling, combat, six
bosses, enemies and wildlife regions, taming, survival, crafting (with time and station queues), building (with
claims, raids and door keypads), clans, PvP rules, worlds and world settings, the stone-and-leather UI, sound, the
installer and auto-updater. `CHANGELOG.md` has every version, newest first, with what changed and why.

Since 0.7.1 (unreleased): **F8 reports** (testers' reports land in the studio inbox, in `Saved/Reports` on their PC
and, from a client, in the host's `Saved/Reports`), **full controller support** (`Data/gamepad.json`; menus driven by
one input processor in `WRPadInput.cpp`, so a new screen needs nothing for the pad as long as its clickable pieces
show the hand cursor or are buttons / slots), the **F1 controls page**, and **look sensitivity / invert**. New prompt
rule: show a key with `WRInput::ActionKey(<action>)`, not `WRUI::KeyName(...Key(...))`, so pads see their buttons.
Also since 0.7.1: **the MetaHuman hero** (M1 Ash / F1 Wren from the hero lab, `Content/Wildreach/Characters/Hero`)
with **motion-matched locomotion** (`UWRHeroAnimInstance`), retargeted action clips with combat windows, sockets and
full-body first person on the head bone; the Mixamo survivor is gone. How it's put together is under "Art" below.
Also since 0.7.1: **fishing** (`WRFishing.cpp`, `Data/fish.json`), **swimming and engineering XP**, **66 of 120 perks
working** with the rest hidden by `waitsFor` in `Data/lifeskills.json` (remove it when a system ships), the **tame
whistle** (H / hold X), **modelled held tools** (`WRToolMesh.cpp`, `M_HeldTool`), and build pieces that collide as
they look (furniture, banner, rug; the Hearth, stations and totem fit their models via `FWRBuildLook::FitCollision`).

Open, as of 0.7.1:
- **No packaged dedicated server** (on hold, Raven's call). The host's PC is the server. Unreal built from source sits
  at `C:\Dev\UE5Source` on Raven's PC (`Tools\build_engine.ps1`), needed only for that.
- **The gate playtest** (a group of friends plays a full evening and wants to come back) hasn't been run.
- **Volcanic shrubs stay green**: the ash-grey coat hung the GPU (VSM Nanite), so `ashCoatMin` stays 0.
- **Bounties** stay a setting only (nothing reads it) until the PvP pass. **54 perks** wait for their systems
  (`waitsFor` in `Data/lifeskills.json`: diving, breeding, bees, machine power, turrets, explosives, timed brews...).
- **F8 pictures from the game don't reach the web inbox yet**: the feedback endpoint keeps screenshots (and context over
  4000 characters) for signed-in players only, and the game isn't signed in. The full report and picture are in
  `Saved/Reports` on the tester's PC (and the host's). Text fields (chat, names, the report note) need a keyboard.

## Two people, one repo

The repo is https://github.com/RavenArcher17/Survival-Game (public). Everyone works on `master`.

- **Pull before you start, and again before you push** (`git pull`). Push each change once it builds and its tests
  pass. Don't leave finished work sitting unpushed: the other person builds on master.
- **Commit your own files by name** (`git add <paths>`), never `git add -A` / `git add .`. Raven's PC has a local
  `Config/DefaultEditor.ini` that stays out of git, and the Fab packs must never go in.
- **Maps and `.uasset` files can't be merged.** If both of you change the same one, one person's change is lost. C++,
  `Data/*.json`, scripts and docs merge as usual. Before changing a map (`Content/Maps`, `Content/__ExternalActors__`)
  or an asset someone else may be working on, say so; one person at a time.
- **One release at a time.** Agree the version number with Raven first (check `gh release list --repo
  RavenArcher17/Survival-Game -L 3`), so two people never publish the same number.
- **Art goes through Git LFS** on Raven's GitHub account: every clone and pull counts against its LFS allowance.
  Don't re-clone for fun, and say before adding a lot of new art (a creature is about 11 MB, an island rebuild ~80 MB).

## Building and testing

`README.md` has the build command and every test. The ones you'll use most:

```
$env:UE_SKIP_UBT_SDK_SETUP = '1'                       # always, or Unreal waits on any other build on the PC
Tools\alltests.ps1                                     # build, then every test (~90 min), PASS / FAIL table
Tools\alltests.ps1 -SkipBuild -Only door,clan          # just some
Tools\play.ps1 listen                                  # play it yourself from the editor build
```

- Tests drive the real game with bot commands: `-wrbot -wrbotcmds="cmd|cmd|..."` (`WRClick <button text>`,
  `WRKey <key> [hold <s>]`, `WRUIText`, `WRUIState`, `WRTeleport`, `WRGive`, `WRCmd <server command>`, `waitchat <text>`
  to keep two bots in step; prefixes `@1:` / `net:` / `solo:` pick which world a command runs in). The bot runner and
  `waitchat` are in `Source/Wildreach/WRPlayerController.cpp`, the UI commands (`WRClick`, `WRKey`, `WRUIText`,
  `WRUIState`) in `WRUIDrive.cpp`. To write a test, copy one (`Tools\doortest.ps1` is a short two-player one).
- Two-player tests run a host with no window (`-nullrhi`, it still has the UI) and a rendered friend.
- **Known flakes**: `menus` and other two-window tests sometimes hit a GPU hang (`DXGI_ERROR_DEVICE_HUNG` in Nanite
  shadow culling); `updater`'s dropped-connection step can fail while the PC is busy. Rerun a failure alone
  (`-Only <name>`) before believing it.
- Runtime errors go to the log, not the exit code: check logs for `Error:`.
- Look at visual changes from at least two camera angles; one angle has hidden real defects before.

## Releasing an update

Only with Raven's OK. Players' games update themselves from the newest GitHub release.

1. `git pull`, and make sure the working tree is clean apart from files that stay local.
2. Bump `ProjectVersion` in `Config/DefaultGame.ini` (it's also the network version: different versions can't play
   together). Write `Docs/release-notes/v<ver>.md` (copy the last one's layout) and the `CHANGELOG.md` entry.
3. Run the whole suite; rerun failures alone.
4. Package into a **new** folder (a running packaged game locks its folder):
   `Tools\package.ps1 -Out C:\Dev\WildreachBuild<N> -SkipEditorBuild`, then
   `Tools\make_installer.ps1 -Build C:\Dev\WildreachBuild<N>` (makes `Wildreach-Setup.exe` there, ~1.7 GB).
5. Test the installer on a copy, never the build folder: `Wildreach-Setup.exe --silent --nolaunch --noshortcuts --dir
   C:\Dev\WildreachTest`, then start that copy's `Wildreach.exe` and play a little (host a world, open the menus). The
   first start of a game in a new folder brings up a Windows Firewall prompt; answer it yourself.
6. Commit (by name) and push. Then publish:
   `gh release create v<ver> C:\Dev\WildreachBuild<N>\Wildreach-Setup.exe --repo RavenArcher17/Survival-Game
   --target <full 40-character commit id> --latest --title "Wildreach <ver>: <what's new>" --notes-file
   Docs\release-notes\v<ver>.md`. A short commit id gets HTTP 422.
7. Check it: `gh release view v<ver> --repo RavenArcher17/Survival-Game --json assets` shows a `digest` that must equal
   `(Get-FileHash <setup> -Algorithm SHA256).Hash`, and https://github.com/RavenArcher17/Survival-Game/releases/latest
   must lead to the new tag. Then mark the CHANGELOG entry released, commit, push.

## Traps

- **Unity builds**: Unreal joins `.cpp` files into one, and adding a file reshuffles them. A local with the same name as
  a global or member elsewhere then breaks the build (C4459 / C4458 / C4456 are errors here). Keep file helpers in
  named namespaces, and don't reuse short global names.
- **Full-screen UI widgets must be `SelfHitTestInvisible`**, or an empty layer eats every click (0.4.0 shipped with no
  menu taking a click). Only tests that click for real (`menus`, `base`, `clan`...) catch it.
- **Anything loaded by path** (creature art, dressing meshes) must be under a `DirectoriesToAlwaysCook` entry in
  `Config/DefaultGame.ini`, or the packaged game won't have it.
- **Adding a Fab pack**: a "complete project" pack can overwrite `Config/Default*.ini` and drop a `.uproject` in the
  root. Run `Tools\check_setup.ps1` (or `git diff -- Config`) after adding any pack.
- **The Barghest pack's mane and tail** (`SK_BARGHEST_MANE`, `SK_BARGHEST_TAIL`, `BARGHEST_MANE_Anim`,
  `BARGHEST_TAIL_Anim`) crash cooking (`Assertion failed: !Hash.IsZero()`). The game doesn't use them; move them out of
  `Content` after adding QuadrapedCreatures. `check_setup.ps1` finds them.
- **GPU hangs**: Nanite tessellation is off (`r.Nanite.Tessellation=0` in `DefaultEngine.ini`) because it hung the GPU;
  don't turn it back on. The same goes for the shrubs' ash coat.
- **PowerShell 5.1**: a `.ps1` with non-ASCII characters needs a UTF-8 BOM; config and JSON files must not have one
  (write them with `[IO.File]::WriteAllText`, not `Set-Content -Encoding UTF8`). `git commit -F -` with a here-string
  fails in PowerShell: write the message to a file and use `-F <file>`.
- **Headless clients** need `t.MaxFPS 60` or they flood the server. Use `?WRName=<name>` for a stable player name.
- **Server presets**: with no settings a server plays `playtest` (10x XP, taming 10x, PvP off). PvP tests pass
  `-wrpreset=pvp_standard`.
- **The island's landscape** rebuilds only in the full editor (`Tools\terrain\build_island.ps1 -Force`), not a
  `-nullrhi` commandlet. Trees, rocks, caves and hazards change without a rebuild: `node Tools/terrain/dressing.mjs`.

## Art

- Creatures and props came from a local AI pipeline on Raven's PC (SDXL concept, Hunyuan3D 2.0 shape and texture,
  Blender rig; `Tools/art/README.md`). Its environment and models (`C:\Dev\WildreachArt`) aren't in git; what the game
  uses is imported under `Content/Creatures` and `Content/Props`, which are.
- Hunyuan3D 2.0's licence doesn't cover the European Union, the United Kingdom or South Korea, so nothing made with it
  is licensed there.
- The humanoid bosses and the player were rigged on Mixamo with Raven's Adobe account.
- Outside files and their licences are listed in `Docs/CREDITS.md` (shipped beside the game). Add a row for anything
  new from outside.
- **The hero** is made in the hero lab (`C:\Dev\WildreachCharLab` on Aaron's PC: MetaHuman Creator, the outfits, the
  retargeting in `Scripts/anim/`). Only built output comes here: `Content/Wildreach/Characters/Hero/{Unpacked,Outfits,
  Materials,Anims}` (2K textures, cards hair, no editable `MH_*` sources). The skeleton and the face rig are the
  MetaHumanCharacter plugin's (enabled in the `.uproject`, ships with 5.8), so they aren't in git. `Data/player_art.json`
  (schema 2) says which body, face, grooms and outfit each sex wears, the sockets, the action clips by key and the
  motion-matching chooser. After new clips arrive, run `Tools/anim/build_hero_anim.py` (the command line is in the
  file) to rebuild the databases from `Tools/anim/hero_databases.json` and the combat windows from
  `Data/anim_windows.json`.
- The hero animates with **no animation blueprint**: `UWRHeroAnimInstance` (a `UWRAnimInstance`) does motion matching
  in C++ (`UPoseSearchLibrary::MotionMatch` with its own pose history and blend stack), because the engine's
  motion-matching node can't run outside an animation blueprint in a packaged build and anim graphs can't be made
  headless.
