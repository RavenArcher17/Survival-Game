# Wildreach hero: research for the cook

2026-11-13. Research only. Nothing in `C:\Dev` was changed.

**Already decided (not reopened):** MetaHuman male and female built from script in UE 5.8's in-editor MetaHuman Creator; GASP motion matching; a thin AnimBP over a C++ `UAnimInstance`; true full-body first person with the camera on the head bone; path body changes as material layers; attractive, real-world-sexy bodies and faces, never sanitized.

**Read first:** `FULL-GAME.md` sections 2, 4.2, 6.3 and 9; `Docs/design/design-doc.md` (Classes, Gear); `Data/player_art.json`; `Data/items.json`; `WildreachCharLab/Scripts/mh_*.py` and `mh_api.txt`; the lab's current shots in `WildreachCharLab/Saved/hero/`.

---

## The answers

| # | Question | Recommendation |
|---|---|---|
| 1 | How heroes are presented | A short creator with good-looking presets, not a slider swamp. Per sex: **6 faces, 3 builds, 10 hair, 6 beards (male), hair/eye/brow colour**. Start in **survivor wraps that show skin**. Each tier adds coverage and bulk, so you can read a player's tier from 30 m. Each biome gets a named set themed on its signature material. Add an **appearance layer (transmog)** and a **hide-helmet toggle** so path marks stay visible. |
| 2 | Attractive MetaHuman faces | Start from **one strong preset per face (weight 0.7 or more)**, then sculpt the landmarks. Averaging 2 to 4 presets is why the lab faces look soft. Target the **jaw, cheekbones, canthal tilt, lips and brow ridge**. Use **4K source textures, roughness 1.05 to 1.10, warm accents on the cheeks, nose and ears, real lashes and brows**. Exact API calls are in section 2. |
| 3 | Outfits | Make our own garments in Blender or Marvelous as **MetaHuman Outfit Assets (resizable), fitted offline per build and cooked as skinned meshes**. Use cloth simulation only on capes, fur skirts and tassels. **Mutable is the step up** once we pass about 40 garments or want real body sliders. Fab outfit packs can't go into Raven's public git. |
| 4 | GASP on our C++ character | Keep the **CMC** (no Mover). Build the trajectory in C++ `NativeThreadSafeUpdateAnimation` and pick databases with a **Chooser table**. The AnimBP is a single chain: Motion Matching, then Pose History, warping, an upper-body slot and foot IK. **Retarget the GASP library offline, once per sex**, rather than at runtime as the 5.8 sample does. Bring the Mixamo clips through IK Retargeter with the **Root Motion op**. Network cost is about **+4 bytes per update** (quantised acceleration plus gait). |
| 5 | Full-body first person | Use one body mesh. **Camera position = head bone, stabilised; camera rotation = control rotation, never the head's.** Hide the face and grooms from the owner but keep their shadow. Use UE's **First Person Output gradient** on the arms only, for anti-clipping. **Near clip 5 cm, FOV 90 by default, a centre dot.** Note: Dune Awakening has **no** full-body first person. |
| 6 | Multiplayer cost | **Cards-only hair in game**; strands only in the creator. LOD0 for the local player, then LOD1 from 8 m, LOD3 from 25 m and LOD5 from 60 m. **2K textures; animated maps off for remote players.** Animation budget: **2.0 ms for 16 players**, MM throttled on remote players. **Repo: the lab builds are ~270 MB per hero today. Cut them to ≤ 120 MB per hero and keep MetaHuman source assets out of the game repo.** |

---

## 1. How the best games present the player hero

### 1.1 What they do

