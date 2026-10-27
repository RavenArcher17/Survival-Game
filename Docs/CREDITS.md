# Credits

Third-party files in the repository, with where they came from and their licence. Each area of the game keeps its
own section.

## Audio (the sound pass)

Every sound and music file in `Data/Audio` comes from a free library whose licence allows commercial use and
redistribution, checked on each source's own page when it was downloaded (2026-10-26). Nearly all of it is CC0
(public domain: no attribution needed, credited here anyway). Two sources are **CC-BY 3.0** and must stay credited
wherever the game is distributed: *Lava loop* (Blender Foundation) and *Crossbow sounds* (HolgiB). Where a page offered
several licences (CC0 alongside CC-BY / GPL / OGA-BY) the CC0 option is the one used.
The files were cut, mixed to mono for 3D effects, loudness-matched and re-encoded as Ogg Vorbis by
`Tools/audio/build_audio.py`; `Data/Audio/sounds.json` says where each one plays.

| Source | Author | Licence | Page | Used for (Data/Audio/...) |
| --- | --- | --- | --- | --- |
| Fire Crackling | AntumDeluge | CC0 1.0 | https://opengameart.org/content/fire-crackling | amb/volcanic_crackle, world/hearth_feed |
| Crickets Ambient Noise - loopable | Wolfgang_ | CC0 1.0 | https://opengameart.org/content/crickets-ambient-noise-loopable | amb/night_insects |
| Loopable Dungeon Ambience | JaggedStone | CC0 1.0 | https://opengameart.org/content/loopable-dungeon-ambience | amb/cave |
| Nocturnal insect #3 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/nocturnal-insect-3-s0426.html | amb/night_insects |
| Lava loop | Blender Foundation (submitted by Lamoot) | CC-BY 3.0 | https://opengameart.org/content/lava-loop | amb/lava, spell/lavapool (x2) |
| AMB Morning Sounds (Perfect Loop) | Kresiek The Furry | CC0 1.0 | https://opengameart.org/content/amb-morning-sounds-perfect-loop | amb/day_birds |
| Tawny Owls | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/owls-s0429.html | amb/owl (x5) |
| Low Rumbling | Musheran | CC0 1.0 | https://opengameart.org/content/low-rumbling | amb/volcanic_rumble |
| Sea Waves | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/sea-waves-s0698.html | amb/coast_surf |
| Whistling of the Wind #1 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/whistling-of-the-wind-1-s0147.html | amb/high_wind |
| Strong Wind #1 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/strong-wind-1-s0146.html | amb/tundra_wind |
| Bat Screeches | AntumDeluge | CC0 1.0 | https://opengameart.org/content/bat-screeches | creature/flyer_call (x3) |
| Bear Growls | AntumDeluge | CC0 1.0 | https://opengameart.org/content/bear-growls | creature/beast_idle (x2) |
| 16 Monster Growls | StarNinjas | CC0 1.0 | https://opengameart.org/content/16-monster-growls | creature/canine_aggro (x5) |
| Insect or alien scream | qubodup | CC0 1.0 | https://opengameart.org/content/insect-or-alien-scream | creature/bug_death |
| Osare Minotaur Sounds | Brandon Morris (submitted by HaelDB) | CC0 1.0 | https://opengameart.org/content/osare-minotaur-sounds | creature/beast_attack, creature/beast_death |
| Monster Sound Pack, Volume 1 | Ogrebane | CC0 1.0 | https://opengameart.org/content/monster-sound-pack-volume-1 | creature/boss_hurt (x3), creature/herd_death, creature/herd_hurt (x2) |
| Grumpy Pig, with the Mouth #2 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/grumpy-pig-2-s1659.html | creature/pig (x5) |
| 80 CC0 creature SFX | rubberduck | CC0 1.0 | https://opengameart.org/content/80-cc0-creature-sfx | creature/beast_aggro (x2), creature/beast_attack (x4), creature/beast_hurt (x3), creature/bug_death, creature/bug_idle (x4), creature/canine_hurt (x5), creature/canine_idle (x3), creature/critter (x10), creature/flyer_scream (x2), creature/herd_attack, creature/herd_hurt, creature/herd_idle (x2) |
| 80 CC0 creture SFX #2 | rubberduck | CC0 1.0 | https://opengameart.org/content/80-cc0-creture-sfx-2 | creature/beast_death, creature/bug_aggro (x4), creature/bug_attack (x5), creature/bug_hurt (x3), creature/canine_attack (x4), creature/canine_death (x3), creature/flyer_attack, creature/golem_roar (x3), creature/herd_attack (x2), creature/herd_death, creature/herd_idle (x3), creature/human (x8) |
| CC0 Deep Monster Roar | trazzz123 | CC0 1.0 | https://opengameart.org/content/cc0-deep-monster-roar | creature/boss_roar |
| Big scary troll sounds | Darsycho | CC0 1.0 | https://opengameart.org/content/big-scary-troll-sounds | creature/beast_idle, creature/boss_attack (x4), creature/boss_idle (x2), creature/boss_roar, creature/herd_aggro (x2) |
| Wolf Monster Sound | CaveboyTup | CC0 1.0 | https://opengameart.org/content/wolf-monster-sound | creature/canine_idle |
| Hammer on Anvil | themightyglider | CC0 1.0 | https://opengameart.org/content/hammer-on-anvil | world/craft_done |
| 75 CC0 breaking / falling / hit sfx | rubberduck | CC0 1.0 | https://opengameart.org/content/75-cc0-breaking-falling-hit-sfx | world/build_demolish (x2) |
| Cave In | StarNinjas | CC0 1.0 | https://opengameart.org/content/cave-in | creature/golem_death |
| 7 Eating Crunches | StarNinjas | CC0 1.0 | https://opengameart.org/content/7-eating-crunches | item/eat (x4) |
| Liquid Bottle Drink Set | qubodup | CC0 1.0 | https://opengameart.org/content/liquid-bottle-drink-set | item/drink (x4), item/potion (x2) |
| Catching fire | themightyglider | CC0 1.0 | https://opengameart.org/content/catching-fire | world/hearth_feed |
| Water Splash and sand footsteps | Peludo | CC0 1.0 | https://opengameart.org/content/water-splash-and-sand-footsteps | foot/sand (x6) |
| 100 CC0 SFX #2 | rubberduck | CC0 1.0 | https://opengameart.org/content/100-cc0-sfx-2 | foot/water (x3), ranged/blowgun (x2) |
| Blacksmith's Hammer | VishwaJai | CC0 1.0 | https://opengameart.org/content/blacksmiths-hammer | world/craft_done |
| 40 CC0 water / splash / slime SFX | rubberduck | CC0 1.0 | https://opengameart.org/content/40-cc0-water-splash-slime-sfx | foot/water (x2) |
| 100 CC0 metal and wood SFX | rubberduck | CC0 1.0 | https://opengameart.org/content/100-cc0-metal-and-wood-sfx | world/build_demolish (x2) |
| Impact Sounds | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/impact-sounds | combat/hit_armor (x5), combat/hit_block (x4), combat/hit_flesh (x7), combat/land (x4), foot/grass (x5), foot/snow (x5), foot/stone (x5), foot/wood (x5), world/build_place (x5), world/gather_stone (x5), world/gather_wood (x5) |
| Interface Sounds | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/interface-sounds | creature/wisp_chime (x5), ui/click, ui/drag (x2), ui/drop (x3), ui/error (x2), ui/hover |
| Music Jingles | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/music-jingles | moment/discover, moment/levelup, moment/recipe_unlock, moment/tame_success |
| RPG Audio | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/rpg-audio | combat/dodge (x4), combat/jump (x2), foot/gravel (x6), item/equip (x4), item/pickup (x4), ranged/bow_draw (x3), ui/close (x2), ui/open (x2), world/door_close (x4), world/door_open (x2), world/gather_wood |
| UI Audio | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/ui-audio | ui/click (x2), ui/hover |
| Battle Theme A | cynicmusic | CC0 1.0 | https://opengameart.org/content/battle-theme-a | music/combat |
| Boss Battle Music (Epic Boss Battle) | Juhani Junkala (SubspaceAudio) | CC0 1.0 | https://opengameart.org/content/boss-battle-music | music/boss |
| Dramatic Boss Encounter | cynicmusic | CC0 1.0 | https://opengameart.org/content/dramatic-boss-encounter | moment/boss_intro |
| Cathedral in the forest (ambient loop) | congusbongus | CC0 1.0 | https://opengameart.org/content/cathedral-in-the-forest-ambient-loop | music/night_cathedral |
| Medieval: Exploration | RandomMind | CC0 1.0 | https://opengameart.org/content/medieval-exploration | music/explore_lands |
| Fantasy Orchestral Theme | Joth | CC0 1.0 | https://opengameart.org/content/fantasy-orchestral-theme | music/menu |
| Fantasy: Rising Moon | RandomMind | CC0 1.0 | https://opengameart.org/content/fantasy-rising-moon | music/night_moon |
| Mysterious Winds | A_Thor | CC0 1.0 | https://opengameart.org/content/mysterious-winds | music/explore_winds |
| Long Winter | Indieteur | CC0 1.0 | https://opengameart.org/content/long-winter | music/explore_winter |
| 25 CC0 bang / firework SFX | rubberduck | CC0 1.0 | https://opengameart.org/content/25-cc0-bang-firework-sfx | spell/fire_impact (x3) |
| Ice spells | bart | CC0 1.0 | https://opengameart.org/content/ice-spells | spell/cast_frost (x2) |
| Magic Spell SFX | JaggedStone | CC0 1.0 | https://opengameart.org/content/magic-spell-sfx | creature/wisp_magic, moment/respawn |
| 80 CC0 RPG SFX | rubberduck | CC0 1.0 | https://opengameart.org/content/80-cc0-rpg-sfx | creature/beast_aggro (x3), creature/beast_death, creature/beast_hurt (x2), creature/golem_stones (x4), creature/wisp_spell (x2), spell/cast_fire (x3) |
| Ice breaking/shattering | IgnasD | CC0 1.0 | https://opengameart.org/content/ice-breakingshattering | spell/frost_impact (x4) |
| Female RPG Voice Starter Pack | cicifyre | CC0 1.0 | https://opengameart.org/content/female-rpg-voice-starter-pack | voice/hurt_female (x3) |
| Female Hurt Grunts & Groans | Nocturnal_Vanguard | CC0 1.0 | https://opengameart.org/content/female-hurt-grunts-groans | voice/death_female (x2), voice/hurt_female (x4) |
| 15 vocal male strain/hurt/pain/jump sounds | qubodup | CC0 1.0 | https://opengameart.org/content/15-vocal-male-strainhurtpainjump-sounds | voice/hurt (x10) |
| grunts of male death and pain | thebardofblasphemy | CC0 1.0 | https://opengameart.org/content/grunts-male-death-and-pain | voice/death (x4), voice/hurt (x3) |
| Archery | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/archery-s0549.html | ranged/bow_release |
| An arrow on a target | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/an-arrow-on-a-target-s0548.html | ranged/arrow_hit (x3) |
| Battle Sound Effects | artisticdude (submitted by Ogrebane) | CC0 1.0 | https://opengameart.org/content/battle-sound-effects | combat/swing_heavy (x3), ranged/bow_release, ranged/throw |
| 20 Sword Sound Effects (Attacks and Clashes) | StarNinjas | CC0 1.0 | https://opengameart.org/content/20-sword-sound-effects-attacks-and-clashes | combat/parry (x4) |
| Crossbow sounds | HolgiB | CC-BY 3.0 | https://opengameart.org/content/crossbow-sounds | ranged/crossbow |
| Footsteps Leather, Cloth, Armor | HaelDB | CC0 1.0 | https://opengameart.org/content/footsteps-leather-cloth-armor | combat/jump |
| Swishes Sound Pack | artisticdude | CC0 1.0 | https://opengameart.org/content/swishes-sound-pack | combat/swing_heavy, combat/swing_light (x6), ranged/throw |

