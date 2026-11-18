# Wildreach: the full cook (laptop, this terminal = Wildreach only)

## Context
Aaron: "full cook of Wildreach from here", built to plan and spec, quality raised to the Waveraider bar, every building lesson applied, and the main character rig scrapped and rebuilt from the ground up. The desktop keeps crashing, so this laptop is split: another terminal does Waveraider, this one does Wildreach only.

**What already exists (no re-deciding):**
- Plan: `showcase-notes/notes/Wildreach/FULL-RELEASE-PLAN.md`, with Round 0, then R1–R10 (one biome each, wheel order), then Final.
- Combat bible: `COMBAT-AND-LESSONS.md`, with Elden Ring combat, build order C0–C12, and region sizes (49 km², 105 creatures, 12 majors, 40 minis).
- Quality bars: `QUALITY-PLAN.md`, 11 categories.
- Settled decisions (11-10): all 12 biomes; Development cooks, Shipping only at the end; we work in Raven's repo as an approved author; no wipe; Raven decides releases, wipes and anything not in his plan; ten design items emailed to Raven.
- **Character (decided today):** MetaHuman + GASP.

**Repo state:**
- `RavenArcher17/Survival-Game` HEAD is `372557e` (v0.7.1). Nothing new since.
- We have WRITE access.

**This PC:**
- RTX 4070 laptop GPU (8 GB), 31 GB RAM, **85 GB free on C:**.
- UE 5.8.3 at the default path, so the repo scripts work as-is.
- VS 2022 with the game workload, git-lfs, gh logged in.
- GASP 5.8 is in the VaultCache. Blender 4.5 is at `C:\AI\Blender45`. ComfyUI is at `C:\Users\acars\AI\ComfyUI_windows_portable`, with Hunyuan3D 2.1 nodes but no 3D weights yet.
- **Missing:** the 6 Fab packs and the Wildreach clone.

## Ground rules for this cook (lessons applied)
- **Gates:** compile, cook and package only. Aaron and testers test via F8. I don't run Raven's bot or test suites or screenshot loops: Aaron's 11-12 no-checks rule is the latest call and supersedes 11-10's "suites before push".
- **One version per cook:** each focus below = one Development package in `C:\Dev\WildreachBuild\<ver>`, with a desktop shortcut and one email link. Only the latest 2 packages are kept, for disk.
- **Repo:**
  - `git pull` before work and before push. Commit to master, adding files by name.
  - Never force-push or stash. Update `CHANGELOG.md` and `Docs/HANDOFF.md` in the same commit.
  - **Never cut a GitHub release.** The auto-updater ships releases to every player, and that is Raven's call.
