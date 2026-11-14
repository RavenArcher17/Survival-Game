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
| Synthesised here (`Tools/audio/synth_raid.py`: bell partials, noise, sines) | Freedom35 Studios | CC0 1.0 | (no page: generated) | raid/bell_1-3, raid/blast_1-2, raid/fuse, raid/codebreaker, raid/pot_break |
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
| Impact Sounds | Kenney (kenney.nl) | CC0 1.0 | https://kenney.nl/assets/impact-sounds | combat/hit_armor (x5), combat/hit_block (x4), combat/hit_flesh (x7), combat/land (x4), and the combat hit layers derived from them (combat/hit_transient_*, hit_body_*, stance_break_*, guard_break_*, crit_stab_*: Tools/audio/make_combat_layers.py), foot/grass (x5), foot/snow (x5), foot/stone (x5), foot/wood (x5), world/build_place (x5), world/gather_stone (x5), world/gather_wood (x5) |
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
| Mill wheel, bladed | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/mill-wheel-bladed-s2768.html | machine/waterwheel |
| Inside a traditional Dutch windmill | pcwvdmark (freesound) | CC0 1.0 | https://freesound.org/people/pcwvdmark/sounds/345846/ | machine/windmill |
| Wind and tree squeaks | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/wind-and-tree-squeaks-s1051.html | machine/windmill |
| Steam engine foley #2 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/steam-engine-foley-2-s3311.html | machine/turbine, machine/smelter |
| Hiss of steam train #1 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/hiss-of-steam-train-1-s0227.html | machine/turbine |
| Manual chain hoist, downhill | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/manual-chain-hoist-downhill-s1134.html | machine/conveyor |
| Wooden gear rattling | hisoul (freesound) | CC0 1.0 | https://freesound.org/people/hisoul/sounds/461166/ | machine/conveyor, machine/coil |
| Trip hammer | leinadsorihcak (freesound) | CC0 1.0 | https://freesound.org/people/leinadsorihcak/sounds/449241/ | machine/drill (x4) |
| Millstone grinding grain | PLukx (freesound) | CC0 1.0 | https://freesound.org/people/PLukx/sounds/430589/ | machine/grinder |
| Sawmill #3 | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/sawmill-3-s1538.html | machine/sawmill |
| Navette de metier a tisser (loom shuttle) | Naima (freesound) | CC0 1.0 | https://freesound.org/people/Na%C3%AFma/sounds/510266/ | machine/loom |
| Large ratchet | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/large-ratchet-s0795.html | machine/sort (x3) |
| Manual water pump and metal bucket | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/manual-water-pump-and-metal-bucket-s0542.html | machine/pump |
| Steam locomotive starting | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/locomotive-a-vapeur-starting-s3021.html | machine/start |
| Steam locomotive stop | Joseph Sardin (BigSoundBank.com) | CC0 1.0 | https://bigsoundbank.com/steam-locomotive-stop-s3015.html | machine/stop |

## Audio: Storm peaks (2026-11-14)

The Storm peaks music, creature voices, storm hazards and ambient beds (`Tools/audio/build_storm_audio.py`, sources in
`Tools/audio/sources_storm.py`). All real recordings and real compositions, no AI generation and no synthesis. The music is
**Kevin MacLeod (incompetech.com), CC-BY 4.0**: every piece must stay credited wherever the game is distributed, as
"<Title>" Kevin MacLeod (incompetech.com), Licensed under Creative Commons: By Attribution 4.0,
http://creativecommons.org/licenses/by/4.0/. The recordings are from Freesound: CC0 except the six rows marked
CC-BY 4.0 (MichiJung's red kite calls, J.Zazvurek's ferret, klankbeeld's goat sneeze, markb's aeolian harp), which must
stay credited too. Calls were cut, layered (real electrical / material recordings under the animal calls), re-pitched,
loudness-matched and encoded as Ogg Vorbis by the build script.

