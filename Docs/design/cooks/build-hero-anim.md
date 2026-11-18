# Build lane: build-hero-anim (research + plan)

Written 2026-11-13 by the build-hero-anim lane. Follows `cooks/hero.md` (2.7, 3.2, 3.3) and `cooks/HERO-RESEARCH.md` (4, 5, 6). This file adds what the build needs that the cook left open, with what was found in the lab and the engine on the night.

---

## 1. Research

### 1.1 What the best games do

| Game / system | The specific thing | What makes it work | What we use |
|---|---|---|---|
| **For Honor** (Ubisoft, GDC 2017 "Motion Matching and The Road to Next-Gen Animation", Simon Clavet) | Motion matching for every hero's locomotion; the whole system is a pose search over a few hundred clips scored on future trajectory + current foot positions/velocities | The trajectory is the *gameplay's* prediction (spring-damped toward the stick), not the animation's; the search runs every few frames and only jumps when the new pose is clearly cheaper, so feet never pop | Trajectory built from our CMC + a critically-damped prediction toward the replicated acceleration; search every 0.1 s remote / every frame local; continuing-pose bias |
| **The Last of Us Part II** (Naughty Dog, GDC 2021 "Motion Matching in The Last of Us Part II", Michal Mach) | Motion-matched locomotion with combat layered on top; "MM sets" swapped by state (relaxed, combat, crouch) | Database *per stance*, picked by a chooser; upper-body actions are additive/masked layers that never stop the legs | A data chooser (gait × state × stance → databases) in `player_art.json`; upper-body slot from `spine_01` over MM legs |
| **Epic Game Animation Sample (GASP 5.8)** | 500+ clips in `PSD_Dense_*`, schemas `PSS_Default/Idle/Jump/Stop`, history collector → MM node → orientation & stride warping → foot placement | Separate idle/start/loop/stop/pivot databases with their own schemas (stops weight the future trajectory less, idles match pose only) | The **Extreme Sparse** database split (the lightest set GASP ships, same clip families) plus Dense idles/lands; the same schema split |
| **Lyra** (`FLyraReplicatedAcceleration`) | 3 bytes of quantised acceleration replicated for simulated proxies | Remote characters get start/stop intent a frame early; velocity alone lags by the network delay | `FWRReplicatedAccel` on `AWRCharacter` |
| **Elden Ring / Dark Souls** | Every attack clip carries frame windows: hit frames, combo window, cancel window, i-frames on the roll | The windows live *in the animation*, so timing always matches what you see | Notify states on the retargeted clips: `WR.Window.Combo/Cancel/IFrames/Trace`, placed by measuring the clip (peak hand speed) and the combat rule (combo = last 40% of recovery, cancel at 50% light / 65% heavy) |
| **Uncharted 4 / God of War (2018)** | Upper-body attacks over moving legs, full-body for rolls, deaths and heavy reactions | A per-bone mask from the spine up plus a mesh-space spine twist keeps the chest on the aim | `UpperBody` slot masked from `spine_01`, full-body slot for dodge/death/gather/swim |
| **Star Citizen / Mirror's Edge** | True first person on the third-person body | Eye-point camera off the head bone, horizontal 1:1, vertical spring, rotation from the controller only | `TickFirstPersonCamera` exactly per hero.md 2.7 |
| **Assassin's Creed Unity / Valhalla** | Feet planted on stairs and slopes in idle | Pelvis sinks to the lower foot, two-bone IK lifts the other | Kept from Raven's `PlantFeet` (pelvis drop + two-bone IK), now on `foot_l/foot_r` |

### 1.2 Real-world reference
- **Gait:** human walk 1.2-1.5 m/s, jog 2.5-3.5 m/s, sprint 6-7 m/s (biomechanics texts). Our 450/600/750 cm/s maps to GASP's walk (~170), run (~400-500) and sprint (~650-700) databases scaled by stride warping. The walk key (450) is a fast jog, so the **Walk** gait uses GASP's *run* clips and the **Run** gait (600) uses run clips played faster; **Sprint** uses sprint clips. Real gait thresholds (walk/run switch at ~2 m/s, Froude 0.5) put 450 cm/s firmly in running.
- **Stride warping limits:** feet stay planted up to about ±25% speed change (GASP default clamp 0.8-1.25); beyond that, pick a different clip.
- **Female stance:** the anterior pelvic tilt is typically 2-4° greater in women; shoulders slightly retracted. hero.md 2.3 asks for +3° pelvis, 2° shoulders, weight on one hip in idle: built as a procedural bone offset (no extra clips).

