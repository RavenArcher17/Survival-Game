# Wildreach cook: the hero

Lane: hero. Written 2026-11-13. Scope: the player hero. That covers the MetaHuman male and female, GASP motion matching, outfits, armour tiers, path body changes, first person, the creator, and the LOD, texture and LFS budget. The output is the design plus two build lanes: **build-hero-look** (lab) and **build-hero-anim** (game repo).

**Read with:** `cooks/HERO-RESEARCH.md` (the research is done; this file follows it), `FULL-GAME.md` sections 2, 4.2, 6.3 and 9, and `cooks/combat.md` (montage rows, weapon classes, trace notifies, two-handing).

Tags: **SPEC** = BUILT or RAVEN-PLANNED in FULL-GAME, kept as written. **GAP-COOKED** = the FULL-GAME first draft, replaced here with the full design.

---

## 1. Research (short; full version in HERO-RESEARCH.md)

| Topic | What the best games do | What we take |
|---|---|---|
| Creator | Valheim is tiny and fast. Elden Ring and Once Human use presets plus tabs. Once Human made re-edits free after players hated paying ([PlayNews](https://www.playnews.gg/en/guides/once-human-character-customization-and-hair-dyeing-become-free-on-september-28-retroactive-3-month-refund)) | 5 tabs, all presets, no ugly option, free re-edit at a Mirror |
| Start outfit | Conan and the Elden Ring Wretch start near naked; Valheim starts in rags | Survivor wraps with about 40% skin showing |
| Tier silhouettes | ARK (cloth → hide → chitin → flak) and Monster Hunter (the set is the monster) read at distance ([GamesRadar](https://gamesradar.com/games/monster-hunter/monster-hunter-wilds-armor-appearance-layered-transmog)) | One new big shape per tier; tier and biome read at 30 m |
| Transmog | MH Wilds layered armour; Enshrouded cosmetic slot ([PinDrop](https://pindrop.gg/enshrouded/armor/category/cosmetic-armor-set)) | Appearance layer per slot, plus a hide-helmet toggle |
| Body change | Elden Ring Frenzied Flame eyes: a small mark that reads instantly | Every path changes the eyes **and** the skin |
| Faces | Averaging presets loses structure (Rhodes 2006, *Annu. Rev. Psychol.* 57:199). MetaHuman sculpts project back onto real scans ([Epic Head Sculpt](https://dev.epicgames.com/documentation/metahuman/head-sculpt-controls)) | One preset at ≥ 0.75, then landmark moves of 2 mm or less |
| Outfits on MetaHuman | 5.6+ resizable Outfit Asset ([Epic](https://dev.epicgames.com/documentation/unreal-engine/getting-started-with-parametric-clothing)); Mutable for phase 2 ([Epic news](https://www.unrealengine.com/news/mutable-sample-for-runtime-character-customization-now-updated-for-ue-5-8)) | Our garments → Outfit Asset → resized offline to 3 builds → skinned meshes |
| Locomotion | GASP motion matching ([Epic MM](https://dev.epicgames.com/documentation/unreal-engine/motion-matching-in-unreal-engine)); the 5.8 sample runtime-retargets a hidden mannequin ([forum](https://forums.unrealengine.com/t/has-anyone-figured-out-how-to-add-a-character-to-the-game-animation-sample-project-in-ue-5-8/2836922)) | Keep the CMC. Retarget offline once per sex. Use a C++ AnimInstance with a thin AnimBP and a Chooser |
| First person | Star Citizen uses one mesh plus VOR-style stabilisation. Mirror's Edge uses a centre dot and eye-point camera ([GDC](https://gdcvault.com/play/1012171/Creating-First-Person-Movement-for)). UE First Person Rendering ([Epic](https://dev.epicgames.com/documentation/unreal-engine/first-person-rendering)) | Camera position from the head bone, rotation from the control rotation, arms-only FP gradient |
| Cost | Epic LOD spec ([Epic](https://dev.epicgames.com/documentation/metahuman/platform-support-and-lod-specifications-for-metahumans)); groom cards stay resident in VRAM ([forum](https://forums.unrealengine.com/t/groom-lod-data-remains-fully-resident-in-vram-regardless-of-distance-lod-level/2715458)) | Cards in game, 2K textures, ≤ 120 MB per hero |

---

## 2. Design

### 2.1 The hero (SPEC)

| Item | Tag | Source |
|---|---|---|
| MetaHuman male and female, GASP motion matching; the Mixamo survivor is scrapped | SPEC | FULL-GAME 9 (Aaron 11-13) |
| Name is free text. No voice lines and no story of their own | SPEC | FULL-GAME 9 |
| Walk 450, run 600, sprint 750 cm/s; 0.15 s to full speed; jump 1.2 m; mantle up to 1.8 m (0.5 s); slopes to 50°; no fall damage under 6 m, lethal over 18 m | SPEC | FULL-GAME 9 |
| Swim and ride (built); glide via Wind (planned) | SPEC | FULL-GAME 9 |
| Body changes with path: Fire glowing veins, Necromancy pale skin | SPEC | DD Classes |
| Every screen works by mouse and pad; F1 opens the controls page | SPEC | FULL-GAME 9 |

### 2.2 Faces (GAP-COOKED)

**Rules for every face:**
- One base preset at weight **0.75 to 0.8**, plus a second preset at 0.2 to 0.25 for skin and character. Never 50/50.
- Then do landmark moves. Each move is **2 mm or less, mirrored left/right**, and you commit after each region.
- Skin roughness: **F 1.06, M 1.10**. `high_frequency_delta` and `global_delta` stay at 1.0.
- Request **4K texture sources** in the lab and build at 2K for the game.
- Eyes: limbal ring 0.78, softness 0.06, iris saturation 1.2, sclera vascularity 0.10.
- Teeth: worn 0, plaque 0, variation 0.2.

**Landmark directions** are in head space: **fwd** = from the ear midpoint to the nose tip, **out** = away from the midline, **up** = head up. The landmark names below are resolved to indices once by `mh_landmarks.py` (see 3.1, L1).

**Ship first: M1 and M2, F1 and F2.** M3 to M6 and F3 to F6 follow after the LFS pass (3.4).

#### Male faces

| Face | Base (weight) + second | Tone band | Landmark moves (mm) | Skin and accents | Lashes / brows default | Hair / beard default |
|---|---|---|---|---|---|---|
| **M1 Ash** | Orlando 0.8 + Victor 0.2 | Light | Gonion out 2.0, down 1.0. Chin fwd 1.5, down 1.0. Zygion (cheekbone) out 1.5, up 1.0. Infra-zygoma (under the cheekbone) in 1.0. Lateral canthus up 1.0. Brow ridge fwd 1.5, brow down 0.5. Nasal dorsum (bridge) fwd 0.5. Upper lip vermilion fwd 0.5 | Rough 1.10. Cheeks red +0.10, nose +0.15, ears +0.20, chin and scalp light −0.10 | `S_Fine` / `M_Full` | `S_Messy` / `S_Stubble` |
| **M2 Kade** | Mateo 0.75 + Trey 0.25 | Medium brown | Gonion out 1.5, down 1.0. Chin fwd 1.0. Zygion out 1.5, up 1.5. Infra-zygoma in 1.5. Lateral canthus up 1.0. Upper lid down 0.5 (hooded). Brow ridge fwd 1.0. Lower lip fwd 0.5, out 0.5 | Rough 1.10. Cheeks +0.10, nose +0.10, ears +0.20, chin −0.12 | `S_Fine` / `M_Thick` | `S_CurlyFade` / `M_Stubble` |
| M3 Ren | Kelvin 0.8 + Orlando 0.2 | Light-medium | Gonion out 1.0. Chin fwd 1.5, down 0.5. Zygion out 1.0, up 1.0. Lateral canthus up 1.5. Nasal dorsum fwd 1.0. Brow ridge fwd 1.0 | Rough 1.08. Cheeks +0.08 | `S_Thin` / `M_FlatThick` | `S_SlickBack` / none |
| M4 Kofi | Cameron 0.8 + Isaiah 0.2 | Dark | Gonion out 1.5. Chin fwd 1.0. Zygion up 1.5. Infra-zygoma in 1.0. Lateral canthus up 1.0. Brow ridge fwd 1.0. Lips out 0.5 | Rough 1.10. Accents saturation +0.05 (dark skin reads accents as saturation, not redness) | `S_Fine` / `M_Natural` | `S_Coil` / `S_Stubble` |
| M5 Varro | Victor 0.75 + Mateo 0.25 | Olive | Gonion out 2.0, down 1.5 (the heaviest jaw). Chin fwd 2.0. Brow ridge fwd 2.0, brow down 1.0. Nasal dorsum fwd 1.0 (Roman nose) | Rough 1.12. Chin and scalp −0.12 | `S_Sparse` / `M_Thick` | `S_BuzzCut` / `S_Full` + `Mustache_S_Full` |
| M6 Obi | Omari 0.75 + Mikel 0.25 | Deep | Gonion out 1.5. Chin fwd 1.0, down 1.0. Zygion out 1.0, up 1.0. Lateral canthus up 1.0. Brow ridge fwd 1.5 | Rough 1.10. Accents saturation +0.05 | `S_Fine` / `M_Full` | `M_TwistedBraids` / `M_Curly` + `Mustache_M_Curly` |

#### Female faces

| Face | Base (weight) + second | Tone band | Landmark moves (mm) | Skin, accents and makeup | Lashes / brows default | Hair default | Freckles |
|---|---|---|---|---|---|---|---|
| **F1 Wren** | Vivian 0.8 + Jelena 0.2 | Light | Gonion in 1.5, up 1.0 (V jaw). Chin fwd 1.0, a narrower point (chin out −0.5). Zygion out 1.0, up 1.5. Infra-zygoma in 1.0. Lateral canthus up 1.5, out 0.5 (almond). Brow tail up 1.0 (arch). Nasal tip up 0.5, alar in 0.5. Lower lip fwd 1.0, down 0.5. Cupid's bow peaks up 0.5. Philtrum up 0.5 (shorter) | Rough 1.06. Cheeks red +0.15, nose +0.10, ears +0.20, lips +0.10, under-eye light −0.05. Lips `NATURAL` 0.35 opacity, rough 0.45. Eyes `THIN_LINER`. Blush `HIGH_CURVE` 0.15 | `L_Curl` / `M_SlightArch` | `M_Layered` | `TYPE1` 0.3 / 0.25 |
| **F2 Nima** | Sunita 0.75 + Etta 0.25 | Medium brown | Gonion in 1.0, up 1.0. Chin fwd 0.5. Zygion out 1.5, up 1.5. Infra-zygoma in 1.0. Lateral canthus up 2.0 (the strongest tilt). Brow tail up 1.0. Lower lip fwd 1.0. Upper lip fwd 0.5. Philtrum up 0.5 | Rough 1.06. Accent saturation +0.05, lips +0.10. Lips `NATURAL` 0.40. Eyes `SOFT_SMOKEY`. Blush `HIGH_CURVE` 0.12 | `L_ThickCurl` / `M_Natural` | `L_MessyClumps` | Off |
| F3 Sora | Tuya 0.8 + Aera 0.2 | Light-medium | Gonion in 1.0. Chin fwd 1.0. Zygion up 1.0. Lateral canthus up 1.5. Nasal dorsum fwd 0.5. Lower lip fwd 1.0 | Rough 1.05. Lips 0.35. Eyes `THIN_LINER` | `L_Curl` / `M_Fine` | `S_LowPonytail` | Off |
| F4 Amara | Zuri 0.8 + Ada 0.2 | Deep | Gonion in 1.0, up 1.0. Zygion out 1.5, up 1.5. Lateral canthus up 1.5. Brow tail up 0.5. Lips out 0.5, lower lip fwd 1.0 | Rough 1.07. Accent saturation +0.06. Lips 0.40, rough 0.40 | `L_ThickCurl` / `M_Natural` | `M_TwistedBraids` | Off |
| F5 Mila | Jelena 0.8 + Celeste 0.2 | Fair | Gonion in 1.5. Chin fwd 1.0, point −0.5. Zygion out 1.5, up 1.5. Infra-zygoma in 1.5 (the most hollow). Lateral canthus up 1.0. Brow tail up 1.0. Lower lip fwd 0.5 | Rough 1.06. Cheeks +0.18, ears +0.22. Eyes `THIN_LINER` | `L_Curl` / `M_SlightArch` | `L_Straight` | `TYPE1` 0.4 / 0.3 |
| F6 Kesi | Asha 0.75 + Ada 0.25 | Dark brown | Gonion in 1.0. Chin fwd 0.5. Zygion up 1.5. Lateral canthus up 1.5. Lower lip fwd 1.0. Cupid's bow up 0.5 | Rough 1.07. Accent saturation +0.05. Lips 0.40 | `L_SlightCurl` / `M_Swept` | `S_UpdoBraids` | Off |

**Head scale** (set once per character in the Creator UI, Head Sculpt → Head Parameters; it is read-only from Python): **F 0.96, M 1.00**.

**Skin tone** comes from the base preset's `u/v`, blended 0.8/0.2 with the second preset. This is not adjustable in the creator, because tone is baked into the synthesized textures. The 4 tone bands share body textures (3.4).

### 2.3 Bodies (GAP-COOKED; HERO-RESEARCH 2.4 targets kept)

| Build | Male (cm; constraints) | Female (cm; constraints) |
|---|---|---|
| Lean | 184. Chest 104, waist 79, hip 96, bicep 34. Muscularity 0.8, fat −1.2, masc −1.5 | 170. Chest 88, waist 62, hip 94. Muscularity 0.2, fat −0.8, fem +1.5 |
| **Athletic** (first, retarget source) | 184. 110 / 81 / 98. Muscularity 1.3, fat −0.9, masc −1.5 | 170. 93 / 64 / 99. Muscularity 0.5, fat −0.4, fem +1.5 |
| Strong | 186. Chest 116, waist 84, hip 100, bicep 42, across-shoulder 43. Muscularity 1.8, fat −0.5 | 172. Chest 98, waist 67, hip 104, thigh 60. Muscularity 1.0, fat 0.0 |

- **Height slider:** ±4%, applied as a uniform scale on the Body component only. The capsule stays the same.
- **Female idle stance:** an additive pose (pelvis tilt +3°, shoulders back 2°, weight on one hip in idle only) on the female AnimBP. This gives a natural feminine stance without a second animation library.

### 2.4 Hair, colour and makeup options (GAP-COOKED)

Groom names come straight from the UE 5.8 wardrobe (`/MetaHumanCharacter/Optional/Grooms/Bindings/`). They are all cards in game.

| Slot | Male (10 / 6 / 4) | Female (10 / 4) |
|---|---|---|
| Hair | `S_Messy`, `S_Casual`, `S_BrushCut`, `S_BuzzCut`, `S_SlickBack`, `S_PulledBack`, `S_CurlyFade`, `S_Cornrows`, `M_TwistedBraids`, `L_MessyClumps` | `M_Layered`, `L_Straight`, `L_MessyClumps`, `S_LowPonytail`, `S_UpdoBraids`, `S_Pixie`, `M_BobMessy`, `L_AfroCurly`, `M_TwistedBraids`, `S_SweptUp` |
| Beard (+ matched moustache) | none, `S_Stubble`, `M_Stubble`, `S_Full` + `Mustache_S_Full`, `M_Curly` + `Mustache_M_Curly`, `L_Full` + `Mustache_L_Full` | n/a |
| Brows | `M_Full`, `M_Thick`, `M_FlatThick`, `M_Natural` | `M_SlightArch`, `M_Natural`, `M_Fine`, `M_Swept` |
| Lashes | Set by the face (male `S_*`) | Set by the face (female `L_*`) |

| Colour option | Values |
|---|---|
| Hair (8 swatches, melanin / redness) | Black 0.95/0.05; Dark brown 0.80/0.15; Chestnut 0.60/0.30; Auburn 0.50/0.60; Copper 0.35/0.85; Dark blonde 0.35/0.20; Ash blonde 0.20/0.05; Silver-white 0.0/0.0 + grey 1.0 |
| Eyes (8 swatches, iris hue) | Dark brown, hazel, amber, green, grey-green, blue, grey, ice blue |
| Brows and lashes | Follow the hair, or "dark" (melanin +0.2) |
| Female lip tint (3) | Natural, berry (+red 0.2, −light 0.1), dark rose (+red 0.15, −light 0.2) |
| Female liner (3) | None, `THIN_LINER`, `SOFT_SMOKEY` |

### 2.5 Outfit tiers and armour (GAP-COOKED)

**Rule:** the stats stay on the one `armor` item Raven has (`items.json` slot `armor`). The look splits into **4 visual slots: Head, Chest, Hands, Legs**, with feet included in Legs. Each tier adds **one big shape**, and the tier must read at 30 m. Light sets are cut to flatter: a fitted waist, an open throat, and bare upper arms where the set allows.

**Element trims.** An armour item's element is set forever at crafting (CF Rules). It picks the trim palette for the **T1 to T4 families**. One mesh per family takes 12 palettes through a single `PathTint` and `TrimColor` MID pair. The palettes:

Fire ember-orange #E2541A; Lightning violet-white #B9A7FF; Sun gold #E8B230; Nature moss #5E8C31; Wind pale teal #9FD6CF; Arcane violet #7A3FD1; Frost ice-blue #8CC8F0; Blood crimson #8E0F1E; Tide sea-green #1E8C86; Necro bone-green #A8B89A; Spore bioluminescent cyan #39E6C2; Void black-violet #2B1840.

| Tier | Stat item (SPEC) | Look name | Head | Chest | Hands | Legs | Skin shown | Big shape added | Materials |
|---|---|---|---|---|---|---|---|---|---|
| 0 | none | **Survivor Wraps** | Bare; optional sweat-cloth headband | M: open linen wrap crossing the chest, bare shoulders and arms. F: leather chest binder with a linen half-wrap over one shoulder, bare midriff | Fibre-bound forearm wraps, fingers bare | Short linen kilt (M) or wrapped hip skirt split at the thigh (F); shin wraps; bare feet in rawhide soles | ~40% | (the body line) | Linen, fibre rope, rawhide |
| 1 | `hide_armor` | **Hidewear**. Fire trim = **Cinderhide** (charred, ash-dusted pelt); Frost = **Snowhide** (white fur); other elements recolour the fur | Fur-lined hood, down by default | Pelt mantle over one shoulder; laced hide vest (F: a deep laced V) | Hide bracers to mid-forearm | Hide leggings, fur-topped boots | ~30% | Pelt mantle and collar | Hide, fur |
| 2 | `copper_armor` | **Copperscale** | Leather cap with a copper brow band | Hide vest under copper scale on the chest and left shoulder | Bracers with copper studs | Leggings, knee guards | ~20% | Shoulders 8 cm wider | Copper scale, verdigris |
| 3 | `bronze_armor` | **Bronze Lamellar** | Open bronze helm with cheek guards | Lamellar cuirass with pauldrons | Leather gloves, bronze cuffs | Lamellar skirt over trousers | ~10% | The skirt plus pauldrons | Bronze lamellar, dyed cord in the trim colour |
| 4 | `iron_armor` | **Ironmail** | Closed iron helm with a crest in the trim colour (the hide-helmet toggle shows the face) | Mail hauberk with plate breast and pauldrons | Gauntlets | Mail chausses, plate greaves | <5% | Full block, shoulders +15 cm, crest | Iron, mail, tabard in the trim colour |
| 5 | steel, Hearth 4 (RAVEN-PLANNED) | **The 12 biome sets** (table below) | | | | | Face, neck and forearms left open | One unique shape per biome | The biome's signature resource |
| 6 | starsteel, Hearth 5 (RAVEN-PLANNED) | **Ascendant** variant of the same biome set | Same meshes, plus starsteel inlay (emissive 1.5 nits in the path colour) and a halo-crest on the helm | | | | | The crest | + starsteel |

**The 12 biome sets** (Hearth 4). FULL-GAME's list is kept except for 3 names that clashed with resource item names: Heartwood → **Heartguard**, Tidecoral → **Reefmail**, Voidglass → **Hollowplate**.

| Biome | Set | Signature material | Silhouette hook (reads at 30 m) | Palette | Open windows | Sim pieces |
|---|---|---|---|---|---|---|
| Fire | **Forgeplate** | Obsidian plates and sulfur-glass seams | Tall back-swept pauldrons; horned helm | Black, ember seams at 2-4 nits | Forearms, neck, face | none |
| Lightning | **Stormweave** | Stormglass rods in woven grey wool | Fulgurite spikes rising from the shoulders like a crown of glass | Slate, violet-white glints | Forearms, face | Long tattered half-cape |
| Sun & sand | **Dunewrap** | Sunstone discs on sand-coloured wraps | Wide hooded wrap with a face veil (lowered), sunstone disc on the chest | Bone, ochre, amber glow at night | Arms to the shoulder, face; F: midriff | Veil tails, sash |
| Nature | **Heartguard** | Heartwood bark plates on leather | Layered bark pauldrons with an antler crown | Amber-brown, moss green | Forearms, throat | Leaf-strip skirt flaps |
| Wind | **Skysilk** | Levistone shards on silk | Floating levistone shards orbiting the shoulders (bone-attached, sine bob) | White, pale teal | Arms, collarbones, face | Long silk scarf |
| Arcane | **Wardcloth** | Resonant crystal in deep-blue cloth | High collar with crystal points; rune-sewn long coat | Indigo, violet crystal glow | Forearms (runes), face | Coat tails |
| Frost | **Rimeguard** | Pale steel, ice-crystal pauldrons, white fur | Huge fur collar with crystal points | Blue-white, white fur | Face, throat, fingertips (glove-less option) | Fur cloak |
| Blood & curse | **Mirehide** | Bloodmoss-cured black leather | Hooded half-mask and a spiked collar; long coat | Black-red, crimson stitching | Neck (veins), forearms | Coat tails |
| Tide | **Reefmail** | Tidecoral plates over sharkskin | Coral pauldrons branching up past the head | Coral red-orange, sea green | Neck (gill lines), arms | Kelp-strip tassels |
| Necromancy | **Gravecloth** | Soulstone clasps on grey shroud cloth | Bone-ribbed chest; tall cowl | Ash grey, bone, pale-green soul glow | Face, hands | Shroud skirt |
| Spore | **Sporeweave** | Deepcap leather and woven mycelium | Mushroom-cap pauldrons with glowing gills under them | Umber, cyan bioluminescence | Face, forearms, freckle zones | Hanging mycelium strands |
| Void | **Hollowplate** | Voidglass plates in black iron | Floating rubble halo behind the back; jagged, asymmetric pauldron | Black glass, violet edge light | Face (black eyes), forearms | none |

**Appearance layer (transmog).** Each of the 4 visual slots can show any look you've ever crafted. Stats stay on the worn item.

**Hide-helmet toggle.** One option. Tier 0 always shows the head.

### 2.6 Path body changes (GAP-COOKED; Fire veins and Necromancy pale skin are SPEC)

**How it works:**
- One skin material layer on the body and head drives all 12 paths.
- **Mask textures (authored once per sex):**
  - Body: 2K RGBA. R = vein network (neck, chest, arms, hands, temples). G = joint patches (shoulders, elbows, knuckles, knees, collarbones, cheekbones). B = mark field (forearms, upper arms, upper back, ribs). A = extremities (fingertips, nails, lips, ear tips, eyelids).
  - Head: 1K RGBA.
- **One 1K pattern texture per path:** R = pattern, G = animation noise. It tiles inside the mask.
- **Strength:** main path s = smoothstep(0.08, 1.0, points ÷ 100). Sub path = 0.6 × the same.
  - Both draw at once; the main path draws on top.
  - Eyes: the iris follows the main path; the limbal ring takes the sub path's colour at 50%.
- **Combat pulse:** the emissive ×1.6 for 1.5 s whenever you use that path's ability.

| Path | Skin (mask · look) | Eyes | Hair | Extra | 0.3 / 0.6 / 1.0 |
|---|---|---|---|---|---|
| Fire (SPEC: glowing veins) | R · veins glow ember #E2541A, emissive up to 3 nits, slow 0.4 Hz pulse; A · soot-dark nails | Ember ring at the pupil | none | Heat haze at the hands when Heat is full (Niagara, owner and 15 m only) | Faint warm veins at the wrists / up the arms and neck / full network, glowing |
| Lightning | B · branching Lichtenberg scars, pale; they flicker white at 6 Hz for 0.3 s on each cast | Violet-white spark ring | WPO lift +1.5 cm at the tips; flickers in combat | Static crackle sound on cast | Scars on the hands / arms and chest / full scars with flicker |
| Sun & sand | Albedo darkened 15% (sun-darkened); G · gold flecks with a metallic mask | Gold iris that catches light (specular boost 1.4) | Sun-bleached tips +0.15 lightness | none | Flecks on the cheekbones / shoulders / full sun-skin |
| Nature | G · bark-textured patches at the joints (normal + albedo, no emissive) | Leaf-green iris tint | none | Small leaf-bud decals at the temples at 1.0 | Knuckles / elbows and knees / bark at the shoulders and collarbones |
| Wind | B · pale spiral tattoos that drift slowly (UV pan 0.02/s) | Pale teal limbal ring | WPO stir always on (0.5 Hz sine, 1 cm) | none | Spirals on the forearms / arms and back / full, drifting |
| Arcane | B · rune lines on the forearms, violet glow, brighter while casting | Violet rim | none | Runes light 2 nits for 2 s on cast | Wrist runes / forearms / forearms plus the back of the neck |
| Frost | A · frost-pale lips and fingertips with rime sparkle; skin cooled 8% | Pale-blue limbal ring | Rime-white tips at 1.0 | Breath mist: Niagara at the mouth socket, one 1.2 s plume every 4 s (owner and 15 m) | Fingertips / lips and fingertips / rime to the knuckles, plus breath |
| Blood & curse | R · dark red veins at the neck and wrists (no glow) | Red sclera vessels (vascularity 0.6) | none | **Sharper canines:** a 2-piece rigid cap mesh on the upper teeth bone (200 tris), shown from 0.6 | Wrists / neck / neck to the jaw, plus canines |
| Tide | G · scale sheen on the neck and collarbones (iridescent spec); B · gill lines along the neck that open underwater (mask scroll on `Underwater`) | Sea-green iris | Wet look +0.3 roughness drop | Gills open when you swim under the surface | Sheen / sheen plus gill lines / full, gills open underwater |
| Necromancy (SPEC: pale skin) | Albedo desaturated 40% and lightened toward ash; A · grey lips and nails | Pale grey iris, pupil ring glow pale green 0.5 nits | Hair desaturates 30% | none | Pale hands / pale arms and face / full ash skin |
| Spore | G · faintly glowing fungal freckles (cyan, 0.8 nits) | Cyan fleck ring | Mossy green tips (lower 15% of the card V) | Spore motes drifting from the shoulders in the dark (owner and 15 m) | Freckles on the cheeks / shoulders / full, plus mossy tips |
| Void | Skin greys with **current corruption**, not points (0 to 40% desaturate); A · black fingertips | Eyes go black from the centre (pupil scale 1.0 → 2.6 at full corruption) | none | Light-bend haze near the hands at high corruption | Driven by corruption: 25% / 50% / 100% |

**Armour respects path marks:**
- Every set's trim and seams take the main path colour through `PathTint`.
- The open windows keep the marks visible.
- The creator's Path tab previews 0.3, 0.6 and 1.0 on the bare body.

### 2.7 First person (GAP-COOKED; FP toggle BUILT)

| Part | Number |
|---|---|
| Camera position | The `head` socket after anim, then +9 cm forward and +6 cm up (the eye point). Horizontal 1:1. Vertical: critically damped spring, half-life 0.05 s, bob clamp ±2.5 cm |
| Camera rotation | Control rotation only. Never the head's. No roll |
| Body follows the view | Spine aim offset: pitch ±70°, yaw ±60° before the legs turn in place (GASP turn database) |
| Owner hides | Face, Hair, Brows, Lashes, Beard, Moustache and the Head-slot armour: `SetOwnerNoSee(true)` and `bCastHiddenShadow=true` |
| Anti-clip | Near clip 5 cm. First Person Output gradient: 1 on the hands and forearms, falling to 0 at the upper arm. Held weapons and tools are `FirstPerson` primitives. FP scale 0.7. FP FOV off |
| FOV | 90° horizontal default; slider 75 to 110 (settings) |
| Centre dot | 3 px, always on in FP |
| Dodge roll in FP | Camera translates only: down 30 cm and back over the roll; vignette 15% for 0.1 s |
| Mantle and ladder | Camera pitch change capped at 15° |
| Mount | Spring as above plus sway cap ±3 cm |
| Third person | Same mesh with everything visible. The toggle (V) is instant |

### 2.8 The creator (GAP-COOKED)

**Where:**
- The main menu's character area: a **Customise** button next to the Male/Female choice.
- The **Mirror** building piece in your base, which gives free re-edits of everything except sex.
  - Mirror recipe: workbench, Hearth 1. Wood ×20, stone ×5, copper ingot ×2.

**Layout:**
- Left panel: tabs on top, big tiles below.
- Right: the hero on a turntable.
- Bottom: Randomise, Undo, Done.

**Tabs:**

| Tab | Tiles |
|---|---|
| Face | 6 face portraits (pre-rendered 256 px thumbnails) |
| Body | 3 builds plus the height slider (−4% to +4%) |
| Hair | 10 hair, 4 brows, 6 beards (male) |
| Colour | Hair swatches 8; eye swatches 8; brows "match / dark"; female lip tint 3 and liner 3 |
| Path | Main path list. The preview slider steps through 0.3, 0.6 and 1.0 strength on the bare body |

**Preview:**
- An `AWRHeroPreview` actor in the menu map, lit with a 3-light rig: key 35° up and 40° side, rim behind, soft fill at 25%.
- **Strands on** in the creator only (`r.HairStrands.UseCardsInsteadOfStrands 0` while open).
- Camera presets: Face (FOV 20, 45 cm), Upper (FOV 30), Full (FOV 35).
- Rotation: mouse drag or right stick. Zoom: wheel, or LT/RT. Idle animation: the GASP idle.
- Outfit toggle: Wraps / Hidewear / Ironmail preview.

**Input:**
- Mouse: click a tile, drag to rotate, wheel to zoom.
- Pad: LB/RB change tab, D-pad or left stick move tiles, A pick, B back, Y randomise, X undo, right stick rotate.
- F1 shows the controls page. Every state has a focused widget (no dead states).

**Saving:**
- `FWRAppearance` is packed into 20 bytes and saved in GameUserSettings under `[Wildreach.Menu] Look=<hex>`.
- It is sent on the join URL as `?Look=<hex>`.
- The server clamps every index into range and stores it on the PlayerState and in the world save per player.

### 2.9 Sound for the hero (GAP-COOKED)

| Cue | What |
|---|---|
| Efforts (no words) | Per sex: light swing (4 variants), heavy swing (4), roll (3), hurt light (4), hurt heavy (3), death (2), jump and land (3), drink (2), climb and mantle (3). Real recorded breaths and grunts |
| Outfit foley | Rides the footstep and swing notifies. Per tier: wraps (cloth rustle), hide (leather creak), copper and bronze (scale chink), iron (mail jingle plus plate clank), each biome set (its material: glass tink, bark knock, silk flutter, coral rattle) |
| Path cues | Fire vein pulse (low ember hum when casting); Lightning crackle; Frost breath (soft exhale); the others are silent |

### 2.10 Replication and data (GAP-COOKED)

| Data | Size | How |
|---|---|---|
| `FWRAppearance` | ~20 bytes at join | Sex, face, build, height, hair, brows, beard, hair colour, eye colour, lip, liner, 4 appearance-slot look ids, helmet-hidden. On `AWRPlayerState`, RepNotify |
| Path marks | 4 bytes | Main and sub path id plus their points; already on `WRPathComponent`. RepNotify drives the MID |
| Movement for MM | +4 bytes per update | Quantised acceleration (3 bytes) plus a gait/stance/lock-on bitfield (1 byte) |

---

## 3. Plan

### 3.1 Lane build-hero-look (WildreachCharLab only)

Art route: MetaHuman faces and bodies come from script. Outfits go concept → Blender/Marvelous → MetaHuman Outfit Asset → resize; hard plates go Blender → skin-weight transfer. **No Hunyuan** for the hero (MetaHuman meshes plus hand-made garments).

| Step | Work | Output |
|---|---|---|
| **L1 proof-of-one** | Write `mh_landmarks.py`: dump `get_face_landmarks`; name the indices by geometry (gonion = widest point of the lower third; zygion = widest at eye height; canthi = eye-corner extremes; chin = lowest midline; and so on). Save `landmarks_map.json`. Write `mh_face.py`, a table-driven replacement for `mh_faceblend.py`: base at 0.8, the landmark moves, skin, accents, eyes, lashes, makeup and teeth from section 2.2, plus 4K source request. **Build M1 Ash**, set head scale in the UI, and check it in a lit viewport HighResShot | M1 in `BP_Hero_M_Face1` |
| L2 | The same for F1, M2 and F2. Confirm the grooms show (viewport shot). Check that `mh_fix.py`'s slot fix has run | 4 faces |
| L3 | `mh_shape.py`: lean, athletic and strong per sex as **body constraint presets**. Build the athletic body first (it is the anim lane's source), then the other 2 | 6 bodies |
| L4 | Game build settings: `desired_texture_sources_resolutions` 2K; `common_folder_path` = `/Game/Wildreach/Characters/Hero/Common` (shared grooms and textures); cards-only grooms stripped of strands for the game copy; body textures shared per tone band | Built hero at ≤ 120 MB |
| L5 | Skeleton sockets on the MetaHuman body skeleton. All hand sockets: X = grip axis (pinky → index metacarpal), Z = palm normal, origin at the palm centre (mean of `hand_r`, `index_metacarpal_r`, `pinky_metacarpal_r`, pushed 2 cm along the palm normal). Sockets: `weapon_r`, `weapon_l`, `bow_l`, `torch_l`; `shield_l` (on `lowerarm_l`, 55% toward the wrist, Z = outer forearm); `back_weapon` (`spine_05`, 12 cm behind, diagonal 35°); `back_shield` (`spine_04`, 15 cm behind); `hip_r` and `hip_l` (`pelvis`, dagger and quiver); `mouth` (head, for the breath FX); `teeth_upper` (canine caps) | Sockets |
| L6 | **Survivor Wraps.** Concept sheet → Blender/Marvelous on the athletic body → Outfit Asset → resize to lean and strong → clothing mask (hide body under cloth) → 4 slot meshes, 12-18k tris at LOD0, 4 LODs. One 2K BC/N/ORM set plus shared 1K tiling linen, leather and fur | 2 sexes × 3 builds |
| L7 | Path-mark masks (body 2K, head 1K per sex) painted in Blender on the MetaHuman UVs; 12 pattern textures at 1K; canine caps; `M_WR_Skin` and `M_WR_Head`, copies of the MetaHuman skin materials plus a `MF_PathMarks` layer (params `MainPath`, `SubPath`, `MainStrength`, `SubStrength`, `Pulse`, `Corruption`, `Underwater`); eye MID params; hair-card WPO params | Path layer |
| L8 | Hidewear, Copperscale, Bronze Lamellar, Ironmail (one mesh family each, `TrimColor` and `PathTint`), then Forgeplate and Rimeguard. Concepts first; hard plates are rigid, skin-transferred to bones | 6 families |
| L9 | The remaining 10 biome sets (one per round, with its biome), then Ascendant inlays. Faces M3-M6 and F3-F6 after the LFS pass | |
| L10 | Thumbnails: 256 px portrait per face and hair, rendered in the lab (art output, not a test) | Creator tiles |

**Rule:** the editable `MH_*.uasset` sources and the 4K sources **stay in the lab**. Only built, 2K, cards-only output migrates.

### 3.2 Lane build-hero-anim (game repo, after Aaron's OK)

**C++** (Raven's patterns: data-driven, all damage through `ApplyHit`, new abilities through `UWRGameplayAbility` and `ClassForNode`):

| File | Change |
|---|---|
| **new** `WRHeroAnimInstance.h/.cpp` | `UWRHeroAnimInstance : UAnimInstance`, the base of the thin `ABP_Hero`. **`UWRAnimInstance` stays for creatures, unchanged.** <br>• Fields: `Gait` (Walk/Run/Sprint), `MoveState` (Idle/Moving/Air/Swim), `Stance` (Melee/Ranged/BowAim/RifleAim/Block/TwoHand), `bLockedOn`, `AimPitch`, `AimYaw`, `Trajectory` (`FTransformTrajectory`), `FootLift[2]`, `bIsFemale`, `LODMode`. <br>• `NativeThreadSafeUpdateAnimation` builds the trajectory with `UPoseSearchTrajectoryLibrary` (past −0.3 s; future 0.35, 0.7 and 1.0 s) from the CMC plus replicated acceleration. <br>• Keeps `PlayOneShot`, `StopOneShot` and `StrikeFraction` signatures as wrappers, so callers don't change. One-shots become `UAnimMontage::CreateSlotAnimationAsDynamicMontage(Clip, "UpperBody"/"FullBody", 0.1, 0.15, Rate)`. Authored montages (combat lane's `AM_*`) play directly. Upper loops (block, bow aim, rifle aim) are sequence players picked by `Stance` |
| `WRCharacter.cpp/.h` | **LoadBodyArt:** read schema v2; build components: Body (MetaHuman body), Face (leader-pose to Body at LOD ≥ 2; the build's face AnimBP with RigLogic at LOD 0-1), `UGroomComponent` ×5 (Hair, Brows, Lashes, Beard, Moustache), outfit `USkeletalMeshComponent` ×4 (leader pose), `ULODSyncComponent`. Apply `FWRAppearance` and the 2 MIDs. Weapon and shield attach `SnapToTarget` to the sockets plus the item's `grip` offset; **delete the Mixamo palm-down math**. <br>**SetupBodyAnim:** `Body->SetAnimInstanceClass(ABP_Hero)` (path from `player_art.json`). **BodyClip / PlayBodyAnim:** same keys and API; montage underneath. <br>**TickBodyAnim:** fills `UWRHeroAnimInstance` fields instead of `FWRAnimInput`; the foot traces move into foot IK. <br>**OnRep_FirstPerson / ApplyViewMode:** the hide list from 2.7, plus `bCastHiddenShadow`. <br>**New `TickFirstPersonCamera(dt)`:** the head-bone spring from 2.7. <br>**Movement (FULL-GAME 9):** `MaxAcceleration` 5000, `BrakingDecelerationWalking` 4000, `bUseSeparateBrakingFriction` true, `GroundFriction` 8 (all from `tuning.json` `player`). <br>**New `FWRReplicatedAccel`:** Lyra pattern, 3 bytes, plus a 1-byte bitfield. **New `OnRep_Appearance`** and `ApplyPathMarks()` |
| `WRRanged.cpp` | Line 166: the default socket comes from `player_art.json` `sockets.weapon` (`weapon_r`), not the hardcoded `RightHand`. The `along/face/pose` placement is replaced by the socket frame plus the item's `rot` |
| `WRHeldTool.cpp` | The finger lookup `Socket + "Index1"` becomes the socket frame (`tool_r` = `weapon_r`). The handle runs along socket X |
| `WRPathComponent.cpp` | Broadcast `OnPathMarksChanged` on a points or path change and on an ability use (the pulse) |
| `WRMainMenu.cpp` | Add the Customise button and `SWRCreator` (new `WRUICreator.cpp`, Slate, WRUI styles, pad focus). Save `Look`; add `?Look=` to the join URL |
| `WRGameMode.cpp` / `WRPlayerState` | Parse `?Look=` with `?Body=`; clamp; replicate; save per player |
| `WRBuilding` data | Mirror piece → opens `SWRCreator` in Mirror mode |

**Data:**

| File | Change |
|---|---|
| `Data/player_art.json` → **schema v2** | `"hero": { "m": {...}, "f": {...} }`. Per sex: `skeleton`, `animBP`, `bodies` [3 build mesh paths], `faces` [6: `head`, `toneBand`, `defaults`], `grooms` {`hair`[10], `brows`[4], `beards`[6 with moustache]}, `outfits` {lookId: {`head`, `chest`, `hands`, `legs`} per build}, `skinMat`, `headMat`, `pathMasks`. Shared: `sockets` {`weapon`: `weapon_r`, `shield`: `shield_l`, `bow`: `bow_l`, `torch`: `torch_l`, `back`: `back_weapon`, `backShield`: `back_shield`}, `anims` (same keys as today; values point at the retargeted clips), `fullBody` (same list plus `ride`), `paths` (12 rows: colour, emissive, eye params, hair params, FX), `fp` {`eyeFwd` 9, `eyeUp` 6, `halfLife` 0.05, `bob` 2.5, `nearClip` 5, `fpScale` 0.7, `fov` 90}. **Delete** `animPrefix`, `upperBody`, `spineBones`, `legBones`, `meshYaw` |
| `Data/items.json` | Ranged `socket`: `RightHand` → `weapon_r`, `LeftHand` → `bow_l`. Drop `along`/`face`/`pose`; keep `rot` for the crossbow and blowgun. Armour items get `"look": "hidewear"` and so on. The steel biome set items (ids `forgeplate_armor` … `hollowplate_armor`) arrive with the crafting cook |
| `Data/tuning.json` | `player` movement numbers; `anim.mm` {`remoteSearchInterval` 0.1, `fallbackLOD` 3}; `fp` mirror of the art values for live tuning |

**Animation assets** (in the repo, `/Game/Wildreach/Characters/Hero/Anims/`):

| Asset | From |
|---|---|
| 14 MM databases (`PSD_Idle`, Walk/Run/Sprint × Starts/Loops/Stops/Pivots, `PSD_TurnInPlace`, `PSD_JumpFallLand`, `PSD_Crouch`) + schema `PSS_Wildreach` | GASP clips, **retargeted offline once per sex** onto the athletic skeleton. Skeleton translation retarget = `Skeleton` on every bone except root and pelvis, so lean and strong reuse the clips. Check the hand chains (5.8 wrist issue) |
| `CHT_Locomotion` Chooser | Gait × MoveState × Stance → database. A row for LOD ≥ 3 → a single blendspace fallback |
| `ABP_Hero_M` / `_F` (child of `UWRHeroAnimInstance`, no logic) | Motion Matching → Pose History → Orientation Warping → Stride Warping → Upper loop (layered from `spine_01`) → Slot `UpperBody` → Slot `FullBody` → Aim offset (spine) → Foot placement → (F: stance additive) → Output |
| Mixamo action clips | Today's survivor clips go through the IK Retargeter (source root `mixamorig:Hips`) → MetaHuman body, with the **Root Motion op**. Check `Enable Root Motion` on export. Keys kept: light, heavy, cast, hit, death, block, dodge, gather, chop, pickup, bow and rifle sets, throw, swim, tread |
| New keys | `ride` (Mixamo riding loop); `mantle_050`, `mantle_100`, `mantle_180` montages with Motion Warping windows (warp targets from C++ traces) |

**Performance:** URO on remote players; the MM node's search throttle is 0.1 s on remote players; the Animation Budget Allocator is on; animated face maps are off for remote players; RigLogic only at LOD 0-1. Budget: 2.0 ms for 16 players.

**Map dressing:** a menu-map preview pad with the 3-light rig, and the Mirror building piece.

**Migration:**
1. Built 2K output → `Content/Wildreach/Characters/Hero/{Common,M,F,Anims,Outfits}`.
2. Switch `player_art.json` to v2.
3. Then **remove `Content/Characters/Survivor` and `Content/Characters/SurvivorF`** (and their clips) in the same change.

### 3.3 Build order

**build-hero-look:** L1 (M1 proof) → L2 → L3 → L4 → L5 → L6 → L7 → L8 → L9 → L10.

**build-hero-anim:** can start on today's lab `BP_Survivor_M` body (same skeleton).

| Step | Work |
|---|---|
| **A1 proof-of-one** | Retarget GASP walk/run/idle (3 databases) onto the M athletic skeleton. `UWRHeroAnimInstance` + `ABP_Hero_M` + `CHT_Locomotion`. The male hero walks, runs and sprints in game with the sword still in a socket. Gate: compile, cook, package |
| A2 | All 14 databases, plus the female |
| A3 | Mixamo retarget plus dynamic montages; `PlayBodyAnim` and the combat lane's `AM_*` on the slots |
| A4 | Sockets; `WRRanged`, `WRHeldTool` and `items.json` socket changes |
| A5 | Full-body first person (2.7) |
| A6 | Schema v2: face, grooms, outfits, LODSync, `FWRAppearance` replication |
| A7 | Path-mark MIDs and FX |
| A8 | Creator UI plus the Mirror |
| A9 | Mantle, ride and acceleration replication; performance settings |
| A10 | Migration and Survivor removal |

**Joint dependency:** A6 needs L2, L4 and L6. A7 needs L7.

### 3.4 Budget (LOD, texture, LFS)

| Item | Budget |
|---|---|
| LODs | Body 0-3; head 0-5; grooms cards LOD 0-3 plus helmet. LOD0 local and < 3 m; LOD1 from 3 m; LOD2 from 8 m; LOD3 from 25 m; LOD5 from 60 m. Outfits 4 LODs at 50%. LODSync drives all of it |
| Textures | Face 2K BC/N/SRMF, body 2K, cavity 1K, outfits 2K per set plus shared 1K tiling, path masks 2K/1K, patterns 1K ×12 |
| Tris at LOD0 | Head 24k verts, body 30.5k verts (Epic), outfit 12-18k tris, hair cards ≤ 30k verts |
| Repo (LFS) | First 2 faces per sex: **≤ 120 MB per sex**. Shared grooms and Common are paid once (~80 MB). Extra faces ~40 MB each. Outfits ~8 MB per family. Anims ~60 MB. **Total for R0 (4 faces, wraps, T1-T4, Forgeplate, Rimeguard): about 420 MB.** All 12 faces plus 12 sets come to about 900 MB, which waits on the LFS question (FRP question 1) |
| Runtime | Groom VRAM about 16 MB per player (~260 MB for 16); animation 2.0 ms for 16 |
