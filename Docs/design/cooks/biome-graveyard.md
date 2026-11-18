# Cook: biome-graveyard (Graveyard / Necromancy, round R4)

Lane: biome-graveyard. Written 2026-11-13. Scope: FULL-GAME 3.2.10, the Graveyard rows of 3.3, and the Necromancy row of 4.2. This covers the whole Graveyard, its catacombs, and the whole Necromancy path.

**Tags.** **SPEC** keeps Raven's design, or a built thing, exactly as written. **GAP-COOKED** is this lane's design and replaces the FULL-GAME first draft. **KEPT** means a number or name another 11-13 cook already set; it's restated here so the build has one page, and this lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** where the Graveyard sits (B5 Rootlands, west of the Spine), the three camps, Nhal's chapel at (−1,500, +3,800), the catacombs under the grave fields, the Rootgrave pass, the POI count (58 surface + 60 catacombs = 118), the weather weights, the Lantern Mourners and their tōrō waymarks, the bone giant at Rootgrave, and the borders (ash-graves of smiths to the north, graves that end mid-row to the south).
- **combat.md:** the decay status, the Necromancy spell and Wildbrand numbers, Souls as a spender resource, the creature attack-row schema, the boss template and visible-contact damage.
- **survival-crafting.md:** Rest, darkness and lights, corruption (numbers), Corpse fog, Gravecloth, the four Graveyard trophies' talismans, Black rye and Asphodel, the soul engine, the bone altar ritual for permanent undead tames, catacomb fishing, the Mourning chapel set.
- **hero.md:** the Necromancy body-change frame (pale skin is SPEC) and the path colour bone-green #A8B89A.

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Elden Ring, catacombs and Deathbirds** ([GameDeveloper on ER combat](https://www.gamedeveloper.com/game-platforms/the-cracks-in-elden-ring-s-combat-design), [Cook and Becker: It's all about the boss](https://www.cookandbecker.com/en/article/295/elden-ring-it-s-all-about-the-boss.html), [horrorobsessive: the monsters of Elden Ring](https://horrorobsessive.com/2022/04/08/the-many-monsters-of-elden-ring-part-1)) | Catacombs are short hand-made dungeons with one idea each (a trap, a boss, a lever). Deathbirds appear only at night in fixed spots and are gone by day, so the open world changes with the clock. Grave Wardens and Imps reuse bodies in new rooms | **Night owns this biome.** Every grave field has a different night table, and three threats (Gravehounds rising, the Gravedigger's walk, the Risen of the Blood Moon) exist only after dusk. **Every catacomb room has one idea** (a draught that kills torches, a flooded cistern, a bone chandelier you can drop) |
| **Bloodborne, Hemwick Charnel Lane** ([Spawning Point: FromSoft design discipline](https://spawningpoint.com/article/why-fromsoft-games-keep-landing-design-discipline-analysis), [TechRadar: what made Bloodborne better](https://techradar.com/features/elden-ring-is-great-but-its-missing-something-that-made-bloodborne-better)) | A small, dense, hand-made death village: bodies in carts, graves, lanterns, a witch boss who raises helpers. The place teaches the threat before it explains it | **The Bell-Ringer and Nhal raise helpers, and the counter is to stop the raising,** not to out-damage the adds. The land shows the counter first: sealed graves with heartwood stakes in them sit along the chapel road |
| **Valheim, Swamp crypts and body piles** ([PC Gamer swamp guide](https://www.pcgamer.com/how-to-find-valheim-swamp-key-biome-seed)) | Undead pour from body piles until you break the piles; crypts hold the metal the next tier needs | **Break the source, not the stream:** Bell rope, egg sacs, open graves, Ossuary keystones. And the catacombs hold the round's best ore (silver, rich iron, a star-iron seam) so you have to go down |
| **V Rising, Unholy school** ([GameSkinny necromancer build](https://www.gameskinny.com/tips/v-rising-best-necromancer-build/), [GamerGuides Unholy](https://gamerguides.com/v-rising/guide/powers/unholy)) | Corpse Explosion, Army of the Dead for about 8 s, skeletons that draw aggro. Strong in PvE, weak in PvP | Our numbers keep Raven's frame: raises are short (30 s), Army is a 20 s burst on a 120 s cooldown. Minions are **aggro and pressure, not a win button**. PvP raises take ×0.6 damage like everything else |
| **Diablo IV, Book of the Dead** ([GameSpot](https://www.gamespot.com/articles/diablo-4-update-dives-deep-on-the-necromancer-including-its-unique-book-of-the-dead-mechanic/1100-6504544/), [Wowhead](https://www.wowhead.com/news=327366/diablo-iv-quarterly-update-june-2022-necromancer-class-features-and-skills)) | Corpses are the resource; you choose what your minions are | **Corpses matter here too.** Raven raises *the dead creature itself* (a raised Gravehound fights like a Gravehound), which is better than generic skeletons: what you kill is what you get. Bodies stay down 3 minutes (survival-crafting KEPT), so fighting near a fresh kill is a choice |
| **Monster Hunter World, Rotten Vale / Vaal Hazak** (KEPT from biome-swamp research) | One ecological idea drives the whole map | **Our idea: the Graveyard lives off the dead.** Every creature feeds on, buries, guards or drinks from the dead: burying beetles carry grave slabs, moths drink the mourner's tears, crows find graves, hounds sleep in grave soil, goats gnaw old bones, and soulstone grows on bones |
| **Ghost of Tsushima / Horizon** (guiding wind, birds) | The world points the way without UI | **Gloamcrows circle over secrets and full graves,** and lantern lines (tōrō) lead to every shrine at night |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Vivianite on buried remains ([ScienceAlert](https://www.sciencealert.com/vivianite-the-blue-mineral-that-eerily-turns-buried-bodies-blue), [Atlas Obscura](https://atlasobscura.com/articles/vivianite-blue-human-remains), [AAFS 2010](https://www.aafs.org/sites/default/files/media/documents/AAFS-2010-H44.pdf)) | Phosphate from bodies plus iron in waterlogged graves grows crystals on bone. They are colourless when dug up and turn deep blue in air | **Soulstone** is a vivianite-like crystal that grows on old bones in wet grave soil. Fresh it's clear with a faint pale-green inner glow; in air it darkens to blue-green over 10 minutes and stops glowing (that's the "used" look on dropped stacks). Real chemistry, ghost colour |
| Moths drinking the tears of sleeping birds ([Smithsonian](https://www.smithsonianmag.com/smart-news/watch-moth-gently-slurp-tears-slumbering-bird-180970430/), [Wikipedia: Lachryphagy](https://en.wikipedia.org/wiki/Lachryphagy), [Audubon](https://www.audubon.org/magazine/drinker-of-bird-tears-moth-of-your-nightmares)) | A Madagascar moth slides its proboscis under the closed eyelid of a sleeping bird to drink its tears | **Wraithmoths drink tears.** They roost on Nhal, who weeps grave-light without end, and they land on downed players' faces to drink. The grab is the most unsettling move in the biome |
| Death's-head hawkmoth (general entomology) | A skull pattern on the thorax; it squeaks; it's drawn to light | **Wraithmoth** body, the skull mark that faces you on the dive, the squeak as its tell, and its pull toward your torch |
| Church grim and Black Shuck ([Wikipedia: Church grim](https://en.wikipedia.org/wiki/Church_grim), [Spooky Isles](https://www.spookyisles.com/?p=75963)) | A dog buried in a new churchyard so its spirit would guard it; seeing one was an omen of death | **Gravehounds** sleep buried in grave soil and rise at dusk to guard the fields. Their howl from the mist is the biome's night signature |
| Xoloitzcuintli and Xolotl ([Wikipedia: Dogs in Mesoamerican myth](https://en.wikipedia.org/wiki/Dogs_in_Mesoamerican_folklore_and_myth), [Kinship](https://www.kinship.com/pet-lifestyle/xoloitzcuintli-dogs-day-of-the-dead)) | The hairless dog that guides souls through Mictlan; dogs were buried with their owners | Gravehound anatomy: a hairless hound whose thin dark skin shows the bone of its face and spine. Lore item: dog graves beside the Mourners' own |
| Amblypygi, whip spiders ([American Arachnological Society](https://www.americanarachnology.org/about-arachnids/arachnid-orders/amblypygi/), [Wikiwand](https://wikiwand.com/en/articles/Amblypygi)) | Flat, sideways-running arachnids with whip-like sensing front legs and spined, raptorial palps; cave species are eyeless; they dominate the predators of some caves; females carry their young on their backs | **Cryptstalker** and **the Crypt Mother:** eyeless, flat, whip legs that sweep the dark to find you, spined palps that snatch and impale. The Mother carries her brood on her back |
| Burying beetles, *Nicrophorus* ([Wikipedia](https://en.wikipedia.org/wiki/Burying_beetle)) | They bury small carcasses, prepare them as food for their larvae and guard them as a pair | **Tombback** buries carcasses under a grave slab grown onto its back. A tamed one buries kills at base and turns them into remains (for the necro ritual) and bone meal |
| Mari Lwyd ([Wikipedia](https://en.wikipedia.org/wiki/Mari_Lwyd), [Atlas Obscura](https://www.atlasobscura.com/foods/mari-lwyd-horse-skull-wales-wassailing)) | A horse skull on a pole with a white sheet over the carrier, going door to door in the dark | **The Gravedigger** wears a horse skull and a white sheet. **Palestrider's** face is pale bone under thin skin |
| Mortsafes and watch-houses ([Wikipedia: Mortsafe](https://en.wikipedia.org/wiki/Mortsafe), [University of Aberdeen: bodysnatchers](https://www.abdn.ac.uk/bodysnatchers/background.php), [Wellcome Collection](https://wellcomecollection.org/articles/WxfvsiQAACcAY5Kp)) | Iron cages over fresh graves and towers where watchers sat through the night against the resurrection men | Iron mortsafes over graves across the fields (scrap iron and a story); a **Watch House** where the Gravedigger sleeps; the Gravedigger carries a mortsafe cage on his back. Someone robbed these graves, and someone tried to stop it |
| Sedlec Ossuary ([Wikipedia](https://en.wikipedia.org/wiki/Sedlec_Ossuary), [Atlas Obscura](https://www.atlasobscura.com/articles/2757)) | A chapel decorated with the bones of tens of thousands, with a chandelier holding every bone of the body | **The Chandelier Hall,** the Ossuary Lord's arena; the chandelier is a set-piece you can shoot down onto him |
| Paris catacombs (old limestone quarries filled with bones; general history) | The bones were moved into worked-out quarry galleries | The catacombs' lowest level is the Mourners' **old limestone quarry**. That's also why the grave ground above sinks (subsidence micro-hazard) |
| Keening and the banshee ([Wikipedia: Keening](https://en.wikipedia.org/wiki/Keening), [Wikipedia: Banshee](https://en.wikipedia.org/wiki/Banshee), [MDPI: Keening the Dead](https://mdpi.com/2077-1444/10/4/235)) | Hired women keened for the dead, rocking and wailing; the banshee legend grew from it; the washer at the ford washes the grave-clothes of those about to die | **Nhal's Keen** is her signature attack. The music's solo voice is a keen. Lore: a ford in the grave fields with grave-clothes left on the stones |
| Tōrō nagashi ([Visit Matsue](https://www.visit-matsue.com/calendar/august/obon_lantern_floating_festival), [Nextshark](https://nextshark.com/celebrating-toro-nagashi-japans-floating-lanterns-that-guide-the-departed), [MLIT](https://www.mlit.go.jp/tagengo-db/en/R1-00499.html)) | Paper lanterns floated to guide the spirits of the dead home, some with names written on them | The Lantern Mourners (world-map KEPT) light a lantern for every grave. **Cenotaph lanterns** carry names. The chapel pond is covered in drifted lanterns |
| Cenotaphs; churchyard yews; plague pits limed with quicklime; Mictecacihuatl, Lady of Mictlan ([World History: Mictlan](https://www.worldhistory.org/article/1714/)) | Empty tombs for people whose bodies lie elsewhere; ancient yews in old churchyards and yew as the longbow wood; lime laid over mass graves; a death goddess who rules the underworld of bones | Most surface graves are **empty cenotaphs** (the story, 2.20). The **Great Yew** landmark and yew longbows. **Lime pits** as a micro-hazard and lime as the Ossuary Lord's counter. Nhal's unhinged jaw in phase 2 |

### 1.3 Lessons, short

1. **One idea feeds everything:** the Graveyard lives off the dead.
2. **Night owns the biome.** Day is for gathering and learning the ground; night changes the tables, the hazards and the music. The Blood Moon is the night at its worst.
3. **Light is the gear check.** Raven's darkness and corruption are the main hazard. Torches can be blown out, lanterns can't, Wickbeetles never go out. A Wraithmoth flies at your torch.
4. **Break the source, not the stream.** Every raiser has a visible source (rope, sacs, graves, keystones) and a non-path way to break it.
5. **Undead are bone, shade and carrion, never zombies** (Raven's rule). No rotting flesh walks. Living creatures here look deathly because of real biology (hairless skin, skull marks, pale coats); the actually-raised are clean bone knitted with grave-light.
6. **Colour law (KEPT from biome-swamp):** crimson #DC143C is only the unblockable cue. Graveyard colours are bruise violet, ash, bone and soul-green. The Blood Moon is rust-red #8A2A1A, never crimson.
7. **Beautiful, not grotesque.** The graveyard reads as a mournful, gorgeous place: lanterns on mist, white asphodel, black yews, carved stone. The horror is in the creatures and the dark, not in gore.

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item | This lane |
|---|---|
| Path theme: raise dead creatures as minions and tames | SPEC (DD) |
| Tone: dark fantasy | SPEC (DD World) |
| Look, sub-zones, landmarks, POIs | GAP-COOKED 2.1 to 2.4 |
| Hidden sub-biome: the catacombs | GAP-COOKED 2.5 (name and place KEPT) |
| Signature resource: soulstone | GAP-COOKED 2.7 (name KEPT; soul energy as power SPEC) |
| Hazard: darkness and corruption | SPEC (DD); numbers KEPT (survival-crafting); micro-hazards GAP-COOKED 2.8 |
| Weather | Weights KEPT (world-map); effects reconciled 2.9 |
| Event: Blood Moon | Name SPEC (DD); behaviour GAP-COOKED 2.10 |
| No zombies | SPEC rule, applied everywhere |
| Shrine, path master | RAVEN-PLANNED exist; who and where GAP-COOKED 2.11 |
| Creatures | 8 names, roles and taming KEPT from FULL-GAME; a 9th added to meet Raven's 3/3/3 shape (CR); everything else GAP-COOKED 2.12 |
| The Risen (raised bone puppets of bosses and the Blood Moon) | GAP-COOKED 2.12.4 |
| 4 minis, 1 major | Names and premises kept; fights GAP-COOKED 2.13, 2.14 |
| Voices, music | GAP-COOKED 2.15, 2.16 (tempo, key and instruments SPEC 3.3) |
| Necromancy tree | Nodes SPEC (ST). Spell and Wildbrand numbers KEPT (combat). Souls, passives, stat nodes, root passive numbers GAP-COOKED 2.17 |
| Necro raising and the ritual | Raises SPEC (DD); ritual numbers KEPT (survival-crafting 2.22) |
| Body change | SPEC pale skin; frame KEPT (hero.md); stages GAP-COOKED 2.18 |
| Gear | Gravecloth KEPT; graveyard items GAP-COOKED 2.19 |
| Path raiding (necromancers raise the dead inside enemy bases) | SPEC (DD); numbers belong to survival-crafting 2.18 (R9) |
| Story through environment | GAP-COOKED 2.20 |
| Invasions Nature ↔ Necromancy | GAP-COOKED 2.21 |

### 2.1 Where it sits and how it reads (KEPT placement, GAP-COOKED look)

| Item | Detail | Tag |
|---|---|---|
| Place | B5 Rootlands, west of the Spine. x −2,650 (coast) to −1,100 (Spine foot), y +2,350 to +4,600. About 2.5 km² surface, about 2 km² of catacombs below | KEPT (world-map) |
| Neighbours | Volcanic to the north (ash-graves of smiths), Corrupted wasteland to the south (graves that end mid-row), the Ancient forest straight across Rootgrave (−1,150, +3,300) | KEPT |
| Camps | (−2,450, +2,750), (−2,450, +3,500), (−2,350, +4,300), each by the wreck of a castaway ship | KEPT |
| Bands | Lowland on the west coast (grave fields, camps), midland in the middle (mausoleums, bone pits, the crypt mouths), highland against the Spine (Chapel Heights, Rootgrave, the Lantern Stair). Bands squeeze, since the biome is narrow (world-map rule) | KEPT rule, layout GAP-COOKED |
| Ground | Rolling chalk downs falling to low sea cliffs. Thin turf over white chalk; where the turf is worn, the ground is bright white, so paths read from far off. The Spine behind is grey limestone | GAP-COOKED |
| Sky | Bruise-coloured: a violet-grey overcast with a sickly yellow-green rim at the horizon at dusk. Moonless at night (survival-crafting KEPT: no moon over the Graveyard and Void), so the only lights are lanterns, grave-light and your own | GAP-COOKED (moonless KEPT) |
| Trees | Black-green churchyard yews (some thousands of years old, hollow), leaning grey hawthorns, a few dead elms. No forest | GAP-COOKED |
| Readable night (world-map KEPT) | Tōrō stone lanterns along every old road, lit with pale-green grave-light that never goes out; a lantern every 30 m on roads and one on every great tomb. From 1 km you see the roads as dotted green lines | KEPT, detail GAP-COOKED |

**Palette (biome-wide):** bruise violet #4B4358, ash grey #8A8580, chalk white #E6E1D3, bone #D9CFB8, yew black-green #1F2A22, soul-green grave-light #9FE3C0, soulstone blue-green #2F7F86, and the path colour bone-green #A8B89A (hero KEPT). Rust-red #8A2A1A for the Blood Moon only.

### 2.2 Sub-zones (GAP-COOKED; names from FULL-GAME's list)

The biome is narrow, so four surface sub-zones of at least 0.6 km² fit, plus one underground sub-zone (world-map rule 1: at least 0.3 km² underground). FULL-GAME's five names all stay.

| # | Sub-zone | Where (box) | Size | Band | Ground layers | Palette (3) | Unique prop kits | Signature silhouette | Micro-hazard | Ambient bed |
|---|---|---|---|---|---|---|---|---|---|---|
| G1 | **Grave Fields** | x −2,650 to −2,150, y +2,500 to +4,500 (west coast strip) | 0.75 km² | Lowland | Chalk turf, grave soil (dark, freshly turned patches), white asphodel meadow | Turf grey-green #6E7A5E, chalk #E6E1D3, asphodel white #F2EFE6 | Rows of headstones in four styles (slab, cross-ring, tōrō-topped, wooden marker), iron mortsafes, lychgates, dry-stone walls | The Great Yew | Grave subsidence | Wind over grass, distant surf, crows, a far bell every few minutes |
| G2 | **Mausoleum Rows** | x −2,150 to −1,500, y +2,350 to +3,300 (north, midland) | 0.6 km² | Midland | Flagstone streets, moss on stone, ash drift (the Volcanic border) | Grey stone #8A8580, moss #4F5B3A, verdigris bronze #4E8C7A | Family mausoleums with bronze doors, obelisks, weeping-figure statues, a funeral road with carved stelae, the smiths' slag headstones at the north edge | The Belfry | none | Stone echo, wind through iron gates, wind chimes of bone |
| G3 | **Bone Pits** | x −2,150 to −1,550, y +3,700 to +4,600 (south, midland) | 0.6 km² | Midland | Chalk spoil, bone scree, lime crust | Lime white #EDEBE0, bone #D9CFB8, grey dust #9A958C | Open burial pits with bone heaps, lime kilns, the Watch House, wooden cranes and carts, plague-cross posts, grey withered edges at the Void border | The Watch House tower | Lime pits | Dry clatter of bone underfoot, flies, a kiln roar, Gravedigger's spade at night |
| G4 | **Chapel Heights** | x −1,750 to −1,100, y +3,300 to +4,300 (east, against the Spine) | 0.6 km² | Highland | Limestone pavement, yew litter, lantern-oil-stained flags | Limestone #B4AFA4, yew black-green #1F2A22, grave-light #9FE3C0 | Ruined chapel complex, cloister walls, a lantern pond, lantern racks, the Lantern Stair, Rootgrave's root-split tombs | The Chapel of Nhal (the super-landmark) | none | Organ-like wind through the ruined nave, dripping, faint keening on the wind |
| G5 | **Crypt Gates** (upper catacombs) | Under G1, G2 and G4, y +2,900 to +4,000, 0 to 40 m deep | 0.4 km² underground | Midland | Cut limestone, burial niches (loculi), wet bone dust | Limestone #B4AFA4, soulstone #2F7F86, candle wax #E8DDB0 | Family vaults, niche walls, watchers' guardrooms, wax-crusted Wickbeetle nests, iron grilles | The Watchers' Vault great door (seen from the main crypt mouth) | Crypt draught | Drip, distant water, wax crackle, moth wings |

**Blends (world-map rule 5):** 60 to 120 m. G1 → G2: headstones grow taller and closer and turn into tombs. G2's north edge carries the Volcanic border kit at half density (ash-graves of smiths with slag headstones, KEPT). G3's south edge carries the Void border (graves that end mid-row where the ground goes grey, KEPT).

### 2.3 Skyline landmarks (GAP-COOKED; 5, one per ~0.5 km², each read from 1 km)

| # | Landmark | Where | Silhouette (1 km) | Up close |
|---|---|---|---|---|
| L1 | **The Chapel of Nhal** (super-landmark, reads from 3 km) | G4, (−1,500, +3,800) | A roofless Gothic chapel on the heights: bare stone ribs where the roof was, and a broken bell tower leaning 8°. At night its windows burn grave-green with hundreds of lanterns inside | The major's arena (2.14). The lantern pond in front. The road to it lined with sealed graves with heartwood stakes in them (the counter, shown before the fight) |
| L2 | **The Great Yew** | G1, (−2,300, +3,150) | A black dome of a tree 30 m across on the only hill in the grave fields | Hollow trunk big enough to stand a camp in. Its roots grip a ring of headstones and lift them off the ground. Bones in the roots. A Gloamcrow roost. Lesser secret inside (2.6) |
| L3 | **The Belfry** | G2, (−1,800, +2,900) | A freestanding square bell tower 35 m high with an open top and a huge bronze bell visible against the sky | The Bell-Ringer's yard (2.13.1). The rope hangs through an arch to the ground |
| L4 | **The Bone Giant of Rootgrave** | G4, (−1,300, +3,250), the Graveyard side of the pass | A giant human-like skeleton 40 m long, slumped against the Spine, ribs the height of houses, with a forest of roots grown through the ribcage | The Grovekin's champion (world-map KEPT: "a bone giant on the Graveyard side of Rootgrave, grown through by roots"). Mourner lantern-poles are driven through its bones like spears (they killed it here). Gravehounds and Thornlynx fight under it at dusk (KEPT) |
| L5 | **The Lantern Stair** | G4, (−1,200, +4,150), climbing the Spine | A stone stair zig-zagging up the Spine face, lined both sides with tōrō lanterns, a green dotted line up the mountain at night that stops dead in mid-air | The Lantern Mourners' stair toward the Crown (world-map KEPT: every stair breaks off at 700 m). The last 50 m of lanterns are unlit. At the break: a single lantern set on the edge, facing up |

### 2.4 POIs: 58 surface + 60 in the catacombs = 118 (count KEPT from world-map; contents GAP-COOKED)

**Mix (world-map rule):** 5 landmarks, 5 shrines (4 surface + 1 underground, counted in the catacombs), 13 hand-made story sites, and about 36 kit POIs on the surface.

**Hand-made story sites (13, surface).** Coordinates are the site's centre.

| # | Site | Sub-zone, x, y | What's there (the story it tells, no words) | Gives |
|---|---|---|---|---|
| H1 | **The Lychgate Chapel** | G1, −2,350, +3,450 | A small intact chapel beside camp 2, its roof sound, candles burning, swept floor. Someone still keeps it | Great Shrine of the Lantern; Morwen, the path master (2.11) |
| H2 | **Row of the Named** | G1, −2,500, +3,000 | 120 headstones in perfect rows, each with a lantern on top and a name. Dig any of them: no body, a lantern with a paper name-tag inside | The cenotaph reveal; lore item **Lantern tag** |
| H3 | **The Dog Graves** | G1, −2,200, +2,700 | Small graves beside human ones, each with a collar hung on the stone. Two are dug out from below: the soil is pushed up, not down | First Gravehound den; tells you they rise from the ground |
| H4 | **The Ford of Washing** | G1, −2,250, +4,050 | A chalk stream ford where grave-clothes lie on the stones, half washed, a wooden beetle (washing bat) left beside them | Lore item **Washerwoman's bat**; Mournhare warren nearby |
| H5 | **Smith's Row** | G2, −1,900, +2,450 | Ash-graves on the Volcanic border: slag headstones, a smith's hammer laid on each grave, an anvil as a grave marker | The Mourners buried other peoples' dead too. Iron ×6 from the anvil |
| H6 | **The Seven Names** | G2, −1,750, +2,650 | A great mausoleum with seven name plates on its door, six filled in, one blank, the chisel left beside it | Lesser shrine (G2) |
| H7 | **The Broken Procession** | G2, −1,650, +3,150 | A funeral cart on the road with its coffin open and empty, the pall-bearers' lantern-poles dropped in a line pointing east toward the Lantern Stair | Points the way to the Stair |
| H8 | **The Watch House** | G3, −2,050, +4,250 | A three-storey stone watch tower over the bone pits, its door barred from the inside, the watchers' bones at the window, a tally of nights scratched on the wall that stops. Iron mortsafes stacked in the yard | The Gravedigger's day lair (2.13.4); lore item **Watcher's tally** |
| H9 | **The Lime Kilns** | G3, −1,850, +4,350 | Three lime kilns, one still warm, quicklime in barrows, a pit beside them white with lime and full of animal bones | Lesser shrine (G3); lime (2.7) |
| H10 | **Where the Rows End** | G3, −1,900, +4,580 | The southern grave rows run into grey dead ground and stop mid-row: dug graves waiting, with no stones and no lanterns | The Void border (KEPT). The rows were planned for more dead than came |
| H11 | **The Lantern Pond** | G4, −1,560, +3,700 | A still pond in front of the chapel, its surface covered edge to edge in drifted paper lanterns, some still lit | Night beauty shot; ghost carp (fishing, KEPT); the chapel's run-back stone 50 m west (−1,555, +3,800) |
| H12 | **The Root-Split Tombs** | G4, −1,250, +3,380 | At the Rootgrave mouth, forest roots have burst tombs open from below and lifted the coffins into the air | The opposite biome pushing in (world-map KEPT: "giant roots grip an old cemetery") |
| H13 | **Foot of the Stair** | G4, −1,350, +4,050 | The bottom of the Lantern Stair: a stone gate, a pile of travellers' sandals left at the foot, all one style | Lesser shrine (G4); everyone climbed, and nobody came down |

**Kit POIs (about 36, surface, generator-placed; world-map 2.9 types, 2 to 4 variants each):**

| Kit type | Count | Variants |
|---|---|---|
| Mausoleum (lootable) | 6 | Small family tomb, bronze-door tomb, collapsed tomb with a Gravehound den, tomb with a Wickbeetle nest |
| Mortsafe grave cluster | 5 | 3, 5 or 9 iron cages; one prised open |
| Crypt mouth (cave entrance) | 6 | These are the biome's 6 caves (rule: 3 to 6), each a stair down into G5 |
| Soulstone outcrop (exposed bone bed) | 5 | A grave slumped open on a slope, bones crusted in soulstone |
| Predator on a kill | 3 | Gravehounds on a Marrowgoat; Gloamcrows on a dead Palestrider; a Tombback burying a Mournhare |
| Wayside tōrō shrine (lantern niche with offerings) | 4 | Coins, a doll, a bowl, a lock of hair |
| Burned cottage / Ergot field (survival-crafting KEPT) | 1 | Black rye, a crooked scarecrow |
| Asphodel meadow (survival-crafting KEPT) | 1 | Pale lilies, a half-built bone altar |
| Ossuary kitchen (survival-crafting KEPT) | 1 | A crypt-keeper's hearth, mourner's bread gone to stone |
| Bone cairn on a rise | 2 | Skulls stacked as a waymark |
| Collapsed subsidence pit | 2 | A grave field fallen into the quarry below, a rope down (a back door into G5) |

**Underground: the catacombs' 60 POIs are in 2.5.**

### 2.5 Hidden sub-biome: the catacombs (KEPT "the catacombs, a deep maze of crypts under the grave fields"; layout GAP-COOKED)

**What they are.** The Mourners quarried the white limestone under the downs to build their tombs and chapel. When the surface graves filled, they carried the old bones down into the worked-out galleries and stacked them into walls. Three levels, about 2 km² in all, 9 km of main route (one POI per 150 m, world-map rule).

| Level | Depth | Area | What it is | Ore band (survival-crafting KEPT) | Creatures |
|---|---|---|---|---|---|
| **L1 Crypt Gates** (sub-zone G5) | 0 to 40 m | 0.4 km² | Family vaults, niche walls, watchers' guardrooms, grilled stairs down | Band 1 to 2: copper, tin, iron, coal seam, thin soulstone | Wraithmoth, Wickbeetle, Gravehound dens, Cryptstalker young |
| **L2 The Ossuary** | 40 to 100 m | 0.9 km² | Galleries walled with skulls and long bones in patterns, bone chapels, a sunken river | Band 3: rich iron, silver, gem clusters, thick soulstone **grave seams** | Cryptstalker, Wraithmoth, Gloamcrow (lost ones, nesting in skull niches) |
| **L3 The Old Quarry and Cisterns** | 100 to 160 m | 0.7 km² | Raw quarry galleries with pick marks, pillar-and-stall halls, three flooded cisterns, the Crypt Mother's brood gallery, the Starfall Pit | Band 3, and band 4 in the Starfall Pit (star iron) | Cryptstalker, Crypt Mother's brood, nothing else lives this deep |

**Entrances:** the 6 crypt mouths (kit POIs) plus 2 subsidence pits (rope down) and the chapel's crypt stair (inside Nhal's arena, opens after his first kill). All entrances lead into L1.

**Catacomb POIs (60).**

| Type | Count | Notes |
|---|---|---|
| Lesser shrine: **the Watchers' Vault** | 1 | L1, (−2,000, +3,300), −25 m. G5's lesser shrine. A great iron door; inside, chairs facing the door, lanterns, a spent ration crate |
| Mini arenas | 2 | The Chandelier Hall (L2, the Ossuary Lord), the Brood Gallery (L3, the Crypt Mother) |
| Hand-made rooms | 12 | Listed below |
| Kit rooms | 45 | Niche corridor with loot (8), family vault (6), grave seam of soulstone (6), silver vein (4), skull-wall gallery with a pattern puzzle (3), Wickbeetle nest (4), Cryptstalker web-less lair (strewn husks) (4), flooded stair (3), collapsed gallery with a squeeze (3), cistern bank (fishing) (2), draught corridor (2, the micro-hazard) |

| # | Hand room | Level | Idea of the room |
|---|---|---|---|
| C1 | **The Watchers' Vault** | L1 | Shrine (above) |
| C2 | **Niches of the Unnamed** | L1 | A 200 m corridor of empty niches, each with a lantern and no name. The only lit corridor in L1 |
| C3 | **The Draught Stair** | L1→L2 | A long spiral stair where a constant draught blows out torches (micro-hazard). A lantern or Wickbeetle is the key. Halfway down, a dead explorer with a torch in hand |
| C4 | **The Skull Wall of the Wheel** | L2 | A wall of skulls arranged in a twelve-spoked wheel with one spoke missing. Lore only (the twelve peoples) |
| C5 | **The Chandelier Hall** | L2, under (−2,000, +3,500), −80 m | Ossuary Lord (2.13.3) |
| C6 | **The Bone River** | L2 | An underground stream whose bed is all small bones, washed white. Soulstone glows on the bottom. Wading is safe; a Cryptstalker hunts the far bank |
| C7 | **The Sorting Room** | L2 | Stone tables with bones sorted by kind: skulls here, femurs there, a tally board. Bones of creatures too, sorted with the same care. Lore item **Ossuary ledger** |
| C8 | **The Starfall Pit** | L3, under (−1,700, +4,000), −150 m | A shaft from the surface sealed long ago; at the bottom a black meteorite fused into the quarry floor. Star iron ×3 per visit, respawns weekly. The deepest point in the biome (band 4) |
| C9 | **The Brood Gallery** | L3, under (−1,900, +4,100), −140 m | Crypt Mother (2.13.2) |
| C10 | **The Three Cisterns** | L3 | Flooded quarry halls joined by a sunken passage (swim 25 m). Fishing KEPT: ghost carp, lamprey, the Drowned Bell. A real bronze bell lies on the bottom of the third |
| C11 | **The Quarry Face** | L3 | Half-cut limestone blocks still in the wall, tools dropped mid-swing, a lunch bundle turned to dust. Work stopped all at once |
| C12 | **The Chapel Crypt** | L1, under the chapel | The stair from Nhal's arena. Twelve coffins, open, each holding only a lantern. A back way out to the surface for parties who beat Nhal |

### 2.6 Secrets (GAP-COOKED; 5)

| # | Secret | How it's found | Reward |
|---|---|---|---|
| S1 | **Inside the Great Yew** | The hollow trunk is dark; Gloamcrows circle the tree at dusk. A lantern shows a grave-slab in the floor among the roots; dig it (pick, 3 s) | A sealed coffin with a **Yew longbow blueprint** and the lore item **First Lantern** (the oldest grave in the biome: a dog's) |
| S2 | **The One Full Grave** | In the Row of the Named, a tamed Gloamcrow lands on exactly one stone (Gloamcrows sense full graves, 2.12). Dig it | The only body in the row: a Mourner buried with silver grave goods: **Mourning ring** (2.19) and silver ×6 |
| S3 | **The Unlit Lantern** | At the top break of the Lantern Stair, light the one unlit lantern on the edge (hold Interact 3 s with any flame) | Every tōrō on the Stair turns from green to warm gold for 1 game day; Corruption doesn't build on the Stair while they're gold; path XP 1,000 once |
| S4 | **The Seventh Name** | Bring the chisel from the Seven Names mausoleum to its blank plate and carve (hold 5 s) | The bronze door opens on the seventh crypt: **Sexton's pick** (a pick-hammer weapon, silver tier stats, 2.19) |
| S5 | **The Drowned Bell's Chamber** | In C10, dive under the bronze bell in the third cistern; there's an air pocket inside the bell | A dry chamber in the bell's mouth: the lore item **Bell-founder's mark**, soulstone ×8, a +2 upgrade shard ×4 |

### 2.7 Resources (GAP-COOKED; soulstone name KEPT)

| Resource | Where | Tool | Use |
|---|---|---|---|
| **Soulstone** (signature, never farmable: survival-crafting KEPT) | Grave seams in L1 to L3 (thick in L2), soulstone outcrops on the surface (bone beds), bone piles left by the Risen after the Blood Moon. Clear crystal with a pale-green inner glow on bone; darkens to blue-green in air | Pick (iron or better for grave seams) | Gravecloth, soul engine fuel (KEPT: 40 P for 1 h per soulstone), bone altar, Rune of the Grave (KEPT), the decay-resist potion (KEPT: Asphodel + soulstone), Tombback food (as dust), necro-element weapons (path material) |
| **Soulstone dust** | Grinder (survival-crafting): 1 soulstone → 4 dust | Grinder | Tombback taming food (FULL-GAME KEPT "offer soulstone dust"), undead tame upkeep (KEPT 1 dust per real day) |
| **Bone** | Everywhere: bone piles, bone pits, Risen, carcasses | Hand, any tool | Bone meal (KEPT grinder), bone altar, soul engine, Marrowgoat food, Wickbeetle-wax candles |
| **Yew wood** | Churchyard yews in G1 and G4 (slow: one yew gives 6 logs, regrows in 2 game days) | Axe | Yew longbow (2.19), bow upgrades. Real longbow wood |
| **Limestone** | Quarry walls in L3, Spine foot in G4 | Pick | Stone building (gives stone ×1.5 per hit vs normal rock), the lime kiln |
| **Quicklime** | Lime kilns (H9) or your own kiln (workbench L2: stone 20, clay 10; 4 limestone + 2 charcoal → 2 quicklime) | Kiln | Lime pot (Ossuary Lord counter, 2.19), mortar (stone pieces +10% HP), tanning |
| **Grave iron** | Iron mortsafes (scrap) | Pick or hammer, 4 hits each | Iron ×2 per mortsafe. Lets a Graveyard starter reach iron early without caves (and pulls players out to the cages at night) |
| **Silver** | L2 silver veins (band 3, survival-crafting KEPT) | Iron pick or better | Rings, lanterns (KEPT), Mourning ring, silvered weapons (2.19) |
| **Wax** | Wickbeetles (tamed: 1 wax per 20 min) and Wickbeetle nests | Hand | Candles (comfort Light 1, KEPT), lantern seal, waterproofing |
| **Black rye, Asphodel** (crops, survival-crafting KEPT) | Wild patches in G1, the Asphodel meadow | Hand, sickle | Mourner's bread, ergot narcotic; Asphodel for decay resist, Cleansing draught and the undead ritual |
| **Star iron** | The Starfall Pit (C8) only, in this biome | Steel pick | Starsteel (KEPT: 2 steel + 1 star iron + 1 core shard). R4 is the starsteel round, so the biome holds a sure (weekly) source |

### 2.8 Hazards

**Main hazard: darkness and corruption (SPEC DD; numbers KEPT from survival-crafting 2.3 and 2.5).**
- **Unlit** players are hunted: Gravehounds, Wraithmoths and Cryptstalkers get double aggro range on them (SPEC 3.3, KEPT).
- **Corruption** builds +1 per 6 s at night or underground, +1 per 15 s by day on the surface; stops within 10 m of a brazier, campfire or Hearth; a Wickbeetle or lantern halves it. Steps at 25 / 50 / 75 / 100 (healing −10%; stamina regen −15% and treated as unlit; max stamina −20 and whispers; Marked). It falls −1 per 3 s outside the biome and halves on Rest; a Cleansing draught removes 60.
- **Necromancers** in the Graveyard take no corruption penalties and get +3 / +6 / +10 / +15% path damage at 25 / 50 / 75 / 100 (KEPT, DD "minor buffs if the biome matches your path").
- **How it reads (GAP-COOKED):** at 50, colour drains from the edges of your view by 15%; at 75 the whispers (a planned layer of breathy, wordless keening, KEPT) come in under the music at −24 LUFS; at 100 the tōrō lanterns near you flicker as you pass (other players see it too: a Marked player is visible).

**Micro-hazards (GAP-COOKED; 3; each ≥ 0.9 s sound plus a small visual cue; each can kill).**

| Hazard | Where | Tell | Effect | Counter |
|---|---|---|---|---|
| **Grave subsidence** | G1 (and kit subsidence pits) | 1.0 s: the turf cracks in a ring around you, chalk dust puffs, a dull crunch from below | The grave collapses into the quarry: a 3 m drop into a 4 m pit (6 damage from the fall rule, KEPT: no damage under 6 m) with a 50% chance of a sleeping Gravehound den (2 to 3 hounds wake). Climb out on the far side (mantle 1.8 m) | Walk on paths and on stone. Fresh-turned dark soil marks risk (12% of grave soil cells). A Gloamcrow tame screams before you step on one |
| **Lime pits** | G3 | 0.9 s: the white crust crackles and hisses under your foot, a puff of white steam | Wading quicklime slurry: 8 damage a second and armour durability −1% a second; rain makes it boil (×2 damage) | Walk round; the plank bridges over them (freeform, KEPT style); a Tombback mount (resists) |
| **Crypt draught** | G5 and L2 draught corridors (C3, kit draught corridors) | 1.2 s: a rising whistle and every flame leans hard one way | A gust every 8 to 15 s puts out held **torches** and campfires within the corridor (lanterns, Wickbeetles and grave-light are fine). You're unlit until you relight (a torch relights by holding Interact at any flame for 1 s). Unlit = double aggro from the dark-hunters below | Lantern (KEPT, workbench L2), Wickbeetle, or fight in the dark |

**Creatures resist their own biome's hazard (CR SPEC):** every Graveyard creature ignores corruption, lime and subsidence (they know the ground), and none of them is a dark-hunter to another.

### 2.9 Weather (weights KEPT from world-map; effects reconciled)

world-map's weights stand: Clear 15, Overcast 35, Drizzle 15, Ground mist 35. survival-crafting's **Corpse fog** becomes the night form of the Ground mist (same as biome-swamp's reconciliation).

| State | Weight | Effect | Look and sound |
|---|---|---|---|
| Clear | 15 | Normal | Bruise sky by day, starless black at night with the lantern roads glowing |
| Overcast | 35 | Normal | Low violet-grey ceiling; crows louder |
| Drizzle | 15 | Torches burn half as long (KEPT torch rule); lime pits boil (×2) | Fine rain sheets, dripping stone, rain on yew needles |
| **Ground mist** (signature) | 35 | **By day:** knee-high mist; vision normal. **At night = Corpse fog (KEPT):** mist rises to chest height, every light radius ×0.7, and Gravehounds rise out of it (each Gravehound pack spawn in G1 and G3 can rise from any mist cell within 40 m of you instead of a den) | Mist that clings to the low ground so headstone tops and lanterns float on it. Counter (KEPT): bigger lights, Gravecloth 4 |

Weather never stops the music (KEPT).

### 2.10 Event: the Blood Moon (name SPEC, DD "a blood moon over the graveyard"; behaviour GAP-COOKED in the fire-frost frame)

One game day (60 real minutes), about once every 3 real days (server setting Biome events, KEPT). Pays soulstone at triple rate (SPEC 3.3). Never touches spawn safe zones (KEPT).

| Time | What happens |
|---|---|
| −5 min (sign) | The Graveyard's sky turns rust-red #8A2A1A and a red moon rises over the chapel. It's the only moon this biome ever has, so everyone knows. The Lantern Mourners' pillar on the Crown's Ring burns rust-red (world-map KEPT). Every bell in the biome tolls once, by itself. Music: the event layer fades in (2.16) |
| 20:00 to 05:00 game time (about 22 real min), **the Rising** | Ground mist is forced on (Corpse fog). Corruption builds ×2. **Processions** of the Risen (2.12.4) climb out of the grave fields: 4 to 8 Risen behind a lantern-bearer, walking the old roads at 2 m/s toward the nearest light within 600 m. Spawns: one procession per 150 m of road in G1 and G3, re-forming 6 min after it's destroyed. **At spawn camps** they stop at the safe-zone edge and stand in silence facing the camp until dawn (no attack; a terrifying ring of bone). **Players' lights:** a procession turns aside to any player carrying a light within 60 m and attacks. **Claims inside the Graveyard:** processions march on lit claims and attack the walls and Hearth as a creature raid (the built `WRRaids` frame, so it follows the server's Creature raids setting; Off means they only stand vigil outside the claim, never attack) |
| Same window | **The Gravedigger** walks a longer route with two more stops. **The Bell-Ringer** tolls every 15 s instead of 20 |
| Same window, Necromancers | Souls from kills ×2; combat raises last ×2 (60 s); Risen killed by a Necromancer's raised minion drop a Soul too |
| 05:00 (dawn) | Every standing Risen collapses where it stands into a **bone pile** with soulstone crusts (the crystal turns blue in the morning light). Each pile: bone ×4, soulstone ×1 to 3 (×3 event rate already applied) |
| 05:00 to the end of the game day, **the Mourning** | The red fades. Mist lies thick until noon. Bone piles stay 30 game minutes. Any grave seam mined pays ×3 soulstone. The Lantern Pond's lanterns are all lit, and grave-light is brighter (lights the roads at ×1.5 radius) |

**Server notes:** at most two biome events at once (world-map KEPT). PvE servers: processions never target claims, only lights and players.

### 2.11 Shrines and the path master (RAVEN-PLANNED exist; who and where GAP-COOKED in the fire-frost frame)

| Shrine | Where | Guards (2 packs, one midland) |
|---|---|---|
| **Great Shrine of the Lantern** | The Lychgate Chapel (H1), G1, (−2,350, +3,450), beside camp 2 | none (path master) |
| The Seven Names | G2, (−1,750, +2,650) | Gravehound pack (3); Wraithmoths (2) at night, Palestrider herd by day (neutral, avoidable) |
| The Lime Kilns | G3, (−1,850, +4,350) | Gravehound pack (4); a Tombback |
| Foot of the Stair | G4, (−1,350, +4,050) | A Cryptstalker (highland, solo) and Wraithmoths (2) |
| The Watchers' Vault | G5, (−2,000, +3,300), −25 m | Gravehound den (3) and a Cryptstalker young pair |

**The draft put the Great Shrine inside the ruined chapel. It moves to the Lychgate Chapel**, because the ruined chapel is Nhal's arena and a path master can't stand in a boss lair. The altar for the Necromancy subclass stays in Nhal's arena (BO SPEC: the arena holds the altar).

Fire-frost's shrine rules are KEPT: claim by killing the guards and lighting the brazier (here a tōrō lantern) for 3 s; lesser shrines pay 1,000 path XP to Necromancy if you own it, otherwise 500 to each path you own; the Great Shrine pays 2,000 on first visit; fast travel between discovered shrines, not in combat; re-guard every 4 game days.

**Path master: Morwen, the Lantern-keeper (GAP-COOKED; FULL-GAME's "gravedigger" becomes the roaming mini's role, so she's the sexton who keeps the lights instead).**

| Part | Detail |
|---|---|
| Who | The last newcomer who stayed. She came on one of the castaway ships, found the chapel's lanterns still burning, and has kept them since. She doesn't know who lit them first |
| Look | MetaHuman, late 20s, tall and strong-shouldered from lifting stone, a long black braid, striking pale features (she's a 100-point Necromancer, so the full ash-pale body change shows on her, 2.18: a living example of where the path goes). A fitted black waistcoat over a loose white shirt open at the throat with the sleeves rolled to the elbow, a leather apron, high boots, a lantern on her belt, soulstone earrings. Attractive and unbothered by the dead |
| Idle | Trimming wicks along the lantern rack, carrying a lantern to the Row of the Named at dusk, sitting on the lychgate step at dawn |
| How she talks | Short lines within 6 m, no camera grab (KEPT rule). Dry, calm, a little amused |
| Quests (6; pay path XP plus a recipe or secret; the last needs the third mini, KEPT rule) | **1. Keep the light:** light 5 tōrō on the Row of the Named at night (teaches lights and corruption) → lantern recipe if unknown, 1,500 XP. **2. The first raise:** kill a Gravehound and raise it (teaches Raise dead; non-Necromancers: bring a Gravehound's remains) → 1,500 XP. **3. Iron for the dead:** break 6 mortsafes → grave iron route, Lime pot recipe. **4. What's under the yew:** points at S1 without saying it ("the oldest one has roots for a roof") → 2,500 XP. **5. Count the bones:** bring the Ossuary ledger from the Sorting Room (C7) → Bone altar recipe, 3,000 XP. **6. The one who digs:** kill the Gravedigger (the roaming mini) and bring his spade's lantern → Heartwood stake recipe (Nhal's non-Nature counter), 5,000 XP |

**Run-back stones (KEPT rule, 40 to 60 m from every arena door):** Nhal (−1,555, +3,800), west of the chapel door. Bell-Ringer: outside the Belfry yard gate (−1,850, +2,900). Ossuary Lord: in the gallery before the Chandelier Hall. Crypt Mother: on the stair down to the Brood Gallery. Each is a small tōrō lantern on a post.

### 2.12 Creatures (8 names, roles and taming KEPT from FULL-GAME; the 9th added; everything else GAP-COOKED)

**Rules (CR SPEC):**
- Raven's first-playable shape is 3 aggressive, 3 neutral, 3 passive. The draft had 3 / 3 / 2, so one passive is added: **Marrowgoat**.
- Every Graveyard creature ignores corruption, lime and subsidence (resists its own hazard).
- Neutral herds defend each other. Each creature tames only by its listed method (DR #16). Necromancers can raise any of them after death (CR SPEC).
- Tiers follow 3.3: hard aggressive = highland, medium aggressive = midland, passives = lowland.
- Every attack shows the real jaw, palp, wing or body, and damage lands only on visible contact. No attacker limit: packs come all at once.
- **No zombies (DD SPEC).** These are living animals that look deathly for real biological reasons. Only the Risen (2.12.4) are actually dead, and they're clean bone.

#### 2.12.1 The nine at a glance

| Creature | Tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Gravehound** | Midland, pack 3 to 5 | Xoloitzcuintli (hairless) body at wolf-plus size with a greyhound's deep chest; skin so thin and dark that the skull, the spine ridge and the ribs show through; the bare face is bone-pale like a horse skull. Sleeps buried in grave soil (church grim) | A lean, deep-chested hound with a bone-white skull face, a knuckled spine ridge and long legs | Skin slate-black #2A2A2E, face and spine bone #D9CFB8, eyeshine soul-green #9FE3C0 | 1.1 m at the shoulder | Knockout and feed, or necro path magic. Medium | Raw meat ×3, Gravehound hide ×2 (thin, supple), bone ×2 |
| **Cryptstalker** | Highland, 1 (2 in L3) | Amblypygid (whip spider) at 4 m leg span: flat body, whip-thin sensing front legs 5 m long, spined raptorial palps that snap shut; eyeless; runs sideways like a crab; clings to walls and ceilings | A flat, wide shape on the wall with two impossibly long whips feeling the air, and folded spiked arms at the front | Chalk-grey #B4AFA4 mottled like limestone, palps darker #5A5248, spines bone-white | 4 m span, 0.8 m tall | Knockout and feed. Hard | Cryptstalker carapace ×2 (light armour plate), palp spines ×4 (arrowheads), raw meat ×2 |
| **Wraithmoth** | Midland, 1 to 3, dusk and night | Death's-head hawkmoth at 2.6 m span: the skull pattern on the thorax, a thick furred body, narrow swept wings; a tear-drinking moth's proboscis; drawn to light | A swept-wing moth the size of a door, with a skull on its back that faces you as it dives | Wings ash-brown and grey #6E655C with bark mottling, thorax skull bone #D9CFB8 on black, underwings bruise violet #4B4358 flashing in flight | 2.6 m span | Knockout and feed. Medium | Wraithmoth scales ×3 (alchemy: stamina potions), silk ×2 |
| **Palestrider** | Midland neutral, herd 2 to 4 | Tall lean horse (Akhal-Teke frame), grey-white coat; the face is hairless and so thin-skinned the skull shows bone-pale, with dark hollow nostrils and pale eyes (Mari Lwyd); long ragged black mane and tail | A gaunt white horse with a skull for a face and a black mane like torn ribbons | Coat ash-white #D8D4CC, face bone #E6E1D3, mane black #141414, eyes pale grey | 1.9 m at the withers | Knockout and feed. Medium | Raw meat ×6, pale hide ×3, horsehair ×4 (bowstrings) |
| **Tombback** | Midland neutral, 1 to 2 | Burying beetle (*Nicrophorus*: black with orange bands) at 3 m long; it has cemented a grave slab to its back with a waxy secretion and buries carcasses under it | A huge black beetle with a headstone on its back | Black #151515, bands rust-orange #B5562A, the slab grey stone with moss and a worn name | 3 m long, 1.8 m tall with the slab | Offer soulstone dust. Medium | Beetle shell ×3, bone ×4; if killed, its slab (decor piece "Tombback's stone") |
| **Gloamcrow** | Lowland neutral, group 3 to 8 | Hooded crow with a jackdaw's pale eyes and a grey hood, at raven size | A grey-hooded black crow with pale staring eyes | Black #121212, hood ash-grey #9A958C, eyes silver-white | 1.3 m span | Offer food. Easy | Feathers ×3 (arrows), raw meat ×1 |
| **Mournhare** | Lowland passive, 1 to 4 | Mountain hare in a grey coat; sits up and "moon-gazes" at dusk | A tall grey hare sitting upright, ears straight | Grey #8E8A84, white belly, black ear tips | 0.7 m | Offer food. Easy | Raw meat ×1, Mournhare fur ×2 (KEPT: Gravecloth) |
| **Wickbeetle** | Lowland passive, 2 to 6 | A 50 cm beetle whose back secretes white wax (real wax-secreting insects), with a firefly light organ at the tail that glows candle-yellow | A walking candle: a waxy white dome with a warm glow at the back | Wax #E8DDB0, legs black, glow #FFC870 | 50 cm | Steal an egg (wax-cell egg clusters in mausoleums and L1). Easy | Wax ×2 |
| **Marrowgoat** (new) | Lowland passive, herd 3 to 6 | Long-haired pale goat, horns sweeping back and down; gnaws old bones for minerals (real osteophagia in goats, sheep and deer) | A shaggy white goat with a bone in its mouth | Hair chalk-white #E6E1D3, horns #3A3530, gold eyes with goat pupils | 0.9 m | Offer food (it eats bones: bone ×3). Easy | Milk (farm), goat hair ×2 (felt), raw meat ×2 |

**Path magic on the Gravehound (GAP-COOKED in Raven's "calm a creature of the same element" frame).** On a Gravehound that's knocked out, or below 30% HP with decay proc'd, hold Interact for 4 s with 10 Souls: the Souls flow into it, which counts as a full meal of narcotic, and taming continues as knockout-and-feed.

**Utilities and mounts:**
- **Palestrider (mount): it never tires.** Ridden sprint 1,350 cm/s (survival-crafting band) with **no stamina drain**, all night. It won't enter water over 1.2 m and rears for 1 s at fire within 3 m.
- **Tombback (hauler):** carries 700. Its slab is cover: standing behind a stopped Tombback blocks projectiles (the slab has 600 HP, regrows 10 a second). A tamed Tombback at base **buries carcasses** within 20 m: each becomes **remains** (the necro ritual item, KEPT) and bone meal ×2 after 10 min.
- **Gloamcrow (utility): finds the dead and the hidden.** Circles over any secret, buried cache or full grave within 40 m (a slow circle you can see from far off), lands on full graves, and screams 1 s before you step on subsidence ground.
- **Wickbeetle (utility):** light 12 m aura (KEPT) that never goes out in draughts or rain; 1 wax per 20 min.
- **Marrowgoat (farm):** milk (KEPT pen system, fed bones), hair for felt; a pen of them clears bone piles at a base (1 bone per 10 min).

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55. Decay is combat's decay status (KEPT).

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Gravehound** (mid, pack) | **Rising** (opener) | 0.95 (the soil heaves in a 1 m ring, two soul-green eyes open in the dirt, a dry rattle of breath) / 0 / 0.25 / 1.0 | 1.2 | 35 (3) | From buried, within 500 cm; parryable; decay 15 | Night or from a den; in Corpse fog it can rise from any mist cell within 40 m |
| | Bite | 0.42 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable; decay 15 | 40% → Bite 2 (0.40 / 0.2 / 0.15 / 0.70) |
| | **Hamstring** | 0.55 (circles to your flank, head low, a growl dropping in pitch) / 0.2 / 0.15 / 0.80 | 0.8 | 25 (2) | Only from behind or the side; stamina −15 and sprint broken 1.5 s | Flankers use it while a packmate faces you |
| | Pounce | 0.75 (crouch, the spine ridge flexes up like hackles) / 0.3 / 0.25 / 1.0 | 1.4 | 45 (4) | 300 to 900 cm; knockdown on hit; parryable | → Bite on a downed target |
| | **Grim howl** (call) | 0.80 (head back, the skull face points at the sky) / 0 / — / 0.8 | — | — | Every buried hound within 40 m rises (Rising opener) | Once per fight. A pack you hear howling is bigger than the one you see |
| | readsAction | Pounce with no hold within 0.4 s of a drink | | | | |
| **Cryptstalker** (high) | **Whip sweep** (sensing) | 0.60 (the whips draw back and fan out) / 0 / 0.5 / — | 0 | — | 360°, 500 cm. A touch means you're **found** (a click-lock sound): its next attack comes with no delay hold | Not damage. In the dark it attacks only what it has touched or heard (sneaking matters) |
| | **Palp snatch** (grab) | 0.85 (palps open wide, spines splay, crimson glint on the spines) / 0.3 / 0.20 catch | 1.2 catch, then 15/s for 2 s | 60 (6) | `grab`, `unblockable`; dodge only; reach 350 cm | Pins you to the nearest wall. Ally hits on the palps 150 damage free you. Mash −0.15 s per press |
| | Palp strike ×2 | 0.45 / 0 / 0.15 / 0.60, then 0.40 / 0.2 / 0.15 / 0.90 | 1.0 / 1.1 | 40 (4) | parryable; pierce | 30% → Lateral rush |
| | **Ceiling drop** | 1.00 (chalk dust sifts from the ceiling over you, dry clicking above) / 0 / 0.30 / 1.30 | 1.6 | 55 (5) | From ceiling or wall above; impact 250 cm; underground and in tombs only | Opener in L2 and L3 |
| | Lateral rush | 0.50 (body flattens to the floor) / 0 / 0.40 / 0.90 | 1.1 | 45 (4) | Runs sideways through you; knockback 400 sideways | Ends on a wall and climbs (out of melee 2 s) |
| | readsAction | Palp snatch with no hold within 0.4 s of a drink or charged heavy | | | | |
| **Wraithmoth** (mid, flyer) | **Light dive** | 0.90 (wings fold, the thorax turns so the skull faces you, a rising squeak) / 0.2 / 0.30 / 1.4 grounded | 1.2 | 30 (3) | Targets the brightest light within 30 m first. A hit puts out a held torch | Grounded 1.4 s = the bring-down window (combat template) |
| | **Scale dust** | 1.00 (wings beat hard in place, pale dust lifts off them) / 0 / 0.8 cone / 0.8 | 0 | — | Cone 6 m × 50°, not a projectile, blocked by cover; stamina −25 and **chilled** (stamina regen ×0.6 for 6 s) | FULL-GAME "drains warmth and stamina" |
| | **Tear-drinker** (grab) | 0.60 (it lands and walks to you, wings half open, proboscis uncoiling) / 0 / 0.20 catch | 6/s and stamina 8/s for 2.5 s | — | `grab`; **only on a knocked-down or staggered player**; your view darkens to a sliver while it's on your face | Mash −0.2 s per press; any hit on it removes it; fire burns it off |
| | Wing buffet | 0.45 / 0 / 0.20 / 0.70 | 0.7 | 25 (2) | push 300 cm | Close range |
| **Palestrider** (mid, neutral) | Rear and strike | 0.60 (rears, forelegs high, a shrill scream) / 0.2 / 0.20 / 0.80 | 1.2 | 40 (4) | parryable | — |
| | Double kick | 0.45 (weight shifts forward, ears flat) / 0 / 0.15 / 0.70 | 1.3 | 45 (4) | `behindOnly`, 250 cm | — |
| | Charge | 0.90 (stamps twice, head low) / 0 / 1.0 run / 1.20 | 1.5 | 50 (5) | knockback 700; it never tires, so it charges again after 3 s | Hits a wall: stunned 2 s |
| | **Death scream** (rally) | 0.70 / 0 / — / 0.5 | — | — | Herd within 30 m turns hostile | Plays when one is hit |
| **Tombback** (mid, neutral) | **Slab slam** | 1.00 (rears on its back legs, the headstone tips forward over you) / 0.3 / 0.25 / 1.20 | 1.6 | 55 (5) | Impact 300 cm; guard-breaking | — |
| | Horn shove | 0.50 / 0 / 0.20 / 0.70 | 1.0 | 35 (3) | push 400 cm | — |
| | **Carrion spray** | 0.80 (abdomen tips up, a wet click) / 0 / 0.40 cone / 0.80 | 0.3 | — | Cone 5 m × 40° behind it, not a projectile; decay 30 | The real burying-beetle defensive secretion |
| **Gloamcrow** (low, neutral) | Mob dive | 0.60 (caw, wings fold) / 0 / 0.20 / 1.0 | 0.5 | 10 (1) | The whole group dives in turn | Only when one is hurt (crow "funeral" mob) |
| **Mournhare** (low) | Boxing kick (cornered) | 0.45 / 0 / 0.12 / 1.0 | 0.4 | 10 (1) | — | Flees zig-zag |
| **Wickbeetle** (low) | Wax squirt (cornered) | 0.60 / 0 / 0.20 / flees | 0 | — | Slowed ×0.8 for 3 s | Flees, glow pulsing |
| **Marrowgoat** (low) | Butt | 0.55 (head lowers) / 0 / 0.15 / 0.90 | 0.6 | 20 (2) | push 300 cm | Herd rule: they butt together |

**Physical resist:** Cryptstalker carapace slash 0.85 / strike 1.2 (it cracks). Gravehound strike 1.2. Tombback: frontal hits land on the slab (×0.3) unless it's rearing (belly ×1.3). **Lock points:** Cryptstalker body and palps; Tombback head and belly; Palestrider head.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table)

| Sub-zone | Day | Night |
|---|---|---|
| G1 Grave Fields | Mournhare 16, Marrowgoat 12, Gloamcrow 12, Palestrider 6, Gravehound 2 (dens, asleep) | Gravehound 16 (rising), Wraithmoth 8, Mournhare 8, Gloamcrow 4 (roosting; wake on noise) |
| G2 Mausoleum Rows | Palestrider 12, Tombback 8, Gloamcrow 8, Wickbeetle 6 | Gravehound 10, Wraithmoth 10, Tombback 6, Wickbeetle 8 |
| G3 Bone Pits | Tombback 10, Marrowgoat 10, Gloamcrow 10, Gravehound 4 | Gravehound 16, Tombback 6, Wraithmoth 6 |
| G4 Chapel Heights | Palestrider 8, Gloamcrow 6, Gravehound 6, Cryptstalker 3 (inside tombs) | Cryptstalker 8, Wraithmoth 12, Gravehound 10 |
| G5 Crypt Gates (night table 22:00 to 04:00, KEPT underground rule) | Wickbeetle 12, Wraithmoth 6 (roosting), Gravehound 6 (dens), Cryptstalker 4 (young, midland stats) | Wraithmoth 12, Cryptstalker 8, Gravehound 8, Wickbeetle 8 |
| L2 / L3 | Cryptstalker 10, Wraithmoth 8, Wickbeetle 4 | Same, Cryptstalker 14 |

**Living-world beats (built AI, WRWildlife):**
- Gravehound packs run down Mournhares and Marrowgoats at dusk; Marrowgoat herds butt back together (herd rule).
- Tombbacks walk to fresh kills, bury them under their slab and sit on them for 10 min. You find mounded slabs on the fields in the morning.
- Gloamcrows gather and fall silent over any dead creature (the crow funeral), then mob whatever killed it. A silent ring of crows on a headstone means a body.
- Wraithmoths gather on lanterns along the roads at night and fly at any torch that passes. By day they roost on the chapel walls and on Nhal.
- Palestriders graze the asphodel and run the downs all night.

#### 2.12.4 The Risen (raised bone puppets; GAP-COOKED; DD's "bone, shade and carrion, never zombies")

The Risen are not a species. Only the Bell-Ringer, Nhal and the Blood Moon raise them. They're bones knitted with threads of pale-green grave-light and bound with iron funeral bands, with **no flesh at all**. They can't be tamed. When killed they fall into a bone pile (bone ×1 to 2, 20% soulstone dust ×1) and **leave no corpse**, so they can't be raised again. (The Necromancy path raises the dead creature itself, 2.17, not Risen.)

| Risen | Stats | Look | Attacks (W / H / A / P, dmg ×) |
|---|---|---|---|
| **Risen mourner** | Lowland (70 HP, hit 12); Blood Moon: midland | A human skeleton in grave wrappings carrying a tōrō lantern on a pole | Pole sweep 0.60 / 0 / 0.20 / 0.80, ×1.0, 220° · Lantern jab 0.50 / 0 / 0.15 / 0.70, ×0.8, decay 15 · **Grave grasp** 0.80 (it kneels and plunges a hand into the ground) / 0 / 0.30: a bone hand rises under you, rooted 1 s, `low` (jump clears it) |
| **Risen hound** | Lowland | A Gravehound skeleton, grave-light in the eye sockets | Bite 0.45 / 0 / 0.15 / 0.70, ×1.0 · Pounce 0.75 / 0.3 / 0.25 / 1.0, ×1.3 |
| **Risen bearer** | Midland (120 HP, hit 20) | Two fused human skeletons carrying a coffin between them as a ram | **Coffin ram** 0.90 (they lower the coffin and lean in) / 0 / 0.8 run / 1.20, ×1.5, knockback 600 · Coffin slam 1.00 / 0.3 / 0.25 / 1.20, ×1.6, guard-break |

Strike damage ×1.3 on all Risen (bone breaks). Necrotic damage does ×0.7 to them. Nature roots hold them twice as long.

### 2.13 Mini-bosses (premises from the FULL-GAME draft kept; fights GAP-COOKED)

Rules (KEPT, combat 2.17): 3,000 solo HP, +80% per extra player or tame; stance 90; 2 parries; 4 to 5 attacks plus one counter mechanic; chase 65 m; respawn 4 h; drop upgrade shards ×4 and a trophy (BO). Trophies become talismans (survival-crafting KEPT: Bell clapper = corruption builds 20% slower; Mother's fang = +15% damage vs swarms; Ossuary crown = necro raises last 20% longer; Gravedigger's spade = mining and digging +15%). Specials 0.65 to 1.2 s windup; projectiles ≥ 0.9 s warning, max 1,600 cm/s, never home, nothing inside 4 m.

#### 2.13.1 The Bell-Ringer (the Belfry; folklore: the passing bell and the death knell)

The draft's "without the rope he's a weak old man" becomes "without the rope he can't raise anything, and his stance breaks easily." He stays terrifying.

| Part | Detail |
|---|---|
| Body | A hunched man-shape 4.2 m tall with arms that reach his knees, in a sexton's coat gone to rags. His head is hidden inside half of a cracked bronze bell worn as a helmet, two eye-slits cut in it, green light inside. A hand-bell the size of a barrel hangs on a chain at his belt |
| Silhouette | A bell-headed hunchback with ape-long arms, always near the rope |
| Arena | **The Belfry yard** (−1,800, +2,900): a walled churchyard 50 m across round the 35 m tower; 16 grave mounds; 6 tall headstones (cover). The great bell's rope hangs through an arch in the tower's base. Its **weak point** is a frayed section 4 m up, glowing faintly where grave-light runs up the rope |
| Voice | Bull walrus groan, a gorilla hoot pitched down, the hum of bronze (his breath rings in the helmet); the toll is a real 2-tonne church bell |
| Lock points | Head (the bell helm), chest; the rope is a separate target |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **The Toll** (raise) | 1.20 (he wraps the rope round his forearm and leans back; the bell above swings; a deep hum rises before the strike) / 0 / 0.40 / 1.0 | 25 + stagger in a ring 8 m round the tower base | **3 Risen mourners** climb out of yard graves within 15 m. Every 20 s while the rope holds (15 s in the Blood Moon). Max 9 Risen alive |
| Hand-bell bludgeon ×2 | 0.65 / 0 / 0.20 / 0.60, then 0.75 / 0.3 / 0.25 / 1.0 | 35 / 50 | Parryable; the second is a downward smash (impact 200 cm) |
| Rope lash | 0.85 (he pulls a loop of slack and swings the iron counterweight) / 0.3 / 0.30 / 0.9 | 40 | 9 m arc in front; only while the rope holds |
| Headstone throw | 1.00 (wrenches a headstone up and hauls it over his head) / 0 / — / 0.9 | 45 | Projectile 1,500 cm/s, ballistic, never homes; min 4 m; the stone stays as new cover |
| **The Knell** (grab) | 1.00 (lunges, one long hand open; crimson glint on the hand-bell's lip) / 0.2 / 0.20 catch | 20 + 1.5 s stun + decay 40 | `grab`, `unblockable`; he lifts you and rings the hand-bell against your head three times |

**Counter: cut the rope.** The frayed section has **400 HP**: slash ×2, fire ×2, arrows and spells hit it, jump attacks reach it. When it parts, the great bell gives one cracked note and falls silent, and he tears the hand-bell off his belt:
- No more Tolls, so no more raises. Risen already up stay up.
- His stance drops from 90 to 60 for the rest of the fight (2 parries still break it).
- He gains **Hand-bell ring** 0.90 / 0 / 0.30 / 1.0: 15 damage and a stagger 5 m round him, no raise.

Drops: shards ×4, **Bell clapper** (trophy), bronze ×6, lore item **Bell-founder's tally**.

#### 2.13.2 The Crypt Mother (the Brood Gallery, L3; biology: amblypygid mothers carry their young)

| Part | Detail |
|---|---|
| Body | A Cryptstalker with a 9 m leg span and 7 m whips, her flat back covered in a living cloak of about 20 pale young clinging on. Her palps are as long as a man and lined with spines like a harrow |
| Arena | **The Brood Gallery:** a quarry gallery 60 × 30 m with an 8 m ceiling and 6 square stone pillars. **8 egg sacs** (pale, veined, person-sized) are glued to walls and pillars 1 to 3 m up. Full dark except soulstone glow in the floor cracks; you bring the light |
| Voice | Scorpion stridulation, a stag beetle's dry chitter amplified, wet palp clicks; the whips whistle when they sweep |
| Lock points | Body, the two palps |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Whip sweep** | 0.70 (whips draw back) / 0 / 0.6 / — | 15 | 360°, 700 cm; a hit **finds** you: her next attack has no delay hold |
| **Palp snatch** (grab) | 0.90 (palps open, crimson glint on the spines) / 0.3 / 0.25 catch | 30 + 18/s for 2 s | `grab`, `unblockable`; reach 500 cm; pins you to a pillar; ally hits on a palp 200 damage free you |
| Palp rake ×3 | 0.50 / 0 / 0.15 / 0.5 · 0.45 / 0.2 / 0.15 / 0.5 · 0.70 / 0.4 / 0.20 / 0.9 | 30 / 30 / 45 | Parryable; the third is a two-palp clap |
| **Ceiling drop** | 1.10 (she climbs a pillar into the dark; chalk dust sifts over one player, clicking above) / 0 / 0.30 / 1.30 | 60 | Impact 350 cm |
| **Brood shed** | 0.80 (she shudders; the young lift on her back) / 0 / — / 0.8 | — | 6 young leap off toward the player furthest from her (swarm: 40 HP, 8-damage bites, combat 2.18.3). Once per 25 s while she still carries young |

**Counter: burn the egg sacs.** Each sac hatches **2 young every 30 s**. A sac has 150 HP; fire does ×3; holding a lit torch on it for 2 s burns it out. With all 8 burned, nothing more hatches. Fire on her back kills the young riding there (10 per fire hit), so a Fire player ends Brood shed early.

Drops: shards ×4, **Mother's fang** (trophy), Cryptstalker carapace ×6, palp spines ×10.

#### 2.13.3 The Ossuary Lord (the Chandelier Hall, L2; Sedlec Ossuary)

| Part | Detail |
|---|---|
| Body | A 6 m giant built from the bones of hundreds: thigh bones bound into its limbs, ribcages stacked into a barrel chest, a crown of skulls. **Three keystone skulls** crusted in glowing soulstone hold it together: forehead, chest, base of the spine |
| Arena | **The Chandelier Hall:** a round bone chapel 40 m across, walls and vault garlanded with skulls. A **bone chandelier** 6 m across hangs on an iron chain (300 HP). Four bone pyramids at the edge (cover) |
| Voice | Thousands of bones rattling as its breath, an elephant seal roar pitched down and echoed through its ribs, dry knocking |
| Lock points | The three keystones (spine visible from behind) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Femur club ×2 | 0.70 / 0 / 0.25 / 0.6, then 0.80 / 0.4 / 0.25 / 1.0 | 45 / 55 | Parryable; the second is overhead |
| **Ribcage grab** | 0.90 (its ribs crack open like a door; crimson glint inside) / 0.2 / 0.25 catch | 12/s for 3 s | `grab`, `unblockable`; you're shut inside its chest. Allies breaking 3 ribs (60 HP each) open it; from inside you hit the chest keystone at ×2 |
| Bone spray | 1.00 (it hunches and its shoulders shiver loose) / 0 / — / 0.9 | 12 per shard | A fan of 5 shards at 1,500 cm/s, 30°, never home; min 4 m |
| **Collapse and surge** | 1.20 (every bone rattles loose at once, a dry roar) / 0 / 0.60 / 1.2 | 40 | It falls into a wave of bone rolling 10 m across the floor (`low`: jump clears it) and stands up at the end, behind you |

**Counter mechanic: it reforms.**
- At 0 HP it collapses into a bone pile and **reforms in 8 s at 50% HP**, as often as it needs.
- It dies for good if, inside those 8 s: **(a)** a Nature root or snare lands on the pile (FULL-GAME "nature roots bind the bones"); **(b)** 2 **Lime pots** (2.19) break on it (quicklime stops the grave-light knitting); or **(c)** all three keystones in the pile are smashed (120 HP each, strike ×1.5; the glowing skulls). That last one is FULL-GAME's "scatter them before it reforms".
- **The chandelier:** break its chain while the Lord stands under it: 400 damage and a full stance break (a 4 s riposte window). Once per fight.

Drops: shards ×4, **Ossuary crown** (trophy), bone ×20, soulstone ×6, silver ×4.

#### 2.13.4 The Gravedigger (roaming; folklore: the resurrection men and the Mari Lwyd)

| Part | Detail |
|---|---|
| Body | A gaunt figure 5.2 m tall in a long oilskin coat. Over his head and shoulders a **horse skull with a white sheet** hanging from it (the Mari Lwyd), green light in the eye holes. An iron spade 2.5 m long. Strapped on his back like a pack, a **coffin-shaped iron mortsafe cage**, with a lantern on a pole hooked to it |
| Route | By day he sleeps in the **Watch House** (H8), sitting against the barred door; you can find him and open with a sneak attack. At dusk he walks Watch House → Row of the Named → Dog Graves → Great Yew → the Ford → Lime Kilns → back, about 3.2 km, stopping at 6 graves to dig (3 game min each). Fresh-dug graves show where he's been. Blood Moon: two extra stops |
| On the way | He attacks creatures that block him; Gravehound packs trail him at 30 m and join his fights |
| Voice | A horse scream slowed to a groan, a camel's gargle, a spade scraping gravel; the skull's jaw clacks before each attack. You hear the spade at 40 m before you see the lantern |
| Lock points | Head (the skull), chest; the lantern (150 HP) and the cage door (300 HP) are separate targets |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Spade arc** | 1.00 (spade drawn back over the shoulder, earth falling off the blade) / 0.3 / 0.30 / 1.20 | 50 | 280° in front, 6 m; guardable with heavy chip |
| **Bury** | 1.20 (he drives the spade in and heaves up a huge clod; the landing point locks at the end of the windup) / 0 / 0.30 / 1.0 | 10/s while buried | A 3 m circle. **Buried** = rooted and unable to act for up to 5 s, stamina regen 0. Mash −0.2 s per press; an ally holding Interact digs you out in 1.0 s (FULL-GAME "allies dig you out") |
| **Mortsafe** (grab) | 1.10 (he turns his back and the cage door swings open; crimson glint on the hinges) / 0.3 / 0.25 catch | 8/s for 6 s | `grab`, `unblockable`; you're shoved into the cage on his back. Inside you can light-attack the door at 50%. Allies breaking the door (300 HP) or hitting his back for 200 free you |
| Spade slam | 1.10 / 0.5 / 0.25 / 1.30 | 70 | A line crack 8 m forward |
| Lantern swing | 0.50 / 0 / 0.20 / 0.70 | 30 + decay 20 | Close range |

**Counter: dodge the arc and dig each other out.** And **smash his lantern** (150 HP): he hunts by its light, so with it gone he swings at the last place he saw you for 10 s, then lights a spare from the cage (once per fight).

Drops: shards ×4, **Gravedigger's spade** (trophy), grave iron ×10, a random tomb's loot (silver ×2 to 6 or a gem). Morwen's quest 6 needs his lantern.

### 2.14 Major boss: Nhal, the Last Mourner (GAP-COOKED; FULL-GAME premise kept: a veiled, towering mourner in the ruined chapel who raises the dead each phase; phase 1 scythe arcs, grave hands, a wail; phase 2 every grave opens; hidden phase 3 she becomes the shade of what she mourns; weak to Nature, roots seal the graves; Mourner's lantern)

**Look (terrifying and believable).**

| Part | Detail |
|---|---|
| Anatomy | 9 m tall, a woman's shape stretched too far: a long torso, arms hanging to her knees, hands with fingers 1.2 m long and one joint too many. Long digitigrade legs under her skirts, so she moves in long, slow, planted strides (real stride clips, never gliding) |
| **The veil** | From a crown of tōrō lantern-horns, a veil falls to the floor all round her. It's **alive**: thousands of roosting Wraithmoths, wings closed, layered like black lace. When she moves, edges lift and resettle. They drink her tears (lachryphagy) |
| Face | Seen only when the veil lifts: beautiful, pale, porcelain-smooth, eyes always closed, tears of grave-light running down without end. In phase 2 the jaw unhinges and hangs open to her chest for the Keen (Mictecacihuatl's open jaw) |
| Weapon | A scythe 8 m long: a pole of black yew, the blade forged from a bell's bronze lip, its edge rimed with soulstone |
| Silhouette | A towering veiled cone of black lace with lanterns burning at the crown and a long scythe. From 100 m: a lit lantern-tree in a dark chapel |
| Voice | A solo female keen (caoineadh) pitched down an octave for her call; a whale groan under it; a barn owl shriek on her strikes; the dry roar of ten thousand moth wings when the veil lifts |

**The arena: the Chapel of Nhal** (−1,500, +3,800).
- A roofless Gothic chapel: nave 70 × 26 m, transept 46 m wide, an apse at the east end. Stone roof-ribs overhead, open to the bruise sky.
- **12 floor tombs**, six down each side of the nave (slabs 3 × 1.5 m).
- **8 pillars** (cover from the Keen) and two rows of broken pews (low; block nothing).
- The **apse** holds a rack of hundreds of lanterns and the **Necromancy altar** (BO SPEC: the subclass altar is in the arena).
- The **broken bell tower** is at the west end, where you enter. The **run-back stone** is 50 m out by the Lantern Pond (−1,555, +3,800).
- Lit at the start by all the lanterns. The phase-2 transition puts them out.

**The Nature counter (FULL-GAME "roots seal the graves and stop her raises", GAP-COOKED as a visible system):**
- **Sealing a tomb.** Any Nature-path hit landing within 2 m of an **open tomb** (Thorn shot, Root snare, Strangling vines, a Sprout turret planted on it, Briar strike's impact) seals it: roots burst from the cracks and bind the slab shut for the rest of the fight, with white blossom over it.
- **No Nature path? Bring heartwood stakes** (2.19; heartwood is the Ancient forest's resource, built in R3). Hold Interact 1.5 s within 2 m of an open tomb to drive one in. Same seal. This keeps Raven's "bring the opposite" push without making Nature the only way in. The road to the chapel shows staked graves, so the land teaches it.
- **Weakness:** Nature damage ×1.5 (SPEC). A Nature hit also cancels any Grave hands circle it lands in.

**Numbers (KEPT rules):** 12,000 solo HP, +80% per extra player or tame. **Stance 170.** 3 parries to break. Phase 2 at 50%, hidden phase 3 at 25% with artifacts (damage ×1.4). Decay buildup on her ×0.5. **Lock points:** head (veil), chest, the scythe arm.

**Phase 1: the Mourner (6 moves, with a 3-hit combo and a grab; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell (small cue, combat 2.17.1b) |
|---|---|---|---|---|
| **Reaping arc** | 1.00 / 0.3 / 0.30 / 1.20 | 55 | 300° in front, 7 m; parryable | The blade rises behind her shoulder; grave-light runs along the edge |
| **Three-cut lament** (combo) | Cut 1 0.70 / 0 / 0.25; cut 2 0.55 / 0 / 0.25; cut 3 1.10 / H 0.5 / 0.30; P 1.2 | 40 / 40 / 70 | Cuts 1 and 2 parryable (horizontal, then rising); cut 3 an overhead slam, guard-breaking | She sways left, then right, rocking like a keener, before each cut |
| **Grave hands** | 1.20 / 0 / 0.40 / 1.0 | 35 + rooted 1.5 s + decay 40 | A 2.5 m circle under every player within 20 m | She plants the scythe butt; the floor under you cracks with green light for the last 0.9 s |
| **The Keen** | 1.40 / 0 / 1.0 / 1.30 | 30 + decay 80 + stamina −40 | Cone 25 m × 70°; `unblockable`; **line of sight**: pillars and tombs block it | The veil lifts from her face, her chest heaves, the keen rises; crimson flecks in her breath |
| **Shroud** (grab) | 1.10 / 0.3 / 0.30 catch | 15/s + stamina 10/s for 3 s | `grab`, `unblockable`; dodge only; 8 m | The veil spreads wide like wings and the moths stir; crimson glints in a thousand moth eyes |
| **Lantern cast** | 1.00 / 0 / — / 0.9 | 25 + a grave-fire pool 4 m for 6 s (decay 20/s) | Projectile 1,400 cm/s, ballistic, never homes; nothing inside 4 m; aimed at players beyond 10 m | She plucks a lantern from her crown and draws her arm back |

**Tame-punishing move (KEPT rule: one per major, furthest tame first): "Lay to rest."** 1.20 s: a grave-light circle opens under the tame furthest from her, then bone hands drag it half into the floor: 40% of its max HP, rooted 6 s. A player holding Interact on it for 2 s pulls it out. Every 30 s while tames are in the fight.

**Transition at 50%: "The Opening" (an attack, KEPT rule).** 1.60 s: every lantern in the chapel gutters out, then relights pale green; she lifts the scythe over her head in both hands, then drives it into the floor. A shockwave ring 14 m: **60 damage, unblockable**, dodge through it (i-frames). All 12 tombs crack open. The moth-veil lifts off her in a roaring cloud and settles on the roof ribs. The lanterns die. **The arena is dark** except grave-light from the open tombs and your own lights.

**Phase 2: every grave opens (2 new delayed attacks, 1 new combo, 1 arena change; template KEPT)**
- **Timing:** every phase-1 windup −10%; Reaping arc and Grave hands gain delay holds of up to 0.6 s.
- **Arena change, the open graves:** each open tomb raises **a Risen every 25 s** (mostly mourners; one in four a bearer). At most 8 Risen alive. Sealing tombs is the counter: with all 12 sealed, she raises nothing.
- **Her face shows.** Hits on the open jaw count as head crits.

| New move | W / H / A / P | Damage | Flags | Tell |
|---|---|---|---|---|
| **Widow's lunge** (delayed) | 1.20 / H 0.6 / 0.30 / 1.20 | 60 | A 14 m stride-lunge, scythe thrust; parryable | She draws one long leg back and the scythe tip drops to the floor |
| **Unhinged keen** (delayed) | 1.20 / H 0.4 / 0.60 / 1.0 | 30 + decay 60 | 360°, radius 12 m, then the 25 m cone; line of sight still blocks | The jaw drops open to her chest |
| **Requiem** (new combo, 4 hits) | Sweep 0.70; sweep 0.55; spin 0.80 (360°, 8 m); slam 1.40 with H 0.5; P 1.30 | 40 / 40 / 50 / 80 | Sweeps parryable; spin `low` (jump it); slam guard-breaking | A rising hum under the keen on each hit; the hum stops dead before the slam |

**Hidden phase 3 at 25% (artifacts only; damage ×1.4): "The Procession", the shade of what she mourns.**
- She falls to her knees. Her body empties into shade: **a procession of 40 shades** of the Lantern Mourners pours out of her and walks a ring round the nave at 3 m/s, each carrying a lantern on a pole.
- **The ring** starts at 20 m radius, closes to 8 m over 30 s, then opens again. There's a **3 m gap after every 6th shade**: step through the gaps. A shade's swinging pole (visible contact) deals 25 + decay 30.
- **The bell:** the broken tower tolls every 10 s, and each toll reverses the procession (the toll is the tell).
- **Nhal's shade** rises 10 m tall, translucent, jaw open, and fights inside the ring with Requiem, Unhinged keen and Lantern cast. She takes damage normally. Open graves still raise; sealed ones stay sealed.

**Drops (BO SPEC: core, path materials, a chance at the part):** **Nhal's core** (Hearth core), soulstone ×12, Wraithmoth scales ×10, silver ×6, **Mourner's lantern** (boss part: 20% per kill; on the first kill it's always dropped, named, and flies to the killer, KEPT). **Trophy head** "Nhal's veiled head" and **2 blueprints**, the **Mourner's Scythe** (boss weapon, scythe class) and the **Mourning chapel** decor pack (KEPT survival rule and set name).
- **Mourner's lantern (SPEC text "each kill gives a short stacking damage buff"; numbers GAP-COOKED):** each kill gives +4% damage for 12 s, stacking to 5 (+20%); each kill refreshes the timer. While stacked, a small grave-light lantern floats at your shoulder with one flame per stack.

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT from fire-frost 3.5)

Four layers per attack (tell, strike, hit, pain) plus idle, aggro and death; each built from at least 3 real animal calls and one material layer. Tells mixed 3 dB over the bed; every tell over 0.5 s rises in pitch; calls duck music by at most 4 dB (KEPT).

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Gravehound | Grey wolf howl (mournful, slowed 10%), Xolo/basenji yodel, spotted hyena low groan | Dry soil sifting; bone knock in the chest on each breath | The Grim howl from the mist: several overlapping howls tell you the pack's size before you see one |
| Cryptstalker | Scorpion stridulation, hissing cockroach, the dry click of a stag beetle's mandibles | Chalk grit under its feet; the whips' thin whistle | The click-lock when its whip touches you |
| Wraithmoth | Death's-head hawkmoth squeak (real, amplified), barn owl wing hush, fruit-bat chitter | Dusty wing flutter, a paper rustle | The rising squeak as the skull turns to face you |
| Palestrider | Horse scream pitched down, donkey bray reversed, camel groan | Hollow bone resonance in the face | The death scream that turns the herd |
| Tombback | Rhinoceros beetle hiss, tortoise grunt, wild boar snort | Stone grinding on stone (the slab) | The slab's scrape as it rears |
| Gloamcrow | Hooded crow rattle, jackdaw "tchak", raven knock call | Wing clap | The sudden silence of a crow funeral |
| Mournhare | Hare scream (real and chilling), rabbit thump, a vole squeak | Grass rustle | The scream when caught |
| Wickbeetle | Click beetle snap, cricket, a soft candle-flame flutter | Wax crackle | A soft gutter as it flees |
| Marrowgoat | Goat bleat (deep), sheep cough, the crunch of bone chewing | Bone crunch | Chewing, heard across the fields at dawn |
| Risen (all) | No animal voice: dry bone rattle, a breath of wind through ribs, grave-light hum | Iron bands clinking | The rattle of a procession on the road |
| Bell-Ringer | Bull walrus groan, gorilla hoot pitched down, breath ringing inside a bronze bell | Bell bronze hum | The great bell's pre-ring hum before each Toll |
| Crypt Mother | Scorpion stridulation ×3 layered, emperor scorpion hiss, wet palp clicks | The young skittering on her back | The whistle of the whips in the dark |
| Ossuary Lord | Elephant seal roar through a long pipe, ten thousand bones rattling | Bone knock and dry roar | The collapse rattle, then silence for 8 s, then the knitting crackle |
| Gravedigger | Horse scream slowed to a groan, camel gargle, a heavy man's breathing | Spade in gravel, cage chain, the skull's jaw clack | The spade at 40 m in the dark |
| Nhal | A solo female keen (caoineadh) pitched down an octave, humpback whale groan under it, barn owl shriek | Ten thousand moth wings | The Keen, and the silence before the Opening |

### 2.16 Music: the Graveyard (tempo, key, instruments SPEC 3.3: 54 BPM, F minor, organ, slow choir, tolling bell; cues GAP-COOKED; rules KEPT)

Real composed pieces, always audible, ducking ≤ 4 dB, planned silences only, −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP (KEPT). Library first (credited), commissioned where nothing fits. The **bell** is tuned to F and lands on beat 1 of every 4th bar in every Graveyard cue, so the biome has one heartbeat.

| Cue | Length | Feel and scoring | When |
|---|---|---|---|
| **Explore, day** | 4 to 5 min, loops on a phrase end | Soft pipe organ (8' flutes only), a slow wordless choir in thirds, a solo cello line; the bell every 4th bar | Day on the surface |
| **Explore, night** | 4 to 5 min | Same theme on low organ reeds and a solo female keening voice (caoineadh style) over a held choir drone; the bell louder and closer | Night on the surface |
| **The catacombs** | 5 min | Organ pedal notes only (16' and 32'), bowed metal, distant water drips in tempo, the bell heard far above as if through stone | G5, L1 to L3 |
| **Combat layer** | Stems over any explore cue | Low strings ostinato, timpani on beats 1 and 3, full choir on "ah" swells; the organ goes to full principal | Any fight (stems enter in 2 bars) |
| **Mini-boss** | 3 min | Bell-led: the Bell-Ringer's cue starts with his great bell and builds on it; the others share the cue with their own lead (Crypt Mother: col legno strings; Ossuary Lord: bone-like marimba and log drum; Gravedigger: a slow work-song rhythm on low brass) | Minis |
| **Nhal, phase 1** | 3 min loop | Full organ and choir in F minor, the solo keen as the melody; a funeral march at 54 BPM | P1 |
| **Nhal, phase 2** | 3 min loop | The organ drops out at the Opening (the planned silence: 2 bars of only her keen), then returns with brass and a choir in clusters; tempo stays 54 but in 12/8, so it feels faster | P2 |
| **Nhal, phase 3** | 2 min loop | The Procession: a hundred-voice choir singing one held chord as a round, the bell on every 10 s toll (synced to the reversal), the keen soaring over it | P3 |
| **Blood Moon layer** | Stems | The night cue down a semitone (E minor), a low drone, the bell's toll every 2 bars, a muted heartbeat drum | The event |
| **Camp** | 3 min | A single acoustic guitar and a soft hum of the night theme, warm; the only major-key passage in the biome | Inside spawn camps and the Lychgate Chapel |
| **Rootgrave duet** (pass, world-map KEPT rule) | 3 min | Graveyard organ and bell against the Forest's woodwinds and harp, one key (F minor / Ab major, its relative), 63 BPM (between 54 and 72) | In the pass |

### 2.17 The Necromancy path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Souls (DD SPEC: "collected from kills; spent to raise and empower the dead"; a spender, combat KEPT).**
- **Max** 20 (Soul capacity +5 per rank, so 35).
- **Gain:** when anything dies within 30 m of you that you or your minions hit in its last 10 s: lowland 1, midland 2, highland 3, Risen 1, mini-boss 10 (at its death), major boss 25 (at its death), a player 3 (PvP). Each Soul is a pale-green wisp that flies to you from up to 15 m over 0.6 s (Wind + Necromancy's Soul wind widens this, CB).
- **Keeping them:** Souls never fade out of combat. You lose them all when you die.
- **HUD:** the path bar is a row of small bone-green flames, one per Soul, under the stamina bar (path colour #A8B89A).
- **Corruption buff (KEPT):** a Necromancer in the Graveyard gets +3 / +6 / +10 / +15% path damage at corruption 25 / 50 / 75 / 100 and none of the penalties.

**Root passive: Deathless (SPEC text: once per rest, survive a killing blow at 1 HP; behaviour GAP-COOKED).** Main path only (ST rule).
- A hit that would kill you leaves you at exactly 1 HP instead. Nothing else: no i-frames, no heal. The next hit kills you.
- It shows: your skin flashes ash-white for 0.4 s, a single low bell toll plays for you and players within 15 m, and a small grey bell icon shows "spent" under your vitals.
- It re-arms on **Rest** (survival-crafting KEPT: finishing a sleep in your bed or bedroll).
- Works against everything that kills, PvP included.

**Raising (DD SPEC; numbers KEPT from combat 2.13 where set).**
- **What can be raised:** the body of any creature that died in the last **3 minutes** and hasn't been butchered (the built carcass state; KEPT 3 min). Never a boss, a mini, a Risen or a player.
- **What you get:** the same creature, at **60% stats for 30 s** (KEPT), with its own attack rows. It looks like itself with the hide gone grey, bone showing at the joints, grave-light threads in the wounds and pale-green eyes (never rotting flesh: DD no-zombies rule). It fights your lock target, else the nearest hostile, and takes orders from the order wheel's attack and follow.
- **Combat raises don't count against your tame limit** (DD SPEC). **At most 3 alive** at once (5 with Second wave).
- When it expires or dies, it crumbles to bones: the body is used up (no butchering after). So you choose: raise it or butcher it.
- **PvP:** raised minions fight players hostile to you and take the ×0.6 PvP damage rule (combat KEPT).
- **Permanent undead tames by ritual** are KEPT exactly from survival-crafting 2.22 (bone altar, Souls 10 / 25 / 60 by difficulty, soulstone 3, Asphodel 2, 60 s channel; counts against the tame limit; 1 soulstone dust a day).

**The tree.** Gates are points spent in the branch before the node (the `fire.json` pattern; branches sum to 20 before the 10-point capstone).

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Raise dead | 3 pts | 0 | Basic spell: briefly raise a fallen enemy | Summon shape: a corpse within 1,000 cm of your aim; raised at 60% stats for 30 s; 2 Souls; cd 6 | KEPT combat |
| Trunk | Soul capacity | 3 ranks | 3 | Hold more Souls | +5 max Souls per rank | GAP-COOKED |
| Trunk | Wither chance | 2 ranks | 6 | Higher chance to inflict decay | +10% decay buildup on all your hits and +6 decay buildup on every weapon hit, per rank (so any weapon can decay) | GAP-COOKED |
| Trunk | Grave strike | 2 pts | 8 | Basic necrotic weapon art | Wildbrand: all melee, 1 Soul, cd 2.5, W/A/R 0.30/0.20/0.44, ×1.3, decay 40, stance ×1.5 | KEPT combat |
| Horde | Second wave | 4 pts | 0 | Raise two dead at once | Raise dead raises the 2 nearest corpses to your aim for the same 2 Souls; max alive 3 → 5 | GAP-COOKED |
| Horde | Restless dead | 5 ranks | 4 | More minion health | +15% minion HP per rank | GAP-COOKED |
| Horde | Dead march | 3 ranks | 9 | Faster, harder-hitting minions | +10% minion move speed and +8% minion damage per rank | GAP-COOKED |
| Horde | Corpse explosion | 5 pts | 12 | Spell: detonate one of your minions | Nova on your minion nearest your aim: 40 dmg, radius 400 (KEPT), decay 50; 1 Soul; cd 4; the minion is used up | KEPT combat (cost, cd, decay GAP-COOKED) |
| Horde | Shared pain | 3 pts | 17 | Minions take some of your damage | 20% of the damage you take is split across your minions within 15 m (none in range: you take it all) | GAP-COOKED |
| Horde | Army of the dead | 10 pts | 20 | Capstone: raise every corpse nearby at once | Every corpse within 1,200 cm rises for 20 s, ignoring the alive cap; cd 120 (KEPT); 6 Souls | KEPT combat (cost GAP-COOKED) |
| Reaper | Scythe sweep | 5 pts | 0 | Wide sweeping weapon art | Wildbrand: scythe, greatsword, spear; 2 Souls; cd 6; W/A/R 0.40/0.30/0.55; ×1.5; decay 50; stance ×2; hyperarmor; 300° sweep, 320 cm | KEPT combat |
| Reaper | Harvest | 5 ranks | 5 | More Souls per kill | +20% Souls per kill per rank (fractions carry over), so ×2 at 5 | GAP-COOKED |
| Reaper | Soul siphon | 4 pts | 10 | Kills heal you | Each kill heals 5% max HP + 4 (at most once a second) | GAP-COOKED |
| Reaper | Death mark | 3 ranks | 14 | Marked enemies drop extra Souls | Grave strike, Scythe sweep and Shadow bolt mark the target for 10 s (a grey sigil over it; no status meter, not Arcane's Mark); a marked kill gives +1 Soul per rank | GAP-COOKED |
| Reaper | Reaping stride | 3 pts | 17 | Kills restore stamina | Each kill restores 25 stamina | GAP-COOKED |
| Reaper | Final harvest | 10 pts | 20 | Capstone: execute enemies below 20% health | Your weapon and Wildbrand hits on a non-boss creature under 20% HP kill it on contact (a reaping finish: the body folds and a Soul tears free). On bosses and players: +30% damage under 20% instead (no instant kills on them, CAL) | SPEC text; numbers GAP-COOKED |
| Lich | Shadow bolt | 5 pts | 0 | Spell: heavy necrotic bolt | Bolt 30, decay 40, 1,800 cm/s, cd 3, 2 Souls | KEPT combat |
| Lich | Necrotic power | 5 ranks | 5 | More necrotic spell damage | +6% necrotic spell damage per rank | GAP-COOKED |
| Lich | Curse of weakness | 4 pts | 10 | Cursed enemies deal less damage | Bolt (no damage), curse buildup 80, 2 Souls, cd 10. Uses the shared curse status (combat KEPT: creatures −15% damage dealt for 15 s). The name overlap with Blood's Hex of weakness stays (DR #25 is Raven's) | KEPT combat status; numbers GAP-COOKED |
| Lich | Soul shield | 3 ranks | 14 | Spend Souls to absorb damage | When hit with more than 5 Souls, each Soul spent absorbs 3 damage per rank, up to 3 Souls per hit (so 27 absorbed at rank 3). A bone-grey ward flickers where the hit lands | GAP-COOKED |
| Lich | Life drain | 3 pts | 17 | Spell: drain HP from one enemy | A single beam to a target within 1,200 cm, 2.0 s channel, 12 dmg/s, heals 50% of it; cd 12; 2 Souls | KEPT combat (cost GAP-COOKED) |
| Lich | Lich form | 10 pts | 20 | Capstone: transform; spells cost no Souls for a short time | Transform 15 s, cd 120 (KEPT). While it lasts, every Necromancy spell costs 0 Souls. Look: skin goes full ash-white, the eyes burn pale green, a crown of grave-light flickers above the head, breath mists green | KEPT combat |

**Combos with Necromancy built in R4 (CB SPEC names and effects as written; numbers GAP-COOKED):**

| Pair | 25/25 | 50/50 |
|---|---|---|
| Fire + Necromancy | **Ash wraith:** a body that died burning rises as an ash wraith; its hits add burn 15 | **Funeral pyre:** Corpse explosion becomes a fire nova: +burn 60 and a 4 m fire pool for 4 s |
| Frost + Necromancy | **Preserved:** bodies that died Frostbitten or Frozen stay raisable for 10 min, not 3 | **Frozen legion:** an enemy killed while Frozen rises as an ice minion for free (20 s; hits add frost 15; up to 2 beyond the cap) |
| Lightning + Necromancy | **Jolt of life:** a lightning hit on a corpse raises it free at 80% stats (once per 10 s) | **Storm lich:** in Lich form, Necromancy spells chain to 2 more targets at 50% |
| Blood + Necromancy | **Blood bond:** minions get your lifesteal share on the damage they deal | **Death curse:** an enemy killed by Doom rises as your minion for free (30 s) |
| Nature + Necromancy (opposites) | **Cycle of life:** when a minion dies, allies within 8 m heal 20 HP | **Rot and bloom:** a corpse you don't raise within 10 s grows into a Sprout turret under your control (Nature numbers KEPT), at most 2 at once |

The other six Necromancy pairs (Sun, Wind, Arcane, Tide, Spore, Void) keep their CB names and get numbers in their partner's round.

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Soul gain | A pale-green wisp arcs to your chest; one more flame in the bar | A soft breath in |
| Raise dead | Grave-light threads pull the body upright; the eyes light | A dry crack of joints, then the creature's own call, hollowed |
| Corpse explosion | The minion's bones blow outward in a ring of green fire | A deep bone crack and a whoosh |
| Death mark | A turning grey sigil over the target | A low hum near it |
| Final harvest | The body folds; a Soul tears free brighter than normal | A scythe hiss and a toll |
| Soul shield | A bone-grey ward ripples at the hit | A muffled knock |
| Deathless | Your skin flashes ash-white | One bell toll |
| Lich form | Full ash skin, green eyes, a crown of grave-light | The choir's held chord rises under your mix for 15 s |

### 2.18 Body change: Necromancy (SPEC "pale skin", DD; frame KEPT from hero.md 2.6; stages GAP-COOKED)

**Frame (KEPT):** strength = smoothstep of points ÷ 100; subclass ×0.6; one skin layer; hero.md's Necromancy row (albedo desaturated 40% and lightened toward ash, grey lips and nails, pale grey iris with a pale-green pupil ring at 0.5 nits, hair desaturated 30%; pale hands / pale arms and face / full ash skin at 0.3 / 0.6 / 1.0).

**The look:** striking and moonlit, never corpse-like. Alabaster skin with a cool, smooth finish (roughness −0.05 so it catches light), no blotching, no bruising, no visible rot. Colour stays bone-green #A8B89A for any glow.

| At | What shows |
|---|---|
| 10 | Nails take a soft grey; the skin of the hands cools by a shade |
| 30 | Hands and forearms alabaster-pale; lips a cool rose-grey |
| 50 | Arms, neck and face pale; a pale-green ring around the pupil that brightens in the dark; hair loses 15% of its colour |
| 75 | Full alabaster to the collarbones and down the back; fine silver-grey veins show faintly at the temples and inner wrists (no glow); hair loses 30% (dark hair goes ash-dark, light hair goes platinum) |
| 100 | Full ash-white skin, grey lips and nails, pale grey irises with the green ring; in the dark the ring glows 0.5 nits so your eyes catch lantern light like an animal's |

- **Raise flare:** each Raise dead lights the pupil ring ×1.6 for 1.5 s (KEPT combat pulse rule).
- **Lich form:** the 100 look for 15 s at any point count, plus the grave-light crown and green breath (2.17).
- **With other paths:** Fire's ember veins glow through the alabaster (a striking contrast). Frost's rime sits on pale lips. Blood's garnet veins show darker on pale skin.

### 2.19 Gear and items from the Graveyard

**Gravecloth set (KEPT, survival-crafting; restated):** light, poise 30. Inputs (full set): steel plate 6, soulstone 8, Mournhare fur 8, linen 8. 2 pieces: decay buildup ×0.5. 4 pieces: corruption builds at half rate; dark-hunters never get double aggro on you (the light check, passed).

**Look, merged (survival-crafting and hero.md describe it differently; this lane proposes one look and logs it to LATER-COOKS):** a high-collared, fitted black long coat (survival) closed with **soulstone clasps** that glow faintly bone-green (hero); under it a **bone-ribbed fitted bodice or vest** cut close to the body (hero), silver thread, bone buttons; a deep hood or tall cowl that shadows the eyes; a slit coat skirt over fitted breeches and high boots. Black, ash grey and silver, with the soul-green glow. Cut on the MetaHuman male and female to show a strong, attractive shape: open at the throat, fitted at the waist.

**Graveyard items (GAP-COOKED):**

| Item | Station, level | Inputs | What it does |
|---|---|---|---|
| **Heartwood stake** ×2 | Workbench 3 (recipe from Morwen's quest 6, or found on the chapel road) | Heartwood 1, iron 1 | Seals an open tomb in Nhal's fight (2.14). Also pins a Risen to the ground for 6 s (Interact on a downed one) |
| **Lime pot** | Workbench 2 | Quicklime 2, clay 1 | Thrown (ballistic, never homes): 4 m splash, 20 damage and 8/s for 3 s; 2 on the Ossuary Lord's pile end it (2.13.3) |
| **Grave lantern** | Workbench 3 | Silver 2, glass 1, soulstone 1 | A lantern (KEPT rules) with a 10 m radius that burns soulstone (1 soulstone = 4 h) and never goes out in draughts or rain; corruption build halved (KEPT lantern rule) |
| **Mourning ring** | Found only (S2) | — | Corruption builds 15% slower; +5% necrotic damage |
| **Yew longbow** | Workbench 3 (blueprint from S1) | Yew 4, horsehair 2, linen 1 | A longbow: ×1.15 arrow damage, draw +0.1 s; the Graveyard's ranged weapon |
| **Palp-spine arrows** ×10 | Workbench 2 | Palp spines 2, wood 2, feathers 2 | Pierce through the first target; +15 bleed buildup |
| **Silvered** (weapon extra) | Forge 3 | Silver 2 | One per weapon (CF extras): +15% damage to Risen and shades |
| **Sexton's pick** | Found only (S4) | — | A pick-hammer (combat class) with silver-tier stats; ×1.3 vs bone (Risen, Ossuary keystones) |
| **Moth-dust tonic** | Alchemy 2 | Wraithmoth scales 2, water | A brew (buffs come from brews, DD): stamina regen +15% for 5 min |
| Decay-resist potion, Cleansing draught, Mourner's bread, bone altar, soul engine | KEPT (survival-crafting) | | |
| **Mourner's Scythe** (boss weapon blueprint) | Forge 4 | Steel 6, soulstone 6, yew 4, Nhal's core shard 1 | Scythe class (combat KEPT), element locked to Necromancy. Its moveset and Wildbrand belong to the boss-weapons cook (logged already by survival-crafting) |
| **Mourning chapel** decor (blueprint) | KEPT (survival-crafting set) | | Candle rack, carved pew, lectern, ossuary shelf, bell, velvet chaise; comfort as the set rules |

**Necro-element weapons:** soulstone is the Graveyard's path material (CF rule: path material sets the element forever).

### 2.20 The story the Graveyard tells, with no words (GAP-COOKED; fits world-map's Twelve Peoples and pass wars)

**The spine of it (never said, only shown):**
1. **The Lantern Mourners buried the world's dead.** Smiths from the Volcanic lie on the north edge under slag stones (H5). Dogs lie beside their people (H3). Even the creatures' bones are sorted with care in the Sorting Room (C7). They kept a lantern burning for every grave.
2. **They quarried the hill hollow to do it.** The catacombs were a limestone quarry first; the Quarry Face (C11) shows work stopped mid-swing.
3. **Someone robbed the graves.** Mortsafes over the fields, the Watch House with its watchers dead at the window and the tally that stops (H8). The Gravedigger still digs every night.
4. **Then the Mourners climbed the Crown, and never came back.** The Broken Procession's dropped poles point at the Stair (H7). Sandals are piled at its foot (H13). The Stair breaks at 700 m with one lantern left facing up (L5). The surface graves are **cenotaphs**: dig one and there's no body, only a lantern with a name (H2). The rows in the south were dug and never filled (H10). The seventh name on the mausoleum is blank (H6).
5. **Nhal stayed.** She weeps in the chapel, lights the lanterns, and raises the old dead because she has no new ones. Her hidden phase is her people walking round her, the ones she's mourning.
6. **The pass war.** The Grovekin's champion, a bone giant, lies on this side of Rootgrave with Mourner lantern-poles through its bones; the forest's roots have grown through it and are breaking the tombs open (L4, H12). The Mourners' own champion, a treant, lies rotted black on the forest side (world-map KEPT).
7. **Someone keeps it now.** Morwen keeps the lights. She's a newcomer; the chapel's candles were burning when she arrived.

**Lore items (Souls-style item text; short):**

| Item | Where | Text |
|---|---|---|
| Lantern tag | H2, any cenotaph | "A paper tag, a name in an old hand. Folded inside: a second tag, blank." |
| Washerwoman's bat | H4 | "A washing bat, worn smooth. The grave-clothes beside it were never finished." |
| Watcher's tally | H8 | "Four hundred and twelve nights scratched in the stone. The last mark is only half a line." |
| Ossuary ledger | C7 | "Every bone counted and shelved. Hounds, hares, horses, smiths, strangers. One shelf is labelled for the Mourners themselves. It is empty." |
| First Lantern | S1 | "The oldest lantern in the Graveyard, found in a dog's grave under the yew." |
| Bell-founder's tally | Bell-Ringer | "A list of bells cast for the chapel. Beside the last: 'for the day they come down.'" |
| Bell-founder's mark | S5 | "The founder's stamp from inside the drowned bell. It was rung so long it cracked." |

### 2.21 Invasions: Nature ↔ Necromancy through Rootgrave (3.3 rule KEPT; contents GAP-COOKED for the Graveyard's side)

**Graveyard → Forest.** 8 to 14 Graveyard creatures: 2 Gravehound packs, 2 to 3 Wraithmoths, 1 Cryptstalker, and a procession of 4 Risen mourners, led by **the Gravedigger** (the roaming mini comes along, as 3.3 asks). Graveyard invasions always **start at dusk**: when one rolls by day, the 5-minute warning still goes out, and they wait in the Root-Split Tombs (H12) until 20:00, the Gravedigger's spade audible across the pass. They march on the nearest claim or shrine within 1.5 km on the forest side (KEPT rule). Where they walk, the forest ground greys in a 3 m trail for one game day (decor only).

**Forest → Graveyard.** The Forest cook sets its roster. On this side they come out through the Root-Split Tombs and target the nearest claim or shrine within 1.5 km (Foot of the Stair shrine is the nearest, ~770 m; then the Seven Names and claims in G4 and G2). Wherever they walk, roots seal the graves they pass: **Gravehounds can't rise from sealed ground in a 20 m band along their path for one game day** (the ecology shows the war).

**Pays (KEPT):** both biomes' path materials (soulstone and heartwood) and 2,000 path XP to everyone who dealt 10%.

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing from data, so `L_Island` is touched as little as possible.

**R4 also delivers these shared pieces, which this lane uses (other lanes design them):**
- the **decay** status (combat 2.11) and the shared **curse** status (built in R2);
- **darkness and lights, corruption, Rest and Rested** (survival-crafting 2.2, 2.3, 2.5);
- **Hearth 5 and starsteel, the soul engine, the bone altar ritual** (survival-crafting);
- the attack-row runner and grab framework (combat; LATER-COOKS);
- the world-map terrain for B5 (the Graveyard half) and the underground fix for `undergroundMargin` (world-map W3);
- shrines, path masters, quests and the biome-event subsystem (fire-frost frame).

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/necromancy.json`** | Same schema as `fire.json`: `id: "necromancy"`, `name: "Necromancy"`, `biome: "graveyard"`, `starter: "raise_dead"`, `resource {id: "souls", name: "Souls"}`, `rootPassive {id: "deathless", name: "Deathless", text: SPEC}`, trunk and three branches with node ids `raise_dead, soul_capacity, wither_chance, grave_strike; second_wave, restless_dead, dead_march, corpse_explosion, shared_pain, army_of_the_dead; scythe_sweep, harvest, soul_siphon, death_mark, reaping_stride, final_harvest; shadow_bolt, necrotic_power, curse_of_weakness, soul_shield, life_drain, lich_form`, `kind` (ability / stat / capstone), costs, ranks and gates as in 2.17. Node text copied from ST word for word |
| `Data/tuning.json` `resources` | `soulsMax: 20, soulsPerCapacityRank: 5, soulGain {lowland: 1, midland: 2, highland: 3, risen: 1, mini: 10, major: 25, player: 3}, soulCreditWindow: 10, soulRadius: 3000, soulPickup: 1500, soulFlySeconds: 0.6, soulsLostOnDeath: true` |
| `Data/tuning.json` `nodeEffects` | `necromancy.soul_capacity {soulsMax: 5}`, `necromancy.wither_chance {decayBuildup: 0.10, weaponDecay: 6}`, `necromancy.restless_dead {minionHealth: 0.15}`, `necromancy.dead_march {minionSpeed: 0.10, minionDamage: 0.08}`, `necromancy.harvest {soulGain: 0.20}`, `necromancy.death_mark {markSouls: 1}`, `necromancy.necrotic_power {necroSpellDamage: 0.06}`, `necromancy.soul_shield {absorbPerSoul: 3}` |
| `Data/tuning.json` `passives` | `deathlessHp: 1, raiseCorpseSeconds: 180, raiseStatScale: 0.6, raiseSeconds: 30, raiseMaxAlive: 3, secondWaveMaxAlive: 5, sharedPainShare: 0.2, sharedPainRadius: 1500, soulSiphonPct: 0.05, soulSiphonFlat: 4, soulSiphonCooldown: 1, deathMarkSeconds: 10, reapingStamina: 25, finalHarvestAt: 0.2, finalHarvestBossBonus: 0.3, soulShieldMinSouls: 5, soulShieldMaxPerHit: 3, gravehoundPathMagicSouls: 10` |
| `Data/tuning.json` `spells` | Necromancy entries as combat 2.13 (KEPT), plus `raiseDead.souls: 2`, `corpseExplosion {souls: 1, cd: 4, decay: 50}`, `armyOfTheDead.souls: 6`, `curseOfWeakness {souls: 2, cd: 10, curse: 80}`, `lifeDrain.souls: 2` |
| `Data/tuning.json` `status` | Decay row from combat 2.11 (KEPT); colour decay grey-green |
| `Data/tuning.json` `hazards` | `graveSubsidence {tell: 1.0, drop: 300, denChance: 0.5, cellShare: 0.12}`, `limePit {damagePerSecond: 8, durabilityPerSecond: 0.01, rainMult: 2, tell: 0.9}`, `cryptDraught {tell: 1.2, everyMin: 8, everyMax: 15}` |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 and 2.12.4 in the combat 2.18.1 schema. New kinds: `rising, hamstring, grim_howl, whip_sweep, palp_snatch, ceiling_drop, lateral_rush, light_dive, scale_dust, tear_drinker, rear_strike, slab_slam, carrion_spray, mob_dive, wax_squirt, grave_grasp, coffin_ram` |
| `Data/tuning.json` `bosses` | `nhal {stance: 170, parries: 3, tombs: 12, raiseEvery: 25, maxRisen: 8, bearerShare: 0.25, sealRadius: 200, stakeSeconds: 1.5, layToRestEvery: 30, layToRestPct: 0.4, layToRestRoot: 6, procession {count: 40, speed: 300, radiusStart: 2000, radiusEnd: 800, closeSeconds: 30, gapEvery: 6, gapWidth: 300, tollEvery: 10}, decayResist: 0.5}`, `bell_ringer {tollEvery: 20, tollEveryBloodMoon: 15, risenPerToll: 3, maxRisen: 9, ropeHealth: 400, ropeSlash: 2.0, ropeFire: 2.0, stanceAfterRope: 60}`, `crypt_mother {sacs: 8, sacHealth: 150, sacFire: 3.0, torchBurnSeconds: 2, hatchEvery: 30, hatchCount: 2, shedCount: 6, shedEvery: 25, backFireKills: 10}`, `ossuary_lord {reformSeconds: 8, reformHealth: 0.5, keystoneHealth: 120, keystoneStrike: 1.5, limePotsToBind: 2, chandelierChain: 300, chandelierDamage: 400, ribHealth: 60}`, `gravedigger {buryMax: 5, buryDps: 10, digOutSeconds: 1.0, cageSeconds: 6, cageDoor: 300, lanternHealth: 150, blindSeconds: 10, route: "gravedigger_night"}`; every move row from 2.13 and 2.14; run-back stone positions |
| `Data/tuning.json` `events` | `bloodMoon {warnSeconds: 300, risingFrom: 20, risingTo: 5, procession {min: 4, max: 8, speed: 200, per: 15000, reform: 360, lightRadius: 6000, targetRadius: 60000}, corruptionMult: 2, soulMult: 2, raiseMult: 2, bonePile {bone: 4, soulstoneMin: 1, soulstoneMax: 3, minutes: 30}, payout: 3.0}` |
| `Data/creatures.json` | 9 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `necromancy`, `resists ["corruption", "lime", "subsidence"]`, taming, difficulty, food (Tombback `soulstone_dust`, Marrowgoat `bone`), attacks, activity (Gravehound and Wraithmoth nocturnal; Gravehound `buriedByDay`), loot, mount and rideSpeed and `noSprintStamina` for Palestrider, hauler and `slabCover` for Tombback, utility `light` (Wickbeetle), `senseSecrets` (Gloamcrow)); 3 Risen rows (`tameable: false`, `leavesCorpse: false`); 5 boss rows (`nhal` major, `bell_ringer`, `crypt_mother`, `ossuary_lord`, `gravedigger` with `roaming` and `route`) |
| `Data/spawn_regions.json` | Graveyard regions G1 to G5, L2, L3 with `area` boxes from 2.2 and 2.5 and `night` tables from 2.12.3; `mistRise` on G1 and G3 Gravehound entries |
| `Data/items.json` | `soulstone, soulstone_dust, bone, yew_log, limestone, quicklime, grave_iron, silver_ore, silver_ingot, wax, gravehound_hide, cryptstalker_carapace, palp_spine, wraithmoth_scale, pale_hide, horsehair, beetle_shell, goat_hair, core_nhal, mourners_lantern, trophy_bell_clapper, trophy_mothers_fang, trophy_ossuary_crown, trophy_gravediggers_spade, trophy_nhal_head, mourners_scythe_bp, mourning_chapel_bp, yew_longbow_bp`; tools and consumables `heartwood_stake, lime_pot, grave_lantern, mourning_ring, yew_longbow, palp_spine_arrow, silvered_extra, sextons_pick, moth_dust_tonic`; lore items (7) |
| `Data/recipes.json` | The 2.19 recipes; lime kiln (workbench L2); quicklime; mortsafe → grave iron; silver ore → silver ingot (forge 3); Gravecloth (KEPT inputs) |
| `Data/pieces.json` | `lime_kiln`, `tombbacks_stone` (decor), `toro_lantern` (light piece: grave-light 8 m, no fuel; comfort Light 1), `plank_bridge` reuse over lime pits |
| `Data/world_pois.json` (fire-frost's file) | Graveyard sub-zone polygons, 5 landmarks, 13 hand sites, 5 shrines, 6 crypt mouths, 2 subsidence pits, the catacomb level graph (L1 to L3 rooms and links, 60 POIs), kit rules (2.4), the Gravedigger's night route spline, Blood Moon procession road splines, Nhal's arena layout (tombs, pillars, apse altar) |
| `Data/quests.json` | Morwen's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 9, the Risen and 5 bosses; pools `explore.graveyard.day/night`, `sub.catacombs`, `combat.graveyard`, `mini.graveyard` (4 leads), `boss.nhal` (3 sections), `event.blood_moon`, `camp.graveyard`, `pass.rootgrave`; sub-zone beds G1 to G5; `deathless_toll`, `corruption_whispers` |
| `Data/fx.json` | Soul wisp, raise threads, corpse explosion ring, death mark sigil, soul shield ripple, Lich crown and breath, Deathless flash, grave-light tōrō glow, subsidence ring, lime steam, draught flame-lean, Gravehound soil heave, Wraithmoth dust cone, tomb seal roots and blossom, Opening shockwave, Procession shades, Blood Moon sky and red moon |
| `Data/combos.json` (R1 creates it) | Fire+Necro, Frost+Necro, Lightning+Necro, Blood+Necro, Nature+Necro rows from CB with 2.17 numbers |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `DecayBuildup` (curse comes in R2); a `bFromMinionOf` owner pointer so Soul credit and Dead march scale find the necromancer |
| `WRStatusComponent.cpp/.h` | **Decay** (combat KEPT): buildup meter, proc = max HP −10% until fight end (creatures) or until Rest (players), healing ×0.5 for 20 s; threshold growth ×1.3. Necrotic-resist hooks for Nhal (×0.5) and the Risen (×0.7 damage) |
| `WRPathComponent.cpp/.h` | **Souls resource:** replicated `Souls` and `SoulsMax` (from `soulsMax` + capacity ranks); `AddSouls(n)`, `SpendSouls(n)` (fails and shakes the slot if short; Lich form makes cost 0); lost on death. **Deathless** as the Necromancy root passive (main only): an `bDeathlessArmed` flag re-armed by the survival lane's `OnRest`. The corruption-match buff reads the corruption meter (survival lane) |
| `WRCombatLibrary.cpp` `ApplyHit` | **Killing-blow check** before death: if the victim is a main-path Necromancer with Deathless armed, clamp HP to 1, disarm, fire the toll and flash. **Shared pain:** before applying damage to a Necromancer, move 20% to minions within 15 m. **Soul shield:** after Shared pain, absorb with Souls. **Final harvest:** on a weapon or Wildbrand hit from a capstone owner, a non-boss creature target under 20% dies; bosses and players +30%. **On kill:** credit Souls (tier table) to Necromancers within 30 m who hit it in the last 10 s; Soul siphon, Reaping stride, Death mark bonus, Mourner's lantern stack |
| New `AWRSoulWisp` (WRFx actor) | A pooled visual that flies from a kill to the credited player over 0.6 s; the server credits Souls at once, the wisp is cosmetic |
| `WRCreature.cpp/.h` | **Raised minions:** `RaiseFromCarcass(Owner, StatScale, Seconds)` turns a carcass-state `AWRCreature` back into a living one on the owner's team (tame-like follow and attack orders, no tame-limit count, `bRaised`), swaps to `MI_<species>_Raised` (grave-light threads and bone exposure material layer), applies 0.6 stats and node bonuses, and on expiry or death plays a crumble and destroys the body. Corpse eligibility: `CarcassAge < 180 s` (600 with Preserved), not butchered, not a boss. **Gravehound:** `buriedByDay` state (hidden under a soil decal, wakes on the Rising row or Grim howl), mist-rise spawning in Corpse fog. **Cryptstalker:** wall and ceiling locomotion (surface-normal movement on tagged tomb and catacomb meshes), the `found` flag from Whip sweep. **Wraithmoth:** target the brightest light (query the survival lane's light registry), torch snuffing on hit, the face grab with a screen-dark client effect. **Palestrider:** `noSprintStamina` while ridden. **Tombback:** slab as a blocking component with its own HP, carcass burying (tamed). **Gloamcrow:** circle-over secrets (query `world_pois` secret markers within 40 m), land on full graves, subsidence scream |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_RaiseDead` (summon from carcass; Second wave picks 2), `UWRGA_CorpseExplosion` (AreaSpell nova centred on an owned minion), `UWRGA_ArmyOfTheDead` (raise all carcasses in 1,200 cm), `UWRGA_ShadowBolt` (UWRGA_Spell, decay payload), `UWRGA_CurseOfWeakness` (UWRGA_Spell, curse payload, no damage), `UWRGA_LifeDrain` (channelled beam, heal 50%), `UWRGA_LichForm` (transform: a tag that zeroes Souls costs, plus the look), `UWRGA_GraveStrike` and `UWRGA_ScytheSweep` (UWRGA_Melee Wildbrands; Death mark tag on hit). All Necromancy abilities call `SpendSouls`. `ClassForNode`: a new `necromancy` block mapping the 9 ability node ids. Passives (`deathless`, `shared_pain`, `soul_siphon`, `death_mark`, `reaping_stride`, `final_harvest`, `soul_shield`) through `UWRPathComponent::HasNode`, as Fire's are |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `nhal`, `bell_ringer`, `crypt_mother`, `ossuary_lord`, `gravedigger`. **Nhal:** 12 `AWRGraveTomb` actors (closed / open / sealed; open ones spawn Risen on a timer; sealed by a Nature-element hit within 200 cm or a heartwood-stake interact), the Opening (lantern actors gutter and go dark, `AWRSky` arena light off), Lay to rest (furthest-tame target, root and pull-out interact), the Keen as a line-of-sight cone, the Shroud grab, P3 Procession (40 shade actors on a closing ring spline with gaps, reversing on a toll timer). **Bell-Ringer:** an `AWRBossProp` rope with part HP (damage multipliers by type); on break, swap move set and stance. **Crypt Mother:** 8 egg-sac `AWRBossProp`s with hatch timers and torch-hold burn; brood-on-back count reduced by fire hits on a back hit-zone. **Ossuary Lord:** a collapse state (pile actor with 3 keystone parts), reform timer, bind checks (Nature root, 2 lime pots, all keystones), the chandelier chain prop. **Gravedigger:** roaming on a spline with dig stops and a day-sleep lair; the Buried state (root + interact dig-out), the mortsafe cage grab (attach to a back socket, door part HP), the lantern part (on break, a blind-like target-memory loss for 10 s) |
| New `AWRGraveTomb` | An openable grave: closed / open / sealed; a Risen spawn point; a dig interaction (pick, 3 s) for cenotaphs and secrets that spawns its loot table (lantern + tag for cenotaphs) |
| `WRWorldDressing.cpp` | `FWRBiomeHazards::KindAt` gains `lime` and `subsidence` from `island_dressing.json` hazard cells. New: `AWRSubsidence` (weight check → 1.0 s ring tell → drop the cell's floor into a pit volume, maybe wake a den), the lime volume (damage tick plus durability loss; ×2 in rain from `WRSky`), `AWRDraught` volumes (timer; on gust, extinguish held torches and campfires inside through `WRHeldTool` and the light registry). Kit and hand POI spawning from `world_pois.json`; tōrō lanterns as pooled lights (one shared material, distance-culled) |
| `WRHeldTool.cpp` | Torch `Extinguish()` and `Relight()` (hold Interact at any flame 1 s) |
| `WRProjectile.cpp` | `lime_pot` as a ballistic, never-homing projectile with a splash; Nhal's Lantern cast and the Bell-Ringer's headstone as ballistic boss projectiles (homing off) |
| `WRGameMode.cpp` | The catacombs need the world-map W3 fix (skip `undergroundMargin` under the landscape). No underground claims (DD rule; world-map KEPT): the claim totem refuses placement in catacomb volumes |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The Blood Moon timeline: sky and moon swap, the Ring pillar, bells, forced Corpse fog, procession spawning on road splines and light-seeking, camp vigil at safe-zone edges, claim raids through `WRRaids` (respecting the Creature raids setting; never on PvE), Soul and raise multipliers, dawn collapse to bone piles, the Mourning ×3 mining |
| `WRRaids.cpp` | Accept a procession as a raid party source (the event passes its group in) |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 Graveyard shrines (tōrō lantern lighting instead of a brazier); Morwen (MetaHuman, lantern-rack and lychgate idle loops); quests |
| `WRBodyChange.cpp` (fire-frost's / hero lane) | Necromancy parameters from 2.18: pale albedo blend, lips and nails, iris ring with dark glow, hair desaturation, temple and wrist vein reveal at 75, raise flare, Lich form override and crown/breath Niagara |
| `WRUIHud` / `WRPlayerUI` | The Souls flame row; the Deathless bell icon; decay meters (combat's status UI); minion count pips |
| `WRAudioDirector.cpp` | Graveyard pools, sub-zone beds, the bell-on-beat-1 sync, Nhal's 3 sections (the Opening's 2-bar silence), Blood Moon layer, corruption whispers at 75 |
| `WRSky.cpp` | Bruise-sky presets, moonless night for this biome, Ground mist / Corpse fog (knee to chest height), Blood Moon sky and red moon, the chapel's lantern glow |

### 3.3 Assets and the art route

**Route (KEPT):** concept picked by Aaron → Hunyuan3D 2.1 base → Blender finish (sculpt, retopo, real textures) → `Tools/art` rig scripts → **real clips** from Fab animal packs or Truebones mocap, retargeted with UE's IK Retargeter → every creature shown standing at scale beside the hero before any behaviour is wired. No engine primitives, no unfinished AI meshes.

| Asset | Route and clip source |
|---|---|
| Gravehound | Concept from a Xolo/greyhound with a bone face; Hunyuan base; Blender skin-over-bone sculpt (ribs, spine ridge, skull face), subsurface thin-skin material. Clips: a Fab wolf or dog pack (walk, trot, run, bite, pounce, howl, death) plus a keyed dig-out-of-soil rise |
| Cryptstalker | Custom sculpt from amblypygid references; rig with 2 whip chains (spline IK, 14 segments) and palp bones. Clips: a Fab spider pack for the 6 walking legs (sideways gait, wall climb), keyed palp snatch and whip sweeps |
| Wraithmoth | Concept death's-head hawkmoth; wings as rigid plates with a fold rig. Clips: a Fab moth or butterfly pack if one fits (fly cycle, land, walk), else keyed; the face-grab keyed against the hero rig |
| Palestrider | Fab horse pack (full locomotion and rear, kick, death) with a new head sculpt (bone-pale face) and ragged mane grooms |
| Tombback | Concept burying beetle; Fab beetle pack clips (walk, rear, attack); the slab is a separate mesh with stone material, attached to a socket |
| Gloamcrow | Fab crow or raven pack (fly, glide, hop, peck, caw); grey hood texture |
| Mournhare, Marrowgoat | Fab rabbit and goat packs; Marrowgoat long-hair grooms and a bone prop in the mouth |
| Wickbeetle | Small keyed beetle on a Fab insect rig; wax shader with subsurface and an emissive tail organ |
| Risen (3) | Human skeleton from a Fab skeleton pack (with its clips: walk, attack, death) dressed in grave wrappings (cloth sim) and iron bands; Risen hound from the Gravehound skeleton; the bearer as two skeletons on a shared coffin rig |
| Bell-Ringer | MetaHuman-sized proportions scaled to 4.2 m with long-arm bone edits, a sculpted half-bell helm; clips from a Fab giant or ogre pack (heavy swings, grab, throw) plus a keyed rope-pull |
| Crypt Mother | The Cryptstalker rig ×2.25 with a back-brood mesh (20 young as instanced meshes that detach) |
| Ossuary Lord | Kitbashed in Blender from a scanned bone set (instanced bones on a giant skeleton rig); collapse as a physics-driven chaos pile with a reform reverse-play; keystone skulls with emissive soulstone |
| Gravedigger | Tall gaunt figure on a scaled human rig; a horse-skull mask with a cloth-sim sheet; mortsafe cage and spade props; clips from a Fab undead or giant pack (heavy two-handed swings) plus a keyed dig cycle |
| Nhal | New concept (Aaron picks): long-limbed giantess on a custom rig (digitigrade legs under skirts, long fingers with an extra joint), the moth-veil as layered cards with a WPO flutter and a Niagara moth swarm for the lift-off; jaw-unhinge blend shape; the scythe prop. Clips: a Fab tall humanoid or giant pack retargeted (walk, swings, slam), plus keyed keen, Opening and kneel |
| Morwen | MetaHuman preset (attractive, late 20s), outfit as 2.11 |
| Landmarks | Chapel of Nhal (Blender modular Gothic kit: piers, ribs, tracery, apse, broken tower), the Great Yew (hero tree from a Fab yew or a heavily edited oak, hollow trunk sculpt), the Belfry (tower kit plus bell), the Bone Giant (a scaled skeleton with root meshes through it), the Lantern Stair (stair spline modules plus tōrō) |
| Kit | Fab graveyard and cemetery packs (headstones, mausoleums, iron fences, statues: licence rule, use unless explicitly barred), plus our own: tōrō lanterns, mortsafes, lime kiln, Watch House, lychgate, catacomb modules (niche walls, skull walls, quarry galleries, cisterns), yews, hawthorns, asphodel |
| Ground and sky | Chalk turf, grave soil, asphodel, flagstone, bone scree, lime crust, limestone; bruise-sky HDRs; ground-mist volumetric fog profile |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles and Kenney, layered per species. About 8 files per creature and 12+ per boss. Loudness matched.
- **Music:** the 2.16 cue sheet, library first (about 20 cues counting stems), credited; the bell is one recorded church bell tuned to F across every cue. The keening voice is a real solo vocalist recording (library or commissioned).
- **Beds:** 5 sub-zone beds (G1 to G5), a deep catacomb bed for L2 and L3, the Blood Moon bed, the chapel's wind-through-ruins bed.

### 3.5 Map dressing

- `Tools/terrain/dressing.mjs` reads the Graveyard entries in `world_pois.json`:
  - It places the 13 hand sites, 5 landmarks and shrines at their coordinates.
  - It fills kit POIs per sub-zone with Poisson spacing of 150 m or more.
  - It walks the old roads and paths and adds a kit POI wherever a point is more than 125 m from any POI.
- **Grave rows** are placed along row splines (2 m spacing, four headstone styles, 8% leaning, 5% fallen), each with a tōrō every 6th stone, written as instanced meshes.
- **Lantern roads:** a tōrō every 30 m on every old road (pooled lights, one shared material).
- **Hazard cells:** subsidence (12% of grave-soil cells in G1), lime pits (G3 pit polygons), draught volumes (C3 and the 2 kit corridors) written as `hazards` cells and volumes.
- **Ground layers** painted from sub-zone polygons by the existing auto-material.
- **Catacombs:** built as a modular kit level graph from `world_pois.json` (rooms and corridors snapped on a 4 m grid), streamed as their own cells under the grave fields.
- **The landmass** comes from world-map's B5 step. This lane only dresses it.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R4 pieces first** (other lanes): decay status, darkness and lights, corruption, Rest, the catacomb `undergroundMargin` fix | Everything below uses them |
| 1 | **Proof of one: a Gravehound pack rising from a grave field at night, end to end.** Concept (Aaron picks), mesh, real wolf/dog clips plus the dig-out rise, the fused voice and the Grim howl, attack rows with socket hits (Rising, Bite, Hamstring, Pounce), decay on hit, unlit double aggro, corruption building, Ground mist / Corpse fog, and one G1 POI dressed round it (the Dog Graves, H3) with tōrō lanterns | Proves night spawns, a pack with no attacker limit, visible-contact damage, real clips, a fused voice, decay, darkness and corruption, and a dressed place in one item |
| 2 | **The Necromancy path:** `necromancy.json`, Souls and wisps, Deathless, `RaiseFromCarcass` and the raised material, the 9 ability classes in `ClassForNode`, the passives in `ApplyHit`, proc FX, the 5 CB combos | The round's headline; needs only step 0 and a creature to raise (step 1) |
| 3 | Graveyard terrain dressing: G1 to G4, grave rows, lantern roads, the 5 landmarks, 13 hand sites, kit fill, crypt mouths; resources (soulstone outcrops, mortsafes, yews, limestone, lime kilns) | The world the rest stands in |
| 4 | The other 8 creatures with clips, voices and rows; day and night tables; living-world beats | Reuses step 1's frame |
| 5 | Micro-hazards (subsidence, lime, draughts) and torch extinguish; weather reconcile | After the ground exists |
| 6 | The catacombs: G5 and L1 to L3 kit, the 12 hand rooms, ore bands, the Starfall Pit, cisterns | A big build; needs the underground fix and creatures |
| 7 | The Risen archetype, then minis in order of new tech: the Bell-Ringer (rope part, raises), the Gravedigger (roaming, bury, cage), the Ossuary Lord (reform, keystones, lime, chandelier), the Crypt Mother (sacs, brood) | Risen first, since two minis and the major use them |
| 8 | Nhal: the chapel arena, tombs and sealing (Nature hits and heartwood stakes), phase 1, the Opening, phase 2 raises and darkness, Lay to rest, phase 3 behind the artifact flag; drops; music | The biggest single item; needs the Risen, tombs and the path for testing raises |
| 9 | Shrines, Morwen and her 6 quests, run-back stones, fast travel | After POIs and the hero rig |
| 10 | The 5 secrets and lore items | After the catacombs and quests |
| 11 | Music set and sub-zone beds | Independent; can slot in anywhere after step 3 |
| 12 | The Blood Moon event | Needs the Risen, processions on roads, raids, and the minis' event changes |
| 13 | Gear: Gravecloth fit on the MetaHuman bodies, graveyard items, lime kiln, tōrō piece | After the R2 slot split (survival-crafting) |
| 14 | Body change for Necromancy | After the MetaHuman hero (R0) |
| 15 | Nature ↔ Necromancy invasions through Rootgrave | Needs both biomes' rosters, the Gravedigger, and the invasion frame |

**Gates:** compile, cook and package only. Aaron tests each step.
