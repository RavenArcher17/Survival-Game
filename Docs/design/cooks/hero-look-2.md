# Build lane: build-hero-look-2 (research + plan + build record)

Written 2026-11-14. Follows `cooks/hero.md` (2.2, 2.5, 2.6, 3.1 L4-L10, 3.4), `cooks/HERO-RESEARCH.md`, `cooks/build-hero-anim.md` (how the hero went into the repo) and `cooks/survival-crafting.md` 2.14 (set names, looks, slots). Lab: `C:\Dev\WildreachCharLab`. Game: `C:\Dev\Wildreach`.

---

## 0. Requirement ledger

Every row gets its proof (file:line, asset path or commit) in section 5 when built.

| # | Requirement (single) | Source |
|---|---|---|
| R1 | Face M3 Ren built to its hero.md 2.2 row | brief; hero.md 2.2 |
| R2 | Face M4 Kofi | same |
| R3 | Face M5 Varro | same |
| R4 | Face M6 Obi | same |
| R5 | Face F3 Sora | same |
| R6 | Face F4 Amara | same |
| R7 | Face F5 Mila | same |
| R8 | Face F6 Kesi | same |
| R9 | Faces and bodies attractive, naturally sexy by real-world standards, never sanitized (faces, builds and armour cuts) | brief; CLAUDE.md |
| R10 | Bodies for the new faces (every face has its athletic body; lean/strong per sex) | brief "faces/bodies M3-M6 and F3-F6" |
| R11 | Armour tier 2 **Copperscale** (Head, Chest, Hands, Legs; 6 bodies) | brief; hero.md 2.5 |
| R12 | Armour tier 3 **Bronze Lamellar** | same |
| R13 | Armour tier 4 **Ironmail** (closed helm with crest; hide-helmet toggle shows the face) | same |
| R14 | Element trims on T1-T4: 12 palettes through TrimColor / PathTint | hero.md 2.5 |
| R15 | **Forgeplate** (Fire) | brief; hero.md 2.5; survival-crafting 2.14 |
| R16 | **Stormweave** (Lightning) | same |
| R17 | **Dunewrap** (Sun & sand) | same |
| R18 | **Heartguard** (Nature) | same |
| R19 | **Skysilk** (Wind) | same |
| R20 | **Wardcloth** (Arcane) | same |
| R21 | **Rimeguard** (Frost) | same |
| R22 | **Mirehide** (Blood & curse) | same |
| R23 | **Reefmail** (Tide) | same |
| R24 | **Gravecloth** (Necromancy) | same |
| R25 | **Sporeweave** (Spore) | same |
| R26 | **Hollowplate** (Void) | same |
| R27 | Every set has the four slots Head / Chest / Hands / Legs | brief; survival-crafting 2.14 |
| R28 | Ascendant (tier 6) variant: starsteel inlay 1.5 nits in the path colour + halo-crest | hero.md 2.5, L9 |
| R29 | Creator thumbnails: 256 px portrait per face and per hair | brief; hero.md L10, 2.8 |
| R30 | Migrate to C:\Dev\Wildreach the way build-hero-anim did (built 2K output, no MH sources, no BP actors) | brief |
| R31 | F1 over the size target (hair) fixed | brief (build-hero-look not-finished) |
| R32 | Clothing masks: cloth never pokes through skin | same |
| R33 | Fire veins too pale / too bright: tuned in a lit level | same |
| R34 | Fire's soot-dark nails | same |
| R35 | Path effects on eyes (all 12 paths) | same |
| R36 | Path effects on hair | same |
| R37 | Blood extra: sharper canines (cap mesh on `teeth_upper`, from strength 0.6) | same; hero.md 2.6 |
| R38 | Lightning extras: scar flicker 6 Hz for 0.3 s per cast, hair lift that flickers in combat, static crackle on cast | same; hero.md 2.6 |
| R39 | Only the body mesh goes to the game for lean and strong (no duplicate face) | same |
| R40 | Design with intention: every piece has a job, meets the body and its neighbours, nothing passes through anything | brief (Aaron 11-14) |
| R41 | No placeholders shown as finished; unfinished stays hidden | brief |
| R42 | Multiplayer: the worn look, face, build and path marks show the same on every machine (server authority, replication) | brief |
| R43 | Data-driven: rows in Data/*.json, no numbers buried in code | brief |
| R44 | Full cook: research doc + plan written before building (this file) | brief |
| R45 | Git: pull, add by name, CHANGELOG Unreleased + HANDOFF in the same commit, no tag/release, LFS sizes reported | brief |
| R46 | LANDED line | brief |
| R47 | Gate: compile only | brief |

---

## 1. Research

### 1.1 Skin poking through clothes
| Who | How | What we take |
|---|---|---|
| **MetaHuman 5.6+ Outfit assets** ([Epic](https://dev.epicgames.com/documentation/unreal-engine/testing-and-setup-in-metahuman-creator)) | A *Body Hidden Face Map* painted in body UV (white keep, black hide); assembly deletes those faces. One-way: a new outfit needs a rebuild. 5.8 bug: an all-black mask hides nothing ([forum](https://forums.unrealengine.com/t/fully-hidden-metahuman-body-becomes-visible-after-assembly/2833124)) | Same map idea (body UV, black hide) but applied **at runtime as an opacity mask**, so armour swaps in game. Never ship an all-black mask (the head-neck seam band is always white anyway) |
| **Cyberpunk 2077** ([redmodding](https://wiki.redmodding.org/cyberpunk-2077-modding/modding-guides/items-equipment/influencing-other-items)) | Body sub-meshes hidden per item (PartsOverrides, visual tags); "GarmentSupport" layering stops stacked garments clipping each other | Per-slot masks (4) multiplied, and a fixed layer order of offsets so stacked pieces never cross |
| **UE community practice** ([forum](https://forums.unrealengine.com/t/metahumans-body-mesh-depeding-on-clothes/1661087)) | Opacity mask from verts "within a few mm" of the garment's inner shell; cutting the body per outfit breaks morphs and multiplies assets | Hide only triangles fully inside the piece by ≥ 1.2 cm (our shells sit 0.4-2.6 cm off the skin; a 1.2 cm inset keeps the hem edge covered while the cloth stretches) |

### 1.2 Real armour (numbers we build to)
| Armour | Real numbers | Our build |
|---|---|---|
| Roman **lorica squamata** ([Wikipedia](https://en.wikipedia.org/wiki/Lorica_squamata), [RAT](https://romanarmytalk.com/rat/showthread.php?tid=31235)) | Scales 25 × 37 mm, 22 per row, wired in rows; each row covers the top third of the row below | Copperscale texture: scales 2.5 × 3.7 cm, 1/3 overlap, wire holes at the top, verdigris (#43B3AE) in the crevices and round the wire holes |
| Japanese **kozane** lamellar ([Kyoto NM guide](https://www.kyohaku.go.jp/eng/learn/assets/guides/2025_armor_guide_en.pdf), [sengokudaimyo](https://www.sengokudaimyo.com/katchu-katchuch19)) | Plates 5-8 cm high, 2-4 cm wide, 1-2 mm thick; *kebiki* dense lacing hides the plates, *sugake* sparse vertical pairs | Bronze Lamellar: lamellae 2.5 × 6.5 cm in laced rows, **sugake** lacing (pairs of vertical cords) so the bronze shows; cords take the element trim colour |
| Central-Asian lamellar ([CEEOL](https://ceeol.com/search/article-detail?id=915321)) | 78 × 30 mm, 1-1.5 mm thick, 8 holes, laced to 3 straps | Skirt strips: 3 lacing bands per strip |
| Riveted **mail** 4-in-1 ([Gjermundbu](https://sagy.vikingove.cz/en/the-mail-from-gjermundbu-norway/), [replica](https://burgschneider.com/products/chainmail-hauberk-roundring-riveted-10mm-steel-oiled)) | Rings 7.4-10 mm, wire 1.0-1.6 mm; chausses as small as 5.4 mm | Mail texture: 9 mm rings, 1.5 mm wire, each ring linked to 4; riveted flat spot on every ring |
| Bronze **Corinthian** helm ([British Museum](https://www.britishmuseum.org/collection/object/G_1865-0722-1), [ROM](https://www.rom.on.ca/blogs/weapon-wednesday-nugent-marathon-corinthian-helmet)) | 250 mm tall, one sheet, crown < 1 mm, rim thicker | Bronze helm bowl 0.25 cm walls, rolled rim 0.6 cm, hinged cheek guards; bronze #B08D57, dark #4A3C2A in recesses |
| Plate ([bookandsword](https://www.bookandsword.com/2017/01/07/how-much-does-the-protection-of-low-tech-armour-vary/)) | Breastplates 1.25-7.8 mm (≈3 mm typical); gauntlets and faulds < 1 mm | Breastplate thickness 0.45 cm (reads at game scale), lames 0.3 cm, gauntlet plates 0.2 cm, every edge rolled |
| **Great helm** + crest ([Wikipedia](https://en.wikipedia.org/wiki/Great_helm), [ffoulkes](https://medievalist.globalfolio.net/eng/f/ffoulkes-armour-and-weapons/index0024.php)) | Mature by 1300; crests of boiled leather moulded to shape | Ironmail closed helm: flat-topped bowl, eye slit, breaths on the right; leather crest in the trim colour |

### 1.3 Tiered and biome armour in games
| Game | Lesson | Use |
|---|---|---|
| **Monster Hunter World/Wilds** ([multiplayer.it](https://multiplayer.it/notizie/team-monster-hunter-wilds-parla-design-armi-ognuno-sara-unico-differenza-di-world.html), [Game Informer](https://gameinformer.com/b/features/archive/2017/11/17/monster-hunter-world-bestiary-day-3-the-destructive-titan-nergigante.aspx)) | "Wear Rathalos, look like Rathalos." Nergigante's set was redone for being "too neat": spikes added to break the silhouette | Every biome set carries its land's signature shape (spikes, coral, caps, antlers, halo) as a **silhouette break**, not a texture |
| **Valheim** ([PC Gamer](https://www.pcgamer.com/best-valheim-armor-wolf-iron-troll-bronze)) | Rag → bronze → iron → wolf (fur cape) → padded → carapace: each tier one obvious new material and shape | One new big shape per tier (hero.md rule) |
| **Enshrouded** ([allthings.how](https://allthings.how/enshrouded-1-0-craftable-armor-sets-and-the-flame-level-that-unlocks-each-one/)) | 6 tiers × warrior/ranger/mage sets + a cosmetic slot | Appearance layer per slot (hero.md 2.5); the runtime keys looks by id so transmog plugs in |
| **Horizon Forbidden West** ([PS Blog](https://blog.playstation.com/?p=354177)) | "Grounded fashion construction": every piece has a reason to be built that way | Each piece's job is written in 3.5 (why, what it meets) |

### 1.4 Path marks on eyes, hair, teeth
| Game | How | Use |
|---|---|---|
| **The Witcher 3** toxicity ([Game Informer](https://gameinformer.com/games/the_witcher_3_wild_hunt/b/xboxone/archive/2015/04/14/geralt-s-face-acts-as-a-toxicity-monitor-in-the-witcher-3-wild-hunt.aspx)) | Pale skin, dark veins, dark under-eyes, all scaled by one meter, from one shader | Our one skin layer scaled by path strength (built), now plus eyes and nails |
| **Elden Ring** Frenzied Flame ([wiki](https://eldenring.wiki.gg/wiki/Endings)) | Glowing yellow eyes: a small mark that reads instantly | The iris/pupil ring is every path's first, smallest mark |
| **Skyrim** vampires ([Nexus](https://nexusmods.com/skyrim/mods/54917)) | Fangs are morphs on the teeth; eyes a separate glowing mesh/texture | MetaHuman teeth can't take new morphs headless; Epic's own route is sculpted canines as separate geometry ([Epic](https://dev.epicgames.com/documentation/metahuman/metahuman-stylized-character-character-geometry-and-rig-creation)) → our 2-piece cap mesh on `teeth_upper` (hero.md 2.6) |

### 1.5 Thumbnails and portrait light
- Loop lighting: key **30-45°** to the side, nose and cheek shadows don't touch ([glass.photo](https://glass.photo/photography-terms/loop-lighting)); Rembrandt 45-60° ([Lens Lounge](https://thelenslounge.com/rembrandt-lighting-setup/)). hero.md's creator rig (key 35° up, 40° side, rim, fill 25%) is loop lighting: the thumbnails use the **same rig** so a tile matches the turntable (MH Wilds players complained creator light ≠ game light: [exitlag](https://www.exitlag.com/blog/monster-hunter-wilds-character-creation)).
- BG3 picks heads as presets with hair as attachments ([Kotaku](https://kotaku.com/baldurs-gate-3-character-creator-races-genitals-options-1850653066)): one tile per face, one per hair.

### 1.6 Glowing veins that read
- UE emissive is in nits under physical units, and emissive ignores exposure: the same value is invisible at noon and blinding at night ([Epic](https://dev.epicgames.com/documentation/unreal-engine/using-physical-lighting-units-in-unreal-engine), [forum](https://forums.unrealengine.com/t/4-19-physical-lights/105121)). Scene average L = 2^(EV100−3) nits ([scantips](https://www.scantips.com/lights/math.html)): night ≈ 2, interior ≈ 64, daylight ≈ 4000.
- **Fix for "too pale/bright":** the vein glow is made **exposure-relative** (the material's `EyeAdaptationInverse` node): veins sit a fixed ~1.5 stops above mid-grey after exposure in every light, plus a dark ember albedo at the vein core so they read red-on-skin by day instead of washing pale.

---

## 2. What was found in the lab and repo (facts the plan rests on)

| Fact | Consequence |
|---|---|
| MetaHuman Character editing, texture synthesis, cloud auto-rig and build all run headless in `UnrealEditor-Cmd -run=pythonscript` (probe 2026-11-14 05:25: subsystem, 79 landmarks, `can_build` true) | All faces are built with no editor window. `Scripts/hero2/hl.sh` runs one step per process and captures prints to `logs/*.out` (the commandlet drops Python stdout) |
| The repo LFS tree is already **1,050 MB** (`git lfs ls-files -s`, 2026-11-14). The hero is 259 MB of it; 95 MB of that is two groom assets carrying full strand data (F1 `Hair_M_Layered` 63 MB, M1 `Hair_S_Messy` 19 MB) | Every face's grooms point at the **engine plugin's own groom assets** (`/MetaHumanCharacter/Optional/Grooms/...`, shipped with UE 5.8, already enabled in the repo). Only the small per-face bindings (6 KB) and the face mesh/textures go in LFS. This removes F1's 63 MB hair from the tree (R31) and makes every extra face ~55 MB instead of ~120 MB |
| Each built hero = Body (mesh 11.7 MB + DNA 4.6 + baked 2K textures 16 MB), Face (mesh 7.3 + DNA 5.5 + baked textures 39 MB), Grooms | A face migrates as Face + its athletic Body + bindings. Lean/strong migrate as **Body mesh + DNA + physics only** and reuse the face's body material at runtime (R39) |
| All bodies share one skeleton and one topology/UV layout per sex | One body-hide mask per look, slot and sex serves all three builds (R32) |
| The outfit generator (`hero_outfit.py`) builds from each body's surface; pieces are single-sided shells with no thickness | Armour needs: thickness and rolled edges on hard plates, rigid (single-bone) weights on plates so they don't bend, head-envelope helms, and new shape builders (below) |
| No metal / stone / coral / bark tiling textures exist in the lab or repo | Tiling textures are authored in Python (numpy height fields → albedo + normal), 1K, like the existing linen/leather/fur/rope set |
| The game draws one face per sex, Survivor Wraps only; worn armour doesn't change the look; path marks aren't driven (build-hero-anim not finished) | This lane adds the runtime: face/build appearance (replicated), worn-armour look (replicated), body-hide masks, path-mark MIDs on skin/eyes/hair, canines, lightning pulse |

---

## 3. Design

### 3.1 Faces M3-M6, F3-F6 (SPEC rows from hero.md 2.2, built as written)
Base preset ≥ 0.75, second preset 0.2-0.25, landmark moves ≤ 2 mm mirrored (iterated to the result, as M1/M2/F1/F2), skin roughness per row, eyes (limbal 0.78, softness 0.06, saturation 1.2, vascularity 0.10), teeth clean, head scale F 0.96 / M 1.00. The rows are now data in `Scripts/hero/hero_spec.json`.

Gaps the cook left, decided here:
| Face | Hair colour (swatch) | Why | Makeup (F) |
|---|---|---|---|
| M3 Ren | Black 0.95/0.05 | East-Asian base (Kelvin); slick-back reads as glossy black | – |
| M4 Kofi | Black | Dark tone band; coils | – |
| M5 Varro | Dark brown 0.80/0.15 | Olive Roman look; buzz cut + full beard match | – |
| M6 Obi | Black | Deep tone band; twisted braids | – |
| F3 Sora | Black | Tuya base; low ponytail | Natural lips 0.35, thin liner, high blush 0.12 |
| F4 Amara | Black | Deep; twisted braids | Natural lips 0.40 rough 0.40, thin liner, blush 0.12 |
| F5 Mila | **Copper** 0.35/0.85 | Fair + freckles: the classic red-head read, and the cast has no redhead yet (F1 chestnut) | Natural 0.35, thin liner, blush 0.12 |
| F6 Kesi | Dark brown 0.80/0.15 | Dark brown band; updo braids show the neck and shoulders | Natural 0.40, soft smokey, blush 0.12 |

Landmark names the cook used that the 79-point map doesn't have are resolved to the nearest named points: "lips out" = `mouth_corner` + `lower_lip_lat` out; "nasal dorsum" = `dorsum` + `nasion` fwd; "brow ridge" = `brow_head/mid/tail` fwd (as M1/M2 did).

### 3.2 Bodies
Each face is built on its sex's **athletic** body (the anim lane's retarget source). Lean and strong stay one body each per sex (built by the first lane), and only their body mesh migrates (R39). At runtime the face's own body material (its skin tone) is put on the lean/strong mesh: same UV layout, so the neck seam matches.

### 3.3 Armour: how the shapes are made (all tiers)
**Layers** (offset off the skin, cm): skin-tight cloth 0.4-0.8 · leather 0.7-1.0 · mail 1.0-1.3 · scale/lamellar 1.4-1.8 · plates 1.8-2.6 (+ 0.35-0.5 thickness) · fur and outer coats 2.5+. A piece never sits inside the layer below it, so nothing passes through anything.

**Builders** (`Scripts/hero2/hero_armor.py`, on top of the first lane's `shell/tube/loop_roll/limb_ring`):
| Builder | Makes | Detail |
|---|---|---|
| `shell` + `thicken` | Cuirass, greaves, vambraces, gauntlet plates, gorget | Back face + side walls; a rolled rim (vertex colour R = trim) on every free edge |
| `lames` | Pauldrons, couters, poleyns | 3-4 overlapping bands of an ellipsoid round the joint, each 1 cm under the one above (the real spaulder order: upper lame on top so blows slide down), rim rolled, rivet heads at each lame end |
| `helm` | Caps, open helms with cheek guards, closed helms | Built from the **envelope of all six faces' heads of that sex** (+1.2 cm clearance), so one helm fits every face. Rigid to `head` |
| `strips` | Lamellar skirt, tassets, leaf flaps, kelp tassels, moss fringes, mycelium strands, veil tails | Hanging strips with thickness, staggered lengths, each skinned to the thigh under it |
| `coat` / `cape` | Long coats (Wardcloth, Mirehide, Gravecloth), half-capes, fur cloaks | Tube from the waist or shoulders, open at the front or covering the back only, flared; weights from the legs under them |
| `collar` | Standing collars, fur collars, gorgets | Swept profile round the neck base, standing up and out |
| `spikes` / `crystals` / `branches` | Fulgurite spikes, crystal points, Rimeclaw spines, coral branches, horns, antlers | Tapered faceted prisms or swept tubes; each rooted on a plate (its base sunk 0.5 cm into the plate it grows from) |
| `caps` | Mushroom-cap pauldrons with gills under them | A dome with radial gill fins underneath |
| `orbit` | Levistone shards, rubble halo, Ascendant halo-crest | Rigid to `spine_05`; vertex colour A = bob weight, the material bobs them (sine, 0.5 Hz, 1.2 cm) |

**Weights.** Cloth and leather: closest-point transfer from the body (they stretch with skin). Plates, lames, helms, spikes: every vertex of a part gets the **single** bone its centre is nearest (`set_vertex_bone_weights`), so plates move rigid like metal.

**Material** `M_WR_Armor` (new master): BaseTex (RGB albedo, A = crevice/pattern mask), NormalTex, Tint, Tile, Roughness (+ crevice roughness), Metallic, TrimColor (vertex R), PatternTrim (A mask → trim colour: lamellar cords, stitching), PathTint × PathGlow (vertex G seams), SeamPulse (combat pulse), InlayColor × InlayNits (vertex B: Ascendant starsteel), Bob (vertex A WPO). Two-sided for cloth MIs only.

**Tiling textures** (1K, authored as height fields → albedo + normal, `Scripts/hero2/gen_armor_tex.py`): CopperScale, BronzeLamellar, Mail, HammeredIron, Obsidian, Bark, Coral, Crystal, Silk, QuiltedWool, Bone, Mycelium, VoidGlass, Shroud.

### 3.4 Armour: the tiers (hero.md 2.5 SPEC rows, built with the details below)
| Tier | Head | Chest | Hands | Legs | Big shape | Skin shown |
|---|---|---|---|---|---|---|
| T2 **Copperscale** | Leather cap (shell over the head envelope crown, 0.8 cm) with a 3 cm copper brow band, riveted at the temples | Hide vest (T1 cut, closed V) under a copper-scale breast panel (scales 3.5 × 4.5 cm, 30% overlap) and a **left** shoulder of 3 scale lames; right arm bare | Leather bracers with 2 rows of copper studs | Leggings, rigid copper knee guards (2 lames), hide boots | Shoulders 8 cm wider on the left | ~20%: right arm, both upper arms, throat |
| T3 **Bronze Lamellar** | Open bronze helm: domed bowl, brow ridge, hinged cheek guards to the jaw line, short neck flare; face open | Lamellar cuirass (lamellae 2 × 6 cm in 7 laced rows, cord in the trim colour) with lamellar pauldrons (4 lames each) | Leather gloves, bronze cuffs (thickened) | Lamellar skirt (2 rows of strips, split front/back/sides) over dark trousers; bronze greaves front | Skirt + pauldrons | ~10%: face, throat, upper arms between pauldron and glove |
| T4 **Ironmail** | Closed iron great-helm (envelope +1.6 cm; eye slit 1.2 cm, breaths right side) with a crest in the trim colour; hide-helmet toggle shows the face | Mail hauberk to mid-thigh, plate breast (thickened, 0.5 cm, medial ridge), pauldrons 4 lames, tabard front + back in the trim colour | Gauntlets: plate cuff + 4 finger lames, mail under | Mail chausses, plate greaves and poleyns, sabatons | Full block, shoulders +15 cm, crest | <5% |

Element trims (R14): every T1-T4 MI takes `TrimColor` and `PathTint` from the worn item's element (12 palettes, hero.md 2.5); default = the look's own trim. Hide-helmet toggle and "head always shown at T0" are data flags.

**Hair under helms and hoods:** a look's Head slot carries `hidesHair` (helms, cowls, raised hoods). The game hides the Hair groom while it's worn; brows and lashes stay.

### 3.5 The 12 biome sets (hero.md 2.5 + survival-crafting 2.14, merged)
Silhouette hook = what reads at 30 m. Open windows show the path marks. Steel underplate everywhere (×2.2 armour).

| Set | Head | Chest | Hands | Legs | Hook | Palette / material | Windows |
|---|---|---|---|---|---|---|---|
| **Forgeplate** | Horned blackened helm: open face, two horns swept back 28 cm (rooted in riveted horn sockets) | Blackened plate with basalt inlays, **tall back-swept pauldrons** (4 lames, top lame rising 14 cm), open-throated gorget, ember seams 2-4 nits that pulse in combat | Plate vambraces over bare forearms? no: vambrace stops mid-forearm, forearms open below the elbow per spec → short couters + gauntlets | Scorched leather skirt (strips), plate greaves | Pauldrons + horns | Black #121010, iron grey, ember #E2541A seams; Obsidian + HammeredIron | Forearms, neck, face |
| **Stormweave** | Wind goggles on a leather hood, pushed up on the brow | Quilted oilcloth coat (open, to the hip) over a fitted vest, copper wire stitched in lightning lines, fur collar, **fulgurite spikes rising from the shoulders** (7 per side, 10-24 cm) | Fingerless copper-wired gloves | Fitted trousers, wrapped boots, long tattered half-cape (back, to the calf) | Shoulder glass crown + half-cape | Slate #3B4250, storm blue, copper, violet-white glints | Forearms, face |
| **Dunewrap** | Gold circlet with a veil draped back (lowered), wide hood down | Cropped linen vest, layered wraps, sunstone disc on the chest, bare shoulders; F: bare midriff | Linen hand wraps, gold arm cuffs on bare upper arms | Wrap trousers gathered at the ankle, sunstone-studded belt, sash tails | Wide hood + veil tails, chest disc | Sand #D8C49A, white, sun gold #E8B230, amber glow at night | Arms to the shoulder, face; F midriff |
| **Heartguard** | **Antler crown** (two branching antlers on a bark circlet) | Lacquered heartwood bark plates with amber cores over green leathers, **layered leaf-cut bark pauldrons** (3 lames) | Leather gloves, bark vambrace plates | Leaf-strip skirt flaps, bark greaves, braided cords | Antlers + layered pauldrons | Moss #5E8C31, amber, bark brown; Bark + Leather | Forearms, throat |
| **Skysilk** | Silver circlet, hair free | Pale fitted silk vest, feathered half-cape over one shoulder, long silk scarf, **levistone shards orbiting the shoulders** (5, bob 1.2 cm at 0.5 Hz) | Silk wraps, silver clasps | Fitted silk trousers, soft boots | Floating shards + scarf | White, sky blue #9FD6CF, silver; Silk + Crystal | Arms, collarbones, face |
| **Wardcloth** | High collar with crystal points (worn on the Head slot) | Deep violet long coat (to the calf, open) over a fitted bodice/doublet, rune stitching (trim), crystal studs | Rune-stitched gloves, crystal knuckle studs | Fitted trousers, tall boots | Collar points + coat tails | Violet #7A3FD1, midnight blue, crystal white glow | Forearms (runes), face |
| **Rimeguard** | Pale steel open helm with ice-crystal crest | Scaled pale steel, **huge white fur collar with crystal points**, Rimeclaw spines on the shoulders, fur cloak | Pale steel gauntlets; glove-less option = fingertips bare (fingerless) | Steel greaves, fur-topped boots, frost-blue lacing | Fur collar + spines | Blue-white, white fur, ice blue #8CC8F0; HammeredIron + Fur + Crystal | Face, throat, fingertips |
| **Mirehide** | Hood (raised) with a croc-hide half-mask | Glossy dark croc-hide long coat (to the knee), **spiked collar**, red-lacquered bone buckles, moss fringes at the shoulders | Croc-hide gloves | Sealed waders (thigh-high), coat tails | Hood + spiked collar | Black-green #1C2418, blood red #8E0F1E, bone | Neck (veins), forearms |
| **Reefmail** | Coral-crowned open helm | Coral plate grown over steel, pearl inlay, **coral pauldrons branching up past the head** (branches 20-35 cm) | Sharkskin gloves, coral knuckles | Scale-mail skirt, kelp-strip tassels, sharkskin leggings | Coral branches | Coral #E0674A, sea teal #1E8C86, pearl; Coral + Mail | Neck (gill lines), arms |
| **Gravecloth** | **Tall cowl** that shadows the eyes | High-collared black coat, bone-ribbed chest plate, soulstone clasps (glow 0.5 nits), silver thread | Grey wraps, bone rings | Shroud skirt (ragged strips), fitted trousers | Cowl + bone ribs | Ash grey, black, bone, pale-green soul glow | Face, hands |
| **Sporeweave** | Half-mask (filter) and a cap hood | Living mycelium cloth over leather, **mushroom-cap pauldrons** with glowing gills under them, seams glow teal | Leather gloves, mycelium wraps | Leather leggings, hanging mycelium strands | Cap pauldrons | Umber #4A3426, cyan #39E6C2 glow, cream; Mycelium + Leather | Face, forearms, freckle zones |
| **Hollowplate** | Jagged black-glass open helm | Voidglass plates in black iron, **jagged asymmetric right pauldron**, **floating rubble halo behind the back** (9 stones) | Black iron gauntlets | Voidglass greaves, grey leather | Halo + asymmetric pauldron | Black glass, grey, violet #2B1840 edge light; VoidGlass + HammeredIron | Face, forearms |

(Forgeplate's forearms open: vambraces end at the elbow; gauntlets are short cuffs, so the forearm between shows the Fire veins.)

**Ascendant (R28):** every biome set gets an Ascendant MI set (InlayNits 1.5 in the path colour on vertex-B inlay lines) and a **halo-crest** piece on the helm (thin starsteel ring 18 cm behind the head, rigid to `head`).

**Hide-helmet toggle, cut and appeal (R9):** every set keeps the face, throat and at least one arm zone open (hero.md "Face, neck and forearms left open"); women's sets take a fitted waist and open throat; nothing is boxed or shapeless.

### 3.6 Clothing masks (R32)
- For every look, slot and sex, a **512 px body-hide mask** in the body UV layout: black where an opaque piece of that slot sits over the skin, inset 1.2 cm from the piece's edge (so skin never shows a gap at the hem), white elsewhere. Built from the same selection fields that made the shells: a body triangle is hidden when all three corners are inside the field by ≥ 1.2 cm and the piece is opaque (not a net, not a strip skirt).
- Union over the three builds (same UVs).
- `M_WR_Skin` gets 4 mask params (`HideHead/Chest/Hands/Legs`), opacity mask = their product, clip 0.5. Masked skin costs one extra sample ×4 on the body only (not the face).
- The game sets the 4 masks from the worn look's slots on the body MID.

### 3.7 Path effects (R33-R38)
| Item | Plan |
|---|---|
| Fire veins (R33) | NitScale 30 → **8**, Fire emissive 3 → **2.2 nits**, albedo mix 0.35 → **0.55** with a darker ember base (#8A2A0C at the vein core): veins read as glowing red under the skin, not pale. Tuned in a lit level render (lab, art output) |
| Soot nails (R34) | Body + head mask channel A (extremities) × `nailDark` 0.6: albedo × (1 − 0.75 × nailDark), roughness +0.2. Path rows get `nailDark` (Fire 0.6, Void 0.9 black fingertips, Necro grey) |
| Eyes (R35) | Eye MIDs: iris tint, limbal ring colour, pupil ring glow and pupil scale from the path rows (`eye` block). Main path drives the iris, sub path the limbal ring at 50% (hero.md 2.6) |
| Hair (R36) | Hair-card MID params from the `hair` block: tip lightness (Sun), rime-white tips (Frost), mossy tips (Spore), desaturation (Necro), wet roughness (Tide); WPO stir (Wind) and lift with flicker (Lightning) |
| Canines (R37) | 2-piece cap mesh (200 tris) on the face's upper canines, rigid to `teeth_upper`; shown from Blood strength 0.6 |
| Lightning (R38) | On each ability cast: `Pulse` 1 → 0 over 0.3 s (scar flicker 6 Hz), hair lift flicker, static crackle sound row |

### 3.8 Thumbnails (R29)
256 × 256 px. Face tiles: head-and-shoulders, 3/4 view (turned 25° to camera-left), camera FOV 20 at 45 cm (the creator's Face preset), loop lighting: key 35° up / 40° side, rim behind, fill 25% (hero.md 2.8 rig); background a dark warm gradient (#1A1714 → #2A241E) so skin and hair read. Hair tiles: the same framing on the hair's default face of that sex, back 3/4 for long styles.

### 3.9 Game runtime (C:\Dev\Wildreach)
| Piece | Plan |
|---|---|
| `Data/player_art.json` | `hero.<sex>.faces[]` (6: id, name, face, body, bodyMat, grooms, defaults), `hero.<sex>.builds` (lean/athletic/strong body meshes), `looks.<lookId>` (tier, slots → per sex-build mesh paths, `hideMask` per slot, `hidesHair`), `elements` (12 trim colours), `paths` (12 rows for skin/eyes/hair) |
| `Data/items.json` | Armour items get `"look"` (`hide_armor` → hidewear, `copper_armor` → copperscale, `bronze_armor` → bronze, `iron_armor` → ironmail). The biome set items arrive with the crafting cook; their looks are ready by id |
| `AWRCharacter` | Replicated `HeroFace` (uint8), `HeroBuild` (uint8), `WornLook` (FName); server sets `WornLook` from the equipped armour; OnRep rebuilds the face/body/outfit; body-hide masks; hair hidden under helms; path MIDs from `WRPathComponent` (replicated already); Blood canines; Lightning pulse on cast |
| `?Look=` | Face and build from the join URL (`?Look=<face><build>`), clamped by the server, stored on the PlayerState (the creator lane writes it) |

---

## 4. Build steps
1. Faces M3-F6: `mh_face.py` → `mh_hero_build.py` (tex + rig) → `hero_rebuild.py` (2K, build) → `hero_tex2k.py` (2K cap).
2. Tiling textures (`gen_armor_tex.py`) → `M_WR_Armor` + MIs (`hero_armor_mat.py`).
3. Armour (`hero_armor.py`): T2, T3, T4, then the 12 sets, all 6 bodies, 4 slots, 4 LODs; Ascendant MIs + halo-crest.
4. Body-hide masks (`hero_masks.py` dump → `gen_hide_masks.py` raster → import) + `M_WR_Skin` opacity.
5. Path material: Fire tune, soot nails, eye/hair params, canine mesh.
6. Thumbnails.
7. Migrate (faces, bodies, looks, masks, materials, thumbnails); grooms re-pointed at the plugin; F1's 63 MB copy removed.
8. Game C++ + data; compile; spot-check against this plan; CHANGELOG + HANDOFF; commit; push.

---

## 4b. What changed from the plan while building (and why)
| Plan | Built as | Why |
|---|---|---|
| Faces built in commandlets | Sculpt, skin, texture synthesis and cloud auto-rig run in `UnrealEditor-Cmd -run=pythonscript`; the **build** step runs in the editor engine offscreen (`UnrealEditor -ExecutePythonScript -RenderOffscreen -unattended`, no window) | The MetaHuman build bakes through the Texture Graph engine, which UE 5.8 blocks in commandlets (`TextureGraphEngine::BlockCommandlets`) and crashes when forced (`TextureGraph.AllowCommandlets=1`). Every commandlet build "succeeded" with missing iris/groom bakes, so those runs were thrown away |
| Garments cut from the body's LOD0 surface, 4 LODs | Cut from **LOD1**, 3 asset LODs | The first lane's outfits were 46 MB per family for 6 bodies (legs 5 MB each). LOD1 halves the vertices with the same silhouette; 15 more families at LOD0 would have been ~700 MB of LFS |
| Cinderhide / Snowhide as mesh copies | One Hidewear mesh set + slot-name material swaps in `player_art.json` (`looks.cinderhide.materials`), picked by the armour's element (`armorLooks.hide_armor.byElement`) | Same look, no 92 MB of duplicate meshes |
| Per-face groom copies | The engine plugin's grooms + a 6 KB binding per face per option (`Hero/Grooms/<face>/`) | LFS (R31); also gives the creator every hair on every face |
| Hair lift/stir | `M_WR_HairCards` (the MetaHuman cards material + WPO from the strand coordinate) | The engine hair material has no motion input |

## 5. Build record and ledger proof
Commits: **9e9ed3c** (runtime, data, armour, materials, masks, bindings, builds) and **f8c63cb** (faces 2-6). Compiled
clean (no warnings). Not packaged, not played (Aaron tests).

| # | Done? | Proof |
|---|---|---|
| R1-R8 | Done | `Content/.../Hero/Unpacked/BP_Hero_{M3..M6,F3..F6}/Face` (f8c63cb); rows in lab `Scripts/hero/hero_spec.json`; `player_art.json hero.<sex>.faces` |
| R9 | Done as designed | spec rows (base ≥ 0.75, landmark moves), open throats / V cuts / bare arms in `hero_armor.py` (`vgap_fn`, `light_top`, Dunewrap crop) |
| R10 | Done | every face on the athletic body (`Scripts/hero2/athletic.py`); lean/strong meshes `BP_Hero_*_Lean/Strong/Body` |
| R11-R13 | Done | `Outfits/{Copperscale,Bronze,Ironmail}` × 6 bodies × 4 slots; looks `copperscale/bronze/ironmail` in `armorLooks` |
| R14 | Done | `player_art.json elements`; `WRHeroLook.cpp:168` ApplyOutfit sets TrimColor/PathTint |
| R15-R26 | Built, mapped | `Outfits/<Set>` × 6 bodies; `armorLooks.<set>_armor`. The steel set **items** come with the crafting cook (later-cooks) |
| R27 | Done | Head/Chest/Hands/Legs per set (`hero_armor.py` LOOKS) |
| R28 | Done | `Outfits/Ascendant` crest × 6; `WRHeroLook.cpp:285`; `armorLooks.<set>_ascendant_armor` |
| R29 | **Not finished** | Tile renderer `Scripts/hero2/hero_thumbs.py` exists; offscreen editor renders skin grey and asserts on async-loaded BPs. No tiles shipped (later-cooks) |
| R30 | Done | `Scripts/hero2/migrate.sh` (closure of what `player_art.json` loads, no MH sources/BPs); 9e9ed3c + f8c63cb |
| R31 | Done | groom copies removed (9e9ed3c deletes `BP_Hero_M1/F1/Grooms`), plugin grooms + bindings `Hero/Grooms/<face>` |
| R32 | Done | 104 masks `Outfits/Masks`; `M_WR_Skin` masked; `WRHeroLook.cpp:308` |
| R33 | Done in material, not tuned by eye | exposure-relative glow + ember albedo (`mh_pathmat2.py`, `paths.fire.skin`); no lit-level render was made (no test renders) |
| R34 | Done | `NailDark` × body mask A (`mh_pathmat2.py`), `paths.fire.skin.NailDark 0.6` |
| R35 | Done | `paths.*.eye / eyeSub`; `WRHeroLook.cpp:555` Blend |
| R36 | Done | `paths.*.hair`; `M_WR_HairCards` (lift/stir); `WRCharacter.cpp:602` card material |
| R37 | Done | `Materials/SM_Canines`; `WRHeroLook.cpp:692` |
| R38 | Done | flicker 6 Hz via `Pulse` (0.3 s), hair `PathLift` flicker, `path.lightning.crackle` (3 OGG); `WRAbilities.cpp:163`, `WRHeroLook.cpp:497` |
| R39 | Done | lean/strong carry body mesh + DNA + physics + a 10 KB MI pointing at the athletic skin |
| R40 | Done as designed | layer offsets, rigid bones per plate, lames lap downward, helms from the head envelope (3.3) |
| R41 | Done | creator-only parts hidden (no UI); steel items not added |
| R42 | Done | `FWRHeroLook`/`FWRPathMarks` replicated (`WRCharacter.h`), server sets (`WRHeroLook.cpp:75,114,521`) |
| R43 | Done | all looks/paths/elements/masks in `Data/player_art.json` |
| R44 | Done | this file |
| R45 | Done | CHANGELOG + HANDOFF in 9e9ed3c; added by name; no tag |
| R46 | Done | LANDED.md |
| R47 | Done | Build.bat WildreachEditor: Succeeded |

LFS added: 676 MB (9e9ed3c) + 702 MB (f8c63cb) uploaded.