### 1.3 What was found on the night (facts the plan rests on)
| Fact | Consequence |
|---|---|
| Every hero body (M and F, all builds) uses **one skeleton**, `/MetaHumanCharacter/Female/Medium/NormalWeight/Body/metahuman_base_skel` (plugin content) | One set of retargeted clips serves both sexes (GASP itself shares one library between Manny and Quinn). Clips are retargeted once onto the **M athletic** body; the skeleton's translation retargeting = `Skeleton` on every bone except `root` (Animation) and `pelvis` (AnimationScaled) makes them fit F and the lean/strong builds. Halves the LFS cost of animation |
| The built heroes reference ~170 packages of the **MetaHumanCharacter** engine plugin (materials, face/body post-process AnimBPs, skeleton) plus ControlRig, HairStrands, Niagara | The game repo enables the `MetaHumanCharacter` plugin (ships with UE 5.8, no install). Nothing from it is copied into the repo |
| Built MH = `Body` (post-process `ABP_Body_PostProcess` on the mesh), `Face` (anim class `ABP_Face`, copies the body pose, RigLogic), grooms Hair/Eyebrows/Eyelashes/Beard/Mustache bound to the face, `LODSync` | `AWRCharacter` builds the same components in C++ from `player_art.json` |
| **Anim graphs can't be authored headless** (no Python API for anim-graph nodes), and a native `FAnimNode_MotionMatching` outside an AnimBP hits `check(NodeData)` in packaged builds (fold properties) | Motion matching runs **natively**: `UWRHeroAnimInstance` owns an `FPoseHistory`, calls `UPoseSearchLibrary::MotionMatch` and plays the result on its own blend stack. This matches Raven's rule for the repo (`UWRAnimInstance`: "built in code, no animation blueprint, runs headless"). The "thin AnimBP" of the cook has no logic by design, so nothing is lost; the chooser becomes a data table |
| Pose Search databases keep their clip list in a private property (`DatabaseAnimationAssets`); Python can't fill it. `UPoseSearchDatabase::AddAnimationAsset` is exported C++ | An editor-only `UWRHeroAnimTools::BuildDatabases` (Wildreach module, `WITH_EDITOR`) builds the schemas and databases from `Tools/anim/hero_databases.json`, run once through `-run=pythonscript` |
| GASP clip sizes: Dense set ~1.3 GB of source; **Extreme Sparse** ~70 MB for the same families | Ship Extreme Sparse + Dense idles/turns/lands (≈ 75 clips) |
| `UWRWeaponTraceNotifyState` and `WRAnimNotifies` don't exist in the repo yet (combat.md calls them BUILT/new) | This lane adds `UWRANS_Window` (combat.md "Windows" row) and places the windows on the clips; the combat-core lane drives gameplay from the tags |

---

## 2. Plan (every detail decided before building)

