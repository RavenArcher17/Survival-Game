# The sound pass's sources: key -> (page, file). fetch_sources.py checks each page's licence and that it links the file,
# then downloads it; build_audio.py cuts and encodes them into Data/Audio. Docs/CREDITS.md lists who made each.
OGA = "https://opengameart.org/content/"
F = "https://opengameart.org/sites/default/files/"
BSB = "https://bigsoundbank.com/"
BF = "https://bigsoundbank.com/UPLOAD/"

ITEMS = {
    # music
    "m_menu": (OGA + "fantasy-orchestral-theme", F + "FantasyOrchestralTheme_1.mp3"),
    "m_winter": (OGA + "long-winter", F + "Long%20Winter_0.mp3"),
    "m_winds": (OGA + "mysterious-winds", F + "mysteriouswinds.mp3"),
    "m_battle": (OGA + "battle-theme-a", F + "battleThemeA.mp3"),
    "m_boss": (OGA + "boss-battle-music", F + "Juhani%20Junkala%20-%20Epic%20Boss%20Battle%20%5BSeamlessly%20Looping%5D.wav"),
    "m_bossintro": (OGA + "dramatic-boss-encounter", F + "Heavy%20Concept%20A%20Bass%20Master.wav"),
    "m_moon": (OGA + "fantasy-rising-moon", F + "Rising_Moon_0.mp3"),
    "m_explore": (OGA + "medieval-exploration", F + "Exploration_0.mp3"),
    "m_cathedral": (OGA + "cathedral-in-the-forest-ambient-loop", F + "forest_2_0.ogg"),
    # ambient
    "a_wind": (BSB + "strong-wind-1-s0146.html", BF + "ogg/0146.ogg"),
    "a_whistle": (BSB + "whistling-of-the-wind-1-s0147.html", BF + "ogg/0147.ogg"),
    "a_woosh": (OGA + "wind-whoosh-loop", F + "wind%20woosh%20loop.ogg"),
    "a_seawaves": (BSB + "sea-waves-s0698.html", BF + "ogg/0698.ogg"),
    "a_smallwaves": (BSB + "small-waves-froth-and-beach-s1447.html", BF + "mp3/1447.mp3"),
    "a_forest": (BSB + "forest-3-s2715.html", BF + "ogg/2715.ogg"),
    "a_morning": (OGA + "amb-morning-sounds-perfect-loop", F + "amb_morning_0.ogg"),
    "a_birds": (OGA + "ambient-bird-sounds", F + "birds-isaiah658_0.ogg"),
    "a_crickets": (OGA + "crickets-ambient-noise-loopable", F + "crickets_1.mp3"),
    "a_insect": (BSB + "nocturnal-insect-3-s0426.html", BF + "ogg/0426.ogg"),
    "a_owls": (BSB + "owls-s0429.html", BF + "ogg/0429.ogg"),
    "a_dungeon": (OGA + "loopable-dungeon-ambience", F + "dungeon_ambient_1_0.ogg"),
    "a_cave": (BSB + "cave-1-s2135.html", BF + "mp3/2135.mp3"),
    "a_cavern": (OGA + "dark-cavern-ambient", F + "dark_cavern_ambient_002.ogg"),
    "a_rumble": (OGA + "low-rumbling", F + "low-rumbling-176033.mp3"),
    "a_crackle": (OGA + "fire-crackling", F + "fire-1_0.ogg"),
    "a_lava": (OGA + "lava-loop", F + "lavaloop.zip"),
    # creatures
    "c_rd1": (OGA + "80-cc0-creature-sfx", F + "80-CC0-creature-SFX_0.zip"),
    "c_rd2": (OGA + "80-cc0-creture-sfx-2", F + "80-CC0-creature-sfx-2.zip"),
    "c_ogre": (OGA + "monster-sound-pack-volume-1", F + "monster-sounds-volume-2.zip"),
    "c_trollroar": (OGA + "big-scary-troll-sounds", F + "troll-roars_0.ogg"),
    "c_trollidle": (OGA + "big-scary-troll-sounds", F + "troll-idle-noises_0.ogg"),
    "c_bear": (OGA + "bear-growls", F + "bear.zip"),
    "c_wolf": (OGA + "wolf-monster-sound", F + "wolf_monster_6.mp3"),
    "c_bat": (OGA + "bat-screeches", F + "bat.zip"),
    "c_insect": (OGA + "insect-or-alien-scream", F + "insectoralien.flac"),
    "c_pig": (BSB + "grumpy-pig-2-s1659.html", BF + "ogg/1659.ogg"),
    "c_roar": (OGA + "cc0-deep-monster-roar", F + "monster_roar.wav"),
    "c_mino": (OGA + "osare-minotaur-sounds", F + "mino1.wav"),
    "c_growls": (OGA + "16-monster-growls", F + "monster_-_starninjas.zip"),
    # combat
    "w_swish": (OGA + "swishes-sound-pack", F + "swishes.zip"),
    "w_sword": (OGA + "20-sword-sound-effects-attacks-and-clashes", F + "sword_-_starninjas_1.zip"),
    "w_clash": (OGA + "20-sword-sound-effects-attacks-and-clashes", F + "sword_clash_-_starninjas_0.zip"),
    "w_battle": (OGA + "battle-sound-effects", F + "battle_sound_effects_0.zip"),
    "w_archery": (BSB + "archery-s0549.html", BF + "ogg/0549.ogg"),
    "w_arrowhit": (BSB + "an-arrow-on-a-target-s0548.html", BF + "ogg/0548.ogg"),
    "w_crossbow": (OGA + "crossbow-sounds", F + "crossbow_shot_and_hit.flac"),
    "w_crossbowdry": (OGA + "crossbow-sounds", F + "crossbow_dryshot.flac"),
    "w_leather": (OGA + "footsteps-leather-cloth-armor", F + "footsteps.zip"),
    "v_female": (OGA + "female-rpg-voice-starter-pack", F + "RPG%20Voice%20Starter%20Pack.zip"),
    "v_femalehurt": (OGA + "female-hurt-grunts-groans", F + "female_hurt_grunts_groans_1.ogg"),
    "v_male": (OGA + "15-vocal-male-strainhurtpainjump-sounds", F + "slightscreams.7z"),
    "v_maledeath": (OGA + "grunts-male-death-and-pain", F + "death%20pain%20grunts.wav"),
    # spells
    "s_fireball": (OGA + "fireball-1", F + "105016__julien-matthey__jm-fx-fireball-01.wav"),
    "s_rpg": (OGA + "80-cc0-rpg-sfx", F + "80-CC0-RPG-SFX_0.zip"),
    "s_ice": (OGA + "ice-spells", F + "icespells.zip"),
    "s_freeze": (OGA + "freeze-spell-0", F + "freeze.wav"),
    "s_shatter": (OGA + "ice-breakingshattering", F + "IceShatters_0.zip"),
    "s_bang": (OGA + "25-cc0-bang-firework-sfx", F + "25-CC0-bang-sfx.zip"),
    "s_magic": (OGA + "magic-spell-sfx", F + "magical_1_0.ogg"),
    # foley
    "f_crunch": (OGA + "7-eating-crunches", F + "crunch_-_tito.zip"),
    "f_drink": (OGA + "liquid-bottle-drink-set", F + "qubodup-bottle-and-drink-sounds.7z"),
    "f_sand": (OGA + "water-splash-and-sand-footsteps", F + "sand_footsteps_0.mp3"),
    "f_splash": (OGA + "water-splash-and-sand-footsteps", F + "splash1_0.wav"),
    "f_water": (OGA + "40-cc0-water-splash-slime-sfx", F + "water-splash-slime-sfx.zip"),
    "f_anvil": (OGA + "hammer-on-anvil", F + "forger_0.ogg"),
    "f_smith": (OGA + "blacksmiths-hammer", F + "blacksmithhammer.wav"),
    "f_ignite": (OGA + "catching-fire", F + "flame_0.ogg"),
    "f_break": (OGA + "75-cc0-breaking-falling-hit-sfx", F + "sfx_breaking_and_falling.zip"),
    "f_woodmetal": (OGA + "100-cc0-metal-and-wood-sfx", F + "100-CC0-wood-metal-SFX.zip"),
    "f_cavein": (OGA + "cave-in", F + "cave_in_-_starninjas_0.ogg"),
    "f_sfx100": (OGA + "100-cc0-sfx-2", F + "sfx_100_v2.zip"),
}

# Kenney's CC0 packs (zip links come from each page: fetch_sources.py finds them).
KENNEY = ["rpg-audio", "impact-sounds", "interface-sounds", "ui-audio", "music-jingles"]