| Source | Author | Licence | Page | Used for (Data/Audio/...) |
| --- | --- | --- | --- | --- |
| Tempting Secrets | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1300038 | music/storm_explore_a |
| Master Disorder | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1600034 | music/storm_explore_b |
| Long Road Ahead | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100588 | music/storm_explore_c |
| Lost Frontier | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1300039 | music/storm_night_a |
| Decline | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1300016 | music/storm_night_b |
| Hitman | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1300013 | music/storm_combat |
| Interloper | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100401 | music/storm_mini |
| Corruption | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1600004 | music/storm_boss_a |
| Truth of the Legend | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100592 | music/storm_boss_b |
| Night Vigil | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1900047 | music/storm_monastery |
| Thunder Dreams | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1200063 | music/storm_hollow_spire |
| Nu Flute | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100680 | music/storm_camp |
| Arcane | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100831 | music/storm_great_storm |
| Phantasm | Kevin MacLeod (incompetech.com) | CC-BY 4.0 | https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100113 | music/storm_pass_stormgut |
| Lion roaring.mp3 | Bidone (freesound) | CC0 1.0 | https://freesound.org/people/Bidone/sounds/69572/ | creature/roc_aggro (x3), creature/roc_attack (x3), creature/roc_death (x2), creature/roc_hurt (x2) |
| Slow Creaking Stereo.wav | harveyism (freesound) | CC0 1.0 | https://freesound.org/people/harveyism/sounds/130364/ | amb/storm_shrines, world/storm_plank_creak (x3) |
| wolfs.wav | ondrosik (freesound) | CC0 1.0 | https://freesound.org/people/ondrosik/sounds/143568/ | creature/stormwolf_aggro (x3) |
| Tesla ascending-m.wav | Rmutt (freesound) | CC0 1.0 | https://freesound.org/people/Rmutt/sounds/145506/ | world/storm_charge_whine (x2), world/storm_pretell (x2) |
| Mooring Rope.wav | Rmutt (freesound) | CC0 1.0 | https://freesound.org/people/Rmutt/sounds/145721/ | amb/storm_bridges |
| Ferret | J.Zazvurek (freesound) | CC-BY 4.0 | https://freesound.org/people/J.Zazvurek/sounds/155115/ | creature/mustelid_idle (x3) |
| Wind blowing into some cactus spine, on the top of the mountain, in the desert of Atacama (Chile). | felix.blume (freesound) | CC0 1.0 | https://freesound.org/people/felix.blume/sounds/156414/ | amb/storm_peaks_night |
| Wind howling in an electric transformer box on the High Atlas mountain (Morocco). | felix.blume (freesound) | CC0 1.0 | https://freesound.org/people/felix.blume/sounds/157280/ | amb/storm_cliffs, world/storm_rod_hum (x2) |
| wolves.wav | Paresh (freesound) | CC0 1.0 | https://freesound.org/people/Paresh/sounds/158780/ | creature/stormwolf_idle (x3) |
| Wind blowing in an antenna on the High Atlas mountain (Morocco). | felix.blume (freesound) | CC0 1.0 | https://freesound.org/people/felix.blume/sounds/160469/ | amb/storm_rodfield, world/storm_rod_hum (x2) |
| Gravel Impacts and Falls.wav | lolamadeus (freesound) | CC0 1.0 | https://freesound.org/people/lolamadeus/sounds/179341/ | world/storm_rockfall_trickle (x2) |
| Whining Dog.flac | qubodup (freesound) | CC0 1.0 | https://freesound.org/people/qubodup/sounds/184808/ | creature/stormwolf_death (x2), creature/stormwolf_hurt (x3) |
| Angry Ram | ReadeOnly (freesound) | CC0 1.0 | https://freesound.org/people/ReadeOnly/sounds/186921/ | creature/ram_aggro (x3), creature/ram_attack (x3) |
| Dog snarl (Self-made) | deleted_user_3424813 (freesound) | CC0 1.0 | https://freesound.org/people/deleted_user_3424813/sounds/190595/ | creature/stormwolf_attack (x3) |
| rayjensen - thunder-crack 41k 16bit mono.wav | rayjensen (freesound) | CC0 1.0 | https://freesound.org/people/rayjensen/sounds/200990/ | world/storm_clap (x3) |
| Spark sounds | xavimuse (freesound) | CC0 1.0 | https://freesound.org/people/xavimuse/sounds/232841/ | world/storm_spark (x3) |
| Arc Welding.wav | squashy555 (freesound) | CC0 1.0 | https://freesound.org/people/squashy555/sounds/256915/ | creature/mustelid_aggro (x3), creature/mustelid_attack (x3), world/storm_ground_current (x2) |
| baby lamb calling his mother .WAV | EduFigueres (freesound) | CC0 1.0 | https://freesound.org/people/EduFigueres/sounds/266689/ | creature/sheep_death (x2), creature/sheep_hurt (x2) |
| Rocky Mountain Outdoors:  wind and birds | petebuchwald (freesound) | CC0 1.0 | https://freesound.org/people/petebuchwald/sounds/288899/ | amb/storm_peaks_day |
| Crumble #8.wav | abstraktgeneriert (freesound) | CC0 1.0 | https://freesound.org/people/abstraktgeneriert/sounds/348955/ | creature/mole_attack (x2), creature/mole_death (x2), world/storm_burrow (x3) |
| Cowbell - Single 01.wav | RP1312 (freesound) | CC0 1.0 | https://freesound.org/people/RP1312/sounds/350711/ | world/storm_bell_clank (x3) |
| gekko.mp3 | mopedmetzger (freesound) | CC0 1.0 | https://freesound.org/people/mopedmetzger/sounds/351301/ | creature/lizard_hurt (x3), creature/lizard_idle (x3) |
| wind_gust.aif | joseph.larralde (freesound) | CC0 1.0 | https://freesound.org/people/joseph.larralde/sounds/352421/ | amb/storm_passes, world/storm_gust (x3) |
| Goat .MP3 | macdaddyno1 (freesound) | CC0 1.0 | https://freesound.org/people/macdaddyno1/sounds/352600/ | creature/goat_hurt (x3) |
| Sheep bleating | zachrau (freesound) | CC0 1.0 | https://freesound.org/people/zachrau/sounds/362283/ | creature/sheep_aggro (x3), creature/sheep_attack (x3) |
| Big Tesla coil sound cut.wav | follytowers (freesound) | CC0 1.0 | https://freesound.org/people/follytowers/sounds/362975/ | world/storm_arc (x3) |
| Eagle Cry.wav | kaydinhamby (freesound) | CC0 1.0 | https://freesound.org/people/kaydinhamby/sounds/381200/ | creature/raptor_death (x2), creature/roc_aggro (x3), creature/roc_death (x2) |
| RockFall1b.wav | AlanCat (freesound) | CC0 1.0 | https://freesound.org/people/AlanCat/sounds/381645/ | world/storm_rockfall_impact (x3) |
| Sheep bleating | zachrau (freesound) | CC0 1.0 | https://freesound.org/people/zachrau/sounds/383144/ | creature/sheep_aggro (x3), creature/sheep_death (x2), creature/sheep_hurt (x2) |
| Digging a Hole and Shovelling Soil/Dirt 2 | Ali_6868 (freesound) | CC0 1.0 | https://freesound.org/people/Ali_6868/sounds/384362/ | world/storm_burrow (x3) |
| rockfall2a.wav | AlanCat (freesound) | CC0 1.0 | https://freesound.org/people/AlanCat/sounds/389303/ | world/storm_rockfall_impact (x3) |
| Goat.wav | Forpus_Prod (freesound) | CC0 1.0 | https://freesound.org/people/Forpus_Prod/sounds/391320/ | creature/goat_death (x2), creature/goat_idle (x3) |
| wolf 2.wav | genel (freesound) | CC0 1.0 | https://freesound.org/people/genel/sounds/399186/ | creature/stormwolf_aggro (x3) |
| Wood panel board cracking snapping long | LampEight (freesound) | CC0 1.0 | https://freesound.org/people/LampEight/sounds/400645/ | world/storm_plank_snap (x3) |
| high-voltage-discharge-2.wav | follytowers (freesound) | CC0 1.0 | https://freesound.org/people/follytowers/sounds/415962/ | creature/mustelid_aggro (x3), world/storm_arc (x3), world/storm_ball_pop (x2), world/storm_shrine (x2) |
| Close lightning strike 2018 07 07 | csengeri (freesound) | CC0 1.0 | https://freesound.org/people/csengeri/sounds/434359/ | world/storm_strike (x3) |
| Heavy rain with thuder | dersinnsspace (freesound) | CC0 1.0 | https://freesound.org/people/dersinnsspace/sounds/435222/ | amb/storm_thunderstorm |
| Very close lightning strike. | Simon Spiers (freesound) | CC0 1.0 | https://freesound.org/people/Simon Spiers/sounds/437243/ | world/storm_clap (x3), world/storm_strike (x3) |
| Wind chimes 1 | giddster (freesound) | CC0 1.0 | https://freesound.org/people/giddster/sounds/437337/ | amb/storm_shrines |
| G12-05-Camel Grunt.wav | craigsmith (freesound) | CC0 1.0 | https://freesound.org/people/craigsmith/sounds/437937/ | creature/yak_aggro (x2), creature/yak_death (x2) |
| hum electric transformer in large room2.flac | kyles (freesound) | CC0 1.0 | https://freesound.org/people/kyles/sounds/453445/ | amb/storm_hollow, world/storm_ball_hum (x2) |
| mountain wind heavy strong gusts Swiss Alps.flac | kyles (freesound) | CC0 1.0 | https://freesound.org/people/kyles/sounds/454092/ | amb/storm_bridges, amb/storm_crags, amb/storm_peaks, amb/storm_peaks_day, amb/storm_peaks_night, amb/storm_shrines |
| TOLLING BELL | SamuelGremaud (freesound) | CC0 1.0 | https://freesound.org/people/SamuelGremaud/sounds/454855/ | world/storm_bell_toll (x3), world/storm_shrine (x2) |
| Sheep Bleating | Sheyvan (freesound) | CC0 1.0 | https://freesound.org/people/Sheyvan/sounds/471162/ | creature/sheep_attack (x3), creature/sheep_idle (x3) |
| 21_Lobo_gruñendo.wav | JoseAgudelo (freesound) | CC0 1.0 | https://freesound.org/people/JoseAgudelo/sounds/472401/ | creature/stormwolf_attack (x3) |
| 22_Lobo_aullando.wav | JoseAgudelo (freesound) | CC0 1.0 | https://freesound.org/people/JoseAgudelo/sounds/472402/ | creature/stormwolf_aggro (x3), creature/stormwolf_death (x2) |
| 20120808_064004 Cathedral Mountain rock slide, slow blocky chunks.wav | betchkal (freesound) | CC0 1.0 | https://freesound.org/people/betchkal/sounds/500638/ | world/storm_rockfall_impact (x3) |
| Cooper Creek 20160313_014852 solitary wolf howl very clear.wav | betchkal (freesound) | CC0 1.0 | https://freesound.org/people/betchkal/sounds/500646/ | creature/stormwolf_idle (x3) |
| Snow crackling on the top of the mountain Chacaltaya (Corona Discharge) | felix.blume (freesound) | CC0 1.0 | https://freesound.org/people/felix.blume/sounds/503139/ | world/storm_cloud_static (x2) |
| Wood Creak Single V10 | Rudmer_Rotteveel (freesound) | CC0 1.0 | https://freesound.org/people/Rudmer_Rotteveel/sounds/506665/ | world/storm_plank_creak (x3) |
| Bison bellowing - Yellowstone National Park | Nivatius (freesound) | CC0 1.0 | https://freesound.org/people/Nivatius/sounds/519594/ | creature/ram_death (x2), creature/ram_hurt (x3), creature/yak_death (x2) |
| Three Rams | D.jones (freesound) | CC0 1.0 | https://freesound.org/people/D.jones/sounds/521856/ | creature/ram_idle (x3) |
| Stone Crumble 2 | Kastenfrosch (freesound) | CC0 1.0 | https://freesound.org/people/Kastenfrosch/sounds/521956/ | world/storm_rockfall_trickle (x2) |
| Angry hedgehog sniffing 1 | fthgurdy (freesound) | CC0 1.0 | https://freesound.org/people/fthgurdy/sounds/528183/ | creature/mole_aggro (x3), creature/mole_attack (x2) |
| Snake Hiss | xoiziox (freesound) | CC0 1.0 | https://freesound.org/people/xoiziox/sounds/553374/ | creature/lizard_aggro (x3) |
| Fox scream in the forest | nielstii (freesound) | CC0 1.0 | https://freesound.org/people/nielstii/sounds/554308/ | creature/mustelid_death (x2) |
| river-canyon-winter-heavy-flow-with-fadeout.wav | 1888software (freesound) | CC0 1.0 | https://freesound.org/people/1888software/sounds/555736/ | amb/storm_bridges |
| LION ROAR | Podcapocalipsis (freesound) | CC0 1.0 | https://freesound.org/people/Podcapocalipsis/sounds/559600/ | creature/roc_attack (x3) |
| 180903 Summer cricket heavy field hot clean PEC.mp3 | TRP (freesound) | CC0 1.0 | https://freesound.org/people/TRP/sounds/571206/ | creature/mole_aggro (x3), creature/mole_idle (x3) |
| 180806 Thunder, distant rolling boomy, rain drops TORONTO.flac | TRP (freesound) | CC0 1.0 | https://freesound.org/people/TRP/sounds/572443/ | world/storm_thunder (x3) |
| Thunder, single long rolling, distant, light rain, TORONTO, 2011.wav | TRP (freesound) | CC0 1.0 | https://freesound.org/people/TRP/sounds/577315/ | creature/roc_aggro (x3), creature/roc_death (x2), world/storm_thunder (x3), world/storm_wingbeat (x3) |
| Purring Guinea Pig | Breviceps (freesound) | CC0 1.0 | https://freesound.org/people/Breviceps/sounds/583077/ | creature/mole_death (x2), creature/mole_idle (x3) |
| sneeze goat 04.wav | klankbeeld (freesound) | CC-BY 4.0 | https://freesound.org/people/klankbeeld/sounds/587167/ | creature/goat_aggro (x3), creature/goat_attack (x3) |
| Mountain River, Large 001.wav | Auxide_Audio (freesound) | CC0 1.0 | https://freesound.org/people/Auxide_Audio/sounds/588180/ | amb/storm_bridges |
| Large Wings Flapping - Foley.wav | tothrec2 (freesound) | CC0 1.0 | https://freesound.org/people/tothrec2/sounds/596541/ | world/storm_wingbeat (x3) |
| Big Lion Roar | _justMonke_ (freesound) | CC0 1.0 | https://freesound.org/people/_justMonke_/sounds/611721/ | creature/roc_death (x2), creature/roc_idle (x2) |
| 210914 Thunder, crack, pretty close, bit of distortion, urban, TORONTO 1am.wav | TRP (freesound) | CC0 1.0 | https://freesound.org/people/TRP/sounds/616992/ | world/storm_clap (x3) |
| 2022-02-06 wood creak 1.wav | sit_walk (freesound) | CC0 1.0 | https://freesound.org/people/sit_walk/sounds/618085/ | world/storm_plank_creak (x3) |
| underground mine dripping.wav | poots (freesound) | CC0 1.0 | https://freesound.org/people/poots/sounds/627789/ | amb/storm_hollow |
| Fox Screams | Soundburst (freesound) | CC0 1.0 | https://freesound.org/people/Soundburst/sounds/634005/ | creature/mustelid_aggro (x3), creature/mustelid_hurt (x3) |
| spark2.ogg | Kneeling (freesound) | CC0 1.0 | https://freesound.org/people/Kneeling/sounds/643861/ | creature/mole_idle (x3), world/storm_spark (x3) |
| Waterfall_Meraner_Höhenweg_01_160922.wav | datapopstar2 (freesound) | CC0 1.0 | https://freesound.org/people/datapopstar2/sounds/650699/ | amb/storm_cliffs |
| Meadow prairie atmosphere with insect, subtle gust of wind, grass, cicada, distant bird in Agua Fría falls in Amazon rainforest | felix.blume (freesound) | CC0 1.0 | https://freesound.org/people/felix.blume/sounds/672330/ | amb/storm_passes |
| 230104-060_normalized_titanic_bell.wav | mmmtoop (freesound) | CC0 1.0 | https://freesound.org/people/mmmtoop/sounds/674043/ | world/storm_bell_toll (x3), world/storm_shrine (x2) |
| S28-16 Eagle squawks & calls.wav | craigsmith (freesound) | CC0 1.0 | https://freesound.org/people/craigsmith/sounds/675962/ | amb/storm_peaks_day, creature/raptor_attack (x3), creature/raptor_death (x2), creature/roc_death (x2), creature/roc_idle (x2) |
| Snake Tongue Hiss.WAV | bowlingballout (freesound) | CC0 1.0 | https://freesound.org/people/bowlingballout/sounds/678971/ | creature/lizard_aggro (x3), creature/lizard_attack (x3), creature/lizard_death (x2) |
| Cat Growling and Hissing | dinoroki (freesound) | CC0 1.0 | https://freesound.org/people/dinoroki/sounds/679954/ | creature/mustelid_idle (x3) |
| Tookgeh Gecko | Organuz (freesound) | CC0 1.0 | https://freesound.org/people/Organuz/sounds/686626/ | creature/lizard_aggro (x3), creature/lizard_death (x2), creature/lizard_hurt (x3) |
| Digging Into Soft Soil with Small Trowell.wav | dhallcomposer (freesound) | CC0 1.0 | https://freesound.org/people/dhallcomposer/sounds/696531/ | world/storm_burrow (x3) |
| Garip Island major from cliff side - binaural | tomhannen (freesound) | CC0 1.0 | https://freesound.org/people/tomhannen/sounds/698546/ | amb/storm_passes |
| Large Wings / Superhero Cape Foley | Cultureshock007 (freesound) | CC0 1.0 | https://freesound.org/people/Cultureshock007/sounds/711122/ | world/storm_wingbeat (x3) |
| BPSN electrical hum 1 | chungus43A (freesound) | CC0 1.0 | https://freesound.org/people/chungus43A/sounds/721812/ | world/storm_ball_hum (x2) |
| Bleating Goat | aisselles (freesound) | CC0 1.0 | https://freesound.org/people/aisselles/sounds/722658/ | creature/goat_attack (x3), creature/goat_death (x2) |
| Angry Cat (Perran) | Sadiquecat (freesound) | CC0 1.0 | https://freesound.org/people/Sadiquecat/sounds/750495/ | creature/mustelid_attack (x3), creature/mustelid_death (x2), creature/mustelid_idle (x3) |
| Bird - Red-shouldered hawk | Vrymaa (freesound) | CC0 1.0 | https://freesound.org/people/Vrymaa/sounds/770032/ | creature/raptor_attack (x3), creature/roc_hurt (x2) |
| Wind whistle 3 | Sadiquecat (freesound) | CC0 1.0 | https://freesound.org/people/Sadiquecat/sounds/770282/ | amb/storm_passes, world/storm_gust (x3) |
| ELECArc_Electric arc hum and buzz. A longer movement of electricity, a power station switching arc when decoupled. designed with synth_Anton Bushinskiy_AB FX | pryanic (freesound) | CC0 1.0 | https://freesound.org/people/pryanic/sounds/776446/ | world/storm_pretell (x2), world/storm_ring_buzz (x2) |
| Hoary marmot whistles | SoundsLikeYukon (freesound) | CC0 1.0 | https://freesound.org/people/SoundsLikeYukon/sounds/822939/ | creature/goat_aggro (x3) |
| 250910_22h43 Gergueil D104 - Brame | Sadiquecat (freesound) | CC0 1.0 | https://freesound.org/people/Sadiquecat/sounds/826421/ | creature/ram_aggro (x3), creature/ram_death (x2) |
| aeolian_harp_20250906_1721 | markb (freesound) | CC-BY 4.0 | https://freesound.org/people/markb/sounds/826812/ | amb/storm_rodfield |
| Yak grunts | ali.g (freesound) | CC0 1.0 | https://freesound.org/people/ali.g/sounds/834331/ | creature/yak_aggro (x2), creature/yak_hurt (x2), creature/yak_idle (x3) |
| Whistling wind | Artistic_Dragonborn (freesound) | CC0 1.0 | https://freesound.org/people/Artistic_Dragonborn/sounds/838744/ | amb/storm_crags, world/storm_gust (x3) |
| Condor Scream | floatby (freesound) | CC0 1.0 | https://freesound.org/people/floatby/sounds/838963/ | creature/raptor_hurt (x3), creature/roc_aggro (x3), creature/roc_attack (x3) |
| Closeup Thunder Strike 01 | loganzsound (freesound) | CC0 1.0 | https://freesound.org/people/loganzsound/sounds/840628/ | world/storm_strike (x3) |
| Rodents - Brown Rats; Several Babies Squeaking, Close Perspective | TheKingOfGeeks360 (freesound) | CC0 1.0 | https://freesound.org/people/TheKingOfGeeks360/sounds/841571/ | creature/mole_death (x2), creature/mole_hurt (x3) |
| Tesla Tower Zap - Electric Zap Short - Game SFX | el_boss (freesound) | CC0 1.0 | https://freesound.org/people/el_boss/sounds/853286/ | creature/stormwolf_attack (x3), world/storm_arc (x3), world/storm_ball_pop (x2), world/storm_shrine (x2) |
| Birds of Prey: Red Kite, soft social calls | MichiJung (freesound) | CC-BY 4.0 | https://freesound.org/people/MichiJung/sounds/865812/ | amb/storm_peaks_day, creature/raptor_idle (x3) |
| Goats - Two Billies Sparring; Headbutt, Horns Clanking | TheKingOfGeeks360 (freesound) | CC0 1.0 | https://freesound.org/people/TheKingOfGeeks360/sounds/866683/ | creature/goat_attack (x3), creature/ram_attack (x3) |
| Bovines - American Bison; Nasal Huffs, Close Perspective | TheKingOfGeeks360 (freesound) | CC0 1.0 | https://freesound.org/people/TheKingOfGeeks360/sounds/867549/ | creature/yak_attack (x2) |
| Birds of Prey: Red Kite, medium loud excited calls | MichiJung (freesound) | CC-BY 4.0 | https://freesound.org/people/MichiJung/sounds/868531/ | creature/raptor_aggro (x3) |
| Birds of Prey: Red Kite, loud territorial call | MichiJung (freesound) | CC-BY 4.0 | https://freesound.org/people/MichiJung/sounds/868738/ | creature/raptor_aggro (x3) |
| Drips, Giant Cave Drips, Slow | CHallSmith (freesound) | CC0 1.0 | https://freesound.org/people/CHallSmith/sounds/870748/ | amb/storm_hollow |
| ELECArc_Sparking Electricity Bed | harrisonlace (freesound) | CC0 1.0 | https://freesound.org/people/harrisonlace/sounds/871766/ | creature/stormwolf_aggro (x3), creature/stormwolf_death (x2), world/storm_ball_hum (x2), world/storm_ground_current (x2) |
| WOODBrk_Layered Cracking Thin Plywood Board Splinters x 7 | harrisonlace (freesound) | CC0 1.0 | https://freesound.org/people/harrisonlace/sounds/873248/ | world/storm_plank_snap (x3) |
| Bison | timpet55 (freesound) | CC0 1.0 | https://freesound.org/people/timpet55/sounds/873888/ | creature/ram_hurt (x3), creature/yak_attack (x2) |
| Electrical Static - crackles and pops | cb9382 (freesound) | CC0 1.0 | https://freesound.org/people/cb9382/sounds/875011/ | amb/storm_crags, creature/sheep_aggro (x3), creature/stormwolf_hurt (x3), creature/stormwolf_idle (x3), world/storm_ball_hum (x2), world/storm_cloud_static (x2) |
| Red Tailed Hawk Cries | loganzsound (freesound) | CC0 1.0 | https://freesound.org/people/loganzsound/sounds/875513/ | creature/raptor_hurt (x3), creature/raptor_idle (x3) |

