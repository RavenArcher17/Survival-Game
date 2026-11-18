# Cook: biome-ocean (Ocean & reefs / Tide, round R6)

Lane: biome-ocean. Written 2026-11-13. Scope: FULL-GAME 3.2.9, the Ocean rows of 3.3, and the Tide row of 4.2. This covers the whole Ocean & reefs biome, the shared underwater play R6 needs, and the whole Tide path.

**Tags.**
- **SPEC** keeps Raven's design, or a built thing, exactly as written.
- **GAP-COOKED** is this lane's design. It replaces the FULL-GAME first draft.
- **KEPT** means another 11-13 cook already set this number or name (world-map, combat, survival-crafting, hero, fire-frost, biome-desert). It's restated here so the build has one page. This lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md.** Belt B2, east of the Spine, opposite the Desert across **Saltgate**. Camps, the Thalassor arena and the Drowned Hall coordinates. The POI count (113). Weather weights. The **Saltkin** and their driftwood-and-shell-chime waymark. Open sea, tidal races, wreck tide, boats, and the sea theme.
- **biome-desert.md.** The Saltgate champion fix: the Sunwatch's colossus lies on the Ocean side, eaten by coral. The Desert-invades-Ocean contents. The Ocean's 6 m wet-sand mark when it invades.
- **combat.md.** The soak status. The Tide spell and Wildbrand numbers. The attack-row schema. The boss template. The swimmer archetype. Visible-contact damage.
- **survival-crafting.md.** Breath, depth and swim stamina numbers. Tidecoral set stats and inputs. Diving helm. Sea kelp and sea grapes. Ocean fish and the Lazarus Fish. Deepbreath brew. Trophy talismans (Barnacle claw, Siren scale, Tyrant fin). Swimming mounts and the hauler. Tide mill. The Coral keep building set.
- **hero.md.** The Tide body-change look. Tide colour sea-green #1E8C86. The set name **Reefmail**, which replaces "Tidecoral armour" so it doesn't clash with the resource.

