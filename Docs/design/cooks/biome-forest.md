# Cook: biome-forest (Ancient forest / Nature, round R3)

Lane: biome-forest. Written 2026-11-13. Scope: FULL-GAME 3.2.4, the Ancient forest rows of 3.3, and the Nature row of 4.2. This covers the whole forest and the whole Nature path.

**Tags.**
- **SPEC** keeps Raven's design, or a built thing, exactly as written.
- **GAP-COOKED** is this lane's design. It replaces the FULL-GAME first draft.
- **KEPT** means a number or name another 11-13 cook already set (world-map, combat, survival-crafting, hero). It's restated here so the build has one page. This lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** where the forest sits (belt B5, east of the Spine), the camps, the village site, Sylvaroth's arena and the Hollow Root coordinates, the POI count (175), the weather weights, the Grovekin and their trail trees, the Rootgrave pass, and the readable-night rule (glowworm and foxfire banks).
- **combat.md:** the Nature spell and Wildbrand numbers, the bleed status, the attack-row schema, the creature archetypes (Thornlynx canopy drop, Rootcoil constrictor), the boss template, and visible-contact damage.
- **survival-crafting.md:** the Heartwood armour stats, briarproof leathers, the forest crops (meadow oats, Iðunn apple), beekeeping with Hiveback, Fernsheep and Dappledoe husbandry, forest fishing, the three forest farm set-pieces, the Heartwood lodge set, the Grove press, and the four forest talismans.
- **hero.md:** the set name **Heartguard** (renamed from Heartwood to stop a clash with the resource item) and the Nature body-change row.