## Animation

| What | Files in the repo | Source | Author | Licence |
| --- | --- | --- | --- | --- |
| Survivor animations: the Sword And Shield pack (idles, walks, runs, strafes, walking / running back, turns, slashes, attacks, blocks, casts, hits, deaths) and singles (dodge roll, overhead chop, gathering, pick up, swimming, treading water) | `Content/Characters/Survivor*/survivor_*Armature_*.uasset` | https://www.mixamo.com | Adobe Mixamo (motion capture) | Mixamo terms of use: royalty-free in personal and commercial projects, including games; the raw files may not be redistributed on their own |
| Survivor ranged animations from the Longbow pack: bow idle / walk / run / equip / draw / aim / recoil / aim-walk, and (added in the 0.4.0 polish pass) bow walk and run back / left / right, aim-walk back / left / right and turn 90 left / right; rifle aim / fire / reload; throw | `Content/Characters/Survivor*/survivor_*Armature_Bow*.uasset`, `..._Rifle*`, `..._Throw` | https://www.mixamo.com (downloaded on Mixamo's stock Y Bot, retargeted to each survivor with `Tools/art/survivor_build.py`) | Adobe Mixamo | Mixamo terms of use, as above |
| Humanoid boss animations (Creature Pack: idles, walking, run, swiping, punch, jump attack, roaring, dying) | `Content/Creatures/Vulkarn`, `Grimjaw`, `Obsidian_warden` | https://www.mixamo.com | Adobe Mixamo | Mixamo terms of use, as above |

The creature animations made by `Tools/art/rig_quadruped.py` and `rig_special.py` are procedural (written for this
game); the pack creatures (Gloomhowl, Scorchwing, Skaldrith) use the animations that came with the QuadrapedCreatures
Fab pack, which is not in the repository.
