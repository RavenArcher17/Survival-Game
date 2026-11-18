# Cook: biome-void (Corrupted wasteland / Void, plus the endgame hook, round R10)

Lane: biome-void. Written 2026-11-13/14. Scope: FULL-GAME 3.2.12 (Corrupted wasteland), the Wasteland rows of 3.3, the Void row of 4.2, and FULL-GAME 11's R10 line: raids, mythic gear, prestige, leaderboards, achievements, titles, cosmetics, monuments, and every pass opening.

**Tags.** **SPEC** = Raven's design or a built thing, kept exactly as written. **GAP-COOKED** = this lane's design, replacing a FULL-GAME first draft. **KEPT** = a name or number another cook already set (world-map, combat, survival-crafting, hero, ui-controls, biome-crystal, biome-graveyard, fire-frost). It's restated here so the build has one page. This lane doesn't change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** the Wasteland is the **west half of belt B6, the Broken End**, y +4,600 to +6,400. Its neighbour is the Graveyard to the north, joined by a grey gradient where graves end mid-row. Its opposite is the Crystal caves, across **the Severance**: centre (+1,250, +5,400), saddle 20 m, 1,100 × 80 m. Camps sit at (−2,550, +5,000), (−2,400, +5,800) and (−1,600, +6,300). **Vorhollow** fights on a collapsing island over the biggest rift at (+300, +5,600), and **the Unmade Place** is inside that rift. There are 138 surface POIs plus 8 in the Unmade Place, 146 in all. Weather weights: Clear 20, Overcast 40 grey, Rain 0, Fog 10 dust, **Dead calm** 30 (no wind, no ambient sound, footsteps doubled, hearing hunters ×1.5; one of the map's three planned silences). The night light is rift-edge glow, and there's no moon. The old people are the **Hollow Choir**, and their waymark is **cairns balanced point-down**. At the Severance a void-shape's ash cast sits in a crystal cage on the Void side, and a crystal giantess hollowed from the inside lies on the Crystal side. Their stair stone on the Crown is void-grey glass. The **Broken Race** tidal race runs off the tip. The Broken End "splits like legs at the Severance" (the giant-body motif).
- **survival-crafting.md:** **Rest** (10 s sleep in an owned bed or bedroll) restores all max HP lost to Void. It also covers the **Corruption** world meter (0 to 100; builds in the Graveyard and the Wasteland; steps at 25/50/75/100; a Void walker in the Wasteland takes no penalties and gets +3/6/10/15% path damage). The HUD calls the Void path bar **Void** so it never reads like the world meter. Attribute `MaxHealthLost`. **Grey wind** (corruption ×2, footsteps muffled 30%). The **Voidglass armour numbers** (heavy, poise 50, steel plate 16, voidglass 10, Withergoat hide 6, star iron 2; curse buildup ×0.5; rift drain halved; void max HP cost −25%). **Nullward** (talisman, Armorer 4, voidglass 4 + steel plate 2, rift drain halved). The five Void trophy talismans. **Rift chest** (60 slots shared by all of a clan's Rift chests). The **Hollow bastion** building set. **Withergoat** produce (milk 10 min, hide 30 min, eats ashen grain). Void fishing (Hollow perch, Null eel, **the Fish That Isn't** in the Unmade Place's pool). **Mandrake** (Void rare crop; it screams, and wax earplugs block it). Curse resist potion (ashen grain + voidglass). Rune of Hollowing. Boss monuments (R10). Void ×1.25 against building pieces. Signature resources are never farmable.
- **combat.md:** every Void spell and Wildbrand number (2.12, 2.13: Void bolt, Void rift, Collapse, Blink, Shadow step, Phase, Nowhere, Black hole, Maw of the void, Sacrifice, Null strike, Consume), the attack-row schema (2.18.1), the templates (2.18.3), no attack token (2.18.4), the boss template (2.17.1, 2.17.5), visible-contact damage and small cues (2.17.1b), colour law (gold yours, crimson unblockable), the Curse status (−25% path resource, −15% damage dealt, 15 s), sparks by surface (void = inward ink smear), hit feel (2.14), the PvP block (2.20).
- **hero.md:** Void colour black-violet **#2B1840**. The Void set is renamed **Hollowplate**, because "Voidglass" is the resource's name: voidglass plates in black iron, a floating rubble halo behind the back, one jagged asymmetric pauldron, face (black eyes) and forearms open. The Void body-change row (skin greys, black fingertips, eyes black from the centre, light-bend haze at the hands).
- **biome-crystal.md:** the Severance invasion rules for both sides (2.22). Ward fields throw void creatures ×2. The pass duet's Void half is reversed and detuned strings against the armonica. Lyrathis's dome takes void ×3, and a rift eats it at once (R10). `UWRNoiseSubsystem` (hearing hunters). The `thinFloor` hazard. Every major gets two run-back stones.
- **fire-frost.md:** shrine frame (5 per biome: 1 Great + 4 lesser; guards, 3 s light, XP 1,000 / 500 / 2,000, fast travel, re-guard every 4 game days), path masters (short lines at 6 m, no camera grab, quests in the Journal), roaming minis (spline route, 65 m chase from engage point, 4 h respawn), 3 to 4 micro-hazards per biome with ≥ 0.9 s warning, biome event frame (one game day = 60 real min, about once every 3 real days, server setting), POI spacing (150 m minimum between POIs, fill any route point > 125 m from one).

---

## 0. Requirement ledger

Every requirement from the brief, Aaron's quotes in it, and the lane frame, one per row. "Answered in" points at this doc. "Build proof" gets file:line or a commit when the round is built. A row without proof is not done.

| # | Requirement (source) | Answered in | Build proof |
|---|---|---|---|
| 1 | Full cook: research, then plan, then build, spot-check; no skipping (brief; Aaron 11-13 night) | 1, 2, 3, 3.8 | (build) |
| 2 | Research names the games, the specific places and systems, and why they work; real-world reference | 1.1, 1.2 | n/a (doc) |
| 3 | Research doc holds the specific details used | 1.3 | n/a (doc) |
| 4 | Plan solves every design gap before building: layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases | 2, 3 | n/a (doc) |
| 5 | Every build step listed | 3.7 | n/a (doc) |
| 6 | SPEC items kept exactly; every GAP replaced with the best design | 2.0 scope map | (build) |
| 7 | Look of the biome | 2.1 | (build) |
| 8 | 3 to 5 sub-zones, each with its own ground, palette, props, creature mix and bed | 2.2 | (build) |
| 9 | A POI every 150 to 250 m | 2.4 | (build) |
| 10 | Landmarks | 2.3 | (build) |
| 11 | Hidden areas (hidden sub-biome, secrets) | 2.5, 2.4.4 | (build) |
| 12 | Resources (signature and others) | 2.6, 2.7 | (build) |
| 13 | Hazards | 2.8 | (build) |
| 14 | Weather | 2.9 | (build) |
| 15 | World events | 2.10, 2.22 | (build) |
| 16 | Shrines and path master | 2.11 | (build) |
| 17 | Full creature roster (about 9) with attack lists and timings | 2.12 | (build) |
| 18 | Mini-bosses | 2.13 | (build) |
| 19 | Major boss: phases, arena, moves | 2.14 | (build) |
| 20 | Unique creature voices (fused real animal calls) | 2.15 | (build) |
| 21 | Biome music: real composed, always audible, ducks ≤ 4 dB, only planned silences | 2.16 | (build) |
| 22 | Path skill tree: every node, with numbers | 2.17 | (build) |
| 23 | Path body change | 2.18 | (build) |
| 24 | Gear and armour set from this biome | 2.19 | (build) |
| 25 | Story told only through the environment | 2.20 | (build) |
| 26 | Fits Raven's biome and path rules (DD, ST, CR, BO, CF) | 2.0, 2.12 rules, 2.17 | (build) |
| 27 | FULL-GAME 3.2, 3.3 and 4.2 rows for this biome | 2.0 | n/a (doc) |
| 28 | Combat bar: full hit stack, hitstop 0.06 to 0.30 s, visible coil (0.14 light / 0.20 heavy), one verb per button, 0.33 s buffer | 2.12 rules, 2.14.0 (KEPT combat) | (build) |
| 29 | Attacks show the real weapon, claw or projectile; damage only on visible contact; the telegraph is a small cue; tells read at 15 m | 2.12, 2.13, 2.14 | (build) |
| 30 | Projectiles give ≥ 0.9 s warning and never home | 2.12.2, 2.13, 2.14, 2.13.5 (Echo copies) | (build) |
| 31 | No attacker limit; packs aggro freely | 2.12 rules | (build) |
| 32 | Players die; no safety floors; no helpers after a fail | 2.8, 2.14, 2.17.1 (the self-cost limit is a resource rule, not a floor) | (build) |
| 33 | Monsters terrifying, readable silhouettes, believable anatomy, no slop, no primitives | 2.12.1, 2.13, 2.14.1, 3.3 | (build) |
| 34 | Real animation clips, no gliding | 3.3 | (build) |
| 35 | UI: mouse and pad on every screen, no dead states, F1 | 2.23.9, 3.2 | (build) |
| 36 | People attractive, naturally sexy, never sanitised | 2.11, 2.19, 2.20 | (build) |
| 37 | Weapon skills called Wildbrands; taming as Raven designed; no spirit summons | 2.12.4, 2.17 | (build) |
| 38 | Design with intention: why each thing is here, what it connects to, how it meets what it touches (Aaron 11-14) | 2.1 grand puzzle, 2.3 to 2.5, 2.8, 2.14.1 | (build) |
| 39 | Machines look engineered; parts mesh; nothing passes through anything; moving parts move smoothly on a believable mechanism, never snap | 2.14.1 chains and island, 2.13.4 Null Gate, 2.5 Descent lift, 3.2 time-driven movers | (build) |
| 40 | No missed requirements: ledger at the top, every row gets proof | this table, 4 | (build) |
| 41 | Endgame: every pass opens at R10 | 2.23.1 | (build) |
| 42 | Endgame: raids for 6 to 10 | 2.23.2 | (build) |
| 43 | Endgame: mythic gear from every biome | 2.23.3 | (build) |
| 44 | Endgame: prestige (reset a path for permanent perks) | 2.23.4 | (build) |
| 45 | Endgame: boss kill-time leaderboards | 2.23.5 | (build) |
| 46 | Endgame: achievements | 2.23.6 | (build) |
| 47 | Endgame: titles | 2.23.7 | (build) |
| 48 | Endgame: cosmetics for hard content | 2.23.8 | (build) |
| 49 | Endgame: boss trophies as monuments (KEPT survival-crafting) | 2.23.8 | (build) |
| 50 | R10 combos: the 11 new Void pairs; all 66 pairs complete | 2.17.5 | (build) |
| 51 | Invasions Arcane ↔ Void switch on (DD pair) | 2.22 | (build) |
| 52 | Corrupted variants and a mini-boss gauntlet before the last major (CAL Part 3) | 2.12.5, 2.2 V5, 2.13 | (build) |
| 53 | Plan in Raven's repo: data rows, C++ (UWRGameplayAbility + ClassForNode, ApplyHit, data-driven), assets and art route, sound and music cues, map dressing, build order with proof-of-one first | 3 | n/a (doc) |
| 54 | Don't edit Raven's repo or FULL-GAME; no git; no tests, builds or renders | lane rule (followed) | this doc only |
| 55 | Out-of-scope findings get one line each in LATER-COOKS.md | 4 | LATER-COOKS.md |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game, place or system | What it does and why it works | What we take |
|---|---|---|
| **Enshrouded, the Shroud** ([GameSpot](https://gamespot.com/articles/enshrouded-survive-shroud-effects-guide/1100-6520563/), [xGamingServer](https://xgamingserver.com/blog/enshrouded-shroud-flame-progression/)) | Entering the Shroud starts a visible timer. Flame Altar levels add about a minute each, and flasks and Hourglass capsules buy more. The hazard *is* the progression: you return stronger and stay longer. The cost is time, so every trip has a plan | **Rift zones cost max HP, not time, and the gear you earn makes them affordable:** nullward and Hollowplate halve the drain, anchor salt halves it for 5 min, Empty vessel passes it entirely. The loss is visible on your health bar as a grey hatched band, so you always know what a trip has cost. Rest gives it all back. That plans every trip the way Enshrouded's timer does, without a countdown |
| **Once Human, Sanity and contamination** ([Once Human dev blog 06-2025](https://www.oncehuman.game/news/devBlog/20250603/40781_1221471.html), [Sportskeeda](https://www.sportskeeda.com/mmo/once-human-sanity-managment-everything-need-to-know)) | Sanity sets max HP. Contamination zones drain Sanity at (contamination − resist) per second, it shows as a blue band eating the health bar, and sleeping in a bed restores it | **The grey band is the right read.** One `MaxHealthLost` attribute shared by rifts, Gnawer latches, Vorhollow's bite and your own void abilities, drawn on the right end of the health bar, restored by Rest. The formula is simple: base drain × resist multipliers |
| **Conan Exiles, Corruption** ([xGamingServer](https://xgamingserver.com/blog/conan-exiles-corruption-guide/), [GamerGuides](https://gamerguides.com/conan-exiles/guide/sorcery/corruption)) | Sorcery is paid in corruption, which locks part of max HP and stamina until cleansed. Players love the "power that eats you" trade, but the flat 50% penalty made corrupted builds feel bad for long stretches | **Void's power is paid in max HP, but it can be earned back mid-fight:** Consume, Feed, the Void heart part, and four combos restore it. A good Void player stays topped up by killing; a careless one ends a fight at half bar. There's a tolerance buffer, so light use is free (Raven's Void tolerance node) |
| **Elden Ring, Astel, Naturalborn of the Void** ([Fextralife](https://eldenring.wiki.fextralife.com/Astel+Naturalborn+of+the+Void), [GameSpot](https://www.gamespot.com/articles/elden-ring-how-to-beat-astel-naturalborn-of-the-void/1100-6502671/)) | A cosmic insect-skull thing in a starfield. It teleports after it takes several hits and can follow with a grab, and it uses gravity orbs, meteors and a cosmic beam. It reads as alien because its body is wrong yet coherent | **Vorhollow's Rift dive** (gone, then bursting up under a piece after a lensing-ring tell) and **its chest-hole beam.** We keep Astel's lesson that a void thing still needs a coherent body. Vorhollow is built on real anatomy (aye-aye forelimbs, hyena back, lamprey mouth), not a floating shape |
| **Elden Ring DLC, the Abyssal Woods** ([GamesRadar](https://gamesradar.com/games/action-rpg/fromsoftwares-next-game-should-be-full-on-horror-and-one-sequence-in-shadow-of-the-erdtree-proves-it), [Kotaku](https://kotaku.com/elden-ring-shadow-of-the-erdtree-abyssal-woods-torrent-1851560149)) | Lantern-bearers you're told not to let see you, which grab and inflict Madness. Their horror tone is praised. But the area was called the DLC's weakest because the long, effortful stretch wasn't dense or rewarding, and the "invincible" enemies were only killable by parrying | **The Hush:** a hearing stalker you sneak past in Dead calm, with a lifting grab. **But it dies to normal play** (parry, stance break, a full kill), and the Withered Forest is dense (22 kit POIs and 6 story sites) so stealth stretches pay. No unkillable enemies |
| **Dark Souls, Manus and the Chasm of the Abyss** ([Fextralife](https://darksouls.wiki.fextralife.com/manus,+father+of+the+abyss)) | A pitch-dark arena where a huge, gaunt, long-armed figure runs a 6-hit combo and dark-orb barrages (whose homing we never copy). The Silver Pendant counter rewards the player who explored | **The long-armed silhouette** and **a counter you find in the world:** the Choir's voice pillars ring to crack Vorhollow's shell for anyone, while Arcane does it best (the opposite rule). Nothing homes |
| **Dragon's Dogma 2, the Unmoored World** ([PowerPyx](https://www.powerpyx.com/dragons-dogma-2-how-to-reach-unmoored-world/), [PC Gamer](https://pcgamer.com/games/rpg/dragons-dogma-2s-true-ending-is-a-high-point-for-rpg-climaxes)) | The endgame remakes the known world: a blood-red sky, the sea pulled into a maelstrom, five strong bosses at light beams across the map, harsher death. The endgame *feels* like the world changed | **R10 opens every pass at once,** and the Void's events reach the rest of the map: the Rift Storm, the black-violet beacon on the Ring, and the Severance invasions. The endgame is a changed world you walk into, not a menu |
| **Ark Extinction, Element Node defence** ([ark.wiki.gg](https://ark.wiki.gg/wiki/Element_Node_(Extinction))) | Corrupted creatures attack a node in waves, and the reward scales with how many nodes survive. Its known flaw was waves getting lost and wandering | **The Rift Storm:** temporary rifts spit waves at the nearest claim within 1.5 km. Each rift has a **rift heart** to break, and the payout is voidglass ×3 per closed rift. Waves walk a navmesh path to a fixed target picked at spawn, so they never wander; a wave without a target walks to the nearest shrine |
| **V Rising, the Cursed Forest** ([PC Gamer](https://www.pcgamer.com/games/survival-crafting/v-risings-big-free-content-update-introduces-a-cursed-poisoned-forest-this-week/), [Progameguides](https://progameguides.com/v-rising/all-debuffs-in-v-rising-and-how-to-deal-with-them)) | A stacking debuff zone answered by a crafted cape from that zone's boss. A biome that asks you to come back geared | **The gear check loops through the biome:** voidglass from rift edges makes the nullward that halves rift drain, letting you mine deeper rift rings. That's the V Rising loop |
| **Valheim, the Mistlands** ([PC Gamer](https://pcgamer.com/valheim-wisp-fountain-torches-blue-lights-mistlands), [Gamever](https://gamever.io/knowledge-base/wisps-and-the-wisplight-in-valheim-one-silver-one-wisp-ten-metres-of-sight)) | Permanent fog with a crafted light that clears 10 m. It's praised for its tension but criticised as tedious because it never lets up | **Lensing and silence come in states, never permanent:** Dead calm is 30% of the weather, not all of it. Light bends only near rifts. Most of the wedge reads clear to 1 km so the skyline landmarks guide you |
| **STALKER, anomalies, bolts and detectors** ([STALKER wiki: Bolt](https://stalker.fandom.com/wiki/Bolt), [STALKER 2 support](https://support.stalker2.com/hc/en-us/articles/29871997411473)) | Invisible gravity anomalies are found by throwing a bolt and watching its path. Artifacts form at anomalies, and a detector beeps faster as you near one. The world is read by poking it | **Thrown stones read the Void:** a stone thrown (the combat lane's X on a loose stone) bends toward a rift's pull, cracks hollow ground and shows the drop, and floats where a rift zone begins. **The Driftmote** is our detector: it drifts toward rifts, unseen rift seeds and caches, pulsing faster as it nears |
| **Monster Hunter, Gore Magala** ([monsterhunterwiki](https://monsterhunterwiki.org/wiki/Gore_Magala_(MHWilds))) | An eyeless dragon that senses through scales, and its "frenzy" grows the longer the fight runs. A blind monster with a legible sense, which the player can play against | **Blind hunters with readable senses:** the Hush hears, and its Listening rise is the tell. Hollowfangs have no eyes and track by scent trail and sound. The Riftmaw hunts from rifts and fights badly away from them |
| **Dune: Awakening, the Deep Desert** ([awakening.wiki](https://awakening.wiki/The_Deep_Desert), [PC Gamer](https://pcgamer.com/games/mmo/dune-awakenings-latest-trailer-offers-a-glimpse-of-its-massive-coriolis-storms-which-reshape-swathes-of-the-map-each-week-for-infinite-exploration)) | An endgame zone where the best resources sit beside the worst danger, reset by storms | **The richest voidglass sits in the worst rift rings,** and the Rift Storm re-seeds rings across the wedge every event. No base wipes (Raven's "no wipes" rule wins) |
| **Rust and Ark, endgame loops** (CAL S-sources) | Hard places pay rare materials that every top recipe needs | **Mythic gear needs all 12 signature resources,** so the endgame sends a group back across the whole map. Voidglass is the one only the endgame wedge gives |

### 1.2 The real world we build from

| Reference | What's true | Where it lands |
|---|---|---|
| **Ginnungagap**, the yawning void of Norse creation ([Mythopedia](https://mythopedia.com/topics/ginnungagap/), [EBSCO](https://www.ebsco.com/research-starters/religion-and-philosophy/norse-creation-cosmos)) | The gap between Niflheim's ice and Muspelheim's fire, where Ymir the first giant was born. Fire-frost already lays the Reach out as a fallen giant (the Ymir motif) | The Void is **the giant's wound**: the gap the giant came out of, opened again where his legs were split. The Great Rift is the cut, and **the Broken Knee**, a cleanly cut femur ridge, stands in the Rift Fields. No one says it |
| **Rahu**, the severed eclipse head ([National Geographic](https://www.nationalgeographic.com/science/article/131101-solar-eclipse-myth-legend-space-science), [Madras Courier](https://madrascourier.com/insight/of-mythical-demons-rare-celestial-phenomena/)) | Rahu swallows the sun, but having no throat, the sun falls out of the bottom of his head | **Vorhollow's open chest:** what it swallows falls out through a hole in its ribs. Its grab drops you out behind it, and in hidden phase 3 you go in through that hole |
| **Bakunawa**, the moon-eating serpent ([Aswang Project](https://www.aswangproject.com/bakunawa/), [Wikipedia](https://en.wikipedia.org/wiki/Bakunawa)) | Bakunawa swallowed six of seven moons. People banged drums and shouted until it spat out the last | **Noise drives back the dark:** the Choir sang to hold the rifts shut. In the boss fight, striking a **voice pillar** rings it, and a ringing pillar cracks Vorhollow's shell |
| **Ħal Saflieni Hypogeum**, Malta ([Wikipedia](https://en.wikipedia.org/wiki/%C4%A6al_Saflieni_Hypogeum)) | A rock-cut underground temple whose "Oracle Room" resonates a deep male voice through the whole complex | **The Choir's architecture is acoustic:** horn-shaped towers, stepped amphitheatres facing the rifts, hollow fluted columns that moan in the Grey wind. The Undercroft's halls carry a low hum |
| **Sky burial** ([Wikipedia](https://en.wikipedia.org/wiki/Sky_burial)) | Giving the dead to the open sky rather than the ground | **The Choir gave their dead to the rifts:** seated rows at rift edges, facing in, half drawn into the tear. The point-down cairns are offerings into the earth |
| **Chernobyl's Red Forest** ([Smithsonian](https://www.smithsonianmag.com/science-nature/forests-around-chernobyl-arent-decaying-properly-180950075/), [Live Science](https://www.livescience.com/44318-chernobyl-trees-barely-decomposed.html)) | Near the reactor, dead trees and leaf litter barely decompose because microbes and fungi are suppressed. The litter piles up like kindling, a real fire risk | **The Withered Forest:** dead pines that never fell, grey needles that never rotted, litter knee-deep. **The litter burns:** any fire hit lights it, and a burn front runs downwind. Nothing rots near the void, so greywood is a prized, rot-proof timber |
| **Trinitite**, the Trinity test glass ([Wikipedia](https://en.wikipedia.org/wiki/Trinitite)) | Desert sand fused into a thin glass sheet out to about 350 m from ground zero, mostly bottle green, with rare black and red pieces | **Voidglass sheets:** each rift lays a 4 to 8 m ring of black fused glass. **The Glass Sea** is a 300 m sheet where a rift collapsed. Voidglass is black with a violet edge, the rare black trinitite as a world rule |
| **Gravitational lensing** ([Wikipedia](https://en.wikipedia.org/wiki/Gravitational_lens)) | Mass bends light into arcs and rings | **Light bends near rifts:** a screen-space distortion ring at each tear's edge, an Einstein-ring halo over the Great Rift (the 400 m column), and lensed starfields inside every rift |
| **Aye-aye** ([Wikipedia](https://en.wikipedia.org/wiki/Aye-aye)) | A primate with a long, skeletal middle finger it taps on wood to find hollows by sound, and huge mobile ears | **The Hush** taps the ground and listens with huge ear fans. **Vorhollow's** forelimbs end in aye-aye fingers with voidglass hooks |
| **Naked mole-rat** ([Wikipedia](https://en.wikipedia.org/wiki/Naked_mole-rat)) | A wrinkled, hairless, nearly blind eusocial digger with chisel incisors outside its lips | **The Gnawer** |
| **Sea lamprey and hagfish** ([Lamprey](https://en.wikipedia.org/wiki/Lamprey), [Hagfish](https://en.wikipedia.org/wiki/Hagfish)) | A jawless ring mouth of hooked teeth that latches and rasps | **The Riftmaw's** four-jaw mouth and **Vorhollow's** head |
| **Black Shuck** ([Wikipedia](https://en.wikipedia.org/wiki/Black_Shuck)) | East Anglia's spectral black dog, the omen hound | **Hollowfang's** silhouette (lean, black-grey, eyeless) and its silence |
| **Tuvan throat singing** ([Wikipedia](https://en.wikipedia.org/wiki/Tuvan_throat_singing)) | One singer makes a drone plus a whistling overtone | **The Hollow Choir's motif** in the music: a low male choir with overtones, recorded, then played reversed |
| **Penderecki, *Threnody*** ([Wikipedia](https://en.wikipedia.org/wiki/Threnody_to_the_Victims_of_Hiroshima)) and the **Shepard tone** ([Wikipedia](https://en.wikipedia.org/wiki/Shepard_tone)) | Tone clusters, glissandi and extended string techniques; an endlessly falling pitch illusion | **The Wasteland score:** string clusters, a falling Shepard line under Vorhollow's Inhale, reversed bowing |
| **Annihilation, the Shimmer score** ([Wikipedia](https://en.wikipedia.org/wiki/Annihilation_(soundtrack)), [Slate](https://www.slate.com/culture/2018/02/annihilation-co-composer-ben-salisbury-explains-the-musical-cue-from-the-alien.html)) | The composers led the Shimmer with sound design, not a theme, and saved one huge cue for the climax | **The explore pieces lean on texture,** and **the boss gets the one real theme**: the Choir motif turned inside out |
| **Lyrebird** ([Wikipedia](https://en.wikipedia.org/wiki/Superb_lyrebird)) | The world's great mimic: chainsaws, camera shutters, other birds | **The Echo** is built on the lyrebird's display tail and its mimicry, and its voice is lyrebird calls |

### 1.3 Lessons, short

1. **One currency of pain.** Rifts, Gnawers, Vorhollow and your own spells all take from `MaxHealthLost`, shown as one grey band and restored by Rest. Players learn it once.
2. **The world tells you where the void is.** Pebbles float inside a rift zone, dust streams toward a rift's pull, thrown stones bend, the Driftmote pulses, and light lenses at tears. No floor paint.
3. **Blind hunters are fair when their sense is readable** (the Hush's rise, the Hollowfang's breath, the Riftmaw's rift edge).
4. **Every counter works for everyone, and the opposite works best:** voice pillars and anchor salt for all, Arcane ×3.
5. **Silence is a state, not the biome:** Dead calm 30%, music always on otherwise.
6. **Dense, not long:** 138 POIs on 6 km², and stealth stretches pay.
7. **The endgame is a changed world:** all passes open, the Void's events reach out, and mythic recipes send you everywhere.
8. **Never home, never unkillable, never a helper after a death.**

---
## 2. Design

### 2.0 Scope map (every FULL-GAME row for this biome)

| FULL-GAME row | Tag there | Here |
|---|---|---|
| Path theme "big power that costs max HP" (DD) | RAVEN-PLANNED | SPEC, kept (2.17) |
| Corruption builds here as in the graveyard (DD) | RAVEN-PLANNED | SPEC; numbers KEPT (survival-crafting 2.5) |
| Look | GAP | GAP-COOKED (2.1) |
| Sub-zones: rifts, withered forest, void-scarred ruins, gauntlet road (CAL) | GAP | 4 names KEPT, plus the Grey Strand; GAP-COOKED (2.2) |
| Signature resource: voidglass from rift edges | GAP | Premise KEPT; GAP-COOKED (2.6) |
| Hazard: rift zones drain max HP until rest; Empty vessel or nullward passes | GAP | Numbers KEPT (3.3: −2/s; nullward halves; anchor salt halves); micro-hazards GAP-COOKED (2.8) |
| Hidden sub-biome: the Unmade Place | GAP | Name and place KEPT (world-map); design GAP-COOKED (2.5) |
| Event: the Rift Storm | GAP | Premise KEPT; GAP-COOKED (2.10) |
| Shrine at a rift edge; path master "someone who came back from a rift" | RAVEN-PLANNED / GAP | Premise KEPT; GAP-COOKED (2.11) |
| Music mood: reversed sounds, detuned strings, sub-bass, silence as threat (80 BPM, atonal) | GAP | KEPT; cues GAP-COOKED (2.16) |
| 8 creatures (Hollowfang, Riftmaw, Nullwing, Gnawer, Voidstrider, Riftback, Driftmote, Withergoat) | GAP | Names, roles and taming KEPT; **the Hush** added (9); **Riftborn** corrupted variants (CAL "corrupted variants"); everything else GAP-COOKED (2.12) |
| 5 minis on the gauntlet road (Unravelled, Mother of Rifts, Starving One, Null Warden, Echo roaming) | GAP | Names, counters and trophies KEPT; fights GAP-COOKED (2.13) |
| Major: Vorhollow, the Devouring Dark; P1 pulls, consumes, rift beams; P2 the island breaks; hidden P3 inside it; weak to Arcane; Void heart part | GAP (weakness SPEC rule) | Premise KEPT; body, arena and moves GAP-COOKED (2.14) |
| Void tree: Corruption, Empty vessel, Abyss / Hollow / Devour (ST) | RAVEN-PLANNED | Nodes SPEC; numbers KEPT from combat where set, the rest GAP-COOKED (2.17) |
| Body change | GAP | Hero row KEPT; driver and stages GAP-COOKED (2.18) |
| Voidglass armour set | GAP | Numbers KEPT (survival-crafting); name **Hollowplate** KEPT (hero) (2.19) |
| Invasions Arcane ↔ Void (DD) | RAVEN-PLANNED | Rules KEPT (3.3, crystal 2.22); Void side GAP-COOKED (2.22) |
| R10: raids, mythic gear, prestige, leaderboards, achievements, titles, cosmetics, monuments; every pass opens | RAVEN-PLANNED (DD Endgame) | GAP-COOKED (2.23); monuments KEPT |

### 2.1 Where it sits and how it reads

**Footprint.** The west half of B6, from the Graveyard border (y +4,600) to the south sea (y +6,400). It runs from the west coast (x −2,950) to the Spine's last stretch, whose crest runs from (−900, +4,200) to the Severance floor (+1,250, +5,400). That's about 4.2 × 1.6 km of land, or 6.0 km². The bands are measured from the Spine crest (world-map rule):

| Band | Where | What lives there |
|---|---|---|
| Lowland (900 to 1,900 m from the crest) | West and south coasts, the Withered Forest's west half | Camps, Withergoats, Gnawers, Driftmotes, Riftbacks on the strand |
| Midland (450 to 1,050 m) | The Quire, the Withered Forest's east half, the Rift Fields' west half | Nullwings, Voidstriders, Hollowfangs at night, three gauntlet stations, three caves |
| Highland (140 to 620 m) | The Rift Fields against the Spine, the Great Rift, the Severance mouth | Hollowfangs, Riftmaws, the Hush, Riftborn, the Null Gate, Vorhollow |

**The read from 1 km (the skyline).**
- **The Great Rift column:** a 400 m pillar of bent light over (+300, +5,600). It's a slowly turning Einstein ring that makes the sky behind it swim. You see it from the Graveyard's south edge and from the Crown's Ring, so it's the first thing anyone sees of the Void.
- **The Throat:** a 90 m horn-tower in the Quire.
- **The Hollow Colossus:** a 45 m singer with her chest carved out.
- **The Hanging Wood:** pines floating 20 to 40 m up.
- **The Broken Knee:** a white bone ridge.
- **The Black Beacon** on the south-west cape.

**Palette (one law: grey land, black glass, violet only at the void's edges).**

| Use | Colour |
|---|---|
| Dead ground, ash-dust | Grey #9A958C, pale grey #B9B4AC |
| Withered pine bark and needles | Silver-grey #8E8A84, needles #A39E95 |
| Choir stone (dolomite) | Bone-grey #C9C3B8 with grey veins |
| Voidglass | Black #0B0A10, edge violet #7B4FD6 (rim light only, 0.5 to 2 nits) |
| Void (inside rifts) | Black-violet #2B1840 (hero Void colour) with lensed star points #E8E4FF |
| Sea and sky | Overcast grey #7E8086; surf grey-white #D0CFCB |
| Bone (the giant) | #D9CFB8 |

**Light.** By day the sky is flat grey (Overcast 40% by weight) and the sun is small and white. There's no moon (KEPT), so at night the only lights are rift-edge glow (violet, 20 to 60 m reach), the Black Beacon's gathered light, camp fires and your own. Light bends at every rift: a screen-space refraction ring 1.5× the tear's height, which makes the horizon behind it bow.

**Sound bed.** Lower than any other biome. A dry tick of glass cooling, needles hissing, a sub-bass hum near rifts (28 to 35 Hz, felt more than heard), and a whispered inrush at every tear. There are no birds anywhere. That absence is the first thing that tells you you've crossed out of the Graveyard.

**The grand puzzle (why the Void sits where it does, and what it touches).**
- **It's the giant's wound.** The Reach is a fallen giant (world-map 2.12), the Broken End is his legs, and the Severance split them. The Great Rift is the cut, so the Void sits at the place the world was broken.
- **It touches the Graveyard (neighbour, north)** across a grey gradient. Both biomes build the same Corruption meter, and graves end mid-row where the ground goes grey. Riftborn Gravehounds, Cryptstalkers and Palestriders wander south out of the Graveyard and are changed.
- **It faces the Crystal caves (opposite, east) through the Severance.** The Wardcarvers learned to be silent. The Hollow Choir learned to sing. The Choir's champion lies as an ash cast in a crystal cage on this side, and the crystal giantess, hollowed, lies on the far side. The two biomes still use each other: ward fields throw void creatures ×2, a rift eats Lyrathis's dome, Arcane breaks Vorhollow's shell, and **anchor salt** (2.7) needs crystal dust from the caves.
- **It feeds the whole endgame.** Voidglass is the only signature resource from the R10 wedge, so mythic gear (2.23.3) can't be made until someone has walked the Void.

### 2.2 Sub-zones (CAL's four kept, plus the Grey Strand)

| # | Sub-zone | Where (m) | Area | Band | Ground | Palette | Props | Skyline piece | Creatures (day / night) | Bed |
|---|---|---|---|---|---|---|---|---|---|---|
| V1 | **The Withered Forest** | x −2,800 to −700, y +4,600 to +5,200 | 1.4 km² | Lowland → midland (east) | Knee-deep grey needle litter over pale sand; root plates; litter fire scars | Silver-grey bark, needle grey, black char where it burned | Standing dead Scots pines 12 to 22 m with their grey needles still on; leaning hawthorns, black; root plates of the few that fell (they never rotted); Choir point-down cairns; newcomer camp ruins; graves that end mid-row along the north edge | The Hanging Wood | Withergoat herds, Gnawers, Driftmotes, Voidstriders (east) / + Hollowfang packs, the Hush (east), Riftborn Gravehound | Trunk creaks, needle hiss, no birds; litter crunch underfoot (noise ×1.3, so sneaking matters) |
| V2 | **The Grey Strand** | West coast x −2,950 to −2,450, y +4,900 to +6,200; south coast y +6,050 to +6,350, x −2,450 to +900 | 1.0 km² | Lowland | Black glass sand (fine voidglass grit) on the beaches, grey salt pans behind them, shingle | Black sand, white salt crust #ECEAE4, grey surf | Newcomer shipwrecks (the camps); salt pans with rakes and salt cones; drift-bone; Choir watch posts; tide lines of inert voidglass crumbs | The Black Beacon | Riftbacks (licking salt), Withergoats, Gnawers, Driftmotes / + Hollowfangs (rare), Nullwing roosts on sea stacks | Surf with its high end cut 50%, as if heard through glass; salt crunch |
| V3 | **The Quire** (void-scarred ruins) | x −2,300 to −800, y +5,200 to +6,000 | 1.4 km² | Midland | Choir paving with chant-grooves, rubble, grey grass in the cracks | Bone-grey dolomite, voidglass inlay in doorways, violet only at the Quiet Rift | Amphitheatres facing rifts; horn towers; colonnades of hollow fluted columns; rows of seats; the seated dead at rift edges; offering bowls; floating stones over the Quiet Rift | The Throat, the Hollow Colossus | Nullwings (towers), Gnawers, Hollowfangs, Withergoats in the courts / + the Hush, Hollowfang packs | Columns moan in Grey wind (a low organ chord, A♭1 and E♭2, detuned); footsteps echo off stone |
| V4 | **The Rift Fields** | x −800 to +1,150, y +4,800 to +6,050 (Great Rift excluded) | 1.7 km² | Midland → highland | Grey dust, voidglass sheets round every rift, hollow crust over rift pockets | Black glass, violet edges, grey dust, bone | 16 of the 24 stable rifts; floating rubble (each stone keeled with voidglass); the Glass Sea; the Broken Knee; Choir offering rows; dead grass | Great Rift column, Broken Knee, Glass Sea | Riftmaws (at rifts), Hollowfangs, Nullwings, Riftborn, Voidstriders / + the Hush, more Hollowfangs; the Echo roams | Sub-hum near rifts, glass ticking, reversed gusts at tears |
| V5 | **The Gauntlet Road** | A 1.9 km processional way: the Quire's East Gate (−900, +5,650) → S1 (−550, +5,500) → S2 (−300, +5,250) → S3 (−20, +5,100) → S4 (+120, +5,330) → bridgehead (+140, +5,600) | 0.5 km² (60 to 100 m wide) | Midland → highland | Paved processional, chant-grooved, with a cairn every 30 m | Bone-grey, black gate iron, violet at the stations | Point-down cairns at double density; offering niches; four station gatehouses; the Choir's last chain bridge to the island | The Great Rift ahead the whole way | Hollowfangs and Riftborn between stations / the same + the Hush | The road hums under your feet near stations (a 55 Hz tone that rises a semitone at each station passed) |

**Blends.**
- **V1 into V3** runs 120 m: litter thins to paving and pines give way to columns.
- **V3 into V4** is the dry ravine under the Unravelling Viaduct.
- **V4's edges** carry grey grass for 80 m before the glass starts.
- **The Graveyard border** (y +4,600, KEPT) is graves that end mid-row where the ground goes grey, plus Riftborn Gravehounds. Lantern flames lean south toward the void: a small tell that the void pulls even here.

### 2.3 Landmarks (10, about 7% of 138, the world-map mix)

| # | Landmark | Where | What it is | Why it's here and what it connects to |
|---|---|---|---|---|
| L1 | **The Great Rift column** | (+300, +5,600) | A 320 m-wide hole in the ground into starless black, with the floating island at its centre 20 m below the lip and the 400 m lensed light column above | Vorhollow's arena; the giant's wound; the Unmade Place below; the end of the Gauntlet Road. It reads from the Graveyard and the Crown, so it pulls everyone south |
| L2 | **The Throat** | (−1,450, +5,600) | A 90 m stone horn-tower: a tapering cone of dolomite rings, its open mouth at the top aimed at the Great Rift, an internal stair to a singing gallery at 70 m | The Choir sang from here at the Great Rift. It's a vista (reveals 600 m), and the gallery holds lore item "Cantor's breath-stone". Nullwings roost on its outer rings |
| L3 | **The Hanging Wood** | (−1,250, +4,900) | Forty dead pines floating 20 to 40 m up over a mid-sized rift, roots trailing, each root-plate keeled with voidglass shards; a few drift a few metres on a slow 120 s cycle | Shows the floating rule (2.6) at its largest outside the Great Rift. A fallen trunk forms a ramp up to the lowest tree: the hidden cache route (X-2) |
| L4 | **The Hollow Colossus** | (−1,700, +5,450), the Quire square | A 45 m statue of a Choir singer: a woman with her head thrown back mid-note, hair loose, robe slipping off one shoulder, her chest carved out into an open hollow you can walk into | The Choir's ideal: an empty vessel that sings (Empty vessel, the root passive). A stair inside her chest leads to the Great Shrine's approach terrace (2.11) |
| L5 | **The Hollow Fort** | (+720, +5,420), the Severance's west mouth | The Choir's pass fort facing east: two curved horn-walls, arrow slits aimed into the gorge, racks of iron tuning-staves | Pass kit (world-map): fort, lesser shrine, mixed creatures, no claims. The Severance invasions arrive here |
| L6 | **The Unravelling Viaduct** | (−600, +5,850) | A five-span stone viaduct over a dry ravine. Its two middle spans are broken, and their stones hang in the air frozen mid-fall, 2 to 8 m below the gap, slowly turning on keels | The V3 to V4 crossing. Walk the hanging stones as a jump route (gaps 1.5 to 2.5 m), or go down into the ravine and up. It shows that voidglass keels hold stone up |
| L7 | **The Ash-cast cage** | (+880, +5,380), inside the gorge on the Void side | A Wardcarver crystal cage (eight crystal bars, hexafoil-carved) round a grey ash cast of the Choir's champion, **the Cantor**: a tall, long-armed figure whose ash still holds the shape of an open mouth | KEPT (world-map). The Severance raid's Void boss rises from it (2.23.2) |
| L8 | **The Broken Knee** | (−200, +5,750) | A 60 m ridge of white giant bone: the end of a femur, sheared flat on its east side as if cut | The giant motif: the legs were cut here. The sheared face has the same voidglass sheen as a rift ring. It's a vista with a hidden crack (X-4) |
| L9 | **The Glass Sea** | (+600, +5,900) | A 300 m disc of black fused glass where a rift collapsed long ago. It shines violet in rift glow at night, and old Choir footprints stand fused in it | The richest surface voidglass and the Echo's resting place (2.13.5). Glass underfoot rings (noise ×1.5), so the Hush hears you here |
| L10 | **The Black Beacon** | (−2,750, +6,150), the south-west cape | A Choir watch-tower crowned by a 6 m lens of polished voidglass. By day it reads as a hole in the sky's brightness; at night it holds a captured glow from the rifts and throws a violet cone 300 m out to sea | Marks the coast for the Broken Race crossing and the south camp: the Choir's lighthouse that warned ships off |

### 2.4 POIs: 138 surface (count KEPT; contents GAP-COOKED)

**Mix (fire-frost's frame):**
- 10 landmarks, 5 shrines, 3 camps, 4 cave mouths, 5 mini lairs, 30 hand-made story sites.
- 81 kit POIs placed by the dressing generator on the 150 m / 125 m rule.
- Every POI is revealed by a rise or a bend (BotW triangle rule).
- Every POI has a light or skyline shape at night: a rift glow, a fire, the beacon or a lit stone.

#### 2.4.1 Camps (positions KEPT; names and contents GAP-COOKED)

| Camp | Where | Wreck and kit | First-run note (fire-frost frame) |
|---|---|---|---|
| **Driftwrack** | (−2,550, +5,000) | The *Grey Gull*, a two-masted newcomer ship on its side, its hold a shelter; salt pan and a drying rack | "Don't sleep near the tears. Sleep here. It gives back what they take." (points at the bedroll) |
| **Saltglass** | (−2,400, +5,800) | A hull broken in two on black sand; a stone hearth built of Choir blocks | "Throw a stone before you walk. If it floats, go round." |
| **Lantern Cove** | (−1,600, +6,300) | A beached longboat under the Black Beacon's light; fish racks | "The goats are safe. The quiet days are not." |

#### 2.4.2 Caves (4; ore by depth, CF)

| Cave | Mouth | Depth | What it is | Ore and pay |
|---|---|---|---|---|
| **The Listening Wells** | V1, (−1,600, +4,750) | 40 m | Three Choir wells cut down to a shared cistern hall; the walls carved with ear-shaped niches | Iron veins; a Gnawer warren (steal-egg taming) |
| **Sea Throat** | V2, (−2,900, +5,700) | 25 m, half-flooded | A sea cave whose back wall is a voidglass seam | Voidglass (a cave seam, no rift, so no drain), onyx; a Null eel pool |
| **The Undercroft** | V3, (−1,500, +5,700), under the Throat | 70 m | The Choir's cellars: barrel vaults, rows of seated dead in niches, a chamber that hums at 110 Hz when its gong is struck | Rich iron, onyx deep; lore item "Seat tally"; the silent bell (X-6); a Hush lair at the bottom |
| **The Hollow Ground** | V4, (+400, +5,000) | 90 m | Rift pockets under the fields: hollow crust above, caverns with floating stones and two small rifts below | Rich iron and onyx at the bottom; voidglass; a Riftmaw nest; star iron in an old meteor bowl |

#### 2.4.3 Hand-made story sites (30; each tells one thing with no words)

| # | Site | Sub-zone, where | What you see | Pays |
|---|---|---|---|---|
| S-1 | **Graves that stop** | V1, (−1,900, +4,640) | A Lantern Mourner grave row (Graveyard stone, tōrō lantern) that ends mid-row. The last three graves are open and empty, the soil drawn south in streaks | Lore item "Unfinished row" |
| S-2 | **The first hut** | V1, (−2,300, +4,800) | A newcomer's lean-to of greywood, a cold fire, a bedroll, and a circle of thrown stones round a small rift, each stone floating a hand's width up | Maren's early note "Count the stones" (2.11) |
| S-3 | **The burned stand** | V1, (−1,500, +5,050) | 200 m of pines burned to black spikes; a melted bronze lantern at its upwind edge | Teaches the litter-fire hazard; charcoal ×10 |
| S-4 | **Goatherd's shelter** | V1, (−2,600, +4,700) | A Choir shepherd's hut beside a pen of Withergoat bones, the gate left open | Withergoat taming tip carved on the gatepost (a goat eating grain) |
| S-5 | **Hollowfang kill** | V1, (−1,100, +4,700) | A living set-piece: a Hollowfang pack on a Withergoat at dusk | A fight, or the carcass if you wait |
| S-6 | **The floating cairn field** | V1, (−900, +5,100) | Thirty point-down cairns over a small rift, their top stones risen and hanging above them | Voidglass ×4 from the keels |
| S-7 | **The wreck of the *Grey Gull*** | V2, Driftwrack | See 2.4.1 | Camp kit |
| S-8 | **Salt workers' rest** | V2, (−2,700, +5,300) | Choir salt pans with wooden rakes leaning on a cone of grey salt, and three seated dead facing the sea, not the rifts | Grey salt ×20; lore item "Salt rake tally" |
| S-9 | **The Hanging Wood cache** | V1 / L3, (−1,240, +4,890) | A satchel caught in the highest floating pine's crown | Anchor salt ×3, onyx ×2 |
| S-10 | **Beacon keeper's room** | V2 / L10 | A cot, a polishing cloth, a voidglass lens shard on a stand, and ship marks scratched on the wall; the last mark is half done | Recipe scrap: Beacon lantern (2.19.2) |
| S-11 | **The drowned procession** | V2, (−600, +6,250) | A row of Choir cairns that marches straight into the sea and carries on under the surf | A sunken cache at 6 m depth: iron ×12 |
| S-12 | **The seated listeners** | V3, (−1,050, +5,330), the Quiet Rift | Twelve Choir dead seated in a curve facing the Quiet Rift, hands open in their laps | The Great Shrine's approach |
| S-13 | **Alms court** | V3, (−1,850, +5,750) | Hundreds of offering bowls stacked against a wall, every one licked clean, and great claw marks on the wall above | Foreshadows the Starving One; offering bowl decor recipe |
| S-14 | **The tuning hall** | V3, (−1,300, +5,850) | A hall of hung bronze bars of graded length; some bars are bent inward toward one point on the floor | Strike the bars low to high and a hidden door opens (X-1) |
| S-15 | **Children's choir benches** | V3, (−2,000, +5,350) | Small seats in an amphitheatre and a dropped wooden whistle | Lore item "Child's whistle" (blowing it makes a 15 m noise, and the Hush answers) |
| S-16 | **The last rehearsal** | V3, the Throat's gallery | Stone music stands with scored slates; one slate's last bar is scored entirely in rests | Lore item "Cantor's breath-stone" |
| S-17 | **The Marrow cache** | V4 / L8, (−200, +5,780) | A crack in the Broken Knee's cut face leads into the hollow marrow channel | Star iron ×3, Gnashblade blueprint (2.19.2) |
| S-18 | **Rift offering row** | V4, (+100, +4,950) | Seated Choir dead in a row at a rift's lip. The front three are half drawn into the tear, their back halves still in their seats | Voidglass ×6 |
| S-19 | **The surveyors** | V4, (−400, +5,400) | A newcomer survey camp: tripod, string lines, chalk marks, and three bodies in the line of a rift's pull, dragged until they stopped against a cairn | Recipe scrap: anchor salt (2.7) |
| S-20 | **Glass footprints** | V4 / L9 | Footprints fused into the Glass Sea, leading toward the collapsed rift's centre, and none leading back | The Echo's rest (2.13.5) |
| S-21 | **The Riftmaw slough** | V4, (+550, +5,150) | A rift ringed with gnawed bones, and a sloughed Riftmaw skin hung over a cairn | A fight; the skin (leather ×6) |
| S-22 | **Nullwing roost** | V4, (−300, +5,050) | A broken horn-tower stub, its top rings crusted with droppings and voidglass grit, and bones underneath | Nullwing bone ×3 |
| S-23 | **The last pilgrim** | V3 / L6, the viaduct | A Choir pilgrim's pack on the last standing span, and a staff hanging in the air below the break, where it fell and stopped | Climb the hanging stones for lore item "Pilgrim's staff" |
| S-24 | **Station 1** | V5, S1 | The Cantor's Stair: wide steps, a lectern, a choir of empty robes on stone hooks | Mini lair (2.13.1) |
| S-25 | **Station 2** | V5, S2 | The Rift Pens: six stone pens, each round a small tear, gates hanging off | Mini lair (2.13.2) |
| S-26 | **Station 3** | V5, S3 | The Alms House: a long hall of bowl racks with a collapsed back wall | Mini lair (2.13.3) |
| S-27 | **Station 4** | V5, S4 | The Null Gate (2.13.4) | Mini lair |
| S-28 | **The last choir** | V5, the bridgehead (+140, +5,600) | On the bridge's first slabs, thirty dead seated in rows facing the island, mouths open, two standing at the front with tuning-staves raised | Run-back stone 1 stands 50 m behind them |
| S-29 | **Hollow Fort war rack** | V4 / L5 | Iron tuning-staves racked facing the gorge; crystal arrowheads embedded in the fort's east wall | The Severance pass story; iron ×10 |
| S-30 | **The cage's watchers** | V4 / L7 | Two Choir statues outside the crystal cage, reaching through the bars toward the ash cast, their stone hands bleached where they touch the crystal | Lore item "Cantor's name-stone" |

#### 2.4.4 Kit POIs (81, placed by the generator) and secrets

| Sub-zone | Kit POIs | Types used (world-map's kit) |
|---|---|---|
| V1 Withered Forest | 22 | Ruin (shepherd huts, cairn circles) 6, nest or den (Gnawer warrens) 3, kill set-piece 2, resource outcrop (greywood stands, small rift rings) 5, cache 2, hazard teacher (a small litter-fire scar, a small rift) 2, vista (fire towers) 2 |
| V2 Grey Strand | 14 | Wreck 4, resource outcrop (salt pans, black sand) 4, ruin (watch posts) 3, cache 2, fishing spot (rift pools behind the dunes) 1 |
| V3 The Quire | 18 | Ruin 8 (houses, small theatres, horn stubs), cache 3, nest 2 (Nullwing), resource outcrop (iron quarries) 2, hazard teacher (one rubble-drop spot) 1, vista 2 |
| V4 Rift Fields | 20 | Stable rift with a voidglass ring 8, ruin (offering rows, cairn fields) 4, kill set-piece 2 (a Riftmaw on a Voidstrider), cache 2, hazard teacher (a hollow-crust patch) 2, vista 2 |
| V5 Gauntlet Road | 7 | Offering niches with caches 4, cairn stacks (no XP) 3 |

**Secrets (6, found by reading the world, never by UI):**

| # | Secret | Clue | Reward |
|---|---|---|---|
| X-1 | The tuning hall's door | Bars bent toward one point; the S-15 whistle plays the same rising run | Hushfinger spear recipe |
| X-2 | The Hanging Wood cache | A satchel strap visible in a crown; a fallen trunk leaning up like a ramp | Anchor salt ×3, onyx ×2 |
| X-3 | The drowned procession | Cairns walking into the sea | The sunken cache |
| X-4 | The Marrow cache | A dark crack in the bone's cut face; a Driftmote pulses here | Star iron, Gnashblade blueprint |
| X-5 | The Descent lift | A winch drum with a snapped rope at the Great Rift's north lip (2.5) | A repairable shortcut to and from the Unmade Place |
| X-6 | The silent bell | A Choir bell in the Undercroft, its clapper tied off. Untie it (Interact 2 s) and ring it during Dead calm | The weather ends at once over the Quire (sound returns), and a niche opens: lore item "The clapper's knot" (Null Warden lore) |

### 2.5 Hidden sub-biome: the Unmade Place (name and place KEPT; design GAP-COOKED)

**What it is.** The pocket inside the Great Rift, 60 to 160 m below the lip: floating Choir ruins in starless black, lit only by lensed light from above. The Choir lowered their dead and their last singers here when the rift opened under their temple. It holds 8 POIs.

**How you get in (two routes, both earned).**
1. **The Choir's Descent** (north lip, (+300, +5,440)). A spiral stair cut into the rift wall, 120 m down, that ends where the rock ends. From there floating stones lead out (gaps 1.5 to 2.5 m).
2. **The Descent lift (X-5).** A Choir winch on the lip:
   - **The mechanism.** A timber drum 2 m across on an iron axle in two stone bearing blocks, a ratchet wheel with a pawl, two crank handles, and a hemp rope that runs over a cantilevered timber jib to a basket cage. The rope is snapped.
   - **Repair.** Rope ×10 and iron ×4 (hold Interact 5 s).
   - **Use.** Hold Interact on a crank: the cage moves at 1.2 m/s, the rope visibly winds on the drum, and the pawl clicks every 20 cm. The pawl holds the cage wherever you let go, so it never drops on release. It carries 4 players to the U1 ledge at −100 m.
   - **Why it's there.** It's the way back up after a deep run. It stays repaired for the server's life.

**The rules inside.**
- Rift drain is ×1.5 (−3 max HP/s). Empty vessel passes it; nullward or Hollowplate halves it, and anchor salt halves it again.
- Falling off the stones into the black kills you (Unmade). Your body appears at U1 so it can be recovered.
- Gravity is normal.
- You hear only your own sounds and the inrush. Music plays the quiet **Unmade** cue (2.16), so this isn't a fourth silence.

| # | POI | What | Pays |
|---|---|---|---|
| U1 | **The Descent's foot** | A landing ledge with the cage's rest, a Choir rope anchor and a brazier (stops corruption building within 10 m, KEPT) | A safe edge; bodies of the Unmade appear here |
| U2 | **The Seated Choir** | 200 dead seated on floating terraces in a great curve, facing one point of black, mouths open | Lore item "Choir seat 1 of 200" |
| U3 | **The pool of the Fish That Isn't** | A floating basin of black water that reflects stars that aren't there | KEPT fishing: the Fish That Isn't |
| U4 | **The Inverted Tower** | A horn-tower hanging point-down from a floating foundation; its stair walks down to its open mouth | A vista of the pocket; a cache (rune materials) |
| U5 | **The rubble garden** | A field of floating stones whose keels are voidglass shards 1 to 2 m long | The richest voidglass nodes (×2 yield) |
| U6 | **The Riftmaw drift** | A Riftmaw nest on a floating slab with three sleeping Riftmaws | The hardest Riftmaw fight; teeth |
| U7 | **The Cantor's reliquary** | A small sealed chapel on its own stone: a stone box with the Cantor's tuning-staff | Wildbrand scroll "Null strike: Cantor's variant" (a cosmetic clip) and void essence ×6 |
| U8 | **The last note** | One woman standing alone at the farthest stone's edge, dried, beautiful in profile, holding a carved stone up toward the black as if offering it | Lore item "The last note", the Choir's last line: two words |

### 2.6 Signature resource: voidglass (premise KEPT; never farmable)

**What it is.** Black fused glass that forms in a 4 to 8 m ring round every rift where the tear's edge has touched the ground. It's black with a violet edge glow in the dark.

**The floating rule (one world rule, used everywhere).**
- Voidglass within 40 m of an open rift pushes away from the ground.
- A rock with a voidglass shard grown into its underside floats; the bigger the shard, the higher it floats.
- Mined voidglass is inert, so it never floats in your pack or base.
- This one rule explains the floating rubble, the Hanging Wood, the Viaduct's hanging stones, the boss island and its pieces, and the Unmade Place.
- Every floating thing shows its keel from below.

| Source | Where | Yield | Respawn |
|---|---|---|---|
| Rift rings (pick, 4 hits per node) | 24 stable rifts, 3 to 8 nodes each (about 110) | 2 to 3 | 6 game hours |
| Floating keels (knock the stone down with a heavy or charged strike, then mine the keel) | Floating rubble near rifts | 4 | The stone rises again after 2 game hours |
| The Glass Sea | L9, 20 nodes | 3 | 6 game hours |
| The rubble garden | U5, 10 nodes | 6 | 12 game hours |
| Rift Storm rings | Event only | ×3 rate (KEPT event rule) | Gone with the event |
| Sea Throat seam | Cave, no rift (no drain), 8 nodes | 1 | 12 game hours |

Mining in a rift ring costs max HP (the zone drains), so the best voidglass costs the most. That's the loop: nullward → deeper rings → Hollowplate → the Unmade Place.

### 2.7 Other resources

| Resource | From | Used for |
|---|---|---|
| **Greywood** | Withered pines (V1), axe; never rots | Rot-proof timber: the Hollow bastion set's wood (KEPT set), bow staves, spear shafts |
| **Grey salt** | Salt pans (V2), hand-gathered, 4 per pan, 2 h respawn | Salting food (KEPT) and anchor salt |
| **Anchor salt** (KEPT item; recipe and throw GAP-COOKED) | Alchemy L3: grey salt 2, crystal dust 1, voidglass 1 → 2 bags | Eat: 5 min, rift drain ×0.5 (KEPT). **Throw** (a throwable bag): bursts in a 3 m cloud that closes a brood rift (2.13.2), takes 300 HP off a rift heart (2.10), or holds a crumbling island piece solid for 10 s (2.14). It needs crystal dust from the Crystal caves, so the opposites trade |
| **Void essence** | Void creatures (1 to 3), minis (8), Vorhollow (20) | The Void path material (CF: element) |
| **Ashen grain** (KEPT crop) | Wild stands in V1 clearings | Withergoat feed (KEPT), curse resist potion (KEPT), Driftmote taming |
| **Mandrake** (KEPT rare crop) | Wild in the Quire's courts, 6 plants | It screams when pulled: a 0.9 s shiver, then a 12 m scream that staggers you 0.8 s unless you wear wax earplugs (KEPT) |
| Iron (rich), onyx | Caves (2.4.2) | CF |
| Star iron | The Hollow Ground's old meteor bowl (3 nodes, 24 h) and Starfall (KEPT) | Hollowplate (KEPT recipe) |
| Creature parts | Hollowfang hide, Riftmaw teeth, Nullwing bone, Hush finger bone and ear membrane, Gnawer incisors, Riftback plate, Voidstrider filament | 2.19.2 |

### 2.8 Hazards

**The rift zone (numbers KEPT; behaviour GAP-COOKED).**

| Part | Rule |
|---|---|
| Size | Large rifts: zone radius 40 to 60 m (12). Small rifts: 25 m (12). The Great Rift: everything inside the lip plus 30 m round it. The Unmade Place: ×1.5 |
| Effect | Max HP −2 per second (KEPT) into `MaxHealthLost`; corruption builds at the night rate (KEPT) |
| Passes | Empty vessel (root, main path only) passes. Hollowplate or nullward halves (KEPT). They're the same effect, so they don't stack. Anchor salt halves, and it stacks with either: ×0.25 |
| How you see it | **Pebbles and dust float 10 to 40 cm off the ground inside the zone,** bobbing slowly (the floating rule at its smallest). Grass inside is grey and stands straight up. The tear's lensing ring and inrush grow as you near it. The grey band on your health bar grows while you're inside. No floor paint |
| Death | When `MaxHealthLost` reaches your max HP you die (**Unmade**). No floor. Your bag drops at the zone's edge nearest where you fell |
| Creatures | Void creatures are immune (CR rule). Off-biome tames take the drain on their own max HP |

**Micro-hazards (4, each with ≥ 0.9 s of warning).**

| Hazard | Where | Warning | Effect | Counter |
|---|---|---|---|---|
| **Rift pull** | 6 m round every tear | Dust and pebbles always stream toward the tear; a sharper inrush when you enter | Pulls 180 cm/s toward the tear. Touching the tear: 40 damage, knockback 600 out, curse 40 | Walk away (run 600 beats the pull); thrown stones show it |
| **Rubble drop** | Under floating rubble within 30 m of a large rift | Each large rift **pulses** every 40 to 90 s: the inrush stops for 1.0 s (sudden silence), the stones' shadows sharpen and grow, a grinding sound plays | The stones drop for 2 s, then rise: 45 damage, impact 3 | Step out from under a shadow during the 1.0 s |
| **Litter fire** | V1 | Any fire hit on litter (spells, burning arrows, a dropped torch, a fire mount): a 0.9 s smoulder with smoke before flame | A burn front at 1.5 m/s downwind (0.5 m/s upwind): 8 burn buildup and 3 dps per second inside. Each patch burns 60 s and leaves black ground that can't burn again for 2 game days | Fire players can use it; upwind is safe. Gnawers and Withergoats flee it; a Hollowfang pack caught in it scatters |
| **Hollow crust** | V4, the Hollow Ground's mouth, the Glass Sea's edges | A hollow drumming under your feet (heard at 6 m) and a faint grid of cracks (crystal lane's `thinFloor`) | It breaks under a heavy landing, a roll, a heavy attack's impact or a heavy creature. You drop 4 to 8 m into a rift pocket (normal fall damage); one of the two pockets holds a small rift | Throw a stone (it cracks the crust and shows the drop); walk, don't roll |

### 2.9 Weather (weights KEPT; effects GAP-COOKED where not set)

| State | Weight | Effect |
|---|---|---|
| Clear | 20 | Long sight lines; the Great Rift column reads from anywhere |
| Overcast grey | 40 | Flat light; no effect |
| Fog (dust) = **Grey wind** | 10 | The dust state *is* survival-crafting's Grey wind: sight 150 m, corruption builds ×2, your footsteps muffled 30% (creature tells always full, KEPT), the Quire's columns moan. Lasts 15 min (KEPT) |
| **Dead calm** | 30 | KEPT: no wind, no ambient sound, footsteps doubled in the mix. Hollowfangs **and the Hush** hear at ×1.5 range. **Music fades to silence over 8 s** (the planned silence) and returns over 8 s when it ends. A fight inside Dead calm brings in only the combat layer's sub-bass pulse and string clusters (2.16): silence used as threat, but fights still have music |
| Rain | 0 | Never: the Spine's rain shadow (KEPT) |

The Dimming (KEPT world event) halves corruption build in the open here too.

### 2.10 Event: the Rift Storm (premise KEPT; frame KEPT from fire-frost; details GAP-COOKED)

| Part | Rule |
|---|---|
| Frame (KEPT) | One game day (60 real min), about once every 3 real days, server setting; pays voidglass ×3 |
| World sign (5 min before) | The Great Rift's column thickens and blackens at its core, the sun dims 30%, every rift's inrush rises to a howl, and **the Void pillar on the Crown's Ring burns black-violet** (world-map beacon rule). No UI beyond the KEPT event notice |
| Rift seeds | 10 fixed seed points: 4 in V4, 2 in V3, 2 in V1, 1 in V2, and 1 on the Graveyard border at (−1,600, +4,620). A Driftmote senses a seed 60 s before it opens and drifts to it |
| What opens | 6 of the 10 seeds, chosen so that every claim within 1.5 km has a seed within 600 m where possible. Each becomes a **storm rift**: a 10 m tear with a 40 m rift zone |
| Waves | Each storm rift spits 3 waves, 4 min apart. Wave 1: a Gnawer swarm (8) and 2 Hollowfangs. Wave 2: 3 Hollowfangs and a Nullwing. Wave 3: a Riftmaw (it stays at its rift) and 2 Riftborn. Each wave picks its target at spawn (the nearest claim's Hearth or totem within 1.5 km, else the nearest shrine) and walks a navmesh path there. It never retargets to a farther base and never wanders. It attacks pieces on the way only when blocked |
| Closing a rift | Kill its live wave, then break its **rift heart**: a 1.5 m voidglass core floating at the tear's centre (HP 1,500; Arcane ×3, void ×0.5, everything else ×1; thrown anchor salt −300 each). A closed rift leaves a voidglass ring worth ×3 for the rest of the event |
| Unclosed rifts | They close at the day's end. Waves already walking keep walking. No lasting damage to the land |
| Pay | Voidglass ×3 at every ring (KEPT); 1,500 path XP per closed rift, split across everyone who damaged its heart (to the Void path if owned, else 50% to each owned path) |
| Reach | Only the border seed reaches the Graveyard, so Graveyard bases near the border learn to fear the Storm |

### 2.11 Shrines and the path master

**Shrines (frame KEPT).**

| Shrine | Where | Guards (2 packs, one midland) | Look |
|---|---|---|---|
| **Great Shrine: the Shrine of the Edge** | V3, (−1,050, +5,350), at the **Quiet Rift**: a small, old, steady rift ringed by S-12's seated listeners | 3 Hollowfangs + a Gnawer swarm | Reached through the Hollow Colossus's chest, whose inner stair leads out to a terrace at the rift's edge. The brazier is a black iron bowl on a tripod 8 m from the tear: outside its 6 m pull, inside its zone |
| Lesser: **Greywood** | V1, (−2,100, +4,850) | Day: a Withergoat herd guarded by 2 Gnawer swarms. Night: a Gnawer swarm + 2 Hollowfangs | A ring of point-down cairns round a dead pine |
| Lesser: **the Strand** | V2, (−2,600, +5,950) | 2 Gnawer swarms + a Riftback (neutral; it joins in if hit) | A Choir watch post on the dunes |
| Lesser: **Severance West** (the pass shrine) | V4, (+640, +5,470) | 2 Hollowfangs + 2 Riftborn | Beside the Hollow Fort's gate |
| Lesser: **the East Gate** | V5, (−950, +5,700), the Gauntlet Road's start | 3 Hollowfangs + a Nullwing | A horned gateway arch |

**The path master: Maren Vey, the one who came back** (premise KEPT; who GAP-COOKED).
- **Who she is.**
  - A newcomer, about 32: tall, lean and strong from salvaging wrecks, her arms sun-browned.
  - Dark hair with one streak gone white-grey from the root.
  - Her left eye is black from the centre out to the iris edge, and her left hand and forearm are grey to the elbow. It's the Void body change at full, worn as a fact, not a curse.
  - She wears a Hollowplate cuirass over a loose linen shirt open at the throat, sleeves rolled, with salvaged sailor trousers and boots.
  - She has a low voice and a dry, warm way of talking, and she looks at you a beat too long. She's attractive by any real-world measure, and the game doesn't hide it.
- **Her story, told by places only.**
  - Her hut (S-2) is the first in the Withered Forest, ringed by thrown stones.
  - She walked into the Quiet Rift and came back out. No one sees it.
  - She now lives at the Great Shrine below the Colossus: a bedroll, a lantern, a slate of counted marks.
- **How she talks (KEPT frame).** Short lines at 6 m, no camera grab:
  - "You feel lighter here. That's not a good thing."
  - "Sleep gives it back. Mostly."
  - "Don't sing near the big one."
- **Her quests (6, in the Journal).** Each pays Void path XP and one thing.

| # | Quest | Ask | Pays |
|---|---|---|---|
| 1 | **Count the stones** | Throw a stone into 3 different rift zones and watch it float | 1,000 XP |
| 2 | **What the edge gives** | Mine 10 voidglass from rift rings | 1,500 XP; nullward recipe |
| 3 | **Salt and glass** | Bring grey salt 4 and crystal dust 2 (from the Crystal caves) | 2,000 XP; anchor salt recipe |
| 4 | **The Choir's road** | Beat the Unravelled and the Mother of Rifts | 3,000 XP; Wildbrand scroll "Null strike: edge variant" (a cosmetic clip) |
| 5 | **Where it went** | Reach the last note (U8) and bring back her stone | 4,000 XP; the stone (decor: it hums in Dead calm) |
| 6 | **Empty** | Beat the Null Warden (the last-gated mini; KEPT rule: the last quest needs a late mini) | 5,000 XP; title "Hollow" (2.23.7); her lantern (decor) |

---
### 2.12 Creatures (8 names, roles and taming KEPT; the Hush added; Riftborn variants; everything else GAP-COOKED)

**Rules (all KEPT).**
- Tier numbers: lowland 70 HP / 12 hit / stance 20; midland 120 / 20 / 35; highland 200 / 32 / 55.
- Aggressive "Hard" is highland, "Medium" midland, "Easy" and passives lowland.
- Every attack is a 2.18.1 row. Damage lands only on visible contact (jaw, claw, finger, tail or the real projectile), and the cue is a small glint on that part.
- Packs attack freely with no token, flank by position and breathe back to 5 to 8 m.
- Enemy projectiles never home and give ≥ 0.9 s from windup start to impact.
- Void creatures resist rift drain (CR).
- **Void creature hits carry Curse buildup** (15 on bites by default). Curse is the status the Void set and the curse potion answer (KEPT), so the gear loop closes.
- Sparks: void = an inward ink smear (KEPT). Fleshy hits add dark-grey blood mist.

#### 2.12.1 The nine at a glance

| Creature | Behaviour, tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming (KEPT) | Drops |
|---|---|---|---|---|---|---|---|
| **Hollowfang** | Aggressive pack hunter, highland, 3 to 5 | Black Shuck at greyhound-plus size: deep chest, tucked waist, long forelegs; a hairless hide so tight the ribs and spine show; an eyeless elongated skull with two deep pits where eyes would be, and black glassy teeth past the lips | A lean, long-legged hound with no eyes and a mouth of black glass, its outline smearing when it blinks | Hide grey-black #2E2C2E, spine ridge ash #6E6A66, teeth voidglass #0B0A10 with violet glints, eye pits black-violet #2B1840 | 1.3 m at the shoulder | Knockout and feed, or Void path magic. Hard | Hollowfang hide ×2, raw meat ×3, void essence ×1 |
| **Riftmaw** | Aggressive ambusher, highland, 1 (3 at U6) | Sea lamprey ring mouth split into four hooked jaw-plates; crocodile forelimbs and belly armour; a tapering eel tail that stays in the rift | A huge ring of hooked teeth that blooms open out of a tear in the air, on a thick grey body that never fully leaves it | Hide wet grey #4A4848 with a pale belly #A8A39A, jaw-plates bone #D9CFB8, teeth black glass | 4.5 m long (2.5 m out of the rift when bursting) | Knockout and feed. Hard (knocked out, it falls fully out of its rift) | Riftmaw tooth ×3, leather ×4, void essence ×2 |
| **Nullwing** | Aggressive flyer, midland, 1 to 2 | Lammergeier frame and bare neck; fruit-bat wing membranes; a toothed beak; its torn membranes show the void through the holes (the holes lens light) | A gaunt long-necked vulture on torn bat wings, with sky-through-the-holes in its wings | Body ash grey #8E8A84, membranes dark grey with violet edges #4B3A5E at the holes, beak black | 5 m wingspan | Knockout and feed (bring it down). Medium | Nullwing bone ×2, membrane ×2, raw meat ×2 |
| **Gnawer** | Aggressive swarm, lowland, 6 to 12 | Naked mole-rat (wrinkled, hairless, tiny eyes, chisel incisors outside the lips) at 40 cm | A pale wrinkled grub with two long orange-yellow teeth, pouring out of a warren in dozens | Skin pink-grey #B8A8A0, incisors yellow #C8A040 | 40 cm | Steal an egg (warren nests). Easy | Gnawer incisor ×1 (arrowheads) |
| **The Hush** (new) | Aggressive stalker, highland, 1 (2 at night) | Aye-aye: huge mobile ears, a long skeletal tapping finger; scaled to 3.2 m with digitigrade legs and an emaciated, knuckle-walking stance; eyeless | A tall starved shape that rises from all fours, unfolding two huge ear fans either side of a bald eyeless head, one finger much longer than the rest | Skin pale grey #B4AFA8 with darker joints, ear membranes veined pink-grey #C8B0B0, nails black | 3.2 m upright, 1.6 m on all fours | Knockout and feed. Hard | Hush finger bone ×1, ear membrane ×2, void essence ×1 |
| **Voidstrider** | Neutral mount, midland, herd 2 to 3 | A gaunt Akhal-Teke horse at 2.4 m, its face hairless and skull-tight; a mane and tail of fine black filaments that drift as if underwater | A very tall thin horse whose black mane floats upward round its head | Coat ash-grey #9A958C, face pale #C9C3B8, filaments black with violet tips | 2.4 m at the withers | Knockout and feed. Hard | Voidstrider filament ×3 (bowstrings), hide ×3, raw meat ×6 |
| **Riftback** | Neutral hauler, midland, 1 to 2 | Glyptodont (giant armadillo) body and domed shell; a sloth's long-clawed forelimbs; the shell's top is split by a lens-shaped hollow where space folds (you see deeper than the shell is) | A huge domed beast with a black glass seam down its back that you see too far into | Shell plates grey #6E6A66 edged with voidglass, skin pale grey, seam black-violet with star points | 3.8 m long, 2.0 m tall | Offer voidglass. Medium | Riftback plate ×2 (heavy shield plate), raw meat ×8 |
| **Driftmote** | Passive utility, lowland, 1 to 3 | Moon jelly bell; comb-jelly light rows; trailing filaments; floats 1 to 3 m up on voidglass grit in its bell (the floating rule) | A translucent grey bell drifting at head height, a dark seed at its heart, light rippling down its sides | Bell clear grey, light rows pale violet #B8A8F0, seed black | 60 cm bell, 2 m filaments | Offer food (ashen grain). Easy | Mote gel ×1 (alchemy) |
| **Withergoat** | Passive farm, lowland, herd 3 to 6 | A Valais-type goat with long drooping ears and four horns (Jacob sheep's multi-horn), a dull grey coat that sheds like ash | A long-eared four-horned grey goat | Coat ash grey #A39E95, horns black, eyes gold with goat pupils | 0.9 m | Offer food. Easy | Milk, hide (KEPT farm), raw meat ×2 |

**Path magic on the Hollowfang (Raven's "calm a creature of the same element" frame).** On a Hollowfang that's knocked out or below 30% HP, hold Interact 4 s with your Void bar at 50 or more. The bar empties into it (black threads from your hand to its skull), which counts as a full narcotic dose, and taming continues as knockout and feed. It needs 10+ Void points.

**Utilities and mounts.**
- **Voidstrider (mount):**
  - Sprint 1,350 cm/s, stamina 200 (KEPT band).
  - **Blink 5 m every 8 s** (KEPT): press Dodge while mounted. The rider's i-frames cover the 0.18 s travel.
  - It walks rift zones unhurt; the rider still takes the drain unless passed.
  - It shies from ward fields.
- **Riftback (hauler):**
  - Carries 250.
  - **Its pocket opens your clan's Rift chest network** (KEPT Rift chest: 60 shared slots). Interact on a tamed Riftback to reach the same storage as every Rift chest your clan owns. It's an ender-chest on legs: why the creature exists, and what the pocket connects to.
  - It folds into its shell when hit with 25% of its stance inside 2 s (3 s, damage ×0.2).
- **Driftmote (utility):**
  - Senses rifts, hidden caches, hollow crust and Rift Storm seeds (60 s early) within 30 m.
  - It drifts toward the nearest one and pulses faster as it closes in: 0.5 Hz at 30 m, up to 3 Hz at 3 m (the STALKER detector).
  - At base it hangs over the Hearth and flashes when a Rift Storm is coming.
- **Withergoat (farm):** KEPT produce. A herd at base eats ashen grain. They flee litter fire.

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

| Creature (tier) | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Hollowfang** (high, pack 3 to 5) | Bite | 0.40 / 0 / 0.15 / 0.70 | 0.9 | 30 (2) | parryable, curse 15 | 40% → Bite 2 |
| | Bite 2 | 0.36 / 0 / 0.15 / 0.70 | 0.9 | 30 (2) | parryable, curse 15 | — |
| | Lunge (300 to 700 cm) | 0.60 / 0.3 / 0.25 / 0.80 | 1.4 | 45 (4) | parryable, curse 20 | Flankers lunge from 90 to 150° |
| | **Blink** (movement) | Tell 0.50: its body shudders and it draws one hard breath (a sharp reversed inhale); a lensing ripple appears at the destination 0.5 s before it arrives | — | — | — | 4 to 6 m, to 90 to 150° off your facing; cooldown 6 s. **Its next attack still plays its full windup** (tells always) |
| | Hamstring | 0.45 / 0 / 0.15 / 0.70 | 1.0 | 25 (2) | `behindOnly`, `low`, slows ×0.8 for 2 s | — |
| | readsAction | Lunge with no hold within 0.4 s of a drink or cast | | | | |
| **Riftmaw** (high, 1) | **Rift burst** (opener) | 0.90: the rift's edge bulges, the teeth ring shows inside, a sucking gasp / 0 / 0.30 / 1.2 | 1.6 | 55 (5) | blockable, curse 25 | From any rift within 8 m |
| | **Four-jaw grab** | 0.95: the jaw-plates flare open, a crimson glint on the tooth ring / 0 / 0.25 / 1.4 on a miss | Catch: 1.5 | — | `unblockable`, `grab`, curse 40 | Catch 1.4 s paired; roll through it |
| | Jaw snap | 0.42 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | parryable, curse 15 | 50% → Body lash |
| | Body lash | 0.55 / 0 / 0.25 / 0.80 | 1.1 | 40 (4) | 160°, r 600 | — |
| | Retreat | 1.5 s back into its rift at 25% HP or after 3 attacks | — | — | — | Returns 6 s later through any rift within 30 m (opener). **More than 30 m from any rift, it fights on land at move ×0.6 and can't retreat:** luring it away is the skill |
| **Nullwing** (mid, flyer) | Dive | 0.80: screech, wings fold / 0.2 / 0.30 / 1.5 grounded | 1.4 | 40 (4) | parryable | Grounded 1.5 s = the bring-down window |
| | **Trail pass** | 0.70: it banks wide and its wing holes flare | — | — | Lays a void trail: a 30 m ribbon at 1 to 4 m height, 1 m thick, black with a lensing edge, lasting 6 s. Touching it: 8 damage a tick (every 0.5 s) and curse 30/s | It's laid across your route, never inside 3 m of you. A placed hazard, not a projectile |
| | Void spit (≥ 500 cm) | 0.75 / 0 / — / 0.60 | 0.8 | 10 (1) | projectile 1,200 cm/s, curse 25 | ≥ 1.17 s total at minimum range |
| | Beak stab (grounded) | 0.45 / 0 / 0.15 / 0.70 | 1.0 | 30 (2) | parryable | — |
| | Wing buffet | 0.45 / 0 / 0.20 / 0.70 | 0.7 | 25 (2) | push 300 | Close range |
| **Gnawer** (low, swarm 6 to 12, 25 HP each) | Bite | 0.45 / 0 / 0.12 / 0.80 | 0.5 | 10 (1) | — | — |
| | **Latch** | 0.50: it crouches and chitters / 0 / 1.0 latched / 0.80 | 3 damage per second | — | Each latched Gnawer **drains 3 max HP per second** into `MaxHealthLost` | A roll or a hit shakes off every latched Gnawer. They eat you, the Void's one currency of pain |
| | Gnaw (structures) | — | Wood pieces ×1.5 | — | — | In creature raids and Rift Storm waves |
| **The Hush** (high, 1) | **Listening rise** (pose, not an attack) | 1.2 s: it rises to full height, its ear fans unfold with a leathery snap, and it taps the ground twice with its long finger | — | — | — | While risen it hears at ×2 range. It's your cue to stop, crouch or move behind sound cover |
| | Finger lance (reach 400 cm) | 0.70 / 0.5 / 0.15 / 0.80 | 1.5 | 50 (4) | pierce, parryable, curse 20 | — |
| | Rake ×2 | 0.45, 0.40 / 0 / 0.20 / 0.70 | 0.9 each | 30 (3) | parryable | 30% → Lift |
| | **Lift** (grab) | 1.00: it opens both arms wide, a crimson glint on the long finger / 0 / 0.25 / 1.2 on a miss | Catch: 1.4 + stagger + 40 stamina | — | `unblockable`, `grab` | Catch 1.6 s: it lifts you to its face, folds its ears over you and screams |
| | **Scream** (r 1,200) | 1.0: ear fans flare, chest heaves / 0 / 0.4 / 1.0 | 0 | — | Drains 50 stamina, **cancels a drink or cast in progress**; guarding halves it | Every 14 s at most |
| | Knuckle charge (line 1,500) | 0.80 / 0 / 0.8 run / 1.2 | 1.3 | 45 (5) | knockback 600 | — |
| | Senses | Blind. It hunts by noise through `UWRNoiseSubsystem` (KEPT, crystal). Crouch-walking is 0 noise; litter ×1.3; glass ×1.5; Dead calm ×1.5 hearing | | | | |
| **Voidstrider** (mid, neutral) | Charge | 0.90: hoof scrape and its mane lifts / 0 / 1.0 run / 1.3 | 1.6 | 45 (5) | knockback 900; a wall hit stuns it 2.5 s | — |
| | Blink kick | Tell 0.50: a lensing ripple behind you; then a rear kick 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | `behindOnly` | — |
| | Rear stomp | 0.70 / 0.2 / 0.20 / 1.0 | 1.3 | 40 (4) | `low`, r 300 | — |
| **Riftback** (mid, neutral) | Tail club | 0.70 / 0 / 0.25 / 0.90 | 1.3 | 45 (4) | 180° behind | — |
| | Shell bash | 0.65 / 0 / 0.25 / 0.90 | 1.2 | 45 (4) | blockable | — |
| | Stomp | 1.00 / 0 / 0.20 / 1.20 | 1.5 | 40 (4) | `low` | — |
| **Driftmote** (low, passive) | Pulse (when hit) | 0.60 / 0 / 0.30 / — | 0 | — | A 1.5 s vignette dim; it then rises 10 m and drifts off | — |
| **Withergoat** (low, passive) | Headbutt (cornered or hit) | 0.55 / 0 / 0.20 / 0.90 | 1.0 | 30 (3) | — | Then flees |

**Stance and resist.**
- Stance: tier values (Hollowfang, Riftmaw and Hush 55; Nullwing, Voidstrider and Riftback 35; Gnawer, Driftmote and Withergoat 20).
- `physResist`: Riftback shell strike 1.2 / slash 0.8. Riftmaw belly armour slash 0.9. Hollowfang and Hush hide 1.0.
- Lock points: Hollowfang head and chest; Riftmaw jaw-ring and body; Hush head and chest (its ears are part of the head and take strike ×1.5 head daze); Nullwing head, body and both wings (each wing a part with stance 15: breaking one grounds it for 4 s); Riftback head and shell.

#### 2.12.3 Day and night tables (`spawn_regions.json` with `night`)

| Region | Day | Night |
|---|---|---|
| V1 Withered Forest | Withergoat herds 40, Gnawer swarms 25, Driftmote 15, Voidstrider herd (east) 10, Riftborn Gravehound (north edge) 10 | Hollowfang packs 35, Gnawer 25, the Hush (east half) 10, Riftborn Gravehound 15, Withergoat 15 |
| V2 Grey Strand | Withergoat 30, Riftback 25, Gnawer 25, Driftmote 20 | Gnawer 35, Withergoat 25, Hollowfang (pack of 3) 15, Driftmote 25; Nullwings roost on the stacks |
| V3 The Quire | Nullwing 30, Gnawer 25, Hollowfang 20, Withergoat 15, Driftmote 10 | Hollowfang packs 35, the Hush 20, Gnawer 25, Nullwing (roosting) 20 |
| V4 Rift Fields | Riftmaw (one per large rift) fixed; Hollowfang packs 30, Nullwing 25, Riftborn 20, Voidstrider 15, Riftback 10 | Hollowfang packs 40, the Hush 25, Riftborn 25, Nullwing 10; Riftmaws fixed |
| V5 Gauntlet Road | Hollowfang packs 50, Riftborn 50 (between stations) | Same + the Hush 1 per segment |

Underground (caves) uses the night table at all times (KEPT rule).

#### 2.12.4 Taming notes (methods KEPT)
- Every creature tames only by its listed method (DR #16).
- Knockout works on the Riftmaw only once it's out of its rift: knocked out, it falls out onto the voidglass ring.
- Riftback: offer voidglass ×3. Approach slowly (crouch); it lets you within 4 m.
- Gnawer eggs: the warren nests in the Listening Wells and in V1 warrens. Stealing one wakes the swarm.
- **No spirit summons** (CAL 1.19). Tames are the ally system.

#### 2.12.5 Riftborn: the corrupted variants (CAL Part 3 "corrupted variants", GAP-COOKED)

**What they are.** Neighbouring creatures that wandered into the rifts and came back changed: Gravehound, Cryptstalker and Palestriders from the Graveyard, and Prismbacks that crossed the Severance from the Crystal caves.

| Rule | Detail |
|---|---|
| Look | The same mesh and rig with the **Riftborn material layer**: albedo desaturated 70%, eyes or eye-sockets solid black-violet #2B1840, a thin lensing edge (screen-space refraction 2 px) on the silhouette, and a few voidglass shards grown out along the spine. Readable at a glance as "that creature, but wrong" |
| Stats | +1 tier (midland → highland numbers; highland → highland ×1.25 HP) |
| Moves | All their own rows, plus **Blink** (Hollowfang's row, cooldown 10 s), plus curse 15 on every hit |
| Where | 15% of V4 and V5 spawns, 5% elsewhere in the wedge; Riftborn Gravehounds along the Graveyard border |
| Drops | Their own drops + void essence ×1 |
| Taming | Same method as the original (CR). Offspring are born normal (the rift doesn't breed true) |
| Why | They show the void's reach without new models, and they're the Graveyard–Void and Crystal–Void contact told by bodies |

### 2.13 Mini-bosses (5; names, counters and trophies KEPT; fights GAP-COOKED)

**Rules (KEPT):** 3,000 HP solo, +80% per extra player or tame; stance 90; 2 parries to break; 4 to 5 moves plus one counter mechanic; wind-ups 0.65 to 0.9 s with punish 0.8 s; chase 65 m; respawn 4 h; trophies as listed, made into the KEPT talismans; drops upgrade shards. Each station has a run-back stone 40 to 60 m behind its approach. **The Gauntlet Road (CAL):** four stations in a line, the Echo roaming. They're the intended road to Vorhollow, though not the only way down (the Descent is a harder road through rift zones).

#### 2.13.1 The Unravelled (Station 1, the Cantor's Stair, (−550, +5,500))

| Part | Detail |
|---|---|
| Look | A 3.4 m Choir cantor whose body is **held apart**: head, ribcage, pelvis, limb segments and robe panels are separate pieces of grey bone and stone with 5 to 15 cm gaps between them, all moving together as one body (the same rule as Vorhollow's tail). Its core is a fist-sized voidglass heart inside the open ribs. It wields a 3 m iron tuning-staff with two tines. It's tall, robed and long-armed, and its face has an open singing mouth |
| Arena | A wide stair of 12 shallow steps up to a lectern platform 30 m across; empty robes hang on stone hooks along both walls |
| Rule | Hits on the body take ×0.3 (pieces part round the blow). The core can be hit only while exposed |
| Moves | **Fork sweep ×2:** 0.70 / 0.2 / 0.25, then 0.55 / 0 / 0.25 / 0.8; 45 + 45 damage; parryable. **Fork thrust:** 0.80 / 0.5 / 0.20 / 0.9; 60; parryable. **Choir note:** 0.90, its jaw opens and a dust ring forms at its feet; a sound ring travels 900 cm/s out to 12 m (`low`, jump it); 35 damage + 40 stamina. **Reform slam** (part of the counter): see below |
| **Counter mechanic (KEPT: hit the core between reforms)** | After 300 damage or any heavy-impact hit, it **unravels**. The pieces burst into a whirling column (10 damage per touch within 3 m), travel 8 to 15 m, and reform over 1.2 s. For the first 0.6 s of the reform, **the core hangs alone in the air, exposed** (core damage ×3, every hit counts ×4 stance). Then the pieces swirl inward (0.6 s, visible and loud) and snap shut: **Reform slam**, 40 damage within 3.5 m. Hit the core early, then roll out |
| Break | Stance break: it falls apart for 4 s with the core on the floor. The crit is a stamp on the core |
| Trophy (KEPT) | Unravelled thread |
| Voice | A low male choir chord with stone grinding under it; its pain is the chord cracking |

#### 2.13.2 Mother of Rifts (Station 2, the Rift Pens, (−300, +5,250))

| Part | Detail |
|---|---|
| Look | A Riftmaw matriarch, 9 m long, her front half out of a 12 m lair rift and her back studded with small tears (the brood rifts she opens). Her four jaw-plates are each the size of a door, and her belly plates are scarred |
| Arena | The six stone pens (each a fixed brood-rift socket) round a central court 40 m across with her lair rift at the north end |
| Moves | **Four-jaw bite:** 0.75 / 0.3 / 0.25 / 0.9; 70; parryable. **Body lash:** 0.65 / 0 / 0.30 / 0.8; 180°, r 800; 55. **Sink and burst:** she pulls back into the lair (1.0 s), then bursts out of an open brood rift near a player; the tell is that brood rift bulging and gasping for 1.0 s; 80 damage, knockback. **Grab:** 0.90, crimson glint on the tooth ring; unblockable; catch 1.4 s, 90 damage + curse 60 |
| **Counter mechanic (KEPT: Arcane wards close rifts)** | **Open rift** (0.8 s, she shrieks and a pen's tear widens): she opens a brood rift in one of the six pens. Each open brood rift spits a Gnawer swarm (6) or a Hollowfang every 15 s, up to 4 open at once. **Any Arcane spell or Wildbrand hit on a brood rift closes it at once** (the opposite rule). For everyone else, a brood rift has 400 HP, or a thrown anchor salt closes it. **When all open brood rifts are closed, the strain stuns her for 4 s** (stance ×2 taken): the punish window |
| Trophy (KEPT) | Rift heart |
| Voice | Crocodile bellow pitched down, an elephant-seal gurgle and glass cracking |

#### 2.13.3 The Starving One (Station 3, the Alms House, (−20, +5,100))

| Part | Detail |
|---|---|
| Look | A 5 m emaciated giant: a gorilla's knuckle-walking frame starved to skin over rib and hip, a hyena's sloping back, and a jaw that unhinges down to its chest. Its skin is grey with old wounds, its fingers are long, and its belly is collapsed |
| Arena | The long Alms House hall (60 × 20 m), bowl racks on both sides, the back wall fallen open onto the road |
| Moves | **Grasping swipe ×2:** 0.55 / 0 / 0.20, then 0.50 / 0 / 0.20 / 0.8; 45 each. **Lunge-bite:** 0.80 / 0.3 / 0.25 / 1.0; 70. **Ground grab:** 1.00, crimson glint on the fingers; unblockable; catch 1.6 s, 80 damage. **Hunger howl:** 0.90; r 10 m; drains 30 stamina |
| **Counter mechanic (KEPT: eats your buffs and grows; fight with no buffs or bait it with food)** | **Buff-eater:** every hit or grab that lands on you **removes one active buff** (brew, elixir, Rested, Barrier, self-buff spells, shields) and it grows one stack: +10% size, +10% damage, +5% speed (max 5 stacks). **Bait:** drop or throw any food item (thrown with the throw key, 15 m) and it smells it within 15 m and runs to eat it. Eating takes 2.5 s, during which every hit on it counts as a backstab for stance (×2), and each food eaten **removes one stack** |
| Trophy (KEPT) | Starving maw |
| Voice | Hyena whoop slowed, a gorilla chest-rumble, and wet chewing |

#### 2.13.4 The Null Warden (Station 4, the Null Gate, (+120, +5,330))

| Part | Detail |
|---|---|
| Look | A 4.2 m Choir gate-guard in voidglass plate (black glass plates with violet edges over grey skin), a closed helm shaped like a bell. It carries a 2 m black bronze bell on a chain (a flail and shield) in one hand and a long iron key-halberd in the other |
| Arena and gate (the machine) | A gatehouse court 35 m across before **the Null Gate**: an iron portcullis 8 m wide, hung on two chains that run up over iron pulleys in the gatehouse roof to **two counterweight stones** in stone shafts on either side. The chains are pinned by a lock bar through the warden's own halberd-key socket. When he dies, his key falls into the lock socket (it's built so the key returns to its slot), the lock bar slides, the counterweights drop 4 m down their shafts over 6 s (visible through slit windows, chains running over the pulleys), and the portcullis rises smoothly. When he respawns (4 h), he re-pins it and it lowers over 6 s. **From the bridge side**, a crank winch (hold Interact, 8 s) raises it for anyone who reached the far side another way |
| Rule (KEPT counter "cancels every spell near it; melee only") | **Null field r 12 m** round him (a faint distortion ring on the ground, the floating pebbles inside it pressed flat): every spell cast inside fizzles with a puff of grey dust (50% of the cost refunded). Projectiles entering the field drop dead. Wildbrands still work, but physically only (their element is stripped inside) |
| Moves | **Halberd sweep ×3:** 0.60 / 0 / 0.25, 0.55 / 0 / 0.25, then 0.85 / 0.3 / 0.30 / 1.0; 45, 45, 75; parryable. **Bell slam:** 0.90 / 0.3 / 0.20 / 1.1; r 500 ring (`low`, jump it); 70 + stagger. **Bell toll:** 1.2, he lifts the bell overhead; it rings a 15 m wave that strips one buff and drains 30 stamina (guarding halves it). **Charge:** 0.80; line 1,500; 65, knockback 700 |
| Break | 2 parries (the halberd's third sweep is the best parry) or stance. Crit: through the helm's open bell-mouth |
| Trophy (KEPT) | Null seal |
| Voice | No voice: only the bell, his armour's glass chime, and breath inside the helm |

#### 2.13.5 The Echo (roaming)

| Part | Detail |
|---|---|
| Look | A lanky 3.6 m biped built on a lyrebird's display tail and a raven's long beak-skull, with long clawed arms. Its tail is a 4 m fan of mirror-black plumes (obsidian-glossy) that it raises like a lyre. Its body is matte grey-black, so the tail is what you see at night: reflected rift light |
| Route | A 2.4 km loop through V4 and along V5's middle stretch (fire-frost's route component). It walks at 3 m/s, rests by day on the Glass Sea (S-20) and hunts by night |
| Moves | **Claw rake ×2:** 0.55 / 0 / 0.20, then 0.50 / 0 / 0.20 / 0.8; 45 each; parryable. **Beak stab:** 0.70 / 0.4 / 0.15 / 0.9; 70; pierce. **Tail fan buffet:** 0.65; 220° in front; 30 damage + blind 40 (from R5; until then a 1.5 s vignette dim) |
| **Counter mechanic (KEPT: copies the last path ability used on it; vary your attacks)** | **Mimic:** it stores the last path ability (spell or Wildbrand) that hit it. It fans its tail (1.0 s tell: your ability's colour and its sound play **reversed** in the plumes), then casts its own version from its beak or claws. Enemy rules apply: projectiles never home, ≥ 0.9 s total, nothing fired inside 4 m, max 1,600 cm/s; areas use the crimson glint if unblockable. Damage ×0.8 of yours. **Vary:** if the last 3 path abilities that hit it were all different, its next Mimic **fails** (the plumes spark and short): it's stunned 2.0 s (stance ×2). The same ability twice in a row lets it Mimic with no tell hold (reads action 0.4 s) |
| Trophy (KEPT) | Echo stone |
| Voice | Real lyrebird mimic calls (camera shutters, chainsaws, other birds), raven croaks slowed, and your own ability sounds reversed |

### 2.14 Major boss: Vorhollow, the Devouring Dark (premise KEPT; body, arena and fight GAP-COOKED)

#### 2.14.0 Numbers (KEPT combat and 3.3)
- **Health and stance.** 12,000 HP solo, +80% per extra player or tame. Stance 170 (a heavy body). 3 parries.
- **Phases.** Phase 2 at 50% by an unblockable transition attack. Hidden phase 3 at 25% with artifacts, damage ×1.4.
- **Weakness.** **Arcane ×1.5** (BUILT multiplier). Void damage on it ×0.75 (its own element).
- **Rules.** Chase: the arena. Projectiles max 1,600 cm/s, never home, nothing inside 4 m. At most 1 projectile or ranged area in flight in P1 and 2 in P2.
- **Hit feel.** The full hit stack (KEPT combat 2.14): hitstop 0.10 on heavies against it, 0.20 on crits, 0.30 on the kill.
- **Lock points.** Head (jaw-ring), chest hole rim, left hook, right hook, tail. The hooks and the tail are parts with stance 50; breaking one changes moves (below).

#### 2.14.1 Who it is and what it looks like

**Who (told only by the world, 2.20).** The Hollow Choir sang into the Great Rift and fed it their dead and their offerings to hold it shut. What the rift made from all it was fed is Vorhollow. It sits on the Choir's last stage, the island their final choir stood on, and eats what comes.

**Body (believable anatomy, terrifying silhouette).**
- **Size.** 8.5 m at the shoulder on all fours; 13 m when it rears; 24 m long with the tail.
- **Forelimbs.** Very long, from an aye-aye's arm, ending in three fingers. The long middle finger ends in a 1.8 m hooked voidglass claw (the "hooks").
- **Back and legs.** Short, powerful hind legs and a hyena's sloping back, so it's always looking down at you.
- **Head.** A blunt, eyeless wedge that splits into **four jaw-petals** round a lamprey ring of hooked teeth, 3 m across when open.
- **Chest.** The ribcage is **open at the front: a 4 m hole straight through the chest.** The rib ends are polished like glass, and the hole shows a lensed starfield that isn't behind it. What it swallows falls through (Rahu).
- **Tail.** **14 floating vertebrae,** each 1 to 2 m, held 20 to 40 cm apart, moving as one whip (the floating rule).
- **Skin and spines.** Grey skin cracked like dry clay (#5E5A57), black-violet in the cracks (#2B1840), and voidglass spines down the back (#0B0A10, violet edge #7B4FD6).
- **Motion.** It walks on its knuckles and hind feet with its hooks folded back. Its gait is slow, and every footfall is planted. It's shown at scale beside the hero before any behaviour is written (KEPT rule).

**The arena: the Choir's last stage (the island and its machine).**

| Part | What it is | How it meets what it touches |
|---|---|---|
| The Great Rift | A 320 m hole into black; the lip is a 4 to 8 m voidglass ring; the 400 m lensed column above | Everything inside the lip is a rift zone (2.8) |
| The island | A 72 m disc of Choir paving on a rock body, floating 20 m below the lip at the rift's centre, held up by a huge voidglass keel underneath (visible from the bridge as a black spike 30 m long) | It floats by the floating rule. It doesn't drift, because the chains hold it |
| **The four chains** | Four iron chains, links 60 cm, run from iron eyes set in the island's rim (N, E, S, W) up to four **anchor stones** on the lip, each a 6 m Choir block with a bronze eye and a cairn on top | They're taut, sag 3 m mid-span, and clink when Vorhollow moves. In the transition they snap (2.14.2): each one breaks at a visible link, the free end whips down into the black, and the anchor stone's cairn topples |
| **The chain bridge** | 124 m from the west lip (+140, +5,600) to the island's west rim: stone slabs, each 2 × 1 m, hung on two parallel iron chains with iron stirrups, swaying 0.3 m in Grey wind | The only walking route in. The last choir (S-28) sits on its first slabs |
| The stage | Six stepped stone rings (an amphitheatre) rising to a central dais 12 m across | Vorhollow sits on the dais at rest |
| **Six voice pillars** | Choir stone horns 4 m tall, 1.2 m wide, set on the third ring | Cover from the Inhale's pull and the Rift beam (3 beam hits break one). **Strike one with a heavy, charged or Wildbrand hit and it rings for 3 s.** A ringing pillar within 12 m of Vorhollow deals 150/s to its shell (2.14.2). The Choir's song still works for anyone. They're the 2.20 story in the fight |
| Run-back stones (2, KEPT) | Stone 1: 50 m behind the bridgehead on the Gauntlet Road. Stone 2: at the Choir's Descent top on the north lip | Two doors, two stones |

#### 2.14.2 Moves (template KEPT, combat 2.17.5)

**Phase 1** (6 moves: one 3-hit combo, one grab).

| Move | Phase | Tell (small cue on the part) | W / H / A / P | Damage | Flags | Notes |
|---|---|---|---|---|---|---|
| **Hook rake** (combo) | 1 | Left hook lifts, white glint on the claw tip | 0.70 / 0.2 / 0.22 / —; 0.55 / 0 / 0.22 / —; 0.90 / 0.4 / 0.25 / 1.4 | 55, 55, 90 (the third an overhead double-hook slam with a 1,200 cm/s dust wave from impact) | parryable (hits 1 and 2); third blockable | The 1.4 s punish after the slam is the main melee window. Breaking a hook (stance 50) shortens the combo to that side's single rake |
| **Lamprey lunge** (grab) | 1 | The head draws back, the petals open and a crimson glint flashes on the tooth ring; a wet inhale | 1.10 / 0 / 0.30 / 1.6 (head stuck in the floor on a miss) | Catch: 40 + **15% of your max HP into `MaxHealthLost`** | `unblockable`, `grab`, range 400 to 1,400 | Catch 1.6 s: it swallows you, and you **drop out through the chest hole behind it,** knocked down (Rahu). Rest gives the max HP back |
| **Inhale** | 1 | The chest hole widens, the petals open; dust, pebbles and loose rubble stream toward the mouth (the pull itself is the tell) | 1.0 / 0 / 3.0 pull / 1.2 | Rubble in the stream hits for 15 if it crosses you; **Snap** at the end 0.20 active, 80 at 3 m | Pull 350 cm/s in a 120° cone to 25 m | Breaking line of sight with a voice pillar stops the pull on you. Under it, a falling Shepard tone plays in the music (2.16) |
| **Rift beam** | 1 | It rears to 13 m; the chest hole's rim brightens violet-white; a whistling inrush | 1.40 / 0 / 2.0 sweep / 1.2 | 25 per 0.25 s tick + curse 40/s | A line 40 m long, 1.5 m wide, sweeping 100° at 50°/s across the floor | A visible beam, not a projectile. Roll through it (the roll's i-frames cover one tick), or get behind a pillar. The floor where it passes cracks with violet light: those tiles are where the island will split in the transition |
| **Tail flail** | 1 | The vertebrae rattle and close their gaps | 0.65 / 0 / 0.30 / 0.9 | 60, knockback 500 | 270° behind and to the sides, r 1,100, `low` | Jump it. Breaking the tail part (stance 50) scatters the vertebrae: the flail's reach drops to 7 m |
| **Nullstones** | 1 | Its throat bulges three times, the petals part | 1.00 / 0 / — / 1.0 | 50 each | Three swallowed rocks in a 20° fan, 1,400 cm/s, never home, ≥ 800 cm only | — |
| **The Hollow** (shell) | At 80% and 60% | It folds its hooks, draws the void round itself: light visibly bends round its body and the sky behind it swims | 1.2 to raise | — | Shell HP 1,000 | **Inside the shell all damage is ×0.2.** Arcane damage on the shell ×3; a ringing voice pillar within 12 m deals 150/s to it; void heals it (×0.5). Broken: it staggers 3 s (stance ×2). Unbroken after 20 s it drops by itself, and its next move is a Rift beam |
| Tame punish (KEPT rule) | Any | The Inhale aimed at the **furthest tame** first | — | Snap 150 to the tame | — | The pull stops at the island's edge (tames brace), so it's never an instant death |

**Transition at 50%: the Gnashing** (unblockable, crimson, KEPT rule).
- **The move.** It climbs onto the island's north rim and sinks both hooks in. The crimson glint shows on the tooth ring. Windup 1.4 s; it takes 50% damage during it.
- **The bite.** It bites the rim: 120 damage within 10 m, knockback.
- **The chains.** As the bite lands the four chains snap one after another, 0.3 s apart, each at a visible link.
- **The split.** The island splits along the beam cracks into **7 pieces** that drift apart. Separation starts at 0.5 m/s and eases to orbit speed over 6 s, so nothing snaps.

**Phase 2** (2 new delayed attacks, 1 new combo, 1 arena change, 1 delayed variant; idle gaps 0.3 to 0.9 s).

| Item | Detail |
|---|---|
| **Arena change: the broken stage** | 7 pieces orbit the rift centre on a ring at 1.2 m/s: the **Stage** (the dais piece, 30 m) and six others 10 to 18 m across. Gaps run 1.5 to 3.5 m and vary as they orbit, and every 20 s neighbouring pieces touch and grind (a deep stone grind, sparks of voidglass) before drifting apart. Each piece bobs 0.3 m on a 10 s cycle. **The pieces move continuously by server time:** each piece's position is a function of server time, so clients compute the same smooth motion and never snap (3.2). Unwarded pieces shed an edge chunk every 30 s; the two smallest fall away after 90 s and are replaced 30 s later by rubble rising from below on its keel. **Arcane wards keep the ground solid (KEPT):** any Arcane spell or Wildbrand hit on a piece's keel-stone (a black spike at its centre) wards it for 20 s (no crumbling; a violet hexafoil glow on the stone). Thrown anchor salt does the same for 10 s, for everyone |
| **Delayed hook slam** (delayed variant of the combo's third hit) | 0.90 / **0.6 hold** / 0.25 / 1.4; 90; the slam cracks the piece it hits (the piece drops 1 m and wobbles 2 s, unsure footing) |
| **Rift dive** (new delayed) | It leaps off its piece into the black. 2.5 s later it bursts up under a player's piece. **Tell: a lensing ring opens beside that piece and rubble rises round its edge 1.2 s before**, with a hold of up to 0.4 s. 100 damage, launch (impact 5). If the piece is warded, the burst cracks only the ward (the ward ends, no launch) |
| **Twin-beam sweep** (new delayed) | From the Stage: a Rift beam sweeping 200° at 35°/s, with a **0.5 s stop mid-sweep** before it resumes (the delay). 25 per tick, curse 40/s. It cuts across gaps, so pieces' keels cast cover |
| **New combo: Rake, rake, snap, flail** | 0.55 / 0 / 0.22 (55); 0.50 / 0 / 0.22 (55); lamprey snap (not a grab) 0.45 / 0 / 0.20 (70); tail flail 0.65 / 0 / 0.30 / 1.0 (60, `low`) |
| Carried moves | Lamprey lunge (it lunges across gaps), Inhale (pulls you toward the gap edge, so position against a pillar), Nullstones (2 in flight now allowed), the Hollow at 40% |
| Falling | **Falling between pieces into the black kills you** (Unmade). Your body appears at run-back stone 2. No floor; no helpers after a death |

**Hidden phase 3 at 25% (artifacts on; damage ×1.4; behind the BUILT `AnyArtifactModifiers` flag): Inside.**
- **The setup.** It climbs onto the Stage and opens its chest hole wide (6 m), and a constant 200 cm/s pull covers the whole arena toward the hole.
- **Getting in.** Anyone who is pulled in, or jumps in, enters **the Gullet**.
- **The Gullet.** A 30 m chamber inside it: ribbed glass walls, a floor of compacted swallowed rubble and Choir bones, and **the Void heart** hanging from cords at the centre, beating.
- **Inside:**
  - Max HP drains 3/s (KEPT passes apply).
  - Every 20 s the walls contract: a crushing ring with a 1.2 s tell (the ribs creak and close). Stand in the alcoves between the ribs.
  - Every 30 s it coughs: everyone inside is thrown out onto the Stage, knocked down.
- **Outside.** The body keeps fighting with every P2 move, idle gaps cut to 0.3 to 0.6 s.
- **Damage.** The boss's HP is shared. Hits on the heart count ×1.5 and hits on the body ×0.75, so the group splits: some go in, some hold the body.
- **Why.** "You fight inside it" (KEPT premise) as play, not a cutscene.

**Drops (KEPT frame):**
- Boss core.
- Void path materials: void essence ×20.
- A chance at the **Void heart** boss part (KEPT: kills restore max HP lost to void: 5% of your max HP per kill).
- Its **trophy head** on each player's first kill (KEPT monument rule).
- First kill: a named, visible drop that flies to the killer within 6 s (KEPT).
- The **Void altar** sits on the island's dais; it survives the split on the Stage piece and is reachable after the fight (BO rule: the arena holds the altar).

### 2.15 Creature voices (fused real animal calls; 4 layers per attack KEPT: tell, strike, hit, pain)

| Creature | Voice (real calls fused) | Tell layer | Pain / death |
|---|---|---|---|
| Hollowfang | Dhole whistles + a wolf howl played reversed + a sharp human inhale on the blink | The reversed inhale before every blink and lunge | A dog yelp pitched down 5 semitones, cut short |
| Riftmaw | Crocodile bellow pitched down + elephant-seal gurgle + cracking glass | A sucking gasp from the rift | Wet thrashing, glass splintering |
| Nullwing | Lammergeier hiss + fruit-bat shriek + wind through cracked glass | The screech before the dive | A vulture's grunting hiss |
| Gnawer | Naked mole-rat chirps (they're among the most vocal rodents) + beaver gnawing | Chitter rising before a latch | A squeak |
| The Hush | Aye-aye "hai-hai" calls + barn-owl screech + a slowed fox scream; its finger **taps** (the aye-aye's real tap-foraging) as its listening sound | Two taps and a leathery ear-snap | A long owl-scream that breaks |
| Voidstrider | A horse whinny reversed + a low elk bugle | A hoof scrape and filament hiss | A horse's scream slowed |
| Riftback | Tortoise grunts + a fin-whale moan pitched down | A grunt before the tail club | A deep moan |
| Driftmote | Beluga chirps pitched up 1 octave, low-passed | — | A soft pop |
| Withergoat | Goat bleat slowed 15% + sheep cough | A snort | A bleat |
| Riftborn | The original creature's voice with a reversed tail on every call and a 30-cent detune | Same | Same |
| Unravelled | Low male choir chord + stone grinding | The chord swelling | The chord cracking |
| Mother of Rifts | As Riftmaw, an octave lower, with a female crocodile's grunting call | — | — |
| Starving One | Hyena whoop slowed + gorilla chest-rumble + chewing | A stomach growl before the grab | — |
| Null Warden | No voice: bell, glass chime, breath in the helm | The bell's hum rising | A bell cracking |
| The Echo | Lyrebird mimicry (shutters, chainsaws, birds) + raven croak slowed + your own sounds reversed | Your sound reversed | A raven's broken croak |
| **Vorhollow** | Tiger chuff + elephant infrasound rumble (felt at 16 to 20 Hz on sub) + walrus bellow reversed; the Inhale is a giant reversed breath | A rising reversed breath before every special | A long falling roar that turns into wind |

### 2.16 Music: the Corrupted wasteland (80 BPM, atonal, reversed and detuned strings, sub-bass, silence as threat: SPEC 3.3; cues GAP-COOKED; rules KEPT)

**Rules (KEPT):** real composed pieces, never generated loops; always audible, ducking ≤ 4 dB in combat; the only silence is Dead calm (planned); mix −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP; bar-synced crossfades at borders over 120 m.

**The palette.**
- **Strings.** A string orchestra using Penderecki-style clusters, glissandi, sul ponticello and col legno, recorded, then **some phrases played reversed** (the attack swelling in, the bow stroke vanishing). Tuning is 30 to 50 cents flat against A = 440 for the Void, so the Graveyard's organ sounds wrong next to it.
- **Choir.** A low male choir singing the **Choir motif**: a drone on A♭1 with a whistling overtone (throat-singing), a four-note figure A♭–D–E♭–A (tritone-shaped, so it never rests), recorded forward and reversed.
- **Colour.** Waterphone, bowed cymbal, contrabass clarinet, prepared piano low strings struck with mallets.
- **Floor.** A 28 to 35 Hz sub-bass pulse at 80 BPM (one pulse every two beats), felt more than heard.
- **What's held back.** No drums except the sub pulse, and no melody instrument except the choir.

| Cue | Where / when | Length | Feel and build |
|---|---|---|---|
| `mus_void_explore_day` "Grey Hours" | Surface by day | 5:00, 3 stems | Sparse string clusters, the reversed choir figure every 16 bars, sub pulse low |
| `mus_void_explore_night` "No Moon" | Surface at night | 5:00 | Lower, the choir drone held, the waterphone, a slower string gliss |
| Sub-zone stems | V1 needle hiss texture with high harmonics; V2 a bowed-cymbal "surf"; V3 the choir forward (the one place it's sung forward); V4 the sub pulse up 3 dB with glass harmonics; V5 the road's hum (55 Hz) tuned into the score, rising a semitone per station passed | Layered on the explore cue | Bar-synced |
| `mus_void_combat` | Any fight | Loops by section | Col-legno string ostinato at 160 (double time), contrabass clarinet stabs, the sub pulse every beat. In Dead calm only the sub pulse and string clusters enter |
| `mus_void_mini` (5 variants) | Each mini | 3:00 each | Unravelled: the choir chord breaking apart into single voices. Mother: low brass smears. Starving One: prepared-piano thumps. Null Warden: a real bell as the pulse, no strings. Echo: the strings mimic the player's last sound |
| `mus_boss_vorhollow` | The fight, 3 sections | P1 3:30, P2 3:30, P3 3:00 | **P1:** the Choir motif in full for the first and only time, forward, with strings clustering against it. **P2:** the motif reversed; the orchestra splits left and right as the island splits. **Inhale:** a falling Shepard tone in the strings under the pull. **P3 (the Gullet):** the music muffled as if heard through a body, a heartbeat on the sub, the choir motif sung one note at a time. Kill sting: the motif resolves for the first time (A♭ to A♭) and stops |
| `mus_void_riftstorm` | The event | 6:00 loop | The sub pulse doubled, reversed brass swells, the choir drone with no figure |
| `mus_void_unmade` | The Unmade Place | 4:00 | Quiet (−26 LUFS): one bowed cymbal, one reversed choir voice, long gaps that are never full silence |
| `mus_pass_severance` (Void half) | The Severance | 4:00 | KEPT: reversed and detuned strings against the Crystal half's armonica in B minor; the Void half plays only from R10 |
| Dead calm | Weather | — | KEPT planned silence: an 8 s fade out, 8 s back in |

---
### 2.17 The Void path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

#### 2.17.1 The resource: Corruption, shown as the **Void** bar (SPEC "huge power; each use lowers max HP until you rest"; mechanics GAP-COOKED)

| Part | Rule |
|---|---|
| The bar | 0 to 100, labelled **Void** on the HUD (KEPT, so it never reads like the world Corruption meter). It's black-violet with a violet edge |
| Fills | Every void ability **adds** Corruption (amounts in 2.17.3). It falls 5 per second after 4 s without a void ability |
| Tolerance | While the bar (after the ability's addition) is **below your tolerance**, the ability costs **no max HP**. At or above tolerance it costs its listed max HP (KEPT combat costs). Base tolerance 20; **Void tolerance** +10 per rank (3 ranks → 50). Light use is free, and pressing on costs you |
| Max HP cost | Paid into `MaxHealthLost` (KEPT attribute): the grey hatched band at the right end of the health bar, with a soft crack sound and a grey pulse on the bar each time it grows |
| Huge power | Void abilities deal **+0.3% damage per point in the bar** (+30% at 100) |
| Root passive: **Empty vessel** (main only, SPEC) | Corruption gain ×0.7; max HP costs ×0.75; **passes rift drain entirely** (KEPT 3.3) |
| Earning it back | Rest (all of it, KEPT). Consume (10% of max HP lost, KEPT combat). Feed. The Void heart part. Four combos (Void leech, Hungry spores, Abyssal hunger, Devouring grove) |
| **The self-cost limit** | Your own abilities can't push `MaxHealthLost` past 75% of your max HP: an ability that would is refused, with the stamina-refusal flash on its slot and a dry crack sound. **This is a resource limit like stamina, not a safety floor.** Rift drain, Gnawers, Vorhollow's bite and every enemy can still take you to 0 and kill you (Unmade). No floors |
| PvP | Max HP costs are the same; the Void bar shows on your nameplate to enemies only as the body change (2.18), never as a number |

#### 2.17.2 The tree (ST nodes, cost and order SPEC; gates by branch points 0 / 5 / 10 / 14 / 17 / 20, the frame the Fire, Frost and Arcane trees use)

| Branch | Node | Cost | Gate | Effect (ST) | Numbers (KEPT combat / GAP-COOKED) |
|---|---|---|---|---|---|
| Trunk | **Void bolt** | 3 pts | — | Basic void spell | KEPT: Bolt, 24 damage, 2 max HP, cd 1.2. +6 Corruption. Speed 2,800 cm/s; player bolts keep lock-on lead (KEPT) |
| Trunk | **Void tolerance** | 3 ranks | — | Use more void abilities before max HP drops | +10 tolerance per rank (20 → 50) |
| Trunk | **Unravel chance** | 2 ranks | — | Higher chance to weaken enemy defences | Void hits have an 8% chance per rank (8 / 16%) to apply **Unravelled** for 6 s (bosses 3 s): armour −20%, `physResist` set to 1.0, stance damage taken +15%. A grey thread-fray FX on the target |
| Trunk | **Null strike** | 2 pts | — | Basic void weapon art (**Wildbrand**) | KEPT: all melee, 3 max HP, cd 3, 0.30 / 0.20 / 0.44, ×1.5, ignores 30% armour, stance ×1.5. +8 Corruption |
| Abyss | **Void rift** | 5 pts | 0 | Spell: a rift that damages enemies inside | KEPT: Pool r 300, 14 dps, 6 s, 4 max HP, cd 12. +14 Corruption. A small tear with your violet edge; enemies inside take the dps (no drain on enemies: rift drain is a player hazard) |
| Abyss | **Abyssal power** | 5 ranks | 5 | More void damage | +6% void damage per rank (+30%) |
| Abyss | **Collapse** | 4 pts | 10 | Spell: an implosion that pulls enemies together | KEPT: Pull 360°, r 500, 30 damage on pull end, cd 16. +14 Corruption. Pulls lowland and midland fully, highland 50%, bosses not at all |
| Abyss | **Dark matter** | 3 ranks | 14 | More damage against shields and armour | +10% per rank vs shields (shield HP, Lyrathis's dome, Vorhollow's shell before its ×0.5, boss plates) and armoured parts |
| Abyss | **Event horizon** | 3 pts | 17 | Rifts swallow enemy projectiles | Your Void rift and Black hole delete enemy projectiles that enter them (not beams). A small inward flash and a gulp sound |
| Abyss | **Black hole** (capstone) | 10 pts | 20 | A black hole that drags in and crushes enemies | KEPT: Strike r 600, pulls 600 cm/s for 4 s, then 80 damage, cd 60. +30 Corruption, 12 max HP. Bosses resist the pull (they're slowed 30% inside) |
| Hollow | **Blink** | 5 pts | 0 | Short teleport | KEPT: Dash 700 cm, cd 3, 1 max HP; i-frames for the whole 0.18 s travel. +5 Corruption |
| Hollow | **Fade** | 5 ranks | 5 | Longer invincibility on dodges and blinks | +0.03 s i-frames per rank on dodge and Blink (+0.15 s) |
| Hollow | **Phase** | 4 pts | 10 | Briefly pass through attacks | KEPT: Self buff, i-frames 0.6 s, cd 10. +8 Corruption, 3 max HP. You turn grey-translucent; grabs pass through you |
| Hollow | **Shadow step** | 3 pts | 14 | Blink behind a targeted enemy | KEPT: Dash behind the lock target, cd 8. +8 Corruption, 2 max HP. You arrive 1.2 m behind, facing its back (the backstab arc holds for 0.5 s) |
| Hollow | **Unseen** | 3 ranks | 17 | Enemies lose track of you after a blink | After Blink or Shadow step, creatures within 15 m lose their target for 0.6 s per rank (bosses 0.3 s per rank; PvP: breaks the attacker's lock-on) |
| Hollow | **Nowhere** (capstone) | 10 pts | 20 | Vanish, become untargetable, and your next hit is critical | KEPT: untargetable 3 s, next hit crits (riposte multiplier), cd 60. +20 Corruption, 8 max HP. Your body goes to a shimmer of bent light |
| Devour | **Consume** | 5 pts | 0 | Finisher that eats a weakened enemy and restores max HP | KEPT: Wildbrand, all melee and fists, cd 15, 0.35 windup, paired 1.4 s, on a target below 25% HP; kills it, restores 10% of max HP lost; bosses only in a stance break (riposte ×1.2 instead). **Corruption −20** |
| Devour | **Gluttony** | 5 ranks | 5 | More damage for each point of max HP lost | +0.1% damage per point of `MaxHealthLost` per rank (+0.5% at rank 5); cap +50% |
| Devour | **Sacrifice** | 4 pts | 10 | Spend max HP for a burst of damage | KEPT: Self buff, spend 10 max HP, +30% damage for 10 s (tolerance doesn't apply: it always costs). +0 Corruption |
| Devour | **Feed** | 3 pts | 14 | Kills restore some max HP | Kills restore max HP lost: lowland 3, midland 5, highland 8, mini 25, major 60 |
| Devour | **Insatiable** | 3 ranks | 17 | Consume works on stronger enemies | Consume threshold 28 / 31 / 35% (KEPT cap 35%); at rank 3 it also works on Riftborn and highland creatures at full |
| Devour | **Maw of the void** (capstone) | 10 pts | 20 | A giant maw devours everything in front of you | KEPT: Cone 90°, 800 cm, 90 damage, 15 max HP, cd 90. +35 Corruption. A four-petal maw of black glass opens from the ground in front of you and closes (0.8 s, real geometry); lowland and midland enemies under 30% HP inside are eaten outright (each restores 5 max HP) |

#### 2.17.3 Corruption gain per ability (GAP-COOKED)

| Ability | Gain | | Ability | Gain |
|---|---|---|---|---|
| Void bolt | +6 | | Shadow step | +8 |
| Null strike | +8 | | Phase | +8 |
| Void rift | +14 | | Nowhere | +20 |
| Collapse | +14 | | Black hole | +30 |
| Blink | +5 | | Maw of the void | +35 |
| Consume | −20 | | Sacrifice | 0 |

#### 2.17.4 Feel (combat KEPT; Void specifics)
- **Hit stack.** Every Void hit plays the inward ink smear (KEPT sparks), a 30 ms pull-in of the hit target's outline toward the impact (a tiny reverse shockwave), a low thump plus a reversed whoosh, and 3 more layers per combat 2.14.
- **Coil.** Casts show the hand closing on a point of black (0.14 s coil on light casts, 0.20 s on heavy).
- **Blinks.** Your body smears for 2 frames and reappears with a lensing ring that fades over 0.3 s.
- **One verb per button and the 0.33 s buffer** are KEPT.

#### 2.17.5 Combos: the 11 new Void pairs (names and effects SPEC from CB; numbers GAP-COOKED; passive unless stated, the FULL-GAME default). With these, all 66 pairs are complete

| Pair | 25/25 (SPEC name: effect) | 50/50 (SPEC name: effect) |
|---|---|---|
| Fire + Void (neighbours) | **Black flame:** burns ignore armour. Your burn ticks ignore armour and `physResist`; +10% burn damage vs armoured targets | **Void furnace:** Corruption fuels Heat, and full Heat lowers void costs. Each Corruption point gained also gives 0.5 Heat; at full Heat, void max HP costs ×0.5 |
| Lightning + Void | **Dark lightning:** lightning ignores 30% armour | **Singularity spark:** Black hole crackles with lightning. A 20-damage bolt (shock 25) from it to a random enemy inside every 0.5 s |
| Sun & sand + Void | **Sun-eater:** Consume fills Radiance to 100 | **Dead star:** a dark sun that burns and pulls. Sunburst becomes a black sun (needs Sunburst): r 600, pulls 300 cm/s for 3 s, then 70 damage and blind 100 |
| Nature + Void | **Withered:** roots weaken defences. Enemies held by Root snare or Strangling vines are Unravelled for the hold | **Devouring grove:** your plants consume weak enemies. Sprout turret, World tree and Ancient guardian kill lowland enemies under 20% HP within 3 m outright; each kill restores 3 max HP lost |
| Wind + Void | **Void glide:** a free Blink (no cost) at the end of a glide, once per glide | **Event gale:** a Tornado and a Black hole within 10 m merge: pull 800 cm/s for 3 s, then 100 crush |
| Arcane + Void (opposites) | **Order and chaos:** each void ability restores 6 Mana; spending 30 Mana makes your next void ability cost no max HP | **Paradox:** spells can spend Corruption instead of Mana for much more damage. A wheel toggle: Arcane spells add Corruption equal to their Mana cost instead of spending Mana, and deal ×1.5. Past tolerance they cost max HP (1 per 10 Corruption) |
| Frost + Void | **Frozen blink:** blinking freezes the spot you left. A 250 r frost patch, frost buildup 60 a second on enemies inside, 4 s | **Ice maw:** Consume works on any frozen non-boss enemy at any HP |
| Blood & curse + Void | **Void leech:** 30% of your lifesteal healing restores max HP lost first | **Abyssal hunger:** Consume also heals 25% of max HP |
| Tide + Void | **Deep water:** Blink works underwater and pulls you down 300 cm | **Abyssal maelstrom:** Maelstrom's pull ×2, and it crushes for 60 at the end |
| Necromancy + Void | **Soul void:** each Soul you hold lowers void max HP costs by 2% (max 40%) | **Hollow army:** your raised minions blink 6 m to their target every 6 s |
| Spore + Void (neighbours) | **Hungry spores:** kills by Spore damage restore 2 max HP lost | **Entropy:** rot and void stack, draining max HP. A rotting enemy hit by void loses 5% of its max HP per hit until the fight ends (bosses 1%, max 10 stacks) |

### 2.18 Body change: Void (hero row KEPT; driver and stages GAP-COOKED)

| Part | Rule |
|---|---|
| Stage by path points (KEPT frame 0.3 / 0.6 / 1.0) | 0.3 at 1 to 32 points, 0.6 at 33 to 66, 1.0 at 67 to 100. The stage sets the **cap**; the live look moves within it |
| Live driver | Intensity = max(Void bar ÷ 100, `MaxHealthLost` ÷ max HP × 2), clamped to the stage cap. As the main path: ×1.0. As the subclass: ×0.6 (KEPT: the main path strongest) |
| Skin (KEPT mask A) | Greys from the fingertips toward the shoulders: desaturation 0 to 40% (KEPT), black fingertips at 0.3, grey to the wrists at 0.6, grey forearms with black-violet veins showing at 1.0 |
| Eyes (KEPT) | Black from the centre: pupil scale 1.0 → 2.6 at full |
| Hands (KEPT) | Light-bend haze round the hands over 0.6 intensity (screen-space refraction, owner and 15 m) |
| In the Wasteland | Your world Corruption meter adds +0.2 intensity there (you belong to it) |
| After Rest | It fades to the stage's floor (0.3 × cap) over 3 s as you wake. You see yourself come back |
| Attractiveness | Grey skin is matte, never sickly green; the veins are fine; the black eyes are clean and glossy. At full it reads striking, not rotten (Maren is the reference) |

### 2.19 Gear and items from the Wasteland

#### 2.19.1 Hollowplate (KEPT: survival-crafting numbers, hero name and silhouette)

| Field | Value |
|---|---|
| Set | **Hollowplate** (hero name; survival-crafting's "Voidglass" set) |
| Load | Heavy; poise 50 |
| Inputs | Steel plate 16, voidglass 10, Withergoat hide 6, star iron 2 (whole set) |
| Resist | Curse buildup ×0.5 |
| Bonus | Rift max-HP drain halved (nullward effect); max HP lost to void abilities −25% |
| Look (hero, KEPT) | Voidglass plates in black iron; a floating rubble halo behind the back (bone-attached, five stones on a slow 8 s orbit, each with a visible keel); one jagged asymmetric pauldron; face and forearms open so the black eyes and grey forearms show. On the female body, the cuirass is cut close to the waist with a fitted grey leather under-layer; on the male, broad plates over a bare-armed under-tunic. Black glass, violet edge light |
| Ascendant (starsteel, KEPT hero) | Starsteel inlay glowing in the path colour, a halo-crest |

#### 2.19.2 Weapons, tools and consumables (GAP-COOKED unless KEPT)

| Item | Class | Station, level | Inputs | What it does |
|---|---|---|---|---|
| **Gnashblade** (blueprint at X-4) | Greatsword | Forge 4 | Riftmaw tooth 6, steel 8, void essence 2 | A serrated black-toothed blade: +10% stance on charged heavies. Void element |
| **Hushfinger spear** (recipe X-1) | Spear | Forge 4 | Hush finger bone 1, steel 6, greywood 4 | Reach +40 cm over a steel spear; its thrust whistles (it's loud: noise +500) |
| **Nullwing bow** | Bow | Workbench 4 | Nullwing bone 3, greywood 4, Voidstrider filament 2 | Arrows fly 10% faster |
| **Voidglass dagger** | Dagger | Forge 4 | Voidglass 3, steel 2, Hollowfang hide 1 | Backstab +10%; glass, so durability ×0.6 |
| **Riftback shield** | Heavy shield | Armorer 4 | Riftback plate 2, steel 4 | Guard boost +5 over steel; void and curse negation +20% |
| **Beacon lantern** (recipe scrap S-10) | Lantern | Workbench 3 | Voidglass 2, bronze 3, wax 2 | A 14 m forward cone of light (KEPT lantern effect: corruption build ×0.5). **Floating motes glow in its beam,** so rift zones show at night |
| **Nullward** (KEPT) | Talisman | Armorer 4 | Voidglass 4, steel plate 2 | Rift drain halved |
| **Anchor salt** (KEPT effect; recipe 2.7) | Consumable, throwable | Alchemy 3 | Grey salt 2, crystal dust 1, voidglass 1 → 2 | 2.7 |
| Curse resist potion (KEPT) | Brew | Alchemy 3 | Ashen grain + voidglass | Curse buildup ×0.5, 10 min |
| Trophy talismans (KEPT) | Talisman | Armorer 2+ | Trophy + voidglass 2 | Unravelled thread: own void max HP loss −15%. Rift heart: rift drain 25% slower. Starving maw: hunger drains 20% slower. Null seal: spell buildup on you ×0.8. Echo stone: the first hit after a dodge +10% |
| Rift chest, Hollow bastion set (KEPT) | Pieces | — | KEPT | KEPT |

### 2.20 The story the land tells, with no words (fits world-map's Twelve Peoples, the giant, and the Void–Arcane war at the Severance)

No staged scenes, no cutscene actors, no narrator (DD). Six things the player pieces together, each from places:

1. **The giant was cut here.** The Broken Knee's sheared bone (L8), the Severance splitting the Spine's last stretch, and the Great Rift where the cut is deepest: the gap the giant came out of, opened again (Ginnungagap). Nobody says it; the map screen's shape shows the legs split.
2. **The Choir sang to hold it shut.** Horn towers aimed at rifts (L2), amphitheatres facing tears, hollow columns that moan, the tuning hall (S-14), the children's benches (S-15). Their noise was their ward, the opposite of the Wardcarvers' silence across the gorge.
3. **They emptied themselves to hold more.** The Hollow Colossus's carved-out chest (L4); the seated dead at rift edges, given to the dark (S-12, S-18); the point-down cairns on every road (offerings into the earth); the Alms court's licked-clean bowls (S-13). They fed it.
4. **It grew anyway.** Cairns whose tops floated off (S-6), the Hanging Wood (L3), the Viaduct frozen mid-fall (L6), the pilgrim's staff hanging below the break (S-23), the drowned procession walking into the sea (S-11), the last rehearsal scored in rests (S-16).
5. **They fought the Wardcarvers at the Severance.** The Hollow Fort's staves aimed into the gorge and crystal arrowheads in its walls (S-29); the Cantor's ash cast in a crystal cage (L7), with two Choir statues reaching through the bars (S-30); across the gorge, the hollowed giantess (KEPT, crystal lane).
6. **The rift took their temple and made something.** The Gauntlet's stations are what became of their keepers: the Cantor's Stair (the Unravelled), the Rift Pens (the Mother), the Alms House (the Starving One), the Null Gate (the Warden, his bell's clapper tied off: X-6). The last choir sits on the bridge facing the island (S-28), and on the island sits what their singing fed (Vorhollow). Below, the Seated Choir and the last note (U2, U8).

**The newcomers** (world-map rule 5): the camps' wrecks, the first hut with its stone circle (S-2), the surveyors dragged against a cairn (S-19), and Maren, who came back.

**Lore items (item descriptions only, Souls-style, KEPT rule).** Each is 2 to 4 lines, never a full explanation:
- Unfinished row
- Salt rake tally
- Child's whistle
- Cantor's breath-stone
- Pilgrim's staff
- Cantor's name-stone
- Seat tally
- The clapper's knot
- Choir seat 1 of 200
- The last note

### 2.21 How the Void meets everything it touches (design with intention, Aaron 11-14)

| Thing it touches | How they meet |
|---|---|
| Fire | Litter fire in V1 (a tool and a danger). Black flame and Void furnace combos |
| Frost | Frozen blink and Ice maw combos. No ice forms in the Wasteland: the rain shadow has no water to freeze |
| Arcane and the Crystal caves | Arcane ×3 on Vorhollow's shell, on rift hearts and on brood rifts; Arcane wards the island pieces; anchor salt needs crystal dust; ward fields throw void creatures ×2; a rift eats Lyrathis's dome; the Severance invasions; the Severance raid |
| The Graveyard and Necromancy | The shared Corruption meter (KEPT); graves ending mid-row; Riftborn Gravehounds, Cryptstalkers and Palestriders; the Rift Storm's border seed; the Soul void and Hollow army combos |
| Light and dark | No moon (KEPT). Rift-edge glow is the night light; the Black Beacon gathers it; the Beacon lantern shows rift zones; braziers stop corruption build (KEPT) |
| Sound | The Hush and the Hollowfang hunt by noise (`UWRNoiseSubsystem`); litter and glass are loud; Dead calm doubles your footsteps; voice pillars ring; the silent bell ends Dead calm in the Quire; the Choir's song is the counter to the dark |
| Water | The Grey Strand's surf, rift pools (KEPT fishing), the drowned procession. Deep water combo |
| Tames | Off-biome tames take rift drain; void tames don't. Riftback pockets reach the clan's Rift chests. Vorhollow's Inhale goes for the furthest tame first |
| Bases and claims | **No claims inside rift zones** (a claim totem can't be placed where the drain is live; the placement ghost shows red with the reason "The ground won't hold it"). The Rift Storm targets claims within 1.5 km. Gnawers gnaw wood ×1.5; void ×1.25 vs pieces (KEPT). No claims in the Severance corridor (KEPT) |
| PvP | The Severance is a PvP hotspot (KEPT pass rule). Rift zones drain enemies too: fighting there wears both sides. Blink and Unseen break lock-on in PvP. The Void bar shows only as the body change |
| The rest of the map | The Great Rift column is seen from the Graveyard and the Crown; the Void pillar on the Ring burns in the Rift Storm; mythic gear needs voidglass |

### 2.22 Invasions: Arcane ↔ Void through the Severance (DD SPEC pair; FULL-GAME 3.3 numbers KEPT; switched on at R10)

| Part | Detail |
|---|---|
| Rule (KEPT) | Server setting; 5 min warning to the defending biome; 8 to 14 attackers plus one mini through the pair's pass; they march on the nearest claim or shrine within 1.5 km and hold a shrine for 30 min (fast travel to it shuts); beating it pays both paths' materials and 2,000 path XP to everyone with 10% |
| **Void invaders into the Crystal caves** (Void side GAP-COOKED) | 4 Hollowfangs, 3 Gnawer swarms (6 each), 2 Nullwings, 2 Riftborn Prismbacks (crystal bodies gone grey) and a Hush, **led by the Echo**, which leaves its route for the invasion. They enter through the Severance Gate (crystal H8, KEPT), where the ward fields still throw them ×2 (KEPT) |
| **Crystal invaders into the Void** (KEPT roster) | 4 Prismbacks, 4 Geodemite swarms, 3 Echobats and a Lumenhorn bull, led by the Geode Tyrant. They march west out of the gorge; their target is the Severance West shrine (80 m from the mouth) or the nearest claim within 1.5 km |
| Warning signs on the Void side (world, no UI beyond the KEPT notice) | The Hollow Fort's racked tuning-staves ring on their own; the gorge's grey crystal veins light as the invaders pass (KEPT crystal sign); the pass duet's crystal half swells |
| Warning signs on the Crystal side (KEPT) | The Hollowed Giantess's cage rods ring; ward fields flicker violet; the duet's Void half swells |

### 2.23 The endgame hook (R10; RAVEN-PLANNED in DD Endgame; details GAP-COOKED unless KEPT)

**DD's frame (SPEC, kept).** Endgame is sized for groups, not the whole server. Difficulty is personal (artifacts). **Ruled out (kept out):** server-wide boss events, a world boss, cross-biome endgame bosses, world-wide escalation, weekly challenges, first-kill announcements.

#### 2.23.1 Every pass opens

- **What opens.** At R10 the rockfall at the Severance's west end is cleared, so the seventh and last pass opens. All twelve wedges and all seven passes are then walkable (world-map W12).
- **How.** There's no event and no announcement (DD rules out server-wide announcements). The rubble is simply gone in the R10 build, with the old rockfall's boulders left split along both gorge walls, so the land shows it happened.
- **The invasions.** The Severance invasions switch on (2.22).

#### 2.23.2 Raids for 6 to 10 (DD SPEC; tuned for 8, not capped: FRP decision, DR #22 proposed)

**The frame (GAP-COOKED, built once at R10, used by all seven passes).**

| Part | Rule |
|---|---|
| Where | Under each pass, **the war-hall**: a sealed cavern beneath the pass floor where the old pair's two champions were laid after they killed each other. The raid gate is a sealed door in each pass fort's undercroft (both forts have a door; either works) |
| Who | One group at a time: a party or clan of any size (tuned for 8; boss HP scales by Raven's +80% per extra player or tame). The door seals when the first raider crosses the inner threshold and opens when the fight ends or the room empties for 8 s |
| **The bosses: two champions, each of its own single element** | Both the pair's champions rise (their skeletons in the pass are the tell). **They are enemies of each other as well as of you:** each one's attacks hit the other at ×0.5, and if one champion drives the other under 30% it gains **Triumph** (+20% damage, faster). The raid has to split and keep the two even. **No boss mixes elements**, so this stays inside Raven's "no cross-biome endgame bosses" rule. That reading is flagged to Raven in 4 |
| Numbers | Each champion: 12,000 solo base, +80% per extra player or tame (8 players: 79,200 each). Stance 180, 3 parries. Phase 2 at 50% by a transition attack. Hidden phase 3 with artifacts (KEPT). Fight length 15 to 20 min for 8 |
| Loot (KEPT 10% rule) | Deal 10% of **either** champion's health for that champion's loot. Each champion drops its biome's path materials, a **Champion's mark** (needed for mythic gear, 2.23.3), an upgrade shard ×3 and a chance at a raid cosmetic (2.23.8) |
| Respawn | Server setting "raid respawn" (1 to 168 h, default 24 h) |
| Death | Server death rules (KEPT). Bodies appear at the raid door if the group leaves |
| Run-back | A shrine-stone respawn point in the fort undercroft, 40 m from the door |

**The Severance raid: the Broken Choir (the R10 proof, full design).**

| Part | Detail |
|---|---|
| The war-hall | A cavern 120 × 60 m under the gorge, split down its length by a 6 m-wide crack of black (a rift seam). The west half is Choir stone and voice pillars; the east half is crystal floor and Wardcarver ward stones. Both sides' old weapons are racked facing each other |
| **The Cantor** (Void champion) | Risen out of its ash cast (L7): a 7 m long-armed figure of grey ash and bone, its open mouth a rift. Moves: **Choir scream** (a 20 m sound cone, 1.2 s tell, strips buffs, 60 damage); **Ash hooks ×3** (combo 0.65 / 0.55 / 0.90 with a 0.4 hold, 60 / 60 / 100); **Mouth rift** (it opens a tear in the floor under a player, 1.0 s tell of rising pebbles, drains 6 max HP/s inside for 6 s); **Grab** (unblockable, 1.1 s, crimson; it swallows you into its mouth rift, then you drop out in the crystal half, putting you next to the other champion); transition: **the Hollowing**: it inhales the room's light for 1.4 s and the whole west half goes dark |
| **The Hollowed Giantess** (Crystal champion, Lyrathis's sister) | A 9 m crystal woman, hollowed from the front, kneeling until the fight starts. Moves: **Shard rain** (a 2.0 s tell of chiming overhead, falling crystals, each with a growing shadow); **Prism lance** (a beam split by her own ward stones, reusing Lyrathis's beam code); **Ward slam** (a ward field ring, r 8 m, 25 damage and a 600 throw, KEPT numbers); **Hollow embrace** (grab, unblockable, 1.0 s); transition: **the Mirror**: the crystal floor flares and every void creature or Void ability in the room is thrown ×2 |
| How they meet | The Cantor's Mouth rift on the crystal floor eats a ward stone; the Giantess's ward slam throws the Cantor across the seam. Keep them apart and even. Arcane players are best on the Cantor, and Void players are best on the Giantess (the opposites rule, both ways) |
| Pays | 2 Champion's marks (Void and Arcane), voidglass ×12, resonant crystal ×12, void essence ×10, arcane essence ×10; a 3% chance at the raid cosmetic "Cantor's mouth" (a rift-mouth helm glow) or "Giantess's hollow" (a crystal chest-cavity cloak clasp) |

**The other six raids (same frame; each pair's champions are KEPT from world-map 2.12; their move tables come from the frame above and their biome's KEPT kit).**

| Pass | Champions | War-hall gimmick |
|---|---|---|
| North and south passes (Fire / Frost) | The Fallen Smith and the Burnt Wyrm (SPEC fire-frost) | The floor alternates lava and ice tiles that each champion's slams flip (fire melts, frost freezes); Thaw combos for groups |
| Stormgut (Lightning / Blood) | The Rodwarden roc and the Coven leech-drake | Copper rods ground the roc's strikes; bleedwater pools heal the drake |
| Saltgate (Sun / Tide) | The sea-serpent skeleton and the coral-eaten stone colossus | The tide floods the hall on a 90 s cycle; the serpent swims, and the colossus is slowed by water |
| Rootgrave (Nature / Necromancy) | The rotted treant and the root-grown bone giant | Roots raise the floor; graves open raised dead |
| The Flue (Wind / Spore) | The Kitewright's winged champion and the paper mushroom-titan | A vertical shaft: updrafts carry you up; spore clouds sink |

#### 2.23.3 Mythic gear (DD SPEC "needs materials from every biome"; GAP-COOKED)

**Where it's made: the Ring on the Crown.** The twelve pillars (world-map 2.12) each carry a people's waymark, and the floor bears the twelve-coloured starburst. Mythic forging uses them. There's no NPC and no menu station: it's the world itself.

| Step | Rule |
|---|---|
| 1. Offer | Place 10 of each biome's signature resource on its pillar's offering stone (12 pillars: sulfur, lichen, stormglass, sunstone, heartwood, levistone, resonant crystal, bloodmoss, tidecoral, soulstone, deepcap, voidglass). The offering stays until used or taken back by its owner |
| 2. Mark | Place 3 Champion's marks (from any raids) and a **+10 masterwork starsteel weapon** (or a ring) on the starburst's centre stone |
| 3. Channel | Hold Interact for 3 min at the centre. Every pillar lights its beacon column (world-map: each beacon is a 300 m light seen across the map), so everyone on the server can see someone is forging. **Taking damage breaks the channel** (the offerings stay). It's a PvP hotspot by design |
| 4. Result | The weapon becomes **mythic**: damage ×1.15 over starsteel, 3 sockets fixed, upgrade cap stays +10 (no grind), and **two elements at once** (your main and subclass paths, 50/50; the only dual-element gear in the game), plus its own **mythic Wildbrand** |

**The ten mythics (one per class plus a ring; the Wildbrand uses your main path's element).**

| Mythic | Class | Mythic Wildbrand |
|---|---|---|
| **Reachblade** | Sword | *Twelvefold cut:* three cuts (0.28 / 0.18 / 0.36 each), the third ×2.0 with both your elements' buildup at 60 |
| **Spinebreaker** | Great hammer | *Fault line:* a slam (0.70 windup) sending a travelling crack 12 m (rides the ground, `low`), stance ×6 |
| **Giantsfell** | Axe | *Fell:* a charged overhead (to 1.0 s) dealing ×2.6 to staggered targets |
| **Crownspear** | Spear | *Pillar thrust:* a 6 m lunge (0.40 windup) that pins lowland and midland enemies 1.5 s |
| **Wakebreaker** | Greatsword | *Wake:* a 300° sweep that leaves a 3 s line of your main element on the ground |
| **Tidewhisper** | Dagger | *Unseen edge:* a 4 m dash-stab that backstabs from any angle once every 20 s |
| **Harrowmoon** | Scythe | *Harvest:* a spin; kills inside refund 15% stamina each |
| **Delvewright** | Pick-hammer | *Deep strike:* ignores 50% armour; mining yield ×2 for 60 s after use |
| **Farsight** | Bow | *Twelve arrows:* 12 arrows in a 20° fan (player projectiles, not homing) |
| **Twelvefold band** | Ring | No Wildbrand: +5% to both paths' resource gain and +5% to both elements' damage |

#### 2.23.4 Prestige (DD SPEC "reset a path for permanent perks"; GAP-COOKED)

| Part | Rule |
|---|---|
| When | A path at 100 points, at that path's altar (BUILT altars) |
| What happens | The altar screen's **Return** button (a confirm with a 1.5 s hold so it's never a mis-click) resets that path to 0 points. Its XP curve restarts at point 1, and the freed points go back under the 150 cap. Its nodes are kept in a saved layout, so re-buying them is one click per node as points come back |
| What you keep | One **Prestige mark** for that path (max 3 per path) |
| The perk (pick 1 of 3 at each mark) | (a) That path's damage +5%. (b) That path's resource capacity and gain +10%. (c) That path's **root passive works on the subclass slot at 50% strength** (pickable once per path). **Marks work only while that path is equipped** (main or subclass), so the most you ever carry is 6 marks: small, permanent, never runaway |
| What it shows | Each mark adds a gold thread to that path's body-change marks (a 1-nit gold edge on the veins, scars or runes); at 3 marks, the title "<Path>-sworn" (Ember-sworn, Rime-sworn, Hollow-sworn, …) |
| Fit to Raven's rules | 150 points total and 100 per path stay fixed (KEPT). Respec is still rare (DR #9): prestige isn't a respec, since you lose the points |

#### 2.23.5 Boss kill-time leaderboards (DD SPEC; GAP-COOKED)

| Part | Rule |
|---|---|
| What's timed | Every major, mini and raid: from the first hit landed on it to its death |
| Bands | Solo, 2, 3 to 4, 5 to 7, 8 to 10, 11+ (tames count as members, matching boss scaling) |
| Artifact runs | Shown in their own column with the artifact level |
| Where it shows | **In the world:** a carved stone tablet beside each boss's run-back stone 1 lists the top 3 names and times for the fastest band; interact to open the top 10 for every band. **In the Journal:** a Records tab with every boss, your best, and the server's top 10 |
| Scope | Per server (no global board; there's no central service). It never resets; a server setting "records season" lets a host start a new season (the old board is kept read-only) |
| Monuments (KEPT) | The monument plaque reads its kill time from this board |

#### 2.23.6 Achievements (DD SPEC; GAP-COOKED)

| Part | Rule |
|---|---|
| File | `Data/achievements.json` (FRP proposed): `{id, name, text, category, trigger{event, params}, reward{title?, cosmetic?}, hidden}` |
| Count | 150 at R10 across 10 categories: Explorer (each biome's landmarks, hidden sub-biomes, secrets), Hunter (each creature killed and tamed), Slayer (each boss, solo, deathless), Path (each path maxed, each combo pair owned), Life (each skill to 100), Builder (claims, monuments, every biome building set), Survivor (Unmade by rift drain and lived to Rest; 100 days survived), Raider (each raid, Triumph prevented), Mythic (each mythic forged), Prestige (each path prestiged) |
| Hidden | 20 are hidden until earned (the Void's: "Count the stones", "The last note", "Rang the silent bell", "Eaten by Vorhollow and lived", "Inside") |
| Where | The Journal's Records tab, plus a 3 s toast at screen top when earned (pad and mouse dismissable) |
| Platform | When Steam/EOS login lands (RAVEN-PLANNED M7), the same ids map to Steam achievements. Until then they're server-saved per character |

#### 2.23.7 Titles (DD SPEC; GAP-COOKED)

| Part | Rule |
|---|---|
| What | One title shown under your name on your nameplate and before your name in chat |
| From | Achievements, prestige, raids, mythic forging, path masters' last quests |
| Choosing | Journal → Records → Titles: a list; A/click to wear, B/Esc back; F1 shows the controls |
| The Void's titles | **Hollow** (Maren's quest 6), **Devourer** (kill Vorhollow), **Inside** (kill Vorhollow while in the Gullet, hidden phase 3), **Unmade** (die to rift drain), **Choirbreaker** (all five Void minis), **Hollow-sworn** (Void prestige 3), **Twelvefold** (forge a mythic) |

#### 2.23.8 Cosmetics and monuments (DD SPEC "cosmetics for hard content"; monuments KEPT)

| Part | Rule |
|---|---|
| No store | Cosmetics come only from hard content. There are no purchases |
| Kinds | **Weapon glows** (12, one per major, from a kill where nobody in the group died: a faint element-coloured edge on any weapon). **Armour dyes** (raids: 14 named dyes). **Claim banners** with boss sigils (the claim's map icon shows them, KEPT). **Raid pieces** (2 per raid: 14). **Prestige gold threads** (2.23.4). **The rubble halo trail** (Vorhollow deathless: your Hollowplate halo leaves a 1 s trail of drifting pebbles) |
| How they're worn | Through the KEPT appearance layer (hero transmog): each cosmetic is a look you can show on any slot that fits |
| Monuments (KEPT survival-crafting) | A major's trophy head on a stone plinth: comfort 3, a bronze plaque with the killers' names and kill time, and the boss sigil on the claim's map icon. Vorhollow's monument is its four-petal head, closed, on a black plinth; the petals open 10 cm when anyone with the Void path stands within 3 m |

#### 2.23.9 Screens (mouse and pad on every one, F1 page, no dead states; ui-controls rules KEPT)

| Screen | Contents | Pad / mouse | No dead states |
|---|---|---|---|
| Altar: Return (prestige) | The path's points, marks, the 3 perk cards, a held Return button | D-pad / stick between cards, A hold 1.5 s; mouse hover and click-hold; B/Esc back | Greyed with the reason when the path isn't at 100 ("Reach 100 points in Void") |
| Raid door | The raid's name, the champions' sigils, your group's members in range, Enter | A / click Enter, B / Esc leave | If another group is inside: "Sealed: a group is inside" plus the time since they entered; never a blank |
| Journal → Records | Leaderboards, achievements, titles: three sub-tabs | LB/RB or Q/E between tabs; lists by stick or wheel | Empty boards say "No kills yet" |
| Ring forging | No screen: world prompts on each pillar ("Offer 10 voidglass") and on the centre stone ("Forge: hold") | Interact | Missing offerings show on the centre stone's prompt as a list of what's missing |
| Riftback pocket | The KEPT Rift chest grid beside your inventory | KEPT inventory controls | "Your clan has no Rift chest yet" if none exists, with the piece's recipe shown |
| Descent lift | World prompts on the crank ("Hold to lower", "Hold to raise", "Repair: rope 10, iron 4") | Interact hold | Broken: shows the repair cost |

---
## 3. Plan: building it in Raven's repo

**Raven's patterns, always (CLAUDE.md):**
- Data first in `Data/*.json` with first-draft numbers, read through `WRTune` (`WRTuning.cpp`).
- Every path ability is a `UWRGameplayAbility` subclass registered in `UWRAbilityRegistry::ClassForNode` (`WRAbilities.cpp:1102`).
- All damage goes through `UWRCombatLibrary::ApplyHit` on the server.
- Statuses are replicated tags owned by `UWRStatusComponent`.
- The server is the authority; clients send intent.
- Sound and FX are named events (`WRSound`, `WRFx`) with data rows.
- Dressing spawns at runtime from data; `L_Island` is touched as little as possible.
- Gates: compile, cook, package. Aaron tests through F8.

**R10 needs in place first (from earlier rounds; not rebuilt here):**
- World-map's B6 terrain step (the Wasteland ground, the Great Rift hole, the Severance gorge) with the west end sealed until this round.
- Combat C0 to C12: the attack-row runner (`attacks.json`, `UWRGA_CreatureAttack`), visible-contact damage, stance and parts, the status framework with Curse (R2), the PvP block, `UWRGA_SelfBuff`, `UWRGA_DashSpell`, the Pull shape.
- survival-crafting: Rest and `OnRest` (R0), `MaxHealthLost` and the Corruption meter (R4), Hearth 5 and starsteel (R4), steel (R2/R4), the Rift chest and Hollow bastion set, monuments, wax earplugs, the appearance layer (hero).
- Fire-frost frame: shrines, path masters, roaming-mini route component, `UWRBiomeEventSubsystem`, `world_pois.json`, `quests.json`.
- R5 artifacts (the `AnyArtifactModifiers` flag gates hidden phase 3).
- Crystal R9: `UWRNoiseSubsystem`, `thinFloor`, `UWRShieldComponent`, the Severance's crystal half, crystal dust, the Lyrathis beam code (reused for the Giantess).

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/void.json`** | Same schema as `frost.json`: `id: "void"`, `name: "Void"`, `biome: "wasteland"`, `starter: "void_bolt"`, `resource {id: "corruption", name: "Void"}`, `rootPassive {id: "empty_vessel", name: "Empty vessel", text: "Corruption builds more slowly, and void abilities cost less max HP."}`. Trunk: `void_bolt (ability 3), void_tolerance (stat 3), unravel_chance (stat 2), null_strike (ability 2)`. Branches: `abyss: void_rift(5,g0), abyssal_power(stat 5,g5), collapse(4,g10), dark_matter(stat 3,g14), event_horizon(3,g17), black_hole(capstone 10,g20)`; `hollow: blink(5,g0), fade(stat 5,g5), phase(4,g10), shadow_step(3,g14), unseen(stat 3,g17), nowhere(capstone 10,g20)`; `devour: consume(5,g0), gluttony(stat 5,g5), sacrifice(4,g10), feed(3,g14), insatiable(stat 3,g17), maw_of_the_void(capstone 10,g20)`. Node `text` copied from ST word for word. Abilities carry `art {fits, resourceMode: "maxhp"}` or `spell {shape, chargeable, roots}` (combat schema) |
| `Data/tuning.json` `resources` | `corruption {max: 100, decayPerSecond: 5, decayDelay: 4, tolerance: 20, tolerancePerRank: 10, damagePerPoint: 0.003, emptyVesselGain: 0.7, emptyVesselCost: 0.75, selfCostLimit: 0.75}`; `gains {void_bolt: 6, null_strike: 8, void_rift: 14, collapse: 14, blink: 5, shadow_step: 8, phase: 8, nowhere: 20, black_hole: 30, maw_of_the_void: 35, consume: -20, sacrifice: 0}` |
| `Data/tuning.json` `nodeEffects` | `void.void_tolerance {tolerance: 10}`, `void.unravel_chance {unravelChance: 0.08}`, `void.abyssal_power {voidDamage: 0.06}`, `void.dark_matter {shieldArmorDamage: 0.10}`, `void.fade {iFrames: 0.03}`, `void.unseen {loseTarget: 0.6, loseTargetBoss: 0.3}`, `void.gluttony {perLostPoint: 0.001, cap: 0.5}`, `void.insatiable {consumeThreshold: [0.28, 0.31, 0.35]}` |
| `Data/tuning.json` `passives` | `unravelled {seconds: 6, bossSeconds: 3, armor: -0.2, stanceTaken: 0.15}`, `eventHorizon {eatsBeams: false}`, `feed {lowland: 3, midland: 5, highland: 8, mini: 25, major: 60}`, `mawEat {threshold: 0.30, restore: 5}`, `voidMaxHpTamePathMagic {minBar: 50, holdSeconds: 4, minPoints: 10}` |
| `Data/tuning.json` `spells` | Void entries KEPT from combat 2.13 plus `maxHp` costs (KEPT), `black_hole {maxHp: 12, bossSlow: 0.3}`, `nowhere {maxHp: 8}`, `phase {maxHp: 3}`, `shadow_step {maxHp: 2, behind: 120}` |
| `Data/tuning.json` `hazards` | `riftZone {drain: 2, unmadeMult: 1.5, passTags: ["Path.Void.EmptyVessel"], halveTags: ["Gear.Nullward", "Set.Hollowplate"], anchorSalt: 0.5, corruptionRate: "night"}`; `riftPull {radius: 600, speed: 180, touchDamage: 40, touchKnock: 600, touchCurse: 40}`; `rubbleDrop {radius: 3000, pulseMin: 40, pulseMax: 90, tell: 1.0, drop: 2.0, damage: 45, impact: 3}`; `litterFire {smoulder: 0.9, downwind: 150, upwind: 50, burnPerSecond: 8, dps: 3, patchSeconds: 60, reburnDays: 2}`; `thinFloor` KEPT with `voidPockets` |
| `Data/tuning.json` `weather` | Wasteland weights KEPT; `greyWind` (KEPT numbers) mapped to the dust row; `deadCalm {hearingMult: 1.5, footstepMix: 2.0, musicFadeSeconds: 8}` |
| `Data/tuning.json` `creatureAI` | Every 2.12.2 row in the 2.18.1 schema. New motion kinds: `blink` (tell, destination ripple, arc 90 to 150°), `rift_burst`, `rift_retreat` (`riftLeash: 3000`, `landMove: 0.6`), `trail_pass` (`length: 3000, seconds: 6, tick: 0.5, damage: 8, curse: 30, minGap: 300`), `latch_drain` (`maxHpPerSecond: 3`), `listen_rise` (`seconds: 1.2, hearMult: 2.0`), `scream` (`radius: 1200, stamina: 50, cancelsDrink: true, cooldown: 14`), `lift_grab`. `hearingHunter: true` for the Hush and Hollowfang. `riftborn {tierUp: 1, hpMult: 1.25, blinkCooldown: 10, curse: 15, desat: 0.7}` |
| `Data/tuning.json` `bosses` | `vorhollow {stance: 170, parries: 3, arcaneMult: 1.5, voidMult: 0.75, shellAt: [0.8, 0.6, 0.4], shellHealth: 1000, shellDamageTaken: 0.2, shellArcane: 3.0, pillarDps: 150, pillarRange: 1200, pillarRing: 3.0, shellSeconds: 20, shellStagger: 3, pieces: 7, orbitSpeed: 120, orbitEase: 6, gapMin: 150, gapMax: 350, touchEvery: 20, bobAmp: 30, bobPeriod: 10, crumbleEvery: 30, smallFallAt: 90, riseAfter: 30, wardSeconds: 20, saltWardSeconds: 10, gulletPull: 200, gulletDrain: 3, gulletContractEvery: 20, gulletContractTell: 1.2, gulletCoughEvery: 30, heartMult: 1.5, bodyMultP3: 0.75, graspMaxHp: 0.15}` plus each move row of 2.14.2; `unravelled {bodyTaken: 0.3, unravelAfter: 300, reform: 1.2, coreWindow: 0.6, coreMult: 3, coreStance: 4, reformSlam: 40, reformRadius: 350}`; `mother_of_rifts {broodSockets: 6, broodMax: 4, broodHealth: 400, spawnEvery: 15, strainStun: 4}`; `starving_one {stackMax: 5, perStack {size: 0.1, damage: 0.1, speed: 0.05}, baitRadius: 1500, eatSeconds: 2.5}`; `null_warden {fieldRadius: 1200, refund: 0.5, gateSeconds: 6, winchSeconds: 8}`; `echo {mimicTell: 1.0, mimicDamage: 0.8, varyCount: 3, failStun: 2.0, loopKm: 2.4}`; run-back stones (2 for Vorhollow, 1 per station); lock points |
| `Data/tuning.json` `events` | `riftStorm {warnSeconds: 300, seeds: 10, open: 6, nearClaim: 60000, claimRange: 150000, waves: 3, waveEvery: 240, heartHealth: 1500, heartArcane: 3.0, heartVoid: 0.5, saltDamage: 300, payout: 3.0, xpPerRift: 1500, sunDim: 0.3}` |
| `Data/tuning.json` `raids` (new) | `{respawnHours: 24, doorSealEmptySeconds: 8, championBase: 12000, perPlayer: 0.8, stance: 180, triumphAt: 0.3, triumphDamage: 0.2, crossDamage: 0.5, runbackDistance: 4000}` |
| `Data/tuning.json` `mythic` (new) | `{offering: 10, marks: 3, channelSeconds: 180, damageMult: 1.15, sockets: 3, elementSplit: 0.5}` |
| `Data/tuning.json` `prestige` (new) | `{marksPerPath: 3, holdSeconds: 1.5, perks {damage: 0.05, resource: 0.10, subRootPassive: 0.5}}` |
| `Data/tuning.json` `records` (new) | `{bands: [1, 2, 4, 7, 10, 99], tabletTop: 3, panelTop: 10}` |
| `Data/server_presets.json` | Add `raidRespawnHours` (1 to 168, default 24), `recordsSeason` (number, default 1). Invasion frequency (KEPT) includes the Arcane/Void pair from R10 |
| `Data/creatures.json` | 9 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `void`, `resists ["rift_zone"]`, taming, difficulty, food, attacks, activity, loot, mesh, anims, voice). Mount: Voidstrider (`rideSpeed 1350`, `stamina 200`, `blink {distance 500, cooldown 8}`). Hauler: Riftback (`carry 250`, `riftChestAccess true`). Utility: Driftmote (`sense ["rift", "cache", "thinFloor", "stormSeed"]`, `range 3000`). Farm: Withergoat KEPT. Riftborn as a **variant rule**, not rows: `riftbornOf ["gravehound", "cryptstalker", "palestrider", "prismback"]`. 6 boss rows: `vorhollow` (major), `unravelled`, `mother_of_rifts`, `starving_one`, `null_warden`, `echo` (`roaming`, `route`); raid champions `cantor`, `hollowed_giantess` |
| `Data/spawn_regions.json` | V1 to V5 with `night` tables (2.12.3); `riftborn` share by region; `raiders` (Gnawer, Hollowfang by band; a Riftmaw never raids) |
| `Data/items.json` | `voidglass`, `greywood`, `greywood_planks`, `grey_salt`, `anchor_salt` (`use{brew{riftDrain 0.5, minutes 5}}`, `throwable{radius 300, closesBrood true, heartDamage 300, wardSeconds 10}`), `void_essence`, `mote_gel`, `hollowfang_hide`, `riftmaw_tooth`, `nullwing_bone`, `nullwing_membrane`, `hush_finger_bone`, `hush_ear_membrane`, `gnawer_incisor`, `riftback_plate`, `voidstrider_filament`, `core_vorhollow`, `void_heart` (boss part), `trophy_head_vorhollow`, trophies (KEPT 5), `gnashblade`, `gnashblade_bp`, `hushfinger_spear`, `nullwing_bow`, `voidglass_dagger`, `riftback_shield`, `beacon_lantern`, `hollowplate_{head,chest,hands,legs}` (KEPT numbers), `champions_mark`, 10 mythic items (`category: "mythic"`, `mythicWildbrand`), lore items (10), `last_note_stone` (decor), `maren_lantern` (decor) |
| `Data/recipes.json` | 2.19.2 and 2.7 recipes; Hollowplate and nullward KEPT inputs; the Descent lift repair as a world-interaction recipe |
| `Data/pieces.json` | No new player pieces (Rift chest, Hollow bastion, monument KEPT). World-only kinds: `voice_pillar`, `anchor_stone`, `records_tablet`, `raid_door` |
| `Data/world_layout.json` (world-map's) | The wedge polygon, V1 to V5 polygons, the Great Rift volume, the Unmade Place volume (`underground` flag for night spawns and the planned near-silence), the Gullet volume (placed 300 m under the rift, unreachable except by the boss's pull) |
| `Data/world_pois.json` | Every POI of 2.4 with x, y, z; 10 landmarks; 24 stable rifts `{pos, height, zoneRadius, ringNodes, pulse}`; 10 storm seeds; floating rubble sets per rift; salt pans; ashen grain stands; mandrake; 4 caves; 30 story sites; 6 secrets; cairn waymarks every 150 m on roads (30 m on the Gauntlet); the Echo's route spline; the island, bridge, 4 chains and anchors, 6 voice pillars; the Descent stair and lift; 7 raid doors and war-halls (the Severance's fully dressed); 1 records tablet per boss |
| `Data/quests.json` | Maren Vey's 6 quests (2.11) |
| `Data/combos.json` | The 11 Void pairs (2.17.5) with `grants`; Paradox `kind: "toggle"` |
| **New `Data/achievements.json`** | The 150 achievements (2.23.6) |
| **New `Data/titles.json`** | Titles with their sources |
| **New `Data/cosmetics.json`** | Weapon glows, dyes, banners, raid pieces, trails: `{id, kind, look, source}` |
| `Data/Audio/sounds.json` | Voice sets for 9 creatures, the Riftborn filter, 5 minis, Vorhollow, the Cantor and the Giantess (2.15); beds per sub-zone; rift inrush (3 sizes), pulse silence, pull whistle, rubble grind, litter smoulder and fire front, hollow drum, glass ring footsteps, chain clink and snap, island grind, voice pillar ring, bell, gate counterweights and chain over pulley, lift drum and pawl; music pools (2.16) |
| `Data/fx.json` | Rift tear (lensed starfield + edge), floating motes, dust streams, lensing ring, rubble keels, voidglass ring sheen, the Great Rift column, Rift Storm column, ink-smear sparks (KEPT), Unravelled fray, Void bolt, Void rift, Collapse, Blink smear, Phase, Nowhere shimmer, Black hole, Maw (real mesh plus FX), Nullwing trail ribbon, Hollowfang blink ripple, the Hush ear-snap dust, Unravelled reform swirl, brood rift, null field ring, mimic reversed-colour plumes, Vorhollow shell lensing, beam, crack glow, chain snap, piece ward hexafoil, Gullet contraction, body-change haze, prestige gold thread, weapon glows (12) |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRAttributeSet.h/.cpp` | **`VoidCorruption`** (the path resource, 0 to 100) beside Heat and Chill. `MaxHealthLost` (KEPT, survival-crafting) feeds `GetEffectiveMaxHealth() = MaxHealth − MaxHealthLost`; health clamps to it. `PostGameplayEffectExecute`: when `MaxHealthLost ≥ MaxHealth`, kill with the `Death.Unmade` tag (no floor) |
| `WRPathComponent.cpp/.h` | **`FWRVoidCost::Pay(ASC, BaseMaxHp, Gain)`**: adds Corruption (×0.7 with Empty vessel as main), compares to tolerance, applies max HP cost ×0.75 (Empty vessel), ×0.75 (Hollowplate), ×(1 − 0.02·Souls) (Soul void), ×0.5 at full Heat (Void furnace); refuses past `selfCostLimit` (returns false; the ability doesn't commit and the slot flashes). Corruption decay tick. `Prestige(PathId, Perk)` at an altar: zero points, keep the node layout, add a mark, replicate. Marks apply through `nodeEffects`-style modifiers only while the path is equipped |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_VoidBolt` (Bolt), `UWRGA_NullStrike` (`UWRGA_Wildbrand`, armour ignore), `UWRGA_VoidRift` (Pool, spawns `AWRVoidRiftPool`, Event horizon projectile delete), `UWRGA_Collapse` (Pull 360°), `UWRGA_Blink` (`UWRGA_DashSpell`, Fade i-frames, Unseen target clear), `UWRGA_ShadowStep` (dash to lock target's back), `UWRGA_Phase` (`UWRGA_SelfBuff`, `State.Phased` makes `ApplyHit` return `Dodged` and grabs miss), `UWRGA_Nowhere` (untargetable tag + next-hit-crit tag), `UWRGA_Consume` (Wildbrand with paired montage via the crit library, threshold from Insatiable, Corruption −20, max HP restore), `UWRGA_Sacrifice` (self buff, always costs), `UWRGA_BlackHole` (Strike with a pull field actor), `UWRGA_MawOfTheVoid` (Cone with a real maw mesh actor `AWRMaw`, eats sub-30% lowland/midland). Every one calls `FWRVoidCost::Pay` in `CommitAbility`. **`ClassForNode`: a new `void` block** mapping `void_bolt, null_strike, void_rift, collapse, blink, shadow_step, phase, nowhere, consume, sacrifice, black_hole, maw_of_the_void`. Passives (`event_horizon, feed, unravel_chance` proc) through `UWRPathComponent::HasNode` like Fire's; stat nodes through `nodeEffects` |
| `WRCombatLibrary.cpp` `ApplyHit` | Void damage scaling (`+damagePerPoint × bar`, Abyssal power, Gluttony); Unravelled proc roll and its armour/resist/stance modifiers; Dark matter vs shield HP (`UWRShieldComponent`, KEPT crystal) and parts; Arcane ×3 vs `Shell` and `RiftHeart` tagged targets; void ×0.5 vs rift hearts; Feed restore on kill credit; Void heart part restore; combos' max HP restores (Void leech share of lifesteal, Hungry spores, Abyssal hunger) |
| `WRStatusComponent.cpp/.h` | `Status.Unravelled` (tag + timed GE). Curse KEPT (R2). Player `MaxHealthLost` HUD event for the grey band |
| `WRBeds.cpp` | `OnRest` (KEPT) → `MaxHealthLost = 0`, `VoidCorruption = 0`; body-change fade event |
| **New `WRRift.h/.cpp`** | `AWRRift` (placed from `world_pois.json`): tear mesh and material, zone sphere (server overlap → a periodic GE adding `MaxHealthLost` at 2/s × multipliers; tags for pass and halve), pull field within 6 m (CMC root-motion source, 180 cm/s), touch damage via `ApplyHit`, **pulse** timer (40 to 90 s, random per rift, replicated as a server timestamp so clients play the silence and shadows in sync), voidglass ring nodes (`AWRResourceNode` children), floating rubble set (HISM with per-instance custom data `Lift`, `Phase`; drop and rise driven by the pulse timestamp; a server sphere trace at drop time applies the 45 damage only where a stone's shadow was), floating motes Niagara inside the zone. `bStorm` variant with a `RiftHeart` actor and wave spawner. Claim placement asks `UWRRiftSubsystem::InZone(pos)` |
| **New `WRRiftSubsystem.h/.cpp`** | World subsystem: rift registry, `InZone`, `NearestRift(pos)` (Riftmaw leash), storm seed list, Driftmote sense queries |
| `WRWorldDressing.cpp` | Litter as a ground layer in V1 with a **fire grid** (`UWRGroundFireGrid`: 2 m cells over V1 only, states unburnt / smoulder / burning / burnt-until; spread by wind vector from `WRSky`; fire hits ignite a cell; server-owned, replicated as burning-cell runs for FX). Thin floors KEPT from crystal |
| `WRCreature.cpp` + `UWRGA_CreatureAttack` (combat) | Motion handlers: `blink` (destination ripple FX at windup start, teleport with an i-frame-free 0.18 s smear, never inside 2 m of a wall), `rift_burst` and `rift_retreat` (uses `NearestRift`), `trail_pass` (spawns `AWRVoidTrail`: a spline ribbon actor with overlap ticks via `ApplyHit`), `latch_drain` (attach to a leg socket, `MaxHealthLost` tick, roll detaches), `listen_rise` (sets `HearMult`), `scream`, `lift_grab`. The Hush and Hollowfang register as `UWRNoiseSubsystem` listeners. Path magic for Hollowfang in `UWRTamingComponent`. Riftborn: on spawn, a `bRiftborn` flag sets the tier up, adds Blink to the attack list, and swaps in the Riftborn MID layer |
| `WRCreature.cpp` (tames) | Riftback: Interact opens the clan Rift chest container (KEPT `WRUIStorage` with the clan's shared inventory id). Voidstrider: mounted Dodge → blink. Driftmote: utility `Sense` tick (2 s) chooses the nearest sensed target and steers; pulse rate replicated |
| `WRBoss.h/.cpp` | `BuildAttacks` rows for `vorhollow`, the 5 minis, `cantor`, `hollowed_giantess`. **Vorhollow:** `AWRRiftIsland` (one actor with 7 piece components). **Before the transition** the pieces are welded and held by 4 `AWRChain` actors (spline mesh chains with a sag curve and clink sounds driven by boss movement). **On the transition** each chain snaps at a chosen link (the server sends the link index; clients swap to two free spline halves with a physics-free whip animation). **Piece motion is a pure function of server time:** `Pos(t) = Centre + Ring(θ0 + ω·(t − t0)) + Bob(t) + Ease(t − t0)`, computed identically on server and clients from replicated `t0` and per-piece `θ0`, so nothing ever snaps or teleports. Touch events and crumble/fall/rise are scheduled timestamps in the same way. The piece ward state (Arcane hit or salt on the keel-stone component) pauses that piece's crumble timer. The shell (`UWRShieldComponent` with the Arcane ×3 and void-heals rules), voice pillars (`AWRVoicePillar`: strike-to-ring state, a 3 s ring, a range check to the boss, dps to the shell, 3 beam hits to break), the beam (Lyrathis's trace beam code, KEPT crystal: a trace from the chest socket sweeping by angle, ticking `ApplyHit`, writing crack decals that mark the split lines), Inhale (a pull root-motion source on players in the cone with a pillar line-of-sight block; flying rubble as pooled actors on curves into the mouth), Lamprey grab (paired montage, max HP cost, drop-out through the chest socket with the knockdown), Rift dive (boss hidden 2.5 s, the burst target piece's ring FX at −1.2 s), P3 Gullet (a pull to the chest socket; entering the chest volume moves the player to the Gullet arena with a 0.4 s tunnel effect; the heart actor shares the boss's health component; contraction ring, cough expel), furthest-tame Inhale target, run-back stones ×2, records timing hooks. **Minis:** Unravelled (one skeletal mesh with a "separation" anim layer that offsets each piece bone; the unravel state hides the body and moves a piece-swirl Niagara along a path, then the reform montage; the core is a separate hittable component active only during the 0.6 s window); Mother of Rifts (6 brood sockets as `AWRRift` storm-type mini rifts with HP, closed by Arcane hit or salt; strain stun when the count hits 0); Starving One (buff strip on hit via `UAbilitySystemComponent::RemoveActiveEffectsWithTags(Buff)`; stack GE; a food-bait perception sense on dropped food pickups); Null Warden (`UWRNullFieldComponent`: a sphere that blocks activation of spell-tagged abilities with a refund, kills projectile actors entering it, and strips element from Wildbrand hits; the gate as `AWRCounterweightGate` with chain splines over pulley meshes, counterweight stones on a timeline, opening on the warden's death, closing on respawn, and a bridge-side winch interact); Echo (stores the last player ability id that hit it; a `mimic` table maps each player spell or Wildbrand to an enemy row with the projectile rules enforced; a vary-counter ring buffer of the last 3 ids; route via the fire-frost route component) |
| **New `WRRaid.h/.cpp`** | `AWRRaidDoor` (seal on the first threshold cross, open on fight end or 8 s empty, "Sealed" state replicated for the door screen), `AWRRaidEncounter` (owns 2 champion bosses, the cross-damage rule ×0.5, Triumph at 30%, its own respawn timer from the preset), run-back stone |
| **New `WRMythic.h/.cpp`** | `AWRRingPillar` ×12 (offering slot per pillar, owner kept), `AWRRingForge` (the centre stone: checks offerings, marks and base item; a 3 min channel broken by damage; lights the 12 beacon columns through `WRSky`'s KEPT beacon calls; produces the mythic item with two elements and its Wildbrand id) |
| **New `WRRecords.h/.cpp`** | `UWRRecordsSubsystem` (server, saved per world in the KEPT JSON save): kill entries `{bossId, band, seconds, artifactLevel, names}` from `WRBoss` and raids; top lists; the season number. `AWRRecordsTablet` (shows the top 3 as text on a stone material via a render-target text, interact opens the panel) |
| **New `WRAchievements.h/.cpp`** | `UWRAchievementSubsystem` reads `achievements.json`, listens to game events (kills, tames, discoveries, quests, path points, deaths with tags like `Death.Unmade`), grants per character, rewards titles and cosmetics, sends the toast |
| `WRPlayerState.cpp/.h` | Replicated `Title`, `PrestigeMarks` (per path), owned cosmetics, achievement bits |
| UI | `WRUIPaths.cpp`: the Void tree page and bar. `WRUIHud.cpp`: the grey hatched max-HP band, the Void bar, the refusal flash. `WRAltar.cpp` + a new altar Return screen (2.23.9). `WRUIJournal.cpp`: Records tab with three sub-tabs. A raid door screen. Riftback pocket via `WRUIStorage.cpp`. All mouse and pad, F1 pages (`WRUIControls.cpp`), no dead states |
| `WRBiomeEventSubsystem` (fire-frost) | `RiftStorm` event: seed choice by claim proximity, storm rifts, wave targets fixed at spawn (navmesh path query, fallback the nearest shrine), heart HP and payout, the Ring pillar's black-violet colour |
| `WRSky.cpp` | The wasteland sky preset (flat grey, small white sun, no moon), the Great Rift column (a tall refraction cylinder with a slow-turning ring texture), the sun dim during the Rift Storm |
| `WRAudioDirector.cpp` | Dead calm music fade (8 s) as a planned-silence state with the combat-in-silence rule; the Void cues (2.16); the road hum that rises per station passed |
| Hero (hero lane's `MF_PathMarks`) | The Void layer reads `Corruption` = the 2.18 intensity (bar, `MaxHealthLost`, world meter in the Wasteland); prestige gold thread param |

### 3.3 Assets and the art route

**Route for every creature and boss (KEPT bar):**
1. Concept sheets: side, front, at scale beside the hero, silhouette test in black. Aaron picks.
2. A Hunyuan3D 2.1 base from the picked concept.
3. Blender finish: clean topology, anatomy fixed to the real animal references in 2.12.1, materials in the palette.
4. `Tools/art` rig scripts.
5. Real animation clips: keyed in Blender on the rig, or a Fab animal pack retargeted where the body fits.
6. No procedural attack clips, feet planted, gait matched to ground speed.

| Asset | Count | Route and notes |
|---|---|---|
| Hollowfang | 1 model, ~22 clips (idle, walk, trot, run, bite ×2, lunge, blink tell and arrive, hamstring, flinch ×2, stagger, stance break, knockdown, get up, parried, crit victim, death, sleep) | Fab wolf or greyhound clips retargeted (quadruped canine fits); the blink tell is keyed |
| Riftmaw | 1 model, ~16 clips | Keyed (no pack has a ring-mouth burster); the four jaw-plates are bones with corrective shapes |
| Nullwing | 1 model, ~18 clips | Fab vulture or bat flight clips retargeted; dive and trail-pass keyed |
| Gnawer | 1 model, ~10 clips | Fab rodent pack retargeted; latch keyed |
| The Hush | 1 model, ~20 clips | Keyed from aye-aye and primate reference; the ear fans are bone chains with a cloth-free keyed unfold |
| Voidstrider | 1 model, ~20 clips (incl. riding) | Fab horse pack retargeted (the Palestrider's rig if the graveyard lane built it); filament mane as hair cards with WPO drift |
| Riftback | 1 model, ~14 clips | Keyed on a tortoise-type rig (the Basaltback's rig as a base); the seam is a material with a parallax depth "too deep" effect |
| Driftmote | 1 model, ~6 clips | Keyed; the bell pulse is a morph |
| Withergoat | 1 model, ~12 clips | Fab goat pack retargeted |
| Riftborn layer | 1 material function | Applied to 4 existing creatures |
| Vorhollow | 1 model (hero-asset quality), ~40 clips | Keyed in Blender; the tail is 14 bones with the gap rule; the chest hole is real geometry with a starfield material inside. Shown at scale before behaviour |
| Unravelled, Mother of Rifts, Starving One, Null Warden, Echo | 5 models, ~20 clips each | Same route. The Mother reuses the Riftmaw rig scaled; the Starving One uses a primate rig (Fab gorilla clips retargeted, keyed attacks); the Echo uses a bird-biped rig |
| The Cantor, the Hollowed Giantess (raid) | 2 models | The Cantor from the L7 ash cast mesh; the Giantess from the crystal lane's Hollowed Giantess mesh (KEPT), rigged |
| Maren Vey | 1 MetaHuman (hero lane's pipeline) with the Void body change at 1.0 on the left side | MetaHuman preset tuned (attractive, real-world); Hollowplate cuirass and linen shirt from the hero kit |
| Withered Forest kit | Dead Scots pines ×6 variants (12 to 22 m), hawthorns ×3, root plates ×4, litter ground layer, burned variants | A Fab dead-pine pack re-textured silver-grey with needles kept on (needle cards); litter as a landscape layer plus RVT-blended clumps |
| Choir architecture kit | ~40 modular pieces: paving with chant-grooves, steps, amphitheatre rings, hollow fluted columns, horn-tower rings, arches, seats, gatehouse, bell, pens, lectern, offering bowls, point-down cairns ×6 | Concept → Blender modular kit (hard-surface, bevels, dolomite material with voidglass inlay); statues (the Colossus, seated dead, the last note) as Hunyuan3D bases finished in Blender |
| Voidglass | Ring sheets ×6, shards ×10, keels ×6, Glass Sea tiles, cave seam | Blender fracture → baked; a glass material with an edge-fresnel violet rim and screen-space refraction |
| Floating rubble | 30 rock variants with keels | Fab rock packs plus Blender-added keels |
| Rifts | 3 tear sizes, the Great Rift column, the storm rift | Niagara plus a refraction material (screen-space distortion ring), a starfield inside |
| Grey Strand kit | Black sand material, salt pans, rakes and cones, the 3 wrecks (newcomer ship kit, KEPT world-map), the Black Beacon | Fab beach and wreck kits recoloured; the Beacon lens in Blender |
| The island and machines | The island (one piece mesh split into 7 along the beam-crack lines, pre-fractured in Blender so the split is exact), 4 chains (link mesh on splines), 4 anchor stones, the chain bridge (slab + stirrups), 6 voice pillars, the Descent stair and lift (drum, axle, bearings, ratchet, pawl, jib, cage), the Null Gate (portcullis, pulleys, chains, counterweights, shafts, winch) | Blender hard-surface; every moving part pivots at its real axle |
| Weapons and gear | Gnashblade, Hushfinger spear, Nullwing bow, Voidglass dagger, Riftback shield, Beacon lantern; Hollowplate ×2 bodies (hero lane); the 10 mythics | Concept → Hunyuan3D 2.1 → Blender (combat lane's weapon route) |
| UI | Void path medallion, tree icons (22), status icon Unravelled, records tablet font, achievement icons (150, one style sheet), title plates | The ui-controls style |

### 3.4 Sound and music

- **Voices.** 2.15 sets, built from licensed real recordings: Sonniss GDC bundles, Freesound CC0 and CC-BY, own Foley. Each set has 4 layers per attack plus pain, death and call.
- **Hit sounds.** The Void hit set adds a reversed whoosh and a low thump; void creatures use flesh, and Riftmaw and Vorhollow use flesh + glass.
- **Beds.** Five sub-zone beds plus a rift layer by distance (3 rift sizes), the Unmade Place bed, and the Gullet bed (a heartbeat and wet creaks).
- **Machines.** Chains, the island grind, pillar rings (a 3 s bronze-stone tone at A♭2), the gate counterweights and pulleys, and the lift drum and pawl, all recorded or designed from real metal and stone sources.
- **Music.** The cues in 2.16 are commissioned and composed. The brief per cue (length, stems, instruments, the Choir motif notation) goes to the composer route the music lane uses. They're implemented as music pools in `sounds.json` with the KEPT ducking and LUFS rules.

### 3.5 Map dressing

- **Placed in `world_pois.json`, spawned at runtime** (KEPT pattern): the wedge's POIs, rifts, rubble, cairns (every 150 m on roads, 30 m on the Gauntlet), camps, caves, landmarks, the Great Rift set, the Descent, and the Severance west half.
- **Litter and ground.** The ground layers come from the terrain step: litter, black sand, salt crust, Choir paving decals, glass sheets.
- **The Glass Sea and the Great Rift hole** are terrain holes plus meshes.
- **Night.** Every POI has its night light or skyline shape (2.4).
- **No claims** in rift zones or the Severance corridor (data flags read by `WRBuilding.cpp`).

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests through F8)

**Proof of one (first): the Void bolt end to end.** It touches every new core piece at once:
- The `void` path file and `ClassForNode` entry.
- The Void bar with tolerance.
- `FWRVoidCost::Pay`.
- `MaxHealthLost` shown as the grey band.
- Rest restoring it.
- The ink-smear hit stack.

When Aaron's F8 reports say it reads and feels right, every other Void ability uses the same path.

| Step | Build | Proof it's in the build |
|---|---|---|
| 1 | **Proof of one:** `void.json` (trunk only), `VoidCorruption`, `FWRVoidCost`, Void bolt, HUD bar and grey band, `OnRest` clear | (build) |
| 2 | Full Void tree: all 12 abilities, passives, stat nodes, combos (11), body-change driver, Unravelled | (build) |
| 3 | Terrain dressing pass for the wedge: V1 to V5 kits, landmarks, camps, caves, cairns, story sites; the Severance west rockfall removed (every pass open) | (build) |
| 4 | Rifts: `AWRRift`, zones, pull, pulses and rubble, voidglass rings, the floating rule dressing, `UWRRiftSubsystem`; anchor salt; nullward; Hollowplate | (build) |
| 5 | Hazards: litter fire grid, hollow crust pockets; weather (Grey wind mapping, Dead calm with the music fade) | (build) |
| 6 | Creatures: Hollowfang first (its blink is the new motion), then the Riftmaw, Hush, Nullwing, Gnawer, Voidstrider, Riftback, Driftmote, Withergoat; Riftborn layer; spawn tables; voices | (build) |
| 7 | Shrines, Maren Vey and her 6 quests; the Unmade Place and the Descent lift; secrets | (build) |
| 8 | Minis: the Unravelled, Mother of Rifts, Starving One, Null Warden with the gate, the Echo with its route; run-back stones | (build) |
| 9 | Vorhollow: the island, chains, bridge, voice pillars, P1, the transition, P2's time-driven pieces, P3's Gullet behind the artifact flag; drops, altar, monument head | (build) |
| 10 | The Rift Storm; the Severance invasions switched on (both sides) | (build) |
| 11 | Raid frame and the Severance raid (the Cantor and the Giantess) | (build) |
| 12 | Mythic forging at the Ring and the 10 mythics; prestige at the altars | (build) |
| 13 | Records (leaderboards and tablets), achievements, titles, cosmetics; Journal Records tab | (build) |
| 14 | The other six raids, one per later cook, on the step-11 frame | (build) |

### 3.7 Every build step, listed (for the build lane)

1. Add `VoidCorruption` to `UWRAttributeSet`; effective max HP from `MaxHealthLost`; the `Death.Unmade` kill.
2. Write `FWRVoidCost::Pay` in `WRPathComponent` with tolerance, Empty vessel, the self-cost limit and the refusal event.
3. Create `Data/paths/void.json` (trunk first) and the `resources.corruption` block.
4. `UWRGA_VoidBolt` + the `void` block in `ClassForNode`; the Void bolt FX and sounds.
5. HUD: the Void bar, the grey hatched band, the refusal flash; F1 page entry.
6. `OnRest` clears `MaxHealthLost` and `VoidCorruption`; body-change fade event.
7. Branches: the remaining 11 abilities, 3 passives, 8 stat nodes; `nodeEffects`; Unravelled status.
8. `combos.json` Void pairs with their hooks in `ApplyHit`, `FWRVoidCost` and the relevant abilities; the Paradox toggle on the wheel.
9. Body-change driver into `MF_PathMarks` (hero lane param).
10. Wedge dressing from `world_pois.json`: kits, landmarks (10), camps (3), caves (4), story sites (30), kit POIs (81), cairns, secrets (6).
11. Remove the Severance west rockfall; dress the Hollow Fort, the cage, the war rack.
12. `AWRRift` + `UWRRiftSubsystem`: zones, pull, pulse sync, rubble HISM, ring nodes, motes; claim block in rift zones.
13. Items and recipes: voidglass, greywood, grey salt, anchor salt (eat and throw), nullward, Hollowplate, weapons, Beacon lantern.
14. `UWRGroundFireGrid` for V1 litter; thin-floor pockets in V4.
15. Weather: Grey wind as the dust row; Dead calm hearing, footsteps and the 8 s music fade.
16. Creature rows in `attacks.json` and `creatures.json`; new motion handlers; Hollowfang first, then the other eight; noise listeners; path magic; tame utilities (Riftback pocket, Voidstrider blink, Driftmote sense).
17. Riftborn variant rule and material layer; spawn tables V1 to V5 day and night.
18. Voice sets and beds into `sounds.json`.
19. Shrines (5) and their guards; Maren Vey (MetaHuman, idle, lines) and `quests.json`.
20. The Unmade Place volume and 8 POIs; the Descent stair; the Descent lift machine (drum, pawl, jib, cage) with repair and crank.
21. The 4 station arenas and mini bosses; the Null Gate's counterweight machine and winch; the Echo's route; run-back stones.
22. `AWRRiftIsland`, `AWRChain`, the bridge, `AWRVoicePillar`; Vorhollow's model, clips and attack rows; the shell, beam, Inhale, grab drop-out, tail and nullstones.
23. The transition: chain snaps at chosen links, the pre-fractured split, time-driven piece motion (server time function), wards on keel-stones, crumble and rise schedules.
24. P2 moves: delayed slam, Rift dive with the ring tell, twin-beam sweep, the new combo, tame punish.
25. P3: the Gullet volume, the pull into the chest, contraction ring, cough expel, shared heart health, damage splits; gated by `AnyArtifactModifiers`.
26. Drops: core, essence, Void heart part (kill restore), trophy head, first-kill flying drop; the Void altar on the Stage piece.
27. Rift Storm event in `UWRBiomeEventSubsystem`: seeds, storm rifts, fixed-target waves, hearts, payout, the Ring pillar's colour, the sun dim.
28. Invasions: the Arcane/Void pair on; Void-side roster and Echo leader; Void-side warning signs.
29. Raid frame: `AWRRaidDoor`, `AWRRaidEncounter`, run-back; the Severance war-hall; the Cantor and the Giantess rows; Champion's marks.
30. Ring forging: 12 pillars, the centre stone, the channel, beacons; 10 mythic items and their Wildbrands.
31. Prestige: the altar Return screen, marks and perks, gold threads, titles.
32. Records: subsystem, tablets at every boss, Journal tab; the season setting.
33. Achievements (150) and toasts; titles list; cosmetics and the appearance-layer hookup; Vorhollow's monument head.
34. Music: the Void cues implemented with stems, the Dead calm rule, the road hum, the boss sections and the Shepard Inhale layer.
35. Spot-check (3.8), then compile, cook, package, and ship to Aaron's build.

### 3.8 Spot-check against the plan and the bar (read the code, no test suites)

| Check | Look for |
|---|---|
| Damage on contact | Every creature and boss attack's damage comes from a socket trace, beam trace or projectile overlap, never from a shape test alone |
| Projectiles | Every enemy projectile row: ≥ 0.9 s windup + travel, ≤ 1,600 cm/s, `bHoming` false, nothing under 400 cm; the Echo's mimic table enforces the same |
| Tells | Every windup over 0.5 s has a sound cue and a glint; crimson on every unblockable (Lamprey lunge, Four-jaw grab, Lift, the minis' grabs, the Gnashing) |
| No floors | `MaxHealthLost` can reach max HP from rifts, Gnawers and bosses; only the self-cost limit refuses, and only for your own abilities |
| No helpers | No buff, drop or difficulty change after a player death anywhere in the Wasteland |
| No token | Packs pick attacks on their own timers |
| Smooth motion | Island pieces, the lift cage, the gate, the Hanging Wood, Viaduct stones and the Echo route are all time- or timeline-driven; nothing moves by setting a position on a tick without interpolation |
| Machines | Chains end at eyes, ropes run over the jib to the drum, counterweight chains run over pulleys; nothing passes through geometry |
| UI | Every new screen works by mouse and pad, has an F1 entry, and shows a reason where it would otherwise be empty |
| Music | Always audible except Dead calm; duck ≤ 4 dB |
| Ledger | Every row in 0 gets its file:line before the push |

---

## 4. Out-of-scope findings (one line each appended to `C:\Users\acars\wildreach-night\LATER-COOKS.md`)

- Grey wind's 30% chance (survival-crafting) vs the world-map's 10% dust-fog weight: this lane maps Grey wind onto the dust row (10%); the weather owner should confirm.
- The Void armour set: survival-crafting calls it "Voidglass", hero renamed it "Hollowplate"; this lane uses Hollowplate with survival-crafting's numbers. Survival-crafting should adopt the name.
- Raids: two single-element champions who fight each other is our reading of Raven's "no cross-biome endgame bosses"; this needs Raven's ruling (FULL-GAME conflict 22).
- Raids: Raven's fixed 10% loot rule means some players in a 10-player group may get no loot; the loot owner should decide whether healing or stance damage counts.
- The other six raids need their full move tables from their biome lanes on the 2.23.2 frame.
- The self-cost limit (own abilities refused past 75% max HP lost) is a resource rule; the combat lane should confirm it reads as a limit, not a floor.
- FULL-GAME lists 8 Void creatures; this lane adds the Hush (9) and Riftborn variants (CAL "corrupted variants"); the coordinator should update FULL-GAME.
- The Rift Storm's border seed reaches Graveyard bases; the graveyard lane should know.
- Prestige marks work only while that path is equipped (max 6 active); progression owner to confirm against DD "permanent perks".
