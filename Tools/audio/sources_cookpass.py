# The audio cook pass's sources (build_cookpass_audio.py; cooks/cookpass-audio.md 4.2, 4.6-4.8). Music: Kevin MacLeod
# (incompetech.com), CC-BY 4.0, real instruments only (picked from incompetech's pieces.json by tempo, instruments and
# feel; nothing listing synths, electric instruments, drum kit or guitar). Recordings: Freesound, CC0 1.0 or CC-BY 4.0,
# the licence read off each sound's page (2026-11-15), the HQ preview MP3 (no login needed). Docs/CREDITS.md lists them.
# Fetch: python -I sources_cookpass.py <work>  -> <work>/dl/music/<Title>.mp3, <work>/dl/fs/<id>.mp3, then
#        python build_cookpass_audio.py <work> Data/Audio   (FFMPEG=<path to ffmpeg> if it isn't on PATH)
import os, sys, urllib.parse, urllib.request

MUSIC_FILE = "https://incompetech.com/music/royalty-free/mp3-royaltyfree/"
# output file -> (title, ISRC, BPM, the cue it plays as)
MUSIC = {
    "vol_explore_a": ("Death of Kings", "USUAN1100876", 60, "explore.volcanic A: slow brass over a drum"),
    "vol_explore_b": ("Steel Rods", "USUAN1200052", 69, "explore.volcanic B: struck steel, the anvil ostinato"),
    "vol_explore_c": ("Drums of the Deep", "USUAN1400021", 77, "explore.volcanic.c (V4): sparse drums, low strings"),
    "vol_night_a": ("Mary Celeste", "USUAN1500016", 50, "night.volcanic A: low brass and bass clarinet drone"),
    "vol_night_b": ("Oppressive Gloom", "USUAN1100885", 58, "night.volcanic B: death-march strings"),
    "vol_combat": ("The Descent", "USUAN1200094", 103, "combat.volcanic: horns, trombones, tuba, driving percussion"),
    "vol_mini": ("Colossus", "USUAN1100358", 86, "mini.volcanic: brass and percussion"),
    "vol_boss_p1": ("Industrial Revolution", "USUAN1100811", 140, "boss.vulkarn.p1: pipe organ, trombones, forge percussion"),
    "vol_boss_p2": ("Death and Axes", "USUAN1700049", 140, "boss.vulkarn.p2: the full orchestra doubles the drive"),
    "vol_boss_p3": ("Stoneworld Battle", "USUAN1100352", 110, "boss.vulkarn.p3: the giant battle"),
    "vol_kiln_roads": ("Tenebrous Brothers Carnival - Act Three", "USUAN1100642", 78, "sub.kiln_roads: metallic percussion, a giant mechanism"),
    "vol_eruption": ("StompDance", "USUAN1100828", 80, "layer.eruption: war drums, accelerating"),
    "vol_camp": ("Peaceful Desolation", "USUAN1200017", 54, "camp.volcanic: low brass and timpani at rest"),
    "tun_explore_a": ("Wounded", "USUAN1100586", 50, "explore.tundra A: piano and strings"),
    "tun_explore_b": ("For Originz", "USUAN1100700", 55, "explore.tundra B: strings, sliding violin, a woman's voice"),
    "tun_explore_c": ("Frost Waltz", "USUAN1100516", 99, "explore.tundra.c (T4, T5): celesta, glockenspiel, strings"),
    "tun_night_a": ("At Rest", "USUAN1100748", 57, "night.tundra A: a voice over piano and strings"),
    "tun_night_b": ("Blue Feather", "USUAN1100524", 42, "night.tundra B: solo piano"),
    "tun_combat": ("Gloom Horizon", "USUAN1100813", 100, "combat.tundra: the low string ostinato"),
    "tun_mini": ("The Snow Queen", "USUAN1100872", 96, "mini.tundra: strings, bassoon, celesta, timpani"),
    "tun_boss_p1": ("The Escalation", "USUAN1400041", 96, "boss.skaldrith.p1: one long string build"),
    "tun_boss_p2": ("Hero Down", "USUAN1100880", 110, "boss.skaldrith.p2: horns rise with the flight"),
    "tun_boss_p3": ("Darkling", "USUAN1700050", 140, "boss.skaldrith.p3: pulsing doom in the whiteout"),
    "tun_blue_below": ("Relent", "USUAN1100777", 56, "sub.blue_below: harp and clarinet"),
    "tun_whiteout": ("Spirit of the Girl", "USUAN1700057", 54, "layer.whiteout: low drums and bells"),
    "tun_camp": ("Water Lily", "USUAN1400035", 62, "camp.tundra: piano lullaby"),
    "des_explore_a": ("Hidden Wonders", "USUAN1700006", 75, "explore.desert: santur, strings, tanpura"),
    "des_explore_b": ("Opium", "USUAN1100371", 90, "explore.desert: strings, santoor, voice drone"),
    "des_night": ("Nightdreams", "USUAN1700054", 54, "night.desert: hang drum"),
    "des_combat": ("Return of the Mummy", "USUAN1700046", 140, "combat.desert: tar, qanun, choir"),
    "des_mini": ("Curse of the Scarab", "USUAN1600014", 190, "mini.desert: winds and percussion"),
    "grave_explore_a": ("Halls of the Undead", "USUAN1100355", 90, "explore.graveyard: pipe organ, choir, chimes"),
    "grave_explore_b": ("Rites", "USUAN1100722", 0, "explore.graveyard: voice and pipe organ"),
    "grave_night": ("Agnus Dei X", "USUAN1100679", 60, "night.graveyard: choir and pipe organ"),
    "grave_combat": ("Grim Idol", "USUAN1700043", 140, "combat.graveyard: strings, organs, cimbasso"),
    "grave_mini": ("Nightmare Machine", "USUAN1700048", 140, "mini.graveyard: organ, basses, percussion"),
    "crys_explore_a": ("Tenebrous Brothers Carnival - Mermaid", "USUAN1100671", 0, "explore.crystal: glass harmonica, harps, waterphone"),
    "crys_explore_b": ("The Other Side of the Door", "USUAN1100744", 95, "explore.crystal: harp, oboe, flute"),
    "crys_night": ("Gone Beyond", "USUAN1100752", 118, "night.crystal: music box and choir"),
    "crys_combat": ("Serpentine Trek", "USUAN1100428", 75, "combat.crystal: flutes, clarinets, violin, percussion"),
    "sky_explore_a": ("Angevin B", "USUAN1200111", 120, "explore.sky_islands: flute, lute, choir"),
    "sky_explore_b": ("Willow and the Light", "USUAN1100591", 90, "explore.sky_islands: harp, flute, voice"),
    "sky_night": ("Unpromised", "USUAN1100603", 56, "night.sky_islands: vocalist, harp, flute"),
    "sky_combat": ("Journey To Ascend", "USUAN2200001", 116, "combat.sky_islands: tongue drum, driving percussion"),
    "sea_explore_a": ("Finding Movement", "USUAN1100693", 70, "explore.ocean: koto and strings"),
    "sea_explore_b": ("Myst", "USUAN1100448", 85, "explore.ocean: oboe, strings, choir"),
    "sea_night": ("Plaint", "USUAN1300009", 65, "night.ocean: bassoon and harp"),
    "sea_combat": ("Crossing the Chasm", "USUAN1700026", 100, "combat.ocean: drums, strings, singer"),
    "fun_explore": ("Confused State", "USUAN1100453", 95, "explore.fungal: marimba, kalimba, bassoon, clarinet"),
    "fun_night": ("Vulcan", "USUAN1100525", 78, "night.fungal: flutes, bells, low drums"),
    "fun_combat": ("Ignosi", "USUAN1100450", 79, "combat.fungal: trombones, marimba, percussion"),
    "void_explore": ("This House", "USUAN1500055", 74, "explore.wasteland: strings, waterphone"),
    "void_night": ("Colorless Aura", "USUAN1400008", 50, "night.wasteland: solo piano"),
    "void_combat": ("Unlight", "USUAN1100878", 151, "combat.wasteland: dissonant strings and brass"),
}