**Creature count.** The brief says about 9. This cook keeps **11**. The forest is the largest and richest biome (7.0 km²; CAL allows 6 to 12), and survival-crafting and combat already build on Fernsheep, Dappledoe, Glimmerfinch, Hiveback, Truffler, Elderelk, Burlbeast, Thornlynx and Rootcoil. One draft creature is cut: the **Treeglider**. Its job (scouting nests) overlaps the Truffler and the Glimmerfinch, and nothing else uses it.

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Monster Hunter World, the Ancient Forest** ([Inven: Tokuda lecture](https://www.invenglobal.com/articles/6549/creating-a-dense-open-world-a-lecture-from-yuya-tokuda-the-director-for-monster-hunter-world), [Mein-MMO](https://mein-mmo.de/en/hinter-den-kulissen-von-monster-hunter-world,177917), [Twinfinite](https://twinfinite.net/features/monster-hunter-world-beta-impressions/)) | The whole map was redesigned around a food chain, from insects to herbivores to the apex Rathalos. Everything started from one ancient tree in the centre, and everything flows into it. The verticality reads well, but players complain they get lost in it | **One idea drives the biome: the forest is one living body.** A single root-and-fungus network joins every giant tree, and Sylvaroth is its walking heart (2.20). **Verticality with a floor:** canopy paths sit only in F5 and at a few named trees. The forest floor is always a full route, so nobody has to climb to get anywhere |
| **Valheim, the Black Forest** ([XGamingServer](https://xgamingserver.com/blog/valheim-black-forest-guide/), [PC Gamer](https://www.pcgamer.com/games/survival-crafting/valheim-biome-order/), [Game Informer](https://www.gameinformer.com/2021/02/08/valheim-how-to-summon-the-elder-and-find-the-swamp)) | Your first real danger after the cosy Meadows. Trolls are the walking threat you learn to kite. The Elder is a tree-giant that spawns roots and is best fought behind the arena pillars with fire arrows | **Meadows outside, dark wood inside.** The coast is open, sunny farmland (F1); the threat grows as the canopy closes. Mossmaul fills the troll's job: the big walking danger you hear before you see. Sylvaroth keeps cover stones in the arena. **We don't copy** fire as the boss answer: Raven's rule says Necromancy (2.14) |
| **Elden Ring, the Erdtree Avatars and Ulcerated Tree Spirit** ([eldenring.wiki.gg](https://eldenring.wiki.gg/wiki/Erdtree_Avatar), [eldenring.wiki.gg](https://eldenring.wiki.gg/wiki/Ulcerated_Tree_Spirit)) | Big tree bodies with readable staff and slam swings and one wide breath. The Tree Spirit can be lured onto fire pyres and killed by the arena | **A tree titan reads by its swing, not by particles.** Sylvaroth's big moves are body moves: arm sweeps, slams, a charge. The arena itself is a weapon both ways: the heart-trees heal it, but you can cut their lines |
| **Breath of the Wild, Korok Forest and the Lost Woods; Ghost of Tsushima** ([TechRadar](https://www.techradar.com/news/ghost-of-tsushima-interview-nature-sprawls-in-ps4s-epic-samurai-swan-song)) | A forest that guides you with its own signs (fog, torches, the wind), with no UI | **Trail trees** (KEPT Grovekin waymark) bend toward roads; Glimmerfinch flocks lift over fresh kills and ripe hives; foxfire banks mark the old roads at night |
| **Enshrouded, Palworld, Ark** | Grove-style biomes rich in tames, farm animals and crafting plants | The forest is the **farm-and-tame biome**: the most passive and neutral creatures on the map, the main village, beekeeping and orchards. The threat is concentrated, not everywhere |
| **Horizon** | Herds that move together, a predator that reads as a silhouette at range | Elderelk herds with their Hornwarden walking a route you can see across the meadows |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Pando ([Earthdate](https://www.earthdate.org/pando-a-forest-of-one), [National Geographic](https://www.nationalgeographic.com/environment/article/the-biggest-living-thing-on-earth-is-being-nibbled-to-death)) | One quaking aspen clone in Utah: tens of thousands of trunks on one root system, about 6,000 tonnes, one organism | **The forest is one tree.** Every giant shares Sylvaroth's root system. The four heart-trees are its organs |
| Humongous fungus, *Armillaria ostoyae* ([Guinness](https://www.guinnessworldrecords.com/world-records/82857-largest-bioluminescent-organism), [Earthdate](https://www.earthdate.org/episodes/the-humongous-fungus-among-us)) | The world's largest organism, about 965 ha, mostly underground. Its mycelium glows faintly green ("foxfire"), and it kills conifers as it spreads | **Foxfire** lights the forest at night (world-map KEPT). The fungus's black rhizomorphs ("bootlaces") are the **tree-killer**: the Bootlace blight jar cuts Sylvaroth's heart-tree lines (2.14). Real biology becomes the boss counter for players without Necromancy |
| Mycorrhizal networks and "mother trees" ([Mother Tree Project](https://mothertreeproject.org/wp-content/uploads/2020/01/the-mother-tree_the_word_for_world_is_still_forest.pdf), [Newswise](https://www.newswise.com/articles/exploring-the-underground-connections-between-trees)) | Fungal threads join tree roots; old hub trees may feed young ones (the idea is real and contested) | The heart-trees **feed** Sylvaroth through glowing root lines you can see and cut. The Growth resource fills faster near plants: you draw on the same network |
| Strangler fig ([Britannica](https://britannica.com/plant/strangler-fig-tree), [Cloudbridge](https://www.cloudbridge.org/wp-content/uploads/2011/11/NatureNotesNo5.pdf)) | It seeds in a host's canopy, sends roots down that fuse into a lattice, the host dies and rots out, and the fig stands hollow | **Sylvaroth's body** is a strangler lattice round a rotted-out host. The **Strangler Arch** landmark is a hollow fig tower you can climb inside |
| Japanese giant hornet and the bee ball ([Phys.org](https://phys.org/news/2012-03-japanese-honeybees-swarm-huge-hornet.html), [Nature 1995](https://www.nature.com/articles/377334a0)) | A scout hornet marks a hive with a pheromone and the nest converges on it. Japanese honeybees mob a hornet in a ball and cook it at about 47 °C | **Bramblewasp scouts mark you** (a pale pollen-like dust; water washes it off) and the swarm converges. **Hivebacks heat-ball wasps:** wild and tamed Hivebacks mob Bramblewasps, the counter-tool for the Bramblequeen |
| The leshy ([Clara Bush](https://www.clarabush.com/slavic-mythology-forest-spirits/)) | The Slavic forest lord leads travellers astray and copies voices; hunters left him bread | **Grovekin offerings** (bread stones at trail trees) and the **Lost Path** secret: a foxfire path that loops unless you follow the trail trees' bends |
| Herne the Hunter ([RSC](https://rsc.org.uk/the-merry-wives-of-windsor/about-the-play/herne-the-hunter), [Mythopedia](https://mythopedia.com/topics/herne-the-hunter/)) | The antlered huntsman of Windsor Forest, hanged from an oak, who leads a ghostly hunt; Grimm tied him to the Wild Hunt | **The Hornwarden** and his herd ride the **Wild Hunt** across the meadows on Bloom nights. His route passes the **Hanging Oak** (a rope still on it). Living beasts, never ghosts (DD: no zombies) |
| Black Annis ([Wikipedia](https://en.wikipedia.org/wiki/Black_Annis)) | An English hag of the Dane Hills who hid in a great oak at her cave ("Black Annis' Bower") and leapt on passers-by | **The Grey Matron** lives in the **Bower**: a ring of hollow oaks. She only attacks from above |
| Hulder ([Wikipedia](https://en.wikipedia.org/wiki/Hulder)) | The Scandinavian forest woman, beautiful from the front, hollow-backed like a rotten trunk | Grovekin carvings: women whose backs are bark, at every village gate. It's never explained. The art direction for the forest's people: beautiful, half tree |
| Elder Mother (Danish *Hyldemoer*; [Wikipedia: Elder Mother](https://en.wikipedia.org/wiki/Elder_Mother)) | Cut an elder without asking and she takes her revenge | **Old Gnarl** woke in the newcomers' clear-cut. The axes still stuck in the stumps tell why |
| Pleaching and tree shaping; trail trees (world-map KEPT) | Living trees trained and woven into fences, arbours and arches | **Grovekin architecture is grown, not built:** pleached ringforts, living bridges of trained roots (real Meghalaya root bridges), houses in forks |

### 1.3 Lessons, short

1. **One idea feeds everything: the forest is one body.** The roots, the foxfire, the heart-trees and Sylvaroth are one organism. Every creature, hazard and POI follows from that.
2. **Open at the edge, closed in the middle.** Light falls as you go inland: meadow sun, then green shafts, then the dark under the Elder Canopy. Danger rises with the darkness.
3. **The floor is always a route.** Canopy paths are the fast line and the ambush line, never the only line.
4. **Ambushes always tell.** Every drop from above has a shadow and a leaf-fall rustle 0.9 s before it. Every "log" that's a Rootcoil breathes.
5. **Colour law.** Crimson (#DC143C) is the unblockable cue and gold is "yours or parry". Pollen is pale yellow #E8E0A0, foxfire is pale green #7FE07A, and heartwood amber is #D08A1E (orange-brown). None of them is crimson or the UI gold.
6. **Taking only fallen wood.** Heartwood comes from fallen giants, never from living ones (KEPT). The land's story punishes the clear-cut.

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item | This lane |
|---|---|
| Path theme: vines, healing, plant summons | SPEC (DD) |
| Look, sub-zones, landmarks, POIs, hidden areas | GAP-COOKED 2.1 to 2.6 |
| Signature resource: heartwood | GAP-COOKED 2.7 (the name KEPT; "fallen giants only" KEPT) |
| Hazard: thornbrakes | Numbers KEPT (3.3, world-map); tells and micro-hazards GAP-COOKED 2.8 |
| Weather | Weights KEPT (world-map); effects reconciled 2.9 |
| Event: the Bloom | GAP-COOKED 2.10 |
| Villages | Site KEPT (world-map); the place GAP-COOKED here; the people need the villages cook (already in LATER-COOKS) |
| Shrine, path master | RAVEN-PLANNED exist; who and where GAP-COOKED 2.11 |
| 11 creatures | Names, roles and taming KEPT; bodies, attacks, voices, tables GAP-COOKED 2.12 |
| 4 minis, 1 major | Names and premises kept; fights GAP-COOKED 2.13, 2.14 |
| Voices, music | GAP-COOKED 2.15, 2.16 (tempo, key and instruments SPEC 3.3) |
| Nature tree | Nodes SPEC (ST). Spell and Wildbrand numbers KEPT (combat). Resource, root passive, stat and passive nodes GAP-COOKED 2.17 |
| Body change | Look KEPT (hero.md); detail GAP-COOKED 2.18 |
| Gear | Heartguard and briarproof leathers KEPT; forest items GAP-COOKED 2.19 |
| Story through environment | GAP-COOKED 2.20 |
| Invasions Nature ↔ Necromancy | GAP-COOKED 2.21 (built in R4 when the Graveyard opens) |

### 2.1 Where it sits and how it reads (KEPT placement, GAP-COOKED look)

**Placement (KEPT, world-map 2.2 and 2.4):**
- Belt B5, east of the Spine, south of the Old Island.
- y +2,350 to +4,600. Land runs from the Spine's cliff foot at about x −850 to the sea at about x +2,500. 7.0 km².
- **Neighbours:**
  - Frozen tundra to the north, over the **Pine Sound** and through the **south-east neck** (x +250 to +700, y +1,950 to +2,350): pine hollows grading into old growth.
  - Crystal caves to the south at y +4,600: crystal-veined boulders under moss.
  - Graveyard across **Rootgrave** (saddle at −1,150, +3,300; 190 m high).
- **Bands.** The forest is wider than the band rule's 1.9 km, so the lowland runs deep:
  - Highland: x −850 to −400 (140 to 620 m from the crest).
  - Midland: x −400 to +450.
  - Lowland: x +450 to the coast.
- **Camps (KEPT):** (+2,300, +2,800), (+2,350, +3,600), (+2,250, +4,400).
- **The newcomer village (KEPT site):** (+2,100, +3,450). It's the largest on the map.

**The look in one line.** Sunny coastal meadows and old farms climb inland into a forest that gets older and taller with every step. Oaks give way to beech and lime, then to giant redwood-scale trees 80 to 130 m tall, mossed to the crown. At the back, under the Spine's cliffs, four giants stand taller than everything with amber light in their bark: the Four Hearts. By day, green shafts of light fall through gaps. By night, the forest floor glows with foxfire, and glowworms hang under the banks.

**Read rules (GAP-COOKED):**
- **The canopy is a gradient.** Canopy cover goes from 0% at the coast to 40% in F2 and F3, 75% in F4 and 90% in F5. Light and fog follow it. You always know how deep you are by how dark it is.
- **Trunks are the skyline.** Every landmark is a tree or a gap in the trees. From the meadows, the Four Hearts and the Eldest stand over the canopy line, so you can see where the danger lives from the first hour.
- **Gaps are places.** Every POI inside the forest sits in a light gap: a fallen giant, a clearing, a stream, a clear-cut. A shaft of light means something's there (triangle rule, world-map).
- **Ground tells the zone:** grass and flowers (F1), river gravel and ferns (F2), cobbles under moss (F3), deep needle and leaf litter (F4), moss and root-mat (F5).
- **Night light (world-map KEPT):** foxfire banks (pale green, on fallen wood and old roads), glowworm curtains under stream banks and cave mouths, Glimmerfinch roost-glow (a faint shimmer in hedges), lit windows in the village and the Hermit's Oak, and the Four Hearts' amber sap veins, the brightest thing in the forest.

### 2.2 Sub-zones (GAP-COOKED; the five names from CAL's list)

| # | Sub-zone | Where (x, y m) | Band | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|---|
| F1 | **Meadows and Farmland** | Coast band x +1,400 to +2,500, the whole y range. All three camps and the village. 2.2 km² | Lowland | Rolling flower meadows, hay fields gone wild, hedgerows, orchards, stone field walls, a sandy coast with dunes and a salt marsh at the Pine Sound | Meadow green #7FA34A, wildflower white and violet #E8E4F0 / #8A6BB0, wheat gold #C9A85A, sea blue #4E7FA0 | Hedgerows, field walls, haystacks, orchards, beehive trees, oat fields, windbreak oaks, the newcomer wrecks | Skylarks, bees, wind in grass, distant surf, sheep and cowbells near the village |
| F2 | **Streams and Falls** | North strip: x −600 to +1,400, y +2,350 to +3,000. Holds the river **Alder**, the Great Falls and the tundra neck border. 1.3 km² | Lowland to midland | A rocky river valley through alder, ash and willow. Fern banks, gravel bars, mossy boulders, a mill race, rope fords. The Alder drops off a 45 m basalt scarp at the Great Falls | Fern green #4E7A3A, river slate #5A6870, alder bark grey #7A7468, white water | Fords, stepping stones, the mill race, Grovekin root bridges, fish weirs, otter slides | Running water (rises at the falls), dipper song, a woodpecker far off, frogs at night |
| F3 | **Grovekin Villages** | South: x −200 to +1,400, y +3,700 to +4,600. The old people's ruined tree-villages. 1.4 km² | Midland | Beech and lime groves with Grovekin houses grown into living trees, pleached-tree walls, moss-covered cobbled lanes, terraced herb gardens gone wild. The Crystal border at the south edge | Beech silver #A8A49A, moss #5E8C31, lime-leaf yellow-green #A8C060, old stone #8A8478 | House-trees, pleached ringforts, bread stones, carved hulder gates, bell-trees, root bridges, herb terraces | Leaves, wood creaks, bells far off (wind in the bell-trees), Glimmerfinch song |
| F4 | **Old-Growth Groves** | Centre: x −400 to +1,400, y +3,000 to +3,700. The deep wood. The Eldest stands at its north-west corner. 1.25 km² | Midland | Giant trees 60 to 100 m with buttress roots, fallen giants (heartwood), nurse logs, thornbrake walls, deep leaf litter. The thickest canopy below F5 | Bark umber #4A3A2A, moss #4E6B2A, shaft light #E8F0C0, shadow green-black #1A241A | Fallen giants, nurse logs, buttress hollows, thornbrakes, foxfire banks, wasp nests | Deep hush, wind high up in the crowns (far above), a creak, a bear's huff far off, owls at night |
| F5 | **Elder Canopy** (canopy paths) | Highland: x −850 to −400, y +2,600 to +4,500, under the Spine's cliffs. Holds the Four Hearts, the Rootgrave mouth and the Grey Matron's Bower. 0.85 km² | Highland | The tallest trees in the world, 90 to 130 m, joined by Grovekin canopy walkways 20 to 40 m up. The floor is root-mat and moss in permanent green dusk. Mist in the morning | Deep moss #3A5222, root grey-brown #5A4A3A, amber sap #D08A1E, foxfire green #7FE07A | Canopy walkways, rope lifts, platform houses, root-mat, sap veins in the giants, the Spine's cliff behind | Huge slow creaks (giants moving in the wind), wind 40 m up, the distant river, a low rumble now and then (Sylvaroth, under the ground) |

**Blends:** 60 to 120 m between sub-zones (world-map rule 5).
- **Tundra neck (F2 north, y +2,350 to +2,500):** pines and snow patches mix into alder; frost on the ferns at dawn.
- **Crystal border (F3 south, y +4,450 to +4,600):** crystal veins in mossy boulders; a few crystals grown into tree roots and lit from inside at night.
- **Rootgrave mouth (F5, x −850 to −700, y +3,200 to +3,400):** world-map KEPT. Leaning headstones and tōrō lanterns deep in the forest side; Gravehounds and Thornlynx fight here at dusk.

### 2.3 Skyline landmarks (GAP-COOKED; 12, one per ~0.58 km², each read from 1 km)

| # | Landmark | At (x, y) | What it is | Reads at night as |
|---|---|---|---|---|
| L1 | **The Four Hearts** (super landmark, reads from 3 km) | −500, +3,600 (F5) | Four giant trees, 140 m tall, standing round Sylvaroth's clearing. Amber sap shows in cracks running up their bark. Their crowns stand 40 m over the Elder Canopy | Four amber-veined columns, the brightest thing in the forest; a pale foxfire haze over the clearing between them |
| L2 | **The Eldest** | −300, +3,000 (F4/F5) | The oldest tree, 130 m, its crown broken off at 110 m by some old blow, a shelf of bracket fungus 8 m wide round its trunk. The Hollow Root is under it | Foxfire glowing out of the cracks at its base; owls |
| L3 | **The Great Falls** | +300, +2,650 (F2) | The Alder drops 45 m off a basalt scarp into a round pool. Spray rainbows at noon | A white curtain in the moonlight; glowworms under the overhang |
| L4 | **The Rotted Treant** | −800, +3,350 (F5, Rootgrave mouth) | World-map SPEC: the Lantern Mourners' champion, a treant fallen on the forest side of Rootgrave, roots rotted black. Its arm reaches toward the pass. Tōrō lanterns hang from its dead branches | The hanging lanterns, unlit, and foxfire crawling over the black wood |
| L5 | **The Clear-cut** | +600, +3,300 (F4) | A 250 m hole in the canopy: the newcomers' clear-cut. Hundreds of stumps. In the middle stands one huge twisted oak that is **Old Gnarl** | A gap of moonlight in the dark forest, and one silhouette standing in it |
| L6 | **The Paper Hollow** | +900, +4,200 (F3) | A dead oak 50 m tall wrapped in a grey paper wasp hive 20 m high, ribbed like a lantern | A dull hum; the hive's shell faintly lit amber by Glowmoth-style grubs inside |
| L7 | **The Bower** | −550, +4,250 (F5) | A ring of six hollow oaks 70 m tall linked by sagging Grovekin walkways, deer hides hung like flags in the branches. The Grey Matron's home | Pairs of green eyeshine moving between trunks |
| L8 | **The Hermit's Oak** | +1,100, +3,550 (F4/F1 edge) | A living oak with a Grovekin house in its fork 12 m up, reached by a spiral stair round the trunk. Smoke from a clay chimney. The path master lives here | Warm window light and lanterns down the stair |
| L9 | **The Strangler Arch** | +300, +4,000 (F3) | A strangler-fig tower 60 m tall whose host rotted away, leaving a hollow lattice of fused roots you can climb inside to a vista at the top. Its lower roots arch over the main village road | Foxfire glowing inside the lattice, like a lantern of roots |
| L10 | **The Antler Barrow** | +1,700, +2,700 (F1) | A long barrow on a meadow hill with an elk skull 6 m across set on its mound and standing stones at its door. The Hornwarden's route starts here | The skull's pale outline against the sky |
| L11 | **The Bell-Tree** | +200, +4,450 (F3, Crystal border) | A dead giant with a Grovekin bronze bell hung 40 m up in it. Crystal veins grow through its roots | Crystals in the roots glow faint violet; the bell rings in the wind |
| L12 | **The Ringfort** | +1,600, +4,200 (F1) | A Grovekin hillfort on a coastal hill, its wall a ring of pleached living beeches grown into one hedge 12 m tall | Campfire light inside (merchant stop) |

### 2.4 POIs: 161 surface + 14 in the Hollow Root = 175 (count KEPT from world-map; contents GAP-COOKED)

**Counts by sub-zone:** F1 45, F2 30, F3 34, F4 30, F5 22.

**Fixed (28):** 3 camps, the village, 12 landmarks (four of them hold a mini lair or the arena), 5 shrines (2.11), 5 caves (2.6), the Rootgrave approach (pass fort), the Four Hearts' run-back stone and door.

**Hand-made story and set-piece sites (36):**

| # | Site | Sub-zone, at | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| 1 | **The Wreck Meadow** | F1, +2,400, +2,900 | The newcomers' ship, its keel broken on the dunes; ribs half buried, a cabin turned into a hen house by later castaways | Wood, iron nails ×6, rope; lore item |
| 2 | **Abandoned farmstead** (KEPT set-piece) | F1, +1,900, +3,050 | Oat field gone wild, a beehive tree, an orchard with one Iðunn apple tree, the farmer's tools hung neatly | Oat and Iðunn apple seeds; a wild Hiveback hive |
| 3 | **The Mill Race** (KEPT set-piece) | F2, +900, +2,550 | A water-wheel ruin on a side stream of the Alder, gears to salvage | Gears ×2 (machinery part 1 scrap), wood, iron ×4 |
| 4 | **Truffle glade** (KEPT set-piece) | F4, +1,200, +3,300 | Rooted-up ground, a pig skull, a basket of rotten truffles | Truffles; Truffler herd |
| 5 | **The Woodcutters' Camp** | F4, +700, +3,200 (Clear-cut edge) | A newcomer logging camp: a crosscut saw, a sledge loaded with oak, three tents flattened. Roots have grown **through** the tents and the bunks, and the woodcutters' bones are held inside the roots. Their axes are still stuck in the stumps round the camp | Axe (worn bronze), wood ×20; lore item "Tally of trees felled" |
| 6 | **The Clear-cut** | F4, +600, +3,300 | Old Gnarl's arena (2.13.1) | Mini |
| 7 | **The Paper Hollow** | F3, +900, +4,200 | The Bramblequeen's hive (2.13.2) | Mini |
| 8 | **The Bower** | F5, −550, +4,250 | The Grey Matron's lair (2.13.3) | Mini |
| 9 | **The Antler Barrow** (door) | F1, +1,700, +2,700 | Standing stones; a stone door in the barrow; inside, a Grovekin hunter laid out with an antler crown and a bow of heartwood. The Hornwarden rests on the barrow at noon | Heartwood longbow (crude); lore item "Antler crown" |
| 10 | **The Hanging Oak** | F1, +1,550, +3,250 | A lone oak in a meadow with an old frayed rope on one branch and antlers nailed above it (Herne). On the Hornwarden's route | Lore item; the Hornwarden passes here at dusk |
| 11 | **Herne's Ford** | F2, +1,100, +2,750 | A wide gravel ford where elk trails cross the Alder; hoof-churned mud, an antler shed in the shallows | Shed antler (bone, glue); fishing spot |
| 12 | **The Great Falls pool** | F2, +300, +2,700 | KEPT legendary fish: **Grandfather Sturgeon** in the pool under the falls. A Grovekin fishing platform on stilts | Fishing; the Alder Grotto behind the curtain (cave C2) |
| 13 | **The Root Bridge** | F2, −100, +2,800 | A living bridge of trained roots across a 15 m gorge (real Meghalaya root bridges), still growing. Moss rails | The main F2 to F5 crossing |
| 14 | **Otter Steps** | F2, +600, +2,450 | A stair of small falls with otter slides and fish bones, a Grovekin eel trap still baited | Brook trout; eel trap (works once a game day) |
| 15 | **The Frosted Ferns** | F2, +450, +2,380 (tundra neck) | Ferns rimed with frost at dawn; a Rime-folk inuksuk standing in a Grovekin herb ring (the neighbours touched) | Lichen, herbs |
| 16 | **The Lost Path** | F4, +200, +3,450 | A foxfire road that loops back on itself (the leshy). Follow the trail trees' knee-bends, not the foxfire, and it leads somewhere else (F-S1) | Secret F-S1 |
| 17 | **The Nurse Log** | F4, +0, +3,250 | A fallen giant 90 m long with a row of young trees growing out of its back, roots straddling it like legs; heartwood exposed at its broken end | The richest heartwood node in F4 (12 hits) |
| 18 | **The Bear's Larder** | F4, +450, +3,600 | A buttress hollow full of bones, a Burlbeast carcass half buried under moss (bears cache kills), claw gouges on the bark 4 m up | A Mossmaul fight; hide, bone |
| 19 | **Wasp Ravine** | F4, +900, +3,550 | A ravine whose overhangs carry eight paper nests | Bramblewasp larvae (steal an egg) |
| 20 | **The Coil Hollow** | F4, −150, +3,600 | A tangle of exposed roots like a nest of logs. One of them is a Rootcoil. Shed skins 12 m long hang in the roots | Rootcoil shed (leather), a Rootcoil |
| 21 | **The Offering Stones** | F4, +1,000, +3,100 | A trail tree bent at the knee pointing at a flat stone with a carved bread loaf on it (offerings to the forest). Coins and bone beads in the moss | Copper ×3, a gem chance; Growth +50 when you leave food on it (once a game day) |
| 22 | **Hermit's Oak** | F4/F1, +1,100, +3,550 | The path master's home (2.11) | Path master |
| 23 | **The Grovekin Long-hall** | F3, +600, +3,900 | A hall grown from a ring of eight trees whose crowns were trained to meet as a roof. A cold hearth, carved hulder posts (beautiful women with bark backs), long tables of living root | Great Shrine (2.11) |
| 24 | **The Cradle Wood** | F3, +350, +4,250 | Hundreds of small bark cradles bound to the roots of beeches, all empty, some grown over. The roots are fat where the cradles are | Lore item "Bark cradle"; the story (2.20) |
| 25 | **The Herb Terraces** | F3, +1,100, +3,850 | Stone terraces of wild herbs, an apiary wall of skeps, a still | Herbs ×20, honey, wax; Hiveback hive |
| 26 | **Bread-stone Lane** | F3, +800, +4,050 | A cobbled lane between house-trees, a bread stone at every door. One door is barred from inside, its house-tree grown shut | Copper, lore item "Door rune" |
| 27 | **The Root-Mine head** | F3, +500, +4,350 | A Grovekin mine whose timbering is living roots; ore carts grown into the walls | Cave C3 |
| 28 | **The Sunken Orchard** | F3, +1,300, +4,400 | An orchard in a hollow with every tree hung with old charms of woven straw; one Iðunn apple tree still fruiting | Iðunn apples |
| 29 | **The Crystal Roots** | F3, +200, +4,550 | A beech whose roots split a crystal boulder; violet light at night (the neighbours touched) | Resonant crystal ×1 (not farmable; one), moss |
| 30 | **The Rootgrave Fort** | F5, −750, +3,250 | The Grovekin pass fort: a living palisade of pleached trees facing west into Rootgrave. Every bent tree in it points at the pass. Its gate is broken outward, and grave salt crusts its west face | Bow, heartwood arrows; a vista down Rootgrave |
| 31 | **Lantern Line** | F5, −700, +3,450 | Tōrō lanterns pushed deep into the forest, each one wrapped and split by roots (world-map KEPT: lanterns on the forest side) | Grave salt ×2 (graveyard resource, one-time) |
| 32 | **The Fern Stair** | F5, −430, +3,620 | A stone stair up from the forest floor to the clearing; the run-back stone stands 50 m east of it | Sylvaroth's door; run-back stone |
| 33 | **The Canopy Lift** | F5, −600, +2,900 | A Grovekin counterweight lift: a basket on a rope up to the canopy walkways 30 m above. Pull the stone counterweight to go up | Route to the canopy |
| 34 | **The Sky Larder** | F5, −650, +3,900 | A platform house 35 m up between two giants, with dried meat and hides still hung | Hides, jerky, a Grey Matron sighting |
| 35 | **The Sinkroot** | F5, −650, +2,800 | A hole where roots have pulled the ground apart, falling to cave C5 | Cave C5 |
| 36 | **The Ringfort** | F1, +1,600, +4,200 | The pleached beech hillfort; inside, a newcomer merchant stop (world-map caravan) and a well | Merchant stop; water |

**Kit POIs (the generator places about 97 with variants; world-map density rules):**

| Kit piece | Sub-zones | Variants | About |
|---|---|---|---|
| Trail tree (a sapling bent at knee height and grown that way; the waymark, world-map SPEC) | all, 120 to 200 m apart on Grovekin roads | 3 | 16 |
| Fallen giant (heartwood at the broken end; 4 to 8 nodes) | F4, F5, F3 | 3 | 12 |
| Hedgerow gap with a stile and a berry patch | F1 | 3 | 10 |
| Beehive tree (wild Hiveback colony in a hollow) | F1, F3 | 2 | 7 |
| House-tree (a Grovekin home in a living trunk: empty, looted, or a Truffler den) | F3, F5 | 3 | 8 |
| Bramblewasp nest (paper, on a branch or an overhang) | F3, F4 | 2 | 7 |
| Thornbrake wall with a cache behind it | F4, F5 | 3 | 7 |
| Kill set-piece (Mossmaul on a Burlbeast; Thornlynx on a Dappledoe; Rootcoil swallowing a Truffler) | F2, F4 | n/a | 6 |
| Stream ford or stepping stones with a fish run | F2 | 2 | 6 |
| Foxfire bank (a glowing log; night readable) | F4, F5 | 2 | 6 |
| Field ruin (a stone barn or byre) | F1 | 3 | 6 |
| Canopy platform (walkway node with a cache) | F5 | 2 | 6 |

### 2.5 Hidden sub-biome: the Hollow Root (RAVEN-PLANNED "hidden sub-biomes"; FULL-GAME name kept; layout GAP-COOKED)

**What it is.** A cave world inside the roots of the Eldest at (−300, +3,000), 10 to 60 m down. The roots are so big that they form halls, and the soil between them has washed out. Foxfire mycelium lines every surface. This is where the Grovekin buried their dead, and where you see that the whole forest is one body: the roots of the Eldest run on into the dark toward the Four Hearts. About 0.8 km of route. 14 POIs.

**Ways in:**
1. **The Eldest's Door.** A crack at the foot of the Eldest, 3 m wide, behind a thornbrake wall (thornbrake pass needed, or burn it).
2. **The Sinkroot** (cave C5) connects through a squeeze at its bottom (F-S3).
3. **Truffler dig.** A tamed Truffler finds a soft spot at (−220, +3,080) and digs it open over 20 s.
4. **The Heart Root** (from inside only). A one-way root tunnel into the Four Hearts' clearing, under the west heart-tree: the shortcut, Elden-Ring style.

**Rooms (in order from the Eldest's Door):**

| Room | What's there |
|---|---|
| **The Root Hall** | A hall 40 m long whose pillars are the Eldest's roots, each wider than a house. Foxfire on every surface. Glowworm curtains. 2 Rootcoil |
| **The Cradle Vault** | The Grovekin dead: hundreds of bodies wrapped in bark and laid in root cradles, the roots grown round and into them like hands holding them. The oldest are nearly roots themselves. Calm, not horror |
| **The Grove of Bones** | A chamber where the roots have pushed up a Grovekin giant's skeleton (2.20) and grown through its ribs. A heartwood spear is still in its hand |
| **The Weeping Roots** | Roots that drip amber sap into stone basins: heartwood resin ×10 |
| **Mossmaul Den** | A hibernation hollow with a Mossmaul asleep (sneak past or fight) and bones of what it ate |
| **The Bootlace Gallery** | Black *Armillaria* rhizomorphs as thick as ropes hanging from the ceiling, glowing green at the tips. The source of **Bootlace blight** (2.19): harvest rhizomorph ×6 |
| **The Thrumming Root** | One root, 6 m thick, that pulses slowly with a deep sound like a heartbeat (Sylvaroth's). Putting your hand on it (Interact) shows a 3 s flash of the four heart-trees on the map. Lore item "Root-pulse" |
| **The Drowned Root** | A flooded chamber where the Alder sinks underground; brook trout and a blind cave crayfish; swim 15 m |
| **The Seed Store** | A Grovekin granary in a dry root pocket: sealed clay jars of seeds from every biome they traded with. One random rare seed per game day |
| **The Wasp Grave** | A paper nest the size of a cart, abandoned, full of dead Bramblewasps balled round a dead Hiveback (the bee ball, the other way round) |
| **The Glowworm Chasm** | A 25 m drop lit like stars by glowworms; a root stair spirals down. Fall = death |
| **The Lattice Chamber** | A room where the roots fuse into a strangler lattice round a void: a Grovekin heart-tree's host that rotted out. You can see that Sylvaroth's body (2.14) is the same lattice |
| **The Iron Seam** | A collapsed wall with iron ×4 nodes and a gem chance |
| **The Heart Root** | The one-way tunnel to the Four Hearts (route 4) |

**Light:** full dark except foxfire (12% light) and glowworms. Bring a lantern or a tamed Glowmoth. **Music:** the "Hollow Root" cue (2.16).

### 2.6 Caves (CF rule: every biome has caves; deeper is better)

| Cave | At | Depth | Ore | Creatures | What's special |
|---|---|---|---|---|---|
| **C1 Badger Sett** | F1, +1,800, +4,000 | 10 m | Copper, tin | Truffler 3 | A dry sandy cave under a meadow hill. The newcomers' first cave |
| **C2 Alder Grotto** | F2, +300, +2,660 | 25 m | Copper, tin, iron (1) | Rootcoil 1 | Behind the Great Falls' curtain. Glowworms. Wet |
| **C3 Root-Mine** | F3, +500, +4,350 | 40 m | Iron ×3 | Mossmaul 1, Bramblewasp nest | The Grovekin mine, timbered with living roots. A collapsed shaft leads to a gem pocket (pick, 6 hits) |
| **C4 Foxfire Cellar** | F4, +700, +3,150 | 30 m | Tin, iron ×2 | Rootcoil 2 | A cave in the Clear-cut's edge whose ceiling is a mat of foxfire. Bootlace rhizomorph ×2 |
| **C5 The Sinkroot** | F5, −650, +2,800 | 60 m | Iron ×4, gem chance | Mossmaul 1, Rootcoil 2 | The deepest. A squeeze at the bottom joins the Hollow Root (F-S3) |

### 2.7 Resources (GAP-COOKED; heartwood name and rule KEPT)

| Resource | Where | Gather | Uses |
|---|---|---|---|
| **Heartwood** (signature) | The broken ends of **fallen giants only** (F3, F4, F5; the Nurse Log; Elder Falls in the Bloom). Never from a living tree, never farmable (survival-crafting KEPT) | Axe on the amber core: 8 a hit (woodcutting KEPT). Node respawns in 3 game days. A tamed Burlbeast within 15 m: +25% | Heartguard set. Heartwood bow and shield. Heartwood lodge set. Grove press. Battering ram. Heartwood seedling decor. Rune of Thorns (KEPT) |
| **Heartwood resin** (amber sap) | Weeping wounds on fallen giants, the Weeping Roots (Hollow Root), and the Four Hearts' bark after the boss dies | Knife or hand, 2 per node | Briarproof leathers, Ironbark brew (KEPT), briar balm, lacquer for Heartguard |
| **Honey, wax, royal jelly** | Wild beehive trees (F1, F3) and the beehive piece (KEPT beekeeping) | Smoke first (a torch held near the hive for 3 s calms it) or the Hivebacks defend | Food, brews (KEPT), wax for candles and ear plugs (KEPT) |
| **Truffles** | Truffle glade, oak roots in F4 | Only a Truffler finds them (or the Truffle sense perk) | Cooking (high value), Truffler taming food |
| **Bootlace rhizomorph** | Bootlace Gallery, Foxfire Cellar | Knife, 2 per node; it glows for 1 game day after cutting | **Bootlace blight** jar (the Sylvaroth counter, 2.19) |
| **Foxfire wood** | Foxfire banks (rotten logs) | Hand | Foxfire lantern (a cold green light, 8 m, never goes out, never draws dark-hunters, R4) |
| **Thorns** | Thornbrake edges | Knife (no bleed while cutting with a knife held) | Briar balm, thorned arrows (bleed 20), Thorned skin rune scrap |
| **Shed antler** | Herne's Ford, Dappledoe pens (KEPT) | Hand | Bone, glue |
| **Iron, tin, copper** | Caves (2.6) | Pick | Metals by depth |
| **Oak, beech, alder wood** | Every living tree except the giants and heart-trees | Axe | Wood. **Felling within 60 m of a heart-tree or a giant's root crown is impossible** (the axe bounces: "The roots hold it."). Clear-cutting the meadows and F2 to F3 is allowed |
| Meadow oats, Iðunn apple | Crops (KEPT, survival-crafting) | Farm or wild nodes | Food, elixirs, the Dadeni draught |

### 2.8 Hazards

**Thornbrakes (SPEC 3.3 numbers; tells GAP-COOKED)**

| Item | Value |
|---|---|
| Effect | Pushing through: **6 bleed buildup a second, run speed ×0.5** (KEPT). The proc is combat's bleed burst (12% max HP + 15). It can kill |
| What they are | Walls of bramble 2 to 4 m tall and 3 to 12 m deep with thorns as long as a finger, grown round the giants' root crowns, across old roads and round caches |
| Tell | Dark red-brown canes with pale thorn tips; a dry crackle and a scrape sound on contact; your character lifts their arms to shield their face (animation layer). The first tick comes 0.6 s after contact so a step back is free |
| Passes | **Any Nature points** (main or subclass): you walk through at full speed, untouched, and the brambles part round you (Raven: paths matter outside combat). **Briarproof leathers** (KEPT, Chest). **Heartguard 4** (KEPT). **Briar balm** (world-map KEPT): buildup ×0.3. **A torch** burns a 4 m gap that stays open 5 game hours (KEPT); fire spells do the same |
| Where | About 9% of F4 and F5 floor; every root crown of a giant; 4 old roads in F4 cut by them, each with a route round it 80 to 150 m longer |
| Creatures | Every forest creature ignores them (CR rule) |

**Micro-hazards (GAP-COOKED; one per sub-zone at most; each has ≥ 0.9 s of tell; each can kill)**

| Hazard | Sub-zone | Tell | Effect | Answer |
|---|---|---|---|---|
| **Widowmakers** (dead hanging branches, a real logger's term) | F4 | Dead grey limbs caught in the canopy over a path; a loud crack and a shower of bark 1.0 s before the fall; a shadow grows on the ground | Falls on any player or creature under it that's running, fighting, or within 6 m of a Mossmaul roar: 70 damage, knockdown, radius 2 m | Look up; walk under them; they can be shot down from range on purpose (30 damage brings one down) to crush a creature |
| **Leaf pits** | F5 | Leaf litter sagging in a ring, a hollow drum sound under your feet on the edge step, a few leaves trickling down a hole | Root holes under the litter, 4 to 9 m deep (fall damage KEPT rules; over 18 m is lethal, the deep ones are near the Sinkroot). Some drop into cave galleries | Walk the root-mat; a Truffler snorts at pits within 10 m |
| **Bramblewasp nests** | F3 | A paper nest on a branch or overhang; a rising hum within 15 m; scouts circling | Hit the nest (any hit, even an arrow) or stand within 4 m for 5 s: the scout marks you (2.12) and the nest swarms (6 to 10 wasps) | Smoke (a torch held 3 s at the nest calms it for 60 s); water washes the mark off; Hivebacks fight the swarm |
| **Fords in spate** | F2 | Only after a Rain block: the Alder runs brown and loud, logs riding the current | Crossing a ford in spate: 900 cm/s push downstream; at the Great Falls lip it takes you over (45 m, lethal) | Use the Root Bridge or the mill race weir; wait a block |

F1 (the meadows) has no micro-hazard: it's the spawn coast.

### 2.9 Weather (weights KEPT from world-map; effects reconciled)

world-map gives Clear 35, Overcast 20, Rain 30, Morning mist 15, with **Rain** as the signature (crops +25% growth, Truffler finds double). survival-crafting gave "Rain 40%, 15 min" with plots irrigated, burns half as long and tracks fading 2× faster. **Reconciled:** the map's weights stand; Rain carries both cooks' effects.

| State | Weight | Effect |
|---|---|---|
| Clear | 35 | Green light shafts in F4 and F5 (volumetric); Glimmerfinch flocks fly |
| Overcast | 20 | Flat light; F5 goes from dusk to near-dark (foxfire shows by day) |
| **Rain** (signature) | 30 | Shared Wet rules (world-map). Every outdoor plot counts as irrigated; crops +25% growth for the block; Truffler finds double; burns last half as long; tracks fade 2× faster (survival-crafting). Under F4 and F5 canopy, the rain arrives as heavy drips 20 s after it starts. **The Alder spates** (2.8). Mossmauls and Rootcoils hunt by day in rain |
| Morning mist | 15 | Only 05:00 to 10:00. Sight 70 m on the floor; the canopy sticks out above it. Thornlynx drop from 1.5× range. The **plenty** block after Rain lands in mist often: +50% heartwood resin yield (world-map plenty rule, signature resource) |

### 2.10 Event: the Bloom (GAP-COOKED; frame KEPT from fire-frost 3.1 and world-map 2.7)

**Folklore and fact.** Mast years are real: oaks and beeches flower and seed all at once across a whole forest every few years, and the animals boom with them. In the story, the forest's one body flowers.

| Part | Detail |
|---|---|
| Frequency | One game day, about once every 3 real days (the server's Biome events setting, KEPT) |
| Warning (5 min) | Every giant in F4 and F5 drops a gold-green pollen haze from its crown (pale yellow #E8E0A0); every Glimmerfinch in the biome takes off at once and rings the Four Hearts; the Four Hearts' sap veins brighten; the **Grovekin pillar on the Crown's Ring burns green** (world-map 2.12 rule) |
| Growth | Every crop and wild plant in the forest grows at ×2 for the day (FULL-GAME draft kept). Wild nodes respawn at ×2. Each wild plant node has a 10% chance to drop a **rare seed** of any biome's rare crop (survival-crafting list) |
| **Elder Falls** | Three times in the day, a dead giant falls in a random F4 or F5 spot (never within 150 m of a camp, claim, shrine or arena). 5 s warning: loud cracks from 1 km, birds lift. The fall is a 90 m line of crushing damage (lethal) along a visible lean. Each leaves a fresh fallen giant with 12 heartwood nodes and 6 resin nodes for the day |
| Creatures | Hivebacks swarm the meadows (honey ×2). **Bramblewasp raids:** at noon, two Bramblewasp swarms of 12 fly from the Paper Hollow to the nearest beehive trees and player beehives within 1.5 km; the Hivebacks heat-ball them (a living fight you can join; player beehives under attack lose honey if the wasps win). Trufflers dig up rare seeds |
| **The Wild Hunt** (at night) | The Hornwarden leaves his normal route and leads a herd of 10 Elderelk at full gallop on a loop through F1 and F2 (Antler Barrow → Hanging Oak → Herne's Ford → back), with a sound you hear from 800 m (hooves, antler rattle, a bellow). Anything in its path is charged: players, creatures, tames. Living beasts in a frenzy of the mast, never ghosts. Their eyes catch the foxfire. Kill the Hornwarden during the Hunt for a second trophy roll and the "Huntsman's crown" (2.19). The Hunt avoids camps and the village (they are safe zones) |
| Sylvaroth | The heart-trees' lines heal ×1.5 during the Bloom. Fighting it in the Bloom is harder; a note on the run-back stone says nothing, but the sap veins blaze |
| Pays | Heartwood ×3 everywhere (3.3 SPEC), crops ×2, honey ×2, rare seeds |
| Never | Spawn camps and the village stay safe |

### 2.11 Shrines and the path master

**Shrines (rules KEPT from fire-frost 3.1: 5 per biome, guarded, 3 s light, path XP, fast travel; FULL-GAME "shrine in a grove circle"):**

| Shrine | At | Guard (sub-zone pack) | Look |
|---|---|---|---|
| **Great Shrine: the Grove Circle** | F3, +600, +3,900 (inside the Long-hall) | none (the path master works here by day) | A ring of nine pleached lime trees inside the Long-hall; in the middle, a stone bowl on a living root pedestal. The brazier burns heartwood resin with an amber smoke |
| Hedge Shrine | F1, +1,950, +3,800 | Truffler pair (lowland) | A stone niche in an ancient hedgerow with a carved hulder face |
| Falls Shrine | F2, +330, +2,720 | Rootcoil (midland young) | On the Grovekin fishing platform under the Great Falls' spray |
| Nurse Shrine | F4, +20, +3,240 | Thornlynx pair | A brazier in the hollow of the Nurse Log's broken end |
| Canopy Shrine | F5, −620, +3,050 | Thornlynx and Bramblewasps | A brazier on a walkway platform 30 m up, reached by the Canopy Lift |

**Path master: Rowan Ashby, the hermit (GAP-COOKED; DD "a path master NPC per biome who teaches the path through quests"; FULL-GAME "a herbalist hermit").**

| Part | Detail |
|---|---|
| Who | A newcomer, not a Grovekin: he walked off his ship ten years before the player and into the trees, and learned the Grovekin's craft from their gardens, carvings and graves (world-map "the newcomers" rule). Mid 30s, tall, lean-muscled from climbing; a close dark beard, long dark-brown hair tied back with a leather cord, a strong jaw, sun lines at his eyes. He wears an open-collared linen shirt with sleeves rolled to the elbow, a leather herbalist's apron with pouches and a pruning knife, fitted breeches and soft boots. His own Nature body change shows at 75 (2.18): bark at the knuckles and elbows, leaf-green eyes. Calm, dry humour, physically at ease |
| Where | Lives in the Hermit's Oak. Works at the Grove Circle by day (tending the limes, grinding resin); at home at night (window lit) |
| How he talks | Short spoken lines within 6 m, no camera grab (KEPT rule). Quests go to the Journal |
| Teaches | Anyone. His last quest needs the third mini (the Grey Matron) |

**His 6 quests (GAP-COOKED; schema in `quests.json` as fire-frost):**

| # | Quest | Task | Reward |
|---|---|---|---|
| 1 | **Fallen, not felled** | Bring 10 heartwood from a fallen giant | 1,000 Nature XP; briarproof leathers recipe |
| 2 | **Through the briars** | Cross a thornbrake wall 8 m deep (any way: path, leathers, fire) and open the cache behind it | 1,500 Nature XP; briar balm recipe |
| 3 | **Smoke and honey** | Calm a wild hive with smoke and take honey without a sting | 2,000 Nature XP; beehive piece recipe (KEPT) |
| 4 | **The bent trees** | Follow the trail trees on the Lost Path to its end (F-S1) | 2,500 Nature XP; Foxfire lantern recipe |
| 5 | **The black laces** | Bring 6 bootlace rhizomorph from the Hollow Root | 3,000 Nature XP; Bootlace blight recipe |
| 6 | **The Matron's skins** | Beat the Grey Matron by cutting her walkways | 5,000 Nature XP; "Hermit's ring" (ring slot: Growth regen +15%, Rooted heal starts 0.5 s sooner) |

### 2.12 Creatures (names, roles and taming KEPT from FULL-GAME's list; everything else GAP-COOKED)

**Rules (CR SPEC):**
- Every forest creature ignores thornbrakes and leaf pits.
- Neutral herds defend each other.
- Each creature tames only by its listed methods.
- Tiers follow 3.3: hard aggressive = highland, medium aggressive = midland, passives = lowland.
- Every attack shows the real jaw, claw, antler, stinger or coil, and damage lands only on visible contact.
- No attacker limit (combat 2.18.4).
- `physMaterial` for blood and spark FX (combat 2.14).

#### 2.12.1 The eleven at a glance

| Creature | Tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Mossmaul** | Highland, 1 (a sow with 2 cubs at dens) | Short-faced bear (*Arctodus*) frame: long legs, short broad muzzle, huge shoulder hump. **Giant ground sloth forearm claws** (*Megatherium*): three hooked claws 35 cm long that make it walk on its knuckles. A living mat of moss, ferns and shelf fungus on the hump and spine | A long-legged bear with a mossy hill of a hump and hooked claws that curl under like scythes; reared, 5 m tall | Fur umber-black #2E241C, moss mat #4E6B2A, shelf fungus cream #D8C8A0, claws horn-black, small dark eyes with green eyeshine | 2.2 m at the hump, 5 m reared | Knockout and feed, or nature path magic. Hard | Raw meat ×8, thick hide ×4, Mossmaul claw ×2, fat ×3 |
| **Thornlynx** | Midland, 1 to 2 | Clouded leopard (the longest canines for its size of any cat; climbs down trunks head-first and hangs by its hind feet) at 2.4 m long. Lynx ear tufts become long dark spikes; a short heavy tail | A long low cat with sabre-like canines and two black spikes on the ears, seen hanging head-down on a trunk | Coat grey-gold with large dark cloud blotches #6A6048 / #2A2620, belly cream, ear tufts black, eyeshine green | 2.4 m long | Knockout and feed. Medium | Raw meat ×3, spotted pelt ×2, fang ×2 |
| **Rootcoil** | Highland, 1 | Reticulated python length and green anaconda bulk at 12 m, with heavy keeled scales textured like bark; heat pits along the lips | Lying still it's a fallen log among roots; reared, a thick S-curve with a wedge head | Bark brown #4A3A2A with moss-green saddles #4E6B2A, lichen-grey speckle, belly cream, eyes amber | 12 m long, body 50 cm thick | Knockout and feed. Hard | Raw meat ×10, Rootcoil leather ×4, shed skin (when found) |
| **Bramblewasp** | Midland swarm, 6 to 12 from a nest (25 HP each, swarm archetype) | Japanese giant hornet at fist size: big orange head with heavy mandibles, black-and-amber banded abdomen, a stinger as long as a finger | A buzzing cloud of fist-sized hornets with orange heads | Head orange #D07A1A, thorax black-brown, abdomen bands amber #C9A030 and black, wings smoky | 10 cm | Steal an egg (larva from a nest). Medium | Wasp sting ×1 (25%), paper ×1 |
| **Elderelk** | Midland neutral, herd 4 to 8 | Irish elk (*Megaloceros*) build: 2.1 m at the shoulder, palmate antlers 3.6 m across. Ivy and moss grow on the antlers | A tall deer with a huge flat antler crown hung with green | Coat chestnut #7A4A2A with a pale rump, a dark mane, antlers bone with ivy #3E6A2A | 2.1 m at the shoulder | Knockout and feed. Medium | Raw meat ×6, hide ×3, antler (rare) |
| **Burlbeast** | Midland neutral, herd 2 to 4 | Gaur/aurochs: a massive humped wild ox, 2.0 m at the shoulder. Its back and shoulders carry thick ridged keratin plates with the colour and texture of bark burls | A huge dark ox whose back looks like a tree stump | Hide black-brown #2A2018, bark plates grey-brown #6A5A48, white stockings, horns pale with dark tips | 2.0 m at the shoulder | Offer heartwood shavings. Medium | Raw meat ×10, thick hide ×5, fat ×4 |
| **Truffler** | Lowland neutral, sounder 2 to 4 | Eurasian wild boar at 1.2 m, a long flexible snout, short tusks, a bristle crest | A heavy-fronted boar with a long rooting snout | Grizzled grey-brown #5A4E40, crest black | 1.2 m long | Offer food. Easy | Raw meat ×4, hide ×1, fat ×1 |
| **Hiveback** | Midland neutral, 3 to 6 at a hive | A bumblebee at 1.0 m: dense fur, round body, short blurring wings, pollen baskets on the legs | A flying fuzzy barrel humming at knee-to-head height | Fur black and amber bands #D0A040, white tail tip, wings smoky | 1.0 m | Steal an egg. Medium | Wax ×1 (honey comes from the hive) |
| **Fernsheep** | Lowland passive, flock 4 to 10 | A primitive heath sheep (Soay-like) whose fleece grows in curls like fern fiddleheads, moss-green when wet | A small dark sheep with curling green-grey fleece and swept horns | Fleece grey-green #7A8A6A, face dark, horns amber-horn | 0.7 m | Offer food. Easy | Wool (KEPT), raw meat ×2 |
| **Dappledoe** | Lowland passive, group 3 to 6 | Fallow deer: spotted coat, palmate antlers on bucks | A spotted deer with a white rump and a tail always flicking | Coat chestnut #9A5A30 with white spots, black-edged rump | 0.9 m at the shoulder | Offer food. Easy | Raw meat ×3, hide ×2, shed antler (KEPT) |
| **Glimmerfinch** | Lowland passive, flock 6 to 20 | A goldfinch body with a hummingbird's hovering flight and iridescent throat | A tiny shimmering bird hovering at flowers | Green-gold iridescent back #6AA040, violet throat #7A40A0, red face patch | 12 cm | Offer seeds. Easy | Feathers (decor) |

**Path magic (Mossmaul, nature path; GAP-COOKED in Raven's "calm a creature of the same element" frame).** On a Mossmaul that's knocked out, or below 30% HP and Entangled, hold Interact for 4 s with 10+ Nature points: flowers and ferns grow from your hand into its moss mat. That counts as a full meal of narcotic and kibble, and taming continues as knockout-and-feed. It costs 40 Growth.

**Utilities and mounts:**
- **Elderelk (mount):** ride sprint 1,350 cm/s. **It slips between trees:** while ridden its collision is a thin capsule, so it never snags on trunks or brush (the forest's fastest mount). It leaps fallen giants up to 2 m and passes thornbrakes.
- **Burlbeast (hauler):** carries 300 (KEPT); +25% heartwood yield within 15 m; drags a fallen giant's log home as a 40-heartwood bundle once a game day.
- **Truffler:** finds truffles, buried caches, leaf pits (it snorts at them) and the Hollow Root dig; digs up forest seeds (KEPT).
- **Hiveback:** lives in a beehive piece (KEPT beekeeping). A following Hiveback **heat-balls** Bramblewasps: ×3 damage to wasps, and it grabs one at a time (2.12.2).
- **Glimmerfinch:** pollinator (KEPT: crops within 20 m grow in ×0.85 time). A tamed flock (one tame slot) lifts and circles over the nearest ripe wild plant or hive within 60 m when you whistle.
- **Fernsheep, Dappledoe:** farm animals (KEPT husbandry).

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55. Bramblewasps use the swarm row (25 HP).

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Mossmaul** (high) | Hook swipe | 0.45 (the arm rises, claws uncurl and spread) / 0 / 0.18 / 0.70 | 1.0 | 40 (4) | parryable; slash; bleed 20 | 60% → Hook swipe 2 |
| | Hook swipe 2 | 0.40 / 0.2 / 0.18 / 0.75 | 1.0 | 40 (4) | parryable; bleed 20 | 35% → Hammer down |
| | **Hammer down** (delayed) | 0.75 (rears onto its hind legs, both arms high, claws hooked) / 0.6 / 0.22 / 1.20 | 1.8 | 60 (6) | blockable, not parryable; impact ring 300 cm `low` (jump clears it) | The claws sink in: 1.2 s punish, chest lock point open |
| | **Maul hug** (grab) | 0.90 (stands tall, arms wide, a deep huff; crimson glint on the claws) / 0.3 / 0.25 catch | 0.5 catch + 18/s for 2.5 s, then thrown 5 m | 60 (6) | `grab`, `unblockable`; dodge only | An ally's 120 damage on the head frees you. Mash −0.15 s per press |
| | Bull rush | 0.85 (head drops, a ground scrape with both claws) / 0 / 1.0 run / 1.30 | 1.6 | 55 (5) | knockback 800; hitting a trunk stuns it 2.5 s | Range 6 to 25 m |
| | Roar | 0.60 / 0 / 1.0 / 0.80 | 0 | — | 12 m: players stagger 0.6 s unless guarding; drops widowmakers within 6 m (2.8) | Once per fight, opening |
| | readsAction | Hook swipe with no hold within 0.4 s of a drink or a charged heavy | | | | |
| **Thornlynx** (mid) | **Canopy drop** (opener) | 0.90 (a shadow grows on the ground, bark and leaves rain down, a rustle overhead; KEPT combat) / 0 / 0.25 / 1.10 | 1.6 | 45 (4) | blockable; from a trunk or branch up to 12 m above; bleed 30 | → Fang bite 50% if it hits |
| | Paw swipe ×2 | 0.40 / 0.15 / 0.15 + 0.15 / 0.70 | 0.8 each | 25 (2) | parryable | — |
| | **Fang bite** | 0.55 (jaw gapes to 90°, the sabre canines show) / 0.2 / 0.15 / 0.80 | 1.4 | 35 (3) | parryable; pierce; bleed 35 | — |
| | Pounce | 0.65 (crouches flat, hindquarters wiggle) / 0.3 / 0.25 / 0.90 | 1.3 | 35 (3) | 300 to 900 cm leap | — |
| | Tree escape | 0.30 / 0 / 1.0 climb / — | — | — | Below 40% HP it climbs the nearest trunk and waits 4 to 8 s to drop again | An arrow, a thrown stone or Vine lash brings it down (falls, 1.5 s grounded) |
| **Rootcoil** (high) | **Log strike** (opener) | 0.90 (the "log" breathes: a slow rise and fall, a hiss, the head lifts from the root tangle) / 0 / 0.20 / 1.10 | 1.5 | 50 (5) | parryable; pierce; from rest within 700 cm | → Constrict 50% if it hits |
| | Strike | 0.42 (the S-curve tightens) / 0.2 / 0.15 / 0.80 | 1.0 | 40 (4) | parryable; pierce | 40% → Strike |
| | **Constrict** (grab) | 1.00 (coils slide out on both sides of you in a ring; crimson glint on the coil scales) / 0.2 / 0.30 catch | 0.5 + 20/s for 3.0 s, stamina −30 | 60 (6) | `grab`, `unblockable`; dodge out of the ring or jump (leg i-frames) | An ally's 120 damage on the head frees you; fire on it frees you at once. Mash −0.15 s |
| | Tail whip | 0.55 / 0 / 0.20 / 0.80 | 1.1 | 40 (4) | `low`, 270° behind, r 600 | — |
| | Body slam | 0.80 (rears 3 m high, neck flares) / 0.3 / 0.25 / 1.20 | 1.6 | 55 (5) | blockable; r 250 at the landing point | Head lock point open 1.2 s |
| **Bramblewasp** (swarm) | Sting dive | 0.45 (hovers, abdomen curls forward, wing pitch rises) / 0 / 0.12 / 0.6 | 0.5 | 5 (0) | blockable; poison 15 (poison moves to R3, see 3.0) | 8 wasps sting about 4 times a second in total |
| | **Scout mark** | 0.90 (a scout hovers at your head and fans its abdomen; a pale dust puff) / 0 / — | 0 | — | Marks you 30 s: every Bramblewasp within 40 m targets you, and nests within 40 m swarm. Water (wade or swim) or smoke removes it | The real hornet marking pheromone |
| | Death | — | 0 | — | No burst (real hornets don't burst) | The swarm template's burst row is off for wasps |
| **Elderelk** (mid, neutral) | Antler charge | 0.90 (head low, two hoof scrapes) / 0.3 / 0.9 run / 1.30 | 1.6 | 50 (5) | knockback 800; a trunk hit stuns it 2.5 s | Herd charger |
| | Antler rake | 0.55 / 0 / 0.20 / 0.80 | 1.1 | 35 (3) | parryable | — |
| | Rear strike | 0.60 (rears, forelegs high) / 0.2 / 0.20 / 0.90 | 1.3 | 40 (4) | blockable; r 200 in front | — |
| | Bugle (rally) | 0.80 / 0 / — / 0.5 | — | — | The herd within 40 m turns hostile | Plays when one is hit |
| **Burlbeast** (mid, neutral, heavy) | Stomp | 0.80 (forelegs lift together) / 0 / 0.20 / 1.00 | 1.3 | 45 (4) | `low` ring r 350; hyperarmor | Herd heavy |
| | Horn sweep | 0.70 / 0.2 / 0.25 / 0.90 | 1.4 | 50 (5) | blockable; 160° front; hyperarmor | — |
| | Crash | 0.95 (head down, a bellow, a short run-up) / 0 / 0.7 run / 1.40 | 1.8 | 60 (6) | knockback 1,000; hyperarmor | — |
| **Truffler** (low, neutral) | Tusk gore | 0.50 / 0 / 0.15 / 0.80 | 1.0 | 20 (2) | parryable | — |
| | Snout charge | 0.70 (grunt rises, forefeet paw) / 0 / 0.6 run / 1.0 | 1.2 | 25 (3) | — | — |
| **Hiveback** (mid, neutral) | Sting | 0.60 (abdomen curls, the buzz drops in pitch) / 0 / 0.15 / 0.90 | 0.8 | 15 (1) | parryable; poison 25 | Only near its hive or when hit |
| | **Heat ball** (on wasps, and on players at its hive) | 0.70 (several bees converge on one target, the hum rises) / 0 / 3.0 cling | 6/s per clinging bee, max 4 | — | Each clinging bee −10% move. A roll knocks one off (40%). Fire on you knocks all off | On a Bramblewasp: grabs and kills it in 3 s |
| **Fernsheep, Dappledoe** (low, passive) | Butt / kick (cornered) | 0.50 / 0 / 0.15 / 1.0 | 0.4 | 10 (1) | — | Then flee |
| **Glimmerfinch** | none | — | — | — | Flees in a shimmering burst | Never attacks |

**Physical resist:** Mossmaul fur and moss slash 0.85 (its belly when reared ×1.3). Rootcoil scales slash 0.85, strike 1.1, and strike hits count ×1.5 toward freeing a constricted ally. Burlbeast bark plates strike 0.8 on the back only. Bramblewasps die to any 1 or 2 hits (25 HP); sweeping Wildbrands and novas answer the swarm. **Lock points:** Mossmaul head and chest; Rootcoil head and mid-body; Elderelk and Burlbeast head and chest.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table)

| Sub-zone | Day | Night |
|---|---|---|
| F1 Meadows | Fernsheep 14, Dappledoe 12, Glimmerfinch 12, Truffler 8, Hiveback 6, Elderelk 4 (the Hornwarden's herd passes) | Dappledoe 8 (bedded in rings: a sneak-tame window), Truffler 8, Thornlynx 3 (hedge edges only), Elderelk 4 |
| F2 Streams and Falls | Elderelk 10, Dappledoe 8, Truffler 6, Glimmerfinch 6, Rootcoil 3 (fords) | Rootcoil 8 (fords and banks), Thornlynx 6, Elderelk 6, Truffler 4 |
| F3 Grovekin Villages | Hiveback 10, Bramblewasp nests 8, Truffler 6, Glimmerfinch 6, Thornlynx 4 | Thornlynx 10, Truffler 6, Bramblewasp nests 4 (asleep; a hit wakes them), Mossmaul 3 |
| F4 Old-Growth | Burlbeast 8, Thornlynx 8, Mossmaul 5, Rootcoil 4, Bramblewasp nests 4 | Thornlynx 10, Mossmaul 8, Rootcoil 6 |
| F5 Elder Canopy | Thornlynx 12, Mossmaul 6, Rootcoil 4, Bramblewasp nests 3 | Thornlynx 14, Mossmaul 8, Rootcoil 6 |

**Living-world beats (built AI, WRWildlife):**
- Mossmauls kill Burlbeasts, cache the carcass under moss, and guard the cache (the Bear's Larder kit).
- Thornlynx hang on trunks over elk trails at dusk and drop on Dappledoe.
- Rootcoils lie across fords; a Truffler sounder walking over one loses a member.
- At midday, Bramblewasp scouts mark a beehive tree; an hour later a swarm raids it and the Hivebacks heat-ball them. You hear the fight before you see it.
- Glimmerfinch flocks lift over any fresh carcass or ripe wild patch: a vertical sign seen from 300 m (Ghost of Tsushima's birds).
- The Hornwarden's herd grazes the meadows by day and drinks at Herne's Ford at dusk.
- At Rootgrave's mouth at dusk, Thornlynx and Gravehounds fight (world-map KEPT; Gravehounds arrive in R4).

### 2.13 Mini-bosses (names and premises from the FULL-GAME draft kept; fights GAP-COOKED)

**Rules (KEPT, combat 2.17):**
- 3,000 solo HP, +80% per extra player or tame.
- Stance 90; 2 parries to break.
- 4 to 5 attacks plus one counter mechanic.
- Chase 65 m; respawn 4 h.
- Drops: upgrade shards ×4 and a trophy (BO).
- Trophies become talismans (survival-crafting KEPT): Gnarl heart (woodcutting yield +15%), Queen's sting (poison buildup on your hits +10%), Matron claw (backstab damage +10%), Warden antler (your mount +10% speed).
- Every first kill drops a visible, named item that flies to the killer within 6 s (KEPT).

#### 2.13.1 Old Gnarl (an awakened oak; folklore: the Elder Mother)

| Part | Detail |
|---|---|
| Body | An oak 18 m tall that walks on a mass of roots like a crouched giant. Its trunk is split down the front into a deep, dark mouth-hollow; two knotholes high on the bole glow with amber sap like eyes. Two main limbs are its arms, each ending in a fist of knotted branches. Bark plates 30 cm thick, deeply furrowed, cover its front, back and both arms. Axes from the clear-cut are still sunk in its legs |
| Silhouette | A hunched, broad oak on root legs with two long arms, its crown thrashing as it moves |
| Arena | **The Clear-cut** (+600, +3,300): 250 m across, hundreds of stumps (cover from the boulder throw, and trip-free: the stumps are 0.5 to 1 m high and you can step up on them). The Woodcutters' Camp on the east edge. Run-back stone 50 m east |
| Voice | Timber creak and groan (real tree stress cracking), a deep elephant rumble pitched down, wood splitting for its roar, a wind rush through its crown |
| Lock points | Head (the knothole eyes), chest (the mouth-hollow) |
| Resist | Slash 0.9, strike 1.0, **axes ×1.5 against bark**, fire 1.0 on bark, **fire ×2 and burn buildup ×2 on stripped wood** |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Branch sweep | 0.70 (an arm swings back, its crown leans) / 0.3 / 0.25 / 0.90 | 40 | Blockable; 180° front, r 700 |
| Root slam | 0.80 (it lifts its whole root mass, soil pouring off) / 0 / 0.20 / 1.10 | 45 | `low` ring r 600 (jump clears it) |
| **Boulder throw** | 1.20 (it tears a stump and its root-ball out of the ground) / 0 / release / 1.0 | 50 + knockdown | Projectile 1,400 cm/s on a gravity arc, a shadow grows where it'll land, never homes, never thrown inside 8 m. Stumps between you and it block it |
| **Grasp** (grab) | 1.00 (a fist opens into splayed branches, crimson glint on the twig-tips) / 0.2 / 0.25 catch | 15/s for 2.5 s, then thrown 8 m | `grab`, `unblockable`; dodge only. An ally's 100 damage on that arm frees you |
| Regrow bark | 1.5 (it stands still; sap bubbles out of a stripped spot and bark crawls over it) | — | Regrows one stripped plate every 40 s unless that spot is **burning**. A hit with fire during the 1.5 s cancels it and it staggers 2 s |

**Counter mechanic: strip and burn.** Four bark plates (front, back, left arm, right arm) have 400 part HP each. Axes strip them fastest. A stripped spot shows pale wood: fire there deals ×2 and sets it burning (burn ×2 buildup), and a burning spot can't regrow. Burning on two spots at once makes it **stagger 3 s** every 10 s while both burn. Fire players burn it fast; others carry a torch (a torch swing on stripped wood counts as fire) or fire arrows.

#### 2.13.2 The Bramblequeen (a hornet queen; real biology: the giant hornet colony)

| Part | Detail |
|---|---|
| Body | A Bramblewasp queen 4 m long: a huge orange head with mandibles as long as a forearm, a black thorax, an amber-and-black banded abdomen as thick as a barrel, wings 6 m across that roar. A stinger 60 cm long, glistening |
| Silhouette | A giant hornet; the abdomen curls under before every sting |
| Arena | **The Paper Hollow** (+900, +4,200): a dead oak wrapped in a 20 m paper hive. The fight runs inside the hive's three paper floors (open galleries joined by ramps of chewed paper) and its outside shell. Four **brood combs** hang inside, one on each floor plus one in the crown |
| Voice | Giant hornet wingbeat (a deep throb), cicada screech, mandible clicks amplified, a buzz that drops in pitch before a sting |
| Lock points | Head, thorax |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Mandible shear ×2 | 0.55 (mandibles spread wide) / 0.2 / 0.15 + 0.15 / 0.80 | 30 / 30 | Parryable |
| **Sting lance** | 0.75 (abdomen curls forward under her, a white glint at the tip, buzz pitch drops) / 0.4 / 0.20 / 1.10 | 50 + poison 60 | Blockable; a forward lunge 6 m. The stinger sticks in paper or wood on a miss: 1.1 s punish |
| **Pheromone mark** | 0.90 (she hovers and fans her abdomen; a pale dust cloud) / 0 / cone 6 m × 60° | 0 | Marks you 30 s: all her wasps target you (as 2.12.2). Water or smoke removes it. The hive has one water butt per floor (Grovekin rain barrels) |
| Wing gust | 0.60 (wings stop, then a deep beat) / 0 / 0.30 / 0.70 | 10 | Push 400 cm; can push you off a gallery edge (a 5 to 8 m fall) |
| **Call the swarm** | 1.00 (she bows over a comb, a rising buzz) / — | — | 6 Bramblewasps from that comb. Only while that comb lives; every 25 s |

**Counter mechanic: the combs.** Each comb has 300 HP. With all combs broken she can't call swarms, and she loses 20% move speed (she's guarding nothing). **Smoke:** a smoke pot (2.19) thrown into the hive makes all wasps passive for 10 s and slows her by 30% for 5 s. **Hivebacks:** a tamed Hiveback heat-balls wasps (×3) and a ball of 3 Hivebacks on her holds her for 2 s (she's too big to kill). Poison resist: she takes poison ×0.5.

#### 2.13.3 The Grey Matron (a Thornlynx matriarch; folklore: Black Annis' Bower)

| Part | Detail |
|---|---|
| Body | A Thornlynx 4 m long, her coat gone grey-blue with age, her face crossed with old scars, one ear tuft torn off. Sabre canines yellowed. She moves with a heavy grace and limps on the ground (an old wound in her right hind leg) |
| Silhouette | A huge grey cat hanging head-down from a trunk, tail slowly swinging |
| Arena | **The Bower** (−550, +4,250): a ring of six hollow oaks 70 m tall, 30 m apart, linked by six Grovekin walkways 15 to 25 m up. Deer and elk hides hang from the branches like flags (Black Annis hung her victims' skins in her oak). The floor is open moss with a few low ferns |
| Voice | Clouded leopard snarl and "chuff", cougar scream, a snow leopard's deep yowl, bark scraped by claws |
| Lock points | Head, chest |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Canopy drop** | 0.95 (a shadow grows, bark and leaves rain, a rustle) / 0 / 0.25 / 1.20 | 55 + bleed 40 | Blockable; from a walkway or branch |
| **Bridge leap** | 0.80 (she crouches on a walkway, looking at you) / 0.3 / 0.30 / 1.0 | 45 | She leaps across the arena from tree to tree and rakes you on the way if you're under her line (the line shows as her shadow sweeping) |
| Fang bite | 0.55 / 0.2 / 0.15 / 0.80 | 50 + bleed 40 | Parryable; on the ground only |
| Paw swipe ×3 | 0.45 / 0.15 / 0.15 ×3 / 0.90 | 30 each | Parryable; on the ground only |
| **Kit scream** | 0.90 (she hangs from a trunk and screams) / — | — | 2 Thornlynx come from the ferns; once at 60% and once at 30%. The kits go for the furthest tame first (tame-punish) |

**Counter mechanic: cut her walkways.** Each walkway has two rope anchors (100 HP each) at its trees; cut either anchor and it falls. While she can reach you from the canopy she never comes down. **When 4 of 6 walkways are down, or the tree she's in has no walkway left, she drops to the ground for good:** ground move ×0.6 (the old leg), her canopy moves are gone, and every paw combo leaves a 1.2 s punish. Arrows into a walkway she's on make her jump to another one (she hates the noise). The Grovekin walkways regrow (they're living) 4 game hours after she dies.

#### 2.13.4 The Hornwarden (roaming; folklore: Herne the Hunter and the Wild Hunt)

| Part | Detail |
|---|---|
| Body | An Elderelk bull 4.5 m at the shoulder, antlers 8 m across hung with ivy, moss and the rags of old Grovekin hunting banners. A thick dark mane, a scarred muzzle, eyes that catch the light amber. Old arrows in his flank, one Grovekin heartwood spear-head under the skin of his neck (a lump) |
| Silhouette | A giant elk with an antler crown wider than a house, at the head of a herd |
| Route | A 3.2 km loop through F1 and F2: Antler Barrow (rests at noon) → Hanging Oak (dusk) → Herne's Ford (drinks at nightfall) → the meadow coast → back. He walks at 250 cm/s with a herd of 6 Elderelk. Roaming mini rules (fire-frost KEPT): fights wherever you meet him; chase 65 m from where it started |
| Voice | Red deer stag roar, an elk (wapiti) bugle, a moose grunt, antler clash and rattle |
| Lock points | Head, chest; the antlers are a breakable part (stance 60, 1,200 part HP: break one side and his antler toss is gone) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Antler charge** | 0.90 (head low, three hoof scrapes, a snort) / 0.5 / 1.2 run / 1.40 | 60 + knockback 1,000 | Blockable. A trunk, stone or wall hit stuns him 2.5 s |
| Antler sweep | 0.70 / 0.2 / 0.25 / 0.90 | 45 | Blockable; 200° front, r 600 |
| Rear stomp | 0.80 (rears, forelegs high) / 0.3 / 0.20 / 1.10 | 50 | `low` ring r 400 |
| **Antler toss** (grab) | 1.00 (lowers his crown to the ground and sweeps toward you; crimson glint on the tines) / 0 / 0.25 catch | 40 + thrown 10 m | `grab`, `unblockable`; dodge only |
| **Bellow** | 1.00 (head back, mane up, a roaring bugle) / — | — | His herd charges the target together (6 Elderelk, herd charger rows) |

**Counter mechanic: the herd and the snares.**
- **Never hurt his herd.** If any herd member takes damage from you or your tames, he enrages for 60 s: damage ×1.3, speed ×1.2, punish windows −30%, and the Bellow comes every 12 s. Fight him away from the herd: a Bellow call pulls them in, so kill time is the trade.
- **Root snares hold him.** Root snare (spell), Strangling vines, bolas (KEPT ranged weapon) and **snare stakes** (2.19) hold him 3 s (bosses' slow ×0.7 elsewhere; the Hornwarden is the one mini roots hold properly) and deal stance ×2 while held. Three holds within 20 s = stance break.
- **The Wild Hunt** (the Bloom's night, 2.10): his herd is 10 and he never stops running; snares are the only way to pin him.

### 2.14 Major boss: Sylvaroth, the World-Root (GAP-COOKED; FULL-GAME premise kept: a walking tree-titan in a clearing ringed by four heart-trees that heal it; phase 1 root eruptions, branch sweeps, seed bombs; phase 2 pulls itself out and walks, vines snare; hidden phase 3 every heart-tree wakes and fights; weak to Necromancy; Verdant core)

**Look (built to be terrifying and believable).**

| Part | Detail |
|---|---|
| Anatomy | A **strangler-fig titan.** Its body is a lattice of grey-brown roots fused into a hollow cage, grown round a host tree that rotted out long ago (real strangler-fig life cycle). Through the gaps in the lattice you see **foxfire** glowing green inside, and **Grovekin burials** (bark-wrapped bodies in root cradles) held in the lattice like seeds in a pod |
| Head | A heavy burl the size of a cottage, split down the middle into a jaw of splintered heartwood fangs. Two hollows above it glow with amber sap (the heartwood core) like eyes |
| Arms | Two lattice arms 18 m long, knuckle-walking, each ending in a mass of root-claws that splay and grip the ground |
| Back | A hump of living canopy: moss, ferns, saplings, a few dead branches. It looks like a hill with trees on it |
| Legs (phase 2) | In phase 1 its lower body is still in the ground. When it tears free, the root mass braids into two thick hind legs, so it moves like a gorilla on its knuckles and rears to stand |
| Scale | Phase 1: the torso rises 22 m out of the ground. Phase 2: 34 m reared, 20 m on all fours |
| Palette | Lattice grey-brown #5A4A3A, moss #4E6B2A, amber sap #D08A1E (glows), foxfire #7FE07A (pale green, never the UI gold), dead bark black #1E1A16 |
| Silhouette | A hunched mountain of roots with a crown of trees on its back and two long arms; unmistakable from 1 km |
| Voice | Elephant infrasound rumble, real timber stress cracks and groans, a humpback's low moan pitched down, a red stag's roar layered into its roar, wood splitting on every slam. Its breathing is wind through the lattice |

**The arena: the Clearing of Four Hearts** (−500, +3,600).
- A round clearing 140 m across under the Spine's cliffs, mossed, with low ferns.
- **The four heart-trees** stand at north, east, south and west, 55 m from the centre, 140 m tall, amber sap in their bark.
- **The root lines.** Each heart-tree's root runs across the floor to the centre as a raised root as thick as a person, with amber-green light pulsing along it toward Sylvaroth. Where each crosses the 25 m ring, a **Root Knot** bulges up (a lock point you can hit, 900 part HP).
- **Cover:** four Grovekin standing stones, 4 m tall, on the 40 m ring between the lines (cover from seed bombs).
- **The Nature altar:** a Grovekin stone ring against the west cliff, behind the west heart-tree (Raven: the altar is in the arena).
- **Run-back stone:** 50 m east of the Fern Stair (KEPT rule 40 to 60 m).
- **The Heart Root** (Hollow Root shortcut) opens under the west heart-tree.

**The healing (FULL-GAME "four heart-trees that heal it", made visible and cuttable):**
- Each **live line** heals Sylvaroth 0.15% of max HP a second while its root mass touches the ground (all of phase 1; in phase 2 only during Re-root). Four lines: 0.6% a second.
- **Sever a line** by breaking its Root Knot (900 part HP). Axes deal ×1.3 to knots, fire ×1.5. The line goes dark and regrows after **45 s** (a 3 s visible crawl of light back up the root).
- **Bootlace blight** (2.19; the Armillaria tree-killer): a jar thrown on a knot kills that line for **90 s** with no damage needed.
- **Decay** (Necromancy status, R4): while Sylvaroth is decayed, no line heals it (its opposite element; FULL-GAME "necrotic damage stops its roots healing").
- **Weakness: Necromancy 1.5×** (SPEC weakness rule). Fire is neutral on its body (its living sap is wet) but ×1.5 on knots and on vine sectors.
- During the Bloom, lines heal ×1.5 (2.10).

**Numbers (KEPT rules):** 12,000 solo HP, +80% per extra player or tame. **Stance 180** (the heaviest bodies). 3 parries to break. Phase 2 at 50% by a telegraphed unblockable transition. Hidden phase 3 at 25% with artifacts (damage ×1.4). Chase 90 m.

**Lock points:** head; chest (the foxfire core: crits from a stance break land here); each arm (an arm has its own stance 70: break it and that side collapses for 3 s, a crit window); the four Root Knots (phase 1).

**Phase 1: the rooted heart (6 moves including a 3-hit combo and a grab; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell (small cue, combat 2.17.1b) |
|---|---|---|---|---|
| **Branch sweep ×3** (combo) | Low sweep 0.80 (`low`) / backhand 0.55 / overhead slam 0.90 + H 0.4; P 1.3 | 45 / 45 / 70 | First jump, second roll, third sidestep; reach 20 m | The arm draws back across the body, the crown on its back leans the other way |
| **Root eruption** | 1.00 (the ground bulges in a line toward you, moss tearing, a crackle running along it) / 0.2 / 0.40 / — | 45 + bleed 25 | A line 25 m long, 3 m wide, along the ground; not a projectile; sidestep | The bulging line itself |
| **Seed bombs** | 1.00 (the back canopy shakes, six pods swell orange-brown) / 0 / launch | 35 + Entangled 0.8 s each | Six pods lobbed at players' positions at launch time; gravity arc, about 1.2 s in the air (≥ 0.9 s KEPT), never home; a shadow grows at each landing spot; burst r 300; never inside 4 m | The pods swelling and the shadows |
| **Root hand** (grab) | 1.00 (one arm sinks into the earth; the ground under the target trembles in a 3 m ring with a crimson rim) / 0 / 0.30 catch | Lifted, 20/s for 3 s, then slammed for 40 | `grab`, `unblockable`; dodge out of the ring | Allies break the root wrist (600 part HP) to free you |
| **Canopy shake** | 1.20 (it rears its torso and shakes its back like a wet dog, a roar) / — / 4.0 | 40 per branch | 8 dead branches fall across the arena in 4 s, each with a shadow 1.0 s before impact; none land within 8 m of a heart-tree (the trees shelter you) | The rear-and-shake |
| **Thorn whip** (tame-punish) | 0.90 (a vine uncoils from the lattice toward the furthest tame) / 0 / 0.25 | Pull 15 m + 120 to the tame | Always targets the furthest tame first (SPEC "punish or scatter tames") | The uncoiling vine |

**Transition at 50%: the Uprooting (unblockable, KEPT template).** 1.5 s windup: the whole clearing heaves, a crimson-rimmed ring 20 m round its body cracks open, roots rip up out of the ground, and it tears its lower body free. Everything inside 20 m takes 60 and is knocked down. Outside the ring is safe. All four lines go dark for 10 s while it stands (a free knot window). The music changes on the bar (2.16).

**Phase 2: the walking root (2 new delayed attacks, 1 new combo, 1 arena change; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell |
|---|---|---|---|---|
| **Knuckle charge** (delayed) | 0.90 (it rocks back on its knuckles and roars) / **0.5** / 1.2 run / 1.50 | 70 + knockback 1,200 | A 40 m charge across the arena; it never hits its own heart-trees: it stops short 6 m before one, which gives a 1.5 s punish at the stop | The rock-back and the roar |
| **Hammer fists** (delayed) | 1.10 (both arms rise high, clasped) / **0.6** / 0.25 / 1.40 | 80 | `low` shockwave ring r 800 after the hit; blockable, not parryable | Arms high, the crown tilts forward |
| **Thrash ×4** (new combo) | Claw L 0.50 / claw R 0.45 / spin 0.70 / stomp 0.80; P 1.2 | 40 / 40 / 50 / 55 | Claws parryable (the 3 parries to break come from here and the backhand); spin 360° r 12 m; stomp `low` r 500 | Shoulders hunch, head drops |
| **Vine snares** (arena change) | Every 30 s | — | The floor splits into 6 sectors; 3 sectors' vines creep for 1.5 s (visible vines curling toward your feet), then go live for 12 s. Standing in a live sector 1.0 s = **Entangled 1.0 s**. Fire burns a sector clean for 10 s | The creeping vines |
| **Re-root** | 1.00 (it kneels and drives both arms into the ground) / — / 6.0 | — | For 6 s every live line heals it; it can't attack. A huge punish window, but cut lines first or it out-heals you | The kneel |
| Branch sweep ×3, Root eruption, Seed bombs, Root hand, Thorn whip | As phase 1, timings ×0.9 (phase 2 changes timing, KEPT) | ×1.1 | | |

**Hidden phase 3 at 25% (artifacts only, KEPT flag): every heart-tree wakes.** Each heart-tree becomes a rooted fighter: it swings its lower limbs at anyone within 15 m (W 0.9, `low` sweep, 40 damage, every 6 s). Its line can only be cut by killing its **amber heart** (a glowing knot in its trunk at 3 m, 1,500 part HP). The canopy shake comes every 20 s. Sylvaroth's damage ×1.4.

**Drops.** Boss core (Hearth), Nature path material **Verdant essence** ×6, a chance at the boss part **Verdant core** (KEPT: healing potions also heal your tames nearby), a trophy head (survival-crafting KEPT rule), and 2 blueprints: **World-Root Maul** (a heartwood great hammer bound in living root; moveset to the boss-weapons cook) and **Grovekin decor pack** (KEPT rule). After the kill, the Four Hearts' bark weeps resin for a game day (resin ×20 nodes). The Nature altar lights up (subclass unlock, BUILT `WRAltar`).

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT from fire-frost 3.5)

Every species gets tell, strike, hit, pain, idle, aggro and death sounds. Each is built from at least 3 real animal calls plus one material layer. Tells rise in pitch and sit 3 dB over the bed.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Mossmaul | Grizzly roar and huff, a walrus bellow pitched down, the jaw-pop of a black bear | Moss and fern rustle, claw on wood | The huff-huff-pop before a charge; a roar that shakes needles off the branches |
| Thornlynx | Clouded leopard snarl and chuff, Eurasian lynx yowl, a cougar scream | Bark scrape, leaf shower | Silence, then a rustle overhead: the drop has no voice until it lands |
| Rootcoil | Python hiss, a king cobra "growl" hiss, a soft bull-snake bellow | Bark-on-bark scale slide, creaking like a branch | The slow breathing of a log (a long, low exhale every 3 s) |
| Bramblewasp | Asian giant hornet wingbeat, yellowjacket swarm, mandible clicks amplified | Paper rustle | The rise from a single buzz to a swarm roar when a nest is hit |
| Elderelk | Wapiti bugle, red stag roar, Irish elk scale by pitching a moose grunt down | Antler rattle, ivy swish | The bugle that turns a herd |
| Burlbeast | Gaur snort and bellow, American bison grunt, yak grunt | Wood-like knock of its back plates | A deep rolling bellow before the crash |
| Truffler | Wild boar grunt and squeal, peccary bark, pig snuffle | Soil rooting | Snuffle, snuffle, a sharp snort (a find or a leaf pit) |
| Hiveback | Bumblebee drone pitched down, honeybee piping, a carpenter bee buzz | Soft fur brush | The hum rising to a heat-ball thrum |
| Fernsheep | Soay sheep bleat, goat bleat, ram grunt | Fleece rustle | Flock bleats running in a wave |
| Dappledoe | Fallow deer groan, roe deer bark, fawn bleat | Hoof on leaf litter | The alarm bark and the white-rump flash |
| Glimmerfinch | Goldfinch twitter, hummingbird chirp and wing hum | Tiny wing whirr | A chorus over a find |
| Old Gnarl | Timber stress cracks and groans, elephant rumble pitched down, wind through a crown | Wood splitting, roots tearing from soil | The crack of a stump torn up before a boulder throw |
| Bramblequeen | Giant hornet wingbeat slowed, cicada screech, mandible clicks | Paper roar of the hive | The buzz pitch dropping before the sting |
| Grey Matron | Clouded leopard chuff, cougar scream, snow leopard yowl | Claw on bark | A scream from somewhere above you |
| Hornwarden | Red stag roar, wapiti bugle, moose grunt | Antler clash, banner rags flapping | The bellow that sends the herd |
| Sylvaroth | Elephant infrasound, timber cracks, humpback moan pitched down, red stag roar | Wood splitting, roots tearing, wind through the lattice | A deep groan from under the clearing as you cross the run-back stone, before it rises |

### 2.16 Music: the Ancient forest (tempo, key, instruments SPEC 3.3: 72 BPM, G major, woodwinds, harp, warm strings, birdsong in the mix; cues GAP-COOKED; rules KEPT)

**Palette:**
- Wooden flute and low whistle for melody; oboe and cor anglais for the warm middle.
- Celtic harp, and a nyckelharpa (the Swedish keyed fiddle) for the old-people colour.
- A warm string section.
- **Real birdsong** (blackbird, song thrush, wren) recorded and pitched into G, woven into the explore cues by day; tawny owls at night.
- Bodhrán and low frame drum for combat.
- A low choir, wordless, for the boss and the Hollow Root.

**Always audible.** No planned silences in the forest. Ducks at most 4 dB. Mixed −18 LUFS, calm −23, boss −15.

| Cue | Length | Use | Feel |
|---|---|---|---|
| FOR_Explore_A / B / C | 3 to 4 min each | Day. A: F1, F2. B: F3, F4. C: F5 | A: flute melody over harp, bright, birdsong forward. B: nyckelharpa and cor anglais, slower harmony, the birds further off. C: low strings and choir hum, the harp in its low register, sparse birds (it's the deep wood) |
| FOR_Night_A / B | 3 min | Night | Harp and a low whistle over a string pad; owls; the melody of A played slower |
| FOR_Combat (stems for A, B, C) | loops | Combat layer on the bar | Bodhrán and frame drum, a string ostinato, the flute doubling at the octave |
| FOR_Mini | 3 min loop | All four minis | 4-bar intro per mini: low string col legno knocks (Gnarl); a buzzing sul ponticello cluster (Bramblequeen); a high harp glissando falling (Grey Matron); a hunting-horn call on low brass (Hornwarden) |
| FOR_Boss_Sylvaroth P1 / P2 / P3 | sections | Phase changes on the bar | P1: low choir and strings, slow, huge, the harp as a heartbeat. P2: drums enter, the melody goes to brass, the choir sings in fifths. P3: everything, with the flute screaming high over it |
| FOR_HollowRoot | 4 min | The Hollow Root | Choir hum, a slow harp, a low pulse like the Thrumming Root's heartbeat |
| FOR_Bloom | layer | Event | A bright celesta and flute layer and fuller birdsong over the normal cue; at night, the Wild Hunt layer: galloping drums and a horn call |
| FOR_Camp / FOR_Village | 2 min each | Camps, the village, your Hearth in the forest | Harp and flute, warm and slow; the village adds a fiddle |

**Source:** first pass from CC0 and CC-BY libraries matched to tempo, key and instruments (OpenGameArt, Free Music Archive, Pixabay Music, incompetech); birdsong from Freesound CC0 and xeno-canto (CC-BY where allowed). The boss track is commissioned later. Credited in `Docs/CREDITS.md`. No AI generation (KEPT, fire-frost 3.6).

### 2.17 The Nature path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Growth (DD SPEC: "regenerates fast near plants, slow in barren areas"; built GAP-COOKED).**
- A **spender** (combat KEPT): abilities cost Growth. Max 100 (+10 per Growth capacity rank).
- **Regen by the ground around you**, sampled every 0.5 s within 8 m:
  - **Lush** (grass, moss, crops, forest floor, any of your own plants or tames' plant parts within 8 m): **6 a second**.
  - **Normal** (dirt, rock with some plants, wooden floors with potted plants): **3 a second**.
  - **Barren** (lava, ice, snow, sand, bare stone, void ground, caves with no plants, stone or metal floors): **1 a second**.
- Regen runs in and out of combat. Sap ranks multiply it.
- Leaving food on a Grovekin offering stone gives +50 Growth (once a game day per stone; 2.4 #21).

**Root passive: Rooted (SPEC text: "standing still slowly heals"; numbers GAP-COOKED).** Main path only.
- After **1.5 s** with no move input and no roll (attacking, casting and guarding in place are fine), you heal **1% of max HP a second**.
- Taking any damage resets the 1.5 s.
- On barren ground it heals half as much.
- See: thin root-hairs grow from your soles into the ground; a faint green tint along the bark patches (2.18). Hear: a soft creak of roots.
- It is never a safety net: standing still in a Souls fight is the risk you pay for it.

**Entangled (the Nature status, GAP-COOKED; used by Entangle chance, Root snare, seed bombs, vine snares).** Roots hold the feet: the target can't move, roll or jump, but can still turn, attack and cast. Lowland 1.5 s, midland 1.0 s, highland 0.6 s, players 0.6 s (PvP block). Mini and major bosses are never held; they get slow ×0.8 for 1.0 s instead (except the Hornwarden, 2.13.4). After an Entangle, the target is immune for 8 s.

**The tree.** Gates are points spent in the branch before the node, like `frost.json`.

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Thorn shot | 3 pts | 0 | Basic nature spell | Bolt 15 dmg, bleed 20, cd 1.0, 6 Growth | KEPT combat |
| Trunk | Growth capacity | 3 ranks | 3 | Larger Growth bar | +10 max Growth per rank | GAP-COOKED |
| Trunk | Entangle chance | 2 ranks | 6 | Higher chance to root enemies briefly | Each weapon hit and each Nature spell hit has 5% per rank to Entangle | GAP-COOKED |
| Trunk | Briar strike | 2 pts | 8 | Basic nature weapon art | Wildbrand: 15 Growth, cd 3.0, W/A/R 0.28/0.20/0.42, ×1.3, bleed 30, Entangle 0.6 s on lowland, stance ×1.5 | KEPT combat |
| Grove | Regrowth | 5 pts | 0 | Spell: heal over time | Ally target (self if no ally in aim): 30 HP over 10 s, cd 8, 15 Growth | KEPT combat (cost GAP-COOKED) |
| Grove | Lifebloom | 5 ranks | 5 | Stronger healing | +8% to all your Nature healing per rank | GAP-COOKED |
| Grove | Sanctuary | 4 pts | 10 | Spell: a zone that heals allies inside | Pool r 450, heals 4/s for 10 s, cd 30, 30 Growth | KEPT combat |
| Grove | Sap | 3 ranks | 14 | Growth regenerates faster | +15% Growth regen per rank | GAP-COOKED |
| Grove | Purify | 3 pts | 17 | Healing also removes poison and rot | Each heal tick from Regrowth, Sanctuary or World tree removes 20 poison and 20 rot buildup; the first tick ends an active poison or rot | SPEC text; numbers GAP-COOKED |
| Grove | World tree | 10 pts | 20 | Capstone: summon a tree that heals and shields allies | Summon: heals 6/s and gives −20% damage taken to allies within 600 cm, 15 s, cd 90, 50 Growth | KEPT combat ("shields 20%" read as damage taken −20%) |
| Thorn | Vine lash | 5 pts | 0 | Whip-like weapon art | Wildbrand: 20 Growth, cd 5, W/A/R 0.30/0.20/0.45, ×1.2, bleed 25, reach 700; follow-up within 0.6 s pulls the target 300 cm to you (not bosses) | KEPT combat |
| Thorn | Barbs | 5 ranks | 5 | More bleed damage | +10% bleed burst damage on bleeds caused by your Nature hits, per rank | GAP-COOKED |
| Thorn | Root snare | 4 pts | 10 | Spell: hold enemies in place | Pool r 250, Entangles 2.5 s (ignores the tier times; bosses slow ×0.7 for 3 s), cd 14, 20 Growth | KEPT combat |
| Thorn | Thorned skin | 3 pts | 14 | Attackers take damage | Every melee hit that lands on you returns 8 damage + bleed 10 to the attacker on the same contact frame (PvP ×0.6) | GAP-COOKED |
| Thorn | Briar spread | 3 ranks | 17 | Larger root areas | +15% radius per rank on Root snare, Strangling vines and Entangle-on-hit areas | GAP-COOKED |
| Thorn | Strangling vines | 10 pts | 20 | Capstone: vines hold and crush a large enemy | On a target: holds 4 s, 12 dps; highland creatures and minis are held; major bosses aren't held but take 12 dps, stance 10/s and slow ×0.6 for 4 s; cd 60, 60 Growth | KEPT combat (boss rule GAP-COOKED) |
| Bloom | Sprout turret | 5 pts | 0 | Plant a living turret that shoots thorns | Summon within 800: 3 thorns a second × 6 dmg (+3 bleed each), 60 HP, 20 s, cd 24, 25 Growth. Thorns are your projectiles (2,400 cm/s, no homing) | KEPT combat |
| Bloom | Deep roots | 5 ranks | 5 | More plant health | +15% HP for your plants per rank | GAP-COOKED |
| Bloom | Pollen | 3 pts | 10 | Your plants slow nearby enemies | Enemies within 400 cm of any of your plants move ×0.8 (bosses ×0.9); a pale-yellow pollen haze shows the area | GAP-COOKED |
| Bloom | Overgrowth | 4 pts | 13 | Plant one extra summon at a time | Turret limit 1 → 2 | GAP-COOKED |
| Bloom | Fertile | 3 ranks | 17 | Plants last longer | +15% plant duration per rank | GAP-COOKED |
| Bloom | Ancient guardian | 10 pts | 20 | Capstone: summon a walking tree guardian | Summon a 4 m walking tree, 400 HP, 25 dmg slams (W 0.8, visible contact), 30 s, cd 120, 60 Growth. It draws aggro from creatures within 10 m on arrival | KEPT combat |

Branch totals: Grove 30, Thorn 30, Bloom 30; trunk 10 (ST rule).

**Plants and the boss rule.** Plant summons are Raven's path abilities, not spirit summons (combat KEPT). They don't count as tames for boss HP and don't use tame slots. Creatures and bosses attack them normally; World tree and Ancient guardian draw the tame-punish moves when no tame is present.

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Rooted | Root-hairs at the soles; the bark patches tint green | Soft root creak |
| Entangle (any source) | Roots burst from the ground and wrap the legs | Wood snap and a rustle |
| Thorned skin | Thorns flick out of your armour at the hit point | A scratching hiss |
| Pollen | Pale-yellow haze round your plants | Faint bee hum |
| Purify | Grey-green motes lift out of the target | A clean chime |
| World tree | A tree 8 m tall grows out of the ground in 0.8 s, a gold-green canopy | Wood growth creak, birdsong |
| Ancient guardian | A walking tree pulls itself up out of the ground | A deep groan and footfalls |

**Combos with Nature (CB SPEC, names and effects as written; numbers GAP-COOKED; built in R3 for the paths that exist):**

| Pair | 25/25 | 50/50 |
|---|---|---|
| Fire + Nature | **Smoulder roots:** Entangled enemies take 40 burn buildup the moment roots wrap them | **Forest fire:** your plants explode when they die: r 300, 30 dmg, burn 50 |
| Frost + Nature | **Evergreen:** snow and ice count as Normal ground for Growth, and your plants don't wither there | **Frozen grove:** allies in Sanctuary or World tree also get +15 poise and frostbite resist ×0.5 |
| Lightning + Nature | **Quickening:** each shock proc within 10 m makes your plants fire 30% faster for 5 s | **Living conductor:** each turret thorn has 20% to chain 10 dmg + shock 15 to a second enemy within 4 m |
| Nature + Blood | **Red sap:** healing others heals you for 30% of it | **Thorn feast:** each bleed proc from your Nature hits heals your plants 20% of their HP |

The other 7 Nature pairs (Sun, Wind, Arcane, Tide, Necromancy, Spore, Void) keep their SPEC names from CB and get numbers in their partner's round.

### 2.18 Body change: Nature (look KEPT from hero.md; detail GAP-COOKED in fire-frost's frame)

**Frame (KEPT, hero.md and fire-frost 3.7):**
- Strength is points ÷ 100. Main path full, subclass ×0.6. Changes ramp smoothly.
- Built on the MetaHuman hero (R0) with the G mask channel, no emissive.
- **The look:** a healthy, sun-warm, sensual forest look. The bark is fine, smooth young-beech bark (silver-grey and soft-looking, like a tattoo with texture), never crusty or diseased.

| At | What shows (hero.md stages 0.3 / 0.6 / 1.0 KEPT) |
|---|---|
| 0.3 | Fine bark patches at the knuckles; the iris takes a leaf-green tint at the rim |
| 0.6 | Bark at the elbows and knees, curving like the grain of wood; the iris is half green; skin gets a warm, sun-kissed tone (+5% warmth) |
| 1.0 | Bark at the shoulders and collarbones in a pattern like the edge of a leaf; full leaf-green iris; small leaf-bud decals at the temples (KEPT) |

- **Rooted heal:** the bark patches tint green for as long as Rooted is healing (an albedo shift, no glow).
- **Nature with another path:** with Fire, ember veins run in the cracks of the bark; with Frost, rime forms along the bark edges; with Blood, garnet threads under the bark at the wrists.

### 2.19 Gear and items from the forest

**Heartguard set (stats and inputs KEPT from survival-crafting; name KEPT from hero.md):** medium, poise 40.
- **Inputs (full set):** steel plate 10, heartwood 10, Fernsheep wool 6, resin 4.
- **2 pieces:** bleed buildup ×0.5.
- **4 pieces:** pass thornbrakes; standing still 2 s heals 1 HP a second (stacks with Rooted).
- **Look (hero.md KEPT, merged):** layered bark pauldrons of lacquered heartwood with amber cores, an antler crown helm, green-dyed leathers, leaf-strip skirt flaps (cloth sim), braided cords. Forearms and throat open. Moss green, amber, bark brown.

**Hazard gear (KEPT):** briarproof leathers (Chest, armorer 2: hide 8, heartwood resin 2).

**Forest items (GAP-COOKED):**

| Item | Station | Inputs | Effect |
|---|---|---|---|
| **Bootlace blight** jar | Alchemy 2 | Bootlace rhizomorph 2, clay pot 1, fat 1 | Thrown: gravity arc 1,200 cm/s, never homes. On a Sylvaroth Root Knot: that line dies 90 s. On a stripped Old Gnarl spot: no regrow for 30 s. On any tree-bodied creature: its healing stops 20 s. The recipe is learned by quest 5 **or** by picking up your first rhizomorph |
| **Smoke pot** | Workbench 1 | Clay 1, damp moss 3, fiber 2 | Thrown: a 6 m smoke cloud for 10 s; wasps and bees in it go passive; the Bramblequeen in it slows ×0.7 for 5 s; removes the scout mark |
| **Briar balm** | Alchemy 1 | Thorns 4, resin 1, fat 1 | 10 min: thornbrake bleed buildup ×0.3 (world-map KEPT) |
| **Snare stake** | Workbench 2 | Heartwood 1, rope 2 | Placed hunting snare in the wild (not inside claims: Raven's no-traps-in-bases rule): the first creature up to mount size that steps on it is Entangled 3 s (the Hornwarden too). Lasts 1 game hour |
| **Thorned arrows** | Workbench 1 | Wood 1, thorns 2 (×10) | Bleed 20 per hit |
| **Heartwood longbow** | Workbench 3 | Heartwood 4, resin 1, gut string 1, + the metal tier's fittings | Heartwood is the forest's bow material: +10% draw speed and +15% range over the same tier's normal bow |
| **Heartwood shield** | Armorer 3 | Heartwood 6, iron 2, resin 2 | Medium shield; Nature negation 80% (combat's biome-shield rule), bleed negation +20% |
| **Foxfire lantern** | Workbench 2 | Foxfire wood 3, glass 1, copper 1 | A cold green light, radius 8 m, never runs out, never burns out in rain |
| **Huntsman's crown** | Drop: the Hornwarden during the Wild Hunt | — | Head slot: your mount's sprint stamina −20%; looks like a circlet of antler tines and ivy |
| **Hermit's ring** | Quest 6 (2.11) | — | Growth regen +15%, Rooted starts 0.5 s sooner |
| Trophies → talismans | Armorer 2+ | Trophy + 2 heartwood (KEPT rule) | Gnarl heart, Queen's sting, Matron claw, Warden antler (KEPT effects, 2.13) |

**Already set elsewhere (KEPT, restated):** Heartwood lodge building set; Grove press (L5 biome machine); forest crops (meadow oats, Iðunn apple) and the Iðunn apple pie; beehive and beekeeping; Ironbark brew; Rune of Thorns; battering ram; forest fishing (brook trout, silver salmon, Grandfather Sturgeon).

### 2.20 The story the forest tells, with no words (GAP-COOKED; fits world-map's Twelve Peoples and pass wars)

**The one idea:** the forest is one body, and the Grovekin chose to become part of it.

| What happened | How the player reads it, without a word |
|---|---|
| **The Grovekin lived by a pact with the trees.** They took only fallen wood, trained living trees into houses, walls and bridges, and left bread on stones for the forest | Every Grovekin structure is grown, never cut (house-trees, pleached forts, root bridges). Bread stones at every door and offering stones by trail trees. Carved hulder posts: beautiful women with bark backs, at every gate |
| **Their dead went into the roots.** They laid their dead in bark cradles on the roots, and over centuries the roots took them in | The Cradle Wood: hundreds of empty cradles, the roots fat beneath them. The Hollow Root's Cradle Vault: the oldest bodies are nearly roots. Nobody's grave has a body in it; the trees have them |
| **Their heart-tree became a god.** One heart-tree was strangled by its own roots, rotted hollow and stood up | The Lattice Chamber shows the same hollow lattice as Sylvaroth's body. The Grovekin burials are inside Sylvaroth's lattice. The Thrumming Root beats. The trail trees all bend **away** from the Four Hearts: a warning |
| **The pass war with the Lantern Mourners.** The Mourners came through Rootgrave for the dead the roots had taken; the roots crossed the pass and split the Mourners' own tombs | Rootgrave (KEPT): roots gripping a cemetery, tombs split open. The Rootgrave Fort's gate is broken **outward** and its west face is crusted with grave salt (salt kills roots: the Mourners salted them). The Mourners' champion, a treant they raised, lies dead on the forest side with its roots rotted black (KEPT). The Grovekin champion, a giant, lies on the Graveyard side, grown through by roots (KEPT); another giant's bones lie in the Grove of Bones with a heartwood spear still in its hand. Tōrō lanterns pushed deep into the forest, each split by roots |
| **The newcomers cleared the meadows, then went too far** | The meadows and farms are newcomer work (fresh field walls, a wreck on the dunes). The Clear-cut, the Woodcutters' Camp with roots grown through the tents and bones, axes still in the stumps, the "Tally of trees felled" lore item. In the middle of it, Old Gnarl |
| **The hermit learned it all** | Rowan's house is a Grovekin house he moved into; his shelves hold Grovekin carvings and a newcomer's ship lantern. His garden copies the Herb Terraces. He never explains anything |

### 2.21 Invasions: Nature ↔ Necromancy through Rootgrave (3.3 rule KEPT; contents GAP-COOKED; built in R4 when the Graveyard opens)

| Direction | Who comes | Where they march | Sign |
|---|---|---|---|
| **Forest into the Graveyard** | 8 to 14 of Thornlynx, Mossmaul, Rootcoil and Burlbeast (no passives) plus **the Hornwarden** (the roaming mini) | The nearest Graveyard claim or shrine within 1.5 km of Rootgrave's west mouth | Moss and flowers spring up along their path for 30 min; the Grovekin pillar on the Ring burns green during the 5 min warning |
| **Graveyard into the Forest** | 8 to 14 Graveyard creatures plus its roaming mini (the R4 cook names them) | The nearest forest claim or shrine within 1.5 km of Rootgrave's east mouth: usually the **Canopy Shrine** (−620, +3,050), then the Nurse Shrine | The tōrō lanterns of the Lantern Line light by themselves 5 min before |

Pays both biomes' path materials and 2,000 path XP to everyone who dealt 10% (KEPT 3.3).

### 2.22 Hidden places (GAP-COOKED; referenced above as F-S1 to F-S6)

| # | Secret | How you find it | What's there |
|---|---|---|---|
| F-S1 | **The Lost Path's end** | At the Lost Path (+200, +3,450) the foxfire loops forever. Ignore it and follow the trail trees' knee-bends: four bends, then a gap in a thornbrake | A Grovekin hunter's cache in a buttress hollow: a heartwood longbow blueprint, thorned arrows ×30, a gem |
| F-S2 | **The Alder Grotto's back room** | Behind the Great Falls curtain (C2), a squeeze under the glowworm overhang that only shows its draught (the glowworms sway) | A dry room with a Grovekin fishing shrine and the "Sturgeon hook" (fishing line +1 tier for the legendary fish) |
| F-S3 | **The Sinkroot squeeze** | At the bottom of C5, a crawl under a root (crouch), 30 m | The back way into the Hollow Root's Iron Seam |
| F-S4 | **The barred door** | Bread-stone Lane's grown-shut house-tree. Burn the door's living bark (torch, 6 s) | A Grovekin family seated round a table, grown into the chairs by the roots, hands joined. A heartwood circlet (cosmetic) and the lore item "Last loaf" |
| F-S5 | **The Bell-Tree's bell** | Climb the Bell-Tree (root holds, 40 m) and ring the bell | Rung once a game day: every Glimmerfinch within 300 m flies to the nearest ripe wild plant or hive and hovers (a free forager sweep). At night, ringing it makes the Grey Matron leave the Bower and hunt the bell (a fight on your terms, outside her arena, with no walkways: she's slow) |
| F-S6 | **The Matron's larder** | After the Grey Matron dies, the hollow of the Bower's tallest oak opens (a hide curtain falls) | Hides ×10, a Thornlynx egg-like "kit" for a guaranteed Thornlynx tame (KO not needed: offer food, medium), and old Grovekin jewellery |

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing from data, so `L_Island` is touched as little as possible.

### 3.0 What R3 needs from shared work first

- **The attack-row runner** (combat C9), **visible-contact boss damage** (combat C10) and the **grab framework** (LATER-COOKS, swamp lane) for Maul hug, Constrict, Root hand, Grasp and Antler toss.
- **Bleed** (R2, built before this round).
- **Poison moves up from R8 to R3.** Bramblewasps, Hivebacks, the Bramblequeen, the Queen's sting talisman (KEPT) and the Purify node (SPEC) all need it. The status row is already written in combat 2.11 (0.4% max HP + 2 per s for 30 s; antidote cures). Logged in LATER-COOKS.
- **Entangled** is new (2.17): one status row and handler.
- **The world-map terrain** for B5 east (world-map's W step for R3) and the Rootgrave pass sealed by rockfall until R4.
- **Shrines, path masters, quests, events, roaming minis, body change** from the fire-frost frame.
- **The summon framework** (FULL-GAME 11, R3 shared): one `AWRPlantSummon` base this lane uses for all four plant abilities.

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/nature.json`** | Same schema as `frost.json`: `id: "nature"`, `name: "Nature"`, `biome: "forest"`, `starter: "thorn_shot"`, `resource {id: "growth", name: "Growth"}`, `rootPassive {id: "rooted", name: "Rooted", text: "Standing still slowly heals you."}`. Trunk `thorn_shot, growth_capacity, entangle_chance, briar_strike`. Branches `grove` (`regrowth, lifebloom, sanctuary, sap, purify, world_tree`), `thorn` (`vine_lash, barbs, root_snare, thorned_skin, briar_spread, strangling_vines`), `bloom` (`sprout_turret, deep_roots, pollen, overgrowth, fertile, ancient_guardian`); `kind`, costs, ranks and gates as in 2.17. Node text copied from ST word for word |
| `Data/tuning.json` `resources` | `growth {max: 100, lush: 6, normal: 3, barren: 1, sampleRadius: 800, sampleSeconds: 0.5, offeringStone: 50}` |
| `Data/tuning.json` `nodeEffects` | `nature.growth_capacity {maxResource: 10}`, `nature.entangle_chance {entangleChance: 0.05}`, `nature.lifebloom {natureHealing: 0.08}`, `nature.sap {resourceRegen: 0.15}`, `nature.barbs {natureBleedBurst: 0.10}`, `nature.briar_spread {rootRadius: 0.15}`, `nature.deep_roots {plantHealth: 0.15}`, `nature.fertile {plantDuration: 0.15}` |
| `Data/tuning.json` `passives` | `rootedDelay: 1.5, rootedHealPct: 0.01, rootedBarrenMult: 0.5, thornedSkinDamage: 8, thornedSkinBleed: 10, thornedSkinPvp: 0.6, pollenRadius: 400, pollenSlow: 0.8, pollenBossSlow: 0.9, purifyPoison: 20, purifyRot: 20, overgrowthTurrets: 2` |
| `Data/tuning.json` `spells` | Nature entries as combat 2.13 (KEPT), plus Growth costs (`thornShot 6, regrowth 15, sanctuary 30, rootSnare 20, sproutTurret 25, worldTree 50, strangling 60, guardian 60`), `strangling.bossSlow 0.6, strangling.bossStance 10`, `worldTree.damageTaken -0.2`, `sproutTurret.thornBleed 3, thornSpeed 2400`, `guardian.slamWindup 0.8` |
| `Data/tuning.json` `status` | New `entangled {lowland: 1.5, midland: 1.0, highland: 0.6, player: 0.6, bossSlow: 0.8, bossSlowSeconds: 1.0, immunity: 8}`; the `poison` row from combat 2.11 moved to R3 |
| `Data/tuning.json` `hazards` | `thornbrake {bleedPerSecond: 6, runMultiplier: 0.5, firstTickDelay: 0.6, torchGapRadius: 400, torchGapHours: 5, balmMultiplier: 0.3}`, `widowmaker {tell: 1.0, damage: 70, radius: 200, shootDown: 30, roarRadius: 600}`, `leafPit {depth: [400, 900]}`, `waspNest {proximitySeconds: 5, proximityRadius: 400, swarm: [6, 10], smokeSeconds: 60}`, `spate {push: 900}` |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `hook_swipe, hammer_down, maul_hug, bull_rush, roar_stagger, canopy_drop, fang_bite, pounce, tree_escape, log_strike, constrict, body_slam, sting_dive, scout_mark, antler_charge, antler_rake, rear_strike, bugle, stomp_low, horn_sweep, crash, heat_ball` |
| `Data/tuning.json` `bosses` | `sylvaroth {stance: 180, parries: 3, lineHealPct: 0.0015, lineRegrow: 45, blightSeconds: 90, knotHealth: 900, knotAxe: 1.3, knotFire: 1.5, armStance: 70, wristHealth: 600, uprootRadius: 2000, uprootDark: 10, chargeStopShort: 600, vineSectors: 6, vineLive: 12, vineEvery: 30, vineCreep: 1.5, rerootSeconds: 6, bloomHealMult: 1.5, p3HeartHealth: 1500, p3SweepEvery: 6, p3ShakeEvery: 20}`, `old_gnarl {barkPlateHealth: 400, axeVsBark: 1.5, fireVsWood: 2.0, regrowEvery: 40, regrowWindup: 1.5, burnStaggerEvery: 10, boulderSpeed: 1400, boulderMinRange: 800}`, `bramblequeen {combHealth: 300, swarmEvery: 25, swarmSize: 6, markSeconds: 30, smokeSlow: 0.7, hivebackHold: 2, poisonTaken: 0.5, noCombSlow: 0.8}`, `grey_matron {anchorHealth: 100, walkways: 6, groundAt: 4, groundSpeed: 0.6, kitsAt: [0.6, 0.3], regrowHours: 4}`, `hornwarden {antlerPartHealth: 1200, enrageSeconds: 60, enrageDamage: 1.3, enrageSpeed: 1.2, enragePunish: 0.7, holdSeconds: 3, holdStance: 2.0, holdsToBreak: 3, holdWindow: 20, herd: 6, huntHerd: 10}`; per-attack rows from 2.13 and 2.14; run-back stone positions; lock points |
| `Data/tuning.json` `events` | `bloom {warnSeconds: 300, growthMult: 2.0, rareSeedChance: 0.10, elderFalls: 3, elderFallWarn: 5, elderFallLength: 9000, waspRaid: {swarms: 2, size: 12, radius: 150000}, wildHunt: {herd: 10, route: "hornwarden_hunt"}, payout: 3.0}` |
| `Data/creatures.json` | 11 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `nature`, `resists ["thornbrake", "leaf_pit"]`, taming, difficulty, food (Burlbeast `heartwood_shavings`, Glimmerfinch `seeds`), attacks, activity, loot, mount and rideSpeed for Elderelk, hauler carry for Burlbeast, utilities `truffle_sense`, `pollinator`, `heat_ball`); 5 boss rows (`sylvaroth` major; `old_gnarl`, `bramblequeen`, `grey_matron`, `hornwarden` with `roaming` and `route`) |
| `Data/spawn_regions.json` | Forest regions F1 to F5 with `area` boxes from 2.2 and `night` tables from 2.12.3 |
| `Data/items.json` | `heartwood` (exists in survival-crafting's list; check), `heartwood_resin, heartwood_shavings, truffle, bootlace_rhizomorph, foxfire_wood, thorns, wasp_sting, wasp_paper, mossmaul_claw, spotted_pelt, rootcoil_leather, rootcoil_shed, verdant_essence, core_sylvaroth, verdant_core, trophy_gnarl_heart, trophy_queens_sting, trophy_matron_claw, trophy_warden_antler, huntsmans_crown, hermit_ring, world_root_maul_bp, grovekin_decor_bp, heartwood_longbow_bp, sturgeon_hook, heartwood_circlet`; consumables `bootlace_blight, smoke_pot, briar_balm, snare_stake, thorned_arrow, foxfire_lantern`; lore items (7: tally of trees felled, antler crown, bark cradle, door rune, root-pulse, last loaf, ship lantern) |
| `Data/recipes.json` | The 2.19 recipes; Heartguard pieces and briarproof leathers (KEPT inputs); heartwood shield and longbow |
| `Data/pieces.json` | No new building pieces here (Heartwood lodge and beehive are survival-crafting's) |
| `Data/world_pois.json` (fire-frost's file) | Forest sub-zone polygons, 12 landmarks, 36 hand sites, 5 shrines, 5 caves, Hollow Root rooms, secrets F-S1 to F-S6, kit rules (2.4), the Hornwarden's route and Wild Hunt spline, the Clearing of Four Hearts layout (heart-trees, lines, knots, stones, altar), the Bower's walkway graph, the Paper Hollow's floors and combs, the Clear-cut stump field, thornbrake polygons, widowmaker spots, leaf pits, the "no felling" radius round giants |
| `Data/quests.json` | Rowan's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 11 and 5 bosses; pools `explore.forest.day/night`, `combat.forest`, `mini.forest`, `boss.sylvaroth` (3 sections), `sub.hollow_root`, `event.bloom`, `event.wild_hunt`, `camp.forest`, `village.forest`; sub-zone ambient beds F1 to F5; birdsong one-shots |
| `Data/fx.json` | Entangle roots, Rooted root-hairs, thorn projectile, Sprout turret, World tree growth, Ancient guardian rise, Pollen haze, Purify motes, Thorned skin flick, canopy-drop leaf shower and shadow, scout-mark dust, heat-ball cluster, Sylvaroth line pulse and knot break, vine sector creep, seed pod swell and burst, Uprooting ring, Old Gnarl bark strip and regrow, Bloom pollen haze, Elder Fall dust, foxfire (night) |
| `Data/combos.json` (R1 creates it) | Fire+Nature, Frost+Nature, Lightning+Nature, Nature+Blood rows from CB with 2.17 numbers |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRStatusComponent.cpp/.h` | **Entangled:** root lock on movement (sets a `bRooted` flag the CMC reads: no move, no roll, no jump; turning, attacks and casts allowed), tier durations, boss slow fallback, 8 s immunity. **Poison** brought forward (combat 2.11 row). Purify hook: a `RemoveBuildup(Tag, Amount)` call used by heals |
| `WRCombatLibrary.cpp` `ApplyHit` | Entangle-on-hit roll for Nature hits; Thorned skin return damage (same contact frame, as a new `FWRHit` from victim to attacker with `bReflected` so it can't loop); Barbs bleed burst scaling by source tag; Smoulder roots and Living conductor combo hooks |
| `WRPathComponent` / `WRAttributeSet` | **Growth regen sampler:** every 0.5 s on the server, classify the 8 m ground as Lush / Normal / Barren from the landscape layer weights under the player (grass, moss, forest floor = lush; dirt, rock = normal; lava, ice, snow, sand, stone = barren), plus "lush" if any actor tagged `Plant` (crops, plant summons, hedges) is within 8 m; inside caves without plants = barren. **Rooted:** a timer on no-move-input and no-roll; heal tick 1% max HP a second, ×0.5 on barren; reset on damage taken. Both through `HasNode`-style checks and tuning |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_ThornShot` (UWRGA_Spell bolt), `UWRGA_Regrowth` (ally-target HoT, self fallback), `UWRGA_Sanctuary` (AreaSpell pool, heal), `UWRGA_RootSnare` (AreaSpell pool with Entangle payload), `UWRGA_StranglingVines` (target hold with boss fallback), `UWRGA_SproutTurret`, `UWRGA_WorldTree`, `UWRGA_AncientGuardian` (all on `AWRPlantSummon`), `UWRGA_BriarStrike` and `UWRGA_VineLash` (UWRGA_Melee Wildbrands; Vine lash follow-up pull reuses the grab framework's pull). `ClassForNode`: a new `nature` block mapping the 10 ability node ids. Passives (`rooted`, `entangle_chance`, `thorned_skin`, `pollen`, `purify`, `overgrowth`) through `UWRPathComponent::HasNode` as Fire's are |
| New `WRPlantSummon.h/.cpp` | `AWRPlantSummon : AActor` (the R3 summon framework): owner, HP from tuning × Deep roots, lifetime × Fertile, a limit per owner per kind (Overgrowth), Pollen aura, `Plant` tag (counts as lush for Growth), takes damage through ApplyHit, death event (Forest fire combo). Subclasses: `AWRSproutTurret` (thorn projectile timer, target pick: nearest enemy in 1,500 with line of sight), `AWRWorldTree` (heal and damage-taken aura), `AWRAncientGuardian` (a simple walker using the creature AI's chase and one slam row with visible contact) |
| `WRCreature.cpp` | Attack-row kinds from 3.1. **Canopy drop:** a perch state on tagged trunk and branch points (from `world_pois.json` and tree instances above 10 m), a ground shadow decal and leaf-shower FX for 0.9 s, then a root-motion drop onto the target. **Tree escape:** a climb to the nearest perch below 40% HP. **Log strike:** a rest state with the body as a log among roots and a breathing loop. **Constrict:** the grab framework with a coil ring overlap test. **Scout mark:** a tag on the player that every Bramblewasp in 40 m reads as its target; water volumes and smoke clear it. **Heat ball:** Hiveback cling attach (max 4) on a target with a damage tick. Elderelk mount: thin ridden capsule and the 2 m leap. Burlbeast log drag (a carried bundle actor) |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `sylvaroth`, `old_gnarl`, `bramblequeen`, `grey_matron`, `hornwarden`. **Sylvaroth:** phase 1 rooted (no locomotion; yaw only), four `AWRRootLine` actors (heal tick, knot part HP, dark/regrow timers, blight, decay check, Bloom multiplier), Root hand with a ground ring and a wrist part, seed pod projectiles (ballistic, never homing), canopy shake branch spawner with shadows and the heart-tree shelter radius, the Uprooting transition (ring check, line blackout), phase 2 quadruped locomotion with knuckle-walk clips, the charge's stop-short query against heart-tree positions, vine sectors (six decal/volume sectors with creep, live and burn states), Re-root, phase 3 heart-tree fighters (`AWRHeartTree` with an amber heart part). **Old Gnarl:** four bark plate parts, stripped state with fire ×2 and burn, regrow timer cancelled by fire, the two-spot burn stagger, the stump-boulder projectile. **Bramblequeen:** four comb actors with HP, swarm call per live comb, smoke slow, Hiveback hold. **Grey Matron:** a walkway graph (6 `AWRWalkway` actors with two rope anchors each) as her nav; canopy-only behaviour until 4 are down; ground limp state. **Hornwarden:** roaming route component (fire-frost's), herd link (damage to any herd member → enrage), hold counter for stance break, the Wild Hunt route swap |
| `WRWorldDressing.cpp` | `FWRBiomeHazards::KindAt` gains `thornbrake` (and the torch-gap timer per cell). New actors: `AWRThornbrake` (overlap volume: bleed tick, run slow, the Nature-points and gear checks, burn-gap state), `AWRWidowmaker` (crack tell, fall, shoot-down), leaf pits (a landscape hole under a litter decal that collapses on step), `AWRWaspNest` (proximity and hit triggers, smoke state). A **no-felling radius** check in the tree-harvest code round giants and heart-trees. Kit and hand POI spawning from `world_pois.json` |
| `WRProjectile.cpp` | Thrown `bootlace_blight` and `smoke_pot` (ballistic, never homing); blight applies to `AWRRootLine` knots, Old Gnarl stripped spots and tree-bodied creatures |
| `WRWater.cpp` | The Alder's spate: a flow-push volume enabled for the block after Rain; the falls lip kill volume |
| `WRBiomeEvents.cpp` (fire-frost's) | The Bloom timeline: warning (pollen FX on giants, Glimmerfinch take-off, Ring pillar), ×2 crop and node growth, rare seed rolls, three Elder Falls (pick spot, 5 s crack tell, fall line damage, spawn a fresh fallen giant), wasp raids on hive pieces and beehive trees, the Wild Hunt at night, the Sylvaroth heal multiplier |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 forest shrines; Rowan (MetaHuman, Hermit's Oak and Grove Circle idle loops); quests |
| `WRBodyChange.cpp` (fire-frost's) | Nature parameters: G mask bark reveal (knuckles → elbows and knees → shoulders and collarbones), iris green ramp, temple leaf-bud decals at 1.0, Rooted green tint event, the blend rules with Fire, Frost and Blood |
| `WRUIHud` / `WRPlayerUI` | The Growth bar (moss green #5E8C31) with a small ground icon showing Lush / Normal / Barren; Entangled and poison meters (combat's status UI) |
| `WRAudioDirector.cpp` | Forest pools and sub-zone beds; Sylvaroth's 3 sections; the Bloom and Wild Hunt layers; birdsong scheduler (day only) |
| `WRSky.cpp` | Canopy light gradient by sub-zone (fog density and sky light), morning mist state, rain drip delay under canopy, foxfire and glowworm Niagara at night, the Four Hearts' amber sap emissive |

### 3.3 Assets and the art route

**Route (KEPT):**
1. Concept, picked by Aaron.
2. Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. **Real clips** from Fab animal packs or Truebones mocap, retargeted with UE's IK Retargeter.
6. Every creature shown standing at scale beside the hero before any behaviour is wired.

No engine primitives, no unfinished AI meshes.

| Asset | Route and clip source |
|---|---|
| Mossmaul | Concept from short-faced bear + ground sloth claws; Hunyuan base; Blender claws, moss and fern groom cards on the hump. Clips: a Fab bear pack (walk, run, rear, swipe, bite, roar, death) plus a knuckle-walk blend layer for the claws |
| Thornlynx | Concept clouded leopard; a Fab big-cat pack (walk, stalk, pounce, swipe, bite, climb, idle). The canopy hang and drop keyed in Blender on the cat rig |
| Rootcoil | Concept python with bark scales; a Fab snake pack (slither, strike, coil, idle) on a spline rig; constrict as a paired montage with the grab framework; a bark-scale material with moss saddles |
| Bramblewasp | Custom sculpt from giant hornet reference; a small rig with keyed fly, hover, sting, curl; Niagara swarm drives instances at range (meshes within 15 m) |
| Elderelk | A Fab deer/elk pack (walk, trot, gallop, rear, charge, graze); antlers sculpted big with ivy cards |
| Burlbeast | A Fab bison/bull pack; bark plates sculpted onto the back |
| Truffler | A Fab boar pack; long-snout morph |
| Hiveback | Custom bumblebee sculpt with a fur groom; keyed hover and sting; a cling pose for the heat ball |
| Fernsheep, Dappledoe | A Fab sheep or goat pack with fern-curl fleece cards; a Fab deer pack (fallow) |
| Glimmerfinch | A Fab small-bird pack; iridescent material |
| Old Gnarl | Concept (Aaron picks); a custom tree-giant rig (root-mass legs, two branch arms); clips keyed in Blender plus a giant-humanoid pack's slams and throws retargeted; bark plates as separate meshes that strip off |
| Bramblequeen | Bramblewasp rig scaled ×40 with a new head and abdomen sculpt; paper hive interior as a modular paper kit |
| Grey Matron | Thornlynx rig scaled ×1.7, grey coat, scars, a torn ear; a limp blend layer |
| Hornwarden | Elderelk rig scaled ×2.1, antler set 8 m with banner rags (cloth sim) |
| Sylvaroth | New concept (Aaron picks). A great-ape quadruped skeleton from a Fab gorilla pack as the base for phase 2 locomotion (knuckle-walk, rear, charge), with the lattice body sculpted in Blender (a hollow cage with foxfire emissive inside and burial pods). Phase 1 uses the upper body only, the lower body hidden in a ground socket. The canopy hump as a static mesh cluster with wind. Root arms as spline-skinned chains for the eruption and grab |
| Rowan | MetaHuman preset (attractive, mid 30s, bearded), outfit as 2.11 |
| Landmarks | The Four Hearts (one giant tree mesh, 140 m, ×4 variants with amber sap emissive), the Eldest, the Great Falls (terrain plus waterfall cards), the Rotted Treant, the Paper Hollow, the Bower (oaks and walkways), the Hermit's Oak, the Strangler Arch (a hollow lattice mesh you can walk inside), the Antler Barrow, the Bell-Tree, the Ringfort (pleached hedge ring) |
| Kit | Fab forest packs for oak, beech, lime, alder, redwood-scale giants, ferns, moss, brambles, wildflowers, hedgerows (licence rule: use unless explicitly barred); Grovekin props in Blender: house-trees, pleached walls, root bridges, bread stones, hulder posts, bark cradles, trail trees, walkways and lifts |
| Ground | Five ground layer sets (meadow grass, river gravel and fern, mossy cobble, leaf litter, root-mat) |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles and Kenney, layered in a DAW per species. About 8 files per creature and 12+ per boss. Loudness matched.
- **Music:** the 2.16 cue sheet chosen from libraries first (about 20 cues counting stems), credited.
- **Beds:** 5 sub-zone ambient beds, a Hollow Root bed, a falls bed that rises near the Great Falls, a day birdsong scheduler and a night owl scheduler.

### 3.5 Map dressing

- `Tools/terrain/dressing.mjs` reads the forest entries in `world_pois.json`:
  - It places hand sites at their coordinates.
  - It fills kit POIs per sub-zone with Poisson spacing of 150 m or more.
  - It walks routes (Grovekin roads, elk trails, the Alder's banks, canopy walkways) and adds a kit POI wherever a point is more than 125 m from any POI.
- **Tree density follows the canopy gradient:** 0% cover at the coast, 40% in F2 and F3, 75% in F4, 90% in F5. Giants (60 m+) only in F4 and F5. Every giant gets a thornbrake ring at its root crown (9% of F4 and F5 floor in total).
- **Fallen giants** are placed first (12 kit plus the Nurse Log), then light gaps are cut round POIs so each sits in a shaft of light.
- **Ground layers** are painted from sub-zone polygons by the existing auto-material.
- **Performance:** giants and heart-trees are Nanite meshes with HLOD; foliage uses the world-map streaming plan. Canopy walkways are only in F5 and at the Bower.
- **The landmass itself** comes from world-map's R3 terrain step (B5 east). This lane only dresses it.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R3 statuses: Entangled and poison** in `WRStatusComponent` + `FWRHit` buildups + status meters | Everything below uses them |
| 1 | **Proof of one: the Thornlynx in an Old-Growth gap, end to end.** Concept (Aaron picks), mesh, real big-cat clips, voice, attack rows with socket hits (canopy drop with shadow and leaf tell, swipes, fang bite, pounce, tree escape), one fallen giant with heartwood nodes beside it, a thornbrake ring round a giant's root crown with its tells and the 6/s buildup, briar balm, and the light gap dressed round it | Proves the canopy ambush, visible-contact damage, real clips, a fused voice, the hazard, the signature resource and a dressed place in one item |
| 2 | **The Nature path:** `nature.json`, the Growth sampler, Rooted, the 10 ability classes in `ClassForNode`, `AWRPlantSummon` and its three kinds, the passives, the proc FX, the CB combos with Fire, Frost, Lightning and Blood | The path is the round's headline; it needs only step 0 |
| 3 | Forest terrain dressing: sub-zones F1 to F5, the canopy gradient, the 12 landmarks, the 36 hand sites, the kit fill, the village site, caves, resources (heartwood, resin, truffles, rhizomorph, foxfire) | The world the rest stands in |
| 4 | The other 10 creatures with clips, voices and rows; day and night tables; living-world beats (wasp raids, larders, Glimmerfinch signs) | Reuses step 1's frame |
| 5 | Micro-hazards (widowmakers, leaf pits, wasp nests, spates) and weather (Rain effects, morning mist, canopy drips) | After the ground exists |
| 6 | Old Gnarl (strip and burn), then the Bramblequeen (combs, smoke, Hivebacks), then the Grey Matron (walkways), then the Hornwarden (roaming, herd, snares) | Minis in order of new tech |
| 7 | Sylvaroth: arena, root lines and knots, Bootlace blight, phase 1, the Uprooting, phase 2 quadruped and vine sectors, Re-root, phase 3 behind the artifact flag; drops; altar; music | The biggest single item; needs creatures, Entangled and the plant tech |
| 8 | Shrines, Rowan and his 6 quests, run-back stone, fast travel | After POIs and the hero rig |
| 9 | The Hollow Root and the six secrets | After the hazards and quest 5 |
| 10 | Music set, birdsong, sub-zone beds | Independent; can slot in anywhere after step 3 |
| 11 | The Bloom (Elder Falls, wasp raids, the Wild Hunt) | Needs the minis, hives and fallen-giant tech |
| 12 | Gear: Heartguard fit on the MetaHuman bodies, forest items, heartwood bow and shield | After the R2 slot split (survival-crafting) |
| 13 | Body change for Nature | After the MetaHuman hero (R0) |
| 14 | Nature ↔ Necromancy invasions | R4, when the Graveyard and Rootgrave open |

**Gates:** compile, cook and package only. Aaron tests each step.
