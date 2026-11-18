# Cook: biome-swamp (Swamp / Blood & curse, round R2)

Lane: biome-swamp. Written 2026-11-13. Scope: FULL-GAME 3.2.8, the Swamp rows of 3.3, and the Blood & curse row of 4.2. This covers the whole Swamp and the whole Blood path.

**Tags.** **SPEC** keeps Raven's design, or a built thing, exactly as written. **GAP-COOKED** is this lane's design and replaces the FULL-GAME first draft. **KEPT** means a number or name another 11-13 cook already set (combat, world-map, survival-crafting); it's restated here so the build has one page, and this lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** where the Swamp sits, the camps, the arena and Coven coordinates, the POI count (102), the weather weights, the Mire Coven, and the Bled Roc.
- **combat.md:** the bleed and curse statuses, the Blood spell and Wildbrand numbers, the creature attack-row schema, the boss template, and visible-contact damage.
- **survival-crafting.md:** Mirehide, sealed waders, swamp crops, brews, fish, the stilt-house set, and the three swamp farm set-pieces.

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / source | What it does | What we take |
|---|---|---|
| **Valheim, the Swamp** ([PC Gamer](https://www.pcgamer.com/how-to-find-valheim-swamp-key-biome-seed), [PC Gamer, Bonemass](https://pcgamer.com/valheim-bonemass-summon-withered-bone), [XGamingServer guide](https://xgamingserver.com/blog/valheim-swamp-guide/)) | The swamp is the iron gate: sunken crypts hold the metal. Leeches are a water threat you avoid by staying out of the water, and you can level ground with a hoe to make paths. Draugr spawn from body piles until you break the piles. The boss needs a poison-resist mead and a blunt weapon | **The water is the threat and the dry route is the skill.** Every route through the swamp has a dry line (boardwalk, root, mound) and a fast wet line (bleedwater). Gear (sealed waders) or a mount turns the wet line into a shortcut. Bonemass's mead-plus-weapon check is exactly Raven's "hazards are gear checks". The Bloat's salt counter copies this "bring the right prep" rule. **We don't copy** infinite spawners: no body piles, because zombies are out (DD) |
| **Dark Souls Blighttown, Elden Ring Lake of Rot** ([Wikipedia: Blighttown](https://en.wikipedia.org/wiki/Blighttown), [Source Gaming](https://sourcegaming.info/2024/08/17/center-stage-blighttown-dark-souls/), [Game Developer](https://www.gamedeveloper.com/design/what-everyone-gets-wrong-about-soulslike-design)) | Remembered as the most hated areas. The swamp slows *and* poisons on contact, the framerate collapsed in the vertical clutter, and it gets better once you learn its secrets | **Lessons, as rules:** (1) Never stack slow and status on the same water: bleedwater bleeds but doesn't slow, and mud slows but doesn't bleed. (2) The swamp is flat and open-skied, so frame budget goes on fog and water, not on stacked geometry. (3) Mastery shows: learned routes, salt, waders and mounts make the swamp fast for a veteran |
| **Monster Hunter World, the Rotten Vale** ([sac.media](https://sac.media/2019/10/31/getting-lost-and-loving-it/), [PC Gamer](https://pcgamer.com/monster-hunter-world-showcases-toxic-rotten-vale-swampland), [MH Wiki: Vaal Hazak](https://www.monsterhunterwiki.org/wiki/Vaal_Hazak_(MHWorld))) | The whole map comes from one idea: animals come here to die, so scavengers live here, and the elder dragon lives in symbiosis with the rot | **One ecological idea drives the biome:** *the Mire is fed blood.* The Coven tithed blood to the Mother (Vharra), the leeches are her brood, the crows and Bogstalkers feed on what the leeches drain, and the bloodmoss grows where blood soaks in. Every creature, hazard and POI follows from that |
| **Bloodborne, Rally** ([Fextralife: Rally](https://bloodborne.wiki.fextralife.com/Rally), [GameSpot](https://www.gamespot.com/articles/ps4-exclusive-bloodborne-features-health-regenerat/1100-6421712/), [Destructoid](https://destructoid.com/bloodbornes-health-regaining-system-detailed)) | HP you lose shows as an orange segment for about 5 s, and hitting back wins it back. That pushes you to stay aggressive | **The Blood resource bar is a rally bar for HP you choose to spend.** HP spent on Blood abilities shows as a dark "Spilled" segment that your lifesteal refills at double rate for 8 s. Damage taken never goes into it: spending is refundable, getting hit isn't. This keeps Raven's "spends your HP; lifesteal earns it back" and gives the path a real bar (4.1 rule) |
| **Elden Ring, Hemorrhage** ([Fextralife: Bleed](https://eldenring.wiki.fextralife.com/Bleed), [eldenring.wiki.gg](https://eldenring.wiki.gg/wiki/Bleed)) | Buildup fills a hidden meter. A proc deals a flat amount plus a share of max HP (about 15% on normal enemies, about 10% on bosses) | Already KEPT by combat (12% max HP + 15, bosses ×0.7). The swamp tunes its own creature buildups to that and its hazard to 10 per second |
| **Ark** swamp (Kaprosuchus pulls from trees, Sarco ambushes, leeches latch on and need fire or salt to remove), **Conan Exiles** and **Rust** swamps (fog, ambush water), **Horizon** and **Ghost of Tsushima** (wind and creatures guide you to places) | Swamp predators are pull-and-grab ambushers, and leeches latch and stay on | Murkjaw's death roll, the Bogstalker's tongue pull and the Gorgeleech latch are the three grab verbs, each with its own tell. Salt and fire remove latches, from real leech lore. Mirestrider flocks and Hexcrow murders show where kills and carcasses are, as Ghost of Tsushima's birds do |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Sphagnum in WWI ([The Conversation](https://theconversation.com/war-and-peat-how-bog-moss-helped-save-thousands-of-lives-in-world-war-i-106630), [Smithsonian](https://www.smithsonianmag.com/science-nature/how-humble-moss-helped-heal-wounds-thousands-WWI-180963081/), [Science Museum](https://collection.sciencemuseumgroup.org.uk/objects/co8567010/surgical-dressing-substitute-europe-1914-1918)) | Bog moss became the standard British field dressing. It soaks up more than twice what cotton wool does and is mildly antiseptic | **Bloodmoss** is a red sphagnum. It's the poultice that cures bleed, the hide cure for sealed waders, and the base of blood brews. Real use, swamp colour |
| Bog iron ([Wikipedia](https://en.wikipedia.org/wiki/Bog_iron), [Hurstwic](https://www.hurstwic.com/history/articles/manufacturing/text/bog_iron.htm), [USGS](https://www.usgs.gov/publications/bog-iron-formation-nassawango-watershed-maryland)) | Iron that bacteria drop out of groundwater at the bog surface. Most Viking iron came from it, and a bog regrows its iron over a generation. Bloomery yield is low | **Bog iron lumps** sit in red-stained shallows and at the falls. They smelt to iron at a third of cave ore's yield. The red water of the Weeping Wall is that iron, rusting |
| Will-o'-the-wisp ([National Geographic](https://www.nationalgeographic.com/environment/article/bogs-fens-wetlands-folklore), [Popular Science](https://www.popsci.com/jack-o-lanterns-marsh-lights/), [Folklore Scotland](https://folklorescotland.com/will-o-the-wisp/)) | Lights over bogs, probably phosphine and methane from decay. In folklore they lead travellers off the safe path; in Wales they're corpse candles | **Marsh gas vents** are a hazard and a tool (fire sets them off). **Corpse candles** drift over the Mere at night; following one without stepping off its line finds a secret (M-S1) |
| Slavic swamp spirits ([Wikipedia: Bolotnik](https://EN.wikipedia.com/wiki/Bolotnik), [Wikipedia: Kikimora](https://ipfs.ssi.eecc.de/ipns/en.wikipedia-on-ipfs.org/wiki/Kikimora), [Ancient Pages](https://ancientpages.com/2016/11/16/evil-spirits-demons-marshes-swamps-slavic-folklore/amp)) | The bolotnik lures victims by copying animal calls and lights the water's surface. The kikimora is the swamp wife of the leshy | The **Hex-Warden** copies calls: she plays a tame's own cry to pull it off you. The Coven's hut on bird legs (Baba Yaga) becomes the **Walking House** |
| Jenny Greenteeth ([Wikipedia](https://en.wikipedia.org/wiki/Jenny_Greenteeth)) | In north-west England, duckweed on a pond meant the hag was under it | **Duckweed is the Murkjaw tell.** A carpet of duckweed over black water means a Murkjaw may lie there. The weed parts in a line 0.9 s before it bursts. Folklore becomes a readable rule |
| Electric eels leap to shock ([Vanderbilt](https://www.vanderbilt.edu/research/2016/06/06/electric-eels-make-leaping-attacks/), [PNAS via Vanderbilt PDF](https://cdn.vanderbilt.edu/vu-wordpress-0/wp-content/uploads/sites/182/2018/04/19174939/2016-Leaping-eels-electrify-threats-supporting-Humboldt%E2%80%99s-account-of-a-battle-with-horses.pdf)) | Current runs through water into whatever touches it, and eels leave the water to hit harder | **Vharra's lightning weakness is physical:** shock the water while she's in it and she convulses. Players standing in that water get shocked too |
| Shoebill ([Wikipedia](https://en.wikipedia.org/wiki/Shoebill), [Britannica](https://www.britannica.com/animal/shoebill)) | Silent stork with a giant hooked bill; its bill-clatter sounds like a machine gun | **Mirestrider's** head and alarm call |
| Medicinal leech (general zoology) | Three jaws in a Y with about 100 teeth each; ten eyes; a front and a back sucker; it loops like an inchworm on land; olive body with orange-red stripes | **Gorgeleech** body and gait, and **Vharra's** three-way splitting jaw, ten eyes, stripes and anchor disc |
| Hippo "blood sweat"; babirusa tusks; horned-frog scream; apple-snail pink egg clutches; crocodile red eyeshine; Spanish moss; mangrove prop roots and pneumatophores; bald-cypress knees; floating peat mats (schwingmoor); bog bodies (Tollund Man) with cords at the neck | Real swamp biology and archaeology | Mudhulk's red sheen. Bogsnout's tusks. The Bogstalker's scream. Slimeshell eggs as bright pink clutches on reeds (the steal-an-egg target is readable). Murkjaw's red eyes on night water. The Sunken Coven under a floating peat mat. Bog bodies at the peat cut |

### 1.3 Lessons, short

1. **One idea feeds everything:** the Mire is fed blood.
2. **Every water threat has a readable surface tell:** duckweed means Murkjaw, red ripples mean bleedwater, a glossy grey sheen means quag, bubble clusters mean gas.
3. **Never slow and status on the same ground.**
4. **Three grab verbs, three different answers:** death roll is dodge only, tongue is roll sideways, latch is shake, burn or salt.
5. **Dry line vs wet line** on every route. Gear and mounts buy the wet line.
6. **The Blood bar is a rally for spent HP**, never a safety net. Players die: a Blood spell can take you to 1 HP, and the next hit kills you.
7. **Colour law:** crimson (#DC143C) is the unblockable cue. All Blood path and swamp red is **garnet and oxblood** (#7A0A14, #4A0A0F, rust #8A3B1E), never crimson. That keeps the one warning colour clean in the reddest biome.

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item | This lane |
|---|---|
| Path theme lifesteal, hexes | SPEC (DD) |
| Look, sub-zones, landmarks, POIs, hidden areas | GAP-COOKED 2.1 to 2.6 |
| Signature resource bloodmoss | GAP-COOKED 2.7 (the name KEPT) |
| Hazard: bleedwater | 10 per second KEPT (3.3); tells and micro-hazards GAP-COOKED 2.8 |
| Weather | Weights KEPT (world-map); effects reconciled 2.9 |
| Event: the Red Tide | GAP-COOKED 2.10 |
| Shrine, path master | RAVEN-PLANNED exist; who and where GAP-COOKED 2.11 |
| 10 creatures | Names, roles and taming KEPT from the FULL-GAME draft (combat, survival and world lanes already use them); bodies, attacks, voices and tables GAP-COOKED 2.12 |
| 3 minis, 1 major | Names and premises kept; fights GAP-COOKED 2.13, 2.14 |
| Voices, music | GAP-COOKED 2.15, 2.16 (tempo, key and instruments SPEC 3.3) |
| Blood tree | Nodes SPEC (ST). Spell and Wildbrand numbers KEPT (combat). Passives, stat nodes, resource and root passive GAP-COOKED 2.17 |
| Body change | GAP-COOKED 2.18 |
| Gear | Mirehide and sealed waders KEPT (survival-crafting); swamp items GAP-COOKED 2.19 |
| Story through environment | GAP-COOKED 2.20 |
| Invasions Lightning ↔ Blood | GAP-COOKED 2.21 |

### 2.1 Where it sits and how it reads (KEPT placement, GAP-COOKED look)

**Placement (KEPT, world-map 2.2):**
- Belt B3, east of the Spine, under the Crown's east face.
- y −5,400 to −2,400. Land from the cliff foot at about x +300 to the sea at about x +2,450, with a ragged coast of channels and bays. 4.0 km² of land and shallow water. Open murk counts; the open sea doesn't.
- **Neighbours:**
  - Ocean to the north, through a brackish mangrove estuary.
  - Frozen tundra to the south, through the Rime Sound neck: a frozen bog with stopped bubbles.
  - Storm peaks across **Stormgut** (east mouth at +200, −4,900).
- **Bands:** lowland x > +1,800 (coast), midland +1,100 to +1,800, highland < +1,100 (against the Crown's cliffs).
- **Camps (KEPT):** (+2,250, −2,750), (+2,250, −3,800), (+2,150, −4,800).

**The look in one line.** A flat, low, black-water world under the Crown's sheer east face, where five red waterfalls drop out of the clouds. Close up it's dense with mangroves, bald cypress hung with grey Spanish moss, and duckweed carpets. Rotting Coven boardwalks thread through it, bone mounds rise like islands, and low fog sits at knee height. The sky is the one big open thing.

**Read rules (GAP-COOKED):**
- **Height draws the eye.** The land is flat, so every POI reads by height: dead cypress, stilt huts, cage gallows, bone mounds, the falls.
- **Fog is low, sky is clear.** Fog sits at 0 to 2.5 m on calm days, so silhouettes read above it. Only the Fog and Miasma weather states raise it.
- **Water has four looks, and each one tells you what it is:**
  1. **Clear black murk:** safe, swimmable. Reflections are sharp.
  2. **Duckweed carpet:** Murkjaw ground.
  3. **Bleedwater:** oxblood-brown, with thin leech ripples that never stop moving and a faint metal-red film.
  4. **Quag:** glossy grey mud with slow bubbles.
- **Night light:** Lanterntoad rings (pulsing green-gold throat sacs), corpse candles (pale blue, over the Mere only), the red mist over the Mother's Throat, Coven bone-lanterns on lit shrines, and Murkjaw eyeshine (red pairs on water) (world-map readable-nights rule).

### 2.2 Sub-zones (GAP-COOKED; the five names from FULL-GAME's list)

| # | Sub-zone | Where (x, y m) | Band | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|---|
| M1 | **Reed Flats** | Coast band x +1,800 to +2,450, y −2,400 to −4,500. Camps 1 and 2 | Lowland | Tidal reed beds 2 m tall, brackish mudflats, peat banks, wild bog-rice paddies, salt pans. Mostly dry routes on reed causeways | Reed gold #B49A5A, mud grey-brown #5A4E40, salt white #E6E1D3, sky-water steel #6F7F86 | Reed stacks, fish weirs, salt pans, newcomer wrecks, rice terraces gone wild, stilt huts | Wind in reeds, distant surf, Reedfowl calls, frog chorus at dusk |
| M2 | **Mangrove Thickets** | North: x +1,100 to +2,450, y −4,500 to −5,400. Camp 3. Grades into the Ocean's estuary | Lowland to midland | Red mangrove prop roots arching over tidal channels, pneumatophore spikes in the mud, root-bridges you walk along. Brackish. Channels 2 to 4 m deep | Root red-brown #5C3A2A, leaf dark green #2E4A2E, channel teal-black #1E2B2A | Root arches, oyster-crusted roots, crab holes, the Estuary Wreck | Water lapping, crab clicks, tide gurgle in the roots, distant gulls |
| M3 | **Rotting Boardwalks** | Centre: x +1,100 to +1,800, y −3,400 to −4,500 | Midland | Open murk between low raised-bog domes, crossed by the Coven's boardwalks (2 m wide, broken in places). Bloodmoss domes glow dull red in low sun | Black water #121414, plank grey-green #4E5248, bloodmoss garnet #7A1E22, fog white | Boardwalks, stilt houses, the Walking House, peat cuts, fetish posts with crow skulls, hanging bottle charms | Plank creaks, water drips, hanging charms chiming, Hexcrow caws far off |
| M4 | **Murk Pools** (the Red Pools) | South: x +900 to +1,800, y −2,400 to −3,400. Includes the Deep Pool (+1,300, −3,100) and the Frost border | Midland to highland | Bald cypress swamp: cypress knees, Spanish moss curtains, duckweed carpets, the bleedwater pools, karst sinkholes. The thickest canopy | Cypress grey #6A6458, moss grey-green #8A9078, duckweed lime #8DB040, bleedwater oxblood #4A1A14 | Cypress knees, cage gallows, sinkholes, the Hanging Cypress, leech ponds | Bullfrog chorus, insects, a slow drip, a low pulsing hum near bleedwater (the leeches) |
| M5 | **Bone Mounds** (the Crown Foot) | West: x +300 to +1,100, y −3,400 to −5,000, under the Crown's cliffs. Holds the Throat (Vharra), the Weeping Wall, the Ossuary and the Stormgut mouth | Highland | Raised Coven ossuary mounds of packed bone and peat, linked by bone-paved causeways over bleedwater. The five red falls pour from the cliffs. Mist from the falls | Bone #D6CDB8, iron-red falls #9A3B22, cliff slate #3E4148, mist pink-grey #B8A8A8 | Bone mounds, fetish forests, copper rods (Rodwarden), rusted cages, the Bled Roc | Waterfall roar (rises toward the cliffs), crow murder chatter, a low wind off the cliffs |

**Blends:** 60 to 120 m between sub-zones (world-map rule 5). Frost border (M4 south, y −2,400 to −2,550): frozen bog with bubbles stopped in the ice, cypress knees iced. Ocean border (M2 north): brackish estuary, mangroves give way to salt marsh.

### 2.3 Skyline landmarks (GAP-COOKED; 7, one per ~0.6 km², each read from 1 km)

| Landmark | At (x, y) | What it is | Reads at night as |
|---|---|---|---|
| **The Weeping Wall** | +330, −3,500 to −4,300 | Five waterfalls 300 to 900 m tall off the Crown's east face. The water is rust-red with bog iron (real: iron-rich water oxidising, like Antarctica's Blood Falls). Red-stained cliffs, pink mist at the base. Bog-iron shallows below | Moonlit red streaks; the mist glows faintly from the Falls Shrine braziers |
| **The Mother's Throat** (super landmark, reads from 3 km) | +900, −3,600 | Vharra's flooded sinkhole, 110 m across. Ringed by 12 giant dead cypress, 40 m tall, leaning inward, with rusted cages hanging from them. Six green-copper Rodwarden rods stand inside the ring. A column of red mist rises off the water | A red-lit mist column 150 m tall (lit from the water below) and the cage silhouettes |
| **The Bled Roc** | +420, −4,860 | The Rodwardens' champion (world-map SPEC): a giant roc skeleton, 45 m wingspan, sprawled over the Stormgut mouth. Coven bone hooks still in its wing bones, chains running from them into the bog. Leech-scarred hide still on the ribs | Ribs against the lightning flicker from the gut |
| **The Walking House** | +1,500, −3,850 | A Coven witch hut 15 m up on two giant bird-leg stilts carved from black cypress, with clawed feet sunk in the bog (Baba Yaga). The path master lives inside | Warm window light and a smoking chimney: the only home-light in the swamp |
| **The Hanging Cypress** | +1,180, −2,950 | One bald cypress, 60 m tall, the oldest in the swamp. Hundreds of bottle charms, bone chimes and 9 iron cages hang from it; one cage holds a skeleton holding a key | Bottle charms catch the moon; Lanterntoads ring its knees |
| **Mangrove Cathedral** | +1,500, −4,950 | A single giant red mangrove whose prop roots arch 20 m over a crossroads of channels, like a vaulted nave | Lanterntoads in the arches |
| **Corpse-Candle Mere** | +1,350, −4,200 | The largest open water in M3, a 300 m mere. On the far side stands the skeleton of a giant fish weir | Pale blue corpse candles drifting low over the water every night |

### 2.4 POIs: 92 surface + 10 in the Sunken Coven = 102 (count KEPT from world-map; contents GAP-COOKED)

Counts by sub-zone: M1 22, M2 16, M3 20, M4 18, M5 16.

**Fixed (counted):** 3 camps, 4 caves (2.6), 7 landmarks, 5 shrines (2.11), 3 mini lairs, the Throat, the Stormgut approach.

**Hand-made story and set-piece sites (24):**

| # | Site | Sub-zone, at | What the player finds (environment only) | Pays |
|---|---|---|---|---|
| 1 | **The Rod Line** | M5→M3, from +450, −4,800 running east 600 m | Nine Rodwarden copper rods sunk in the bog in a line from Stormgut: the storm people's advance. A Rodwarden skeleton leans on each rod, every one with leech-scar pits in the bone and a Coven bone hook in the ribs. The last rod is bent double | Copper ×3 per rod; lore item "Rodwarden's tally-cord" |
| 2 | **The Thorn Gate** (Coven pass fort) | M5, +520, −4,950 | A Coven fort of driftwood and sharpened bone stakes facing the gut. All its stakes point west. Its gate is burnt from the west side: lightning-glassed scorch | Bone ×10; bow and bone arrows; view down Stormgut |
| 3 | **Hex-Warden's Ossuary** | M5, +700, −4,300 | A bone mound 25 m high. Four fetish totems on its shoulders, crow skulls nailed in spirals, a stone basin of black dried blood at the top | Mini arena (2.13.2) |
| 4 | **The Throat Stair** | M5, +1,010, −3,560 | A bone-paved stair down into the sinkhole, flanked by Coven tithe basins. Each basin has a stone channel running down toward the water. The run-back stone stands 50 m out | Vharra's door; run-back stone |
| 5 | **The Weeping Pools** | M5, +380, −3,900 | Red plunge pools at the foot of the falls. Bog-iron crust on every stone. A Coven smelting pit of clay bloomeries, cold, with slag heaps | Bog iron ×12; a "Bloomery mould" recipe scrap (bloomery piece, 2.19) |
| 6 | **Hexcrow Rookery** | M5, +620, −4,620 | A grove of 30 dead cypress black with crows. The ground is white with droppings and picked bones, including a Mudhulk skull | Hexcrow feathers ×20 (fletching); a crow murder fight |
| 7 | **Mother Maw's Nest** | M4, +1,500, −2,700 | A rotting-vegetation nest mound (real croc nests are compost heaps) in a duckweed pool, ringed by a broken boardwalk square. Eggshells and a newcomer's boot | Mini arena (2.13.1) |
| 8 | **The Bloat's Wallow** | M4, +1,050, −2,600 | A sunken bleedwater basin with a Mudhulk carcass drained to a husk, skin collapsed on bone | The Bloat's lair (2.13.3) |
| 9 | **The Deep Pool** | M4, +1,300, −3,100 | A perfectly round black pool 40 m across, roofed by a floating peat mat with a gap in the middle. Iron cages hang from three cypress over it, their chains running down into the water | Sunken Coven entrance 1 (2.5) |
| 10 | **The Hanging Cypress** (base) | M4, +1,180, −2,950 | Cage gallows; the key-skeleton cage 18 m up (climb the knees and lower branches) | The **Coven key**: opens the Walking House cellar (M-S2); Cage Shrine |
| 11 | **The Leech Pond** (KEPT set-piece, survival-crafting) | M4, +1,650, −2,550 | A bleedwater pool with a pair of sealed waders hung on a post | Sealed waders (worn, 40% durability) |
| 12 | **Frozen Bog** | M4 south, +1,250, −2,460 | The Frost border: bubbles stopped in clear ice, a Reedfowl frozen mid-step, a Rime-folk inuksuk on a Coven bone mound (the neighbours touched) | Clear ice, lichen |
| 13 | **The Walking House** | M3, +1,500, −3,850 | The bird-legged hut. A rope ladder hangs down. Inside: the path master's home, a cauldron hearth, drying bloodmoss, a reliquary shelf | Path master (2.11) |
| 14 | **The Tithe Mound** | M3, +1,640, −3,720 | A low bone mound with a stone tithe basin and a ring of fetish posts | Great Shrine (2.11) |
| 15 | **The Peat Cut** | M3, +1,250, −3,750 | A peat cutters' trench 60 m long in a raised-bog dome. A spade left upright, stacked turves, and three preserved bog bodies laid out side by side, each with a braided cord at the neck (sacrifice, Tollund-style), their faces calm | Peat ×20 (fuel); lore item "Braided cord" |
| 16 | **Bloodmoss Dome** | M3, +1,200, −4,000 | A raised bog dome 200 m across, carpeted in red bloodmoss, springy underfoot. Ringed by pitcher plants | The richest bloodmoss field (24 nodes) |
| 17 | **The Drowned Causeway** | M3, +2,000 → +1,050 along y −3,700 | The Coven's main boardwalk from the coast toward the Throat, broken in four places (jump, wade or swim) | The main dry line (2.20); counts as 1 |
| 18 | **Corpse-Candle Mere** (shore) | M3, +1,350, −4,200 | The far shore holds the giant fish weir; a punt half-sunk at the near shore | Secret M-S1 at night |
| 19 | **The Stilt Village** (KEPT set-piece) | M1, +2,000, −3,300 | Rice paddies gone to reeds, a still dripping slime, a hex-marked door | Slime, bog rice seeds |
| 20 | **The Clay Pit** (KEPT set-piece) | M1, +2,100, −4,200 | Wet clay banks, a potter's wheel, fired pots | Clay |
| 21 | **The Salt Lick** | M1, +2,300, −3,550 | A brackish salt pan where Mudhulks come to lick. A broken salt-rake | Salt ×10 (the Bloat counter, 2.19); easy Mudhulk taming at dawn |
| 22 | **The Estuary Wreck** | M2, +2,050, −5,200 | A newcomer ship sunk to its rails in the mangroves; a Murkjaw nest under the stern; a salvage chest in the captain's cabin (swim in) | Iron nails ×8, rope, canvas, lore item |
| 23 | **Mangrove Cathedral** (floor) | M2, +1,500, −4,950 | A crossroads of channels under the root arches; a Coven root-altar in the middle | Root Shrine |
| 24 | **Gar Weir** | M2, +1,800, −4,700 | A woven fish weir with bog eel in it and a gar skeleton 2.5 m long on the bank | Bog eel; fishing spot |

**Kit POIs (the generator places about 49 with variants; world-map density rules):**

| Kit piece | Sub-zones | Variants | About |
|---|---|---|---|
| Coven fetish post (crow skull on a bone pole, a hung bottle charm; the waymark, world-map SPEC) | all, 120 to 200 m apart along Coven routes | 3 | 8 |
| Stilt hut (empty, looted, or holding a Lanterntoad nest) | M1, M3 | 3 | 5 |
| Bloodmoss hummock (6 to 10 nodes) | M3, M4 | 3 | 6 |
| Bog-iron shallows (red crust, 3 nodes) | M1, M4, M5 | 2 | 4 |
| Cage gallows (a cypress with 1 to 3 cages; loot in one) | M4, M5 | 3 | 4 |
| Slimeshell egg reeds (pink clutches) | M1, M4 | 2 | 4 |
| Kill set-piece (Gorgeleech pack on a drained Mudhulk; Hexcrows on a carcass) | M3, M4 | n/a | 4 |
| Marsh-gas field (6 to 12 vents) | M3, M4 | 2 | 3 |
| Sinkhole (karst; some drop to a cave) | M4, M5 | 3 | 3 |
| Peat stack and cutter's lean-to | M3 | 2 | 3 |
| Reedfowl nest island | M1, M2 | 2 | 3 |
| Broken punt with a cache | M2, M3 | 2 | 3 |
| Lanterntoad ring (6 to 10 toads round a cypress knee) | M3, M4 | 2 | 2 |

### 2.5 Hidden sub-biome: the Sunken Coven (RAVEN-PLANNED "hidden sub-biomes"; FULL-GAME name kept; layout GAP-COOKED)

**What it is.** The Coven's village, which sank whole when the sinkhole opened under it. A floating peat mat (a real *schwingmoor*) closed over it like a lid, trapping air pockets among the houses. Under the Deep Pool at (+1,300, −3,100), 6 to 20 m down. About 0.6 km of lanes and rooms. 10 POIs.

**Ways in:**
1. **Dive the Deep Pool.** Swim down through the gap in the peat mat (12 s of breath) to the Drowned Lane air pocket. The pool's top 2 m is bleedwater, so sealed waders, Mirehide 4 or a swamp mount make it clean.
2. **The Walking House cellar.** The Coven key from the Hanging Cypress opens a trapdoor and a dry well-shaft, a 30 m ladder down (M-S2).
3. **Bogsnout dig.** A tamed Bogsnout finds a collapsed chimney at (+1,380, −3,040) and digs it open over 20 s.
4. **The Weeping Gate** (from inside only). A one-way door into the Mother's Throat: the shortcut, Elden-Ring style.

**Rooms (in order from the Deep Pool):**

| Room | What's there |
|---|---|
| **The Drowned Lane** | A street of stilt houses standing in black water up to the knee, roofed by the peat mat 4 m up. Roots hang through. Lanterntoads light it green-gold. 3 Gorgeleech |
| **The Tithe Hall** | The Coven's hall. Long stone basins along the walls, each with a channel running down to one drain in the floor that leads toward the Throat. Forty bronze bowls stacked. Blood stains on every basin lip, high-water lines of dried blood. Lore item "Tithe bowl" |
| **The Bog-Body Gallery** | Twenty-two bog bodies seated in niches, perfectly kept by the acid peat, with braided cords at their necks and calm faces, women and men. Each holds a small bronze bowl. One niche is empty with a cord left in it |
| **The Birthing Pools** | Three warm pools where Gorgeleech young boil; Vharra's egg cases (leathery cocoons, real leech cocoons, 1 m long) on the walls. Breaking an egg case drops a Gorgeleech egg (steal-an-egg route, 2.12). 6 young, 2 adult Gorgeleech. Bleedwater |
| **The Witch's Library** | A dry loft with wax-sealed jars, a hex-ink stone, and shelves of bark books gone to pulp. One readable: a lore item "Recipe of the tithe" (it's the Blood-path recipe for the **Hex-ward charm**) |
| **The Old Gar's Pool** | A deep flooded chapel: KEPT legendary fish (survival-crafting): the Old Gar, a 3 m alligator gar. A fishing ledge |
| **The Ringing Room** | Bone chimes on every beam. Walking through rings them, and the chime wakes the Birthing Pools' adults (sneak check). Ore: iron ×2 nodes in a collapsed wall |
| **The Bell Pit** | A bronze Coven bell half-sunk in silt. Ringing it (Interact) once per game day calls every Gorgeleech in the Coven to you: a farming spot for leeches, at your own risk |
| **The Sealed Reliquary** | Opened only during the Red Tide (2.10), when the tide's pressure lifts the peat mat 1 m and a stone door unseals. Holds iron ×6, a ruby, upgrade shards ×2, and the lore item "Mother's first tooth" (a Vharra tooth from a younger moult) |
| **The Weeping Gate** | A stone door into the Throat's bottom terrace. It opens one way from this side |

**Light:** full dark except Lanterntoads. Bring a lantern, a torch (a torch can light marsh gas, 2.8) or a tamed Lanterntoad.

**Music:** the "Sunken Coven" cue (2.16).

### 2.6 Caves (CF rule: every biome has caves; deeper is better)

The swamp's caves are karst: limestone dissolved by acid bog water, opening as sinkholes.

| Cave | At | Depth | Ore | Creatures | What's special |
|---|---|---|---|---|---|
| **Reedmouth** | M1, +2,050, −2,900 | 10 m | Copper, tin | Lanterntoad 4, Slimeshell 3 | A flooded entry you wade; a dry back chamber. The newcomers' first cave |
| **Eel Hole** | M2, +1,700, −5,050 | 15 m | Copper | Gorgeleech 3, bog eels | Tidal: floods at high tide (each 12 game hours, 1 game hour flooded) |
| **Peat Cellar** | M3, +1,300, −4,350 | 20 m | Tin, iron (1) | Bogstalker 2, Gorgeleech 2 | Peat walls you can dig through (pick, 4 hits) to a hidden side gallery with a gem |
| **The Gullet** | M5, +450, −4,100 | 40 m | Iron ×4, a chance of a gem | Gorgeleech 4, Murkjaw 1 in a sump | Behind the third waterfall (M-S4). The best iron in the swamp |

### 2.7 Resources (GAP-COOKED; bloodmoss name KEPT)

| Resource | Where | Gather | Uses |
|---|---|---|---|
| **Bloodmoss** (signature) | Raised-bog domes (M3), hummocks (M3, M4), cypress knees (M4), the edges of every bleedwater pool. Never farmable (survival-crafting) | By hand, 2 per node (Bogsnout ×2). Respawns in 2 game days | Bloodmoss poultice (cures bleed). Bloodmoss-cured hide (sealed waders). Mirehide. Blood brews. Hex ink. Mudhulk taming food. Dadeni draught (KEPT). Bleed-resist brew (KEPT) |
| **Bog iron** | Red shallows (M1, M4), the Weeping Pools (M5) | Pick, 3 hits. Regrows in 4 game days (real: bogs regrow iron) | Smelts to iron at a third of cave ore: 3 bog iron = 1 iron ingot (bloomery or furnace) |
| **Peat** | Peat cuts and stacks (M3) | Spade or pick | Fuel: burns 2× as long as wood in furnaces and kilns; peat-smoked fish (preservation, KEPT system) |
| **Salt** | Salt Lick (M1), salt pans | By hand | Salt pouch (the Bloat counter); food preservation (KEPT); leech tar |
| **Leeches** | Gorgeleech drops (sac) and bleedwater edges (hand, while wading: 1 per 10 s) | Hand | **Leech tar** (bleedwater buildup ×0.3, world-map KEPT); **Thinblood oil** (weapon coating: +20 bleed buildup per hit for 60 s) |
| **Hexcrow feathers** | Hexcrow drops, the Rookery | Hand | Black fletching: arrows +10% speed |
| **Murkjaw hide, teeth** | Murkjaw | Butcher | Mirehide; Murkjaw-tooth arrows (bleed 25) |
| **Slime** | Slimeshell (farm) | Pen | Brew base (KEPT) |
| **Clay** | Clay pits, riverbanks (KEPT) | Spade | Bloomery, pots |
| **Bog rice, bloodroot** | Crops (KEPT, survival-crafting) | Farm | Food, Mire wine, brews |
| Cypress wood | M4 | Axe | Wood; rot-proof (posts sunk in water never decay) |

### 2.8 Hazards

**Bleedwater (SPEC 3.3 numbers; tells GAP-COOKED)**

| Item | Value |
|---|---|
| Effect | Wading or swimming: **10 bleed buildup a second** (KEPT). The proc is the combat lane's bleed burst (12% max HP + 15). Threshold ×1.3 after each proc, so a long wade procs at about 10 s, 23 s, then 40 s. It can kill |
| No slow | Bleedwater never slows you (lesson 3) |
| Tell | Oxblood-brown water with constant thin leech ripples. A metallic film catches light. A soft wet "suck" layer in the water's sound bed. At the edge, a 0.9 s ripple builds toward your foot before the first tick |
| Passes | Sealed waders (Legs slot); swamp mount (Mirestrider, Mudhulk); Mirehide 4 (KEPT). **Leech tar** (a smear on the legs, 10 min): buildup ×0.3 (world-map KEPT) |
| Where | All bleedwater pools (M4, M5), the Deep Pool's top 2 m, and the Throat's flood in phase 3. About 18% of the swamp's water. Murk water elsewhere is clean |
| Creatures | Every swamp creature ignores it (CR rule) |

**Micro-hazards (GAP-COOKED; each with ≥ 0.9 s of tell; each can kill)**

| Hazard | Where | Tell | Effect | Answer |
|---|---|---|---|---|
| **Quag** (sucking mud) | M1 mudflats, M4 pool margins | Glossy grey surface, slow fat bubbles, a sucking sound on the first step | Run ×0.5 while in it. Standing still 3 s sinks you to the knee: no roll (backstep and jump only) until you're out. Never a status | Stay on reeds and roots; Mirestrider (its splayed toes) walks it at full speed; a plank piece (freeform) bridges it |
| **Marsh gas** | Gas fields (M3, M4), the Sunken Coven | Bubble clusters; faint blue flicker at night; a rising "whoomp" 0.9 s before it lights | Any fire within 3 m (torch, burn status, Fire bolt, burning creature) lights the field: 4 m flash per vent, 40 damage + burn 40, chained vent to vent at 0.15 s | A **tool** as well: lure a pack in and light it. Fire path players treat it as a weapon |
| **Rotten planks** | Boardwalks (M3, M5) | Black planks with shelf fungus, sagging; a creak 0.9 s before break when weight lands | Breaks under a run, a roll, a heavy landing, or 2 players at once. Drops you 1 to 2 m into what's below (often bleedwater or a Murkjaw's duckweed) | Walk them; sound planks are grey. Repair with 2 wood (Interact) and they stay fixed for 4 game days |
| **Leech boil** (Red Tide only, 2.10) | Bleedwater pools | The pool's surface boils with leeches; a hum rises over 1.0 s as you enter | Inside the event: 15 bleed buildup a second plus 2 latch leeches (swarm archetype, 25 HP) per 10 s spent wading | Waders still pass the buildup; the latchers still come |

### 2.9 Weather (weights KEPT from world-map; effects reconciled)

world-map gives Clear 15, Overcast 20, Rain 25, Fog 25, **Miasma 15**. survival-crafting gave a "Fog 50%" state. **Reconciled:** the map's weights stand. Fog uses survival-crafting's effect, and Miasma is the signature.

| State | Weight | Effect |
|---|---|---|
| Clear | 15 | Low knee fog only at dawn and dusk |
| Overcast | 20 | Flat light; the falls' red reads strongest |
| Rain | 25 | Bleedwater pools spread 10% wider for the block. Plank creaks are louder (better tells). Crops count as watered |
| Fog | 25 | Vision 30 m. Murkjaw and Bogstalker ambush from 2× range (survival-crafting KEPT). Lanterntoads and lanterns aren't cut (light radius unchanged) |
| **Miasma** (signature) | 15 | Fog with an oxblood cast, vision 40 m (world-map KEPT). Murkjaws hunt in water within 2 m of boardwalks. **Hexcrow** curse cries carry 1.5× range. Corpse candles appear by day |

### 2.10 Event: the Red Tide (GAP-COOKED; frame KEPT from fire-frost 3.1)

**Folklore and fact.** Real red tides are algal blooms; a bog's iron bacteria can stain whole pools red. In the story, it's the old tithe coming back: the Mother calls for blood.

| Part | Detail |
|---|---|
| Frequency | One game day, about once every 3 real days (the server's Biome events setting, KEPT) |
| Warning (5 min) | The Weeping Wall's five falls run bright oxblood, the Throat's mist column doubles in height, every Hexcrow murder in the biome takes off at once (a sky full of crows), and the **Mire Coven pillar on the Crown's Ring burns red** (world-map 2.12 rule) |
| Water | Every murk pool in M3, M4 and M5 turns to bleedwater for the day. The leech boil hazard runs in all of it. M1 and M2 (brackish, salt) stay clean: salt kills leeches. The coast is the safe ground |
| Creatures | Gorgeleech spawn ×3. The Bloat leaves its route and walks the Drowned Causeway, gorging 2× faster. Murkjaws hunt by day |
| **Hexed beasts** (at night) | The Hex-Warden leaves her Ossuary and walks the boardwalks from the Thorn Gate to the Tithe Mound and back. Every Bogstalker and Murkjaw within 30 m of her becomes **hexed** for the night: eyes glow garnet, hits carry curse 25, aggro range ×1.3. These are the draft's "hexers": living beasts under her curse, never undead (DD no zombies). Kill her and the hex ends |
| The Sunken Coven | The Sealed Reliquary opens (2.5) |
| Pays | Bloodmoss ×3 everywhere (3.3 SPEC). Gorgeleech sacs drop double. Boss stone note: Vharra's arena water is bleedwater all fight long during the event, phase 1 included |
| Never | Spawn camps (M1) stay clean and safe |

### 2.11 Shrines and the path master

**Shrines (rules KEPT from fire-frost 3.1: 5 per biome, guarded, 3 s light, path XP, fast travel):**

| Shrine | At | Guard (sub-zone pack) | Look |
|---|---|---|---|
| **Great Shrine: the Tithe Mound** | M3, +1,640, −3,720 | none (the path master is near) | Low bone mound, stone tithe basin, a ring of 7 fetish posts; the brazier is a bronze bowl of bloodmoss oil burning low and dark red |
| Salt Cross | M1, +2,150, −3,250 | Bogsnouts and a Gorgeleech pair | A cross of salt-crusted driftwood on a reed island |
| Root Shrine | M2, +1,500, −4,950 (Mangrove Cathedral) | Murkjaw (midland-tier young) and Hexcrows | A root-altar in the channel crossroads |
| Cage Shrine | M4, +1,180, −2,950 (Hanging Cypress) | Bogstalker pair | A brazier inside an iron cage at the tree's foot |
| Falls Shrine | M5, +380, −3,820 (Weeping Pools) | Gorgeleech pack and Hexcrows | A bone brazier under the spray of the middle fall |

**Path master: Ruslana, the Mire-witch (GAP-COOKED; DD "a path master NPC per biome who teaches the path through quests"; FULL-GAME "a swamp witch").**

| Part | Detail |
|---|---|
| Who | A newcomer, not a Coven survivor: she washed ashore years before the player and taught herself the Coven's craft from its bones and books (world-map "the newcomers" rule). Early 30s, tall, striking: black hair in a long rope braid threaded with crow feathers and bone beads, sharp cheekbones, dark-lined grey eyes, lips and fingertips stained garnet with bloodmoss dye. She wears a fitted black-green leather bodice laced at the front over a loose undershirt open at the collar, bare shoulders with a crow-feather mantle, a split wrap skirt over tall waders, and a belt of bottles and bone charms. Confident, amused, dangerous. Her own Blood-path body change shows at 75 (2.18): garnet veins at the throat |
| Where | Lives in the Walking House. Works at the Tithe Mound by day (grinding bloodmoss, hanging charms); at the house by night (window lit) |
| How she talks | Short spoken lines within 6 m, no camera grab (KEPT rule). Quests go to the Journal |
| Teaches | Anyone. Her last quest needs the third mini (The Bloat) |

**Her 6 quests (GAP-COOKED; schema in `quests.json` as fire-frost):**

| # | Quest | Task | Reward |
|---|---|---|---|
| 1 | **Red moss** | Bring 10 bloodmoss and craft a Bloodmoss poultice | 1,000 Blood XP; poultice recipe; sealed waders recipe |
| 2 | **What the water wants** | Wade bleedwater until you proc bleed once, then cure it with the poultice | 1,500 Blood XP; leech tar recipe |
| 3 | **The duckweed rule** | Kill a Murkjaw on land (it has to die out of the water) | 2,000 Blood XP; Murkjaw-tooth arrow recipe |
| 4 | **A hex returned** | Curse 5 creatures, then kill 3 of them while cursed (any path's curse; a Hex-ward charm made from the Witch's Library scrap also counts) | 2,500 Blood XP; Hex-ward charm recipe |
| 5 | **The Coven key** | Fetch the key from the Hanging Cypress cage and open her cellar | 3,000 Blood XP; Sunken Coven route 2 opens (M-S2) |
| 6 | **Salt for the Bloat** | Beat The Bloat using salt at least once | 5,000 Blood XP; "Mire-witch's ring" (ring slot: lifesteal +2%, bleed buildup taken ×0.85) |

### 2.12 Creatures (names, roles and taming KEPT from FULL-GAME's list; everything else GAP-COOKED)

**Rules (CR SPEC):**
- Every swamp creature ignores bleedwater, quag and marsh gas.
- Neutral herds defend each other.
- Each creature tames only by its listed methods.
- Tiers follow 3.3: hard aggressive = highland, medium aggressive = midland, passives = lowland.
- Every attack shows the real jaw, claw, tongue or body, and damage lands only on visible contact.
- No attacker limit (combat 2.18.4).
- `physMaterial` for blood and spark FX (combat 2.14).

#### 2.12.1 The ten at a glance

| Creature | Tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Murkjaw** | Highland, 1 (2 at nests) | Saltwater croc body; false-gharial / *Sarcosuchus* snout with interlocking teeth that splay outside the closed jaw; osteoderm ridges that grow bloodmoss; red tapetum eyeshine | A low 7 m wedge with a toothed bar of a snout and a ridge of moss tufts | Black-olive #2A2E22, moss garnet on the ridge #6A1E1E, belly bone #CFC6AE, eyeshine red | 7 m long | Knockout and feed, or blood path magic. Hard | Raw meat ×6, Murkjaw hide ×3, teeth ×4 |
| **Gorgeleech** | Midland, 2 to 4 | Medicinal leech at dog size: Y-shaped three-jawed front sucker, ten eyes in a crescent, back sucker, inchworm loop on land, ribbon swim | A 1.4 m glistening loop that rears into a striped arch, mouth-disc forward | Olive-black #2B2F1E with paired orange-red stripes #B5482A, belly mottled yellow-grey | 1.4 m, 0.6 m when looped | Knockout and feed. Medium | Leech sac ×2 (blood), leeches ×3 |
| **Hexcrow** | Midland, murder of 3 to 6 | Raven body at 2.4 m wingspan; rook's bare bone-white face; a long crest of ragged hackles | A black cross on the sky with a white face; on the ground a hunched, hackled crow taller than a wolf | Glossy black with a garnet sheen in the light, face bone-white, eyes pale blue | 2.4 m span | Knockout and feed (bring it down). Medium | Feathers ×4, raw meat ×1 |
| **Bogstalker** | Midland, 1 to 2 | Argentine horned frog (*Ceratophrys*, an all-mouth ambusher) at bull size; Goliath frog legs; skin mottled like moss and mud | A low mound that's 60% mouth, two horned brows; rears into a squat with a pale throat | Moss green #4E5A32, mud brown #5A4632, black blotches, throat pale yellow #D8D09A | 2.2 m long, 1.4 m squat | Knockout and feed. Medium | Raw meat ×4, frog skin ×2 (waterproofing) |
| **Mirestrider** | Midland neutral, herd 3 to 5 | Shoebill head and bill on a flightless stilt bird 3.5 m tall; jacana's splayed toes; heron neck | Tall stilt legs, S-neck, a huge shoe-shaped bill | Slate blue-grey #6A7480, bill horn-yellow #B8A060, legs black | 3.5 m tall | Knockout and feed. Medium | Feathers ×6, raw meat ×4 |
| **Mudhulk** | Midland neutral, herd 2 to 4 | Hippo; "blood sweat" (real red skin secretion); short tusks | A barrel body, head like a box, a red sheen on wet skin | Mud grey-brown #5E5248, sweat sheen garnet-orange #9A3A22 | 3 m at the shoulder | Offer bloodmoss. Medium | Raw meat ×8, thick hide ×4, fat ×3 |
| **Bogsnout** | Lowland neutral, 2 to 3 | Babirusa (tusks curling up through the snout) on a truffle-hog body | A pig with two tusks curling back over its face | Grey-pink skin, sparse black bristle | 1.1 m long | Offer food. Easy | Raw meat ×3, hide ×1 |
| **Lanterntoad** | Lowland passive, ring of 6 to 10 | Cane toad bulk; a bioluminescent throat sac | A fist-sized-head toad, 40 cm, that lights up | Warty umber #5A4A36, sac glow gold-green #C8E070 | 40 cm | Offer insects. Easy | Toad gland ×1 (alchemy) |
| **Reedfowl** | Lowland passive, flock 4 to 8 | Purple swamphen at 1 m: red shield on the brow, long toes | A blue-purple hen with a red brow shield | Purple-blue #3E4A8A, shield red #B02A2A | 1 m | Offer food. Easy | Raw meat ×1, feathers ×2, eggs (farm) |
| **Slimeshell** | Lowland passive, 1 to 3 | Apple snail at 1 m; bright pink egg clutches laid on reeds (real) | A big coiled shell on a gliding foot | Shell banded brown #6A4A2A and cream, foot grey, eggs pink #E86AA0 | 1 m | Steal an egg (the pink clutches). Easy | Slime (farm), raw meat ×2 |

**Path magic (Murkjaw, blood path; GAP-COOKED in Raven's "calm a creature of the same element" frame).** On a Murkjaw that's knocked out or below 30% HP and bleeding, hold Interact for 4 s with 10+ Blood points: you bleed 5 HP a second into its mouth (into your Spilled segment, 2.17). That counts as feeding it a full meal of narcotic, and taming continues as knockout-and-feed.

**Utilities:**
- **Lanterntoad:** light radius 10 m; follows; sits on your shoulder pouch when you swim.
- **Bogsnout:** finds bog bodies, buried caches and the Coven chimney; bloodmoss gather ×2 within 15 m.
- **Mirestrider (mount):** full speed in quag, mud and water up to 1.4 m; passes bleedwater; ride sprint 1,300 cm/s (survival-crafting band).
- **Mudhulk (hauler):** carries 600; wades anything up to 2.5 m deep (it walks the bottom, real hippo behaviour); passes bleedwater; the only ground mount that crosses the Deep Pool without diving.

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55.

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Murkjaw** (high) | **Duckweed burst** (opener) | 0.95 (duckweed parts in a line toward you, bubbles, red eyeshine) / 0 / 0.30 / 1.2 | 1.6 | 55 (5) | From water within 700 cm; blockable; bleed 30 | Only from water. → Death roll 40% if it hits |
| | Snap | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable, bleed 20 | 50% → Snap 2 |
| | Snap 2 | 0.38 / 0.2 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable, bleed 20 | 30% → Tail sweep |
| | **Death roll** | 0.90 (jaws gape wide, crimson glint on the teeth) / 0.3 / 0.25 catch | 0.6 catch + 18/s for 2 s | 60 (6) | `grab`, `unblockable`; dodge only. In water within 3 m of the edge it drags you under: +2 s and no breath regen | Ally hit on the head 120 damage frees you. Mash: −0.15 s per press |
| | Tail sweep | 0.55 / 0 / 0.20 / 0.80 | 1.1 | 40 (4) | `low` (jump clears it), 280° behind and sides, r 450 | — |
| | Land lunge | 0.60 / 0.4 / 0.25 / 0.90 | 1.4 | 45 (4) | parryable, bleed 25, 300 to 800 cm | — |
| | readsAction | Land lunge with no hold within 0.4 s of a drink or a charged heavy | | | | |
| **Gorgeleech** (mid, pack) | Loop strike | 0.50 (body arches up into a loop, mouth-disc flares) / 0 / 0.20 / 0.80 | 1.0 | 20 (2) | parryable, bleed 30 | — |
| | **Latch** | 0.70 (rears, three jaws open in a Y, wet click) / 0.2 / 0.20 catch | 0.5 catch, then drain 6/s | 25 (2) | `grab` (blockable: a block knocks it off); bleed 15/s while on; heals itself the drain ×2 | On: you can still act at ×0.85 move speed. Off by: a roll (60%), fire on you or it, a salt pouch, any ally hit, or its death. Max 3 latched on one player (visible: one per limb) |
| | Spring lunge (land) | 0.60 (tail-sucker plants, body coils) / 0 / 0.25 / 0.90 | 1.2 | 30 (3) | parryable; 300 to 650 cm | Pack flankers use it from the sides |
| | Swim lunge | 0.70 (V-wake toward you) / 0 / 0.25 / 0.90 | 1.2 | 30 (3) | from water within 600 cm; → Latch 60% | — |
| **Hexcrow** (mid, flyer) | **Curse cry** | 1.00 (lands or hovers, head back, throat hackles flare, a rising caw) / 0 / 0.6 (cone) | 0 | — | Cone 10 m × 40° from the beak (15 m in Miasma); curse 35 per crow; not a projectile; line of sight; blocked by solid cover | Several crows stack. A crow hit during the windup cancels it (stance) |
| | Dive rake | 0.80 (wings fold, screech) / 0.2 / 0.30 / 1.5 grounded | 1.3 | 30 (3) | parryable; bleed 20 | Grounded 1.5 s = bring-down window (combat template) |
| | Peck flurry (ground) | 0.45 / 0 / 0.6 (3 pecks) / 0.80 | 0.5 each | 10 each (1) | parryable | Prefers latched or downed targets |
| | Call the murder | 0.80 / 0 / — / 1.0 | — | — | Calls Hexcrows within 60 m | Once per fight |
| **Bogstalker** (mid) | **Tongue lash** | 0.90 (throat pumps twice, the pale tongue-tip shows between the lips with a white glint) / 0 / 0.20 | 0.5 + pull 600 cm | 30 (3) | `grab` but blockable at full guard (pull halved); roll sideways to dodge; range 400 to 1,000 cm | If pulled to within 2 m → Gulp |
| | **Gulp** | 0.30 (mouth opens 1.4 m) / 0 / 0.20 catch | 15/s for 2.5 s inside | 50 (5) | `grab`, `unblockable`; only after a successful tongue pull | Ally hits on the throat 100 damage spit you out. Mash −0.15 s per press. Spat out = knocked down |
| | Belly-flop leap | 0.85 (squats deep, legs tremble) / 0.3 / 0.25 / 1.20 | 1.5 | 50 (5) | lands on a target point up to 800 cm; impact ring 350 cm `low` (jump clears it) | — |
| | Bite | 0.45 / 0 / 0.15 / 0.80 | 1.0 | 30 (3) | parryable | — |
| | Mud spit | 0.80 (cheeks bulge) / 0 / — / 0.70 | 0.5 | 10 (1) | projectile 900 cm/s, gravity, never homes, min range 450; hit = slowed 3 s (mud) | — |
| **Mirestrider** (mid, neutral) | Bill strike | 0.55 (neck draws back into an S) / 0.2 / 0.15 / 0.80 | 1.2 | 35 (3) | parryable; pierce | — |
| | Clatter (rally) | 0.80 (bill-clatter like gunfire) / 0 / — / 0.5 | — | — | The herd turns hostile together within 30 m | Plays the moment one is hit |
| | Wing buffet | 0.45 / 0 / 0.20 / 0.70 | 0.7 | 25 (2) | push 300 cm | Close |
| | Stilt kick | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 30 (3) | `behindOnly` | — |
| **Mudhulk** (mid, neutral) | **Gape and chomp** | 1.00 (the hippo yawn: jaws open 150°, tusks bared, a low grunt) / 0.3 / 0.20 / 1.10 | 1.8 | 55 (5) | parryable; strike + pierce | The yawn is also its warning display before aggro |
| | Charge | 0.90 (head down, a snort spray) / 0 / 1.0 run / 1.30 | 1.6 | 50 (5) | knockback 800; wall or trunk hit stuns it 2.5 s | — |
| | Wallow surge | 0.80 (sinks, a bulge rises) / 0 / 0.30 / 1.0 | 1.3 | 45 (4) | from water: a body-rise that overturns you within 300 cm; `low` is false (rolling out works) | Only in water |
| **Bogsnout** (low, neutral) | Tusk gore | 0.50 / 0 / 0.15 / 0.80 | 1.0 | 20 (2) | parryable | — |
| | Squeal charge | 0.70 (squeal rises) / 0 / 0.6 run / 1.0 | 1.2 | 25 (3) | — | — |
| **Lanterntoad** (low, passive) | Flash puff | 0.60 (sac swells and brightens) / 0 / 0.20 / flees | 0 | — | 3 m flash: 1.5 s vignette dim (blind proper from R5) | Then hops away |
| **Reedfowl** | Peck (cornered) | 0.45 / 0 / 0.12 / 1.0 | 0.3 | 5 (1) | — | Flees |
| **Slimeshell** | none | — | — | — | Withdraws into the shell: damage ×0.3 for 6 s | Never attacks |
| **Latch leech** (Red Tide swarm) | Latch | 0.45 / 0 / 0.15 catch | drain 2/s | — | bleed 8/s while on; 25 HP | Swarm archetype (combat 2.18.3) |

**Physical resist:** Murkjaw osteoderms slash 0.85 / pierce 1.1 (the belly, which shows when it rolls or rears, takes ×1.3). Gorgeleech: strike 0.8 / slash 1.2 (it's soft). Mudhulk: slash 0.9. **Lock points:** Murkjaw head and belly; Mudhulk head and chest; Bogstalker throat.

#### 2.12.3 Day and night tables (weights per sub-zone; `spawn_regions.json` with the `night` table)

| Sub-zone | Day | Night |
|---|---|---|
| M1 Reed Flats | Reedfowl 16, Slimeshell 10, Bogsnout 10, Mudhulk 6 (Salt Lick), Gorgeleech 3 | Lanterntoad 14, Gorgeleech 8, Bogsnout 6, Murkjaw 2 (midland-tier young, 120 HP, only at channel mouths) |
| M2 Mangroves | Mirestrider 8, Reedfowl 8, Murkjaw 4, Hexcrow 4 | Murkjaw 8, Gorgeleech 8, Lanterntoad 6 |
| M3 Boardwalks | Mirestrider 10, Mudhulk 8, Hexcrow 8, Bogstalker 4 | Bogstalker 10, Gorgeleech 10, Lanterntoad 8, Hexcrow 3 (roosting; wake on noise) |
| M4 Murk Pools | Bogstalker 8, Gorgeleech 8, Slimeshell 6, Murkjaw 6 | Gorgeleech 14, Murkjaw 10, Bogstalker 6, Lanterntoad 8 |
| M5 Bone Mounds | Hexcrow 12, Murkjaw 6, Gorgeleech 6 | Murkjaw 10, Gorgeleech 10, Hexcrow 4 |

**Living-world beats (built AI, WRWildlife):**
- Gorgeleech packs latch onto Mudhulks and drain them; the herd charges in to defend (herd rule). You find a drained husk later (kill set-piece).
- Hexcrow murders circle any creature or player under 40% HP and land on carcasses: a vertical sign of a fight or a kill, seen from 400 m.
- Murkjaws take Reedfowl and Mirestriders at the water's edge at dawn.
- Mirestriders stand motionless for minutes and stab fish (idle loop).
- Mudhulks crowd the Salt Lick at dawn and wallow in pools by day.

### 2.13 Mini-bosses (premises from the FULL-GAME draft kept; fights GAP-COOKED)

Rules (KEPT, combat 2.17): 3,000 solo HP, +80% per extra player or tame; stance 90; 2 parries; 4 to 5 attacks plus one counter mechanic; chase 65 m; respawn 4 h; minis drop upgrade shards ×4 and a trophy (BO); trophies are talismans (survival-crafting KEPT: Maw tooth, Hex mask, Bloat sac).

#### 2.13.1 Mother Maw (Murkjaw matriarch; folklore: Jenny Greenteeth)

| Part | Detail |
|---|---|
| Body | A Murkjaw grown to 13 m, broad as a punt, with a snout scarred white. Her back carries a living mat of duckweed and bloodmoss, so she looks like a floating island until she moves. Old broken arrows and a Coven bone hook are in her neck plates |
| Silhouette | A huge low wedge; when she rears, the bar of interlocking teeth is taller than you |
| Arena | Mother Maw's Nest (+1,500, −2,700): a duckweed pool 60 m across around a compost nest mound, ringed by a broken boardwalk square (four sides, two gaps). The water is 1.5 to 3 m deep |
| Voice | Saltwater-croc infrasound bellow (the water dances on her back as she calls), alligator hiss, a bittern's boom, wet cracking of reeds |
| Lock points | Head, belly (only while rearing or rolling) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Duckweed burst | 1.0 (a line of duckweed parting toward you across the pool) / 0 / 0.30 / 1.2 | 55 + bleed 40 | From water only. Blockable |
| Snap, snap, head-slam | 0.55 / 0.45 / 0.80 (H 0.3 on the slam) / P 1.0 | 40 / 40 / 60 | The slam breaks the boardwalk section it lands on (arena change: fewer dry planks) |
| **Death roll** | 1.0 (crimson glint on the teeth) / 0.3 / catch | 25 + 20/s for 2.5 s, dragged under | `grab`, unblockable, dodge only |
| Tail club | 0.60 / 0 / 0.20 / 0.90 | 45 | `low`, 300° r 600. From water it sweeps the boardwalk edge |
| Nest roar | 0.9 / 0 / — / 1.2 | — | Calls 2 Murkjaw young (midland-tier, 120 HP) from the nest, once at 60% and once at 30%. A tame-punish echo: the young go for the furthest tame first |

**Counter mechanic: the boardwalk.** She can't dive under planks. On a boardwalk section she has to rear up out of the water to reach you (her belly lock point shows, ×1.3 damage, and every rear has a 1.2 s punish). In open water she gets the duckweed burst and death roll freely. Her slams break sections, so the dry ground shrinks: protect it, or repair a section with 2 wood mid-fight (3 s, Interact). Fire on the nest mound makes her leave the water to defend it for 8 s.

**Drops:** trophy Maw tooth (KEPT: hits on swimming targets +15%), upgrade shards ×4, Murkjaw hide ×6, "Maw's hook" (lore: a Coven bone hook, crafting extra: +10% bleed buildup on spears).

#### 2.13.2 The Hex-Warden (the last Coven witch; folklore: bolotnik call-mimicry, the Morrígan's crows)

| Part | Detail |
|---|---|
| Who | A woman, ageless and kept young by blood. Tall and lean, moving like a dancer. A crow-skull mask over the upper face, a long cloak of black crow feathers over a fitted bone-and-leather corset, bare arms ringed with garnet vein-marks, a bone sickle in each hand. When her mask breaks at a stance break, she's beautiful under it, with garnet eyes, and she laughs |
| Silhouette | Tall, the feathered cloak flaring into wing shapes; the white mask is the brightest point in the dark |
| Arena | Hex-Warden's Ossuary (+700, −4,300): a bone mound with **four fetish totems** on its shoulders, 25 m apart |
| Voice | A female voice (wordless chant, breath, laugh), raven croaks woven into it, rattling bone chimes, crow-wing flurries |
| Lock points | Body, mask |

**The totems (counter).** Each standing totem gives her one curse (all four on at the start):

| Totem | While it stands |
|---|---|
| Crow totem | She can **scatter**: burst into 40 crows (i-frames) and reform at any standing totem 1.2 s later |
| Bone totem | Her sickle hits carry curse 30 |
| Blood totem | She heals 2% max HP whenever anyone near her bleeds or procs bleed |
| Voice totem | **Mimic call**: she plays a tame's own cry and pulls up to 2 tames off you for 6 s (Raven's tame-punishing idea on a mini) |

A totem has 400 HP and takes ×2 from blunt and fire. Breaking one strips its curse for good and staggers her 2 s if she's within 15 m. With all four down she can't scatter: every 5th hit then staggers her.

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Sickle dance | 0.40 / 0.30 / 0.45 (3rd H 0.3) / P 0.9 | 25 / 25 / 35 + curse 30 each (bone totem) | Parryable, all three. Ends facing away (the punish) |
| Hex bolt | 0.95 (the mask's eyes glow, a black-purple sigil gathers in her palm) / 0 / — / 0.8 | 20 + curse 60 | Projectile 1,000 cm/s, never homes, min range 500 |
| Crow scatter | 0.5 (cloak flares) / — / — | — | i-frames; reform at a standing totem 1.2 s later with a **reform strike** 0.6 / 0 / 0.2 / 0.9, 40, parryable |
| **Mimic call** | 1.0 (she cups her mouth, your tame's call plays from her) / — / — | — | Voice totem only. Pulls up to 2 tames to her side for 6 s: they don't attack you, they just leave |
| **Blood harvest** (grab) | 0.9 (crimson glint on both sickles) / 0.2 / catch | 60, heals her 300 | `grab`, unblockable, dodge only |

**Drops:** trophy Hex mask (KEPT: curse buildup ×0.7), upgrade shards ×4, feathers ×10, lore item "Coven tithe-list" (names scratched off one by one, the last one hers).

#### 2.13.3 The Bloat (roaming; folklore and fact: salt on a leech)

| Part | Detail |
|---|---|
| Body | A Gorgeleech swollen to 6 m long and 3 m high, skin stretched translucent so the stolen blood shows inside, dark at rest, bright garnet when gorged. Ten eyes. Stripes stretched wide apart. It moves by slow loops on land and drifts heavily in water |
| **Gorge meter (visible, no UI)** | Its body swells and brightens from 0 to 100%. It gains gorge by draining (latch on a player or tame: +8% a second; on a creature: +4% a second) and by wading bleedwater (+1% a second) |
| Route | 2.2 km loop through M4 and M3: the Wallow (lair) → leech pond → Mother Maw's pool edge (they fight if they meet) → Hanging Cypress → Bloodmoss Dome → back. Walks 1.5 m/s (about 25 min a loop) and rests 4 min at the Wallow. Nocturnal: sleeps at the Wallow by day (a sneak crit opens the fight) |
| Living world | It latches onto Mudhulks on its route and drains them: a herd fight you can watch |
| Voice | Leech-suck squelch, elephant-seal gurgle (deep, wet), a walrus groan, a stretched-skin creak as it fills |
| Lock points | Body, the three brightest veins (part HP 300 each: rupturing one sprays bleed 40 around it and drops gorge 20%) |

| Attack | W / H / A / P | Damage | Notes |
|---|---|---|---|
| Body slam | 0.80 (rears) / 0.3 / 0.25 / 1.0 | 50 | Parryable (it's soft); 400 cm line |
| Latch | 0.9 (front disc opens, a Y of three jaws) / 0.2 / catch | 10/s drain + bleed 20/s | `grab`, blockable (knocked off); roll 60% off; it gains gorge |
| Roll-crush | 0.90 (curls) / 0 / 1.2 roll / 1.3 | 55 | 8 m line; `low` is false; jump doesn't help, roll sideways |
| Spray | 0.80 (a vein pulses at the near end) / 0 / 0.6 | bleed 50 | 6 m cone from the brightest vein |
| **Burst** (at 100% gorge) | 1.4 (the body glows to near white-garnet, a high stretched-skin whine; crimson cue) / 0 / 0.30 | 70 + bleed 80 | `unblockable`; an 8 m ring; leaves an 8 m bleedwater puddle for 20 s. Then it collapses to 0% gorge and is **staggered 6 s** (the big punish) |

**Counter mechanic: salt and fire.**
- **Salt pouch** (2.19) thrown onto it, or a hit with a salted weapon: it **vomits** 40% of its gorge in a 4 m spray in front of it (bleed 40 there), and is staggered 3 s. Real leech behaviour.
- **Fire** (burn proc, a torch hit or lit marsh gas under it): gorge −20% per proc, and a burning Bloat can't latch.
- **Frost** (frostbite proc): gorge gain stops for 12 s.
- **Played well:** keep its gorge low with salt and fire and it never bursts. **Played badly:** it bursts, and that's survivable only by staying out of 8 m when it glows.

**Drops:** trophy Bloat sac (KEPT: bleed buildup ×0.7), upgrade shards ×4, leech sac ×6, "Bloat's blood" ×1 (crafting extra: Leech strike heals +10%).

### 2.14 Major boss: Vharra, Queen of the Mire (GAP-COOKED; FULL-GAME premise kept: leech-drake, flooded sinkhole, drain bite, tail lash, blood spray; phase 2 floods and swims; hidden phase 3 water to blood; weak to Lightning; Mire heart)

**Look (built to be terrifying and believable).**

| Part | Detail |
|---|---|
| Anatomy | 28 m long. **Front:** a drake: powerful webbed forelimbs with hooked claws, a long muscular neck, a heavy hood-head shaped like a drake's skull. **The head opens in three:** the "skull" is three plates that split along Y seams into a radial three-jawed mouth lined with rows of teeth (leech jaws). Closed she looks like a dragon; open she's a nightmare. **Back:** no hind legs. The body becomes a ringed, segmented leech trunk tapering 18 m to a **posterior sucker disc** 3 m across that anchors her to the sinkhole wall (real leech anatomy) |
| Eyes | Ten small eyes in five pairs along the hood's ridge, dull red |
| Skin | Glossy black-olive with paired orange-red longitudinal stripes (the medicinal leech pattern). Wet, faintly translucent at the flanks |
| **Blood sacs** | Three crop chambers along her flanks show through the skin, filling visibly garnet as she drains. A read of her lifesteal, and lock points |
| Silhouette | A cobra-like rearing drake front on a tapering ringed tail, anchored by a disc. Unmistakable from 100 m |
| Scale | Head 3 m; reared, her head stands 11 m above the water |
| Voice | Leopard seal trill (eerie, alien) for her call; bull-alligator bellow for her roar; lamprey/leech wet suction amplified for her drain; whale groan pitched down under it all. Her jaws opening is a three-part wet crack you learn to fear |

**The arena: the Mother's Throat** (+900, −3,600).
- A flooded karst sinkhole 110 m across.
- **Terraces:** three concentric Coven terraces of bone-and-peat step down to a central pool 40 m across: upper ring dry, middle ring 20 cm of water, lower ring 60 cm.
- **Cover and high ground:** four ossuary mounds rise 3 m on the middle ring. **Six Rodwarden copper rods** stand on stone plinths between the terraces (1.5 m plinths: dry high ground).
- **Her anchor:** her disc anchors to the west wall under the cliff, so she always faces east. The fight is a half-circle toward her, which keeps the camera readable.
- **Run-back stone:** 50 m out from the Throat Stair (east).
- **Mood:** red mist column overhead, cages hanging from the ring of cypress.

**The lightning counter (FULL-GAME "shock the water to stun her", GAP-COOKED as a visible system):**
- **Shocking the water.** Any lightning damage that lands on water she's touching (a Lightning spell into the pool, a Lightning Wildbrand hit on her while she's in water) or on one of the six **copper rods** (60+ lightning damage in 2 s) electrifies the connected water for 1.5 s. Shown by blue-white arcs racing across the surface from the rod.
- **The effect on her.** If her body is in that water, she **convulses for 4 s**: an electric-eel-style thrash on the surface, ×2 stance damage taken, ×1.5 lightning damage (weakness SPEC 1.5×).
- **No Lightning path? Bring stormglass.** A thrown **Stormglass jar** (2.19; Storm peaks resource) shocks the water where it lands, as a rod would. That keeps Raven's "bring the opposite" push without making Lightning the only way in.
- **The cost.** Players standing in the shocked water take shock buildup 60 (Grounded immune, SPEC).
- **Cooldown.** Per rod, 20 s. A shocked rod glows blue for the cooldown.

**Numbers (KEPT rules):** 12,000 solo HP, +80% per extra player or tame. **Stance 160.** 3 parries to break. Phase 2 at 50%, hidden phase 3 at 25% with artifacts (damage ×1.4). Weak to Lightning 1.5×. Status: bleed buildup on her ×0.5 (she's blood: KEPT boss resist plus this). Curse works normally.

**Lock points:** head, the three blood sacs (part HP 1,200 each; rupturing one spills a 10 m bleedwater slick for 30 s and cuts her drain heal by a third), and the anchor disc (only shows in phase 2 when she lets go).

**Phase 1: the anchored queen (6 moves: a 3-hit combo and a grab; template KEPT)**

| Move | W / H / A / P | Damage | Flags, reach | Tell (small cue, combat 2.17.1b) |
|---|---|---|---|---|
| **Hood snap** | 0.60 / 0.3 / 0.20 / 0.80 | 45 + bleed 30 | parryable; 9 m reach (her neck) | The closed hood draws back; white glint on the hood's edge |
| **Three-jaw drain** (grab) | 1.10 / 0 / 0.30 catch | Lifts you; 18/s for 3 s; she heals 3× the damage and her sacs fill | `grab`, `unblockable`; a 12 m line lunge | The head splits open into three with a wet crack; crimson glint in the throat |
| **Tail lash ×3** (combo) | Sweep 0.70 (`low`) / backhand 0.50 / overhead 0.80 + H 0.4; P 1.2 | 40 / 40 / 65 | First jump, second roll, third sidestep; 14 m reach from the water | The ringed trunk lifts from the pool, water streaming off |
| **Blood spray** | 0.90 / 0 / 1.2 (sweeps 60°) / 0.9 | 15 per tick + bleed 60 | 12 m × 50° cone; leaves bleedwater puddles 6 s on the terraces | Sacs pulse in sequence, a rising gurgle |
| **Clot spit** | 0.90 / 0 / travel / 0.7 | 30 + bleed 40 each | 3 lobbed clots at players over 12 m away, 1,000 cm/s ballistic, never homing; each lands with a 2 m slow puddle | Cheeks of the hood bulge; a shadow grows at each landing point for the whole flight |
| **Anchor heave** | 1.20 / 0 / 0.30 / 1.4 | 70 + a ring wave 30 | Her disc pulls taut (a creak), the whole forebody slams across a terrace: a 12 m line, then a ring wave at 1,200 cm/s along the water (`low`, jump it) | The disc's creak, then the body coils |

**Tame-punish move (KEPT rule, one per major): Brood shed.** 0.9 s (sacs contract and split at their seams). She sheds 4 Gorgeleech young (swarm, 25 HP) into the pool. **They go for the furthest tame first** and latch on (drain 4/s). Once per 45 s from 75% HP.

**Transition at 50%: the Sinking** (KEPT rule: an unblockable telegraphed attack, 1.4 s, the boss takes 50% damage during it).
- **The tell.** She rears to full height, all three sacs flare, and a crimson glint runs along her teeth.
- **The attack.** She dives and the sinkhole floods. A surge wave 1.4 m high rolls outward at 1,200 cm/s: 60 damage plus a knockdown. Jump it, or stand on a plinth or mound.
- **The arena change.** The water rises 1.2 m over 6 s. Every terrace except the upper ring floods to waist depth (wading ×0.75 run speed; no swimming needed). The plinths, mounds and upper ring stay dry. She lets go of the wall: the disc is now a lock point.

**Phase 2: the swimming queen**

| Move | W / H / A / P | Damage | Flags | Tell |
|---|---|---|---|---|
| **Ambush rise** (new, delayed) | 1.00 / H up to 0.5 / 0.30 / 1.2 | 80, launch 4 m | Rises under a player in water; a 2.5 m circle | A wake circles, then a ring of bubbles forms on the water at the target; final flash 0.2 s before contact |
| **Drowning coil** (new, delayed) | 1.20 / H up to 0.6 / 0.40 / 1.4 | 75 | `unblockable`; wraps a plinth or mound and crushes everyone on it (5 m ring). It forces you off the high ground into the water, where the lightning risk is | Her trunk circles the mound underwater (the wake shows), then rises around it; crimson glint at the squeeze |
| **Snap, snap, drag** (new combo) | 0.5 / 0.45 / 0.8 catch | 40 / 40 / grab: dragged under 2 s, 25/s | The drag is `grab`, unblockable | Third bite: crimson glint |
| **Blood spray (delayed variant)** | 0.90 / H 0.5 / 1.4 (sweeps 120°) / 0.9 | as phase 1 | From the water | As before, plus a held breath (a silent beat) before the sweep |
| Keeps | Hood snap, Three-jaw drain, Clot spit, Brood shed | | | |

**Lightning in phase 2.** She's in the water almost all the time now, so every rod shock that catches her convulses her. The trade is that the players are in the water too: Grounded players, plinth-standers and mounted players on Mirestriders (the bird stands tall) are the shockers. That's the "bring Lightning" lesson, made physical.

**Hidden phase 3 (25%, artifacts only; KEPT frame): the Water Turns.**
- **The water.** The whole flood turns to blood: bleedwater at 15 buildup a second. She heals 1% max HP a second while submerged.
- **Lightning burns it.** A rod shock stops the heal for 10 s and clots the water around that rod into a dark, walkable crust for 10 s (new dry ground).
- **Every move speeds up.** Damage ×1.4 (KEPT). Snap, snap, drag becomes snap, snap, snap, drag.

**Drops (KEPT BO rule plus survival-crafting's major trophy rule):**
- Boss core `core_vharra`.
- Blood essence ×5.
- Chance at the boss part **Mire heart** (FULL-GAME: kills on bleeding enemies heal you; GAP-COOKED number: 8% max HP per kill, 2 s cooldown).
- Trophy head (monument).
- 2 blueprints: **Three-Jaw Glaive** (a polearm whose head is three radial blades; its moveset goes to the combat lane's boss-weapon cook) and the **Coven decor pack**.

**Music:** MIR_Boss_Vharra (2.16).

### 2.15 Creature voices (GAP-COOKED; layering rules KEPT from fire-frost 3.5)

Every species: tell, strike, hit, pain, plus idle, aggro and death. Each built from at least 3 real animal calls and one material layer. Tells rise in pitch, 3 dB over the bed.

| Creature | Fused calls | Material layer | Signature moment |
|---|---|---|---|
| Murkjaw | Saltwater-croc infrasound bellow, American alligator hiss, bullfrog drone pitched down | Water slap, duckweed tearing | A low bellow that makes the water's surface shiver before the burst |
| Gorgeleech | Lamprey suction (wet clicks), leopard slug slime, an elephant-seal gurgle pitched up | Wet suck and pop | The three-jaw "Y-click" before a latch |
| Hexcrow | Common raven croak and knock calls, rook caw, a human whisper reversed under the curse cry | Feather rustle | The rising curse caw, several crows in a ragged chorus |
| Bogstalker | Argentine horned frog scream, pig frog grunt, bullfrog | Throat-sac pump, mud squelch | Two throat pumps, then a wet tongue-crack |
| Mirestrider | Shoebill bill-clatter (machine-gun), great blue heron squawk, shoebill moo | Bill knock | The clatter that turns a whole herd |
| Mudhulk | Hippo wheeze-honk laugh, elephant rumble, walrus grunt | Water churn | The yawn groan before a charge |
| Bogsnout | Babirusa grunt and teeth-chatter, warthog squeal, peccary bark | Mud snuffle | Rising squeal |
| Lanterntoad | Cane-toad trill, common-toad chirp | Soft glass hum as the sac glows | The ring's trill swelling in turn around a knee |
| Reedfowl | Purple swamphen honk, moorhen kek, coot | Reed rustle | Alarm honk |
| Slimeshell | (no voice) slow wet glide, shell knock | Slime stretch | Shell withdraw clack |
| Mother Maw | Saltwater-croc bellow (deeper), Eurasian bittern boom, alligator hiss | Reed crack | The water dancing on her back as she bellows |
| Hex-Warden | Wordless female chant and breath, raven croak, laughter | Bone chimes, crow-wing burst | Your own tame's call coming out of her mouth |
| The Bloat | Elephant-seal gurgle, walrus groan, leech suck amplified | Stretched-skin creak | The rising whine as it nears a burst |
| Vharra | Leopard-seal trill, bull-alligator bellow, whale groan pitched down | Wet three-part jaw crack, chain-like disc creak | The trill echoing round the sinkhole before she rises |

### 2.16 Music: the Swamp (tempo, key, instruments SPEC 3.3: 64 BPM, C minor, cello, bowed metal, heartbeat kick; cues GAP-COOKED; rules KEPT)

**Palette:**
- Solo cello and a low cello section.
- **Bowed metal:** a waterphone and a bowed saw (the musical saw's human-voice wail).
- A hurdy-gurdy drone.
- A **heartbeat kick** at 64 BPM (a resting heart).
- A low female voice in the open-throated Eastern European folk style (Slavic folklore fits the Coven), wordless.
- Frame drum for combat.

**Always audible**, with silences only where they're written into the piece. Ducks at most 4 dB. Mixed −18 LUFS, calm −23, boss −15.

| Cue | Length | Use | Feel |
|---|---|---|---|
| MIR_Explore_A / B / C | 3 to 4 min | Day. A: M1, M2. B: M3, M4. C: M5 | A: cello melody over the heartbeat, reeds-and-wind texture. B: bowed saw and hurdy-gurdy drone, heartbeat slower in the mix. C: falls-roar texture with the low voice, waterphone swells |
| MIR_Night_A / B | 3 min | Night | Voice alone over a cello drone; the heartbeat comes forward; Lanterntoad trill woven in |
| MIR_Combat (stems for A, B, C) | loops | Combat layer on the bar | Frame drum doubles the heartbeat, cello ostinato, metal scrapes |
| MIR_Mini | 3 min loop | Mother Maw, Hex-Warden, The Bloat | 4-bar intro per mini: sub-bass booms (Maw); chant and bone chimes (Warden); a slow rising saw glissando (Bloat) |
| MIR_Boss_Vharra P1 / P2 / P3 | sections | Phase changes on the bar | P1: cello march over the heartbeat, waterphone stabs. P2: the heartbeat doubles to 128 felt, choir of low voices, the bowed saw takes the melody. P3: everything in, a heartbeat that stutters |
| MIR_SunkenCoven | 4 min | The Sunken Coven | Drips, muffled heartbeat (as if underwater), the voice far off, bone-chime percussion |
| MIR_RedTide | layer | Event | Sub-bass pulse and a second, faster heartbeat over whatever plays |
| MIR_Camp | 2 min | Camps and your Hearth in the swamp | Warm cello and soft frame drum; the heartbeat at rest |

**Source:** first pass from CC0 and CC-BY libraries matched to tempo, key and instruments (OpenGameArt, Free Music Archive, Pixabay Music, incompetech). Boss tracks commissioned later. Credited in `Docs/CREDITS.md`. No AI generation (KEPT, fire-frost 3.6).

### 2.17 The Blood & curse path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Blood (DD SPEC: "spends your own HP; lifesteal earns it back"), built as the Spilled bar (GAP-COOKED).**
- **Costs are HP.** Every Blood ability costs HP, listed per node.
- **The Spilled segment.** HP spent on Blood abilities shows as a dark garnet **Spilled** segment on the end of your HP bar. It's the path's resource bar (4.1 rule: each path has its own bar).
- **Rally window.** For **8 s** after the last spend, your lifesteal (from any source) refills Spilled HP at **×2**. Once Spilled is full again, lifesteal heals at ×1. When the window ends, any unrefilled Spilled HP is simply lost.
- **Damage taken never adds to Spilled** (lesson 6).
- **No floor.** You can't cast with HP ≤ the cost (the slot greys and shakes). A cast can leave you at 1 HP, and the next hit kills you.
- **Base lifesteal is 0** for everyone; it comes only from Blood nodes, gear and boss parts.
- **One lifesteal pipeline.** All lifesteal sources add together, then go through Spilled first.

**Root passive: Bloodthirst (SPEC text: lifesteal increases as your HP drops; numbers GAP-COOKED).** Main path only. Lifesteal on all damage you deal: 0% at ≥ 90% HP, rising in a straight line to 15% at ≤ 20% HP.

**The tree.** Gates are points spent in the branch before the node, as `fire.json` does (0, 3, 8, 13, 16, 20 pattern).

| Branch | Node | Cost / ranks | Gate | Text (SPEC) | Numbers | Source |
|---|---|---|---|---|---|---|
| Trunk | Blood bolt | 3 pts | 0 | Basic blood spell, paid with HP | Bolt 22 dmg, bleed 30, costs 5 HP, cd 1.0 s, 2,600 cm/s; homes only on creatures (KEPT fire-frost RAISE 3) | KEPT combat |
| Trunk | Blood reserve | 3 ranks | 3 | More max HP | +8 max HP per rank | GAP-COOKED |
| Trunk | Bleed chance | 2 ranks | 6 | Higher chance to cause bleed | +10% bleed buildup on all your hits, and +6 bleed buildup on every weapon hit, per rank (so any weapon can bleed) | GAP-COOKED |
| Trunk | Crimson slash | 2 pts | 8 | Basic blood weapon art | Wildbrand: 6 HP, cd 2.5, W/A/R 0.28/0.20/0.42, ×1.4, bleed 45, stance ×1.5 | KEPT combat |
| Hemomancy | Blood spear | 5 pts | 0 | Spell: a piercing spear of blood | Lance 34, bleed 50, pierces 3 targets, 10 HP, cd 6 | KEPT combat (pierce count GAP-COOKED) |
| Hemomancy | Sanguine power | 5 ranks | 5 | More blood spell damage | +6% Blood spell damage per rank | GAP-COOKED |
| Hemomancy | Blood boil | 4 pts | 10 | Spell: bleeds on enemies burst for damage | Nova radius 600: every enemy with bleed buildup ≥ 30% of its threshold, or bleeding in the last 4 s, takes 20% of its bleed burst again; cd 12; costs 8 HP | KEPT combat (HP cost GAP-COOKED) |
| Hemomancy | Clotting | 3 pts | 14 | HP spent on spells regrows faster | Spilled HP regrows by itself at 2 HP/s, and the rally window lasts 12 s instead of 8 | GAP-COOKED |
| Hemomancy | Transfusion | 3 pts | 17 | Spell: heal an ally with your own HP | A garnet stream to an ally within 12 m: costs 20 HP (into your Spilled), the ally gets 30 HP over 1.5 s; cd 10 | KEPT combat (cost and ratio GAP-COOKED) |
| Hemomancy | Crimson tide | 10 pts | 20 | Capstone: a huge wave of blood | A wall 600 cm wide that travels 1,500 cm at 900 cm/s; 45 dmg, bleed 60; 15 HP; cd 60; it's your own ability, so it may pass through allies harmlessly | KEPT combat |
| Hex | Hex of weakness | 5 pts | 0 | Spell: cursed enemies deal less damage | Bolt (no damage), curse 80 buildup, cd 10, 4 HP. Curse status (KEPT combat): creatures −15% damage dealt, players −25% path resource and −15% damage dealt, 15 s | KEPT combat |
| Hex | Lingering hex | 3 ranks | 5 | Curses last longer | +25% curse duration per rank (15 → 26 s at 3) | GAP-COOKED |
| Hex | Misfortune | 4 pts | 8 | Cursed enemies miss more often | 20% of a cursed creature's attacks **visibly whiff**: on the contact frame the hit is cancelled, it plays a stumble and a black-purple hex spark. PvP: 10% | GAP-COOKED |
| Hex | Doom | 5 pts | 12 | Spell: a delayed curse that deals heavy damage | Bolt: a mark that bursts after 4 s for 60 + 25% of the damage the target took during those 4 s (bosses ×0.6); ×1.3 if the target is cursed; cd 20; 8 HP. No instant kills (CAL) | KEPT combat (scaling GAP-COOKED) |
| Hex | Contagion | 3 pts | 17 | Curses spread when the target dies | A cursed enemy dying passes its curse, at full duration, to up to 2 enemies within 8 m (a black-purple thread jumps 0.15 s) | GAP-COOKED |
| Hex | Ruin | 10 pts | 20 | Capstone: cursed enemies take greatly increased damage from everyone | Cursed enemies take +25% damage from all sources (bosses +15%); a black-purple sigil turns over their head so the whole party sees it | SPEC text; numbers GAP-COOKED |
| Vampiric | Leech strike | 5 pts | 0 | Weapon art that heals you | Wildbrand: 4 HP, cd 4, W/A/R 0.26/0.18/0.40, ×1.2, bleed 30; heals 50% of damage dealt | KEPT combat |
| Vampiric | Lifesteal | 5 ranks | 5 | More healing from hits | +2% lifesteal on weapon and Wildbrand damage per rank | GAP-COOKED |
| Vampiric | Blood frenzy | 4 pts | 10 | Faster attacks at low HP | Below 50% HP: weapon windups and recoveries ×0.9 (play rate 1.11); below 25%: ×0.82 (1.22). Hit-stop unchanged | GAP-COOKED |
| Vampiric | Hemorrhage | 3 ranks | 14 | More bleed damage | +15% bleed burst damage per rank | GAP-COOKED |
| Vampiric | Feast | 3 pts | 17 | Finishing blows heal a large amount | A kill by melee, Wildbrand or riposte heals 15% max HP + 10 (once per 3 s). Mini-boss kill: 30%. Plays a short drinking flourish only if you're not in combat after it | GAP-COOKED |
| Vampiric | Sanguine lord | 10 pts | 20 | Capstone: for a short time, all damage you deal heals you | Self buff 10 s, cd 90: 30% of all damage you deal (spells and bleed included) heals you, through Spilled first | KEPT combat (share GAP-COOKED) |

**Stat and passive behaviour (all server-side, through `ApplyHit`):**
- **Lifesteal sources, added together:** Bloodthirst, the Lifesteal ranks, Leech strike, Sanguine lord, Mirehide's 4-piece +2%, Mire-witch's ring, the Rune of Thirst (KEPT survival-crafting), and the Mire heart's kill heal.
- **Lifesteal in PvP:** ×0.5 against players (keeps the PvP block, combat 2.20).

**Proc feedback (fire-frost RAISE 2 rule):**

| Node | See | Hear |
|---|---|---|
| Bloodthirst / any lifesteal | Thin garnet threads drawn from the target into your hand for 0.2 s; your Spilled segment fills | A soft wet draw |
| Blood boil | Each bleeding target's wounds flare and burst | Pop chain |
| Misfortune | The whiff stumble with a black-purple spark | Hollow whiff |
| Doom | A black-purple sigil counting down three pulses over the target | Three low tolls, then a crack |
| Contagion | A thread jumping to the next target | A hiss |
| Ruin | A slow-turning sigil over every cursed enemy | A low hum near them |
| Feast | A garnet burst into you from the kill | A heartbeat thump |
| Sanguine lord | Your veins (2.18) light garnet for the full 10 s; a faint garnet aura | Heartbeat in your mix |

**Combos with Blood (CB SPEC, names and effects as written; built in R2 for the paths that exist):**
- **Fire + Blood:** Hex flame / Burning lifesteal.
- **Frost + Blood:** Cold blooded / Ice veins.
- **Lightning + Blood** (opposites): Adrenaline / Red lightning.
- The other 8 Blood pairs are built in their paths' rounds.

### 2.18 Body change: Blood & curse (FULL-GAME draft "dark red veins at the neck, sharper canines"; detail GAP-COOKED in fire-frost's frame)

**Frame (KEPT, fire-frost 3.7):**
- Strength is points ÷ 100. Main path full, subclass ×0.6. Changes ramp smoothly.
- Built on the MetaHuman hero (R0).
- **The look:** sensual and dangerous, never corpse-like. Colour is garnet #7A0A14 and oxblood, never crimson (lesson 7).

| At | What shows |
|---|---|
| 10 | Lips deepen to a natural berry-garnet; a faint flush at the throat in combat |
| 30 | A fine dark-garnet vein tracery at the sides of the neck and the inner wrists that darkens while Spilled is high |
| 50 | Canines lengthen 2 mm (a clean, attractive sharpness, never fangs); a thin garnet ring at the iris edge; the vein tracery reaches the collarbones |
| 75 | Tracery at the temples and down the sternum line; skin takes a cool porcelain clarity with a warm flush in the cheeks and lips (high contrast, striking) |
| 100 | The iris goes fully garnet; in combat below 25% HP the pupils narrow and the veins pulse with the heartbeat (70 bpm calm, 110 in combat) |

- **Lifesteal flush:** every heal from lifesteal sends a 0.3 s warm flush across the visible skin from the hand that struck.
- **Sanguine lord:** lights the veins garnet for 10 s.
- **Blood with another path:** Blood with Fire shows the veins as garnet cores inside the ember lines. Blood with Frost: the frost-fern frosts over garnet veins.

### 2.19 Gear and items from the swamp

**Mirehide set (KEPT, survival-crafting; restated):** medium, poise 40.
- **Inputs (full set):** steel plate 10, bloodmoss 8, Murkjaw hide 4, Slimeshell slime 4.
- **2 pieces:** bleed buildup ×0.5.
- **4 pieces:** wade bleedwater unhurt, lifesteal +2%.
- **Look:** glossy dark croc-hide, hanging moss fringes, red-lacquered bone buckles. Black-green, blood red, bone.

**This lane adds the fit (GAP-COOKED, survival-crafting's tailoring rule):**
- **Women's cut:** a fitted croc-hide corset-cuirass laced at the front, bare shoulders under a short moss-fringed mantle, a split leather skirt over tall sealed thigh-boots.
- **Men's cut:** an open-collared croc-hide jerkin with bare forearms, a moss-fringed shoulder cape, fitted trousers in tall boots.
- Bone buckles lacquered **garnet** (not crimson).

**Swamp items (GAP-COOKED):**

| Item | Station | Inputs | Effect |
|---|---|---|---|
| Bloodmoss poultice (KEPT name, combat) | Alchemy 1 | Bloodmoss 3, fiber 1 | Cures bleed and clears bleed buildup; belt item, 0.9 s drink-style animation (combat 2.16) |
| Leech tar (KEPT effect, world-map) | Alchemy 1 | Leeches 2, salt 1, resin 1 | Bleedwater buildup ×0.3 for 10 min |
| Salt pouch | Workbench 1 | Salt 4, hide 1 | Thrown (arc, 1,600 cm/s, 15 m): on a leech, the Bloat or a latched Gorgeleech: vomit/detach; on anyone else, 2 s of stinging eyes (vignette) |
| Thinblood oil | Alchemy 2 | Leech sac 2, slime 1 | Weapon coating, 60 s: +20 bleed buildup per hit |
| Hex-ward charm (KEPT name, combat cure) | Alchemy 2 (recipe from quest 4 or the Witch's Library) | Bloodroot 2, Hexcrow feathers 3, bone 2 | Talisman: cures curse on use (60 s cooldown) and curse buildup ×0.8 while worn |
| Stormglass jar | Alchemy 2 | Stormglass 1 (Storm peaks), glass 1, copper 1 | Thrown (arc, 1,400 cm/s): shocks water within 6 m of where it lands for 1.5 s (shock 60); Vharra's non-Lightning counter |
| Murkjaw-tooth arrow | Workbench 2 | Murkjaw tooth 1, wood 1, Hexcrow feather 1 → 4 arrows | Bleed 25 per hit |
| Swamp lantern | Workbench 1 | Toad gland 1, glass 1, copper 1 | Hand light 12 m, doesn't light marsh gas (cold light) |
| Bloomery (piece) | Workbench 2 | Clay 20, stone 10 | Smelts bog iron (3:1) with peat or charcoal; the swamp's first iron |
| Stilt foundation (piece) | Workbench 2 | Cypress logs 4 | A foundation set on posts in water up to 2 m deep, so you can build on the murk (cypress never rots) |

**Trophy talismans (KEPT, survival-crafting):** Maw tooth, Hex mask, Bloat sac.

**Mire-witch's ring** (quest 6): lifesteal +2%, bleed buildup taken ×0.85.

### 2.20 The story the swamp tells, with no words (GAP-COOKED; fits world-map's Twelve Peoples and pass wars)

**The Mire Coven fed the Mother.** The Coven were blood-witches who believed the Mire was alive and hungry. They tithed their own blood to her, a little each, in bronze bowls in the Tithe Hall. The channels from every basin run down toward the Throat. The leeches are her brood, the bloodmoss grows where blood soaks in, and the bleedwater is the tithe made permanent.

**What the player sees, in the order they'll likely find it:**
1. **The Stilt Village** (M1): an ordinary, abandoned life of rice and slime stills, but the doors are hex-marked from *outside*. The village sealed itself against something.
2. **Fetish posts** (all): crow skulls on bone poles every 150 m, always facing inward toward the Crown. They point at the Throat.
3. **The Peat Cut** (M3): three bog bodies with braided cords at their necks, calm-faced. They went willingly.
4. **The Tithe Mound and the Walking House** (M3): the Coven's craft, still practised by a newcomer (Ruslana) who learned it from the bones.
5. **The Hanging Cypress** (M4): nine cages; one holds a skeleton gripping a key. It's the key to the cellar of the Walking House, which leads to the Sunken Coven. Someone locked themselves out of their own home, or was locked up so they couldn't go back.
6. **The Sunken Coven** (hidden): the village that sank when the Mother rose. There are twenty-two seated bog bodies, each holding a bowl, and one empty niche with a cord left in it. The Hex-Warden's tithe-list has every name scratched out but hers: she's the empty niche. She never went down.
7. **The Rod Line and the Thorn Gate** (M5): the storm people marched in through Stormgut with copper rods to kill the Mother with lightning. Their bones lie leech-scarred along the rods; the last rod is bent double. The Coven's gate is burnt from the west. Both sides lost.
8. **The Bled Roc** (M5): the Rodwardens' champion, hooked from the sky by Coven bone hooks and chains and bled dry in the bog. On the far side of Stormgut, the Storm lane has the leech-drake burnt black. Each lies dead on the other's side, killed by its opposite (Raven's weakness rule, told by the land).
9. **The Mother's Throat** (M5): six copper rods still stand in the arena, green with age. The storm people got this far, and their rods still work. That's how the player learns to fight her.

**Nobody explains it.** Lore items (Tithe bowl, Braided cord, Coven tithe-list, Rodwarden's tally-cord, Mother's first tooth, Recipe of the tithe) are short, Souls-style item text.

**Secrets (GAP-COOKED):**

| Id | Secret | How found |
|---|---|---|
| M-S1 | **The candle path.** At night, a corpse candle drifts out across Corpse-Candle Mere. Walk its line exactly (the duckweed under its path is a hidden submerged boardwalk, 30 cm down) and it leads to a reed island with a Coven reliquary: Coven ring (+5% curse buildup), bronze bowl, ruby | Night, any weather. Step off the line and the candle goes out |
| M-S2 | The Walking House cellar → the Sunken Coven | Coven key from the Hanging Cypress (or quest 5) |
| M-S3 | **The fourth bog body.** A Bogsnout roots at a dome edge near the Peat Cut and uncovers a fourth body holding a gold torc | Bring a tamed Bogsnout within 15 m |
| M-S4 | **Behind the third fall** of the Weeping Wall: a ledge path to The Gullet cave and a Coven smith's cache (bog-iron ingots ×6, the Bloomery mould scrap) | See the gap from the Falls Shrine at sunset, when the falls thin |
| M-S5 | **The Reliquary** in the Sunken Coven | Red Tide only |

### 2.21 Invasions: Lightning ↔ Blood through Stormgut (3.3 rule KEPT; contents GAP-COOKED)

**Blood invades Storm peaks:**
- **Who:** 8 to 14 creatures. Gorgeleech packs (night) or Bogstalkers with Mudhulks (day), Hexcrow murders over them, 1 Murkjaw.
- **Led by The Bloat**, which leaves its route.
- **Their mark:** each invader that stands 4 s leaves a 3 m bleedwater puddle for 2 game hours.
- **The answer:** Storm players' lightning chains through the invaders' wet bodies, so the counter is on the defenders' side, as it should be.

**Storm invades the Swamp:**
- **Who:** Voltfang packs, Stormkites, a Thunderhorn, led by the Bellwether (Storm lane's roaming mini).
- **Their mark:** a storm cell rides over them. Strikes hit the tallest thing within 60 m every 20 to 40 s (1.0 s ground glow, 3.3 KEPT strike numbers). **Strikes on water shock everyone in that water within 15 m:** the swamp's own players are punished for standing in it.

**Payout:** blood and storm essence, plus 2,000 path XP to everyone with 10% (3.3 KEPT).

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:**
- Data first, in `Data/*.json`, with first-draft numbers.
- Abilities are `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode`.
- All damage goes through `UWRCombatLibrary::ApplyHit`.
- Server authority.
- Runtime-spawned dressing from data, so `L_Island` is touched as little as possible.

**R2 also delivers these shared pieces, needed here:**
- the **bleed** and **curse** statuses (FULL-GAME 11);
- the attack-row runner (combat C9);
- visible-contact boss damage (combat C10);
- the world-map R1/R2 terrain for the Swamp half of B3;
- shrines, path masters and events from the fire-frost frame.

This lane plans the swamp content on top of them.

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/blood.json`** | Same schema as `fire.json`: `id: "blood"`, `name: "Blood & curse"`, `biome: "swamp"`, `starter: "blood_bolt"`, `resource {id: "blood", name: "Blood"}`, `rootPassive {id: "bloodthirst", name: "Bloodthirst", text: SPEC}`, trunk and three branches with node ids `blood_bolt, blood_reserve, bleed_chance, crimson_slash; blood_spear, sanguine_power, blood_boil, clotting, transfusion, crimson_tide; hex_of_weakness, lingering_hex, misfortune, doom, contagion, ruin; leech_strike, lifesteal, blood_frenzy, hemorrhage, feast, sanguine_lord`, `kind` (ability/stat/capstone), costs, ranks and gates as in 2.17. Node text copied from ST word for word |
| `Data/tuning.json` `resources` | `bloodSpilledWindow: 8, bloodRallyMultiplier: 2.0, clottingWindow: 12, clottingRegen: 2` |
| `Data/tuning.json` `nodeEffects` | `blood.blood_reserve {maxHealth: 8}`, `blood.bleed_chance {bleedBuildup: 0.10, weaponBleed: 6}`, `blood.sanguine_power {bloodSpellDamage: 0.06}`, `blood.lingering_hex {curseDuration: 0.25}`, `blood.lifesteal {lifesteal: 0.02}`, `blood.hemorrhage {bleedBurst: 0.15}` |
| `Data/tuning.json` `passives` | `bloodthirstMax: 0.15, bloodthirstFullAt: 0.9, bloodthirstMaxAt: 0.2, misfortuneChance: 0.2, misfortunePvp: 0.1, contagionRadius: 800, contagionTargets: 2, ruinBonus: 0.25, ruinBossBonus: 0.15, frenzyHalf: 0.9, frenzyQuarter: 0.82, feastHeal: 0.15, feastFlat: 10, feastMini: 0.30, feastCooldown: 3, pvpLifesteal: 0.5` |
| `Data/tuning.json` `spells` | Blood entries as combat 2.13 (KEPT), plus `bloodSpear.pierce: 3`, `bloodBoil.hpCost: 8`, `transfusion {hpCost: 20, heal: 30, seconds: 1.5, range: 1200}`, `doom {delay: 4, base: 60, takenShare: 0.25, bossScale: 0.6, cursedScale: 1.3, hpCost: 8}`, `hexOfWeakness.hpCost: 4`, `sanguineLord.share: 0.30` |
| `Data/tuning.json` `status` | Bleed and curse rows from combat 2.11 (KEPT); colours bleed #7A0A14, curse black-purple |
| `Data/tuning.json` `hazards` | `bleedwater {buildupPerSecond: 10, tellSeconds: 0.9, tarMultiplier: 0.3}`, `quag {runMultiplier: 0.5, sinkAfter: 3}`, `marshGas {igniteRadius: 300, flashRadius: 400, damage: 40, burn: 40, chainDelay: 0.15, tell: 0.9}`, `rotPlank {creak: 0.9, repairWood: 2, repairDays: 4}`, `leechBoil {buildupPerSecond: 15, latchEvery: 10}` |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `duckweed_burst, death_roll, tail_sweep_low, latch, loop_strike, spring_lunge, swim_lunge, curse_cry, dive_rake, peck_flurry, call_murder, tongue_lash, gulp, belly_flop, mud_spit, bill_strike, clatter, gape_chomp, wallow_surge, flash_puff, withdraw` |
| `Data/tuning.json` `bosses` | `vharra {stance: 160, parries: 3, rodShockSeconds: 1.5, rodThreshold: 60, rodCooldown: 20, convulseSeconds: 4, convulseStance: 2.0, floodRise: 120, floodSeconds: 6, sacPartHealth: 1200, slickSeconds: 30, p3HealPerSecond: 0.01, p3ClotSeconds: 10, broodCount: 4, broodCooldown: 45, bleedResist: 0.5}`, `mother_maw {youngAt: [0.6, 0.3], nestBurnSeconds: 8, plankRepairSeconds: 3}`, `hex_warden {totemHealth: 400, totemBluntFire: 2.0, scatterReform: 1.2, mimicSeconds: 6, mimicTames: 2, bloodTotemHeal: 0.02, staggerEveryHits: 5}`, `bloat {gorgeOnPlayer: 0.08, gorgeOnCreature: 0.04, gorgeInBleedwater: 0.01, burstRadius: 800, burstStagger: 6, saltVomit: 0.40, saltStagger: 3, burnGorge: 0.20, frostStop: 12, veinHealth: 300}`; per-attack rows from 2.13 and 2.14; run-back stone positions; lock points |
| `Data/tuning.json` `events` | `redTide {warnSeconds: 300, subZones: ["M3", "M4", "M5"], leechSpawn: 3.0, bloatGorge: 2.0, hexRadius: 3000, hexCurse: 25, hexAggro: 1.3, payout: 3.0}` |
| `Data/creatures.json` | 10 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `blood`, `resists ["bleedwater", "quag", "marsh_gas"]`, taming, difficulty, food, attacks, activity, loot, mount and rideSpeed for Mirestrider and Mudhulk, utility `light` (Lanterntoad), `dig` (Bogsnout)); 4 boss rows (`vharra` major, `mother_maw`, `hex_warden`, `bloat` with `roaming` and `route`) |
| `Data/spawn_regions.json` | Swamp regions M1 to M5 with `area` boxes from 2.2 and `night` tables from 2.12.3 |
| `Data/items.json` | `bloodmoss, bog_iron, peat, salt, leech, leech_sac, hexcrow_feather, murkjaw_hide, murkjaw_tooth, frog_skin, toad_gland, thick_hide, cypress_log, blood_essence, core_vharra, mire_heart, trophy_maw_tooth, trophy_hex_mask, trophy_bloat_sac, maws_hook, bloats_blood, coven_key, coven_ring, mire_witch_ring, three_jaw_glaive_bp, coven_decor_bp`; consumables `bloodmoss_poultice, leech_tar, salt_pouch, thinblood_oil, hex_ward_charm, stormglass_jar, murkjaw_arrow, swamp_lantern`; lore items (6) |
| `Data/recipes.json` | The 2.19 recipes; bog iron 3 → iron ingot at the bloomery or furnace; Mirehide (KEPT inputs); sealed waders (KEPT) |
| `Data/pieces.json` | `bloomery`, `stilt_foundation` (water up to 200 cm), `plank_bridge` (freeform, allowed outside claims, decays in 4 game days) |
| New/extended `Data/world_pois.json` (fire-frost's file) | Swamp sub-zone polygons, 7 landmarks, 24 hand sites, 5 shrines, 4 caves, Sunken Coven rooms, kit rules (2.4), the Bloat's route spline, the Hex-Warden's Red Tide route, the corpse-candle line, Vharra's arena layout (terraces, mounds, rod plinths) |
| `Data/quests.json` | Ruslana's 6 quests (2.11) |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 10 and 4 bosses; pools `explore.swamp.day/night`, `combat.swamp`, `mini.swamp`, `boss.vharra` (3 sections), `sub.sunken_coven`, `event.red_tide`, `camp.swamp`; sub-zone ambient beds M1 to M5 |
| `Data/fx.json` | Lifesteal threads, Spilled bar fill, Blood boil burst, Doom sigil, Ruin sigil, Contagion thread, marsh-gas flash chain, duckweed part line, bleedwater ripple, rod-shock water arcs, Vharra sac fill, Bloat swell, corpse candles, Red Tide falls |
| `Data/combos.json` (R1 creates it) | Fire+Blood, Frost+Blood, Lightning+Blood rows from CB |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `BleedBuildup`, `CurseBuildup` (beside Burn and Frost), `bLifestealable`, and `HpCost` for logging |
| `WRStatusComponent.cpp/.h` | **Bleed:** buildup meter, proc burst (12% max HP + 15, bosses ×0.7), threshold ×1.3 growth (combat KEPT). **Curse:** 15 s × Lingering hex, −15% damage dealt (creatures), −25% path resource and −15% damage for players; Ruin's damage-taken bonus; Misfortune flag read by the creature attack runner. Grimjaw's regen check gains "bleeding" (the existing comment in `WRBoss.cpp` 1107 asks for it) |
| `WRCombatLibrary.cpp` `ApplyHit` | After damage resolves: **one lifesteal pipeline.** Sum Bloodthirst (HP curve), Lifesteal ranks, Leech strike, Sanguine lord, gear and rune lifesteal → heal = damage × share (PvP ×0.5) → goes to Spilled first at ×2 inside the window. Feast and Mire heart on kill. Contagion on a cursed death. Doom damage-taken tracking |
| `WRAttributeSet` / `WRPathComponent` | **Blood resource = HP.** Add a replicated `Spilled` amount and `SpilledUntil` time on the path component (no new attribute; HP is the resource). `SpendBlood(Cost)`: fails if HP ≤ cost; else HP −= cost, Spilled += cost, window reset. Clotting regen tick. HUD reads Spilled for the garnet segment |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_BloodBolt` (UWRGA_Spell), `UWRGA_BloodSpear` (AreaSpell line/pierce like Ice lance), `UWRGA_BloodBoil` (AreaSpell nova), `UWRGA_Transfusion` (ally-target channel), `UWRGA_CrimsonTide` (travelling wall, like Fire wall moving), `UWRGA_HexOfWeakness` and `UWRGA_Doom` (UWRGA_Spell with curse/mark payload), `UWRGA_CrimsonSlash` and `UWRGA_LeechStrike` (UWRGA_Melee Wildbrands), `UWRGA_SanguineLord` (self buff). All Blood abilities override the cost check to call `SpendBlood`. `ClassForNode`: a new `blood` block mapping the 10 ability node ids. Passives (`bloodthirst`, `clotting`, `misfortune`, `contagion`, `ruin`, `blood_frenzy`, `feast`) through `UWRPathComponent::HasNode` as Fire's are. Blood frenzy sets the melee montage play rate |
| `WRCreature.cpp` | Attack-row kinds from 3.1. **Latch:** attach the leech to a player limb socket (max 3, one per limb), drain tick through ApplyHit with `bDamageOverTime`, detach on roll (60%), fire, salt, ally hit or death. **Tongue lash:** a sphere sweep along the tongue bone; on hit, a server-side pull of 600 cm (root motion on the victim). **Gulp:** hide the victim inside (attach to the mouth socket), a mash-to-escape timer, throat lock point damage frees. **Death roll:** a paired grab with motion warping (combat's paired-crit tech), drag-under if in water. **Duckweed burst:** ambush state in water under a duckweed decal; a parting-line tell. **Curse cry:** a cone check with line of sight, stacking per crow. Mirestrider and Mudhulk mounts ignore quag and bleedwater (`resists`) |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `vharra`, `mother_maw`, `hex_warden`, `bloat`. **Vharra:** anchored-yaw constraint in phase 1 (disc socket fixed to the west wall), the flood (drives an `AWRWater` volume's height 0 → 120 cm over 6 s), swim locomotion in phase 2, sacs as part-health lock points, Brood shed spawning 4 swarm leeches that target the furthest tame. **Rods:** a new `AWRConductorRod` actor (copper rod: takes lightning damage, then broadcasts a shock field to the connected water volume for 1.5 s); a water-volume query for "is the boss body in shocked water" → convulse state. Phase 3: water tagged bleedwater, heal while submerged, a clot crust spawn. **Mother Maw:** boardwalk section actors with health (her slams break them; Interact to repair); "can't dive under planks" = nav area flag; rear-and-expose belly. **Hex-Warden:** 4 totem actors with health and a curse each; scatter teleport with i-frames; mimic call picks up to 2 tames and plays their own voice cue from her. **Bloat:** gorge float (replicated, drives a material parameter for swell and glow), salt-hit vomit, burst at 100. Roaming route component (fire-frost's) |
| `WRWorldDressing.cpp` | `FWRBiomeHazards::KindAt` gains `bleedwater, quag, marsh_gas, rot_plank` from `island_dressing.json` hazard cells. New actors: `AWRMarshGas` (ignite on fire within radius, chain), `AWRRotPlank` (weight check → creak → break; repair), the quag volume (CMC speed and no-roll flag). Kit and hand POI spawning from `world_pois.json` |
| `WRProjectile.cpp` | Thrown `salt_pouch` and `stormglass_jar` as ballistic, never-homing projectiles; the jar spawns a shock field on the water it lands in |
| `WRWater.cpp` | Water volumes carry a `kind` (clean / bleedwater / brackish) and a `shocked` flag. Wading in bleedwater ticks buildup through the status component. Brackish volumes block the Red Tide conversion |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The Red Tide timeline: warn (falls material swap, crow takeoff, the Ring pillar), water kinds swapped in M3 to M5, spawn multipliers, the Bloat's route change, the Hex-Warden's night walk and the hex aura on beasts within 30 m, the Reliquary door |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 swamp shrines; Ruslana (MetaHuman, Walking House and Tithe Mound idle loops); quests |
| `WRBodyChange.cpp` (fire-frost's) | Blood parameters: lip tint, vein-tracery mask reveal (neck, wrists, collarbones, temples, sternum), canine morph target 0 → 1, iris ring and fill, pulse with heartbeat, lifesteal flush event, Sanguine lord glow; blend rules with Fire and Frost |
| `WRUIHud` / `WRPlayerUI` | The Spilled garnet segment on the HP bar with the rally window shown as a fading edge; bleed and curse meters (combat's status UI) |
| `WRAudioDirector.cpp` | Swamp pools and sub-zone beds; Vharra's 3 sections; the Red Tide layer |
| `WRSky.cpp` | Fog and Miasma states (knee fog default, raised fog in states), the falls' red material, the Throat's mist column, corpse candles (Niagara, night only) |

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
| Murkjaw | Concept from saltwater croc + false-gharial teeth; Hunyuan base; Blender teeth bar, osteoderm moss cards, red eyeshine (an emissive tapetum masked to night). Clips: a Fab crocodile/alligator animated pack (walk, high-walk, swim, lunge, bite, death roll, tail sweep, death); death roll as a paired montage |
| Gorgeleech | Custom sculpt (no pack has leeches); a spline-chain rig (12 segments) with the front and back suckers as end controls. Clips: hand-keyed in Blender on the chain (inchworm loop cycle, swim undulation, latch, rear) plus Control Rig secondary motion; translucent subsurface skin shader |
| Hexcrow | Concept raven + rook face; clips from a Fab bird/raven pack (fly, glide, land, hop, peck, caw) |
| Bogstalker | Concept horned frog; clips from a Fab frog/toad pack if one fits, else keyed (squat idle, hop, belly-flop leap, tongue lash with a spline tongue, gulp); the tongue is a spline mesh driven by the trace |
| Mirestrider | Concept shoebill + stilt bird; clips from a Fab ostrich/emu or heron pack (walk, run, stab, wing buffet, kick, clatter) |
| Mudhulk | Hippo; a Fab hippo or rhino pack (walk, run, yawn, bite, wallow, swim); the blood-sweat sheen is a wet material layer |
| Bogsnout | Fab boar/pig pack plus babirusa tusks |
| Lanterntoad, Reedfowl, Slimeshell | Small: a toad pack or keyed; a Fab bird pack (chicken/heron) for Reedfowl; Slimeshell keyed (foot wave, shell withdraw), with pink egg clutch props |
| Mother Maw | Murkjaw rig scaled ×1.9 with a new back (duckweed and moss mat as groom cards) and scars |
| Hex-Warden | MetaHuman female body (attractive, athletic) with a sculpted crow-skull mask, feather cloak (groom cards plus cloth sim), corset and sickles; clips from the hero's dual-wield set plus a scatter VFX (crow particle burst) |
| The Bloat | Gorgeleech rig ×4.3 with a swell blend shape and an emissive blood-volume material (gorge 0 → 1) |
| Vharra | New concept (Aaron picks). Drake front from a Fab dragon pack's skeleton (forelimbs, neck, head) fused to a 14-bone spline trunk with a disc end; the three-plate jaw is three bones with a blend shape for the Y-split. Clips: dragon pack attacks for the front (bite, swipe, rear, roar) plus keyed trunk lashes, swim and convulse. Sacs as separate meshes with a fill parameter |
| Ruslana | MetaHuman preset (attractive, early 30s), outfit as 2.11 |
| Landmarks | Weeping Wall (terrain plus waterfall cards, iron-red material), the Throat (sinkhole sculpt, terraces, 12 dead cypress, cages), Bled Roc (a Fab bird skeleton scaled up, plus hooks and chains), Walking House (Blender: hut plus carved bird legs), Hanging Cypress, Mangrove Cathedral |
| Kit | Fab swamp packs for cypress, mangrove, Spanish moss, duckweed, reeds, pitcher plants (licence rule: use unless explicitly barred); Coven props in Blender: fetish posts, bone mounds, boardwalk modules (sound and rotten variants), cages, tithe basins, bottle charms |
| Water | Four water materials (clear murk, duckweed, bleedwater with the leech-ripple flipbook, quag) |

### 3.4 Sound and music

- **Voices:** the 2.15 recipes from Freesound CC0/CC-BY, Sonniss GDC bundles and Kenney, layered in a DAW per species. About 8 files per creature and 12+ per boss. Loudness matched.
- **Music:** the 2.16 cue sheet chosen from libraries first (about 18 cues counting stems), credited.
- **Beds:** 5 sub-zone ambient beds plus a Sunken Coven bed and a waterfall bed that rises toward the cliffs.

### 3.5 Map dressing

- `Tools/terrain/dressing.mjs` reads the swamp entries in `world_pois.json`:
  - It places hand sites at their coordinates.
  - It fills kit POIs per sub-zone with Poisson spacing of 150 m or more.
  - It walks routes (boardwalks, reed causeways, root paths) and adds a kit POI wherever a point is more than 125 m from any POI.
- **Water kinds are painted from polygons:** bleedwater 18%, duckweed 25% of murk, quag margins. They're written as `hazards` cells and water-volume kinds.
- **Ground layers:** reed mud, peat, bloodmoss, bone, mangrove mud. Painted from sub-zone polygons by the existing auto-material.
- **Boardwalks** are spline-placed modules with 15% rotten planks, never two rotten in a row on the main Drowned Causeway.
- **The landmass itself** comes from world-map's R1/R2 W8 step (B3 swamp half). This lane only dresses it.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| # | Step | Why here |
|---|---|---|
| 0 | **Shared R2 statuses: bleed and curse** in `WRStatusComponent` + `FWRHit` buildups + status meters (with combat C-steps) | Everything below uses them |
| 1 | **Proof of one: the Murkjaw at a duckweed pool, end to end.** Concept (Aaron picks), mesh, real croc clips, voice, attack rows with socket hits (duckweed burst, snaps, death roll, tail sweep, land lunge), the duckweed tell, a bleedwater pool beside it with its tells and the 10/s buildup, the Bloodmoss poultice, and one M4 POI dressed around it | Proves the water ambush, a grab, visible-contact damage, real clips, a fused voice, the bleed status, the hazard and a dressed place in one item |
| 2 | **The Blood path:** `blood.json`, `SpendBlood` and the Spilled bar, the 10 ability classes in `ClassForNode`, the passives, the one lifesteal pipeline in `ApplyHit`, the proc FX, the CB combos with Fire, Frost and Lightning | The path is the round's headline; it needs only step 0 |
| 3 | Swamp terrain dressing: sub-zones M1 to M5, the four water kinds, boardwalks, the 7 landmarks, the 24 hand sites, the kit fill, caves, resources (bloodmoss, bog iron, peat, salt) | The world the rest stands in |
| 4 | The other 9 creatures with clips, voices and rows; day and night tables; living-world beats | Reuses step 1's frame |
| 5 | Micro-hazards (quag, marsh gas, rotten planks) and weather (Fog, Miasma) | After the ground exists |
| 6 | Mother Maw (boardwalk counter), then the Hex-Warden (totems), then The Bloat (roaming, gorge, salt) | Minis in order of new tech |
| 7 | Vharra: arena, the anchored phase 1, the flood transition, swimming phase 2, rods and the stormglass jar, phase 3 behind the artifact flag; drops; music | The biggest single item; needs creatures, water kinds and rods |
| 8 | Shrines, Ruslana and her 6 quests, run-back stone, fast travel | After POIs and the hero rig |
| 9 | The Sunken Coven and the five secrets | After the hazards and the key quest |
| 10 | Music set and sub-zone beds | Independent; can slot in anywhere after step 3 |
| 11 | The Red Tide event (and the Hex-Warden's night walk) | Needs the minis, water kinds and the Coven reliquary |
| 12 | Gear: Mirehide fit on the MetaHuman bodies, swamp items, bloomery, stilt foundation | After the R2 slot split (survival-crafting) |
| 13 | Body change for Blood | After the MetaHuman hero (R0) |
| 14 | Lightning ↔ Blood invasions | Needs both biomes' roaming minis and the invasion frame |

**Gates:** compile, cook and package only. Aaron tests each step.