- **Fab packs:** never open, save or build assets without all 6 packs present (Raven's rule; a saved asset loses its pack links).
- **Character work runs before the packs land:** it happens in a separate scratch project, `C:\Dev\WildreachCharLab`, and is migrated in later.
- **One heavy job at a time on this PC,** shared with Waveraider: one UE editor/build/cook at once, art batches count as heavy, and ComfyUI is closed the moment a batch ends. Light work (C++, data, design) goes to `night.ps1` `claude -p` lanes, max 2.
- **Prove on one, then scale:** one creature, one boss, one Wildbrand, one biome kit item first.
- **Never claim done** unless it's in the cooked build's build.json. Fix forward, never revert.
- **F8 reports:** pulled at the start of every turn; work only on status=new.

## The Waveraider bar, written into every round (added to QUALITY-PLAN as hard bars)
- **Combat feel:**
  - full hit stack (reaction, flash, sparks, camera trauma, rumble, 4 sound layers)
  - hitstop 0.06–0.30 s at the deepest point of the blade
  - visible coil (0.14 s light, 0.20 s heavy)
  - one verb per button
  - 0.33 s input buffer
- **Telegraphs:**
  - the real weapon, claw or projectile is visible, and damage lands only on visible contact
  - the telegraph is a small cue; tells read at 15 m
  - projectiles give ≥0.9 s warning and never home
  - the colour law covers combat cues only
- **Enemies:** packs aggro freely, with no attacker limit. Players die, with no safety floors. Never add helpers after a fail.
- **Monsters:** terrifying, with a readable silhouette, posed and shown standing before any behaviour is written. No procedural primitives and no AI slop: concepts are picked, then models are finished in Blender.
- **Animation:** real clips; procedural only refines. No gliding and no loop snap. Gait speed matches ground speed.
- **Sound and music:**
  - creature voices synthesized from fused animal calls; a unique voice per creature
  - real composed music per biome (ACE-Step); always audible, ducks ≤4 dB
  - mix at −18 LUFS
- **World:**
  - a POI every 150–250 m and 3–5 sub-zones per biome
  - story told through the environment only, no staged scenes
  - the riverbed-gravel texture bar
- **UI:** industry-leading. Mouse and pad on every screen, no dead states, one prompt for the device in use, F1 controls page.
- **Perf:** smooth at Medium on an 8 GB card (this laptop is the reference). PSO cache bundled every cook, no sync loads, 40 GB free before a cook.

## Order of work

### Step 0: Setup (light, today)
1. Clone to `C:\Dev\Wildreach` and pull LFS (about 755 MB).
2. **Aaron, click by click** (WILD-3, on this laptop):
   1. Epic Games Launcher → Fab.
   2. Search each pack: GV_FreeShrubsPack, LavaMat, QuadrapedCreatures, SimpleRocks, WaterMaterials, MWLandscapeAutoMaterial.
   3. On each pack: Add to My Library → Library → Add to Project → Wildreach (5.8).

   I then move the 4 Barghest mane/tail files as the README says and run `check_setup.ps1` (setup check, not a game test).
3. Copy GASP 5.8 out of the VaultCache into the CharLab project.

### Step 1: New main character (the scrap and rebuild). One cook.
1. **Bodies:** male + female made in MetaHuman Creator (5.8, in-editor).
   - Attractive real-world bodies, built from explicit preset and measurement targets (the Trails D-30811 lesson).
   - A hair item on every head. Body sliders feed the character creator.
2. **Outfits:** a Wildreach survivor outfit (leather, fur, wraps) per body, tailored per body (Trails D-30812: no shared bust).
   - Path body changes (fire veins, frost rime, etc.) are material-layer parameters driven from `player_art.json`.
3. **Skeleton and locomotion:**
   - One skeleton for both bodies (MetaHuman body skel), exported at unit scale (Trails' x100 trap).
   - **GASP motion matching** for locomotion, with clips that slide a planted foot rejected.
   - A thin AnimBP (Motion Matching node + montage slots), parented to a rewritten C++ `UWRAnimInstance`, keeping Raven's code-driven style. No GASP Blueprint logic or Mover.
4. **Action clips:**
   - Sword-and-shield, longbow, rifle, throw, chop, gather, swim and roll, retargeted in-editor with the IK Retargeter from the existing Mixamo set.
   - Each clip is curated: coil frames and notify windows (combo, cancel, i-frames, hit frames).
   - Replaced by better clips (Fab) where one reads weak.
5. **Sockets and held items:** Grip sockets for weapon, shield, tools and ranged; each item is re-seated once the body loads.
6. **First person:** camera on the head bone and true full body, with only the head hidden in first person.
7. **Multiplayer cost:**
   - MetaHuman LODs trimmed; remote players use lower LODs and anim budgeting.
   - The LFS size is checked before push and kept lean: no 8K textures, no unused LODs.
8. **Code to change:** `WRCharacter.cpp` (`LoadBodyArt`, `SetupBodyAnim`, `BodyClip`, `PlayBodyAnim`, `TickBodyAnim`, `OnRep_FirstPerson`, camera at lines 69–73), `WRAnimInstance.{h,cpp}`, `WRRanged.cpp:157,166`, `WRHeldTool.cpp:112`, the `Data/player_art.json` schema, and the `HeldSocket` / `ranged.socket` fields in `items.json`.
9. **Remove** `Content/Characters/Survivor` and `SurvivorF` once the new body works. The main-menu body pick stays.

### Step 2: Reporting and controls (combat C1). Ships inside Step 1's cook if ready, else its own.
- **F8 bug report**, cloned from Waveraider's design: frozen frame, 60 s event ring, 8 categories, note.
- Reports POST to F35 `/api/feedback` with game=`wildreach` (pulled with `tools/reports.mjs`) and are also saved to `Saved/Reports`. Client notes are forwarded to the host.
- F1 controls page per device, the one-verb pad layout, held-input isolation, and mouse sensitivity/invert.

### Step 3: Combat core C0, C2–C5 on the new rig. One cook each.
C0 read-and-check, then C2 feel, C3 stamina and roll, C4 guard/parry/crits with server rewind, C5 stance and poise. This is "one boss fight feels good" (Raven's M1 rule).

### Step 4: Combat C6–C12. One cook each.
Full attack set, status, Wildbrands and spells (building the "[not built yet]" Fire/Frost nodes), all 18 creatures' attack rows, Vulkarn/Skaldrith plus minis (and a third mini each), camera and flask, PvP pass.

### Step 5: Round 0 polish and map plan (FULL-RELEASE 0.2–0.4). One cook.
- Bars written in; inventory and grading.
- L_Zoo map (in QUALITY-PLAN), and the 0.3 polish list: feel pass, held tools as real models, unique creature voices, whistle screen, controller, GPU-hang root cause, melee lag comp.
- **Music audit:** score Fire and Frost to the Waveraider music bar.
- The 12-wedge map layout goes to the review portal for Aaron's approval.

### Step 6: Biome rounds R1 → R10 (FULL-RELEASE section 5, as written). One cook per round, split at seams (R1a/b/c, R3a/b) where noted.
Each round:
1. **Design sheet:** roster, bosses, resource, hazard, sub-zones, POIs, music cue, lore-in-world. It goes on the review portal for Aaron's OK. Anything not in Raven's plan is logged in the next planning doc for Raven.
2. Data → C++ (`UWRGameplayAbility` + `ClassForNode`, all damage through `ApplyHit`).
3. **Art:** concepts picked → Hunyuan3D 2.1 base (ComfyUI, local weights) → Blender finish and retopo → existing `Tools/art` rig scripts. One creature is proven first.
4. Map dressing by script, then sound/fx rows, then cook.

### Step 7: Final round
Balance across 12 paths, dedicated server (needs a source-built engine of about 200 GB; done on the desktop or the 4 TB drive once fixed), PSO cache, the first Shipping build, installer and signing. The release itself happens only with Raven's OK.

## Risks I'm carrying, not asking about
- **Disk (85 GB):** clone + packs + DDC + 2 packages fits; the dedicated-server engine does not, so that waits for the desktop or the 4 TB drive.
- **8 GB VRAM:** Hunyuan runs in low-VRAM mode, art batches never overlap a UE build, and ComfyUI is closed after each batch.
- **LFS on Raven's account:** the new art goes well over 1 GB. I report the size each push; buying LFS data packs is Raven's call when needed.
- **Replacing the body Raven picked:** this is Aaron's direction (Wildreach is in his hands). It's noted in CHANGELOG/HANDOFF for Raven.

## Hand-off / verification
- **Gate per cook:** the C++ build compiles, the cook succeeds, and `package.ps1` produces the Development package.
- I confirm it's in the package's build.json, then open the build folder and send one email (link only).
- **MASTER-TRACKER:** Wildreach rows updated (new WILD-16+ rows: character rebuild, F8, C-steps, rounds). The laptop row now reads: this terminal = Wildreach.
