# Cook: biome-crystal (Crystal caves / Arcane, plus the PvP pass, round R9)

Lane: biome-crystal. Written 2026-11-13/14. Scope: FULL-GAME 3.2.6 (Crystal caves), the Crystal rows of 3.3, the Arcane row of 4.2, and FULL-GAME 11's R9 line: rune sockets and the PvP pass (clan ranks and alliances, anti-zerg limits, explosives and raid tools, siege, path-based raiding, tame raiding, bounties and infamy, contested bosses).

**Tags.** **SPEC** = Raven's design or a built thing, kept exactly as written. **GAP-COOKED** = this lane's design, replacing a FULL-GAME first draft. **KEPT** = a name or number another 11-13 cook already set (world-map, combat, survival-crafting, hero, ui-controls, biome-fungal, biome-forest). It is restated here so the build has one page, and this lane does not change it.

**What this lane builds on (KEPT, not re-decided):**
- **world-map.md:** the Crystal caves are the **east half of belt B6, the Broken End**, mostly underground. FULL-GAME's "north-east" is replaced. 1.0 km² of surface, about 2 km² of caves. Neighbour: the Ancient forest to the north (border at y +4,600: crystal-veined boulders under moss). Opposite: the Void, across **the Severance** (rift-cut gorge, centre (+1,250, +5,400), saddle 20 m, 1,100 × 80 m; ward fields flicker at its east end). Camps on cliff mouths over the sea at (+2,250, +4,900), (+2,200, +5,500), (+2,000, +6,000). Lyrathis's hall of prisms at (+1,700, +5,300), −120 m; the Silent Vault at (+1,600, +5,900), −180 m. 76 POIs (23 surface + 53 in the caves on 8 km of route). Surface weather Clear 40, Overcast 20, Rain 30, Fog 10; underground none. Hazard numbers 25 damage and a 600 cm throw every 1.5 s; passes: Arcane path, ward charm; early pass **attuning chalk** (5 min: no throw, damage ×0.5). The old people are the **Wardcarvers**, waymark **ringing chime stones every 150 m**. At the Severance: a void-shape's ash cast warded in a crystal cage (Void side) and a crystal giantess hollowed from the inside (Crystal side). Night light: crystal hum light. The Silent Vault is one of the map's three planned silences. No claims or bases underground (DD).
- **biome-fungal.md 3.0:** the **cavern frame** (Nanite cave kit in World Partition cells, landscape holes, `UWRUndergroundSubsystem` with `IsUnderground(pos)`, climb lanes, per-hall reverb). R9 reuses it; it is not rebuilt.
- **combat.md:** the Mark status (+15% damage from all sources for 10 s, visible through walls to the marker's party), every Arcane spell number (2.13), the Runic strike and Inscribe Wildbrands (2.12), the creature attack-row schema (2.18.1), the boss template (2.17.1, 2.17.5), visible-contact damage and small cues (2.17.1b), colour law (gold = yours, crimson = unblockable), the PvP block (2.20), hit feel (2.14), sparks by surface (crystal = glass ring + prism glints).
- **survival-crafting.md:** Wardcloth (set, inputs, bonuses), ward charm, the Mark-resist potion (Candle rhubarb + resonant crystal), Candle rhubarb and Glowmelon (dark crops), Chimecrab produce (crystal dust every 15 min), blind cavefish, glass shrimp and the Prism Eel in the Silent Vault's lake, crystal lamp, Resonance pylon, Geode sanctum, the forcing shed / glass lake / rune smithy set-pieces, the 8 crafted runes, explosives, siege, raid costs, path and tame raiding, turrets, the raid economy (black powder = sulfur + saltpetre + charcoal), wax earplugs.
- **hero.md:** Arcane colour violet #7A3FD1; the Wardcloth silhouette hook (high collar with crystal points, rune-sewn long coat); the Arcane body-change row (mask B, rune lines on the forearms).
- **ui-controls.md:** the Clan tab (key O), the message stone, F1 and F8, mouse and pad on every screen.

---

## 0. Requirement ledger

Every requirement from the brief, Aaron's quotes in it, and the lane frame, split one per row. "Answered in" points at the section of this doc. "Build proof" is filled with file:line or commit when the round is built; a row without proof is not done.

| # | Requirement (source) | Answered in | Build proof |
|---|---|---|---|
| 1 | Full cook: research, then plan, then build, spot-check; no skipping (brief, Aaron 11-13 night) | 1, 2, 3, 3.8 | (build) |
| 2 | Research names the games, the specific places and systems, and why they work; real-world reference where it applies | 1.1, 1.2 | n/a (doc) |
| 3 | Research doc holds the specific details used | 1, 1.3 | n/a (doc) |
| 4 | Plan solves every design gap before building (layout, composition, materials, colours, sounds, motion, behaviour, routes, prompts, edge cases) | 2, 3 | n/a (doc) |
| 5 | Every build step listed | 3.7 | n/a (doc) |
| 6 | SPEC items kept exactly; every GAP replaced with the best design | 2.0 scope map | (build) |
| 7 | Look of the biome | 2.1 | (build) |
| 8 | 3 to 5 sub-zones, each its own ground, palette, props, creature mix, bed | 2.2 | (build) |
| 9 | A POI every 150 to 250 m | 2.4 | (build) |
| 10 | Landmarks | 2.3 | (build) |
| 11 | Hidden areas (hidden sub-biome, secrets) | 2.5 | (build) |
| 12 | Resources (signature + others) | 2.6, 2.7 | (build) |
| 13 | Hazards | 2.8 | (build) |
| 14 | Weather | 2.9 | (build) |
| 15 | World event(s) | 2.10 | (build) |
| 16 | Shrines and path master | 2.11 | (build) |
| 17 | Full creature roster (about 9) with attack lists and timings | 2.12 | (build) |
| 18 | Mini-bosses | 2.13 | (build) |
| 19 | Major boss: phases, arena, moves | 2.14 | (build) |
| 20 | Unique creature voices (fused real animal calls) | 2.15 | (build) |
| 21 | Biome music: real composed, always audible, ducks ≤ 4 dB, only planned silences | 2.16 | (build) |
| 22 | Path skill tree: every node with numbers | 2.17 | (build) |
| 23 | Path body change | 2.18 | (build) |
| 24 | Gear and armour set from this biome | 2.19 | (build) |
| 25 | Story told only through the environment | 2.20 | (build) |
| 26 | Fits Raven's biome and path rules (DD, ST, CR, BO, CF) | 2.0, 2.12 rules, 2.17 | (build) |
| 27 | FULL-GAME 3.2, 3.3 and 4.2 rows for this biome | 2.0 | n/a (doc) |
| 28 | Combat bar: full hit stack, hitstop 0.06 to 0.30 s, visible coil (0.14 light / 0.20 heavy), one verb per button, 0.33 s buffer | 2.12 rules, 2.17 rules (KEPT combat) | (build) |
| 29 | Attacks show the real weapon, claw or projectile; damage only on visible contact; telegraph is a small cue; tells read at 15 m | 2.12, 2.13, 2.14 | (build) |
| 30 | Projectiles give ≥ 0.9 s warning and never home | 2.12, 2.13, 2.14, 2.17 (reflection rule) | (build) |
| 31 | No attacker limit; packs aggro freely | 2.12 rules | (build) |
| 32 | Players die; no safety floors; no helpers after a fail | 2.8, 2.14, 2.21 | (build) |
| 33 | Monsters terrifying, readable silhouettes, believable anatomy, no slop, no primitives | 2.12.1, 2.13, 2.14, 3.3 | (build) |
| 34 | Real animation clips, no gliding | 3.3 | (build) |
| 35 | UI: mouse and pad on every screen, no dead states, F1 | 2.21.9, 3.2 | (build) |
| 36 | People attractive, naturally sexy, never sanitised | 2.11, 2.19 | (build) |
| 37 | Weapon skills called Wildbrands; taming as Raven designed; no spirit summons | 2.12, 2.17 | (build) |
| 38 | Design with intention: why each thing is here, what it connects to, how it meets what it touches (Aaron 11-14) | 2.1 grand puzzle, 2.3 to 2.5, 2.14.1, 2.21.5 | (build) |
| 39 | Machines look engineered; parts mesh; nothing passes through anything; moving parts move smoothly, never snap | 2.14.1 turntables, 2.13.1 lens gimbal, 2.21.5 siege | (build) |
| 40 | PvP: clan ranks | 2.21.1 | (build) |
| 41 | PvP: alliances | 2.21.2 | (build) |
| 42 | PvP: anti-zerg limits | 2.21.3 | (build) |
| 43 | PvP: explosives and raid tools | 2.21.4 | (build) |
| 44 | PvP: siege weapons | 2.21.5 | (build) |
| 45 | PvP: path-based raiding | 2.21.6 | (build) |
| 46 | PvP: tame raiding | 2.21.6 | (build) |
| 47 | PvP: bounties and infamy | 2.21.7 | (build) |
| 48 | PvP: contested bosses | 2.21.8 | (build) |
| 49 | R9 shared system: rune sockets | 2.19.3 | (build) |
| 50 | R9 combos: the 10 new Arcane pairs | 2.17.5 | (build) |
| 51 | Invasions Arcane ↔ Void (DD) | 2.22 | (build, R10 switch) |
| 52 | Plan in Raven's repo: data rows, C++ (UWRGameplayAbility + ClassForNode, ApplyHit, data-driven), assets and art route, sound and music cues, map dressing, build order with proof-of-one first | 3 | n/a (doc) |
| 53 | Don't edit Raven's repo or FULL-GAME, no git, no tests, builds, renders | lane rule (followed) | this doc only |
| 54 | Out-of-scope findings → one line each in LATER-COOKS.md | 4 | LATER-COOKS.md |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game / place / system | What it does and why it works | What we take |
|---|---|---|
| **Dark Souls, the Crystal Cave** ([Fextralife](https://darksouls.wiki.fextralife.com/Crystal+Cave), [Giant Bomb](https://giantbomb.com/wiki/Locations/Crystal_Cave)) | Invisible walkways over a drop, found by watching the constant falling crystal "snow" land on them, plus the sound of it striking. The cue is the world itself, never a UI mark. Elden Ring repeats it at the Heretical Rise ("falling snow marks something unseen") | **Ward fields are read by the world:** fine crystal dust on the floor jumps into Chladni lines along a ward's edge as you near it, and the chime stones beside it hum. No floor paint, no UI. Lesson: a hidden thing is fair if the world shows it to a player who looks |
| **Elden Ring, Crystalians** ([Fextralife: Lesser Crystalian](https://eldenring.wiki.fextralife.com/Lesser+Crystalian), [wiki.gg](https://eldenring.wiki.gg/wiki/Crystalians)) | About 90% damage reduction to everything except Strike until the first stance break; the crystal shell cracks visibly; blunt weapons are the answer | **Crystal armour is a part with stance, and strike damage breaks it** (Prismback plates, the Geode Tyrant, the Twins). We use a shell that reflects and resists, not a 90% wall: plates take strike ×1.4 and slash ×0.7 so every weapon still works, but hammers win |
| **Elden Ring, Rennala in Raya Lucaria** ([Game Informer](https://gameinformer.com/guide/2022/03/02/how-to-beat-rennala-queen-of-the-full-moon-elden-ring-boss-guide), [PushSquare](https://www.pushsquare.com/guides/elden-ring-how-to-beat-rennala-queen-of-the-full-moon)) | Phase 1: a floating queen behind a shield you break by hitting the glowing students around the hall. Phase 2 at half health: a new arena and a new rhythm. The shield phase is a puzzle, not a damage sponge | **Lyrathis's dome is broken by the room, not by DPS:** turn a prism so her own beam hits her dome. Phase 2 changes the space itself (her refracted copies stand on the prisms' light lines) |
| **Elden Ring, legacy dungeons and stakes** (CAL S15) | A lit far goal in every hall; shortcuts loop back; a respawn beside the boss door | Every hall has one far light (here: the crystal that is ringing loudest). Run-back stones at both Prism Hall doors |
| **Tears of the Kingdom, the Depths** ([Zelda Dungeon: Lightroot](https://www.zeldadungeon.net/wiki/Lightroot), [Brightbloom Seed](https://www.zeldadungeon.net/wiki/Brightbloom_Seed)) | Total dark. You throw Brightbloom seeds to light the ground, and lighting a Lightroot reveals the map around it and acts as a checkpoint. The tension is light as a resource | **Sound is the light source.** Crystals glow when sound reaches them, so you light a hall by making noise: striking a crystal, ringing a chime stone, a Lumenhorn's bellow. Lighting a hall's **keystone chime** keeps it lit and reveals the map round it (our Lightroot). The price: noise is what Echobats and the Echo Warden hunt |
| **Monster Hunter Wilds, the Ruins of Wyveria and Zoh Shia** ([MH wiki: Zoh Shia](https://www.monsterhunterwiki.org/wiki/Zoh_Shia_(MHWilds)), [Dragontorch Shrine](https://monsterhunterwiki.org/wiki/Dragontorch_Shrine)) | A crystalline ancient ruin and a boss whose attacks leave crystals that its own element can detonate: the arena changes because of the fight | Lyrathis's Glint lance stays in the floor as a new small prism for 20 s, and her copies can stand on its line. The fight builds its own geometry |
| **Monster Hunter World environment traps** (fire-frost cook, KEPT) | Fixed traps you learn (shoot crystals onto a monster) | **Shard chandeliers:** make a loud noise under one and it drops. The Geode Tyrant's counter is a place on its route |
| **Valheim, Ark, Rust, Conan, V Rising: the PvP base game** ([Rust raid chart](https://www.gameserverkings.com/knowledge-base/rust/rust-raid-chart/), [ARK Alliance](https://ark.wiki.gg/wiki/Alliance), [Conan clan ranks](https://guildorder.com/games/conan_exiles/wiki/clan-ranks-and-permission-limits), [V Rising siege golem](https://vrising.wiki.spellsandguns.com/Siege_Golem_Stone)) | **Rust:** every explosive has a sulfur price, so raiding is an economy. **Ark:** tribes have rank groups with per-structure required ranks; alliances hold up to 12 tribes and can be switched off by a server setting. **Conan:** 4 fixed ranks (Recruit, Member, Officer, Leader) and players complain they can't gate doors and chests by rank. **V Rising:** siege needs a crafted, visible, slow engine; a stone takes 150 s to activate and the server may be told | **Ranks:** 4 fixed ranks (Conan's names) **plus** Ark's per-chest and per-door minimum rank, the thing Conan players asked for. **Alliances:** Ark's model, capped small (3 clans) for anti-zerg, with a server switch. **Raid economy:** KEPT from survival-crafting (sulfur from Volcanic, saltpetre from cave bat roosts: this biome's Echobat roosts are the richest saltpetre in the game). **Siege:** slow, visible, crewed engines, never instant |
| **Sea of Thieves, the Reaper's Mark** ([wiki.gg](https://seaofthieves.wiki.gg/wiki/Flag_Of_The_Reaper%27s_Mark)) | A flag that shows your ship on every crew's map table: you choose risk and become prey | **Infamy marks** that show a hunted player's rough position on every map, earned by unprovoked kills, not chosen |
| **Dark Souls, sin and the Blades of the Darkmoon** ([Fextralife](https://darksouls.wiki.fextralife.com/Blade+of+the+Darkmoon), [Way of Blue](https://darksouls3.wiki.fextralife.com/Way+of+Blue)) | Invading or killing innocents makes you "indicted"; a covenant is summoned to hunt sinners. Punishment is other players, not a stat penalty | Infamy is answered by **bounties other players collect**, and by losing the safe-zone camps as a wake point. The world never fines you; players do |
| **Guild Wars 2, Outmanned** ([GW2 wiki](https://wiki.guildwars2.com/wiki/Outmanned)) | A side that is greatly outnumbered gets a small effect; players criticise that big groups split to game it | Our anti-zerg is **hard caps** (clan size SPEC, alliance cap) plus a small **Outnumbered** effect measured by who is fighting whom near you, not by squad, so splitting doesn't game it |

### 1.2 The real world we build from

| Source | Fact | Where it goes |
|---|---|---|
| Amethyst geodes of Ametista do Sul and Los Catalanes ([Göttingen](https://www.uni-goettingen.de/en/73613.html?id=7555), [Springer 2024](https://link.springer.com/article/10.1007/s00126-024-01310-2)) | The cavities are gas bubbles in 134-million-year-old **basalt** flows; silica-rich water filled them with quartz, then amethyst; geodes sit in the massive, unfractured part of the flow | **The biome's geology:** a sea-cliff plateau of black flood basalt whose gas cavities grew into geodes the size of halls. The surface is black basalt; every hall is a geode lined violet. It is also why the Wardcarvers came: the rock is hollow and it rings |
| Naica, the Cave of the Crystals ([Wikipedia](https://en.wikipedia.com/wiki/Cave_of_the_Crystals), [Live Science](https://www.livescience.com/planet-earth/geology/cave-of-crystals-the-deadly-cavern-in-mexico-dubbed-the-sistine-chapel-of-crystals)) | Selenite beams up to about 12 m long grew in 58 °C water; humidity near 100%, sweat can't cool you | **The Crystal Forests:** white selenite beams 4 to 14 m crossing a hall like fallen columns, steaming pools. Raven bans a temperature meter (DD), so heat is a look, a sound and one micro-hazard (scalding pools burn on contact), never a meter |
| Lechuguilla Cave ([NPS](https://home.nps.gov/cave/learn/nature/lechuguilla_cave.htm), [Wikipedia](https://Www.wikipedia.org/wiki/Lechuguilla_Cave)) | The Chandelier Ballroom: gypsum chandeliers up to 6 m, the largest known; delicate formations break at a touch | **Shard chandeliers:** gypsum-and-quartz chandeliers 4 to 6 m that drop when a loud noise rings them (micro-hazard and the Tyrant's counter) |
| Lava tubes in basalt (real basalt-plateau caves) | Long round tubes with flat floors and ledges where lava levels stood | **The Echo Tunnels** are old lava tubes, round in section, which is why they carry sound so far |
| Chladni figures ([Colorado physics](https://physicslabs.colorado.edu/equipment/fine-sand/), [UCSB](https://web.physics.ucsb.edu/~lecturedemonstrations/Composer/Pages/44.45.html)) | Sand on a vibrating plate leaves the moving parts and gathers on the still nodal lines; Hooke saw it in 1680 | **The ward field tell:** floor dust jumps into nodal lines along the field's edge 0.9 s before a throw. Also the Twins' hall floor and Lyrathis's phase 2 floor |
| Hexafoils, "witch marks" ([Historic England](https://www.HistoricEngland.org.uk/whats-new/features/discovering-witches-marks/what-are-witches-marks/), [Atlas Obscura](https://www.atlasobscura.com/articles/apotropaic-witches-marks-carvings)) | Compass-drawn six-petal marks scratched by builders into doorways, beams and hearths from the medieval period on, to avert evil; the most common ritual mark in England | **The Wardcarvers' ward glyph** is a hexafoil. Every Wardcarver doorway, keystone and ward field carries one, compass-scribed (you can see the compass pricks). The Arcane body-change rune lines are hexafoil arcs |
| Ægishjálmur and the galdrastafir ([Galdrasýning](https://galdrasyning.is/en/galdrastafir/), [Wikipedia](https://wiki.krisyotam.com/content/wikipedia_en_all_nopic_2026-03/Helm_of_Awe)) | The "helm of awe": in the sagas a sphere of power that radiates fear; later a protection stave of eight three-pronged arms round a circle, carved on lead and pressed to the brow | **Aegis** (the Ward capstone) draws an eight-armed stave on the ground as it rises. Lyrathis's dome carries the same stave. The Wardcarvers' keystones are pressed-lead plates |
| Ringing rocks and lithophones (world-map KEPT) | Some stones ring like bells when struck | The chime-stone waymarks (KEPT) and **resonant crystal**, which rings on every pick strike |
| Quartz: piezoelectricity and triboluminescence ([Colorado](https://physicslabs.colorado.edu/demos/electricity-and-magnetism/electromotive-force-and-current/piezoelectricity), [ChemEdX flash rocks](https://chemedx.org/blog/flash-rocks-green-chemistry-and-lego-brick-perspectives)) | Squeezed quartz makes a voltage (Curie, 1880); struck quartz flashes orange-white in the dark | **"Sound makes light":** crystals glow when sound hits them. Real enough to feel true, strange enough to be the biome's rule |
| Paleolithic art at resonance points (Reznikoff; [ScienceDaily](https://sciencedaily.com/releases/2008/07/080704130439.htm), [Atlas Obscura](https://www.atlasobscura.com/articles/archaeoacoustics)) | In French caves, paintings cluster where the cave resonates most; red dots mark resonance spots too narrow to paint | **Wardcarver ochre marks** in the Echo Tunnels: red hand-dots mark the tube's resonant spots. Shout there (any noise) and the whole tube lights; that's where they painted. It also tells you where an Echobat will hear you first |
| Iceland spar, the "sunstone" ([Live Science](https://www.livescience.com/27696-viking-sunstone-shipwreck.html), [IYCr](https://iycr2014.org/learn/crystallography365/articles/20140217)) | Calcite splits one image into two; a calcite stone was found beside navigation dividers in a 1592 wreck | **The Mirrored Twins:** one knight is real; the other is its double thrown by a great spar lens. **Doubling spar** is a resource (Twins' trophy, the Arcane Wildbrand focus) |
| Scaly-foot snail ([Scientific American](https://www.scientificamerican.com/article/celebrate-endangered-species-day-meet-the-scaly-foot-snail-the-most-metal-animal-in-the-world/), [Science News](https://www.sciencenews.org/article/snail-shining-armor)) | The only animal known to build iron sulfide into its shell; three layers, with sacrificial microcracks | **Prismback** grows its plates from the crystal it licks: mineral armour on a living animal, three-layered (crystal, horn, bone), cracking in visible stages |
| Harvestmen (Opiliones) ([British Arachnological Society](https://britishspiders.org.uk/harvestmen), [Nat Geo](https://www.nationalgeographic.com/animals/article/daddy-long-legs-opiliones-arachnids)) | A small body slung between very long legs; shed legs keep twitching to distract a predator and never grow back | **Spindleglass:** a glass-bodied harvestman whose legs pass for selenite beams. Break a leg and it twitches on the floor (a distraction it uses); it never regrows in that life |
| Glass frogs ([Popular Science](https://www.popsci.com/transparent-glassfrog/), [Interesting Engineering](https://interestingengineering.com/innovation/mystery-behind-glass-frogs-transparency)) | Transparent belly skin shows the heart and gut; the frog hides its red blood cells in a mirrored liver to stay clear | Spindleglass's body is clear: you see its slow heart and its last meal. When it hunts, the blood floods back and the body clouds pink-grey (its tell) |
| Oilbirds ([Wikipedia](https://en.wikipedia.org/wiki/Oilbird), [Brinkløv et al.](https://www.frontiersin.org/journals/physiology/articles/10.3389/fphys.2013.00123/pdf)) | Cave birds that echolocate with audible clicks (1 to 15 kHz, 40 to 80 ms, up to 12 a second) and scream in the dark | **Echobat's voice and hunting:** audible click trains you hear before it arrives, rising in rate as it closes; a scream before its dive |
| The Alicanto ([Wikipedia](https://en.wikipedia.org/wiki/Alicanto)) | A Chilean mining-lore bird that eats gold or silver and glows that colour; followed unseen it leads to a mother lode; if it sees you, it dims and leads you off a cliff | **Alicanto** (creature, real name kept): a glowing ore-eating bird. Follow it unseen and it leads to a rich vein; if it notices you it dims and runs for the nearest drop |
| The carbuncle / carbunclo ([Wikipedia](https://en.wikipedia.org/wiki/Carbuncle_(legendary_creature))) | A small animal with a glowing gem on its head that walks over buried ore and betrays it | **Gemsniffer's** brow bears a natural crystal boss that glows when it scents a gem |
| Giant isopods and pill bugs ([Wikipedia mirror](https://ipfs.ssi.eecc.de/ipns/en.wikipedia-on-ipfs.org/wiki/Giant_isopod)) | Curl into a ball to shield the soft underside | **Druseback** rolls into a crystal-lined ball |
| The glass armonica ([Franklin Institute](https://fi.edu/en/science-and-education/collection/benjamin-franklins-glass-armonica)) | Franklin's 37 rotating glass bowls played with wet fingers; Mozart and Beethoven wrote for it | The biome's lead instrument (KEPT "glass harmonica"); also Lyrathis's voice layer |
| Arvo Pärt, *Für Alina* and tintinnabuli ([IRCAM](https://brahms.ircam.fr/en/composer/arvo-part/workcourse)) | His first tintinnabuli piece, in **B minor**: one stepwise melody voice plus a "bell" voice that only plays the notes of the B-minor triad | **The biome theme is written in tintinnabuli** in the KEPT key (B minor, 60 BPM): a melody voice on celesta or armonica, and a bell voice on B, D and F♯ only. It sounds like the caves ringing |

### 1.3 Lessons, short

1. **One rule drives the biome: sound makes light.** Crystals glow when sound reaches them. Light is never free: the noise that lights a hall is what the hunters hear.
2. **The grand puzzle is light and sound.** Light: prisms, beams, Lumenhorns, the Alicanto, dark tunnels. Sound: chime stones, echo tubes, Echobats, the Echo Warden, the Silent Vault. The Wardcarvers' wards are made of both.
3. **The world shows its secrets:** Chladni dust for ward fields, ochre dots for resonance, tinkling for chandeliers, hollow footsteps for thin geode floors, a clouding body for Spindleglass.
4. **Crystal armour is a part, broken by strike.** Every weapon works; hammers work best.
5. **Reflection is a rule, not homing.** A reflected projectile flies straight back along the line it came in on. It never turns.
6. **PvP consequences come from players**, not fines: bounties, marks, losing the camps as wake points.
7. **Colour law (KEPT):** gold = yours, crimson = unblockable. Ward light is violet #7A3FD1; crystal hum light is cool lilac-white #D8D0FF. No crimson in the caves except unblockable glints.

---

## 2. Design

### 2.0 Scope map

| FULL-GAME item (3.2.6, 3.3, 4.2) | Tag | This lane |
|---|---|---|
| Path theme: shields, reflected projectiles | SPEC (DD) | Kept; the whole tree is built around it (2.17) |
| Placement | KEPT (world-map) | Replaces "north-east" (2.1) |
| Look | GAP-COOKED | 2.1 |
| Sub-zones (geode halls, ward chambers, crystal forests, echo tunnels) | GAP (CAL names) → GAP-COOKED | The four CAL names kept, plus the surface (2.2) |
| Signature resource: resonant crystal ("rings when struck; runes and wards") | GAP → GAP-COOKED | Name and premise kept (2.6) |
| Hazard: ward fields and full dark | Numbers KEPT (3.3, world-map) | Behaviour, tells, answers GAP-COOKED (2.8) |
| Hidden sub-biome: the Silent Vault ("sound and spells die") | GAP → GAP-COOKED | Name and premise kept (2.5) |
| Event: the Resonance ("caves ring; every crystal regrows; ward fields shift") | GAP → GAP-COOKED | Premise kept (2.10) |
| Shrine inside a ward chamber; path master a blind rune-carver | RAVEN-PLANNED exist / GAP who | 2.11 |
| Music: glass harmonica, bells, celesta, echoing; 60 BPM B minor | GAP (3.3) | Tempo, key, feel kept; pieces GAP-COOKED (2.16) |
| Creatures (FULL-GAME 6; brief "about 9") | GAP | The 6 names kept; 3 added: **9** (2.12) |
| Minis: Mirrored Twins, Echo Warden, Geode Tyrant (roaming) | GAP | Names and counters kept; fights GAP-COOKED (2.13) |
| Major: Lyrathis, the Prism Sovereign | GAP | Premise kept (beams split by prisms, shard rain, dome, light copies, every prism fires); fight GAP-COOKED (2.14) |
| Weak to Void; boss part Prism heart | SPEC rule (opposite) / GAP part | Kept (2.14) |
| Arcane tree, Mana, Deep well | SPEC (ST, DD) | Nodes exactly as ST; numbers KEPT from combat where set; the rest GAP-COOKED (2.17) |
| Body change: rune lines on the forearms | GAP (hero KEPT frame) | Stages GAP-COOKED (2.18) |
| Wardcloth set | KEPT (survival-crafting, hero) | Restated, items around it GAP-COOKED (2.19) |
| R9: rune sockets, PvP pass | RAVEN-PLANNED (BP Full release PvP, CF Extras) | 2.19.3, 2.21 |
| Invasions Arcane ↔ Void | RAVEN-PLANNED (DD) | Crystal side GAP-COOKED (2.22); switches on with R10 |

**Roster count.** FULL-GAME lists 6, CAL Part 3 says 6, the brief says about 9. The brief is the newer word: **9**. The 6 names stay (combat names Echobat and Prismback in its caster template; survival-crafting uses Chimecrab and Gemsniffer). Added: **Spindleglass** (highland ambusher: the biome had one terrifying predator), **Druseback** (hauler: the biome had none) and **Alicanto** (light and ore utility: Raven's tundra "has no cave light"; a cave biome needs one). Split 4 aggressive, 3 neutral, 2 passive.

### 2.1 Where it sits and how it reads

**Placement (KEPT, world-map 2.2):**
- East half of belt B6 (y +4,600 to +6,400), east of the Spine. **Surface footprint** x +1,800 (the Spine's foot and the Severance's east mouth) to +2,450 (sea cliffs), about 1.17 km², of which **1.0 km² is walkable** (the KEPT figure); the rest is sea stacks, cliff and collapsed geode domes.
- **Caves:** about 2 km² on three levels under the surface and west under the Spine to x +1,350.
- **Elevations** (m above sea level): surface plateau +60 to +140; Upper level +60 to 0; Middle 0 to −90; Deep −90 to −200. Prism Hall floor −120 (KEPT), Silent Vault −180 (KEPT).
- **Bands by depth below the local surface (world-map rule):** lowland 0 to 60 m (the surface and the upper Geode Halls), midland 60 to 180 m (deep Geode Halls, Crystal Forests, Echo Tunnels), highland deeper (Ward Chambers, Prism Hall, the Vault).
- **Camps (KEPT):** K1 (+2,250, +4,900), K2 (+2,200, +5,500), K3 (+2,000, +6,000), each in a cliff mouth (a geode broken open by the sea) above a newcomer wreck in the cove (world-map rule). Players build on the surface only (DD: no underground bases).

**The look in one line.** A black basalt sea-plateau, cracked into hexagonal columns, with violet light glowing up from round holes where geode domes have fallen in. Below: halls that are the insides of giant geodes, lined with amethyst points as tall as a man; white selenite beams crossing steaming halls; round black lava tubes painted with red dots; and, deepest, carved stone chambers where the floor's dust moves by itself.

**The rule that shapes everything: sound makes light (GAP-COOKED).**
- Every crystal in the caves has a resting glow of 0.15 nits (you can see shapes at 10 m) and flares when sound reaches it, up to 4 nits, then fades over 6 s.
- A noise event lights crystals within its radius, brightness falling with distance. Noise radii (shared with Echobat and Echo Warden hearing, 2.12, 2.13): footstep walking 4 m, running 8 m, sprint 15 m, roll 8 m, jump landing 6 m, weapon hit on a creature 12 m, weapon hit on crystal 20 m, spell cast 10 m, Lumenhorn bellow 40 m, chime stone struck 25 m, powder charge 80 m; crouch-walking 0.
- **Keystone chimes:** each hall has one large chime stone (the Wardcarvers' hall bell). Strike it and the hall stays lit at 2 nits for 20 game minutes and its map is revealed (our Lightroot). Striking it is a 25 m noise.
- Tunnels between halls have no crystal: **full dark** (SPEC), light only from what you carry or tame.
- **Why the Wardcarvers lived here:** the rock is hollow and rings, so a struck chime lights a whole hall. They built their whole craft on it.

**Read rules:**
- **The surface reads by its holes.** Each collapsed geode dome on the plateau glows violet from below at night and has a Wardcarver chime-stone arch at its lip, so the ways down read from 300 m.
- **Underground reads by light colour, one per sub-zone:** violet (Geode Halls), white-gold steam (Crystal Forests), black with red dots (Echo Tunnels), cold lilac and violet ward-glow (Ward Chambers).
- **Every hall has a far light:** its keystone chime sits at the far end under a crystal cluster, so the first strike in a hall lights the way across it.
- **Ceilings always visible:** geode linings glow faintly at rest; the halls' scale always reads.
- **Night on the surface:** violet holes, chime-stone arches lit when the wind rings them, the Prism Crown's spires beaming up (2.3 L1).

### 2.2 Sub-zones (the four CAL names kept, plus the surface)

| # | Sub-zone | Where (x, y m; elevation) | Band | Ground and feel | Palette | Props | Ambient bed |
|---|---|---|---|---|---|---|---|
| C1 | **The Basalt Mouths** (surface) | The whole plateau x +1,800 to +2,450, y +4,600 to +6,400; +60 to +140. Camps K1 to K3 | Lowland | Black columnar basalt pavement (hexagonal tops like the Giant's Causeway), salt-grass and sea campion in the cracks, wind-cut heather, collapsed geode domes 30 to 80 m across, sea cliffs 40 to 80 m on the east | Basalt black #1C1C20, salt-grass #7E8C5A, sea slate #4E5F66, violet hole-glow #7A3FD1 | Geode dome rims, chime-stone arches, Wardcarver watch-cairns, dome-lip stairs, shipwrecks in the coves | Sea wind, gulls, surf below; a low hum from the holes; chime stones ringing in gusts |
| C2 | **Geode Halls** | North and centre under the plateau, x +1,850 to +2,400, y +4,700 to +5,400; +60 to −30 | Lowland (upper) / midland (deep) | The insides of giant geodes, 20 to 60 m across, round, lined with amethyst points 0.5 to 2 m long, banded agate rinds at the walls' base, quartz sand floors, thin crust floors over lower geodes | Amethyst violet #6B3FA0, agate bands cream/rust #E6D8C0 / #A85A3A, quartz white #EDEAF4 | Amethyst linings, agate rind ledges, Geodemite nest geodes (sealed, unopened), Wardcarver rope-and-bronze ladders, crystal-dust drifts | Slow drips with long glassy tails, tiny crystal ticks as the rock cools, Chimecrab chimes |
| C3 | **Crystal Forests** | South, x +1,800 to +2,350, y +5,500 to +6,300; +20 to −60 | Midland | Naica halls: white selenite beams 4 to 14 m crossing the space at angles, steaming blue-green pools (scalding), gypsum chandeliers in the side galleries, the air thick with mist | Selenite white #F2F0E8, steam-teal water #4FA8A0, mineral gold #C8A048, mist grey #B8BCC0 | Selenite beam thickets, scalding pools with mineral rims, chandelier galleries, Wardcarver steam-baths (ruined) | Hiss and bubble of hot water, a constant soft roar of steam vents, beams creaking with heat |
| C4 | **Echo Tunnels** | Old lava tubes running east-west under the middle, x +1,400 to +2,300, y +5,000 to +6,000; 0 to −80 | Midland | Round black tubes 6 to 15 m across with flat lava-shelf floors and benches where old lava levels stood, glassy lava-glaze walls, Echobat roosts in skylights and side vents, guano mounds (saltpetre), red ochre dot-marks | Lava-glaze black #121216, ochre red #A8402A, guano grey #8C8678, roost-crystal lilac #D8D0FF | Ochre dot panels, chime stones at tube junctions, guano mounds, roost vents, Wardcarver tally walls | Long reverb (3 to 6 s), far wind breathing through the tubes, Echobat click trains |
| C5 | **Ward Chambers** | West under the Spine, x +1,350 to +1,850, y +5,000 to +6,100; −60 to −160 | Highland | Carved stone: square chambers hewn into geode walls, floors of polished black basalt dusted with fine crystal powder (the Chladni dust), hexafoil-carved doorways, ward fields across thresholds, the Prism Hall and the Twins' Archive | Hewn basalt #2A2A30, ward violet #7A3FD1, crystal dust lilac-white #D8D0FF, bronze #8C6A3A | Hexafoil doorways, lead keystone plates, bronze tuning-fork racks, ward-field thresholds, Wardcarver work benches, bronze lens gimbals | A single held low tone (the wards humming at 49 Hz, felt more than heard), dust hiss, distant chimes |

**Hidden sub-biome: the Silent Vault** (2.5), under C5 at −180.

**Blends (world-map rule 5):** 60 to 120 m, mostly vertical: the Geode Halls' violet fades into the Echo Tunnels' black over a 40 m descent of lava-tube throat; the Crystal Forests' steam thins into the tubes. **Forest border** (y +4,600, KEPT): crystal-veined boulders under moss, a few amethyst points grown into tree roots and lit from inside when the wind rings the forest's bells (biome-forest's Bell-Tree, KEPT, sits just north). **Void border:** the Severance only (KEPT): grey-bleached crystal veins on the gorge walls, ward fields at its east mouth.

### 2.3 Landmarks (5, about 7% of 76, the world-map mix)

| # | Landmark | At (x, y; elevation) | What it is and why it's here | Reads at night as |
|---|---|---|---|---|
| L1 | **The Prism Crown** (super landmark, reads from 3 km, world-map rule) | Over the Prism Hall, at the Spine's foot, (+1,850, +5,300), spires to +200 | Eight violet crystal spires 40 to 60 m tall standing in a ring where the Prism Hall's dome breaks the surface. They are the tops of eight **light shafts** the Wardcarvers cut so Lyrathis's light, turned by the hall's prisms, could rise and shine across the Severance as a ward. Bronze collars with hexafoils bind each spire's base | Each spire beams a faint violet column straight up when the prism below it faces its shaft. During the boss fight's phase 3 all eight blaze |
| L2 | **The Hollowed Giantess** (KEPT world-map placement) | The Severance's east mouth, Crystal side, (+1,820, +5,380), +20 | A crystal woman 30 m tall, kneeling, facing west into the gorge, arms spread as if holding a door. Her back is whole; her front is hollowed out from inside, a cavity full of grey bleached crystal. She is Lyrathis's sister, the first Sovereign, who held the Severance and failed. A Wardcarver ward cage of bronze rods still rings her | Her intact back faintly glows lilac; the hollow front is black |
| L3 | **The Split Geode** | The east sea cliff south of K2, (+2,430, +5,750), cliff from +0 to +90 | A geode 80 m tall cut in half by the sea: its amethyst interior faces the ocean like a violet cathedral wall, with the sea washing its floor. Visible from every camp, the sea and the Forest coast. Chimecrabs live on its ledges | Violet glow over the surf; the waves ring the lower crystals |
| L4 | **The Hall of Beams** | C3, (+2,050, +5,950), −40 | The biggest Naica hall: 180 × 90 m, crossed by a hundred selenite beams up to 14 m, the largest three bridging the whole width and walkable 20 m above the steaming pools. The Beam Shrine sits on the central beam | Steam glows white-gold under lit beams; the keystone chime lights it all |
| L5 | **The Organ** | C4, the main tube, from (+1,450, +5,500) to (+2,300, +5,600), 0 to −40 | A straight lava tube 850 m long with eleven vertical vents (skylights) up to the plateau, like organ pipes. Wind across the vents sounds one deep chord through the whole tube; in a gale it rises. Ochre dots mark the eleven resonant points. Biggest Echobat roost in the game | Moonlight down the eleven vents in a row; lit crystals wherever someone made a noise |

### 2.4 POIs: 23 surface + 53 caves = 76 (count KEPT; contents GAP-COOKED)

Spacing check: 8 km of cave route / 53 = one per 151 m; surface walkable routes ~4.2 km / 23 = one per 183 m. No point on any route is more than 125 m from a POI (world-map rule).

**Surface 23:** 3 camps · 3 landmarks (L1 to L3) · 2 shrines (Cliff Bell, Severance) · 4 descents · 6 hand sites · 5 kit POIs.

**Descents (each a collapsed geode dome or tube vent; each has a Wardcarver arch with a chime stone, KEPT waymark):**

| # | Descent | At | Leads to | How you go down |
|---|---|---|---|---|
| D1 | **North Dome** | (+2,150, +4,750), +90 | C2 Upper Halls | Dome-lip stairs cut into the rind, 40 m |
| D2 | **Bell Throat** | (+2,300, +5,150), +80 | C2, then C4 | A tube vent with a Wardcarver bronze bell (2 m) hung on a basalt beam across the throat; a spiral of rope ladders (allowed underground, KEPT) 60 m |
| D3 | **The Sinking Geode** | (+2,000, +5,600), +100 | C3 and C4 junction | A dome slowly collapsing: a ramp of fallen rind blocks, 50 m. Thin-floor hazard on the way |
| D4 | **South Steam Vent** | (+2,100, +6,200), +70 | C3 | A steaming vent with Wardcarver stone steps spiralling down its wall, 60 m; warm air rising |
| — | Severance mouth | (+1,820, +5,420), +20 | C5 by the Severance Gate tunnel | The pass road (2.22) |

**Surface hand sites (story through place; each pays a named thing):**

| # | Site | At | What you see (the story) | Pays |
|---|---|---|---|---|
| S1 | **The Watch-Cairn Line** | Along the Spine foot, (+1,860, +4,800) to (+1,860, +6,200) | Seven basalt watch-cairns, each with a bronze sighting ring aimed west at the Severance. The northern three still have their bronze; the southern four were torn out and the stones lie scattered toward the gorge, grey-bleached | A sighting ring (decor), lore item "Watch tally" |
| S2 | **The Ringing Wreck** | K1's cove, (+2,380, +4,920), +2 | The castaways' hull (world-map newcomer rule) lodged on a crystal reef; every wave rings the reef. Newer rope and canvas than any ruin | Rope, iron nails, a newcomer's journal (lore) |
| S3 | **Dome of the Dropped Hammer** | (+2,050, +5,050), +110 | A geode dome with one perfect hexafoil scribed across its lip, 6 m wide, and a Wardcarver bronze compass and mallet left at the centre point as if the carver walked away mid-work | Wardcarver compass (talisman: ward fields take 1.5 s longer to rethrow you) |
| S4 | **The Wax Fields** | (+2,250, +5,900), +80 | A meadow of wild candle rhubarb (KEPT crop, here growing in a collapsed tube's shade) round a ruined chandlery: vats of old wax, moulds shaped like ear-plugs | Wax ×6, Candle rhubarb seeds, wax earplug recipe (KEPT item) |
| S5 | **The Alicanto Runs** | (+2,150, +6,300), +70, cliff edge | Bird tracks in basalt grit leading along a cliff path; at the end, a newcomer's skeleton at the foot of the cliff with a lantern and a pick (he followed it openly). The tracks themselves loop back to a real silver vein in a crack | Silver ore ×6 (if you find the vein), lore "The miner's last page" |
| S6 | **The Chime Garden** | (+2,350, +5,350), +100 | Forty wind-chimes of crystal and bronze strung on basalt posts in a circle by the cliff, half broken; the whole hillside sings in a gale | Resonant crystal ×2, the chime tree decor blueprint (KEPT Geode sanctum piece) |

**Surface kit POIs (5):** 2 resonant crystal outcrops at dome rims; 1 Druseback grazing ledge; 1 Lumenhorn herd's drinking pool; 1 vista on the highest basalt stack (+2,200, +5,250), +140 (reveals 400 m of the fill-in map).

**Caves 53:** 2 landmarks (L4, L5) · 3 shrines (Geode, Beam, Great Hexafoil) · 14 hand sites (8 below + 6 Vault rooms, 2.5) · 3 mini lairs (Twins' Archive, Echo Warden's Door, the Tyrant's Wallow) · 1 boss approach (two run-back stones and the Prism doors) · 4 ore galleries (2.7) · 26 kit POIs.

**Cave hand sites (8):**

| # | Site | Sub-zone, at (x, y, z) | What you see | Pays |
|---|---|---|---|---|
| H1 | **The Singing School** | C2, (+2,200, +4,850), +30 | A geode hall fitted with stone benches in rows facing a chime wall of 21 tuned crystal bars. Ochre hand-prints on the benches at child height. The bars still play: struck in order (low to high) they light the whole hall and open a wall cache | Cache: amethyst ×3, resonant crystal ×4, "Child's tuning fork" (lore) |
| H2 | **The Broken Nest** | C2, (+1,950, +5,200), −10 | A cracked-open geode with the husks of a thousand Geodemites round it, all facing in, and a dead Wardcarver in the middle with a mallet; he opened it on purpose | Geodemite egg (steal-egg taming site), mallet (lore) |
| H3 | **The Steam Baths** | C3, (+2,200, +5,700), −10 | A Wardcarver bathhouse built round hot pools: carved stone tubs, bronze pipes leading steam to a sweat room, a scalding pool broken into the tubs from below. Two Prismbacks bask on the warm stone | Bath oils (rested buff +10 min, survival-crafting rule), pumice |
| H4 | **The Fallen Chandelier** | C3, (+1,950, +6,150), −30 | A 6 m chandelier lying shattered on a crushed Lumenhorn skeleton, a newcomer's broken lute beside it (he played under it). The other chandeliers in the gallery still hang | Lute (decor), selenite ×4; the lesson of the gallery |
| H5 | **The Tally Wall** | C4, (+1,800, +5,450), −20 | A 40 m tube wall covered in Wardcarver tally marks in groups of six (hexafoil petals), each group crossed through. The marks get shakier and stop. Below: a hundred wax earplugs in a stone bowl | Lore "Count of the quiet"; wax ×4 |
| H6 | **The Guano Mines** | C4, (+2,050, +5,850), −30 | A Wardcarver saltpetre works under the big roost: guano scraped into leaching vats, bronze evaporating pans, a crystal ladder up into the roost vent. This is the richest saltpetre in the game (the PvP raid economy, 2.21.4) | Saltpetre node (respawning), saltpetre ×8 |
| H7 | **The Lens Grinder's Room** | C5, (+1,700, +5,100), −80 | A workshop: a bronze lathe with a half-ground doubling-spar lens in its chuck, a rack of grinding laps, and the same lens's twin cracked on the floor. Light from a crystal on the bench passes through the lens and throws two images of the bench on the wall | Doubling spar ×2; the Twins' lens explained without words |
| H8 | **The Severance Gate** | C5, (+1,800, +5,420), −20, the cave end of the pass | A great hexafoil door hewn into the gorge's east wall, its ward field still live (attuned players pass; the keystone is high above the door, reachable with an arrow or spell, 2.8). Behind it the Wardcarvers' last guard room: weapons racked facing the door, never drawn | Lore "Order of the door"; the pass's run of the Crystal side |

**Cave kit POIs (26):** 6 resonant crystal outcrops (one per geode hall cluster) · 4 Geodemite nest geodes · 3 Echobat roosts (also saltpetre) · 3 Wardcarver chime junctions with a cache behind a ward · 2 scalding pool basins with mineral crusts (pumice, sulfur-free salts) · 2 Chimecrab ledges · 2 Druseback wallows · 2 underground lakes (fishing: blind cavefish, glass shrimp, KEPT) · 1 Lumenhorn wintering hall · 1 vista (a geode window high in the C2/C3 wall looking down on the Hall of Beams).

**Kit rules:** each kit type has 2 to 4 variants; every kit POI sits round a bend or below a lip so it reveals itself (triangle rule); each has a crystal that glows when you arrive (the noise of your steps lights it), so every POI reads in the dark (world-map night rule).

### 2.5 Hidden sub-biome: the Silent Vault (name and premise KEPT; design GAP-COOKED)

**What it is.** A chamber where sound and spells die. The Wardcarvers' last refuge: they learned the Void found them by sound (2.20), so they built a place where nothing can be heard, and sat down in it.

**Where.** (+1,600, +5,900), −180, under C5 (KEPT). Six rooms in a line, 220 m in all.

**How you get in (the puzzle).** The **Vault Door** is a round lead-plated door 6 m across in the Echo Warden's hall (2.13.2). It opens only for silence: stand on the hexafoil before it and let it hear **nothing within 20 m for 10 s** (no footsteps, no combat, no tame moving, no chime). Its rim crystals dim one petal at a time as you hold still (six petals, one per 1.7 s): that is the progress. Any noise relights all six. The Echo Warden guards the hall, so you either beat it or sneak it far away first (throw a stone into the far side, then crouch back). **During the Resonance event the door cannot open** (too loud).

**Inside: the silence rule.**
- No sound from you, creatures or the world inside the Vault: the mix drops to a planned silence (KEPT world-map) with one exception, your own heartbeat, faint, rising if your HP is below 30%.
- **Spells die:** spell casts fail with a soft puff of violet dust; Wildbrands still work (they're weapon arts). Path resources don't regen inside. Arcane Barrier and Aegis can't be cast. The Arcane main path's root, Deep well, still holds (passive).
- Light: crystals here never flare (no sound reaches them). Only carried or tame light works.

**The six rooms (count as 6 of the 14 hand sites):**

| Room | What you see | Pays |
|---|---|---|
| V1 **The Antechamber** | Wax earplugs in bowls by the door; Wardcarver mallets and chisels laid down neatly on a bench, heads wrapped in wool | Wax earplugs ×4 |
| V2 **The Choir Benches** | Forty Wardcarvers, dead and dry, seated on stone benches, their mouths sealed with wax, hands in laps. Their faces show no fear. Fine crystal has grown on their shoulders like frost | Lore "Wax seal" (item description only) |
| V3 **The Archive of Forks** | Shelves of 600 bronze tuning forks, each with a tiny lead tag scratched with a hexafoil variant: each fork is a ward's "key note". Silent here, they ring outside the Vault | **Ward fork** (consumable ×3): struck outside, it drops the nearest ward field for 60 s |
| V4 **The Lake** | A still black lake 60 m long under a glittering ceiling; nothing moves. The **Prism Eel** lives here (KEPT legendary fish); fishing works (it's not a spell) | Fishing (KEPT) |
| V5 **The Sovereigns' Mould** | A huge stone casting-pit shaped like a woman lying down: where the Sovereigns were grown. Two bronze name-plates, one polished (Lyrathis), one scratched out (her sister) | Lore "Two plates" |
| V6 **The Last Carver** | One Wardcarver not on a bench: a woman kneeling at the far wall, her chisel still in the last stroke of a hexafoil on the wall. Unfinished, one petal short | **The Carver's Chisel** (unique talisman: your Runes in sockets work at 125%), and the Rune of Silence (2.19.3) |

**Creatures:** none live inside (they can't hear). That's the threat: the Echo Warden outside, and leaving into the Resonance if it starts.

### 2.6 Signature resource: resonant crystal (name and premise KEPT; never farmable, survival-crafting rule)

| Part | Detail |
|---|---|
| Look | Violet quartz with a pale blue phantom inside (a smaller crystal ghost within the big one); clusters 1 to 3 m tall at geode walls; each point rings a clear note when struck |
| Where | Only in Geode Halls (C2) and Ward Chambers (C5) linings: 48 nodes, plus 2 surface outcrops at dome rims; respawn 2 game days; the Resonance regrows every node at once (2.10) |
| Gathering (GAP-COOKED, the "pick on the ring" rule) | Each pick strike rings the node. Strike again **while it still rings** (within 1.2 s, the ring's visible shimmer) and that strike yields +1 extra. Miss the ring and the node yields normally. A full node: 6 strikes, 6 to 12 crystal. Every strike is a **20 m noise** (lights the hall; Echobats hear it) |
| Uses (KEPT and GAP-COOKED) | Runes (8 crafted, KEPT), ward charm (KEPT), Wardcloth (KEPT), Resonance pylon (KEPT), crystal lamp (KEPT), Mark-resist potion (KEPT), Focus elixir (KEPT), Druseback taming food (GAP-COOKED), Arcane path material (crafting element "arcane"), attuning chalk, Lyrathis's summoning offering (2.14) |

### 2.7 Other resources and ore galleries

| Resource | Where | Gathered | Used for |
|---|---|---|---|
| **Crystal dust** (KEPT) | Chimecrab produce, dust drifts in C2 | Hand | Gemsniffer food (CR), glass flux (KEPT), attuning chalk |
| **Selenite** | C3 beam ends (snapped stubs, not the beams themselves) | Pick | Selenite pane (a glass that lets in light but not sight, Geode sanctum windows), chalk |
| **Doubling spar** | Ward Chambers (rare, 6 nodes), the Twins' trophy | Pick | The Prismglass spear (2.19), Lens of the Twins talisman, Arcane Wildbrand focus |
| **Amethyst** (new gem) | Gem clusters in band 3 | Pick | Socket gem: **+10 max path resource** (Mana, Heat… whichever path's item); joins ruby, topaz, onyx (BUILT gems) |
| **Saltpetre** (KEPT item) | Echobat roosts (C4), H6 Guano Mines | Shovel/pick | Black powder (KEPT); the raid economy (2.21.4) |
| **Ochre** | C4 tube floors | Hand | Red dye; attuning chalk |
| **Wax** | S4 Wax Fields, Vault | Hand | Earplugs (KEPT item), candles |
| Hides, meat, chitin, plates | Creatures (2.12) | Butcher (BUILT) | 2.19 |

**Attuning chalk (KEPT name and effect; recipe GAP-COOKED):** hand recipe: crystal dust 2, ochre 1, selenite 1 → 2 chalk. You draw it on your forearms (a 1.0 s animation): 5 min, ward throw off, ward damage ×0.5 (KEPT).

**Ore galleries (CF rule: deeper is better; world-map bands):**

| Gallery | At | Band | Ores |
|---|---|---|---|
| O1 **Mouth seams** | C1 dome walls and C2 upper | 1 | Copper, tin, stone, clay |
| O2 **Agate rinds** | C2 deep, (+2,050, +5,250), −20 | 2 | Iron, coal seams, ruby/topaz/onyx, saltpetre (bat roost) |
| O3 **The Silver Tube** | C4 east, (+2,250, +5,800), −50 | 3 | Rich iron, **silver** (rings, lanterns), gem clusters incl. amethyst (the Alicanto's favourite hall) |
| O4 **The Deep Geode** | C5 south, (+1,500, +6,050), −160 | 4 | **Star iron** (world-map band 4) |

### 2.8 Hazards

**Main hazard: ward fields (numbers KEPT: 25 damage and a 600 cm throw every 1.5 s; passes: Arcane path, ward charm; early pass attuning chalk).**

| Part | Detail (GAP-COOKED) |
|---|---|
| What they are | Old Wardcarver barriers across doorways, tube mouths and vault thresholds: a vertical sheet the shape of the opening, invisible at rest except a faint violet shimmer at its edges |
| Tell (≥ 0.9 s, world-only) | At 3 m: the floor's crystal dust jumps into **Chladni nodal lines** along the field's plane, the hexafoil carved over the doorway glows, and a rising hum (49 Hz to 196 Hz over 0.9 s) plays. Stop and back off and nothing happens |
| Effect | Entering the plane unattuned: 25 damage through `ApplyHit` (arcane, unblockable, no stance) and a throw of 600 cm back the way you came, then 1.5 s before it can throw you again (KEPT). It kills (no floor) |
| Attuned (pass) | Arcane path (main or sub), ward charm (talisman), Wardcloth 4-piece, attuning chalk (half damage, no throw): you walk through with a soft chime |
| Keystone | Every ward field is powered by one **lead keystone plate** with a hexafoil, set somewhere in sight of the field (above the door, on a ledge behind it, across a pool). Hit the keystone with any projectile or spell and the field drops for 90 s (a descending chime, the dust lies still). This is the skill route for any path. Some keystones are only reachable from the far side (a loop back) |
| Creatures | Crystal creatures are attuned (CR: creatures resist their biome's hazard). Void creatures take the throw ×2 (Severance invasions, 2.22) |
| Where | 34 fields: every Ward Chamber threshold, the Severance Gate, vault caches, 3 chime junctions, the Silent Vault's inner doors |

**Full dark (SPEC).** Tunnels between halls have no crystal: full dark. Answers: torch, lantern, crystal lamp (KEPT), Glowmelon lantern (KEPT), Lumenhorn (rider light 15 m), Alicanto (10 m). Crystal halls follow "sound makes light" (2.1).

**Micro-hazards (3; one per sub-zone at most, world-map rule):**

| Micro-hazard | Sub-zone | Tell (≥ 0.9 s) | Effect | Answers |
|---|---|---|---|---|
| **Shard chandeliers** | C3 side galleries, C2 two halls | A noise of 15 m or louder under one makes it **tinkle** for 1.0 s (the crystals visibly shiver), then it drops | 300 cm radius, 70 damage, impact 5; leaves a shard pile. Re-grows (re-hangs) after 2 game days. Drops on creatures too (Tyrant counter) | Move quietly under them; or drop them on purpose |
| **Scalding pools** | C3 | Steam and a bubbling hiss; the water is clear blue-green with a mineral rim | Wading: 15 burn buildup a second + 4 damage a second; full immersion 12 damage a second. Burning blood (Fire root) passes; fire gear halves | Walk the rims and the beams |
| **Thin geode floors** | C2 | Footsteps sound **hollow** (a ringing boom under each step) and the floor's crust shows hairline cracks lit violet from below | A heavy attack, a roll or a jump landing on it breaks a 4 m hole: you fall 8 to 14 m into the geode below (normal fall damage; Druseback and Lumenhorn too heavy: they break it by walking) | Walk, don't roll; or break it on purpose for a short-cut down |

### 2.9 Weather (surface weights KEPT; effects GAP-COOKED)

| State (KEPT weight) | Effect here |
|---|---|
| Clear (40) | Night: violet holes, Prism Crown columns |
| Overcast (20) | — |
| Rain (30) | **Drip song:** rain feeds the cave drips; in the upper Geode Halls (C2) every drip is a tiny noise, so the halls glow brighter (resting glow 0.15 → 0.6 nits) and Echobats hunt the upper halls less (too much noise to pick you out: their hearing range ×0.7 there). The plenty window after rain (world-map rule): resonant crystal yield +50% for the next block |
| Fog (10) | Sea fog over the plateau; chime-stone arches ring in the damp (light at dome rims). Fog's own rules (world-map) |
| Underground | None (KEPT); the clock-driven night table runs 22:00 to 04:00 (world-map KEPT) |

### 2.10 Event: the Resonance (premise KEPT; frame KEPT from fire-frost: one game day, about once every 3 real days, signature resource ×3)

| Phase | What happens |
|---|---|
| **Warning** (5 real min before) | A deep tone (a 40 Hz swell, felt in rumble) rises from every cave mouth; the eight Prism Crown spires beam full violet; the Wardcarver pillar on the Crown's Ring burns violet (world-map KEPT). Chime stones across the plateau start ringing by themselves |
| **Peals** | Every 4 real minutes a **peal** sweeps the caves from the Prism Hall outward at 300 m/s: a 6 s wave of sound. During a peal every crystal flares to 4 nits (no darkness), **every Echobat is deafened** for 30 s and drops to roost (clicking stops), shard chandeliers in its path fall (stand clear of galleries), and Geodemite nest geodes **crack open** and release swarms |
| **Ward shift** | Every ward field switches to its second pattern for the event: 12 normally live fields drop (opening sealed caches and two shortcut routes), and 12 normally open thresholds close. Keystones still drop them. The routes you know change for a day |
| **Regrowth** | Every resonant crystal node regrows at once at the first peal; 6 **singing geodes** (sealed geodes that only open in the event) split along their seams at (map in `world_pois.json`), each holding resonant crystal ×12 and an amethyst cluster |
| **Danger** | Geodemite swarms from every nest; chandeliers; the Silent Vault can't be entered |
| **Pays** | Resonant crystal ×3 (KEPT), the singing geodes, Echobats easy to kill or tame (deafened = no hunt) |
| **End** | The last peal fades; the pillar dims; nests reseal in 1 game day |

### 2.11 Shrines and the path master

**Shrines (rules KEPT: 5 per biome, guarded, 3 s to light, path XP, fast travel; one at the pass mouth):**

| Shrine | At | Guard | Look |
|---|---|---|---|
| **Great Shrine: the Hexafoil** (FULL-GAME "shrine inside a ward chamber", kept) | C5, (+1,780, +5,650), −90 | None (the path master works here) | A square chamber whose floor is one hexafoil 8 m across inlaid in bronze; the brazier is a crystal bowl that lights when you strike its rim (cold violet flame) |
| Cliff Bell Shrine | C1, (+2,300, +5,200), +100, by D2 | Lumenhorn herd (neutral; don't provoke) | A brazier under a bronze bell on a basalt frame at the cliff |
| Geode Shrine | C2, (+2,100, +5,000), +20 | Geodemite nest (2 swarms) | A brazier on an agate rind ledge in an amethyst hall |
| Beam Shrine | C3, on L4's central beam, (+2,050, +5,950), −40 | Spindleglass | A brazier lashed to the beam above the steam |
| Severance Shrine (pass mouth) | (+1,830, +5,420), +20, before the Severance Gate | Prismback and an Echobat | A brazier inside the Hollowed Giantess's cage rods |

**Path master: Saga Venn, the Rune-Carver (GAP-COOKED; DD "a path master NPC per biome who teaches the path through quests"; FULL-GAME "a blind rune-carver" kept).**

| Part | Detail |
|---|---|
| Who | A newcomer, not a Wardcarver (world-map rule). She was a stonemason before the wreck; she went blind in the caves years ago (scarred eyelids, a clean white line across both) and learned the Wardcarvers' craft by touch and by ear: she reads a ward by tapping it and listening. Late 30s, tall and strong-built, broad carver's shoulders and forearms dusted with crystal powder, her own violet rune lines glowing faintly on both forearms (Arcane body change at 80, 2.18). Long black hair with a single silver streak, braided over one shoulder; high cheekbones, full mouth, a slow confident smile. A sleeveless deep-violet leather jerkin laced over a linen shirt open at the throat, a carver's apron, bronze tools at her belt, bare arms. Barefoot on the hexafoil floor so she can feel the hum |
| Where | Works at the Great Shrine by day (carving a new keystone plate); at night sits at the Hexafoil's edge striking a fork and listening |
| How she talks | Short spoken lines within 6 m, no camera grab (KEPT rule); quests go to the Journal |
| Teaches | Anyone; her last quest needs you past the Echo Warden's hall (beaten or sneaked) |

**Her 6 quests (`quests.json` schema from fire-frost):**

| # | Quest | Task | Reward |
|---|---|---|---|
| 1 | **Listen first** | Strike 3 hall keystone chimes (light 3 halls) | 1,000 Arcane XP; attuning chalk recipe |
| 2 | **On the ring** | Mine 20 resonant crystal, at least 8 of it "on the ring" | 1,500 Arcane XP; ward charm recipe (KEPT) |
| 3 | **Drop the door** | Drop 3 ward fields by their keystones (no attunement) | 2,000 Arcane XP; Ward fork ×1 |
| 4 | **The quiet road** | Cross the Organ end to end without an Echobat hearing you (none enters attack state) | 2,500 Arcane XP; wax earplugs recipe (KEPT) and the silver bait recipe (Alicanto taming, 2.12) |
| 5 | **Two of one** | Beat the Mirrored Twins | 3,000 Arcane XP; doubling spar ×2 |
| 6 | **What the Vault keeps** | Open the Silent Vault and bring her the Carver's Chisel to touch (you keep it) | 5,000 Arcane XP; "Venn's ring" (ring slot: Mana regen +15%, ward fields never throw you) |

### 2.12 Creatures (6 names, roles and taming KEPT from FULL-GAME; 3 added; everything else GAP-COOKED)

**Rules (CR SPEC, combat KEPT):**
- Every Crystal creature is attuned to ward fields; none is immune to a chandelier's fall.
- Neutral herds defend each other (CR). Each creature tames only by its listed methods (DR #16).
- Tiers (3.3): hard aggressive = highland, medium = midland, passives lowland. Geodemites use the swarm archetype (25 HP each).
- Every attack shows the real claw, jaw, plate, leg or projectile; damage lands only on visible contact (combat 2.17.1b). Tells read at 15 m: each has a pose change plus a sound before the windup ends.
- **No attacker limit** (combat 2.18.4): packs and swarms all attack at once if they can reach you.
- Element **arcane**: bites and claws carry **mark buildup 15** by default (combat rule: a creature's element on its hits). A marked player takes +15% from everything for 10 s (KEPT), so a Geodemite swarm makes the Prismback behind it hit harder. That's the biome's combat signature.
- `physMaterial` "crystal" (glass ring + prism glints, KEPT) for plated parts; "flesh" or "chitin" elsewhere.
- **Hearing hunters:** Echobat and the Echo Warden perceive only noise events (2.1 radii); light and sight don't matter to them. Spindleglass hunts by vibration through the floor (footsteps, rolls, landings; not crouch-walking).
- **Reflection (Prismback, Geode Tyrant, Twins' ward guard):** a player projectile or spell bolt that hits a reflecting plate face flies straight back along its incoming line at its own speed with damage ×0.8, now owned by the reflector. It never turns or tracks (never-home rule). Beams, area spells, Wildbrands and melee are never reflected.

#### 2.12.1 The nine at a glance

| Creature | Behaviour, tier, pack | Real anatomy it's built on | Silhouette (one line) | Palette | Size | Taming | Drops |
|---|---|---|---|---|---|---|---|
| **Prismback** | Aggressive fighter, highland, 1 (2 at the baths) | Komodo dragon body and gait; girdled-lizard and pangolin overlapping armour; scaly-foot snail mineral plates grown from licked crystal, three layers (crystal face, horn, bone) | A heavy low lizard with a spine of tilted mirror-plates that turn toward you like sails | Hide basalt-grey #3A3A40; plates violet-clear with lilac edges #B8A0E8; mirror faces catch any light | 4.2 m long, 1.1 m at the shoulder, plates to 1.6 m | Knockout and feed, or Arcane path magic. Hard | Prism plate ×2 (light shield plate), hide ×3, raw meat ×5 |
| **Spindleglass** (new) | Aggressive ambusher, highland, 1 | Harvestman (a small body slung between eight very long legs; shed legs twitch and never regrow); glass frog transparency (clear body, visible heart and gut); legs banded white like selenite | Eight pale beams meeting at a clear body hung 3 m up, with a dark heart beating inside | Legs selenite white #F2F0E8 with grey bands; body clear, organs dark plum #4A2A3A; clouds pink-grey #C8A8A8 when hunting | Leg span 9 m, body 1.4 m, stands 4 m | Knockout and feed. Hard | Glass leg ×2 (spear shafts, bow limbs), clear membrane ×2, venom gland ×1 |
| **Echobat** | Aggressive fighter, midland, 2 to 4 | Oilbird's audible clicks and screams; ghost bat's pale membranes and huge ears; leaf-nose; noctule build scaled to a 3 m wingspan | A pale bat with ears as long as its head and body, a wrinkled leaf-nose, hanging in rows | Membranes pale bone #E8E0D8 with lilac veins; fur ash #B0A8A0; teeth translucent | 3 m wingspan | Knockout and feed (bring it down). Medium | Membrane ×2, raw meat ×2 (its roost gives saltpetre) |
| **Geodemite** | Aggressive swarm, lowland, 6 to 14 | Army ant (mandibles, swarm columns, bivouac nests) at 40 cm, with a clear swollen abdomen like a honeypot ant's, here crystal | A scuttling glass bead with jaws, pouring out of a geode in a sheet | Head and legs black chitin #1A1820; abdomen clear violet #8A60C8 | 40 cm | Steal an egg (egg geodes in nests). Easy | Mite crystal ×1 (crystal dust ×2) |
| **Lumenhorn** | Neutral mount, midland, herd 3 to 6 | Irish elk build and its 3 m palmate antlers, here crystal over bone; red deer's bellow | A tall stag carrying a spread of glowing crystal antlers wider than it is long | Coat slate-blue #4A5060 dappled pale; antlers clear violet, glow lilac-white #D8D0FF | 2.1 m at the shoulder, antlers 3.2 m span | Knockout and feed. Medium | Crystal antler ×1 (lamp, staff focus), hide ×4, raw meat ×6 |
| **Druseback** (new) | Neutral hauler, midland, 1 to 3 | Giant isopod segmented armour, seven pairs of legs, rolls into a ball like a pill bug; each back segment is a split geode, crystal inside | A long armoured woodlouse the size of a cart, its back ridged like a stack of split stones | Plates basalt grey with violet crystal at each seam #6B3FA0 | 3.6 m long, 1.4 m tall | Offer resonant crystal. Medium | Drusy plate ×2 (geode-lined shield), raw meat ×6 |
| **Gemsniffer** | Neutral utility, lowland, 1 to 2 | Aardvark snout, ears and digging claws; giant anteater's sticky tongue and hook-claw threat stance; the carbunclo's glowing brow gem | A long-snouted, big-eared digger with a glowing stone set in its brow | Coat sandy grey #9A9080; brow gem amber-violet glow | 1.6 m long | Offer crystal dust. Easy | Hide ×2, raw meat ×2 |
| **Alicanto** (new) | Passive utility, lowland, 1 to 2 (night) | Seriema / secretary-bird long legs and running gait; flightless, wings held open as it runs; feathers take the colour of the metal it eats (the legend) | A long-legged running bird with glowing wings spread wide like lanterns | Silver-white or copper-green glow by diet; legs black; eye gold | 1.6 m tall | Offer food (silver ore or silver bait), approached unseen. Easy | Glow feather ×2 (lantern), raw meat ×1 |
| **Chimecrab** | Passive farm animal, lowland, 3 to 8 | Coconut crab body and size; hermit-crab habit, its shell a geode it grew into | A big crab carrying a ringing geode on its back | Legs rust-brown #8A4A2A; geode violet inside, basalt outside | 1 m across | Offer food. Easy | Crystal dust (farm, 15 min, KEPT), raw meat ×2 |

**Path magic (Prismback; in Raven's "calm a creature of the same element" frame).** On a Prismback knocked out or under 30% HP, hold Interact 4 s with 30 Mana: you **ward** it (a Barrier wraps it; it stops thrashing). Counts as a full narcotic dose; taming continues as knockout and feed. Needs 10+ Arcane points.

**Utilities:**
- **Lumenhorn (mount):** sprint 1,300 cm/s; rider light 15 m (steady, silent); **Bellow** on order (order wheel "call" slot): a 40 m noise that flares every crystal in range for 20 s, and calls every Echobat that hears it. Antler collision off while mounted in tubes under 6 m (it ducks).
- **Druseback (hauler):** carries 200; rolls into a ball when hit with impact ≥ 4 (4 s, damage ×0.3); can't be ridden; breaks thin geode floors by walking (route round, or use it to open a short-cut).
- **Gemsniffer (utility):** senses gems, amethyst, resonant crystal nodes and caches within 20 m; its brow gem brightens and it points and scratches toward the nearest; digs a buried cache in 15 s.
- **Alicanto (utility):** 10 m glow (cold light, silent); senses ore veins within 40 m, runs to the richest and waits there; fed silver it glows silver-white, copper green.
- **Chimecrab:** farm yield (KEPT).

#### 2.12.2 Attack rows (schema KEPT, combat 2.18.1; W / H / A / P = windup / delay hold / active / punish, s)

Tier hit: lowland 12, midland 20, highland 32 (SPEC). Stance: lowland 20, midland 35, highland 55. Default buildup mark 15 on bites and claws.

| Creature | Attack | W / H / A / P | Dmg × | Poise (impact) | Flags, buildup | Next / notes |
|---|---|---|---|---|---|---|
| **Prismback** (high) | Bite | 0.42 / 0 / 0.16 / 0.70 | 1.0 | 35 (3) | parryable; mark 20 | 40% → Head-swing |
| | Head-swing | 0.48 (head cocks back, plates rattle) / 0.3 / 0.20 / 0.70 | 1.3 | 45 (4) | parryable; mark 25 | — |
| | **Plate turn** (reaction) | 0.20 (plates swing to face you with a glassy clack) | — | — | When it hears a bow draw or a cast within 25 m it turns its plated flank toward the source for 3 s: reflects (rule) | Answer: flank it, or break the plates |
| | Tail lash | 0.60 (tail curls and lifts) / 0 / 0.30 / 0.90 | 1.4 | 50 (4) | `behindOnly`, r 380, `low` | Jump it |
| | **Glare** | 0.90 (plates tilt toward the nearest light, a rising glassy whine, lilac glint on the plates) / 0 / 0.6 cone / 1.0 | 0 | — | Cone 1,200 × 25° reflecting the brightest light within 15 m of it (your torch, a lit crystal, a Lumenhorn) into your eyes: screen whites out to 40% for 1.0 s, lock-on drops (a flash, not the Blind status) | Only when a light within 15 m is brighter than 1 nit. Carry light low, or fight it in the dark |
| | Body charge | 0.80 (claws scrape, plates flatten) / 0 / 0.8 run at 1,100 cm/s / 1.2 | 1.6 | 55 (5) | line 1,200 × 160 | Wall impact: stunned 2 s |
| | readsAction | Head-swing with no hold within 0.4 s of a drink or charged heavy | | | | |
| | Plates (part) | 4 rows, stance 40 each; strike ×1.4, slash ×0.7, pierce ×0.8 on plates; a broken row stops reflecting and bares hide (×1.2 there) | | | | Mana burn does ×2 to plates (2.17) |
| **Spindleglass** (high) | **Leg spear** (opener) | 0.95 (the body above clouds pink-grey as blood floods back; a dry glassy tick as one leg lifts) / 0 / 0.20 / 1.2 | 1.7 | 55 (5) | Downward stab from above, r 120; blockable; mark 25 | Only once it feels your footsteps within 8 m and you're under its span |
| | Leg sweep | 0.70 (two legs draw back with a ringing scrape) / 0.2 / 0.30 / 0.80 | 1.2 | 45 (4) | cone 600 × 140°, `low`, parryable | Jump it |
| | Stilt step | 0.40 / 0 / 0.15 / 0.60 | 0.8 | 30 (2) | It plants a leg on you (r 100) as it repositions; blockable | Stay out from under the body |
| | **Lift** (grab) | 1.00 (two forelegs part wide, crimson glint on the tips) / 0 / 0.25 / 1.4 on a miss | 0.6 + bites | 50 (5) | `grab`, unblockable, line 400 × 120: lifts you 3 m to the mouth, bites twice over 1.5 s (0.6 each), drops you 3 m | Dodge only; 60 damage to a holding leg drops you early |
| | **Shed leg** (reaction) | instant on a leg break | — | — | The leg falls and twitches 6 s, ringing (a 12 m noise): hearing hunters go to it; your lock-on jumps to it if it's nearest. It backs off 8 m meanwhile | Legs never regrow in that life |
| | Legs (parts) | 8 legs, stance 25, HP 120 each; 3 broken → it collapses (knockdown 4 s, crit on the body); 5 broken → it crawls | | | | Strike ×1.3 on legs |
| **Echobat** (mid) | **Click trains** (always) | Heard from 30 m; rate rises from 3 a second (searching) to 12 a second (locked on you) | — | — | Not an attack: its sound is the readout (combat "sound before sight") | It hunts only noise |
| | Dive bite | 0.85 (0.5 s scream, wings fold) / 0 / 0.25 / 1.0 grounded | 1.2 | 35 (3) | parryable; from up to 12 m above; mark 20 | Grounded 1.0 s after (flyer template) |
| | **Shriek** | 0.95 (hangs back, mouth wide, ears forward, a rising whistle) / 0 / 0.5 / 1.0 | 0.5 | 20 (2) | Cone 900 × 30°: a visible air ripple travelling 900 cm/s; staggers 0.4 s; guard stamina ×1.5; sound, not a projectile (no reflection) | ≥ 1.4 s from mouth-open to arrival at 5 m |
| | Wing buffet | 0.50 / 0 / 0.20 / 0.70 | 0.8 | 30 (3) | cone 300, knock 200 cm | When you're under it |
| **Geodemite** (low swarm) | Bite | 0.45 / 0 / 0.12 / 0.80 | 0.5 | 10 (1) | mark 15 | — |
| | Latch | 0.50 (rears, mandibles wide) / 0 / 0.15 / — | 0.3 a s | 10 (1) | Clamps to a limb (max 4 on you) for 3 s: mark 10 a second; a roll or a hit shakes 1 off | Four latched = marked in about 2 s |
| | **Burst** (on death) | 0.30 (the abdomen cracks with a tick) | 0.4 | 5 (1) | r 150 shard spray, blockable | Kill them away from you |
| **Lumenhorn** (mid, neutral) | Charge | 0.90 (hoof scrape, antlers lower, glow brightens) / 0 / 1.0 run / 1.30 | 1.6 | 45 (5) | knockback 900; line 1,800 | Wall impact: stunned 2.5 s |
| | Antler rake ×2 | 0.45, 0.40 / 0 / 0.20 / 0.80 | 1.0 each | 30 (3) | parryable; mark 15 | — |
| | **Bellow** | 1.00 (neck stretched, a rising roar) / 0 / 0.5 / 0.9 | 0 | — | 40 m noise: crystals flare, and every Echobat that hears it comes | The herd bellows when one is hit |
| | Rear kick | 0.40 / 0 / 0.15 / 0.70 | 1.0 | 35 (3) | `behindOnly` | — |
| **Druseback** (mid, neutral) | Roll ram | 0.80 (curls, plates grinding) / 0 / 1.2 at 900 cm/s / 1.4 | 1.4 | 45 (4) | line 1,400, blockable; bounces off walls | — |
| | Plate crush | 0.70 (rears its front segments) / 0.2 / 0.25 / 1.0 | 1.3 | 45 (4) | r 250 in front, `low` | — |
| **Gemsniffer** (low, neutral) | Hook claws | 0.55 (rears, forearms spread wide) / 0 / 0.20 / 0.80 | 1.3 | 30 (3) | parryable | Cornered or burrow hit |
| | Tongue flick | 0.40 / 0 / 0.20 / 0.70 | 0.5 | 10 (1) | line 300; dust in the eyes: 0.6 s screen dim | — |
| **Alicanto** (low, passive) | none | — | — | — | Flees, never attacks. If it notices you following (inside its 120° view within 15 m, not crouched) it **dims** and runs to the nearest drop edge within 60 m and stops there (the folklore) | The dimming is the tell |
| **Chimecrab** (low, passive) | Pinch (cornered) | 0.45 / 0 / 0.12 / 1.0 | 0.4 | 10 (1) | — | Its shell rings (10 m noise) when hit |

#### 2.12.3 Day and night tables (`spawn_regions.json` with `night`; underground night 22:00 to 04:00, KEPT)

| Sub-zone | Day | Night (changes) |
|---|---|---|
| C1 Basalt Mouths | Chimecrab (cliffs), Lumenhorn herd (pools at dusk and dawn), Druseback (lichen at dome rims), Gemsniffer | Alicanto runs the cliffs; Echobats leave the vents to hunt the plateau; Lumenhorns go below |
| C2 Geode Halls | Geodemite nests, Chimecrab, Gemsniffer, Druseback wallows | Prismbacks come up to lick crystal; Geodemites forage out of nests in columns |
| C3 Crystal Forests | Prismbacks basking at baths and pools, Spindleglass in beam thickets, Lumenhorn wintering hall | Spindleglass moves to hall edges; Echobats hunt over the steam |
| C4 Echo Tunnels | Echobat roosts asleep (click trains 1 a second), Druseback in the tubes | Echobats awake through the whole tube system; Alicanto in the Silver Tube |
| C5 Ward Chambers | Prismback pairs, Geodemite swarms, Spindleglass in the Deep Geode | Echobats in the outer chambers; the Twins and the Echo Warden never move |

#### 2.12.4 Taming notes (methods FULL-GAME, kept)

| Creature | Notes |
|---|---|
| Prismback | Knock out with strike damage on bare hide, or path magic. Its plates reflect tranq bolts: shoot from behind or break a row. Food: raw meat |
| Spindleglass | Torpor builds only on the body; collapse it (3 legs) and club the body. Food: raw meat |
| Echobat | Bring it down (a hit during a dive grounds it), then blunt torpor. A deafened bat in the Resonance can't hear you coming |
| Geodemite | Steal an egg geode from a nest (3 per nest); the swarm hunts the thief by noise, so carry it crouched |
| Lumenhorn | Knockout (herd defends). Food: lichen or cave moss |
| Druseback | Offer resonant crystal ×3 (it licks it from your palm), crouch-approach |
| Gemsniffer | Offer crystal dust ×5 |
| Alicanto | Offer silver ore ×2 or silver bait (silver ore 1 + raw meat 1, workbench): approach unseen (crouched, outside its 120° view) to 3 m and hold it out 3 s. Seen first: it dims and runs |
| Chimecrab | Offer raw meat or fish |

### 2.13 Mini-bosses (3; names and counters KEPT; fights GAP-COOKED)

**Shared (combat KEPT):** 3,000 HP solo, +80% per extra player or tame; stance 90, 2 parries; 4 to 5 moves and one counter; windups 0.65 to 0.9, punish 0.8; projectiles ≥ 0.9 s, never home, nothing fired inside 4 m; lock points head and chest; run-back stone 40 to 60 m out; first-kill drop flies to the killer; drops upgrade shards and a trophy (BO).

#### 2.13.1 The Mirrored Twins (C5, the Archive of Wards)

**Why here.** They guard the **Archive of Wards** at (+1,620, +5,200), −110, where the Wardcarvers kept their keystone moulds, and the way to the Lens Grinder's Room (H7). Wardcarver constructs: bronze armatures cast by lost-wax, grown over with resonant crystal into armoured knights 2.6 m tall: faceless helms with a hexafoil visor, a crystal longsword and kite shield each.

**The real and the double (FULL-GAME counter kept: "only the one casting a reflection in the floor takes damage").** Only **one** knight exists. The other is its double, thrown by the **Spar Lens**, a doubling-spar lens 1.5 m across in a bronze gimbal arm in the ceiling, through which a shaft of crystal light passes. The double is hard light: it hits as hard as the real one, but weapons pass through it. The floor is black basalt under 2 cm of still water: **only the real knight has a reflection in it.**

| Part | Detail |
|---|---|
| Arena | 40 × 40 m, 18 m high; the water film; four pillars; the Spar Lens on its gimbal arm under a ceiling light vent; a bronze **lens magazine** on the north wall holding 3 spare lenses on a chain feed |
| **Swap** | Every 12 s the gimbal arm turns the lens a quarter turn (1.0 s, a chain rattle and a high chime, the lens flares): real and double swap places. Read the reflection again after each swap |
| **Mirror rule** | The double mirrors the real knight's attack across your position at the same instant: real swings from your left, double from your right. Step back out of both, never between |
| Breaking the lens | While it flares (the 1.0 s swap) the lens is exposed: 3 projectile or spell hits shatter it. The double vanishes; one knight for 20 s while the magazine lowers a new lens down its chain (a visible, clanking 20 s descent). After 3 spares: one knight for good |
| Moves | **Twin cross:** cone 400 × 70°, windup 0.70 (swords lift crossed, white glint), active 0.20, punish 0.9, 35, impact 3, parryable · **Shield bash:** line 300, 0.65, 0.15, 0.8, 30, impact 4, blockable · **Mirror lunge:** line 900, 0.80 (both crouch, swords back), 0.25, 1.1, 45, impact 4, parryable; the pair converges from both sides · **Ward guard:** 0.60, a hexafoil ward forms on the shield for 3 s and reflects projectiles (rule) · **Lens flare** (the swap): 1.0 s tell; the real knight takes no damage during it |
| Stance | 90 on the real knight; parries only count on the real one (parrying the double fails and leaves you open) |
| Trophy | **Twin shard** (talisman: the first hit after you dodge an attack lands twice, the second at 40%) |
| Drops | Upgrade shards, doubling spar ×2 |

#### 2.13.2 The Echo Warden (C5, the Hall of the Door)

**Why here.** Built to kill anything that makes a sound near the Silent Vault. It stands in the **Hall of the Door** at (+1,640, +5,850), −150, before the Vault Door (2.5).

**Look.** A blind bronze construct 5 m tall on four long heron legs; its head is a **great tuning fork** (two tines 2.5 m long); two flared ear-horns on its shoulders swivel toward every sound; humming crystal plates on its back; no eyes.

| Part | Detail |
|---|---|
| Arena | A 50 m round hall; the Vault Door on the far wall; **4 chime stones** (2 m crystal bells on bronze posts) at the compass points; loose stones on the floor (throwable, combat 2.18.4) |
| **How it hunts (KEPT counter)** | It goes to the **loudest noise** within 30 m (2.1 radii) and attacks that point. Crouch-walking is silent: sneak. A thrown stone (15 m noise where it lands) pulls it. A struck chime stone (25 m, the loudest thing in the room) sends it there to **hammer the chime** for 4 s with its back to you (stance ×1.5 from behind). Each chime breaks after 3 hammerings: 4 chimes, 4 openings |
| Moves | **Fork slam:** target circle at the noise point, r 300, windup 0.90 (fork raised, horns locked on, rising hum), active 0.25, punish 1.2, 50, impact 5, blockable · **Tine sweep:** cone 450 × 120°, 0.70, 0.25, 0.9, 40, impact 4, parryable · **Resonance blast:** after any of its hits lands, windup 1.0 (tines blur, a rising whine): a visible ripple ring from it, r 600 at 1,200 cm/s, 30 + mark 40, `low` (jump the ring) · **Listening stance:** it freezes 2.0 s, horns sweeping (a soft repeating ping); any noise then = **Snap lunge:** line 1,200, windup 0.6, 55, impact 5, parryable. Hold still and it walks on · **Chime hammering** (the counter) |
| Stance | 90; 2 parries |
| Trophy | **Warden's tuning fork** (talisman: your footsteps and rolls make half the noise) |
| Drops | Upgrade shards, a Ward fork |

#### 2.13.3 The Geode Tyrant (roaming; premise KEPT: a Prismback the size of a house; plates reflect spells; melee and Void pass)

| Part | Detail |
|---|---|
| Look | A Prismback 12 m long, 4.5 m at the shoulder; its plates are whole split geodes, crystal faces out, some 3 m tall; hide scarred grey; one eye clouded white |
| Route (why it roams) | It feeds on the richest crystal: the **Tyrant's Wallow** (hot pool, C3, (+2,150, +6,050), −40, its lair) → the **Chandelier Gallery** (C3, (+1,950, +6,150)) → the Organ's east end (C4) → the agate rinds (C2, O2) → back. One loop per 1.5 game days; rests 2 game hours in the Wallow |
| **Counter (KEPT, expanded)** | Plates reflect every spell and projectile (rule). Melee and Void pass. Strike cracks plate rows (6 rows, stance 60 and HP 300 each; a broken row stops reflecting there). **The place:** its route passes under the gallery's six 6 m chandeliers. Get it under one and make a 15 m noise (strike a crystal, ring a chime, a powder charge): the chandelier tinkles 1.0 s and drops: 400 damage and a 6 s stun with the weak-point mark |
| Moves | **Plate charge:** line 2,000 × 220, windup 1.00 (claws scrape, plates flatten, grinding roar), runs 1,300 cm/s up to 1.6 s, 60, impact 5; into a selenite beam: the beam shatters and it's stunned 3 s · **Bite:** cone 500, 0.65, 0.20, 0.8, 45, impact 4, parryable · **Tail club:** self 600, `behindOnly`, 0.90, 0.30, 1.0, 50, impact 5, `low` · **Glare:** cone 1,500 × 30°, 0.90 (plates tilt, glassy whine), whites out 1.0 s, drops lock-on (needs a light over 1 nit within 15 m) · **Shard shrug:** 1.00 (it shudders, plates rattling): 8 shards lobbed in high arcs at 900 cm/s to land in an 800 cm ring round it; each landing spot glints the whole flight; 25 each; never home |
| Stance | 90 body; 2 parries |
| Trophy | **Tyrant plate** (talisman: Barrier and Aegis +20% shield HP; non-Arcane users: block stamina −10%) |
| Drops | Upgrade shards, prism plate ×6 |

### 2.14 Major boss: Lyrathis, the Prism Sovereign (premise KEPT; fight GAP-COOKED)

**Who she is (told only by the world, 2.20).** The second of two crystal women the Wardcarvers grew in the Sovereigns' Mould (V5) to be living wards against the Void. Her sister held the Severance and was hollowed (L2). Lyrathis was set in the Prism Hall, where her light, turned by eight prisms up eight shafts (L1), shone over the Severance. When her sister fell she turned the prisms inward and raised her dome, and has stood guarded since.

**Look.** A crystal giantess **9 m tall**, slender and long-limbed, beautiful like a cathedral statue cast in violet glass over a black basalt core you glimpse through her. A faceted face with no eyes; a fist-sized **brow gem** where the eyes would meet; a crown of eight prism spires; long hair of white selenite needles that chime when she moves; a slowly turning **heart crystal** lit inside her chest; crystal blades growing from her wrists when she fights. She moves like a dancer, but every step plants and grinds: floor cracks and dust puffs under each foot (no gliding). Terrifying in scale and silence.

**Numbers (combat KEPT):** 12,000 HP solo, +80% per extra player or tame; stance 160; 3 parries; phase 2 at 50% by a transition attack; hidden phase 3 at 25% with artifacts, damage ×1.4 (BUILT flag). **Weak to Void** (SPEC): ×1.5 (BUILT multiplier), and void rifts eat her dome at once (R10). Lock points: brow gem, heart crystal, left wrist blade, right wrist blade (parts, stance 50 each: broken → that blade is gone, and Shardblade uses the other arm only).

#### 2.14.1 The arena: the Prism Hall (KEPT at (+1,700, +5,300), −120)

| Part | Detail and why |
|---|---|
| Shape | A geode dome 90 m across and 50 m high, amethyst-lined; floor of polished black basalt with fine crystal dust that leaps into Chladni lines when she stamps |
| **The eight prisms** | Clear calcite-and-quartz prisms 6 m tall on **bronze turntables** in a ring 22 m from the centre dais. Each once aimed her light up one ceiling shaft (L1). In the fight her beam passes through the nearest prism and splits; turning a prism changes where the split beams go |
| **Turntable mechanism (engineered)** | A bronze disc 4 m across rides on a ring of bronze rollers in a stone race. A ring gear on its rim meshes with a pinion on a vertical shaft that rises 1.5 m to a **bronze lever arm** with a hexafoil hand-plate; a counterweight on the lever's far end returns it after each throw. A pawl and ratchet on the pinion shaft allow one direction only (clockwise from above). **Strike the hand-plate (any melee hit, arrow or spell):** the lever swings, the pinion turns, the disc rotates exactly 45° over 0.8 s (eased in and out, never a snap) and the pawl drops home with a bronze clack. An engraved arrow on each disc's rim shows the prism's facing. 0.8 s cooldown per turntable. Nothing passes through anything: the rollers sit in the race, the lever clears the prism, the copies (below) stand beyond the ring |
| Light shafts | Eight shafts 2 m wide rising 120 m from the dome to the Prism Crown spires. A prism facing its shaft throws part of her beam up it; that spire beams on the surface |
| Cover | The prisms (their shadow side blocks beam and Flare), four broken pillars, the dais steps |
| Doors | The **West Door** from the Ward Chambers and the **Severance Door** from the gorge tunnel (H8). A run-back stone outside each, 50 m out (two, so no clan can camp both: 2.21.8) |
| Altar | The Arcane altar on the dais (SPEC: the path unlocks at the boss altar): a hexafoil basin |
| Summoning altar (RAVEN-PLANNED, DD) | Offer resonant crystal ×20 + prism plate ×4 at the basin while she's on her respawn timer to wake her early |

#### 2.14.2 Moves (template KEPT, combat 2.17.5)

| Move | Phase | Shape | Windup | Hold | Active | Punish | Damage | Impact | Flags, notes |
|---|---|---|---|---|---|---|---|---|---|
| **Shardblade ×3** | 1 | Cone 500, 60° | 0.65, 0.55, 0.80 | — | 0.20 | 1.1 | 40 / 40 / 55 | 3 / 3 / 4 | Hits 1 and 2 parryable, hit 3 blockable; wrist blades sweep visibly; white glint before each |
| **Prism beam** | 1 | From the brow gem through the nearest prism; splits into 3 beams fanned 30°, each sweeping 40° over the active time | 1.20 (brow gem brightens; a harmless violet **guide thread** draws from her brow to the chosen prism; chosen at windup start, never changed) | — | 1.6 | 1.2 | 35 per contact, re-hits every 0.5 s; mark 30 | 2 | Blockable. Cover: behind any prism, pillar or the dais. **Tame-punish (SPEC rule):** the fan's centre beam aims at the furthest tame. Range 4,000. A sweeping beam on a fixed arc: no homing, no reflection |
| **Shard rain** | 1 | 8 shards break from the ceiling over 2.0 s at points round every player, picked at windup start | 1.00 (palms to the floor; a crack runs up the walls with a groaning ring) | — | each shard's real shadow grows 1.0 s, then it lands | 0.9 | 45 each, r 150 | 3 | Falling objects: ≥ 1.0 s warning, never home; shadows are real (combat 2.17.1b) |
| **Sovereign's grasp** (grab) | 1 | Line 700 × 150 | 1.00 (open hand drawn back to the hip, crimson glint on the fingertips) | — | 0.30 | 1.3 on a miss | 15 + crush 2.0 s × 22/s + throw 900 | grab | Unblockable, dodge only. 150 damage to the grasping wrist part drops you |
| **Prism ward** (the dome) | 1 | At 85% and 65%: she kneels and raises a faceted dome r 600 with the eight-armed stave on its face | 1.20 (her selenite hair stiffens and rings) | — | up to 25 s | — | — | — | Outside: projectiles stop on it; melee through it ×0.25. **Break it with the room:** turn a prism so one split beam strikes the dome: 400 a second while on it (her next Prism beam comes within 6 s of the dome rising). Dome HP 1,200; Mana burn ×2; void ×3 (R10: a rift eats it at once). Broken: she staggers 3 s (stance ×2). Unbroken after 25 s it drops by itself |
| **Sovereign's pulse** | 1 | Self r 500 | 0.90 (heart crystal flares) | — | ring 0.4 | 1.0 | 40 | 4 | Only while the dome is up; `low` ring at 1,200 cm/s (jump it); blockable |
| **The Sundering Flare** (transition) | 1→2 | Radial burst from the heart crystal, r 3,000, hitting everyone in line of sight to her chest | 1.40 (arms spread; the hall goes dark but the prisms; her light drains into her chest; crimson glint at the heart) | — | 0.40 | 1.0 | 55 + mark 60 | 4 | Unblockable; cover behind a prism, pillar or the dais; she takes 50% damage during the windup (KEPT). She shatters into herself plus 3 copies |
| **Refracted copies** (arena change) | 2 | 3 copies of light, each standing where the line from her through a prism meets open floor, 15 m beyond it | — | — | while the line holds | — | 60% of hers | — | **They repeat each of her attacks 0.5 s later** (dodge her, then the echo). They take no damage. **Turn a prism** so its line meets a wall or another prism and its copy dissolves (20 s before it can re-form). The real Lyrathis never stands on a prism line |
| **Held facet** (new delayed) | 2 | Target circle r 300 at up to 900 | 0.90 (two-hand overhead, crystal mass forming between her palms) | 0.60 (her arms ring a rising tone; white glint 0.2 s before contact) | 0.25 | 1.4 | 70 | 5 | Blockable |
| **Glint lance** (new delayed) | 2 | A 6 m crystal javelin thrown straight at where you stood when the hold ends | 0.80 | 0.40 | 1,600 cm/s | 0.8 | 50 | 4 | Projectile: never homes, nothing inside 4 m, ≥ 1.2 s plus flight. **It stays in the floor 20 s as a small prism:** beams split through it and a copy can form on its line. 3 hits shatter it |
| **Prism waltz** (new combo) | 2 | Spin slash → step slash → rising cut, cone 450 | 0.60, 0.50, 0.70 | 0.3 on hit 3 | 0.20 | 1.4 | 38 / 38 / 55 | 3 / 3 / 4 | Hits 1 and 2 parryable |
| **Late Shardblade** (delayed variant) | 2 | As Shardblade | 0.65, 0.55, 0.80 | 0.5 on hit 3 | 0.20 | 1.2 | 40 / 40 / 55 | 3 / 3 / 4 | — |
| **Double prism beam** | 2 | Through 2 prisms at once (two fans) | 1.20 (two guide threads) | — | 1.6 | 1.2 | 35 per contact | 2 | Tame-punish on the first fan |
| **All prisms fire** (hidden phase 3, artifacts, SPEC premise) | 3 | On the dais; one continuous beam turns round her at 30° a second through every prism in turn, each throwing its fan | 1.40 | — | continuous | — | 35 ×1.4 per contact | 2 | Dome permanent; it breaks only if **three prisms are turned to face her within 10 s** (three fans converge: 1,200 in 1 s), then 8 s of stagger and the cycle restarts. All eight Prism Crown spires blaze on the surface |

**Rhythm:** phase 1 idle gaps 0.6 to 1.4 s, one ranged move in flight; phase 2 gaps 0.3 to 0.9 s, two in flight; never the same move 3 times running (KEPT).

**Drops (BO SPEC):** boss core `core_lyrathis`, Arcane path materials (arcane essence ×5), a chance of the **Prism heart** (boss part, kept: spells crit when cast at full resource; spell crit ×1.5). First-kill drop named and flying to the killer within 6 s (KEPT). Blueprints (survival-crafting rule): the **Prismglass spear** (boss weapon, 2.19) and the **Geode sanctum** decor pack (KEPT). Trophy head for the monument (KEPT).

### 2.15 Creature voices (fused real animal calls; 4 layers per attack KEPT: tell, strike, hit, pain)

Every species its own voice, built in the DAW from real recordings, pitched and layered, with a glassy "ring" layer that ties the biome together (a struck wine-glass or crystal bowl, filtered per species). No shared voice classes.

| Creature | Fused from | Idle / call | Tell (rising cue on every windup over 0.5 s) | Pain / death |
|---|---|---|---|---|
| Prismback | Komodo dragon hiss + monitor lizard throat-gulp + rattling glass shards | Slow tongue-flick ticks, plates clinking as it walks | Head-swing: a rising hiss with a glass rattle; Glare: a bowed-glass whine rising a fifth | A cracked-plate shatter over a hissing roar |
| Spindleglass | Harvestman stridulation (amplified) + cracking ice + a slow human-scale heartbeat (low, from inside the body) | Silence; only a faint glass tick as a leg shifts, and the heartbeat if you're within 5 m | Leg spear: the heartbeat quickens and a dry tick; Lift: two long glass scrapes | A high glass snap and a long fading ring |
| Echobat | Oilbird clicks and screams + greater horseshoe bat calls pitched into hearing + a wet leathery wing flap | Click trains (rate is the readout, 2.12.2) | Shriek: a rising whistle like a finger round a glass rim; Dive: the oilbird scream | Scream cut short, wings crumpling |
| Geodemite | Army-ant swarm rustle (contact mic) + tiny glass ticks + bullet-ant stridulation | A sheet of ticking as a swarm pours | Latch: a sharp click of mandibles | Crystal pop (the burst) |
| Lumenhorn | Red deer stag roar + elk bugle top + a struck crystal bowl under it | Soft grunts, antlers chiming when they touch | Charge: a snort into a rising bugle; Bellow: full roar with the bowl ringing | A bugle broken into a groan |
| Druseback | Coconut-crab scrape + rolling stone + armadillo grunt | Leg-clatter, plate grinding | Roll ram: grinding plates speeding up | A hollow stone crack |
| Gemsniffer | Aardvark grunts + giant anteater huff + snuffling hog | Snuffles and a soft rising squeak when it senses a gem | Hook claws: a huff and a hiss | A squeal |
| Alicanto | Seriema yelping call + rhea boom + small brass bells (its glow "rings" faintly) | Yelping duet at night | — (never attacks) | A thin cry |
| Chimecrab | Coconut-crab clicks + its geode shell (a real struck geode recorded) | Chimes as it walks | — | A cracked chime |
| Mirrored Twins | Plate armour foley + bronze bell partials + a reversed breath (the double's "voice" is the real one's played backwards, 30 ms late) | Armour creak; the lens chain rattle | Twin cross: two blade rings a semitone apart | Bronze toppling, a bell muffled |
| Echo Warden | A great tuning fork (struck, recorded slowed) + heron croak pitched down + metal stress groans | A soft ping as its horns sweep | Fork slam: rising hum; Listening stance: repeating ping | A fork ringing down into silence |
| Geode Tyrant | Prismback layers pitched down an octave + elephant seal bellow + boulder grind | Deep breathing, plates grinding | Charge: grinding roar; Shard shrug: a long glass rattle | A collapse of glass and stone |
| Lyrathis | Beluga whistles ("sea canary") + glass armonica tones + a low female choir breath; her selenite hair chimes on every move | She never speaks; she breathes in chords | Beam: the armonica rising to a held tone; Grasp: a sharp intake; Flare: the whole chord swelling to a peak | Every prism in the hall rings as she shatters |

### 2.16 Music: the Crystal caves (tempo, key, feel SPEC 3.3: 60 BPM, B minor, glass harmonica, celesta, bells, long reverb; cues GAP-COOKED; rules KEPT)

**Rules (KEPT):** real composed pieces recorded with real instruments (armonica, celesta, tubular bells and handbells, crystal singing bowls, harp, a string section, a small choir), never generated loops. Always audible; ducks no more than 4 dB under combat. −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP. Bar-synced crossfades at borders over 120 m. **One planned silence: the Silent Vault** (KEPT).

**The writing:** tintinnabuli in B minor (Pärt's *Für Alina* principle): every piece has an **M-voice** (a stepwise melody, celesta or armonica) and a **T-voice** that only plays B, D and F♯ (bells, bowls, harp). The T-voice is the caves ringing; the M-voice is the player.

| Cue | Where / when | Scoring | Length, form |
|---|---|---|---|
| **Explore: The Mouths** | C1 surface, day | Harp and armonica M-voice over sea-like sustained strings; T-voice on handbells; gulls and surf left in the mix | 4:30, A-B-A, loops seamlessly |
| **Explore: Geode Halls** | C2 | Armonica M-voice, crystal bowls T-voice, very long reverb (6 s); a bell on the downbeat every 4 bars | 5:00 |
| **Explore: Crystal Forests** | C3 | Celesta M-voice over low divided strings and a breathing choir pad (the steam); T-voice on vibraphone with motor off | 4:40 |
| **Explore: Echo Tunnels** | C4 | Solo cello M-voice, T-voice on tubular bells played very softly with long tails; the reverb is the tube's (3 to 6 s) | 4:20 |
| **Explore: Ward Chambers** | C5 | A held low B pedal (organ, 49 Hz region, matching the wards' hum) under armonica; T-voice on bowed crystal | 5:20 |
| **Night variation** | Each layer above, 22:00 to 04:00 | The same pieces a fifth lower with the M-voice on alto flute | — |
| **Combat layer** | Over any explore cue | Adds timpani on beats 1 and 3, staccato string ostinato on B-D-F♯, struck crystal bars; tempo held at 60 (double-time feel at 120) | Stems sync to the bar |
| **Mini: the Twins** | Their arena | Two armonica lines in strict canon at 0.5 bar (the mirror), bells T-voice, timpani | 3:00 loop |
| **Mini: the Echo Warden** | Its hall | Sparse and tense: a bowed tuning fork, soft pizzicato and a bell answering each noise you make; during its Listening stance the cue thins to the bowed fork alone at the same level (thinner, never quieter by more than the 4 dB duck rule) | 2:40 loop |
| **Mini: the Geode Tyrant** | Wherever it fights | Low brass and bowls, heavy | 2:50 loop |
| **Boss: Lyrathis I** | Phase 1 | Full choir and armonica, T-voice on tubular bells; the beam windup is scored (a held armonica note rises with her brow gem) | 3:30 loop |
| **Boss: Lyrathis II** | Phase 2 | The same theme in canon at 0.5 s (the copies' echo), strings doubled, tempo 60 with 16th-note ostinato | 3:20 loop |
| **Boss: Lyrathis III** | Hidden phase 3 | Everything at once; organ added; the T-voice becomes the full B-minor chord on every beat | 2:50 loop |
| **Stinger: keystone chime** | Striking a hall keystone | A struck B with the hall's crystals answering in a rising arpeggio | 4 s |
| **Event: the Resonance** | During the event | A 40 Hz swell under every cue; each peal is scored as a bell-chord cluster on B | Layer |
| **Pass duet: the Severance** (world-map rule: one per pass) | The Severance | Crystal half: armonica and bells in B minor; the Void half (R10) plays reversed and detuned strings against it; until R10 the duet plays its crystal half alone | 4:00 |
| **The Silent Vault** | Inside the Vault | **Planned silence** (KEPT): music stops; only your heartbeat when HP < 30% | — |

### 2.17 The Arcane path (nodes SPEC from ST; numbers KEPT from combat where set; the rest GAP-COOKED)

**Resource: Mana (SPEC "steady classic pool"; numbers GAP-COOKED).** Capacity 100. Regenerates **6 a second at all times** (in and out of combat; steady is the point). Deep well (root, main path only, SPEC "faster Mana regen"): regen ×1.5 (9 a second). Shields refund Mana with Absorb. Mana bar violet #7A3FD1 under HP (combat HUD KEPT).

**Combat rules for every node (KEPT combat):** spells cast walking at ×0.5 for bolts, rooted for big ones; chargeable spells up to ×1.5 at 1.0 s; Wildbrands fire through the held weapon (D-pad up / 1 to 8) with stamina 18; coil ≥ 0.20 s on Wildbrands; hit stack and hitstop by tier (combat 2.14); player bolts vs players home at 2000 (combat SPEC) and never vs creatures beyond the built lead; PvP multipliers (2.20 KEPT).

**Shields (new shared system, GAP-COOKED):** a shield is an HP layer that takes damage before your HP (after guard). It shows as a faceted violet film on the body. Shields don't stack (the bigger one wins); they block statuses' buildup only while they hold. **Reflectable** projectiles (a flag on projectile rows: bolts, spits, arrows, shards, lances; not boulders, slag, beams, siege) that hit a shield with Reflect bounce straight back (2.12 rule).

| Branch | Node (ST name, exact) | Cost / gate | Effect (ST) | Numbers (GAP-COOKED unless marked KEPT) |
|---|---|---|---|---|
| Trunk | **Arcane missile** | 3 pts | Basic arcane spell | KEPT: Bolt 14 damage, mark 25, cd 0.8, 8 Mana; 2,800 cm/s; reflectable |
| Trunk | Mana capacity | 3 ranks | Larger Mana pool | +15 Mana per rank (145 at 3) |
| Trunk | Mark chance | 2 ranks | Higher chance to mark enemies; marked enemies take more spell damage | Mark buildup from your hits +20% per rank; marked enemies take +10% from your spells per rank on top of Mark's +15% |
| Trunk | **Runic strike** | 2 pts | Basic arcane weapon art | KEPT: Wildbrand, all melee, 15 Mana, cd 2.5, 0.28 / 0.20 / 0.42, ×1.3, mark 40, stance ×1.5 |
| Ward | **Barrier** | 5 pts, gate 0 | Spell: a shield on yourself | KEPT: self buff shield 30 HP for 8 s, cd 12; 20 Mana; cast 0.40 walking |
| Ward | Hardened ward | 5 ranks, gate 5 | Stronger shields | Shield HP +20% per rank (Barrier 60 at 5) |
| Ward | **Reflect** | 4 pts, gate 10 | Shields reflect projectiles | Your shields reflect reflectable projectiles (rule above) at ×0.8 damage; the shield takes no damage from a reflected hit |
| Ward | Ward allies | 3 pts, gate 14 | Barrier can target allies | Barrier gains an ally-target mode (KEPT shape: range 1,500, line of sight); tames count as allies |
| Ward | Absorb | 3 ranks, gate 17 | Shields refund Mana when hit | 15% of damage a shield absorbs comes back as Mana per rank (45% at 3) |
| Ward | **Aegis** (capstone) | 10 pts, gate 20 | A dome that blocks all projectiles | KEPT: dome r 500, 8 s, cd 60; 50 Mana; rooted 1.0 s cast drawing the eight-armed stave on the ground. Blocks every creature, boss and player projectile and ballista bolt; **a catapult stone breaks it** (stone stopped, dome shatters). Beams and melee pass. With Reflect owned: reflectable projectiles bounce back |
| Sorcery | **Arcane blast** | 5 pts, gate 0 | Spell: a heavy burst of arcane force | KEPT: nova at aim point (range 1,200) r 300, 32 damage, stance ×2, chargeable, cd 8; 25 Mana |
| Sorcery | Focus | 5 ranks, gate 5 | More spell damage | +5% Arcane spell damage per rank |
| Sorcery | **Echo** | 4 pts, gate 10 | Chance to cast a spell twice | 20% chance an Arcane spell (not capstones, not shields) casts again 0.6 s later at no cost, same aim |
| Sorcery | Quick cast | 3 ranks, gate 14 | Faster casting | Spell windups −8% per rank (never under the 0.14 s coil) |
| Sorcery | **Mana burn** | 3 pts, gate 17 | Spells break enemy shields faster | Your spells deal ×2 to shields and shield-flagged parts (player shields, Prismback and Tyrant plates, the Twins' ward guard, Lyrathis's dome); vs players each spell hit also drains 8 of the target's path resource |
| Sorcery | **Starfall** (capstone) | 10 pts, gate 20 | Rain arcane stars over an area | KEPT: Strike, 12 stars over r 700, 18 each, cd 50; 60 Mana; each star's landing point glints 1.2 s before (KEPT strike delay) |
| Rune | **Rune trap** | 5 pts, gate 0 | Spell: a ground rune that explodes | KEPT: summon a rune r 300, 40 damage on enter, 30 s, cd 10; 15 Mana; max 3 runes. Visible to everyone (a faint hexafoil on the ground) so it isn't a hidden trap; can't be placed inside another clan's claim (BL "no traps" spirit) |
| Rune | **Inscribe** | 5 pts, gate 5 | Rune on your weapon adds arcane damage | KEPT: Wildbrand, rooted 0.6, 30 Mana, cd 30: +8 arcane per hit for 30 s (with Glyph power) |
| Rune | Glyph power | 4 ranks, gate 10 | Stronger runes | Rune trap, Inscribe and Rune circle +10% per rank |
| Rune | **Rune link** | 3 pts, gate 14 | Nearby runes link and damage enemies between them | Your runes within 8 m of each other join with a violet thread: an enemy crossing a thread takes 12 and mark 15 (once per 0.5 s) |
| Rune | Lasting runes | 3 ranks, gate 17 | Runes last longer | Rune duration +25% per rank |
| Rune | **Rune circle** (capstone) | 10 pts, gate 20 | A great circle that buffs allies and weakens enemies inside | KEPT: pool r 600, allies +15% damage, enemies mark 10 a second, 15 s, cd 60; 60 Mana |

Sums (ST shape, importer rule): trunk 3+3+2+2 = 10; Ward 5+5+4+3+3+10 = 30; Sorcery 5+5+4+3+3+10 = 30; Rune 5+5+4+3+3+10 = 30.

**Status: Mark (KEPT combat).** +15% damage from all sources for 10 s; marked targets show through walls to the marker's party (an outline in arcane violet); threshold by tier (KEPT); in PvP it shows only to the marker's clan and allies.

**Path magic (taming):** on Prismback (2.12.1).

#### 2.17.5 Combos: the 10 new Arcane pairs (names SPEC from CB; numbers GAP-COOKED; passive unless stated, the FULL-GAME default)

| Pair | 25/25 | 50/50 |
|---|---|---|
| Fire + Arcane | **Mana flame:** burns drain enemy shields: a burning target's shield (or shield part) loses 15 a second | **Meteor storm:** Starfall drops burning meteors: each star adds burn 30 (needs Starfall) |
| Lightning + Arcane | **Quick mind:** faster casting at high Charge: Arcane windups −15% while Charge > 60 | **Ball lightning (active):** a slow orb, 300 cm/s for 6 s, shocks everything within 4 m (shock 20 a second, 6 dmg a tick); 30 Charge + 30 Mana, cd 20; a quick-slot entry |
| Sun & sand + Arcane | **Prism:** spells split into beams of light: Arcane missile splits into 2 bolts at 60% each | **Daystar:** Starfall stars blind (blind 25 each; needs Starfall) |
| Nature + Arcane | **Mana sap:** Growth and Mana regenerate together: Mana regen +25% while Growth > 50, and the reverse | **Faerie ring:** Rune circle grows plants that heal allies inside 4 HP a second (needs Rune circle) |
| Wind + Arcane | **Mana wings:** gliding regenerates Mana: +6 a second while gliding | **Rune volley:** your arrows leave a rune where they land (a Rune trap at 50%, max 2 extra) |
| Arcane + Frost | **Frost missile:** Arcane missiles chill: +15 frost buildup each | **Frozen rune:** runes freeze: Rune trap adds frost 100 (Frozen on most creatures; bosses Rimed, KEPT) |
| Arcane + Blood & curse | **Blood magic:** spend HP when out of Mana: 1 HP per 2 Mana missing | **Blood star:** Starfall causes bleed 25 per star (needs Starfall) |
| Arcane + Tide | **Mana tide:** healing waves restore Mana: 20 Mana to each ally they touch | **Moon tide:** healing waves also shield allies: a 25 HP shield for 6 s |
| Arcane + Necromancy | **Bound dead:** raised minions last twice as long | **Phylactery:** runes store Souls that revive fallen minions: a minion dying within 8 m of your rune rises once at 50% |
| Arcane + Spore | **Fungal mind:** sporelings regenerate your Mana: +2 a second per living sporeling | **Glowcap colony:** sporelings cast Arcane missiles (8 damage, every 3 s) |

Arcane + Void (Order and chaos / Paradox) lands with Void in R10 (FRP).

### 2.18 Body change: Arcane (frame and row KEPT from hero 2.6; stages GAP-COOKED)

| Part | Detail |
|---|---|
| Mask and colour (KEPT) | B · mark field; violet #7A3FD1; runes light to 2 nits for 2 s on each cast |
| The pattern (GAP-COOKED) | **Hexafoil arcs and compass lines** (the Wardcarver glyph, 1.2): six-petal rosettes linked by fine straight lines like a surveyor's construction drawing, with the tiny compass-point dots at each centre |
| Strength 0.3 (wrists) | One rosette on the inside of each wrist, faint, visible only when lit by a cast |
| Strength 0.6 (forearms) | Rosettes and lines up both forearms to the elbow; a faint resting glow (0.2 nits) in the dark |
| Strength 1.0 (forearms and back of the neck) (KEPT) | Full forearms plus a single large rosette at the nape that shows when hair is up; **fingertips leave a brief violet trace line in the air when casting** (owner and 15 m, 0.4 s) |
| Eyes (KEPT violet rim) | At 1.0 a fine faceted fleck pattern in the iris that catches light like a cut stone |
| Sub path | 0.6 × strength; the main path draws on top (KEPT) |
| Sound | A soft crystal chime layered on every cast at 1.0 |
| With Mark | Your marks on enemies use the same rosette (the marked outline carries a small hexafoil) |

### 2.19 Gear and items from the caves

#### 2.19.1 Wardcloth (KEPT: survival-crafting set and numbers, hero silhouette)

Light, poise 30. Inputs: steel plate 6, resonant crystal 8, linen 10, Chimecrab dust 4. 2 pieces: Mark buildup ×0.5. 4 pieces: walk through ward fields; spell cost −10%. **Look (both cooks merged, no conflict):** a deep violet rune-sewn long coat open over a fitted bodice (female) or doublet (male), cut close at the waist; a high collar standing up in crystal points (hero hook); crystal studs on the cuffs that hum when you cast; rune-stitched gloves with bare fingertips; tall boots. Forearms bare between glove and sleeve so the rune lines show (hero "open windows: forearms, face"). Palette violet, midnight blue, crystal white.

#### 2.19.2 Weapons, talismans, consumables (GAP-COOKED unless KEPT)

| Item | Kind | Made | Inputs | Effect |
|---|---|---|---|---|
| **Prismglass spear** | Boss weapon (spear class, combat moveset) from Lyrathis's blueprint | Forge L4 | Steel ingot 4, doubling spar 2, resonant crystal 6, glass leg 1 | Charged heavy splits into 3 thrust-lines of light 15° apart (each ×0.5 of the hit); light attacks build mark 12. Element by path material as usual |
| Glassleg bow | Bow | Workbench L3 | Glass leg 2, Echobat membrane 1, sinew 2 | +15% draw speed; arrows quieter (half noise) |
| Drusy shield | Shield | Armorer L3 | Drusy plate 2, iron 4 | Guard boost as a medium shield; blocking a projectile rings it (a 20 m noise, lights crystals) |
| Prism plate shield | Shield | Armorer L4 | Prism plate 3, steel plate 2 | Blocking a reflectable projectile with a perfect guard (first 0.2 s) reflects it (2.12 rule) |
| Crystal lamp (KEPT) | Placed light | Workbench L4 | KEPT | KEPT |
| Lumen lantern | Held light | Workbench L2 | Crystal antler shard 1, bronze 2 | 12 m cold light, no fuel; strike it (Use) to flare 20 m for 5 s (a 10 m noise) |
| Ward charm (KEPT) | Talisman | Alchemy 3 | KEPT | Ward fields |
| Attuning chalk (KEPT name) | Consumable | Hand | 2.7 | KEPT effect |
| Ward fork | Consumable | Found (Vault, Echo Warden) | — | Struck: the nearest ward field within 10 m drops 60 s |
| Silver bait | Taming food | Workbench | Silver ore 1, raw meat 1 | Alicanto |
| Wax earplugs (KEPT item) | Consumable | KEPT | KEPT | Here: Echobat Shriek stagger off for 10 min, but you hear click trains at half volume (a trade) |
| Talismans from trophies | Talisman | — | Trophy + bronze 2 | Twin shard, Warden's tuning fork, Tyrant plate (2.13) |
| Wardcarver compass | Talisman (found, S3) | — | — | Ward fields wait 1.5 s longer to re-throw you |
| The Carver's Chisel | Talisman (found, V6) | — | — | Runes in your sockets work at 125% |
| Venn's ring | Ring (quest 6) | — | — | Mana regen +15%; ward fields never throw you |
| Amethyst | Gem (new) | Found band 3 | — | +10 max path resource of the item's path |

#### 2.19.3 Rune sockets (R9 shared system; SPEC "gem and rune sockets", "runes crafted or found"; crafted runes KEPT from survival-crafting)

| Rule | Detail (GAP-COOKED) |
|---|---|
| Sockets | Runes use the **same sockets as gems** (quality crude 0 to masterwork 3, SPEC). An item can hold any mix of gems and runes; one of each rune per item (KEPT) |
| Socketing | At the station that made the item (forge, armorer); 5 s; removing destroys it (SPEC) |
| Look | A socketed rune shows as a glowing glyph etched on the blade or plate (violet for found Wardcarver runes, the biome material's colour for crafted ones) |
| Crafted runes (KEPT) | The 8 at the alchemy table L3+: Thirst, Haste, Anvil, Tide, Thorns, Grave, Clear Sight, Hollowing |
| **Found Wardcarver runes (new, crystal caves and rank 3+ portals only)** | **Rune of Silence** (V6 only, unique per player): footsteps, rolls and attacks make half noise · **Rune of the Prism:** spell hits split a 30% copy to a second target within 6 m · **Rune of Warding:** a perfect guard gives a 15 HP shield for 3 s · **Rune of Doubling:** 5% chance a hit lands twice (second at 50%). Found in vault caches behind ward fields (C5, 1 of each), singing geodes in the Resonance (10% each), rank 3 to 5 portals (DD "found") |
| PvP | Rune effects follow the PvP block (combat KEPT ×0.6 player damage applies to their damage parts) |

### 2.20 The story the caves tell, with no words (fits world-map's Twelve Peoples and the Void–Arcane war at the Severance)

No staged scenes, no cutscene actors, no narrator (DD). Five things the player pieces together, each from places:

1. **They sang the crystal.** The Singing School (H1), the chime walls, ochre dots at every resonant spot (C4), the keystone chimes in every hall, the Chime Garden (S6). The Wardcarvers lit and grew their world by sound.
2. **They warded the Severance.** Hexafoils on every door; lead keystones; the watch-cairns aimed west (S1); the Severance Gate's racked weapons facing the door, never drawn (H8); the eight light shafts (L1) built to shine a Sovereign's light over the gorge.
3. **The first Sovereign failed.** The Hollowed Giantess kneels at the gorge, hollowed from the front, inside her ward cage (L2); the southern watch-cairns torn out and scattered toward the gorge (S1); in the Sovereigns' Mould one name-plate scratched out (V5). The Void champion's ash-cast in its crystal cage on the far side (KEPT) shows they gave as good as they got.
4. **The Void heard them.** The Tally Wall's hexafoil tallies crossed through and getting shakier (H5); the wax earplugs in bowls everywhere (H5, V1, S4); the Echo Warden built to kill any sound near the Vault; the Hollow Choir's name on the far side (world-map KEPT). They stopped making noise.
5. **They chose silence.** The Silent Vault: forty seated with wax-sealed mouths, unafraid (V2); the Archive of Forks (V3); the last carver one petal short of finishing her hexafoil (V6). And Lyrathis, alone in her dome with the prisms turned inward, guarding what's left.

**Lore items (item descriptions only, Souls-style, KEPT rule):** Watch tally, Child's tuning fork, Count of the quiet, Order of the door, Two plates, Wax seal, The miner's last page (a newcomer). Each 2 to 4 lines, never a full explanation.

### 2.21 The PvP pass (R9; RAVEN-PLANNED in BP Full release PvP and DD Multiplayer; details GAP-COOKED unless KEPT)

**SPEC frame (kept):** open PvP everywhere but safe zones; PvE-only option; death penalty per server (Souls-like default); offline raid protection (0 to 100%); clans with ranks and alliances; anti-zerg clan size limits; explosives, raid tools, path-based raiding (fire burns wood walls, necromancers raise the dead inside), tames in raids; bounties and infamy; contested bosses; PvP inside boss arenas (setting). **Ruled out (kept out):** raid windows, territory control points, PvP arenas, scheduled sieges, traps and NPC guards in bases. **PvP combat numbers** (combat 2.20 KEPT): ×0.6 player damage, buildup ×0.7, crit arcs, 250 ms rewind, combat tag 15 s, logout body 30 s.

#### 2.21.1 Clan ranks

| Rank | Default rights (each toggle editable by the Leader per clan) |
|---|---|
| **Leader** (1) | Everything; transfer leadership; disband; edit rank rights; alliances |
| **Officer** | Invite, kick Members and Recruits, promote to Member; demolish clan pieces; Hearth cores and upgrades; claim upkeep; set chest/door minimum ranks |
| **Member** | Build; open chests and doors set to Member or lower; ride and order clan tames set to Member; use stations |
| **Recruit** | Build; can't demolish; chests and doors set to Recruit only; can't take Hearth cores; can't order clan tames; **promotion to Member at Officer's choice, earliest 48 real hours after joining** |

- **Per-object minimum rank (Ark's rule, the one Conan players asked for):** every chest, door, station, tame and the Hearth has a "minimum rank" field (default Member; the Hearth Officer). Set it from the object's own panel (mouse and pad).
- **Saved** in the world's `clans.json` (BUILT file) with ranks and rights.
- **Clan log:** a tab listing the last 200 events (joined, promoted, kicked, chest opened by Recruit, piece destroyed by whom) so a betrayal is visible.

#### 2.21.2 Alliances

| Rule | Detail |
|---|---|
| Form | Leader to Leader: propose, accept (both sides). An alliance has a name and up to **3 clans** |
| Shared | No friendly fire between allies (players and tames); allies show on each other's map and compass in a second colour (teal, not gold or crimson); shared alliance chat; allies may use each other's **beds** if the owner clan allows it |
| Not shared | Claims, building, chests, doors, tames and the Hearth (alliance ≠ clan, Ark rule) |
| Leave | Any clan leaves at once; a 24 h cooldown before it can join another alliance (no flip-flopping mid-raid) |
| Server setting | Alliances On / Off (default On on PvP standard; Off makes it clans only) |

#### 2.21.3 Anti-zerg limits

| Limit | Detail |
|---|---|
| Clan size (SPEC setting 2 to 50 or none; PvP standard 8) | Kept |
| **Alliance size** (new setting) | Total members across an alliance ≤ clan size limit × 1.5 (PvP standard 12). An alliance can't form or take a clan that would pass it |
| **Outnumbered** (new setting, default On on PvP standard) | When you're in combat with players and the hostile players within 40 m who have hit you or your clan in the last 15 s outnumber your side's players within 40 m by 3 or more: player damage you take ×0.85 and stamina regen +15%. Measured by who is fighting whom near you, not by squad, so splitting a group doesn't game it. Tames don't count. A state word "OUTNUMBERED" shows once when it starts |
| Raid groups | No raid caps (DD rules out party limits; DR #22 says "tuned for, not capped") |

#### 2.21.4 Explosives and raid tools (KEPT from survival-crafting 2.16 and 2.18; additions GAP-COOKED)

| Tool | Detail |
|---|---|
| Fire pot, black powder, powder charge, mining charge (KEPT) | Numbers as survival-crafting. Black powder = sulfur (Volcanic) + **saltpetre (richest at this biome's Echobat roosts, H6 and C4)** + charcoal: the raid economy crosses the map |
| Raid costs per wall (KEPT) | Wood 1 charge, stone 2, metal 9, biome 12 |
| **Siege ladder** (new) | Workbench L3: wood 20, rope 6. An item you plant against a wall (not a building piece, so it ignores claim build rules): 6 m, climbable, lasts 10 min or until broken (300 HP). Lets raiders go over a palisade instead of through it |
| **Code breaker** (new) | Workbench L4: steel plate 2, silver 2, resonant crystal 1. Held to a code-lock keypad for a **60 s channel** you can't move during, ringing loudly (a 40 m noise): opens the door's lock (not the door's HP) once. Every member of the owning clan online gets the raid bell (below) at the channel's start |
| **Raid bell** (new, defenders) | Each claim totem rings: when any piece or the Hearth in the claim takes damage from a player, a player's tame, an explosive or siege, every online member of the owning clan gets a state word "CLAIM ATTACKED" with the claim's name and a map ping; offline members see it at login with the time. Offline protection (BUILT) reduces the damage as set |
| Hearth (BUILT) | Raiders can damage it; broken = no crafting, cores safe |

#### 2.21.5 Siege weapons (KEPT numbers; mechanisms and behaviour GAP-COOKED)

**Machines look engineered (Aaron 11-14):** every siege engine is built from parts that visibly work: axles in bearings, ropes over pulleys, ratchets that click, nothing passing through anything. Each moves smoothly on its mechanism: no snaps.

| Engine (KEPT stats) | Mechanism (how it really works) | Crew and behaviour |
|---|---|---|
| **Battering ram** (steel plate 6, heartwood 10; doors and gates only; 250 a hit every 3 s; pushed by 2 players or 1 hauler) | A heartwood log with a steel cap hung by two chains from an A-frame on four solid wheels under a hide-covered roof. The log swings back on its chains (pull) and forward (release) in a real pendulum arc; the wheels roll, never slide | Two push handles (Interact to take one); a hauler harness at the back. Push to move (100 cm/s); hold Attack to draw back (1.5 s), release to strike. Its roof shields the crew from above: arrows and fire pots from above hit the roof (hide HP 800) |
| **Catapult** (KEPT as a counterweight trebuchet: wood 60, steel plate 6, rope 10; 2 m boulders, 15 s reload, 20 to 60 m; arc shown to everyone; 500 to a piece, 80 to anyone it lands on) | A throwing arm on an axle in two A-frame towers; a box counterweight full of stones on the short end; a sling on the long end; a winch with a ratchet pawl hauls the arm down against the counterweight; a trigger pin releases it. The arm swings up on its axle, the sling opens at the top, the counterweight falls into the frame and rocks to a stop | Two crew: one winches (hold Interact; 10 s), one loads and aims (rotate on its base turntable, 10° a second; range by sling release pin, 3 settings). The full flight arc draws for **everyone** in a thin line the whole 15 s reload and the flight (no surprise shots). Aegis stops one stone (2.17) |
| **Ballista** (steel plate 8, wood 30; bolts at a tame or player; 2 s draw visible; straight flight; never homes; 220 to creatures, 120 to players) | A bow of steel leaves on a stock on a swivel tripod; a windlass draws the string back to a catch; a trigger releases | One crew; draw 2 s with the windlass visibly turning; a 0.9 s red aim line before release (the turret rule, survival-crafting KEPT) |

Siege engines are **items you place** (not claim pieces); they can be placed inside another clan's claim radius (that's what they're for) and have HP (ram 2,000, catapult 1,500, ballista 800). Raven rules out scheduled sieges and raid windows; sieges happen whenever players bring them.

#### 2.21.6 Path-based raiding and tame raiding (KEPT from survival-crafting 2.18; Arcane line GAP-COOKED)

| Path | Raid role |
|---|---|
| Fire (SPEC) | ×2 vs wood |
| Lightning | ×1.5 vs metal (KEPT) |
| Spore | Rot ×1.5 vs wood over time (KEPT) |
| Void | ×1.25 vs everything (KEPT) |
| Necromancy (SPEC) | Raises through walls within 12 m at 3× Soul cost (KEPT) |
| **Arcane (new)** | The defender's path: **Aegis stops a catapult stone and all ballista bolts and arrows** for 8 s (2.17); Barrier with Ward allies shields a door-holder; **Mana burn** drains attackers' resources. On attack: Arcane blast ×1.25 vs stone (force), Rune trap can't be placed inside an enemy claim (no traps in bases) |
| Others | ×1 (KEPT) |
| **Tames (KEPT)** | Bite pieces at bite damage ×0.5; haulers push rams; tames vs players ×0.5 (combat KEPT) |

#### 2.21.7 Bounties and infamy (SPEC "bounties on player killers, and infamy that marks aggressive players"; server setting Bounties and infamy On/Off: SPEC, PvP standard On)

**Infamy** (0 to 100, per character, saved):

| Gain | Amount |
|---|---|
| **Unprovoked kill:** you kill a player who hadn't hit you, your clan, allies or tames in the last 60 s | +10 |
| …and they were 10+ character levels below you | +10 more |
| …within 300 m of a spawn camp's safe zone edge | +5 more |
| Killing a player inside their own claim while every member of their clan is offline | +5 |
| Raiding (pieces, Hearth), killing in boss arenas with PvP on, killing a player who hit you first, killing a player with infamy ≥ 30 | 0 (those are the game) |
| **Decay** | −5 per real hour of play (not offline) without an unprovoked kill; −30 when you die to a player |

**Infamy marks (the "marks aggressive players" part):**

| Infamy | Mark | Effect |
|---|---|---|
| 30+ **Branded** | A soot-black brand glyph on your name plate and a small black-iron sigil over your head visible at 60 m (colour law: not gold, not crimson) | You can be bountied by anyone (below) |
| 60+ **Hunted** | Your rough position shows on every player's map as a 150 m smudge, updated every 60 s (Sea of Thieves' Reaper's Mark, unchosen) | **Spawn camps are not wake points for you** (beds and bedrolls only), and you can't enter camp safe zones (you're stopped at the edge with a state word). An automatic world bounty of 1,000 path XP is on your head |
| 90+ **Reviled** | Exact map pip every 30 s | World bounty 2,000 path XP |

**Bounties:**

| Rule | Detail |
|---|---|
| Boards | A **bounty board** stands in every spawn camp and village (a carved post with nailed notices; mouse and pad screen) |
| Who can be posted | Any player with infamy ≥ 30, or anyone who killed you in the last 24 h |
| Stake | Items, up to 10 stacks, held by the board in escrow (Raven's design has no currency; items are the price). The world adds path XP for Hunted and Reviled (above) |
| Claiming | Land the killing blow, or deal ≥ 30% of their HP in the 30 s before they die (Raven's loot-ledger spirit). Clanmates and allies of the target can't claim. The same claimer can't claim the same target again for 24 h |
| Pay | Escrow items go to the claimer's next visit to any board; path XP pays at once to their main path |
| Expire | A bounty expires after 7 real days; the stake returns to the poster's next board visit |
| Off on PvE | Hidden and inactive when PvP or the setting is off (SPEC) |

#### 2.21.8 Contested bosses (SPEC: "contested bosses that rival clans can fight over"; the 10% rule BUILT includes rivals; PvP inside boss arenas is a setting, BUILT)

| Rule | Detail (GAP-COOKED) |
|---|---|
| Ledger | BUILT: anyone who deals 10% gets their own loot, rivals included |
| Rival in the arena | When a second clan engages an already engaged boss, both clans get a one-time state word "RIVAL CLAN" and the boss bar shows a small count of clans in the fight |
| HP | Rescales on join (BUILT) |
| **Two run-back stones** per major arena, one at each door (2.14.1 for Lyrathis; every new major gets two), so no clan can camp the only way back. Minis get one, as BUILT |
| No walls round bosses | No claims within 250 m of arenas and run-back stones (world-map KEPT) |
| PvP in arenas | Setting (BUILT); infamy never counts kills in an arena with PvP on |
| Ruled out (SPEC) | No server-wide boss announcements, no first-kill announcements, no world boss |

#### 2.21.9 Screens (mouse and pad on every one, F1 page, no dead states; ui-controls rules KEPT)

| Screen | Where | Contents |
|---|---|---|
| Clan tab (BUILT, key O) | Journal | New sub-tabs: **Members** (rank dropdown per member, kick, promote), **Ranks** (rights grid: rows = rights, columns = ranks, toggles), **Alliance** (name, member clans, propose/accept/leave, setting state), **Log** (last 200 events) |
| Object panel | Interact on a chest, door, station, tame, Hearth | A "Minimum rank" selector |
| Bounty board | Interact on a board | Posted bounties (target, stake, expiry), Post (pick target from eligible list, drag up to 10 stacks), Collect (your claims and expired stakes) |
| HUD | Always | Infamy brand next to your name when ≥ 30; state words (CLAIM ATTACKED, RIVAL CLAN, OUTNUMBERED) once each; ally markers teal |
| Map (BUILT) | M | Hunted smudges, Reviled pips, raid-bell pings, ally markers |
| Siege engines | Interact | Crew prompts only (one prompt at a time, combat prompt diet) |

### 2.22 Invasions: Arcane ↔ Void through the Severance (DD SPEC pair; FULL-GAME 3.3 numbers KEPT; switches on when R10 opens the Void)

| Part | Detail |
|---|---|
| Rule (KEPT) | Server setting; 5 min warning to the defending biome; 8 to 14 attackers plus one mini through the pair's pass; they march on the nearest claim or shrine within 1.5 km and hold a shrine 30 min (fast travel to it shuts); beating it pays both paths' materials and 2,000 path XP to everyone with 10% |
| Until R10 | The Severance's west end stays sealed by rockfall (FULL-GAME rule: unbuilt wedges closed) |
| **Crystal invaders into the Void** | 4 Prismbacks, 4 Geodemite swarms, 3 Echobats and a Lumenhorn bull, led by **the Geode Tyrant**; they march west through the gorge. Their noise lights the gorge's grey crystal veins as they pass (the warning sign on the Void side) |
| **Void invaders into the Crystal caves** | Come through the Severance Gate (H8); **the Wardcarvers' ward fields still work against them:** void creatures take the ward throw ×2 (2.8), so defenders who drop and re-raise fields by their keystones turn the gate into a fight on their terms. Their target: the Severance Shrine or the nearest claim within 1.5 km (the camps' plateau) |
| Warning signs (world, no UI beyond the 5 min notice) | The Hollowed Giantess's cage rods ring; the ward fields at the gorge flicker violet; the pass duet's Void half swells |

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always (CLAUDE.md):** data first in `Data/*.json` with first-draft numbers read through `WRTune`; every path ability is a `UWRGameplayAbility` subclass registered in `UWRAbilityRegistry::ClassForNode`; all damage through `UWRCombatLibrary::ApplyHit` on the server; statuses as replicated loose tags owned by `UWRStatusComponent`; server authority, clients send intent; sound and FX are named events (`WRSound`, `WRFx`) with data rows; dressing spawned at runtime from data, `L_Island` touched as little as possible. Gates: compile, cook, package. Aaron tests through F8.

**R9 needs in place first (from earlier rounds; not rebuilt here):**
- The **cavern frame** (biome-fungal 3.0: Nanite cave kit, landscape holes, `UWRUndergroundSubsystem` + `IsUnderground`, climb lanes, reverb volumes).
- World-map's B6 terrain step (the Severance gorge and the Crystal plateau), with the Severance west end sealed until R10.
- Combat C0 to C12: attack-row runner, visible-contact damage, stance and parts, status framework (Mark is added here), PvP block, combat tag.
- Shrines, path masters, roaming minis, biome events (fire-frost frame); `combos.json` (R1); quests.json.
- survival-crafting R4 items: steel, Hearth 4 (siege needs L4), the explosives chain, silver ore; R2's four armour slots and talismans.

### 3.1 Data rows

| File | Change |
|---|---|
| **New `Data/paths/arcane.json`** | Same schema as `frost.json`: `id: "arcane"`, `name: "Arcane"`, `biome: "crystal"`, `starter: "arcane_missile"`, `resource {id: "mana", name: "Mana"}`, `rootPassive {id: "deep_well", name: "Deep well", text: "Mana regenerates faster."}`; trunk `arcane_missile (ability 3), mana_capacity (stat 3), mark_chance (stat 2), runic_strike (ability 2)`; branches `ward: barrier(5,g0), hardened_ward(stat 5,g5), reflect(4,g10), ward_allies(3,g14), absorb(stat 3,g17), aegis(capstone 10,g20)`; `sorcery: arcane_blast(5,g0), focus(stat 5,g5), echo(4,g10), quick_cast(stat 3,g14), mana_burn(3,g17), starfall(capstone 10,g20)`; `rune: rune_trap(5,g0), inscribe(5,g5), glyph_power(stat 4,g10), rune_link(3,g14), lasting_runes(stat 3,g17), rune_circle(capstone 10,g20)`. Node `text` copied from ST word for word |
| `Data/tuning.json` `resources` | `mana {capacity: 100, regen: 6, deepWellMult: 1.5}` |
| `Data/tuning.json` `nodeEffects` | `arcane.mana_capacity {manaCapacity: 15}`, `arcane.mark_chance {markBuildup: 0.20, markedSpellBonus: 0.10}`, `arcane.hardened_ward {shieldHealth: 0.20}`, `arcane.absorb {shieldManaRefund: 0.15}`, `arcane.focus {arcaneSpellDamage: 0.05}`, `arcane.quick_cast {spellWindup: -0.08}`, `arcane.glyph_power {runePower: 0.10}`, `arcane.lasting_runes {runeDuration: 0.25}` |
| `Data/tuning.json` `passives` | `reflectDamage: 0.8, echoChance: 0.20, echoDelay: 0.6, manaBurnShieldMult: 2.0, manaBurnDrainPvp: 8, runeLinkRange: 800, runeLinkDamage: 12, runeLinkMark: 15, runeLinkTick: 0.5, runeMax: 3` |
| `Data/tuning.json` `spells` | Arcane entries KEPT from combat 2.13 plus costs `{arcaneMissile: 8, barrier: 20, arcaneBlast: 25, runeTrap: 15, aegis: 50, starfall: 60, runeCircle: 60, runicStrike: 15, inscribe: 30}`, `barrier {health: 30, seconds: 8, cd: 12}`, `aegis {radius: 500, seconds: 8, cd: 60, catapultBreaks: true}`; `shapes` reuse combat's |
| `Data/tuning.json` `status.mark` | KEPT numbers (combat 2.11): `{damageTaken: 0.15, seconds: 10, color: "#7A3FD1", throughWalls: "party"}` |
| `Data/tuning.json` `shields` (new) | `{order: "afterGuard", stack: "max", blocksBuildupWhileUp: true}` |
| `Data/tuning.json` `noise` (new) | Radii from 2.1: `{walk: 400, run: 800, sprint: 1500, roll: 800, land: 600, hitCreature: 1200, hitCrystal: 2000, cast: 1000, bellow: 4000, chime: 2500, powderCharge: 8000, crouch: 0, stone: 1500, codeBreaker: 4000}` |
| `Data/tuning.json` `crystalLight` (new) | `{rest: 0.15, peak: 4.0, fade: 6.0, keystoneLit: 2.0, keystoneMinutes: 20, rainRest: 0.6}` |
| `Data/tuning.json` `hazards` | `wardField {damage: 25, throw: 600, interval: 1.5, tell: 0.9, tellRange: 300, keystoneDrop: 90, voidMult: 2.0, chalkDamageMult: 0.5}` (25 / 600 / 1.5 KEPT), `chandelier {noiseTrigger: 1500, tell: 1.0, radius: 300, damage: 70, impact: 5, rehangHours: 48, bossDamage: 400, bossStun: 6}`, `scaldingPool {burnPerSecond: 15, dps: 4, immersedDps: 12}`, `thinFloor {holeRadius: 400, breakOn: ["heavy","roll","land","heavyCreature"]}` |
| `Data/tuning.json` `weather` | Crystal surface weights (world-map KEPT); `dripSong {restGlow: 0.6, echobatHearing: 0.7}` |
| `Data/tuning.json` `creatureAI` | Every row in 2.12.2 in the combat 2.18.1 schema. New kinds: `plate_turn, glare, leg_spear, leg_sweep, stilt_step, lift, shed_leg, click_train, shriek, latch_mite, burst_death, bellow, roll_ram, plate_crush, hook_claws, tongue_flick`; `hearingHunter` true for Echobat; `vibrationHunter` for Spindleglass; `reflector` with plate parts for Prismback; `parts` arrays (Prismback 4 plate rows, Spindleglass 8 legs) |
| `Data/tuning.json` `bosses` | `lyrathis {stance: 160, parries: 3, prismCount: 8, prismRing: 2200, turnDegrees: 45, turnSeconds: 0.8, turnCooldown: 0.8, domeAt: [0.85, 0.65], domeHealth: 1200, domeSeconds: 25, domeBeamDps: 400, domeStagger: 3, copyDelay: 0.5, copyDamage: 0.6, copyRespawn: 20, copyDistance: 1500, lanceStay: 20, lanceHits: 3, p3TurnRate: 30, p3ThreePrismWindow: 10, p3Stagger: 8}` plus each move row of 2.14.2; `mirrored_twins {swapEvery: 12, swapTell: 1.0, lensHits: 3, lensRespawn: 20, spareLenses: 3}`; `echo_warden {hearRadius: 3000, chimeHammerSeconds: 4, chimeBreakHits: 3, listenSeconds: 2.0}`; `geode_tyrant {plateRows: 6, plateStance: 60, plateHealth: 300, chandelierDamage: 400, chandelierStun: 6, loopDays: 1.5, restHours: 2}`; run-back stones (2 for lyrathis); lock points |
| `Data/tuning.json` `events` | `resonance {warnSeconds: 300, pealEvery: 240, pealSpeed: 30000, pealSeconds: 6, deafenSeconds: 30, singingGeodes: 6, geodeYield: 12, payout: 3.0, wardPattern: "B"}` |
| `Data/tuning.json` `clans` (new) | `{ranks: ["leader","officer","member","recruit"], recruitPromoteHours: 48, defaultRights{...2.21.1}, defaultMinRank: "member", hearthMinRank: "officer", logSize: 200}` |
| `Data/tuning.json` `alliances` (new) | `{maxClans: 3, sizeMult: 1.5, leaveCooldownHours: 24, color: "#3FB8B0"}` |
| `Data/tuning.json` `outnumbered` (new) | `{radius: 4000, margin: 3, recentSeconds: 15, damageTaken: 0.85, staminaRegen: 1.15}` |
| `Data/tuning.json` `infamy` (new) | `{unprovoked: 10, lowLevelGap: 10, lowLevelBonus: 10, nearCampRange: 30000, nearCampBonus: 5, offlineClaimKill: 5, provokedSeconds: 60, decayPerHour: 5, onDeath: 30, branded: 30, hunted: 60, reviled: 90, huntedSmudge: 15000, huntedEvery: 60, reviledEvery: 30, worldBounty {hunted: 1000, reviled: 2000}}` |
| `Data/tuning.json` `bounties` (new) | `{stakeSlots: 10, claimShare: 0.30, claimWindow: 30, reclaimHours: 24, expireDays: 7, postOnKilledHours: 24}` |
| `Data/tuning.json` `siege` | KEPT numbers (survival-crafting) plus mechanism timings: `ram {drawSeconds: 1.5, pushSpeed: 100, roofHealth: 800, health: 2000}`, `catapult {winchSeconds: 10, turnRate: 10, ranges: [2000, 4000, 6000], health: 1500}`, `ballista {drawSeconds: 2.0, aimLine: 0.9, health: 800}`, `ladder {height: 600, minutes: 10, health: 300}`, `codeBreaker {channel: 60, noise: 4000}` |
| `Data/server_presets.json` | Add `alliances` (On/Off), `allianceSizeMult`, `outnumbered` (On/Off); PvP standard: alliances On, outnumbered On; PvE and Casual: n/a (PvP off). Bounties and infamy stays the SPEC setting |
| `Data/creatures.json` | 9 creature rows (2.12.1: tier, pack, behaviour, archetype, role, element `arcane`, `resists ["ward_field"]`, taming, difficulty, food, attacks, activity, loot, mesh, anims; mount for Lumenhorn (`rideSpeed 1300`, `riderLight 1500`), hauler for Druseback (`carry 200`), utility `sense_gems` (Gemsniffer), `light_lead_ore` (Alicanto), produce for Chimecrab KEPT); 4 boss rows (`lyrathis` major, `mirrored_twins`, `echo_warden`, `geode_tyrant` with `roaming` and `route`) |
| `Data/spawn_regions.json` | Crystal regions C1 to C5 (C2 to C5 by cavern volume id) and `night` tables from 2.12.3; `raiders` list (aggressive fighters by band; the Tyrant 1 raid in 5 from Hearth 3, survival-crafting rule) |
| `Data/items.json` | `resonant_crystal, crystal_dust (exists in survival list), selenite, selenite_pane, doubling_spar, amethyst (gem: +10 path resource), ochre, wax, saltpetre (exists), prism_plate, glass_leg, clear_membrane, venom_gland, echobat_membrane, mite_crystal, crystal_antler, drusy_plate, glow_feather, arcane_essence, core_lyrathis, prism_heart (boss part), trophy_twin_shard, trophy_warden_fork, trophy_tyrant_plate, wardcarver_compass, carvers_chisel, venns_ring, ward_fork, silver_bait, attuning_chalk, lumen_lantern, prismglass_spear, prismglass_spear_bp, glassleg_bow, drusy_shield, prism_plate_shield, siege_ladder, code_breaker`; runes `rune_silence, rune_prism, rune_warding, rune_doubling` (`category: "rune"`); lore items (7) |
| `Data/recipes.json` | 2.19.2 and 2.7 recipes; Wardcloth and ward charm KEPT inputs; siege ladder, code breaker |
| `Data/pieces.json` | `bounty_board` (decor kind `board`, placed in camps and villages by world data, not buildable by players); no other new pieces (siege engines are items) |
| `Data/world_layout.json` (world-map's) | Cavern volumes C2 to C5 by level, the Silent Vault, descents D1 to D4 as links, the Severance Gate link, `underground` flags |
| `Data/world_pois.json` (fire-frost's file) | C1 polygon; every POI of 2.4 with x, y, z; 5 landmarks; 34 ward fields with keystone positions and pattern A/B membership; 48 resonant nodes; hall keystone chimes; chandeliers; scalding pools; thin floors; 6 singing geodes; chime-stone waymarks every 150 m on the shrine road; the Tyrant's route spline; Lyrathis's 8 turntables and shafts |
| `Data/quests.json` | Saga Venn's 6 quests (2.11) |
| `Data/combos.json` | The 10 Arcane pairs (2.17.5) with `grants`; Ball lightning `kind: "active"`, `cost {charge: 30, mana: 30}` |
| `Data/Audio/sounds.json` | `creatures.<id>` voice sets for the 9 and 4 bosses (2.15); pools `explore.crystal.surface/C2/C3/C4/C5`, `night.crystal.*`, `combat.crystal`, `mini.twins`, `mini.echo_warden`, `mini.geode_tyrant`, `boss.lyrathis` (3 sections), `event.resonance`, `pass.severance.crystal`, `stinger.keystone`, `sub.silent_vault` (planned silence + heartbeat); beds per sub-zone with reverb presets; ward hum, Chladni dust hiss, chandelier tinkle, turntable lever/roller/pawl, lens chain, siege engine foley sets |
| `Data/fx.json` | Crystal flare (per-instance emissive pulse), keystone chime wave, ward field shimmer + Chladni dust + throw burst, hexafoil glow, chandelier shiver and shatter, scalding steam, thin-floor crack glow, shield film, reflect flash, Aegis stave and dome, Mark outline, Rune trap hexafoil, Rune link thread, Starfall glints, Rune circle, Prism beam + guide thread + split, dome stave, Sundering Flare, copy shimmer, lens flare, Echo Warden ripple ring, Echobat shriek ripple, Resonance peal wave, body-change rune lines and finger traces |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| `WRCombatTypes.h` `FWRHit` | Add `MarkBuildup`, `bReflected`, `bShieldPiercing`, `NoiseRadius` (so every hit can emit noise) |
| `WRCombatLibrary.cpp` `ApplyHit` | Insert the **shield step** after guard (combat's resolver order): `UWRShieldComponent::Absorb(Hit)` returns the leftover; Absorb refunds Mana; Mana burn ×2 on shield damage; Mark's +15% in the damage modifiers; Mark-chance spell bonus; emit `UWRNoiseSubsystem::Emit(Hit.Location, Hit.NoiseRadius)`; part damage for plate rows and legs (combat's parts) with strike/slash/pierce multipliers; PvP drain of 8 resource from Mana burn |
| New `WRShieldComponent.h/.cpp` | `UWRShieldComponent` on characters: `Health`, `Expiry`, `bReflect`, `Owner`; replicated for the film FX; `Absorb`, `Apply(Amount, Seconds, Source)` (max-wins) |
| `WRProjectile.cpp/.h` | `bReflectable` flag from data rows; `Reflect(AActor* NewOwner)`: reverse velocity along the incoming line, damage ×0.8, owner swap, never re-target; overlap with a reflecting part (Prismback/Tyrant plate row facing, Twins' ward guard, a Reflect shield, a perfect-guarded prism shield) calls it |
| `WRStatusComponent.cpp/.h` | **Mark:** buildup meter, proc → `Status.Marked` tag 10 s, damage-taken modifier; marker's party list for the outline (replicated owner set) |
| `WRAttributeSet` / `WRPathComponent` | **Mana** attribute: capacity from tuning + nodes; regen tick 6/s always, ×1.5 with Deep well on main; no regen inside the Silent Vault (asks `UWRUndergroundSubsystem` volume flag) |
| `WRAbilities.cpp/.h` | New classes: `UWRGA_ArcaneMissile` (Bolt, reflectable, mark), `UWRGA_RunicStrike` (Melee Wildbrand), `UWRGA_Barrier` (`UWRGA_SelfBuff`/ally mode with Ward allies; applies shield), `UWRGA_ArcaneBlast` (Nova at aim, chargeable), `UWRGA_RuneTrap` (`UWRGA_Summon` of `AWRRune`; claim check; max 3; Rune link threads), `UWRGA_Inscribe` (weapon buff), `UWRGA_Aegis` (spawns `AWRAegisDome`: projectile stop/reflect volume, catapult-stone break), `UWRGA_Starfall` (Strike), `UWRGA_RuneCircle` (Pool), `UWRGA_BallLightning` (combo active). `ClassForNode`: new `arcane` block mapping `arcane_missile, runic_strike, barrier, arcane_blast, rune_trap, inscribe, aegis, starfall, rune_circle`. Passives (`reflect, ward_allies, echo, mana_burn, rune_link`) through `UWRPathComponent::HasNode` like Fire's; stat nodes through `nodeEffects`. Spell cast in the Silent Vault fails with the dust puff (one check in `UWRGameplayAbility::CanActivate` for spell-tagged abilities) |
| New `WRNoiseSubsystem.h/.cpp` | `UWRNoiseSubsystem` (world): `Emit(Location, Radius, Instigator)`; listeners register (Echobat, Echo Warden, Spindleglass vibration filter, chandeliers, crystals' light, the Vault door); movement emits from the CMC state (walk/run/sprint/roll/land; crouch 0) server-side. If Gloomhowl's built hearing exists in `WRCreature.cpp`, it moves onto this subsystem (one noise model) |
| New `WRCrystalLight.h/.cpp` | `UWRCrystalLightSubsystem`: crystal clusters are HISM instances with a per-instance custom data `Ring` (0..1); noise events set ring by distance; a fade tick; keystone chimes (`AWRKeystoneChime`) hold a hall at `keystoneLit` for 20 game minutes and reveal its map cells (`WRUIMap`). Runs on clients from replicated noise pings (cosmetic) so the server only sends events |
| `WRWorldDressing.cpp` / `WRHazardSubsystem` (fungal's) | `AWRWardField` (plane volume, attunement check: Arcane path owned, ward charm, Wardcloth 4, chalk; tell FX at 300 cm; throw via `LaunchCharacter` + `ApplyHit` 25 arcane unblockable; 1.5 s re-throw; keystone link; pattern A/B), `AWRWardKeystone` (lead plate; any projectile/spell hit drops its field 90 s), `AWRShardChandelier` (noise listener ≥ 1,500 → 1.0 s tinkle → falling trace + `ApplyHit` to players and creatures, Tyrant bonus), `AWRScaldingPool` (wading volume: burn buildup + dps), `AWRThinFloor` (breaks on heavy, roll, land or heavy creature; reveals a hole into the lower volume; rebuilds in 2 game days), `AWRSilentVaultDoor` (silence timer 10 s, six-petal progress, Resonance lockout), Silent Vault volume (audio silence, spell block, no regen) |
| `WRCreature.cpp` | Attack-row kinds from 3.1. **Prismback:** plate rows as parts facing-tested for reflection, Plate turn reaction on noise from a bow draw or cast, Glare (finds the brightest light within 15 m: torches, lit crystals via `UWRCrystalLightSubsystem`, Lumenhorn light), whiteout applied client-side via a replicated flash event. **Spindleglass:** 8 leg parts with HP, collapse states, Lift grab with bone-socket attach, Shed leg actor (twitching, noise 12 m, lock-on target). **Echobat:** perception only from `UWRNoiseSubsystem`; click-train audio rate from its state; Shriek ripple volume moving 900 cm/s. **Geodemite:** latch (limb sockets, max 4), burst on death. **Lumenhorn:** Bellow emits 40 m noise (an order on the wheel), rider light component. **Druseback:** roll state. **Gemsniffer:** sense (gems, nodes, caches) with snout-point. **Alicanto:** night route along ore veins, notice cone, dim-and-run-to-edge behaviour (finds the nearest drop edge from the walk map), lead-to-vein when tamed, unseen offer-food check |
| `WRBoss.cpp/.h` | `BuildAttacks` rows for `lyrathis`, `mirrored_twins`, `echo_warden`, `geode_tyrant`. **Lyrathis:** `AWRPrismTurntable` ×8 (lever hit → replicated target yaw +45°, eased rotation 0.8 s on the server, clients interpolate; ratchet one-way; pawl/roller sounds by state); `AWRPrismBeam` (trace from brow gem to the chosen prism, then fan of 3 traces from the prism along its facing; re-trace each tick while sweeping; damage on contact through `ApplyHit`; hitting the dome = dome damage; hitting a shaft = surface spire FX via a replicated spire state); dome actor (HP, projectile stop, melee ×0.25 multiplier on hits that pass its radius, stave FX, Mana burn ×2, void ×3); copies (light actors placed on each prism's line beyond the ring if the line reaches open floor within 15 m; they replay her current attack 0.5 s later; dissolve when the line breaks); Glint lance projectile that becomes a temporary prism; Sundering Flare line-of-sight check from the heart; P3 rotating beam behind the artifact flag (BUILT `AnyArtifactModifiers`); furthest-tame aim on the first fan. **Twins:** one real knight pawn + one double (a non-colliding copy driven by mirrored attack positions across the target), the lens gimbal actor (quarter turn every 12 s, flare window, 3 hits break, magazine chain lowering a spare over 20 s, 3 spares), reflection test = only the real pawn takes damage; floor water material shows only the real one's reflection (planar reflection of a render layer). **Echo Warden:** loudest-noise targeting, chime-stone actors (hammer state, 3 hits break), Listening stance, Resonance blast ring. **Geode Tyrant:** 6 plate parts, roaming route (fire-frost's route component), chandelier interaction, beam shatter stun |
| `WRBiomeEvents.cpp` (fire-frost's subsystem) | The Resonance timeline: warning (40 Hz swell, spire FX, the Crown pillar), peals every 240 s spreading at 300 m/s (each pass: crystals full, Echobats deafened 30 s, chandeliers in range fall, nests crack and spawn swarms), ward pattern swap A↔B, node regrowth, singing geodes open, Vault door lockout |
| `WRShrine.cpp`, `WRPathMaster.cpp` (fire-frost's) | 5 Crystal shrines; Saga Venn (MetaHuman; carving, listening and fork-striking idles); quests |
| `WRBodyChange.cpp` (hero's) | Arcane parameters: hexafoil pattern texture, stage reveal (wrist rosettes → forearms → nape), resting glow at 0.6+, cast pulse, fingertip trace Niagara at 1.0, iris facet fleck, cast chime |
| `WRSocial.cpp/.h` (`FWRClans`) | **Ranks:** `FClan` gains `TMap<FString, EWRClanRank> Ranks`, `FWRClanRights Rights[4]`, `JoinedAt` per member, `Log` ring buffer; `SetRank`, `SetRight`, `CanDo(Player, Right)`; `clans.json` gains the fields (old saves: leader = Leader, everyone else = Member). **Per-object min rank:** a `MinRank` field on chests, doors, stations, tames and the Hearth checked in `FWRSocialRules::CanShare`. **Alliances:** new `FWRAlliances` (same file, saved in `alliances.json`): `Propose/Accept/Leave`, size check against `clanSizeLimit × allianceSizeMult`, 24 h cooldown; `PlayersCanFight` returns false for allies; map and compass ally markers |
| `WRPlayerState` | `Infamy` (replicated to everyone as a tier only: 0, Branded, Hunted, Reviled), `LastHitBy` map for the provoked test, `WakeExcludesCamps` when Hunted |
| New `WRPvP.h/.cpp` | `UWRPvPSubsystem`: infamy gains on player death (unprovoked test from the combat log: who hit whom in the last 60 s), decay tick per hour of play, world bounties, Hunted/Reviled map broadcasts (smudge centre jittered within 150 m, every 60/30 s), **Outnumbered** check every 1 s per player in PvP combat (hostiles who hit your side in 15 s vs your side within 40 m), raid bell (piece damage from players → clan notify, offline queue) |
| New `WRBounty.h/.cpp` | `AWRBountyBoard` (placed by world data in camps and villages): escrow storage, post (eligibility: infamy ≥ 30 or killed you in 24 h), claim on kill (killing blow or ≥ 30% in 30 s; not clan or allies; 24 h reclaim lock), expiry 7 days, payouts on next visit; saved per world |
| `WRSafeZone` | Hunted players can't enter (pushed to the edge with a state word); camps removed from their death-map choices (`WRFlowDeath`) |
| `WRBuilding.cpp` | Raid bell hook on piece damage by player sources; code breaker channel on door keypads (`WRUIDoor`) |
| `WRSiege.h/.cpp` (survival-crafting's planned file) | `AWRBatteringRam` (chain pendulum swing driven by hold/release, wheel roll, roof HP, hauler harness), `AWRTrebuchet` (arm on axle with counterweight physics-free animation curve, winch ratchet, sling release by range pin, base turntable, everyone-visible arc line), `AWRBallista` (windlass draw, aim line, straight bolt), `AWRSiegeLadder`; all damage through `ApplyHit`; placeable inside enemy claims |
| UI (`WRUIClan.cpp`, new `WRUIBounty.cpp`, `WRUIHud`, `WRUIMap`) | 2.21.9 screens, registered with `WR_REGISTER_SCREEN` / `WR_REGISTER_JOURNAL_TAB`; every control by mouse and pad (`FWRKeyBindings`), F1 pages for each, B/Esc backs out, no screen without an exit; Mana bar; Mark meter |
| `WRAudioDirector.cpp` | Crystal pools by sub-zone volume; the tintinnabuli stems; boss sections; the Silent Vault silence; Resonance layer; Severance duet half |
| `WRSky.cpp` | Prism Crown spire beams (state from the boss and event); surface hole glow at night |

### 3.3 Assets and the art route

**Route for creatures and bosses (KEPT):** concept sheets (front, side, top, at scale beside the hero) picked by Aaron → Hunyuan3D 2.1 base mesh → Blender finish (retopology, UVs, hand-fixed anatomy, plates and legs as separate parts for breaking) → `Tools/art` rig scripts → **real animation clips per species** (feet planted, gait matched to speed, no gliding; Spindleglass's eight legs keyed with IK on the ground, Lyrathis's steps grinding the floor). Glass and crystal materials: a shared master material with refraction, an inner "phantom" layer, emissive driven by `Ring` (sound makes light). No engine primitives anywhere.

| Asset | Count | Route / notes |
|---|---|---|
| Creatures | 9 | As above. Prismback and Tyrant share a skeleton (Tyrant scaled, plates swapped); Spindleglass needs the clear-body shader with visible organs and a blood-cloud parameter |
| Bosses | Lyrathis, Twins (one knight mesh + double shader), Echo Warden, (Tyrant above) | Lyrathis is the hero asset: sculpted in Blender over the base, crystal shell + basalt core + selenite hair cards; her eight prisms and turntables as engineered kits (rollers, race, ring gear, pinion, lever, pawl, counterweight modelled, not faked) |
| Cave kit | Reuse fungal's Nanite kit with new trims: basalt columns, geode rinds (agate bands), amethyst linings (HISM point clusters), selenite beams (6 sizes), gypsum chandeliers (3), lava-tube sections (round, with shelf benches), hewn ward chambers | Fab cave and crystal packs (licence rule: use unless explicitly barred), finished in Blender |
| Wardcarver props | Hexafoil doorways, lead keystones, chime stones (waymark and hall keystone sizes), bronze tuning-fork racks, lens gimbal and magazine, watch-cairns with sighting rings, chime garden, bathhouse, Singing School benches and chime wall | Blender, from photos of real hexafoils and lithophones |
| Landmarks | Prism Crown spires, Hollowed Giantess (30 m), Split Geode (cliff kit), Hall of Beams, the Organ | Blender + kit |
| Path master | Saga Venn | MetaHuman (hero lane's pipeline), her outfit tailored per 2.11 |
| Wardcloth set (KEPT) | 4 pieces × 2 bodies | Hero lane's tier 5 route |
| Weapons and items | Prismglass spear, Glassleg bow, two shields, lumen lantern, runes as socket glyph decals, bounty board, siege ladder, code breaker | Blender |
| Siege engines | Ram, trebuchet, ballista | Fab medieval siege pack (KEPT) refitted so every mechanism works (chains on the ram, axle/counterweight/sling/winch/pawl on the trebuchet, windlass on the ballista) |
| VFX | All of 3.1 `fx.json` | Niagara, preloaded via `preload.json` |

### 3.4 Sound and music

| Item | Route |
|---|---|
| Voices (2.15) | Real recordings (licensed libraries, use unless barred), fused and pitched in the DAW; 4 layers per attack; each species its own set, never a shared class |
| Crystal world sounds | Record real crystal: struck amethyst geodes, quartz points, singing bowls, wine-glass rims, a tuning fork set; build the ring layer and the per-hall keystone arpeggios from them |
| Ward hum | A 49 Hz tone with a slowly beating 196 Hz partial; the 0.9 s rising tell |
| Music (2.16) | Composed and recorded (armonica, celesta, bells, bowls, harp, strings, choir); stems for combat layering; mixed to the KEPT loudness targets |
| Silence | The Vault: music and world off; heartbeat only under 30% HP (KEPT planned silence) |

### 3.5 Map dressing

- **Surface (C1):** basalt column kit as the plateau's top surface; 14 collapsed geode domes (4 are descents) with violet emissive below; chime-stone arches at every dome lip; the shrine road (Cliff Bell → Severance) with chime stones every 150 m (KEPT waymark); sea cliffs with the Split Geode; three camp mouths with newcomer wrecks; salt-grass, sea campion and heather scatter; Druseback lichen at dome rims.
- **Caves:** assembled by hand from the kit in WP cells at the 2.2 coordinates; every hall gets its keystone chime at the far end; crystal clusters as HISM with `Ring` custom data; chandeliers only in galleries off the main route (so the micro-hazard is a choice); scalding pools only beside beam routes; thin floors only over a real lower geode (the fall lands somewhere designed).
- **Every placed thing answers why / connects / meets (Aaron 11-14):** keystones sit in sight of their field; chimes sit where their sound reaches the whole hall; ladders hang from bronze pins drilled into rock; turntable rollers sit in a carved race; the lens magazine's chain runs over a pulley to a counterweight in a wall slot; nothing floats or clips.
- **Forest border:** crystal-veined boulders under moss (KEPT), amethyst points in a few roots lit when the Bell-Tree rings.
- **Severance east end:** grey-bleached crystal veins, the Giantess in her cage, the Severance Gate with its live ward.

### 3.6 Build order inside this scope (each step one cook into Aaron's build; Aaron tests)

| Step | Build | Proof it shows (filled at build) |
|---|---|---|
| **1 (proof of one)** | **One Geode Hall slice:** the cave kit trims, one hall (H1 the Singing School) with "sound makes light" (`UWRNoiseSubsystem`, `UWRCrystalLightSubsystem`, keystone chime), one resonant crystal node with the ring-timing gather, one ward field with its keystone and Chladni tell, one thin floor | |
| 2 | Mana, Arcane missile, Runic strike, Mark status, shields + Barrier, the `arcane.json` tree in the Paths screen | |
| 3 | The rest of the Arcane tree (Reflect, Ward allies, Absorb, Aegis, Arcane blast, Echo, Quick cast, Mana burn, Starfall, Rune trap, Inscribe, Rune link, Lasting runes, Rune circle), projectile `reflectable` + `Reflect` | |
| 4 | The full cave layout C2 to C5 and the surface C1: descents, POIs, landmarks, chime road, hazards (all 34 ward fields, chandeliers, pools, floors), ore galleries, shrines | |
| 5 | Creatures one at a time, proof first with Prismback (plates, reflect, glare), then Echobat (hearing), Spindleglass, Geodemite, Lumenhorn, Druseback, Gemsniffer, Alicanto, Chimecrab; spawn tables day/night | |
| 6 | Minis: the Echo Warden (noise model reuse), the Mirrored Twins (lens gimbal + reflection), the Geode Tyrant (route + chandeliers) | |
| 7 | Lyrathis: arena with 8 working turntables, beam tracer, dome, transition, copies, phase 2, hidden phase 3; altar; summoning; drops | |
| 8 | Silent Vault (door, silence rule, six rooms), the Resonance event, Saga Venn and quests, body change, Wardcloth, gear, found runes and rune sockets | |
| 9 | Music and voices pass for the biome (all cues, beds, the Vault silence) | |
| 10 | PvP pass part 1: clan ranks, per-object min rank, clan log, alliances, anti-zerg limits and Outnumbered, screens | |
| 11 | PvP pass part 2: infamy, marks, bounty boards, Hunted camp lockout | |
| 12 | PvP pass part 3: raid bell, code breaker, siege ladder, siege engines with working mechanisms, path-raiding Arcane line, contested-boss rules (two run-back stones per major) | |
| 13 | Combos: the 10 Arcane pairs | |
| 14 (R10 switch) | Severance invasions (Crystal side ready; turns on when the Void opens) | |

### 3.7 Every build step, listed (for the build lane)

1. Kit trims and HISM crystal clusters with `Ring` custom data. 2. `UWRNoiseSubsystem` (+ CMC emits). 3. `UWRCrystalLightSubsystem` + `AWRKeystoneChime` + map reveal. 4. Resonant node ring-timing gather. 5. `AWRWardField` + `AWRWardKeystone` + attunement checks + attuning chalk. 6. `AWRThinFloor`. 7. Mana attribute + regen + HUD bar. 8. Mark status + outline. 9. `UWRShieldComponent` + `ApplyHit` shield step. 10. Arcane abilities ×9 + `ClassForNode` block + passives + nodeEffects + `arcane.json`. 11. Projectile `bReflectable` + `Reflect()`. 12. `AWRAegisDome`, `AWRRune` + links. 13. Cave layout C2 to C5 + Vault volumes in WP cells. 14. Surface C1 dressing + descents + landmarks + chime road. 15. Chandeliers, scalding pools. 16. `world_pois.json` rows + kit spawns. 17. Ore galleries + nodes. 18. 5 shrines. 19. 9 creature rows + meshes + clips + attack rows + behaviours. 20. Spawn regions day/night + raiders. 21. Echo Warden + chimes. 22. Twins + lens gimbal + magazine + reflection floor. 23. Geode Tyrant + route + chandelier interaction. 24. Lyrathis: turntables, beam tracer, dome, moves, copies, lance prisms, P3, altar, summoning, drops, spire FX. 25. Silent Vault door + silence rule + rooms + loot. 26. Resonance event. 27. Saga Venn + quests. 28. Body change params. 29. Wardcloth + items + recipes + found runes + rune sockets. 30. Voices. 31. Music + beds + silence. 32. Clan ranks + rights + min rank + log + save migration. 33. Alliances + settings. 34. Outnumbered. 35. Infamy + marks + map broadcasts + camp lockout. 36. Bounty boards. 37. Raid bell + code breaker + siege ladder. 38. Siege engines (ram, trebuchet, ballista) with mechanisms. 39. Arcane raid line + two run-back stones per major. 40. 10 combos. 41. Invasion Crystal side (dormant until R10). 42. Screens for 32 to 37 with F1 pages, mouse and pad. 43. Gates: compile, cook, package; Aaron's build.json updated only when it's really in.

### 3.8 Spot-check against THE BAR (by reading this plan; the build round re-checks in code)

| Bar item | Where this plan meets it |
|---|---|
| Full hit stack, hitstop 0.06 to 0.30, coil 0.14 / 0.20, one verb per button, 0.33 s buffer | KEPT from combat for every Arcane spell and Wildbrand (2.17 rules); crystal hits add the glass-ring surface FX |
| Real weapon / claw / projectile; damage only on visible contact; small cue; tells at 15 m | Every row in 2.12.2, 2.13, 2.14.2 names its visible part and a pose + sound tell; beams and rings are visible things; ground marks only for real shadows and Chladni dust |
| Projectiles ≥ 0.9 s, never home | Shard rain 1.0 s shadows, Glint lance ≥ 1.2 s + flight, shard shrug glints the whole flight, Echobat shriek ≥ 1.4 s; reflection flies straight back (2.12 rule) |
| No attacker limit | 2.12 rules; swarms latch freely |
| Players die; no floors; no helpers after a fail | Ward fields and every hazard kill; nothing gets easier after a death; Outnumbered is a standing PvP rule, not a reaction to a death |
| Terrifying, readable, believable; no slop, no primitives | 2.12.1 silhouettes from real anatomy; 3.3 route; Lyrathis and Spindleglass as hero assets |
| Real clips, no gliding | 3.3 |
| World: POI every 150 to 250 m, 3 to 5 sub-zones, story by environment only | 2.4 (151 m / 183 m), 2.2 (5), 2.20 |
| Music real, always audible, ducks ≤ 4 dB, only planned silences | 2.16 (the Vault is the one silence; the Echo Warden cue thins, never drops) |
| UI mouse and pad, no dead states, F1 | 2.21.9, 3.2 UI row |
| People attractive | Saga Venn (2.11), Wardcloth cut (2.19.1) |
| Machines engineered, smooth | Turntables (2.14.1), lens gimbal and magazine (2.13.1), siege engines (2.21.5) |

---

## 4. Out-of-scope findings (each sent to `wildreach-night/LATER-COOKS.md` as one line)

1. A shared **noise model** (`UWRNoiseSubsystem`, radii in tuning `noise`) now drives Echobat, the Echo Warden, Spindleglass, chandeliers, crystal light and the Vault door; Gloomhowl (built) and the fungal Sporestalker hear too, so the creature/combat owner should move all hearing onto it.
2. **Projectile `reflectable` flag**: Arcane Reflect, Aegis, Prismback and prism shields reflect "reflectable" projectiles; every creature and boss projectile row across all biomes needs the flag set (combat lane).
3. **Shield HP layer** (`UWRShieldComponent`, after guard in `ApplyHit`) is shared: Tide's Tidal shield, Nature's World tree, Moon tide and the Tyrant talisman use it; combat lane should own the resolver order.
4. Crystal surface footprint assumed x +1,800 to +2,450 with the Severance's east mouth at x +1,800; world-map should confirm the Spine line through B6.
5. Bounties pay in items plus path XP because Raven's design has no currency; whoever cooks villages and merchants should confirm no currency arrives that bounties should use.
6. **Two run-back stones per major arena** (contested-boss rule) applies to all 12 majors; the boss frame and each biome cook should add the second stone.
7. Hero and survival-crafting describe Wardcloth slightly differently; this cook merged them (2.19.1); the hero lane should adopt the merged look.