# Freesound id -> (author, title, licence, HQ preview)
FREESOUND = {
    18405: ("kokuya", "factory explosion steam burst.wav", "CC0 1.0", "https://cdn.freesound.org/previews/18/18405_71682-hq.mp3"),
    66930: ("juskiddink", "Thin ice on lake.wav", "CC-BY 4.0", "https://cdn.freesound.org/previews/66/66930_649468-hq.mp3"),
    77637: ("lendrick", "growl6.wav", "CC0 1.0", "https://cdn.freesound.org/previews/77/77637_237568-hq.mp3"),
    90143: ("pengo_au", "steam_burst.wav", "CC0 1.0", "https://cdn.freesound.org/previews/90/90143_793892-hq.mp3"),
    103116: ("jeo", "svalbard_recherche_glacier_20060904.wav", "CC-BY 4.0", "https://cdn.freesound.org/previews/103/103116_287621-hq.mp3"),
    162674: ("blaukreuz", "avl05.wav", "CC0 1.0", "https://cdn.freesound.org/previews/162/162674_623488-hq.mp3"),
    164477: ("deleted_user_2104797", "Birds_flapping 1.wav", "CC0 1.0", "https://cdn.freesound.org/previews/164/164477_2104797-hq.mp3"),
    173190: ("kvgarlic", "WalkingWinteronIceandSnowJanuary2013.wav", "CC0 1.0", "https://cdn.freesound.org/previews/173/173190_1050391-hq.mp3"),
    185070: ("juryduty", "howling_wind.wav", "CC0 1.0", "https://cdn.freesound.org/previews/185/185070_3279921-hq.mp3"),
    234782: ("wubitog", "Steam/hiss", "CC0 1.0", "https://cdn.freesound.org/previews/234/234782_3501487-hq.mp3"),
    244926: ("hnhnh", "fire-whoosh.wav", "CC0 1.0", "https://cdn.freesound.org/previews/244/244926_3983630-hq.mp3"),
    260555: ("LookIMadeAThing", "Basic Fire Whoosh 2", "CC0 1.0", "https://cdn.freesound.org/previews/260/260555_3968707-hq.mp3"),
    262635: ("j_p_higgins", "Ice Crack 1", "CC0 1.0", "https://cdn.freesound.org/previews/262/262635_3175560-hq.mp3"),
    262671: ("bretberger", "Red Spouter Fumarole", "CC-BY 4.0", "https://cdn.freesound.org/previews/262/262671_945420-hq.mp3"),
    263879: ("YleArkisto", "Islanti, geysir purkautuu, turistit / Iceland, geyser erupting twice, tourists reacting, talking, walking", "CC-BY 4.0", "https://cdn.freesound.org/previews/263/263879_4415905-hq.mp3"),
    271008: ("CCCanary", "glacierCalving.mp3", "CC0 1.0", "https://cdn.freesound.org/previews/271/271008_4119059-hq.mp3"),
    275388: ("metaepitome", "Horse Breathing.wav", "CC0 1.0", "https://cdn.freesound.org/previews/275/275388_1053041-hq.mp3"),
    277058: ("kwahmah_02", "Single Dog Bark", "CC0 1.0", "https://cdn.freesound.org/previews/277/277058_4486188-hq.mp3"),
    288941: ("toefur", "rat-squeak.wav", "CC0 1.0", "https://cdn.freesound.org/previews/288/288941_34567-hq.mp3"),
    316920: ("Rudmer_Rotteveel", "Chicken Single Alarm Call", "CC0 1.0", "https://cdn.freesound.org/previews/316/316920_4921277-hq.mp3"),
    345636: ("cabled_mess", "Frame drum_oneshot_RAW_13.wav", "CC0 1.0", "https://cdn.freesound.org/previews/345/345636_5450487-hq.mp3"),
    346318: ("nielstii", "raven x3", "CC0 1.0", "https://cdn.freesound.org/previews/346/346318_6290213-hq.mp3"),
    348942: ("abstraktgeneriert", "Crackle #1.wav", "CC0 1.0", "https://cdn.freesound.org/previews/348/348942_3610778-hq.mp3"),
    378334: ("Kingcornz", "Howling wolves", "CC0 1.0", "https://cdn.freesound.org/previews/378/378334_4804147-hq.mp3"),
    379472: ("vandale", "Wind5.mp3", "CC0 1.0", "https://cdn.freesound.org/previews/379/379472_1934086-hq.mp3"),
    386385: ("ldezem", "Torch - Ignite flame on soft", "CC0 1.0", "https://cdn.freesound.org/previews/386/386385_1655965-hq.mp3"),
    389618: ("_stubb", "Rock Tumble 2.wav", "CC0 1.0", "https://cdn.freesound.org/previews/389/389618_6068748-hq.mp3"),
    389638: ("_stubb", "Growl 7", "CC0 1.0", "https://cdn.freesound.org/previews/389/389638_6068748-hq.mp3"),
    389653: ("_stubb", "growl 9.wav", "CC0 1.0", "https://cdn.freesound.org/previews/389/389653_6068748-hq.mp3"),
    390733: ("FunWithSound", "Buzzing Insect Short.mp3", "CC0 1.0", "https://cdn.freesound.org/previews/390/390733_6456158-hq.mp3"),
    400663: ("Sojan", "Cricket Chirp", "CC0 1.0", "https://cdn.freesound.org/previews/400/400663_5477651-hq.mp3"),
    415608: ("RTB45", "Mud Volcano Field - Salton Sea", "CC0 1.0", "https://cdn.freesound.org/previews/415/415608_2409224-hq.mp3"),
    415609: ("RTB45", "Mud Volcanos - Salton Sea", "CC0 1.0", "https://cdn.freesound.org/previews/415/415609_2409224-hq.mp3"),
    422669: ("Lynx_5969", "Ice Break With Hand.wav", "CC0 1.0", "https://cdn.freesound.org/previews/422/422669_7844747-hq.mp3"),
    426174: ("NeoSpica", "Soil Slide", "CC0 1.0", "https://cdn.freesound.org/previews/426/426174_7704891-hq.mp3"),
    426318: ("MTJohnson", "Rocks Falling.wav", "CC0 1.0", "https://cdn.freesound.org/previews/426/426318_8522109-hq.mp3"),
    435748: ("RICHERlandTV", "Guineapig", "CC0 1.0", "https://cdn.freesound.org/previews/435/435748_3450800-hq.mp3"),
    437111: ("craigsmith", "G38-16-Four Horse Snorts.wav", "CC0 1.0", "https://cdn.freesound.org/previews/437/437111_2524442-hq.mp3"),
    452094: ("kyles", "forest wind blow snow of tree trickle like rain stick cool mono.flac", "CC0 1.0", "https://cdn.freesound.org/previews/452/452094_612689-hq.mp3"),
    452109: ("kyles", "branch break snap crunch nice.wav", "CC0 1.0", "https://cdn.freesound.org/previews/452/452109_612689-hq.mp3"),
    454372: ("kyles", "wind strong winter gusty alley balcony blowing prickly snow powder +debris flapping, some wood chimes Montreal, Canada sorry I know it&#x27;s long.flac", "CC0 1.0", "https://cdn.freesound.org/previews/454/454372_612689-hq.mp3"),
    456806: ("Breviceps", "Hot Bubbling Mud", "CC0 1.0", "https://cdn.freesound.org/previews/456/456806_9159316-hq.mp3"),
    457528: ("Breviceps", "Avalanche Hit", "CC0 1.0", "https://cdn.freesound.org/previews/457/457528_9159316-hq.mp3"),
    472402: ("JoseAgudelo", "22_Lobo_aullando.wav", "CC0 1.0", "https://cdn.freesound.org/previews/472/472402_9934646-hq.mp3"),
    475483: ("o_ciz", "Horse(snort)_3.wav", "CC0 1.0", "https://cdn.freesound.org/previews/475/475483_7009965-hq.mp3"),
    486140: ("Anuj_Shrestha1", "taiko.wav", "CC0 1.0", "https://cdn.freesound.org/previews/486/486140_7543744-hq.mp3"),
    486141: ("Anuj_Shrestha1", "taiko2.wav", "CC0 1.0", "https://cdn.freesound.org/previews/486/486141_7543744-hq.mp3"),
    512780: ("iwanPlays", "CrowOrRaven2.wav", "CC0 1.0", "https://cdn.freesound.org/previews/512/512780_7108319-hq.mp3"),
    561004: ("KaBlazik_Samples", "KB_SleepingCat_Short.wav", "CC0 1.0", "https://cdn.freesound.org/previews/561/561004_12170839-hq.mp3"),
    563571: ("Cinetony", "Wind in forest with creaking tree", "CC0 1.0", "https://cdn.freesound.org/previews/563/563571_5985747-hq.mp3"),
    567249: ("iwanPlays", "Bricks/Stones/Rocks/Gravel Falling", "CC0 1.0", "https://cdn.freesound.org/previews/567/567249_7108319-hq.mp3"),
    573282: ("Sorinious_Genious", "Timpani 4.wav", "CC0 1.0", "https://cdn.freesound.org/previews/573/573282_12517458-hq.mp3"),
    613571: ("toban", "Frozen lake freezing again", "CC0 1.0", "https://cdn.freesound.org/previews/613/613571_3016740-hq.mp3"),
    615896: ("frodeims", "melting ice folgefonna glacier.wav", "CC0 1.0", "https://cdn.freesound.org/previews/615/615896_3755886-hq.mp3"),
    630648: ("haulaway", "single bark - small to medium dog", "CC0 1.0", "https://cdn.freesound.org/previews/630/630648_7228277-hq.mp3"),
    640992: ("burgersmoke", "Ground Squirrel First Chirp", "CC0 1.0", "https://cdn.freesound.org/previews/640/640992_392990-hq.mp3"),
    663040: ("felix.blume", "Volcano Mount Yasur, Tanna Island, from the bottom", "CC0 1.0", "https://cdn.freesound.org/previews/663/663040_1661766-hq.mp3"),
    665084: ("sunflora", "cracking_ice.mp3", "CC0 1.0", "https://cdn.freesound.org/previews/665/665084_10262644-hq.mp3"),
    666482: ("giwake", "Ice Breaking 5", "CC0 1.0", "https://cdn.freesound.org/previews/666/666482_4421107-hq.mp3"),
    669872: ("SergioJbs", "bah bala oveja/baa bleat sheep.WAV", "CC0 1.0", "https://cdn.freesound.org/previews/669/669872_8392567-hq.mp3"),
    677218: ("satoristudios3", "mini Goat Baa", "CC0 1.0", "https://cdn.freesound.org/previews/677/677218_14166079-hq.mp3"),
    697217: ("dhallcomposer", "Looping Gentle Wind Ambience on an Open Desert Plain.wav", "CC0 1.0", "https://cdn.freesound.org/previews/697/697217_7678208-hq.mp3"),
    709430: ("alexdecastro", "Snoring dog", "CC0 1.0", "https://cdn.freesound.org/previews/709/709430_15221465-hq.mp3"),
    716962: ("fudgealtoid", "Raven Croak or Crow Caw", "CC0 1.0", "https://cdn.freesound.org/previews/716/716962_159528-hq.mp3"),
    737896: ("Vrymaa", "Ice - Lake fractures", "CC0 1.0", "https://cdn.freesound.org/previews/737/737896_13973196-hq.mp3"),
    738085: ("phuonglamb", "WindGust", "CC0 1.0", "https://cdn.freesound.org/previews/738/738085_9523513-hq.mp3"),
    764657: ("getwecked", "ice cracking", "CC0 1.0", "https://cdn.freesound.org/previews/764/764657_16538157-hq.mp3"),
    779944: ("felix.blume", "Strong wind in an antenna, slight whistle, squalls sometimes", "CC0 1.0", "https://cdn.freesound.org/previews/779/779944_1661766-hq.mp3"),
    812026: ("qubodup", "Bird Freaking Out", "CC0 1.0", "https://cdn.freesound.org/previews/812/812026_71257-hq.mp3"),
    827539: ("qubodup", "Dog Long Greeting Yelp", "CC0 1.0", "https://cdn.freesound.org/previews/827/827539_71257-hq.mp3"),
    832102: ("TheKingOfGeeks360", "Mules - Mule Breaths, Close Perspective", "CC0 1.0", "https://cdn.freesound.org/previews/832/832102_15895934-hq.mp3"),
    847807: ("aerror", "rumbling_ice", "CC0 1.0", "https://cdn.freesound.org/previews/847/847807_2472895-hq.mp3"),
}


def get(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (Wildreach sound pass)"})
    data = urllib.request.urlopen(req, timeout=120).read()
    open(path, "wb").write(data)
    print("got", os.path.basename(path), len(data))


if __name__ == "__main__":
    work = sys.argv[1]
    for f, (title, isrc, bpm, cue) in MUSIC.items():
        get(MUSIC_FILE + urllib.parse.quote(title) + ".mp3", os.path.join(work, "dl", "music", title + ".mp3"))
    for sid, (author, title, lic, url) in FREESOUND.items():
        get(url, os.path.join(work, "dl", "fs", "%d.mp3" % sid))