### 2.1 Retargeting (lab, `Scripts/anim/`)
| Step | Detail |
|---|---|
| R1 Mixamo IK rig | Copy `Content/Characters/Survivor` from the repo into the lab (same path). `IK_Survivor_Mixamo` on `survivor_m`: retarget root `Hips`; chains Spine (Spine→Spine2), Neck, Head, LeftClavicle, LeftArm (LeftArm→LeftHand, IK goal LeftHand), RightArm, LeftLeg (LeftUpLeg→LeftToeBase, IK goal LeftFoot), RightLeg, fingers ×10 (Thumb/Index/Middle/Ring/Pinky 1-3 each hand). Built with the auto-generated retarget definition, then chain names matched to `IK_Metahuman_Retarget` |
| R2 RTG | `RTG_Mixamo_to_Hero`: source `IK_Survivor_Mixamo`, target `IK_Metahuman_Retarget` with preview `SKM_MH_Hero_M1_BodyMesh`; op stack Pelvis Motion → FK chains → IK chains (feet, hands) → **Root Motion (generate root from pelvis, horizontal only, rotation from pelvis yaw)**. Target ref pose: the MH A-pose; source: Mixamo T-pose auto-aligned (`auto_align_all_bones`) |
| R3 GASP | `RTG_UEFN_to_Metahuman_nrw` (GASP's own), target mesh overridden to the M athletic body |
| R4 Batch | `IKRetargetBatchOperation.run_batch_retarget`, output `/Game/Wildreach/Characters/Hero/Anims/{Loco,Actions}`; prefix `A_` for actions, `MM_` for locomotion; overwrite on |
| R5 Per clip | `enable_root_motion` = true on actions (`root_motion_root_lock` = RefPose) so the game plays them in place while the root bone carries the travel; `retarget_source_asset` = the M athletic body; locomotion clips keep GASP's root motion (needed by the trajectory match) |
| R6 Skeleton | translation retargeting: `root` Animation, `pelvis` AnimationScaled, every other bone Skeleton (set once on `metahuman_base_skel`; it is plugin content, so this is set on the repo copy… see 2.6) |

Clip keys kept (survivor → hero): every key in today's `player_art.json` anims plus `ride` (fallback: `tread` until a riding clip exists), `kick`, `sheath`, `crouch_idle`.

### 2.2 Notify windows (repo, editor Python after migration)
`UWRANS_Window` (new `WRAnimNotifies.h/.cpp`): one notify state with `FGameplayTag Window`; begin adds the loose tag on the owner's ASC with `TagOnly` replication when the owner has authority (or local prediction), end removes it. Tags (new, `WRGameplayTags`): `State.ComboWindow`, `State.CancelWindow`, `State.IFrames`, `State.TraceActive`.

Placement (`Tools/anim/notify_windows.py`, run in the repo editor):
| Clip family | Trace (hit frames) | Combo | Cancel | I-frames |
|---|---|---|---|---|
| light ×3 | `hand_r` component-space speed sampled at 60 Hz; window = frames where speed ≥ 55% of the peak, around the peak | last 40% of the time after the trace ends | from 50% of recovery to end | – |
| heavy ×3 | same | last 40% of recovery | from 65% | – |
| chop / kick | same (`hand_r` / `foot_r`) | – | from 65% | – |
| dodge (StandToRoll) | – | – | last 30% | from `player.dodgeIFrameStart` to start+`dodgeIFrames`, scaled by clip length ÷ `dodgeVisualSeconds` |
| cast ×2 | `hand_r` peak = release frame (window 0.1 s) | – | from 60% | – |

Results are written to `Data/anim_windows.json` (clip → windows) as the human-readable record and read back by `UWRHeroAnimInstance` for the dynamic montages (a montage made from a sequence keeps the sequence's notifies).

### 2.3 `UWRHeroAnimInstance` (new, C++)
Fields filled by `AWRCharacter::TickBodyAnim`: `Gait` (Walk/Run/Sprint), `MoveState` (Idle/Moving/Air/Swim), `Stance` (Melee/Ranged/BowAim/RifleAim/Block), `bLockedOn`, `AimPitch/AimYaw`, `FootLift[2]`, `bFemale`, `LODMode`, `UpperLoop`, `FullLoop` (+hold/start).

Frame (game thread, `NativeUpdateAnimation`):
1. **Trajectory**: history ring of the actor transform every 0.033 s for 0.4 s; future samples at 0.35/0.7/1.0 s (plus 0.1/0.2 for the schema's near samples) predicted by a critically damped spring from the current velocity toward `MaxSpeed × accel direction` (half-life 0.15 s = the spec's 0.15 s to full speed), facing toward the controller yaw when strafing (block, aim, lock-on, first person) or toward travel otherwise. Written as an `FTransformTrajectory` in world space.
2. **Chooser** (`player_art.json hero.locomotion`): rows `{state, gait, stance} → [databases]`; first match wins; `LODMode ≥ fallbackLOD` uses the `fallback` row (a single loop database).
3. **Search** (worker thread, proxy `Update`): every frame for the local player, every `anim.mm.remoteSearchInterval` (0.1 s) for others; `UPoseSearchLibrary::MotionMatch` with the history, the chooser's databases and the continuing pose; jump when the new cost is lower than the continuing cost − 0.01 and the new pose isn't within 0.2 s of the playing one in the same clip.
4. **Blend stack**: up to 4 entries, newest blends in over 0.2 s (inertial-style ease), playing at rate = clamp(ground speed ÷ clip root speed, 0.8, 1.25) (= stride warping by rate); root bone zeroed (the capsule moves the body).
5. **Orientation warping**: the clip's root-motion heading vs the real travel direction, clamped ±45°; the lower body turns by it, `spine_01..05` counter-rotate so the chest stays on the facing.
6. **Layers**: `UpperLoop` (block, bow aim, rifle aim) masked from `spine_01`; one-shots on `UpperBody`/`FullBody` exactly like today (`PlayOneShot` signature kept; montages are built with `UAnimMontage::CreateSlotAnimationAsDynamicMontage` on the slots so the windows' notifies fire on every machine); spine aim offset pitch ±70°, yaw ±60° (first person and aiming).
7. **Feet**: pelvis drop + two-bone IK (Raven's `PlantFeet`) on `thigh/calf/foot`.
8. **Female stance** (idle only, weight by idle blend): pelvis pitch +3°, `clavicle_l/r` back 2°, pelvis roll 2° toward the weight leg.
9. **Pose history**: the final component-space pose is fed into the `FPoseHistory` (`EvaluateComponentSpace_AnyThread`), so the next search matches what was actually shown.

### 2.4 `AWRCharacter` changes
| Area | Detail |
|---|---|
| LoadBodyArt v2 | `player_art.json` `hero.m/f`: body mesh, face mesh, face anim class, grooms (hair, brows, lashes, beard, moustache with bindings), outfit slot meshes (wraps: chest/hands/legs), `skinMat`. Components created on first load: `Face` (attached to Body, `ABP_Face`), 5 `UGroomComponent`s attached to Face, 4 outfit `USkeletalMeshComponent`s with `SetLeaderPoseComponent(Body)`, `ULODSyncComponent`. Body anim class `UWRHeroAnimInstance` |
| Grip | Weapon/shield/ranged/tool attach to sockets (`weapon_r`, `shield_l`, `bow_l`…) from `player_art.json sockets`, `SnapToTarget`, plus the item's `grip` offset/`rot`. **The Mixamo palm-down math is deleted** |
| View | First person hides Face + grooms + head outfit from the owner (`SetOwnerNoSee`, `bCastHiddenShadow`), body stays visible (full-body FP); camera from `TickFirstPersonCamera` |
| FP camera | eye = `head` socket + 9 cm fwd + 6 cm up (actor space), horizontal 1:1, vertical critically-damped spring half-life 0.05 s, clamp ±2.5 cm around the target; rotation = control rotation, no roll; near clip 5 cm (project setting `NearClipPlane=5`); dodge in FP: camera down 30 cm and back over the roll (sin curve), no rotation |
| Movement | `MaxAcceleration` 5000, `BrakingDecelerationWalking` 4000, `bUseSeparateBrakingFriction` true, `BrakingFriction` 6, `GroundFriction` 8 — all `tuning.json player.*` |
| Accel replication | `FWRReplicatedAccel {uint8 Angle, uint8 Magnitude, int8 Z}` + `uint8 AnimBits` (gait 2 bits, stance 3 bits, lock-on 1), server sets from the CMC each tick, `COND_SimulatedOnly` |
| Body pick | `BodyStyle` `m`/`f` from the menu (unchanged URL `?Body=`), picks `hero.m` or `hero.f` |

### 2.5 Other code
- `WRRanged.cpp AttachHeld`: socket from the item (`weapon_r`/`bow_l`), placement = socket frame (X along the grip, Z palm normal) + item `rot`/`offset`; `along/face/pose/meshAlong/meshFace` removed from code and data.
- `WRHeldTool.cpp`: the tool's handle runs along the `weapon_r` socket X; the `Index1` finger lookup goes.
- `items.json`: ranged sockets `RightHand`→`weapon_r`, `LeftHand`→`bow_l`; `rot` per weapon tuned to the socket frame (bow: limbs vertical, string toward the body; crossbow/blowgun: stock back along −X of the trigger hand; spear: point forward).

### 2.6 Migration (repo)
1. Copy lab output to the same `/Game/Wildreach/Characters/Hero/...` paths in the repo: `Unpacked/BP_Hero_M1/{Body,Face,Grooms}`, `Unpacked/BP_Hero_F1/{Body,Face,Grooms}`, `Outfits/Common`, `Outfits/Wraps/*_Athletic_*`, `Materials` (MI_WR_Skin + MF_PathMarks + path textures), `Anims`. No `MH_*` sources, no BP actors, no lean/strong builds (those wait for the creator lane), no 4K.
2. Enable plugins in `Wildreach.uproject`: `MetaHumanCharacter`, `PoseSearch`, `HairStrands`, `RigLogic`, `LiveLink` (MH dependency), `ControlRig`.
3. `DefaultGame.ini` `DirectoriesToAlwaysCook` += `/Game/Wildreach/Characters/Hero`.
4. Remove `Content/Characters/Survivor` and `SurvivorF` in the same commit.
5. Skeleton translation retargeting is a property of the plugin's skeleton asset (plugin content, can't be committed). So `UWRHeroAnimInstance::SetupProportions` compares the playing body with the clips' retarget-source pose (`UAnimSequence::GetRetargetTransforms`, the M athletic body) and, unless the skeleton already asks the engine to, gives every bone but root and pelvis the playing body's own length (Skeleton mode) and scales the pelvis by the hip-height ratio (AnimationScaled). One clip set, no edit to plugin content.

### 2.7 Edge cases
- Dedicated server: no body, no anim instance, no grooms (unchanged rule). Server still replicates the accel/anim bits.
- Missing plugin/asset: falls back to a log warning and the capsule placeholder (unchanged behaviour).
- Death: full-body hold; MM stops searching.
- Swimming: full-body loop (swim/tread), MM paused; history keeps recording.
- Falling: `PSD_Jumps` when the jump starts, lands from `*_Lands_*` by the landing speed (heavy over 700 cm/s).
- Body swap at runtime (`OnRep_BodyStyle`): components rebuilt, anim instance reset.
- First person ↔ third person: instant, same pose.
- Remote players at LOD ≥ 3: fallback loop only, searches stop.

### 2.8 Build steps (in order)
1. Lab: R1-R6 scripts, batch retarget Mixamo actions + GASP sparse set onto M1 athletic.
2. Repo: plugins, module deps, `WRHeroAnimInstance`, `WRAnimNotifies`, `WRHeroAnimTools` (editor), tags, `AWRCharacter` v2, FP camera, accel replication, ranged/tool/items sockets.
3. Copy assets, build databases (`BuildDatabases`), place notify windows, write `player_art.json` v2.
4. Remove Survivor folders. Compile. Spot-check against this plan. Commit + push.


## 3. Built (2026-11-13 night) and deviations from the cook

| Plan item | Built as | Why |
|---|---|---|
| Thin `ABP_Hero_M/F` + `CHT_Locomotion` Chooser asset | Native motion matching in `UWRHeroAnimInstance` (subclass of Raven's `UWRAnimInstance`), chooser rows in `player_art.json` | Anim graphs can't be authored headless, a native `FAnimNode_MotionMatching` hits `check(NodeData)` in packaged builds, and Raven's repo rule is no animation blueprints |
| 14 databases (Dense) | 12 databases from GASP's Dense idles + Extreme Sparse sets (58 clips, ~68 MB); no pivots, no crouch, walk stops use run stops | LFS budget (Dense alone is ~1.3 GB of source); the game has no crouch |
| Retarget once per sex | Once, onto M1 athletic; F fitted at runtime (2.6.5) | Halves the clip LFS; GASP itself shares one library across Manny and Quinn |
| Mixamo root motion op | Not used: the Mixamo clips are in place (hips never travel); the op broke the pelvis | Clips play in place under the capsule as before |
| Socket frame + item `rot` (drop along/face/pose) | Socket frame kept, along/face/pose kept and measured in the socket's frame; new tokens `grip`, `palm`, `backhand`, `arm` | Can't look at renders tonight; measured placement keeps every item's tested hold |
| Dynamic montages on slots | Raven's one-shot layers kept; their notifies forwarded through the proxy (`AddAnimNotifies`) | Same result (windows fire on every drawing machine) with no slot graph |
| A6-A8 (creator, appearance replication, path marks, outfits by armour) | Not in this lane | Brief scope: anim + migrate the finished heroes |

NOT finished: lean/strong builds and faces M2/F2 not migrated; armour looks (Hidewear etc.) not switched by worn armour; Mixamo mantles / ride clip not added (no mantle or riding in code); the FP "First Person Output" material gradient and FirstPerson primitives not set (needs skin-material edits in the lab); F1 hair groom still carries strands (63 MB); RigLogic LOD gating and Animation Budget Allocator not configured.