## Animation

| What | Files in the repo | Source | Author | Licence |
| --- | --- | --- | --- | --- |
| Hero action animations from Mixamo's Sword And Shield pack (block, slashes, attacks, casts, hits, deaths, kick) and singles (dodge roll, overhead chop, gathering, pick up, swimming, treading water), and the Longbow pack (bow idle / equip / draw / aim / recoil, rifle aim / fire / reload, throw), retargeted onto the MetaHuman hero | `Content/Wildreach/Characters/Hero/Anims/Actions/A_*.uasset` | https://www.mixamo.com (first rigged on the old survivor with `Tools/art/survivor_build.py`, then retargeted in the hero lab with an IK Retargeter) | Adobe Mixamo (motion capture) | Mixamo terms of use: royalty-free in personal and commercial projects, including games; the raw files may not be redistributed on their own |
| Hero locomotion for motion matching: idles, walk / run / sprint starts, loops, stops and transitions, turns in place, jumps, falls and landings (the Game Animation Sample's Dense idles and Extreme Sparse sets), retargeted onto the MetaHuman hero; the databases rebuilt from the sample's own settings | `Content/Wildreach/Characters/Hero/Anims/Loco/MM_*.uasset`, `Anims/MM/` | Epic Games' Game Animation Sample Project 5.8 (Fab) | Epic Games | Epic Content (Unreal Engine EULA): free for use in Unreal Engine projects, including commercial games |
| The hero's MetaHuman body, face and grooms (M1 Ash, F1 Wren, athletic build) | `Content/Wildreach/Characters/Hero/Unpacked/` | Built in the hero lab with UE 5.8's MetaHuman Creator (engine plugin) | Epic Games (MetaHuman) | MetaHuman licence: free for use in Unreal Engine projects |
| Humanoid boss animations (Creature Pack: idles, walking, run, swiping, punch, jump attack, roaring, dying) | `Content/Creatures/Vulkarn`, `Grimjaw`, `Obsidian_warden` | https://www.mixamo.com | Adobe Mixamo | Mixamo terms of use, as above |

The creature animations made by `Tools/art/rig_quadruped.py` and `rig_special.py` are procedural (written for this
game); the pack creatures (Gloomhowl, Scorchwing, Skaldrith) use the animations that came with the QuadrapedCreatures
Fab pack, which is not in the repository.

## World (polish pass, 2026-10)

### Base props made with the local AI pipeline (in git)

The workbench, forge, armorer, alchemy table, Hearth (a stone fire ring), claim totem (a carved post) and storage
chest under `Content/Props/` (`Workbench`, `Forge`, `Armorer`, `AlchemyTable`, `Hearth`, `ClaimTotem`,
`StorageChest`) were made on this machine with the pipeline in `Tools/art/README.md`:

- Concept images: Stable Diffusion XL base 1.0 (Stability AI, CreativeML Open RAIL++-M licence: commercial use
  allowed). Prompts in `Tools/art/batch_stations.json` and `batch_stations2.json`.
- Shape and texture: **Hunyuan3D 2.0** (Tencent Hunyuan 3D 2.0 Community License). Commercial use is allowed below
  1 million monthly active users, and the licence **does not cover the European Union, the United Kingdom or South
  Korea**: these props, like the AI creatures, carry that territory limit.
- Cleanup: `Tools/art/normalize_prop.py` (Blender) decimates, shrinks the texture and centres each one.

### The sea (Fab pack, not in git)

The ocean material is `M_Ocean` from the **WaterMaterials** pack (`Content/WaterMaterials`, Raven's Fab library,
standard Fab licence for use in a game). The game uses it through its own instance `Content/Sea/MI_Ocean` (in git),
which is cooked with the game (`DirectoriesToAlwaysCook` `/Game/Sea`), so the packaged game carries the parts of
the pack it needs.

### Building pieces, sky and camera

Made from engine shapes and the project's own `M_BuildWood` / `M_BuildStone` materials (`Tools/make_build_materials.py`,
engine content only); the atmosphere and camera are code. Nothing downloaded.