| Game | Creator scope | Starting outfit | How armour tiers change the silhouette | Body change from progression |
|---|---|---|---|---|
| **Valheim** | Very small: sex, skin-tone slider, hair, beard, hair tone and blondness. Locked after creation until the Barber Station. [dbltap](https://www.dbltap.com/posts/valheim-character-creator-guide-01ez2ndy56rg), [BisectHosting](https://www.bisecthosting.com/fr-fr/blog/valheim-barber-station-haircuts-requirements-build-guide) | Bare tunic. Rags are the first craft. | Each biome's set is one clear step: rag, leather, troll, bronze, iron, wolf, padded, carapace. Capes mark the biome. Skin shows less as you climb. | None |
| **Enshrouded** | Presets plus a small set of sliders | Flameborn rags | 5-piece sets for each class line. A cosmetic appearance slot is worn over combat gear, and stats stay. [PinDrop (fan)](https://pindrop.gg/enshrouded/armor/category/cosmetic-armor-set) | None |
| **Conan Exiles** | Deep sliders for head and body, including sexual ones. [Wikipedia](https://en.Wikipedia.com/wiki/Conan_Exiles), [PC Gamer](https://www.pcgamer.com/games/mmo/heres-why-dune-awakenings-character-creator-wont-have-an-endowment-slider-like-conan-exiles) | Near naked, cut down from the cross | Light, medium and heavy culture sets. Heavy armour fully hides the body. | Corruption is a stat. Its visuals are unconfirmed. [Funcom](https://conanexiles.com/?p=11451) |
| **Palworld** | Sliders for face and body, simple | Plain clothes | Armour plus cosmetic skins | None |
| **Once Human** | Deep: presets, face, hair and makeup, body. Re-edits are free since 09-2026. [Dot Esports](https://dotesports.com/once-human/news/once-human-character-creation-guide-all-customization-options-explained), [PlayNews](https://www.playnews.gg/en/guides/once-human-character-customization-and-hair-dyeing-become-free-on-september-28-retroactive-3-month-refund) | Survivor clothes | Gear stats are separate from per-slot cosmetic skins. [Sportskeeda](https://www.sportskeeda.com/mmo/how-change-gear-appearance-once-human) | None |
| **Dune: Awakening** | Deep Funcom creator. Pointedly no endowment slider. [PC Gamer](https://www.pcgamer.com/games/mmo/heres-why-dune-awakenings-character-creator-wont-have-an-endowment-slider-like-conan-exiles) | Prisoner / stillsuit | Stillsuit and armour sets | None. Third person, with first person **only on scopes and binoculars**. [Steam](https://steamcommunity.com/app/1172710/discussions/0/4638240954399181530) |
| **Elden Ring** | Deep face sliders plus presets, body type A or B | Set by class (Vagabond, Samurai, Wretch nearly naked) | 4 slots (head, chest, arms, legs). Weight sets roll speed, which Raven already plans as equip load. | Frenzied Flame turns the eyes: a small, unmistakable mark |
| **Monster Hunter Wilds** | Deep. Full re-edit needs a voucher. [RPG Site](https://www.rpgsite.net/guide/16956-how-to-change-appearance-in-monster-hunter-wilds-voice-character-edit-vouchers-and-more-covered) | Hope set | **Each set is the monster you killed**, with its colours, scales and horns. Layered armour (transmog) per piece, plus underclothes. [GamesRadar](https://gamesradar.com/games/monster-hunter/monster-hunter-wilds-armor-appearance-layered-transmog) | None |
| **Horizon Forbidden West** | Fixed hero (Aloy), with dyes | Nora brave outfit | Tribe-themed outfit sets. The machine parts on them show where they came from. | None |
| **Dragon's Dogma 2** | Very deep, including height of 160 to 215 cm and muscle; build changes carry weight. [VideoGamer](https://www.videogamer.com/guides/dragons-dogma-2-character-creation-settings) | Plain clothes | **Armour warps to every body**, so a heavy build makes some sets look worse. [Laptop Mag](https://www.laptopmag.com/gaming/i-hate-dragons-dogma-2s-character-creator-and-its-not-for-the-reason-you-think) | None |
| **ARK: Survival Ascended** | Body sliders (head, torso, limbs) | Naked | Cloth, hide, chitin, flak, riot, tek: one of the clearest silhouette ladders in survival games | None |

### 1.2 What Wildreach takes from this

1. **Creator: short and pretty by default** (Valheim's speed, Elden Ring's presets, Once Human's tabs). Five tabs: **Face, Body, Hair, Colour, Path**. Every option already looks good; no option can make the hero ugly. Allow free re-edits at a mirror in the base. Once Human dropped its fee because players hated paying.

| Tab | Male | Female | How it's built |
|---|---|---|---|
| Face | 6 faces | 6 faces | Each one is built in the editor from script. Skin tone comes with the face, because MetaHuman bakes skin into the synthesized face and body textures. Spread the 6 across light to dark tones. |
| Body | 3 builds: lean, athletic, strong | 3 builds: slim, athletic, curvy-strong | Body constraint sets (see 2.4). Height is a separate ±4% slider (uniform scale, root only). |
| Hair | 10 hair, 6 beards, 4 brows | 10 hair, 4 brows | Epic's groom wardrobe items (`/MetaHumanCharacter/Optional/Grooms/Bindings/`), **cards in game** |
| Colour | Hair melanin and redness, eye hue, brow and lash darkness | Same, plus 3 lip tints and 3 eye-liner types | MetaHuman post-assembly parameters (hair colour is documented) and an eye MID |
| Path | Preview of the path marks at strength 0.3 / 0.6 / 1.0 | Same | The material layer (see 1.4) |

2. **Start outfit: "survivor wraps".** The player wakes on ash or snow in linen wraps, a leather chest wrap or binder, fiber-bound forearms, wrapped shins and a belt pouch. **About 40% skin shows**: shoulders, upper arms, midriff on the female, chest V on the male, thighs. This is the Conan / Valheim / Wretch moment: you start exposed and clothe yourself by surviving. It is also the outfit that shows off the attractive bodies and the first path marks.

3. **Tier ladder: coverage and bulk grow with each tier.** Today `items.json` has one `armor` slot with tiers hide, copper, bronze and iron. The proposal keeps those tiers and splits the look into **4 visual slots (head, chest, hands, legs+feet)**, as in Elden Ring. That means 4 meshes per set, and the stats can stay on one item until Raven splits them.

| Tier | Set (Fire / Frost) | Skin showing | Silhouette | Material |
|---|---|---|---|---|
| 0 Survivor | Wraps (both) | ~40% | Body line fully readable | Linen, fiber rope, raw leather |
| 1 Hide | **Cinderhide** (Cinderfang pelt, charred leather) / **Snowhide** (white fur, hooded collar) | ~30% | Fur collar and bracer bulk | Hide, fur |
| 2 Copper | Hide with copper scale on chest and shoulders | ~20% | Shoulder width +8 cm | Copper scale, verdigris |
| 3 Bronze | Lamellar over leather, skirted | ~10% | Skirt plus pauldrons | Bronze lamellar |
| 4 Iron | Mail plus plate, closed helm option | <5% | Full armour block, +15 cm shoulders | Iron, mail |
| Biome set (Hearth 4) | **Forgeplate**: obsidian plates, glowing seams / **Rimeguard**: blue-white steel, ice-crystal pauldrons, white fur | Designed per set: face, neck and forearms left open | One unmistakable shape per biome, like a Monster Hunter set | Signature resource (FULL-GAME 6.3) |

**Silhouette rule:** a player's tier and biome must read at **30 m from shape alone**, as ARK and Monster Hunter manage. Each tier adds one big shape (collar, pauldron, skirt, helm crest).

4. **Path body marks must stay visible under armour.** Use three means:
   - An **appearance layer**: show any unlocked set over your stat set, as in Monster Hunter Wilds and Enshrouded.
   - A **hide-helmet toggle**.
   - Every biome set leaves the **face, neck and forearms open**, which is where the marks live.

   Elden Ring proves that a mark in the eyes alone reads strongly, so every path gets **an eye change plus a skin change** (Fire: ember ring in the iris plus veins; Frost: pale-blue limbal ring plus rime on lips and fingertips).

---

## 2. Making MetaHuman faces genuinely attractive

### 2.1 What the lab has now (`Saved/hero/*_face.png`)

- **Both faces read soft and wide:** round skull, wide jaw on the female, low cheekbones, flat brow.
- **No hair, brows or lashes in the shots.** The groom slots are set in `mh_fix.py`. Either SceneCapture isn't rendering strands, or the build ran before the slot change. Confirm in a viewport HighResShot (`mh_shot.py`) before judging the grooms.
- **Flat, uniform skin with a pink, waxy mouth.** No accents, no makeup, roughness at default.
- The cause is `mh_faceblend.py`. It **linearly averages the face-model coefficients of two presets** (Orlando 0.6 + Victor 0.4; Vivian 0.55 + Sunita 0.45). Averaging pulls every feature toward the mean, so the jaw, cheekbones and eye shape shrink. Some averageness helps attractiveness, but **sexual dimorphism and clear structure matter as much** (Rhodes 2006, *The evolutionary psychology of facial beauty*, Annual Review of Psychology 57:199-226). A 50/50 PCA blend throws the structure away.

### 2.2 Which parameters matter, and the API call for each

| What | Target, female | Target, male | API (from `mh_api.txt`) |
|---|---|---|---|
| **Base face** | One preset at weight **≥ 0.7**, with up to 0.3 of a second for skin and ethnicity. Never 50/50. | Same | `get_face_model_coefficients(preset)` → weighted sum → `set_face_model_coefficients(ch, c)` → `commit_face_state(ch)` |
| Jaw and chin | Narrow V, defined chin point, jaw angle under the ear lobe | Wide square jaw, strong chin, visible mandible angle | `get_face_landmarks(ch)` → `translate_face_landmarks(ch, indices, deltas)` → `commit_face_state` |
| Cheekbones | High and forward, slight hollow under them | Same, plus more hollow | Same landmark calls |
| Eyes | **Positive canthal tilt** (outer corner 1 to 2 mm up), almond shape, slightly larger | Positive tilt, slightly hooded, deep-set | Same landmark calls |
| Brow ridge | Soft, arched brow | Heavier ridge, low straight brow | Landmark calls + `Eyebrows` slot |
| Nose | Narrow bridge, small defined tip | Straight, strong bridge | Landmark calls |
| Lips | Full lower lip, defined Cupid's bow, short philtrum | Defined, medium-full | Landmark calls |
| Head scale | **0.96** | **1.00** | `face_evaluation_settings.head_scale`. The property is read-only in Python. Set it in the MetaHuman Creator UI (Head Sculpt → Head Parameters) or through a C++ editor helper. [Epic: Head Sculpt](https://dev.epicgames.com/documentation/metahuman/metahuman-creator-head-sculpt-tool-in-unreal-engine) |
| Skin detail | `global_delta` 1.0, `high_frequency_delta` 1.0 (never lower: that is where pores and fine lines come from) | Same | `face_evaluation_settings` (read-only, as above) |
| Skin tone | Per face, spread light to dark | Per face | `skin_settings.skin.u / v` → `commit_skin_settings` |
| Skin texture | `face_texture_index` = a young-adult texture with visible pores | Same, with more texture | `skin.face_texture_index`, `body_texture_index` |
| **Anti-wax** | `skin.roughness` **1.06** (range 0.85 to 1.15) | **1.10** | `skin_settings.skin.roughness` |
| Life in the skin | Accents: cheeks redness +0.15, nose +0.10, ears +0.20, lips +0.10, under-eye lightness −0.05 | Cheeks +0.10, nose +0.15, ears +0.20, scalp and chin (stubble shadow) lightness −0.10 | `skin_settings.accents.<region>.redness / lightness / saturation` |
| Freckles | Optional per face: `TYPE1`, density 0.3, strength 0.25 | Off | `skin_settings.freckles` |
| Source textures | **Request 4K** for face albedo, normal and cavity, and body albedo and normal; downsize at build (see section 6) | Same | `skin_settings.desired_texture_sources_resolutions` → `request_texture_sources(ch, params)` |
| Eyes | Limbal ring size **0.78**, softness 0.06, iris saturation 1.2, sclera vascularity 0.1 | Same | `eyes_settings.eye_left / eye_right.iris / sclera` → `commit_eyes_settings` |
| Lashes | `LONG_CURL` (`LONG_THICK_CURL` on 2 of 6 faces) | `SHORT_FINE` | `head_model_settings.eyelashes.type` → `commit_head_model_settings` + the `Eyelashes` groom slot |
| Brows | `Eyebrows_M_SlightArch`, fuller variants | `Eyebrows_M_Full`, thick | Groom slot (`try_add_item_from_wardrobe_item`, `set_single_slot_selection`) |
| Makeup | Natural survivor look: lips `NATURAL`, opacity 0.35, lip roughness 0.45 (soft gloss); eyes `THIN_LINER` on 3 faces, `SOFT_SMOKEY` on 1; blush `HIGH_CURVE` 0.15; foundation off | None (foundation off, lips `NONE`) | `makeup_settings.lips / eyes / blush / foundation` → `commit_makeup_settings` |
| Teeth | `worn_down` 0, `plaque_amount` 0, `variation` 0.2, slightly warm white | Same | `head_model_settings.teeth` |
| Rig | Joints and blend shapes (for expressions in the creator and in emotes) | Same | `request_auto_rigging(ch, JOINTS_AND_BLEND_SHAPES)` |

### 2.3 Known techniques

- **Pick the best real faces first.** Grab all 29 presets (`mh_faces.py` already does this), choose the 6 strongest per sex by eye, and use each as the 0.7 base. MetaHuman's own Blend tool mixes **up to 3 presets per region**, not across the whole face ([Epic: Blend tool](https://dev.epicgames.com/documentation/metahuman/metahuman-creator-blend-tool-in-unreal-engine)). The script equivalent is the global blend plus **region-targeted landmark moves**.
- **Sculpt with landmarks, not more blending.** Every sculpt is projected back onto the nearest real scanned face ([Epic: Head Sculpt](https://dev.epicgames.com/documentation/metahuman/head-sculpt-controls)), so small moves stay human. Keep each move under **2 to 3 mm**, symmetric, and commit after each region.
- **Lighting sells skin.** Judge faces under a key light at 35° plus rim light, never under flat front light. MetaHuman 5.8 adds custom lighting previews in the Creator ([MetaHuman 5.8](https://forums.unrealengine.com/t/metahuman-5-8-released/2729288)).

### 2.4 Bodies (`mh_shape.py` targets are sound; keep them)

| | Male athletic (current) | Female athletic (current) | Notes |
|---|---|---|---|
| Height | 184 | 170 | |
| Chest / waist / hip | 110 / 81 / 98 | 93 / 64 / 99 | M waist:chest 0.74 (V-taper). F waist:hip **0.65**, with a 0.7 target |
| Muscularity / fat / masc-fem | 1.3 / −0.9 / −1.5 | 0.5 / −0.4 / +1.5 | |
| **Lean variant** | Muscularity 0.8, fat −1.2, chest 104, bicep 34 | Muscularity 0.2, fat −0.8, chest 88, hip 94 | |
| **Strong variant** | Muscularity 1.8, fat −0.5, chest 116, bicep 42, across shoulder 43 | Muscularity 1.0, fat 0.0, chest 98, hip 104, thigh 60 | |

### 2.5 Pitfalls

| Pitfall | Cause | Fix |
|---|---|---|
| Soft, rounded face after build | Averaged presets; LOD1 face used too early; low-res source textures | 0.7 base + landmarks; LOD0 within 3 m; request 4K sources |
| Waxy skin | Roughness too low, no accents, no cavity map | Roughness 1.06 to 1.10, accents, check the cavity map is in the build |
| Blurry DCC export | Virtual textures + Substrate in the DCC path ([Epic 5.7 known issues](https://dev.epicgames.com/documentation/metahuman/known-issues-5-7)) | We don't export to a DCC. The note only applies if we do. |
| Looks great in the Creator, bad in game | Creator uses Cinematic / Epic material quality ([forum](https://forums.unrealengine.com/t/created-a-metahuman-in-5-6-looks-great-while-editing-but-horrible-when-exported/2544330)) | Judge the **built** Blueprint in a lit level at game scalability |
| Hair turns to a helmet at distance | Groom LOD 5 to 7 is a "helmet" mesh | Fine past 60 m. Make sure the LOD bias doesn't push it closer. |
| Hair missing in the capture | Strands in SceneCapture | Use a viewport HighResShot to judge |

---

## 3. Outfits on MetaHuman bodies in UE 5.8

### 3.1 Options, ranked for us

| Rank | Route | For | Against | Verdict |
|---|---|---|---|---|
| **1** | **Our own garments → MetaHuman Outfit Asset (resizable) → fitted offline per build → cooked as skinned meshes** | Official 5.6+ path; one garment fits all 6 builds; the cloth asset carries the source body ([Epic: parametric clothing](https://dev.epicgames.com/documentation/unreal-engine/getting-started-with-parametric-clothing?lang=de)); can be scripted | MetaHuman Creator strips the sim mesh when it resizes. Keeping sim needs a **second outfit asset per body from the Resizing Graph template** with *Strip Sim Mesh = false* ([Epic staff, 08-2026](https://forums.unrealengine.com/t/intended-workflow-for-resizing-a-chaos-outfit-asset-while-preserving-cloth-simulation/2762838)) | **Use.** Skinned meshes for all body garments; sim only on capes, fur skirts and tassels |
| 2 | **Mutable Customizable Object** (production-ready in 5.8; the sample drives a full MetaHuman including MetaHuman Cloth) ([Epic news](https://www.unrealengine.com/news/mutable-sample-for-runtime-character-customization-now-updated-for-ue-5-8)) | Real runtime body sliders, removes hidden body under clothes, merges meshes and materials into fewer draws | Steep; DNA is only passed through, so the face can't be reshaped ([forum](https://forums.unrealengine.com/t/mutable-metahuman-morph-target-dna-baking/2657434)) | **Phase 2:** when garments pass ~40 or body sliders become a must |
| 3 | Blender + skin-weight transfer (Data Transfer modifier, or UE's skin-weights tool) | Fast for **rigid armour plates** (pauldrons, helms, bracers) | Every body variant needs its own fit | **Use for hard armour pieces**, rigid-attached to bones |
| 4 | Fab parametric MetaHuman outfits (resizable medieval, for example [PolyWear](https://forums.unrealengine.com/t/polywear-medieval-resizable-parametric-outfit-for-metahuman/2722453)) | Instant | **Raven's repo README forbids Fab pack files in git** (FULL-RELEASE-PLAN, risks 5); not our look | Reference only, or local-only placeholders |
| 5 | Chaos Outfit with runtime sim on everything | Pretty | Cost for 16 players; loses the parametric resize | No |

### 3.2 Survival outfit route (leather, fur, wraps)

1. **Model** in Blender on the MetaHuman **athletic** body of each sex, exported from the built Blueprint. Use **Marvelous** only for the cloth panels: wraps, tunic, skirt. Budget: **12-18k tris per full outfit at LOD0**, 4 LODs at 50% each, one 2K texture set per outfit (BC, N, ORM), with tiling leather and fur detail maps shared by all outfits.
2. **Import as an Outfit Asset** with the source body set, then **resize offline to the other 2 builds per sex** (Resizing Graph). That gives 3 fits per outfit per sex.
3. **Body masking:** remove the body triangles hidden under each garment. In the MetaHuman route this is the wardrobe item's clothing mask (`has_clothing_mask`). In Mutable it's clip-morph or remove. This stops skin poking through and saves vertices.
4. **Sim:** only fur skirt flaps, cape and hair tassels, through a Chaos Cloth component on the local player and players within 15 m; beyond that, frozen.

### 3.3 Armour-set route and per-biome tiers

- **4 visual slots** (head, chest, hands, legs+feet). Each piece is either a resized garment or a rigid plate set, Monster Hunter style.
- **Recolour, don't remodel, for the metal step within a tier.** Copper, bronze and iron share a mesh family with different trims (one MID parameter set), as Valheim's bronze and iron do. Only biome sets get unique meshes.
- **Count to build first (Fire + Frost):** survivor wraps + 2 hide sets + 3 metal families + Forgeplate + Rimeguard = **8 sets × 4 slots × 2 sexes = 64 meshes, then 3 fits each.**

| Biome | T1 hide | Biome set | Silhouette hook | Path-mark windows left open |
|---|---|---|---|---|
| Fire (volcanic) | **Cinderhide**: Cinderfang pelt mantle, charred leather, ash-grey linen | **Forgeplate**: obsidian-black plates, molten seams (emissive 2 to 4 nits), horned helm | Tall back-swept pauldrons | Forearms, neck, face |
| Frost (tundra) | **Snowhide**: white fur hood and collar, hide leggings | **Rimeguard**: pale steel, frosted ice-crystal pauldrons, white fur trim | Big fur collar plus crystal points | Face, throat, fingertips (glove-less option) |

---

## 4. GASP on our C++ character (no Mover, no GASP Blueprint logic)

### 4.1 The minimum set

| Piece | What to build | Notes |
|---|---|---|
| Movement | Keep **`UCharacterMovementComponent`** | Mover is still experimental ([StraySpark](https://www.strayspark.studio/learn/courses/modern-locomotion-ue5/the-end-of-cmc)). Our spec: walk 450 / run 600 / sprint 750, 0.15 s to full speed → `MaxAcceleration` ≈ **5000**, `BrakingDecelerationWalking` ≈ **4000**, `bUseSeparateBrakingFriction` true, `GroundFriction` 8. That is snappier than GASP's ~800-1200, so lean on **orientation and stride warping** to keep feet planted. |
| Schema | `PSS_Wildreach`: trajectory channel (past −0.3 s; future 0.35, 0.7, 1.0 s; position + facing), pose channel (`foot_l`, `foot_r` position + velocity; `pelvis` velocity) | MetaHuman bone names match UE5 Mannequin |
| Databases | **14**: Idle; Starts / Loops / Stops / Pivots for walk, run and sprint; Turn-in-place; Jump-Fall-Land; Crouch | Copy the clips from GASP. Mode **PCAKDTree**, components 8 to 10, pruning threshold on ([Epic: Motion Matching](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)) |
| Chooser | `CHT_Locomotion`: rows by Gait (walk/run/sprint) × Movement state (idle/moving/air) × Stance → database list | Driven by enums on the C++ AnimInstance |
| Trajectory | In `UWRAnimInstance::NativeThreadSafeUpdateAnimation`, build it with `UPoseSearchTrajectoryLibrary` (it fills an `FTransformTrajectory` from the CMC) | Replaces the deprecated Character Trajectory component path. Keep the history buffer on the instance. |
| AnimBP (thin) | **Motion Matching** (Searchable = Chooser output) → **Pose History** (Generate Trajectory on) → **Orientation Warping** → **Stride Warping** → **Slot `UpperBody`** (layered blend from `spine_01`, mesh-space rotation) → **Slot `FullBody`** (rolls, gather, death) → **Foot placement / Leg IK** → Output | All variables come from C++; the graph has no logic |
| Traversal (mantle 1.8 m) | One montage per height band (0.5 / 1.0 / 1.8 m) with **Motion Warping** windows set from C++ traces | GASP's traversal chooser is Blueprint. Copy its idea, not its graph. |

### 4.2 Retargeting

**GASP clips.** The 5.8 sample **animates a hidden UEFN mannequin and runtime-retargets the pose to the MetaHuman** ([forum](https://forums.unrealengine.com/t/has-anyone-figured-out-how-to-add-a-character-to-the-game-animation-sample-project-in-ue-5-8/2836922)). That doubles skeletal evaluation per character, which is fine for a demo but not for 16 players. Instead, **batch-retarget the GASP library offline once per sex** onto our athletic M and F skeletons (IK Retargeter → Export). Set **translation retargeting mode = Skeleton** on all bones except root and pelvis, so the lean and strong builds (±5% limbs) run the same clips. Users report 5.8 wrist and hand rotation errors on MetaHuman ([forum](https://forums.unrealengine.com/t/gameanimationsample-5-8-bad-retargeting-to-metahuman/2837097)), so check the hand chains once in the retargeter.

**Mixamo action clips** (sword and shield, bow, rifle, throw, chop, gather, swim, roll):

| Step | Setting |
|---|---|
| IK Rig (source) | Mixamo FBX skeleton; **retarget root = `mixamorig:Hips`**; auto-create chains (5.6+ recognises Mixamo) |
| IK Rig (target) | The MetaHuman body skeleton's rig (`pelvis` root) |
| Op stack (5.6+) | Pelvis Motion → FK Chains → IK Chains (feet, hands) → **Root Motion op (generate root from pelvis)** → Floor Constraint |
| Root motion | Mixamo has no root bone, so either generate it in the op, or use the plugin route that adds a Root bone ([Mixamo Retargeting 2](https://docs.unrealengine.com/marketplace/ja/product/mixamo-animation-retargeting-2)). Known 5.6.1 trap: root motion shows in the preview but is lost on export, so check `Enable Root Motion` on the exported sequence ([forum](https://forums.unrealengine.com/t/ue-5-6-1-issue-with-losing-root-bone-movement-when-retargetting-mixamo-animations/2651945)). |
| Sockets | `player_art.json` names Mixamo bones. Remap `RightHand` → `hand_r` socket `weapon_r`, and `LeftForeArm` → `lowerarm_l` socket `shield_l`. Add `bow_l`, `tool_r` and `back_weapon` sockets on `spine_05`. |
| Where they play | Attacks, cast, throw, chop and gather on the **UpperBody** slot while moving. Roll, death, swim and hit-react go **FullBody**. Swim stays a separate database later; for v1 it is a slot loop. |

### 4.3 Replication cost

Motion matching runs **locally on every machine**; nothing about it replicates. Remote characters need:

| Data | Size | How |
|---|---|---|
| Position, velocity, rotation | Already replicated by the CMC | |
| **Acceleration (quantised)** | 3 bytes | Lyra's `FLyraReplicatedAcceleration` pattern (XY angle, XY magnitude, Z). Without it, the trajectory of remote characters is built from velocity alone and starts and stops lag. Users report the stock trajectory replicates poorly ([plugin note](https://docs.unrealengine.com/marketplace/ja/product/replicated-character-trajectory-component)). |
| Gait + stance + lock-on | 1 byte | Bitfield |
| Attacks and actions | Existing montage RPCs | Unchanged |

**Total: about +4 bytes per movement update.**

---

## 5. Full-body first person with a MetaHuman

### 5.1 How others do it

| Game | Method |
|---|---|
| **Star Citizen** | One skeleton, one mesh and one animation set for both views. Eyes sit in the same place. A **stabilisation layer cloned from the vestibulo-ocular reflex** (gimbal-like) stops head bob ([GamersNexus](https://gamersnexus.net/gg/2627-sean-tracy-interview-p2-cpu-threads-character-tech-ai), [TweakTown](https://www.tweaktown.com/news/40874/star-citizen-fps-mode-detailed-at-pax-australia-2014/index.html)) |
| **Mirror's Edge** | Camera at the eyes, not the head centre. Camera motion **tied to a distant focal point** so it steadies itself. **Centre dot** against sickness. Wider FOV. A purpose-built first-person rig with spine, neck and shoulders ([GDC Vault](https://gdcvault.com/play/1012171/Creating-First-Person-Movement-for), [MTV](https://www.mtv.com/news/1j3myl/ea-discusses-mirrors-edge-sickness-concerns-lack-of-color-green), [Engadget](https://www.engadget.com/2008-07-17-joystiq-e3-hands-on-mirrors-edge.html)) |
| **Escape from Tarkov** | True first person with a full body and heavy weapon inertia. No public breakdown exists. The common pattern is the camera at head position plus an offset, damped, with the spine solved first ([gamedev.net](https://gamedev.net/forums/topic/669278-ways-to-setup-true-first-person-viewpoint/)) |
| **Dune: Awakening** | **Not full-body first person.** Third person, with first person only on scopes and binoculars ([Steam](https://steamcommunity.com/app/1172710/discussions/0/4638240954399181530)). Don't copy it for this. |

### 5.2 Our setup

| Part | Setting |
|---|---|
| Camera position | Sample `head` after animation each frame. Eye point = head + (0, **9 cm forward**, **6 cm up**). Follow it **1:1 horizontally**. Vertically, use a critically damped spring (half-life **0.05 s**) and clamp the bob to **±2.5 cm**. |
| Camera rotation | **Always the control rotation**, never the head bone's. Head roll, dodge-roll tumble and hit-react never rotate the view. |
| Body follows the view | Aim offset on the spine (pitch ±70°) so the chest, arms and weapon point where you look; the head bone follows the camera, not the other way round |
| Hide the head from the owner | MetaHuman face, hair, brows, lashes and beard are separate components: `SetOwnerNoSee(true)` + `bCastHiddenShadow = true`, so your shadow keeps its head. Body neck stump: scale `neck_02` children to 0 **for the owner only**, or rely on the near clip. |
| Clipping | Near clip **5 cm** (project-wide; default 10). UE First Person Rendering ([Epic](https://dev.epicgames.com/documentation/unreal-engine/first-person-rendering)): on the owner's body, set the material's **First Person Output** gradient to **1 at the hands and forearms, fading to 0 at the chest**. Legs and feet stay in world space so they touch the ground. Camera **First Person Scale ≈ 0.7** (tune). FP FOV off at first (true first person wants matched FOV). Grooms aren't supported in FP rendering, but ours are hidden anyway. Needs Virtual Shadow Maps for ground shadows. |
| FOV | Default **90° horizontal**, slider 75-110 |
| Anti-nausea | Centre dot always on. No camera roll. Dodge-roll in FP: camera translates only (down 30 cm and back up over the roll) with a 0.1 s, 15% vignette. Ladders and mantles keep the camera inside a 15° pitch change. Mount: the spring as above plus a mount-sway cap of ±3 cm. |
| Third person | Same mesh with everything visible. Toggle is instant because both views share the same pose (Star Citizen). |

---

## 6. Multiplayer cost

### 6.1 LODs (Epic's spec)

| LOD | Head verts | Body verts | Hair | Face anim | Use |
|---|---|---|---|---|---|
| 0 | 24,000 | 30,500 | Strands 50k or cards 30k verts | Animated maps | Local player TP, creator, < 3 m |
| 1 | 12,000 | 7,600 | Cards 15k | Animated maps | 3-8 m |
| 2 | 6,000 | 3,350 | Cards 10k | No | 8-25 m |
| 3 | 2,500 | 1,507 | Cards 3k | No | 25-60 m |
| 5-7 | 560 → 130 | | Helmet mesh 500 → 100 | No | > 60 m |

Source: [Epic: LOD specifications](https://dev.epicgames.com/documentation/metahuman/platform-support-and-lod-specifications-for-metahumans). Drive every part with **LODSync** ([Epic](https://dev.epicgames.com/documentation/metahuman/lodsync-component-for-unreal-engine)).

### 6.2 Rules

| Item | Rule |
|---|---|
| Hair in game | **Cards only**: `r.HairStrands.UseCardsInsteadOfStrands 1` in gameplay, strands only in the creator and mirror. Even cards-only grooms sit in VRAM. One team saw ~800 MB for 50 MetaHumans, about 16 MB each, with no streaming pool for cards ([forum](https://forums.unrealengine.com/t/groom-lod-data-remains-fully-resident-in-vram-regardless-of-distance-lod-level/2715458)). Turn **hair sim and RBF interpolation off** on remote players to drop the guide data. Budget: **16 players × ~16 MB = ~260 MB worst case**, less when grooms are shared assets. |
| Face rig | RigLogic face anim only on LOD 0-1. Our hero has no voice, so remote faces need blinks only. **Animated maps off for remote players.** They and material curves were the main cost in a 300-MetaHuman test; keep Material Parameter Caching on ([MetaHuman 5.8 thread](https://forums.unrealengine.com/t/metahuman-5-8-released/2729288)). |
| Textures in game | Face BC/N/SRMF **2K**, body **2K**, cavity 1K, outfits 2K, shared tiling detail maps 1K |
| Animation budget | **2.0 ms total on worker threads for 16 players.** Local player full rate. Remote players: **URO** (update-rate optimisation), motion-matching search throttle **0.1 s** on the MM node, and LOD ≥ 3 drops motion matching for a single walk/run blend fallback (Chooser by LOD). Turn on the **Animation Budget Allocator** for remote players. The per-character cost (~0.1 ms remote) is an **estimate**, to be confirmed in Insights during the cook. |
| Path marks | One MID per character with 2 scalars (main path, sub path) + 2 path IDs = **4 bytes, RepNotify** |
| Appearance | One struct replicated at join with RepNotify: face, build, hair, beard, brows, hair colour (3), eye hue, makeup (2), 4 appearance-slot IDs (2 bytes each) = **~20 bytes once** |

### 6.3 Repo size (must stay lean)

**The lab's own numbers, measured today:**

| Asset | Size |
|---|---|
| `MH_Survivor_M.uasset` / `_F` (editable MetaHuman source, high-res textures inside) | **141 MB each** |
| Built `BP_Survivor_F` (Optimized, HIGH) | **283 MB** (body 117, face 96, grooms 70) |
| Built `BP_Survivor_M` | **260 MB** |
| Biggest single files | Body SRMF VT 59 MB, body normal VT 38 MB, head SRMF 28 MB, long-hair groom 63 MB |
| `/Game/MetaHumans/Common` | 220 MB |

The game repo is already ~755 MB of LFS (FULL-RELEASE-PLAN), and **both heroes at today's settings add ~1.1 GB**. The plan:

| Cut | How | Result |
|---|---|---|
| Keep the editable `MH_*.uasset` sources **out of the game repo** | They live in WildreachCharLab, with the Python scripts as their recipe | −282 MB |
| Build textures at **2K, not VT 4K+** | `desired_texture_sources_resolutions` 2K for in-game builds; 4K only for the creator close-up, if ever | Body ~117 → ~35 MB, face ~96 → ~35 MB per hero (estimate) |
| **Share grooms** | `common_folder_path` in `MetaHumanCharacterEditorBuildParameters`, so all 12 faces share one copy of each of the 10 hair grooms | Grooms paid once |
| Cards-only groom cook | Strip strand data from cooked game grooms; strands stay in the lab | −30-50% per groom (estimate) |
| Faces 3-6 per sex | Same body textures per skin-tone band (4 bands); only the head mesh and face textures differ per face | ~40 MB per extra face |

**Target: ≤ 120 MB per hero for the first 2 faces, ~40 MB per extra face, ~600 MB total for 12 faces + 10 shared grooms + Common.** That is still large for Raven's LFS, so ship faces 1-2 per sex first and add the rest after the LFS question (FULL-RELEASE-PLAN question 1) is settled.

---

## Build order for the cook

1. Fix the two hero faces: one preset as base + landmarks + skin, eye and lash settings (section 2). Confirm grooms in a viewport shot.
2. Retarget GASP once per sex; build `UWRAnimInstance` + the thin AnimBP + Chooser + 14 databases.
3. Retarget the Mixamo clips through the IK Retargeter with the Root Motion op; remap the sockets.
4. Full-body first person camera (section 5.2).
5. Survivor wraps outfit, fitted to 3 builds per sex.
6. Path-mark material layer: Fire veins and Frost rime first.
7. Creator UI (5 tabs) on the prebuilt faces and builds.
8. Cinderhide, Snowhide, the metal families, Forgeplate, Rimeguard.
9. LOD, cards and repo-size pass before any push to Raven's repo.