**What's new in the repo for this round.** Raven's `WRWater.cpp` (`AWRWaterVolume`, `UWRWaterSubsystem`) and `WRSea.cpp` (`AWRSea`) give surface swimming and drinking only. There is **no diving, no breath and no depth** in the code today (checked). R6 builds them, and this lane designs them (2.6).

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Monster Hunter Tri, underwater combat** ([GamesRadar](https://www.gamesradar.com/games/monster-hunter/why-doesnt-monster-hunter-wilds-bring-back-underwater-combat-former-series-producer-reckons-its-like-making-a-whole-extra-game-its-really-super-hard), [Automaton](https://automaton-media.com/en/news/former-monster-hunter-series-producer-says-adding-underwater-battles-is-like-developing-a-whole-new-game/), [GamingBolt](https://gamingbolt.com/10-video-game-sequels-that-removed-features-due-to-criticism-for-predecessors/3), [Wikipedia](https://en.wikipedia.org/wiki/Monster_Hunter_Tri)) | Full 3D underwater fights. Capcom's own producers said it was like building a second game. Players found movement clumsy, weapons that didn't feel right, and multi-monster fights a mess. The series dropped it | **Underwater play is lean and readable, never a second game.** (1) A short underwater verb set: thrust weapons, harpoon, bolts, a kick-dodge (2.6). (2) Underwater enemies come at you on a readable plane, with a bubble-trail and sound tell. (3) Bosses keep players at the surface or on perches. The deep is for exploring, harvesting and the odd ambush, not long brawls. (4) Gills (the Tide root passive) is what makes the deep comfortable: it's the path's reward |
| **Monster Hunter Wilds, Uth Duna** ([Mobalytics](https://mobalytics.gg/mhw/uth-duna-guide), [Push Square](https://pushsquare.com/guides/monster-hunter-wilds-how-to-beat-uth-duna-weaknesses-and-rewards), [PowerPyx](https://www.powerpyx.com/monster-hunter-wilds-uth-duna-large-monster)) | A water leviathan that wears a "water veil" for armour and speed. You break the veil to open it up. It owns the region's flood weather | **The boss's armour is its element, and you break it with the opposite.** Thalassor's wet skin is its armour. Sun-dried skin cracks open (2.14). Its arena floods with the tide, the way Uth Duna rides the Downpour |
| **Subnautica, the leviathans** ([The Geek Wave](https://thegeekwave.com/2020/06/subnautica-and-sound), [deepwave](https://deepwavegame.github.io/news/subnautica-horror-unknown-depth)) | You hear the Reaper long before you see it. Distance-based audio and low rumbles sell the size and danger of the deep | **Sound before sight, underwater most of all.** Every big ocean creature has a far call that carries 200 m or more through water. Its tell is a sound (a click train, a thrum, a song) before the visual cue. Thalassor's sperm-whale "creak" is its attack tell (2.14) |
| **Horizon Forbidden West, diving** ([Gameranx](https://gameranx.com/updates/id/343406/article/about-the-horizon-forbidden-wests-underwater-quest-design/), [Horizon Wiki: Diving Mask](https://horizon.fandom.com/wiki/Diving_Mask)) | Breath limits early dives. Then a story item (the Diving Mask) removes the limit and the whole sea opens. Some quests drain water to change a space | **Breath is the early gate. Gear, brews, mounts and the Tide path open the deep in steps** (2.6): Diver's draught, diving helm, Reefmail, Wavecharger, Gills. **Changing the water level reshapes a space:** the tide (2.8), the King Tide (2.10), and Thalassor's flood (2.14) |
| **Ark, the ocean** ([Ark wiki: Deep Ocean](https://ark.wiki.gg/wiki/Deep_Ocean), [ludo.guide](https://www.ludo.guide/guide/ark-survival-evolved/side-quests-activities/deep-sea-exploration), [Ark wiki: Aquatic](https://ark.wiki.gg/wiki/Aquatic)) | The deep holds the best loot and the worst threats. Shark packs and grabbers make diving tense. Tames are the answer: a swimming mount turns a deathtrap into a route | **Depth = value = danger.** Tidecoral and pearls are richest at the reef edge, where Reefjaws patrol. A Wavecharger (swimming mount) is the big quality-of-life unlock. The deepest cave holds the legendary fish (KEPT) |
| **Valheim, the Ocean** ([gamewith](https://gamewith.net/valheim/article/show/25146), [eip.gg](https://eip.gg/valheim/guides/6-tips-to-overcome-the-ocean/)) | Serpents spawn in deep water in storms and at night. Rafts break; real boats survive. The Leviathan is a "floating island" you mine until it sinks | **Weather and night make the open sea dangerous.** Squalls and night double the Reefjaw spawn at sea. The Reef Tyrant hunts boats. A raft is a death sentence past the reef (world-map KEPT) |
| **Breath of the Wild / Tears of the Kingdom** (Zora's Domain, geysers) | Water as traversal: waterfalls you swim up, geysers that launch you | **Blowholes launch you** (2.8) onto ledges at high tide. **Tow** (Harpooner) is a grapple you aim at any rock (2.17) |
| **Elden Ring and Dark Souls** | Bosses fought in shallow water, where wading slows you and splashes read every step | **The shallows are a fighting floor:** wading ×0.85 run speed, and splashes show every step and every tell. Thalassor phase 1 is fought on a tidal flat |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Ys, the drowned city of Brittany ([Wikipedia: Ys](https://en.wikipedia.org/wiki/Ys), [Wikipedia: Dahut](https://en.wikipedia.org/wiki/Dahut), [Wikipedia: Gradlon](https://en.wikipedia.org/wiki/Gradlon)) | A city behind a great dyke, sealed by a sluice gate opened only at low tide. Only the king held the key. His daughter Dahut opened the gate (for a lover, or drunk), and the sea took the city. Its bell still rings under the sea | **The Saltkin city under the reef (the Drowned Hall).** The Long Dyke, the Sluice Works with the key still in the lock, the empty key-hook on the king's tower, and the drowned bell you hear at night (2.5, 2.20) |
| Tsunami stones of Japan ([Wikipedia](https://en.wikipedia.org/wiki/Tsunami_stone), [Smithsonian](https://www.smithsonianmag.com/smart-news/century-old-warnings-against-tsunamis-dot-japans-coastline-180956448/), [99% Invisible](https://99percentinvisible.org/article/tsunami-stones-ancient-japanese-markers-warn-builders-high-water/)) | Hundreds of old stones mark how far past waves reached. Aneyoshi's says "do not build below this point"; in 2011 the wave stopped below it | **The Tsunami Line:** nine Saltkin stones on the terraces. Ruins sit below them, the later town above. They mark the King Tide's Return line exactly, so the land teaches you where to stand (2.10) |
| Orca wave-washing ([National Geographic](https://www.nationalgeographic.com/animals/article/animals-up-close-wave-washing-killer-whales), [Visser et al. 2008](https://www.orcaresearch.org/wp-content/uploads/2024/12/Visser-et-al-2008-Antarctic-killer-whales-use-waves.pdf)) | Orcas swim in formation to make a wave that washes seals off ice floes. Water draws back from the floe first (the trough), then the wave hits | **Thalassor's Wave wash** (2.14). The tell is the real one: water draws back off the flat, then the wave comes. The **Reefjaw surf lunge** copies orcas beaching themselves to grab sea lions |
| Humpback bubble-net feeding ([Wikipedia](https://en.wikipedia.org/wiki/Bubble-net_feeding), [National Geographic](https://www.nationalgeographic.com/animals/article/humpback-whales-bubbles-tools)) | A whale spirals up under prey, blowing a ring of bubbles that traps it, then lunges up through the centre | **Bubble net**, Thalassor's tame-punish move: a bubble ring rises around the furthest tame, then the lunge comes (2.14) |
| Mantis shrimp strike ([Patek et al., Nature 2004](https://doi.org/10.1038%2F428819a), [ScienceBlog](https://scienceblog.com/sb-the-mantis-shrimp-punches-with-the-acceleration-of-a-22-calibre-bullet-creating-a-cavitation-bubble-that-collapses-at-nearly-the-temperature-of-the-suns-surface-for-a-fraction-of-a-millisecond-mean/)) | The club moves so fast the water cavitates. The bubble collapses with a flash and a second shockwave, so every strike hits twice | **Spinecrab's Club snap:** a visible white cavitation pop, then a second hit 0.06 s later (2.12) |
| Moray pharyngeal jaws ([UC Davis](https://www.ucdavis.edu/news/alien-jaws-help-moray-eels-feed), [National Geographic](https://www.nationalgeographic.com/science/article/moray-eels-attack-with-second-pair-of-alien-style-jaws), [Smithsonian](https://www.smithsonianmag.com/smart-news/extra-set-hidden-jaws-drags-food-down-eels-throat-180978049/)) | A second set of jaws in the throat shoots forward to grab prey. Snowflake morays use it out of water too | **Tidecoil's Second jaw** grab, and its slither between tide pools at low water. **The Siren Eel's** drag (2.12, 2.13) |
| Funayūrei and umibōzu ([Wikipedia: Umibōzu](https://en.wikipedia.org/wiki/Umib%C5%8Dzu), [yokai.jp](https://yokai.jp/en/yokai/funayurei), [Uncanny Japan](https://uncannyjapan.com/podcast/ship-goddesses-boat-ghosts-and-sea-monks-funadama-funa-yurei-umi-bozu/)) | Drowned spirits ask boats for a ladle to fill them with water. Sailors carry ladles with the bottoms knocked out | **The Ladle Rack:** a shore charm of bottomless bronze ladles, and the reef lighthouse hung with them. Environment only |
| Sirens and the wax (Homer, *Odyssey* XII); Blue Men of the Minch ([Wikipedia](https://en.wikipedia.org/wiki/Blue_men_of_the_Minch)) | Odysseus's crew plugged their ears with beeswax to pass the sirens. The Blue Men are storm spirits in tidal straits | **The Siren Eel** turns tames and lures players. **Beeswax plugs** block the lure, but they also muffle every sound tell. That's a real cost (2.13) |
| Rip currents ([The Swim Guide](https://www.theswimguide.org/2025/09/01/escape-rip-current), [Click Orlando](https://www.clickorlando.com/weather/2026/02/26/how-to-spot-and-escape-a-rip-current/)) | A dark, flat gap in the breakers, with foam moving seaward. You escape by swimming parallel to shore; fighting it straight in is how people drown | **Rip channels** on the White Strand (2.8). The real tell and the real escape. Thalassor's **Undertow** uses the same rule |
| Tidal races and whirlpools: Corryvreckan, Saltstraumen ([Wikipedia: Whirlpool](https://en.wikipedia.org/wiki/Whirlpool), [Geographical](https://geographical.co.uk/news/phenomena-maelstroms-and-whirlpools)) | Whirlpools form where opposing tidal currents meet over a sill. They're strongest at spring tide and calm at slack water | **Thalassor's hidden phase 3 Maelstrom.** The tide clock's slack water lines up with world-map's race slack (2.8) |
| Coral mass spawning ([eLife](https://elifesciences.org/articles/09991), [SCU research portal](https://researchportal.scu.edu.au/esploro/outputs/journalArticle/Mass-spawning-in-tropical-reef-corals/991012820688802368)) | Once a year, after a full moon, whole reefs release spawn on the same night across 500 km | **The spawning night** inside the King Tide: the reef glows and tidecoral regrows at once (2.10) |
| Bajau and haenyeo freedivers ([sci.news](https://www.sci.news/genetics/bajau-people-spleens-free-diving-05944.html), [eScholarship](https://www.escholarship.org/uc/item/1h41g9r9)) | The Bajau evolved spleens 50% larger, which squeeze out oxygen-rich blood on a dive. The haenyeo of Jeju are women who freedive for a living | **The path master**, Hae-won, a haenyeo-style freediving harpooner (2.11). The body change shows the diver's adaptation: gill lines, the sheen (2.18) |
| Sperm whale (general zoology) | A block head a third of its length. The blowhole sits off to the left. Clicks for sonar, with a fast "creak" just before a catch. Circular sucker scars from giant squid. Wrinkled, corrugated skin. It produces ambergris | **Thalassor's** head, blowhole lock point, attack tell (the creak), scars, and an ambergris drop |
| Oarfish; goblin shark; coconut crab; Steller's sea cow; sea otter tool use; male seahorse brood pouch; leafy seadragon; skuas and frigatebirds stealing food; leatherback nesting; Portuguese man o' war; Noctiluca blue surf; kelp-ash industry of Orkney; whalebone arches; whale falls | Real ocean biology and coastal history | Siren Eel's body (oarfish, the real "sea serpent"). Reefjaw's slingshot jaw (goblin shark). Spinecrab's crusher, the strongest pinch of any crustacean (coconut crab). Kelpgrazer (Steller's sea cow, a real 8 m kelp-eater). Pearldiver's stone and armpit pouch. Wavecharger eggs taken from the male's pouch. Gullraider steals from your bag. Driftback nests on the beach at night. Man o' war fleets (hazard). Bioluminescent night surf. Kelp-burning pits. A whalebone arch. The Whale Fall POI |

### 1.3 Lessons, short

1. **One idea feeds everything: the sea takes back.** The Saltkin walled the sea out and penned the Leviathan in. One key opened the gate, and the sea took the city. Every POI, creature and hazard follows the tide.
2. **The tide is the biome's clock.** Two highs and two lows a game day. Low water opens caves, pools and the shelf; high water opens blowhole launches and drowns the flats.
3. **Sound before sight, most of all underwater.** Every underwater attack has a bubble or wake tell and a sound tell.
4. **Underwater stays lean** (MH Tri lesson). Few verbs, readable planes, bosses at the surface.
5. **Breath is the early gate.** Gear and the path open the deep in steps. Gills is the payoff.
6. **The land teaches the rule:** tsunami stones mark the surge line; a shell line on a post shows how to swim out of a rip.
7. **Colour law.** Crimson (#DC143C) stays the unblockable cue only. Ocean reds (coral, Thalassor's scars) are coral orange #E07A4F and salmon pink. Sea-green #1E8C86 is Tide's own colour (hero KEPT); soak meters stay teal (combat KEPT).

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item | This lane |
|---|---|
| Path theme: water magic, underwater breathing, harpoons | SPEC (DD) |
| Look, sub-zones, landmarks, POIs, hidden areas | GAP-COOKED 2.1 to 2.5 |
| Underwater play (R6 shared system) | GAP-COOKED 2.6 |
| Caves, resources (tidecoral name KEPT) | GAP-COOKED 2.7 |
| Hazard: deep water | Numbers KEPT (3.3, survival-crafting). Tide, micro-hazards and tells GAP-COOKED 2.8 |
| Weather | Weights KEPT (world-map). Squall reconciled 2.9 |
| Event: the King Tide | Name kept. GAP-COOKED 2.10 |
| Shrine, path master | Shrines exist (RAVEN-PLANNED); who and where GAP-COOKED 2.11 |
| Creatures | 11 native species, names and taming KEPT from the FULL-GAME list. Floeback goes back to the tundra (2.12). Bodies, attacks and tables GAP-COOKED |
| 3 minis, 1 major | Names and premises kept. Fights GAP-COOKED 2.13, 2.14 |
| Voices, music | GAP-COOKED 2.15, 2.16 (tempo, key and instruments SPEC 3.3) |
| Tide tree | Nodes SPEC (ST). Spell and Wildbrand numbers KEPT (combat). Flow, Gills, passives and stat nodes GAP-COOKED 2.17 |
| Body change | Look KEPT (hero). Thresholds GAP-COOKED 2.18 |
| Gear | Reefmail (Tidecoral set) stats KEPT, look merged 2.19. Ocean items GAP-COOKED |
| Story through environment | GAP-COOKED 2.20 |
| Invasions Sun ↔ Tide | Desert side KEPT. Ocean side GAP-COOKED 2.21 |

### 2.1 Where it sits and how it reads (KEPT placement, GAP-COOKED look)

**Placement (KEPT, world-map 2.2):**
- **Belt B2, east of the Spine.** y −8,000 (north) to −5,400 (south).
- **Land.** From the Spine's foot (x ≈ +350 at Saltgate, +250 further north) to the outer coast at x ≈ +1,850. 3.0 km² of land.
- **Reef sea.** East of the coast to x +4,200, about 6 km².
- **Neighbours:**
  - Sky islands to the north: **sea cliffs with cage-lift chains** (world-map).
  - Swamp to the south: **a brackish estuary with mangroves** (world-map).
  - Desert across **Saltgate**, a 60 m saddle (east mouth at about +950, −6,700).
- **Bands** (distance from the Spine crest):
  - Highland: x < +950, the Pound and the Salt Terraces' upper tier.
  - Midland: +800 to +1,400.
  - Lowland: x > +1,250, the coast and the sea shallows.
  - The open sea (over 600 m from shore, so x > +2,450) follows world-map's open-sea rules.
- **Camps (KEPT):** (+1,650, −5,800), (+1,700, −6,650), (+1,550, −7,500).

**The look in one line.** Silica-white beaches and turquoise shallows under a high bright sky. Rust-gold coral-limestone terraces climb to the Spine, where a ring-walled lagoon steams with spray. Out to sea a line of drowned towers marches toward a reef that breaks white 700 m offshore. Beyond it the water goes ink-blue over the drop.

**Read rules (GAP-COOKED):**
- **Water colour tells depth and danger.** You read the whole sea from the beach:
  - Pale turquoise #7FD8D0: under 3 m, safe.
  - Teal #1E9C9A: kelp, 3 to 25 m.
  - Coral-mottled: the reef.
  - Ink blue #0B2A4A: the drop, past 25 m, crushing depth for the unprepared.
- **Height reads on the land.** Sea stacks, the Beacon Stack, the Pound's wall, the colossus and the dyke towers break the flat skyline.
- **Underwater visibility.** 40 m in the shallows and kelp, 25 m on the reef face, 12 m past the drop. Blue fog plus god rays from the surface. Sea fog weather halves it.
- **Night light** (world-map readable-nights rule):
  - Bioluminescent surf: blue Noctiluca glow in every breaking wave and around swimmers' strokes.
  - Glowray rings over the reef.
  - Thalassor's photophores: a blue-green line of lights you see moving in the Pound from the wall.
  - The Beacon Stack's fire, once lit.
  - Shell-chime beacons that glow faintly with phosphorescent paint.

### 2.2 Sub-zones (GAP-COOKED; 5, from FULL-GAME's list of beaches, tide pools, kelp forests, reefs, wrecks and sea caves)

World-map's 3-to-5 rule means wrecks become a POI type everywhere, not a sub-zone, and sea caves become the biome's caves (2.7).

| # | Sub-zone | Where (x, y m) | Band | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|---|
| O1 | **The White Strand** | Coast band x +1,300 to the waterline, y −5,400 to −6,950. Camps 1 and 2. The Swamp estuary at its south end | Lowland | Silica-white beach 80 to 200 m deep, low sea-grass dunes behind it, a turquoise sandbar lagoon. Driftwood everywhere. Rip channels between the bars | Sand white #EDE8DC, shallows turquoise #7FD8D0, sea-grass #8FA65A, driftwood silver #A8A39A | Newcomer wrecks, driftwood shelters, kelp-burning pits, turtle nests, shell middens, whalebone arch | Breaking surf (close), wind in sea-grass, Tidefowl chatter, the shell chimes |
| O2 | **The Blowhole Shelf** | North coast x +1,100 to the waterline, y −6,950 to −8,000. Camp 3. The sea cliffs on the Sky border | Lowland to midland | Wave-cut platforms of rust-gold coral limestone, honeycombed with tide pools, blowholes and surge channels. Sea stacks offshore. Floods at high tide | Limestone gold #C9A46A, pool green #3E8C7A, wet black rock #2A2A28, spray white | Tide pools, blowholes, sea stacks, the Gullery cliffs, Saltkin fish weirs | Booming blowholes, sucking surge, gull colony, wind |
| O3 | **The Salt Terraces** | Against the Spine x +350 to +1,300, y −5,400 to −8,000. Holds the Saltgate east mouth, the Pound, the Tsunami Line and the Upper Town | Midland to highland | Terraced Saltkin salt pans stepping up the Spine's foot. White salt crust, ochre limestone walls, tamarisk scrub. The Sluiceway channel. The great ring wall of the Pound in the north | Salt white #F2EFE6, terrace ochre #B98A52, tamarisk grey-green #8C9A7A, sluice-water teal | Salt pans, rakes, tsunami stones, terraced stone houses, bronze bollards, Sunwatch war gear at the gate | Dry wind off the pass, salt crackle underfoot, distant spray booming from the Pound |
| O4 | **The Kelp Forest** | Sea x +1,850 to +2,500, the whole belt length, 3 to 25 m deep | Lowland (shallows) to midland | Giant kelp 20 to 30 m tall, its canopy matting the surface, sunbeams through the fronds. Sandy channels. Urchin-barren patches. The Long Dyke's drowned towers cross it | Kelp gold-olive #8A7A2E, water teal #1E9C9A, sand floor #D8CDB0, urchin purple #5A2E6A | Kelp groves, otter rafts, dyke stumps, Saltkin mooring stones, newcomer anchors | Muffled surge, kelp creak, otter cracks, Wavecharger clicks, distant whale song |
| O5 | **The Reef and the Drop** | Sea x +2,500 to +4,200. Reef crest breaking at x +2,500 to +2,800, reef slope to x +3,500, then **the Blue Wall**: a drop to 70 m, then open sea | Midland to highland (by depth) | Coral gardens of tidecoral (orange, pink, cream), giant clams, the reef crest breaking white. Then the vertical blue drop-off. The Drowned Hall lies under the crest | Coral orange #E07A4F, coral pink #E8A0A8, reef cream #EADFC8, drop ink-blue #0B2A4A | Coral heads, wrecks on the crest, the reef lighthouse, clam gardens, man o' war fleets, the Whale Fall | Crackling reef (snapping-shrimp noise, real), surf thunder on the crest, deep-water hum past the drop |

**Blends:** 60 to 120 m (world-map rule 5).
- **Swamp border (O1 south, y −5,400 to −5,550):** the beach turns to brackish mud and oyster beds, red mangroves creep in, and the water browns.
- **Sky border (O2 north, y −8,000):** the shelf rises into 80 m sea cliffs with Kitewright cage-lift chains hanging down them, and Mistlarks over the edge.
- **Saltgate mouth (O3 west):** white pass salt flat into a tidal salt marsh, the **Gate Inlet**, which runs to the sea beside camp 2. Desert dunes spill east over the inlet beach (world-map).

### 2.3 Skyline landmarks (GAP-COOKED; 5 on land and 3 at sea, each read from 1 km)

| Landmark | At (x, y) | What it is | Reads at night as |
|---|---|---|---|
| **The Pound** (super landmark, reads from 3 km) | +1,000, −7,300 | Thalassor's lagoon. A Saltkin ring wall 240 m across and 30 m tall in coral-limestone blocks, with four bronze sluice gates on its east side. One gate is burst outward, its bronze leaves bent like foil. Spray booms over the wall when the beast moves. Giant bronze chain bollards stand round the rim, and the broken chains run down into the water | Thalassor's blue-green photophores moving under the water, seen from 1 km as a slow line of lights. Spray lit by moonlight |
| **The Drowned Colossus** | +1,000, −6,760 | The Sunwatch's champion (biome-desert KEPT fix). A sandstone colossus, twin of Ahrumet, 60 m long, lying face-down in the Gate Inlet's tidal water. Coral and barnacles have eaten through its back. Saltkin harpoons and a whaling chain are wrapped round its neck. One arm reaches back toward the pass | The inlet's blue surf around its outline. Its gold-leaf eye catches the moon |
| **The Beacon Stack** | +1,780, −7,250 | A 70 m sea stack joined to the shelf by a half-collapsed Saltkin causeway. On top, a giant driftwood beacon (their waymark at full size) under a cage of shell chimes. A keeper's stair winds up | Unlit at first. Light it (Interact, 4 wood): it burns all night, lighting O2 within 300 m, and it stays lit for your clan (a map vista) |
| **The Spouting Horn** | +1,560, −7,700 | A blowhole in the shelf that throws a 40 m water column on every rising-tide wave set. You hear its boom 1 km away. A Saltkin arch is carved around the vent | Moonlit spray column, glowing blue with Noctiluca |
| **The Dyke Head** | +1,860, −6,300 | The landward tower of the **Long Dyke**, 25 m tall, with a gate stair. Over its seaward door hangs an iron key-hook the size of a man, **empty**. From here the dyke's tower stumps march out to sea | A Saltkin lamp niche with phosphor shells |
| **The Reef Lighthouse** (sea) | +2,700, −7,200 | A Saltkin lighthouse tower on the reef crest, broken at 20 m, leaning seaward. Bronze ladles with knocked-out bottoms hang from every sill (funayūrei charms) | Waves bursting white and blue against its foot |
| **The Bell Tower** (sea) | +2,610, −6,450 | The top 6 m of the Drowned Hall's bell tower breaks the surface at low tide, in the reef crest's lee. At high tide only its finial shows. The Ys bell rings inside it (2.20) | The finial's phosphor glow. At midnight high tide, the bell rings under the water |
| **The Long Dyke** (sea, a route of drowned towers) | +1,860, −6,300 to +2,550, −6,350, then curving north to +2,650, −6,800 | Eleven tower stumps, 200 m apart, joined by a sunken wall top 1 to 3 m under the surface at low tide. You can wade or walk the wall top out toward the reef at low water | Phosphor lamp niches on the tower stumps, a dotted line out to sea |

**Plus the border landmark (KEPT, world-map):** the **Chain Cliffs** on the Sky border, where Kitewright cage-lift chains hang down the sea cliffs at (+1,400 to +1,800, −8,000). Counted with the Sky lane.

### 2.4 POIs: 69 surface + 36 sea + 8 in the Drowned Hall = 113 (count KEPT from world-map; contents GAP-COOKED)

**Surface 69:**
- 3 camps
- 5 land landmarks (2.3)
- 3 shrines (2.11)
- 2 land caves (2.7)
- 1 mini lair approach (Siren's Hollow shore mouth; Barnacle Cove is hand site 8)
- the Pound's run-back stone
- the Saltkin pass fort ("the Gate of Shells", +1,000, −6,650: driftwood-and-coral-block walls, harpoon racks and arrow slits facing west down Saltgate, per the world-map pass kit)
- the village site (world-map)
- 18 hand-made sites
- 33 kit POIs

By sub-zone: O1 23, O2 22, O3 24.

**Sea 36:**
- 3 sea landmarks
- 2 sea shrines
- 3 sea caves
- the Reef Tyrant's rest at the Mooring Stones
- 9 hand-made sea sites
- 18 kit POIs

By sub-zone: O4 15, O5 21.

**Hand-made surface sites (18):**

| # | Site | Sub-zone, at | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| 1 | **The Tsunami Line** | O3, a line of 9 stones along the 6 m contour from +1,150, −5,600 to +1,250, −7,800 | Nine Saltkin stones, 2 m tall, each carved with a wave and a single line. Below the line there are only ruins: wave-scoured house floors and a drowned boat in a terrace. Above it stand whole houses. **The line is exactly where the King Tide's Return stops (2.10)** | Lore item "Tide-stone rubbing" (charcoal on linen); a lesson you learn by looking |
| 2 | **The Upper Town** | O3, +900, −6,200 | Terraced Saltkin stone houses above the Tsunami Line. Nets on racks, salt jars, shell-tile roofs. Every door faces the sea and every house has a lookout ledge. Abandoned with meals set out | Containers: salt, rope, a fishing rod; the village site sits beside it (world-map) |
| 3 | **The Salt Pans** | O3, +700 to +1,000, −5,700 to −6,100 | Forty terraced evaporation pans, white crust, wooden rakes leaning in rows, a sluice board on each | Salt nodes ×16 (KEPT salt use); Spinecrabs on the crust at night |
| 4 | **Harpooners' Hall** | O3, +780, −7,020 | A Saltkin longhouse. A wall of harpoon racks, every point aimed toward the Pound. A frieze carved round the beams shows a whale-serpent with men standing on its back and chains in its flesh. Tally scratches in hundreds by the door | A worn harpoon (spear-class); lore item "Harpooner's tally-cord"; the **Whaler's harpoon** recipe scrap (2.19) |
| 5 | **The Chain Anchors** | O3, +900 to +1,120, −7,120 to −7,480 (the Pound's rim) | Four bronze bollards 4 m tall, green with age, each with a broken chain link the size of a door running over the wall into the lagoon. The breaks are fresh-bright inside | The arena approach; copper ×6 per bollard (scrape) |
| 6 | **The Sunwatch Mirrors** | O3, +880, −6,860 | At the Saltgate east mouth, a Sunwatch war camp: sun-disc standards, a burnt tent, and three bronze signal mirrors on tripods, all **aimed at the Pound**. The sun people fought the beast with sunlight. This is the clue to Thalassor's counter (2.14) | Lore item "Sunwatch mirror-shard"; a polished bronze sheet (Sun mirror recipe part) |
| 7 | **The Sluiceway Bridge** | O2/O3, +1,380, −7,260 | A Saltkin stone bridge over the tidal channel from the Pound to the sea, its middle span fallen. The channel runs 8 m deep at high tide and 2 m at low; it's where Reefjaws come in. Tidecoils nest in its piers | A Tidecoil pair; iron nails ×4 in the fallen span |
| 8 | **Barnacle Cove** | O2, +1,720, −7,640 | A rocky cove where a newcomer longboat hull sits upright on the sand like a shell, barnacle-crusted, with claw gouges round the hull. Two spare "shells" lie on the beach: a Saltkin bronze bell on its side and a Driftback carapace | Old Barnacle's lair (2.13.1) |
| 9 | **The Ladle Rack** | O2, +1,760, −7,380 | A fishermen's shore charm: a rope between two driftwood posts hung with 30 bronze ladles, each with its bottom knocked out, clinking in the wind | Bronze ×4; lore item "Bottomless ladle" |
| 10 | **The Gullery** | O2, +1,500, −7,880 (cliff face) | A seabird cliff on the Sky border, white with guano. Gullraider and Tidefowl burrows. The Gullraider nests are lined with stolen things: newcomer rings, a compass, a child's carved boat | Tidefowl eggs; stolen trinkets (a ring with a random gem, 10%) |
| 11 | **The Tide Clock** | O2, +1,640, −7,100 | A Saltkin tide gauge. A 6 m stone pillar standing in a pool, with carved notches and a float in a slot that rides the real tide. The top notch, the King Tide mark, is carved deeper, with a wave sign | Shows the live tide (the float moves with `UWRTideSubsystem`); a vista; Great Shrine nearby |
| 12 | **The Turtle Beach** | O1, +1,800, −6,100 | A gentle beach scored with flipper tracks. Fenced Saltkin egg pits, the posts carved with turtles. At night Driftbacks haul out to dig. Spinecrab burrows ring the pits | Driftback ambience; easy offer-kelp tames at night |
| 13 | **The Rip Beach** (hazard teacher) | O1, +1,820, −5,720 | A beach where a rip channel cuts the bar (the dark flat gap, foam running out). A driftwood post at the tide line holds a line of white shells strung **sideways, parallel to the shore**. A newcomer's sandal lies at the foot of the post | Teaches the rip rule (2.8) without words |
| 14 | **The Driftwood Chapel** | O1, +1,500, −6,300 | A Saltkin chapel built of ship timbers. Votive model boats hang from the rafters, each with a tiny harpoon in its bow. The altar is a whale vertebra with a tide-worn bronze bowl of sea-glass | Sea-glass ×6; lore item "Votive boat" |
| 15 | **The Kelp Pits** | O1, +1,450, −5,900 | Stone-lined kelp-burning pits (the real Orkney kelp-ash trade). Black kelp-ash crust, rakes, cooled lumps of soda ash | Kelp ash ×6 (glass flux, KEPT use); a "Glassmaker's pit" recipe scrap |
| 16 | **The Shipwright's Slip** | O1, +1,660, −6,470 | A stone boat slip into the Gate Inlet with the ribs of a half-built Saltkin hull. Tidecoral fittings lie scattered, hardened white | Tidecoral ×3 (air-hardened); "Tidecoral fittings" recipe scrap (boat fittings, survival-crafting KEPT) |
| 17 | **The Whalebone Arch** | O3/O1, +1,250, −6,000 | Two whale jawbones, 9 m tall, set as a gate over the coast road (the real Whitby arch) | A waymark; road start |
| 18 | **Mangrove Mouth** | O1 south, +1,600, −5,460 | The Swamp border. Oyster reefs on mangrove roots, a sunken Coven punt, a Saltkin shell chime hung from a mangrove (neighbours touched) | Oysters (food, a pearl chance 5%) |

**Hand-made sea sites (9):**

| # | Site | Sub-zone, at | What's there | Pays |
|---|---|---|---|---|
| S1 | **The Sunken Market** | O5 reef flat, +2,520, −6,520, 2 to 5 m deep (dry at the King Tide) | Stone stall frames, a paved square, amphorae in rows, a fountain with a carved fish. The Drowned Hall's Low Gate stands at its east edge, sealed by sand except in the King Tide | Pottery, sea-glass, copper; the Low Gate (2.5) |
| S2 | **The Kelp Cathedral** | O4, +2,200, −7,000 | The tallest kelp grove (30 m), sunbeams in columns. At night the Wavecharger herd anchors here, tails wrapped on stipes, males' brood pouches swollen | **Wavecharger egg steal** (2.12) |
| S3 | **The Otter Barren** | O4, +2,100, −5,950 | A purple urchin barren in a kelp gap. An otter raft floats above, cracking urchins on their bellies | Pearldiver taming; urchins (food) |
| S4 | **The Mooring Stones** | O4 shallows, +2,000, −6,900, 1 to 2 m deep | A ring of 8 Saltkin bollard stones in the shallows, iron rings set in each. Old rope rotted in the rings | The Reef Tyrant's rest, and its counter (2.13.3) |
| S5 | **The Impaled Ship** | O5 crest, +2,800, −6,000 | A newcomer ship run up onto the reef crest, its bow 4 m out of the surf, its stern hanging over the reef slope. Tidecoils in the hull | Captain's chest: iron ×6, rope, canvas, lore item "Ship's log, last page" |
| S6 | **The Sunwatch Barque** | O5 slope, +2,950, −7,420, 18 m | A Sunwatch sun-barque (cedar, high curved prow) sunk on the reef slope, its sun-disc prow coral-crusted. The war came by sea too | Gold leaf, bronze, sunstone ×1 |
| S7 | **The Clam Garden** | O5, +2,420, −7,560, 6 m | Forty giant clams 1 to 2 m across, mantles glowing blue and green | Pearls (Interact on an open clam, 2%; Pearldiver ×5) |
| S8 | **The Whale Fall** | O5, on a ledge of the Blue Wall, +3,620, −6,880, 45 m deep | A whale skeleton on a ledge, furred with bone-worms, hagfish drifting through the ribs. Past 25 m, so Gills, a helm, Reefmail 4 or a Wavecharger. A newcomer diver's helmet lies beside it | Bone, a sealed chest (steel plate ×2, a gem, upgrade shards ×1); the Reef Tyrant's route passes it |
| S9 | **The Coral Colossus Hand** | O5, +2,700, −6,650, 10 m | A Sunwatch colossus's severed stone hand, 8 m long, on the reef. It belongs to a second statue that fell into the sea. Tidecoral grows thickest on it | The richest tidecoral field (20 nodes) |

**Kit POIs (the generator places 33 on land and 18 at sea; world-map density rules):**

| Kit piece | Sub-zones | Variants | About |
|---|---|---|---|
| Saltkin shell-chime beacon (driftwood pole, shell chime, phosphor paint; waymark KEPT) | O1, O2, O3, 120 to 200 m apart on the coast road | 3 | 9 |
| Newcomer castaway hull (a skiff or boat broken on the sand, a cache) | O1, O2 | 3 | 4 |
| Tide-pool cluster (6 to 10 pools with urchins, anemones, crabs, a Tidecoil pool) | O2 | 3 | 4 |
| Blowhole field (3 to 6 vents) | O2 | 2 | 3 |
| Saltkin fish weir (stone tidal trap) | O1, O2 | 2 | 3 |
| Salt-pan terrace with a stone hut | O3 | 2 | 3 |
| Tsunami-stone outlier (a single stone, older and lower, below the line) | O3 | 2 | 2 |
| Kill set-piece (Reefjaw stranded in a pool on a Kelpgrazer; Gullraiders mobbing a Tidefowl colony) | O1, O2 | n/a | 3 |
| Driftwood shelter (a newcomer lean-to with a fire ring) | O1 | 2 | 2 |
| Coral head (tidecoral nodes 6 to 10) | O4, O5 | 3 | 5 |
| Saltkin drowned house (roof ridge in the kelp, a cache inside) | O4 | 3 | 3 |
| Man o' war fleet (drifting hazard, 2.8) | O5 | n/a | 2 |
| Newcomer anchor and chain (a cache in a sunk barrel) | O4, O5 | 2 | 3 |
| Sea stack (a climbable stack with a Tidefowl colony, a vista) | O4 | 3 | 3 |
| Shark cleaning station (a coral bommie where Reefjaws circle slowly; real cleaner-fish behaviour) | O5 | 2 | 2 |

### 2.5 Hidden sub-biome: the Drowned Hall (RAVEN-PLANNED "hidden sub-biomes"; FULL-GAME name kept; layout GAP-COOKED)

**What it is.** The great hall of the Saltkin city that stood on the reef flat behind the Long Dyke, now under the reef crest at (+2,600, −6,500). The sea took it when the sluice opened. The hall's dome was cemented shut by tidecoral, and the tidecoral trapped a huge air pocket. It runs 6 to 22 m down, with about 0.5 km of rooms. 8 POIs.

**Underground rule (world-map later-cooks):** `WRGameMode`'s underground teleport must exempt the Drowned Hall volume (W3 fix). No claims (DD: no underground bases).

**Ways in:**
1. **Dive the Bell Tower.** From the tower top at low tide, swim down the stairwell, 18 m. Clean breath needs 18 s at base swim speed, so a base 30 s breath makes it with a little to spare. Through a window into the Bell Chamber air pocket.
2. **The Low Gate**, at the Sunken Market's east edge. Sealed by sand except during the King Tide (2.10), when the sea draws back and the gate stands dry: walk in.
3. **The Pearl Cave crawl.** A tamed Pearldiver finds a collapsed culvert at the back of the Pearl Cave (2.7) and clears it in 20 s. It's a 40 m flooded tunnel (breath: 25 s at base speed).
4. **The Sluice Door** (from inside only). A one-way door out into the Long Dyke's seventh tower stump: the shortcut back to the dyke wall top.

**Rooms (in order from the Bell Tower):**

| Room | What's there |
|---|---|
| **The Bell Chamber** | A bronze bell 3 m tall hanging in an air pocket at the top of the tower. Its clapper is wrapped in rope. Ringing it (Interact) sounds across the whole reef. Every Tidecoil in the Hall wakes and comes, so it's a farming spot at your own risk. At midnight high tide it rings by itself from the swell (the secret, 2.20) |
| **The Air Dome** | The great hall. A coral-cemented dome 40 m across holding a lake of trapped air over waist-deep water. Glowray light comes through gaps in the coral. Long tables, fallen chandeliers of shell, a raised dais with an empty throne. The trapped air is safe: breath refills here |
| **The King's Tower** | A stair up to a dry room. A bed, a skeleton on it in a gold circlet, with one hand at its own throat, where a neck chain is broken. The key that hung there is gone. Lore item "Broken key-chain" |
| **The Sluice Works** | A flooded machine hall of bronze gears and chains, 12 m down (breath needed). The great sluice lever stands **open**. The **key is still in the lock**. A woman's skeleton sits at the lever wearing Saltkin pearls and **a Sunwatch gold sun-disc bracelet**. Taking the key (Interact 3 s) gives the **Sluice key** (secret O-S2) |
| **The Pearl Beds** | A flooded courtyard of giant clams, all open. Pearls ×1 to 3 per visit (Pearldiver ×2). 3 Tidecoils guard it |
| **The Tidecoil Warren** | Collapsed houses full of eel holes. 6 Tidecoils. Iron ×3 nodes in a fallen wall. Ore from the drowned smithy |
| **The Archive** | A dry loft above the Dome. Shelves of clay tide-tablets. One readable lore item "Tablet of the Pound" (a recipe: **Sun mirror**, 2.19). Saltkin harpoon heads in rows, each tagged with a date notch |
| **The Low Gate Hall** | A paved ramp up to the sand-sealed Low Gate. In the King Tide the gate stands open to the dry Sunken Market. Otherwise it's a dead end with a sand wall |

**Light:** dim blue Glowray light in the Dome and dark everywhere else. Bring a lantern (it goes out underwater; KEPT), a tamed Glowray (12 m aura, KEPT), or a Nightsight brew (KEPT).

**Music:** the "Drowned Hall" cue (2.16).

### 2.6 Underwater play (R6 shared system; GAP-COOKED; numbers KEPT where survival-crafting set them)

Nothing here exists in code yet. This is the frame every R6 water feature stands on, and later biomes reuse it: flooded grottos (Spore), cistern catacombs (Graveyard).

**Movement**

| Item | Value | Tag |
|---|---|---|
| Surface swim | Built `water.swimSpeed`; surface strokes and treading | SPEC (BUILT) |
| **Dive** | Pad B hold or keyboard LCtrl hold: pitch down and dive. A tap of A or Space rises; holding it surfaces fast. Underwater, the camera direction steers (swim where you look, with forward) | GAP-COOKED |
| Underwater speed | 300 cm/s, fast swim 450 cm/s (6 stamina a second, KEPT). Gills ×1.25. Swim fins +25%. Wavecharger 1,100 (KEPT) | KEPT (stamina) / GAP-COOKED |
| Kick-dodge (replaces the roll underwater) | A 400 cm burst in the input direction (3D) over 0.3 s, i-frames 0.20 s, stamina 14. Perfect-dodge rules as on land | GAP-COOKED |
| Wading | Water 60 to 140 cm deep: run ×0.85, no sprint over 110 cm. Splashes on every step | GAP-COOKED |
| Currents | `AWRCurrentVolume` (world-map KEPT) pushes swimmers: rips 250 cm/s, races 500, the Sluiceway on the flood tide 150 | KEPT / GAP-COOKED |

**Breath and depth (KEPT from survival-crafting 2.6, built here)**

| Item | Value |
|---|---|
| Breath | 30 s base, +0.6 s per Swimming level (90 s at 100), Diver perk ×1.5. Refills in 3 s at the surface or in any air pocket |
| Out of breath | 10 damage a second until you surface. No floor: you drown |
| Crushing depth | Below 25 m: 5 damage a second. Pressure perk pushes it to 40 m; Reefmail 4 or the diving helm to 40 m; Abyss diver removes it. **Gills: no breath limit, and crush starts at 80 m, deeper than anywhere in the Ocean** (2.17) |
| Swim stamina | 6 a second fast, 0 floating; Strong stroke −30%; Seaborn 0 |
| Breath read | A thin sea-green arc around the crosshair, shown only underwater. At 25% the arc pulses, a heartbeat comes up in the mix, and bubbles stream from the mouth. At 0 the screen edges darken with each damage tick |
| Depth read | Depth in metres next to the arc once below 10 m. It turns coral orange past your crush limit |

**Combat underwater (MH Tri lesson: lean and readable)**

| Item | Rule |
|---|---|
| Melee | Thrust weapons (spear, dagger, harpoon, fists) at full speed. Every other class swings at play rate ×0.8 (water drag), unless the player has Gills. No charged heavies with greatswords, great hammers or axes. No jump or rolling attacks (the kick-dodge attack is a lunge: thrust weapons only) |
| Guard and parry | Work as on land. Guard stamina +20% (no footing) |
| Ranged | Bows can't fire underwater. Crossbows fire at 50% range. Thrown spears and harpoons work at full range |
| Spells | Bolt, Lance and Pull shapes work. Bolts travel ×0.6 speed, except Tide spells (×1.0). **Fire spells fizzle**: ×0.3 damage, no burn. **Lightning in water spreads:** a 400 cm sphere that shocks everyone in the same water, caster included, unless Grounded |
| Lock-on | Works in 3D. The camera arm shortens to 250 cm; the springarm ignores kelp and soft props |
| Wildbrands | Only those flagged `underwater`: Wave cut, Harpoon throw, every thrust Wildbrand, Static strike (it spreads as above) |
| Tells | Every underwater attack has a bubble trail or wake plus a sound tell (combat swimmer archetype KEPT). Nothing attacks from directly below without a rising bubble column of at least 1.0 s |
| Who fights underwater | Only Reefjaw, Tidecoil, Wavecharger, Glowray, the Siren Eel and the Reef Tyrant attack fully underwater. Every boss keeps players mostly at the surface or on perches |

**Surface fighting from water.** A swimmer at the surface can use one-handed light attacks and thrust heavies, guard and parry. There's no kick-dodge at the surface: the dodge is a short surface roll, 250 cm. Boats are their own lane (world-map KEPT).

### 2.7 Caves and resources

**Caves (CF rule: every biome has caves; deeper is better).** The coast's caves are sea caves cut by waves into the coral limestone.

| Cave | At | Depth | Ore | Creatures | What's special |
|---|---|---|---|---|---|
| **Blowhole Grotto** | O2, +1,600, −7,720 (under the Spouting Horn) | 8 m | Copper, tin | Tidecoil 2, Spinecrab 3 | Dry at low tide, flooded at high. The Horn's thunder fills it. The newcomers' first cave |
| **Saltkin Mine** | O3, +500, −7,600 (in the Spine's foot) | 30 m, dry | Copper, iron ×3 | Spinecrab 3 (cave-pale) | The only dry deep ore on the coast. Saltkin cart rails and a tide-tablet on the wall |
| **Pearl Cave** | O4, +2,050, −6,200, entrance 6 m down | 14 m | Tin, a gem chance | Pearldiver 4, Tidecoil 2 | Half-flooded, with an air pocket. Giant clams. The Drowned Hall culvert (way in 3) |
| **Siren's Hollow** | O2/O4, +1,950, −7,700, shore mouth at the cliff foot plus a sea entrance 10 m down | 20 m | Iron ×2 | The Siren Eel | Mini arena (2.13.2). Dry at the King Tide |
| **Lazarus Deep** | O5, in the Blue Wall at +3,700, −7,500, 52 m down | 60 m | Iron ×4, gems ×2 | Reefjaw 2, Tidecoil 4 | **The deepest sea cave: the Lazarus Fish (KEPT, survival-crafting)** in a black pool at the back. Past crush depth for anyone without Gills, Abyss diver or a 40 m kit plus speed. The best iron in the biome |

**Resources (GAP-COOKED; tidecoral name KEPT)**

| Resource | Where | Gather | Uses |
|---|---|---|---|
| **Tidecoral** (signature) | Living coral on reef heads (O5), the Colossus Hand, coral heads (O4 deep edge), the reef crest. Never farmable (survival-crafting KEPT) | Pick, 3 hits, **underwater**. A node gives 2. It's soft and orange alive; it hardens white within 10 s in air (it "dies into stone"). Regrows in 3 game days | Reefmail, diving helm, boat fittings, Rune of the Tide, soak-resist brew, longboat (all KEPT); Sun mirror frame; Coral keep pieces (KEPT) |
| **Pearls** | Giant clams (S7, the Pearl Beds, the Pearl Cave), oysters (2%, 5% at Mangrove Mouth), Pearldiver finds | Interact on an open clam | Pearl ring and gems (a pearl is a gem: +5% max Flow when socketed); Coral keep pearl lamp (KEPT); trade |
| **Kelp** | O4 everywhere, kelp lines (KEPT) | Hand or knife | Food, Driftback and Kelpgrazer feed, kelp ash (KEPT) |
| **Sea grapes** | Crop (KEPT) and wild on reef flats | Hand | Deepbreath brew, food (KEPT) |
| **Salt** | Salt Pans (O3), salt pans on any beach (KEPT) | Hand | Preservation (KEPT), Diver's draught |
| **Sea-glass** | Beaches, the Driftwood Chapel | Hand | Reefmail beads, lanterns, glass flux substitute |
| **Sharkskin, shark teeth** | Reefjaw | Butcher | Reefmail underlayer (hero KEPT "over sharkskin"); shark-tooth arrows; sandpaper grip on harpoons |
| **Spinecrab shell** | Spinecrab | Butcher | Reefmail (KEPT); shell shield |
| **Ambergris** | Thalassor drop ×1 to 3; washes up after a Squall (1 in 6 wreck tides) | Hand | Alchemy fixative: any brew made with it lasts ×1.5 |
| **Driftwood** | O1, O2 | Hand | Wood (counts as wood); beacons |
| **Oysters, urchins, fish** | Mangrove Mouth, barrens, every coast (KEPT fish list) | Hand, rod, spear | Food |
| Iron, copper, tin | Caves, wrecks (nails), the Saltkin Mine | Pick, salvage | Metal |

### 2.8 Hazards

**Deep water (SPEC 3.3 numbers, KEPT; tells GAP-COOKED)**

| Item | Value |
|---|---|
| Effect | Breath 30 s, then 10 damage a second; below 25 m, 5 damage a second (KEPT) |
| Passes | Gills (root, main only); diving helm (KEPT); Reefmail 4 (KEPT); swimming mount (rider still uses their own breath on a Wavecharger, KEPT); **Diver's draught** (breath 90 s, KEPT world-map) |
| Where | The reef slope past 25 m, the Blue Wall, Lazarus Deep, the Whale Fall, the Drowned Hall's flooded rooms, open sea |
| Tell | Water colour (2.1), the depth readout past 10 m, a rising pressure hum under the music past 20 m, and ear-pop crackles at 25 m |
| Creatures | Every Ocean creature ignores breath and depth (CR rule) |

**The tide (GAP-COOKED; the biome's clock).**
- **The cycle.** Two highs and two lows each game day: high at 00:00 and 12:00, low at 06:00 and 18:00. That puts slack water at the lows, matching world-map's race slack at 05:30 to 06:30 and 17:30 to 18:30.
- **The range.** 2.4 m: water level ±1.2 m from mean, on a sine curve.
- **What it changes:**
  - Low tide exposes the O2 shelf pools, the Blowhole Grotto, the dyke wall top (wadeable) and the Bell Tower's top.
  - High tide floods the shelf to 0.5 m, wakes the blowholes and drowns the dyke wall.
- **Scope.** The tide is local to the B2 east-coast water, blended to mean level over 300 m at the biome's edges. Other coasts keep the still sea, so built spawn camps never flood (logged to LATER-COOKS for world-map).

**Micro-hazards (GAP-COOKED; 3; each with at least 0.9 s of tell; each can kill)**

| Hazard | Where | Tell | Effect | Answer |
|---|---|---|---|---|
| **Rip channels** | O1 beaches, 4 to 6 channels at bar gaps; strongest on the falling tide | A dark, flat gap in the breakers. Foam and kelp moving steadily seaward. A hiss of outgoing water. The Rip Beach post teaches it (POI 13) | A 250 cm/s seaward current, 8 m wide, running 120 m out. Swimming straight against it costs ×2 swim stamina and makes no headway; out of stamina you can't fast-swim and you drift past the bar into Reefjaw water | **Swim sideways** (parallel to shore) for 8 m and you're out. A swimming mount ignores it. Gills players still feel it (it's water, not breath) |
| **Blowholes and surge channels** | O2 shelf, Blowhole fields, the Spouting Horn; live from mid to high tide | A rising boom and hiss from the vent, spray mist puffing from it, rock shaking underfoot: 1.0 s, then the column | A 6 m column (40 m at the Horn): 40 damage and a launch 6 m up (fall damage applies; 18 m at the Horn). Surge channels: a wave runs up the channel at 600 cm/s, 30 damage plus a knockdown and a drag 6 m seaward | **Also a tool.** Stand on a vent on purpose (no damage if you jump just before the column) and it launches you onto ledges (TotK geysers): marked by worn Saltkin footholds. It's the route to secret O-S3 |
| **Man o' war fleets** | O5 surface and the open sea; drifting with the wind; Squalls blow them into O4 and onto O1 (stranded) | Blue-violet sails 30 cm tall bobbing on the surface in groups of 6 to 20. Purple tentacle shimmer trailing 8 m below. A faint glassy tick when you're within 6 m | Touching a tentacle: 25 damage plus poison buildup 50 (poison status from R3, KEPT forest change). Several touches stack. Stranded ones on the beach still sting (real) | Swim under at more than 8 m, or around. Fire on the surface pops a sail (the fleet drifts apart). Reefmail 2 and swimming mounts take ×0.5 |

### 2.9 Weather (weights KEPT from world-map; Squall reconciled)

world-map has Clear 40, Overcast 15, Rain 15, Sea fog 15, **Squall 15** with "lightning flashes with no strikes". survival-crafting has "Squall 30%, strikes on the tallest mast every 40 s".

**Reconciled:** the map's weights stand. There are no strikes on land or within 600 m of shore. **On open sea, the tallest mast takes a strike every 40 s** (survival-crafting's rule), with a 1.0 s glow on the mast tip and 60 damage. A grounding charm on the mast stops it (KEPT). Logged to LATER-COOKS so survival-crafting matches.

| State | Weight | Effect |
|---|---|---|
| Clear | 40 | Visibility 40 m underwater in the shallows; the turquoise read is strongest |
| Overcast | 15 | Underwater visibility ×0.8. **Sun mirrors work at half strength** (2.14) |
| Rain | 15 | Wet (world-map KEPT). The sea surface pocks, so surface tells are harder to read (V-wakes ×0.7 visibility). Fish bite 25% sooner |
| Sea fog | 15 | Sight 60 to 80 m (world-map Fog). **Gullraiders hunt in it** (snatch from 2× range). Underwater visibility ×0.5. The Beacon Stack, once lit, cuts through it (300 m) |
| **Squall** (signature) | 15 | Rain plus wind 20 m/s, swells 2 to 3 m (rafts capsize), lightning flashes. Surface swimming costs ×1.5 stamina (KEPT). Man o' war fleets blow inshore. Reefjaws at sea ×2. **Wreck tide after** (world-map KEPT), with ambergris on 1 in 6 |

### 2.10 Event: the King Tide (GAP-COOKED; frame KEPT from fire-frost 3.1)

**Real root.** A perigean spring tide, the highest and lowest of the year, when sun and moon line up near perigee. Here it's the sea drawing breath before it takes back what was built on it.

| Part | Detail |
|---|---|
| Frequency | One game day (60 real minutes), about once every 3 real days (the server's Biome events setting, KEPT) |
| Warning (5 min) | The sea visibly draws back off the shelf and the strand. Every shell-chime beacon on the coast rings at once with no wind. Gullraider and Tidefowl flocks lift inland. **The drowned bell rings under the reef.** The **Saltkin pillar on the Crown's Ring burns sea-green** (world-map 2.12 rule) |
| The Low (the first 50 real min) | Sea level −6 m in the biome (blended to 0 at the B2 edges). O4's inner shallows become a tide flat of wet sand and flattened kelp. The Long Dyke stands clear, so you can walk it to the reef. The reef crest is dry, so you can mine tidecoral on foot. The Sunken Market is dry and the Drowned Hall's Low Gate opens. Siren's Hollow can be walked into. Every wreck in under 6 m of water can be walked |
| Creatures in the Low | **Stranded:** Kelpgrazers and Driftbacks lie on the flat; offer-food taming on a stranded creature runs ×3 faster (you're rescuing it). **Pool-bound Reefjaws:** 4 to 6 are trapped in deep tide pools on the flat; they take ×1.3 damage but surf-lunge from the pool edge at anyone within 6 m. **Old Barnacle** leaves his cove and roams the flats gathering stranded shells. Spinecrabs ×3 on the flats; Gullraider flocks feast |
| The spawning night | If the King Tide falls at night (20:00 to 06:00), the reef spawns. Orange-pink glowing clouds rise off the coral, every tidecoral node regrows at once, and Glowrays gather in rings (taming ×2 speed) |
| **The Return** (the last 10 real min) | At T−10 min a white line of breaking water forms 1.5 km out, and a roar builds. At T−3 min the sea comes back as a **3 m tidal bore** moving inland at 600 cm/s. It runs up the beaches and the terraces **to the Tsunami Line and no further**. Anyone below the line when the bore passes takes 80 damage, is carried 20 m and soaked, and is dumped in moving water. It can drown you, and it can stack with the Pool Reefjaws. **The line of stones is the safe line.** No floors |
| Pays | Tidecoral ×3 everywhere (3.3 SPEC); pearls ×2; wreck salvage ×2 |
| Never | Spawn camps sit on dunes above the line, so the bore never enters a camp safe zone (KEPT rule). Bases below the line take water damage like any piece in the bore's path (structures: 200 damage per piece, once). Building below the stones is the player's choice. That's the tsunami-stone lesson, told by the land |

### 2.11 Shrines and the path master

**Shrines (rules KEPT from fire-frost 3.1: 5 per biome, guarded, 3 s light, path XP, fast travel; world-map: one lesser shrine at each pass mouth counts as one of the 5)**

| Shrine | At | Guard (sub-zone pack) | Look |
|---|---|---|---|
| **Great Shrine: the Mirror Pool** | O2, +1,620, −7,060 (beside the Tide Clock) | none (the path master is near) | A round Saltkin tide pool 12 m across, carved into a basin. Its brazier is a bronze bowl on a stone post standing in the water: a whale-oil flame that burns sea-green on the water's mirror |
| Driftwood Shrine | O1, +1,540, −6,310 (beside the Driftwood Chapel) | Spinecrab pair and Gullraiders | A driftwood brazier hung with shell chimes |
| Gate Shrine (pass mouth) | O3, +960, −6,620 (Saltgate east mouth) | Spinecrabs and, from R5, the Desert's Glasstail and Dunejaw at the mouth (pass rule: mixed creatures) | A Saltkin brazier inside a ring of bollards on the salt flat |
| Kelp Stack Shrine (sea) | O4, +2,150, −6,750, on top of a sea stack you climb out of the water to | Reefjaw (midland young, 120 HP) circling the stack, and Tidecoils at its foot | A brazier in a cleft on the stack top, gulls all round |
| Lighthouse Shrine (sea) | O5, +2,700, −7,200 (Reef Lighthouse) | Tidecoils in the tower, a Reefjaw in the surf | A brazier in the lighthouse's broken lamp room, ladles clinking |

**Path master: Hae-won, the Harpooner (GAP-COOKED; DD "a path master NPC per biome who teaches the path through quests"; replaces FULL-GAME's "old harpooner").**

| Part | Detail |
|---|---|
| Who | A newcomer who washed ashore years before the player. She was a freediver at home, a haenyeo-style sea woman, and she taught herself the Saltkin harpoon from Harpooners' Hall (world-map "the newcomers" rule). Late 30s. Lean, strong freediver's build with broad swimmer's shoulders. Sun-browned skin, salt-bleached tips on long black hair worn in a wet braid wound with fishing line and shark teeth. Dark eyes with a sea-green ring, and fine gill lines at the neck, her own Tide change at 75 (2.18). A thin pale Reefjaw scar runs from hip to knee. She wears a fitted sleeveless sharkskin dive suit cut high at the hip and open at the collar, a short wrap skirt of sailcloth knotted at one hip, a dive knife strapped to her thigh, and a Saltkin harpoon on her back. Warm, loud laugh, blunt, fearless |
| Where | A stilt house over the Mirror Pool (Great Shrine). She dives the Kelp Cathedral at dawn, mends lines at the pool by day, and sits on her porch at night with a lamp |
| How she talks | Short spoken lines within 6 m, with no camera grab (KEPT rule). Quests go to the Journal |
| Teaches | Anyone. Her last quest needs the third mini (the Reef Tyrant) |

**Her 6 quests (GAP-COOKED; schema in `quests.json` as fire-frost):**

| # | Quest | Task | Reward |
|---|---|---|---|
| 1 | **First breath** | Dive to 15 m and touch the Kelp Cathedral's anchor stone, then surface | 1,000 Tide XP; Diver's draught recipe |
| 2 | **Sideways** | Get caught in a rip channel and swim out of it sideways (the game checks that you left it by its side) | 1,500 Tide XP; swim fins recipe |
| 3 | **A line on the reef** | Harpoon 5 fish or creatures underwater (any harpoon or thrown spear) | 2,000 Tide XP; Whaler's harpoon recipe |
| 4 | **Coral for the helm** | Mine 8 tidecoral on the reef and craft a diving helm | 2,500 Tide XP; shark-tooth arrow recipe |
| 5 | **The stones say** | Stand above the Tsunami Line during a King Tide's Return, or read all 9 stones (Interact) | 3,000 Tide XP; the Sun mirror recipe (if not found in the Archive) |
| 6 | **Tie the Tyrant** | Beat the Reef Tyrant after tethering it to a Mooring Stone at least once | 5,000 Tide XP; "Hae-won's knife" (dagger, tidecoral edge: soak 20 per hit, full speed underwater for anyone) |

### 2.12 Creatures (names, roles and taming KEPT from FULL-GAME's list; everything else GAP-COOKED)

**Roster: 11 native.**
- FULL-GAME listed Floeback here too, but Raven puts it in the tundra (CR SPEC: "carries a rider through icy water") and world-map has it in the Rime Sound.
- **Floeback stays a tundra spawn.** R6 builds its swimming-mount code alongside the Wavecharger. Logged to LATER-COOKS for the FULL-GAME merge.
- The 11 are 4 aggressive, 4 neutral and 3 passive. Ripplefin is a school fish (cheap to build), which keeps the count close to the brief's "about 9" in build cost.

**Rules (CR SPEC):**
- Every Ocean creature ignores breath, depth, rips and blowholes.
- Neutral herds defend each other.
- Each creature tames only by its listed methods.
- Tiers follow 3.3: hard aggressive = highland, medium aggressive = midland, easy aggressive and passives = lowland, neutrals by difficulty.
- Every attack shows the real jaw, claw, club, barb or body, and damage lands only on visible contact.
- No attacker limit (combat 2.18.4).

#### 2.12.1 The eleven at a glance

| Creature | Tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Reefjaw** | Highland, 1 to 3 (packs on the open sea, world-map KEPT) | Tiger-shark body and stripes; goblin-shark slingshot jaw that shoots forward out of the head; dermal denticles; a scythe tail | A 6 m torpedo with a long flat blade of a snout. When it bites, the whole jaw unhinges and shoots out ahead of the head | Bronze-grey back #5E5A4E with faded tiger bars, bone-white belly, pale pink gums, black eye | 6 m | Knockout and feed, or tide path magic. Hard | Raw fish ×6, sharkskin ×3, shark teeth ×6 |
| **Tidecoil** | Midland, 1 to 2 (6 in the Warren) | Moray body and gaping "breathing" mouth; the pharyngeal second jaw; an electric eel's banded organs that light when it discharges | A 3.5 m muscular ribbon with a fat gaping head poking out of a hole | Mottled olive and yellow #6A6A2E / #C8B860 (honeycomb moray pattern), bands that light blue-white | 3.5 m | Knockout and feed. Medium | Raw fish ×3, eel skin ×2 (waterproofing), shock gland ×1 |
| **Spinecrab** | Midland, 1 to 3 | Coconut-crab crusher claw; king-crab spined carapace; mantis-shrimp club on the second claw; a 3 m leg span | A spiky dome on stilt legs with one huge crusher and one cocked club, eyestalks up | Carapace rust-red #9A3E2A with cream spines, club enamel blue-green #3E8C8A | 3 m leg span, 1.2 m tall | Knockout and feed. Medium | Spinecrab shell ×2, crab meat ×4 |
| **Gullraider** | Lowland, flock of 3 to 6 | Great skua's thug body; frigatebird's hooked bill and throat pouch | A dark, broad-winged gull with a hooked bill and a balloon throat | Dark brown #4A3A2E, white wing flashes, pouch scarlet-orange #D8502A (not crimson), black legs | 2 m span | Knockout and feed (bring it down). Easy | Feathers ×3, raw meat ×1, whatever it stole |
| **Wavecharger** | Midland neutral, herd 3 to 5 | Seahorse head and armoured ring-plates; leafy-seadragon fronds; a dolphin's driving tail stock with a seahorse's curl at the tip | An upright horse-head on a ridged, frond-trailing body. It swims fast and curls its tail round kelp to rest | Coral orange #E07A4F to gold, olive-gold fronds, black eyes | 4.5 m long | Steal an egg. Medium | Raw fish ×4, frond ×2 |
| **Driftback** | Midland neutral, 1 to 2 | Leatherback turtle's ridged leathery shell at 5 m; a barnacle-and-weed garden on its back | A huge ridged dome with long front flippers and a small beaked head | Slate-black shell with white spots, barnacle cream, weed green | 5 m shell | Offer kelp. Medium | Raw meat ×6, turtle hide ×3 |
| **Glowray** | Lowland neutral, ring of 3 to 8 at night | Manta's wings and cephalic fins; a stingray's tail barb; firefly-squid light organs in patterns on its back | A 4 m wing-shape gliding, two curled horns at the front | Midnight blue #1A2A4A with blue-green light spots #5AE0C8, white belly | 4 m span | Offer fish. Easy | Ray wing ×2, photophore gland ×1 |
| **Pearldiver** | Lowland neutral, raft of 2 to 6 | Sea otter with a stone in its armpit pouch | A sleek otter floating on its back cracking shells | Dark chocolate #4A3426, frosted-silver head | 1.4 m | Offer fish. Easy | Pelt ×1 |
| **Kelpgrazer** | Lowland passive, pod of 2 to 4 | Steller's sea cow (the real 8 m kelp-eater, hunted out within 27 years of its discovery); bark-like hide; a forked tail | A vast barrel with a whiskered snout and a small head | Bark-grey #6A665E, pale scars | 7 m | Offer kelp. Easy | Raw meat ×10, fat ×4, thick hide ×3; milk (farm, KEPT) |
| **Tidefowl** | Lowland passive, colony | Puffin's bill on an eider's heavy body | A round black-and-white seabird with a striped orange bill | Black and white; bill orange, blue and yellow | 0.6 m | Offer food. Easy | Eggs (farm, KEPT), feathers ×2, meat ×1 |
| **Ripplefin** | Lowland passive, school of 20 to 60 | Mackerel body with a flying fish's long ripple fins | A silver flicker in a moving cloud | Silver-blue with green tiger lines | 0.6 m | Offer bait (KEPT) | Raw fish ×1 (sea pen, KEPT) |

**Path magic (Reefjaw, tide path; GAP-COOKED in Raven's "calm a creature of the same element" frame).**
- **When.** On a Reefjaw that's knocked out, or below 30% HP and soaked.
- **How.** Stand in water with 10+ Tide points and hold Interact for 4 s. You stroke its snout as water swirls round both of you.
- **Cost.** 40 Flow.
- **Result.** It counts as a full narcotic meal, and taming continues as knockout-and-feed.

**Wavecharger egg (steal an egg, CR SPEC; built on the real male brood pouch).**
- **Where and when.** At night the herd sleeps at the Kelp Cathedral, tails curled on kelp stipes. The males' brood pouches are swollen.
- **The steal.** Sneak up and hold Interact for 3 s on a sleeping male. If anything in the herd wakes (a sneak check using the Swimming and Sneaking skills), the herd turns hostile together (herd rule).
- **The egg.** It goes in a sea pen or a kelp line in salt water, and hatches in 20 real minutes.

**Utilities:**
- **Wavecharger (swimming mount, KEPT numbers).** 1,100 cm/s underwater, dives to 40 m, and the rider uses their own breath. Surface sprint 1,300 cm/s. Rider attacks: thrust weapons and harpoon only.
- **Driftback (swimming hauler, KEPT).** Surface 600 cm/s, carries 300 kg. A rider can stand on its back as a fighting platform. It hauls out onto beaches.
- **Glowray.** 12 m light aura (KEPT). Underwater it also shows the bubble trails of anything hostile within 20 m as glowing lines.
- **Pearldiver.** Finds pearls (clams ×5 within 20 m), sunken caches (a ping on the HUD compass within 40 m) and the Drowned Hall culvert. It cracks clams on its belly with its stone.

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55.

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Reefjaw** (high) | **Slingshot bite** | 0.70 (gill slits flare, the jaw drops and unhinges, white glint on the teeth) / 0.3 / 0.15 / 0.90 | 1.4 | 45 (4) | parryable; bleed 25; reach +120 cm (the jaw shoots out) | 40% → Thrash-drag if it hits |
| | Ram | 0.60 (it backs off 3 m, tail beats fast, a pressure thrum) / 0 / 0.25 / 0.80 | 1.1 | 40 (4) | knockback 500 | Underwater and at the surface |
| | **Thrash-drag** | 0.95 (jaws gape, the head shakes side to side, crimson glint in the mouth) / 0.2 / 0.25 catch | 0.6 catch + 15/s for 2 s | 60 (6) | `grab`, `unblockable`; dodge only. Drags you 6 m down; no breath regen while held | An ally hitting the gills for 100 frees you. Mash: −0.15 s per press |
| | **Breach strike** | 1.00 (a bubble column rises under the target, low thrum building) / H 0.4 / 0.30 / 1.20 | 1.6 | 55 (5) | From below, only at a surface swimmer or a boat's side (hull 300 damage) | The column is the tell; it never comes from below without one |
| | **Surf lunge** | 0.90 (a wave lifts it, its fin cuts the surf line toward shore) / 0 / 0.30 / 1.50 stranded | 1.5 | 50 (5) | At targets on wet sand within 6 m of the waterline (orca stranding). It's stranded 1.5 s after (belly lock point) | Only from the surf or a pool edge |
| | Tail scythe | 0.50 / 0 / 0.20 / 0.70 | 1.0 | 35 (3) | 200° behind | — |
| | readsAction | Slingshot bite with no hold within 0.4 s of a drink or a charged heavy | | | | |
| **Tidecoil** (mid) | **Hole strike** (opener) | 0.90 (its head pulses out of its hole, mouth gaping in rhythm, then still for a beat) / 0 / 0.20 / 1.00 | 1.3 | 35 (3) | from a crevice within 4 m; parryable; bleed 20 | It pulls back into the hole after; only its head is a target there |
| | **Second jaw** | 0.80 (the outer jaw opens wide and a white inner jaw shows in the throat) / 0 / 0.20 catch | 0.5 catch + 12/s for 2 s | 30 (3) | `grab`, blockable at full guard (the block knocks it off, guard stamina ×1.5); drags you toward its hole | An ally hit frees you; fire or a shock on it frees you |
| | **Shock pulse** | 1.00 (blue-white bands race down its body, a rising crackle) / 0 / 0.30 / 1.00 | 0.8 | 30 (3) | Shock 50 (status R1). Radius 400 in water, 200 out of water. Hits everything in that water, other creatures too. Grounded immune | Soaked targets take shock buildup +50% (KEPT soak rule) |
| | Coil whip | 0.50 / 0 / 0.20 / 0.70 | 1.0 | 30 (3) | — | — |
| | Pool slither | — | — | — | At low tide it crosses wet rock between pools at 200 cm/s (snowflake moray) | It can chase you across the shelf |
| **Spinecrab** (mid) | Crusher pinch | 0.55 / 0.2 / 0.15 / 0.80 | 1.1 | 35 (3) | parryable | 50% → Club snap |
| | **Club snap** | 0.80 (the club claw cocks back with a ratchet click, white glint at the club) / 0 / 0.08 / 0.90 | 1.2 + pop 0.4 at +0.06 s | 40 (4) | Two hits: the strike, then a **visible white cavitation pop**. A block takes both. Underwater the pop also hits within 150 cm | — |
| | Shell guard | 0.30 (claws fold over its face) / — / 3 s / — | — | — | Frontal damage ×0.3 for 3 s; flanks and back normal. Breaks at 60 stance damage: it flips onto its back for 2 s, belly ×1.3 | Used when you charge a heavy at it |
| | **Sand burst** (opener) | 0.90 (eyestalks poke up out of the sand and swivel, sand trickles in a ring) / 0 / 0.25 / 1.00 | 1.4 | 40 (4) | From buried, within 5 m | A buried crab is invisible except for its eyestalks |
| | Side charge | 0.70 (legs drum on the sand) / 0 / 0.8 run / 1.00 | 1.2 | 40 (4) | knockback 400; a rock or wall hit stuns it 2.5 s | — |
| **Gullraider** (low, flyer) | Dive peck | 0.75 (wings fold, a harsh laughing call) / 0.2 / 0.20 / 1.50 grounded | 1.0 | 15 (2) | parryable | Grounded 1.5 s = bring-down window (KEPT template) |
| | **Snatch** | 0.80 (it swoops low at you, talons out, throat pouch inflating) / 0 / 0.20 | 0.3 | 10 (1) | Steals one random **food or fish** stack (max 5 items; never gear or quest items) from the player's bag. A dodge or block stops it | It flies to the Gullery with the item in its bill, where you can see it. Kill it within 60 s and the item drops |
| | Mob call | 0.80 (pouch fully inflated, drumming) / 0 / — / 1.0 | — | — | Calls Gullraiders within 50 m (real skua mobbing) | Once per fight |
| | Wing buffet | 0.45 / 0 / 0.20 / 0.70 | 0.5 | 10 (1) | push 200 cm; it matters at a cliff edge | — |
| **Wavecharger** (mid, neutral) | **Snout snap** | 0.55 (the head tilts back on its neck, a click) / 0 / 0.10 / 0.80 | 1.1 | 35 (3) | parryable; pierce (the real seahorse pivot-strike) | — |
| | Tail lash | 0.60 / 0 / 0.20 / 0.80 | 1.0 | 30 (3) | — | — |
| | Charge swim | 0.85 (fronds flatten back, a rising whirr of fins) / 0 / 0.8 / 1.10 | 1.4 | 45 (4) | knockback 600 | — |
| | Herd click (rally) | 0.60 | — | — | The herd turns hostile together within 30 m | Plays the moment one is hit |
| **Driftback** (mid, neutral) | **Shell ram** | 0.90 (it draws its flippers back, head low) / 0 / 0.30 / 1.20 | 1.5 | 50 (5) | knockback 700 | — |
| | Flipper slam | 0.70 / 0 / 0.20 / 1.00 | 1.2 | 40 (4) | `low` (jump clears it) | On land or in shallows |
| | Beak bite | 0.60 / 0 / 0.15 / 0.90 | 1.3 | 35 (3) | parryable | — |
| **Glowray** (low, neutral) | **Barb sting** | 0.60 (the tail arches over its back, the barb glints) / 0 / 0.15 / 0.90 | 1.0 | 20 (2) | poison 30 (status from R3) | — |
| | Light flare | 0.70 (its spots ripple brighter, front to back) / 0 / 0.20 / flees | 0 | — | 4 m flash: blind 30 (status from R5) and a 1.5 s vignette | Then it glides away |
| | Wing slam | 0.50 / 0 / 0.20 / 0.70 | 0.8 | 15 (2) | push 300 | — |
| **Pearldiver** (low, neutral) | Bite | 0.45 / 0 / 0.12 / 0.80 | 0.8 | 10 (1) | parryable | — |
| | Stone smash | 0.60 (it lifts its stone two-handed) / 0 / 0.15 / 0.90 | 1.0 | 15 (2) | `strike` | Then it dives and flees |
| **Kelpgrazer** (low, passive) | Panic bump | 0.80 (it rolls, flukes up) / 0 / 0.30 / flees | 0.6 | 20 (2) | push 300 | Never attacks; flees |
| **Tidefowl** | Peck (cornered) | 0.45 / 0 / 0.12 / 1.0 | 0.3 | 5 (1) | — | Flees |
| **Ripplefin** | none | — | — | — | The school scatters and re-forms 10 m away (boids) | — |

**Physical resist:**
- Reefjaw: its denticles take slash ×0.85 and pierce ×1.1.
- Spinecrab: the shell takes strike ×1.2 and slash ×0.8; the flipped belly takes ×1.3.
- Driftback: the shell takes slash ×0.8.
- Tidecoil: strike ×0.9 (it's slippery).

**Lock points:**
- Reefjaw: head and gills.
- Spinecrab: carapace and club claw.
- Driftback: head and shell.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table)

| Sub-zone | Day | Night |
|---|---|---|
| O1 White Strand | Tidefowl 14, Spinecrab 8, Gullraider 8, Pearldiver 4 (surf), Kelpgrazer 2 (surf) | Spinecrab 14, Driftback 8 (nesting, Turtle Beach), Tidecoil 2 (rock pools), Reefjaw 2 (surf lunge at the rip channels) |
| O2 Blowhole Shelf | Gullraider 12, Tidefowl 10, Spinecrab 8, Tidecoil 6 (pools) | Tidecoil 12, Spinecrab 10, Glowray 4 (shallows) |
| O3 Salt Terraces | Spinecrab 8, Gullraider 8, Tidefowl 6, Reefjaw 4 (the Sluiceway only) | Spinecrab 12, Tidecoil 6 (the Sluiceway), Reefjaw 6 (the Sluiceway on the flood tide) |
| O4 Kelp Forest | Ripplefin 12, Kelpgrazer 10, Wavecharger 8, Pearldiver 8, Driftback 6, Reefjaw 3 (outer edge) | Reefjaw 8, Tidecoil 8, Wavecharger 6 (asleep at the Kelp Cathedral), Glowray 6 |
| O5 Reef and Drop | Ripplefin 14, Glowray 8, Tidecoil 8, Reefjaw 6, Driftback 4 | Reefjaw 12, Tidecoil 10, Glowray 10 |
| Open sea (world-map KEPT) | One Reefjaw pack (2 to 3) per km² | ×2 in Squalls and at night |

**Living-world beats (built AI, `WRWildlife`):**
- Reefjaws take Kelpgrazers at the kelp edge. A blood cloud in the water and Gullraiders diving on it show the kill from 300 m.
- Gullraiders mob Tidefowl colonies and steal their catch in the air (real skua food-theft).
- Pearldivers raft in the kelp, crack shells on their bellies, and wrap themselves in kelp to sleep (real).
- Driftbacks haul out on the Turtle Beach at night to dig; Spinecrabs creep after the eggs.
- At low tide Tidecoils slither between pools and Spinecrabs pick over the shelf.
- Glowrays gather in rings over the reef at night, their lights pulsing in sync.
- Ripplefin schools bait-ball when a Reefjaw passes: a silver sphere that tells you a shark is near.

### 2.13 Mini-bosses (premises from the FULL-GAME draft kept; fights GAP-COOKED)

Rules (KEPT, combat 2.17):
- 3,000 solo HP, +80% per extra player or tame. Stance 90. 2 parries.
- 4 to 5 attacks plus one counter mechanic. Wind-ups 0.65 to 0.9 s; punish 0.8.
- Chase 65 m. Respawn 4 h.
- Minis drop upgrade shards ×4 and a trophy (BO). Trophies are talismans (survival-crafting KEPT: Barnacle claw, Siren scale, Tyrant fin).

#### 2.13.1 Old Barnacle (a giant hermit crab; real behaviour: hermit crabs fight for and swap shells)

| Part | Detail |
|---|---|
| Body | A hermit crab 4 m tall with its soft abdomen curled inside a **newcomer longboat hull** worn as a shell (9 m with the hull). One crusher claw the size of a door (coconut crab: the strongest pinch of any crustacean). Long red-and-cream striped legs. Eyestalks on blue stalks. The hull is barnacle-crusted, with a broken mast stub, nets and dangling anchors |
| Silhouette | An upturned boat walking on red legs, a giant claw out front |
| Arena | Barnacle Cove (+1,720, −7,640): a rocky cove 70 m wide. Sand floor at low tide, 1 m of water at high (wading). Two spare "shells" lie on the beach: a Saltkin bronze bell and a Driftback carapace |
| Voice | Coconut-crab hiss and clicking, a walrus grunt pitched up, hull timbers groaning, barnacles grinding |
| Lock points | Claw, eyestalks, the soft abdomen (only once he's out of the hull) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Crusher pinch | 0.65 / 0.3 / 0.20 / 0.8 | 50 | Parryable. 40% → **Pinch and hold** (0.9, crimson glint on the claw tips): a grab, unblockable, 18/s for 2 s, then a throw 8 m |
| Hull slam | 0.85 (he pulls his legs in, the hull tips up) / 0 / 0.25 / 1.0 | 60 + a ring of 500 cm, `low` | He drops the hull on the spot where he stood; jump the ring |
| Debris toss | 0.90 (he digs a barrel or an anchor out of the hull) / 0 / travel / 0.8 | 40 | Thrown at players over 6 m away. 1,100 cm/s ballistic, never homing, landing ring shown the whole flight |
| Shell charge | 0.80 (legs drum, a sideways crouch) / 0 / 1.2 run / 1.0 | 55, knockback 600 | Runs sideways 15 m. Hitting a cove rock or the cliff stuns him 2.5 s |
| Hull fortress | 0.40 / — / 6 s / — | — | Withdraws fully into the hull: slash and pierce ×0.2 for 6 s; blunt and fire still work. Used below 60% HP or when two players flank him |

**Counter mechanic: break or burn him out of his shell.**
- **The hull has its own 1,200 HP.** Blunt (strike) damage does ×2 to it, and every 300 hull damage tears a plank off where you can see it.
- **Fire on the hull** (tarred timber) burns for 3 s, then he **bolts out naked**. A hull at 0 forces him out too.
- **Naked:** the soft abdomen is a lock point at ×2 damage and ×1.5 stance damage for 12 s. He's frantic and uses only Crusher pinch and Shell charge.
- **He runs for a spare shell.** After the 12 s he scuttles to the nearest one: the bell (heavy: the same as the hull, but his speed ×0.7 in it) or the Driftback carapace (light: his speed ×1.2, but only 600 HP).
- **Deny it.** Break a spare first (bell 800 HP, carapace 400 HP), or stand on it (he won't take an occupied shell; he'll try to Pinch you off instead). Then the next time he's naked, he stays naked.
- **Readable.** The spare shells sit in plain view from the cove entrance.

**Trophy:** Barnacle claw (KEPT: guard stamina cost −10%).

#### 2.13.2 The Siren Eel (oarfish, the real "sea serpent"; folklore: the sirens and Odysseus's wax)

| Part | Detail |
|---|---|
| Body | An 18 m ribbon eel (oarfish). A silver, mirror-scaled body as flat as a blade, with a scarlet-orange crest of long rays rising from the head like a mane that glows when she sings. A small head with huge silver eyes. Her throat hides a moray-style second jaw. Dark bars down the silver |
| Silhouette | A long, rippling silver ribbon with a blazing crest, rising straight up out of black water |
| Arena | Siren's Hollow (+1,950, −7,700): a sea cave with a lake 60 m across under a dome, 5 rock islands and a ring ledge, dripping stalactites. Lit by phosphor in the water |
| Voice | Humpback whale song (her song, pitched up into a woman's range), beluga whistles, a soprano's wordless vowel under it, the crackle of the crest rays |
| Lock points | Head, crest |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| **Siren song** | 1.20 (she rises straight up on a rock island, the crest flares bright, a rising melody) / 0 / 8 s / — | — | See the counter mechanic. She must be on a rock to sing |
| Coil whip | 0.65 / 0.2 / 0.25 / 0.8 | 45 | Her body lashes along a ledge: `low` on the first half (jump it), high on the second (duck or roll) |
| **Second-jaw strike** | 0.90 (at the water's edge her mouth opens, the white inner jaw shows, crimson glint) / 0 / 0.25 catch | 20 + 15/s for 2 s, dragged under | `grab`, `unblockable`. Pulls you into the lake 4 m down; breath counts |
| Crest lash | 0.80 / 0 / 0.30 / 0.9 | 50 | A 260° sweep of her crest from the water across a ledge |
| Shatter shriek | 1.00 (a single high note, the dome rings) / 0 / — / 1.0 | 45 per stalactite | 4 to 6 stalactites fall at marked spots: a shadow and a dust trickle on the floor for 1.0 s before each lands. Broken stalactites leave rubble cover |

**Counter mechanic: the song.**
- **Tames turn.** For 8 s, every tame within 30 m of her **turns on its owner** and attacks the nearest player of its owner's clan. This is the FULL-GAME premise and Raven's "bosses that punish tames".
- **Players are Lured.** Every player within 20 m who can hear her has their movement input pulled toward her at 250 cm/s, and stamina regen stops. Each dodge away cuts the lure by 25%.
- **Answer 1: interrupt her.** 150 damage to her crest during the 1.2 s windup (arrows, harpoon, bolts) cancels the song and staggers her 3 s.
- **Answer 2: beeswax plugs** (2.19). They make you immune to the Lure, but they muffle every sound (−18 dB above 1 kHz, tells dimmed), so you lose your best warnings. A real trade.
- **Answer 3: keep your tames out.** Order tames to Stay outside 30 m, or wear the Siren scale once you have it.
- **Tame-punish (KEPT rule; among minis, the Siren Eel only).** The song picks the furthest tame first when choosing which tames turn, so a tame parked at 29 m still turns.

**Trophy:** Siren scale (KEPT: your tames can't be turned or scattered).

#### 2.13.3 The Reef Tyrant (roaming; a Reefjaw grown huge)

| Part | Detail |
|---|---|
| Body | A Reefjaw 14 m long, its tiger bars faded to ghost lines, its snout scarred white. A Saltkin harpoon is still lodged in its jaw hinge, a length of chain trailing from it. The slingshot jaw shoots out 3 m. Its dorsal fin is notched like a saw |
| Silhouette | The Reefjaw shape at more than twice the size, with a chain trailing from its mouth |
| Route | A 3.2 km loop: Reef Lighthouse → down the Blue Wall to the Whale Fall (45 m) → north along the drop → in over the reef → **rests at the Mooring Stones** (S4) in the shallows for 2 game hours a day (sharks rest) → out along the Long Dyke → back. It hunts boats: a ram does 400 hull damage |
| Voice | Sharks have no voice. Its sound is a **sub-bass thrum** from its tail beat, chain clink and the jaw's wet crack. You hear the chain first |
| Lock points | Head, gills, belly (when beached or breaching) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Slingshot bite | 0.80 (gills flare, the jaw unhinges, white glint) / 0.3 / 0.20 / 0.9 | 60 + bleed 40 | Reach +300 cm |
| Ram | 0.70 / 0 / 0.30 / 0.8 | 50, knockback 800 | At swimmers and boats |
| **Thrash-drag** | 1.00 (the head shakes, crimson glint) / 0.2 / catch | 25 + 18/s for 3 s | `grab`, `unblockable`. Drags you 15 m down; breath stops, and the depth can crush the unprepared |
| **Breach** | 1.00 (bubble column, rising thrum) / H 0.5 / 0.35 / 1.4 | 90 | From below at a surface target or a boat. It arcs out of the water and crashes down. It beaches itself if it lands in water under 2 m |
| Tail scythe | 0.70 / 0 / 0.25 / 0.8 | 45 | 200° behind |

**Counter mechanic: harpoon it and tie it off in the shallows** (FULL-GAME "harpoon and anchor it in the shallows").
- **Get a line on it.** Any harpoon hit attaches a line: the Tide **Harpoon throw**, or a thrown **Whaler's harpoon** (2.19) for anyone.
- **Tie it off.** Run the line's end to a Saltkin **mooring stone** (8 at S4) or a boat's cleat and Interact within 3 m (1 s). It's now **tethered**: it can't go more than 10 m from the stone or deeper than the line allows, and it thrashes.
- **Beach it.** Two tethers in water under 2 m and it's beached for 8 s: belly lock point, stance damage ×2, a 1.4 s punish after.
- **Line limits.** A line breaks after 20 s, or once it has taken 600 damage of pull.
- **Tide path shortcuts.** The Anchor node pins it 3 s without a stone. Leviathan's hook drags it 500 cm and staggers it (2.17).
- **Why it works there.** It rests at the Mooring Stones, so that's where you set the trap. The Saltkin built the stones for exactly this.

**Trophy:** Tyrant fin (KEPT: swim speed +15%).

### 2.14 Major boss: Thalassor, the Tide Leviathan (GAP-COOKED; FULL-GAME premise kept: whale-serpent in a tidal lagoon whose tide rises and falls with each phase; phase 1 waves, tail slams, swallowing lunges; phase 2 high tide, fight swimming; hidden phase 3 maelstrom; weak to Sun & sand: light dries and cracks its skin, at low tide it's beached; Leviathan pearl)

**Look (built to be terrifying and believable).**

| Part | Detail |
|---|---|
| Anatomy | 52 m long. **The front 14 m is a sperm whale's head**: a battering-ram block of a skull, a narrow underslung lower jaw with 40 ivory cone teeth, a small eye set far back, and a blowhole off to the left (real sperm-whale asymmetry). **Behind the pectoral fins the body becomes a serpent**: 38 m of eel-like body with a low dorsal ridge, ending in a horizontal whale fluke 9 m across |
| Skin | Slate grey-black, wrinkled and corrugated like a sperm whale's. The head is covered in **white circular sucker scars** (real: giant-squid battle scars) and long pale harpoon scars |
| The Saltkin's mark | **Broken Saltkin harpoons** stand in rows along the dorsal ridge like spines. Bronze chain hangs from collar rings driven through the blubber behind the head: the chains that held it in the Pound, snapped |
| Lights | Fourteen photophores (deep-sea light organs) along each flank glow blue-green #5AE0C8, seen from the wall at night as a slow line of lights |
| Silhouette | A blocky ram-head on endless coils. Unmistakable from the wall |
| Scale | Head 5 m tall. When it rears, its head stands 16 m above the water |

**The arena: the Pound** (+1,000, −7,300).
- **The wall.** A Saltkin ring wall 240 m across and 30 m tall. The top walkway is 4 m wide, reached by 3 stairs (north, west, south). The run-back stone stands 50 m out from the north stair.
- **Phase 1 floor (low tide).** A central deep pool, 80 m across and 8 m deep, where Thalassor lies. Round it, a **tidal flat** 60 m wide under 0.3 to 1.0 m of water (wading ×0.85).
- **Perches and cover.** **8 Saltkin stilt-tower stumps** stand on the flat: stone towers 5 m tall with stairs, each top 4 m across. **4 bronze chain bollards** stand on the flat's rim as cover.
- **Sluices.** Four bronze sluice gates on the east wall lead to the Sluiceway. One is already burst; three are closed.
- **Three Sunwatch mirrors** stand on the wall top at north, west and south: bronze dishes 3 m across on stone pivots. The Sunwatch Mirrors POI outside points at them.
- **Mood.** Spray booming over the wall, gulls, Saltkin harpoons rusting in the flat.

**The Sun counter (FULL-GAME "light dries and cracks its skin; at low tide it's beached", GAP-COOKED as a visible system).**
- **Sun mirrors.** Hold Interact on a mirror to take it, then aim with the camera. In daylight (06:00 to 20:00 game time) it throws a gold beam. Overcast halves it; Sea fog and Rain block it.
- **Sun-dried.** 2 s of beam on one body part (head, blowhole, flank, fluke) makes that part **Sun-dried** for 20 s. The skin visibly greys and cracks, puffing dust. That part takes ×1.5 damage and ×2 stance damage. One crack per part per phase (Ahrumet's rule, mirrored).
- **The cost.** The mirror operator stands still on the wall, exposed to Spout volley.
- **At night or without sun:** put a **sunstone** (a Desert resource that stores daylight) in the mirror's cup for 30 s of beam per sunstone, day or night. Or throw a **Sunstone flare** (2.19), which counts as 1 s of beam on the part it hits.
- **The Sun path.** Every Sun & sand ability hit counts as 1 s of beam. Solar flare, Sunburst and Noon strike dry a part at once. Sun also gets the opposite rule's ×1.5 damage (SPEC).
- **Beached.** In phase 1, Head ram and Swallow lunge can carry it onto the flat. If it ends more than 10 m from the pool, or hits a tower, it's **beached for 5 s**: it rolls, the belly lock point shows, it takes ×1.3 damage, and beam time on it counts double.

**Numbers (KEPT rules):**
- 12,000 solo HP, +80% per extra player or tame.
- **Stance 170.** 3 parries to break.
- Phase 2 at 50%. Hidden phase 3 at 25% with artifacts (damage ×1.4).
- Weak to Sun & sand 1.5×. Soak buildup on it ×0.3 (it is the sea).

**Lock points:**
- Head.
- Blowhole: part HP 1,000, exposed only when it surfaces to breathe. Breaking it stops Spout volley for the phase.
- Belly: only when beached or breaching.
- Fluke.

**Attack tell (sound before sight).** Thalassor clicks steadily the whole fight (sperm-whale sonar). Before every attack the clicks speed up into a **creak** (the real buzz before a catch). Before a grab, the creak **stops dead**: a beat of silence.

**Phase 1: low tide (6 moves: a 3-hit combo and a grab; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell (small cue, combat 2.17.1b) |
|---|---|---|---|---|
| **Head ram** | 0.75 / H 0.3 / 0.25 / 0.9 | 55, knockback 600 | A 25 m line across the flat. A tower hit beaches it | The creak; the head lowers and the water humps ahead of it |
| **Jaw snap ×3** (combo) | Snap 0.60 / snap 0.50 / overhead slam 0.90 + H 0.4; P 1.2 | 40 / 40 / 70 | Parry the first two. The slam throws a splash wave 6 m out (`low`, jump it) | The lower jaw drops; white glint on the teeth |
| **Swallow lunge** (grab) | 1.10 / 0 / 0.30 catch | 20 + 20/s for 3 s, then spat out 10 m | `grab`, `unblockable`. A 20 m lunge out of the pool onto the flat. Dodge sideways. An ally hitting the head for 150 frees you. It can beach itself | The creak stops; silence; the mouth gapes; crimson glint in the throat |
| **Fluke slam** | 1.00 / 0 / 0.25 / 1.2 | 80 in 8 m, then a ring wave of 30 at 1,200 cm/s | The ring is `low` (jump it). Slam punish 1.2 s (KEPT) | The fluke rises 15 m out of the pool, water streaming off it |
| **Spout volley** | 1.00 / 0 / travel / 0.8 | 35 + soak 40 each | 5 water bombs from the blowhole, 1,200 cm/s ballistic, landing rings shown for the whole flight (≥ 0.9 s), never homing, none inside 4 m. Targets wall-top and tower players first | The blowhole puffs twice; a rising whistle |
| **Wave wash** | 1.20 / 0 / 0.40 / 1.0 | 50 + knockdown, push 800 | It swims under and throws a 2 m wave across a 60° sector of the flat. Tower tops (5 m) are above it | **The water draws back off the flat toward the pool** (the real orca trough), then a V-wake |

**Tame-punish move (KEPT rule, one per major): Bubble net** (the real humpback hunt).
- **When.** From 75% HP, once per 45 s. Windup 1.0 s: it circles under the **furthest tame first**.
- **The ring.** A ring of bubbles 8 m across rises around that tame. Inside the ring the water loses its buoyancy: swimmers sink at 200 cm/s and waders slow to ×0.5.
- **The lunge.** 2.0 s later it lunges straight up through the centre for 90 damage.
- **Players can be caught too.** Step out of the ring.

**Transition at 50%: the Sluice Breaks** (KEPT rule: an unblockable telegraphed attack, 1.4 s; the boss takes 50% damage during it).
- **The tell.** It rears to 16 m, every photophore flares, and the creak rises to a scream.
- **The attack.** It dives and rams the east sluices, and the three closed gates burst. A surge wave 2 m high crosses the flat at 1,000 cm/s: 60 damage plus a knockdown. Jump it, or be on a tower or the wall.
- **The arena change.** The sea pours in. The water rises 4 m over 10 s, so the flat is 3.5 to 4.5 m deep and everyone down there is swimming. The tower tops are now 0.5 to 1.5 m above the water: they're the perches. The wall walkway and its mirrors stay dry.

**Phase 2: high tide**

| Move | W / H / A / P | Damage | Flags | Tell |
|---|---|---|---|---|
| **Breach** (new, delayed) | 1.00 / H up to 0.6 / 0.40 / 1.4 | 100, in a line 30 m long and 8 m wide | It leaps clear and belly-flops along the line. **A tower in the line is destroyed** (arena change: fewer perches each time) | A bubble column, the creak, then its shadow on the water along the landing line |
| **Undertow** (new, delayed) | 1.20 / H up to 0.5 / 3.0 pull, then a 0.30 bite / 1.2 | Bite 70 | It circles tight under a perch. Swimmers within 15 m are pulled toward the centre at 250 cm/s for 3 s, then it bites up through the centre. **Swim sideways out of it** (the rip rule) | A whirl on the surface and the creak; foam streaming inward shows the pull |
| **Coil, snap, lash** (new combo) | Coil 0.90 / snap 0.50 / lash 0.70; P 1.2 | 60 / 40 / 50 | The coil wraps a tower and crushes everyone on it (`unblockable`, 5 m ring). Then a snap at whoever's nearest, then a tail lash across the water (`low`) | Its body circles the tower underwater (the wake shows); crimson glint at the squeeze |
| **Blowhole breath** (opening) | — | — | Every 30 s it surfaces for 4 s to breathe. The blowhole is exposed: ×1.5 damage, and Sun mirrors dry it ×2. A 12 m plume of spray marks it | The plume |
| Keeps | Head ram (from the water at the perches), Spout volley, Swallow lunge (from below at swimmers: a bubble column 1.0 s), Bubble net | | | |

**Sun in phase 2.** The mirrors only catch it while it surfaces, breathes or breaches. Mirror players wait for the plume, and that's the rhythm of the phase. Sun players stand on the perches and the wall.

**Hidden phase 3 (25%, artifacts only; KEPT frame): the Maelstrom** (real whirlpools form where tidal currents meet).
- **The water.** The whole Pound spins. A whirlpool 60 m across forms at the centre and pulls swimmers toward it at 300 cm/s. The eye does 20 damage a second and drags you down.
- **The boss.** It rides the vortex wall and lunges out at a perch every 6 s (a Head ram from the vortex).
- **Safe ground.** Only the surviving towers and the wall.
- **The Sun breaks it.** 3 s of beam on its body while it rides the vortex makes it lose the vortex for 8 s: the whirlpool stops and it flounders at the surface.
- **Damage ×1.4** (KEPT). Jaw snap ×3 becomes ×4.

**Drops (KEPT BO rule plus survival-crafting's major-boss trophy rule):**
- Boss core `core_thalassor`.
- Tide essence ×5.
- Ambergris ×1 to 3.
- Chance at the boss part **Leviathan pearl**. FULL-GAME: dodging through water restores stamina. GAP-COOKED number: a dodge or kick-dodge while wading or swimming restores 10 stamina, at most once per 1 s.
- Trophy head (monument).
- 2 blueprints: the **Tidebreaker** (a spear-class harpoon on a chain; its moveset goes to the boss-weapons cook) and the **Coral keep decor pack** (KEPT set name).

**Music:** TID_Boss_Thalassor (2.16).

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT from fire-frost 3.5)

Every species gets tell, strike, hit and pain, plus idle, aggro and death. Each is built from at least 3 real animal calls and one material layer. Tells rise in pitch and sit 3 dB over the bed. **Underwater, every voice runs through a 1.2 kHz low-pass with long reverb and carries further** (far calls reach 200 m).

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Reefjaw | (Sharks are silent.) Tail-beat sub-bass thrum, a sea-lion growl pitched down for the surface, an alligator gar's gulp | Water tearing, denticle rasp on hulls | The thrum building under you before a breach |
| Tidecoil | Moray hiss and gape click, electric-fish discharge from a hydrophone (a real buzzing hum), a snake's hiss | Crackle on discharge | The rising buzz-crackle before Shock pulse |
| Spinecrab | Coconut-crab hiss, mantis-shrimp snap (a real hydrophone pop), a castanet ratchet | Shell scrape, sand hiss | The ratchet click of the club cocking |
| Gullraider | Great skua call, frigatebird pouch drumming, herring-gull laugh | Wing flap | The laughing call before a snatch |
| Wavecharger | Seahorse clicks (real feeding clicks), dolphin whistle pitched down, a horse's snort | Fin whirr | The herd's click chorus |
| Driftback | Leatherback hiss and grunt (real nesting sounds), an elephant-seal exhale | Shell knock, sand throw | The heavy exhale before a ram |
| Glowray | (Near silent.) Manta wing-whoosh, a soft beluga chirp pitched low | Glass shimmer on the flare | The shimmer as its lights ripple |
| Pearldiver | Sea-otter squeal and chuckle, the stone crack | Splash | Squeals in the raft |
| Kelpgrazer | Manatee chirps and squeaks pitched down, a walrus groan | Water churn | A long low groan when frightened |
| Tidefowl | Puffin growl, eider "ah-OO" | Burrow scuffle | The colony's growl |
| Ripplefin | None (a school swish) | Water swish | The swish as it scatters |
| Old Barnacle | Coconut-crab hiss, walrus grunt pitched up | Hull timbers groaning, barnacle grind | The hull groaning as he rises |
| Siren Eel | Humpback song pitched into a woman's range, beluga whistle, a soprano's wordless vowel | Crest-ray crackle | Her song filling the cave |
| Reef Tyrant | Sub-bass tail thrum, chain clink | Wet jaw crack | The chain clink before you see it |
| Thalassor | Sperm-whale clicks and creak (real codas and echolocation buzz), humpback moan pitched down an octave, walrus bellow | Blowhole exhale like a steam vent, chain grind | The creak, and the silence before a grab |

### 2.16 Music: the Ocean (tempo, key, instruments SPEC 3.3: 76 BPM, Eb major, rolling strings, low choir, swells timed like tides; cues GAP-COOKED; rules KEPT)

**Palette:**
- Rolling strings: cello and viola ostinati in 6/8 that rise and fall like swell.
- A low male choir singing wordless vowels in the call-and-response of Hebridean waulking songs (real Gaelic work songs).
- Concert harp.
- An **ocean drum**: a real frame drum filled with beads, tilted for the sound of surf.
- Bowed vibraphone for the underwater shimmer.
- A conch horn (the Polynesian pū) for boss calls.
- Combat percussion: low frame drums and timpani.

**Swells timed like tides.** Each explore cue breathes in a 16-bar rise and fall, and the mix follows the tide clock: fuller at high tide, thinned to harp and drum at low tide. It's a real-time stem mix driven by `UWRTideSubsystem`.

**Underwater.** The music gets a 900 Hz low-pass and adds the bowed-vibraphone stem. It stays audible (it ducks no more than 3 dB here).

**Always audible**, with silences only where they're written into the piece. Ducks at most 4 dB. Mixed −18 LUFS, calm −23, boss −15. **One planned silence:** the first 8 bars when the King Tide draws the sea back. The sea goes quiet and the music stops; then the Low layer enters.

| Cue | Length | Use | Feel |
|---|---|---|---|
| TID_Explore_A / B / C | 3 to 4 min | Day. A: O1, O2. B: O3. C: O4, O5 (underwater stem) | A: harp and rolling cellos, the ocean drum, bright. B: drier, plucked strings and low choir, the pass wind. C: bowed vibraphone, slow strings, the choir far away |
| TID_Night_A / B | 3 min | Night | Choir alone over a cello drone, harp harmonics like flecks of Noctiluca |
| TID_Combat (stems for A, B, C) | loops | Combat layer on the bar | Frame drums and timpani in 6/8, the string ostinato doubling in speed, choir shouts on the downbeats |
| TID_Mini | 3 min loop | Old Barnacle, Siren Eel, Reef Tyrant | A 4-bar intro per mini: low brass and creaking-timber percussion (Barnacle); a solo soprano that becomes her song (Siren); a single bass note pulsing at heartbeat speed, then the drums (Tyrant) |
| TID_Boss_Thalassor P1 / P2 / P3 | sections | Phase changes on the bar | P1: waulking-song call and response, choir and drums, the conch on the phrase ends. P2: the full orchestra comes in as the sea does, strings in rising swells. P3: the meter breaks into 7/8, the strings circle (the maelstrom), the conch screams |
| TID_DrownedHall | 4 min | The Drowned Hall | A distant bell, muffled choir as if underwater, harp drips |
| TID_KingTide | layer | Event | After the planned silence, the Low layer: low strings and the ocean drum, slow and sparse. The Return: a timpani roll building for 10 min to the bore |
| TID_Camp | 2 min | Camps and your Hearth on the coast | Harp and a single fiddle, warm |
| Sea theme (world-map KEPT) | — | Open sea beyond 600 m | Rolling strings in Eb |
| Saltgate duet (world-map rule) | — | The pass | Sun's frame drums and ney against Tide's rolling strings over a shared Eb drone (D Hijaz meets Eb major), 80 BPM |

**Source:** first pass from CC0 and CC-BY libraries matched to tempo, key and instruments (OpenGameArt, Free Music Archive, Pixabay Music, incompetech). Boss tracks commissioned later. Credited in `Docs/CREDITS.md`. No AI generation (KEPT, fire-frost 3.6).

### 2.17 The Tide path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Flow (DD SPEC: "builds while moving and dodging; drains when standing still"). A spender (combat KEPT: Tide Wildbrands and spells cost Flow).**

| Item | Value |
|---|---|
| Max | 100, +15 per Flow capacity rank (145 at 3) |
| Gain while moving | +5 a second moving over 300 cm/s on the ground; +8 a second sprinting, swimming or riding a swimming mount |
| Gain from dodging | +10 per dodge or kick-dodge; +15 per **perfect dodge** (an attack passes through your i-frames) |
| Gain from hits | +4 per weapon hit landed while moving over 150 cm/s |
| Drain | After 1.5 s below 150 cm/s: −8 a second. Rooted casts (Maelstrom, High tide) pause the drain while they play |
| Read | A sea-green bar that ripples when it's filling; it stills and dims when it's draining. A soft water-rush sound on the perfect-dodge gain |

The rule it teaches: Tide players keep moving. Standing still empties you. That's the flowing, dancing fighter Raven's resource describes.

**Root passive: Gills (SPEC text: "Breathe and fight freely underwater"; numbers GAP-COOKED).** Main path only.
- **Breathing.** No breath limit. Crushing depth starts at 80 m, deeper than anywhere in the Ocean, so the biome's deep-water hazard is fully passed (3.3 SPEC).
- **Fighting freely.** No underwater penalties: every weapon swings at full speed underwater, charged heavies work, and guard stamina is normal.
- **Moving.** Underwater speed ×1.25.
- **Flow.** Flow gain in water ×1.5.

**The tree.** Gates are points spent in the branch before the node, as `fire.json` does.

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Water bolt | 3 pts | 0 | Basic water spell | Bolt 15, soak 35, cd 1.0, 6 Flow; 2,600 cm/s; ×1.0 speed underwater; homes only on creatures (KEPT fire-frost RAISE 3) | KEPT combat |
| Trunk | Flow capacity | 3 ranks | 3 | Larger Flow bar | +15 max Flow per rank | GAP-COOKED |
| Trunk | Soak chance | 2 ranks | 6 | Higher chance to soak enemies, slowing them | +10% soak buildup on all your hits, and +6 soak buildup on every weapon hit, per rank (so any weapon can soak) | GAP-COOKED |
| Trunk | Wave cut | 2 pts | 8 | Basic water weapon art | Wildbrand: 15 Flow, cd 2.5, W/A/R 0.28/0.20/0.42, ×1.25, soak 45, stance ×1.5; flagged `underwater` and `mounted` | KEPT combat |
| Undertow | Riptide pull | 5 pts | 0 | Spell: pull enemies toward you | Pull shape (cone 30°, 1,200 cm, pulls 500 cm; not bosses), cd 10 (KEPT); costs 15 Flow, soak 20; lowland pulled targets stagger 0.5 s at the end | KEPT combat (cost GAP-COOKED) |
| Undertow | Drag | 3 ranks | 5 | Stronger pulls | +100 cm pull per rank (800 at 3); at rank 3 highland creatures are pulled half the distance | GAP-COOKED |
| Undertow | Drowning | 4 pts | 8 | Soaked enemies lose stamina | Soaked players lose 6 stamina a second (KEPT combat). Soaked creatures (which have no stamina) stop regenerating stance and take +15% stance damage | KEPT / GAP-COOKED |
| Undertow | Whirlpool | 5 pts | 12 | Spell: a whirlpool that holds enemies | Pool radius 350 holds lowland and midland, 6 dps, 6 s, cd 18 (KEPT); 30 Flow; soak 15 a second inside; highland slowed ×0.7 inside; works on land (a column of water) or in water | KEPT combat (cost GAP-COOKED) |
| Undertow | Heavy water | 3 ranks | 17 | Stronger soak slows | The soak slow goes from ×0.8 to ×0.75, ×0.70, ×0.65 by rank (slow capped at −60%, fire-frost rule); soak lasts +1 s per rank | GAP-COOKED |
| Undertow | Maelstrom | 10 pts | 20 | Capstone: a huge whirlpool drags everything in | Storm radius 1,000, pull 300 cm/s, 12 dps, cd 60 (KEPT); 60 Flow; 8 s; soak 20 a second; lowland and midland drawn to the centre take ×2 damage there; highland pulled at half speed; bosses not pulled but soaked | KEPT combat (rest GAP-COOKED) |
| Harpooner | Harpoon throw | 5 pts | 0 | Weapon art: spear an enemy and reel it in | Wildbrand: spear or throwing spear; 25 Flow; cd 8; 0.40 throw, flight 2,700 cm/s, 0.40 recovery; ×1.5; soak 30; stance ×2; line range 1,800 cm. Follow-up within 1.5 s: reel. Lowland and midland are pulled to you; larger ones stay put (Tow pulls you to them) (KEPT). Flagged `underwater` | KEPT combat |
| Harpooner | Barbed tip | 5 ranks | 5 | More harpoon damage and bleed | +6% harpoon damage and +10 bleed buildup per rank (bleed status, R2) | GAP-COOKED |
| Harpooner | Quick reel | 3 ranks | 10 | Faster harpoon recovery | Harpoon cooldown −1 s per rank (8 → 5); reel speed +15% per rank | GAP-COOKED |
| Harpooner | Anchor | 4 pts | 13 | Pin large enemies in place | Hold the reel input after a hit on a highland creature or a mini: highland rooted 3 s (KEPT 3 s); minis slowed ×0.5 for 3 s; bosses no (that's Leviathan's hook). +10 Flow. A boat cleat or mooring stone within 3 m makes it a full tether (2.13.3) | KEPT / GAP-COOKED |
| Harpooner | Tow | 3 pts | 17 | Pull yourself to enemies or walls | Follow-up on a hit to terrain, a wall or a large target: you're pulled along the line at 1,800 cm/s (max 1,800 cm), i-frames on the last 0.2 s, fall damage resets on arrival. **A grapple on any static surface within 18 m**: cliffs, sea stacks, the Pound's wall, ship hulls. Works underwater | GAP-COOKED |
| Harpooner | Leviathan's hook | 10 pts | 20 | Capstone: drag even bosses, staggering them | The harpoon becomes a heavy hook (visible). On a boss: hit then reel within 1.5 s drags it 300 cm toward you, adds 60 stance and staggers it 1.2 s, once per 30 s per boss. On a mini: drag 500 cm and stagger 2 s. 40 Flow, cd 20 | SPEC text; numbers GAP-COOKED |
| Tidecaller | Healing wave | 5 pts | 0 | Spell: a wave that heals allies | A wall-shaped ally heal of 35 that travels 1,200 cm, cd 12 (KEPT); 25 Flow; it passes through enemies harmlessly | KEPT combat |
| Tidecaller | Mending tide | 5 ranks | 5 | Stronger healing | +8% to all your Tide heals and shields per rank | GAP-COOKED |
| Tidecaller | Cleanse | 3 pts | 10 | Remove status effects from allies | Ally target, cd 15 (KEPT); 20 Flow; removes every status and buildup except corruption | KEPT combat |
| Tidecaller | Tidal shield | 4 pts | 13 | Spell: a water shield on an ally | Ally target, 35 HP shield for 8 s, cd 15 (KEPT); 20 Flow. When it breaks it bursts, soaking attackers within 2 m (soak 30) | KEPT combat (burst GAP-COOKED) |
| Tidecaller | Current | 3 ranks | 17 | Allies in your waves move faster | Allies your waves pass get +6% move speed per rank for 6 s (+18% at 3), and allies who own Tide gain +10 Flow | GAP-COOKED |
| Tidecaller | High tide | 10 pts | 20 | Capstone: a great wave heals allies and knocks enemies back | A wall 1,000 wide that travels 1,800 cm at 1,000 cm/s; heals allies 60 (KEPT); knocks lowland and midland back 600 cm, staggers highland 1.0 s, 40 stance on bosses; cd 90 (KEPT); 60 Flow | KEPT combat (shape GAP-COOKED) |

**Stat and passive behaviour (all server-side, through `ApplyHit`):**
- **The soak status** is KEPT from combat: slowed ×0.8 for 10 s, fire buildup −50%, shock buildup +50%, Conductive +25% lightning damage; cure by standing by a fire for 3 s; teal meter.
- **Reactions** are KEPT from combat: Lightning on Soaked +25% and a 30% free shock proc; Frost on Soaked +50% frost buildup; Fire on Soaked −50% burn buildup.
- **Water hits.** Every Tide hit carries `bWaterHit` (biome-desert's flag), so Tide cracks Ahrumet's sun-hot stone and wets sand under Dunejaws (desert KEPT).

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Flow perfect-dodge gain | A ribbon of water trails your dodge and flows into your hand | A water rush |
| Soak (any) | Targets drip and darken; a teal sheen | A wet slap |
| Drowning | Bubbles from the soaked target's mouth; its stamina or stance bar flickers | A gurgle |
| Barbed tip bleed | Barbs flare on the harpoon head on impact | A tearing rasp |
| Anchor | The line goes rigid and glows sea-green; the target strains | A taut rope creak |
| Tow | You zip along the line in a spray trail | A line whine |
| Leviathan's hook | The hook glows, the line thickens to a chain, and the boss lurches toward you | A chain clank and a deep thud |
| Tidal shield burst | A water burst ring | A splash |
| Current | Allies trail faint water streaks | A soft surf rush |

**Combos with Tide (CB SPEC, names and effects as written; built in R6 for the paths that exist):**
- **Fire + Tide:** Hot spring / Boiling sea.
- **Lightning + Tide:** Live water / Thunder tide.
- **Sun & sand + Tide** (opposites): Oasis / Rainbow.
- **Nature + Tide:** Rain / Swamp guardian.
- **Frost + Tide:** Flash freeze / Frozen maelstrom.
- **Blood & curse + Tide:** Life tide / Shark harpoon.
- **Tide + Necromancy:** Tidal dead / Depths. Depths needs underwater minions, which the underwater frame (2.6) supports.
- **Later:** Wind + Tide (R7), Spore (R8), Arcane (R9), Void (R10) are built in those paths' rounds.

### 2.18 Body change: Tide (look KEPT from hero.md 2.6; thresholds GAP-COOKED in fire-frost's frame)

**Frame (KEPT, fire-frost 3.7 and hero 2.6):**
- Strength is points ÷ 100. Main path at full strength, subclass ×0.6. Changes ramp smoothly.
- The masks and looks come from hero.md: G = scale sheen (iridescent spec), B = gill lines that open underwater, a sea-green iris, wet-look roughness.
- **The look:** a diver's grace, sleek and glossy, never fish-like or scaly-monstrous.

| At | What shows |
|---|---|
| 10 | Skin takes a faint wet-look gloss (roughness −0.1); hair looks freshly wet at the ends |
| 30 | The **scale sheen** (hero 0.3): a fine iridescent shimmer, teal to violet, on the sides of the neck and the collarbones, seen only when the light catches it |
| 50 | Three fine **gill lines** on each side of the neck, as faint as scars (hero 0.6). The iris takes a sea-green ring #1E8C86 |
| 75 | The sheen spreads down the spine and across the shoulder blades. **The gills open when you go under the surface**: the lines part and flutter, with a fine bubble stream (Niagara). Wet-look roughness −0.3 (hero KEPT) |
| 100 | The full sea-green iris. In darkness underwater the sheen glows very faintly with bioluminescent points along the neck and spine, like the night surf |

- **Flow drives it:** high Flow brightens the sheen and quickens the gill flutter underwater.
- **Tide with another path:** with Fire, the steam of Hot spring rises off wet skin. With Frost, the gill lines rime pale. With Blood, garnet veins run under the sheen.

### 2.19 Gear and items from the coast

**Reefmail (the Tidecoral set; name KEPT from hero.md, stats KEPT from survival-crafting):** medium, poise 40.
- **Inputs (full set):** steel plate 10, tidecoral 10, Spinecrab shell 4, kelp 6.
- **2 pieces:** soak buildup ×0.5.
- **4 pieces:** breath ×2 and no crushing damage to 40 m (it counts as the diving helm).

**Look, merged (the two cooks differ; this lane proposes one, logged to LATER-COOKS for the look lane):**
- From hero.md: tidecoral plates over sharkskin, coral pauldrons branching up past the head, gill windows at the neck and arms, kelp-strip tassels.
- From survival-crafting: pearl inlay, sea-glass beads, a scale-mail skirt.
- **Palette:** coral red-orange #E07A4F and sea green #1E8C86 (hero), with pearl and sea-glass accents (survival). Colour law: no crimson.
- **Women's cut:** a fitted sharkskin bodysuit cut high at the hip, coral-plate cuirass over the chest and ribs, bare arms with coral vambraces, a short scale-mail skirt, coral pauldrons branching up behind the shoulders, sea-glass beads at the throat.
- **Men's cut:** a sleeveless sharkskin jerkin open at the collar, coral plate over the chest, bare arms with coral vambraces, a scale-mail kilt over fitted sharkskin leggings, the same branching pauldrons.
- **Path marks stay visible:** the neck and arm windows show the Tide sheen and gills (hero rule).

**Hazard gear (KEPT):**
- **Diving helm:** Head slot, Armorer 3. Tidecoral 4, copper 6, glass 1. Crushing depth to 40 m, breath ×2.

**Coast items (GAP-COOKED):**

| Item | Station | Inputs | Effect |
|---|---|---|---|
| Diver's draught (KEPT effect, world-map) | Alchemy 1 | Sea grapes 2, salt 1, kelp 1 | Breath 90 s for 10 min |
| Swim fins | Workbench 2 | Ray wing 2, eel skin 1, fiber 4 | Legs slot: swim speed +25%; land run ×0.9 while worn |
| Whaler's harpoon | Workbench 2 (scrap from Harpooners' Hall or quest 3) | Iron 2, wood 2, rope 4, shark teeth 2 | A throwing-spear-class weapon with a 18 m line. A hit attaches a line you can tie to a mooring stone or a cleat (Interact). Lowland and midland are reeled in. It's the Reef Tyrant counter for anyone. Reusable (you reel it back) |
| Beeswax plugs | Alchemy 1 | Beeswax 2 (Forest Hiveback, R3), fiber 1 | Talisman slot: immune to the Siren song's Lure and to Gullraider mob calls; all sound above 1 kHz −18 dB while worn |
| Sun mirror (piece) | Workbench 3 (recipe from the Archive tablet or quest 5) | Bronze 8, tidecoral 2, polished bronze sheet 1 | A placeable bronze mirror on a pivot that throws a gold beam in daylight (or 30 s per loaded sunstone). In the Pound, the three built mirrors are the same piece. At bases it's decor and a signal (seen from 1 km) |
| Sunstone flare | Alchemy 2 | Sunstone 1 (Desert), glass 1, kelp ash 1 | Thrown (arc, 1,400 cm/s, never homing): a 2 m burst of daylight. On Thalassor: 1 s of beam on the part it hits. On anything: blind 25 |
| Shark-tooth arrow | Workbench 2 | Shark tooth 1, wood 1, feather 1 → 4 arrows | Bleed 20 per hit; ×1.1 damage to swimming targets |
| Eel-skin waterskin | Workbench 1 | Eel skin 1, fiber 2 | A waterskin of 60 thirst (built waterskin 40) |
| Pearl ring | Forge (ring rule, KEPT) | Any band + a pearl | +3% max Flow (pearl gem 5% × 60%), or for non-Tide players +5 s breath |
| Kelp line, sea pen, tide mill, fish trap (KEPT) | Survival-crafting recipes | | |
| Tidecoral fittings (boat, KEPT) | Workbench 4 | Tidecoral 4 per boat | +25% hull HP, no barnacle drag (KEPT) |
| Driftwood beacon (piece) | Hand | Driftwood 6, shells 4 | A Saltkin-style beacon you can light at a base: a 300 m light and a map marker for your clan. Burns 1 game day per 4 wood |

**Trophy talismans (KEPT, survival-crafting):** Barnacle claw, Siren scale, Tyrant fin.

**"Hae-won's knife"** (quest 6): a dagger with a tidecoral edge. Soak 20 per hit, and full attack speed underwater for anyone.

### 2.20 The story the coast tells, with no words (GAP-COOKED; fits world-map's Twelve Peoples, the pass wars and the desert's "this was a sea")

**The Saltkin walled out the sea and penned in the Leviathan.** They were a sea people. They built their city on the reef flat behind a great dyke, the **Long Dyke**, with one sluice gate opened only at low tide, and only the king held its key (Ys). They hunted and worshipped the Leviathan, and in the end they penned it in the **Pound**, a tidal lagoon walled and chained. The Sunwatch came over Saltgate to take the coast. A Sunwatch woman, or a Saltkin woman wearing a Sunwatch lover's bracelet, took the king's key and opened the sluice. The sea took the city. The Pound's gate burst, and the Leviathan was free. The survivors climbed the terraces, carved stones where the water stopped, and never built below them again.

**What the player sees, in the order they'll likely find it:**
1. **The camps' wrecks** (O1, O2): the newcomers came the same way as everyone (world-map rule).
2. **Shell-chime beacons** every 150 m along the coast road, the Saltkin's waymark. They all lean slightly seaward, toward the reef.
3. **The Rip Beach post** with its shell line strung sideways: someone taught this coast's lessons in shells.
4. **The Driftwood Chapel** (O1): votive boats with tiny harpoons. They prayed to the hunt.
5. **The Long Dyke and the Dyke Head** (O1 to O5): a wall into the sea, and above its door an empty key-hook the size of a man.
6. **The Tsunami Line** (O3): stones on a contour, ruins below, whole houses above. *Something came this far.*
7. **Harpooners' Hall** (O3): every harpoon points at the Pound; the frieze shows men on the beast's back with chains in its flesh.
8. **The Chain Anchors and the Pound** (O3): chains snapped bright inside, a burst bronze gate. It got out.
9. **The Sunwatch Mirrors and the Drowned Colossus** (O3): the sun people came through the pass with mirrors aimed at the Pound. Their champion lies face-down in the Gate Inlet, wrapped in a Saltkin whaling chain and eaten by coral. On the other side of Saltgate the Saltkin's sea-serpent lies bleached by the sun (biome-desert KEPT). Each lies dead on the other's side, killed by its opposite: Raven's weakness rule, told by the land.
10. **The Sunwatch Barque** (O5): the war came by sea too.
11. **The Drowned Hall** (hidden): the king dead in his bed, his hand at his throat where the key-chain broke. In the Sluice Works the lever stands open with the key still in the lock, and a woman's skeleton in Saltkin pearls and a Sunwatch sun-disc bracelet sits at it. The bell still rings at midnight high tide.

**Nobody explains it.** Lore items are short, Souls-style item text: Tide-stone rubbing, Harpooner's tally-cord, Sunwatch mirror-shard, Votive boat, Bottomless ladle, Broken key-chain, Tablet of the Pound, Ship's log, last page.

**Secrets (GAP-COOKED):**

| Id | Secret | How found |
|---|---|---|
| O-S1 | **The drowned bell.** At midnight on a high tide the Bell Chamber's bell rings by itself from the swell. You hear it across the reef. Swim to the Bell Tower: a window that's shut at other times stands open (the swell lifts its shutter), a shortcut straight into the Bell Chamber with a Saltkin offering bowl: pearls ×3, a gem | Midnight high tide, any weather but Squall |
| O-S2 | **The Sluice key.** Take the key from the Sluice Works lock (Interact 3 s). It opens the **Dyke Head's vault** on land, a door under the empty key-hook: the king's treasury (a pearl circlet: head slot, +10 s breath, +5% max Flow; gold; upgrade shards ×2) | Drowned Hall, any time |
| O-S3 | **The Horn ledge.** Ride the Spouting Horn's launch at high tide (stand on the vent, jump just before the column): it throws you 18 m up onto a hidden ledge with a Saltkin watch hut and a cache (iron ×4, a harpoon head) | High tide, see the worn footholds round the vent |
| O-S4 | **The mirror line.** At the Sunwatch Mirrors, turn the three tripod mirrors so their beams meet (Interact on each; the beams show at noon only). The crossing point lights a Sunwatch cache buried in the salt at (+760, −6,980): sunstone ×3, a bronze sun-disc (decor), the Sun mirror recipe | Noon, clear weather |
| O-S5 | **The Lazarus ledge.** A tamed Pearldiver inside Lazarus Deep finds a side pocket at 40 m holding a newcomer diver's last cache: a diving helm (used), steel plate ×4 | Bring a Pearldiver |
| O-S6 | **The Low Gate.** Only in the King Tide: walk the dry Sunken Market to the Drowned Hall's Low Gate | King Tide |

### 2.21 Invasions: Sun ↔ Tide through Saltgate (3.3 rule KEPT; Desert side KEPT from biome-desert; Ocean side GAP-COOKED; live from R6)

**Desert invades Ocean (KEPT, biome-desert 2.21):**
- Duskjackal packs at night, or Dromad stampedes driven by Glasstails by day, plus Dunejaws.
- Ssarrok leads, swimming the beach sand.
- Invaders dry the ground they cross: wet sand turns dry for 2 game hours and tide pools steam away.
- **Ocean-side answer (GAP-COOKED):** Tide abilities rewet the ground (`bWaterHit`), so Dunejaws and Ssarrok can't dive there. On the strand, the tide itself wets the sand: an invasion at high tide fights on worse ground.

**Ocean invades Desert:**
- **Who:** 8 to 14 creatures. Spinecrab ranks (6 to 10), with Gullraider flocks over them (2 to 4) snatching food from defenders' bags.
- **Led by Old Barnacle.** He leaves his cove and walks the pass in his hull. The Reef Tyrant is a shark and can't leave the water, so the walking mini leads (logged to LATER-COOKS: the desert cook named "its roaming mini").
- **Their mark (KEPT, biome-desert):** each invader leaves a 6 m patch of wet sand for 4 s, which stops Dunejaws diving there.
- **The sea fog (GAP-COOKED):** a sea-fog bank (the Fog state, 300 m radius) rolls through Saltgate with them and sits over the target for the invasion. Radiance charges ×0.6 in it (overcast rule), so it blunts the Sun path's home advantage.

**Payout:** Sun and Tide essence, plus 2,000 path XP to everyone with 10% (3.3 KEPT).

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing from data, so `L_Island` is touched as little as possible.

**R6 also delivers these shared pieces, needed here:**
- the **soak** status (FULL-GAME 11);
- **underwater play** (2.6): dive, breath, depth, underwater combat rules;
- **swimming mounts** and the Floeback (tundra spawn);
- boats (world-map W6, W10) and fishing (survival-crafting);
- the world-map terrain for the Ocean half of B2, including the sea mask out to x +4,200 and the reef bathymetry.

This lane plans the coast content and the underwater frame on top of them.

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/tide.json`** | Same schema as `fire.json`: `id: "tide"`, `name: "Tide"`, `biome: "ocean"`, `starter: "water_bolt"`, `resource {id: "flow", name: "Flow"}`, `rootPassive {id: "gills", name: "Gills", text: SPEC}`. Trunk and three branches with node ids `water_bolt, flow_capacity, soak_chance, wave_cut; riptide_pull, drag, drowning, whirlpool, heavy_water, maelstrom; harpoon_throw, barbed_tip, quick_reel, anchor, tow, leviathans_hook; healing_wave, mending_tide, cleanse, tidal_shield, current, high_tide`. `kind` (ability/stat/capstone), costs, ranks and gates as in 2.17. Node text copied from ST word for word |
| `Data/tuning.json` `resources` | `flow {max: 100, moveGain: 5, sprintGain: 8, swimGain: 8, dodgeGain: 10, perfectDodgeGain: 15, hitGain: 4, hitMoveSpeed: 150, moveThreshold: 300, stillAfter: 1.5, stillSpeed: 150, drain: 8}` |
| `Data/tuning.json` `nodeEffects` | `tide.flow_capacity {maxFlow: 15}`, `tide.soak_chance {soakBuildup: 0.10, weaponSoak: 6}`, `tide.drag {pullDistance: 100}`, `tide.heavy_water {soakSlow: 0.05, soakDuration: 1}`, `tide.barbed_tip {harpoonDamage: 0.06, harpoonBleed: 10}`, `tide.quick_reel {harpoonCooldown: -1, reelSpeed: 0.15}`, `tide.mending_tide {tideHeal: 0.08}`, `tide.current {allySpeed: 0.06, allyFlow: 10, seconds: 6}` |
| `Data/tuning.json` `passives` | `gills {breath: false, crushDepth: 8000, swimSpeed: 1.25, waterFlowGain: 1.5, noUnderwaterPenalty: true}`, `drowning {playerStamina: 6, creatureStanceTaken: 0.15, stopStanceRegen: true}`, `anchor {rootSeconds: 3, miniSlow: 0.5, flow: 10}`, `tow {speed: 1800, range: 1800, iframes: 0.2}`, `leviathansHook {bossDrag: 300, bossStance: 60, bossStagger: 1.2, bossCooldown: 30, miniDrag: 500, miniStagger: 2}` |
| `Data/tuning.json` `spells` | Tide entries as combat 2.13 (KEPT), plus Flow costs (`waterBolt 6, riptidePull 15, whirlpool 30, maelstrom 60, healingWave 25, cleanse 20, tidalShield 20, highTide 60`), `whirlpool.soakPerSecond: 15`, `whirlpool.highlandSlow: 0.7`, `maelstrom {seconds: 8, centreMult: 2, soakPerSecond: 20}`, `tidalShield.burstSoak: 30`, `highTide {width: 1000, travel: 1800, speed: 1000, knockback: 600, bossStance: 40}`, Wildbrands `waveCut` and `harpoonThrow` (KEPT combat rows) with flags `underwater`, `mounted` |
| `Data/tuning.json` `status` | The soak row from combat 2.11 (KEPT): `slow 0.8, seconds 10, burnBuildupMult 0.5, shockBuildupMult 1.5, conductive 0.25, cureFireSeconds 3`; colour teal |
| **New `Data/tuning.json` `underwater`** | `swimSpeed 300, fastSwim 450, fastStamina 6, kickDodge {distance: 400, seconds: 0.3, iframes: 0.2, stamina: 14}, surfaceDodge 250, wadeDepth [60, 140], wadeRun 0.85, meleePlayRate 0.8, thrustClasses ["spear","dagger","harpoon","fists"], guardStaminaMult 1.2, bowAllowed false, crossbowRange 0.5, boltSpeedMult 0.6, fireMult 0.3, lightningSpreadRadius 400, cameraArm 250, breath {base: 30, perSwimLevel: 0.6, refillSeconds: 3, drownDps: 10}, crush {depth: 2500, dps: 5}, musicLowpass 900, voiceLowpass 1200` |
| **New `Data/tuning.json` `tide`** | `range 240, highHours [0, 12], lowHours [6, 18], biome "ocean", edgeBlend 30000, kingTide {drop: 600, warnSeconds: 300, lowMinutes: 50, returnWarnMinutes: 10, boreHeight: 300, boreSpeed: 600, boreDamage: 80, boreCarry: 2000, pieceDamage: 200, lineContour: 600, payout: 3.0, strandedTameMult: 3, spawnNightTame: 2}` |
| `Data/tuning.json` `hazards` | `rip {speed: 250, width: 800, length: 12000, againstStamina: 2.0}`, `blowhole {tell: 1.0, height: 600, hornHeight: 4000, damage: 40, launch: 600, hornLaunch: 1800, surgeSpeed: 600, surgeDamage: 30, surgeDrag: 600, liveFromTide: 0.0}`, `manOWar {damage: 25, poison: 50, tentacleDepth: 800, tickRadius: 600, strandedOnBeachAfterSquall: true}`, `deepWater` (KEPT 3.3) |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `slingshot_bite, ram, thrash_drag, breach_strike, surf_lunge, tail_scythe, hole_strike, second_jaw, shock_pulse, coil_whip, pool_slither, crusher_pinch, club_snap, shell_guard, sand_burst, side_charge, dive_peck, snatch, mob_call, wing_buffet, snout_snap, tail_lash, charge_swim, herd_click, shell_ram, flipper_slam, beak_bite, barb_sting, light_flare, wing_slam, stone_smash, panic_bump` |
| `Data/tuning.json` `bosses` | `thalassor {stance: 170, parries: 3, soakResist: 0.3, mirrorDrySeconds: 2, dryDuration: 20, dryDamage: 1.5, dryStance: 2.0, sunstoneBeamSeconds: 30, overcastMult: 0.5, beachedSeconds: 5, beachDistance: 1000, beachedDamage: 1.3, floodRise: 400, floodSeconds: 10, breatheEvery: 30, breatheSeconds: 4, blowholeHealth: 1000, bubbleNetRadius: 400, bubbleNetCooldown: 45, bubbleNetFrom: 0.75, sinkSpeed: 200, undertowPull: 250, maelstromPull: 300, maelstromEyeDps: 20, maelstromBreakBeam: 3, maelstromBreakSeconds: 8, towerCount: 8, towerHealth: 1}`, `old_barnacle {hullHealth: 1200, hullBluntMult: 2, plankEvery: 300, burnSeconds: 3, nakedSeconds: 12, nakedDamage: 2, nakedStance: 1.5, bellHealth: 800, carapaceHealth: 400, bellSpeed: 0.7, carapaceSpeed: 1.2, fortressSlashPierce: 0.2, fortressSeconds: 6}`, `siren_eel {songWindup: 1.2, songSeconds: 8, tameRadius: 3000, lureRadius: 2000, lurePull: 250, lureDodgeCut: 0.25, interruptDamage: 150, interruptStagger: 3, earplugCut: -18}`, `reef_tyrant {tetherRange: 1000, beachDepth: 200, beachSeconds: 8, beachStance: 2.0, lineSeconds: 20, lineHealth: 600, restHours: 2, boatRam: 400}`; per-attack rows from 2.13 and 2.14; run-back stone positions; lock points |
| `Data/tuning.json` `events` | `kingTide` (the block above, referenced by the event subsystem) |
| `Data/creatures.json` | 11 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `tide`, `resists ["deep_water","rip","blowhole"]`, `swimmer: true` and `depthRange` per species, taming, difficulty, food, attacks, activity, loot; mount `swim` and `rideSpeed` 1100 for Wavecharger; hauler `swim` 600 for Driftback; utility `light` (Glowray), `dig` and `pearl` (Pearldiver); `school: true` for Ripplefin). 4 boss rows (`thalassor` major; `old_barnacle`, `siren_eel`; `reef_tyrant` with `roaming`, `swimmer` and `route`). Floeback row (RAVEN-PLANNED CR) gets `mount: "swim"`, `surfaceOnly: true`, `haulOut: true`, spawning in the tundra |
| `Data/spawn_regions.json` | Ocean regions O1 to O5 with `area` boxes from 2.2, `depth` ranges for the sea zones (O4 0 to 2500, O5 0 to 7000), and `night` tables from 2.12.3 |
| `Data/items.json` | `tidecoral, pearl, sea_glass, sharkskin, shark_tooth, spinecrab_shell, crab_meat, eel_skin, shock_gland, ray_wing, photophore_gland, turtle_hide, frond, ambergris, driftwood, oyster, urchin, kelp_ash, bronze_sheet, tide_essence, core_thalassor, leviathan_pearl, trophy_barnacle_claw, trophy_siren_scale, trophy_tyrant_fin, sluice_key, pearl_circlet, haewons_knife, tidebreaker_bp, coral_keep_decor_bp`; consumables and gear `divers_draught, swim_fins, whalers_harpoon, beeswax_plugs, sunstone_flare, shark_tooth_arrow, eel_waterskin, pearl_ring`; Reefmail pieces (survival-crafting slot split); diving helm (KEPT); lore items (9) |
| `Data/recipes.json` | The 2.19 recipes; Reefmail (KEPT inputs); diving helm (KEPT); Deepbreath and soak-resist brews (KEPT) |
| `Data/pieces.json` | `sun_mirror` (pivot piece, beam), `driftwood_beacon` (light, clan map marker), Coral keep set (KEPT, survival-crafting), `mooring_stone` as a world-only actor (not buildable) |
| `Data/world_pois.json` (fire-frost's file) | Ocean sub-zone polygons, 8 landmarks, 18 land and 9 sea hand sites, 5 shrines, 5 caves, the Drowned Hall rooms and 4 entrances with their open rules, kit rules (2.4), the Reef Tyrant's route spline and rest point, the Tsunami Line stones and contour, rip channel splines, blowhole points (with tide gates), man o' war drift spawners, the Pound's layout (wall, flat, pool, 8 towers, 4 bollards, 3 mirrors, 4 sluices) |
| `Data/quests.json` | Hae-won's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 11 and 4 bosses, each with an `underwater` variant flag; pools `explore.ocean.day/night` (A, B, C with tide stems), `combat.ocean`, `mini.ocean`, `boss.thalassor` (3 sections), `sub.drowned_hall`, `event.king_tide` (with the planned silence), `camp.ocean`, `pass.saltgate`; sub-zone beds O1 to O5; `underwater.bed` (hum, pressure past 20 m, ear-pop at 25 m), `breath.low` (heartbeat), `drowned_bell` |
| `Data/fx.json` | Bubble trails and bubble columns (tells), V-wakes, the slingshot-jaw glint, cavitation pop, shock bands, sun-mirror beam and Sun-dried cracks, the bubble-net ring, Thalassor's photophores and blowhole plume, the bore wave, coral spawning clouds, Noctiluca surf, Flow ribbons, soak drip, harpoon line (rope and chain), Tow spray trail, gill bubble stream, blowhole column, man o' war tentacle shimmer |
| `Data/combos.json` (R1 creates it) | Fire+Tide, Lightning+Tide, Sun+Tide, Nature+Tide, Frost+Tide, Blood+Tide, Tide+Necromancy rows from CB |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `SoakBuildup` (beside Burn and Frost) and `bWaterHit` (if biome-desert hasn't already); `bUnderwater` (attacker or target submerged) for the 2.6 multipliers |
| `WRStatusComponent.cpp/.h` | **Soak:** buildup meter, proc applies Slowed ×0.8 (Heavy water ranks) for 10 s (+ranks), fire and shock buildup multipliers while soaked, Drowning (player stamina drain, creature stance regen stop), cure by fire within 300 cm for 3 s (a fire query against the light registry, LATER-COOKS). Reactions on soaked targets from combat 2.11 |
| `WRAttributeSet` | **Breath** and **MaxBreath** attributes (replicated); a **Flow** resource attribute alongside Heat and Chill (same pattern); MaxFlow from `flow_capacity` |
| `WRCharacterMovementComponent.cpp` | **3D swimming:** dive/rise input, camera-directed underwater movement, fast swim with stamina, the kick-dodge (a root-motion burst with i-frames, replicated as Raven's roll is), wading speed by depth, current forces from `AWRCurrentVolume`. Gills speed multiplier |
| `WRWater.cpp/.h` | `AWRWaterVolume` gains `SurfaceHeight` that can move at runtime (tide, the Pound's flood, King Tide) and a `kind` (sea / lagoon / pool / air-pocket). `UWRWaterSubsystem` gains: the submerged test by head socket, **breath tick and drown damage** (through ApplyHit with `bEnvironment`), **depth below surface and crush damage**, Gills/helm/Reefmail/Abyss-diver checks, air-pocket refills. New `AWRCurrentVolume` (world-map KEPT) used for rips (with the "left by the side" check quest 2 reads) and the Undertow |
| **New `WRTide.h/.cpp`** | `UWRTideSubsystem` (world subsystem, server-driven, replicated time): the tide curve from `tuning.tide` and the game clock, offsets the B2 coastal water volumes and the sea mesh section (`AWRSea::SetMaterialScalar` for the shore line plus the volume height), the edge blend, blowhole live windows, the Tide Clock float, music stem intensity. The King Tide timeline hooks in through the biome event subsystem |
| `WRSea.cpp` | A local surface offset for the B2 coast (material parameter plus mesh section offset) blended to 0 at the biome edge, so the rest of the sea stays still (world-map owns the mesh; this lane adds the offset hook) |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_WaterBolt` (UWRGA_Spell), `UWRGA_RiptidePull` (new Pull shape), `UWRGA_Whirlpool` (AreaSpell Pool with hold), `UWRGA_Maelstrom` (AreaSpell Storm with pull), `UWRGA_HealingWave` and `UWRGA_HighTide` (travelling Wall that heals allies and pushes enemies), `UWRGA_Cleanse` and `UWRGA_TidalShield` (new Ally target shape), `UWRGA_WaveCut` (UWRGA_Melee Wildbrand), `UWRGA_HarpoonThrow` (a thrown projectile with a line actor; follow-up reel, Anchor hold, Tow, Leviathan's hook branch by node). All Tide abilities cost Flow. `ClassForNode`: a new `tide` block mapping the 10 ability node ids. Passives (`gills`, `drowning`, `heavy_water`, `current`) through `UWRPathComponent::HasNode` as Fire's are |
| `WRPathComponent.cpp` | Flow tick: gain from velocity, dodges (the roll/kick-dodge event), perfect dodges (the i-frame hit-avoided event), hits while moving; drain when still; pause during rooted casts |
| **New `WRHarpoonLine.h/.cpp`** | `AWRHarpoonLine`: a replicated cable (rope, chain for Leviathan's hook) between the thrower's hand socket and the hit point or actor. Reel, pull-self (Tow), pin (Anchor), and **tie-off** to an `AWRMooringStone` or a boat cleat (Interact). Line health and timeout. Used by the Whaler's harpoon item too |
| `WRCreature.cpp` | Swimmer locomotion (3D nav in water volumes: a simple volume flight-nav, with depth range clamped per species), surface/underwater state, the attack-row kinds from 3.1. **Thrash-drag:** a paired grab that moves the victim down (motion warping, combat's paired tech), breath held. **Breach strike:** the bubble-column tell spawned at the target 1.0 s ahead. **Surf lunge:** beach onto wet sand, stranded state. **Second jaw:** blockable grab with a drag to the creature's hole. **Shock pulse:** a sphere through the water-volume query (everything in the same water). **Snatch:** removes one food/fish stack server-side into the creature's carried item, drops it on death, flies to the Gullery. **Club snap:** two hits 0.06 s apart. **Shell guard:** a frontal damage multiplier. **Pool slither:** land movement between pools. Wavecharger and Floeback swim mounts (rider input to 3D swim), Driftback haul platform. Schools (Ripplefin) as a boids Niagara plus a few sim actors for the sea pen |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `thalassor`, `old_barnacle`, `siren_eel`, `reef_tyrant`. **Thalassor:** pool-bound swim locomotion in phase 1 with lunges onto the flat; **beached** state; the click/creak tell as a sound event the attack runner fires 0.4 s ahead (and silence before grabs); the sluice transition drives the Pound's `AWRWaterVolume` from flat level to +400 cm over 10 s; phase 2 breathing cycle (blowhole part health); towers as destructible actors; Bubble net (a buoyancy-off zone on `UWRWaterSubsystem` plus the lunge); Undertow (a temporary `AWRCurrentVolume`); phase 3 maelstrom (a rotating current plus the eye damage). **New `AWRSunMirror`**: a pivot actor a player possesses (Interact) and aims with the camera; casts a beam (line trace) in daylight or with a loaded sunstone; accumulates beam time per boss part; Sun-dried part state. **Old Barnacle:** a hull part actor with health and a fire check, naked state, the spare-shell seek and occupancy check. **Siren Eel:** a song state applying `UWRTameControl` hostility override to tames in radius (furthest first) and a Lure input bias to players (a client movement modifier, server-checked), the crest interrupt. **Reef Tyrant:** roaming swim route (fire-frost's route component in 3D), boat ram, tether reaction to `AWRHarpoonLine` tie-offs, beached state |
| `WRWorldDressing.cpp` | Hazard actors: `AWRBlowhole` (tide-gated column with a 1.0 s tell, launch impulse, the Horn variant), `AWRManOWar` (wind-drifted fleets with tentacle overlap volumes), rip channels as `AWRCurrentVolume` splines. `AWRMooringStone` (tie-off points). Kit and hand POI spawning from `world_pois.json`; the Tsunami Line stones; the Beacon Stack beacon (clan-lit state saved) |
| `WRProjectile.cpp` | Spout volley bombs and Old Barnacle's debris (ballistic, never homing, landing decals for the full flight); the thrown Sunstone flare (beam pulse on hit); underwater bolt speed multiplier |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The King Tide timeline: warn (chimes, flocks, the bell, the Ring pillar), sea drop via `UWRTideSubsystem`, stranded spawns and the taming multiplier, pool-bound Reefjaws, Old Barnacle's roam, the Low Gate open, the spawning night, the Return (a bore actor sweeping inland along the coast spline to the Tsunami Line contour, hitting players, creatures and pieces once), payout ×3 |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 ocean shrines (2 at sea, reached by swimming or climbing); Hae-won (MetaHuman, stilt-house and pool idle loops, a dawn dive loop at the Kelp Cathedral); quests, including the rip "left sideways" check and the 15 m depth touch |
| `WRBodyChange.cpp` (fire-frost's) | Tide parameters from hero.md (sheen mask, gill mask scroll on `Underwater`, iris ring, wet-look roughness) at 2.18's thresholds; gill bubble Niagara at 75; Flow drives brightness |
| `WRUIHud` / `WRPlayerUI` | The Flow bar (sea-green, ripple when filling); the **breath arc** round the crosshair and the depth readout (2.6); the soak meter (combat's status UI); a tide indicator on the compass while in B2 (a small wave glyph rising and falling) |
| `WRAudioDirector.cpp` | Ocean pools with tide-driven stem intensity; the underwater low-pass bus for music and voices; Thalassor's 3 sections; the King Tide's planned silence and layer; the drowned bell at midnight high tide |
| `WRSky.cpp` | Sea fog and Squall presets; underwater post-process (blue fog by depth, god rays, caustics); Noctiluca surf (Niagara on breaking waves at night) |
| `WRUIControls.cpp` (ui-controls lane) | New rows on the F1 page for swimming: dive, rise, fast swim, kick-dodge, harpoon reel and tie-off, for pad and keyboard |

### 3.3 Assets and the art route

**Route (KEPT):**
1. Concept, picked by Aaron.
2. Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. **Real clips** from Fab animal packs or Truebones mocap, retargeted with UE's IK Retargeter.
6. Every creature shown standing (or floating) at scale beside the hero before any behaviour is wired.

No engine primitives, no unfinished AI meshes.

| Asset | Route and clip source |
|---|---|
| Reefjaw | Concept from tiger shark plus goblin-shark jaw. Hunyuan base; Blender jaw as a separate bone chain that translates forward (the slingshot), denticle normal detail. Clips: a Fab animated shark pack (swim, fast swim, turn, bite, thrash, death) plus a keyed jaw-shoot layer and a keyed surf-lunge/strand |
| Tidecoil | Concept moray plus electric bands. A spline-chain rig (14 segments) with a separate pharyngeal-jaw bone. Clips: a Fab eel/moray pack if one fits, else keyed in Blender (hole idle gape, strike, coil whip, slither); band emissive driven by the shock state |
| Spinecrab | Concept coconut crab plus king-crab spines plus mantis club. A Fab crab pack skeleton (walk, sidewalk, pinch, burrow, death) with a keyed club snap |
| Gullraider | Concept skua plus frigatebird pouch (a morph target). Clips from a Fab seagull/seabird pack (fly, glide, dive, land, hop, peck, call) |
| Wavecharger | Custom concept (Aaron picks; no pack has it). Seahorse-dragon body on a 10-bone spine with frond groom cards. Clips keyed in Blender (swim cycle, fast swim, tail-curl rest, snap, charge), plus Control Rig secondary for the fronds. A rider socket and saddle (survival-crafting saddles) |
| Driftback | A Fab sea turtle pack (swim, crawl on sand, dig, bite, death) at 5 m with barnacle and weed cards on the shell |
| Glowray | A Fab manta/stingray pack (swim, glide, barrel roll) plus a keyed barb sting; photophore emissive mask |
| Pearldiver | A Fab otter pack (float on back, swim, dive, eat) plus a stone prop and a crack loop |
| Kelpgrazer | Concept Steller's sea cow; a manatee/dugong Fab pack if one fits, else keyed (graze, swim, roll); bark-like skin |
| Tidefowl | A Fab puffin or seabird pack (waddle, fly, burrow) |
| Ripplefin | A Fab fish pack mesh driven by a Niagara boids school, plus 6 sim actors for sea pens |
| Old Barnacle | Concept hermit crab plus a longboat hull (the hull is world-map's ship kit, broken and barnacled). A crab skeleton scaled ×2.5 with a soft-abdomen mesh; clips from the crab pack plus keyed withdraw, naked scuttle and shell-swap |
| Siren Eel | Concept oarfish plus crest (Aaron picks). A 24-segment spline chain (the Gorgeleech/Apepa tech from the swamp and desert cooks) with crest-ray bones and an emissive crest. Clips keyed: undulate, rise vertical, sing, whip, jaw strike |
| Reef Tyrant | The Reefjaw rig ×2.3 with scars, notched fin, a lodged harpoon and a chain (physics) |
| Thalassor | New concept (Aaron picks). Sperm-whale head and jaw from a Fab whale pack's skeleton (head, jaw, flippers) fused to a 30-bone spline trunk with a fluke end; harpoon spines and chain as static meshes with physics chains. Clips: whale-pack swim, breach, dive and tail slap for the front, plus keyed coil, ram, lunge, beached roll and breathing; Control Rig undulation along the trunk with feet-equivalent contact (it lies on the bottom, never gliding: its body rests on the pool floor and flat) |
| Hae-won | MetaHuman preset (attractive, late 30s, athletic freediver), outfit as 2.11; a harpoon prop |
| Landmarks | The Pound (Blender: ring wall modules, sluice gates with a burst variant, towers with destroyed states, bronze bollards and chains), the Drowned Colossus (Ahrumet's twin mesh from the desert lane, coral-overgrown material plus barnacle cards), the Beacon Stack (terrain stack plus a beacon and causeway), the Spouting Horn (a vent sculpt plus a water-column Niagara), the Dyke Head and 11 dyke stumps (one modular tower in 3 ruin states), the Reef Lighthouse, the Bell Tower (with the Drowned Hall interior kit) |
| Kit | Fab coastal and underwater packs: kelp (animated), coral (recoloured to tidecoral orange, pink, cream), sea grass, beach debris, rocks, giant clams, anemones, urchins (licence rule: use unless explicitly barred). Saltkin props in Blender: shell-chime beacons, tsunami stones, harpoon racks, salt pans and rakes, bollards, ladles, votive boats, terrace houses |
| Water | Shore materials by depth (turquoise, teal, ink), the tide line wet-sand band, rip channel material (dark flat gap plus foam flipbook), Noctiluca surf, underwater caustics and god rays, the Pound's lagoon water with a moving surface height |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles, Kenney and NOAA public-domain hydrophone recordings (sperm-whale clicks, humpback song, snapping shrimp: US government works). Layered in a DAW per species, about 8 files per creature and 12+ per boss, each with an underwater-processed twin.
- **Music:** the 2.16 cue sheet chosen from libraries first (about 20 cues counting stems and tide layers), credited.
- **Beds:** 5 sub-zone beds, the underwater bed (with the depth layers), the Drowned Hall bed, the Pound's spray bed, the blowhole booms, and the reef crackle (snapping shrimp, real).

### 3.5 Map dressing

- **The generator.** `Tools/terrain/dressing.mjs` reads the ocean entries in `world_pois.json`:
  - It places hand sites at their coordinates.
  - It fills kit POIs per sub-zone with Poisson spacing of 150 m or more, and sea kit at 6 per km².
  - It walks routes (the coast road, the dyke wall top, boat lanes along the coast, the main dive lines: Kelp Cathedral, Clam Garden, Whale Fall) and adds a kit POI wherever a point is more than 125 m from any POI.
- **Bathymetry** comes from world-map's B2 sea step. This lane paints the depth bands:
  - Turquoise shallows: 0 to 3 m.
  - The kelp shelf: 3 to 25 m.
  - The reef crest: rising to −0.5 m at mean tide.
  - The reef slope: to 35 m.
  - The Blue Wall: a vertical face to 70 m at x +3,500 to +3,700.
  - The open sea floor: 70 to 90 m beyond.
- **Tide marks.** The tide range sets the wet-sand band and the shelf's intertidal material (barnacle line, weed line) from mean ±1.2 m.
- **The Tsunami Line contour** (6 m) is written into `world_pois.json`. The generator never places camps, claims' default sites or the village below it. The King Tide's bore uses the same contour.
- **Ground layers:** silica sand, wet sand, sea grass, coral limestone, salt crust, terrace earth. Painted from sub-zone polygons by the existing auto-material.
- **The landmass itself** comes from world-map's W10 step (the B2 Ocean half and Saltgate). This lane only dresses it.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R6: underwater play and soak.** Dive and 3D swim, the kick-dodge, breath and depth with their UI, underwater combat multipliers, `AWRCurrentVolume`, the soak status and its reactions, the F1 swim rows | Everything below needs it |
| 1 | **Proof of one: the Reefjaw at the reef edge, end to end.** Concept (Aaron picks), mesh with the slingshot jaw, real shark clips, voice (thrum and growl, underwater-processed), attack rows with socket hits (slingshot bite, ram, thrash-drag down with breath held, breach strike with its bubble-column tell, surf lunge onto wet sand), one coral head with tidecoral nodes, one rip channel beside it, and one O5 POI dressed around it (the Impaled Ship) | Proves diving, breath, an underwater grab, the bubble-column tell rule, a beach-stranding attack, visible-contact damage, real clips, a fused voice, the signature resource and a dressed place in one item |
| 2 | **The Tide path:** `tide.json`, the Flow resource and its tick, the 10 ability classes in `ClassForNode` (with `AWRHarpoonLine`, Tow and Leviathan's hook), Gills, the passives, proc FX, the CB combos with Fire, Lightning, Sun, Nature, Frost, Blood and Necromancy | The round's headline; it needs only step 0 |
| 3 | **The tide and the coast:** `UWRTideSubsystem` and the local sea offset, sub-zones O1 to O5, bathymetry bands, the 8 landmarks, 27 hand sites, kit fill, caves, resources (tidecoral, pearls, sea-glass, salt), the Tide Clock | The world the rest stands in |
| 4 | The other 10 creatures with clips, voices and rows; the Wavecharger and Driftback as swimming mount and hauler (and the Floeback in the tundra); day and night tables; living-world beats | Reuses step 1's frame |
| 5 | Micro-hazards (rips, blowholes and the Horn launch, man o' war fleets) and weather (Sea fog, Squall with the open-sea mast rule) | After the coast exists |
| 6 | Old Barnacle (hull and spare shells), then the Siren Eel (song, lure, earplugs, tame turn), then the Reef Tyrant (roaming swim, harpoon tether to the Mooring Stones) | Minis in order of new tech |
| 7 | Thalassor: the Pound, phase 1 on the flat with beaching, the sluice transition and flood, phase 2 with breathing and perches, the Sun mirrors and the Sunstone flare, phase 3 behind the artifact flag; drops; music | The biggest single item; needs creatures, water-level control and the harpoon line |
| 8 | Shrines (2 at sea), Hae-won and her 6 quests, run-back stone, fast travel | After POIs and the hero rig |
| 9 | The Drowned Hall and the six secrets | After diving, the tide and the key quest items |
| 10 | Music set, tide-driven stems, the underwater bus and sub-zone beds | Independent; can slot in anywhere after step 3 |
| 11 | The King Tide event (the Low, the spawning night, the Return and the Tsunami Line) | Needs the tide subsystem, the minis and the Drowned Hall's Low Gate |
| 12 | Gear: Reefmail fit on the MetaHuman bodies, coast items, the Sun mirror and driftwood beacon pieces | After the R2 slot split (survival-crafting) |
| 13 | Body change for Tide | After the MetaHuman hero (R0) |
| 14 | Sun ↔ Tide invasions through Saltgate | Needs both biomes, Old Barnacle and the invasion frame |

**Gates:** compile, cook and package only. Aaron tests each step.
