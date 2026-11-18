# Cook: biome-fungal (Fungal caverns / Spore, round R8)

Lane: biome-fungal. Written 2026-11-13. Scope: FULL-GAME 3.2.11, the Fungal rows of 3.3, and the Spore row of 4.2. This covers the whole Fungal caverns biome and the whole Spore path.

**Tags.** **SPEC** keeps Raven's design, or a built thing, exactly as written. **GAP-COOKED** is this lane's design and replaces the FULL-GAME first draft. **KEPT** means a number or name another 11-13 cook already set (world-map, combat, survival-crafting, hero, biome-sky); it's restated here so the build has one page, and this lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** the biome is the **west half of belt B1, the Hollow Cape**, mostly underground. FULL-GAME's "south-south-west" is replaced. 1.5 km² of surface; about 3 km² of caverns below B1 and the north edge of B2. Neighbours are the Desert (south, a karst field of sinkholes) and the Sky islands (straight above). The opposite, Wind, is joined by **the Flue**, a vertical shaft at (0, −8,800), floor −40 m, rim 700 m. Camps sit on sinkhole rims at (−1,800, −8,300), (−1,650, −8,900) and (−1,100, −9,300). Mycelorn's Heart chamber is at (−600, −8,600), −220 m, with the Heart Grotto ring around it. POI count is 115 (35 surface + 80 in the caverns on 12 km of route). Surface weather: Clear 30, Overcast 20, Rain 20, **Spore drift** 30. The early pass is the **spore wad** (5 min, buildup ×0.3). The old people are the **Deepfarmers**, whose waymark is **glow-cap rings**; their champion is the Paper Titan on the highest sky island, and the Kitewright champion lies wrapped in mycelium on the Flue floor. Cavern glow follows the surface clock. No claims or bases underground (DD).
- **combat.md:** the poison and rot statuses, every Spore spell number, the Infect strike Wildbrand, Detonate, the attack-row schema, the boss template, visible-contact damage, colour law, and the reaction "Wind on spore clouds pushes the cloud 6 m".
- **survival-crafting.md:** the Spore bloom weather, the filter mask, Sporeweave, Oyster cap and Lion's mane (dark crops), Capling and Puffball farm yields, the Glowfin, Cave crayfish and Mycelial Koi, the Mycelium vat and Mycelium den, the three fungal farm set-pieces, the trophy talismans (Mother's cap, Rot crown, Weaver silk), Tunnel toad (12 m leaps), Shroomback (carry 220, grows oyster caps), the dark-hunter rule (Rotmaw, Webspinner), Glowgrub (12 m light aura).
- **hero.md:** Spore colour bioluminescent cyan #39E6C2, the Sporeweave silhouette, and the Spore body-change row.
- **biome-sky.md:** the Kitewright story from the sky side, Aeolith's spore weakness and the **three spore plumes exhaling up the Flue every 40 s** (this lane keeps them and builds their source), and the Wind side of the invasion.

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Ark: Aberration** ([ark.wiki.gg: Aberration](https://ark.wiki.gg/wiki/Aberration), [PCGamesN](https://www.pcgamesn.com/ark-survival-evolved/ark-aberration-horror), [wikily.gg map](https://wikily.gg/ark-survival-ascended/maps/aberration/)) | A whole map underground: a safe-ish green surface, then bioluminescent mushroom forests, then the deadly Element region. Only **charge light** (from shoulder pets like the Bulbdog and Featherlight) stops the Nameless spawning; ordinary torches don't | **Depth is difficulty, light is a choice.** Bands run by depth (world-map KEPT). Our twist on charge light: **fire and cold light are different tools.** A torch burns spore clouds out (a dust flash) but draws Mycomites, which hunt warm light. Cold light (Glowgrub, glow-cap lantern) is safe in clouds but burns nothing. The player always picks which light to carry |
| **Monster Hunter World, the Rotten Vale and Vaal Hazak** ([Dualshockers](https://www.dualshockers.com/monster-hunter-world-videos-rotten-vale/), [RPG Site: effluvia](https://www.rpgsite.net/feature/6661-monster-hunter-world-effluvia-how-to-deal-with-the-toxic-gas-in-rotten-vale), [MH Wiki: Vaal Hazak](https://monsterhunter.fandom.com/wiki/Vaal_Hazak)) | A valley under the Coral Highlands that eats whatever falls into it. The effluvium gas lives on the dead, and the elder dragon lives in symbiosis with it. One ecological idea drives the whole map | **One idea feeds everything: the Deep eats what falls.** Carcasses drop through skylights and down the Flue. Deepcap fruits only on the dead. Rotmaws and Sporestalkers scavenge the falls. The Deepfarmers fed their own dead to the Heart, and at the end, themselves. Every creature, hazard, resource and POI comes from that |
| **Enshrouded, the Shroud** ([pindrop.gg](https://pindrop.gg/enshrouded/guides/shroud), [GameSpot](https://gamespot.com/articles/enshrouded-survive-shroud-effects-guide/1100-6520563/), [TechRaptor](https://techraptor.net/gaming/guides/enshrouded-shroud-survival-guide)) | A toxic fog with a visible timer, deeper fog drains faster, and the Flame level gates how deep you can go | **The hazard is the progression gate**, but Raven bans meters (DD "hazards are gear checks, no temperature meter"). So we keep the gear check (filter mask, Sporeweave, Immune system) and **show the danger on the body**: cyan motes cling to your arms and face as poison builds, and the status meter is the only readout. Clouds are **pockets**, never a whole-map fog |
| **Valheim, the Mistlands** ([PC Gamer](https://pcgamer.com/valheim-mistlands-update-new-biome), [TechRaptor guide](https://techraptor.Net/gaming/guides/valheim-mistlands-biome-guide), [GamesRadar](https://www.gamesradar.com/valheim-mistlands-update)) | Heavy mist plus very vertical, broken ground. Critics called the pairing bad design; a torch does nothing, and only the Wisplight clears the mist | **Rules:** (1) Never stack blinding fog on vertical terrain: inside a spore cloud you always see at least 12 m, and clouds never sit over ledges or jumps. (2) A tool must answer the fog: wind (path, gusts, Gale flask), fire (flash it), frost (settle it). (3) Spore bloom is 3 minutes, never permanent (survival-crafting KEPT) |
| **Elden Ring: the Lake of Rot, Siofra and Nokron** ([wiki.gg: Lake of Rot](https://eldenring.wiki.gg/Lake_of_Rot), [Fextralife: Lake of Rot](https://eldenring.wiki.fextralife.com:443/Lake+of+Rot), [TheGamer: best areas](https://www.thegamer.com/elden-ring-best-looking-areas-locations/), [wiki.gg: Siofra](https://eldenring.wiki.gg/wiki/Siofra_River)) | Scarlet rot deals % max HP + flat per second and is lethal. Siofra and Nokron give the underground a **false starry sky**, the most praised view in the game | Rot already uses % max HP + flat (combat KEPT). The **False Sky** in the Glow-moss Halls is our Nokron: a 400 m ceiling of glowing threads that is beautiful, and is also a Webspinner hunting ground (the threads are snares). Wonder and threat in one image |
| **Dark Souls, Darkroot Garden; Blighttown** ([Fextralife: Mushroom People](https://darksouls.wiki.fextralife.com/Mushroom+People), [Fextralife: Darkroot](https://darksouls.wiki.fextralife.com/Darkroot+Garden)) | Big mushroom men hit with slow, crushing wind-ups; the little ones wander harmlessly. Blighttown is hated for poison plus slow plus vertical clutter | Our fungal creatures are **animals with fungus on and in them**, not walking mushrooms with faces. Lesson kept from the swamp cook: **never slow and status on the same ground** |
| **Hollow Knight, Fungal Wastes** ([hollowknight.wiki](https://hollowknight.wiki/w/Fungal_Wastes)) | Bounce mushrooms turn the cave into a movement playground | **Springcaps:** real pressure-cap mushrooms (Pilobolus, below) that launch you 9 m up when you jump on them. Fast, fun vertical routes that skip ladders. They're the swamp's boardwalks turned upward |
| **Ghost of Tsushima, Horizon** (world-map KEPT) | Wind and creatures lead you to places | **Glow-cap rings** (the Deepfarmer waymark) brighten in sequence toward the nearest undiscovered POI when you stand in one at night. Puffballs drift toward warm air, so they float up toward skylights and the surface, and following them gets you out |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| *Ophiocordyceps*, the zombie-ant fungus ([AntWiki: Summiting](https://www.antwiki.org/wiki/Summiting_Behaviour), [The Scientist](https://www.the-scientist.com/zombie-fungi-hijack-hosts-brains-72271), [EarthSky](https://earthsky.org/earth/zombie-ants/)) | An infected ant climbs vegetation ("summit disease"), locks its jaws on a leaf vein around **solar noon**, and can't let go. Its jaw muscles atrophy. A stalk grows from its head. 48-million-year-old leaves show the bite scars | **The summited.** Every dead Deepfarmer is found **high**: on rooftops, rack tops and stair heads, jaws clamped on a root, face toward the Heart. A white stalk rises from the back of each skull. Nobody explains it. The **Sporestalker**'s crest is that stalk. Mycelorn's tame-punish move "summits" a tame |
| Armillaria, the humongous fungus ([Guinness](https://www.guinnessworldrecords.com/world-records/66481-largest-fungi), [Big Think](https://bigthink.com/strange-maps/humongous-fungus-in-oregon/), [USFS PDF](https://webarchive.library.unt.edu/web/20170225001506/http://www.fs.usda.gov/Internet/FSE_DOCUMENTS/fsbdev3_033146.pdf)) | One individual covers about 965 ha in Oregon and is 2,400 to 8,650 years old. It spreads by black bootlace **rhizomorphs** and shows itself only as brief autumn honey mushrooms | **Mycelorn is one organism the size of the biome.** Everything glowing is it. Its black rhizomorphs are the cables of the **Shoestring Bridge** and the **rhizomorph cord** resource. The boss is just the fruiting body |
| Bioluminescent fungi, foxfire, circadian glow ([Dartmouth 2015](https://geiselmed.dartmouth.edu/news/2015/researchers-discover-why-some-mushrooms-glow/), [ScienceDaily](https://sciencedaily.com/releases/2015/03/150319123956.htm), [ASU Ask a Biologist](https://askabiologist.asu.edu/plosable/mushroom-light)) | About 71 species glow, on a circadian clock that lights them at night to draw insects. Foxfire is glowing honey-mushroom mycelium in rotten wood | Cavern glow follows the surface clock (world-map KEPT). **Foxfire** glows on rotten wood on the surface at night. Glow draws prey: Webspinner lures, the Rotmaw's fake deepcap |
| Waitomo glowworms, *Arachnocampa* ([Wikipedia](https://en.wikipedia.org/wiki/Arachnocampa_luminosa), [Scientific American](https://www.scientificamerican.com/blog/artful-amoeba/the-glowing-spider-worms-of-new-zealand/), [PBS Deep Look](https://www.pbs.org/video/deep-look-carnivorous-worms-mimic-night-sky)) | A fungus-gnat larva hangs up to 30 to 70 sticky silk threads from a cave ceiling and glows to lure insects, then reels them up. It needs still, humid air | **Lure lines** (micro-hazard) and the **Webspinner**: glowing beaded threads from the ceiling that read as stars. Touch one and you're stuck; the spider reels. Still air is the rule: wind (Wind path, gusts, Gale flask) tears the lines down |
| Hang Son Doong ([National Geographic](https://www.nationalgeographic.com/magazine/article/vietnam-cave), [Discover Wildlife](https://www.discoverwildlife.com/environment/hang-son-doong-cave)) | Two collapsed dolines let light into the world's largest cave; a jungle grows on the floor; clouds form inside over the underground river | The **Spore Gardens** are daylit dolines with a fungal jungle on their floors. **The Cloud Room** makes its own rain under the ceiling |
| Movile Cave ([Wikipedia](https://en.wikipedia.org/wiki/Movile_Cave), [GESS lab](https://gesslab.org/movile-cave)) | Sealed for millions of years; an ecosystem built on chemosynthesis; air bells over flooded passages with 7 to 10% oxygen and ~100× the CO₂ | **Foul air** micro-hazard in the Flooded Grottos' air bells. The real miner's tell: your flame shrinks and turns blue |
| *Physarum* and the Tokyo rail map ([National Geographic](https://www.nationalgeographic.com/science/article/slime-mould-attacks-simulates-tokyo-rail-network), [Scientific American](https://www.scientificamerican.com/blog/observations/slime-mold-validates-efficiency-of-tokyo-rail-network/)) | A slime mould grew a network as efficient as Tokyo's railways by strengthening the paths that worked | **Mycelorn's phase 2 tell.** The floor's glowing veins pulse along the shortest path toward where it will surface. You read the map it's drawing |
| *Pilobolus* and dung-cannon fungi ([Discover Wildlife](https://www.discoverwildlife.com/plant-facts/fungi/pilobolus-crystallinus-dung-cannon), [Gizmodo](https://gizmodo.com/this-poop-loving-fungus-accelerates-faster-than-a-bulle-1443409496)) | A fluid bladder builds pressure until it fires its spore packet at about 20,000 g | **Springcaps** (the launch pad) and the Sporestalker's **stalk volley** |
| Fairy rings ([Woodland Trust](https://www.woodlandtrust.org.uk/blog/2019/08/what-is-a-fairy-ring), [Almanac](https://www.almanac.com/magic-fairy-rings-what-they-are-and-how-they-form), [Northern Woodlands](https://northernwoodlands.org/outside_story/article/fairy-rings)) | German *Hexenringe* mark witches' Walpurgis dances; Celtic and Scandinavian rings are elf dances that ensnare whoever steps in; French rings were guarded by **giant bug-eyed toads** that cursed intruders | **The Ring of Caps** (a landmark), guarded by Tunnel toads that sit in it. **Mycelorn surfaces inside a ring of caps that springs up 1.2 s before it erupts: never stand in the ring.** Folklore becomes the tell |
| Cornish knockers ([Exeter, Institute of Cornish Studies](https://www.exeter.ac.uk/research/centres/ics/research/cornish-diaspora/knockerstommyknockersandthequestionofghosts/), [Wikipedia](https://wikipedia.com/wiki/Tommyknocker), [Folklore Thursday](https://folklorethursday.com/tag/piskies)) | Mine spirits who knock to lead miners to ore and to warn of cave-ins. Miners set clay figures in the tunnels as guardians | The **Truffle mole** knocks on walls where ore or a hidden passage is. **Cap-fall** ceilings knock before they drop. Deepfarmer **knocker idols** (clay, stick eyes) stand at junctions |
| Olm, *Proteus anguinus* ([Biology Letters via USF](https://digitalcommons.usf.edu/kip_articles/2031), [Frontiers](https://www.frontiersin.org/journals/ecology-and-evolution/articles/10.3389/fevo.2022.874123/pdf)) | Blind, pigmentless, lives 100 years, survives 10 years without food, keen hearing and smell | Cave creatures are **pale and eyeless, with huge nostrils and ears**: the Tunnel toad's pink-white skin and buried eyes, the Sporestalker hunting by sound. Sneaking (built skill) matters down here |
| Fungal facts used for looks | Tinder fungus (amadou, carried by Ötzi); veiled-lady stinkhorn's lace skirt; spore-print colours (white, cream, pink, brown, black); shelf fungi; cave saltpetre (nitre crusts mined for gunpowder) | **Amadou** tinder (fire without flint), the **Spore Archive**'s jars of spore prints, shelf-fungus stairs, the **Saltpetre Walls** (a source for R9's black powder) |

### 1.3 Lessons, short

1. **One idea feeds everything:** the Deep eats what falls.
2. **Every threat has a readable tell.** A lone deepcap in a clearing with no bones round it is a **Rotmaw lure**. Starry ceilings mean **lure lines**. Knocking means a **cap-fall**. A ring of caps springing up means **Mycelorn is coming up through it**. A dimming blue flame means **foul air**.
3. **Light is a choice, not a fix:** fire burns clouds and draws mites; cold light is safe and burns nothing.
4. **Clouds are pockets with three answers:** wind pushes, fire flashes, frost settles. Never area-wide fog; never over a jump.
5. **Never slow and status on the same ground** (swamp lesson kept).
6. **Fast vertical movement:** springcaps and Tunnel toads make the caverns quick for a veteran.
7. **Colour law:** crimson (#DC143C) stays the only unblockable cue. Spore light is cyan #39E6C2 (KEPT), the poison meter green, the rot meter rust (KEPT). Cave glow never uses crimson.
8. **No zombies (DD).** The summited are dead and stay dead. Bloomed beasts in the event are living animals carrying a fruiting body; nothing rises.

---
## 2. Design

### 2.0 Scope map

| FULL-GAME item | This lane |
|---|---|
| Path theme: poison clouds, rot, regrowth | SPEC (DD) |
| Placement | KEPT (world-map); replaces "south-south-west" |
| Look, sub-zones, landmarks, POIs, hidden areas, caves | GAP-COOKED 2.1 to 2.6 (the four CAL sub-zone names kept, one surface sub-zone added) |
| Signature resource deepcap | GAP-COOKED 2.7 (name KEPT) |
| Hazard: spore clouds | 12 poison buildup a second and rot after 10 s KEPT (3.3); behaviour, tells and answers GAP-COOKED 2.8 |
| Weather | Surface weights KEPT (world-map), Spore bloom KEPT (survival-crafting); effects reconciled 2.9 |
| Event: the Sporefall | GAP-COOKED 2.10 |
| Shrine, path master | RAVEN-PLANNED exist; who and where GAP-COOKED 2.11 |
| 10 creatures | Names, roles and taming KEPT from the FULL-GAME draft (combat, survival and sky lanes use them); bodies, attacks, voices, tables GAP-COOKED 2.12 |
| 3 minis, 1 major | Names and premises kept; fights GAP-COOKED 2.13, 2.14 |
| Voices, music | GAP-COOKED 2.15, 2.16 (tempo, key, feel SPEC 3.3) |
| Spore tree | Nodes SPEC (ST). Spell and Wildbrand numbers KEPT (combat). Resource, costs, stat nodes, passives GAP-COOKED 2.17 |
| Body change | Frame KEPT (hero); stages GAP-COOKED 2.18 |
| Gear | Sporeweave, filter mask, spore wad KEPT; fungal items GAP-COOKED 2.19 |
| Story through environment | GAP-COOKED 2.20 |
| Invasions Wind ↔ Spore | Route KEPT (world-map), Wind side KEPT (biome-sky); Spore side GAP-COOKED 2.21 |

**Roster count.** FULL-GAME lists 10 creatures; the brief says about 9. The 10 stay: combat's ambusher template already names the Rotmaw, survival-crafting uses all 10 (mounts, haulers, farm yields, light tames), and Raven's own rule lets biomes lean ("biomes don't need every role", CR). Split: 4 aggressive, 3 neutral, 3 passive.

### 2.1 Where it sits and how it reads (KEPT placement, GAP-COOKED look)

**Placement (KEPT, world-map 2.2):**
- Belt B1, west of the Spine, under the sky islands. Surface y −9,450 (the cape's north coast) to −8,000 (the Desert border); x −2,300 (west sea cliffs) to about −200 (the Spine's foot below the Flue).
- **The footprint is 2.6 km², of which 1.5 km² is walkable ground** (the KEPT surface figure). The other 1.1 km² is open dolines and collapse: you look straight down into the caverns. That's what makes it read as "the land with holes in it" from the sky islands above.
- **Caverns:** about 3 km² on three levels, under the whole cape and 300 m south under the Desert's Haboob edge, where the Desert's Sieve Throats join them (KEPT, biome-desert).
- **Elevations** (metres above sea level, used for every cavern number below): surface plateau +40 to +140; Upper Halls +80 to +20; Middle Deep +20 to −100; the Deep −100 to −220. The Flue floor is −40 (KEPT), the Heart chamber −220 (KEPT).
- **Bands by depth below the local surface (world-map KEPT):** lowland 0 to 60 m (the surface and the Upper Halls), midland 60 to 180 m, highland deeper (the Webs and the Heart Grotto).
- **Camps (KEPT):** C1 (−1,800, −8,300) on the Desert side, C2 (−1,650, −8,900) on the west plateau, C3 (−1,100, −9,300) on the north tip. Each sits on a sinkhole rim with a newcomer wreck in the cove below (world-map rule). Players build on the surface only (DD: no underground bases).

**The look in one line.** A green-grey karst plateau on a sea cape, pocked with round sinkholes, under a sky full of floating islands. At night the holes glow cyan from below. Climb down and you're in a world of giant pale mushrooms, black root-cords as thick as ships' cables, glowing moss, still teal water, and threads of light hanging from the ceilings like stars.

**Read rules (GAP-COOKED):**
- **The surface reads by its holes.** Every doline rim has a Deepfarmer cap-terrace or a glow-cap ring, so you see the ways down from 300 m.
- **Underground reads by light colour, one per sub-zone:** daylight-gold shafts (Gardens), cyan moss under white starfields (Halls), black and amber (Webs), teal water glow (Grottos), warm city amber (the Heart Grotto). You always know where you are.
- **Every hall has a far light.** No hall is lit evenly: each has one bright source at its far end (a skylight, a glowing cap, a fall of sand in a sunbeam) so you always have a direction (Elden Ring's lit-goal rule).
- **Ceilings are always visible.** Glow on the ceiling is how players judge a cavern's size; no black void overhead.
- **Night (KEPT world-map):** cavern glow peaks at surface midnight and dims at noon. The surface's night light is the cyan glow rising from every doline, plus foxfire on rotten wood.

### 2.2 Sub-zones (GAP-COOKED; the four CAL names kept, plus the surface)

| # | Sub-zone | Where (x, y m; elevation) | Band | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|---|
| F1 | **The Sinkhole Rim** (surface) | The whole plateau, 1.5 km² walkable. Camps C1 to C3 | Lowland | Salt-grass and sea thrift on limestone pavement (real clints and grykes: blocks split by deep cracks), mossy boulders, wind-bent juniper, foxfire on deadwood, fern-skirted doline rims. Sea cliffs on the west and north | Limestone grey #A7A498, salt-grass green #7E8C5A, sea slate #4E5F66; cyan doline glow at night | Doline rims, Deepfarmer cap-terraces, glow-cap rings, granary towers, drying racks, puffball rookeries on the cliffs | Sea wind, gulls, the hollow hum of air breathing in and out of the dolines, sky-island chains creaking far above |
| F2 | **Spore Gardens** | Under the doline field, x −2,100 to −1,100, y −9,300 to −8,100; +80 to +20 | Lowland | The floors of collapsed dolines, open to the sky: a jungle of giant caps 4 to 25 m tall in shafts of daylight, ferns and moss where the light lands, Deepfarmer log-bed terraces on the walls, springcap groves | Daylight gold #E8C878, cap ivory #E6DCC6, moss green #5E7A3A, gill pink #C88A8A | Giant caps (parasol, ink cap, oyster shelves), log beds, Capling meadows, springcaps, puffball fields | Drips, wind across the doline mouth far above, birdsong falling in from the surface, Capling squeaks |
| F3 | **Glow-moss Halls** | West and south, x −2,100 to −1,000, y −8,600 to −7,700 (reaching under the Desert edge); +20 to −100 | Midland | Vast dark halls 40 to 70 m high, floors carpeted in cyan glow-moss, flowstone, stalagmite forests, shelf-fungus stairs up the walls, lure threads hanging like stars, sand falls from the Desert's sinkholes | Glow-moss cyan #39E6C2 (KEPT path colour), flowstone cream #D8CBB0, cave black #121416, starthread white #F2F6F0 | Stalagmites, shelf-fungus stairs, lure-thread ceilings, knocker idols, saltpetre crusts, Glowgrub colonies | Huge reverb, single drips with long tails, a faint high whine from the lure threads in moving air, Webspinner clicks far off |
| F4 | **Mycelium Webs** | Under the Spine side, x −1,100 to −250, y −8,500 to −7,800, wrapping the Heart Grotto on its south and west; −80 to −200 | Highland | Chasms spanned by black rhizomorph cables 0.3 to 3 m thick, white mycelium sheets like torn sails, Webspinner silk, cocoon larders. The floor is often 50 m down. Amber light leaks through fissures from the Heart Grotto | Rhizomorph black #1A1714, mycelium white #E8E4DA, Heart amber #E8A040, silk grey #B8B8B0 | Rhizomorph bridges, mycelium sheets, cocoons, the Titan Nursery scaffolds, summited bodies on cable tips | Cables creaking under weight, a slow deep pulse (the Heart, 35 a minute, half the music's tempo), silk rustle, far wind from the Flue |
| F5 | **Flooded Grottos** | East toward the Flue, x −1,100 to −50, y −9,300 to −8,300; 0 to −60, plus the Flue floor at (0, −8,800), −40 | Midland | The underground river and its lakes: clear teal water over white sand, cenote pools under small skylights, air bells, rimstone dams, Tunnel toad leap chasms. Ends at the Flue floor (the Fall Field) under daylight 740 m above | Water teal #1E8C86, rimstone cream #E0D4BC, toad pink #E8C8C0, Flue daylight white-blue #DDE8F0 | Rimstone pools, Deepfarmer weirs and vats, bronze diving bells, toad colonies, the Fall Field's bones | Running water, echoing drips, toad calls; at the Flue floor, the shaft's slow roar of air |

**Hidden sub-biome: the Heart Grotto** (2.5), a ring round the Heart chamber at −180 to −220.

**Blends (world-map rule 5):** 60 to 120 m between sub-zones, mostly vertical underground: the Gardens' daylight fades down the doline walls into the Halls' moss over about 60 m of descent. **Desert border:** the Sand Falls (L8), where dunes pour into the Halls through the Desert's Sieves (KEPT). **Sky border:** the Flue only (KEPT); spore crust creeps up the shaft walls toward the islands.

### 2.3 Landmarks (GAP-COOKED; 8, about 7% of 115, the world-map mix)

Three on the surface (each read from 1 km) and five underground (each read across its hall from 300 m or more).

| # | Landmark | At (x, y; elevation) | What it is | Reads at night as |
|---|---|---|---|---|
| L1 | **The Ancestor Cap and the Breath** (super landmark, reads from 3 km, world-map rule) | The Great Doline, (−620, −8,580), a sinkhole 260 m across directly over the Heart | A single honey-gold mushroom **90 m tall** grows out of the Great Doline and stands 70 m above the rim, its 60 m cap glowing from the gills. From fissures round its stem a slow column of warm air carries glowing spores **400 m** up: **the Breath**, the Heart breathing through the rock. From the Sky biome you look straight down on it | A cyan-gold column of drifting light over a glowing cap, seen from every B1 and B2 coast and from the islands above |
| L2 | **The Kneeling Titan** | West cliffs, (−1,950, −8,950) | A Deepfarmer mushroom titan 35 m tall, dead and stone-hard, kneeling at the cliff edge with one arm sunk to the shoulder in a doline. Its gill-ribs are open like a broken birdcage; its cap is split by a Kitewright spear 12 m long | Foxfire glows along the cracks in its body |
| L3 | **The Ring of Caps** | Central plateau, (−1,350, −8,250) | A fairy ring **200 m across** of white caps 6 to 12 m tall in a perfect circle. Inside, seven Deepfarmer standing stones carved with spiralling mycelium and a flat altar. Tunnel toads sit inside the ring at night (the French toad-guardian legend) | The gills glow faintly cyan: a perfect halo on the plateau |
| L4 | **The False Sky** | Starfall Hall (F3), (−1,600, −8,000), −60 | A hall 400 × 250 m, ceiling 70 m up, covered in hundreds of thousands of glowing beaded lure threads: a starfield with a "milky way" along a ceiling crack. The Weaver's lair (2.13.3) | Always lit by itself; brightest at surface midnight |
| L5 | **The Shoestring Bridge** | F4, (−900, −8,250), −140 | One black rhizomorph cable 3 m thick spanning a 120 m chasm 60 m deep in a sagging curve, wrapped in Webspinner silk. Smaller cables fan off it like a suspension bridge's hangers | Amber Heart-light from below outlines it; the silk catches it |
| L6 | **The Fall Field** (Flue floor, the pass) | (0, −8,800), −40 | The bottom of the Flue: a round floor 250 m across under a shaft of daylight 740 m tall. Everything that ever fell from the sky lies here: birds, sky beasts, kites and people, all furred white with mycelium. At its centre the **Kitewright champion** (world-map SPEC): a winged skeleton 9 m tall wearing a great kite rig, spread-eagled, wrapped in mycelium cords, a Deepfarmer bronze sickle in its ribs. Three **Breath vents** (2.8) exhale spore plumes up the shaft every 40 s (biome-sky KEPT) | Moonlight straight down the shaft; the plumes glow cyan as they rise |
| L7 | **The Drowned Granary** | F5 lake, (−500, −9,050), −30 | Nine round Deepfarmer silo-towers grown from shelf fungus, standing half-sunk in the largest underground lake (400 m long). Rope bridges join the tops. The silos are full of grain gone to black dust | Glowfin shoals light the water round the towers |
| L8 | **The Sand Falls** | F3, (−1,200, −7,850), −20 | The underside of the Desert's Sieves (KEPT): dunes pour through three skylights as slow golden falls of sand 80 m tall, building dunes inside the cave | By day three golden shafts full of falling sand; by night moonlit grey threads |

### 2.4 POIs: 35 surface + 80 cavern = 115 (count KEPT from world-map; contents GAP-COOKED)

**Surface 35:** 3 camps, 3 landmarks (L1 to L3), 1 lesser shrine (the Ring of Caps), 6 descents, 8 hand sites, 14 kit POIs.
**Caverns 80 (12 km of route, one per 150 m):** 5 landmarks (L4 to L8), 4 shrines (the Great Shrine and three lesser), 3 mini lairs, Mycelorn's arena, 10 Heart Grotto rooms (2.5), 18 hand sites, 39 kit POIs.

**Descents (every one is a POI):**

| # | Descent | At | From → to | How |
|---|---|---|---|---|
| D1 | **The Garden Doline** | (−1,500, −8,700) | F1 → F2 Great Shrine | A spiral Deepfarmer path cut into the doline wall, 60 m down, cap-terraces on every turn. The newcomer's way in |
| D2 | **The Long Stair** | (−1,250, −8,450) | F1 → F2 → F3 | A stair 180 m long cut down a fissure, the steps worn into troughs by bare feet going down. Knocker idols at every landing |
| D3 | **The Brood Doline** | (−1,850, −9,150) | F1 → F2 (the Sporemother) | Rope ladders down a 45 m wall; the Sporemother's lair is its floor |
| D4 | **The Springcap Shaft** | (−1,050, −8,900) | F1 → F2 → F5 | A narrow shaft lined with shelf fungus and springcaps: climb down shelf to shelf, or ride springcaps up from the bottom in 4 bounces |
| D5 | **The Sea Cave** | West cliff, (−2,280, −8,600) | Coast → F5 river mouth | The river leaves the caverns here: wade or swim in. Toads at the mouth |
| D6 | **The Great Doline rim path** | (−620, −8,580) | F1 → F4 | A ledge path spiralling round the Great Doline under the Ancestor Cap, down to the Webs at −80. The highland route; a spore cloud on every third ledge |
| — | The Desert's Sieves and the Flue from the sky (KEPT) | | | External routes: walk in from the Desert, or glide or Featherfall down the Flue |

**Surface hand sites (8):**

| # | Site | At | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| S1 | **The Down-Stair Terraces** | (−1,480, −8,760), D1's rim | Deepfarmer cap-farm terraces in perfect order. Tools hung neatly, doors propped open, a meal laid on a table and never eaten. In the clay of the path down, a crowd of bare footprints set hard in flowstone, **every one pointing down** | Oyster cap spawn, Deepfarmer sickle (tool), lore item "Terrace tally" |
| S2 | **The Knocker Cairn** | (−1,260, −8,430), D2's mouth | A cairn of clay knocker figures with stick eyes (Cornish lore) at the top of the Long Stair, all facing down into it, as if set there to keep something below | Clay; "Knocker idol" (talisman, 2.19) |
| S3 | **The Drying Yard** | (−1,700, −9,050) | Mushroom drying racks 5 m tall, the caps on them dried to grey paper. A Deepfarmer is clamped at the top of the tallest rack, jaw locked on the crossbar, a white stalk out of the back of the skull, face toward the Ancestor Cap: the first of **the summited** | Dried caps (food), amadou |
| S4 | **The Grain Towers** | (−1,600, −8,350), near C1 | Three round granaries on stone feet, empty, their ladders taken down and laid neatly beside them. Whatever they stored, they carried below | Rope, a bronze measure |
| S5 | **The Puffball Cliffs** | (−2,250, −9,250) | Puffballs drifting in a rookery on the sea updraft, tethered by thin cords to the cliff fungus. Burst one and its cloud drifts out to sea | Spore sacs; Puffball taming |
| S6 | **The Fallen Kite** | (−1,050, −8,650) | A Kitewright war-kite (cloth and bamboo, lung ta prayer cloth on its tail) crashed nose-first on the pavement. The flyer's bones are still strapped in, a Deepfarmer bronze sickle through the ribs: the war reached the surface too | Bamboo, cloth, lore item "Kite harness token" |
| S7 | **The Ringstones** | Inside L3 | Seven standing stones and an altar. At night the Tunnel toads sit in a circle inside the ring and croak in turn. Step in and they turn to face you; attack one and they all leap (herd rule) | Tunnel toad taming (sneak up on a sleeping toad by day) |
| S8 | **The Breath Vents** | (−700, −8,700) | A field of fissures round the Great Doline breathing warm, spore-laden air out and in on a slow cycle (60 s out, 60 s in: real cave breathing). Surface spore drift starts here | Deepcap (rare, on bones in the fissures); spore clouds on the out-breath |

**Cavern hand sites (18):**

| # | Site | Sub-zone, at | What the player finds | Pays |
|---|---|---|---|---|
| C1 | **The Offering Niches** | F2, (−1,520, −8,720), +40 | Under the Great Shrine: a wall of 200 niches, each holding the folded bones of one person, furred with mycelium and crowned with a glowing **deepcap**. The first deepcap the player sees grows on the dead | Deepcap ×3 |
| C2 | **Garden Farm** (KEPT set-piece, survival-crafting "Mushroom farm") | F2, (−1,400, −8,650) | Log beds in rows, a Capling pen with its gate open, a spore mask on a hook | Oyster cap spawn, Capling taming, filter mask recipe scrap |
| C3 | **Springcap Grove** | F2, (−1,150, −8,950) | A doline floor full of springcaps; a route of 5 bounces up to a ledge 40 m up with a Deepfarmer cache | Bronze, a Glowgrub egg |
| C4 | **The Cloud Room** | F2, (−1,800, −8,500), +30 | A doline hall where warm river air meets cool doline air: a real cloud hangs under the ceiling and rains on a cap jungle (Son Doong). Rainbows in the daylight shaft | Wild Lion's mane, fern |
| C5 | **The Lure Gallery** | F3, (−1,750, −8,300), −20 | A long low gallery whose ceiling is thick with lure threads. Hung in silk at different heights: Glowgrubs, a Capling, a Desert jackal, a newcomer's satchel | The first Webspinner; satchel loot |
| C6 | **The Dunes Within** | F3, under L8 | Dunes inside a cave. Half-buried in them, a Desert caravan that fell through the Sieves: a Dromad skeleton, pots of sunstone, a Sunwatch gnomon (border contact) | Sunstone ×2, glass |
| C7 | **Knockers' Junction** | F3, (−1,350, −8,150), −40 | A four-way junction with a clay knocker idol facing down each tunnel. A Truffle mole nest in the wall. A tame mole knocking here finds a hidden fifth tunnel (secret F-S2) | Truffle mole eggs |
| C8 | **The Spore Archive** | F3, (−1,900, −8,050), −50 | A Deepfarmer room of shelves holding thousands of sealed clay jars, each with a spore print pressed on its lid in a different colour (white, cream, pink, rust, purple-black): the farmers' library | Lore item "Spore print jar"; Poison-resist and Rot-resist recipes early |
| C9 | **The Saltpetre Walls** | F3, (−1,500, −7,900), −70 | White nitre crust on the walls, Deepfarmer scraping tools and baskets left mid-work | Saltpetre ×12 (R9 black powder, survival-crafting) |
| C10 | **The Silk Larders** | F4, (−700, −8,050), −130 | A Webspinner larder: cocoons hung from cables, and you can see what's in them: a Rotmaw, a Puffball, a sky Mistlark, and a **Kitewright glider with its pilot** | Silk ×10, glider frame scrap, lore item |
| C11 | **The Root Ceiling** | F4, (−400, −8,200), −110 | A chasm whose ceiling is a forest of hanging black rhizomorphs. A Deepfarmer climbed one to its very tip and clamped there, summited, over the 60 m drop | Rhizomorph cord ×8 |
| C12 | **The Titan Nursery** | F4, (−950, −8,000), −160 | Wooden scaffolds round three Deepfarmer titans half-grown inside clay moulds, each mould built round a stone bier. They grew their guardians **from their honoured dead**. One mould is broken open from the inside, and empty | Clay, bronze, lore item "Titan mould sherd" |
| C13 | **The Sealed Gate** | F4, (−820, −8,380), −180 | A round stone door into the Heart Grotto, sealed by a web sheet 12 m across (route 1, 2.5) | The way into the Heart Grotto |
| C14 | **Grotto Weir** (KEPT set-piece) | F5, (−800, −9,150), −20 | A Deepfarmer fish weir with Glowfin in it | Glowfin; fishing spot |
| C15 | **The Air Bell** | F5, (−300, −8,950), −50 | A dome of trapped air over a sump (Movile). A Deepfarmer bronze diving bell rests on the ledge, a cache and the diver's bones inside. **Foul air** (2.8) | Bronze ×6, a gem, lore item "Diver's bell" |
| C16 | **The Vat Room** (KEPT set-piece) | F5, (−350, −9,150), −50 | A cracked mycelium vat, its pipes still pulsing. It leads on to the Steeping Vats (the Rot King) | Mycelium vat recipe scrap |
| C17 | **Toad Leaps** | F5, (−600, −8,850), −10 | A chain of five chasms 10 to 12 m wide over the river, crossable only by a Tunnel toad's leap or by springcaps on the far walls. A toad colony on the ledges | Tunnel toad taming; a cache on the last ledge |
| C18 | **The Fallen Champion** | L6, (0, −8,800), −40 | The Kitewright champion close up. Climb its chest to find a carved jade kite-token in its fist and the Deepfarmer sickle in the ribs. Its wings point up the shaft | Lore item "Champion's kite-token" |

**Kit POIs (53 in all, placed by the generator with variants; world-map density rules):**

| Kit piece | Where | Variants | Surface | Caverns |
|---|---|---|---|---|
| **Glow-cap ring** (the Deepfarmer waymark, world-map SPEC: a 3 m ring of glowing caps round a carved stone, every 150 m on Deepfarmer routes; at night the ring's caps brighten in turn toward the nearest undiscovered POI) | all | 3 | 4 | 10 |
| Deepcap host cluster (bones of a fallen creature furred white, 1 to 3 deepcaps) | F1 fissures, F3, F4, F5 | 3 | 1 | 6 |
| Puffball field (6 to 12 tethered Puffballs and a spore cloud) | F1 cliffs, F2, F3 | 2 | 2 | 4 |
| Ore pocket (copper and tin up top, iron deeper, 2.6) | F2 to F5 | 3 | 0 | 5 |
| Summited niche (a Deepfarmer clamped high on a ledge, a cache below) | F2 to F4 | 3 | 1 | 4 |
| Mycomite nest (egg balls on a wall) | F2, F3 | 2 | 1 | 3 |
| Web pocket (lure lines and a Webspinner) | F3, F4 | 2 | 0 | 3 |
| Glowgrub colony | F3, F5 | 2 | 0 | 2 |
| Capling meadow | F1 dolines, F2 | 2 | 2 | 2 |
| Shelf-fungus deadwood (amadou; foxfire at night) | F1 | 2 | 2 | 0 |
| Doline lookout (a rim ledge with a view straight down) | F1 | 3 | 1 | 0 |

### 2.5 Hidden sub-biome: the Heart Grotto (RAVEN-PLANNED "hidden sub-biomes"; FULL-GAME name and "lit like a city" kept; layout GAP-COOKED)

**What it is.** The Deepfarmers' last city, grown (not built) in a ring round the Heart chamber. Houses are hollow caps and shelf-fungus towers, streets are mycelium mats, and everything glows warm amber like a town at dusk. **And it's full of people:** every Deepfarmer who came down is here, summited on top of their own house, jaw clamped on the roof-root, a white stalk out of the skull, face toward the Heart's dome. A ring 120 to 380 m out from (−600, −8,600), at −180 to −220. About 0.4 km². 10 POIs.

**Ways in:**
1. **The Sealed Gate** (C13): burn the web sheet (fire, 3 s) or blow it down (any wind hit, or the Gale flask).
2. **The moat dive:** from the Drowned Granary lake, a sump 40 m long (14 s of breath) into the Grotto's moat. Glowfin light the way. Uses the shared underwater frame (biome-ocean 2.6).
3. **The mole wall:** a tame Truffle mole knocks at (−300, −8,500) in the Webs and digs a 6 m crawl through in 20 s.
4. **The Heart Gate** (from inside only): a one-way shortcut back to the Fall Field's shrine, Elden-Ring style.

**Rooms (in order from the Sealed Gate):**

| Room | What's there |
|---|---|
| **The Lantern Street** | The first street. Cap-houses with amber-glowing gills for windows; on every roof a summited Deepfarmer, dozens of them. Caplings graze the street. 2 Sporestalkers hunt the rooftops |
| **The Market of Hands** | A square of stone stalls. On each, a Deepfarmer's tools laid out neatly with a deepcap growing from the handle. Trade stopped mid-day |
| **The Feeding Hall** | A long hall where they laid their dead in the mycelium floor in rows, each grave mounded and sprouting caps (sky burial turned downward). The oldest graves are only caps now. Deepcap ×6 |
| **The Elders' Tower** | The tallest shelf-fungus tower, 40 m. At the top, seven elders summited in a ring round a bronze bowl, facing the Heart. Lore item "Elder's bowl" |
| **The Last House** | The only house barred from inside. In it, one Deepfarmer who didn't climb: lying down, mouth bound shut with cord, wrists bound by their own hand (knots pulled tight with the teeth). No stalk. They refused it |
| **The Koi Pool** | The Grotto's pool: the **Mycelial Koi** (KEPT legendary fish), a fishing ledge |
| **The Heart Wall** | The wall of the Heart chamber: a living membrane of mycelium with the boss's glow pulsing through at 35 beats a minute. The run-back stone (2.14) stands 50 m out |
| **The Seed Vault** | A sealed room that opens only during the Sporefall (2.10): iron ×6, a gem, upgrade shards ×2, lore item "Heart-seed husk" |
| **The Mite Warren** | Collapsed houses full of Mycomite nests: the swarm room |
| **The Heart Gate** | The one-way door to the Fall Field |

**Light:** the city's amber glow (full sight). **Music:** the Heart Grotto cue (2.16).

### 2.6 Ore galleries (CF rule: every biome has caves; deeper is better)

The whole biome is a cave, so its "caves" are ore galleries off the main route.

| Gallery | Sub-zone, at | Depth below surface | Ore | Creatures | Special |
|---|---|---|---|---|---|
| **Upper Vein** | F2, (−1,950, −8,700), +30 | 40 m | Copper, tin | Mycomites, Caplings | The newcomer's first ore |
| **Nitre Gallery** | F3, (−1,500, −7,900), −70 (with C9) | 130 m | Iron ×2, saltpetre | Webspinner 1, Truffle moles | Saltpetre for R9 |
| **Deep Vein** | F4, (−500, −7,900), −180 | 260 m | Iron ×4, gem chance ×2 | Sporestalker 2 | The best iron; a cap-fall gallery (2.8) |
| **Rimstone Pocket** | F5, (−200, −9,200), −55 | 120 m | Iron ×2, a gem | Tunnel toads, Glowgrubs | Behind a sump (8 s dive); foul air |

### 2.7 Resources (GAP-COOKED; deepcap name KEPT)

| Resource | Where | Gather | Uses |
|---|---|---|---|
| **Deepcap** (signature; never farmable, KEPT) | **Only on the dead:** host clusters (bones of fallen creatures), the Offering Niches, the Feeding Hall, the Fall Field, the Breath Vents. A cyan-glowing mushroom 30 cm tall with a translucent blue-white cap. Real: some fungi fruit only on corpses | By hand (a knife doubles it), 1 per node; respawns in 3 game days. A tame Truffle mole senses them through rock (15 m) | Filter mask, Sporeweave, Poison resist (all KEPT), Shroomback taming food (KEPT), glow-cap lantern, spore wad, Gale flask binder |
| **Spore sac** | Puffball (farm, 20 min, KEPT); wild Puffballs | Pen or hand | Filter mask, alchemy, spore pot |
| **Glow-moss** | F3 floors | By hand, 2 per patch | Glow paste: a cold-light smear on a weapon or helmet, 6 m light for 10 min |
| **Amadou** (tinder fungus, as Ötzi carried) | Shelf-fungus deadwood (F1), F2 walls | By hand | Light a fire or torch without flint; a torch lit with it burns 2× longer |
| **Rhizomorph cord** | F4 cables, C11 | Axe, 4 hits | Rope (×2 strength), rope ladders (allowed underground, KEPT), saddles |
| **Webspinner silk** | Webspinner drops, larders | Hand | Sporeweave (KEPT), bowstrings (+10% draw speed), bandages |
| **Mite chitin** | Mycomite drops | Butcher | Arrowheads (pierce ×1.1), light armour plates |
| **Saltpetre** | C9, the Nitre Gallery | Pick | R9 black powder (survival-crafting), food preservation |
| **Oyster cap, Lion's mane** (crops, KEPT) | Log beds; wild in F2 and C4 | Farm | Food; Lion's mane rot resist, Focus elixir (KEPT) |
| **Cave crayfish, Glowfin** (KEPT fish) | F5 | Fishing | Food; Glowfin oil (lantern fuel) |
| **Stalker stalks** | Sporestalker drops | Butcher | Spore pot; crafting extra (+10% rot buildup on weapons) |
| Ore | 2.6 | Pick | Copper, tin, iron, gems |

### 2.8 Hazards

**Spore clouds (SPEC 3.3 numbers; behaviour and tells GAP-COOKED)**

| Item | Value |
|---|---|
| Effect | Inside: **12 poison buildup a second** (KEPT). After **10 s inside without a break** (a break is 3 s outside), rot builds as well, 12 a second (KEPT "rot after 10 s"). Poison proc: 0.4% max HP + 2 a second for 30 s; rot: 0.8% max HP + 4 a second for 20 s, healing ×0.5 (combat KEPT). Thresholds ×1.3 after each proc. It can kill |
| Look | A slow-turning cyan-green haze 6 to 20 m across, full of bright motes, with a soft but visible edge. **Inside, you always see at least 12 m.** Clouds never sit over a ledge, a jump or a springcap |
| Tell | A soft rising hiss and glitter at the edge. For the first 0.9 s inside, motes gather on your arms before any buildup ticks. Your body shows the buildup: motes settle thicker on skin and clothes as the meter fills |
| No slow | Clouds never slow you (lesson 5) |
| Passes | **Immune system** (root, SPEC). **Filter mask** (Head slot; Alchemy 3; deepcap 2, fiber 6, charcoal 2; KEPT). **Sporeweave 4** (KEPT). **Spore wad** (5 min, buildup ×0.3, world-map KEPT). Every creature of this biome ignores clouds (CR rule) |
| Where | Puffball fields, round host clusters, the Breath Vents, every third ledge of the Great Doline path, dolines downwind in Spore drift weather, the Flue floor's plumes, and Spore bloom expansions (2.9). About 12% of cavern route length is inside a cloud at any time |
| **Three answers (GAP-COOKED)** | **Wind:** any wind hit pushes a cloud 6 m along the gust (combat KEPT); a strong gust (Wind spells, the Gale flask, a Draught gate) pushes it up to 15 m. **Fire:** any fire inside a cloud (a torch swing, a burn proc, a fire spell, a burning creature) **flashes** it like flour dust in a mill: a 0.9 s rising whoomp and glow, then a flash through the whole cloud for 40 damage + burn 40 to everyone inside, and the cloud is gone for 2 game hours. **Frost:** frost damage in a cloud freezes the spores, which fall as glittering "spore snow" over 3 s; the cloud is gone for 1 game hour |

**The Breath vents at the Flue floor (KEPT from biome-sky, built here).** Three Deepfarmer bellows-stones at the Fall Field, 40 m apart round the centre, grown over with mycelium. In turn, every 40 s, one exhales a cyan spore plume 6 m wide straight up the Flue for 6 s. Tell 1.0 s: the stone's glow brightens and a deep huff rises. Standing in a plume counts as standing in a spore cloud. The plumes rise 1,290 m to Aeolith's ring, where they're its spore counter (biome-sky KEPT).

**Micro-hazards (GAP-COOKED; 4, one per sub-zone at most; each with ≥ 0.9 s of tell; each can kill):**

| Hazard | Where | Tell | Effect | Answer |
|---|---|---|---|---|
| **Lure lines** | F3 ceilings (the False Sky, the Lure Gallery, web pockets) | Glowing beaded threads hang to 1.5 to 2.5 m above the floor and hum when air moves. A thread you brush **twangs and brightens for 0.9 s** before it grips | Touching one with head or arms (walking into it, jumping, a swing) sticks you: no moving or rolling for 2 s, though you can still attack and block. Then the line **reels you up 3 m** over 1.5 s and drops you (fall damage) unless you cut it (any light attack on it). A Webspinner within 30 m comes | Crouch (passes under lines at 1.5 m); cut a path (each light swing cuts every line within 1.5 m); burn them (a torch clears 3 m); wind tears 8 m of them down for 1 game hour |
| **Springcaps** (a hazard and a tool) | F2 groves, D4, C17's walls | A bulging cap with glossy, taut skin that creaks as you land, then **pops 0.6 s after you land unless you jump** | Jump while on it: launched 9 m up in an arc, aimable 20° by stick. Landing on it from over 6 m always fires it. Any attack on it fires it, throwing anyone on it 5 m sideways (fall damage applies); creatures can be knocked onto them | The fast way up once learned. Regrows 20 s after firing |
| **Cap-fall** | F4 (the Deep Vein, Root Ceiling edges) | **Knocking** in the rock overhead (two knocks, then three, like the knockers), then a trickle of grit and a falling-dust shaft over the spot for 1.0 s | A rot-softened block falls in a 3 m circle: 70 damage and a knockdown. Loud work within 15 m (mining, fights, explosions) sets a ceiling knocking | Step out of the dust shaft. A tame Truffle mole knocks back and marks knocking ceilings 10 s early |
| **Foul air** | F5 air bells (C15, the Rimstone Pocket, the moat) | Your torch flame **shrinks and turns blue** (a real miner's tell), lantern light dims 30%, breathing gets heavy over 1.0 s | Stamina regen ×0.5 and no sprint inside. After 20 s, 3 damage a second until you leave. No fire can be lit in it | Leave. The filter mask doesn't help (it's no oxygen, not spores). Glowgrubs and glow-cap lanterns still light it |

### 2.9 Weather (weights KEPT; effects reconciled)

**Surface (world-map KEPT weights):**

| State | Weight | Effect |
|---|---|---|
| Clear | 30 | Doline glow reads from the sky islands at night |
| Overcast | 20 | The Breath column reads strongest against the grey |
| Rain | 20 | Rain pours into the dolines as waterfalls; F2 floors puddle; spore clouds can't form outdoors (rain knocks spores down) |
| **Spore drift** (signature) | 30 | Spore clouds spread 200 m round every doline downwind (KEPT), drifting on the world wind vector (world-map KEPT). Surface clouds are thinner: inside you see 20 m |

**Underground: the Spore bloom (survival-crafting KEPT).** Every 20 min in the halls, for 3 min, hazard spore clouds spread 50% wider. **Tell:** 30 s before, every glow in the hall pulses in sync once a second and the cave's breathing sound deepens. During a bloom, Mycomites swarm toward warm light from 2× range. Passes: filter mask, Sporeweave 4, Immune system (KEPT).

**Reconciled:** world-map's Spore drift is the surface state and survival-crafting's Spore bloom is the underground pulse; both stand. The Heart Grotto has no bloom: the city is calm, and the danger there is the boss.

### 2.10 Event: the Sporefall (GAP-COOKED; frame KEPT from fire-frost 3.1; FULL-GAME premise kept: clouds drift up and seed fungus on the nearest wedges for a day)

**Real root.** A mass fruiting (a "flush") after rain, plus the cordyceps summit at noon.

| Part | Detail |
|---|---|
| Frequency | One game day, about once every 3 real days (the server's Biome events setting, KEPT) |
| Warning (5 min) | The Breath column turns from cyan to **gold** and climbs to 800 m. Every cavern glow beats in sync at 35 a minute (the Heart's pulse), and the beat comes into the music. Every Puffball in the biome slips its tether and drifts up. The **Deepfarmer pillar on the Crown's Ring burns cyan** (world-map rule) |
| Where it seeds | The whole Fungal surface; **the Desert's north 400 m** (D5, the Haboob edge); **the three nearest sky islands** (Spore Scar, Wheel Isle, Lid Gate, from biome-sky's landing list). Camps and every spawn safe zone stay clean |
| **The flush** | Over the first game hour, fruiting patches burst up across the seeded ground, one per 40 m², each a cluster of pale caps with a 6 m spore cloud. **Deepcap appears on the surface** in every patch that has bones (×3 pay, 3.3 SPEC) |
| **The Flue** | The Breath vents exhale every 30 s instead of 40 for the day, so Aeolith's spore counter comes round faster (a cross-biome effect for the Sky fight) |
| **Bloomed beasts** | Any creature in the seeded area that spends 10 s in a flush cloud **blooms**: a pale fruiting cap swells on its back. It's alive and still itself (not undead, DD); its bites add poison 20, and **on death it bursts** into a 4 m spore cloud for 6 s. Desert and Sky creatures included; Fungal creatures don't bloom (they're at home) |
| **The summit** (noon) | At 12:00 game time every bloomed **passive** creature (Caplings, Dune hares, Glowgrubs, Mistlarks...) climbs to the highest point within 30 m and clamps there. They die at dusk and fruit overnight: at dawn each summit holds a deepcap. A landscape of small bodies on rock tops, all facing the Breath. Nothing explains it |
| The Heart Grotto | The Seed Vault opens (2.5) |
| Creatures | Sporestalkers hunt on the surface by day; Mycomite swarms ×2; the Weaver leaves her route and webs the Long Stair (D2) for the day |
| Pays | Deepcap ×3 (3.3 SPEC); spore sacs double. In Mycelorn's arena the boss's clouds last 50% longer for the day, and its drops gain +1 spore essence |

### 2.11 Shrines and the path master

**Shrines (rules KEPT from fire-frost 3.1: 5 per biome, guarded, 3 s to light, path XP, fast travel; one at the pass mouth, world-map):**

| Shrine | At | Guard | Look |
|---|---|---|---|
| **Great Shrine: the Garden Doline** (FULL-GAME "shrine in a spore garden", kept) | F2, (−1,500, −8,700), +40, at D1's foot | None (the path master is near) | A ring of 7 caps 4 m tall round a stone basin where a deepcap grows in water; the brazier is a bowl of glow-moss oil burning cold cyan |
| Ring Shrine | F1, inside L3 | Tunnel toads (3; they defend the ring) | The altar stone with a cold-light brazier |
| Starfall Shrine | F3, (−1,500, −8,050), −60, at the False Sky's edge | Webspinner pair | A brazier on a flowstone column under the starfield |
| Cable Shrine | F4, (−1,000, −8,300), −140, at the Shoestring Bridge's west anchor | Sporestalker and a Mycomite swarm | A brazier lashed into the cable roots |
| Flue Floor Shrine (pass mouth) | L6, (+40, −8,780), −40, beside the champion | Rotmaw and Mycomites | A brazier in a ring of fallen kite spars |

**Path master: Ilmari Kask, the Mushroom Farmer (GAP-COOKED; DD "a path master NPC per biome who teaches the path through quests"; FULL-GAME "a mushroom farmer").**

| Part | Detail |
|---|---|
| Who | A newcomer, not a Deepfarmer (world-map "the newcomers"). He came ashore years ago, found the Garden Farm, and taught himself the old ways from their jars and bones. He doesn't climb. Early 30s, tall and lean with a farmer's shoulders and forearms, dark curls with mossy green tips falling over his eyes, a short beard, warm brown skin dusted with faint cyan freckles across the nose, cheeks and shoulders (his own Spore body change at 75, 2.18), an easy crooked grin. An open umber leather vest over a bare chest, sleeves torn off, a belt of knives and pouches, rolled linen trousers, bare feet in the moss. A spore mask hangs round his neck that he never wears (Immune system) |
| Where | Works the Garden Farm (C2) and the Great Shrine's terraces by day; sleeps in a cap-house on the terraces (warm window light) |
| How he talks | Short spoken lines within 6 m, no camera grab (KEPT rule). Quests go to the Journal |
| Teaches | Anyone. His last quest needs the third mini (the Weaver) |

**His 6 quests (GAP-COOKED; `quests.json` schema from fire-frost):**

| # | Quest | Task | Reward |
|---|---|---|---|
| 1 | **First flush** | Plant an oyster cap log bed and harvest it | 1,000 Spore XP; log bed and spore wad recipes |
| 2 | **What grows on the dead** | Bring 3 deepcaps | 1,500 Spore XP; filter mask recipe |
| 3 | **Cold light** | Tame a Glowgrub or craft a glow-cap lantern, then walk the Lure Gallery end to end without being reeled | 2,000 Spore XP; glow-cap lantern and glow paste recipes |
| 4 | **The lone cap** | Kill a Rotmaw before it bursts from its mound (strike the lure first) | 2,500 Spore XP; spore pot recipe |
| 5 | **Down the Long Stair** | Light the Starfall and Cable shrines | 3,000 Spore XP; Gale flask recipe (needs Sky levistone) |
| 6 | **Cut the starfield** | Beat the Weaver | 5,000 Spore XP; "Farmer's ring" (ring slot: Spores regen +20%, poison and rot buildup taken ×0.85) |

### 2.12 Creatures (names, roles and taming KEPT from FULL-GAME's list; everything else GAP-COOKED)

**Rules (CR SPEC):**
- Every Fungal creature ignores spore clouds, foul air and cap-falls' dust (not the block).
- Neutral herds defend each other.
- Each creature tames only by its listed methods.
- Tiers follow 3.3: hard aggressive = highland, medium aggressive = midland, passives = lowland. Mycomites use the swarm archetype (25 HP each).
- Every attack shows the real claw, jaw, tongue, stalk or projectile, and damage lands only on visible contact.
- No attacker limit (combat 2.18.4).
- `physMaterial` "fungal" (spore puff) or "chitin" for blood and spark FX (combat 2.14).
- **Dark-hunters (survival-crafting KEPT):** Rotmaw and Webspinner get double aggro range on unlit players. This lane adds the Sporestalker (it hunts by sound, so darkness is its ground).
- **Warm light draws Mycomites** (GAP-COOKED, lesson 3): a player carrying fire (torch, burning weapon, Fire path Heat over 50%) is seen by Mycomites from 2× range. Cold light isn't.
- **Every creature's spores are cyan; no crimson anywhere but unblockable glints.**

#### 2.12.1 The ten at a glance

| Creature | Tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Sporestalker** | Highland, 1 (2 in the Deep) | Fossa body (long, low, a tree-climber's forelimbs and semi-retractile claws) at leopard size; striped-hyena jaw; olm's blind, pale head with huge nostrils and ears; **a crest of *Ophiocordyceps* stalks** 30 to 60 cm long along the spine; under the jaw a ruff of orange gills | A long low body that clings to ceilings, with a bristling white crest that rises like hackles | Pelt umber #4A3A2C furred with fine grey hyphae; stalks bone-white #E8E2D0 with cyan tips #39E6C2; gill ruff orange #D9772B; skin pale pink-grey on the face | 3.4 m long, 1.0 m at the shoulder | Knockout and feed, or Spore path magic. Hard | Fibrous hide ×3, stalker stalks ×4, raw meat ×4 |
| **Rotmaw** | Highland, 1 | Alligator snapping turtle (the pink worm-lure on the tongue, the beak, the ridged shell) at 4 m; trapdoor-spider ambush; its shell is overgrown with a living mound of caps | A low mound of mushrooms that opens into a 1.8 m mouth | Shell black-brown #2E241C under ivory and tan caps #D8CBB0; the lure glows exactly like a deepcap (cyan, translucent); gums rot-rust #8A4A2A | 4 m long, 1.4 m tall mound | Knockout and feed. Hard | Raw meat ×6, rot-shell ×2 (light shield plate), caps ×6 |
| **Mycomite** | Lowland swarm, 4 to 12 | Hard tick (*Ixodidae*) at 40 cm: eight legs, the barbed hypostome mouthpart, a bloated abdomen; a small cap grows on the back of each | A scuttling grey bead on legs, its back crowned with a pale cap | Chitin grey #6A6458, cap ivory, abdomen cyan veins when fed | 40 cm | Steal an egg (egg balls on nest walls). Easy | Mite chitin ×1 |
| **Webspinner** | Midland, 1 to 2 | Huntsman spider leg span and speed; ogre-faced spider's huge forward eyes; the Waitomo glowworm's luminous snare threads; its abdomen spotted with glowing cyan lure dots | Long jointed legs spanning 4 m round a small body; two huge eye-lenses that catch light | Legs banded umber and cream #6A5040 / #D8CBB0, abdomen black with cyan dots, eyes silver reflectors | 4 m leg span, 1.3 m body | Knockout and feed. Medium | Webspinner silk ×4, chitin ×2, venom sac ×1 |
| **Tunnel toad** | Midland neutral, colony 3 to 6 | Goliath frog legs and bulk; Surinam toad's flat head; the olm's pink-white skin with eyes buried under it; huge eardrums | A squat pale toad as tall as a man, with folded legs longer than its body | Pink-white #E8C8C0 with grey mottling, throat translucent | 2 m tall sitting, 5 m leap reach | Knockout and feed. Medium | Raw meat ×6, toad skin ×3 (waterproofing) |
| **Shroomback** | Midland neutral, herd 2 to 3 | Glyptodont (domed shell, the spiked club tail) crossed with a pangolin's head; an oyster-cap garden grows on its shell (KEPT) | A slow dome on legs carrying a garden of fan-shaped caps; a club tail behind | Shell olive-brown #5E5440, caps oyster grey-blue #9AA4AA, skin grey | 4 m long, 2 m tall | Offer deepcap. Medium | Raw meat ×8, shell plate ×2, oyster caps ×6 |
| **Truffle mole** | Lowland neutral, 1 to 3 | Star-nosed mole (the 22 pink nose tentacles) at dog size; the knockers' knocking: it raps its claws on rock to sound it | A low velvet barrel with a pink star for a nose and spade hands | Velvet black-brown #2A2420, nose pink #E89A9A | 1 m long | Steal an egg. Easy | Velvet pelt ×1, raw meat ×2 |
| **Glowgrub** | Lowland passive, colony 4 to 8 | Fungus-gnat larva (the Waitomo glowworm) at 1 m: soft segmented body, a glowing tail lantern | A soft pale grub with a lantern for a tail | Translucent cream, tail glow blue-green #7FE8D0 | 1 m | Offer food (insects, Glowfin bits). Easy | Glow gland ×1 |
| **Capling** | Lowland passive, herd 4 to 10 | A small hopping animal (a pika's body) whose back is a living cap; it regrows the cap when harvested (KEPT) | A round-eared hopping critter wearing a red-brown mushroom cap | Fur sandy #C8B08A, cap rust-red with cream spots #A84A30 / #F0E6D0 | 50 cm | Offer food. Easy | Capling flesh (regrows, KEPT) |
| **Puffball** | Lowland passive, drift of 6 to 12 | A giant puffball fungus (*Calvatia*) as a floating gas-bag animal, like a siphonophore: a pale sac 1.2 m across with trailing sticky tendrils, drifting on warm air | A pale floating ball trailing threads | Cream-white #EDE6D6, tendrils cyan-tipped | 1.2 m sac | Offer food. Easy | Spore sac (farm 20 min, KEPT) |

**Path magic (Sporestalker; GAP-COOKED in Raven's "calm a creature of the same element" frame).** On a Sporestalker knocked out or under 30% HP and rotting, hold Interact for 4 s with 10+ Spore points: you plant 10 of your Spores on it and detonate them softly (no damage) into its stalks. That counts as a full meal of narcotic; taming continues as knockout-and-feed.

**Utilities:**
- **Tunnel toad (mount):** leaps 12 m gaps (KEPT) and 6 m up; swims; ride sprint 1,250 cm/s; a jump hold charges a leap (0.6 s) with a landing slam ring (300 cm, 20 damage).
- **Shroomback (hauler):** carries 220, grows 1 oyster cap per 10 min (KEPT); its garden's spores make it immune to clouds, and anyone riding or within 3 m gets spore buildup ×0.5.
- **Truffle mole (utility):** senses ore, deepcap and hidden passages within 15 m and knocks on the wall toward them; digs collapsed crawls (20 s); knocks back at cap-fall ceilings (marks them 10 s early).
- **Glowgrub (light):** 12 m cold-light aura (KEPT); doesn't flash clouds; doesn't draw Mycomites.
- **Capling, Puffball:** farm yields (KEPT).

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55. Biome element buildup on bites: poison or rot 15 by default (combat rule).

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Sporestalker** (high) | **Ceiling drop** (opener) | 0.95 (grit trickles from the ceiling, a dry stalk-rattle overhead, its crest's cyan tips glint) / 0 / 0.30 / 1.2 | 1.6 | 55 (5) | From a ceiling or wall within 8 m above; blockable; rot 25 | Only from above. It climbs walls and ceilings between fights |
| | Claw, claw | 0.38 / 0 / 0.16 —, then 0.34 / 0 / 0.16 / 0.60 | 0.9 each | 30 (2) | parryable; poison 15 | 50% → Gill bite |
| | **Gill bite** | 0.70 (the orange gill ruff flares open round the jaw) / 0.5 / 0.20 / 0.70 | 1.5 | 50 (4) | parryable; rot 30 | — |
| | Pounce | 0.60 / 0.4 / 0.25 / 0.90 | 1.4 | 45 (4) | parryable; 300 to 900 cm | Wall-kick variant: off a wall within 3 m, comes from the side |
| | **Stalk volley** | 1.00 (arches its back, crest stalks swell and lock upright, a rising creak) / 0 / — / 0.80 | 0.5 per pellet | 10 (1) | 5 spore pellets lobbed on high arcs to land round the target in a 4 m spread, 900 cm/s ballistic, never home, min range 600; each bursts into a 2 m cloud for 4 s (poison 25 on burst) | ≥ 1.2 s from the crest lock to landing; the pellets show the whole flight |
| | **Spore shake** (reaction) | when it takes 60+ damage in 2 s: 0.5 (stalks flare) | 0 | — | A 3 m puff round it: poison 25 | FULL-GAME "releases spores when hit". 6 s cooldown |
| | readsAction | Gill bite with no hold within 0.4 s of a drink or a charged heavy | | | | |
| **Rotmaw** (high) | **Mound burst** (opener) | 0.90 (the mound's caps shiver outward in a ring, spores puff from under the rim) / 0 / 0.30 / 1.2 | 1.7 | 55 (5) | From hidden; blockable; rot 30; 360° r 350 | If you reach for its lure first, it opens with Lure snap instead |
| | **Lure snap** | 0.90 (the lure jerks back into the mouth and the mound's caps shiver; crimson glint on the beak) / 0 / 0.20 catch | 0.8 + hold | 50 (5) | `grab`: if you're within 1.5 m of the lure (you went to pick the "deepcap"); dodge only; held 1.5 s, 15/s | The learned rule: a lone deepcap with no bones round it is a lure. Hitting the lure from range forces it out stunned 2 s (quest 4) |
| | Beak snap | 0.45 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable; rot 20 | 40% → Beak snap 2, then 30% → Shell spin |
| | **Rot belch** | 0.85 (throat swells rust-coloured, a gurgle) / 0 / 1.0 cone / 0.9 | 0.3 per tick | — | 6 m × 40° cone of rot gas; rot 18 a tick (4 ticks); not a projectile | — |
| | Shell spin | 0.60 (it hunkers, mound caps clench) / 0 / 0.6 spin / 1.0 | 1.2 | 45 (4) | 360° r 300, `low` is false (roll out) | — |
| | Lunge-shove | 0.70 / 0.4 / 0.25 / 0.90 | 1.3 | 50 (5) | parryable; 200 to 600 cm | — |
| | readsAction | Lunge-shove within 0.4 s of a drink | | | | |
| **Mycomite** (swarm) | Bite | 0.45 / 0 / 0.12 / 0.80 | 0.5 | 5 (1) | parryable; poison 10 | — |
| | **Latch** | 0.50 (rears, front legs spread, the barbed hypostome flicks out) / 0 / 0.15 catch | drain 2/s | 5 | `grab` (blockable: a block flicks it off); poison 6/s while on; plants nothing | On: ×0.9 move speed per mite. Off by: a roll (50%), fire on you, any ally hit, its death. Max 4 on one player (one per limb, visible) |
| | **Burst on death** | — | — | — | A 2 m puff on death: poison 15 | Swarm archetype (combat 2.18.3) |
| **Webspinner** (mid) | **Ceiling pounce** | 0.90 (a single bright thread drops to the floor beside you, then its eyes catch the light above) / 0.3 / 0.25 / 1.0 | 1.4 | 40 (4) | From a ceiling within 10 m; blockable; poison 25 | — |
| | Fang strike | 0.50 (front legs lift, fangs flex) / 0 / 0.15 / 0.70 | 1.1 | 30 (3) | parryable; poison 30 | 40% → Leg rake |
| | Leg rake ×2 | 0.40, 0.35 / 0 / 0.18 / 0.70 | 0.8 each | 25 (2) | parryable | — |
| | **Web shot** | 0.85 (abdomen tilts up, spinnerets pulse) / 0 / — / 0.80 | 0.2 | 10 (1) | Projectile net 900 cm/s, slight gravity, never homes, min range 500; hit = **webbed 2 s** (can't move or roll; can attack, mash −0.15 s per press) | — |
| | **Reel** | 0.90 (a lure line you've touched brightens and twangs) / 0 / 1.5 reel | — | — | Pulls a stuck player up the line toward it (lure lines, 2.8) | Cut the line to drop |
| **Tunnel toad** (mid, neutral) | Tongue whip | 0.60 (throat pumps, cheeks bulge) / 0 / 0.20 / 0.80 | 1.0 | 30 (3) | parryable at full guard; pull 300 cm | — |
| | **Leap slam** | 0.85 (crouches, legs tremble) / 0.3 / 0.25 / 1.2 | 1.5 | 50 (5) | Lands on a target point up to 1,200 cm; impact ring 350 cm `low` (jump clears it) | — |
| | **Croak blast** | 0.90 (throat sac swells to the size of its head, translucent) / 0 / 0.20 / 1.0 | 0.6 | 40 (4) | 4 m ring of sound: knockback 400; Webspinner lines within 8 m snap | The herd's rally call |
| | Kick | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | `behindOnly` | — |
| **Shroomback** (mid, neutral) | **Club tail** | 0.80 (tail lifts and swings back, spikes rattle) / 0.2 / 0.25 / 1.1 | 1.6 | 55 (5) | Arc 140° behind and to the side, r 450; parryable | — |
| | Shell shove | 0.70 / 0 / 0.25 / 0.90 | 1.2 | 45 (4) | blockable | — |
| | Garden puff (reaction) | when hit on the shell: 0.4 (caps shudder) | 0 | — | 3 m puff: poison 20 | 8 s cooldown |
| | Withdraw | — | — | — | After 25% of its stance in 2 s it tucks 3 s: damage ×0.2, strike ×1.5 | — |
| **Truffle mole** (low, neutral) | Claw swipe | 0.45 / 0 / 0.15 / 0.80 | 1.0 | 15 (1) | parryable | — |
| | Burrow pop | 0.80 (the floor bulges and cracks in a line) / 0 / 0.25 / 1.0 | 1.2 | 25 (3) | from under you | — |
| **Glowgrub** (low, passive) | Glow flash | 0.60 (tail lantern swells and whitens) / 0 / 0.20 / flees | 0 | — | 3 m flash: 1.5 s vignette dim | Then crawls away |
| **Capling** (low, passive) | Puff | 0.50 (cap trembles) / 0 / 0.20 / flees | 0 | — | 2 m puff: poison 10 | Hops away |
| **Puffball** (low, passive) | Burst (when killed or hit by fire) | 0.90 (the sac tightens and its skin crackles) / 0 / 0.30 / dies | 0 | — | 6 m spore cloud for 8 s (poison 30 on burst); fire on it = a cloud flash (2.8) | Doesn't attack; drifts toward warm light |
| **Sporeling** (bosses' swarm; not the path summon) | Bite | 0.45 / 0 / 0.12 / 0.80 | 0.5 | 5 (1) | parryable; poison 10 | 25 HP; bursts on death 2 m (poison 15) |

**Physical resist:** Rotmaw shell strike 0.8 / slash 0.85 (its belly, shown when it rears in Rot belch or tips in Shell spin, takes ×1.3). Shroomback shell strike 1.2 / slash 0.8. Mycomite and Webspinner chitin pierce 1.15 / slash 0.9. Sporestalker fibrous pelt slash 1.1. **Lock points:** Sporestalker head and chest; Rotmaw head and belly; Webspinner abdomen and head; Shroomback head and tail.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table; underground uses the night table 22:00 to 04:00, world-map KEPT)

| Sub-zone | Day | Night |
|---|---|---|
| F1 Sinkhole Rim | Capling 16, Puffball 10, Truffle mole 8, Shroomback 6, Mycomite 4 | Tunnel toad 10 (in the Ring), Capling 8, Mycomite 8, Sporestalker 2 (doline rims, young: midland-tier, 120 HP) |
| F2 Spore Gardens | Capling 14, Shroomback 8, Puffball 8, Mycomite 8, Glowgrub 4, Rotmaw 2 | Mycomite 12, Glowgrub 8, Rotmaw 4, Webspinner 2 |
| F3 Glow-moss Halls | Webspinner 8, Glowgrub 10, Truffle mole 6, Mycomite 8, Puffball 4 | Webspinner 12, Mycomite 10, Sporestalker 4, Glowgrub 8 |
| F4 Mycelium Webs | Webspinner 10, Sporestalker 8, Mycomite 8 | Sporestalker 12, Webspinner 10, Mycomite 10 |
| F5 Flooded Grottos | Tunnel toad 12, Glowgrub 8, Rotmaw 6, Shroomback 4 | Rotmaw 10, Tunnel toad 8, Mycomite 8, Sporestalker 3 |

**Living-world beats (built AI, WRWildlife):**
- Rotmaws lure Caplings: a Capling hops to a lone "deepcap" and vanishes into a mound. Players see it before they learn the rule.
- Sporestalkers hunt Tunnel toads at the Toad Leaps; the colony's croak blast scatters them (herd rule).
- Webspinners reel Glowgrubs up into the False Sky; you see lights rising into the starfield.
- Mycomite swarms gather on any carcass within 60 s and swarm toward any fire.
- Puffballs drift up the dolines on warm air by day and settle back at night; following them leads to the surface.
- Shroombacks graze the F2 meadows; Caplings ride on their shells.

### 2.13 Mini-bosses (premises from the FULL-GAME draft kept; fights GAP-COOKED)

Rules (KEPT, combat 2.17): 3,000 solo HP, +80% per extra player or tame; stance 90; 2 parries; 4 to 5 attacks plus one counter mechanic; chase 65 m; respawn 4 h; minis drop upgrade shards ×4 and a trophy (BO); trophies are talismans (survival-crafting KEPT: Mother's cap, Rot crown, Weaver silk).

#### 2.13.1 The Sporemother (the Mycomite queen; real root: the engorged tick queen)

| Part | Detail |
|---|---|
| Body | A Mycomite queen grown to 6 m long and 2.5 m high: eight thick legs, a barbed hypostome as long as a spear, and an abdomen swollen tight and translucent, packed with glowing egg sacs you can see shifting inside. Pale caps crown her back in a ring, like a crown |
| Silhouette | A huge round body on stilted legs, the head small and low, the abdomen glowing |
| Arena | **The Brood Doline** (D3's floor, F2, (−1,850, −9,150), +40): a doline floor 60 m across, open to the sky, its walls hung with **6 egg sacs** (puffball-like pods 1.5 m across, glowing cyan, on the walls at 2 to 6 m) |
| Voice | Madagascar hissing cockroach hiss amplified, walrus grunt, wet egg pops, a deep chitin creak |
| Lock points | Head, abdomen |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Hypostome jab | 0.55 / 0.3 / 0.15 / 0.80 | 45 + poison 30 | Parryable; 4 m reach |
| Leg stamp ×3 | 0.50 / 0.40 / 0.65 (H 0.3 on the third) / P 1.0 | 35 / 35 / 50 | Third is `low` (jump it); a 3-hit string |
| **Egg toss** | 1.0 (abdomen contracts, a sac bulges to her rear vent) / 0 / — / 0.9 | 0 | Lobs 2 sacs on 900 cm/s ballistic arcs to points 8 to 15 m away; each lands and hatches 3 Mycomites 1.5 s later (shadow on the landing point the whole flight) |
| **Brood press** (grab) | 0.95 (rears, legs spread, crimson glint on the hypostome) / 0.2 / catch | Pinned under her: 20/s for 2.5 s, 4 Mycomites latch on | `grab`, unblockable, dodge only. Ally hits on the abdomen (150) free you |
| Spore vent | 0.85 (abdomen veins flare) / 0 / 0.8 | — | 8 m ring cloud round her for 6 s (poison 12/s, the hazard rule) |

**Counter mechanic: burn the sacs (KEPT).** Each wall sac hatches 3 Mycomites every 25 s while it stands (all 6 at the start: that's 18 mites every 25 s if ignored). A sac has 150 HP and takes ×3 from fire; **any fire hit kills it at once** (a torch swing, a burn proc, a fire spell, a flashed cloud). Each burnt sac also makes her **shriek and stagger 2 s** if she's within 20 m. Burning all 6 stops the hatching; then her egg toss throws duds (sacs that don't hatch). Lighting her own spore vent cloud with fire flashes it on her: 200 damage and a 3 s stagger.

**Drops:** trophy Mother's cap (KEPT: spore and rot buildup ×0.7), upgrade shards ×4, mite chitin ×10, "Queen's jelly" ×1 (crafting extra: sporelings +15% HP).

#### 2.13.2 The Rot King (an ancient Rotmaw; real root: the alligator snapping turtle that lives a century)

| Part | Detail |
|---|---|
| Body | A Rotmaw grown to 9 m long. His mound has become a **crown** of tall black rotting caps 3 m high, always dripping rust-coloured slurry. His beak is chipped white, his shell scarred by old Deepfarmer bronze hooks still in it. He lies in his own rot |
| Silhouette | A low black wedge under a tall ragged crown of caps |
| Arena | **The Steeping Vats** (F5, (−300, −9,150), −50, past the Vat Room): Deepfarmer retting pits where they rotted fibre: 8 stone vats 6 m across in a hall 70 m long, half of them full of rot slurry, with stone walkways between. Water from the river runs in channels round the edge |
| Voice | Alligator snapping turtle hiss, walrus bellow pitched down, saltwater-croc infrasound, a wet bog belch; material: caps tearing, shell grinding on stone |
| Lock points | Head, belly (when he rears) |

**Rot pools.** The slurry vats are rot ground. **He heals 1.5% max HP a second while in a rot pool**, and players in it take rot buildup 15 a second (no slow, lesson 5). His Rot spread makes new pools.

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Beak snap ×2, crown slam | 0.50 / 0.45 / 0.90 (H 0.4 on the slam) / P 1.1 | 45 / 45 / 70 + rot 40 | The slam spills a 6 m rot pool where it lands (20 s) |
| **Rot spread** | 1.0 (crown caps tilt and pour; slurry streams down his flanks) / 0 / 1.5 | 0 | Drags his crown in a 120° arc, painting a rot pool 8 m wide behind the arc (20 s) |
| **Rot belch** | 0.85 / 0 / 1.2 cone | 12 per tick + rot 25 | 10 m × 45° cone |
| **Engulf** (grab) | 1.0 (mouth gapes to 2 m, a crimson glint on the beak's hook) / 0.3 / catch | Held in the mouth 3 s, 20/s, rot 60 | `grab`, unblockable, dodge only. Ally hits on the head (150) free you |
| Shell spin | 0.70 / 0 / 0.8 spin / 1.2 | 55 | 360° r 450; scatters the slurry: every pool within 6 m spreads 2 m |

**Counter mechanic: frost and wind (KEPT premise).**
- **Frost:** frost damage on a rot pool **freezes it** for 15 s: no heal, no buildup, walkable ice. A frostbite proc on him stops his healing for 12 s.
- **Wind:** a wind hit on him **blows his rot aura off**: the crown dries and stops dripping for 10 s, so Rot spread and the slam make no pools.
- **Water:** the edge channels are clean water; luring him out of the slurry and into the channels (he follows you) means he can't heal there.
- **Played well:** freeze pools and fight him on clean stone or ice. **Played badly:** he lies in his own rot and outheals you.

**Drops:** trophy Rot crown (KEPT: hits on rotting targets +10%), upgrade shards ×4, rot-shell ×4, "King's slurry" ×1 (crafting extra: weapons +8 rot buildup per hit).

#### 2.13.3 The Weaver (roaming; a Webspinner matriarch; real root: the glowworm's snare threads and the huntsman's speed)

| Part | Detail |
|---|---|
| Body | A Webspinner with a 9 m leg span, legs banded cream and black, her abdomen a field of glowing cyan dots arranged like constellations. Her two great eyes are mirror-bright. She trails silk always |
| **Webbing a hall** | Wherever she settles, she webs the space: over 60 s, white web sheets span the hall wall to wall and lure lines drop from the ceiling (2.8). **On web she moves ×1.6; off web ×0.6** (KEPT premise "slow off them") |
| Route | 1.8 km loop: Starfall Hall (lair, the False Sky) → the Sand Falls → the Lure Gallery → Knockers' Junction → the Shoestring Bridge → the Silk Larders → back. Walks at 2 m/s on ceilings and walls, rests 4 min at the lair. Nocturnal: she sleeps hanging in the False Sky by day (a sneak crit opens the fight) |
| Living world | She reels Glowgrubs and Caplings up into the starfield; she fights Sporestalkers she meets (they lose) |
| Voice | Barn owl screech pitched down, purring-spider stridulation amplified, creaking rope, silk tearing |
| Lock points | Head, abdomen |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Fang lunge | 0.55 / 0.4 / 0.20 / 0.80 | 50 + poison 40 | Parryable; 6 m reach |
| Leg sweep ×3 | 0.45 / 0.40 / 0.60 / P 1.0 | 35 / 35 / 45 | Third sweeps low (`low`, jump) |
| **Web volley** | 0.90 (abdomen lifts, spinnerets pulse cyan) / 0 / — / 0.9 | 10 each | 3 nets fanned 30° apart, 1,000 cm/s, never home, min range 500; a hit = webbed 2 s |
| **Starfall drop** | 1.0 (she climbs out of sight; a single bright thread drops beside a player; her eyes catch the light overhead) / H up to 0.5 / 0.30 / 1.3 | 70 | From the ceiling onto a target point; a 3 m impact; blockable |
| **Cocoon** (grab) | 0.95 (front legs spread wide, a crimson glint on the fangs) / 0.2 / catch | Wrapped 3 s, poison 20/s; then hauled up to the ceiling | `grab`, unblockable, dodge only. Cut free by an ally (100 damage on the cocoon) or by fire on it. Hauled up = dropped 6 m when it ends |

**Counter mechanic: tear the web (KEPT premise "wind or fire clears webs").**
- **Fire:** burns web sheets and lure lines in a 4 m radius per hit; a flashed spore cloud burns all web inside it.
- **Wind:** a wind hit tears 8 m of web down; a Gale flask clears 12 m.
- **The Tunnel toad's croak blast** snaps lure lines within 8 m (a tame trick).
- **Off web** she's at ×0.6 speed and her Starfall drop is unavailable: strip the hall and she's slow and grounded. She rewebs at 1 sheet per 10 s, so keep clearing.

**Drops:** trophy Weaver silk (KEPT: webs and snares hold you 50% shorter), upgrade shards ×4, silk ×12, "Star-thread" ×1 (crafting extra: bows +15% draw speed).

### 2.14 Major boss: Mycelorn, the Hivemind (GAP-COOKED; FULL-GAME premise kept: the cavern's fungal heart, a towering mushroom-thing with a hundred tendrils; phase 1 tendril slams, spore clouds, sporeling waves; phase 2 moves through the mycelium and surfaces anywhere; hidden phase 3 the whole cavern attacks; weak to Wind; boss part Hive spore)

**What it is (the story, never told aloud).** Mycelorn is the whole organism: every glowing thread in the caverns is part of it (Armillaria, real). The thing in the Heart chamber is its **fruiting body**: the part that can move, fight and breathe spores. The Deepfarmers fed it their dead, then grew titans from it, then gave themselves.

**Look (terrifying and believable; no faces on mushrooms).**

| Part | Detail |
|---|---|
| Anatomy | **30 m tall when risen.** A massive translucent-amber **stipe** (stem) 6 m thick, grown in a spiral round the bones of the Deepfarmers' greatest titan, whose ribcage you see inside the stem like a fossil in amber. A **cap 36 m across**, ringed, dark umber on top, its underside a deep fan of **gills that glow** cyan. Hanging from the cap's rim, **a hundred rhizomorph tendrils** like a lion's mane jellyfish: black cords 0.3 to 1.5 m thick and 8 to 20 m long, the thickest twelve ending in hooked, bone-tipped clubs. A **veil ring** of torn white membrane round the stem at 8 m |
| **The Heart-seed** | A pulsing amber core 3 m across inside the stem's base, beating 35 times a minute. Its glow drives every glow in the caverns. Lock point |
| **The summited host** | Spiralling up the stem's surface, the bodies of hundreds of Deepfarmers, fused into it, all facing up, jaws open. They don't move. When Mycelorn calls, the sound comes through them |
| Silhouette | A colossal umbrella on a glowing stem with a curtain of black cords under it. From 100 m it reads as one shape; the cords move like a curtain in water |
| Palette | Cap umber #3A2A1E, gills cyan #39E6C2, stem amber #E8A040 with honey translucency, tendrils black #1A1714, veil cream; crimson only on unblockable glints |
| Voice | Sperm whale codas (the clicks of a mind talking to itself), humpback moan pitched down, elephant infrasound rumble under everything, and a breath layer recorded through clay pots so it sounds like hundreds of throats exhaling at once |

**The arena: the Heart chamber** ((−600, −8,600), −220, KEPT).
- A domed chamber 120 m across and 70 m high, walls a living mycelium membrane glowing amber.
- **Floor:** a mycelium mat with glowing veins over stone. Centre: the **Heart pool** (40 m across, ankle-deep amber sap; no slow) where Mycelorn is rooted in phase 1.
- **Raised ground:** six Deepfarmer stone terraces round the edge (1.5 m high, 10 m wide), each with a summited elder statue-body at its back.
- **Four Draught gates** (the Wind counter for everyone, below) at north, south, east and west: stone doors 6 m wide in the wall, sealed by fungal plugs.
- **Run-back stone:** 50 m out from the Heart Wall door (KEPT rule), in the Heart Grotto.
- The arena holds the Spore path **altar** (BO rule): a stone basin at the south terrace.

**The Wind counter (FULL-GAME "wind blows its clouds back on it", GAP-COOKED as a visible system).**
- **Its clouds hurt it.** Mycelorn's own spore clouds are harmless to it **unless they're pushed onto it by wind**. Any cloud blown into its stipe or cap (Wind path gust or spell, the reaction push 6 m, a Gale flask, a Draught gate) **chokes** it: the gills clench and go dark, it staggers 4 s, takes ×2 stance damage and ×1.5 wind damage (weakness SPEC 1.5×) for those 4 s, and the cloud is spent.
- **Draught gates (any path).** Each gate has a fungal plug with 600 HP (×2 from fire and blunt). Breaking a plug opens a cold draught from the Webs that blasts across the arena in a 12 m-wide lane for 10 s, pushing every cloud in the lane 15 m along it (a visible gale of motes and torn silk). Gates regrow their plugs in 45 s. Players time clouds and gates.
- **Gale flask** (2.19): thrown, it bursts into a 6 m gust for 1 s and pushes clouds 12 m (levistone from the Sky biome: Raven's "bring the opposite" push without making Wind the only way in).
- **The cost.** Players in a cloud that's blown along with it take the hazard's buildup like any cloud.

**Numbers (KEPT rules):** 12,000 solo HP, +80% per extra player or tame. **Stance 175.** 3 parries to break (its tendril clubs are parryable). Phase 2 at 50%, hidden phase 3 at 25% with artifacts (damage ×1.4). Weak to Wind 1.5×. Status: poison and rot buildup on it ×0.3 (it's the source). Fire works normally; fire on a cloud next to it flashes for normal damage (the cloud is gone, so no choke).

**Lock points:** the cap's gills (part HP 1,500: breaking them stops Gill breath for 30 s), the Heart-seed (only exposed when it rears or is choked), and in phase 2 the eruption mound.

**Phase 1: the rooted crown (6 moves: a 3-hit combo and a grab; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell (small cue, combat 2.17.1b) |
|---|---|---|---|---|
| **Club slam** | 0.75 / 0.4 / 0.20 / 0.90 | 60 | parryable; one heavy tendril, 20 m reach; leaves a vein crack that glows 2 s (no damage) | The chosen club lifts out of the curtain and swings back; white glint on the bone tip |
| **Lash ×3** (combo) | Sweep 0.65 (`low`) / backhand 0.45 / downward whip 0.80 + H 0.4; P 1.2 | 40 / 40 / 65 | First jump, second roll, third sidestep; 18 m | The curtain parts to one side, cords streaming |
| **Gill breath** | 1.0 / 0 / 1.4 (sweeps 70°) / 1.0 | 15 per tick + poison 50 | The cap tilts toward you; a 16 m × 40° cone of spores from the gills that **leaves a cloud** (8 m, 20 s) where it passed | The gills brighten from cyan to white along the side that will breathe; a deep rising inhale |
| **Spore mortar** | 0.90 / 0 / travel / 0.8 | 30 each + poison 30 | 3 puffball pods lobbed at players over 14 m away, 1,000 cm/s ballistic, never homing; each lands as a 6 m cloud (20 s) | Pods swell on the cap's top edge; a shadow grows on each landing point the whole flight |
| **Sporeling wave** | 1.0 (the floor's veins flare outward in rings) / 0 / — / 1.0 | — | 8 sporelings (swarm, 25 HP) rise from the mat at the arena's edge and rush | The veins pulse outward |
| **Engulf** (grab) | 1.10 / 0 / 0.30 catch | Lifted in a fist of tendrils 3 s: 18/s, 5 Spores planted on you that burst on release (8 each + rot 10) | `grab`, `unblockable`; a 14 m reach | Six tendrils draw together into a hooked fist; crimson glint on the hooks |

**Tame-punish move (KEPT rule, one per major): Summit call.** 0.9 s (a single white stalk rises from the cap and arches toward the furthest tame). It fires a cordyceps stalk at **the furthest tame first** (1,000 cm/s, never homing, visible the whole flight). A tame it hits is **summited**: it walks to the nearest terrace's back wall, climbs, and clamps there for 8 s, out of the fight. Any ally hit of 100 on the stalk on its back frees it. Once per 40 s from 80% HP.

**Transition at 50%: the Sinking Root** (KEPT rule: an unblockable telegraphed attack, 1.4 s, the boss takes 50% damage during it).
- **The tell.** The cap rears back, all its gills flare white, the Heart-seed blazes, and a crimson glint runs along every club tip.
- **The attack.** It drives its whole body down into the floor. A ring wave of heaved mycelium rolls out at 1,200 cm/s: 60 damage and a knockdown. Jump it, or stand on a terrace.
- **The arena change.** The Heart pool drains. The floor's veins brighten into a visible network. Mycelorn is gone underground. Twelve **mound pods** (fungal knobs 2 m tall) sprout round the floor.

**Phase 2: the root that walks (the "surfaces anywhere" premise, built as a readable system)**

**How it moves.** It travels under the mat along the veins. **Tell:** the veins pulse along the **shortest path** from where it is to where it's going (the Physarum rule), like light racing down a map. Then, at the target, **a fairy ring of white caps springs up 6 m across, 1.2 s before it erupts** (never stand in the ring). It surfaces there and fights from that spot for 8 to 14 s, then moves again. It never surfaces on a terrace.

| Move | W / H / A / P | Damage | Flags | Tell |
|---|---|---|---|---|
| **Ring eruption** (new, delayed) | 1.20 / H up to 0.5 / 0.30 / 1.2 | 80, launch 4 m | Everyone inside the 6 m ring | The veins race toward the spot, then the ring of caps springs up; final flash and a rising crack 0.2 s before |
| **Root lash line** (new, delayed) | 1.00 / H up to 0.6 / 0.40 / 1.3 | 70 | `unblockable`; a line of 8 tendrils bursts up from under the floor along one vein, 30 m long, 2 m wide | That vein glows white end to end and the mat over it bulges; crimson glint as the first tendril breaks the surface |
| **Erupt, spin, slam** (new combo) | Erupt 1.0 / spin 0.60 / slam 0.90; P 1.4 | 60 / 45 (360° r 14 m, `low`) / 70 | The slam is parryable | The ring, then the curtain flares outward before the spin |
| **Gill breath (delayed variant)** | 1.0 / H 0.5 / 1.6 (sweeps 120°) / 1.0 | as phase 1 | From wherever it surfaced | As before, plus a silent held beat before the sweep |
| Keeps | Club slam, Lash ×3, Spore mortar, Engulf, Summit call | | | |

**Arena change (phase 2):** the **mound pods** are its breathing pores. Every 20 s, the four pods nearest a player **exhale** a 6 m cloud for 10 s. Burn a pod (fire, any hit of 200) and it's gone for the fight. The clouds are its own: blown onto it, they choke it. So phase 2 hands everyone ammunition.

**Wind in phase 2.** It surfaces in a new place every 8 to 14 s, so the trick is pushing pod clouds and its own breath clouds into the spot where the ring is springing up: it erupts **into** the cloud and chokes on the way up (a 5 s stagger, the best punish in the fight).

**Hidden phase 3 (25%, artifacts only; KEPT frame): the Whole Cavern.**
- **The chamber attacks.** Every 8 s one of these runs, in order, each with its own tell: (1) the ceiling drops **lure lines** over a third of the floor (the twang tell, 2.8); (2) the walls **exhale** clouds in a 10 m band at their foot (a hiss rising round the wall); (3) **cap-falls** knock and drop at three player positions (the knocking tell); (4) all terraces' elder bodies **puff** spores (6 m each, the gills on their stalks brighten first).
- **The Heart-seed shows.** Mycelorn stays surfaced at the centre; its stem splits to show the Heart-seed, which takes ×1.25 damage.
- **Every move speeds up.** Damage ×1.4 (KEPT); Lash ×3 becomes Lash ×4.

**Drops (KEPT BO rule plus survival-crafting's major trophy rule):**
- Boss core `core_mycelorn`.
- Spore essence ×5.
- Chance at the boss part **Hive spore** (FULL-GAME: kills burst into a poisoning cloud; GAP-COOKED numbers: on a kill, the body bursts into a 4 m cloud for 4 s, poison 40 on burst, once per 2 s; your own and your party's buildup from it is 0).
- Trophy (the Heart-seed husk monument).
- 2 blueprints: **Stipe Maul** (a great hammer whose head is a petrified cap; its moveset goes to the boss-weapons cook) and the **Deepfarmer decor pack**.

**Music:** FUN_Boss_Mycelorn (2.16).

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT from fire-frost 3.5)

Every species: tell, strike, hit, pain, plus idle, aggro and death. Each built from at least 3 real animal calls and one material layer. Tells rise in pitch, 3 dB over the bed. Underground voices go through the hall's reverb (long tails in F3, tight in F4), which is part of the read.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Sporestalker | Striped hyena whoop and giggle slowed ×0.6, fossa scream, cougar scream | Dry stalk rattle (porcupine quill shake) | The crest rattling overhead in the dark before a ceiling drop |
| Rotmaw | Alligator snapping turtle hiss, walrus bellow pitched down, a deep wet belch | Mushroom caps tearing, shell grinding on stone | The mound's caps shivering with a soft tearing sound before it bursts |
| Mycomite | Madagascar hissing cockroach hiss, harvester-ant stridulation amplified, cicada clicks | Many tiny legs on stone | A rising hiss chorus as a swarm turns toward your torch |
| Webspinner | Purring-spider stridulation amplified, barn owl hiss, fruit-bat clicks | Silk twang, line creak | One bright twang, then silence, then the drop |
| Tunnel toad | Spadefoot toad bleat, African bullfrog bellow pitched −1 octave, common toad release call | Throat-sac stretch, wet slap on landing | The croak blast: a booming bellow that rings round the hall |
| Shroomback | Armadillo grunts, wombat huff-growl, rhino snort | Cap rustle, shell knock | The tail's spike rattle before the club swing |
| Truffle mole | Star-nosed mole squeaks, naked mole-rat chirps, badger chitter | **Claw knocks on rock** (the knockers) | Knock-knock... knock-knock-knock toward hidden things |
| Glowgrub | Mealworm-crunch clicks slowed, cricket chirp pitched down | Soft wet glide | A slow chirp that brightens with the glow |
| Capling | Pika squeak, guinea pig wheek | Cap flop | The alarm squeak that sets a meadow hopping |
| Puffball | Dugong chirps, a slow bellows huff | Balloon creak, skin crackle | The crackle as the sac tightens before a burst |
| Sporemother | Hissing cockroach hiss amplified, walrus grunt | Wet egg pops, chitin creak | The shriek and stagger when a sac burns |
| Rot King | Snapping turtle hiss (deeper), saltwater-croc infrasound, walrus bellow | Slurry pouring, cap tearing | The crown dripping, a steady patter that stops when the wind dries it |
| Weaver | Barn owl screech pitched down, purring-spider stridulation, creaking rope | Silk tearing | The single thread dropping beside you with a high twang |
| Mycelorn | Sperm whale codas, humpback moan pitched down, elephant infrasound | Breath through clay pots (hundreds of throats), cords creaking | The codas clicking faster as the veins race toward where it will rise |

### 2.16 Music: the Fungal caverns (tempo, key, feel SPEC 3.3: 70 BPM, Bb Locrian, wet organic textures, low drones, slow pulses; cues GAP-COOKED; rules KEPT)

**Palette:**
- **Water drumming:** hand-struck water, after the Baka people's *liquindi* (Cameroon) and Vanuatu women's water music: the "wet organic texture" played as rhythm, slow and deep.
- **Udu** (Nigerian clay pot drum): its low "bloop" is the slow pulse at 70 BPM, like a drip in a cistern.
- **Contrabass and bass clarinet** with multiphonics and breath sounds: the spores breathing.
- **Bowed vibraphone and crotales:** the glow, ringing and decaying.
- **A low male drone choir** using Tuvan *kargyraa* undertone singing: the deepest human sound, under the ground.
- **Prepared piano (felted low strings)** for the melody in the Heart Grotto.
- **Bb Locrian** (Bb, Cb, Db, Eb, Fb, Gb, Ab): the diminished fifth Fb makes the home note unstable, so the music never quite rests. The **Heart Grotto** turns to Gb major for warmth (the same notes, a different centre): the lit city sounds beautiful and wrong.

**Always audible**, with silences only where they're written. **One planned silence (KEPT, biome-sky):** stepping onto the Fall Field, the music fades out over 4 s and leaves only the Flue's roar for 20 s, then the Fall Field cue begins. Ducks at most 4 dB. Mixed −18 LUFS, calm −23, boss −15, true peak −1 dBTP.

| Cue | Length | Use | Feel |
|---|---|---|---|
| FUN_Explore_Surface | 3 to 4 min | F1 by day | Sea-wind texture, udu pulse, bass clarinet melody, gulls in the mix; the most open cue |
| FUN_Explore_A / B / C / D | 3 to 4 min each | A: F2 Gardens. B: F3 Halls. C: F4 Webs. D: F5 Grottos | A: water drumming in daylight, vibraphone glints, the warmest. B: long reverb, crotales like stars, kargyraa drone very low. C: contrabass clarinet multiphonics, creaking textures, the Heart pulse at 35 under the 70 pulse. D: running water rhythms, udu and water drums trading, a lighter melody |
| FUN_Night_Surface | 3 min | F1 at night | Drone and vibraphone; the doline glow as sustained chords |
| FUN_Combat (stems for each Explore) | loops | Combat layer on the bar | Udu doubles to 140 felt, frame drum, clarinet ostinato on the Fb |
| FUN_Mini | 3 min loop | Sporemother, Rot King, Weaver | 4-bar intro per mini: hissing breath clusters (Sporemother); a sub-bass slurry drone (Rot King); a single high crotale and silence (Weaver) |
| FUN_Boss_Mycelorn P1 / P2 / P3 | sections | Phase changes on the bar | P1: kargyraa choir and udu, the clarinets breathing in time with the Heart-seed. P2: water drums race in patterns that move round the stereo field (following the veins), the choir splits. P3: everything in, the 35 and 70 pulses collide in 2-against-1 |
| FUN_HeartGrotto | 4 min | The Heart Grotto | Prepared piano in Gb major over the Heart pulse, a hummed kargyraa line, the warmest and saddest cue in the biome |
| FUN_FallField | 3 min | After the planned silence | The Flue's roar tuned to a low Bb, a slow clarinet line rising, flute-like breath from above (a nod to the Sky's flutes) |
| FUN_Sporefall | layer | Event | The Heart pulse audible as a deep drum over whatever plays |
| FUN_Camp | 2 min | Camps and your Hearth on the rim | Udu and soft vibraphone, the pulse at rest |

**Source:** first pass from CC0 and CC-BY libraries matched to tempo, key and instruments (OpenGameArt, Free Music Archive, Pixabay Music, incompetech, Freesound for water drumming and udu stems). Boss tracks commissioned later. Credited in `Docs/CREDITS.md`. No AI generation (KEPT, fire-frost 3.6).

### 2.17 The Spore path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Spores (DD SPEC: "planted on enemies, then detonated"), built as two linked things (GAP-COOKED):**
- **The Spores bar** (the path's resource bar, 4.1 rule). Capacity 100. It's a *spender* (combat KEPT): spells and Wildbrands cost Spores.
- **Refill:** 5 a second always; **×2 while you stand in any spore cloud** (your own or the world's: the biome feeds you); **+3 for every planted Spore you detonate**.
- **Planted Spores:** small cyan nodules on the target's body, a visible count (1 to 10 per target, 13 with Spore capacity 3). They last 20 s, refreshed by any new plant.
- **Detonate** (combat KEPT: granted free with Spore cloud as its own quick-slot entry, 0.30 s cast while walking): every Spore you've planted within 2,000 cm bursts for 8 damage + rot 10 each. Costs 0. The core loop: plant, plant, detonate.
- **No floor.** Spells cost what they cost; when the bar's short, the slot greys and shakes.

**Costs (GAP-COOKED; combat sets the rest):** Spore cloud 15, Blight 30, Sporeling 25, Spore armor 30, Fungal behemoth 60, Living colony 80, Infect strike 10 (KEPT).

**Root passive: Immune system (SPEC: immune to poison and rot).** Main path only. Being immune to poison and rot makes spore clouds harmless to you, so it's also the hazard pass (FULL-GAME KEPT).

**The tree.** Gates are points spent in the branch before the node, the swamp cook's pattern (0, 5, 10, 14, 17, 20).

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Spore cloud | 3 pts | 0 | Basic spell: plant Spores on enemies in a cloud | Pool r 300 at range 1,500, plants 1 Spore a second, poison 10/s, 6 s, cd 6; 15 Spores | KEPT combat (cost GAP-COOKED) |
| Trunk | Spore capacity | 3 ranks | 3 | Hold more Spores | +20 bar capacity and +1 max planted per target, per rank | GAP-COOKED |
| Trunk | Rot chance | 2 ranks | 6 | Higher chance to inflict rot | +10% rot buildup on all your hits and +5 rot buildup on every weapon hit, per rank (so any weapon can rot) | GAP-COOKED |
| Trunk | Infect strike | 2 pts | 8 | Weapon art that plants Spores | Wildbrand: 10 Spores, cd 2.5, W/A/R 0.28/0.20/0.42, ×1.2, rot 30, plants 3 Spores, stance ×1.5 | KEPT combat |
| Rot | Decay | 5 ranks | 0 | More rot damage | +8% rot damage per rank | GAP-COOKED |
| Rot | Corrode | 4 pts | 5 | Rot lowers enemy armor | A rotting target's physical resist drops 15% (bosses 10%); its body shows an etched, pitted rust texture | GAP-COOKED |
| Rot | Blight | 5 pts | 10 | Spell: a spreading rot cloud | Pool r 400 that spreads 50 cm/s to 700, rot 15/s, cd 18; 30 Spores; its cloud counts as a spore cloud (wind pushes it, fire flashes it) | KEPT combat (cost GAP-COOKED) |
| Rot | Lingering | 3 ranks | 14 | Rot lasts longer | +20% rot duration per rank (20 → 32 s) | GAP-COOKED |
| Rot | Burst | 3 pts | 17 | Detonating Spores spreads rot | Each detonated Spore also puts rot 6 on every other enemy within 300 cm of it | GAP-COOKED |
| Rot | Pandemic | 10 pts | 20 | Capstone: rot jumps between all nearby enemies | Every 2 s, each rotting enemy passes rot (at half its remaining time) to every non-rotting enemy within 600 cm; a cyan-rust thread shows each jump. Bosses receive it at ×0.5 duration | SPEC text; numbers GAP-COOKED |
| Colony | Sporeling | 5 pts | 0 | Summon a small spore creature | Summon: a 60 HP knee-high fungal biter (bite 10, plants 1 Spore), 25 s, cd 10; 25 Spores; 1 at a time | KEPT combat (bite and cost GAP-COOKED) |
| Colony | Swarm | 5 ranks | 5 | More sporelings at once | +1 sporeling at once per rank (up to 6) | GAP-COOKED |
| Colony | Host | 4 pts | 10 | Enemies killed by Spores spawn sporelings | A kill on an enemy carrying any of your Spores (or killed by a detonation) spawns a free sporeling for 15 s (counts toward the cap) | GAP-COOKED |
| Colony | Hive mind | 3 pts | 14 | Sporelings attack your target | Sporelings switch to your lock-on target (or the last thing you hit) within 0.5 s and deal +20% to it | GAP-COOKED |
| Colony | Thick spores | 3 ranks | 17 | More sporeling health | +25% sporeling and behemoth HP per rank | GAP-COOKED |
| Colony | Fungal behemoth | 10 pts | 20 | Capstone: summon a giant spore creature | Summon 500 HP, 40 s, cd 120; 60 Spores. A 4 m fungal giant with a cap for a back: slams 30 (stance ×2), and every slam plants 2 Spores on everything it hits. Real clips (a heavy biped set) | KEPT combat (look and slam GAP-COOKED) |
| Symbiote | Spore armor | 5 pts | 0 | A spore coat that reduces damage | Self buff −15% damage taken for 20 s, cd 30; 30 Spores; a visible coat of soft cyan-tipped fuzz over the body | KEPT combat |
| Symbiote | Regrowth spores | 5 ranks | 5 | Regenerate HP while carrying Spores | +0.4 HP a second per rank while your Spores bar is at 40 or more (2 HP/s at 5) | GAP-COOKED |
| Symbiote | Toxic blood | 3 pts | 10 | Attackers get Spores planted on them | A melee attacker that damages you gets 2 Spores planted and poison 15 | GAP-COOKED |
| Symbiote | Adrenal | 4 pts | 14 | Detonating Spores restores stamina | +2 stamina per Spore detonated (max 30 per detonation) | GAP-COOKED |
| Symbiote | Bloom body | 3 ranks | 17 | Resist more status effects | All status buildup on you ×0.9 per rank (×0.73 at 3) | GAP-COOKED |
| Symbiote | Living colony | 10 pts | 20 | Capstone: become a walking spore cloud that infects enemies and heals you | Transform 15 s, cd 120; 80 Spores. A 5 m aura plants 1 Spore a second on every enemy in it and adds poison 10/s; you heal 1.5% max HP a second per enemy in the aura (max 6% a second). You fight normally. Your aura counts as a spore cloud (wind pushes it off you for 2 s; fire flashes it, and you're immune to the flash's burn but not its damage) | KEPT combat (numbers GAP-COOKED) |

**Stat and passive behaviour (all server-side, through `ApplyHit`):**
- **Planted Spores** are a per-target stack on the status component, owned by the planter (two Spore players plant separately).
- **Path summons don't count as tames for boss HP** (the forest lane's ruling for plant summons, applied to sporelings and the behemoth; flagged for combat).
- **PvP:** planted Spores on players cap at 5; detonation damage ×0.6 (PvP block, combat 2.20); Living colony's heal ×0.5 against players.

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Planting | A small cyan nodule swells on the target per Spore, visible to all | A soft wet "tick" per Spore |
| Detonate | Every nodule bursts in a chain (0.05 s apart) in cyan puffs | A popping chain like seed pods |
| Corrode | The target's surface pits and rusts | A faint fizz |
| Burst | Rust-coloured threads spray from each burst to nearby enemies | A hiss |
| Pandemic | Cyan-rust threads jump between rotting enemies every 2 s | A low crackle |
| Host | A sporeling bursts up out of the corpse | A wet pop and a squeak |
| Toxic blood | Cyan motes spray from your wound onto the attacker | A puff |
| Adrenal | A pale gold flush on your arms per detonation | A quick breath |
| Living colony | Your body haloed in drifting cyan motes, your freckles (2.18) blazing | Your own breath heard as many breaths |

**Combos with Spore (CB SPEC, names and effects as written; built in R8 for the paths that exist by then):**
- **Wind + Spore** (opposites): Spore drift / Plague wind.
- **Fire + Spore:** Combustion / Ash colony. **Lightning + Spore:** Static cloud / Neural spores. **Sun & sand + Spore:** Light rot / Sun rot. **Nature + Spore:** Mycorrhiza / Forest floor. **Frost + Spore:** Hardy colony / Ice bloom. **Blood & curse + Spore:** Infected wounds / Blood colony. **Tide + Spore:** Bog / Swamp colony. **Necromancy + Spore** (neighbours): Fungal puppets / Endless colony.
- Arcane + Spore (Fungal mind / Glowcap colony) and Spore + Void (Hungry spores / Entropy) are built in R9 and R10.
- **Necromancy + Spore's "Fungal puppets"** raise enemies as fungal minions: that's a necro raise (Raven's path ability, SPEC), not a zombie mob; it uses the graveyard lane's raise rules.

### 2.18 Body change: Spore (frame and row KEPT from hero.md 2.6; stages GAP-COOKED)

**Frame (KEPT):** the G (joint and cheek) mask carries faintly glowing fungal **freckles** in cyan at 0.8 nits; a cyan fleck ring in the iris; mossy green tips on the lower 15% of the hair cards; spore motes drifting from the shoulders in the dark (owner and 15 m). Strength s = smoothstep(0.08, 1.0, points ÷ 100); subclass ×0.6. Combat pulse ×1.6 for 1.5 s on any Spore ability.

**The look:** natural freckles you'd find attractive on anyone, in real freckle patterns (across the nose bridge and cheekbones first, then the shoulders and collarbones), that glow softly only in the dark and flare when you cast. Never mould, never scabs.

| At | What shows |
|---|---|
| 10 | A light dusting of freckles across the nose bridge and cheekbones (skin-coloured by day, faint cyan glow in the dark) |
| 30 | Freckles spread to the shoulders and collarbones; a fine cyan fleck ring in the iris |
| 50 | Hair tips go a soft moss green; freckles reach the upper arms and the backs of the hands; they glow visibly in caves |
| 75 | Full freckle field (face, shoulders, upper back, chest line, forearms); spore motes drift off your shoulders in the dark |
| 100 | In the dark the freckles shimmer slowly in a 0.3 Hz wave across the body, like a breathing constellation; your breath fogs faintly cyan in caves |

- **Spore with another path:** with Fire, the freckles glow warm gold at their centres inside the ember veins; with Frost, rime sparkles on the freckle field; with Necromancy, the freckles stand out cyan on the ash-pale skin.

### 2.19 Gear and items from the caverns

**Sporeweave set (KEPT, survival-crafting; restated):** medium, poise 40.
- **Inputs (full set):** steel plate 10, deepcap 8, Webspinner silk 6, Puffball sacs 4.
- **2 pieces:** poison and rot buildup ×0.5.
- **4 pieces:** walk spore clouds unhurt (the filter mask's effect).
- **Look, merged (hero.md and survival-crafting agree in substance):** deepcap leather and living mycelium cloth that glows faintly at the seams; **mushroom-cap pauldrons with glowing cyan gills under them** (the 30 m silhouette hook, hero.md); a lowered half-mask; hanging mycelium strands (sim). Umber, bioluminescent cyan, cream. Open windows: face, forearms and the freckle zones (hero.md).

**This lane adds the fit (GAP-COOKED, survival-crafting's tailoring rule):**
- **Women's cut:** a fitted umber leather bodice laced at the sides over a mycelium-cloth wrap, bare collarbones and shoulders under the cap pauldrons (the freckle field shows), a split wrap skirt over fitted leggings and tall soft boots.
- **Men's cut:** an open-collared mycelium-cloth tunic slit to the sternum under a leather harness, bare forearms, cap pauldrons, fitted trousers in soft boots.

**Cavern items (GAP-COOKED unless marked):**

| Item | Station | Inputs | Effect |
|---|---|---|---|
| Filter mask (KEPT) | Alchemy 3 | Deepcap 2, fiber 6, charcoal 2 | Head slot: walk spore clouds unhurt |
| Spore wad (KEPT effect) | Alchemy 1 | Spore sac 1, fiber 2, charcoal 1 | Spore cloud buildup ×0.3 for 5 min |
| Antidote (SPEC item) | Alchemy 2 | Berries 2, lichen 2 (built recipe) | Cures poison and rot |
| Poison resist, Rot resist (KEPT) | Alchemy 3 | As survival-crafting | That buildup ×0.5 for 10 min |
| **Glow-cap lantern** | Workbench 1 | Deepcap 1, glass 1, copper 1 | Hand light 12 m, **cold**: never flashes clouds, doesn't draw Mycomites |
| **Glow paste** | Alchemy 1 | Glow-moss 3, resin 1 | Smear on a weapon or helmet: cold light 6 m for 10 min |
| **Spore pot** | Alchemy 2 | Spore sac 2, clay 1, stalker stalk 1 | Thrown (arc, 1,400 cm/s): bursts into a 6 m spore cloud for 12 s. A tool: make your own cloud to flash with fire, to push onto Mycelorn, or to feed Aeolith spores (biome-sky asked for a thrown spore pot; this is it: a pot hit on Aeolith adds 150 spore buildup) |
| **Gale flask** | Alchemy 3 | Levistone 1 (Sky), glass 1, deepcap 1 | Thrown (arc, 1,400 cm/s): a 6 m gust for 1 s that pushes clouds 12 m and tears 12 m of web. Mycelorn's non-Wind counter, the Weaver's counter |
| **Amadou tinder** | Hand | Amadou 1 | Lights a torch or fire without flint; the torch burns 2× longer |
| **Rhizomorph rope** | Workbench 1 | Rhizomorph cord 2 | Rope ×2 strength; rope ladder pieces (allowed underground, KEPT) |
| **Chitin arrows** | Workbench 2 | Mite chitin 1, wood 1, feather 1 → 4 | Pierce ×1.1 |
| **Knocker idol** (talisman, from S2) | Found | — | Cap-fall ceilings within 20 m knock 1 s earlier for you |
| **Log bed** (piece, KEPT from survival-crafting) | Workbench 1 | Logs 4, oyster cap spawn 1 | Grows oyster caps in the dark |

**Trophy talismans (KEPT, survival-crafting):** Mother's cap, Rot crown, Weaver silk. **Farmer's ring** (quest 6): Spores regen +20%, poison and rot buildup taken ×0.85.

### 2.20 The story the caverns tell, with no words (GAP-COOKED; fits world-map's Twelve Peoples and the Wind–Spore war in biome-sky)

**The Deepfarmers fed the Deep, and the Deep fed them.** They farmed the cape's dolines. In a famine they found the Heart, a fungus that fed them from below. They laid their dead in its threads (and deepcap grew on them), grew titans from their honoured dead to guard them, built their last city round it, and at the end gave themselves: each one climbed to a high place, bit down, and let it grow. Then the Hive reached for the sky up the Flue, and the Kitewrights fought it.

**What the player sees, in the order they'll likely find it:**
1. **The Down-Stair Terraces** (S1): a farm in perfect order, a meal never eaten, doors open, and footprints set in flowstone, **all pointing down**. They went calmly.
2. **The Grain Towers** (S4): the food was carried down. They stopped needing it.
3. **The Drying Yard** (S3): one body clamped at the top of a rack, a stalk from its skull, facing the Ancestor Cap. The first of the summited.
4. **The Offering Niches** (C1): their dead in niches, each crowned with a deepcap. The signature resource grows on the dead; the player harvests it.
5. **The Knocker Cairn and Knockers' Junction** (S2, C7): clay guardians facing *down*. Someone tried to keep something below.
6. **The Titan Nursery** (C12): guardians grown from the honoured dead in moulds. One broke out.
7. **The Drowned Granary** (L7): a city's grain rotted to dust, untouched.
8. **The summited, everywhere deeper:** more bodies on more high places the deeper you go, all facing toward the Heart.
9. **The Fall Field** (L6) and **the Fallen Kite** (S6): the Kitewright champion fell down the Flue and lies wrapped in mycelium with a Deepfarmer sickle in its ribs; a war-kite crashed on the surface with a sickle in its pilot. On the far side, the Sky lane's Paper Titan, a Deepfarmer titan speared on the highest island. **Each lies dead on the other's ground, killed by its opposite** (Raven's weakness rule, told by the land).
10. **The Heart Grotto** (2.5): the whole people, on their rooftops, facing the Heart. **The Last House:** one who bound their own mouth and refused. The only one who said no.
11. **The Heart chamber:** the titan's ribs inside Mycelorn's stem, and the people spiralling up it.

**Nobody explains it.** Lore items (Terrace tally, Knocker idol, Spore print jar, Titan mould sherd, Diver's bell, Kite harness token, Champion's kite-token, Elder's bowl, Heart-seed husk) are short, Souls-style item text.

**Secrets (GAP-COOKED):**

| Id | Secret | How found |
|---|---|---|
| F-S1 | **The ring's door.** Stand on the Ring of Caps' altar (L3) at midnight with a Tunnel toad tame: the toads' croaking in turn lights the standing stones one by one, and the altar slides to show a stair down to a sealed Deepfarmer reliquary in F2: a Deepfarmer ring (+10% Spores regen), a gem, bronze | Midnight, a toad tame, the altar |
| F-S2 | **The fifth tunnel** at Knockers' Junction (C7): a tame Truffle mole knocks at the blank wall; dig it open (pick, 6 hits) to a gallery with iron ×3 and a knocker idol | Truffle mole tame |
| F-S3 | **The dark star.** In the False Sky (L4), one patch of ceiling has no stars. Climb the shelf-fungus stairs to it: a dry ledge with a Webspinner egg (steal-an-egg isn't its method, so it's a trophy only: "Unhatched star", decor) and a Deepfarmer climber's cache | Look up |
| F-S4 | **Behind the Sand Falls** (L8): at noon, when the sun hits the middle fall straight on, a ledge path shows behind the sand to a Sunwatch-and-Deepfarmer trading post (both peoples' goods side by side, the two met here): sunstone, deepcap, a lore item "Trade token" | Noon light |
| F-S5 | **The Seed Vault** in the Heart Grotto | Sporefall only |

### 2.21 Invasions: Wind ↔ Spore through the Flue (3.3 rule KEPT; route KEPT world-map; Wind side KEPT biome-sky; Spore side GAP-COOKED)

**Spore invades the Sky islands:**
- **Sign (5 min, KEPT):** the Flue exhales a continuous cyan column; mycelium bridges grow up from the rim toward the W4 islands at 1 m a second.
- **Who:** 8 to 14 creatures: Mycomite swarms (2 swarms of 6), 2 Sporestalkers (they climb the Flue walls; the rest cross the bridges), 2 Webspinners (they web the bridges' landings), a Puffball drift of 4, and **led by the Weaver** (roaming mini).
- **Where:** they land on the Spore Scar, Wheel Isle and the Lid Gate (KEPT) and march on the Rose Shrine or the nearest W4 claim.
- **Their mark:** each invader that stands 4 s leaves a 3 m spore cloud for 2 game hours; the Webspinners hang lure lines under island edges.
- **Sky's defence edge (KEPT):** the Flue's gusts push invaders off the bridges, and Wind path knockback kills them over the edge.

**Wind invades the Fungal caverns (KEPT from biome-sky):**
- **Sign:** the Flue's updraft stops and reverses into a howling downdraft; Mistlarks flee the shaft.
- **Who:** 4 Galeharriers, 3 Gallowshrikes, 2 Skyrays, a Sailbell bloom (4), and Kyrra.
- **Target:** the nearest Fungal claim or shrine within 1.5 km of the Flue floor: the **Flue Floor Shrine**, then up the Toad Leaps toward the Garden Doline.
- **Their mark (GAP-COOKED, Fungal side):** the downdraft and the invaders' wings push every spore cloud within 30 m of them 6 m every 8 s, so clouds drift into defenders who rely on routes they know. Fire users get a new problem too: wind-fed flashes travel.
- **Spore's defence edge:** the invaders are flyers in tight caverns: lure lines and web sheets ground them (a flyer touching a lure line is stuck 2 s and falls).

**Payout:** spore and wind essence, plus 2,000 path XP to everyone with 10% (3.3 KEPT).

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing and contents from data; `L_Island` touched as little as possible.

**R8 also delivers, or needs already in place:**
- the **poison** and **rot** statuses (FULL-GAME 11: R8);
- **breeding with mutations** (R8 shared system, survival-crafting 2.21; this lane's Caplings and Tunnel toads lay eggs, and fungal creatures are `birth: egg` (KEPT));
- the attack-row runner (combat C9) and visible-contact boss damage (C10);
- the shared underwater frame (biome-ocean 2.6) for the moat dive and sumps;
- world-map's B1 terrain step and its `IsUnderground(pos)` fix in `WRGameMode.cpp` (`undergroundMargin` would otherwise teleport cavern players to the surface);
- shrines, path masters, roaming minis and biome events from the fire-frost frame;
- levistone in `items.json` (Sky, R7) before the Gale flask.

### 3.0 The cavern frame (new shared tech, built first here because this is the first big cavern biome)

Today's caves are small sculpted shells (`AWRCave`, CaveA..C, placed on the surface). A 3 km² three-level cavern needs real geometry:

| Piece | How |
|---|---|
| **Geometry** | A **Nanite cave kit**: 40 to 60 modular rock meshes (hall walls, ceilings, floors, pillars, fissures, doline throats, rimstone, flowstone curtains), sculpted in Blender from scan-based Fab cave/karst packs (licence rule: use unless explicitly barred) and tiled with trim-sheet limestone and mycelium materials. Halls are assembled by hand in the editor as static mesh actors in **World Partition** cells (128 m), so the caverns stream like the surface. Assembly follows the 2.2 and 2.4 coordinates |
| **Holes** | Landscape **visibility-mask holes** at every doline and descent (D1 to D6, the Sieves, the Flue floor's junction), with the cave kit's throat meshes sealing the edge |
| **Inside/outside** | A `UWRUndergroundSubsystem` (new): cavern volumes from `world_layout.json`; `IsUnderground(pos)` (world-map's function lives here); sub-zone lookup by volume, not by surface polygon; feeds audio (beds, reverb), sky (no sun, the circadian glow curve), spawns (underground night table 22:00 to 04:00), weather (Spore bloom only underground) and the lit/unlit check |
| **Light** | Lumen GI picks up emissive glow-moss, caps and gills, so the caves light themselves. A budget: emissive materials carry most of it; real point lights only at far-lights (one per hall) and the Heart Grotto's streets (≤ 24 in view). The circadian curve drives one `GlowScale` material parameter collection value for all fungal emissives |
| **Reverb** | Audio volumes per hall with their own reverb presets (F3 cathedral, F4 tight, F5 wet) |
| **Nav** | Navmesh per cavern cell; **wall and ceiling nav** for Sporestalkers and Webspinners via spline "climb lanes" authored on ceilings and walls (the creature attaches and follows the lane; it leaves the lane to drop). No full 3D nav needed |

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/spore.json`** | Same schema as `fire.json`: `id: "spore"`, `name: "Spore"`, `biome: "fungal"`, `starter: "spore_cloud"`, `resource {id: "spores", name: "Spores"}`, `rootPassive {id: "immune_system", name: "Immune system", text: SPEC}`, trunk and three branches with node ids `spore_cloud, spore_capacity, rot_chance, infect_strike; decay, corrode, blight, lingering, burst, pandemic; sporeling, swarm, host, hive_mind, thick_spores, fungal_behemoth; spore_armor, regrowth_spores, toxic_blood, adrenal, bloom_body, living_colony`, `kind` (ability/stat/capstone), costs, ranks and gates as 2.17. Node text copied from ST word for word |
| `Data/tuning.json` `resources` | `spores {capacity: 100, regen: 5, cloudRegenMult: 2.0, perDetonated: 3, plantSeconds: 20, plantMax: 10, pvpPlantMax: 5}` |
| `Data/tuning.json` `nodeEffects` | `spore.spore_capacity {sporesCapacity: 20, plantMax: 1}`, `spore.rot_chance {rotBuildup: 0.10, weaponRot: 5}`, `spore.decay {rotDamage: 0.08}`, `spore.lingering {rotDuration: 0.20}`, `spore.swarm {sporelingCap: 1}`, `spore.thick_spores {summonHealth: 0.25}`, `spore.regrowth_spores {regenHps: 0.4, minSpores: 40}`, `spore.bloom_body {statusBuildupTaken: -0.10}` |
| `Data/tuning.json` `passives` | `corrodeResist: 0.15, corrodeBoss: 0.10, burstRot: 6, burstRadius: 300, pandemicEvery: 2, pandemicRadius: 600, pandemicShare: 0.5, pandemicBoss: 0.5, hostSeconds: 15, hiveMindBonus: 0.20, toxicBloodPlant: 2, toxicBloodPoison: 15, adrenalPer: 2, adrenalMax: 30, pvpDetonate: 0.6, pvpColonyHeal: 0.5` |
| `Data/tuning.json` `spells` | Spore entries as combat 2.13 (KEPT) plus costs (`sporeCloud 15, blight 30, sporeling 25, sporeArmor 30, fungalBehemoth 60, livingColony 80`), `sporeling {bite: 10, plants: 1}`, `fungalBehemoth {slam: 30, plants: 2, stance: 2.0}`, `livingColony {radius: 500, plantPerSecond: 1, poison: 10, healPerEnemy: 0.015, healMax: 0.06}`, `detonate {radius: 2000, damage: 8, rot: 10, cast: 0.30}` |
| `Data/tuning.json` `status` | Poison and rot rows from combat 2.11 (KEPT); colours poison green, rot rust; `plantedSpores` as a stack type |
| `Data/tuning.json` `hazards` | `sporeCloud {poisonPerSecond: 12, rotAfter: 10, rotPerSecond: 12, breakSeconds: 3, tell: 0.9, minVisibility: 1200, windPush: 600, strongPush: 1500, flashDamage: 40, flashBurn: 40, flashTell: 0.9, flashGoneHours: 2, frostGoneHours: 1, wadMult: 0.3}`, `lureLine {stick: 2.0, reelUp: 300, reelSeconds: 1.5, tell: 0.9, crouchHeight: 150, cutRadius: 150, burnRadius: 300, windTear: 800}`, `springcap {launch: 900, aimDeg: 20, popAfter: 0.6, autoFromHeight: 600, throwSideways: 500, regrow: 20}`, `capFall {radius: 300, damage: 70, tell: 1.0, noiseRadius: 1500, moleWarn: 10}`, `foulAir {staminaRegen: 0.5, damageAfter: 20, dps: 3}`, `breathVent {every: 40, seconds: 6, width: 600, tell: 1.0, sporefallEvery: 30}` |
| `Data/tuning.json` `weather` | Fungal surface weights (world-map KEPT) and `sporeBloom {every: 1200, seconds: 180, spread: 1.5, tell: 30, miteRange: 2.0}` (survival-crafting KEPT) |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `ceiling_drop, gill_bite, wall_pounce, stalk_volley, spore_shake, mound_burst, lure_snap, rot_belch, shell_spin, latch_mite, burst_death, ceiling_pounce, web_shot, reel, tongue_whip, leap_slam, croak_blast, club_tail, garden_puff, burrow_pop, glow_flash, cap_puff, puffball_burst`; `warmLightAggro: 2.0` for Mycomites; `darkHunter` true for Sporestalker, Rotmaw, Webspinner |
| `Data/tuning.json` `bosses` | `mycelorn {stance: 175, parries: 3, statusResist: 0.3, gillPartHealth: 1500, gillOffSeconds: 30, chokeSeconds: 4, chokeStance: 2.0, chokeWind: 1.5, eruptChokeSeconds: 5, gateHealth: 600, gateBluntFire: 2.0, gateSeconds: 10, gateLane: 1200, gatePush: 1500, gateRegrow: 45, surfaceMin: 8, surfaceMax: 14, ringRadius: 600, ringTell: 1.2, podCount: 12, podEvery: 20, podCloud: 600, podCloudSeconds: 10, podBurn: 200, summitSeconds: 8, summitFree: 100, summitCooldown: 40, summitFromHp: 0.8, p3Every: 8, p3SeedMult: 1.25, sporefallCloudMult: 1.5}`, `sporemother {sacCount: 6, sacHealth: 150, sacFire: 3.0, hatchEvery: 25, hatchCount: 3, sacStagger: 2, ventFlash: 200}`, `rot_king {poolHeal: 0.015, poolRot: 15, poolSeconds: 20, freezeSeconds: 15, frostbiteStop: 12, windDry: 10}`, `weaver {webSpeed: 1.6, offWebSpeed: 0.6, webSheetEvery: 10, webAllSeconds: 60, fireRadius: 400, windTear: 800, flaskTear: 1200}`; per-attack rows from 2.13 and 2.14; run-back stone positions; lock points |
| `Data/tuning.json` `events` | `sporefall {warnSeconds: 300, breathHeight: 80000, patchEvery: 40, patchCloud: 600, bloomAfter: 10, bloomPoison: 20, bloomBurst: 400, summitHour: 12, ventEvery: 30, miteSwarms: 2.0, payout: 3.0, seeded: ["fungal_surface", "desert_d5_north400", "sky_spore_scar", "sky_wheel_isle", "sky_lid_gate"]}` |
| `Data/creatures.json` | 10 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `spore`, `resists ["spore_cloud", "foul_air"]`, taming, difficulty, food (deepcap for Shroomback), attacks, activity, loot, `birth: egg`, mount and rideSpeed for Tunnel toad, hauler for Shroomback, utility `light` (Glowgrub), `sense_dig_knock` (Truffle mole), `climbLanes` true for Sporestalker and Webspinner); a `sporeling` swarm row; 4 boss rows (`mycelorn` major, `sporemother`, `rot_king`, `weaver` with `roaming` and `route`) |
| `Data/spawn_regions.json` | Fungal regions F1 to F5 (F2 to F5 by cavern volume id) and `night` tables from 2.12.3 |
| `Data/items.json` | `deepcap, spore_sac, glow_moss, amadou, rhizomorph_cord, webspinner_silk, mite_chitin, saltpetre (exists in survival-crafting's list), stalker_stalk, rot_shell, fibrous_hide, toad_skin, shell_plate, venom_sac, velvet_pelt, glow_gland, spore_essence, core_mycelorn, hive_spore, trophy_mothers_cap, trophy_rot_crown, trophy_weaver_silk, queens_jelly, kings_slurry, star_thread, farmers_ring, deepfarmer_ring, knocker_idol, stipe_maul_bp, deepfarmer_decor_bp`; consumables `spore_wad, glow_cap_lantern, glow_paste, spore_pot, gale_flask, amadou_tinder, chitin_arrow`; lore items (9) |
| `Data/recipes.json` | The 2.19 recipes; Sporeweave and filter mask (KEPT inputs) |
| `Data/pieces.json` | `log_bed` (KEPT), `rhizomorph_ladder` (rope ladder, allowed underground) |
| `Data/world_layout.json` (world-map's) | Cavern volumes per sub-zone and level, the Heart Grotto ring, descents as links, `underground` flags |
| New/extended `Data/world_pois.json` (fire-frost's file) | F1 polygon, cavern POIs with x, y and elevation: 8 landmarks, 6 descents, 26 hand sites, 5 shrines, 4 ore galleries, Heart Grotto rooms, kit rules (2.4), the Weaver's route spline (on climb lanes), Breath vents, Draught gates, mound pod spots, terrace layout, springcap and lure-line spots |
| `Data/quests.json` | Ilmari's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 10, the sporeling and 4 bosses; pools `explore.fungal.surface/A/B/C/D`, `night.fungal.surface`, `combat.fungal`, `mini.fungal`, `boss.mycelorn` (3 sections), `sub.heart_grotto`, `sub.fall_field` with the 20 s planned silence, `event.sporefall`, `camp.fungal`; sub-zone beds and reverb presets |
| `Data/fx.json` | Spore cloud volume (Niagara + volumetric fog card), mote cling on the player (material param from buildup), cloud flash, spore snow, planted-Spore nodules and detonation chain, Pandemic threads, Corrode pitting, sporeling and behemoth burst-up, Living colony halo, lure-line glow and twang, springcap pop, cap-fall dust shaft, foul-air flame shrink, Breath column and vents, glow-cap ring pulse, fairy-ring eruption caps, vein race (Physarum), mound pod exhale, Draught gate gale, the Sporefall flush and bloom caps |
| `Data/combos.json` (R1 creates it) | The nine Spore pairs built in R8 (2.17) |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `PoisonBuildup`, `RotBuildup`, `SporesPlanted` (int), `bDetonation` |
| `WRStatusComponent.cpp/.h` | **Poison:** buildup meter, proc 0.4% max HP + 2/s for 30 s. **Rot:** proc 0.8% max HP + 4/s for 20 s, healing taken ×0.5 (`ApplyHeal` checks it), duration × Lingering, Corrode's resist drop while rotting. **Planted Spores:** a per-planter stack (owner, count, expiry 20 s), replicated count for the nodule FX; `Detonate(Owner)` returns the count and clears. Immune system flag skips poison and rot. Pandemic tick (2 s) |
| `WRCombatLibrary.cpp` `ApplyHit` | Plants from hits; Burst's splash rot; Toxic blood on being hit; Host on a kill with planted Spores; Hive spore boss part on kill; PvP multipliers |
| `WRPathComponent` / `WRAttributeSet` | **Spores resource:** a replicated `Spores` attribute with capacity from tuning + nodes; regen tick with the ×2 in-cloud check (asks `UWRHazardSubsystem` whether you're inside a cloud); +3 per detonated Spore; Adrenal stamina on detonate |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_SporeCloud` (AreaSpell pool that plants), `UWRGA_Detonate` (instant; walks the planted stacks within 2,000 cm through `ApplyHit` with `bDetonation`), `UWRGA_Blight` (AreaSpell pool that grows 50 cm/s; registers as a spore cloud), `UWRGA_InfectStrike` (UWRGA_Melee Wildbrand that plants 3), `UWRGA_Sporeling` and `UWRGA_FungalBehemoth` (`UWRGA_Summon`, combat's new class; summons are `AWRCreature` with `bPathSummon` so boss HP ignores them), `UWRGA_SporeArmor` (`UWRGA_SelfBuff`), `UWRGA_LivingColony` (`UWRGA_Transform`: aura component that plants, poisons and heals, and registers as a cloud). `ClassForNode`: a new `spore` block mapping `spore_cloud, infect_strike, blight, sporeling, fungal_behemoth, spore_armor, living_colony` (+ `detonate`, granted with `spore_cloud`). Passives (`corrode, burst, pandemic, host, hive_mind, toxic_blood, adrenal, regrowth_spores, bloom_body`) through `UWRPathComponent::HasNode` as Fire's are |
| New `WRHazardSubsystem` (or extend `WRWorldDressing` `FWRBiomeHazards`) | **Spore clouds as actors** (`AWRSporeCloud`: a sphere/capsule volume, Niagara, owner (world/player/boss), lifetime, `Push(Dir, Dist)`, `Flash()`, `Settle()`); continuous-inside timer per player for the rot-after-10 s rule; minimum visibility; registry queries (`IsInCloud(pos)`). Fire hits and burn procs inside a cloud call `Flash`; frost hits call `Settle`; wind hits (any `FWRHit` with a wind element or `AddWindImpulse`, biome-sky's shared entry) call `Push`. `AWRLureLine` (spline thread with a stick volume, reel, cut, burn, tear), `AWRSpringcap` (launch on jump input within its trigger, auto-fire from height, regrow), `AWRCapFall` (noise listener → knock tell → falling block via a trace), foul-air volumes (stamina regen, no sprint, no fire), `AWRBreathVent` ×3 (40 s rotation, plume volume up the Flue that biome-sky's `AWRSporePlume` reads) |
| `WRCreature.cpp` | Attack-row kinds from 3.1. **Climb lanes:** attach to a ceiling/wall spline, move along it, detach for `ceiling_drop`/`ceiling_pounce` (root-motion drop to a target point). **Lure snap:** a hidden Rotmaw with a lure mesh that uses the deepcap's Interact prompt, so picking it fires the grab. **Latch (mites):** limb socket attach (max 4), drain tick, detach rules. **Web shot / reel:** projectile that applies a `Webbed` status (2 s, mash); reel pulls along a lure line. **Spore shake / garden puff:** damage-window listener spawns a small `AWRSporeCloud`. **Warm-light aggro:** Mycomite perception checks the target's `bCarryingFire`. Tunnel toad mount: charged leap (12 m) with a landing ring; Shroomback hauler with the 3 m buildup aura; Truffle mole utility: sense (ore, deepcap, hidden walls) with knock FX, dig crawls, cap-fall warning |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `mycelorn`, `sporemother`, `rot_king`, `weaver`. **Mycelorn:** rooted phase 1 (no locomotion; yaw only), tendril clubs as traced bones on a chain rig, gills as a part with HP, the Heart-seed lock point; choke state on `AWRSporeCloud` overlap with its capsule while the cloud's last push was wind; `AWRDraughtGate` ×4 (plug HP, lane push volume, regrow); Summit call projectile that applies a `Summited` state to a tame (walk to the nearest terrace wall, climb anim, 8 s, freed by 100 damage on a stalk socket); the transition; **phase 2 travel:** pick a target point on the vein graph (a spline network in data), drive the vein material's race parameter along the Dijkstra path (the tell), spawn the eruption ring actor 1.2 s before, hide and re-show the body at the point; mound pods (12 actors, exhale timers, burnable); phase 3 hazard rotation (lure lines, wall exhales, cap-falls, terrace puffs) behind the artifact flag. **Sporemother:** 6 sac actors (HP, fire kill, hatch timer, stagger on death), egg toss projectiles that hatch. **Rot King:** rot-pool decals as volumes (heal him, rot players; freeze to ice on frost; dry on wind), lure into the clean channels. **Weaver:** roaming route on climb lanes (fire-frost route component extended to climb lanes), web sheet spawner (1 per 10 s) and speed by on-web check, Starfall drop, cocoon grab with haul-up |
| `WRWorldDressing.cpp` | Spawns kit and hand POIs from `world_pois.json`, including cavern POIs at their elevation; glow-cap ring waymarks with the night pulse toward the nearest undiscovered POI (reads the player's discovered set); deepcap nodes only at host clusters; summited-body props |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The Sporefall timeline: warn (Breath column colour and height, glow sync, Puffballs untether, the Ring pillar), seeded areas list, flush patch spawning on a 40 m² grid, bloom state on creatures (10 s in a flush cloud), burst on death, the noon summit for passives, dusk death and dawn deepcap, vent rate 30 s, Seed Vault door |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 Fungal shrines; Ilmari (MetaHuman, farm and terrace idle loops); quests |
| `WRBodyChange.cpp` (hero lane's) | Spore parameters: freckle field reveal by stage (nose and cheeks → shoulders and collarbones → arms and hands → full), glow only in the dark (reads the lit/unlit state), iris fleck ring, mossy hair tips, shoulder motes, the 0.3 Hz shimmer at 100, cyan breath in caves; blends with Fire, Frost, Necromancy |
| `WRUIHud` / `WRPlayerUI` | The Spores bar; poison and rot meters (combat's status UI); planted-count on the locked target |
| `WRAudioDirector.cpp` | Fungal pools and beds by `UWRUndergroundSubsystem` sub-zone; Mycelorn's 3 sections; the Fall Field silence; the Sporefall layer |
| `WRSky.cpp` | Underground: no sun or sky, the circadian `GlowScale` curve; surface Spore drift state; the Breath column (Niagara ribbon, height from data); foxfire on surface deadwood at night |
| `WRRaids.cpp` | The Spore side of the Flue invasion: mycelium bridge splines with a growth timeline (biome-sky's `AWRCloudBridge`-style), invader spore marks, the Weaver leading |

### 3.3 Assets and the art route

**Route (KEPT):**
1. Concept, picked by Aaron.
2. Hunyuan3D 2.1 base.
3. Blender finish: sculpt, retopo, real textures.
4. `Tools/art` rig scripts.
5. **Real clips** from Fab animal packs or Truebones mocap, retargeted with UE's IK Retargeter; hand-keyed only where no real animal exists (leg chains on the spider, tendrils on the boss), with Control Rig secondary motion.
6. Every creature shown standing at scale beside the hero before any behaviour is wired.

No engine primitives, no unfinished AI meshes.

| Asset | Route and clip source |
|---|---|
| Sporestalker | Concept from fossa + striped hyena + olm head + cordyceps crest; Hunyuan base; Blender: crest stalks as rigid cards on spine bones (they rise on an "angry" curve), gill ruff with a flare blend shape, hyphae fur as short groom cards. Clips: a Fab big-cat or hyena pack (walk, trot, run, pounce, claw, bite, death), plus a cat/lizard climb set for ceilings (retargeted to the climb lanes) |
| Rotmaw | Concept snapping turtle + cap mound; Hunyuan base; Blender: mound caps as a separate mesh with a shiver blend shape, lure on a tongue bone with an emissive deepcap material (same material instance as real deepcaps). Clips: a Fab turtle/tortoise pack (walk, snap, retract, spin), plus a keyed mound-burst rise |
| Mycomite | Sculpted tick with a cap; clips from a Fab spider/insect pack (8-leg walk, scuttle, rear, latch), retargeted |
| Webspinner | Concept huntsman + ogre-faced eyes; clips from a Fab spider pack (walk, run, pounce, rear, web shoot, idle), plus the climb lane set; abdomen dots emissive |
| Tunnel toad | Concept Goliath frog + olm skin; clips from a Fab frog/toad pack (sit, hop, leap, tongue, croak, swim) |
| Shroomback | Concept glyptodont + pangolin; clips from a Fab armadillo, tortoise or ankylosaur-style pack (walk, tail swing, tuck); the oyster garden as instanced caps on the shell with a regrow state (KEPT yield) |
| Truffle mole | Fab mole or badger pack plus a star-nose sculpt with 22 small tentacle bones (procedural wiggle in Control Rig) |
| Glowgrub, Capling, Puffball | Glowgrub: keyed caterpillar loop on a spline chain. Capling: a Fab rabbit/pika pack plus the cap mesh. Puffball: a floating sac with tendril cloth/chain sim and a slow buoyancy bob; keyed burst |
| Sporeling | Small biped fungal biter: a Fab goblin/imp clip set retargeted onto a squat cap-headed body (the player's summon uses the same rig with Spore-path trims) |
| Fungal behemoth | A heavy-biped clip set (Fab troll/golem) on a 4 m cap-backed body |
| Sporemother | Mycomite rig scaled ×15 with a translucent abdomen material (egg sacs as inner meshes); sacs on walls as separate actors |
| Rot King | Rotmaw rig ×2.25 with a crown-of-caps mesh, a drip Niagara, bronze hooks |
| Weaver | Webspinner rig ×2.25 with constellation-pattern emissive |
| Mycelorn | New concept (Aaron picks). Stipe and cap as a skeletal mesh with a 12-bone stem (rear, sink, bow) and a cap rig (tilt, flare); 12 heavy tendrils as 8-bone chains with bone-club ends (keyed attack clips on the chains + physics for the 88 light cords as rigid-body chains that follow); translucent amber stem with the titan ribcage and Deepfarmer bodies as inner meshes (the bodies use the hero's MetaHuman base sculpted into a fused pose, no animation). Gills as a part mesh with a glow param |
| Ilmari | MetaHuman preset (attractive, early 30s), outfit as 2.11 |
| Landmarks | Ancestor Cap (Blender, 90 m, a stem and cap with a gill underside; Nanite), the Breath (Niagara column), Kneeling Titan (Blender sculpt from the titan concept, stone material), Ring of Caps (instanced cap variants), False Sky (instanced lure-thread cards on the ceiling, ~60k instances, emissive), Shoestring Bridge (spline mesh cable + hangers + silk cards), Fall Field (bone scatter + champion sculpt), Drowned Granary (silo-tower kit), Sand Falls (Niagara sand ribbons + dune meshes) |
| Kit | The Nanite cave kit (3.0); giant cap set (parasol, ink cap, oyster shelf, honey cluster, puffball, deepcap, springcap) at 3 sizes each; rhizomorph cables (spline); mycelium sheets (cloth cards); Deepfarmer props in Blender (cap-houses, terraces, log beds, knocker idols, drying racks, granaries, clay jars, bronze bell, sickles, standing stones); summited body props (4 poses: clamped on a beam, on a rack, on a cable tip, on a rooftop) |
| Materials | Limestone pavement (clints and grykes), flowstone, glow-moss (emissive with the circadian param), mycelium (subsurface, white), rhizomorph (black glossy), cap skins (ivory, rust, honey, umber), water teal over white sand, Heart amber (translucent) |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles and Kenney, layered in a DAW per species. About 8 files per creature, 12+ per boss. Loudness matched.
- **Music:** the 2.16 cue sheet chosen from libraries first (about 22 cues counting stems), credited. Water-drumming and udu stems from Freesound where library pieces need a layer.
- **Beds:** 5 sub-zone beds, the Heart Grotto bed, the Fall Field roar; per-hall reverb.

### 3.5 Map dressing

- `Tools/terrain/dressing.mjs` reads the Fungal entries in `world_pois.json`:
  - It places hand sites at their coordinates and elevations.
  - It fills kit POIs on the surface with Poisson spacing of 150 m or more, and along cavern routes at one per 150 m.
  - It walks routes (surface paths, cavern routes, cable bridges) and adds a kit POI wherever a point is more than 125 m from any POI.
- **Spore clouds** are placed from data at Puffball fields, host clusters, the Breath Vents and the Great Doline ledges, never within 4 m of a ledge, jump or springcap (2.8 rule).
- **Lure lines** are scattered on F3 ceilings by density maps (dense in the False Sky, sparse elsewhere), never over a route's only path without a crouch-height gap.
- **Ground layers:** limestone pavement, salt-grass, moss, mycelium mat, sand, rimstone, painted from F1 and cavern-floor polygons by the auto-material.
- **The B1 landmass itself** comes from world-map's terrain step (KEPT). This lane adds the holes, the cave kit halls and the dressing.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R8 statuses: poison and rot** in `WRStatusComponent` + `FWRHit` buildups + meters + antidote cure (with combat's C-steps); **the cavern frame** (3.0): `UWRUndergroundSubsystem`, the `IsUnderground` fix, the first cave-kit hall | Everything below uses them |
| 1 | **Proof of one: the Rotmaw at a lone-deepcap clearing in the first Spore Gardens doline, end to end.** Concept (Aaron picks), mesh, real turtle clips, voice, attack rows with socket hits (mound burst, lure snap, beak snaps, rot belch, shell spin, lunge-shove), the deepcap-lure tell, a real deepcap host cluster beside it to compare, one Puffball field with a spore cloud (12/s poison, rot after 10 s, push/flash/settle), the glow-moss lighting and circadian glow, and the doline (D1) dressed with its terraces | Proves the cavern frame, the ambush with a readable lure rule, visible-contact damage, real clips, a fused voice, poison and rot, the main hazard and its three answers, and a dressed place in one item |
| 2 | **The Spore path:** `spore.json`, the Spores bar and planted stacks, Detonate, the 7 ability classes in `ClassForNode`, the passives, the proc FX, the nine CB combos | The round's headline; needs only step 0 |
| 3 | Cavern geometry and dressing: the five sub-zones, the 8 landmarks, the 26 hand sites, kit fill, descents, ore galleries, resources | The world the rest stands in |
| 4 | The other 9 creatures and sporelings with clips, voices and rows; climb lanes; day and night tables; living-world beats | Reuses step 1's frame |
| 5 | Micro-hazards (lure lines, springcaps, cap-falls, foul air) and weather (Spore drift, Spore bloom) | After the ground exists |
| 6 | The Sporemother (sacs), then the Rot King (rot pools, frost and wind), then the Weaver (roaming on climb lanes, webbing) | Minis in order of new tech |
| 7 | Mycelorn: the Heart chamber, the rooted phase 1, the transition, the vein-travel phase 2 with fairy-ring tells and mound pods, Draught gates and the Gale flask, Summit call, phase 3 behind the artifact flag; drops; music | The biggest single item; needs creatures, clouds, lure lines and cap-falls |
| 8 | Shrines, Ilmari and his 6 quests, run-back stone, fast travel; the Breath vents at the Flue floor feeding biome-sky's plumes | After POIs and the hero rig |
| 9 | The Heart Grotto and the five secrets | After the hazards and the boss chamber |
| 10 | Music set, sub-zone beds, reverb, the Fall Field silence | Independent; can slot in after step 3 |
| 11 | The Sporefall event | Needs the creatures, clouds, the Grotto's Seed Vault and the Sky and Desert borders |
| 12 | Gear: Sporeweave fit on the MetaHuman bodies, cavern items, log bed and rhizomorph ladder | After the R2 slot split (survival-crafting) |
| 13 | Body change for Spore | After the MetaHuman hero (R0) |
| 14 | Wind ↔ Spore invasions | Needs both biomes' roaming minis and the invasion frame |

**Gates:** compile, cook and package only. Aaron tests each step.

### 3.7 Spot-check against THE BAR (by reading this plan; for the build round to re-check in code)

| Bar item | Where it's met |
|---|---|
| Full hit stack, hitstop, coil, one verb per button, 0.33 s buffer | Combat's systems (KEPT); every new ability here routes through them |
| Real weapon, claw or projectile; damage only on visible contact; small cue; tells read at 15 m | Every row in 2.12.2, 2.13, 2.14 names the body part and a glint or pose; boss moves use traced bones (3.2) |
| Projectiles ≥ 0.9 s warning, never home | Stalk volley ≥ 1.2 s, Web shot 0.85 s windup + travel ≥ 0.9 s at 500 cm minimum range, Egg toss, Spore mortar, Summit call, Web volley: all ballistic or straight, none homing |
| No attacker limit; players die; no floors; no helpers after a fail | 2.12 rules; every hazard "can kill"; nothing added after deaths |
| Terrifying, readable, believable monsters; unique fused voices; real clips | 2.12.1 real-animal anatomy per creature; 2.15; 3.3 clip sources |
| POI every 150 to 250 m; 3 to 5 sub-zones; story only through the place | 2.4 (115 POIs, 125 m max gap); 2.2 (5 sub-zones); 2.20 (no scenes, no narrator) |
| Music always audible, ducks ≤ 4 dB, only planned silences | 2.16 (one planned silence, the Fall Field, KEPT) |
| Mouse and pad on every screen, F1 | No new screens here; the Spores bar and meters use combat's HUD rules |
| People attractive, never sanitized | Ilmari (2.11), the Sporeweave cuts (2.19), the freckle body change (2.18) |
